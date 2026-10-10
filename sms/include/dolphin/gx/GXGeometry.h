#ifndef _DOLPHIN_GX_GXGEOMETRY_H_
#define _DOLPHIN_GX_GXGEOMETRY_H_

#include <dolphin/gx/GXEnum.h>
#include <dolphin/gx/GXStruct.h>

#ifdef __cplusplus
extern "C" {
#endif

void GXSetVtxDesc(GXAttr attr, GXAttrType type);
void GXSetVtxDescv(const GXVtxDescList* attrPtr);
void GXClearVtxDesc(void);
void GXSetVtxAttrFmt(GXVtxFmt vtxfmt, GXAttr attr, GXCompCnt cnt,
                     GXCompType type, u8 frac);
void GXSetVtxAttrFmtv(GXVtxFmt vtxfmt, const GXVtxAttrFmtList* list);
#ifdef TARGET_PC
// Aurora's GXSetArray carries the array size and endianness and writes a 64-bit pointer into the command stream.
// Call sites must use GXSETARRAY (size known) instead of the 3-argument console form.
void GXSetArray(GXAttr attr, const void* data, u32 size, u8 stride, bool le);
#define GXSETARRAY(attr, data, size, stride, le) GXSetArray((attr), (data), (size), (stride), (le))
#else
void GXSetArray(GXAttr attr, const void* base_ptr, u8 stride);
#define GXSETARRAY(attr, data, size, stride, le) GXSetArray((attr), (data), (stride))
#endif
void GXInvalidateVtxCache(void);
void GXSetTexCoordGen2(GXTexCoordID dst_coord, GXTexGenType func,
                       GXTexGenSrc src_param, u32 mtx, GXBool normalize,
                       u32 pt_texmtx);
void GXSetNumTexGens(u8 nTexGens);

static inline void GXSetTexCoordGen(GXTexCoordID dst_coord, GXTexGenType func,
                                    GXTexGenSrc src_param, u32 mtx)
{
	GXSetTexCoordGen2(dst_coord, func, src_param, mtx, GX_FALSE, GX_PTIDENTITY);
}

void GXBegin(GXPrimitive type, GXVtxFmt vtxfmt, u16 nverts);
static inline void GXEnd(void)
{
#if DEBUG
	extern GXBool __GXinBegin;
	extern void OSPanic(char* file, int line, char* msg, ...);
	if (!__GXinBegin) {
		OSPanic(__FILE__, 0x6D, "GXEnd: called without a GXBegin");
	}
	__GXinBegin = GX_FALSE;
#endif
}
void GXSetLineWidth(u8 width, GXTexOffset texOffsets);
void GXSetPointSize(u8 pointSize, GXTexOffset texOffsets);
void GXEnableTexOffsets(GXTexCoordID coord, u8 line_enable, u8 point_enable);
void GXSetCullMode(GXCullMode mode);

#ifdef __cplusplus
}
#endif

#endif
