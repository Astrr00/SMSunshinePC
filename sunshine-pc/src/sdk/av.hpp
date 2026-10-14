// Internal interface of the AI/VI/DSP-mailbox port layer. Not a game header.
//
// av.cpp implements the decomp's SDK API and includes the decomp headers. av_win.cpp implements the host services below
// and must not: <windows.h> and the SDK headers define clashing names, so each file keeps to its own side.
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

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

// Output endpoint for AudioWrite: `id` from AudioOutputDevices(), empty = Windows default. The next AudioWrite reopens
// the device on it (after a running open has ended); an endpoint that is gone or inactive falls back to the default
// with one log line. Any thread.
void AudioSetDevice(const std::string& id);

// Launcher master volume, percent 0..100 (av.cpp). The AI pump scales every DAC block by it before the dump and the
// device: the block holds the DSP mix plus the THP movie audio that JASAiCtrl mixes in after the DSP, so both follow it.
// 100 passes the block through unchanged. Any thread.
void AudioSetVolumePct(int pct);

struct AudioDevice {
	std::string id;   // endpoint id (UTF-8); empty for the Windows default entry
	std::string name; // friendly name (UTF-8)
};
// Active render endpoints, Windows default first. Initializes COM on the calling thread.
std::vector<AudioDevice> AudioOutputDevices();

// Opt-in PCM dump (SUNSHINE_AUDIO_DUMP=<file.wav>, av_dump.cpp): every block the AI pump hands to AudioWrite.
void DumpWrite(const int16_t* stereo, uint32_t frames, double hz);

// --- for the future host DSP (av.cpp) ------------------------------------------------------------------------------

// Producer side of the DSP -> CPU mailbox behind DSPCheckMailFromDSP/DSPReadMailFromDSP. Pass the mail as the DSP
// wrote it: like the hardware, this sets bit 31 (mail valid), which DSPReadMailFromDSP clears again.
void DspPostMailFromDsp(uint32_t mail);

} // namespace av
