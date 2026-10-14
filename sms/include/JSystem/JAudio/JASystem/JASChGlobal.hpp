#ifndef JASCHGLOBAL_HPP
#define JASCHGLOBAL_HPP

#include <JSystem/JAudio/JASystem/JASChannel.hpp>
#include <JSystem/JAudio/JASystem/JASChannelMgr.hpp>
#include <dolphin/types.h>

namespace JASystem {

namespace ChGlobal {

	void init();
	int release(TChannel* channel);
	int releaseAll(TChannelMgr* channelMgr);
	u32 alloc(TChannelMgr* channelMgr, u32 param);
	TChannel* getChannelHandle(u32 param);
	u32 getGlobalHandle();
#ifdef TARGET_PC
	// Dev diagnostic (SUNSHINE_DEV_AUDIO_STATS, JASDSPBuf.cpp): counts[0-4] = logical channels the global pool holds in
	// its lists 0-3 / in no list of its own, counts[5-9] = the same for channels lent to another manager.
	// Only addresses are compared (no manager is read), so a stale manager pointer can only misfile a channel.
	void countChannelsPC(u32 counts[10]);
#endif

} // namespace ChGlobal

} // namespace JASystem

#endif // JASCHGLOBAL_HPP
