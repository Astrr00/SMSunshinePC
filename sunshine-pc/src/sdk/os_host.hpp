// Host-call bracket of the OS layer (os_thread.cpp): see sunshineHostWaitBegin.
#pragma once

extern "C" void sunshineHostWaitBegin(void);
extern "C" void sunshineHostWaitEnd(void);

struct HostWait {
	HostWait() { sunshineHostWaitBegin(); }
	~HostWait() { sunshineHostWaitEnd(); }
};
