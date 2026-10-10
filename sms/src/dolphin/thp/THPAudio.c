#include <dolphin/thp.h>

#ifdef TARGET_PC
#include <hostendian.h>

// The audio frame header is big-endian on the disc.
static void THPSwapAudioHeader(THPAudioRecordHeader* h)
{
	int i;

	h->offsetNextChannel = HOST_BSWAP32(h->offsetNextChannel);
	h->sampleSize        = HOST_BSWAP32(h->sampleSize);
	for (i = 0; i < 8; i++) {
		h->lCoef[i][0] = (s16)HOST_BSWAP16((u16)h->lCoef[i][0]);
		h->lCoef[i][1] = (s16)HOST_BSWAP16((u16)h->lCoef[i][1]);
		h->rCoef[i][0] = (s16)HOST_BSWAP16((u16)h->rCoef[i][0]);
		h->rCoef[i][1] = (s16)HOST_BSWAP16((u16)h->rCoef[i][1]);
	}
	h->lYn1 = (s16)HOST_BSWAP16((u16)h->lYn1);
	h->lYn2 = (s16)HOST_BSWAP16((u16)h->lYn2);
	h->rYn1 = (s16)HOST_BSWAP16((u16)h->rYn1);
	h->rYn2 = (s16)HOST_BSWAP16((u16)h->rYn2);
}
#endif

u32 THPAudioDecode(s16* audioBuffer, u8* audioFrame, s32 flag)
{
	THPAudioRecordHeader* header;
	THPAudioDecodeInfo decInfo;
	u8 *left, *right;
	s16 *decLeftPtr, *decRightPtr;
	s16 yn1, yn2;
	s32 i;
	s32 step;
	s32 sample;
	s64 yn;

	if (audioBuffer == NULL || audioFrame == NULL) {
		return 0;
	}

#ifdef TARGET_PC
	// a movie held in memory is decoded again every loop: swap a copy, never the frame itself
	THPAudioRecordHeader hostHeader;
	memcpy(&hostHeader, audioFrame, sizeof(hostHeader));
	THPSwapAudioHeader(&hostHeader);
	header = &hostHeader;
#else
	header = (THPAudioRecordHeader*)audioFrame;
#endif
	left   = audioFrame + sizeof(THPAudioRecordHeader);
	right  = left + header->offsetNextChannel;

	if (flag == 1) {
		decRightPtr = audioBuffer;
		decLeftPtr  = audioBuffer + header->sampleSize;
		step        = 1;
	} else {
		decRightPtr = audioBuffer;
		decLeftPtr  = audioBuffer + 1;
		step        = 2;
	}

	if (header->offsetNextChannel == 0) {
		__THPAudioInitialize(&decInfo, left);

		yn1 = header->lYn1;
		yn2 = header->lYn2;

		for (i = 0; i < header->sampleSize; i++) {
			sample = __THPAudioGetNewSample(&decInfo);
			yn     = header->lCoef[decInfo.predictor][1] * yn2;
			yn += header->lCoef[decInfo.predictor][0] * yn1;
			yn += (sample << decInfo.scale) << 11;
			yn <<= 5;

			if ((u16)(yn & 0xffff) > 0x8000) {
				yn += 0x10000;
			} else if ((u16)(yn & 0xffff) == 0x8000) {
				if ((yn & 0x10000))
					yn += 0x10000;
			}

			if (yn > 2147483647LL) {
				yn = 2147483647LL;
			}

			if (yn < -2147483648LL) {
				yn = -2147483648LL;
			}

			*decLeftPtr = (s16)(yn >> 16);
			decLeftPtr += step;
			*decRightPtr = (s16)(yn >> 16);
			decRightPtr += step;
			yn2 = yn1;
			yn1 = (s16)(yn >> 16);
		}
	} else {
		__THPAudioInitialize(&decInfo, left);

		yn1 = header->lYn1;
		yn2 = header->lYn2;

		for (i = 0; i < header->sampleSize; i++) {
			sample = __THPAudioGetNewSample(&decInfo);
			yn     = header->lCoef[decInfo.predictor][1] * yn2;
			yn += header->lCoef[decInfo.predictor][0] * yn1;
			yn += (sample << decInfo.scale) << 11;
			yn <<= 5;
			if ((u16)(yn & 0xffff) > 0x8000) {
				yn += 0x10000;
			} else {
				if ((u16)(yn & 0xffff) == 0x8000) {
					if ((yn & 0x10000))
						yn += 0x10000;
				}
			}

			if (yn > 2147483647LL) {
				yn = 2147483647LL;
			}

			if (yn < -2147483648LL) {
				yn = -2147483648LL;
			}

			*decLeftPtr = (s16)(yn >> 16);
			decLeftPtr += step;
			yn2 = yn1;
			yn1 = (s16)(yn >> 16);
		}

		__THPAudioInitialize(&decInfo, right);

		yn1 = header->rYn1;
		yn2 = header->rYn2;

		for (i = 0; i < header->sampleSize; i++) {
			sample = __THPAudioGetNewSample(&decInfo);
			yn     = header->rCoef[decInfo.predictor][1] * yn2;
			yn += header->rCoef[decInfo.predictor][0] * yn1;
			yn += (sample << decInfo.scale) << 11;
			yn <<= 5;

			if ((u16)(yn & 0xffff) > 0x8000) {
				yn += 0x10000;
			} else {
				if ((u16)(yn & 0xffff) == 0x8000) {
					if ((yn & 0x10000))
						yn += 0x10000;
				}
			}

			if (yn > 2147483647LL) {
				yn = 2147483647LL;
			}

			if (yn < -2147483648LL) {
				yn = -2147483648LL;
			}

			*decRightPtr = (s16)(yn >> 16);
			decRightPtr += step;
			yn2 = yn1;
			yn1 = (s16)(yn >> 16);
		}
	}

	return header->sampleSize;
}

static s32 __THPAudioGetNewSample(THPAudioDecodeInfo* info)
{
	s32 sample;

	if (!(info->offsetNibbles & 0x0f)) {
		info->predictor = (u8)((*(info->encodeData) & 0x70) >> 4);
		info->scale     = (u8)((*(info->encodeData) & 0xF));
		info->encodeData++;
		info->offsetNibbles += 2;
	}

	if (info->offsetNibbles & 0x1) {
		sample = (s32)((*(info->encodeData) & 0xF) << 28) >> 28;
		info->encodeData++;
	} else {
		sample = (s32)((*(info->encodeData) & 0xF0) << 24) >> 28;
	}

	info->offsetNibbles++;
	return sample;
}

static void __THPAudioInitialize(THPAudioDecodeInfo* info, u8* ptr)
{
	info->encodeData    = ptr;
	info->offsetNibbles = 2;
	info->predictor     = (u8)((*(info->encodeData) & 0x70) >> 4);
	info->scale         = (u8)((*(info->encodeData) & 0xF));
	info->encodeData++;
}
