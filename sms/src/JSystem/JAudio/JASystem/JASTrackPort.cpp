#include <JSystem/JAudio/JASystem/JASTrackPort.hpp>
#ifdef TARGET_PC
#include <stdio.h>
#endif

namespace JASystem {

#ifdef TARGET_PC
bool TTrackPort::inRange(u32 port, const char* what)
{
	if (port < 16)
		return true;
	static bool reported = false;
	if (!reported) {
		reported = true;
		fprintf(stderr, "JASystem: sequence %s %u (only 16 exist); ignored. Reported once.\n", what, port);
	}
	return false;
}
#endif

void TTrackPort::init()
{
	for (int i = 0; i != 16; i++) {
		mImportFlag[i] = 0;
		mExportFlag[i] = 0;
		mValue[i]      = 0;
	}
}

void TTrackPort::readWriteCheck(u32 param, u8 param2) { }

} // namespace JASystem
