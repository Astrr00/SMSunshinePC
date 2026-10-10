// Hx wiper library: the screen-wipe transitions behind GC2D/ScrnFader (TSMSFader::requestWipe / drawWipe).
//
// Provenance
//   The original hx_wiper.c is not part of the decomp sources. This file is translated function by function from the
//   retail PowerPC assembly (sms/build/GMSJ01/asm/GC2D/hx_wiper.s), so state transitions, frame counts and the order
//   of GX calls are those of the original. Nothing is guessed from the callers; deviations are listed under "PC".
//
// What the game observes (TSMSFader, TMovieDirector)
//   Hx_ResetWipe(w, h)        state 0, display size, forgets provided resources.
//   Hx_StartWipe(idx, n)      state 1 (armed); the next Hx_UpdateWipe latches handler + direction for idx.
//   Hx_GetWipeType(idx)       1: the wipe ends with the screen visible (TSMSFader: FADING_IN -> FULLY_FADED_IN),
//                             0: it ends covered (FADING_OUT -> FULLY_FADED_OUT).
//   Hx_UpdateWipe(rate)       draws one frame; returns the state: 0 idle, 2 running, 3 finished. Once finished a
//                             type-0 wipe keeps drawing an opaque black screen, a type-1 wipe draws nothing.
//   Hx_MovieStartSyncEx()     only while idx == 12 (title logo): 1 = logo pen starts drawing (sound cue),
//                             2 = the movie may start.
//   Hx_ProvideResource(Ex)/Hx_RemoveResource   BTI images used by the logo (12) and game-over (13) wipes.
//
// Wipe table (idx -> handler, type, number of Hx_UpdateWipe calls until it returns 3)
//    0  none (never finishes)         type 0     1  Hx_Circle  type 1  30      2  Hx_Circle  type 0  25
//    3  Hx_Test1  type 1  27          4  Hx_Test1  type 0  27     5  Hx_Test5  type 1  20     6  Hx_Test5  type 0  20
//    7  Hx_Test4  type 1  38          8  Hx_Test4  type 0  38     9  Hx_Test2R type 1  44    10  Hx_Test2  type 0  45
//   11  Hx_Door   type 0  34         12  Hx_Logo   type 1 205    13  Hx_GameOver type 0 213  14  none (never finishes)
//   Indices 14..17 are plain colour fades handled by TSMSFader itself and never reach this library.
//   The duration passed to Hx_StartWipe and the rate passed to Hx_UpdateWipe are stored but not used by any handler:
//   every wipe runs for a fixed number of updates (one update per drawn frame).
//
// PC
//   - Hx_GetWipeType(idx >= 15) returns 0 and Hx_UpdateWipe maps idx >= 15 to the empty handler (the original indexes
//     its tables unchecked).
//   - GXEnd() follows every GXBegin (the original has none; Aurora asserts on a missing GXEnd).
//   - Hx_CameraInit builds its vectors locally instead of writing the screen centre into the .data tables.
//   - The BTI header (ResTIMG) is read as host-endian data like everywhere else in the game code.
//   - Hx_SetVFilter ends in GXSetCopyFilter, which Aurora implements as a no-op: the vertical blur of the
//     fade-in circle (idx 1) is not reproduced.
//   - Float -> unsigned conversions the PowerPC runtime saturates (__cvt_fp2unsigned) are saturated explicitly where
//     the value can leave the range (circle rings, game over table); where it cannot (row/segment counts that stay
//     within 0..500) a plain cast is used. frsqrte-based square roots are sqrtf; fused multiply-adds are plain a*b+c except Hxs1_Circle (fmaf).
//   - Hxs_PenDraw's stack vector has an uninitialised z in the original; it is 0 here.
//   - Hgx_ReadTexture keeps the original developer fallback (DVD path /data/...), used only when no resource was
//     provided; the game always provides one through TSMSFader.

// ---- shared head of hx_wiper.cpp (outside namespace hxi) ----
#include <math.h>

#include <GC2D/hx_wiper.h>
#include <MarioUtil/ReinitGX.hpp>
#include <dolphin/dvd.h>
#include <dolphin/gx.h>
#include <dolphin/mtx.h>
#include <dolphin/os.h>

namespace hxi {

// ---- shared body (inside namespace hxi). Offsets are those of the original `hx` bss object (0x64 bytes). ----

// Hx_MotionSet/Hx_MotionUpdate state: a trapezoidal velocity profile stepped once per update.
struct HxMotion {
	f32 tAccel;  // 0x00 end of acceleration phase (frames)
	f32 tCruise; // 0x04 end of constant-speed phase
	f32 tEnd;    // 0x08 end of deceleration phase
	f32 accel;   // 0x0C
	f32 unk10;   // 0x10 only ever cleared
	f32 decel;   // 0x14
	f32 vel;     // 0x18
	f32 time;    // 0x1C
	f32 pos;     // 0x20
};

struct HxState {
	u32 width;      // 0x00 display size given to Hx_ResetWipe
	u32 height;     // 0x04
	u32 halfWidth;  // 0x08
	u32 halfHeight; // 0x0C
	u8 state;       // 0x10 0 idle, 1 started, 2 running, 3 finished
	u8 idx;         // 0x11 wipe index given to Hx_StartWipe
	u8 type;        // 0x12 handle_type[idx] latched when the wipe starts (0 ends opaque, 1 ends transparent)
	f32 elapsed;    // 0x14 sum of the dt passed to Hx_UpdateWipe since Hx_StartWipe
	f32 dt;         // 0x18 last dt passed to Hx_UpdateWipe
	u32 duration;   // 0x1C frames passed to Hx_StartWipe
	void (*handler)(); // 0x20 handle_table[idx]
	u32 hasRes;     // 0x24 Hx_ProvideResource called
	u32 hasResEx;   // 0x28 Hx_ProvideResourceEx called
	void* res;      // 0x2C resource passed to Hx_ProvideResource, else &hx_buffer
	void* resEx;    // 0x30 resource passed to Hx_ProvideResourceEx
	u32 resParam;   // 0x34 second argument of Hx_ProvideResource, else 0x3300 (size of hx_buffer)
	u32 stage;      // 0x38 handler-private stage counter (0 on start)
	u32 timer;      // 0x3C handler-private countdown, see Hx_TimerCountDown
	HxMotion motion; // 0x40
};

extern HxState hx;                          // bss object `hx`
extern u8 hx_buffer[0x3300];                // bss object `hx_buffer` (32-byte aligned)
extern void* fbuf;                          // .sdata: &hx_buffer
extern void* fbuf2;                         // .sdata: &hx_buffer
extern void* gmover_tex_buffer;             // .sdata: &hx_buffer
extern u16 img_wx, img_wy;                  // .sbss: size of the last BTI given to Hgx_init_tobj_resource
extern int hxs_logo_resetflag;              // .sbss: written by Hx_Logo, consumed by Hx_MovieStartSyncEx
extern int hxs_logodraw_resetflag;          // .sbss: same
extern const f32 camLoc[3];                 // .data camLoc$96 {320, 240, -30}
extern const f32 objPt[3];                  // .data objPt$97  {320, 240, 0}
extern const f32 up[3];                     // .data up$98     {0, -10, 0}

// Core helpers (hx_wiper.cpp core section)
void Hx_MotionSet(HxMotion* m, f32 dist, f32 tAccel, f32 tCruise, f32 tDecel);
f32 Hx_MotionUpdate(HxMotion* m);
u32 Hx_TimerCountDown();
void Hx_Warning(int);
void dummy_handler();

// Infrastructure (part A)
void Hx_CameraInit();
void Hx_GxInit(int mode, int blend); // mode 0: no texgen, 1 tev stage w/o tex; mode 1: one texgen + vtx desc POS/CLR0/TEX0; blend 1 = alpha blend, 0 = none
void Hx_SetVFilter(f32);
void Hx_GetFrBuffer(void* dest, u16 left, u16 top, u16 wd, u16 ht);
void Hx_FrBufferMorf(f32);
void Frb2_RendBox(u32 color, f32 x1, f32 y1, f32 x2, f32 y2);
void Frb2_InitBlackBox();
void Frb2_InitGx(GXTexObj* obj);
void Hxs_FrBufferMorf2(f32);
void Hxs_FrBufferMorf2B(f32);
void Hgx_ReadTexture(const char* path, void* dst);
void Hgx_init_tobj_resource(GXTexObj* obj, const void* bti);

// Wipe handlers (each is one entry of handle_table; called once per Hx_UpdateWipe)
void Hx_Circle();   // part B
void Hx_Door();     // part B
void Hx_Test5();    // part C1
void Hx_Test4();    // part C1
void Hx_Test2R();   // part C2
void Hx_Test2();    // part C2
void Hx_Test1();    // part C2
void Hx_Logo();     // part D
void Hx_GameOver(); // part E

// ---- core (inside namespace hxi) ----

HxState hx;                                  // .bss  hx
alignas(32) u8 hx_buffer[0x3300];            // .bss  hx_buffer
void* fbuf = hx_buffer;                      // .sdata
void* fbuf2 = hx_buffer;                     // .sdata
void* gmover_tex_buffer = hx_buffer;         // .sdata
u16 img_wx, img_wy;                          // .sbss
int hxs_logo_resetflag;                      // .sbss
int hxs_logodraw_resetflag;                  // .sbss
const f32 camLoc[3] = { 320.0f, 240.0f, -30.0f };
const f32 objPt[3]  = { 320.0f, 240.0f, 0.0f };
const f32 up[3]     = { 0.0f, -10.0f, 0.0f };

// Wipe index -> handler and wipe type. Type 1 wipes end with the screen fully visible (TSMSFader reports
// FULLY_FADED_IN), type 0 wipes end covered (FULLY_FADED_OUT). Indices 14..17 never get here: TSMSFader handles them
// itself as plain fades. Index 0 and 14 have no handler and therefore never finish.
constexpr int kWipeCount = 15;
void (*const handle_table[kWipeCount])() = {
	dummy_handler, Hx_Circle, Hx_Circle, Hx_Test1, Hx_Test1, Hx_Test5, Hx_Test5, Hx_Test4,
	Hx_Test4,      Hx_Test2R, Hx_Test2,  Hx_Door,  Hx_Logo,  Hx_GameOver, dummy_handler,
};
const u8 handle_type[kWipeCount] = { 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 0 };

void dummy_handler() { }

// Debug hook in the original (empty `blr` in the retail build); the argument is a warning code.
void Hx_Warning(int) { }

// Trapezoidal velocity profile: accelerate for tAccel updates, cruise for tCruise, decelerate for tDecel, covering
// `dist` in total (peak speed = 2 * dist / (tAccel + 2 * tCruise + tDecel)). The accel/decel rates are only written
// when their phase length is non-zero, exactly as in the original (a zero phase keeps the previous rate).
void Hx_MotionSet(HxMotion* m, f32 dist, f32 tAccel, f32 tCruise, f32 tDecel)
{
	m->tAccel = tAccel;
	f32 sum   = tAccel + tCruise;
	f32 end1  = m->tAccel + tCruise;
	sum       = tCruise + sum;
	f32 total = tDecel + sum;
	m->tCruise = end1;
	m->tEnd    = m->tCruise + tDecel;

	f32 peak = 2.0f * dist;
	peak     = peak / total;
	if (tAccel != 0.0f)
		m->accel = peak / tAccel;
	if (tDecel != 0.0f)
		m->decel = -peak / tDecel;

	m->unk10 = 0.0f;
	m->vel   = 0.0f;
	m->pos   = 0.0f;
	m->time  = 0.0f;
}

// One step of the profile; returns the distance covered so far.
f32 Hx_MotionUpdate(HxMotion* m)
{
	if (m->tAccel > m->time)
		m->vel = m->vel + m->accel;
	else if (m->tCruise <= m->time)
		m->vel = m->vel + m->decel;

	m->time = m->time + 1.0f;
	m->pos  = m->pos + m->vel;
	return m->pos;
}

// Counts hx.timer down to 0 and returns it.
u32 Hx_TimerCountDown()
{
	if (hx.timer != 0)
		hx.timer = hx.timer - 1;
	return hx.timer;
}


// ---- Part A: infrastructure shared by the wipe handlers (GX/camera setup, framebuffer copies, texture helpers) ----

} // namespace hxi
#include <JSystem/ResTIMG.hpp>
namespace hxi {

// Vertical copy-filter weights: vtable_org is the identity, Hx_SetVFilter shifts weight with dec_step/inc_step.
static const u8 vtable_org[7] = {0x10, 0x10, 0x00, 0x00, 0x00, 0x10, 0x10};
static const u8 dec_step[4]   = {0x00, 0x01, 0x05, 0x06};
static const u8 inc_step[3]   = {0x02, 0x03, 0x04};
static u8 vtable[7];

// Stateless draw helper (no hx.state/stage/timer use). With f < width/4, copies 160x16 framebuffer strips
// taken from x = 3*width/4 and draws each one over x in [3*width/4, width - f], then covers [width - f, width]
// with an opaque black box.
void Hxs_FrBufferMorf2B(f32 f)
{
	GXTexObj tobj;
	u32 left = (hx.width >> 1) + (hx.width >> 2);
	Frb2_InitGx(&tobj);
	f32 quarterW = (f32)(hx.width >> 2);
	f32 x2       = (f32)hx.width - f;
	if (f < quarterW) {
		f32 y = 0.0f;
		f32 x1 = (f32)(s32)left;
		while (y < (f32)hx.height) {
			Hx_GetFrBuffer(fbuf2, left, (u32)y, 0xA0, 0x10);
			GXInvalidateTexAll();
			GXLoadTexObj(&tobj, GX_TEXMAP0);
			GXBegin(GX_QUADS, GX_VTXFMT0, 4);
			GXPosition3f32(x1, y, 0.0f);
			GXColor1u32(0);
			GXTexCoord2f32(0.0f, 0.0f);
			GXPosition3f32(x2, y, 0.0f);
			GXColor1u32(0);
			GXTexCoord2f32(1.0f, 0.0f);
			GXPosition3f32(x2, 16.0f + y, 0.0f);
			GXColor1u32(0);
			GXTexCoord2f32(1.0f, 1.0f);
			GXPosition3f32(x1, 16.0f + y, 0.0f);
			GXColor1u32(0);
			GXTexCoord2f32(0.0f, 1.0f);
			GXEnd();
			GXDrawDone();
			y += 16.0f;
		}
	}
	Frb2_InitBlackBox();
	Frb2_RendBox(0xFF, x2, 0.0f, (f32)hx.width, (f32)hx.height);
}

// Stateless draw helper (no hx.state/stage/timer use). With f < width/4, copies 160x16 framebuffer strips
// taken from x = 0 and draws each one over x in [f, width/4], then covers [0, f] with an opaque black box.
void Hxs_FrBufferMorf2(f32 f)
{
	GXTexObj tobj;
	Frb2_InitGx(&tobj);
	if (f < (f32)(hx.width >> 2)) {
		f32 y = 0.0f;
		while (y < (f32)hx.height) {
			Hx_GetFrBuffer(fbuf2, 0, (u32)y, 0xA0, 0x10);
			GXInvalidateTexAll();
			GXLoadTexObj(&tobj, GX_TEXMAP0);
			GXBegin(GX_QUADS, GX_VTXFMT0, 4);
			GXPosition3f32(f, y, 0.0f);
			GXColor1u32(0);
			GXTexCoord2f32(0.0f, 0.0f);
			GXPosition3f32((f32)(hx.width >> 2), y, 0.0f);
			GXColor1u32(0);
			GXTexCoord2f32(1.0f, 0.0f);
			GXPosition3f32((f32)(hx.width >> 2), 16.0f + y, 0.0f);
			GXColor1u32(0);
			GXTexCoord2f32(1.0f, 1.0f);
			GXPosition3f32(f, 16.0f + y, 0.0f);
			GXColor1u32(0);
			GXTexCoord2f32(0.0f, 1.0f);
			GXEnd();
			GXDrawDone();
			y += 16.0f;
		}
	}
	Frb2_InitBlackBox();
	Frb2_RendBox(0xFF, 0.0f, 0.0f, f, (f32)hx.height);
	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	GXPosition3f32(0.0f, 0.0f, 0.0f);
	GXColor1u32(0xFF);
	GXPosition3f32(f, 0.0f, 0.0f);
	GXColor1u32(0xFF);
	GXPosition3f32(f, (f32)hx.height, 0.0f);
	GXColor1u32(0xFF);
	GXPosition3f32(0.0f, (f32)hx.height, 0.0f);
	GXColor1u32(0xFF);
	GXEnd();
}

// Draws one flat-coloured (packed RGBA) quad with corners (x1,y1)-(x2,y2) at z = 0; vertex format is the one of Frb2_InitBlackBox.
void Frb2_RendBox(u32 color, f32 x1, f32 y1, f32 x2, f32 y2)
{
	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	GXPosition3f32(x1, y1, 0.0f);
	GXColor1u32(color);
	GXPosition3f32(x2, y1, 0.0f);
	GXColor1u32(color);
	GXPosition3f32(x2, y2, 0.0f);
	GXColor1u32(color);
	GXPosition3f32(x1, y2, 0.0f);
	GXColor1u32(color);
	GXEnd();
}

// Vertex/TEV setup for untextured colour quads (POS + CLR0 direct, pass-through colour).
void Frb2_InitBlackBox()
{
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEX_DISABLE, GX_COLOR0A0);
}

// Camera + vertex/TEV setup for the framebuffer-strip draws: loads `obj` as a 160x16 RGB565 texture on fbuf2 into TEXMAP0.
void Frb2_InitGx(GXTexObj* obj)
{
	Hx_CameraInit();
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	Hx_SetVFilter(1.0f);
	GXSetNumTexGens(1);
	GXSetNumTevStages(1);
	GXSetTevOp(GX_TEVSTAGE0, GX_REPLACE);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
	GXSetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
	GXInitTexObj(obj, fbuf2, 0xA0, 0x10, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GXInitTexObjLOD(obj, GX_LINEAR, GX_LINEAR, 0.0f, 10.0f, 0.0f, GX_FALSE, GX_TRUE, GX_ANISO_1);
	GXLoadTexObj(obj, GX_TEXMAP0);
}

// Copies a 48x48 framebuffer block at (x, y) into fbuf and draws it back unscaled at the same place (RGB565, no blend).
static void Hx_FrBufferMorf_impl(u32 x, u32 y)
{
	GXTexObj tobj;
	Hx_CameraInit();
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	Hx_GetFrBuffer(fbuf, x, y, 0x30, 0x30);
	GXInvalidateTexAll();
	GXSetNumTexGens(1);
	GXSetNumTevStages(1);
	GXSetTevOp(GX_TEVSTAGE0, GX_REPLACE);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
	GXSetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
	GXInitTexObj(&tobj, fbuf, 0x30, 0x30, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GXInitTexObjLOD(&tobj, GX_LINEAR, GX_LINEAR, 0.0f, 10.0f, 0.0f, GX_FALSE, GX_TRUE, GX_ANISO_1);
	GXLoadTexObj(&tobj, GX_TEXMAP0);
	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	f32 x0 = (f32)x;
	f32 y0 = (f32)y;
	f32 x1 = (f32)(x + 0x30);
	f32 y1 = (f32)(y + 0x30);
	GXPosition3f32(x0, y0, 0.0f);
	GXColor1u32(0);
	GXTexCoord2f32(0.0f, 0.0f);
	GXPosition3f32(x1, y0, 0.0f);
	GXColor1u32(0);
	GXTexCoord2f32(1.0f, 0.0f);
	GXPosition3f32(x1, y1, 0.0f);
	GXColor1u32(0);
	GXTexCoord2f32(1.0f, 1.0f);
	GXPosition3f32(x0, y1, 0.0f);
	GXColor1u32(0);
	GXTexCoord2f32(0.0f, 1.0f);
	GXEnd();
}

// Sets the vertical copy filter from `f` (0..1+, see Hx_SetVFilter), then re-draws the 48x48 block centred on the screen.
void Hx_FrBufferMorf(f32 f)
{
	Hx_SetVFilter(f);
	Hx_FrBufferMorf_impl(hx.halfWidth - 0x18, hx.halfHeight - 0x18);
}

// Builds the 7-tap vertical copy filter: n = (u8)(64 * f) steps, each moving one unit of weight from an outer tap
// (dec_step[i % 4]) to a centre tap (inc_step[i % 3]), then applies it with GXSetCopyFilter.
void Hx_SetVFilter(f32 f)
{
	for (int i = 0; i < 7; i++) {
		vtable[i] = vtable_org[i];
	}
	u8 n = (u8)(s32)(64.0f * f);
	for (u32 i = 0; i < n; i++) {
		vtable[dec_step[i & 3]]--;
		vtable[inc_step[i % 3]]++;
	}
	GXSetCopyFilter(GX_FALSE, nullptr, GX_TRUE, vtable);
}

// Copies the wd x ht EFB rectangle at (left, top) into `dest` as RGB565 (clear colour 0, z clear 0xFFFFFF).
void Hx_GetFrBuffer(void* dest, u16 left, u16 top, u16 wd, u16 ht)
{
	GXColor clear = {0, 0, 0, 0};
	GXSetTexCopySrc(left, top, wd, ht);
	GXSetTexCopyDst(wd, ht, GX_TF_RGB565, GX_FALSE);
	GXGetTexBufferSize(wd, ht, GX_TF_RGB565, GX_FALSE, 0);
	GXSetCopyClear(clear, 0x00FFFFFF);
	GXCopyTex(dest, GX_TRUE);
	GXPixModeSync();
}

// Reads a whole file from the disc into dst; skipped while a resource was provided with Hx_ProvideResource.
void Hgx_ReadTexture(const char* path, void* dst)
{
	DVDFileInfo fileInfo;
	if (hx.hasRes == 0) {
		if (DVDOpen(const_cast<char*>(path), &fileInfo)) {
			s32 n = DVDReadPrio(&fileInfo, dst, fileInfo.length, 0, 2);
			DVDClose(&fileInfo);
			DCStoreRange(dst, n);
		}
	}
}

// Initializes `obj` from a BTI image (format, wrap modes, filters), records its size in img_wx/img_wy; LODs are all 0.
void Hgx_init_tobj_resource(GXTexObj* obj, const void* bti)
{
	const ResTIMG* img = (const ResTIMG*)bti;
	img_wx = img->width;
	img_wy = img->height;
	GXInitTexObj(obj, (u8*)bti + img->imageDataOffset, img_wx, img_wy, (GXTexFmt)img->format,
	             (GXTexWrapMode)img->wrapS, (GXTexWrapMode)img->wrapT, GX_FALSE);
	GXInitTexObjLOD(obj, (GXTexFilter)img->minFilter, (GXTexFilter)img->magFilter, 0.0f, 0.0f, 0.0f, GX_FALSE,
	                GX_FALSE, GX_ANISO_1);
}

// mode 0: untextured (no texgen, colour pass-through); mode 1: one texgen + POS/CLR0/TEX0 vertex format.
// blend 1: src-alpha blend; blend 0: no blend. Other values leave that part unchanged.
void Hx_GxInit(int mode, int blend)
{
	switch (mode) {
	case 0:
		GXSetNumTexGens(0);
		GXSetNumTevStages(1);
		GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
		GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
		break;
	case 1:
		GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, GX_FALSE, GX_PTIDENTITY);
		GXSetNumTexGens(1);
		GXSetNumTevStages(1);
		GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
		GXClearVtxDesc();
		GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
		GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
		GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
		GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
		GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
		GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
		break;
	}
	switch (blend) {
	case 1:
		GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
		break;
	case 0:
		GXSetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
		break;
	}
}

// 2D setup: ortho projection centred on (width/2, height/2) (near 0, far 100), viewport 640x480, no cull/z/lighting,
// one untextured TEV stage, POS + CLR0 vertex format, camera at z = -30 looking at z = 0.
// The original also stored the centre into the camLoc/objPt tables; nothing else reads them, so locals are used.
void Hx_CameraInit()
{
	Mtx44 proj;
	Mtx view;
	f32 halfW = (f32)(hx.width >> 1);
	f32 halfH = (f32)(hx.height >> 1);
	Vec cam   = {halfW, halfH, camLoc[2]};
	Vec obj   = {halfW, halfH, objPt[2]};
	Vec upDir = {up[0], up[1], up[2]};
	C_MTXOrtho(proj, halfH, -halfH, -halfW, halfW, 0.0f, 100.0f);
	GXSetProjection(proj, GX_ORTHOGRAPHIC);
	GXSetViewport(0.0f, 0.0f, 640.0f, 480.0f, 0.0f, 1.0f);
	C_MTXLookAt(view, &cam, &upDir, &obj);
	GXSetCullMode(GX_CULL_NONE);
	GXSetCoPlanar(GX_FALSE);
	GXSetZMode(GX_FALSE, GX_ALWAYS, GX_FALSE);
	GXSetNumTexGens(0);
	GXSetNumTevStages(1);
	GXSetNumIndStages(0);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GXSetTevOp(GX_TEVSTAGE0, GX_PASSCLR);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GXSetLineWidth(6, GX_TO_ZERO);
	GXLoadPosMtxImm(view, GX_PNMTX0);
	GXSetChanCtrl(GX_COLOR0A0, GX_DISABLE, GX_SRC_VTX, GX_SRC_VTX, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
	GXSetChanCtrl(GX_COLOR1A1, GX_DISABLE, GX_SRC_REG, GX_SRC_REG, GX_LIGHT_NULL, GX_DF_NONE, GX_AF_NONE);
	GXSetNumChans(1);
}


// ---- part B: Hx_Door, Hxs2_Circle, Hxs1_Circle, Hx_Circle ----

// Door wipe (wipe index 11). Stages: 0 starts a motion of width/2 units (5/6/5 frames), 1 runs it and draws
// Hxs_FrBufferMorf2(pos), then restarts the same motion, 2 draws Morf2(width/2) + Morf2B(pos) while the
// second motion runs, 3 draws both with width/2 and sets hx.state = 3. A motion phase advances the stage
// once its position (truncated to int) reaches width/2. hx.timer is not used.
void Hx_Door() {
	switch (hx.stage) {
	case 0:
		hx.stage++;
		Hx_MotionSet(&hx.motion, (f32)(hx.width >> 1), 5.0f, 6.0f, 5.0f);
		break;
	case 1: {
		s32 pos = (s32)Hx_MotionUpdate(&hx.motion);
		Hxs_FrBufferMorf2((f32)pos);
		if ((u32)pos >= (hx.width >> 1)) {
			hx.stage++;
			Hx_MotionSet(&hx.motion, (f32)(hx.width >> 1), 5.0f, 6.0f, 5.0f);
		}
		break;
	}
	case 2: {
		Hxs_FrBufferMorf2((f32)(hx.width >> 1));
		s32 pos = (s32)Hx_MotionUpdate(&hx.motion);
		Hxs_FrBufferMorf2B((f32)pos);
		if ((u32)pos >= (hx.width >> 1)) {
			hx.stage++;
		}
		break;
	}
	case 3:
		Hxs_FrBufferMorf2((f32)(hx.width >> 1));
		Hxs_FrBufferMorf2B((f32)(hx.width >> 1));
		hx.state = 3;
		break;
	default:
		break;
	}
}

// Draws a black ring with alpha `alpha`: rows are scanned from (halfHeight - b) down to the middle, mirrored
// to the bottom half. `a` is the inner radius and `b` the outer radius of the ring.
static void Hxs2_Circle(f32 a, f32 b, u8 alpha) {
	Hx_CameraInit();
	Hx_GxInit(0, 1);

	f32 aa = a * a;
	f32 bb = b * b;

	// __cvt_fp2unsigned: negative -> 0, >= 2^32 -> 0xFFFFFFFF
	f32 start = (f32)hx.halfHeight - b;
	u32 i = start < 0.0f ? 0u : (start >= 4294967296.0f ? 0xFFFFFFFFu : (u32)start);

	for (; i <= hx.halfHeight; i++) {
		f32 d = (f32)(hx.halfHeight - i);
		f32 outer = bb - d * d;
		if (outer > 0.0f) {
			outer = sqrtf(outer);
		}
		f32 y = (f32)i;
		f32 cx = (f32)hx.halfWidth;
		f32 yb = (f32)(hx.height - i);

		if (d >= a) {
			// row misses the inner circle: one solid span through the outer circle, top and bottom
			GXBegin(GX_LINES, GX_VTXFMT0, 4);
			GXPosition3f32(cx - outer, y, 1.0f);
			GXColor1u32(alpha);
			GXPosition3f32(cx + outer, y, 1.0f);
			GXColor1u32(alpha);
			GXPosition3f32(cx - outer, yb, 1.0f);
			GXColor1u32(alpha);
			GXPosition3f32(cx + outer, yb, 1.0f);
			GXColor1u32(alpha);
			GXEnd();
		} else {
			f32 inner = aa - d * d;
			if (inner > 0.0f) {
				inner = sqrtf(inner);
			}
			// row crosses the inner circle: left and right spans, top and bottom
			GXBegin(GX_LINES, GX_VTXFMT0, 8);
			GXPosition3f32(cx - outer, y, 1.0f);
			GXColor1u32(alpha);
			GXPosition3f32(cx - inner, y, 1.0f);
			GXColor1u32(alpha);
			GXPosition3f32(cx + inner, y, 1.0f);
			GXColor1u32(alpha);
			GXPosition3f32(cx + outer, y, 1.0f);
			GXColor1u32(alpha);
			GXPosition3f32(cx + inner, yb, 1.0f);
			GXColor1u32(alpha);
			GXPosition3f32(cx + outer, yb, 1.0f);
			GXColor1u32(alpha);
			GXPosition3f32(cx - outer, yb, 1.0f);
			GXColor1u32(alpha);
			GXPosition3f32(cx - inner, yb, 1.0f);
			GXColor1u32(alpha);
			GXEnd();
		}
	}
}

// Fills everything outside a circle of radius `r` (centered on the screen) with opaque black: for every row
// i in [0, halfHeight] (and its mirror at height - i) it draws the full width if the row misses the circle,
// else the two spans left and right of the chord.
static void Hxs1_Circle(f32 r) {
	Hx_CameraInit();
	Hx_GxInit(0, 1);

	f32 rr = r * r;

	for (u32 i = 0; i <= hx.halfHeight; i++) {
		f32 d = (f32)(hx.halfHeight - i);
		f32 y = (f32)i;
		f32 yb = (f32)(hx.height - i);
		f32 width = (f32)hx.width;

		if (d >= r) {
			GXBegin(GX_LINES, GX_VTXFMT0, 4);
			GXPosition3f32(0.0f, y, 1.0f);
			GXColor1u32(0xFF);
			GXPosition3f32(width, y, 1.0f);
			GXColor1u32(0xFF);
			GXPosition3f32(0.0f, yb, 1.0f);
			GXColor1u32(0xFF);
			GXPosition3f32(width, yb, 1.0f);
			GXColor1u32(0xFF);
			GXEnd();
		} else {
			f32 chord = fmaf(-d, d, rr); // fnmsubs: single rounding
			if (chord > 0.0f) {
				chord = sqrtf(chord);
			}
			f32 cx = (f32)hx.halfWidth;
			GXBegin(GX_LINES, GX_VTXFMT0, 8);
			GXPosition3f32(0.0f, y, 1.0f);
			GXColor1u32(0xFF);
			GXPosition3f32(cx - chord, y, 1.0f);
			GXColor1u32(0xFF);
			GXPosition3f32(cx + chord, y, 1.0f);
			GXColor1u32(0xFF);
			GXPosition3f32(width, y, 1.0f);
			GXColor1u32(0xFF);
			GXPosition3f32(cx + chord, yb, 1.0f);
			GXColor1u32(0xFF);
			GXPosition3f32(width, yb, 1.0f);
			GXColor1u32(0xFF);
			GXPosition3f32(0.0f, yb, 1.0f);
			GXColor1u32(0xFF);
			GXPosition3f32(cx - chord, yb, 1.0f);
			GXColor1u32(0xFF);
			GXEnd();
		}
	}
}

// Circle wipe (wipe indices 1 and 2). hx.type 0: black closes in (r = 400 - motion pos), motion 400 over
// 2/13/10 frames, hx.timer = 25. hx.type 1: hole opens (r = motion pos) with the frame buffer morph fading in,
// motion 400 over 5/10/15 frames, hx.timer = 30. Stage 0 initialises and falls into stage 1 in the same call
// (stage becomes 1); stage 1 updates r each call; when hx.timer reaches 0 it sets stage = 2 and hx.state = 3
// (any call with stage >= 2 only sets hx.state = 3). Every call draws the black fill and up to three soft rings.
static f32 circ_r;    // r$181
static f32 circ_p1;   // p1$182
static f32 circ_p2;   // p2$183
static f32 circ_p3;   // p3$184
static u16 circ_a1;   // a1$185
static u16 circ_a2;   // a2$186
static u16 circ_a3;   // a3$187
static f32 circ_boke; // boke$188

void Hx_Circle() {
	switch (hx.stage) {
	case 0:
		circ_p3 = 0.0f;
		circ_p2 = 0.0f;
		circ_p1 = 0.0f;
		circ_a3 = 0;
		circ_a2 = 0;
		circ_a1 = 0;
		circ_r = 1.0f;
		circ_boke = 0.0f;
		if (hx.type == 0) {
			Hx_MotionSet(&hx.motion, 400.0f, 2.0f, 13.0f, 10.0f);
			hx.timer = 25;
		} else if (hx.type == 1) {
			Hx_MotionSet(&hx.motion, 400.0f, 5.0f, 10.0f, 15.0f);
			hx.timer = 30;
		}
		hx.stage++;
		// fall through
	case 1:
		circ_r = Hx_MotionUpdate(&hx.motion);
		if (hx.type == 1) {
			circ_boke = circ_boke + 0.06666667f;
			if (circ_boke > 1.0f) {
				circ_boke = 1.0f;
			}
			Hx_FrBufferMorf(circ_boke);
			Hx_SetVFilter(1.0f);
		} else if (hx.type == 0) {
			circ_r = 400.0f - circ_r;
			if (circ_r < 0.0f) {
				circ_r = 0.0f;
			}
		}
		if (Hx_TimerCountDown() == 0) {
			hx.stage++;
			hx.state = 3;
		}
		break;
	default:
		hx.state = 3;
		break;
	}

	Hxs1_Circle(circ_r);

	if (circ_r > 22.0f) {
		Hxs2_Circle((circ_r - 20.0f) + circ_p1, circ_r, (u8)(circ_a1 >> 8));
		circ_p1 = circ_p1 + 0.05f;
		if (circ_a1 < 0xFF00) {
			circ_a1 = (u16)(circ_a1 + 0x180);
		}
	}
	if (circ_r > 42.0f) {
		Hxs2_Circle((circ_r - 40.0f) + circ_p2, (circ_r - 20.0f) + circ_p1, (u8)(circ_a2 >> 8));
		circ_p2 = circ_p2 + 0.12f;
		if (circ_a2 < 0xFF00) {
			circ_a2 = (u16)(circ_a2 + 0xC0);
		}
	}
	if (circ_r > 62.0f) {
		Hxs2_Circle((circ_r - 60.0f) + circ_p3, (circ_r - 40.0f) + circ_p2, (u8)(circ_a3 >> 8));
		circ_p3 = circ_p3 + 0.25f;
		if (circ_a3 < 0xFF00) {
			circ_a3 = (u16)(circ_a3 + 0x80);
		}
	}
}


// ---- Hx_Test5 / Hx_Test4 (part C1) ----

// Hx_Test4 function-statics (.sbss thin$469, rstep$470, thin_d$471, rstep_d$472)
static f32 test4_thin;   // ring half-thickness, grows by test4_thinD every frame
static u32 test4_rstep;  // number of ring quads drawn this frame, grows by test4_rstepD every frame
static f32 test4_thinD;
static f32 test4_rstepD;

// Wipe indices 5 and 6. The captured frame is cut into 64x64 tiles and every tile is redrawn as a 16-gon fan
// (a textured "circle" cut out of the framebuffer copy) whose radius scale follows hx.timer / 20.
// stage 0: timer = 20, stage = 1, draw. stage 1: draw; when Hx_TimerCountDown() reaches 0 (20th frame)
// stage = 2 and state = 3. stage >= 2: state = 3 and nothing is drawn.
// type 0: scale = 0.1 + 1.41 * t (shrinking to ~0.17); type != 0: scale = 1.41 - 1.41 * t (growing from 0).
void Hx_Test5()
{
	Hx_CameraInit();
	Hx_GxInit(1, 0);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);

	switch (hx.stage) {
	case 0:
		hx.timer = 20;
		hx.stage++;
		[[fallthrough]];
	case 1:
		break;
	default:
		hx.state = 3;
		return;
	}

	GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, GX_FALSE, GX_PTIDENTITY);
	GXSetNumTexGens(1);
	GXSetNumTevStages(1);
	GXSetTevOp(GX_TEVSTAGE0, GX_REPLACE);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);
	GXSetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);

	GXTexObj tex;
	GXInitTexObj(&tex, hx_buffer, 0x40, 0x40, GX_TF_RGB565, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GXInitTexObjLOD(&tex, GX_LINEAR, GX_LINEAR, 0.0f, 10.0f, 0.0f, GX_FALSE, GX_TRUE, GX_ANISO_1);

	f32 t = (f32)hx.timer / 20.0f;
	f32 k = 1.41f * t;
	f32 scaleGrow = 1.41f - k;   // used when type != 0
	f32 scaleShrink = 0.1f + k;  // used when type == 0
	const f32 scale = hx.type != 0 ? scaleGrow : scaleShrink;
	// angular offset between the texture circle and the drawn polygon
	const f32 twist = scale < 1.0f ? 3.1415927f * (1.0f - scale) : 0.0f;

	for (u32 y = 0; y < hx.height; y += 0x40) {
		for (u32 x = 0; x < hx.width; x += 0x40) {
			f32 cx = (f32)x;
			f32 cy = (f32)y;
			Hx_GetFrBuffer(hx_buffer, x, y, 0x40, 0x40);
			GXInvalidateTexAll();
			GXLoadTexObj(&tex, GX_TEXMAP0);

			cx = 32.0f + cx;
			cy = 32.0f + cy;
			GXBegin(GX_TRIANGLEFAN, GX_VTXFMT0, 0x12);
			GXPosition3f32(cx, cy, 0.0f);
			GXColor1u32(0);
			GXTexCoord2f32(0.5f, 0.5f);

			f32 firstDx = 0.0f, firstDy = 0.0f, firstU = 0.0f, firstV = 0.0f;
			for (u32 i = 0; i < 16; i++) {
				f32 ang = 3.1415927f * (2.0f * (f32)i) * 0.0625f;
				f32 sa = sinf(ang);
				f32 ca = cosf(ang);
				f32 u = 0.5f * sa + 0.5f;
				f32 v = 0.5f * ca + 0.5f;
				f32 ang2 = ang + twist;
				f32 dx = scale * sinf(ang2);
				f32 dy = scale * cosf(ang2);
				if (scale >= 1.0f) {
					u = (scale * sa) * 0.5f + 0.5f;
					v = (scale * ca) * 0.5f + 0.5f;
				}
				// clamp the offset to the tile's [-1, 1] box, sliding the texcoord along the clamped edge
				if (dx < -1.0f) {
					u = 0.0f;
					dy = -dy / dx;
					dx = -1.0f;
					v = 0.5f * dy + 0.5f;
				}
				if (dx > 1.0f) {
					dy = dy / dx;
					dx = 1.0f;
					u = 1.0f;
					v = 0.5f * dy + 0.5f;
				}
				if (dy < -1.0f) {
					v = 0.0f;
					dx = -dx / dy;
					dy = -1.0f;
					u = 0.5f * dx + 0.5f;
				}
				if (dy > 1.0f) {
					dx = dx / dy;
					dy = 1.0f;
					v = 1.0f;
					u = 0.5f * dx + 0.5f;
				}
				dx = dx * 32.0f;
				dy = dy * 32.0f;
				if (i == 0) {
					firstDx = dx;
					firstDy = dy;
					firstU = u;
					firstV = v;
				}
				GXPosition3f32(dx + cx, dy + cy, 0.0f);
				GXColor1u32(0);
				GXTexCoord2f32(u, v);
			}
			// close the fan with the first rim vertex
			GXPosition3f32(firstDx + cx, firstDy + cy, 0.0f);
			GXColor1u32(0);
			GXTexCoord2f32(firstU, firstV);
			GXEnd();
		}
	}

	if (Hx_TimerCountDown() == 0) {
		hx.stage++;
		hx.state = 3;
	}
}

// Wipe indices 7 and 8. Draws a black spiral ring (rstep quads, each a ring segment between an outer and an inner
// radius that both shrink by 2.4 per segment while the angle advances 0.12 rad), growing/shrinking every frame.
// stage 0: type 0 -> thin 124.3, rstep 0, thin += 0.15, rstep += 5 per frame (spiral grows);
//          type 1 -> thin 100, rstep 230, thin -= 0.15, rstep -= 5 per frame (spiral shrinks);
//          then timer = 38, stage = 1, draw. stage 1: draw; when Hx_TimerCountDown() reaches 0 (38th frame)
// state = 3 and stage = 2. stage >= 2: returns without touching state (it is already 3).
void Hx_Test4()
{
	switch (hx.stage) {
	case 0:
		switch (hx.type) {
		case 0:
			test4_rstep = 0;
			test4_thin = 124.3f;
			test4_rstepD = 5.0f;
			test4_thinD = 0.15f;
			break;
		case 1:
			test4_rstep = 230;
			test4_thin = 100.0f;
			test4_rstepD = -5.0f;
			test4_thinD = -0.15f;
			break;
		}
		hx.timer = 38;
		hx.stage++;
		[[fallthrough]];
	case 1:
		break;
	default:
		return;
	}

	test4_rstep = (u32)((f32)test4_rstep + test4_rstepD);
	test4_thin = test4_thin + test4_thinD;

	f32 rad = (f32)((hx.width >> 1) + 200);
	f32 ang = 0.0f;
	f32 outerR = rad + test4_thin;
	f32 outerX = outerR * sinf(ang) + (f32)hx.halfWidth;
	f32 outerY = outerR * cosf(ang) + (f32)hx.halfHeight;
	f32 innerR = rad - test4_thin;
	f32 innerX = innerR * sinf(ang) + (f32)hx.halfWidth;
	f32 innerY = innerR * cosf(ang) + (f32)hx.halfHeight;

	Hx_CameraInit();
	Hx_GxInit(0, 1);

	for (u32 i = 0; i < test4_rstep; i++) {
		rad = rad - 2.4f;
		outerR = rad + test4_thin;
		innerR = rad >= test4_thin ? rad - test4_thin : 0.0f;
		ang = ang + 0.12f;
		f32 nextOuterX = outerR * sinf(ang) + (f32)hx.halfWidth;
		f32 nextOuterY = outerR * cosf(ang) + (f32)hx.halfHeight;
		f32 nextInnerX = innerR * sinf(ang) + (f32)hx.halfWidth;
		f32 nextInnerY = innerR * cosf(ang) + (f32)hx.halfHeight;

		GXBegin(GX_QUADS, GX_VTXFMT0, 4);
		GXPosition3f32(outerX, outerY, 0.0f);
		GXColor1u32(0xFF);
		GXPosition3f32(nextOuterX, nextOuterY, 0.0f);
		GXColor1u32(0xFF);
		GXPosition3f32(nextInnerX, nextInnerY, 0.0f);
		GXColor1u32(0xFF);
		GXPosition3f32(innerX, innerY, 0.0f);
		GXColor1u32(0xFF);
		GXEnd();

		outerX = nextOuterX;
		outerY = nextOuterY;
		innerX = nextInnerX;
		innerY = nextInnerY;
	}

	if (Hx_TimerCountDown() == 0) {
		hx.state = 3;
		hx.stage++;
	}
}


// ---- part C2: Hxs1_Test2, Hx_Test2R (index 9), Hx_Test2 (index 10), Hxs1_Test1, Hx_Test1 (indices 3, 4) ----

static f32 r_393; // Hx_Test1: current circle radius (kept between calls; the final call reuses it)
static f32 r_416; // Hx_Test2: motion position scratch
static f32 r_432; // Hx_Test2R: motion position scratch

// Draws one ring (between radii r1 and r2 around (cx, cy)) as black GX_LINES scanlines, one line per integer
// row i in [-cy, r1): half-widths sqrt(r1^2-i^2) / sqrt(r2^2-i^2) are mirrored towards the screen centre.
// Rows run top-down (reverse == 0) or bottom-up; only on-screen rows count against `count`, drawing stops after
// `count` rows. Resets the camera and GX state itself.
static void Hxs1_Test2(u32 count, int reverse, f32 cx, f32 cy, f32 r1, f32 r2)
{
	Hx_CameraInit();
	Hx_GxInit(0, 1);

	f32 r1sq = r1 * r1;
	f32 r2sq = r2 * r2;
	s32 step, i, end;
	if (reverse == 0) {
		step = 1;
		end  = (s32)r1;
		i    = (s32)(-cy);
	} else {
		step = -1;
		i    = (s32)r1;
		end  = (s32)(-cy);
	}

	for (; i != end; i += step) {
		f32 y = cy + (f32)i;
		if (y < 0.0f)
			continue;
		if (y > (f32)hx.height)
			continue;
		if (count == 0)
			break;

		s32 sq = i * i;
		count--;

		f32 w1 = r1sq - (f32)sq;
		if (w1 > 0.0f)
			w1 = sqrtf(w1);
		f32 w2 = r2sq - (f32)sq;
		if (w2 > 0.0f)
			w2 = sqrtf(w2);

		f32 xa, xb;
		if (cx < (f32)hx.halfWidth) {
			xb = cx + w1;
			xa = cx + w2;
			if (xb < 0.0f)
				continue;
		} else {
			xa = cx - w1;
			xb = cx - w2;
			if (xa > (f32)hx.width)
				continue;
		}

		GXBegin(GX_LINES, GX_VTXFMT0, 2);
		GXPosition3f32(xa, y, 1.0f);
		GXColor1u32(0xFF);
		GXPosition3f32(xb, y, 1.0f);
		GXColor1u32(0xFF);
		GXEnd();
	}
}

// Wipe index 9: four ring bands (right side shrinking away). Stage 0 starts a motion and falls into stage 1.
// Stage 1 (11 calls), 2 (11), 3 (10), 4 (12): each stage draws static bands plus one band whose row count is
// 500 - motion position (so it retracts), and advances when Hx_TimerCountDown() reaches 0 (restarting the motion).
// Stage 4 ending sets stage 5 and hx.state = 3 (44 calls in total); any other stage value sets hx.state = 3.
void Hx_Test2R()
{
	switch (hx.stage) {
	case 0:
		r_432 = 1.0f;
		Hx_MotionSet(&hx.motion, 500.0f, 2.0f, 8.0f, 1.0f);
		hx.stage++;
		hx.timer = 11;
		[[fallthrough]];
	case 1:
		Hxs1_Test2(600, 0, (f32)(hx.width + 200), 150.0f, 700.0f, 450.0f);
		Hxs1_Test2(600, 1, (f32)(hx.width + 250), 370.0f, 650.0f, 400.0f);
		Hxs1_Test2(600, 0, (f32)(hx.width + 250), 300.0f, 420.0f, 200.0f);
		r_432 = Hx_MotionUpdate(&hx.motion);
		r_432 = 500.0f - r_432;
		Hxs1_Test2((u32)r_432, 0, (f32)(hx.width + 200), 300.0f, 900.0f, 650.0f);
		if (Hx_TimerCountDown() == 0) {
			hx.stage++;
			hx.timer = 11;
			Hx_MotionSet(&hx.motion, 500.0f, 2.0f, 8.0f, 1.0f);
		}
		break;
	case 2:
		Hxs1_Test2(600, 1, (f32)(hx.width + 250), 370.0f, 650.0f, 400.0f);
		Hxs1_Test2(600, 0, (f32)(hx.width + 250), 300.0f, 420.0f, 200.0f);
		r_432 = Hx_MotionUpdate(&hx.motion);
		r_432 = 500.0f - r_432;
		Hxs1_Test2((u32)r_432, 1, (f32)(hx.width + 200), 150.0f, 700.0f, 450.0f);
		if (Hx_TimerCountDown() == 0) {
			hx.stage++;
			hx.timer = 10;
			Hx_MotionSet(&hx.motion, 500.0f, 2.0f, 7.0f, 1.0f);
		}
		break;
	case 3:
		Hxs1_Test2(600, 0, (f32)(hx.width + 250), 300.0f, 420.0f, 200.0f);
		r_432 = Hx_MotionUpdate(&hx.motion);
		r_432 = 500.0f - r_432;
		Hxs1_Test2((u32)r_432, 0, (f32)(hx.width + 250), 370.0f, 650.0f, 400.0f);
		if (Hx_TimerCountDown() == 0) {
			hx.stage++;
			hx.timer = 12;
			Hx_MotionSet(&hx.motion, 500.0f, 2.0f, 9.0f, 1.0f);
		}
		break;
	case 4:
		r_432 = Hx_MotionUpdate(&hx.motion);
		r_432 = 500.0f - r_432;
		Hxs1_Test2((u32)r_432, 1, (f32)(hx.width + 250), 300.0f, 420.0f, 200.0f);
		if (Hx_TimerCountDown() == 0) {
			hx.stage++;
			hx.state = 3;
		}
		break;
	default:
		hx.state = 3;
		break;
	}
}

// Wipe index 10: the same ring bands as Hx_Test2R but building up instead (row count = motion position).
// Stage 0 only starts the motion (nothing is drawn). Stage 1 (11 calls), 2 (11), 3 (10), 4 (12): completed bands
// are redrawn at full size (600 rows) plus one growing band; each advances on Hx_TimerCountDown() == 0.
// Stage 4 ending sets stage 5 and hx.state = 3 (45 calls in total); any other stage value sets hx.state = 3.
void Hx_Test2()
{
	switch (hx.stage) {
	case 0:
		r_416 = 1.0f;
		Hx_MotionSet(&hx.motion, 500.0f, 2.0f, 8.0f, 1.0f);
		hx.stage++;
		hx.timer = 11;
		break;
	case 1:
		r_416 = Hx_MotionUpdate(&hx.motion);
		Hxs1_Test2((u32)r_416, 1, (f32)(hx.width + 200), 300.0f, 900.0f, 650.0f);
		if (Hx_TimerCountDown() == 0) {
			hx.stage++;
			hx.timer = 11;
			Hx_MotionSet(&hx.motion, 500.0f, 2.0f, 8.0f, 1.0f);
		}
		break;
	case 2:
		Hxs1_Test2(600, 0, (f32)(hx.width + 200), 300.0f, 900.0f, 650.0f);
		r_416 = Hx_MotionUpdate(&hx.motion);
		Hxs1_Test2((u32)r_416, 0, (f32)(hx.width + 200), 150.0f, 700.0f, 450.0f);
		if (Hx_TimerCountDown() == 0) {
			hx.stage++;
			hx.timer = 10;
			Hx_MotionSet(&hx.motion, 500.0f, 2.0f, 7.0f, 1.0f);
		}
		break;
	case 3:
		Hxs1_Test2(600, 0, (f32)(hx.width + 200), 300.0f, 900.0f, 650.0f);
		Hxs1_Test2(600, 0, (f32)(hx.width + 200), 150.0f, 700.0f, 450.0f);
		r_416 = Hx_MotionUpdate(&hx.motion);
		Hxs1_Test2((u32)r_416, 1, (f32)(hx.width + 250), 370.0f, 650.0f, 400.0f);
		if (Hx_TimerCountDown() == 0) {
			hx.stage++;
			hx.timer = 12;
			Hx_MotionSet(&hx.motion, 500.0f, 2.0f, 9.0f, 1.0f);
		}
		break;
	case 4:
		Hxs1_Test2(600, 0, (f32)(hx.width + 200), 300.0f, 900.0f, 650.0f);
		Hxs1_Test2(600, 0, (f32)(hx.width + 200), 150.0f, 700.0f, 450.0f);
		Hxs1_Test2(600, 1, (f32)(hx.width + 250), 370.0f, 650.0f, 400.0f);
		r_416 = Hx_MotionUpdate(&hx.motion);
		Hxs1_Test2((u32)r_416, 0, (f32)(hx.width + 250), 300.0f, 420.0f, 200.0f);
		if (Hx_TimerCountDown() == 0) {
			hx.stage++;
			hx.state = 3;
		}
		break;
	default:
		hx.state = 3;
		break;
	}
}

// Draws the quarter disc of radius r around the screen corner (cx, cy) as black GX_LINES (7 px wide): row i in
// [0, (u32)r], one line from the corner column to the column sqrt(r^2-i^2) away towards the screen centre, at
// y = cy +/- i (also towards the centre). Resets the camera and GX state itself.
static void Hxs1_Test1(f32 cx, f32 cy, f32 r)
{
	Hx_CameraInit();
	Hx_GxInit(0, 1);
	GXSetLineWidth(7, GX_TO_ZERO);

	f32 rsq = r * r;
	GXBegin(GX_LINES, GX_VTXFMT0, (u16)((u32)r * 2 + 2));

	for (u32 i = 0; i <= (u32)r; i++) {
		f32 w = rsq - (f32)(u32)(i * i);
		if (w > 0.0f)
			w = sqrtf(w);

		f32 y;
		if (cy < (f32)hx.halfHeight)
			y = cy + (f32)i;
		else
			y = cy - (f32)i;

		f32 xa, xb;
		if (cx < (f32)hx.halfWidth) {
			xa = cx;
			xb = cx + w;
		} else {
			xa = cx - w;
			xb = cx;
		}

		GXPosition3f32(xa, y, 1.0f);
		GXColor1u32(0xFF);
		GXPosition3f32(xb, y, 1.0f);
		GXColor1u32(0xFF);
	}
	GXEnd();
}

// Wipe indices 3 and 4: black quarter discs at the four screen corners, radius r_393. hx.type == 1: r starts at
// 400 and shrinks (r = 400 - motion pos, motion 400/10/12/8); otherwise r starts at 1 and grows (r = motion pos,
// motion 400/5/10/10). Stage 0 (init, timer = 25, draws r), stage 1 (25 calls, advances when the timer hits 0),
// then any later stage sets hx.state = 3 (27th call) and still draws once with the last r.
void Hx_Test1()
{
	switch (hx.stage) {
	case 0:
		if (hx.type == 1) {
			r_393 = 400.0f;
			Hx_MotionSet(&hx.motion, 400.0f, 10.0f, 12.0f, 8.0f);
		} else {
			r_393 = 1.0f;
			Hx_MotionSet(&hx.motion, 400.0f, 5.0f, 10.0f, 10.0f);
		}
		hx.stage++;
		hx.timer = 25;
		break;
	case 1:
		if (Hx_TimerCountDown() == 0)
			hx.stage++;
		r_393 = Hx_MotionUpdate(&hx.motion);
		if (hx.type == 1)
			r_393 = 400.0f - r_393;
		break;
	default:
		hx.state = 3;
		break;
	}

	Hxs1_Test1(0.0f, 0.0f, r_393);
	Hxs1_Test1((f32)hx.width, 0.0f, r_393);
	Hxs1_Test1((f32)hx.width, (f32)hx.height, r_393);
	Hxs1_Test1(0.0f, (f32)hx.height, r_393);
}


// ---- part D: Hx_Logo (wipe index 12, the title logo "M" mark) and its helpers ----

// One point of the pen path: flag 0 = pen jumps to (x, y) without drawing, flag N > 0 = draw a stroke to (x, y)
// lasting N frames, flag 0xFFFFFFFF = end of path.
struct HxPathPt {
	f32 x;
	f32 y;
	u32 flag;
};

static const HxPathPt drawpath_table[27] = {
	{ 49.0f, 277.0f, 0 },
	{ 20.0f, 294.0f, 1 },
	{ 22.0f, 234.0f, 1 },
	{ 50.0f, 175.0f, 1 },
	{ 110.0f, 112.0f, 1 },
	{ 128.0f, 106.0f, 1 },
	{ 138.0f, 110.0f, 1 },
	{ 134.0f, 113.0f, 1 },
	{ 135.0f, 128.0f, 1 },
	{ 117.0f, 166.0f, 1 },
	{ 119.0f, 201.0f, 1 },
	{ 129.0f, 229.0f, 1 },
	{ 140.0f, 219.0f, 1 },
	{ 149.0f, 172.0f, 1 },
	{ 178.0f, 137.0f, 1 },
	{ 192.0f, 114.0f, 1 },
	{ 216.0f, 104.0f, 1 },
	{ 227.0f, 109.0f, 1 },
	{ 224.0f, 130.0f, 1 },
	{ 183.0f, 265.0f, 3 },
	{ 161.0f, 18.0f, 0 },
	{ 161.0f, 18.01f, 8 },
	{ 141.0f, 79.0f, 3 },
	{ 255.0f, 3.0f, 0 },
	{ 255.0f, 3.01f, 8 },
	{ 220.0f, 71.0f, 4 },
	{ -1.0f, -1.0f, 0xFFFFFFFF },
};

static const HxPathPt* dp_320; // Hx_Logo: path point the pen is currently drawing towards
static f32 bx_321;             // Hx_Logo: pen position at the start of the current stroke
static f32 by_322;
static u32 count_323;          // Hx_Logo: index of dp in drawpath_table (points consumed so far)

// Sets up the TEV for the logo texture. TEV register 0 = (a, 0, 0, alpha0(b)), register 1 = (a, 0, 0, alpha1(b));
// colour = reg0.rgb * tex.a, alpha = lerp(reg1.a, reg0.a, tex.a). b > 0xC0 fades the colour in, b <= 0xC0 fades it out.
static void Hxs_Logo_TexSetup(u8 a, u8 b, const void* bti)
{
	GXTexObj tex;
	GXColor c0, c1;

	Hx_CameraInit();
	Hx_GxInit(1, 1);
	Hgx_init_tobj_resource(&tex, bti);

	c0.r = a;
	c0.g = 0;
	c0.b = 0;
	if (b > 0xC0)
		c0.a = 0xFF;
	else
		c0.a = (u8)(s32)(1.328 * (f64)b);
	GXSetTevColor(GX_TEVREG0, c0);

	c1.r = a;
	c1.g = 0;
	c1.b = 0;
	if (b > 0xC0)
		c1.a = (u8)((0xFF - b) << 2);
	else
		c1.a = 0xFF;
	GXSetTevColor(GX_TEVREG1, c1);

	GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_C0, GX_CC_TEXA, GX_CC_ZERO);
	GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_A1, GX_CA_A0, GX_CA_TEXA, GX_CA_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GXLoadTexObj(&tex, GX_TEXMAP0);
	GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
}

// Draws the caption strip (320x32 quad at (160,205)) of the extra texture, modulated by `alpha`. Sets up its own camera/TEV.
static void Hxs_Logo_ExtraDraw(u8 alpha, const void* bti)
{
	GXTexObj tex;
	GXColor c;

	Hx_CameraInit();
	Hx_GxInit(1, 1);
	Hgx_init_tobj_resource(&tex, bti);

	c.r = 0xFF;
	c.g = 0xFF;
	c.b = 0xFF;
	c.a = alpha;
	GXSetTevColor(GX_TEVREG0, c);

	GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_C0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO);
	GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_A0, GX_CA_TEXA, GX_CA_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GXLoadTexObj(&tex, GX_TEXMAP0);
	GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);

	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	GXPosition3f32(160.0f, 205.0f, 0.0f);
	GXColor1u32(0);
	GXTexCoord2f32(0.0f, 0.0f);
	GXPosition3f32(480.0f, 205.0f, 0.0f);
	GXColor1u32(0);
	GXTexCoord2f32(1.0f, 0.0f);
	GXPosition3f32(480.0f, 237.0f, 0.0f);
	GXColor1u32(0);
	GXTexCoord2f32(1.0f, 1.0f);
	GXPosition3f32(160.0f, 237.0f, 0.0f);
	GXColor1u32(0);
	GXTexCoord2f32(0.0f, 1.0f);
	GXEnd();
}

// Draws the logo texture (texW x texH) centred on the screen, scaled by `mag`, as one screen-sized quad whose
// texture coordinates are extrapolated so that the texture lands at the centre. The quad is shifted up by 32.
static void Hxs_Logo_MagDraw(f32 mag, f32 texW, f32 texH)
{
	f32 sw     = (texW / 1.9230769f) * mag;
	f32 sh     = (texH / 1.924138f) * mag;
	f32 halfW  = sw * 0.5f;
	f32 halfH  = sh * 0.5f;
	f32 cx     = (f32)(hx.width >> 1);
	f32 cy     = (f32)(hx.height >> 1);
	f32 left   = cx - halfW;
	f32 right  = cx + halfW;
	f32 top    = cy - halfH;
	f32 bottom = cy + halfH;
	f32 u0     = left / (left - right);
	f32 v0     = top / (top - bottom);

	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	GXPosition3f32(0.0f, -32.0f, 0.0f);
	GXColor1u32(0);
	GXTexCoord2f32(u0, v0);
	GXPosition3f32((f32)hx.width, -32.0f, 0.0f);
	GXColor1u32(0);
	GXTexCoord2f32(1.0f - u0, v0);
	GXPosition3f32((f32)hx.width, (f32)(hx.height - 32), 0.0f);
	GXColor1u32(0);
	GXTexCoord2f32(1.0f - u0, 1.0f - v0);
	GXPosition3f32(0.0f, (f32)(hx.height - 32), 0.0f);
	GXColor1u32(0);
	GXTexCoord2f32(u0, 1.0f - v0);
	GXEnd();
}

// Draws one brush stroke from (x1, y1) to (x2, y2) (pen coordinates, the logo texture is texW x texH): a quad of
// half-width 0.08 (texture space) around the segment, textured with the logo at the matching texture coordinates,
// so the logo is revealed under the pen. Zero-length segments draw nothing.
static void Hxs_Logo_TexDraw(f32 x1, f32 y1, f32 x2, f32 y2, f32 texW, f32 texH)
{
	f32 hw = (f32)(hx.width >> 1);
	f32 hh = (f32)(hx.height >> 1);
	f32 sy = texH / 1.924138f;
	f32 sx = texW / 1.9230769f;
	f32 v1 = y1 / sy;
	f32 v2 = y2 / sy;
	f32 u1 = x1 / sx;
	f32 u2 = x2 / sx;
	f32 offX = hw - sx * 0.5f;
	f32 offY = (hh - sy * 0.5f) - 32.0f;

	Vec n;
	n.x = -(v2 - v1);
	n.y = u2 - u1;
	n.z = 0.0f;

	if (n.y == 0.0f && n.x == 0.0f)
		return;

	PSVECNormalize(&n, &n);
	PSVECScale(&n, &n, 0.08f);
	f32 px = n.x;
	f32 py = n.y;

	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	GXPosition3f32(fmaf(sx, u1 + px, offX), fmaf(sy, v1 + py, offY), 0.0f);
	GXColor1u32(0);
	GXTexCoord2f32(u1 + px, v1 + py);
	GXPosition3f32(fmaf(sx, u2 + px, offX), fmaf(sy, v2 + py, offY), 0.0f);
	GXColor1u32(0);
	GXTexCoord2f32(u2 + px, v2 + py);
	GXPosition3f32(fmaf(sx, u2 - px, offX), fmaf(sy, v2 - py, offY), 0.0f);
	GXColor1u32(0);
	GXTexCoord2f32(u2 - px, v2 - py);
	GXPosition3f32(fmaf(sx, u1 - px, offX), fmaf(sy, v1 - py, offY), 0.0f);
	GXColor1u32(0);
	GXTexCoord2f32(u1 - px, v1 - py);
	GXEnd();
}

// Draws the path from drawpath_table[0] up to table[count - 1] (every non-jump point as a stroke extended by 6 units
// at both ends), then the stroke in progress from the pen (bx, by) towards `dp`, a fraction (flag - timer) / flag
// of the way. Stops at the end marker.
static void Hxs_PenDraw(u32 count, const HxPathPt* dp, f32 bx, f32 by)
{
	if (count != 0) {
		f32 prevX = drawpath_table[0].x;
		f32 prevY = drawpath_table[0].y;
		for (u32 i = 0; i < count - 1; i++) {
			const HxPathPt* p = &drawpath_table[i + 1];
			if (p->flag == 0xFFFFFFFF)
				break;
			if (p->flag != 0) {
				// the original leaves z uninitialised here; z = 0 is the evident intent
				Vec dir;
				dir.x = p->x - prevX;
				dir.y = p->y - prevY;
				dir.z = 0.0f;
				PSVECNormalize(&dir, &dir);
				PSVECScale(&dir, &dir, 6.0f);
				Hxs_Logo_TexDraw(prevX - dir.x, prevY - dir.y, p->x + dir.x, p->y + dir.y, (f32)img_wx, (f32)img_wy);
			}
			prevX = p->x;
			prevY = p->y;
		}
	}

	u32 flag = dp->flag;
	f32 t    = (f32)(u32)(flag - hx.timer) / (f32)(s32)flag;
	f32 ex   = fmaf(t, dp->x - bx, bx);
	f32 ey   = fmaf(t, dp->y - by, by);
	Hxs_Logo_TexDraw(bx, by, ex, ey, (f32)img_wx, (f32)img_wy);
}

// One frame of the full pen drawing: caption strip, logo TEV, then the strokes drawn so far.
static void Hxs_Logo_PenFrame(const void* res, const void* extra)
{
	Hxs_Logo_ExtraDraw(0xFF, extra);
	Hxs_Logo_TexSetup(0xFF, 0xFF, res);
	Hxs_PenDraw(count_323, dp_320, bx_321, by_322);
}

// Wipe index 12. hx.stage: 0 setup (reads /data/title_mini.bti unless a Ex resource was given, rewinds the path,
// timer = 0x100, raises both movie reset flags); 1 caption strip fades in (timer -= 4 per call, 64 calls);
// 2/3 pen draws the logo stroke by stroke (stage 2 picks the next point and falls into 3, which counts the stroke's
// `flag` frames down); 4/5 path done, logo held for 6 calls (timer = 5); 6 logo fades out (timer 0xFF, -4 per call,
// 64 calls); 7 logo silhouette cut-out zooms in for 25 calls (timer = 25); 8 sets hx.state = 3.
void Hx_Logo()
{
	const void* res   = hx.res;
	const void* extra = hx.hasResEx ? hx.resEx : (void*)hx_buffer;

	switch (hx.stage) {
	case 0:
		if (hx.hasResEx == 0)
			Hgx_ReadTexture("/data/title_mini.bti", (void*)extra);
		hx.stage++;
		dp_320                 = drawpath_table;
		count_323              = 0;
		hx.timer               = 0x100;
		hxs_logo_resetflag     = 1;
		hxs_logodraw_resetflag = 1;
		break;

	case 1:
		if (hx.timer <= 0xC0)
			Hxs_Logo_ExtraDraw(0xFF, extra);
		else
			Hxs_Logo_ExtraDraw((u8)((0x100 - hx.timer) << 2), extra);
		Hx_TimerCountDown();
		Hx_TimerCountDown();
		Hx_TimerCountDown();
		if (Hx_TimerCountDown() == 0)
			hx.stage++;
		break;

	case 2:
		if (dp_320->flag == 0xFFFFFFFF) {
			hx.stage = 3;
			goto endOfPath;
		}
		if (dp_320->flag == 0) {
			bx_321 = dp_320->x;
			by_322 = dp_320->y;
			dp_320++;
			count_323++;
		}
		hx.timer = dp_320->flag;
		hx.stage++;
		[[fallthrough]];

	case 3:
		Hxs_Logo_PenFrame(res, extra);
		if (Hx_TimerCountDown() == 0) {
			bx_321 = dp_320->x;
			by_322 = dp_320->y;
			dp_320++;
			hx.stage = 2;
			count_323++;
		}
		break;

	case 4:
	endOfPath:
		hx.timer = 5;
		hx.stage++;
		[[fallthrough]];

	case 5:
		Hxs_Logo_PenFrame(res, extra);
		if (Hx_TimerCountDown() == 0) {
			hx.timer = 0xFF;
			hx.stage++;
		}
		break;

	case 6:
		if (hx.timer >= 0xC0) {
			Hxs_Logo_ExtraDraw(0xFF, extra);
			Hxs_Logo_TexSetup((u8)hx.timer, (u8)hx.timer, res);
			if (hx.timer > 0xF8)
				Hxs_PenDraw(count_323, dp_320, bx_321, by_322);
			else
				Hxs_Logo_MagDraw(1.0f, (f32)img_wx, (f32)img_wy);
		} else {
			Hxs_Logo_TexSetup((u8)hx.timer, (u8)hx.timer, res);
			Hxs_Logo_MagDraw(1.0f, (f32)img_wx, (f32)img_wy);
		}
		Hx_TimerCountDown();
		Hx_TimerCountDown();
		Hx_TimerCountDown();
		if (Hx_TimerCountDown() == 0) {
			hx.timer = 25;
			Hx_MotionSet(&hx.motion, 30.0f, 12.0f, 8.0f, 5.0f);
			hx.stage++;
		}
		break;

	case 7: {
		Hxs_Logo_TexSetup(0, 0, res);
		f32 pos = Hx_MotionUpdate(&hx.motion);
		Hxs_Logo_MagDraw(1.0f + pos, (f32)img_wx, (f32)img_wy);
		if (Hx_TimerCountDown() == 0)
			hx.stage++;
		break;
	}

	case 8:
		hx.state = 3;
		break;

	default:
		break;
	}
}


// ---- part E: Hx_GameOver (wipe index 13) and its helpers ----

// Bounce table of the Mario head, read as (delta, frames) pairs. The -8.0f "frames" entry of
// the fourth pair converts to 0 (see gameover_f2u), which ends the bouncing there, so the
// pairs after it are never reached.
static const f32 gameover_boundtable[14] = {
	-0.13f, 6.0f, 0.13f, 6.0f, -0.12f, 8.0f, 0.12f, -8.0f, -0.11f, 12.0f, 0.11f, 12.0f, 0.0f, 0.0f,
};

static f32 gameover_mag = 1.0f; // head scale
static f32 gameover_rot;        // head rotation (radians)
static f32 gameover_fade;       // alpha ramp used before stage 2
static u32 gameover_boundstate; // next index into gameover_boundtable
static u32 gameover_boundtimer; // frames left for the current bounce delta
static f32 gameover_bounddelta; // scale change per frame while bouncing
static u8 gameover_alpha;       // red box alpha (stage 4)

// MWCC __cvt_fp2unsigned: negative -> 0, >= 2^32 -> 0xFFFFFFFF, otherwise truncation.
static u32 gameover_f2u(f32 d)
{
	if (d < 0.0f)
		return 0;
	if (d >= 4294967296.0f)
		return 0xFFFFFFFFu;
	return (u32)d;
}

// Draws the full-screen game-over texture, rotated/scaled around its centre, with a constant
// alpha (TEV stage 1 multiplies the texture alpha by the TEV register 0 alpha).
static void Hxs_GameOver(s32 alpha, f32 scale, f32 rot)
{
	Hx_CameraInit();
	gmover_tex_buffer = hx.res;

	GXTexObj tex;
	Hgx_init_tobj_resource(&tex, gmover_tex_buffer);

	GXSetTexCoordGen2(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY, GX_FALSE, GX_PTIDENTITY);
	GXSetNumTexGens(1);
	GXSetNumTevStages(2);

	GXSetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO);
	GXSetTevAlphaIn(GX_TEVSTAGE0, GX_CA_TEXA, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GXSetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR_NULL);

	GXColor reg0 = { 0, 0, 0, (u8)alpha };
	GXSetTevColor(GX_TEVREG0, reg0);
	GXSetTevColorIn(GX_TEVSTAGE1, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO);
	GXSetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_A0, GX_CA_APREV, GX_CA_ZERO);
	GXSetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GXSetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GXSetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD_NULL, GX_TEXMAP_NULL, GX_COLOR_NULL);

	GXLoadTexObj(&tex, GX_TEXMAP0);
	GXClearVtxDesc();
	GXSetVtxDesc(GX_VA_POS, GX_DIRECT);
	GXSetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GXSetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_CLR0, GX_CLR_RGBA, GX_RGBA8, 0);
	GXSetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GXSetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);

	// Texture is stretched so that its aspect ratio matches the screen (integer width/height).
	const f32 aspect = (f32)(hx.width / hx.height) / ((f32)img_wx / (f32)img_wy);

	// Rotated half-diagonal (0.5, 0.5) of the unit texture square.
	Vec v = { 0.5f, 0.5f, 0.0f };
	PSVECNormalize(&v, &v);
	Mtx rotm;
	PSMTXRotRad(rotm, 'Z', rot);
	PSMTXMultVec(rotm, &v, &v);

	// The original evaluates sqrtf(0.5f * 0.5f + 0.5f * 0.5f) once per corner (all identical).
	const f32 p = scale * sqrtf(0.5f * 0.5f + 0.5f * 0.5f);
	const f32 nvx = -v.x;
	const f32 nvy = -v.y;
	const f32 u0 = fmaf(aspect, nvx * p, 0.5f), t0 = fmaf(nvy, p, 0.5f);
	const f32 u1 = fmaf(aspect, v.y * p, 0.5f), t1 = fmaf(nvx, p, 0.5f);
	const f32 u2 = fmaf(aspect, v.x * p, 0.5f), t2 = fmaf(v.y, p, 0.5f);
	const f32 u3 = fmaf(aspect, nvy * p, 0.5f), t3 = fmaf(v.x, p, 0.5f);

	const f32 w = (f32)hx.width;
	const f32 h = (f32)hx.height;
	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	GXPosition3f32(0.0f, 0.0f, 0.0f);
	GXColor1u32(0);
	GXTexCoord2f32(u0, t0);
	GXPosition3f32(w, 0.0f, 0.0f);
	GXColor1u32(0);
	GXTexCoord2f32(u1, t1);
	GXPosition3f32(w, h, 0.0f);
	GXColor1u32(0);
	GXTexCoord2f32(u2, t2);
	GXPosition3f32(0.0f, h, 0.0f);
	GXColor1u32(0);
	GXTexCoord2f32(u3, t3);
	GXEnd();

	GXSetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_ONE, GX_LO_CLEAR);
}

// Solid RGBA box inset by 100 px on every side (stages 4 and 5).
static void Hxs_GameOverBox(u32 color)
{
	Hx_CameraInit();
	Hx_GxInit(0, 1);
	const f32 x2 = (f32)(hx.width - 100);
	const f32 y2 = (f32)(hx.height - 100);
	GXBegin(GX_QUADS, GX_VTXFMT0, 4);
	GXPosition3f32(100.0f, 100.0f, 0.0f);
	GXColor1u32(color);
	GXPosition3f32(x2, 100.0f, 0.0f);
	GXColor1u32(color);
	GXPosition3f32(x2, y2, 0.0f);
	GXColor1u32(color);
	GXPosition3f32(100.0f, y2, 0.0f);
	GXColor1u32(color);
	GXEnd();
}

// Stages (hx.stage): 0 load texture; 1 (50 frames) head spins in, grows, fades in;
// 2 (10 frames) shrinks; 3 (~20 frames) bounces following gameover_boundtable;
// 4 (32 frames) red box fades in; 5 (100 frames) opaque box, then hx.state = 3 and stage 6.
// The head is redrawn on top every frame (alpha 255 from stage 2 on).
void Hx_GameOver()
{
	switch (hx.stage) {
	case 0:
		Hgx_ReadTexture("/data/wipe_gameover.bti", gmover_tex_buffer);
		gameover_rot = 0.0f;
		gameover_mag = 0.3f;
		gameover_fade = 0.0f;
		hx.stage++;
		hx.timer = 50;
		Hx_MotionSet(&hx.motion, -12.566371f, 10.0f, 15.0f, 25.0f);
		break;
	case 1:
		if (Hx_TimerCountDown() == 0) {
			hx.stage++;
			hx.timer = 10;
		}
		gameover_mag = 0.074f + gameover_mag;
		gameover_rot = Hx_MotionUpdate(&hx.motion);
		gameover_fade = 5.1f + gameover_fade;
		break;
	case 2:
		if (Hx_TimerCountDown() == 0) {
			hx.stage++;
			gameover_bounddelta = gameover_boundtable[0];
			gameover_boundtimer = gameover_f2u(gameover_boundtable[1]);
			gameover_boundstate = 2;
		}
		gameover_mag = gameover_mag - 0.1f;
		break;
	case 3:
		if (gameover_boundtimer != 0)
			gameover_boundtimer--;
		if (gameover_boundtimer == 0) {
			u32 idx = gameover_boundstate++;
			gameover_bounddelta = gameover_boundtable[idx];
			idx = gameover_boundstate++;
			gameover_boundtimer = gameover_f2u(gameover_boundtable[idx]);
			if (gameover_boundtimer == 0) {
				hx.stage++;
				hx.timer = 32;
				gameover_alpha = 0xFF;
				break;
			}
		}
		gameover_mag = gameover_mag + gameover_bounddelta;
		break;
	case 4:
		if (Hx_TimerCountDown() == 0) {
			hx.timer = 100;
			hx.stage++;
		}
		gameover_alpha += 8;
		Hxs_GameOverBox(0xFF000000u | gameover_alpha);
		break;
	case 5:
		Hxs_GameOverBox(0xFF0000FFu);
		if (Hx_TimerCountDown() == 0) {
			hx.stage++;
			hx.state = 3;
		}
		break;
	default:
		hx.state = 3;
		break;
	}

	if (hx.stage >= 2)
		Hxs_GameOver(0xFF, 2.0f * gameover_mag, gameover_rot);
	else
		Hxs_GameOver((s32)gameover_fade, 2.0f * gameover_mag, gameover_rot);
}


} // namespace hxi

// ---- exported API (outside namespace hxi; signatures are those of GC2D/hx_wiper.h) ----

extern "C" {

// TSMSFader::requestWipe: Hx_StartWipe(idx, frames) then Hx_GetWipeType(idx) picks FADING_IN (1) / FADING_OUT.
// TSMSFader::drawWipe:    Hx_UpdateWipe(rate) == 3  ->  FULLY_FADED_IN if Hx_GetWipeType(idx) != 0 else FULLY_FADED_OUT.
int Hx_GetWipeType(int idx)
{
	// The original indexes the table unchecked (the game never passes idx >= 15 here).
	return (idx >= 0 && idx < hxi::kWipeCount) ? hxi::handle_type[idx] : 0;
}

// Resets the library for a display of w x h and forgets any resource handed in by Hx_ProvideResource(Ex).
void Hx_ResetWipe(u32 w, u32 h)
{
	hxi::hx.state      = 0;
	hxi::hx.width      = w;
	hxi::hx.height     = h;
	hxi::hx.halfWidth  = w >> 1;
	hxi::hx.halfHeight = h >> 1;
	hxi::hx.hasRes     = 0;
	hxi::hx.hasResEx   = 0;
}

// Arms wipe `idx` to run for `duration` frames (TSMSFader passes seconds * rate). The wipe itself is advanced by
// Hx_UpdateWipe; without a provided resource the handlers capture into hx_buffer.
void Hx_StartWipe(int idx, int duration)
{
	if (hxi::hx.hasRes == 0) {
		hxi::hx.res      = hxi::hx_buffer;
		hxi::hx.resParam = sizeof(hxi::hx_buffer);
	}
	if (hxi::hx.state == 2)
		hxi::Hx_Warning(1);

	hxi::hx.state    = 1;
	hxi::hx.idx      = (u8)idx;
	hxi::hx.elapsed  = 0.0f;
	hxi::hx.duration = (u32)duration;
}

// Draws one frame of the running wipe and returns the wipe state: 0 idle, 2 running, 3 finished (stays 3; for
// type-0 wipes the finished state keeps drawing an opaque black screen). Called from TSMSFader::drawWipe every frame
// while a wipe is fading, with `dt` = TSMSFader::mRate.
u32 Hx_UpdateWipe(f32 dt)
{
	// Not `using namespace hxi`: the header declares same-named globals (Hx_CameraInit, ...) that would be ambiguous.
	hxi::HxState& hx = hxi::hx;

	ReInitializeGX();

	switch (hx.state) {
	case 1: // armed by Hx_StartWipe: latch handler and direction, then run the first frame right away
		hx.handler = hxi::handle_table[hx.idx < hxi::kWipeCount ? hx.idx : 0];
		hx.type    = hxi::handle_type[hx.idx < hxi::kWipeCount ? hx.idx : 0];
		hx.state   = 2;
		hx.stage   = 0;
		[[fallthrough]];
	case 2:
		hx.dt = dt;
		GXDrawDone();
		hx.handler();
		GXDrawDone();
		hx.elapsed = hx.elapsed + dt;
		break;
	case 3:
		if (hx.type != 1) {
			hxi::Hx_CameraInit();
			hxi::Hx_GxInit(0, 0);
			hxi::Frb2_InitBlackBox();
			hxi::Frb2_RendBox(0xFF, 0.0f, 0.0f, (f32)hx.width, (f32)hx.height);
		}
		break;
	default:
		break;
	}

	return hx.state;
}

// Resource (BTI image) used by the next wipes: the game-over/M-mark wipes take it from Hx_ProvideResource, the title
// logo wipe takes a second one from Hx_ProvideResourceEx. TSMSFader calls Hx_RemoveResource before Hx_ProvideResource.
void Hx_RemoveResource()
{
	if (hxi::hx.state == 2)
		hxi::Hx_Warning(1);
	if (hxi::hx.hasRes == 0)
		hxi::Hx_Warning(2);

	hxi::hx.hasRes   = 0;
	hxi::hx.hasResEx = 0;
}

void Hx_ProvideResourceEx(void* bti)
{
	if (hxi::hx.state == 2)
		hxi::Hx_Warning(1);

	hxi::hx.hasResEx = 1;
	hxi::hx.resEx    = bti;
}

void Hx_ProvideResource(void* bti, int param)
{
	if (hxi::hx.state == 2)
		hxi::Hx_Warning(1);
	if (hxi::hx.hasRes != 0)
		hxi::Hx_Warning(3);

	hxi::hx.hasRes   = 1;
	hxi::hx.res      = bti;
	hxi::hx.resParam = (u32)param;
}

// Title movie start synchronisation (TMovieDirector::direct, only polled while wipe index 12 - the M-mark logo - is
// the current one): returns 1 once per logo draw pass (sound cue for the logo), 2 once when the logo animation has
// reached the point where the movie may start, 0 otherwise.
int Hx_MovieStartSyncEx()
{
	const hxi::HxState& hx = hxi::hx;

	if (hx.idx != 12)
		return 0;

	const u32 stage = hx.stage;
	if (stage >= 2 && stage <= 5) {
		if (hxi::hxs_logodraw_resetflag == 0)
			return 0;
		hxi::hxs_logodraw_resetflag = 0;
		return 1;
	}
	if (stage >= 6) {
		if (hxi::hxs_logo_resetflag == 0)
			return 0;
		if (stage == 6 && hx.timer > 0xC0)
			return 0;
		hxi::hxs_logo_resetflag = 0;
		return 2;
	}
	return 0;
}

} // extern "C"
