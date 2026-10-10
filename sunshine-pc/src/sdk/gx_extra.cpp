// GX leftovers: SDK functions the game calls that Aurora's GX layer does not provide.
//
// Sources: original semantics from sms/src/dolphin/gx/*.c (the user's decompilation); texture/FIFO plumbing built on
// Aurora (MIT) public entry points only (no Aurora-private headers).
//
// NOT defined on purpose (the link error is the honest signal, see report):
//   GXPeekARGB, GXReadPixMetric, GXClearPixMetric
// They need GPU readback / per-pixel counting that Aurora has no primitive for, and the game uses the results for
// gameplay (Mario occlusion -> camera, goop cleaning degree), so a constant stub would silently break the game.

#include <atomic>

#include <dolphin/gx.h>
#include <dolphin/gx/GXAurora.h> // AuroraGXSync

extern "C" {

// ---------------------------------------------------------------------------------------------------------------
// Draw sync tokens.
//
// Original: GXSetDrawSync(token) writes the token into the GP FIFO; when the GP reaches it, a PE interrupt calls the
// callback registered with GXSetDrawSyncCallback. TDrawSyncManager additionally parks the GP on a FIFO break point
// (GXEnableBreakPt) so the GP cannot run ahead of the token before the callback reads GP state (pixel metrics, EFB).
//
// Port: the token is delivered synchronously from GXSetDrawSync. AuroraGXSync() flushes pending GX state and blocks
// until the FIFO worker has executed every command issued so far; since this thread is the only producer, nothing
// after the token exists yet when the callback runs. That gives the same guarantee the break point existed for
// (callback sees the effect of all earlier commands and none of the later ones), at the cost of one worker round-trip
// per token.
// ponytail: asynchronous delivery needs a token hook in Aurora's command processor; add it if the round-trip shows up
// in profiles (TDrawSyncManager issues ~2 tokens per pollution layer/object per frame).
// ---------------------------------------------------------------------------------------------------------------

static std::atomic<GXDrawSyncCallback> sDrawSyncCallback{nullptr};

GXDrawSyncCallback GXSetDrawSyncCallback(GXDrawSyncCallback cb)
{
	return sDrawSyncCallback.exchange(cb, std::memory_order_acq_rel);
}

void GXSetDrawSync(u16 token)
{
	// Without a callback the token is unobservable (GXReadDrawSync is not used by the game).
	GXDrawSyncCallback cb = sDrawSyncCallback.load(std::memory_order_acquire);
	if (cb == nullptr)
		return;
	AuroraGXSync();
	cb(token);
}

// Break points only park the GP so a draw sync callback sees a stable GP; GXSetDrawSync above already guarantees it.
void GXEnableBreakPt(void* break_pt) { (void)break_pt; }

void GXDisableBreakPt(void) { }

// GXSetDrawDone() (Aurora) publishes a DRAWDONE command; waiting for it means waiting for the FIFO worker to consume
// everything queued so far, which is what AuroraGXSync() does (without raising Aurora's draw-done callback again).
void GXWaitDrawDone(void) { AuroraGXSync(); }

// ---------------------------------------------------------------------------------------------------------------
// Frame buffer copy configuration
// ---------------------------------------------------------------------------------------------------------------

// Hardware-only copy modes. Aurora resolves the EFB into textures 1:1 (GXCopyDisp is a no-op, GXSetCopyFilter and
// GXSetDispCopyYScale are no-ops), so there is no top/bottom filter line to clamp and no interlaced field to select.
void GXSetCopyClamp(GXFBClamp clamp) { (void)clamp; }

void GXSetDispCopyFrame2Field(GXCopyMode mode) { (void)mode; }

// XFB line math, identical to the original (pure computation, no hardware access).
static u32 GetNumXfbLines(u32 height, u32 scale)
{
	u32 numLines     = (height - 1) * 0x100;
	// PowerPC divwu by zero yields 0 (scale is 0 when 256/yScale is a multiple of 512); x86 would trap.
	u32 actualHeight = (scale ? numLines / scale : 0) + 1;

	u32 newScale = scale;
	if (newScale > 0x80 && newScale < 0x100) {
		while (newScale % 2 == 0)
			newScale /= 2;

		if (height % newScale == 0)
			actualHeight++;
	}

	if (actualHeight > 0x400)
		actualHeight = 0x400;

	return actualHeight;
}

u16 GXGetNumXfbLines(u16 efbHeight, float yScale)
{
	u32 scale = (u32)(256.0f / yScale) & 0x1FF;
	return GetNumXfbLines(efbHeight, scale);
}

f32 GXGetYScaleFactor(u16 efbHeight, u16 xfbHeight)
{
	u32 height1 = xfbHeight;
	f32 scale1  = (f32)xfbHeight / (f32)efbHeight;
	u32 scale   = (u32)(256.0f / scale1) & 0x1FF;
	u32 height2 = GetNumXfbLines(efbHeight, scale);

	while (height2 > xfbHeight) {
		height1--;
		scale1  = (f32)height1 / (f32)efbHeight;
		scale   = (u32)(256.0f / scale1) & 0x1FF;
		height2 = GetNumXfbLines(efbHeight, scale);
	}

	f32 scale2 = scale1;
	while (height2 < xfbHeight) {
		scale2 = scale1;
		height1++;
		scale1  = (f32)height1 / (f32)efbHeight;
		scale   = (u32)(256.0f / scale1) & 0x1FF;
		height2 = GetNumXfbLines(efbHeight, scale);
	}

	return scale2;
}

// ---------------------------------------------------------------------------------------------------------------
// Textures
// ---------------------------------------------------------------------------------------------------------------

// Built on Aurora's per-field getters because the GXTexObj layout is Aurora's, not the SDK's.
// image_ptr is the CPU pointer given to GXInitTexObj (the original returned the 32-byte-shifted physical address),
// which is also the key Aurora uses to match EFB copy textures (GXCopyTex destination).
void GXGetTexObjAll(const GXTexObj* obj, void** image_ptr, u16* width, u16* height, GXTexFmt* format,
                    GXTexWrapMode* wrap_s, GXTexWrapMode* wrap_t, u8* mipmap)
{
	*image_ptr = GXGetTexObjData(obj);
	*width     = GXGetTexObjWidth(obj);
	*height    = GXGetTexObjHeight(obj);
	*format    = GXGetTexObjFmt(obj);
	*wrap_s    = GXGetTexObjWrapS(obj);
	*wrap_t    = GXGetTexObjWrapT(obj);
	*mipmap    = GXGetTexObjMipMap(obj) ? 1 : 0;
}

// GXTexRegion is a plain 16-byte record (TMEM image1/image2 register words + flags) in the SDK and in Aurora.
// Aurora has no TMEM and ignores regions when loading texture objects, but the game keeps these in J3DSys and passes
// the same TMEM layout on to JRNLoadTexCached, so the record is filled exactly as the original does.
struct TexRegionInt {
	u32 image1;
	u32 image2;
	u16 sizeEven;
	u16 sizeOdd;
	u8 is32bMipmap;
	u8 isCached;
};
static_assert(sizeof(TexRegionInt) <= sizeof(GXTexRegion), "GXTexRegion too small");

static u32 TexCacheWidthExp2(GXTexCacheSize size)
{
	switch (size) {
	case GX_TEXCACHE_32K:
		return 3;
	case GX_TEXCACHE_128K:
		return 4;
	case GX_TEXCACHE_512K:
		return 5;
	default: // GX_TEXCACHE_NONE (the original leaves this uninitialised for the even half, 0 for the odd half)
		return 0;
	}
}

// Same field update as the SDK's SET_REG_FIELD: clear the field, then OR the (unmasked) value in.
static u32 SetRegField(u32 reg, u32 size, u32 shift, u32 val)
{
	return (reg & ~(((1u << size) - 1) << shift)) | (val << shift);
}

void GXInitTexCacheRegion(GXTexRegion* region, u8 is_32b_mipmap, u32 tmem_even, GXTexCacheSize size_even,
                          u32 tmem_odd, GXTexCacheSize size_odd)
{
	TexRegionInt* t = (TexRegionInt*)region;

	u32 widthExp2 = TexCacheWidthExp2(size_even);
	t->image1     = 0;
	t->image1     = SetRegField(t->image1, 15, 0, tmem_even >> 5);
	t->image1     = SetRegField(t->image1, 3, 15, widthExp2);
	t->image1     = SetRegField(t->image1, 3, 18, widthExp2);
	t->image1 &= 0xFFDFFFFF;

	widthExp2 = TexCacheWidthExp2(size_odd);
	t->image2 = 0;
	t->image2 = SetRegField(t->image2, 15, 0, tmem_odd >> 5);
	t->image2 = SetRegField(t->image2, 3, 15, widthExp2);
	t->image2 = SetRegField(t->image2, 3, 18, widthExp2);

	t->is32bMipmap = is_32b_mipmap;
	t->isCached    = 1;
}

// ---------------------------------------------------------------------------------------------------------------
// Transform / misc
// ---------------------------------------------------------------------------------------------------------------

// Loads the 3x3 normal matrix #id (SDK numbering: id = 3 * matrix slot) from the GX_NRM_MTX_ARRAY at index mtx_indx.
// Aurora's command processor already executes indexed XF loads (opcode 0x28 = INDX_B) but has no front end for the
// normal-matrix form. The only public way to append raw bytes to its FIFO is GXCallDisplayList, which copies them
// verbatim (and flushes pending state first, which is harmless before a matrix load).
void GXLoadNrmMtxIndx3x3(u16 mtx_indx, u32 id)
{
	const u32 offset = id * 3 + 0x400;
	const u32 reg    = (offset & 0xFFF) | (8u << 12) | ((u32)mtx_indx << 16); // 9 words at XF address `offset`
	const u8 cmd[5]  = { 0x28, (u8)(reg >> 24), (u8)(reg >> 16), (u8)(reg >> 8), (u8)reg };
	GXCallDisplayList((void*)cmd, sizeof(cmd));
}

// GX_MT_XF_FLUSH only schedules the hardware XF-pipeline flush primitive (Aurora's __GXSendFlushPrim does nothing).
// GX_MT_DL_SAVE_CONTEXT only changes what GXBeginDisplayList/GXEndDisplayList save; nothing in the game records
// display lists (GXBeginDisplayList has no caller in sms/src), so there is nothing to configure.
void GXSetMisc(GXMiscToken token, u32 val)
{
	(void)token;
	(void)val;
}

// PE alpha-read mode only changes what a CPU EFB peek/poke returns for the alpha channel of formats without alpha.
// Aurora has no CPU EFB color access (see GXPeekARGB above), so there is no read path it could affect.
void GXPokeAlphaRead(GXAlphaReadMode mode) { (void)mode; }

} // extern "C"
