// Native check of the scene/map data loaders (collision .col, ymap.ymp, scene.ral, SPCB .sb, .prm) on real disc files,
// running the real decomp code of sms_game.lib on the host. Usage: scene_test <scene.szs>
// (driven by tools\verify-scene.py, which prints the same lines from an independent Python parse; build with
// scene_test.cmd). Every line is "<KIND> <key> <values...>"; each loader is run twice on the same buffer to prove the
// in-place swaps are idempotent.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <vector>

#define protected public
#define private public
#include <JSystem/JKernel/JKRDecomp.hpp>
#include <JSystem/JKernel/JKRMemArchive.hpp>
#include <Map/MapCollisionEntry.hpp>
#include <Map/PollutionManager.hpp>
#include <Enemy/Graph.hpp>
#include <Strategic/spcinterp.hpp>
#include <System/Params.hpp>
#include <System/ParamInst.hpp>
#undef private
#undef protected

void installCrashHandler(); // sunshine-pc/src/crash.cpp: symbolized stack on a fault, also during static init
static struct CrashInstall {
	CrashInstall() { installCrashHandler(); }
} sCrashInstall;

static void collect(JKRMemArchive* a, u32 dirIdx, const std::string& prefix, std::vector<std::string>& out)
{
	JKRArchive::SDIDirEntry* dir = a->mDirectories + dirIdx;
	for (u32 i = 0; i < dir->mNum; i++) {
		JKRArchive::SDIFileEntry* f = a->mFileEntries + dir->mFirstIdx + i;
		std::string name            = a->mStrTable + (f->mFlagsAndNameOffset & 0xFFFFFF);
		if (f->mFlagsAndNameOffset >> 24 & 2) {
			if (name != "." && name != "..")
				collect(a, f->mDataOffset, prefix + "/" + name, out);
		} else {
			out.push_back(prefix + "/" + name);
		}
	}
}

static void testCol(const char* vol, const std::string& path, JKRMemArchive* a)
{
	std::string full = std::string("/") + vol + path;
	std::fprintf(stderr, "col %s global=%p direct=%p\n", full.c_str(), JKRGetResource(full.c_str()), a->getResource(path.c_str()));
	for (int pass = 0; pass < 2; ++pass) {
		TMapCollisionStatic col;
		col.TMapCollisionBase::init(full.c_str(), 0x2, nullptr);
		double vsum = 0;
		for (u32 i = 0; i < col.mVertexNum * 3; ++i)
			vsum += (&col.mVertices->x)[i];
		long long idxsum = 0, addsum = 0, gsum = 0, tris = 0, csum = 0;
		for (u32 g = 0; g < col.mCollisionGroupNum; ++g) {
			TMapCollisionBase::TMapCollisionGroup& gr = col.mCollisionGroups[g];
			gsum += gr.mBGType * 1000 + gr.mFlags;
			tris += gr.mTriangleNum;
			const s16* idx = (const s16*)(col.mColBase + gr.mIndicesOfs);
			for (int i = 0; i < gr.mTriangleNum * 3; ++i)
				idxsum += idx[i];
			if (gr.mAdditionalDatasOfs) {
				const s16* ad = (const s16*)(col.mColBase + gr.mAdditionalDatasOfs);
				for (int i = 0; i < gr.mTriangleNum; ++i)
					addsum += ad[i];
			}
			const u8* c = col.mColBase + gr.unkCOfs;
			const u8* d = col.mColBase + gr.unk10Ofs;
			for (int i = 0; i < gr.mTriangleNum; ++i)
				csum += c[i] * 7 + d[i];
		}
		if (pass == 0 || true)
			std::printf("COL%d %s %u %u %lld %.3f %lld %lld %lld %lld\n", pass, path.c_str(), col.mVertexNum,
			            col.mCollisionGroupNum, tris, vsum, idxsum, addsum, gsum, csum);
	}
}

static void testYmp(void* res)
{
	for (int pass = 0; pass < 2; ++pass) {
		TPollutionManager* mgr = new TPollutionManager("test");
		mgr->mJointModelNum    = rdbe16(res);
		mgr->setDataAddress((TPollutionManager::TPollutionInfo*)res);
		std::printf("YMP%d %d\n", pass, mgr->mJointModelNum);
		for (int i = 0; i < mgr->mJointModelNum; ++i) {
			TPollutionLayerInfo& l = mgr->mLayerInfos[i];
			std::printf("YMPL %d %u %u %u %.3f %.3f %.3f %.3f %.3f %.3f %u %u %u %ld\n", i, l.mPollutionType, l.mFlags,
			            l.mPlaneType, l.mVerticalOffset, l.mTexelSize, l.mLeft, l.mTop, l.mRight, l.mBottom, l.mLog2Width,
			            l.mLog2Height, l.unk24, (long)(l.mHeightMap - (u8*)res));
		}
	}
}

static void testRal(void* res)
{
	for (int pass = 0; pass < 2; ++pass) {
		TGraphGroup* g = new TGraphGroup(res);
		std::printf("RAL%d %d\n", pass, g->unk4);
		for (int i = 0; i < g->unk4; ++i) {
			TRailNode* n = g->getNode(i);
			std::printf("RALG %d %s %d\n", i, g->getNodeName(i), g->unk0[i].mNodeNum);
			for (int j = 0; j < g->unk0[i].mNodeNum; ++j) {
				TRailNode& r = n[j];
				std::printf("RALN %d %d %d %d %d %u %u %u %u %u", i, j, r.mPosition.x, r.mPosition.y, r.mPosition.z,
				            r.mFlags, r.mPitch, r.mYaw, r.mRoll, r.mSpeed);
				if (!(r.mFlags & 0x208)) { // translateNodes filters the connections of these nodes
					std::printf(" %d", r.mConnectionNum);
					for (int k = 0; k < r.mConnectionNum; ++k)
						std::printf(" %u:%.4f", r.mConnections[k], r.mPeriods[k]);
				}
				std::printf("\n");
			}
		}
	}
}

static void testSb(const std::string& path, void* res)
{
	for (int pass = 0; pass < 2; ++pass) {
		TSpcBinary bin(res);
		TSpcHeader* h = bin.getHeader();
		std::printf("SB%d %s %u %u %u %u %u\n", pass, path.c_str(), h->mTextOffset, h->mDataOffset, h->mDataNum,
		            h->mSymbolOffset, h->mSymbolNum);
		unsigned long long acc = 0;
		for (u32 i = 0; i < h->mSymbolNum; ++i) {
			TSpcSymbol* s = bin.getSymbol(i);
			for (const char* c = bin.getSymbolName(s); *c; ++c)
				acc = acc * 131 + (unsigned char)*c;
			acc = acc * 131 + s->mType * 3 + s->mData * 5 + s->mNameHash;
		}
		unsigned long long dacc = 0;
		for (u32 i = 0; i < h->mDataNum; ++i)
			dacc = dacc * 131 + bin.getDataOffset(i);
		std::printf("SBS %s %llu %llu\n", path.c_str(), acc, dacc);
	}
}

static void testPrm(const std::string& path, JKRMemArchive* a, void* res)
{
	// Build one TParamT per entry (type chosen by the payload size), then run the real TParams::load.
	u8* p      = (u8*)res;
	s32 length = (s32)rdbe32(p);
	p += 4;
	std::vector<std::string> names;
	std::vector<std::unique_ptr<TBaseParam> > params;
	TParams tp;
	names.reserve(length);
	u8* q = p;
	std::vector<int> sizes;
	for (int i = 0; i < length; ++i) {
		u16 key = rdbe16(q);
		u16 nl  = rdbe16(q + 2);
		names.emplace_back((char*)q + 4, nl);
		s32 sz = (s32)rdbe32(q + 4 + nl);
		sizes.push_back(sz);
		const char* nm = names.back().c_str();
		switch (sz) {
		case 1: params.emplace_back(new TParamT<u8>(&tp, 0, key, nm)); break;
		case 2: params.emplace_back(new TParamT<s16>(&tp, 0, key, nm)); break;
		case 4: params.emplace_back(new TParamT<f32>(&tp, 0.0f, key, nm)); break;
		case 12: params.emplace_back(new TParamT<JGeometry::TVec3<f32> >(&tp, JGeometry::TVec3<f32>(0, 0, 0), key, nm)); break;
		default: std::printf("PRMBAD %s %d\n", path.c_str(), sz); return;
		}
		q += 4 + nl + 4 + sz;
	}
	for (int pass = 0; pass < 2; ++pass) {
		JSUMemoryInputStream stream(res, (s32)(q - (u8*)res));
		tp.load(stream);
		unsigned long long acc = 1469598103934665603ull;
		for (int i = 0; i < length; ++i) {
			unsigned bits[3] = { 0, 0, 0 };
			switch (sizes[i]) {
			case 1: bits[0] = ((TParamT<u8>*)params[i].get())->value; break;
			case 2: bits[0] = (u16)((TParamT<s16>*)params[i].get())->value; break;
			case 4: memcpy(bits, &((TParamT<f32>*)params[i].get())->value, 4); break;
			case 12: memcpy(bits, &((TParamT<JGeometry::TVec3<f32> >*)params[i].get())->value, 12); break;
			}
			for (int k = 0; k < 3; ++k)
				acc = (acc ^ bits[k]) * 1099511628211ull;
		}
		std::printf("PRM%d %s %d %llu\n", pass, path.c_str(), length, acc);
	}
}

int main(int argc, char** argv)
{
	if (argc < 2)
		return 2;
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	std::fprintf(stderr, "main\n");
	std::vector<u8> in;
	{
		FILE* f = std::fopen(argv[1], "rb");
		if (!f)
			return 2;
		u8 buf[65536];
		size_t n;
		while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
			in.insert(in.end(), buf, buf + n);
		std::fclose(f);
	}
	std::vector<u8> out;
	JKRCompression c = JKRDecomp::checkCompressed(in.data());
	if (c == JKR_COMPRESSION_NONE) {
		out = in;
	} else {
		u32 n = JKRDecompExpandSize(in.data());
		out.assign(n + 32, 0);
		JKRDecomp::decodeSZS(in.data(), out.data(), n, 0);
		out.resize(n);
	}
	std::fprintf(stderr, "decoded\n");
	JKRMemArchive* a = new JKRMemArchive(out.data(), (u32)out.size(), MBF_0);
	std::fprintf(stderr, "mounted volume=%s getVolume=%p a=%p list=%d\n", a->mVolumeName, (void*)JKRFileLoader::getVolume("scene"), (void*)a, (int)JKRFileLoader::sVolumeList.getNumLinks());
	std::vector<std::string> files;
	collect(a, 0, "", files);
	for (auto& path : files) {
		auto ends = [&](const char* e) { return path.size() >= strlen(e) && path.compare(path.size() - strlen(e), strlen(e), e) == 0; };
		if (ends(".col")) {
			testCol(a->mVolumeName, path, a);
		} else if (path == "/map/ymap.ymp") {
			testYmp(a->getResource(path.c_str()));
		} else if (path == "/map/scene.ral") {
			testRal(a->getResource(path.c_str()));
		} else if (ends(".sb")) {
			testSb(path, a->getResource(path.c_str()));
		} else if (ends(".prm")) {
			testPrm(path, a, a->getResource(path.c_str()));
		}
	}
	return 0;
}
