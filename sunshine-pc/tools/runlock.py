"""Exclusive game-run lock for port agents (one sunshine.exe session at a time).

  python tools/runlock.py acquire <agent> [pid]  waits up to 10 min, prints LOCKED
  python tools/runlock.py touch <agent>          heartbeat (call every <5 min during a long run)
  python tools/runlock.py release <agent>        only the owner can release (else prints NOT_OWNER)
  python tools/runlock.py status

Stale = owner heartbeat older than 15 min. Every event is appended to build/runlock.log
(who acquired/released/stole, with the optional sunshine.exe PID) so kills can be attributed.
Never kill sunshine.exe by image name; kill only the PID you launched.
"""
import os, sys, time

B = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build")
D = os.path.join(B, "runlock")
OWN = os.path.join(D, "owner")
LOG = os.path.join(B, "runlock.log")


def log(msg):
    os.makedirs(B, exist_ok=True)
    open(LOG, "a").write(time.strftime("%H:%M:%S ") + msg + "\n")


def owner():
    try:
        s = open(OWN).read().split()
        return s[0], float(s[1]) if len(s) > 1 else 0.0
    except (OSError, ValueError, IndexError):
        return None, 0.0


def alive(pid):
    """True while the process is running (Windows: OpenProcess + exit code STILL_ACTIVE; elsewhere: signal 0)."""
    try:
        if os.name == "nt":
            import ctypes
            from ctypes import wintypes
            k = ctypes.WinDLL("kernel32", use_last_error=True)
            k.OpenProcess.restype = wintypes.HANDLE
            k.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
            k.GetExitCodeProcess.argtypes = (wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD))
            k.CloseHandle.argtypes = (wintypes.HANDLE,)
            h = k.OpenProcess(0x1000, False, int(pid))  # PROCESS_QUERY_LIMITED_INFORMATION
            if not h:
                return False
            code = wintypes.DWORD()
            ok = k.GetExitCodeProcess(h, ctypes.byref(code))
            k.CloseHandle(h)
            # A terminated process whose handle someone still holds opens fine but has a real exit code.
            return bool(ok) and code.value == 259  # STILL_ACTIVE
        os.kill(int(pid), 0)
        return True
    except (OSError, ValueError):
        return False


def owner_pid():
    try:
        s = open(OWN).read().split()
        return s[2] if len(s) > 2 else ""
    except OSError:
        return ""


def acquire(agent, pid=""):
    # route.py runs as "lead" and calls us as a child: its PID is our parent. If it is killed (job deadline), the lock
    # is stale at once instead of after the 15 min heartbeat gap. Other agents pass their own PID or none (age rule).
    if not pid and agent == "lead":
        pid = str(os.getppid())
    end = time.time() + 600
    while time.time() < end:
        try:
            os.makedirs(B, exist_ok=True)
            os.mkdir(D)
            open(OWN, "w").write(f"{agent} {time.time()} {pid}")
            log(f"acquire {agent} pid={pid}")
            print("LOCKED")
            return
        except FileExistsError:
            o, _ = owner()
            try:
                age = time.time() - (os.path.getmtime(OWN) if os.path.exists(OWN) else os.path.getmtime(D))
            except OSError:
                age = 0
            opid = owner_pid()
            if age > 900 or (opid.isdigit() and not alive(opid)):
                log(f"steal by {agent} from {o} (stale {age:.0f}s, owner pid {opid or '-'})")
                for f in (OWN,):
                    try:
                        os.remove(f)
                    except OSError:
                        pass
                try:
                    os.rmdir(D)
                except OSError:
                    pass
                continue
            time.sleep(10)
    log(f"timeout {agent}")
    print("TIMEOUT")
    sys.exit(1)


def touch(agent):
    o, t = owner()
    if o != agent:
        print("NOT_OWNER")
        sys.exit(1)
    s = open(OWN).read().split()
    open(OWN, "w").write(f"{agent} {t} {s[2] if len(s) > 2 else ''}")
    print("ok")


def release(agent):
    o, _ = owner()
    if o is not None and o != agent:
        log(f"release refused {agent} (owner {o})")
        print("NOT_OWNER")
        sys.exit(1)
    for f in (OWN,):
        try:
            os.remove(f)
        except OSError:
            pass
    try:
        os.rmdir(D)
    except OSError:
        pass
    log(f"release {agent}")
    print("released")


if __name__ == "__main__":
    a = sys.argv[1:]
    if a[0] == "acquire":
        acquire(a[1], a[2] if len(a) > 2 else "")
    elif a[0] == "touch":
        touch(a[1])
    elif a[0] == "release":
        release(a[1])
    else:
        print(open(OWN).read() if os.path.exists(OWN) else "free")
