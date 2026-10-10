"""Kill sunshine.exe processes of this checkout that have run longer than N minutes (default 20), so no dev run can
linger as a zombie. Usage: python tools/reap.py [minutes]   (also reports what it killed)"""
import subprocess, sys, os, time
limit = float(sys.argv[1]) if len(sys.argv) > 1 else 20.0
root = os.path.abspath(os.path.join(os.path.dirname(__file__), "..")).lower()
ps = ("Get-Process sunshine -ErrorAction SilentlyContinue | ForEach-Object { '{0}|{1}|{2}' -f $_.Id, $_.StartTime.ToFileTimeUtc(), $_.Path }")
out = subprocess.run(["powershell", "-NoProfile", "-Command", ps], capture_output=True, text=True).stdout
now = time.time()
for line in out.splitlines():
    try:
        pid, ft, path = line.split("|", 2)
    except ValueError:
        continue
    started = int(ft) / 1e7 - 11644473600
    age = (now - started) / 60
    if path.lower().startswith(root) and age > limit:
        subprocess.run(["taskkill", "/F", "/PID", pid], capture_output=True)
        print(f"killed pid {pid} ({age:.0f} min, {path})")
