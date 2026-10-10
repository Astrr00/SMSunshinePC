#include <System/TimeRec.hpp>
#include <System/DrawSyncManager.hpp>
#include <dolphin/gx/GXManage.h>
#include <dolphin/os.h>
#include <types.h>

TTimeRec* TTimeRec::_instance;

bool TTimeArray::append(u32 time, u32 color)
{
	bool result = false;
	if (mSize < MAX_SIZE) {
		Entry& entry = mEntries[mSize++];
		entry.time   = time;
		entry.color  = color;
		result       = true;
	}
	return result;
}

TTimeRec* TTimeRec::start(u16 param_1)
{
	if (_instance == nullptr)
		_instance = new TTimeRec(param_1);
	return _instance;
}

void TTimeRec::end() { }

void TTimeRec::drawSyncCallbackSt(u16) { }

TTimeRec::TTimeRec(u16 param_1)
    : unk814(0)
    , unk818(0)
    , unk81A(param_1)
    , unk81C(0)
{
}

void TTimeRec::flip()
{
	suppleGXTime();
	unk814 ^= 1;
	crTimeAry()[0].clear();
	crTimeAry()[1].clear();
}

void TTimeRec::snapGXTime(u32 param_1)
{
	if (crTimeAry()[1].append(0, param_1)) {
		if ((unk81C & 1) == 0) {
			u16 token = unk81A + crTimeAry()[1].size() - 1;
			if (TDrawSyncManager::smInstance)
				TDrawSyncManager::smInstance->pushBreakPoint();
			GXSetDrawSync(token);
		}
	}
}

void TTimeRec::suppleGXTime()
{
	TTimeArray& array = _instance->crTimeAry()[1];
	if (array.size() < 3)
		return;
	u32 curr = array[array.size() - 1].time;
	int i    = array.size() - 1;
	while (i > 0) {
		--i;
		if (array[i].time == 0)
			array[i].time = curr;
		else
			curr = array[i].time;
	}
}

void TTimeRec::drawSyncCallback(u16 param_1)
{
	unk4[unk814][1][param_1 - unk81A].time = OSGetTick();
}
