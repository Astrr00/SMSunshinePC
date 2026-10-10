#include <JSystem/JUtility/JUTNameTab.hpp>
#include <JSystem/JUtility/JUTAssert.hpp>
#include <string.h>

#ifdef TARGET_PC
#include <hostendian.h>

// ResNTAB tables live inside the file buffers of the J3D formats (BMD/BMT name tables, BCK/BTK/BRK/BPK/BTP update
// tables) and are handed out here by pointer, possibly several times for the same table (shared buffers, re-loads).
// Every field is u16, so the table keeps its layout and is converted in place; the strings are plain bytes.
// Idempotency marker: mPad0. It is 0xFFFF in all 37289 name tables of the disc (JNT1, MAT3, TEX1, TTK1, TRK1, TPT1,
// PAK1), identical in both byte orders; the swapped table holds RESNTAB_HOST_MARK there instead.
#define RESNTAB_HOST_MARK 0x4E48

static void swapResNTAB(ResNTAB* tab)
{
	if (tab->mPad0 == RESNTAB_HOST_MARK) {
		return;
	}
	tab->mEntryNum = be16(tab->mEntryNum);
	for (u16 i = 0; i < tab->mEntryNum; i++) {
		tab->mEntries[i].mKeyCode = be16(tab->mEntries[i].mKeyCode);
		tab->mEntries[i].mOffs    = be16(tab->mEntries[i].mOffs);
	}
	tab->mPad0 = RESNTAB_HOST_MARK;
}
#endif

void JUTNameTab::setResource(const ResNTAB* pNameTable)
{
#ifdef TARGET_PC
	if (pNameTable == nullptr) {
		// the console reads address 0 here, the host would fault: an empty table
		mNameTable = nullptr;
		mStrData   = nullptr;
		mNameNum   = 0;
		return;
	}
	swapResNTAB(const_cast<ResNTAB*>(pNameTable));
#endif
	mNameTable = pNameTable;
	mStrData   = (const char*)(pNameTable->mEntries + pNameTable->mEntryNum);
	mNameNum   = pNameTable->mEntryNum;
}

JUTNameTab::JUTNameTab() { setResource(nullptr); }

JUTNameTab::JUTNameTab(const ResNTAB* pNameTable) { setResource(pNameTable); }

s32 JUTNameTab::getIndex(const char* pName) const
{
	JUT_ASSERT(mNameTable != NULL);

	const ResNTAB::Entry* pEntry = mNameTable->mEntries;
	u16 keyCode                  = calcKeyCode(pName);

	for (u16 i = 0; i < mNameNum; i++) {
		if (pEntry->mKeyCode == keyCode
		    && !strcmp(mNameTable->mEntries[i].mOffs + (char*)mNameTable,
		               pName))
			return i;

		pEntry++;
	}

	return -1;
}

const char* JUTNameTab::getName(u16 index) const
{
	if (index < mNameNum)
		return mNameTable->getName(index);
	return nullptr;
}

u16 JUTNameTab::calcKeyCode(const char* pName) const
{
	u32 keyCode = 0;
	while (*pName)
		keyCode = (keyCode * 3) + *pName++;
	return keyCode;
}
