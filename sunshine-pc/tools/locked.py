# Runs the given command while holding build/_build.lock (OS file lock, released when this process ends).
import msvcrt, os, subprocess, sys, time
build_dir = os.path.join(os.path.dirname(__file__), "..", "build")
os.makedirs(build_dir, exist_ok=True)  # a fresh clone has no sunshine-pc/build yet
f = open(os.path.join(build_dir, "_build.lockfile"), "a")
while True:
    try:
        msvcrt.locking(f.fileno(), msvcrt.LK_NBLCK, 1)
        break
    except OSError:
        time.sleep(5)
sys.exit(subprocess.call(sys.argv[1:]))
