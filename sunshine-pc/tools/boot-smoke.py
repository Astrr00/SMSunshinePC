"""Start sunshine.exe from a build directory with the local disc image and print how far it got.

Usage: python tools/boot-smoke.py [build dir name, default windows-clang-relwithdebinfo] [seconds, default 30]
The exe's crash handler prints a symbolized stack to stderr on a fault. Output is the last 4000 characters.
"""
import os
import subprocess
import sys
import time

here = os.path.dirname(os.path.abspath(__file__))
root = os.path.abspath(os.path.join(here, "..", ".."))
name = sys.argv[1] if len(sys.argv) > 1 else "windows-clang-relwithdebinfo"
limit = float(sys.argv[2]) if len(sys.argv) > 2 else 30.0
bld = os.path.join(root, "sunshine-pc", "build", name)
disc = os.path.join(root, "sms", "orig", "GMSJ01", "Super Mario Sunshine (Japan).rvz")

p = subprocess.Popen([os.path.join(bld, "sunshine.exe"), "--dvd", disc], cwd=bld, stdout=subprocess.PIPE,
                     stderr=subprocess.STDOUT, text=True, errors="replace")
t0 = time.time()
while time.time() - t0 < limit and p.poll() is None:
    time.sleep(0.25)
alive = p.poll() is None
print(f"alive after {time.time() - t0:.1f}s: {alive}  returncode: "
      f"{None if p.returncode is None else hex(p.returncode & 0xFFFFFFFF)}")
if alive:
    p.terminate()
try:
    out, _ = p.communicate(timeout=10)
except Exception:
    p.kill()
    out, _ = p.communicate()
print(out[-4000:])
