#include <System/ParamInst.hpp>
#include <JSystem/JGeometry.hpp>
#ifdef TARGET_PC
#include <hostendian.h>
#endif

template <typename T> void TParamT<T>::load(JSUMemoryInputStream& stream)
{
	// TODO: fakematch
	u8 discard[16];

	stream.read(&discard[8], 4);
	stream.read(&value, sizeof(T));
#ifdef TARGET_PC
	// The value is read raw (big-endian on disc, the stream never modifies the source buffer, so this is
	// naturally idempotent). Instances: u8 (nothing to swap), s16 (one halfword), s32/f32 (one word) and
	// TVec3<f32> (three f32 words).
	if (sizeof(T) == 2) {
		swapInPlace16(&value);
	}
	for (size_t i = 0; i + 4 <= sizeof(T); i += 4) {
		swapInPlace32((u8*)&value + i);
	}
#endif
};

template class TParamT<u8>;
template class TParamT<s16>;
template class TParamT<s32>;
template class TParamT<f32>;
template class TParamT<JGeometry::TVec3<f32> >;
