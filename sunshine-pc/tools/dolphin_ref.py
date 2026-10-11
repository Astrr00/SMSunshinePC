#!/usr/bin/env python3
"""Headless Dolphin reference for Super Mario Sunshine GMSJ01 (JP RVZ).

Boots the RVZ in DolphinNoGUI (-p headless, Null video, no audio, temp user dir,
hidden + below-normal process), pauses it through Dolphin's built-in GDB stub
(Main.General.GDBPort) and reads guest RAM over the RSP protocol.

  python dolphin_ref.py --frames 600 --mario --symbol gpMarDirector
  python dolphin_ref.py --seconds 5 --dump mem1.bin
  python dolphin_ref.py --selftest

--frames N   run until VI retraceCount >= N (absolute count since game boot, ~60/s)
--seconds S  run S wall-clock seconds (used when --frames is not given)
--symbol X   print u32 at symbol X (repeatable)
--mario      print gpMarioOriginal->mPosition (x, y, z) or NULL
--dump F     write MEM1 (0x80000000, 24 MiB) to F
"""
import argparse
import os
import re
import shutil
import socket
import struct
import subprocess
import sys
import tempfile
import time
from pathlib import Path

CREATE_NO_WINDOW = 0x08000000
BELOW_NORMAL_PRIORITY = 0x4000

ROOT = Path(__file__).resolve().parents[2]  # ".../Sunshine Decomp"
DOLPHIN = Path(os.environ.get("DOLPHIN_NOGUI", "DolphinNoGUI.exe"))  # blocked for GMSJ01, see platform-notes.md
RVZ = ROOT / "sms/orig/GMSJ01/Super Mario Sunshine (Japan).rvz"
SYMBOLS = ROOT / "sms/config/GMSJ01/symbols.txt"

MEM1_BASE = 0x80000000
MEM1_SIZE = 0x01800000  # 24 MiB
GDB_CHUNK = 4096  # Dolphin GDB_BFR_MAX=10000 caps 'm' replies at ~4998 bytes
RETRACE_SYMBOL = "retraceCount"  # VI retrace counter (frame clock), local in VI.c
MARIO_PTR_SYMBOL = "gpMarioOriginal"
# TMario : TTakeActor : THitActor : JDrama::TActor : TPlacement; TPlacement::mPosition
# is at /* 0x10 */ (sms/include/JSystem/JDrama/JDRPlacement.hpp:29), all bases at offset 0.
MARIO_POS_OFFSET = 0x10

SYM_RE = re.compile(r"^(\S+) = \.(\w+):0x([0-9A-Fa-f]+); // type:(\w+) size:0x([0-9A-Fa-f]+)")


def be32(b):
    return struct.unpack(">I", b)[0]


def parse_symbol_line(line):
    m = SYM_RE.match(line)
    return None if m is None else (m.group(1), int(m.group(3), 16), int(m.group(5), 16))


def load_symbols(path):
    table = {}
    for line in Path(path).read_text(encoding="utf-8", errors="replace").splitlines():
        sym = parse_symbol_line(line)
        if sym:
            table.setdefault(sym[0], (sym[1], sym[2]))  # (addr, size)
    return table


def rsp_frame(body):
    data = body.encode()
    return b"$" + data + b"#" + format(sum(data) & 0xFF, "02x").encode()


class Gdb:
    """Minimal GDB RSP client over Dolphin's stub (ack mode)."""

    def __init__(self, sock):
        self.sock = sock
        self.buf = b""

    def _packet(self):
        while True:
            s = self.buf.find(b"$")
            if s >= 0:
                h = self.buf.find(b"#", s)
                if h >= 0 and len(self.buf) >= h + 3:
                    data = self.buf[s + 1:h].decode("ascii", "replace")
                    self.buf = self.buf[h + 3:]
                    self.sock.sendall(b"+")
                    return data
            chunk = self.sock.recv(65536)
            if not chunk:
                raise ConnectionError("GDB stub closed the connection")
            self.buf += chunk

    def cmd(self, body):
        self.sock.sendall(rsp_frame(body))
        return self._packet()

    def interrupt(self):  # halt the CPU; Dolphin answers with a stop reply
        self.sock.sendall(b"\x03")
        return self._packet()

    def resume(self):  # no reply until the next stop
        self.sock.sendall(rsp_frame("c"))

    def read(self, addr, n):
        out = bytearray()
        while n > 0:
            step = min(n, GDB_CHUNK)
            reply = self.cmd(f"m{addr:x},{step:x}")
            if not reply or reply[0] == "E":
                raise RuntimeError(f"read {addr:#010x}+{step:#x} failed: {reply!r}")
            out += bytes.fromhex(reply)
            addr += step
            n -= step
        return bytes(out)


def free_port():
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def connect(proc, port, timeout):
    deadline = time.monotonic() + timeout
    while True:
        if proc.poll() is not None:
            raise RuntimeError(f"DolphinNoGUI exited early, rc={proc.returncode}")
        try:
            return Gdb(socket.create_connection(("127.0.0.1", port), timeout=10))
        except OSError:
            if time.monotonic() > deadline:
                raise TimeoutError(f"GDB stub on port {port} did not come up in {timeout}s")
            time.sleep(0.2)


def selftest():
    assert be32(b"\x80\x40\xa3\x78") == 0x8040A378
    assert parse_symbol_line(
        "gpMarioOriginal = .sbss:0x8040A378; // type:object size:0x4 scope:global align:4 data:4byte"
    ) == ("gpMarioOriginal", 0x8040A378, 4)
    assert rsp_frame("c") == b"$c#63"  # 'c' = 0x63
    assert rsp_frame("m80000000,4") == b"$m80000000,4#" + format(
        sum(b"m80000000,4") & 0xFF, "02x").encode()
    syms = load_symbols(SYMBOLS)
    assert syms[MARIO_PTR_SYMBOL][0] == 0x8040A378
    assert syms[RETRACE_SYMBOL][0] == 0x80409F00
    print("selftest ok")


def run(args):
    exe = Path(args.dolphin)
    image = Path(args.image)
    syms = load_symbols(args.symbols)
    user = Path(tempfile.mkdtemp(prefix="dolphin_ref_"))
    (user / "Logs").mkdir()
    (user / "Config").mkdir()
    # Panic alerts are blocking MessageBoxes on the emu thread; keep them off (log only).
    (user / "Config" / "Dolphin.ini").write_text(
        "[Interface]\nUsePanicHandlers = False\nOnScreenDisplayMessages = False\n")
    port = free_port()
    cmd = [str(exe), "-p", "headless", "-u", str(user), "-v", "Null",
           "-C", "Main.DSP.Backend=No Audio Output",
           "-C", f"Main.General.GDBPort={port}",
           "-C", "Logger.Options.WriteToFile=True",
           "-C", "Logger.Options.Verbosity=4",
           "-C", "Logger.Logs.BOOT=True",
           "-C", "Logger.Logs.GDB_STUB=True",
           "-C", "Logger.Logs.COMMON=True",
           "-C", "Logger.Logs.MASTER=True",
           "-C", "Main.Interface.UsePanicHandlers=False",
           "-e", str(image)]
    log = open(user / "dolphin-stdout.log", "wb")
    proc = subprocess.Popen(cmd, cwd=str(exe.parent), stdin=subprocess.DEVNULL,
                            stdout=log, stderr=subprocess.STDOUT,
                            creationflags=CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY)
    gdb = None
    ok = False
    try:
        gdb = connect(proc, port, 90)
        print(f"gdb: connected port={port} pid={proc.pid}")
        print(f"halt: {gdb.cmd('?')}")  # CPU starts halted under GDB
        if image.suffix.lower() == ".dol":
            print(f"boot: DOL {image.name} (no disc header check)")
        else:
            header = gdb.read(MEM1_BASE, 8)
            magic = be32(gdb.read(MEM1_BASE + 0x1C, 4))
            print(f"boot: id={header[:6].decode('ascii', 'replace')} magic={magic:#010x}")
            if magic != 0xC2339F3D:
                raise RuntimeError("disc header magic missing; image did not load")

        retrace_addr = syms[RETRACE_SYMBOL][0]
        gdb.resume()
        if args.frames is not None:
            deadline = time.monotonic() + args.timeout
            while True:
                time.sleep(0.25)
                gdb.interrupt()
                count = be32(gdb.read(retrace_addr, 4))
                if count >= args.frames or time.monotonic() > deadline:
                    break
                gdb.resume()
        else:
            time.sleep(args.seconds)
            gdb.interrupt()
        retrace = be32(gdb.read(retrace_addr, 4))
        print(f"frames: {RETRACE_SYMBOL}={retrace} (VI retraces since boot)")

        for name in args.symbol:
            addr, size = syms[name]
            print(f"{name} @ {addr:#010x} u32={be32(gdb.read(addr, 4)):#010x}")
        if args.mario:
            ptr = be32(gdb.read(syms[MARIO_PTR_SYMBOL][0], 4))
            if ptr == 0:
                print(f"{MARIO_PTR_SYMBOL} = NULL (no Mario loaded)")
            elif not MEM1_BASE <= ptr < MEM1_BASE + MEM1_SIZE:
                print(f"{MARIO_PTR_SYMBOL} = {ptr:#010x} (not MEM1)")
            else:
                x, y, z = struct.unpack(">3f", gdb.read(ptr + MARIO_POS_OFFSET, 12))
                print(f"mario @ {ptr:#010x} mPosition=({x:.3f}, {y:.3f}, {z:.3f})")
        if args.dump:
            data = gdb.read(MEM1_BASE, MEM1_SIZE)
            Path(args.dump).write_bytes(data)
            print(f"dump: {len(data)} bytes MEM1 -> {args.dump}")
        ok = True
    finally:
        if gdb:
            gdb.sock.close()
        proc.kill()
        proc.wait(10)
        log.close()
        if ok:
            shutil.rmtree(user, ignore_errors=True)
            print("dolphin: stopped, temp user dir removed")
        else:
            print(f"dolphin: stopped after failure; logs kept in {user}")
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--frames", type=int, help="run until VI retraceCount >= N")
    ap.add_argument("--seconds", type=float, default=5.0, help="wall-clock run time (no --frames)")
    ap.add_argument("--symbol", action="append", default=[], help="print u32 of this symbol")
    ap.add_argument("--mario", action="store_true", help="print Mario mPosition")
    ap.add_argument("--dump", help="write MEM1 (24 MiB) to this file")
    ap.add_argument("--timeout", type=float, default=600, help="wall deadline for --frames")
    ap.add_argument("--dolphin", default=str(DOLPHIN))
    ap.add_argument("--image", default=str(RVZ), help="disc image (RVZ) or a .dol (no disc)")
    ap.add_argument("--symbols", default=str(SYMBOLS))
    ap.add_argument("--selftest", action="store_true")
    args = ap.parse_args(argv)
    if args.selftest:
        selftest()
        return 0
    return run(args)


if __name__ == "__main__":
    sys.exit(main())
