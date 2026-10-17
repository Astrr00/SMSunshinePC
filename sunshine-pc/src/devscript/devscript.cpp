// Dev-only headless gameplay driver. Enabled by env vars, never active by default:
//   SUNSHINE_DEV_SCRIPT=<file>  pad script, advanced once per logic step (30 Hz)
//   SUNSHINE_DEV_STATUS=1       status line once per second of game time
//   SUNSHINE_DEV_VPAD=1         attach an SDL virtual gamepad as player 0 at startup (launcher::devVpadAttach), so the
//                               [controls] dead zones apply to it; `menu vstick:<x>:<y>` moves its left stick
//   SUNSHINE_DEV_VPAD=late      same pad, added only at the game's first event pump (after applyRuntimeSettings)
// Input goes through Aurora's virtual pad status; the game code runs unchanged.
#include <dolphin/pad.h>
#include <dolphin/gx.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <deque>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Camera/Camera.hpp"
#include "Camera/CameraOption.hpp"
#include "System/StageUtil.hpp"
#include "JSystem/JDrama/JDRNameRefGen.hpp"
#include "cheats/cheats.h"
#include "launcher/launcher.hpp"
#include "MoveBG/Item.hpp"
#include "MoveBG/ItemManager.hpp"
#include "MoveBG/MapObjManager.hpp"
#include "MoveBG/MapObjTown.hpp"
#include "MoveBG/MapObjHide.hpp"
#include "Enemy/Emario.hpp"
#include "Enemy/EnemyMario.hpp"
#include "MoveBG/ModelGate.hpp"
#include "Player/MarioAccess.hpp"
#include "Enemy/GateKeeper.hpp"
#include "Enemy/BossPakkun.hpp"
#include "Enemy/Conductor.hpp"
#include "Enemy/EnemyManager.hpp"
#include "Enemy/SmallEnemy.hpp"
#include "Strategic/Spine.hpp"
#include "System/MarioGamePad.hpp"
#include "Player/Mario.hpp"
#include "System/Application.hpp"
#include "System/FlagManager.hpp"
#include "System/MarDirector.hpp"
#include "GC2D/CardLoad.hpp"
#include "GC2D/ConsoleStr.hpp"
#include "System/EmitterViewObj.hpp"
#include "JSystem/JParticle/JPAEmitterManager.hpp"
#include "JSystem/JParticle/JPAEmitter.hpp"

extern JPAEmitterManager* gpEmitterManager4D2;
#include "GC2D/GCConsole2.hpp"

// Aurora's virtual pad API. <dolphin/pad.h> resolves to the decomp copy (sms/include/dolphin, earlier on the
// include path), which lacks these two; same signatures as extern/aurora/include/dolphin/pad.h.
extern "C" void PADSetVirtualStatus(u32 port, const PADStatus* status);

// SUNSHINE_DEV_AIM_ASSIST: while set the R trigger (spray) is removed from every virtual pad status (see aimAssist()).
static bool gAssistNoSpray = false;
static void setVirtualPad(u32 port, const PADStatus* status)
{
	PADStatus copy = *status;
	if (gAssistNoSpray) {
		copy.triggerRight = 0;
		copy.button &= ~PAD_TRIGGER_R;
	}
	PADSetVirtualStatus(port, &copy);
}
extern "C" void PADClearVirtualStatus(u32 port);
extern "C" void aurora_screenshot_next(const char* path);
extern "C" void aurora_dvd_close(void);
extern "C" s32 THPPlayerGetState();

#ifdef TARGET_PC
// Dev hook: cumulative spray hit counters, defined at file scope in sms/src/Enemy/gatekeeper.cpp (TARGET_PC).
extern int gDbgGKHeadSprayHits;
extern int gDbgGKBodySprayHits;
extern int gDbgGKCurBck;
#endif
// Logic tick count from sunshine-pc/src/framerate/framerate.cpp. Return type must be u32: MSVC mangling includes it,
// and an `unsigned` declaration links to an undefined symbol (/FORCE:UNRESOLVED turns the call into a bogus address).
u32 sunshineLogicTickCount();

namespace {

enum class Op { Wait, Hold, Shine, Log, Trace, Shot, End, Hurt, TpShine, WaitFor, Mash, WalkTo, FaceTo, AimBoss, Tp, Cheat, KillBoss, TpEm, Chase, FightBoss, Menu, DiscErr, Clear, Label, Goto, Guard };

struct Command {
	enum class Cond { None, Stage, Scen, Mario, Director, DirState, Flag, Thp, CardMode, CardProg, CamDemo, Washed, Talk, Bck, MarioYGe, MarioYLe, FloorYGe, Weak, Facing, PeteyHp };
	Op op;
	int frames = 0;
	int line = 0;
	std::string path;
	PADStatus pad{};
	float pos[3]{};
	std::vector<std::pair<Cond, int>> conds; // waitfor/mash: ALL must hold to end the op
	int maxTicks = 0;       // waitfor/mash/walkto: fail loudly after this many logic ticks
	float radius = 80.f;    // walkto: arrival distance in the XZ plane
	bool spray = false;     // walkto: hold the analog R trigger while walking (the spray cleans goop ahead, so Mario keeps grip)
	int sprayFrom = 0;      // walkto spray=N: R held from tick N-1 on
	bool hover = false;     // walkto hover=1: hover steering, and R let go once a trigger nozzle is spent
	bool navel = false;     // walkto/faceto navel=1: target Petey's navel, re-read every tick (what the in-game target arrow marks)
	bool petey = false;     // walkto/faceto petey=1: target Petey's position
	float behind = 0.f;     // walkto behind=R: the point R behind Petey, reached around his swing radius
	bool soft = false;      // walkto/faceto/waitfor soft=1: a timeout moves on (to a `goto` retry) instead of failing
	int taken = 0;          // goto: jumps back taken so far (frames holds the label's command index)
	float tol = 8.f;        // faceto: accepted angle error in degrees
	int period = 30;        // mash: press for 2 ticks (hold=N: N ticks, a higher jump) every `period` ticks
	bool gateMouth = false; // hold gate=mouth: the analog R trigger only while Petey's mouth takes water (peteyMouthTakesWater)
	bool tpBehind = false;  // chase tp=1: dev crutch, not retail input: every 60 ticks put Mario behind the shadow Mario and spray for 45 of them
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
int gTrace = 0; // `trace frames=N` ticks left
bool gStarted = false;
bool gLoaded = false;
bool gDone = false;
int gTick = 0;
float gGuard = 0.f; // `guard radius=R`, 0 = off

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

bool parseCond(const std::string& tok, Command& c)
{
	Command::Cond k;
	if (tok == "mario") k = Command::Cond::Mario;
	else if (tok == "director") k = Command::Cond::Director;
	else if (tok.rfind("stage=", 0) == 0) k = Command::Cond::Stage;
	else if (tok.rfind("scen=", 0) == 0) k = Command::Cond::Scen;
	else if (tok.rfind("dirstate=", 0) == 0 || tok.rfind("dir.state=", 0) == 0) k = Command::Cond::DirState;
	else if (tok.rfind("flag=", 0) == 0) k = Command::Cond::Flag;
	else if (tok.rfind("cardmode=", 0) == 0) k = Command::Cond::CardMode; // TCardLoad::unk14
	else if (tok.rfind("cardprog=", 0) == 0) k = Command::Cond::CardProg; // TCardLoad::unk1C (TEProgress)
	else if (tok.rfind("thp=", 0) == 0 || tok.rfind("movie=", 0) == 0) k = Command::Cond::Thp; // THP player state, 0 = no movie
	else if (tok.rfind("camDemo=", 0) == 0) k = Command::Cond::CamDemo; // 1 while a demo camera owns the view
	else if (tok.rfind("washed=", 0) == 0) k = Command::Cond::Washed; // graffiti wash finished (see washedCount)
	else if (tok.rfind("talk=", 0) == 0) k = Command::Cond::Talk; // 1 while a talk/message window owns the game (TMarDirector::isTalkModeNow)
	else if (tok.rfind("bck=", 0) == 0) k = Command::Cond::Bck; // TBiancoGateKeeper/GateKeeper current bck index (gatekeeper.cpp gDbgGKCurBck)
	else if (tok.rfind("marioy>=", 0) == 0) k = Command::Cond::MarioYGe; // Mario's height, what a player sees (a platform carries him up)
	else if (tok.rfind("marioy<=", 0) == 0) k = Command::Cond::MarioYLe;
	else if (tok.rfind("floory>=", 0) == 0) k = Command::Cond::FloorYGe; // the ground under Mario, e.g. a windmill block passing below a jump
	else if (tok.rfind("weak=", 0) == 0) k = Command::Cond::Weak; // TBossPakkun::mWeakPoint (1 navel, 2 mouth), the target arrow a player follows
	else if (tok.rfind("facing=", 0) == 0) k = Command::Cond::Facing; // Petey faces Mario within N degrees (his mouth only takes water from the front)
	else if (tok.rfind("peteyhp=", 0) == 0) k = Command::Cond::PeteyHp; // Petey's hit points (3 at the start, one per navel pound)
	else return false;
	const size_t eq = tok.find('=');
	c.conds.emplace_back(k, eq == std::string::npos ? 0 : static_cast<int>(std::strtol(tok.c_str() + eq + 1, nullptr, 0)));
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
			if (c.op == Op::Hold && tok == "until") continue;
			if (c.op == Op::Hold && (tok.rfind("timeout=", 0) == 0 || tok.rfind("max=", 0) == 0)) {
				c.maxTicks = std::atoi(tok.c_str() + tok.find('=') + 1);
				continue;
			}
			if (c.op == Op::Hold && parseCond(tok, c)) continue;
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
			} else if (tok == "gate=mouth") {
				c.gateMouth = true;
			} else if (tok == "aimhead") {
				c.spray = true; // dev-only: face the airport GateKeeper head every tick while the pad is held (diagnosis only)
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
		if (c.op == Op::Hold && c.frames <= 0 && c.conds.empty()) fail(line, "hold needs frames=N or until <cond>", text);
		if (c.op == Op::Hold && !c.conds.empty() && c.maxTicks <= 0) fail(line, "hold until needs timeout=N (logic ticks)", text);
		return c;
	}
	if (verb == "waitfor" || verb == "mash" || verb == "pressuntil") {
		c.op = verb == "waitfor" ? Op::WaitFor : Op::Mash;
		std::string tok;
		while (in >> tok) {
			if (tok == "until") continue;
			if (tok.rfind("max=", 0) == 0 || tok.rfind("timeout=", 0) == 0) {
				c.maxTicks = std::atoi(tok.c_str() + tok.find('=') + 1);
			} else if (c.op == Op::WaitFor && tok == "soft=1") {
				c.soft = true;
			} else if (tok.rfind("period=", 0) == 0) {
				c.period = std::atoi(tok.c_str() + 7);
			} else if (c.op == Op::Mash && tok.rfind("hold=", 0) == 0) {
				c.frames = std::atoi(tok.c_str() + 5);
			} else if (parseCond(tok, c)) {
				continue;
			} else if (c.op == Op::WaitFor) {
				fail(line, "unexpected token in waitfor", tok);
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
		if (c.conds.empty()) fail(line, "needs a condition (stage=, scen=, dirstate=, flag=, thp=/movie=, mario, director)", text);
		if (c.maxTicks <= 0) fail(line, "needs timeout=N (logic ticks)", text);
		if (c.op == Op::Mash && c.period < 2) fail(line, "period must be >= 2", text);
		if (c.op == Op::Mash && c.frames >= c.period) fail(line, "hold must be < period", text);
		return c;
	}
	if (verb == "walkto" || verb == "faceto") {
		// walkto x=<x> z=<z> [radius=R] [spray=N] [jump=N] [hover=1] timeout=N: main stick toward (x,z), camera-relative, until within R of it.
		// navel=1 / petey=1 / behind=R instead of x/z: Petey's navel, Petey, or R behind him (stage 55), followed every tick.
		c.op = verb == "walkto" ? Op::WalkTo : Op::FaceTo; // faceto x= z= [tol=deg] timeout=N: turn Mario on the spot toward (x,z)
		bool gotX = false, gotZ = false;
		std::string tok;
		while (in >> tok) {
			const size_t eq = tok.find('=');
			if (eq == std::string::npos) fail(line, "unexpected token in walkto", tok);
			const std::string key = tok.substr(0, eq);
			const float v = static_cast<float>(std::atof(tok.c_str() + eq + 1));
			if (key == "x") { c.pos[0] = v; gotX = true; }
			else if (key == "z") { c.pos[2] = v; gotZ = true; }
			else if (key == "radius") c.radius = v;
			else if (key == "spray") { c.spray = v != 0.f; c.sprayFrom = static_cast<int>(v) - 1; } // R from tick N-1: jump first, then hover
			else if (key == "jump" && c.op == Op::WalkTo) c.frames = static_cast<int>(v); // A held for the first N ticks while steering
			else if (key == "hover" && c.op == Op::WalkTo) c.hover = v != 0.f; // opt-in: older routes were tuned without it
			else if (key == "navel") c.navel = gotX = gotZ = v != 0.f;
			else if (key == "petey") c.petey = gotX = gotZ = v != 0.f;
			else if (key == "behind" && c.op == Op::WalkTo) { c.behind = v; gotX = gotZ = v > 0.f; }
			else if (key == "soft") c.soft = v != 0.f;
			else if (key == "tol") c.tol = v;
			else if (key == "timeout" || key == "max") c.maxTicks = static_cast<int>(v);
			else fail(line, "unexpected token in walkto", tok);
		}
		if (!gotX || !gotZ) fail(line, "walkto needs x= and z=", text);
		if (c.maxTicks <= 0) fail(line, "walkto needs timeout=N (logic ticks)", text);
		return c;
	}
	if (verb == "guard") {
		// guard radius=R: from here on, while Mario is on the ground and a small enemy is within R, pause the script and spray it
		// (as a player turns on a goop slug before it jumps him); guard radius=0 turns it off.
		c.op = Op::Guard;
		std::string tok;
		if (!(in >> tok) || tok.rfind("radius=", 0) != 0 || (in >> tok)) fail(line, "guard needs only radius=R", text);
		c.radius = static_cast<float>(std::atof(text.c_str() + text.find('=') + 1));
		return c;
	}
	if (verb == "label" || verb == "goto") {
		// label NAME / goto NAME unless <cond...> max=K: jump back to NAME while the conditions fail, at most K times, then fail.
		// A fight round a knock-back or a missed pound spoils is played again, as a player would.
		c.op = verb == "label" ? Op::Label : Op::Goto;
		if (!(in >> c.path)) fail(line, "needs a label name", text);
		std::string tok;
		while (in >> tok) {
			if (c.op == Op::Goto && tok == "unless") continue;
			if (c.op == Op::Goto && tok.rfind("max=", 0) == 0) c.maxTicks = std::atoi(tok.c_str() + 4);
			else if (c.op != Op::Goto || !parseCond(tok, c)) fail(line, "unexpected token", tok);
		}
		if (c.op == Op::Goto && (c.conds.empty() || c.maxTicks <= 0)) fail(line, "goto needs unless <cond> and max=K", text);
		return c;
	}
	if (verb == "clear") {
		// clear [radius=R] [until <cond...>] timeout=N: walk at the nearest live small enemy within R (XZ, 300 up or down) with R held, as a
		// player sprays goop slugs off; ends when none is left in R (with until: when the conditions hold, standing still while none is
		// near), or quietly at the timeout (they keep spawning).
		c.op = Op::Clear;
		c.radius = 700.f;
		std::string tok;
		while (in >> tok) {
			if (tok.rfind("radius=", 0) == 0) c.radius = static_cast<float>(std::atof(tok.c_str() + 7));
			else if (tok.rfind("timeout=", 0) == 0) c.maxTicks = std::atoi(tok.c_str() + 8);
			else if (tok != "until" && !parseCond(tok, c)) fail(line, "unexpected token in clear", tok);
		}
		if (c.maxTicks <= 0) fail(line, "clear needs timeout=N (logic ticks)", text);
		return c;
	}
	if (verb == "fightboss") {
		// fightboss timeout=N: real-input bot that sprays the airport GateKeeper's head (see step()); ends when the boss is dead.
		c.op = Op::FightBoss;
		std::string tok;
		while (in >> tok) {
			if (tok.rfind("timeout=", 0) == 0) c.maxTicks = std::atoi(tok.c_str() + 8);
			else fail(line, "unexpected token in fightboss", tok);
		}
		if (c.maxTicks <= 0) fail(line, "fightboss needs timeout=N (logic ticks)", text);
		return c;
	}
	if (verb == "aimboss") {
		// aimboss frames=N: every tick turn Mario toward the airport GateKeeper's head hit object (small stick deflection) while the
		// analog R trigger is held; ends early when the boss is dead. Dev-only aim aid for the tutorial fight.
		c.op = Op::AimBoss;
		std::string tok;
		while (in >> tok)
			if (tok.rfind("frames=", 0) == 0) c.frames = std::atoi(tok.c_str() + 7);
		if (c.frames <= 0) fail(line, "aimboss needs frames=N", text);
		return c;
	}
	if (verb == "hurt") {
		// Test-only: applies damage through TMario::decHP, so the cheat health guard can be checked. frames = damage.
		c.op = Op::Hurt;
		if (!(in >> c.frames) || c.frames <= 0) fail(line, "hurt needs a positive damage amount", text);
		return c;
	}
	if (verb == "chase") {
		// chase timeout=N [spray=R]: dev aid, reads the shadow Mario's position and steers Mario toward him with the real stick, holding the
		// analog R trigger within `radius` (default 900) units; ends when a talk window opens. Input stays real, only the target is read.
		c.op = Op::Chase;
		c.radius = 900.f;
		std::string tok;
		while (in >> tok) {
			const size_t eq = tok.find('=');
			if (eq == std::string::npos) fail(line, "unexpected token in chase", tok);
			const std::string key = tok.substr(0, eq);
			const float v = static_cast<float>(std::atof(tok.c_str() + eq + 1));
			if (key == "timeout") c.maxTicks = static_cast<int>(v);
			else if (key == "radius") c.radius = v;
			else if (key == "tp") c.tpBehind = v != 0.f;
			else fail(line, "unexpected token in chase", tok);
		}
		if (c.maxTicks <= 0) fail(line, "chase needs timeout=N (logic ticks)", text);
		return c;
	}
	if (verb == "killboss") { c.op = Op::KillBoss; return c; } // dev-only: marks the airport GateKeeper dead (diagnosis, never acceptance)
	if (verb == "tpem") { // dev-only: puts Mario dz (default 250) units south of the shadow Mario, facing him (diagnosis only)
		c.op = Op::TpEm;
		c.pos[2] = 250.f;
		std::string tok;
		if (in >> tok) {
			if (tok.rfind("dz=", 0) != 0) fail(line, "unexpected token in tpem", tok);
			c.pos[2] = static_cast<float>(std::atof(tok.c_str() + 3));
		}
		return c;
	}
	if (verb == "tp") {
		// Dev-only teleport for diagnosis: tp x=<x> y=<y> z=<z> puts Mario there (velocity cleared). Never in an acceptance route.
		c.op = Op::Tp;
		std::string tok;
		while (in >> tok) {
			const size_t eq = tok.find('=');
			if (eq == std::string::npos) fail(line, "unexpected token in tp", tok);
			const std::string key = tok.substr(0, eq);
			const float v         = static_cast<float>(std::atof(tok.c_str() + eq + 1));
			if (key == "x") c.pos[0] = v;
			else if (key == "y") c.pos[1] = v;
			else if (key == "z") c.pos[2] = v;
			else if (key == "yaw") { c.tol = v; c.spray = true; } // degrees, game yaw convention (forward = (sin, cos))
			else fail(line, "unexpected token in tp", tok);
		}
		return c;
	}
	if (verb == "cheat") {
		// Dev-only: cheat <id> [off] switches one of the shipped cheats (assets/cheats.json), e.g. infinite_health, infinite_water.
		c.op = Op::Cheat;
		std::string tok;
		if (!(in >> c.path)) fail(line, "cheat needs an id", text);
		c.frames = 1;
		if (in >> tok) c.frames = tok == "off" ? 0 : 1;
		return c;
	}
	if (verb == "shine") { // shine [N]: N-th TShine of the scene (default 0)
		c.op = Op::Shine;
		in >> c.frames;
		return c;
	}
	if (verb == "tpshine") {
		// Dev-assisted: moves Mario onto the runtime TShine (+ optional x y z offset); the game's own overlap test then runs.
		c.op = Op::TpShine;
		for (float& v : c.pos) in >> v;
		return c;
	}
	if (verb == "shot") {
		c.op = Op::Shot;
		std::getline(in >> std::ws, c.path);
		while (!c.path.empty() && (c.path.back() == '\r' || c.path.back() == ' ')) c.path.pop_back();
		if (c.path.empty()) fail(line, "shot needs a path", text);
		return c;
	}
	if (verb == "menu") { // menu tok,tok,...: in-game menu dev tokens (launcher::menuDevQueue), one per 30 presented frames
		// key:<SDL scancode name>:down|up pushes real SDL key events (INI [keys] bindings apply); vstick:<x>:<y> needs SUNSHINE_DEV_VPAD
		c.op = Op::Menu;
		if (!(in >> c.path)) fail(line, "menu needs tokens", text);
		return c;
	}
	if (verb == "log") { c.op = Op::Log; return c; }
	if (verb == "trace") { // trace frames=N: one line of Mario/nozzle state per tick for N ticks, alongside the following commands
		c.op = Op::Trace;
		std::string tok;
		while (in >> tok)
			if (tok.rfind("frames=", 0) == 0) c.frames = std::atoi(tok.c_str() + 7);
		if (c.frames <= 0) fail(line, "trace needs frames=N", text);
		return c;
	}
	if (verb == "discerr") { c.op = Op::DiscErr; return c; }
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
	for (Command& g : gScript) {
		if (g.op != Op::Goto) continue;
		g.frames = -1;
		for (size_t i = 0; i < gScript.size(); ++i)
			if (gScript[i].op == Op::Label && gScript[i].path == g.path) g.frames = static_cast<int>(i);
		if (g.frames < 0) fail(g.line, "goto to an unknown label", g.path);
	}
	std::printf("[devscript] loaded %zu commands from %s\n", gScript.size(), path);
}

bool sceneReady()
{
	// PAD reads precede direct(): scene globals survive freeAll(), and unk260 is set only after the setup-thread join and setupObjects().
	return gpApplication.mAppState == TApplication::APP_STATE_GAMEPLAY && gpMarDirector
	    && gpApplication.mDirector == gpMarDirector && gpMarDirector->unk260 == 1;
}

int shineCount()
{
	return TFlagManager::smInstance->getFlag(0x40000);
}

// Wash state of each graffiti (TWaterHitPictureHideObj, e.g. the airport FLUDD tutorial M). state 3 = finished;
// progress runs min..max; alpha tracks progress (0 = fully hidden graffiti); hidden=1 when a reveal object is linked.
void logWash(const char* tag)
{
	if (!gpMapObjManager) return;
	for (int i = 0; i < gpMapObjManager->getObjNum(); ++i) {
		TWaterHitPictureHideObj* w = dynamic_cast<TWaterHitPictureHideObj*>(gpMapObjManager->getObj(i));
		if (!w) continue;
		std::printf("[%s-wash] event=%u state=%u progress=%.1f min=%.1f max=%.1f alpha=%d reverse=%d pos=(%.1f,%.1f,%.1f) hidden=%d tick=%d\n",
		    tag, w->getEventId(), (unsigned)w->mState, w->mProgress, w->mMinProgress, w->mMaxProgress, (int)w->mColor.a,
		    w->mReverse ? 1 : 0, w->mPosition.x, w->mPosition.y, w->mPosition.z, w->mHiddenObj ? 1 : 0, gTick);
		if (w->mHiddenObj)
			std::printf("[%s-wash]   revealed object: type=%s shine=%d coin=%d pos=(%.1f,%.1f,%.1f) tick=%d\n", tag,
			    typeid(*w->mHiddenObj).name(), w->mHiddenObj->isActorType(0x20000013) ? 1 : 0,
			    w->mHiddenObj->isActorType(0x2000000E) ? 1 : 0, w->mHiddenObj->mPosition.x, w->mHiddenObj->mPosition.y,
			    w->mHiddenObj->mPosition.z, gTick);
	}
	for (int i = 0; gpMapObjManager && i < gpMapObjManager->getObjNum(); ++i)
		if (const TMapObjStartDemo* d = dynamic_cast<const TMapObjStartDemo*>(gpMapObjManager->getObj(i)))
			std::printf("[%s-startdemo] unk138=%u pos=(%.1f,%.1f,%.1f) tick=%d\n", tag, d->unk138, d->mPosition.x,
			    d->mPosition.y, d->mPosition.z, gTick);
}

// Graffiti whose wash finished (state 3). Route condition washed=N: at least N finished.
int washedCount()
{
	int n = 0;
	for (int i = 0; gpMapObjManager && i < gpMapObjManager->getObjNum(); ++i)
		if (const TWaterHitPictureHideObj* w = dynamic_cast<const TWaterHitPictureHideObj*>(gpMapObjManager->getObj(i)))
			n += w->mState == TWaterHitPictureHideObj::STATE_FINISHED;
	return n;
}

// The airport GateKeeper object by its Shift-JIS scene name (null outside airport scenario 1 and before the scene is built).
TBiancoGateKeeper* findBoss()
{
	static const char kName[] = "\x83\x51\x81\x5b\x83\x67\x83\x4c\x81\x5b\x83\x70\x81\x5b\x81\x69\x83\x72\x83\x41\x83\x93\x83\x52\x81\x6a";
	if (!JDrama::TNameRefGen::getInstance() || !JDrama::TNameRefGen::getInstance()->getRootNameRef()) return nullptr;
	return dynamic_cast<TBiancoGateKeeper*>(JDrama::TNameRefGen::search(kName));
}

// Petey, the first actor of "ボスパックンマネージャー" (Shift-JIS bytes; a search for the actor name "ボスパックンA" found nothing), null outside stage 55.
TBossPakkun* findPetey()
{
	static const char kName[] = "\x83\x7b\x83\x58\x83\x70\x83\x62\x83\x4e\x83\x93\x83\x7d\x83\x6c\x81\x5b\x83\x57\x83\x83\x81\x5b";
	if (!JDrama::TNameRefGen::getInstance() || !JDrama::TNameRefGen::getInstance()->getRootNameRef()) return nullptr;
	TLiveManager* m = dynamic_cast<TLiveManager*>(JDrama::TNameRefGen::search(kName));
	return m && m->getObjNum() > 0 ? dynamic_cast<TBossPakkun*>(m->getObj(0)) : nullptr;
}

// The nearest live, shown small enemy within r of Mario in XZ and 300 in Y (`clear`), or null.
const TSmallEnemy* nearestSmallEnemy(float r)
{
	const TSmallEnemy* best = nullptr;
	if (!gpConductor || !gpMarioOriginal) return best;
	const JGeometry::TVec3<f32>& m = gpMarioOriginal->mPosition;
	for (JGadget::TList<TEnemyManager*>::iterator it = gpConductor->unk20.begin(); it != gpConductor->unk20.end(); ++it)
		for (int i = 0; i < (*it)->getObjNum(); ++i) {
			const TSmallEnemy* e = dynamic_cast<const TSmallEnemy*>((*it)->getObj(i));
			if (!e || e->checkLiveFlag(LIVE_FLAG_DEAD) || e->checkLiveFlag(LIVE_FLAG_HIDDEN) || std::fabs(e->mPosition.y - m.y) > 300.f) continue;
			// Already hit: one water hit pushes the die nerve (TNameKuri::behaveToWater); more spray only drains the tank.
			if (e->mSpine && e->mSpine->getCurrentNerve() == &TNerveSmallEnemyDie::theNerve()) continue;
			const float d = std::hypot(e->mPosition.x - m.x, e->mPosition.z - m.z);
			if (d < r) { r = d; best = e; }
		}
	return best;
}

void walkStick(const Command& c, s8& sx, s8& sy, float& dist);

// Pad that turns Mario toward e with the analog R trigger full on (`clear`, `guard`); gpCamera must be set.
void sprayAt(const TSmallEnemy* e, PADStatus& pad)
{
	Command t{};
	t.pos[0] = e->mPosition.x;
	t.pos[2] = e->mPosition.z;
	float dist = 0.f;
	walkStick(t, pad.stickX, pad.stickY, dist);
	if (dist < 300.f) { // close: turn toward it at a walk instead of running into its attack
		pad.stickX = static_cast<s8>(pad.stickX / 3);
		pad.stickY = static_cast<s8>(pad.stickY / 3);
	}
	// Water only once Mario roughly faces it (within 45 deg): the Petey arena has no refill, and spraying while turning
	// emptied the tank on the ordinary route (pg11_ramp_spray23e: water 0, then the slugs took the last health). 20 deg
	// was too late: the slugs jumped Mario before the spray started (pg11_petey_navel11, health 8 to 0).
	const float err = std::remainder(std::atan2(e->mPosition.x - gpMarioOriginal->mPosition.x, e->mPosition.z - gpMarioOriginal->mPosition.z)
	                                     - static_cast<s16>(gpMarioOriginal->mFaceAngle.y) * (6.2831853f / 65536.f), 6.2831853f);
	pad.triggerRight = std::fabs(err) < 0.785f ? 255 : 0;
}

// Petey's mouth takes water now: open (weak point 2) and Mario inside the damage cone in front of it, as
// TBPHeadHit::receiveMessage checks before gotWaterDamage (`hold ... gate=mouth`).
bool peteyMouthTakesWater()
{
	const TBossPakkun* p = findPetey();
	if (!p || !gpMarioOriginal || p->mWeakPoint != TBossPakkun::WEAK_POINT_MOUTH) return false;
	const float toMario = std::atan2(gpMarioOriginal->mPosition.x - p->mPosition.x, gpMarioOriginal->mPosition.z - p->mPosition.z) * (180.f / 3.14159265f);
	return std::fabs(std::remainder(toMario - p->mRotation.y, 360.f)) < 0.5f * p->getBossPakkunParams()->mSLDamageAngle.get();
}

// The shadow Mario of the Delfino gate event ("マリオモドキ_0", Shift-JIS bytes): position, distance to Mario.
void logEMario(const char* tag)
{
	static const char kName[] = "\x83\x7d\x83\x8a\x83\x49\x83\x82\x83\x68\x83\x4c\x5f\x30";
	if (!gpMarioOriginal || !JDrama::TNameRefGen::getInstance() || !JDrama::TNameRefGen::getInstance()->getRootNameRef()) return;
	THitActor* e = dynamic_cast<THitActor*>(JDrama::TNameRefGen::search(kName));
	if (!e) return;
	const float dx = e->mPosition.x - gpMarioOriginal->mPosition.x, dy = e->mPosition.y - gpMarioOriginal->mPosition.y, dz = e->mPosition.z - gpMarioOriginal->mPosition.z;
	static const char kDummy[] = "\x83\x5f\x83\x7e\x81\x5b\x82\x6d\x82\x6f\x82\x62";
	TEMario* em = dynamic_cast<TEMario*>(JDrama::TNameRefGen::search(kName));
	std::printf("[%s-emario] pos=(%.0f,%.0f,%.0f) dist3d=%.0f downWait=%d doing=0x%X frame=%.1f dummyNpc=%d talkOrDemo=%d ground4cm=%d jumping=%d tick=%d\n", tag, e->mPosition.x, e->mPosition.y, e->mPosition.z, std::sqrt(dx * dx + dy * dy + dz * dz),
	    em ? (int)em->isDownWaitingToTalk() : -1, em ? (unsigned)em->mEnemyMario->mEMDoing : 0u, em ? em->mEnemyMario->getMotionFrameCtrl().getFrame() : -1.0f, JDrama::TNameRefGen::search(kDummy) ? 1 : 0, (int)gpMarDirector->isTalkOrDemoModeNow(), (int)SMS_IsMarioTouchGround4cm(), (int)gpMarioOriginal->checkStatusType(MARIO_STATUS_FLAG_JUMPING), gTick);
	if (em) std::printf("[%s-emario-take] status=0x%08X held=%p emflags=0x%X timer=%d waterCt=%d waterHitTimer=%d tick=%d\n", tag, (unsigned)em->mEnemyMario->mStatus, (void*)em->mEnemyMario->mHeldObject, (unsigned)em->mEnemyMario->mEMFlags, (int)em->mEnemyMario->mEMDoingTimer, (int)em->mEnemyMario->mWaterCounter, (int)em->mEnemyMario->mWaterHitTimer, gTick);
	for (int i = 0; em && i < em->getColNum(); ++i)
		std::printf("[%s-emario-col] i=%d type=0x%08X name=%s tick=%d\n", tag, i, (unsigned)em->getCollision(i)->getActorType(), typeid(*em->getCollision(i)).name(), gTick);
	static const char kGateName[] = "Gate";
	if (TModelGate* g = dynamic_cast<TModelGate*>(JDrama::TNameRefGen::search(kGateName))) {
		JGeometry::TVec3<f32> local;
		MTXMultVec(g->mInvCenterMtx, gpMarioPos, &local);
		std::printf("[%s-gate] state=%d flags=0x%X dest=%d openRatio=%.2f openTimer=%d hitMaxZ=%.0f pos=(%.0f,%.0f,%.0f) noCollision=%d marioLocal=(%.0f,%.0f,%.0f) tick=%d\n", tag, (int)g->mState, (unsigned)g->mFlags, (int)g->mDestination, g->mOpenRatio, (int)g->mOpenTimer, g->mHitMaxZ, g->mPosition.x, g->mPosition.y, g->mPosition.z, (int)g->checkHitFlag(HIT_FLAG_NO_COLLISION), local.x, local.y, local.z, gTick);
	}
}

// The airport GateKeeper (scene object "ゲートキーパー（ビアンコ）", Shift-JIS bytes below): hit points, vulnerability window,
// dead flag, position. Killing it starts the airport event (TAirportEventSink::watch) that ends the tutorial.
void logBoss(const char* tag)
{
	static const char kName[] = "\x83\x51\x81\x5b\x83\x67\x83\x4c\x81\x5b\x83\x70\x81\x5b\x81\x69\x83\x72\x83\x41\x83\x93\x83\x52\x81\x6a";
	if (!JDrama::TNameRefGen::getInstance() || !JDrama::TNameRefGen::getInstance()->getRootNameRef()) return;
	TGateKeeperBase* g = dynamic_cast<TGateKeeperBase*>(JDrama::TNameRefGen::search(kName));
	if (!g) return;
	int bck = -1;
	if (TBiancoGateKeeper* b = dynamic_cast<TBiancoGateKeeper*>(g))
		for (int i = 0; i < 0x20; ++i)
			if (b->getMActor()->checkCurBckFromIndex(i)) { bck = i; break; }
	std::printf("[%s-boss] hp=%d vulnerable=%d unk154=%d dead=%d bck=0x%X pos=(%.0f,%.0f,%.0f) tick=%d\n", tag, (int)g->getHitPoints(),
	    (int)g->mVulnerable, g->unk154, g->checkLiveFlag(LIVE_FLAG_DEAD) ? 1 : 0, bck, g->mPosition.x, g->mPosition.y, g->mPosition.z, gTick);
#ifdef TARGET_PC
	// Dev hook: cumulative spray hits (not reset by CUE_MOVE), defined in sms gatekeeper.cpp; only with SUNSHINE_DEV_HIT_LOG=1
	if (std::getenv("SUNSHINE_DEV_HIT_LOG"))
		std::printf("[%s-spray] headHits=%d bodyHits=%d\n", tag, gDbgGKHeadSprayHits, gDbgGKBodySprayHits);
#endif
	// Dev hook: sleep/appear counters (gatekeeper.cpp TNerveBGKSleep: unk17C >= 0xFF -> Appear); only with SUNSHINE_DEV_HIT_LOG=1
	if (std::getenv("SUNSHINE_DEV_HIT_LOG")) {
		if (TBiancoGateKeeper* w = dynamic_cast<TBiancoGateKeeper*>(g))
			std::printf("[%s-wake] unk17C=%d unk17E=%d unk298=%d unk288=%d unk28A=%d\n", tag, (int)w->unk17C, (int)w->unk17E, (int)w->unk298, (int)w->unk288, (int)w->unk28A);
	}
	if (TBiancoGateKeeper* b = dynamic_cast<TBiancoGateKeeper*>(g)) {
		if (b->mHead && gpMarioOriginal && gpCamera) {
			const float dx = b->mHead->mPosition.x - gpMarioOriginal->mPosition.x;
			const float dz = b->mHead->mPosition.z - gpMarioOriginal->mPosition.z;
			// bearing in the game's yaw convention (forward = (sin, cos)); face = Mario's yaw in degrees
			std::printf("[%s-boss]   head vulnerable=%d pos=(%.0f,%.0f,%.0f) dist=%.0f bearing=%.0f marioFace=%.0f\n", tag,
			    (int)b->mHead->mVulnerable, b->mHead->mPosition.x, b->mHead->mPosition.y, b->mHead->mPosition.z,
			    std::sqrt(dx * dx + dz * dz), std::atan2(dx, dz) * 57.29578f, gpMarioOriginal->mFaceAngle.y * (360.0f / 65536.0f));
			// nozzle pitch follows the camera pitch only in the L-button camera (WaterGun.cpp:218-232)
			std::printf("[%s-boss]   cam pitch=%d yaw258=%d lcam=%d nozzle36E=%d marioStatus=0x%08X nozzle=%d hasFludd=%d input=0x%X\n", tag,
			    (int)gpCamera->mCurrentTarget.mPitch, (int)gpCamera->getUnk258(), gpCamera->isLButtonCamera() ? 1 : 0,
			    gpMarioOriginal->mWaterGun ? (int)gpMarioOriginal->mWaterGun->mNozzleList[gpMarioOriginal->mWaterGun->mCurrentNozzle]->unk36E : -1,
			    (unsigned)gpMarioOriginal->mStatus, gpMarioOriginal->mWaterGun ? (int)gpMarioOriginal->mWaterGun->mCurrentNozzle : -1,
			    (int)gpMarioOriginal->checkFlag(MARIO_FLAG_HAS_FLUDD), (unsigned)gpMarioOriginal->mInput);
			// env-gated (SUNSHINE_DEV_HIT_LOG): live nozzle clamp params and the stick Y that the squat branch of calcGunAngle reads
			if (gpMarioOriginal->mWaterGun && gpMarioOriginal->mGamePad) {
				TNozzleBase::TEmitParams& ep = gpMarioOriginal->mWaterGun->getCurrentNozzle()->mEmitParams;
				std::printf("[%s-boss]   nozzle prm lAngleBase=%d lAngleMin=%d lAngleMax=%d lAngleChase=%.3f rButtonMult=%.1f stickY=%.1f\n", tag,
				    (int)ep.mLAngleBase.get(), (int)ep.mLAngleMin.get(), (int)ep.mLAngleMax.get(), ep.mLAngleChase.get(), ep.mRButtonMult.get(), gpMarioOriginal->mGamePad->mCompSPos[1]);
			}
		}
	}
}

void logJpa(const char* tag)
{
	JPAEmitterManager* m3d = gpMarioParticleManager ? gpMarioParticleManager->unk3B8 : nullptr;
	JPAEmitterManager* m2d = gpEmitterManager4D2;
	if (!m3d && !m2d)
		return;
	auto sumMgr = [](JPAEmitterManager* mgr, unsigned& emitters, unsigned& drawn, unsigned& particles, unsigned grpP[4]) {
		emitters = drawn = particles = 0;
		for (int g = 0; g < 8; ++g) {
			unsigned gp = 0;
			JSUList<JPABaseEmitter>* list = &mgr->unk44[g];
			for (JSULink<JPABaseEmitter>* link = list->getFirst(); link != list->getEnd(); link = link->getNext()) {
				JPABaseEmitter* e = link->getObject();
				if (!e) continue;
				++emitters;
				const unsigned n = e->getParticleNumber();
				particles += n;
				if (e->isDraw()) {
					++drawn;
					gp += n;
				}
			}
			if (g < 4) grpP[g] = gp;
		}
	};
	unsigned e3 = 0, d3 = 0, p3 = 0, g3[4] = {};
	unsigned e2 = 0, d2 = 0, p2 = 0, g2[4] = {};
	if (m3d) sumMgr(m3d, e3, d3, p3, g3);
	if (m2d) sumMgr(m2d, e2, d2, p2, g2);
	std::printf("[%s-jpa] mgr3d=(emitters=%u drawn=%u particles=%u g0=%u g1=%u g2=%u g3=%u) mgr2d=(emitters=%u drawn=%u particles=%u) tick=%d\n",
	            tag, e3, d3, p3, g3[0], g3[1], g3[2], g3[3], e2, d2, p2, gTick);
}

void logStatus(const char* tag)
{
	std::printf("[%s-thp] thp=%d tick=%d\n", tag, THPPlayerGetState(), gTick);
	if (!sceneReady()) {
		std::printf("[%s] tick=%d t=%.2fs stage=%u scen=%u (scene not ready)\n", tag, gTick, gTick / 30.0,
		            gpApplication.mCurrArea.getStage(), gpApplication.mCurrArea.getScenario());
		return;
	}
	logWash(tag);
	logEMario(tag);
	logBoss(tag);
	logJpa(tag);
	if (gpCardLoad)
		std::printf("[%s-card] unk10=%d unk14=%d unk1C=%d\n", tag, gpCardLoad->unk10, gpCardLoad->unk14,
		            static_cast<int>(gpCardLoad->unk1C));
	if (!gpMarioOriginal || !gpCamera || !gpMarDirector) {
		std::printf("[%s] tick=%d t=%.2fs stage=%u scen=%u (no mario/camera/director yet)\n", tag, gTick, gTick / 30.0,
		            gpApplication.mCurrArea.getStage(), gpApplication.mCurrArea.getScenario());
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
	std::printf("[%s-cam] target=(%.1f,%.1f,%.1f) up=(%.2f,%.2f,%.2f) fovy=%.1f aspect=%.3f near=%.1f far=%.1f\n", tag,
	    gpCamera->mTarget.x, gpCamera->mTarget.y, gpCamera->mTarget.z, gpCamera->mUp.x, gpCamera->mUp.y,
	    gpCamera->mUp.z, gpCamera->getFovy(), gpCamera->getAspect(), gpCamera->getNear(), gpCamera->getFar());
	const TCameraOption* co = gpCameraOption;
	std::printf("[%s-cam2] optionMap=%d mode=%d pos=(%.1f,%.1f,%.1f) cur.pos=(%.1f,%.1f,%.1f) cur.tgt=(%.1f,%.1f,%.1f) "
	            "opt=%d/%d/%d/%d/%d/%d\n", tag, SMS_isOptionMap() ? 1 : 0, gpCamera->mMode, gpCamera->mPosition.x,
	            gpCamera->mPosition.y, gpCamera->mPosition.z, gpCamera->mCurrentTarget.mPosition.x,
	            gpCamera->mCurrentTarget.mPosition.y, gpCamera->mCurrentTarget.mPosition.z,
	            gpCamera->mCurrentTarget.mTarget.x, gpCamera->mCurrentTarget.mTarget.y, gpCamera->mCurrentTarget.mTarget.z,
	            co ? co->unk0 : -1, co ? co->unkA : -1, co ? co->unkE : -1, co ? co->unk12 : -1, co ? co->unk16 : -1, co ? co->unk8 : -1);
}
// Shine manager: MapObjInit shine_data name, as SJIS escapes (source is UTF-8).
TObjManager* shineManager()
{
	return dynamic_cast<TObjManager*>(JDrama::TNameRefGen::search(
	    "\x83\x56\x83\x83\x83\x43\x83\x93\x83\x7d\x83\x6c\x81\x5b\x83\x57\x83\x83\x81\x5b"));
}

TShine* findShine(int& seen, int index = 0)
{
	TObjManager* mgrs[] = {shineManager(), gpItemManager, gpMapObjManager};
	TShine* shine = nullptr;
	seen = 0;
	for (TObjManager* mgr : mgrs) {
		if (!mgr) continue;
		seen += mgr->getObjNum();
		for (int i = 0; !shine && i < mgr->getObjNum(); ++i) {
			TShine* t = dynamic_cast<TShine*>(mgr->getObj(i));
			if (t && index-- == 0) shine = t;
		}
	}
	return shine;
}

// Dev trigger: runs the game's own collect path (TShine::touchPlayer -> TMario::receiveMessage
// -> WIN_DEMO -> fireGetStar -> setShineFlag). Only the overlap test is skipped.
void doShine(int index)
{
	int seen = 0;
	TShine* shine = findShine(seen, index);
	if (!shine || !gpMarioOriginal) {
		std::printf("[devscript] shine: no TShine among %d managed objects (tick=%d)\n", seen, gTick);
		return;
	}
	std::printf("[devscript] shine[%d]: eventId=%u pos=(%.0f,%.0f,%.0f) shines_before=%d tick=%d\n", index, shine->getEventId(), shine->mPosition.x, shine->mPosition.y, shine->mPosition.z, shineCount(), gTick);
	shine->touchPlayer(gpMarioOriginal);
}

void doTpShine(const float* off)
{
	int seen = 0;
	TShine* shine = findShine(seen);
	if (!shine || !gpMarioOriginal) {
		std::printf("[devscript] tpshine: no TShine among %d managed objects (tick=%d)\n", seen, gTick);
		TObjManager* mgrs[] = {shineManager(), gpItemManager, gpMapObjManager};
		for (int m = 0; m < 3; ++m) {
			TObjManager* mgr = mgrs[m];
			if (!mgr) {
				std::printf("[devscript]   mgr=%d missing\n", m);
				continue;
			}
			for (int i = 0; i < mgr->getObjNum() && i < 400; ++i)
				std::printf("[devscript]   mgr=%d obj=%s\n", m, mgr->getObj(i)->getName());
		}
		return;
	}
	TMario* m = gpMarioOriginal;
	m->mPosition.x = shine->mPosition.x + off[0];
	m->mPosition.y = shine->mPosition.y + off[1];
	m->mPosition.z = shine->mPosition.z + off[2];
	std::printf("[devscript] tpshine: eventId=%u shine=(%.1f,%.1f,%.1f) mario=(%.1f,%.1f,%.1f) shines_before=%d tick=%d\n",
	    shine->getEventId(), shine->mPosition.x, shine->mPosition.y, shine->mPosition.z, m->mPosition.x, m->mPosition.y,
	    m->mPosition.z, shineCount(), gTick);
}

void doHurt(int damage) {
	if (!gpMarioOriginal) {
		std::printf("[devscript] hurt: no mario yet (tick=%d)\n", gTick);
		return;
	}
	const int before = gpMarioOriginal->mHealth;
	gpMarioOriginal->decHP(damage);
	std::printf("[devscript] hurt %d: health %d -> %d tick=%d\n", damage, before, (int)gpMarioOriginal->mHealth, gTick);
}

bool condHolds(const Command& c)
{
	for (const auto& [kind, val] : c.conds) {
		bool ok = false;
		switch (kind) {
		case Command::Cond::Stage: ok = static_cast<int>(gpApplication.mCurrArea.getStage()) == val; break;
		case Command::Cond::Scen: ok = static_cast<int>(gpApplication.mCurrArea.getScenario()) == val; break;
		case Command::Cond::Mario: ok = sceneReady() && gpMarioOriginal != nullptr; break;
		case Command::Cond::Director: ok = sceneReady(); break;
		case Command::Cond::DirState: ok = sceneReady() && static_cast<int>(gpMarDirector->mState) == val; break;
		case Command::Cond::Flag: ok = TFlagManager::smInstance->getFlag(val) != 0; break;
		case Command::Cond::Thp: ok = THPPlayerGetState() == val; break;
		case Command::Cond::CardMode: ok = sceneReady() && gpCardLoad && gpCardLoad->unk14 == val; break;
		case Command::Cond::CardProg: ok = sceneReady() && gpCardLoad && static_cast<int>(gpCardLoad->unk1C) == val; break;
		case Command::Cond::CamDemo: ok = sceneReady() && gpCamera && gpCamera->isDemoCamera() == (val != 0); break;
		case Command::Cond::Washed: ok = sceneReady() && washedCount() >= val; break;
		case Command::Cond::Talk: ok = sceneReady() && gpMarDirector->isTalkModeNow() == (val != 0); break;
		case Command::Cond::Bck: ok = sceneReady() && gDbgGKCurBck == val; break;
		case Command::Cond::MarioYGe: ok = sceneReady() && gpMarioOriginal && gpMarioOriginal->mPosition.y >= val; break;
		case Command::Cond::MarioYLe: ok = sceneReady() && gpMarioOriginal && gpMarioOriginal->mPosition.y <= val; break;
		case Command::Cond::FloorYGe: ok = sceneReady() && gpMarioOriginal && gpMarioOriginal->mFloorPosition.y >= val; break;
		case Command::Cond::Weak: { const TBossPakkun* p = sceneReady() ? findPetey() : nullptr; ok = p && p->mWeakPoint == val; break; }
		case Command::Cond::PeteyHp: { const TBossPakkun* p = sceneReady() ? findPetey() : nullptr; ok = p && p->getHitPoints() == val; break; }
		case Command::Cond::Facing: {
			const TBossPakkun* p = sceneReady() ? findPetey() : nullptr;
			ok = p && gpMarioOriginal
			     && std::fabs(std::remainder(std::atan2(gpMarioOriginal->mPosition.x - p->mPosition.x, gpMarioOriginal->mPosition.z - p->mPosition.z)
			                                     - p->mRotation.y * (3.14159265f / 180.f), 6.2831853f))
			            < val * (3.14159265f / 180.f);
			break;
		}
		default: break;
		}
		if (!ok) return false;
	}
	return true;
}

// Main-stick vector (-127..127) that walks Mario toward (x,z). The stick is camera-relative: up = away from the camera.
// Sunshine world is right-handed, y up; camera forward f (XZ), right = (-fz, fx).
void walkStick(const Command& c, s8& sx, s8& sy, float& dist)
{
	const float dx = c.pos[0] - gpMarioOriginal->mPosition.x;
	const float dz = c.pos[2] - gpMarioOriginal->mPosition.z;
	dist = std::sqrt(dx * dx + dz * dz);
	float fx = gpCamera->mTarget.x - gpCamera->mPosition.x;
	float fz = gpCamera->mTarget.z - gpCamera->mPosition.z;
	const float fl = std::sqrt(fx * fx + fz * fz);
	if (fl < 1.f || dist < 1.f) { sx = sy = 0; return; }
	fx /= fl;
	fz /= fl;
	sx = static_cast<s8>(127.f * (dx * -fz + dz * fx) / dist);
	sy = static_cast<s8>(127.f * (dx * fx + dz * fz) / dist);
}

// Dev aid for `chase`: floor cells (40 units, cx/cz/y as int16) cut from the stage's map.col by tools/nav (SUNSHINE_DEV_NAV=<file>).
// The chase steers along the shortest cell path to the target so Mario does not run off the plaza edge into the water.
constexpr float kNavCell = 40.f;
std::unordered_map<long long, short> gNav;
bool gNavLoaded = false;

long long navKey(int cx, int cz) { return static_cast<long long>(cx + 100000) * 200000LL + (cz + 100000); }
void navTest();

void navLoad()
{
	if (std::getenv("SUNSHINE_DEV_NAV_TEST")) { navTest(); std::exit(0); }
	gNavLoaded = true;
	const char* path = std::getenv("SUNSHINE_DEV_NAV");
	if (!path) {
		std::printf("[devscript] nav: SUNSHINE_DEV_NAV unset\n");
		return;
	}
	FILE* f = std::fopen(path, "rb");
	if (!f) {
		std::printf("[devscript] nav: could not open %s\n", path);
		return;
	}
	short r[3];
	while (std::fread(r, sizeof(short), 3, f) == 3) gNav[navKey(r[0], r[1])] = r[2];
	std::fclose(f);
	std::printf("[devscript] nav: %zu floor cells from %s\n", gNav.size(), path);
}

bool navNearest(float x, float z, int& cx, int& cz)
{
	const int bx = static_cast<int>(std::floor(x / kNavCell)), bz = static_cast<int>(std::floor(z / kNavCell));
	for (int r = 0; r <= 12; ++r)
		for (int dx = -r; dx <= r; ++dx)
			for (int dz = -r; dz <= r; ++dz)
				if (gNav.count(navKey(bx + dx, bz + dz))) { cx = bx + dx; cz = bz + dz; return true; }
	return false;
}

// Cell and all 8 neighbours are floor at about the same height: keeps the path off cliff bases and water edges (the grid has no walls).
bool navSafe(int cx, int cz, short y, const std::vector<THitActor*>& contacts)
{
	const float x = (cx + 0.5f) * kNavCell, z = (cz + 0.5f) * kNavCell;
	for (const THitActor* actor : contacts) {
		if (actor->checkHitFlag(HIT_FLAG_NO_COLLISION) || y > actor->mPosition.y + actor->getDamageHeight()
		    || y + gpMarioOriginal->getAttackHeight() < actor->mPosition.y) continue;
		// The four-cell lookahead needs one player-radius clearance beyond keepDistance's separation circle.
		const float radius = actor->getDamageRadius() + 2.f * gpMarioOriginal->unk15C;
		const float dx = x - actor->mPosition.x, dz = z - actor->mPosition.z;
		if (dx * dx + dz * dz < radius * radius) return false;
	}
	for (int dx = -1; dx <= 1; ++dx)
		for (int dz = -1; dz <= 1; ++dz) {
			auto it = gNav.find(navKey(cx + dx, cz + dz));
			if (it == gNav.end() || std::abs(it->second - y) >= 45) return false;
		}
	return true;
}

// When the nearby target has a safe direct floor corridor, face him rather than a grid corner while spraying.
bool navDirect(float mx, float my, float mz, float tx, float tz, const std::vector<THitActor*>& contacts)
{
	const int steps = std::max(1, static_cast<int>(std::ceil(std::hypot(tx - mx, tz - mz) / (kNavCell * 0.5f))));
	float y = my;
	for (int i = 0; i <= steps; ++i) {
		const float t = static_cast<float>(i) / steps;
		const int cx = static_cast<int>(std::floor((mx + (tx - mx) * t) / kNavCell));
		const int cz = static_cast<int>(std::floor((mz + (tz - mz) * t) / kNavCell));
		auto it = gNav.find(navKey(cx, cz));
		if (it == gNav.end() || std::fabs(it->second - y) >= 45.f || !navSafe(cx, cz, it->second, contacts)) return false;
		y = it->second;
	}
	return true;
}

// Maintained native regression: actual navigation, own floor fixtures, no game objects or input/state writes.
void navTest()
{
	std::unordered_map<long long, short> saved;
	gNav.swap(saved);
	const bool loaded = gNavLoaded;
	for (int x = -2; x <= 10; ++x)
		for (int z = -2; z <= 2; ++z) gNav[navKey(x, z)] = 300;
	const std::vector<THitActor*> contacts;
	const bool floor = navDirect(20.f, 300.f, 20.f, 340.f, 20.f, contacts);
	gNav.erase(navKey(4, 0));
	const bool hole = navDirect(20.f, 300.f, 20.f, 340.f, 20.f, contacts);
	gNav[navKey(4, 0)] = 200;
	const bool drop = navDirect(20.f, 300.f, 20.f, 340.f, 20.f, contacts);
	gNav.swap(saved);
	gNavLoaded = loaded;
	if (!floor || hole || drop) {
		std::fprintf(stderr, "[devscript] nav TEST FAILED floor=%d hole=%d drop=%d\n", floor, hole, drop);
		std::exit(2);
	}
	std::printf("[devscript] nav TEST PASS: clear corridor accepted, missing cell and 100-unit drop rejected; driver grid restored\n");
}

// Point `look` cells ahead of (mx,mz) on the shortest floor path to (tx,tz); false when either end is off the grid or unreachable.
bool navAhead(float mx, float mz, float tx, float tz, int look, const std::vector<THitActor*>& contacts, float& ox, float& oz)
{
	int mcx, mcz, tcx, tcz;
	if (!navNearest(mx, mz, mcx, mcz) || !navNearest(tx, tz, tcx, tcz)) return false;
	std::unordered_map<long long, long long> toward; // cell -> next cell toward the target
	std::deque<std::pair<int, int>> q;
	toward[navKey(tcx, tcz)] = -1;
	q.emplace_back(tcx, tcz);
	while (!q.empty()) {
		const auto [cx, cz] = q.front();
		q.pop_front();
		if (cx == mcx && cz == mcz) break;
		for (int dx = -1; dx <= 1; ++dx)
			for (int dz = -1; dz <= 1; ++dz) {
				const long long k = navKey(cx + dx, cz + dz);
				auto it = gNav.find(k);
				if ((dx || dz) && it != gNav.end() && !toward.count(k) && (navSafe(cx + dx, cz + dz, it->second, contacts) || (cx + dx == mcx && cz + dz == mcz))) {
					toward[k] = navKey(cx, cz);
					q.emplace_back(cx + dx, cz + dz);
				}
			}
	}
	if (!toward.count(navKey(mcx, mcz))) return false;
	int cx = mcx, cz = mcz;
	for (int i = 0; i < look; ++i) {
		const long long n = toward[navKey(cx, cz)];
		if (n < 0) break;
		cx = static_cast<int>(n / 200000LL) - 100000;
		cz = static_cast<int>(n % 200000LL) - 100000;
	}
	ox = (cx + 0.5f) * kNavCell;
	oz = (cz + 0.5f) * kNavCell;
	return true;
}

THitActor* findEMario()
{
	static const char kEMarioName[] = "\x83\x7d\x83\x8a\x83\x49\x83\x82\x83\x68\x83\x4c\x5f\x30";
	if (!JDrama::TNameRefGen::getInstance() || !JDrama::TNameRefGen::getInstance()->getRootNameRef()) return nullptr;
	return dynamic_cast<THitActor*>(JDrama::TNameRefGen::search(kEMarioName));
}

void finish()
{
	if (gDone) return;
	gDone = true;
	PADClearVirtualStatus(0);
	std::printf("[devscript] done at tick=%d\n", gTick);
	// SUNSHINE_DEV_EXIT_ON_END: end the process right after the script (route.py sets it), instead of idling until the wall limit.
	const char* exitOnEnd = std::getenv("SUNSHINE_DEV_EXIT_ON_END");
	if (exitOnEnd != nullptr && *exitOnEnd != '\0' && *exitOnEnd != '0') {
		std::fflush(stdout);
		std::_Exit(0); // no static teardown: it aborts with 0xC0000409 after a script exit
	}
}

void step()
{
	while (gCursor < gScript.size()) {
		const Command& c = gScript[gCursor];
		if (c.navel || c.petey || c.behind > 0.f) {
			const TBossPakkun* p = sceneReady() ? findPetey() : nullptr;
			if (!p || !p->mNavel) {
				std::printf("[devscript] FAIL line=%d: Petey target without Petey in the scene, tick=%d\n", c.line, gTick);
				std::fflush(stdout);
				std::exit(3);
			}
			Command& w = gScript[gCursor];
			const JGeometry::TVec3<f32>& t = c.navel ? p->mNavel->mPosition : p->mPosition;
			w.pos[0] = t.x;
			w.pos[1] = t.y;
			w.pos[2] = t.z;
			if (c.behind > 0.f && gpMarioOriginal) {
				// TNerveBPWait swings at Mario inside the swing radius within 60 deg of the facing, and pivots and vomits at him
				// elsewhere: walk around outside that radius until within 30 deg of his back, then step in.
				float a = (p->mRotation.y + 180.f) * (3.14159265f / 180.f), r = c.behind;
				const float am = std::atan2(gpMarioOriginal->mPosition.x - t.x, gpMarioOriginal->mPosition.z - t.z);
				const float d = std::remainder(a - am, 6.2831853f);
				if (std::fabs(d) > 0.5236f) {
					a = am + (d > 0.f ? 0.7854f : -0.7854f);
					r = p->getBossPakkunParams()->mSLSwingLength.get() + 150.f;
				}
				w.pos[0] = t.x + r * std::sin(a);
				w.pos[2] = t.z + r * std::cos(a);
			}
		}
		if (gGuard > 0.f && sceneReady() && gpMarioOriginal && gpCamera
		    && gpMarioOriginal->mPosition.y - gpMarioOriginal->mFloorPosition.y < 10.f) { // not mid-jump or mid-pound
			static const TSmallEnemy* target = nullptr; // pooled actors: the pointer stays valid after death
			static int since = 0;
			const TSmallEnemy* e = nearestSmallEnemy(gGuard);
			static int reface = 0; // ticks left to turn back toward Petey after a slug was dealt with
			if (target && e != target) {
				std::printf("[devscript] guard: target %s after %d ticks, tick=%d\n", target->checkLiveFlag(LIVE_FLAG_DEAD) ? "dead" : "left", since, gTick);
				target = nullptr;
				reface = 60;
			}
			if (!e && reface > 0) {
				const TBossPakkun* p = findPetey();
				const float err = p ? std::remainder(std::atan2(p->mPosition.x - gpMarioOriginal->mPosition.x, p->mPosition.z - gpMarioOriginal->mPosition.z)
				                                         - static_cast<s16>(gpMarioOriginal->mFaceAngle.y) * (6.2831853f / 65536.f), 6.2831853f)
				                    : 0.f;
				if (p && std::fabs(err) > 0.1f) { // same small stick turn as faceto, so the spray window is not lost to a slug's bearing
					Command t{};
					t.pos[0] = p->mPosition.x;
					t.pos[2] = p->mPosition.z;
					PADStatus pad{};
					float dist = 0.f;
					walkStick(t, pad.stickX, pad.stickY, dist);
					pad.stickX = pad.stickX == 0 ? 0 : static_cast<s8>(pad.stickX * 0.25f + (pad.stickX > 0 ? 20.5f : -20.5f));
					pad.stickY = pad.stickY == 0 ? 0 : static_cast<s8>(pad.stickY * 0.25f + (pad.stickY > 0 ? 20.5f : -20.5f));
					setVirtualPad(0, &pad);
					--reface;
					return;
				}
				reface = 0;
			}
			if (e) {
				if (!target) {
					const JGeometry::TVec3<f32>& m = gpMarioOriginal->mPosition;
					std::printf("[devscript] guard: enemy at (%.0f,%.0f,%.0f) dist=%.0f, line=%d paused tick=%d\n", e->mPosition.x, e->mPosition.y, e->mPosition.z,
					    std::hypot(e->mPosition.x - m.x, e->mPosition.z - m.z), c.line, gTick);
					since = 0;
				}
				target = e;
				++since;
				PADStatus pad{};
				sprayAt(e, pad);
				setVirtualPad(0, &pad);
				return; // the current command resumes once no enemy is near
			}
		}
		if (c.op == Op::Guard) {
			gGuard = c.radius;
			std::printf("[devscript] guard radius=%.0f tick=%d\n", gGuard, gTick);
			++gCursor;
			continue;
		}
		if (c.op == Op::Hold && !c.conds.empty()) {
			// hold <buttons/trig> until <cond> timeout=N: pad held until the condition is true; timeout fails loudly.
			if (!gStarted) {
				gStarted = true;
				gRemaining = 0;
				std::printf("[devscript] hold-until start line=%d tick=%d\n", c.line, gTick);
			}
			if (condHolds(c)) {
				std::printf("[devscript] hold-until met line=%d after=%d tick=%d\n", c.line, gRemaining, gTick);
				const PADStatus neutral{};
				setVirtualPad(0, &neutral);
				gStarted = false;
				++gCursor;
				continue;
			}
			if (gRemaining >= c.maxTicks) {
				std::printf("[devscript] TIMEOUT hold-until line=%d: condition not met after %d ticks, tick=%d\n", c.line, gRemaining, gTick);
				std::fflush(stdout);
				std::exit(3);
			}
			setVirtualPad(0, &c.pad);
			++gRemaining;
			return;
		}
		if (c.op == Op::Wait || c.op == Op::Hold) {
			if (!gStarted) {
				gStarted = true;
				gRemaining = c.frames;
				std::printf("[devscript] %s start line=%d frames=%d tick=%d\n",
				    c.op == Op::Wait ? "wait" : "hold", c.line, c.frames, gTick);
			}
			const PADStatus neutral{};
			if (c.op == Op::Hold && c.spray && gpMarioOriginal) {
				if (TBiancoGateKeeper* b = findBoss()) {
					if (b->mHead) {
						const float dx = b->mHead->mPosition.x - gpMarioOriginal->mPosition.x;
						const float dz = b->mHead->mPosition.z - gpMarioOriginal->mPosition.z;
						gpMarioOriginal->mFaceAngle.y = static_cast<s16>(std::atan2(dx, dz) * (32768.0f / 3.14159265f));
					}
				}
			}
			PADStatus held = c.pad;
			if (c.gateMouth && !peteyMouthTakesWater()) {
				held.triggerRight = 0;
				held.button &= ~PAD_TRIGGER_R;
			}
			setVirtualPad(0, c.op == Op::Hold ? &held : &neutral);
			const TBossPakkun* full = c.gateMouth ? findPetey() : nullptr; // filled mouth: more water is wasted
			if (full && full->unk178 >= full->getBossPakkunParams()->mSLWaterMarkLimit.get())
				gRemaining = 1;
			if (--gRemaining == 0) {
				if (c.gateMouth)
					if (const TBossPakkun* p = findPetey())
						std::printf("[devscript] gate=mouth end line=%d fill=%d/%d water=%d tick=%d\n", c.line, (int)p->unk178,
						    (int)p->getBossPakkunParams()->mSLWaterMarkLimit.get(), gpMarioOriginal ? (int)gpMarioOriginal->mWaterGun->mCurrentWater : -1, gTick);
				gStarted = false;
				++gCursor;
			}
			return;
		}
		if (c.op == Op::FightBoss) {
			// fightboss timeout=N: real-input bot for the airport GateKeeper (dev aid, no direct writes to Mario or the nozzle).
			// Crouch (L) always held. Hit window = boss bck 3/11/F and head hit object y > 700: R spray, stick Y -127 (max up),
			// stick X a small correction toward the head bearing. Outside it R is released and the stick neutral, so the pitch holds.
			static bool fbWindow = false, fbSeen = false;
			static int fbLastUnk = 0, fbHits = 0;
			TBiancoGateKeeper* b = findBoss();
			if (!gStarted) {
				gStarted = true;
				gRemaining = 0;
				fbWindow = fbSeen = false;
				fbHits = 0;
				fbLastUnk = 0;
				std::printf("[devscript] fightboss start line=%d tick=%d\n", c.line, gTick);
			}
			if (b) {
				fbSeen = true;
				const int unk = static_cast<int>(b->unk154); // vulnerable spray hits (TGKHitObj / TGateKeeperBase::receiveMessage)
				if (unk > fbLastUnk) fbHits += unk - fbLastUnk;
				fbLastUnk = unk;
			}
			// The boss object is removed once it is dead; before it is built the bot waits with the pad neutral.
			const bool dead = fbSeen && (!b || b->checkLiveFlag(LIVE_FLAG_DEAD));
			const int hp = b ? static_cast<int>(b->getHitPoints()) : -1;
			int bck = -1;
			if (b && !dead)
				for (int i = 0; i < 0x20; ++i)
					if (b->getMActor()->checkCurBckFromIndex(i)) { bck = i; break; }
			const bool headReady = b && b->mHead;
			const float headY = headReady ? b->mHead->mPosition.y : 0.f;
			const bool win = !dead && headReady && (bck == 0x3 || bck == 0x11 || bck == 0xF) && headY > 700.f;
			if (win != fbWindow) {
				fbWindow = win;
				std::printf("[devscript] fightboss window %s tick=%d bck=0x%X headY=%.0f hp=%d headHits=%d\n", win ? "start" : "end", gTick, bck, headY, hp, fbHits);
			}
			if (dead || gRemaining >= c.maxTicks) {
				std::printf("[devscript] fightboss done hp=%d headHits=%d dead=%d ticks=%d tick=%d\n", hp, fbHits, dead ? 1 : 0, gRemaining, gTick);
				if (!dead) {
					std::printf("[devscript] TIMEOUT fightboss line=%d after %d ticks, tick=%d\n", c.line, gRemaining, gTick);
					std::fflush(stdout);
					std::exit(3);
				}
				const PADStatus neutral{};
				setVirtualPad(0, &neutral);
				gStarted = false;
				++gCursor;
				continue;
			}
			PADStatus pad{};
			if (!headReady) { // boss not built yet
				setVirtualPad(0, &pad);
				++gRemaining;
				return;
			}
			pad.button |= PAD_TRIGGER_L;
			pad.triggerLeft = 255;
			// Asleep (bck 0xA/0x4) a body spray wakes the boss (no head hit needed); spray the body, stick Y neutral.
			const bool asleep = bck == 0xA || bck == 0x4;
			if (win || asleep) {
				pad.button |= PAD_TRIGGER_R;
				pad.triggerRight = 255;
				if (win) {
					pad.stickY = -127;
				} else if (gpMarioOriginal && gpMarioOriginal->mWaterGun) {
					// Asleep: step the nozzle pitch (read back from unk36E) toward the body origin; +127 lowers it, -127 raises it.
					const float dxb = b->mPosition.x - gpMarioOriginal->mPosition.x;
					const float dzb = b->mPosition.z - gpMarioOriginal->mPosition.z;
					const float up = std::atan2(b->mPosition.y - gpMarioOriginal->mPosition.y, std::sqrt(dxb * dxb + dzb * dzb)) * 57.29578f;
					const float target = -up * (65536.0f / 360.0f);
					const float cur = static_cast<float>(gpMarioOriginal->mWaterGun->mNozzleList[gpMarioOriginal->mWaterGun->mCurrentNozzle]->unk36E);
					if (cur - target > 600.f) pad.stickY = -127;
					else if (cur - target < -600.f) pad.stickY = 127;
				}
				if (gpMarioOriginal && gpCamera) {
					Command t = c;
					t.pos[0] = win ? b->mHead->mPosition.x : b->mPosition.x;
					t.pos[2] = win ? b->mHead->mPosition.z : b->mPosition.z;
					s8 sx = 0, sy = 0;
					float dist = 0.f;
					walkStick(t, sx, sy, dist);
					pad.stickX = static_cast<s8>(std::fmax(-40.f, std::fmin(40.f, 0.4f * sx)));
				}
			}
			setVirtualPad(0, &pad);
			++gRemaining;
			return;
		}
		if (c.op == Op::AimBoss) {
			if (!gStarted) {
				gStarted   = true;
				gRemaining = c.frames;
				std::printf("[devscript] aimboss start line=%d frames=%d tick=%d\n", c.line, c.frames, gTick);
			}
			PADStatus pad{};
			pad.triggerRight = 255;
			pad.triggerLeft  = 255; // squat
			pad.button       = PAD_TRIGGER_L;
			TBiancoGateKeeper* b = findBoss();
			if (b && b->mHead && gpMarioOriginal && gpCamera) {
				if (b->checkLiveFlag(LIVE_FLAG_DEAD)) {
					std::printf("[devscript] aimboss boss dead line=%d left=%d tick=%d\n", c.line, gRemaining, gTick);
					const PADStatus neutral{};
					setVirtualPad(0, &neutral);
					gStarted = false;
					++gCursor;
					continue;
				}
				Command t{};
				t.pos[0]            = b->mHead->mPosition.x;
				t.pos[2]            = b->mHead->mPosition.z;
				const float dx      = t.pos[0] - gpMarioOriginal->mPosition.x;
				const float dz      = t.pos[2] - gpMarioOriginal->mPosition.z;
				// Squat aim (WaterGun.cpp:218-244): standing, the nozzle is flat; crouching (L trigger squats in this port, observed in
				// c01) the angle is unk36E + stickY * mRButtonMult, clamped to +-0x2000 (45 degrees). The head hangs 400-570 units above
				// Mario's floor in the awake phases, so pitch up while it is clearly higher; stick X then only turns toward the head.
				const bool headHigh = b->mHead->mPosition.y - gpMarioOriginal->mPosition.y > 150.f;
				const float bearing = std::atan2(dx, dz) * 57.29578f;
				const float face    = static_cast<s16>(gpMarioOriginal->mFaceAngle.y) * (360.0f / 65536.0f);
				float err           = bearing - face;
				while (err > 180.f) err -= 360.f;
				while (err < -180.f) err += 360.f;
				if (std::fabs(err) > 3.f) {
					float dist = 0.f;
					s8 sy      = 0;
					walkStick(t, pad.stickX, sy, dist);
					pad.stickX = static_cast<s8>(pad.stickX * 0.4f);
				}
				pad.stickY = headHigh ? 127 : 0;
			}
			setVirtualPad(0, &pad);
			if (--gRemaining == 0) {
				gStarted = false;
				++gCursor;
			}
			return;
		}
		if (c.op == Op::FaceTo) {
			if (!gStarted) {
				gStarted = true;
				gRemaining = 0;
				std::printf("[devscript] faceto start line=%d to=(%.0f,%.0f) tol=%.0f tick=%d\n", c.line, c.pos[0], c.pos[2], c.tol, gTick);
			}
			PADStatus pad{};
			if (gpMarioOriginal && gpCamera) {
				const float dx      = c.pos[0] - gpMarioOriginal->mPosition.x;
				const float dz      = c.pos[2] - gpMarioOriginal->mPosition.z;
				const float bearing = std::atan2(dx, dz) * 57.29578f; // yaw convention: forward = (sin, cos)
				const float face    = static_cast<s16>(gpMarioOriginal->mFaceAngle.y) * (360.0f / 65536.0f);
				float err           = bearing - face;
				while (err > 180.f) err -= 360.f;
				while (err < -180.f) err += 360.f;
				if (std::fabs(err) <= c.tol) {
					std::printf("[devscript] faceto met line=%d after=%d err=%.1f tick=%d\n", c.line, gRemaining, err, gTick);
					const PADStatus neutral{};
					setVirtualPad(0, &neutral);
					gStarted = false;
					++gCursor;
					continue;
				}
				float dist = 0.f;
				walkStick(c, pad.stickX, pad.stickY, dist);
				// shortcut: calibrated for native PADClamp/Mario deadzones; recalibrate if that input pipeline changes.
				pad.stickX = pad.stickX == 0 ? 0 : static_cast<s8>(pad.stickX * 0.25f + (pad.stickX > 0 ? 20.5f : -20.5f)); // small deflection: turns more than it walks
				pad.stickY = pad.stickY == 0 ? 0 : static_cast<s8>(pad.stickY * 0.25f + (pad.stickY > 0 ? 20.5f : -20.5f));
			}
			if (gRemaining >= c.maxTicks && c.soft) {
				std::printf("[devscript] soft timeout faceto line=%d after %d ticks, tick=%d\n", c.line, gRemaining, gTick);
				const PADStatus neutral{};
				setVirtualPad(0, &neutral);
				gStarted = false;
				++gCursor;
				continue;
			}
			if (gRemaining >= c.maxTicks) {
				std::printf("[devscript] TIMEOUT faceto line=%d after %d ticks, tick=%d\n", c.line, gRemaining, gTick);
				std::fflush(stdout);
				std::exit(3);
			}
			setVirtualPad(0, &pad);
			++gRemaining;
			return;
		}
		if (c.op == Op::Chase) {
			static std::vector<THitActor*> contacts;
			static int stalled = 0;
			static float refX = 0.f, refZ = 0.f;
			static TMarDirector* contactScene = nullptr;
			if (!gStarted) {
				gStarted = true;
				gRemaining = 0;
				contacts.clear();
				stalled = 0;
				refX = gpMarioOriginal ? gpMarioOriginal->mPosition.x : 0.f;
				refZ = gpMarioOriginal ? gpMarioOriginal->mPosition.z : 0.f;
				contactScene = gpMarDirector;
				std::printf("[devscript] chase start line=%d tick=%d\n", c.line, gTick);
			}
			if (gpMarDirector && gpMarDirector->isTalkModeNow()) {
				std::printf("[devscript] chase met line=%d after=%d tick=%d\n", c.line, gRemaining, gTick);
				const PADStatus neutral{};
				setVirtualPad(0, &neutral);
				gStarted = false;
				++gCursor;
				continue;
			}
			if (c.tpBehind) { // Legacy teleport diagnosis (ROADMAP 2026-10-12), never ordinary-input progression evidence.
				PADStatus pad{};
				if (THitActor* e = findEMario(); e && gpMarioOriginal) {
					TEMario* em = dynamic_cast<TEMario*>(e);
					const bool down = em && (em->mEnemyMario->mEMDoing == 0xE || em->mEnemyMario->mEMDoing == 0xF); // PRE_DOWN / DOWN: the talk window opens when Mario stands close
					if (gRemaining % 60 == 0) {
						float vx = em ? em->mEnemyMario->mVel.x : 0.f, vz = em ? em->mEnemyMario->mVel.z : 0.f;
						float len = std::hypot(vx, vz);
						if (len < 5.f) { // standing at a node: use his facing
							const float yaw = (em ? em->mEnemyMario->mFaceAngle.y : 0) * (6.2831853f / 65536.f);
							vx = std::sin(yaw);
							vz = std::cos(yaw);
							len = 1.f;
						}
						vx /= len;
						vz /= len;
						gpMarioOriginal->mPosition.set(e->mPosition.x - (down ? 100.f : 250.f) * vx, e->mPosition.y + 20.f, e->mPosition.z - (down ? 100.f : 250.f) * vz);
						gpMarioOriginal->mVel.set(0.f, 0.f, 0.f);
						gpMarioOriginal->mFaceAngle.y = static_cast<s16>(static_cast<int>(std::atan2(vx, vz) * (65536.0f / 6.2831853f)));
						std::printf("[devscript] chase tp em=(%.0f,%.0f,%.0f) doing=0x%X waterCt=%d mario=(%.0f,%.0f,%.0f) tick=%d\n", e->mPosition.x, e->mPosition.y, e->mPosition.z, em ? (unsigned)em->mEnemyMario->mEMDoing : 0u, em ? (int)em->mEnemyMario->mWaterCounter : -1, gpMarioOriginal->mPosition.x, gpMarioOriginal->mPosition.y, gpMarioOriginal->mPosition.z, gTick);
					}
					if (gRemaining % 60 < 45 && !down) pad.triggerRight = 255;
				}
				if (gRemaining >= c.maxTicks) {
					std::printf("[devscript] TIMEOUT chase line=%d after %d ticks, tick=%d\n", c.line, gRemaining, gTick);
					std::fflush(stdout);
					std::exit(3);
				}
				setVirtualPad(0, &pad);
				++gRemaining;
				return;
			}
			PADStatus pad{};
			if (THitActor* e = findEMario(); e && gpMarioOriginal && gpCamera) {
				if (contactScene != gpMarDirector) { contacts.clear(); contactScene = gpMarDirector; }
				if (std::hypot(gpMarioOriginal->mPosition.x - refX, gpMarioOriginal->mPosition.z - refZ) > kNavCell) {
					refX = gpMarioOriginal->mPosition.x;
					refZ = gpMarioOriginal->mPosition.z;
					stalled = 0;
				} else if (++stalled >= 30 && SMS_IsMarioTouchGround4cm() && !gpMarioOriginal->mWallPlane
				           && gpMarioOriginal->mIntendedMag > 16.f) {
					const float yaw = gpMarioOriginal->mIntendedYaw * (6.2831853f / 65536.f);
					const float sinYaw = std::sin(yaw), cosYaw = std::cos(yaw);
					for (int i = 0; i < gpMarioOriginal->getColNum(); ++i) {
						THitActor* actor = gpMarioOriginal->getCollision(i);
						if (actor == e || actor->checkHitFlag(HIT_FLAG_NO_COLLISION)) continue;
						const float dx = gpMarioOriginal->mPosition.x - actor->mPosition.x;
						const float dz = gpMarioOriginal->mPosition.z - actor->mPosition.z;
						const float px = gpMarioOriginal->mPrevPosition.x - actor->mPosition.x;
						const float pz = gpMarioOriginal->mPrevPosition.z - actor->mPosition.z;
						const float radius = actor->getDamageRadius() + gpMarioOriginal->unk15C;
						// Observe repeated outward separation while the stick pushes inward, not mere trigger overlap.
						if (dx * sinYaw + dz * cosYaw < 0.f && dx * dx + dz * dz <= radius * radius
						    && px * px + pz * pz < dx * dx + dz * dz
						    && std::find(contacts.begin(), contacts.end(), actor) == contacts.end())
							contacts.push_back(actor);
					}
				}
				Command t = c;
				t.pos[0] = e->mPosition.x;
				t.pos[2] = e->mPosition.z;
				float dist = 0.f;
				const float dx = e->mPosition.x - gpMarioOriginal->mPosition.x, dz = e->mPosition.z - gpMarioOriginal->mPosition.z;
				const float direct = std::sqrt(dx * dx + dz * dz);
				if (gpMarioOriginal->mPosition.y < -500.f) { // shallow water near y=0 is walkable; the seabed fall reaches -1300
					std::printf("[devscript] TIMEOUT chase fell line=%d mario=(%.0f,%.0f,%.0f) tick=%d\n", c.line, gpMarioOriginal->mPosition.x, gpMarioOriginal->mPosition.y, gpMarioOriginal->mPosition.z, gTick);
					std::fflush(stdout);
					std::exit(3);
				}
				if (!gNavLoaded) navLoad();
				float wx, wz;
				if (direct > 150.f && !gNav.empty()
				    && !(direct < c.radius && navDirect(gpMarioOriginal->mPosition.x, gpMarioOriginal->mPosition.y, gpMarioOriginal->mPosition.z, e->mPosition.x, e->mPosition.z, contacts))
				    && navAhead(gpMarioOriginal->mPosition.x, gpMarioOriginal->mPosition.z, e->mPosition.x, e->mPosition.z, 4, contacts, wx, wz)) {
					t.pos[0] = wx;
					t.pos[2] = wz;
				}
				walkStick(t, pad.stickX, pad.stickY, dist);
				dist = direct;
				if (dist < c.radius) pad.triggerRight = 255;
				if (gRemaining % 60 == 0)
					std::printf("[devscript] chase em=(%.0f,%.0f,%.0f) mario=(%.0f,%.0f,%.0f) dist=%.0f tick=%d\n", e->mPosition.x, e->mPosition.y, e->mPosition.z, gpMarioOriginal->mPosition.x, gpMarioOriginal->mPosition.y, gpMarioOriginal->mPosition.z, dist, gTick);
			}
			if (gRemaining >= c.maxTicks) {
				std::printf("[devscript] TIMEOUT chase line=%d after %d ticks, tick=%d\n", c.line, gRemaining, gTick);
				std::fflush(stdout);
				std::exit(3);
			}
			setVirtualPad(0, &pad);
			++gRemaining;
			return;
		}
		if (c.op == Op::Goto) {
			if (condHolds(c)) {
				std::printf("[devscript] goto %s passed line=%d tick=%d\n", c.path.c_str(), c.line, gTick);
				++gCursor;
				continue;
			}
			if (++gScript[gCursor].taken > c.maxTicks) {
				std::printf("[devscript] FAIL goto %s line=%d: condition still unmet after %d retries, tick=%d\n", c.path.c_str(), c.line, c.maxTicks, gTick);
				std::fflush(stdout);
				std::exit(3);
			}
			const TBossPakkun* p = findPetey();
			std::printf("[devscript] goto %s retry=%d line=%d petey hp=%d weak=%d health=%d tick=%d\n", c.path.c_str(), gScript[gCursor].taken, c.line,
			    p ? p->getHitPoints() : -1, p ? p->mWeakPoint : -1, gpMarioOriginal ? gpMarioOriginal->mHealth : -1, gTick);
			gCursor = static_cast<size_t>(c.frames);
			continue;
		}
		if (c.op == Op::Clear) {
			if (!gStarted) {
				gStarted = true;
				gRemaining = 0;
				std::printf("[devscript] clear start line=%d radius=%.0f tick=%d\n", c.line, c.radius, gTick);
			}
			const TSmallEnemy* e = sceneReady() ? nearestSmallEnemy(c.radius) : nullptr;
			if ((c.conds.empty() ? !e : condHolds(c)) || gRemaining >= c.maxTicks) {
				std::printf("[devscript] clear done line=%d left=%d after=%d tick=%d\n", c.line, e ? 1 : 0, gRemaining, gTick);
				const PADStatus neutral{};
				setVirtualPad(0, &neutral);
				gStarted = false;
				++gCursor;
				continue;
			}
			PADStatus pad{};
			if (e && gpCamera) sprayAt(e, pad);
			setVirtualPad(0, &pad);
			++gRemaining;
			return;
		}
		if (c.op == Op::WalkTo) {
			static int walkStall = 0, walkRecover = 0, walkSide = 1;
			static float walkRefX = 0.f, walkRefZ = 0.f;
			if (!gStarted) {
				gStarted = true;
				gRemaining = 0;
				std::printf("[devscript] walkto start line=%d to=(%.0f,%.0f) radius=%.0f tick=%d\n", c.line, c.pos[0], c.pos[2], c.radius, gTick);
				if (c.navel) std::printf("[devscript] navel y=%.0f weak=%d hp=%d mario=(%.0f,%.0f,%.0f)\n", c.pos[1], findPetey()->mWeakPoint, findPetey()->getHitPoints(),gpMarioOriginal ? gpMarioOriginal->mPosition.x : 0.f, gpMarioOriginal ? gpMarioOriginal->mPosition.y : 0.f, gpMarioOriginal ? gpMarioOriginal->mPosition.z : 0.f);
				walkStall = 0;
				walkRecover = 0;
				walkRefX = gpMarioOriginal ? gpMarioOriginal->mPosition.x : 0.f;
				walkRefZ = gpMarioOriginal ? gpMarioOriginal->mPosition.z : 0.f;
			}
			PADStatus pad{};
			float dist = 0.f;
			if (gpMarioOriginal && gpCamera) {
				walkStick(c, pad.stickX, pad.stickY, dist);
				if (c.hover && gpMarioOriginal->mStatus == MARIO_STATUS_ROCKET && dist > c.radius) {
					// Hover turns only while the stick is 30..150 deg off the facing and thrusts straight inside 30 deg (MarioJump.cpp rocketing):
					// push 45 deg off the facing until it points at the target, so the flight follows the line instead of drifting up to 30 deg.
					const float face = gpMarioOriginal->mFaceAngle.y * (6.2831853f / 65536.f);
					const float d = std::remainder(std::atan2(c.pos[0] - gpMarioOriginal->mPosition.x, c.pos[2] - gpMarioOriginal->mPosition.z) - face, 6.2831853f);
					if (std::fabs(d) > 0.05f && std::fabs(d) < 0.5236f) {
						Command t = c;
						const float a = face + (d > 0.f ? 0.7854f : -0.7854f);
						t.pos[0] = gpMarioOriginal->mPosition.x + 100.f * std::sin(a);
						t.pos[2] = gpMarioOriginal->mPosition.z + 100.f * std::cos(a);
						float unused;
						walkStick(t, pad.stickX, pad.stickY, unused);
					}
				}
				if (c.spray && gRemaining >= c.sprayFrom) pad.triggerRight = 255;
				if (TWaterGun* wg = c.hover ? gpMarioOriginal->mWaterGun : nullptr) { // a hover burst ends after its trigger time (DEAD) and re-arms only once R is let go
					TNozzleBase* nz = wg->getCurrentNozzle();
					if (nz && nz->getNozzleKind() == 1 && static_cast<TNozzleTrigger*>(nz)->unk385 == TNozzleTrigger::DEAD) pad.triggerRight = 0;
				}
				if (gRemaining < c.frames) pad.button |= PAD_BUTTON_A;
				// Stall recovery (dev aid): Mario standing still while the stick pushes (seen once in a real-time run at (3692,704,-2716), cause not found)
				// gets 40 ticks of sideways steering plus a jump, alternating sides. A wall that really blocks the line still ends in the walkto TIMEOUT.
				if (dist > c.radius) {
					const float mx = gpMarioOriginal->mPosition.x, mz = gpMarioOriginal->mPosition.z;
					if (std::hypot(mx - walkRefX, mz - walkRefZ) > 30.f) { // a run covers far more; a crawl along a wall (0.12 units/tick seen) does not
						walkRefX = mx;
						walkRefZ = mz;
						walkStall = 0;
					} else if (walkRecover == 0 && ++walkStall >= 120) {
						walkRecover = 40;
						walkSide = -walkSide;
						walkStall = 0;
						std::printf("[devscript] walkto stall line=%d mario=(%.0f,%.0f,%.0f) dist=%.0f side=%d tick=%d\n", c.line, mx, gpMarioOriginal->mPosition.y, mz, dist, walkSide, gTick);
					}
					if (walkRecover > 0) {
						const s8 sx = pad.stickX, sy = pad.stickY;
						pad.stickX = static_cast<s8>(-walkSide * sy);
						pad.stickY = static_cast<s8>(walkSide * sx);
						if (walkRecover > 36) pad.button |= PAD_BUTTON_A;
						--walkRecover;
					}
				}
				if (dist <= c.radius) {
					std::printf("[devscript] walkto met line=%d after=%d dist=%.0f tick=%d\n", c.line, gRemaining, dist, gTick);
					const PADStatus neutral{};
					setVirtualPad(0, &neutral);
					gStarted = false;
					++gCursor;
					continue;
				}
			}
			if (gRemaining >= c.maxTicks && c.soft) {
				std::printf("[devscript] soft timeout walkto line=%d dist=%.0f tick=%d\n", c.line, dist, gTick);
				const PADStatus neutral{};
				setVirtualPad(0, &neutral);
				gStarted = false;
				++gCursor;
				continue;
			}
			if (gRemaining >= c.maxTicks) {
				std::printf("[devscript] TIMEOUT walkto line=%d: dist=%.0f after %d ticks, tick=%d\n", c.line, dist, gRemaining, gTick);
				std::fflush(stdout);
				std::exit(3);
			}
			setVirtualPad(0, &pad);
			++gRemaining;
			return;
		}
		if (c.op == Op::WaitFor || c.op == Op::Mash) {
			const char* name = c.op == Op::WaitFor ? "waitfor" : "mash";
			if (!gStarted) {
				gStarted = true;
				gRemaining = 0; // ticks spent waiting
				std::printf("[devscript] %s start line=%d tick=%d\n", name, c.line, gTick);
			}
			const PADStatus neutral{};
			if (condHolds(c)) {
				std::printf("[devscript] %s met line=%d after=%d tick=%d\n", name, c.line, gRemaining, gTick);
				setVirtualPad(0, &neutral);
				gStarted = false;
				++gCursor;
				continue;
			}
			if (gRemaining >= c.maxTicks && c.soft) {
				std::printf("[devscript] soft timeout %s line=%d tick=%d\n", name, c.line, gTick);
				setVirtualPad(0, &neutral);
				gStarted = false;
				++gCursor;
				continue;
			}
			if (gRemaining >= c.maxTicks) {
				std::printf("[devscript] TIMEOUT %s line=%d: condition not met after %d ticks, tick=%d\n", name, c.line, gRemaining, gTick);
				std::fflush(stdout);
				std::exit(3);
			}
			const bool press = c.op == Op::Mash && gRemaining % c.period < (c.frames > 0 ? c.frames : 2);
			if (press) std::printf("[devscript] mash press line=%d tick=%d button=0x%04x\n", c.line, gTick, c.pad.button);
			setVirtualPad(0, press ? &c.pad : &neutral);
			++gRemaining;
			return;
		}
		if (c.op == Op::Shine) doShine(c.frames);
		if (c.op == Op::TpShine) doTpShine(c.pos);
		if (c.op == Op::Hurt) doHurt(c.frames);
		if (c.op == Op::TpEm && gpMarioOriginal && JDrama::TNameRefGen::getInstance() && JDrama::TNameRefGen::getInstance()->getRootNameRef()) {
			static const char kEMarioName[] = "\x83\x7d\x83\x8a\x83\x49\x83\x82\x83\x68\x83\x4c\x5f\x30";
			if (THitActor* e = dynamic_cast<THitActor*>(JDrama::TNameRefGen::search(kEMarioName))) {
				gpMarioOriginal->mPosition.set(e->mPosition.x, e->mPosition.y + 20.f, e->mPosition.z + c.pos[2]);
				gpMarioOriginal->mVel.set(0.f, 0.f, 0.f);
				gpMarioOriginal->mFaceAngle.y = static_cast<s16>(180.0f * (65536.0f / 360.0f));
				std::printf("[devscript] tpem: mario=(%.0f,%.0f,%.0f) tick=%d\n", gpMarioOriginal->mPosition.x, gpMarioOriginal->mPosition.y, gpMarioOriginal->mPosition.z, gTick);
			}
		}
		if (c.op == Op::Tp && gpMarioOriginal) {
			gpMarioOriginal->mPosition.set(c.pos[0], c.pos[1], c.pos[2]);
			gpMarioOriginal->mVel.set(0.f, 0.f, 0.f);
			if (c.spray) gpMarioOriginal->mFaceAngle.y = static_cast<s16>(c.tol * (65536.0f / 360.0f));
			std::printf("[devscript] tp: mario=(%.0f,%.0f,%.0f) tick=%d\n", c.pos[0], c.pos[1], c.pos[2], gTick);
		}
		if (c.op == Op::Cheat)
			std::printf("[devscript] cheat %s %s: %s tick=%d\n", c.path.c_str(), c.frames ? "on" : "off",
			    cheatSet(c.path.c_str(), c.frames != 0) ? "ok" : "unknown id", gTick);
		if (c.op == Op::KillBoss) {
			if (TBiancoGateKeeper* b = findBoss()) {
				b->kill();
				std::printf("[devscript] killboss: boss killed tick=%d\n", gTick);
			} else {
				std::printf("[devscript] killboss: no boss tick=%d\n", gTick);
			}
		}
		if (c.op == Op::Shot) {
			logStatus("shot");
			aurora_screenshot_next(c.path.c_str());
		}
		if (c.op == Op::Log) logStatus("devscript");
		if (c.op == Op::Trace) gTrace = c.frames;
		if (c.op == Op::DiscErr) {
			// SUNSHINE_DEV_DISC_ERR=1 (dev-only): close the disc here, so DVDGetDriveStatus() reports no disc and
			// TApplication::drawDVDErr shows its error screen (sms Application.cpp)
			if (std::getenv("SUNSHINE_DEV_DISC_ERR")) {
				aurora_dvd_close();
				std::printf("[devscript] discerr: disc closed tick=%d\n", gTick);
			} else {
				std::printf("[devscript] discerr: skipped, needs SUNSHINE_DEV_DISC_ERR=1 tick=%d\n", gTick);
			}
		}
		if (c.op == Op::Menu) {
			std::printf("[devscript] menu %s tick=%d\n", c.path.c_str(), gTick);
			launcher::menuDevQueue(c.path.c_str());
		}
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


// Called once per logic pad read: JUTGamePad::read() (sms/src/JSystem/JUtility/JUTGamePad.cpp), before PADRead. One step
// per read, so the script clock is the game's logic clock: independent of wall time, FPS cap and replay presents.
// Steps apply via PADSetVirtualStatus before that read.
// SUNSHINE_DEV_AIM_ASSIST=1 (dev-only, never on by default): while Mario crouches (MARIO_STATUS_SQUAT) and a GateKeeper boss exists,
// face its head and set the nozzle pitch to the elevation of the head plus a 10 degree lead, clamped to 0..64 degrees (the ballistic
// model of an earlier version was wrong: its apex came from runs with the nozzle tilted down).
void aimAssist()
{
	static const bool enabled = std::getenv("SUNSHINE_DEV_AIM_ASSIST") != nullptr;
	gAssistNoSpray = false;
	if (!enabled || !sceneReady() || !gpMarioOriginal || !gpMarioOriginal->mWaterGun) return; // the director is gone while a stage is torn down
	// The slippery goop floor turns every crouch into OIL_RUN again; while L is held and Mario stands still on it, keep him crouching.
	const u32 st = gpMarioOriginal->mStatus;
	const TMarioGamePad* pad = gpApplication.mGamePads[0];
	if ((st == MARIO_STATUS_OIL_RUN || st == MARIO_STATUS_OIL_SLIP) && pad && (pad->getButton() & 0x40) // digital L (the L meaning drops after demos)
	    && std::fabs(gpMarioOriginal->mVel.x) < 1.0f && std::fabs(gpMarioOriginal->mVel.z) < 1.0f)
		gpMarioOriginal->changePlayerStatus(MARIO_STATUS_SQUAT, 0, true);
	if (gpMarioOriginal->mStatus != MARIO_STATUS_SQUAT) return;
	TBiancoGateKeeper* b = findBoss();
	if (!b || !b->mHead) return;
	// Damage needs a hit while bck 0x12 is between frames 50 and 160 (TBiancoGateKeeper::isHeadHitActive, same in the retail asm); a hit
	// earlier in 0x12 or in 0xB/7/0xD only sends the boss to bck 7. Water is allowed while it sleeps (bck A/4: body hits wake it) and
	// in 0x12 frames 35..145 (an earlier emit window than 50..160 because the drops need time to arrive); cut everywhere else.
	if (MActor* m = b->getMActor()) {
		const bool sleeping = m->checkCurBckFromIndex(0xA) || m->checkCurBckFromIndex(4);
		const float fr = m->getFrameCtrl(ANM_TYPE_BCK)->getFrame();
		const bool window = m->checkCurBckFromIndex(0x12) && fr > 35.0f && fr < 145.0f;
		gAssistNoSpray = !sleeping && !window;
	}
	const float dx = b->mHead->mPosition.x - gpMarioOriginal->mPosition.x;
	const float dz = b->mHead->mPosition.z - gpMarioOriginal->mPosition.z;
	const float dy = b->mHead->mPosition.y - (gpMarioOriginal->mPosition.y + 60.0f);
	const float dist = std::sqrt(dx * dx + dz * dz);
	gpMarioOriginal->mFaceAngle.y = static_cast<s16>(std::atan2(dx, dz) * (32768.0f / 3.14159265f));
	float best = std::atan2(dy, dist) * 57.29578f + 10.0f; // aim at the head plus a 10 degree lead (the jet drops)
	if (best < 0.0f) best = 0.0f;
	if (best > 64.0f) best = 64.0f;
	gpMarioOriginal->mWaterGun->mNozzleList[gpMarioOriginal->mWaterGun->mCurrentNozzle]->unk36E = static_cast<s16>(-best * (65536.0f / 360.0f));
}

// SUNSHINE_DEV_PAD_TRACE=1 (dev-only): one line per change of port 0's buttons, sticks, triggers or err (JUTGamePad::mPadStatus[0],
// filled by PADRead and PADClamp). JUTGamePad::read() calls sunshineDevScriptTick() before PADRead, so the status printed here is
// the one the previous read delivered: read=N is that read's index (gTick - 1). Hooked here, so sms/ and Aurora stay untouched.
static void padTrace()
{
	static const bool enabled = std::getenv("SUNSHINE_DEV_PAD_TRACE") != nullptr;
	static PADStatus last{};
	if (!enabled) return;
	const PADStatus& s = JUTGamePad::mPadStatus[0];
	if (s.button == last.button && s.stickX == last.stickX && s.stickY == last.stickY && s.substickX == last.substickX &&
	    s.substickY == last.substickY && s.triggerLeft == last.triggerLeft && s.triggerRight == last.triggerRight && s.err == last.err)
		return;
	last = s;
	std::printf("[padtrace] read=%d buttons=0x%04x stick=%d,%d sub=%d,%d trig=%d,%d err=%d\n", gTick - 1, s.button, s.stickX,
	            s.stickY, s.substickX, s.substickY, s.triggerLeft, s.triggerRight, s.err);
	std::fflush(stdout);
}

void sunshineDevScriptTick()
{
	++gTick;
	padTrace();
	aimAssist();
	static const char* const scriptPath = std::getenv("SUNSHINE_DEV_SCRIPT");
	if (scriptPath && !gLoaded) {
		gLoaded = true;
		loadScript(scriptPath);
		navLoad(); // eager: routes without `chase` never reached the lazy call
	}
	if (gLoaded && !gDone) step();
	if (gTrace > 0 && sceneReady() && gpMarioOriginal) {
		--gTrace;
		const TMario* m = gpMarioOriginal;
		int trig = -1, trigT = 0;
		if (m->mWaterGun) {
			TNozzleBase* nz = m->mWaterGun->getCurrentNozzle();
			if (nz && nz->getNozzleKind() == 1) {
				trig = static_cast<TNozzleTrigger*>(nz)->unk385;
				trigT = static_cast<TNozzleTrigger*>(nz)->unk386;
			}
		}
		std::printf("[trace] tick=%d pos=(%.0f,%.1f,%.0f) vel=(%.1f,%.1f,%.1f) fwd=%.1f face=%.1f status=0x%08X upper=%d trig=%d/%d water=%d floor=%.0f\n", gTick,
		    m->mPosition.x, m->mPosition.y, m->mPosition.z, m->mVel.x, m->mVel.y, m->mVel.z, m->mForwardVel,
		    static_cast<s16>(m->mFaceAngle.y) * (360.0f / 65536.0f), (unsigned)m->mStatus, (int)m->mUpperState, trig, trigT,
		    m->mWaterGun ? (int)m->mWaterGun->mCurrentWater : -1, m->mFloorPosition.y);
	}
	if (std::getenv("SUNSHINE_DEV_STATUS") && gTick % 30 == 0) {
		logStatus("status");
		std::printf("[padclock] padreads=%d logic=%u\n", gTick, sunshineLogicTickCount());
	}
	if (gLoaded || std::getenv("SUNSHINE_DEV_STATUS")) std::fflush(stdout); // harness kills the game; keep lines
}
