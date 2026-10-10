"""Make the decomp's Japanese string literals Shift-JIS at build time, like MWCC + sjiswrap do.

The sources are UTF-8, the disc data (object names, BMG text, name tables) is Shift-JIS, and clang-cl cannot emit a
Shift-JIS execution charset. This script rewrites every non-ASCII character that sits inside a string or character
literal into octal escapes of its CP932 bytes ("ふ" -> "\\203\\123"), in generated copies under <out>/files, and writes a
clang VFS overlay (<out>/overlay.yaml) that makes the compiler read the copies wherever the originals are opened (main
sources and every include form). Comments and everything else are left alone; line numbers do not change.

Usage: python sjis_overlay.py <sms dir> <out dir>      (incremental: only stale copies are rewritten)
Exit code 1 if a character cannot be encoded as CP932 (the file is then not overlaid and is listed).
"""
import os
import sys

EXTS = (".c", ".cpp", ".h", ".hpp", ".inc")


def has_high(b):
    return any(c >= 0x80 for c in b)


# BossEel.hpp has the literal "㼀" (U+3F00): the decomp author read the two raw bytes 3F 00 as one UTF-16 character
# (sjiswrap reports the same file as containing encoding errors). Emit the original bytes.
SPECIAL = {"\u3f00": b"\x3f\x00"}


def transform(text, problems, name):
    out = []
    i, n = 0, len(text)
    changed = False
    numtok = False  # inside a pp-number, where ' is a digit separator, not a character literal
    while i < n:
        c = text[i]
        two = text[i:i + 2]
        if two == "//":
            j = text.find("\n", i)
            j = n if j < 0 else j
            out.append(text[i:j])
            i = j
            numtok = False
        elif two == "/*":
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append(text[i:j])
            i = j
            numtok = False
        elif c in "\"'" and not (c == "'" and numtok):
            # literal prefix (L, u, U, u8, R, LR...) sits directly before the quote
            k = i
            while k > 0 and (text[k - 1].isalnum() or text[k - 1] == "_"):
                k -= 1
            prefix = text[k:i]
            raw = prefix.endswith("R") and c == '"'
            wide = prefix in ("L", "u", "U", "u8", "LR", "uR", "UR", "u8R")
            if raw:
                close = text.find("(", i)
                delim = text[i + 1:close]
                end = text.find(")" + delim + '"', close)
                end = n if end < 0 else end + len(delim) + 2
                out.append(text[i:end])
                i = end
                continue
            j = i + 1
            buf = [c]
            while j < n and text[j] != c and text[j] != "\n":
                ch = text[j]
                if ch == "\\" and j + 1 < n:
                    buf.append(text[j:j + 2])
                    j += 2
                    continue
                if ord(ch) >= 0x80:
                    if wide:
                        problems.append(f"{name}: wide literal with non-ASCII text left unchanged")
                        buf.append(ch)
                    else:
                        try:
                            buf.append("".join("\\%03o" % b for b in (SPECIAL.get(ch) or ch.encode("cp932"))))
                            changed = True
                        except UnicodeEncodeError:
                            problems.append(f"{name}: U+{ord(ch):04X} not encodable as CP932")
                            return None
                else:
                    buf.append(ch)
                j += 1
            if j < n and text[j] == c:
                buf.append(c)
                j += 1
            out.append("".join(buf))
            i = j
            numtok = False
        else:
            if c.isdigit() and not numtok and not (i > 0 and (text[i - 1].isalnum() or text[i - 1] == "_")):
                numtok = True
            elif numtok and not (c.isalnum() or c in "._'"):
                numtok = False
            out.append(c)
            i += 1
    return "".join(out) if changed else None


def main():
    sms, outdir = os.path.abspath(sys.argv[1]), os.path.abspath(sys.argv[2])
    files_dir = os.path.join(outdir, "files")
    entries = {}  # original dir -> {filename: copy path}
    problems, done, fresh = [], 0, 0
    for base in ("src", "include"):
        for dp, _, fs in os.walk(os.path.join(sms, base)):
            for f in fs:
                if not f.endswith(EXTS):
                    continue
                src = os.path.join(dp, f)
                raw = open(src, "rb").read()
                if not has_high(raw):
                    continue
                dst = os.path.join(files_dir, os.path.relpath(src, sms))
                if os.path.exists(dst) and os.path.getmtime(dst) >= os.path.getmtime(src):
                    entries.setdefault(dp, {})[f] = dst
                    done += 1
                    continue
                new = transform(raw.decode("utf-8"), problems, os.path.relpath(src, sms))
                if new is None:
                    continue
                os.makedirs(os.path.dirname(dst), exist_ok=True)
                with open(dst, "w", encoding="ascii" if not has_high(new.encode("utf-8")) else "utf-8", newline="") as w:
                    w.write(new)
                entries.setdefault(dp, {})[f] = dst
                done += 1
                fresh += 1

    def q(p):
        return "'" + p.replace("\\", "/").replace("'", "''") + "'"

    lines = ["{", "  'version': 0,", "  'case-sensitive': false,", "  'roots': ["]
    for d, fs in sorted(entries.items()):
        lines.append("    { 'name': %s, 'type': 'directory', 'contents': [" % q(d))
        for f, dst in sorted(fs.items()):
            lines.append("      { 'name': %s, 'type': 'file', 'external-contents': %s }," % (q(f), q(dst)))
        lines.append("    ]},")
    lines += ["  ]", "}"]
    os.makedirs(outdir, exist_ok=True)
    with open(os.path.join(outdir, "overlay.yaml"), "w", encoding="utf-8", newline="\n") as w:
        w.write("\n".join(lines) + "\n")
    print(f"sjis overlay: {done} files ({fresh} rewritten), {len(problems)} problems")
    for p in problems:
        print("  " + p)
    return 1 if any("not encodable" in p for p in problems) else 0


if __name__ == "__main__":
    sys.exit(main())
