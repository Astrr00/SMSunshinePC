#include <JSystem/JKernel/JKRArchive.hpp>
#include <JSystem/JKernel/JKRHeap.hpp>
#include <JSystem/JUtility/JUTAssert.hpp>
#include <ctype.h>
#ifdef TARGET_PC
#include <hostendian.h>
#include <JSystem/ResTIMG.hpp>
#include <stdlib.h>
#endif

u32 JKRArchive::sCurrentDirID;

JKRArchive::JKRArchive()
{
	mIsMounted      = false;
	mMountDirection = MOUNT_DIRECTION_HEAD;
#ifdef TARGET_PC
	mHostFileEntries = nullptr;
#endif
}

JKRArchive::JKRArchive(s32 entryNumber, JKRArchive::EMountMode mountMode)
{
#ifdef TARGET_PC
	mHostFileEntries = nullptr;
#endif
	mIsMounted  = false;
	mMountMode  = mountMode;
	mMountCount = 1;
	_54         = 1;

	mHeap = JKRHeap::findFromRoot(this);
	if (mHeap == nullptr) {
		mHeap = JKRHeap::getCurrentHeap();
	}

	mEntryNum = entryNumber;
	if (sCurrentVolume == NULL) {
		sCurrentVolume = this;
		sCurrentDirID  = 0;
	}
}

JKRArchive::JKRArchive(const char* p1, JKRArchive::EMountMode mountMode)
{
#ifdef TARGET_PC
	mHostFileEntries = nullptr;
#endif
	JUT_ASSERT_F(false, "UNIMPLEMENTED");
}

#ifdef TARGET_PC
JKRArchive::~JKRArchive() { hostFreeFileEntries(); }
#else
JKRArchive::~JKRArchive() { }
#endif

bool JKRArchive::isSameName(JKRArchive::CArcName& name, u32 nameOffset,
                            u16 nameHash) const
{
	u16 hash = name.getHash();
	if (hash != nameHash)
		return false;
	return strcmp(mStrTable + nameOffset, name.getString()) == 0;
}

JKRArchive::SDIDirEntry* JKRArchive::findResType(u32 type) const
{
	SDIDirEntry* node = mDirectories;
	u32 count         = 0;
	while (count < mArcInfoBlock->num_nodes) {
		if (node->mType == type) {
			return node;
		}

		node++;
		count++;
	}

	return nullptr;
}

JKRArchive::SDIDirEntry* JKRArchive::findDirectory(const char* name,
                                                   u32 directoryId) const
{
#ifdef TARGET_PC
	// Root's ".." entry is the sentinel 0xFFFFFFFF, not a directory index.
	if (directoryId >= mArcInfoBlock->num_nodes)
		return nullptr;
#endif
	if (name == nullptr) {
		return mDirectories + directoryId;
	}

	CArcName arcName(&name, '/');
	SDIDirEntry* dirEntry   = mDirectories + directoryId;
	SDIFileEntry* fileEntry = mFileEntries + dirEntry->mFirstIdx;

	for (int i = 0; i < dirEntry->mNum; i++) {
		if (isSameName(arcName, fileEntry->mFlagsAndNameOffset & 0xFFFFFF,
		               fileEntry->mHash)) {
			if (fileEntry->mFlagsAndNameOffset >> 24 & 0x02) {
				return findDirectory(name, fileEntry->mDataOffset);
			}
			break;
		}
		fileEntry++;
	}

	return nullptr;
}

JKRArchive::SDIFileEntry* JKRArchive::findTypeResource(u32 type, u32 id) const
{
	JUT_ASSERT_F(false, "UNIMPLEMENTED");
	return nullptr;
}

JKRArchive::SDIFileEntry* JKRArchive::findTypeResource(u32 type,
                                                       const char* name) const
{
	if (type) {
		CArcName arcName(name);
		SDIDirEntry* dirEntry = findResType(type);

		if (dirEntry) {
			SDIFileEntry* fileEntry = mFileEntries + dirEntry->mFirstIdx;
			for (int i = 0; i < dirEntry->mNum; fileEntry++, i++) {
				if (isSameName(arcName,
				               fileEntry->mFlagsAndNameOffset & 0xFFFFFF,
				               fileEntry->mHash)) {
					return fileEntry;
				}
			}
		}
	}

	return nullptr;
}

JKRArchive::SDIFileEntry* JKRArchive::findFsResource(const char* name,
                                                     u32 directoryId) const
{
#ifdef TARGET_PC
	// A relative lookup may ascend past the archive root; do not form an out-of-bounds host pointer.
	if (directoryId >= mArcInfoBlock->num_nodes)
		return nullptr;
#endif
	if (name) {
		CArcName arcName(&name, '/');
		SDIDirEntry* dirEntry   = mDirectories + directoryId;
		SDIFileEntry* fileEntry = mFileEntries + dirEntry->mFirstIdx;

		for (int i = 0; i < dirEntry->mNum; ++i) {
			if (isSameName(arcName, fileEntry->mFlagsAndNameOffset & 0xFFFFFF,
			               fileEntry->mHash)) {
				if (fileEntry->mFlagsAndNameOffset >> 24 & 0x02) {
					return findFsResource(name, fileEntry->mDataOffset);
				}

				if (name == nullptr) {
					return fileEntry;
				}

				return nullptr;
			}
			fileEntry++;
		}
	}

	return nullptr;
}

JKRArchive::SDIFileEntry* JKRArchive::findIdxResource(u32 fileIndex) const
{
	if (fileIndex < mArcInfoBlock->num_file_entries) {
		return mFileEntries + fileIndex;
	}

	return nullptr;
}

JKRArchive::SDIFileEntry* JKRArchive::findNameResource(const char* name) const
{
	SDIFileEntry* fileEntry = mFileEntries;

	CArcName arcName(name);
	for (int i = 0; i < mArcInfoBlock->num_file_entries; fileEntry++, i++) {
		if (isSameName(arcName, fileEntry->mFlagsAndNameOffset & 0xFFFFFF,
		               fileEntry->mHash)) {
			return fileEntry;
		}
	}

	return nullptr;
}

JKRArchive::SDIFileEntry*
JKRArchive::findPtrResource(const void* resource) const
{
	SDIFileEntry* fileEntry = mFileEntries;
	for (int i = 0; i < mArcInfoBlock->num_file_entries; fileEntry++, i++) {
		if (fileEntry->mData == resource) {
			return fileEntry;
		}
	}

	return nullptr;
}

JKRArchive::SDIFileEntry* JKRArchive::findIdResource(u16 id) const
{
	if (id != 0xFFFF) {
		SDIFileEntry* fileEntry;
		if (id < mArcInfoBlock->num_file_entries) {
			fileEntry = mFileEntries + id;
			if (fileEntry->mFileID == id
			    && fileEntry->mFlagsAndNameOffset >> 24 & 0x01) {
				return fileEntry;
			}
		}

		fileEntry = mFileEntries;
		for (int i = 0; i < mArcInfoBlock->num_file_entries; fileEntry++, i++) {
			if (fileEntry->mFileID == id
			    && fileEntry->mFlagsAndNameOffset >> 24 & 0x01) {
				return fileEntry;
			}
		}
	}

	return nullptr;
}

JKRArchive::CArcName::CArcName() { JUT_ASSERT_F(false, "UNIMPLEMENTED"); }

void JKRArchive::CArcName::store(const char* name)
{
	mHash      = 0;
	s32 length = 0;
	while (*name) {
		s32 ch = tolower(*name);
		mHash  = ch + mHash * 3;
		if (length < (s32)ARRAY_COUNT(mString) - 1) {
			mString[length++] = ch;
		}
		name++;
	}

	mLength         = (u16)length;
	mString[length] = 0;
}

const char* JKRArchive::CArcName::store(const char* name, char endChar)
{
	mHash      = 0;
	s32 length = 0;
	while (*name && *name != endChar) {
		s32 lch = tolower((int)*name);
		mHash   = lch + mHash * 3;
		if (length < (s32)ARRAY_COUNT(mString) - 1) {
			mString[length++] = lch;
		}
		name++;
	}

	mLength         = (u16)length;
	mString[length] = 0;

	if (*name == 0)
		return nullptr;
	return name + 1;
}

#ifdef TARGET_PC
static_assert(sizeof(JKRArchive::SArcHeader) == 0x20, "RARC header is 0x20 bytes on disc");
static_assert(sizeof(JKRArchive::SArcDataInfo) == 0x20, "RARC info block is 0x20 bytes on disc");
static_assert(sizeof(JKRArchive::SDIDirEntry) == 0x10, "RARC directory node is 0x10 bytes on disc");

// A file record as stored on disc: 0x14 bytes, no pointer. JKRArchive::SDIFileEntry adds void* mData (0x18 bytes).
struct SDIFileEntryDisc {
	u16 mFileID;
	u16 mHash;
	u32 mFlagsAndNameOffset;
	u32 mDataOffset;
	u32 mSize;
	u32 _10; // padding on disc, always zero
};
static_assert(sizeof(SDIFileEntryDisc) == 0x14, "RARC file record is 0x14 bytes on disc");

// Idempotency: an archive header reads 'RARC' (0x52415243) only after the swap; before it, the same four bytes
// read as 'CRAR' on the little-endian host. Re-entering with a swapped header (re-mount of the same buffer) or with
// a buffer that is not RARC is a no-op, and a freshly loaded buffer always carries the big-endian signature again.
void JKRArchive::hostSwapHeader(SArcHeader* header)
{
	if (header->signature != be32('RARC')) {
		return;
	}

	u32* words = reinterpret_cast<u32*>(header);
	for (int i = 0; i < 8; i++) {
		words[i] = be32(words[i]);
	}
}

// Swaps the info block, the directory nodes and the 0x14-byte file records in place, so the buffer is entirely in
// host byte order. The string table is plain bytes. The file records keep their 0x14-byte disc stride here;
// hostBuildFileEntries() copies them into the host-size SDIFileEntry array that the runtime actually uses.
// Idempotency: _1B[4] is padding that is zero on disc (checked on every .szs of the disc); 1 marks "swapped".
// A reload of the buffer from disc zeroes it again.
void JKRArchive::hostSwapInfoBlock(SArcDataInfo* info)
{
	if (info->_1B[4] != 0) {
		return;
	}

	swapInPlace32(&info->num_nodes);
	swapInPlace32(&info->node_offset);
	swapInPlace32(&info->num_file_entries);
	swapInPlace32(&info->file_entry_offset);
	swapInPlace32(&info->string_table_length);
	swapInPlace32(&info->string_table_offset);
	swapInPlace16(&info->nextFreeFileID);

	u8* base = reinterpret_cast<u8*>(&info->num_nodes);

	SDIDirEntry* dirs = reinterpret_cast<SDIDirEntry*>(base + info->node_offset);
	for (u32 i = 0; i < info->num_nodes; i++) {
		swapInPlace32(&dirs[i].mType);
		swapInPlace32(&dirs[i].mOffset);
		swapInPlace16(&dirs[i]._08);
		swapInPlace16(&dirs[i].mNum);
		swapInPlace32(&dirs[i].mFirstIdx);
	}

	SDIFileEntryDisc* files = reinterpret_cast<SDIFileEntryDisc*>(base + info->file_entry_offset);
	for (u32 i = 0; i < info->num_file_entries; i++) {
		swapInPlace16(&files[i].mFileID);
		swapInPlace16(&files[i].mHash);
		swapInPlace32(&files[i].mFlagsAndNameOffset);
		swapInPlace32(&files[i].mDataOffset);
		swapInPlace32(&files[i].mSize);
	}

	info->_1B[4] = 1;
}

// Call right after the original code set mFileEntries to the swapped on-disc records (stride 0x14). Replaces it with
// a calloc'ed array of host-size SDIFileEntry (mData == nullptr), owned by mHostFileEntries and freed in the
// destructor; every reader (find*, fetchResource, the destructors) then indexes it with the host stride.
void JKRArchive::hostBuildFileEntries()
{
	hostFreeFileEntries();

	u32 count = mArcInfoBlock->num_file_entries;
	const SDIFileEntryDisc* disc = reinterpret_cast<const SDIFileEntryDisc*>(mFileEntries);
	mHostFileEntries = static_cast<SDIFileEntry*>(calloc(count ? count : 1, sizeof(SDIFileEntry)));
	for (u32 i = 0; i < count; i++) {
		mHostFileEntries[i].mFileID            = disc[i].mFileID;
		mHostFileEntries[i].mHash              = disc[i].mHash;
		mHostFileEntries[i].mFlagsAndNameOffset = disc[i].mFlagsAndNameOffset;
		mHostFileEntries[i].mDataOffset        = disc[i].mDataOffset;
		mHostFileEntries[i].mSize              = disc[i].mSize;
	}
	mFileEntries = mHostFileEntries;
}

void JKRArchive::hostFreeFileEntries()
{
	if (mHostFileEntries) {
		if (mFileEntries == mHostFileEntries) {
			mFileEntries = nullptr;
		}
		free(mHostFileEntries);
		mHostFileEntries = nullptr;
	}
}
#endif

#ifdef TARGET_PC
// The name is mStrTable + (mFlagsAndNameOffset & 0xFFFFFF). swapResTIMG is idempotent by its marker byte, so calling
// this for a file whose buffer was already swapped (cached mData, a copy of an already swapped Mem archive file) is a
// no-op. Only the 0x20-byte header is converted; image and palette bytes stay as they are.
void JKRArchive::hostSwapFetchedFile(const SDIFileEntry* entry, void* data, u32 available) const
{
	if (!data || !mStrTable) {
		return;
	}

	u32 size = entry->mSize < available ? entry->mSize : available;
	if (size < sizeof(ResTIMG)) {
		return;
	}

	const char* name = mStrTable + (entry->mFlagsAndNameOffset & 0xFFFFFF);
	size_t len       = strlen(name);
	if (len < 4 || tolower((unsigned char)name[len - 4]) != '.' || tolower((unsigned char)name[len - 3]) != 'b'
	    || tolower((unsigned char)name[len - 2]) != 't' || tolower((unsigned char)name[len - 1]) != 'i') {
		return;
	}

	// A file still in its compressed form (Yaz0/Yay0) must not be touched; a ResTIMG starts with a texel format
	// byte (0..0xE), never 'Y'.
	if (*static_cast<u8*>(data) == 'Y') {
		return;
	}

	swapResTIMG(static_cast<ResTIMG*>(data));
}
#endif
