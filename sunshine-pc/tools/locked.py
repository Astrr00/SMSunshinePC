# Runs the given command while holding build/_build.lock (OS file lock, released when this process ends).
import msvcrt, os, subprocess, sys, time
f = open(os.path.join(os.path.dirname(__file__), "..", "build", "_build.lockfile"), "a")
while True:
    try:
        msvcrt.locking(f.fileno(), msvcrt.LK_NBLCK, 1)
        break
    except OSError:
        time.sleep(5)
sys.exit(subprocess.call(sys.argv[1:]))
