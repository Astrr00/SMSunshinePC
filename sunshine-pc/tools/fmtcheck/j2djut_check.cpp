// Offline format check for the J2D/JUT/GC2D readers (FmtJ2DJUT). Links the real decomp readers and runs them on real
// disc bytes, then prints what the host structs contain; fmtcheck_j2djut.py parses the same bytes independently
// (big-endian, from the file format) and compares the two outputs line by line. Build/run: fmtcheck_j2djut.py.
//
//   j2djut_check bti  <manifest>   manifest line: path of a raw BTI file          -> JUTTexture::storeTIMG / swapResTIMG
//   j2djut_check tex1 <manifest>   manifest line: path offset count (J3D TEX1)    -> swapResTIMG on embedded headers
//   j2djut_check bfn  <manifest>   manifest line: path of a raw BFN font          -> JUTResFont (swapResFONT)
//   j2djut_check blo  <manifest>   manifest line: path of a raw BLO, archive dir   -> J2DScreen::makeHiearachyPanes
//   j2djut_check bmg  <manifest>   manifest line: path of a raw BMG               -> TMessageLoader, SMSGetMessageData
//   j2djut_check ntab <manifest>   manifest line: path offset                     -> JUTNameTab (ResNTAB swap)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <algorithm>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <GC2D/MessageLoader.hpp>
#include <GC2D/MessageUtil.hpp>
#include <JSystem/J2D/J2DPicture.hpp>
#include <JSystem/J2D/J2DScreen.hpp>
#include <JSystem/J2D/J2DTextBox.hpp>
#include <JSystem/J2D/J2DWindow.hpp>
#include <JSystem/JKernel/JKRArchive.hpp>
#include <JSystem/JKernel/JKRFileLoader.hpp>
#include <JSystem/JSupport/JSUMemoryInputStream.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <JSystem/JUtility/JUTPalette.hpp>
#include <JSystem/JUtility/JUTResFont.hpp>
#include <JSystem/JUtility/JUTTexture.hpp>

namespace fs = std::filesystem;

// ---- files ---------------------------------------------------------------------------------------------------------

static u8* loadFile(const std::string& path, size_t* sizeOut = nullptr)
{
	FILE* f = fopen(path.c_str(), "rb");
	if (!f) {
		fprintf(stderr, "cannot open %s\n", path.c_str());
		exit(2);
	}
	fseek(f, 0, SEEK_END);
	size_t n = (size_t)ftell(f);
	fseek(f, 0, SEEK_SET);
	u8* buf = (u8*)_aligned_malloc(n + 0x40, 0x20); // 0x20 aligned like the game's archive buffers
	memset(buf, 0, n + 0x40);
	fread(buf, 1, n, f);
	fclose(f);
	if (sizeOut)
		*sizeOut = n;
	return buf;
}

static std::vector<std::string> readManifest(const char* path)
{
	std::vector<std::string> lines;
	FILE* f = fopen(path, "r");
	if (!f) {
		fprintf(stderr, "cannot open manifest %s\n", path);
		exit(2);
	}
	char line[2048];
	while (fgets(line, sizeof(line), f)) {
		std::string s = line;
		while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
			s.pop_back();
		if (!s.empty())
			lines.push_back(s);
	}
	fclose(f);
	return lines;
}

static std::vector<std::string> split(const std::string& s, char sep = '\t')
{
	std::vector<std::string> r;
	size_t i = 0;
	while (true) {
		size_t j = s.find(sep, i);
		if (j == std::string::npos) {
			r.push_back(s.substr(i));
			return r;
		}
		r.push_back(s.substr(i, j - i));
		i = j + 1;
	}
}

// ---- resource lookup the readers call (the game's JKR layer is not linked) -------------------------------------------

static std::map<std::string, u8*> gArchive; // lower case path below the archive root -> bytes (loaded lazily)
static std::map<std::string, std::string> gArchivePaths;

static std::string norm(std::string s)
{
	for (auto& c : s) {
		if (c == '\\')
			c = '/';
		c = (char)tolower((unsigned char)c);
	}
	while (!s.empty() && s[0] == '/')
		s.erase(0, 1);
	return s;
}

static void setArchiveDir(const std::string& dir)
{
	gArchive.clear();
	gArchivePaths.clear();
	for (auto& e : fs::recursive_directory_iterator(dir)) {
		if (!e.is_regular_file())
			continue;
		gArchivePaths[norm(fs::relative(e.path(), dir).string())] = e.path().string();
	}
}

static void* findRes(const char* name)
{
	std::string n = norm(name);
	auto it       = gArchivePaths.find(n);
	if (it == gArchivePaths.end()) {
		// bare file name inside the archive ("title_logo.bti", what BLO resource references hold)
		std::string tail = "/" + n;
		for (auto k = gArchivePaths.begin(); k != gArchivePaths.end(); ++k) {
			if (k->first.size() >= tail.size() && k->first.compare(k->first.size() - tail.size(), tail.size(), tail) == 0) {
				it = k;
				break;
			}
		}
	}
	if (it == gArchivePaths.end()) {
		// absolute game path ("/common/2d/x.bmg"): match the longest suffix
		for (auto k = gArchivePaths.begin(); k != gArchivePaths.end(); ++k) {
			if (n.size() >= k->first.size() && n.compare(n.size() - k->first.size(), k->first.size(), k->first) == 0) {
				it = k;
				break;
			}
		}
	}
	if (it == gArchivePaths.end())
		return nullptr;
	auto b = gArchive.find(it->first);
	if (b == gArchive.end())
		b = gArchive.emplace(it->first, loadFile(it->second)).first;
	return b->second;
}

void* JKRArchive::getGlbResource(u32 type, const char* name, JKRArchive* archive) { return findRes(name); }
void* JKRFileLoader::getGlbResource(const char* name) { return findRes(name); }
void* JKRFileLoader::getGlbResource(const char* name, JKRFileLoader*) { return findRes(name); }

extern "C" void OSPanic(const char* file, int line, const char* msg, ...)
{
	fprintf(stderr, "OSPanic %s:%d %s\n", file, line, msg);
	exit(3);
}

// ---- GX stubs: record what the readers hand to the GPU layer ---------------------------------------------------------

struct TexCall {
	const u8* image;
	u16 w, h;
	int fmt, wrapS, wrapT, mip, tlut;
	float minLod, maxLod, bias;
	int biasClamp, edgeLod, aniso;
};
struct TlutCall {
	const u8* data;
	int fmt, n;
};
static TexCall gTex;
static TlutCall gTlut;
static int gTexCalls, gTlutCalls;

extern "C" {
void GXInitTexObj(GXTexObj*, void* image, u16 w, u16 h, GXTexFmt fmt, GXTexWrapMode ws, GXTexWrapMode wt, u8 mip)
{
	gTex.image = (u8*)image;
	gTex.w = w;
	gTex.h = h;
	gTex.fmt = fmt;
	gTex.wrapS = ws;
	gTex.wrapT = wt;
	gTex.mip = mip;
	gTex.tlut = -1;
	gTexCalls++;
}
void GXInitTexObjCI(GXTexObj*, void* image, u16 w, u16 h, GXCITexFmt fmt, GXTexWrapMode ws, GXTexWrapMode wt, u8 mip,
                    u32 tlut)
{
	gTex.image = (u8*)image;
	gTex.w = w;
	gTex.h = h;
	gTex.fmt = fmt;
	gTex.wrapS = ws;
	gTex.wrapT = wt;
	gTex.mip = mip;
	gTex.tlut = (int)tlut;
	gTexCalls++;
}
void GXInitTexObjLOD(GXTexObj*, GXTexFilter, GXTexFilter, f32 minLod, f32 maxLod, f32 bias, GXBool biasClamp,
                     GXBool edgeLod, GXAnisotropy aniso)
{
	gTex.minLod = minLod;
	gTex.maxLod = maxLod;
	gTex.bias = bias;
	gTex.biasClamp = biasClamp;
	gTex.edgeLod = edgeLod;
	gTex.aniso = aniso;
}
void GXInitTlutObj(GXTlutObj*, void* lut, GXTlutFmt fmt, u16 n)
{
	gTlut.data = (u8*)lut;
	gTlut.fmt = fmt;
	gTlut.n = n;
	gTlutCalls++;
}
}

// ---- dump helpers --------------------------------------------------------------------------------------------------

static u32 fnv(const u8* p, size_t n, u32 h = 2166136261u)
{
	for (size_t i = 0; i < n; i++)
		h = (h ^ p[i]) * 16777619u;
	return h;
}

static void dumpTimg(const char* tag, const ResTIMG* t)
{
	printf("%s fmt=%d alpha=%d w=%d h=%d ws=%d wt=%d idx=%d cf=%d nc=%d po=%d mip=%d edge=%d bc=%d an=%d min=%d mag=%d "
	       "minlod=%d maxlod=%d mc=%d bias=%d io=%d\n",
	       tag, t->format, t->alphaEnabled, t->width, t->height, t->wrapS, t->wrapT, t->isIndexTexture, t->colorFormat,
	       t->numColors, (s32)t->paletteOffset, t->mipmapEnabled, t->doEdgeLod, t->biasClamp, t->maxAnisotropy,
	       t->minFilter, t->magFilter, t->minLod, t->maxLod, t->mipmapCount, t->lodBias, (s32)t->imageDataOffset);
}

// ---- BTI -----------------------------------------------------------------------------------------------------------

static void doBti(const std::string& path)
{
	size_t n;
	u8* buf = loadFile(path, &n);
	ResTIMG* h = (ResTIMG*)buf;
	u32 imgHash = fnv(buf + 0x20, n > 0x20 ? n - 0x20 : 0); // everything behind the header must stay untouched

	gTexCalls = gTlutCalls = 0;
	memset(&gTex, 0, sizeof(gTex));
	JUTTexture tex(h); // storeTIMG: swapResTIMG + GXInitTexObj[CI] / JUTPalette
	printf("BTI %s\n", fs::path(path).filename().string().c_str());
	dumpTimg("  hdr", h);
	printf("  mark=%02x tex=%d,%d,%d,%d,%d,%d,%d,%d,%d bias=%d\n", (u8)h->unk19[0], tex.mFormat, tex.mAlphaEnabled,
	       tex.mWidth, tex.mHeight, tex.mWrapS, tex.mWrapT, tex.mMinFilter, tex.mMagFilter, (s8)tex.mMinLOD,
	       tex.mLODBias);
	if (gTexCalls == 0) // numColors != 0 but not an index texture: no texture object is initialised
		printf("  gx calls=0 w=0 h=0 fmt=0 ws=0 wt=0 mip=0 tlut=0 imgoff=0 minlod=0 maxlod=0 bias=0 bc=0 edge=0 an=0\n");
	else
		printf("  gx calls=%d w=%d h=%d fmt=%d ws=%d wt=%d mip=%d tlut=%d imgoff=%d minlod=%g maxlod=%g bias=%g bc=%d "
		       "edge=%d an=%d\n",
		       gTexCalls, gTex.w, gTex.h, gTex.fmt, gTex.wrapS, gTex.wrapT, gTex.mip, gTex.tlut,
		       (int)(gTex.image - buf), gTex.minLod, gTex.maxLod, gTex.bias, gTex.biasClamp, gTex.edgeLod, gTex.aniso);
	if (gTlutCalls)
		printf("  tlut calls=%d fmt=%d n=%d off=%d\n", gTlutCalls, gTlut.fmt, gTlut.n, (int)(gTlut.data - buf));

	// idempotency: a second swap and a second store must change nothing
	u8 before[0x20];
	memcpy(before, buf, 0x20);
	swapResTIMG(h);
	tex.storeTIMG(h);
	if (memcmp(before, buf, 0x20) != 0)
		printf("  IDEMPOTENCY-FAIL header changed on second swap\n");
	if (fnv(buf + 0x20, n > 0x20 ? n - 0x20 : 0) != imgHash)
		printf("  IMAGE-DATA-CHANGED\n");
}

static void doTex1(const std::string& line)
{
	auto f   = split(line);
	size_t n = 0;
	u8* buf  = loadFile(f[0], &n);
	size_t off = (size_t)strtoul(f[1].c_str(), nullptr, 10);
	int cnt    = atoi(f[2].c_str());
	printf("TEX1 %s %zu\n", fs::path(f[0]).filename().string().c_str(), off);
	for (int i = 0; i < cnt; i++) {
		ResTIMG* h = (ResTIMG*)(buf + off + i * 0x20);
		swapResTIMG(h);
		u8 before[0x20];
		memcpy(before, h, 0x20);
		swapResTIMG(h);
		char tag[16];
		snprintf(tag, sizeof(tag), "  [%d]", i);
		dumpTimg(tag, h);
		if (memcmp(before, h, 0x20) != 0)
			printf("  IDEMPOTENCY-FAIL\n");
	}
}

// ---- BFN -----------------------------------------------------------------------------------------------------------

static void doBfn(const std::string& path)
{
	size_t n;
	u8* buf = loadFile(path, &n);
	// the glyph sheets and the width pairs must stay untouched: hash them in file order before the swap
	JUTResFont font((ResFONT*)buf, nullptr);
	u8 head[0x20];
	memcpy(head, buf, 0x20);
	JUTResFont font2((ResFONT*)buf, nullptr); // the same font again (every J2DTextBox of an archive does this)
	if (memcmp(head, buf, 0x20) != 0)
		printf("IDEMPOTENCY-FAIL\n");

	const ResFONT* r = font.getResFont();
	printf("BFN %s\n", fs::path(path).filename().string().c_str());
	printf("  magic=%016llx filesize=%u blocks=%u\n", (unsigned long long)r->magic, r->filesize, r->numBlocks);
	auto* inf = font.mInfoBlock;
	printf("  INF1 type=%d ascent=%d descent=%d width=%d leading=%d defcode=%d max=%d\n", inf->fontType, inf->ascent,
	       inf->descent, inf->width, inf->leading, inf->defaultCode, font.mMaxCode);
	printf("  counts wid=%d gly=%d map=%d\n", font.mWidthBlockNum, font.mGlyphBlockNum, font.mMapBlockNum);
	for (int i = 0; i < font.mWidthBlockNum; i++) {
		auto* w = font.mpWidthBlocks[i];
		int cnt = w->endCode - w->startCode + 1;
		printf("  WID1 size=%u start=%d end=%d hash=%08x\n", w->size, w->startCode, w->endCode,
		       fnv(w->mChunkNum, (size_t)cnt * 2));
	}
	for (int i = 0; i < font.mMapBlockNum; i++) {
		auto* m = font.mpMapBlocks[i];
		u16* tab = &m->mLeading;
		u32 h    = 2166136261u;
		int words = ((int)m->size - 0x10) / 2;
		for (int k = 0; k < words; k++) {
			h = (h ^ (tab[k] & 0xff)) * 16777619u;
			h = (h ^ (tab[k] >> 8)) * 16777619u;
		}
		printf("  MAP1 size=%u method=%d start=%d end=%d n=%d hash=%08x\n", m->size, m->mappingMethod, m->startCode,
		       m->endCode, m->numEntries, h);
	}
	for (int i = 0; i < font.mGlyphBlockNum; i++) {
		auto* g = font.mpGlyphBlocks[i];
		printf("  GLY1 size=%u start=%d end=%d cw=%d ch=%d tsize=%u fmt=%d rows=%d cols=%d tw=%d th=%d pad=%d "
		       "data=%08x\n",
		       g->size, g->startCode, g->endCode, g->cellWidth, g->cellHeight, g->textureSize, g->textureFormat,
		       g->numRows, g->numColumns, g->textureWidth, g->textureHeight, g->padding,
		       fnv(g->data, g->size - 0x20));
	}
	// character lookups (decomp getFontCode / getWidthEntry on the host structs)
	static const int codes[] = { 0x20, 0x31, 0x41, 0x7a, 0x8140, 0x8250, 0x82a0, 0x8340, 0x889f, 0x97ca, 0x9fdc, 0xdead };
	for (int c : codes) {
		JUTFont::TWidth w;
		font.getWidthEntry(c, &w);
		printf("  code %04x -> font=%d w=%d,%d\n", c, font.getFontCode(c), w.field_0x0, w.field_0x1);
	}
}

// ---- BLO -----------------------------------------------------------------------------------------------------------

struct PeekWindow : J2DWindow {
	static void dump(J2DWindow* w)
	{
		PeekWindow* p = (PeekWindow*)w;
		const JUTRect& c = p->mContentsBounds;
		printf(" | win cb=%d,%d,%d,%d", c.x1, c.y1, c.x2, c.y2);
		Texture* t[5] = { p->unk100, p->unk104, p->unk108, p->unk10C, p->unk110 };
		for (int i = 0; i < 5; i++) {
			if (t[i])
				printf(" t%d=%d,%d,%d", i, t[i]->getTexInfo()->format, t[i]->getWidth(), t[i]->getHeight());
			else
				printf(" t%d=-", i);
		}
		printf(" pal=%d flags=%d c118=%08x c120=%08x c11c=%08x c124=%08x c12c=%08x c128=%08x size=%d,%d", p->unkFC != nullptr,
		       p->unk114, (u32)p->unk118, (u32)p->unk120, (u32)p->unk11C, (u32)p->unk124, (u32)p->unk12C,
		       (u32)p->unk128, p->unk130, p->unk134);
	}
};

static void dumpPane(J2DPane* p, int depth)
{
	printf("P d=%d k=%08x vis=%d user=%08x b=%d,%d,%d,%d rot=%d base=%d rox=%d roy=%d alpha=%d infl=%d", depth, p->mKind,
	       p->mVisible, p->mUserInfoTag, p->mBounds.x1, p->mBounds.y1, p->mBounds.x2, p->mBounds.y2, (int)p->mRotation,
	       (int)p->mBasePosition, p->mRotateOffsetX, p->mRotateOffsetY, p->mAlpha, p->mIsInfluencedAlpha);
	if (p->mKind == 'PIC1') {
		J2DPicture* pic = (J2DPicture*)p;
		printf(" | pic nt=%d", pic->mTextureNum);
		if (pic->mTextureNum) {
			const ResTIMG* t = pic->mTextures[0]->getTexInfo();
			printf(" tex=%d,%d,%d,%d", t->format, t->width, t->height, t->alphaEnabled);
		}
		printf(" pal=%d bind=%d mirror=%d flip=%d wh=%d wv=%d black=%08x white=%08x c=%08x,%08x,%08x,%08x",
		       pic->mPalette != nullptr, pic->mBinding, pic->mMirror, pic->unk130, pic->mWrapmodeHor, pic->mWrapmodeVer,
		       (u32)pic->mBlack, (u32)pic->mWhite, (u32)pic->mCornerColor[0], (u32)pic->mCornerColor[1],
		       (u32)pic->mCornerColor[2], (u32)pic->mCornerColor[3]);
	} else if (p->mKind == 'WIN1') {
		PeekWindow::dump((J2DWindow*)p);
	} else if (p->mKind == 'TBX1') {
		J2DTextBox* t = (J2DTextBox*)p;
		printf(" | tbx");
		if (t->mFont) {
			JUTResFont* f = (JUTResFont*)t->mFont;
			printf(" font=%d,%d,%d,%d,%d", f->getAscent(), f->getDescent(), f->getWidth(), f->getLeading(),
			       f->getFontType());
		} else {
			printf(" font=-");
		}
		printf(" cc=%08x gc=%08x hb=%d vb=%d cs=%d ls=%d fsx=%d fsy=%d black=%08x white=%08x text=", (u32)t->mCharColor,
		       (u32)t->mGradColor, t->mHBinding, t->mVBinding, t->mCharSpace, t->mLineSpace, t->mFontSizeX,
		       t->mFontSizeY, (u32)t->mBlack, (u32)t->mWhite);
		for (const char* s = t->mText; *s; s++)
			printf("%02x", (u8)*s);
	}
	printf("\n");
	for (auto* c = p->mPaneTree.getFirstChild(); c; c = c->getNextChild())
		dumpPane(c->getObject(), depth + 1);
}

struct TestScreen : J2DScreen {
	void load(JSUMemoryInputStream* s) { makeHiearachyPanes(this, s, false, true, false, nullptr); }
	u32 color() const { return (u32)mColor; }
};

static void doBlo(const std::string& line)
{
	auto f = split(line);
	setArchiveDir(f[1]);
	size_t n;
	u8* buf = loadFile(f[0], &n);
	JSUMemoryInputStream stream(buf, (s32)n);
	TestScreen screen;
	screen.load(&stream);
	printf("BLO %s\n", fs::path(f[0]).filename().string().c_str());
	printf("S color=%08x bounds=%d,%d,%d,%d\n", screen.color(), screen.mBounds.x1, screen.mBounds.y1,
	       screen.mBounds.x2, screen.mBounds.y2);
	for (auto* c = screen.mPaneTree.getFirstChild(); c; c = c->getNextChild())
		dumpPane(c->getObject(), 0);
}

// ---- BMG -----------------------------------------------------------------------------------------------------------

static void doBmg(const std::string& path)
{
	setArchiveDir(fs::path(path).parent_path().string());
	std::string name = fs::path(path).filename().string();

	void* res0 = findRes(name.c_str());
	const u8* raw = (const u8*)res0;
	int count0 = (raw[0x28] << 8) | raw[0x29];
	int esz    = (raw[0x2A] << 8) | raw[0x2B];
	if (esz != 12) {
		// TMessageLoader reads 12 byte records only; SMSGetMessageData honours the entry size of the file
		printf("BMG %s\n  count=%d esz=%d\n", name.c_str(), count0, esz);
		for (int i = 0; i < count0; i++) {
			const char* t = SMSGetMessageData(res0, i);
			printf("  [%d] util=%08x\n", i, t ? fnv((const u8*)t, strlen(t)) : 0);
		}
		return;
	}
	TMessageLoader byCtor(name.c_str());
	TMessageLoader byLoad;
	u32 fileId = byLoad.loadMessageData(name.c_str());
	void* res  = findRes(name.c_str());
	printf("BMG %s\n", name.c_str());
	printf("  count=%d fileId=%u ctorCount=%d\n", byLoad.unk0, fileId, byCtor.unk0);
	for (int i = 0; i < byLoad.unk0; i++) {
		auto* e = byLoad.getMessageEntry(i);
		const u8* text = byLoad.getMessageData() + e->unk0;
		u32 h = fnv(text, strlen((const char*)text));
		const char* viaUtil = SMSGetMessageData(res, i);
		u32 hu = viaUtil ? fnv((const u8*)viaUtil, strlen(viaUtil)) : 0;
		printf("  [%d] off=%u a=%d b=%d tail=%02x%02x%02x%02x text=%08x util=%08x\n", i, e->unk0, e->unk4, e->unk6,
		       (u8)e->unk8[0], (u8)e->unk8[1], (u8)e->unk8[2], (u8)e->unk8[3], h, hu);
	}
	printf("  oob=%d\n", byLoad.getMessageEntry(byLoad.unk0) == nullptr);
	// the loaded buffer was only read: a second constructor must see the identical table
	TMessageLoader again(name.c_str());
	bool same = again.unk0 == byLoad.unk0 && memcmp(again.unk8, byLoad.unk8, sizeof(TMessageLoader::EntryInfo) * again.unk0) == 0;
	printf("  reload=%d\n", same);
}

// ---- name tables -----------------------------------------------------------------------------------------------------

static void doNtab(const std::string& line)
{
	auto f = split(line);
	static std::map<std::string, u8*> files;
	u8*& buf = files[f[0]];
	if (!buf)
		buf = loadFile(f[0]);
	size_t off = (size_t)strtoul(f[1].c_str(), nullptr, 10);
	ResNTAB* tab = (ResNTAB*)(buf + off);
	JUTNameTab a(tab);
	JUTNameTab b(tab); // second construction of the same table (shared buffers / re-loads)
	u32 hk = 2166136261u, hn = 2166136261u;
	int bad = 0;
	int num = a.getResNameTable()->mEntryNum;
	for (u16 i = 0; i < num; i++) {
		hk = fnv((const u8*)&tab->mEntries[i].mKeyCode, 2, hk);
		hk = fnv((const u8*)&tab->mEntries[i].mOffs, 2, hk);
		const char* nm = b.getName(i);
		hn = fnv((const u8*)nm, strlen(nm) + 1, hn);
		if (a.getIndex(nm) < 0 || strcmp(a.getName((u16)a.getIndex(nm)), nm) != 0)
			bad++;
	}
	printf("NTAB %s %zu n=%d keys=%08x names=%08x bad=%d\n", fs::path(f[0]).filename().string().c_str(), off, num,
	       hk, hn, bad);
}

int main(int argc, char** argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: j2djut_check bti|tex1|bfn|blo|bmg|ntab <manifest>\n");
		return 2;
	}
	std::string mode = argv[1];
	for (auto& l : readManifest(argv[2])) {
		if (mode == "bti")
			doBti(l);
		else if (mode == "tex1")
			doTex1(l);
		else if (mode == "bfn")
			doBfn(l);
		else if (mode == "blo")
			doBlo(l);
		else if (mode == "bmg")
			doBmg(l);
		else if (mode == "ntab")
			doNtab(l);
	}
	return 0;
}
