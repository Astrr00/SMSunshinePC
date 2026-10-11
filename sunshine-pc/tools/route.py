"""Run one scripted, invisible, silent game session and wait for it (dev-only, never touches %APPDATA%\\Sunshine).

  python tools/route.py <tree> <script> [--name N] [--seconds S] [KEY=VAL ...]

  <tree>     build tree under build/ that holds sunshine.exe (e.g. Strict)
  <script>   devscript file (SUNSHINE_DEV_SCRIPT); relative to the repo's sunshine-pc folder or absolute
  --name     run name; output goes to build/Route/<name>/ (run.log, appdata/, PNGs written by `shot` verbs)
  --seconds  wall-clock limit, default 330 (< 6 min: tools/reap.py kills older runs)
  KEY=VAL    extra environment variables (SUNSHINE_DEV_MAX_SECONDS is set to the limit)

Rules kept here: CREATE_NO_WINDOW + BELOW_NORMAL priority, window parked off screen, muted, temporary APPDATA
(settings and card saves live in build/Route/<name>/appdata), serialised through tools/runlock.py, only the own PID is
ever killed. Exit code: the game's own exit code, 124 when the wall-clock limit ended the run.
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
NO_WINDOW = 0x08000000
BELOW_NORMAL = 0x00004000


def lock(cmd, agent="lead", pid=""):
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "runlock.py"), cmd, agent] + ([pid] if pid else []),
                       capture_output=True, text=True, creationflags=NO_WINDOW)
    return r.stdout.strip()


def main():
    args = sys.argv[1:]
    name = None
    seconds = 330
    extra = {}
    pos = []
    i = 0
    while i < len(args):
        a = args[i]
        if a == "--name":
            name = args[i + 1]
            i += 2
            continue
        if a == "--seconds":
            seconds = int(args[i + 1])
            i += 2
            continue
        if "=" in a and not os.path.exists(a):
            k, v = a.split("=", 1)
            extra[k] = v
        else:
            pos.append(a)
        i += 1
    if len(pos) != 2:
        print(__doc__)
        return 2
    tree, script = pos
    script = os.path.abspath(script if os.path.isabs(script) else os.path.join(ROOT, script))
    name = name or os.path.splitext(os.path.basename(script))[0]
    exe = os.path.join(ROOT, "build", tree, "sunshine.exe")
    disc = os.path.abspath(os.path.join(ROOT, "..", "sms", "orig", "GMSJ01", "Super Mario Sunshine (Japan).rvz"))
    out = os.path.join(ROOT, "build", "Route", name)
    os.makedirs(os.path.join(out, "appdata"), exist_ok=True)
    env = dict(os.environ)
    # Keep tooling of the surrounding agent session (Headroom proxy: HEADROOM_*, its onnxruntime in ORT_DYLIB_PATH) out of the game run.
    for key in [k for k in env if k.startswith("HEADROOM_") or k == "ORT_DYLIB_PATH"]:
        del env[key]
    env.update(
        APPDATA=os.path.join(out, "appdata"),
        SUNSHINE_WINDOW_POS="30000,30000",
        SDL_WINDOW_ACTIVATE_WHEN_SHOWN="0",
        SUNSHINE_WINDOW_SIZE="640,480",
        SUNSHINE_MUTE="1",
        SUNSHINE_DEV_SCRIPT=script,
        SUNSHINE_DEV_MAX_SECONDS=str(seconds),
        AURORA_SCREENSHOT_EVERY="60:" + os.path.join(out, "live.png"),
    )
    env.setdefault("SUNSHINE_DEV_STATUS", "1")
    env.setdefault("SUNSHINE_ALLOW_CPU_ADAPTER", "1")  # GPU-less host: Null backend renders nothing, PNGs would be blank
    # WARP is fill-rate bound: 0.25 keeps dolpic0 at ~30 logic ticks/s (full size: 14-17/s, measured m4_s025 vs m1_inbox).
    env.setdefault("SUNSHINE_RENDER_SCALE", "0.25")
    env.setdefault("SUNSHINE_DEV_EXIT_ON_END", "1")  # the game ends right after the script's `end` (keep a `wait` after the last `shot`)
    env.update(extra)
    print(f"[route] render scale {env['SUNSHINE_RENDER_SCALE']} (SUNSHINE_RENDER_SCALE=1 for a sharp frame), cpu adapter {env['SUNSHINE_ALLOW_CPU_ADAPTER']}")
    # ROUTE_NOLOCK=1: skip the run lock for functional diagnosis runs in parallel (6 cores). Never for timing measurements.
    use_lock = os.environ.get("ROUTE_NOLOCK", "") in ("", "0")
    if use_lock and lock("acquire") != "LOCKED":
        print("run lock not acquired")
        return 1
    rc = 124
    p = None
    try:
        logf = open(os.path.join(out, "run.log"), "w")
        p = subprocess.Popen([exe, "--dvd", disc], cwd=out, stdout=logf, stderr=subprocess.STDOUT, env=env,
                             creationflags=NO_WINDOW | BELOW_NORMAL)
        lock("touch", pid=str(p.pid))
        open(os.path.join(out, "pid"), "w").write(str(p.pid))
        t0 = time.time()
        beat = t0
        while p.poll() is None and time.time() - t0 < seconds:
            time.sleep(1)
            if time.time() - beat > 120:
                lock("touch")
                beat = time.time()
        if p.poll() is None:
            subprocess.run(["taskkill", "/F", "/PID", str(p.pid)], capture_output=True, creationflags=NO_WINDOW)
            p.wait()
            rc = 124
        else:
            rc = p.returncode
        print(f"run '{name}' ended rc={rc} after {time.time() - t0:.0f}s, log {os.path.join(out, 'run.log')}")
    finally:
        if use_lock:
            lock("release")
    return rc


if __name__ == "__main__":
    sys.exit(main())
