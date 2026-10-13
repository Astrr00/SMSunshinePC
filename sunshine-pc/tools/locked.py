# Runs the given command while holding build/_build.lock (OS file lock, released when this process ends).
# Reentrant: a command started under this lock (for example build-alt.cmd, which calls locked.py itself) inherits
# SUNSHINE_BUILD_LOCK_HELD and does not wait for the lock its own parent holds (that deadlocked on 2026-10-13).
import msvcrt, os, subprocess, sys, time
env = dict(os.environ)
if not env.get("SUNSHINE_BUILD_LOCK_HELD"):
    build_dir = os.path.join(os.path.dirname(__file__), "..", "build")
    os.makedirs(build_dir, exist_ok=True)  # a fresh clone has no sunshine-pc/build yet
    f = open(os.path.join(build_dir, "_build.lockfile"), "a")
    while True:
        try:
            msvcrt.locking(f.fileno(), msvcrt.LK_NBLCK, 1)
            break
        except OSError:
            time.sleep(5)
    env["SUNSHINE_BUILD_LOCK_HELD"] = "1"
sys.exit(subprocess.call(sys.argv[1:], env=env))
