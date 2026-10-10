#include <JSystem/JParticle/JPAEmitterLoader.hpp>
#include <JSystem/JParticle/JPAResourceManager.hpp>
#include <JSystem/JParticle/JPABaseShape.hpp>
#include <JSystem/JParticle/JPAExtraShape.hpp>
#include <JSystem/JParticle/JPASweepShape.hpp>
#include <JSystem/JParticle/JPAExTexShape.hpp>
#include <JSystem/JUtility/JUTAssert.hpp>
#include <types.h>

// The file image is big-endian (TARGET_PC): block count and block type/size words go through JPA_U32.
#ifdef TARGET_PC
#define JPA_BLOCK_NUM() JPA_U32(&unkC->unkC)
#else
#define JPA_BLOCK_NUM() (unkC->unkC)
#endif

JPAEmitterData* JPAEmitterLoaderDataBase::load(const u8* param_1,
                                               JKRHeap* param_2,
                                               JPATextureResource* param_3)
{
	JPABinaryHeader* header = (JPABinaryHeader*)param_1;
#ifdef TARGET_PC
	if (JPA_U32(&header->unk0) == 'JEFF' && JPA_U32(&header->unk4) == 'jpa1') {
#else
	if (header->unk0 == 'JEFF' && header->unk4 == 'jpa1') {
#endif
		JPAEmitterLoader_v10 loader(param_2, param_1, header);
		return loader.load(param_3);
	} else {
		return nullptr;
	}
}

JPAEmitterLoader_v10::JPAEmitterLoader_v10(JKRHeap* param_1, const u8* param_2,
                                           const JPABinaryHeader* param_3)
    : JPAEmitterLoader(param_1, param_2, param_3)
{
}

// fabricated, name from TWW
struct JPAEmitterBlockHeader {
	/* 0x0 */ u32 unk0;
	/* 0x4 */ u32 unk4;
};

JPAEmitterData* JPAEmitterLoader_v10::load(JPATextureResource* param_1)
{
	// TODO: fakematch, all vars were probably defined at the top originally
	u32 nextOffset;

	unk10 = new (mHeap, 0) JPAEmitterData;
	JUT_ASSERT(unk10);

	u32 offset = 0x20;

	u8 fld1Count = 0;
	u8 tex1Count = 0;
	u8 kfa1Count = 0;

	for (int i = 0; i < JPA_BLOCK_NUM(); ++i) {
		u32 type   = JPA_U32(unk8 + offset);
		nextOffset = JPA_U32(unk8 + offset + 4);

		if (type == 'FLD1') {
			++fld1Count;
		} else if (type == 'TEX1') {
			++tex1Count;
		} else if (type == 'KFA1') {
			++kfa1Count;
		}

		offset += nextOffset;
	}
	unk10->unk4 = 1;
	unk10->unk0 = new (mHeap, 0) JPADataBlockLinkInfo*[unk10->unk4];
	JUT_ASSERT(unk10->unk0);
	JPADataBlockLinkInfo* linkInfo = new (mHeap, 0) JPADataBlockLinkInfo;
	JUT_ASSERT(linkInfo);
	linkInfo->mKeyframeAnimationNum = kfa1Count;
	linkInfo->mKeyframeAnimations
	    = kfa1Count ? new (mHeap, 0) JPAKeyFrameAnime*[kfa1Count] : nullptr;
	JUT_ASSERT(linkInfo->mKeyframeAnimations
	           || linkInfo->mKeyframeAnimationNum == 0);
	linkInfo->mFieldNum = fld1Count;
	linkInfo->mFields
	    = fld1Count ? new (mHeap, 0) JPADataBlock*[fld1Count] : nullptr;
	JUT_ASSERT(linkInfo->mFields || linkInfo->mFieldNum == 0);
	linkInfo->mTextureNum = tex1Count;
	linkInfo->mTextureDataBase
	    = tex1Count ? (u16*)JKRHeap::alloc(tex1Count * sizeof(u16), 4, mHeap)
	                : nullptr;

	u32 offset2 = 0x20;

	u32 nextFld1 = 0;
	u32 nextKfa1 = 0;
	for (int i = 0; i < JPA_BLOCK_NUM() - tex1Count; ++i) {
		nextOffset = JPA_U32(unk8 + offset2 + 4);

		if (JPA_U32(unk8 + offset2) == 'FLD1') {
			linkInfo->mFields[nextFld1]
			    = new (mHeap, 0) JPADataBlock(unk8 + offset2, mHeap);
			JUT_ASSERT(linkInfo->mFields[nextFld1]);
			++nextFld1;
		} else if (JPA_U32(unk8 + offset2) == 'KFA1') {
			linkInfo->mKeyframeAnimations[nextKfa1]
			    = new (mHeap, 0) JPAKeyFrameAnime(unk8 + offset2, mHeap);
			JUT_ASSERT(linkInfo->mKeyframeAnimations[nextKfa1]);
			++nextKfa1;
		} else if (JPA_U32(unk8 + offset2) == 'BEM1') {
			linkInfo->mBaseEmitterBlock
			    = new (mHeap, 0) JPADataBlock(unk8 + offset2, mHeap);
			JUT_ASSERT(linkInfo->mBaseEmitterBlock);
		} else if (JPA_U32(unk8 + offset2) == 'BSP1') {
			linkInfo->mBaseShape
			    = new (mHeap, 0) JPABaseShape(unk8 + offset2, mHeap);
			JUT_ASSERT(linkInfo->mBaseShape);
		} else if (JPA_U32(unk8 + offset2) == 'ESP1') {
			linkInfo->mExtraShape
			    = new (mHeap, 0) JPAExtraShape(unk8 + offset2);
			JUT_ASSERT(linkInfo->mExtraShape);
		} else if (JPA_U32(unk8 + offset2) == 'SSP1') {
			linkInfo->mSweepShape
			    = new (mHeap, 0) JPASweepShape(unk8 + offset2);
			JUT_ASSERT(linkInfo->mSweepShape);
		} else if (JPA_U32(unk8 + offset2) == 'ETX1') {
			linkInfo->mExTexShape
			    = new (mHeap, 0) JPAExTexShape(unk8 + offset2);
			JUT_ASSERT(linkInfo->mExTexShape);
		}

		offset2 += nextOffset;
	}

	for (int i = 0, nextTex1 = 0; i < tex1Count; ++i) {
		nextOffset = JPA_U32(unk8 + offset2 + 4);

		linkInfo->mTextureDataBase[nextTex1]
		    = param_1->registration(unk8 + offset2, mHeap);
		++nextTex1;
		offset2 += nextOffset;
	}

	unk10->unk0[0] = linkInfo;

	return unk10;
}
