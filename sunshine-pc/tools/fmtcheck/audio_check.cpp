// Offline check of the JAudio disc-format readers on the host (see audio_check.py, which builds and drives this).
// Links the real decomp readers from sms_game.lib (private build tree) and prints what the host structures contain
// after loading real disc bytes, one text line per fact; the Python side parses the same bytes independently with
// struct.unpack('>...') and diffs the two.
//
// Modes (argv[1]):
//   aaf  <mSound.aaf>              JAIBasic::checkInitDataOnMemory + sound info table + scene/fx tables, stream list
//   bnk  <mSound.aaf>              BNKParser::createBasicBank on every bank of the AAF
//   wsys <mSound.aaf>              WSParser::createBasicWaveBank / createSimpleWaveBank on every wave bank of the AAF
//   barc <mSound.aaf> <seq.arc>    Vload::initHeaderM on the AAF's sequence archive header (+ again on the same copy)
//   bas  <list.txt>                JAIAnimeSound::initActorAnimSound (twice) on every .bas listed
//   osc  <list.txt>                HostFmt::oscTableToHost on osc tables listed as "<file> <offset>" (twice)
//   seq  <file> <offset>...        TSeqCtrl get16/get24/get32/read16/read24/read32 on real sequence bytes
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <string>
#include <typeinfo>
#include <vector>
#include <malloc.h>

#define protected public
#define private public
#include <JSystem/JAudio/JAInterface/JAIAnimation.hpp>
#include <JSystem/JAudio/JAInterface/JAIBasic.hpp>
#include <JSystem/JAudio/JAInterface/JAIData.hpp>
#include <JSystem/JAudio/JAInterface/JAIGlobalParameter.hpp>
#include <JSystem/JAudio/JASystem/JASBNKParser.hpp>
#include <JSystem/JAudio/JASystem/JASBasicBank.hpp>
#include <JSystem/JAudio/JASystem/JASBasicInst.hpp>
#include <JSystem/JAudio/JASystem/JASBasicWaveBank.hpp>
#include <JSystem/JAudio/JASystem/JASDrumSet.hpp>
#include <JSystem/JAudio/JASystem/JASHostFmt.hpp>
#include <JSystem/JAudio/JASystem/JASInstRand.hpp>
#include <JSystem/JAudio/JASystem/JASInstSense.hpp>
#include <JSystem/JAudio/JASystem/JASSeqCtrl.hpp>
#include <JSystem/JAudio/JASystem/JASSimpleWaveBank.hpp>
#include <JSystem/JAudio/JASystem/JASSystemHeap.hpp>
#include <JSystem/JAudio/JASystem/JASVload.hpp>
#include <JSystem/JAudio/JASystem/JASWSParser.hpp>
#include <JSystem/JAudio/JASystem/JASWaveBankMgr.hpp>
#include <JSystem/JKernel/JKRExpHeap.hpp>
#include <JSystem/JAudio/JASystem/JASBankMgr.hpp>
#include <JSystem/JAudio/JASystem/JASPlayer_impl.hpp>
#include <JSystem/JAudio/JASystem/JASRegisterParam.hpp>
#include <JSystem/JAudio/JASystem/JASTrack.hpp>
#include <JSystem/JAudio/JASystem/JASTrackMgr.hpp>
#include <JSystem/JAudio/JASystem/JASChGlobal.hpp>
#undef private
#undef protected

// The OS services (mutexes, interrupts, OSReport) come from sms_game.lib (sdk/os_thread.cpp, sdk/os_misc.cpp); the
// data cache calls come from Aurora in the game and are plain memory operations here.
void DCZeroRange(void* addr, u32 nBytes) { std::memset(addr, 0, nBytes); }
void DCFlushRange(void*, u32) { }
void DCStoreRange(void*, u32) { }
void DCInvalidateRange(void*, u32) { }
#undef OSPhysicalToCached
// sdk/jkr_new_delete.cpp (global operator delete) asks whether a pointer lies in the emulated MEM1 window; the harness
// has none (its heap is a plain malloc block), so every pointer belongs to the CRT.
void* OSPhysicalToCached(u32) { return nullptr; }

using namespace JASystem;

static std::vector<u8> readFile(const char* path)
{
	FILE* f = std::fopen(path, "rb");
	if (!f) {
		std::fprintf(stderr, "cannot open %s\n", path);
		std::exit(2);
	}
	std::vector<u8> v;
	u8 buf[65536];
	size_t n;
	while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
		v.insert(v.end(), buf, buf + n);
	std::fclose(f);
	return v;
}

// A 32-byte aligned copy that stays alive (the loaders keep pointers into it).
static u8* keep(const std::vector<u8>& v)
{
	u8* p = (u8*)_aligned_malloc(v.size() + 64, 32);
	std::memset(p, 0, v.size() + 64);
	std::memcpy(p, v.data(), v.size());
	return p;
}

static u32 fb(f32 f)
{
	u32 u;
	std::memcpy(&u, &f, 4);
	return u;
}

static u32 rd32(const u8* p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }

static void setupHeap()
{
	static bool done = false;
	if (done)
		return;
	done        = true;
	const u32 n = 96u << 20;
	u8* arena   = (u8*)_aligned_malloc(n, 32);
	const u32 hdr = (sizeof(JKRExpHeap) + 0xF) & ~0xFu; // as JKRExpHeap::create
	auto* root   = new (arena) JKRExpHeap(arena + hdr, n - hdr, nullptr, false);
	JKRHeap::sRootHeap = root;
	JASDram            = root;
	JAIGlobalParameter::interfaceHeapSize = 0; // JAIBasic::allocHeap -> JASDram
}

// The AAF as MSound hands it over: a heap copy, 32-byte aligned.
struct Aaf {
	u8* data;
	size_t size;
	struct Cmd {
		u32 cmd, ofs, size, arg;
	};
	std::vector<Cmd> cmds; // for the bank/wave lists: one entry per record
	explicit Aaf(const char* path)
	{
		auto v = readFile(path);
		size   = v.size();
		data   = keep(v);
		u32 i  = 0;
		for (;;) {
			u32 c = rd32(data + i * 4);
			++i;
			if (c == 0)
				break;
			if (c == 2 || c == 3) {
				while (rd32(data + i * 4) != 0) {
					cmds.push_back({ c, rd32(data + i * 4), rd32(data + i * 4 + 4), rd32(data + i * 4 + 8) });
					i += 3;
				}
				++i;
			} else {
				cmds.push_back({ c, rd32(data + i * 4), rd32(data + i * 4 + 4), rd32(data + i * 4 + 8) });
				i += 3;
			}
		}
	}
};

// ------------------------------------------------------------------------------------------------------- aaf
static int modeAaf(const char* path)
{
	setupHeap();
	Aaf aaf(path);
	alignas(16) static u8 storage[sizeof(JAIBasic)];
	JAIBasic* basic = new (storage) JAIBasic();
	basic->unk0     = (JAIData*)JASystem::Kernel::allocFromSysDram(sizeof(JAIData));
	basic->unk0->init();
	basic->unk0->unk1F4 = basic;
	basic->mInitDataPointer = aaf.data;
	basic->checkInitDataOnMemory();

	JAIData* d = basic->unk0;
	d->initInfoDataWork(&d->mSeTable, (char*)"unused"); // mData is set: only the header parse runs
	JAISoundTable& t = d->mSeTable;
	std::printf("SE size=%u separate=%u fmt=%u,%u,%u,%u unk0=%u catMax=%u\n", t.mDataSize, (u32)d->mSeparateSoundTables,
	            t.mData[0], t.mData[1], t.mData[2], t.mData[3], (u32)t.unk0, (u32)t.mCategoryMax);
	std::printf("SE count=%u\n", (u32)(t.mData[4] | t.mData[5] << 8)); // host-order u16 at 4
	for (int c = 0; c < 18; ++c)
		std::printf("CAT %d max=%u first=%u\n", c, (u32)t.mSoundMax[c],
		            (u32)(t.mCategorySoundInfos[c] - (JAISoundInfo*)(t.mData + 0x50)));
	for (u32 off = 0x50; off + 0x10 <= t.mDataSize; off += 0x10) {
		JAISoundInfo* e = (JAISoundInfo*)(t.mData + off);
		std::printf("INFO %u sw=%08x pri=%u trk=%u off=%u pitch=%08x vol=%u fx=%u\n", (off - 0x50) / 0x10, e->mSwBit,
		            (u32)e->mPriority, (u32)e->mTrackNumber, (u32)e->mOffsetNo, fb(e->mPitch), (u32)e->mVolume,
		            (u32)e->mFxmix);
	}
	// the lookup the game uses
	for (int c = 0; c < 18; ++c) {
		if (!t.mSoundMax[c])
			continue;
		u32 id = c < 16 ? ((u32)c << 12) : (c == 16 ? 0x80000000u : 0xC0000000u);
		void* p = nullptr;
		d->getInfoPointer(id, &p);
		std::printf("LOOK cat=%d id=%08x idx=%u\n", c, id, (u32)((JAISoundInfo*)p - (JAISoundInfo*)(t.mData + 0x50)));
	}

	for (int i = 0; basic->mBankList[i].mBankData; ++i)
		std::printf("BANKREC %d ofs=%u size=%u wave=%d\n", i, (u32)((u8*)basic->mBankList[i].mBankData - aaf.data),
		            *(u32*)basic->mBankList[i].unk4, basic->mBankList[i].mWaveBankNumber);
	for (int i = 0; basic->mWaveBankList[i].mWaveBankData; ++i)
		std::printf("WAVEREC %d ofs=%u size=%u timing=%u\n", i,
		            (u32)((u8*)basic->mWaveBankList[i].mWaveBankData - aaf.data), basic->mWaveBankList[i].unk4,
		            basic->mWaveBankList[i].mLoadTiming);
	std::printf("SEQARC size=%u\n", basic->mSeqArchiveHeader->mSize);
	std::printf("STREAM size=%u\n", basic->mStreamListHeader->mSize);
	for (u32 off = 0; off < basic->mStreamListHeader->mSize; off += 4)
		std::printf("S%02x=%02x%02x%02x%02x\n", off, ((u8*)d->mStreamList)[off], ((u8*)d->mStreamList)[off + 1],
		            ((u8*)d->mStreamList)[off + 2], ((u8*)d->mStreamList)[off + 3]);
	std::printf("STREAMNAME %s\n", d->mStreamList[0].mFileName);
	std::printf("SCENES max=%u\n", JAIGlobalParameter::soundSceneMax);
	for (u32 s = 0; s < JAIGlobalParameter::soundSceneMax; ++s) {
		std::printf("SCENE %u", s);
		for (int c = 0; c < 9; ++c)
			std::printf(" %u,%u", (u32)basic->mSoundSceneList[s][c].mMaxPlaying,
			            (u32)basic->mSoundSceneList[s][c].mMaxSameSound);
		std::printf("\n");
	}
	auto* fx = basic->mFxSceneTable;
	std::printf("FX max=%u buf=%u,%u,%u,%u\n", fx->mSceneMax, fx->mBufferMax[0], fx->mBufferMax[1], fx->mBufferMax[2],
	            fx->mBufferMax[3]);
	for (u32 s = 0; s < fx->mSceneMax; ++s) {
		auto* cfg = (JASystem::DSPInterface::FxlineConfig_*)((u8*)fx + fx->mSceneOffset[s]);
		for (int l = 0; l < 4; ++l) {
			std::printf("FXL %u %d u0=%u u2=%u u4=%d u6=%u u8=%d uC=%d f=", s, l, (u32)cfg[l].unk0, (u32)cfg[l].unk2,
			            (int)cfg[l].unk4, (u32)cfg[l].unk6, (int)cfg[l].unk8, cfg[l].unkC);
			for (int k = 0; k < 8; ++k)
				std::printf("%d%s", (int)cfg[l].unk10[k], k < 7 ? "," : "\n");
		}
	}
	std::printf("WAVEGROUPS %d\n", basic->mWaveGroupNumber ? 1 : 0);
	return 0;
}

// ------------------------------------------------------------------------------------------------------- bnk
static void dumpTable(const char* tag, const s16* t)
{
	if (!t) {
		std::printf(" %s=-", tag);
		return;
	}
	u32 n = HostFmt::oscTableLength(t);
	std::printf(" %s=", tag);
	for (u32 i = 0; i < n; ++i)
		std::printf("%d%s", (int)t[i], i + 1 < n ? "," : "");
}

static void dumpEffect(TInstEffect* e)
{
	if (!e) {
		std::printf(" E:-");
		return;
	}
	if (auto* r = dynamic_cast<TInstRand*>(e))
		std::printf(" E:rand,%u,%08x,%08x", (u32)r->mTarget, fb(r->mBase), fb(r->mWidth));
	else if (auto* s = dynamic_cast<TInstSense*>(e))
		std::printf(" E:sense,%u,%u,%u,%08x,%08x", (u32)s->mTarget, (u32)s->unk8, (u32)s->unk9, fb(s->unkC), fb(s->unk10));
	else
		std::printf(" E:?");
}

static int modeBnk(const char* path)
{
	setupHeap();
	Aaf aaf(path);
	int bankNo = 0;
	for (auto& c : aaf.cmds) {
		if (c.cmd != 2)
			continue;
		TBasicBank* bank = BNKParser::createBasicBank(aaf.data + c.ofs);
		int insts        = 0;
		for (int i = 0; i < 0x100; ++i)
			if (bank->getInst(i))
				++insts;
		std::printf("BANK %d insts=%d\n", bankNo, insts);
		for (int i = 0; i < 0x100; ++i) {
			TInst* inst = bank->getInst(i);
			if (!inst)
				continue;
			if (inst->getType() == 'BSIC') {
				auto* b = (TBasicInst*)inst;
				std::printf("INST %d.%d vol=%08x pitch=%08x nosc=%u neff=%u nkey=%u\n", bankNo, i, fb(b->mVolume),
				            fb(b->mPitch), b->mOscillatorCount, b->mEffectCount, b->mKeyRegionCount);
				for (u32 k = 0; k < b->mOscillatorCount; ++k) {
					auto* o = b->mOscillators[k];
					if (!o) {
						std::printf("  OSC %u -\n", k);
						continue;
					}
					std::printf("  OSC %u t=%u rate=%08x width=%08x vertex=%08x", k, (u32)o->mTarget, fb(o->mRate),
					            fb(o->mWidth), fb(o->mVertex));
					dumpTable("ads", o->mAdsTable);
					dumpTable("rel", o->mRelTable);
					std::printf("\n");
				}
				std::printf("  EFF");
				for (u32 k = 0; k < b->mEffectCount; ++k)
					dumpEffect(b->mEffects[k]);
				std::printf("\n");
				for (u32 k = 0; k < b->mKeyRegionCount; ++k) {
					auto& km = b->mKeyRegions[k];
					std::printf("  KEY %u high=%d nvelo=%u\n", k, km.mHighKey, km.mVeloRegionCount);
					for (u32 v = 0; v < km.mVeloRegionCount; ++v)
						std::printf("   VEL %u %d %d %08x %08x\n", v, km.mVeloRegions[v].unk0, km.mVeloRegions[v].unk4,
						            fb(km.mVeloRegions[v].unk8), fb(km.mVeloRegions[v].unkC));
				}
			} else if (inst->getType() == 'PERC') {
				auto* ds = (TDrumSet*)inst;
				std::printf("PERC %d.%d\n", bankNo, i);
				for (int j = 0; j < 128; ++j) {
					auto& p = ds->unk4[j];
					std::printf("  P %d vol=%08x pitch=%08x pan=%08x rel=%u neff=%u nvelo=%u", j, fb(p.mVolume),
					            fb(p.mPitch), fb(p.mPan), (u32)p.mRelease, p.mEffectCount, p.mVeloRegionCount);
					for (u32 k = 0; k < p.mEffectCount; ++k)
						dumpEffect(p.mEffects[k]);
					std::printf("\n");
					for (u32 v = 0; v < p.mVeloRegionCount; ++v)
						std::printf("   VEL %u %d %d %08x %08x\n", v, p.mVeloRegions[v].unk0, p.mVeloRegions[v].unk4,
						            fb(p.mVeloRegions[v].unk8), fb(p.mVeloRegions[v].unkC));
				}
			} else {
				std::printf("INST %d.%d unknown type\n", bankNo, i);
			}
		}
		++bankNo;
	}
	return 0;
}

// ----------------------------------------------------------------------------------------------------- wsys
static void dumpWave(const JASystem::TWaveInfo& w)
{
	std::printf(" u0=%u u1=%u u2=%u rate=%08x off=%d C=%d 10=%d 14=%d 18=%d 1c=%d 20=%d 22=%d 28=%d", (u32)w.unk0,
	            (u32)w.unk1, (u32)w.unk2, fb(w.unk4), w.mOffset, w.unkC, w.unk10, w.unk14, w.unk18, w.unk1c,
	            (int)w.unk20, (int)w.unk22, w.unk28);
}

static int modeWsys(const char* path)
{
	setupHeap();
	Aaf aaf(path);
	int no = 0;
	for (auto& c : aaf.cmds) {
		if (c.cmd != 3)
			continue;
		void* data = aaf.data + c.ofs;
		u32 groups = WSParser::getGroupCount(data);
		TWaveBank* bank = groups == 1 ? WSParser::createSimpleWaveBank(data) : WSParser::createBasicWaveBank(data);
		if (!bank) {
			std::printf("WBANK %d null\n", no++);
			continue;
		}
		if (bank->getType() == 'SMPL') {
			auto* s = (TSimpleWaveBank*)bank;
			std::printf("WBANK %d simple groups=%u tableSize=%u name=%s\n", no, groups, s->mWaveTableSize,
			            s->mWaveArcFileName);
			for (u32 i = 0; i < s->mWaveTableSize; ++i) {
				if (s->mWaveTable[i].mHeap != &s->mHeap)
					continue;
				std::printf("  WAVE id=%u", i);
				dumpWave(s->mWaveTable[i].mWaveInfo);
				std::printf("\n");
			}
		} else {
			auto* b = (TBasicWaveBank*)bank;
			std::printf("WBANK %d basic groups=%u tableSize=%d\n", no, b->mWaveGroupCount, b->mWaveTableSize);
			for (u32 g = 0; g < b->mWaveGroupCount; ++g) {
				auto& grp = b->mWaveGroups[g];
				std::printf(" GROUP %u waves=%u name=%s\n", g, grp.mWaveCount, grp.mWaveArcFileName);
				for (u32 w = 0; w < grp.mWaveCount; ++w) {
					std::printf("  WAVE id=%u", grp.mWaves[w].mWaveID);
					dumpWave(grp.mWaves[w].mWaveHandle.mWaveInfo);
					std::printf("\n");
				}
			}
		}
		++no;
	}
	return 0;
}

// ------------------------------------------------------------------------------------------------------ barc
struct VLEntryT { // mirrors Vload::VLArcEntry (JASVload.cpp), 0x20 bytes
	u32 a, b, c;
	u16 unkC, unkE;
	u32 u10, u14, u18, u1C;
};

static int modeBarc(const char* aafPath, const char* arcPath)
{
	setupHeap();
	Aaf aaf(aafPath);
	Vload::initVloadBuffers();
	const Aaf::Cmd* hdr = nullptr;
	for (auto& c : aaf.cmds)
		if (c.cmd == 4)
			hdr = &c;
	// the same copy the loader makes
	u8* copy = (u8*)JASystem::Kernel::allocFromSysDram(hdr->size);
	std::memcpy(copy, aaf.data + hdr->ofs, hdr->size);
	for (int pass = 0; pass < 2; ++pass) { // second pass: the marker must make the conversion a no-op
		std::string name = std::string("/AudioRes/Seqs/") + (pass ? "again.arc" : "sequence.arc");
		Vload::initHeaderM((char*)name.c_str(), copy, nullptr);
		u32 handle = Vload::getArchiveHandle((char*)(pass ? "again.arc" : "sequence.arc"));
		std::printf("BARC pass=%d handle=%08x\n", pass, handle);
		for (u32 i = 0; i < 0x30; ++i) {
			auto* e = (VLEntryT*)Vload::getRealHandle(handle + i);
			std::printf("ENT %u size=%u ofs=%u link=%u u14=%08x\n", i, Vload::checkSize(handle + i), e->u18, (u32)e->unkE, e->u14);
		}
	}
	return 0;
}

// ------------------------------------------------------------------------------------------------------- bas
static int modeBas(const char* listPath)
{
	auto lst = readFile(listPath);
	std::string text((char*)lst.data(), lst.size());
	size_t pos = 0;
	alignas(16) static u8 storage[sizeof(JAIAnimeSound)];
	JAIAnimeSound* a = new (storage) JAIAnimeSound(); // never destroyed: the destructor is not part of the decomp
	while (pos < text.size()) {
		size_t e = text.find('\n', pos);
		std::string file = text.substr(pos, e - pos);
		pos              = e + 1;
		auto v           = readFile(file.c_str());
		u8* d            = keep(v);
		for (int pass = 0; pass < 2; ++pass)
			a->initActorAnimSound(d, 1, 0.0f);
		std::printf("BAS %s count=%u\n", file.c_str(), (u32)a->mData->mCount);
		for (u32 i = 0; i < a->mData->mCount; ++i) {
			auto& x = a->mData->mEntries[i];
			std::printf(" E %u id=%08x s=%08x e=%08x p=%08x f=%08x v=%u ps=%d sd=%u pan=%u vs=%d\n", i, x.mSoundID,
			            fb(x.mStartFrame), fb(x.mEndFrame), fb(x.mPitch), x.unk10, (u32)x.mVolume, (int)x.mPitchScale,
			            (u32)x.unk16, (u32)x.mPan, (int)x.mVolumeScale);
		}
		_aligned_free(d);
	}
	return 0;
}

// ------------------------------------------------------------------------------------------------------- osc
static int modeOsc(const char* listPath)
{
	auto lst = readFile(listPath);
	std::string text((char*)lst.data(), lst.size());
	size_t pos = 0;
	std::vector<u8>* cur = nullptr;
	std::string curName;
	u8* base = nullptr;
	while (pos < text.size()) {
		size_t e         = text.find('\n', pos);
		std::string line = text.substr(pos, e - pos);
		pos              = e + 1;
		size_t sp        = line.rfind(' ');
		std::string file = line.substr(0, sp);
		u32 off          = (u32)std::strtoul(line.c_str() + sp + 1, nullptr, 10);
		if (file != curName) {
			auto v  = readFile(file.c_str());
			base    = keep(v);
			curName = file;
		}
		s16* t = (s16*)(base + off);
		for (int pass = 0; pass < 2; ++pass)
			HostFmt::oscTableToHost(t);
		u32 n = HostFmt::oscTableLength(t);
		std::printf("OSC %s %u n=%u", file.c_str(), off, n);
		for (u32 i = 0; i < n; ++i)
			std::printf(" %d", (int)t[i]);
		std::printf("\n");
	}
	(void)cur;
	return 0;
}

// ------------------------------------------------------------------------------------------------------- seq
static int modeSeq(int argc, char** argv)
{
	auto v = readFile(argv[2]);
	u8* d  = keep(v);
	TSeqCtrl c;
	c.init(d, 0);
	for (int i = 3; i < argc; ++i) {
		u32 off = (u32)std::strtoul(argv[i], nullptr, 0);
		c.jump(off);
		u16 r16 = c.read16();
		c.jump(off);
		u32 r24 = c.read24();
		c.jump(off);
		u32 r32 = c.read32();
		std::printf("SEQ %u g16=%04x g24=%06x g32=%08x r16=%04x r24=%06x r32=%08x\n", off, c.get16(off), c.get24(off),
		            c.get32(off), r16, r24, r32);
	}
	return 0;
}

// ----------------------------------------------------------------------------------------------------- replay
// Replays every sequence of the sequence archive through the real TTrack/TSeqParser (no DSP: the banks are not
// registered, so a note-on stops at the bank lookup) and checks that no register id, port, timed parameter, child track,
// oscillator, interrupt slot or table index leaves its table. Every such access is reported by the game code itself on
// stderr ("JASystem: ..."), register accesses are also counted through TRegisterParam::sAccessHook.
static u32 gRegRead[256], gRegWrite[256], gRegBad, gCurSeq;
static int gCurTick;
static void regHook(u8 id, bool write, bool inRange)
{
	(write ? gRegWrite : gRegRead)[id]++;
	if (!inRange) {
		gRegBad++;
		std::fprintf(stderr, "BADREG seq=%u tick=%d id=0x%02x %s; active tracks (seq offset, 12 bytes before):\n", gCurSeq, gCurTick,
		             id, write ? "write" : "read");
		for (u32 k = 0; k < (u32)TrackMgr::sTrackCount; ++k) {
			TTrack& tr = TrackMgr::sTrackPool[k];
			if (tr.mSeqState == 1 || tr.mSeqState == 2) {
				const u8* p = tr.mSeqCtrl.mCurrentFilePtr;
				const u8* base = tr.mSeqCtrl.mRawFilePtr;
				std::fprintf(stderr, "   track %u at +%d:", k, (int)(p - base));
				for (int j = -12; j < 0; ++j)
					std::fprintf(stderr, " %02x", p[j]);
				std::fprintf(stderr, "\n");
			}
		}
	}
}

static int modeReplay(const char* aafPath, const char* arcPath, int ticks)
{
	setupHeap();
	Aaf aaf(aafPath);
	auto arcv  = readFile(arcPath);
	u8* arc    = keep(arcv);
	const Aaf::Cmd* hdr = nullptr;
	for (auto& c : aaf.cmds)
		if (c.cmd == 4)
			hdr = &c;
	const u8* b = aaf.data + hdr->ofs;
	u32 count   = rd32(b + 0xC);

	JASystem::BankMgr::init(0x100);
	JASystem::TrackMgr::init(0xB9, 8); // MSound: systemTrackMax 0xB9, systemRootTrackMax 8
	JASystem::ChGlobal::init(); // the 256-channel pool tracks allocate from (no DSP channel is ever started here)
	JASystem::TrackMgr::reset();
	TRegisterParam::sAccessHook = &regHook;

	u32 rng = 12345;
	auto next = [&]() { return rng = rng * 1664525u + 1013904223u; };
	int played = 0, finished = 0;
	u64 tracksSeen = 0;
	for (u32 i = 0; i < count; ++i) {
		const u8* e = b + 0x20 + 0x20 * i;
		u32 link = (u32)e[0xE] << 8 | e[0xF];
		if (link != 0xFFFF)
			continue; // alias of another entry
		u32 ofs = rd32(e + 0x18), size = rd32(e + 0x1C);
		if (!size || ofs + size > arcv.size())
			continue;
		TTrack* root = TrackMgr::getNewTrack();
		if (!root) {
			std::printf("REPLAY %u no free track\n", i);
			return 1;
		}
		int slot = root->setSeqData(arc + ofs, (s32)size, (Player::SEQ_PLAYMODE)0);
		if (slot < 0) {
			std::printf("REPLAY %u no root slot\n", i);
			return 1;
		}
		++played;
		root->mSeqState = 1; // what TTrack::startSeq does before it registers rootCallback with the subframe scheduler
		int t = 0;
		gCurSeq = i;
		for (; t < ticks; ++t) {
			gCurTick = t;
			if (t % 40 == 0) { // the game side: poke import ports (also raises the port interrupts of ports 0/1)
				u32 r = next();
				root->writePortAppDirect((r >> 8) & 0xF, (u16)(r >> 16));
			}
			if (TTrack::rootCallback(root) != 0)
				break;
		}
		if (t < ticks)
			++finished;
		else {
			int kids = 0;
			for (TTrack* c : root->mChildren)
				kids += c != nullptr;
			u8 before = root->mSeqState;
			root->stopSeq();
			int rc = TTrack::rootCallback(root);
			if (i == 0)
				std::printf("  STOP root children=%d stateBefore=%u stateAfter=%u rc=%d\n", kids, before, root->mSeqState, rc);
		}
		std::printf("REPLAY %u ofs=%u size=%u ticks=%d %s freeTracks=%u\n", i, ofs, size, t, t < ticks ? "ended" : "cut",
		            TrackMgr::seqRemain);
		{
			u32 hist[4] = { 0, 0, 0, 0 };
			for (u32 k = 0; k < (u32)TrackMgr::sTrackCount; ++k) {
				TTrack& tr = TrackMgr::sTrackPool[k];
				tracksSeen += tr.mSeqState != 0;
				if (i == 0 && tr.mSeqState != 0)
					std::printf("  LEFT track %u state=%u inner=%u parent=%s\n", k, tr.mSeqState, tr.mInnerMemory,
					            tr.mParent ? "yes" : "no");
				if (tr.mSeqState < 4)
					hist[tr.mSeqState]++;
			}
			std::printf("  STATES free0=%u s1=%u s2=%u s3=%u\n", hist[0], hist[1], hist[2], hist[3]);
		}
	}
	std::printf("SUMMARY sequences=%d ended=%d stillActive=%llu badRegisterAccess=%u\n", played, finished,
	            (unsigned long long)tracksSeen, gRegBad);
	std::printf("REGISTERS read:");
	for (int id = 0; id < 256; ++id)
		if (gRegRead[id])
			std::printf(" %02x=%u", id, gRegRead[id]);
	std::printf("\nREGISTERS write:");
	for (int id = 0; id < 256; ++id)
		if (gRegWrite[id])
			std::printf(" %02x=%u", id, gRegWrite[id]);
	std::printf("\n");
	return gRegBad ? 1 : 0;
}

void installCrashHandler(); // src/crash.cpp: symbolized stack on a fault

int main(int argc, char** argv)
{
	installCrashHandler();
	if (argc < 3) {
		std::fprintf(stderr, "usage: audio_check <aaf|bnk|wsys|barc|bas|osc|seq> args\n");
		return 2;
	}
	std::string m = argv[1];
	if (m == "aaf")
		return modeAaf(argv[2]);
	if (m == "bnk")
		return modeBnk(argv[2]);
	if (m == "wsys")
		return modeWsys(argv[2]);
	if (m == "barc")
		return modeBarc(argv[2], argc > 3 ? argv[3] : "");
	if (m == "replay")
		return modeReplay(argv[2], argv[3], argc > 4 ? std::atoi(argv[4]) : 6000);
	if (m == "bas")
		return modeBas(argv[2]);
	if (m == "osc")
		return modeOsc(argv[2]);
	if (m == "seq")
		return modeSeq(argc, argv);
	std::fprintf(stderr, "unknown mode %s\n", m.c_str());
	return 2;
}
