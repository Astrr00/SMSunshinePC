// Dev-only headless gameplay driver. Enabled by env vars, never active by default:
//   SUNSHINE_DEV_SCRIPT=<file>  pad script, advanced once per logic step (30 Hz)
//   SUNSHINE_DEV_STATUS=1       status line once per second of game time
// Input goes through Aurora's virtual pad status; the game code runs unchanged.
#include <dolphin/pad.h>
#include <dolphin/gx.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "Camera/Camera.hpp"
#include "JSystem/JDrama/JDRNameRefGen.hpp"
#include "MoveBG/Item.hpp"
#include "MoveBG/ItemManager.hpp"
#include "MoveBG/MapObjManager.hpp"
#include "System/MarioGamePad.hpp"
#include "Player/Mario.hpp"
#include "System/Application.hpp"
#include "System/FlagManager.hpp"
#include "System/MarDirector.hpp"
#include "GC2D/ConsoleStr.hpp"
#include "GC2D/GCConsole2.hpp"

// Aurora's virtual pad API. <dolphin/pad.h> resolves to the decomp copy (sms/include/dolphin, earlier on the
// include path), which lacks these two; same signatures as extern/aurora/include/dolphin/pad.h.
extern "C" void PADSetVirtualStatus(u32 port, const PADStatus* status);
extern "C" void PADClearVirtualStatus(u32 port);

namespace {

enum class Op { Wait, Hold, Shine, Log, End };

struct Command {
	Op op;
	int frames = 0;
	int line = 0;
	PADStatus pad{};
};

struct ButtonName {
	const char* name;
	u16 bit;
};

const ButtonName kButtons[] = {
	{"A", PAD_BUTTON_A},         {"B", PAD_BUTTON_B},         {"X", PAD_BUTTON_X},
	{"Y", PAD_BUTTON_Y},         {"Z", PAD_TRIGGER_Z},        {"START", PAD_BUTTON_START},
	{"L", PAD_TRIGGER_L},        {"R", PAD_TRIGGER_R},        {"UP", PAD_BUTTON_UP},
	{"DOWN", PAD_BUTTON_DOWN},   {"LEFT", PAD_BUTTON_LEFT},   {"RIGHT", PAD_BUTTON_RIGHT},
};

std::vector<Command> gScript;
size_t gCursor = 0;
int gRemaining = 0;
bool gStarted = false;
bool gLoaded = false;
bool gDone = false;
int gTick = 0;

[[noreturn]] void fail(int line, const char* msg, const std::string& tok)
{
	std::fprintf(stderr, "[devscript] line %d: %s '%s'\n", line, msg, tok.c_str());
	std::exit(2);
}

bool parsePair(const std::string& value, long lo, long hi, int& a, int& b)
{
	const size_t comma = value.find(',');
	if (comma == std::string::npos) return false;
	char* end = nullptr;
	const long x = std::strtol(value.c_str(), &end, 10);
	if (end != value.c_str() + comma) return false;
	const long y = std::strtol(value.c_str() + comma + 1, &end, 10);
	if (*end != '\0') return false;
	if (x < lo || x > hi || y < lo || y > hi) return false;
	a = static_cast<int>(x);
	b = static_cast<int>(y);
	return true;
}

Command parseLine(const std::string& text, int line)
{
	std::istringstream in(text);
	std::string verb;
	in >> verb;
	Command c{};
	c.line = line;

	if (verb == "wait" || verb == "hold") {
		c.op = verb == "wait" ? Op::Wait : Op::Hold;
		std::string tok;
		while (in >> tok) {
			if (tok.rfind("frames=", 0) == 0) {
				c.frames = std::atoi(tok.c_str() + 7);
			} else if (c.op == Op::Wait) {
				fail(line, "unexpected token in wait", tok);
			} else if (tok.rfind("stick=", 0) == 0) {
				int x = 0, y = 0;
				if (!parsePair(tok.substr(6), -128, 127, x, y)) fail(line, "bad stick", tok);
				c.pad.stickX = static_cast<s8>(x);
				c.pad.stickY = static_cast<s8>(y);
			} else if (tok.rfind("cstick=", 0) == 0) {
				int x = 0, y = 0;
				if (!parsePair(tok.substr(7), -128, 127, x, y)) fail(line, "bad cstick", tok);
				c.pad.substickX = static_cast<s8>(x);
				c.pad.substickY = static_cast<s8>(y);
			} else if (tok.rfind("trig=", 0) == 0) {
				int l = 0, r = 0;
				if (!parsePair(tok.substr(5), 0, 255, l, r)) fail(line, "bad trig", tok);
				c.pad.triggerLeft = static_cast<u8>(l);
				c.pad.triggerRight = static_cast<u8>(r);
			} else {
				bool found = false;
				for (const ButtonName& b : kButtons) {
					if (tok == b.name) {
						c.pad.button |= b.bit;
						found = true;
						break;
					}
				}
				if (!found) fail(line, "unknown token", tok);
			}
		}
		if (c.op == Op::Wait && c.frames <= 0) fail(line, "wait needs frames=N", text);
		if (c.op == Op::Hold && c.frames <= 0) fail(line, "hold needs frames=N", text);
		return c;
	}
	if (verb == "shine") { c.op = Op::Shine; return c; }
	if (verb == "log") { c.op = Op::Log; return c; }
	if (verb == "end") { c.op = Op::End; return c; }
	fail(line, "unknown command", verb);
}

void loadScript(const char* path)
{
	std::ifstream file(path);
	if (!file) {
		std::fprintf(stderr, "[devscript] cannot open %s\n", path);
		std::exit(2);
	}
	std::string text;
	int line = 0;
	while (std::getline(file, text)) {
		++line;
		const size_t hash = text.find('#');
		if (hash != std::string::npos) text.erase(hash);
		if (text.find_first_not_of(" \t\r") == std::string::npos) continue;
		gScript.push_back(parseLine(text, line));
	}
	std::printf("[devscript] loaded %zu commands from %s\n", gScript.size(), path);
}

int shineCount()
{
	return TFlagManager::smInstance->getFlag(0x40000);
}

void logStatus(const char* tag)
{
	if (!gpMarioOriginal || !gpCamera || !gpMarDirector) {
		std::printf("[%s] tick=%d t=%.2fs (no mario/camera/director yet)\n", tag, gTick, gTick / 30.0);
		return;
	}
	const TMario* m = gpMarioOriginal;
	const TMarioGamePad* pad = gpApplication.mGamePads[0];
	const auto* con = gpMarDirector->mConsole;
	const auto* str = con ? con->unk94 : nullptr;
	std::printf(
	    "[%s] tick=%d t=%.2fs stage=%u scen=%u mario=(%.1f,%.1f,%.1f) status=0x%08X "
	    "cam=(%.1f,%.1f,%.1f) camDemo=%d shines=%d dir.state=%u dir.4C=0x%04X dir.4E=0x%04X dir.261=%u "
	    "con.2BC=%d con.move=%d pad.dis=%d pad.stick=(%.2f,%.2f)\n",
	    tag, gTick, gTick / 30.0, gpApplication.mCurrArea.getStage(), gpApplication.mCurrArea.getScenario(),
	    m->mPosition.x, m->mPosition.y, m->mPosition.z, m->getStatus(),
	    gpCamera->mPosition.x, gpCamera->mPosition.y, gpCamera->mPosition.z, gpCamera->isDemoCamera() ? 1 : 0,
	    shineCount(), gpMarDirector->mState, gpMarDirector->unk4C, gpMarDirector->unk4E, gpMarDirector->unk261,
	    str ? str->unk2BC : -1, con ? (con->unkC.get() & 1) : -1,
	    pad ? pad->mDisabledFrames : -1, pad ? pad->mMainStick.mPosX : 0.f, pad ? pad->mMainStick.mPosY : 0.f);
	std::printf("[%s-mario] stState=%u stTimer=%u stArg=0x%08X unk114=0x%04X prevStatus=0x%08X\n", tag,
	    m->mStatusState, m->mStatusTimer, m->mStatusArg, m->unk114, m->getPreviousStatus());
	std::printf("[%s-mario2] performing=%d freeze=%d mFlag=0x%08X\n", tag,
	    m->checkFlag(MARIO_FLAG_IS_PERFORMING) ? 1 : 0, (int)m->mFreezeTimer, m->mFlag);
	std::printf("[%s-mario3] health=%d floor=(%.1f,%.1f,%.1f)\n", tag, (int)m->getHealth(), m->mFloorPosition.x,
	    m->mFloorPosition.y, m->mFloorPosition.z);
	std::printf("[%s-pad] flags=0x%04X en=0x%08X dis=0x%08X disFrames=%d\n", tag, pad ? pad->mFlags : 0,
	    pad ? pad->mEnabledFrameMeaning : 0u, pad ? pad->mDisabledFrameMeaning : 0u, pad ? pad->mDisabledFrames : -1);
}
// Dev trigger: runs the game's own collect path (TShine::touchPlayer -> TMario::receiveMessage
// -> WIN_DEMO -> fireGetStar -> setShineFlag). Only the overlap test is skipped.
// Shines are found by type in the item and map-object managers (scene shine names vary).
void doShine()
{
	TObjManager* mgrs[] = {gpItemManager, gpMapObjManager};
	TShine* shine = nullptr;
	int seen = 0;
	for (TObjManager* mgr : mgrs) {
		if (!mgr) continue;
		seen += mgr->getObjNum();
		for (int i = 0; !shine && i < mgr->getObjNum(); ++i) {
			shine = dynamic_cast<TShine*>(mgr->getObj(i));
		}
	}
	if (!shine || !gpMarioOriginal) {
		std::printf("[devscript] shine: no TShine among %d managed objects (tick=%d)\n", seen, gTick);
		return;
	}
	std::printf("[devscript] shine: eventId=%u shines_before=%d tick=%d\n", shine->getEventId(), shineCount(), gTick);
	shine->touchPlayer(gpMarioOriginal);
}

void finish()
{
	if (gDone) return;
	gDone = true;
	PADClearVirtualStatus(0);
	std::printf("[devscript] done at tick=%d\n", gTick);
}

void step()
{
	while (gCursor < gScript.size()) {
		const Command& c = gScript[gCursor];
		if (c.op == Op::Wait || c.op == Op::Hold) {
			if (!gStarted) {
				gStarted = true;
				gRemaining = c.frames;
				std::printf("[devscript] %s start line=%d frames=%d tick=%d\n",
				    c.op == Op::Wait ? "wait" : "hold", c.line, c.frames, gTick);
			}
			const PADStatus neutral{};
			PADSetVirtualStatus(0, c.op == Op::Hold ? &c.pad : &neutral);
			if (--gRemaining == 0) {
				gStarted = false;
				++gCursor;
			}
			return;
		}
		if (c.op == Op::Shine) doShine();
		if (c.op == Op::Log) logStatus("devscript");
		if (c.op == Op::End) {
			gCursor = gScript.size();
			break;
		}
		++gCursor;
	}
	finish();
}

} // namespace

namespace {

} // namespace


// Called once per logic step from sunshineLogicTick().
void sunshineDevScriptTick()
{
	++gTick;
	static const char* const scriptPath = std::getenv("SUNSHINE_DEV_SCRIPT");
	if (scriptPath && !gLoaded) {
		gLoaded = true;
		loadScript(scriptPath);
	}
	if (gLoaded && !gDone) step();
	if (std::getenv("SUNSHINE_DEV_STATUS") && gTick % 30 == 0) logStatus("status");
	if (gLoaded || std::getenv("SUNSHINE_DEV_STATUS")) std::fflush(stdout); // harness kills the game; keep lines
}
