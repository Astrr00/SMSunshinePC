#ifndef JASTRACKPORT_HPP
#define JASTRACKPORT_HPP

#include <dolphin/types.h>

namespace JASystem {

class TTrackPort {
public:
	void init();
	void readWriteCheck(u32, u8);

#ifdef TARGET_PC
	// A port number outside 0-15 runs into the following TTrack members on the console; here it reads 0 and writes are
	// dropped, reported once on stderr (sequence operands and registers can carry any 8/16-bit value).
	static bool inRange(u32 port, const char* what);
	u16 readExport(u32 port)
	{
		if (!inRange(port, "reads export port"))
			return 0;
		mExportFlag[port] = 0;
		return mValue[port];
	}
	u16 readImport(u32 port)
	{
		if (!inRange(port, "reads import port"))
			return 0;
		mImportFlag[port] = 0;
		return mValue[port];
	}
	void writeExport(int port_num, u16 value)
	{
		if (!inRange((u32)port_num, "writes export port"))
			return;
		mExportFlag[port_num] = 1;
		mValue[port_num]      = value;
	}

	u16 get(u32 port) const { return inRange(port, "reads port") ? mValue[port] : 0; }
#else
	u16 readExport(u32 port)
	{
		mExportFlag[port] = 0;
		return mValue[port];
	}
	u16 readImport(u32 port)
	{
		mImportFlag[port] = 0;
		return mValue[port];
	}
	void writeExport(int port_num, u16 value)
	{
		mExportFlag[port_num] = 1;
		mValue[port_num]      = value;
	}

	u16 get(u32 port) const { return mValue[port]; }
#endif

	u8 checkImport(int i) const { return mImportFlag[i]; }
	u8 checkExport(int i) const { return mExportFlag[i]; }

public:
	/* 0x00 */ u8 mImportFlag[16];
	/* 0x10 */ u8 mExportFlag[16];
	/* 0x20 */ u16 mValue[16];
};

} // namespace JASystem

#endif // JASTRACKPORT_HPP
