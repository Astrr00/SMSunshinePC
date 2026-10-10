"""Run sunshine.exe from a build dir for N seconds with stdout+stderr going to <build dir>/run.log (full log, unlike
boot-smoke.py which prints only the last 4000 characters). Usage: python tools/run-log.py [build dir name] [seconds]"""
import os, subprocess, sys, time
root = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
name = sys.argv[1] if len(sys.argv) > 1 else "windows-clang-relwithdebinfo"
limit = float(sys.argv[2]) if len(sys.argv) > 2 else 60.0
bld = os.path.join(root, "sunshine-pc", "build", name)
disc = os.path.join(root, "sms", "orig", "GMSJ01", "Super Mario Sunshine (Japan).rvz")
with open(os.path.join(bld, "run.log"), "w", errors="replace") as log:
    p = subprocess.Popen([os.path.join(bld, "sunshine.exe"), "--dvd", disc], cwd=bld, stdout=log, stderr=subprocess.STDOUT)
    t0 = time.time()
    while time.time() - t0 < limit and p.poll() is None:
        time.sleep(0.25)
    alive = p.poll() is None
    if alive:
        p.terminate()
    p.wait()
print(f"alive after {time.time() - t0:.1f}s: {alive}  returncode: {None if alive else hex(p.returncode & 0xFFFFFFFF)}")
