#include <JSystem/J3D/J3DGraphLoader/J3DModelLoader.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DShape.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DTexture.hpp>
#include <JSystem/J3D/J3DGraphAnimator/J3DJoint.hpp>
#include <JSystem/J3D/J3DGraphBase/J3DMaterial.hpp>
#include <JSystem/JSupport.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DShapeFactory.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DJointFactory.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DMaterialFactory.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DMaterialFactory_v21.hpp>
#include <JSystem/JUtility/JUTNameTab.hpp>
#include <JSystem/JKernel/JKRHeap.hpp>

#ifdef TARGET_PC
#include <JSystem/J3D/J3DGraphLoader/J3DModelSwapPC.hpp>
#include <JSystem/J3D/J3DGraphLoader/J3DMatSwapPC.hpp>
#endif

#ifdef TARGET_PC
// J3DMaterial::unk18 is a 32-bit word on the host with DIFF_FLAG/UNIQUE_FLAG in bits 31/30 (see J3DMaterial.hpp), so an
// address-derived ID may only use its low 30 bits. On the console every address is 0x80000000..0x817fffff, which leaves
// bits 31/30 clear for `address >> 4` and bit 31 set for a raw address; reproduce both patterns.
#define J3D_MAT_ID_UNIQUE(p)    ((u32)(PTR_U32(p) >> 4) & 0x3FFFFFFF)
#define J3D_MAT_ID_TABLE(p, id) (J3DMaterial::DIFF_FLAG | ((u32)(PTR_U32(p) + (id)) & 0x3FFFFFFF))
#else
#define J3D_MAT_ID_UNIQUE(p)    ((u32)(p) >> 4)
#define J3D_MAT_ID_TABLE(p, id) ((u32)(p) + (id))
#endif

#ifdef TARGET_PC
// Converts a BMD/BMT file to host byte order, once per buffer: J3DSwapFileHeaderPC() returns true only for the first
// call (the file magic is the marker), so a cached or re-mounted resource is never swapped twice.
static void swapModelFilePC(const void* i_data)
{
	void* file = const_cast<void*>(i_data);
	if (!J3DSwapFileHeaderPC(file))
		return;

	JUTDataFileHeader* header = (JUTDataFileHeader*)file;
	JUTDataBlockHeader* block = &header->mFirstBlock;
	for (u32 i = 0; i < header->mBlockNum; i++) {
		switch (block->mType) {
		case 'INF1':
			J3DSwapInf1PC((u8*)block);
			break;
		case 'VTX1':
			J3DSwapVtx1PC((u8*)block);
			break;
		case 'EVP1':
			J3DSwapEvp1PC((u8*)block);
			break;
		case 'DRW1':
			J3DSwapDrw1PC((u8*)block);
			break;
		case 'JNT1':
			J3DSwapJnt1PC((u8*)block);
			break;
		case 'MAT3':
			J3DSwapMat3BlockPC(block);
			break;
		case 'MAT2':
			J3DSwapMat2BlockPC(block);
			break;
		case 'SHP1':
			J3DSwapShp1PC((u8*)block);
			break;
		case 'TEX1':
			J3DSwapTex1PC((u8*)block);
			break;
		default: // MDL3 is not read by this loader
			break;
		}
		block = (JUTDataBlockHeader*)((u8*)block + block->mSize);
	}
}
#endif

J3DModelData* J3DModelLoaderDataBase::load(const void* i_data, u32 i_flags)
{
	if (i_data == nullptr)
		return nullptr;
#ifdef TARGET_PC
	swapModelFilePC(i_data);
#endif

	const JUTDataFileHeader* fileHeader = (const JUTDataFileHeader*)i_data;
	u32 magic                           = fileHeader->mMagic;

	if (magic == 'J3D1' && fileHeader->mType == 'bmd1') {
		return nullptr;
	}

	if (magic == 'J3D2' && fileHeader->mType == 'bmd2') {
		J3DModelLoader_v21 loader;
		return loader.load(i_data, i_flags);
	}

	if (magic == 'J3D2' && fileHeader->mType == 'bmd3') {
		J3DModelLoader_v26 loader;
		return loader.load(i_data, i_flags);
	}

	return nullptr;
}

J3DMaterialTable* J3DModelLoaderDataBase::loadMaterialTable(const void* i_data)
{
	if (!i_data)
		return nullptr;
#ifdef TARGET_PC
	swapModelFilePC(i_data);
#endif

	const JUTDataFileHeader* fileHeader = (const JUTDataFileHeader*)i_data;

	u32 magic = fileHeader->mMagic;
	if (magic == 'J3D1' && fileHeader->mType == 'bmt1') {
		return nullptr;
	}

	if (magic == 'J3D2' && fileHeader->mType == 'bmt2') {
		J3DModelLoader_v21 loader;
		return loader.loadMaterialTable(i_data);
	}

	if (magic == 'J3D2' && fileHeader->mType == 'bmt3') {
		J3DModelLoader_v26 loader;
		return loader.loadMaterialTable(i_data);
	}
	return nullptr;
}

J3DModelData* J3DModelLoader::load(const void* i_data, u32 i_flags)
{
	mpModelData = new J3DModelData();
	mpModelData->clear();
	mpModelData->unk4               = i_data;
	const JUTDataFileHeader* data   = (JUTDataFileHeader*)i_data;
	u32 totalSize                   = 0;
	const JUTDataBlockHeader* block = &data->mFirstBlock;
	for (u32 block_no = 0; block_no < data->mBlockNum; block_no++) {
		switch (block->mType) {
		case 'INF1':
			readInformation((J3DModelInfoBlock*)block, i_flags);
			break;
		case 'VTX1':
			readVertex((J3DVertexBlock*)block);
			break;
		case 'EVP1':
			readEnvelop((J3DEnvelopBlock*)block);
			break;
		case 'DRW1':
			readDraw((J3DDrawBlock*)block);
			break;
		case 'JNT1':
			readJoint((J3DJointBlock*)block);
			break;
		case 'MAT3':
			readMaterial((J3DMaterialBlock*)block, i_flags);
			break;
		case 'MAT2':
			readMaterial_v21((J3DMaterialBlock_v21*)block, i_flags);
			break;
		case 'SHP1':
			readShape((J3DShapeBlock*)block, i_flags);
			break;
		case 'TEX1':
			readTexture((J3DTextureBlock*)block);
			break;
		default:
			break;
		}
		totalSize += block->mSize;
		block = (const JUTDataBlockHeader*)((u8*)block + block->mSize);
	}
	const J3DModelHierarchy* hierarchy = mpModelData->unk8;
	mpModelData->makeHierarchy(nullptr, &hierarchy);
	setupBBoardInfo();
	return mpModelData;
}

J3DMaterialTable* J3DModelLoader::loadMaterialTable(const void* i_data)
{
	u32 thing       = 0x55100000;
	mpMaterialTable = new J3DMaterialTable();
	mpMaterialTable->clear();
	const JUTDataFileHeader* data   = (JUTDataFileHeader*)i_data;
	const JUTDataBlockHeader* block = &data->mFirstBlock;
	for (u32 block_no = 0; block_no < data->mBlockNum; block_no++) {
		switch (block->mType) {
		case 'MAT3':
			readMaterialTable((J3DMaterialBlock*)block, thing);
			break;
		case 'MAT2':
			readMaterialTable_v21((J3DMaterialBlock_v21*)block, thing);
			break;
		case 'TEX1':
			readTextureTable((J3DTextureBlock*)block);
			break;
		default:
			break;
		}
		block = (const JUTDataBlockHeader*)((u8*)block + block->mSize);
	}

	if (mpMaterialTable->mTexture == nullptr)
		mpMaterialTable->mTexture = new J3DTexture(0, nullptr);

	return mpMaterialTable;
}

void J3DModelLoader::setupBBoardInfo()
{
	for (u16 i = 0; i < mpModelData->getJointNum(); i++) {
		J3DMaterial* mesh = mpModelData->getJointNodePointer(i)->getMesh();
		if (!mesh)
			continue;

		u32 shapeIndex  = mesh->getShape()->getIndex();
		u16* indexTable = JSUConvertOffsetToPtr<u16>(
		    mpShapeBlock, (u32)mpShapeBlock->mpIndexTable);
		J3DShapeInitData* shapeInitDatas
		    = JSUConvertOffsetToPtr<J3DShapeInitData>(
		        mpShapeBlock, (u32)mpShapeBlock->mpShapeInitData);

		J3DShapeInitData* shapeInitData
		    = &shapeInitDatas[indexTable[shapeIndex]];
		switch (shapeInitData->mShapeMtxType) {
		case 0:
			mpModelData->getJointNodePointer(i)->setMtxType(0);
			break;
		case 1:
			mpModelData->getJointNodePointer(i)->setMtxType(1);
			mpModelData->mbHasBillboard = true;
			break;
		case 2:
			mpModelData->getJointNodePointer(i)->setMtxType(2);
			mpModelData->mbHasBillboard = true;
			break;
		case 3:
			mpModelData->getJointNodePointer(i)->setMtxType(0);
			break;
		default:
			break;
		}
	}
}

void J3DModelLoader::readInformation(const J3DModelInfoBlock* i_block,
                                     u32 i_flags)
{
	mpModelData->unkC = i_flags | i_block->mFlags;
	switch (mpModelData->unkC & J3DMLF_MtxCalcMask) {
	case J3DMLF_MtxCalcBasic:
		mpModelData->unk14 = new J3DMtxCalcBasic();
		break;
	case J3DMLF_MtxCalcSoftImage:
		mpModelData->unk14 = new J3DMtxCalcSoftimage();
		break;
	case J3DMLF_MtxCalcMaya:
		mpModelData->unk14 = new J3DMtxCalcMaya();
		break;
	}
	mpModelData->unk80                   = i_block->mPacketNum;
	mpModelData->getVertexData().mVtxNum = i_block->mVtxNum;
	mpModelData->unk8 = JSUConvertOffsetToPtr<J3DModelHierarchy>(
	    i_block, i_block->mpHierarchy);
}

static GXCompType getFmtType(GXVtxAttrFmtList* i_fmtList, GXAttr i_attr)
{
	for (; i_fmtList->attr != GX_VA_NULL; i_fmtList++) {
		if (i_fmtList->attr == i_attr) {
			return i_fmtList->type;
		}
	}
	return GX_F32;
}

#ifdef TARGET_PC
static u32 vtxArraySizePC(const J3DVertexBlock* i_block, u32 i_offset)
{
	return i_offset == 0 ? 0 : J3DVtxArraySpanPC((const u8*)i_block, i_offset);
}
#endif

void J3DModelLoader::readVertex(const J3DVertexBlock* i_block)
{
	J3DVertexData& vertex_data  = mpModelData->getVertexData();
	vertex_data.mVtxAttrFmtList = JSUConvertOffsetToPtr<GXVtxAttrFmtList>(
	    i_block, i_block->mpVtxAttrFmtList);
	vertex_data.mVtxPosArray
	    = JSUConvertOffsetToPtr<void>(i_block, i_block->mpVtxPosArray);
	vertex_data.mVtxNormArray
	    = JSUConvertOffsetToPtr<void>(i_block, i_block->mpVtxNrmArray);
	vertex_data.mVtxNBTArray
	    = JSUConvertOffsetToPtr<void>(i_block, i_block->mpVtxNBTArray);
	for (int i = 0; i < 2; i++) {
		vertex_data.mVtxColorArray[i] = (GXColor*)JSUConvertOffsetToPtr<void>(
		    i_block, i_block->mpVtxColorArray[i]);
	}
	for (int i = 0; i < 8; i++) {
		vertex_data.mVtxTexCoordArray[i] = JSUConvertOffsetToPtr<void>(
		    i_block, i_block->mpVtxTexCoordArray[i]);
	}

	void* nrm_end = NULL;
	if (vertex_data.mVtxNBTArray != NULL) {
		nrm_end = vertex_data.mVtxNBTArray;
	} else if (vertex_data.mVtxColorArray[0] != NULL) {
		nrm_end = vertex_data.mVtxColorArray[0];
	} else if (vertex_data.mVtxTexCoordArray[0] != NULL) {
		nrm_end = vertex_data.mVtxTexCoordArray[0];
	}

	if (vertex_data.mVtxNormArray == NULL) {
		vertex_data.mNrmNum = 0;
	} else if (nrm_end != NULL) {
		vertex_data.mNrmNum
		    = (u32)((PTR_U32(nrm_end) - PTR_U32(vertex_data.mVtxNormArray)) / 12 + 1);
	} else {
		vertex_data.mNrmNum
		    = (i_block->mSize - (u32)i_block->mpVtxNrmArray) / 12 + 1;
	}

	void* color0_end = NULL;
	if (vertex_data.mVtxColorArray[1] != NULL) {
		color0_end = vertex_data.mVtxColorArray[1];
	} else if (vertex_data.mVtxTexCoordArray[0] != NULL) {
		color0_end = vertex_data.mVtxTexCoordArray[0];
	}

	if (vertex_data.mVtxColorArray[0] == NULL) {
		vertex_data.mColNum = 0;
	} else if (color0_end != NULL) {
		vertex_data.mColNum
		    = (u32)((PTR_U32(color0_end) - PTR_U32(vertex_data.mVtxColorArray[0])) / 4 + 1);
	} else {
		vertex_data.mColNum
		    = (i_block->mSize - (u32)i_block->mpVtxColorArray[0]) / 4 + 1;
	}

#ifdef TARGET_PC
	// Array extents for Aurora (J3DShape::makeVtxArrayCmd / loadVtxArray): the position array holds exactly mVtxNum
	// vertices (checked against the file layout offline: span - payload < 32 for every shipped BMD), the others span up to
	// the next array of the block.
	vertex_data.mVtxPosSize
	    = vertex_data.mVtxPosArray == nullptr
	          ? 0
	          : vertex_data.mVtxNum
	                * (getFmtType(vertex_data.mVtxAttrFmtList, GX_VA_POS) == GX_F32 ? 12 : 6);
	vertex_data.mVtxNrmSize = vtxArraySizePC(i_block, i_block->mpVtxNrmArray);
	vertex_data.mVtxNBTSize = vtxArraySizePC(i_block, i_block->mpVtxNBTArray);
	for (int i = 0; i < 2; i++)
		vertex_data.mVtxColorSize[i] = vtxArraySizePC(i_block, i_block->mpVtxColorArray[i]);
	for (int i = 0; i < 8; i++)
		vertex_data.mVtxTexCoordSize[i] = vtxArraySizePC(i_block, i_block->mpVtxTexCoordArray[i]);
#endif
}

void J3DModelLoader::readEnvelop(const J3DEnvelopBlock* i_block)
{
	mpModelData->mWEvlpMtxNum = i_block->mWEvlpMtxNum;
	mpModelData->unk88
	    = JSUConvertOffsetToPtr<u8>(i_block, i_block->mpWEvlpMixMtxNum);
	mpModelData->unk8C
	    = JSUConvertOffsetToPtr<u16>(i_block, i_block->mpWEvlpMixMtxIndex);
	mpModelData->unk90
	    = JSUConvertOffsetToPtr<f32>(i_block, i_block->mpWEvlpMixWeight);
	mpModelData->unk94
	    = JSUConvertOffsetToPtr<Mtx>(i_block, i_block->mpInvJointMtx);
}

void J3DModelLoader::readDraw(const J3DDrawBlock* i_block)
{
	J3DDrawMtxData& data = mpModelData->getDrawMtxData();
	data.mEntryNum       = i_block->mMtxNum;
	data.mDrawMtxFlag
	    = JSUConvertOffsetToPtr<u8>(i_block, i_block->mpDrawMtxFlag);
	data.mDrawMtxIndex
	    = JSUConvertOffsetToPtr<u16>(i_block, i_block->mpDrawMtxIndex);
	u16 i;
	for (i = 0; i < data.mEntryNum; i++) {
		if (data.mDrawMtxFlag[i] == 1) {
			break;
		}
	}
	data.mDrawFullWgtMtxNum = i;
}

void J3DModelLoader::readJoint(const J3DJointBlock* i_block)
{
	J3DJointFactory factory(*i_block);
	mpModelData->mJointNum = i_block->mJointNum;
	if (i_block->mpNameTable != 0) {
		mpModelData->unkB0 = new JUTNameTab(
		    JSUConvertOffsetToPtr<ResNTAB>(i_block, i_block->mpNameTable));
	} else {
		mpModelData->unkB0 = nullptr;
	}
	mpModelData->mJointNodePointer = new J3DJoint*[mpModelData->mJointNum];
	for (u16 i = 0; i < mpModelData->mJointNum; i++) {
		mpModelData->mJointNodePointer[i] = factory.create(i);
	}
}

void J3DModelLoader_v26::readMaterial(const J3DMaterialBlock* i_block,
                                      u32 i_flags)
{
	J3DMaterialFactory factory(*i_block);
	mpModelData->mMaterialNum = i_block->mMaterialNum;
	mpModelData->unk34        = factory.countUniqueMaterials();
	if (i_block->mpNameTable != 0) {
		mpModelData->mMaterialName = new JUTNameTab(
		    JSUConvertOffsetToPtr<ResNTAB>(i_block, i_block->mpNameTable));
	} else {
		mpModelData->mMaterialName = nullptr;
	}
	mpModelData->mMaterials = new J3DMaterial*[mpModelData->mMaterialNum];
	if (i_flags & J3DMLF_UseUniqueMaterials) {
		mpModelData->unk38 = new (0x20) J3DMaterial[mpModelData->unk34];
	} else {
		mpModelData->unk38 = nullptr;
	}
	if (i_flags & J3DMLF_UseUniqueMaterials) {
		for (u16 i = 0; i < mpModelData->unk34; i++) {
			factory.create(&mpModelData->unk38[i], i, i_flags);
			mpModelData->unk38[i].unk18 = J3D_MAT_ID_UNIQUE(&mpModelData->unk38[i]);
		}
	}
	for (u16 i = 0; i < mpModelData->mMaterialNum; i++) {
		mpModelData->mMaterials[i] = factory.create(nullptr, i, i_flags);
	}
	if (i_flags & J3DMLF_UseUniqueMaterials) {
		for (u16 i = 0; i < mpModelData->mMaterialNum; i++) {
			mpModelData->mMaterials[i]->unk18
			    = J3D_MAT_ID_UNIQUE(&mpModelData->unk38[factory.getMaterialID(i)]);
			mpModelData->mMaterials[i]->mOriginalMaterial
			    = &mpModelData->unk38[factory.getMaterialID(i)];
		}
	} else {
		for (u16 i = 0; i < mpModelData->mMaterialNum; i++) {
			mpModelData->mMaterials[i]->unk18
			    = J3DMaterial::DIFF_FLAG | J3DMaterial::UNIQUE_FLAG;
		}
	}
}

void J3DModelLoader_v21::readMaterial_v21(const J3DMaterialBlock_v21* i_block,
                                          u32 i_flags)
{
	J3DMaterialFactory_v21 factory(*i_block);
	mpModelData->mMaterialNum = i_block->mMaterialNum;
	mpModelData->unk34        = factory.countUniqueMaterials();
	if (i_block->mpNameTable != 0) {
		mpModelData->mMaterialName = new JUTNameTab(
		    JSUConvertOffsetToPtr<ResNTAB>(i_block, i_block->mpNameTable));
	} else {
		mpModelData->mMaterialName = nullptr;
	}
	mpModelData->mMaterials = new J3DMaterial*[mpModelData->mMaterialNum];
	if (i_flags & J3DMLF_UseUniqueMaterials) {
		mpModelData->unk38 = new (0x20) J3DMaterial[mpModelData->unk34];
	} else {
		mpModelData->unk38 = nullptr;
	}
	if (i_flags & J3DMLF_UseUniqueMaterials) {
		for (u16 i = 0; i < mpModelData->unk34; i++) {
			factory.create(&mpModelData->unk38[i], i, i_flags);
			mpModelData->unk38[i].unk18 = J3D_MAT_ID_UNIQUE(&mpModelData->unk38[i]);
		}
	}
	for (u16 i = 0; i < mpModelData->mMaterialNum; i++) {
		mpModelData->mMaterials[i] = factory.create(nullptr, i, i_flags);
	}
	if (i_flags & J3DMLF_UseUniqueMaterials) {
		for (u16 i = 0; i < mpModelData->mMaterialNum; i++) {
			mpModelData->mMaterials[i]->unk18
			    = J3D_MAT_ID_UNIQUE(&mpModelData->unk38[factory.getMaterialID(i)]);
			mpModelData->mMaterials[i]->mOriginalMaterial
			    = &mpModelData->unk38[factory.getMaterialID(i)];
		}
	} else {
		for (u16 i = 0; i < mpModelData->mMaterialNum; i++) {
			mpModelData->mMaterials[i]->unk18
			    = J3DMaterial::DIFF_FLAG | J3DMaterial::UNIQUE_FLAG;
		}
	}
}

void J3DModelLoader::readShape(const J3DShapeBlock* i_block, u32 i_flags)
{
	mpShapeBlock = i_block;
	J3DShapeFactory factory(*i_block);
	mpModelData->mShapeNum = i_block->mShapeNum;
	if (i_block->mpNameTable != 0) {
		mpModelData->unkB8 = new JUTNameTab(
		    JSUConvertOffsetToPtr<ResNTAB>(i_block, i_block->mpNameTable));
	} else {
		mpModelData->unkB8 = nullptr;
	}
	mpModelData->mShapeNodePointer = new J3DShape*[mpModelData->mShapeNum];
	factory.allocVcdVatCmdBuffer(mpModelData->mShapeNum);
	const J3DModelHierarchy* hierarchy_entry = mpModelData->unk8;
	GXVtxDescList* vtx_desc_list             = nullptr;
	for (; hierarchy_entry->mType != 0; hierarchy_entry++) {
		if (hierarchy_entry->mType == 0x12) {
			mpModelData->mShapeNodePointer[hierarchy_entry->mValue]
			    = factory.create(hierarchy_entry->mValue,
			                     (J3DMdlDataFlag)i_flags, vtx_desc_list);
			vtx_desc_list = factory.getVtxDescList(hierarchy_entry->mValue);
		}
	}
}

void J3DModelLoader::readTexture(const J3DTextureBlock* i_block)
{
	u16 texture_num = i_block->mTextureNum;
	ResTIMG* texture_res
	    = JSUConvertOffsetToPtr<ResTIMG>(i_block, i_block->mpTextureRes);
	if (i_block->mpNameTable != 0) {
		mpModelData->unkA8 = new JUTNameTab(
		    JSUConvertOffsetToPtr<ResNTAB>(i_block, i_block->mpNameTable));
	} else {
		mpModelData->unkA8 = nullptr;
	}
	mpModelData->unkAC = new J3DTexture(texture_num, texture_res);
}

void J3DModelLoader_v26::readMaterialTable(const J3DMaterialBlock* i_block,
                                           u32 i_flags)
{
	J3DMaterialFactory factory(*i_block);
	mpMaterialTable->mMaterialNum = i_block->mMaterialNum;
	if (i_block->mpNameTable != 0) {
		mpMaterialTable->mMaterialName = new JUTNameTab(
		    JSUConvertOffsetToPtr<ResNTAB>(i_block, i_block->mpNameTable));
	} else {
		mpMaterialTable->mMaterialName = nullptr;
	}
	mpMaterialTable->mMaterials
	    = new J3DMaterial*[mpMaterialTable->mMaterialNum];
	for (u16 i = 0; i < mpMaterialTable->mMaterialNum; i++) {
		mpMaterialTable->mMaterials[i] = factory.create(nullptr, i, i_flags);
	}
	for (u16 i = 0; i < mpMaterialTable->mMaterialNum; i++) {
		mpMaterialTable->mMaterials[i]->unk18
		    = J3D_MAT_ID_TABLE(mpMaterialTable->mMaterials, factory.getMaterialID(i));
	}
}

void J3DModelLoader_v21::readMaterialTable_v21(
    const J3DMaterialBlock_v21* i_block, u32 i_flags)
{
	J3DMaterialFactory_v21 factory(*i_block);
	mpMaterialTable->mMaterialNum = i_block->mMaterialNum;
	if (i_block->mpNameTable != 0) {
		mpMaterialTable->mMaterialName = new JUTNameTab(
		    JSUConvertOffsetToPtr<ResNTAB>(i_block, i_block->mpNameTable));
	} else {
		mpMaterialTable->mMaterialName = nullptr;
	}
	mpMaterialTable->mMaterials
	    = new J3DMaterial*[mpMaterialTable->mMaterialNum];
	for (u16 i = 0; i < mpMaterialTable->mMaterialNum; i++) {
		mpMaterialTable->mMaterials[i] = factory.create(nullptr, i, i_flags);
	}
	for (u16 i = 0; i < mpMaterialTable->mMaterialNum; i++) {
		mpMaterialTable->mMaterials[i]->unk18
		    = J3D_MAT_ID_TABLE(mpMaterialTable->mMaterials, factory.getMaterialID(i));
	}
}

void J3DModelLoader::readTextureTable(const J3DTextureBlock* i_block)
{
	u16 texture_num = i_block->mTextureNum;
	ResTIMG* texture_res
	    = JSUConvertOffsetToPtr<ResTIMG>(i_block, i_block->mpTextureRes);
	if (i_block->mpNameTable != 0) {
		mpMaterialTable->mTextureName = new JUTNameTab(
		    JSUConvertOffsetToPtr<ResNTAB>(i_block, i_block->mpNameTable));
	} else {
		mpMaterialTable->mTextureName = nullptr;
	}
	mpMaterialTable->mTexture = new J3DTexture(texture_num, texture_res);
}
