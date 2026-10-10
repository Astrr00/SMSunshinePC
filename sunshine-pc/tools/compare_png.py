"""Compare two PNG frames (golden vs candidate). Needs PIL + numpy.

Usage: python tools/compare_png.py golden.png candidate.png [--max-mad 2.0] [--min-ssim 0.98]

Prints mean absolute difference (0-255 scale, RGB) and a global SSIM-style score on luma.
Exit 0 when both thresholds pass, 1 when either fails, 2 on bad input (size mismatch, unreadable file).
"""
import argparse
import sys

import numpy as np
from PIL import Image


def load(path):
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.float64)


def luma(rgb):
    return rgb[..., 0] * 0.299 + rgb[..., 1] * 0.587 + rgb[..., 2] * 0.114


def ssim_global(a, b):
    # Single-window SSIM over the whole image (no sliding window): cheap and stable enough for golden frames.
    c1 = (0.01 * 255) ** 2
    c2 = (0.03 * 255) ** 2
    mu_a, mu_b = a.mean(), b.mean()
    var_a, var_b = a.var(), b.var()
    cov = ((a - mu_a) * (b - mu_b)).mean()
    return ((2 * mu_a * mu_b + c1) * (2 * cov + c2)) / ((mu_a ** 2 + mu_b ** 2 + c1) * (var_a + var_b + c2))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("golden")
    ap.add_argument("candidate")
    ap.add_argument("--max-mad", type=float, default=2.0, help="max mean absolute difference per channel (0-255)")
    ap.add_argument("--min-ssim", type=float, default=0.98, help="min SSIM on luma (1.0 = identical)")
    args = ap.parse_args()

    try:
        a = load(args.golden)
        b = load(args.candidate)
    except OSError as e:
        print(f"ERROR: {e}")
        return 2
    if a.shape != b.shape:
        print(f"ERROR: size mismatch golden {a.shape[1]}x{a.shape[0]} vs candidate {b.shape[1]}x{b.shape[0]}")
        return 2

    mad = float(np.abs(a - b).mean())
    ssim = float(ssim_global(luma(a), luma(b)))
    ok_mad = mad <= args.max_mad
    ok_ssim = ssim >= args.min_ssim
    print(f"mean abs diff: {mad:.3f} (max {args.max_mad}) {'ok' if ok_mad else 'FAIL'}")
    print(f"ssim (luma):   {ssim:.4f} (min {args.min_ssim}) {'ok' if ok_ssim else 'FAIL'}")
    print("PASS" if ok_mad and ok_ssim else "FAIL")
    return 0 if ok_mad and ok_ssim else 1


if __name__ == "__main__":
    sys.exit(main())
