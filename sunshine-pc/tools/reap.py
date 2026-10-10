"""Enforce the dev-run rules for sunshine.exe processes of this checkout.

  python tools/reap.py [minutes]          one pass: kill runs older than <minutes> (default 20)
  python tools/reap.py --loop [minutes]   repeat every 5 s (low priority), also kill any run whose window is on a
                                          visible part of the desktop or whose exe predates src/sdk/av.cpp (unmuted)

Every kill is printed and appended to build/reap.log.
"""
import os
import subprocess
import sys
import time

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
LOG = os.path.join(ROOT, "build", "reap.log")
NO_WINDOW = 0x08000000  # CREATE_NO_WINDOW: never flash a console on the user's desktop
MUTE_SRC = os.path.join(ROOT, "src", "sdk", "av.cpp")

PS = r"""
Add-Type -Name W -Namespace U -MemberDefinition '[DllImport("user32.dll")] public static extern bool GetWindowRect(System.IntPtr h, out RECT r); [DllImport("user32.dll")] public static extern int GetSystemMetrics(int i); public struct RECT { public int L,T,R,B; }'
$vx=[U.W]::GetSystemMetrics(76); $vy=[U.W]::GetSystemMetrics(77); $vw=[U.W]::GetSystemMetrics(78); $vh=[U.W]::GetSystemMetrics(79)
Get-Process sunshine -ErrorAction SilentlyContinue | ForEach-Object {
  $r = New-Object U.W+RECT; $vis = 0
  if ($_.MainWindowHandle -ne 0 -and [U.W]::GetWindowRect($_.MainWindowHandle, [ref]$r)) {
    if ($r.R -gt $vx -and $r.L -lt ($vx+$vw) -and $r.B -gt $vy -and $r.T -lt ($vy+$vh)) { $vis = 1 }
  }
  '{0}|{1}|{2}|{3}' -f $_.Id, $_.StartTime.ToFileTimeUtc(), $vis, $_.Path
}
"""


def kill(pid, why, path):
    subprocess.run(["taskkill", "/F", "/PID", pid], capture_output=True, creationflags=NO_WINDOW)
    line = f"{time.strftime('%H:%M:%S')} killed pid {pid}: {why} ({path})"
    print(line, flush=True)
    os.makedirs(os.path.dirname(LOG), exist_ok=True)
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(line + "\n")


def one_pass(limit_min, enforce):
    lock = os.path.join(ROOT, "build", "_build.lock")
    if os.path.isdir(lock) and time.time() - os.path.getmtime(lock) > 1800:
        os.rmdir(lock)
    out = subprocess.run(["powershell", "-NoProfile", "-NonInteractive", "-WindowStyle", "Hidden", "-Command", PS], capture_output=True, text=True, creationflags=NO_WINDOW).stdout
    now = time.time()
    mute_mtime = os.path.getmtime(MUTE_SRC) if os.path.exists(MUTE_SRC) else 0
    for line in out.splitlines():
        try:
            pid, ft, vis, path = line.split("|", 3)
        except ValueError:
            continue
        if not path.lower().startswith(ROOT.lower()):
            continue
        age = (now - (int(ft) / 1e7 - 11644473600)) / 60
        if age > limit_min:
            kill(pid, f"runs {age:.0f} min", path)
        elif enforce and vis == "1":
            kill(pid, "window on the visible desktop", path)
        elif enforce and os.path.exists(path) and os.path.getmtime(path) < mute_mtime:
            kill(pid, "exe older than the forced mute (src/sdk/av.cpp)", path)


def main():
    args = sys.argv[1:]
    loop = "--loop" in args
    args = [a for a in args if a != "--loop"]
    limit = float(args[0]) if args else (6.0 if loop else 20.0)
    if not loop:
        one_pass(limit, enforce=False)
        return
    while True:
        one_pass(limit, enforce=True)
        time.sleep(5)


if __name__ == "__main__":
    main()
