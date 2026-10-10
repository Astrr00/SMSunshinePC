"""Per-segment audio stats for a 16-bit PCM WAV dump: RMS (dBFS), spectral flatness, full-scale clip count.

usage: python audio_stats.py <file.wav> [segment_seconds=10] [max_seconds]

Spectral flatness = geometric mean / arithmetic mean of the power spectrum of the mono mix (0..1). Music with
harmonic content is low (roughly < 0.1); white-ish noise is near 1. Clip count = samples at +32767 or -32768.
"""
import sys
import wave

import numpy as np


def flatness(x):
    win = np.hanning(len(x))
    spec = np.abs(np.fft.rfft(x * win)) ** 2 + 1e-12
    return float(np.exp(np.mean(np.log(spec))) / np.mean(spec))


def main():
    path = sys.argv[1]
    seg = float(sys.argv[2]) if len(sys.argv) > 2 else 10.0
    max_s = float(sys.argv[3]) if len(sys.argv) > 3 else None
    with wave.open(path) as w:
        rate, ch, width = w.getframerate(), w.getnchannels(), w.getsampwidth()
        raw = w.readframes(w.getnframes())
    assert width == 2, "16-bit only"
    pcm = np.frombuffer(raw, dtype="<i2").reshape(-1, ch).astype(np.float64)
    seg_n = int(seg * rate)
    total = pcm.shape[0] if max_s is None else min(pcm.shape[0], int(max_s * rate))
    print(f"{path}: {rate} Hz, {ch} ch, {total / rate:.1f} s analysed, segment {seg:g} s")
    print(f"{'t0':>6} {'t1':>6} {'rms_dBFS':>9} {'flat':>7} {'clip':>6} {'zc/s':>7}")
    for start in range(0, total, seg_n):
        block = pcm[start:min(start + seg_n, total)]
        if len(block) < 64:
            break
        mono = block.mean(axis=1)
        rms = np.sqrt(np.mean(block ** 2)) / 32768.0
        db = 20 * np.log10(rms) if rms > 0 else float("-inf")
        clip = int(np.sum((block >= 32767) | (block <= -32768)))
        zc = int(np.sum(np.signbit(mono[1:]) != np.signbit(mono[:-1])))
        zcs = zc / (len(block) / rate)
        t0 = start / rate
        print(f"{t0:6.1f} {t0 + len(block) / rate:6.1f} {db:9.1f} {flatness(mono):7.3f} {clip:6d} {zcs:7.0f}")


if __name__ == "__main__":
    main()
