// Internal interface of the AI/VI/DSP-mailbox port layer. Not a game header.
//
// av.cpp implements the decomp's SDK API and includes the decomp headers. av_win.cpp implements the host services below
// and must not: <windows.h> and the SDK headers define clashing names, so each file keeps to its own side.
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>

namespace av {

// --- host services (av_win.cpp) ------------------------------------------------------------------------------------

// Blocks the calling thread until `t`, using a high-resolution timer (the default Windows tick is ~15 ms).
void SleepUntil(std::chrono::steady_clock::time_point t);

// Raises the priority of the calling thread for the audio pump / retrace thread.
void BoostThreadPriority();

// True when [p, p + bytes) is committed, readable memory of this process.
bool IsReadable(const void* p, size_t bytes);

// Queues `frames` stereo frames of interleaved host-endian s16 on the default playback device, which is opened (and
// reopened when `hz` changes) on demand. Call from ONE thread only. Without a usable device the data is dropped; the
// next call retries the device after one second.
void AudioWrite(const int16_t* stereo, uint32_t frames, double hz);

// --- for the future host DSP (av.cpp) ------------------------------------------------------------------------------

// Producer side of the DSP -> CPU mailbox behind DSPCheckMailFromDSP/DSPReadMailFromDSP. Pass the mail as the DSP
// wrote it: like the hardware, this sets bit 31 (mail valid), which DSPReadMailFromDSP clears again.
void DspPostMailFromDsp(uint32_t mail);

} // namespace av
