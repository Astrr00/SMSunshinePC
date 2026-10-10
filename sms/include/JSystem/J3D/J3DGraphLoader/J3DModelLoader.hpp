#ifndef J3D_MODEL_LOADER_HPP
#define J3D_MODEL_LOADER_HPP

#include <types.h>
#include <JSystem/J3D/J3DGraphAnimator/J3DModel.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DModelLoaderFlags.hpp>
#include <JSystem/JUtility/JUTDataHeader.hpp>

#ifdef TARGET_PC
// The blocks hold 32-bit offsets from the block start; the console relocates them in place to pointers. A host pointer
// is 8 bytes, so the members stay u32 on the host to keep the disc layout (convert with JSUConvertOffsetToPtr).
typedef u32 J3DBlockOfs;
#else
typedef void* J3DBlockOfs;
#endif

struct J3DModelInfoBlock : public JUTDataBlockHeader {
	/* 0x08 */ u16 mFlags;
	/* 0x0C */ u32 mPacketNum;
	/* 0x10 */ u32 mVtxNum;
	/* 0x14 */ J3DBlockOfs mpHierarchy;
}; // Size: 0x18

struct J3DVertexBlock : public JUTDataBlockHeader {
	/* 0x08 */ J3DBlockOfs mpVtxAttrFmtList;
	/* 0x0C */ J3DBlockOfs mpVtxPosArray;
	/* 0x10 */ J3DBlockOfs mpVtxNrmArray;
	/* 0x14 */ J3DBlockOfs mpVtxNBTArray;
	/* 0x18 */ J3DBlockOfs mpVtxColorArray[2];
	/* 0x20 */ J3DBlockOfs mpVtxTexCoordArray[8];
}; // Size: 0x40

struct J3DEnvelopBlock : public JUTDataBlockHeader {
	/* 0x08 */ u16 mWEvlpMtxNum;
	/* 0x0C */ J3DBlockOfs mpWEvlpMixMtxNum;
	/* 0x10 */ J3DBlockOfs mpWEvlpMixMtxIndex;
	/* 0x14 */ J3DBlockOfs mpWEvlpMixWeight;
	/* 0x18 */ J3DBlockOfs mpInvJointMtx;
}; // Size: 0x1C

struct J3DDrawBlock : public JUTDataBlockHeader {
	/* 0x08 */ u16 mMtxNum;
	/* 0x0C */ J3DBlockOfs mpDrawMtxFlag;
	/* 0x10 */ J3DBlockOfs mpDrawMtxIndex;
}; // Size: 0x14

struct J3DJointBlock;
struct J3DMaterialBlock;
struct J3DMaterialBlock_v21;

struct J3DMaterialDLBlock : public JUTDataBlockHeader {
	/* 0x08 */ u16 mMaterialNum;
	/* 0x0C */ J3DBlockOfs mpDisplayListInit;
	/* 0x10 */ J3DBlockOfs mpPatchingInfo;
	/* 0x14 */ J3DBlockOfs mpCurrentMtxInfo;
	/* 0x18 */ J3DBlockOfs field_0x18;
	/* 0x1C */ J3DBlockOfs field_0x1c;
	/* 0x20 */ J3DBlockOfs mpNameTable;
	/* more */
};

struct J3DShapeBlock;

struct J3DTextureBlock : public JUTDataBlockHeader {
	/* 0x08 */ u16 mTextureNum;
	/* 0x0C */ J3DBlockOfs mpTextureRes;
	/* 0x10 */ J3DBlockOfs mpNameTable;
};

class J3DModelLoaderDataBase {
public:
	static J3DModelData* load(const void*, u32);
	static J3DMaterialTable* loadMaterialTable(const void*);
};

class J3DModelLoader {
public:
	J3DModelLoader()
	{
		mpModelData     = nullptr;
		mpShapeBlock    = nullptr;
		mpMaterialTable = nullptr;
	}

	virtual J3DModelData* load(const void*, u32);
	virtual J3DMaterialTable* loadMaterialTable(const void*);
	virtual void setupBBoardInfo();
	virtual ~J3DModelLoader() { }
	virtual void readMaterial(const J3DMaterialBlock*, u32) { }
	virtual void readMaterial_v21(const J3DMaterialBlock_v21*, u32) { }
	virtual void readMaterialTable(const J3DMaterialBlock*, u32) { }
	virtual void readMaterialTable_v21(const J3DMaterialBlock_v21*, u32) { }

	void readInformation(const J3DModelInfoBlock*, u32);
	void readVertex(const J3DVertexBlock*);
	void readEnvelop(const J3DEnvelopBlock*);
	void readDraw(const J3DDrawBlock*);
	void readJoint(const J3DJointBlock*);
	void readShape(const J3DShapeBlock*, u32);
	void readTexture(const J3DTextureBlock*);
	void readTextureTable(const J3DTextureBlock*);

protected:
	/* 0x04 */ J3DModelData* mpModelData;
	/* 0x08 */ J3DMaterialTable* mpMaterialTable;
	/* 0x0C */ const J3DShapeBlock* mpShapeBlock;
};

class J3DModelLoader_v26 : public J3DModelLoader {
public:
	virtual ~J3DModelLoader_v26() { }
	virtual void readMaterial(const J3DMaterialBlock*, u32);
	virtual void readMaterialTable(const J3DMaterialBlock*, u32);
};

class J3DModelLoader_v21 : public J3DModelLoader {
public:
	~J3DModelLoader_v21() { }
	void readMaterial_v21(const J3DMaterialBlock_v21*, u32);
	void readMaterialTable_v21(const J3DMaterialBlock_v21*, u32);
};

#endif
