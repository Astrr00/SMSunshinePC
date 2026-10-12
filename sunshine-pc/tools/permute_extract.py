"""Helpers for permute_fn.py: pull one function out of an sms .cpp and turn it into
permuter input (C-syntax base.c) plus the wrapper that turns a candidate back into C++.

base.c is only *parsed* by the permuter (pycparser, no preprocessor); the real compile is C++ via
wrap(): `RET name(Cls* this_, args)` -> `RET Cls::name(args) [const]`, `this_` -> `this`,
`typedef int X;` stubs dropped, `#define` aliases (defs.h) restore `A::B<T>` chains that C cannot parse.
"""

import re
import sys
from pathlib import Path

KEYWORDS = set(
    "auto break case char const continue default do double else enum extern float for goto if int long "
    "register return short signed sizeof static struct switch typedef union unsigned void volatile while "
    "inline new delete this true false nullptr NULL".split()
)
BUILTIN_TYPES = "u8 u16 u32 u64 s8 s16 s32 s64 f32 f64 bool size_t".split()
TOKEN = re.compile(
    r"""0[xX][0-9a-fA-F]+\w*|\d[\w.]*|[A-Za-z_]\w*|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|\s+|::|.""", re.S
)
ID = re.compile(r"[A-Za-z_]\w*")


# ---------------------------------------------------------------- source scanning
def strip_comments(t):
    out, i, n = [], 0, len(t)
    while i < n:
        c = t[i]
        if t.startswith("//", i):
            while i < n and t[i] != "\n":
                i += 1
        elif t.startswith("/*", i):
            j = t.find("*/", i + 2)
            i = n if j < 0 else j + 2
            out.append(" ")
        elif c in "\"'":
            j = i + 1
            while j < n and t[j] != c:
                j += 2 if t[j] == "\\" else 1
            out.append(t[i : j + 1])
            i = j + 1
        else:
            out.append(c)
            i += 1
    return "".join(out)


def top_level_blocks(src):
    """[(header_text, open_idx, close_idx_excl)] of depth-0 `{}` blocks; namespace/extern "C" are transparent."""
    blocks, i, n, depth, start, cur = [], 0, len(src), 0, 0, None
    while i < n:
        c = src[i]
        if src.startswith("//", i):
            j = src.find("\n", i)
            i = n if j < 0 else j
            continue
        if src.startswith("/*", i):
            j = src.find("*/", i + 2)
            i = n if j < 0 else j + 2
            continue
        if c in "\"'":
            j = i + 1
            while j < n and src[j] != c:
                j += 2 if src[j] == "\\" else 1
            i = j + 1
            continue
        if c == "#" and src[src.rfind("\n", 0, i) + 1 : i].strip() == "":
            j = i
            while True:
                j = src.find("\n", j)
                j = n if j < 0 else j
                if j > 0 and src[j - 1] == "\\" and j < n:
                    j += 1
                    continue
                break
            i = j
            if depth == 0:
                start = i
            continue
        if c == "{":
            if depth == 0:
                hdr = strip_comments(src[start:i])
                if re.match(r'\s*(namespace\b|extern\s+"C")', hdr):
                    start = i + 1
                    i += 1
                    continue
                cur = (hdr, i)
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0 and cur:
                blocks.append((cur[0], cur[1], i + 1))
                start = i + 1
            elif depth < 0:  # closer of a transparent namespace
                depth = 0
                start = i + 1
        elif c == ";" and depth == 0:
            start = i + 1
        i += 1
    return blocks


def header_start(src, o):
    """Index where the definition whose `{` is at o begins (after the previous `;`, `}` or #line)."""
    i = o
    while i > 0:
        i = src.rfind("\n", 0, i)
        if i < 0:
            return 0
        prev = src[src.rfind("\n", 0, i) + 1 : i].strip()
        if prev == "" or prev.endswith((";", "}", "{")) or prev.startswith("#") or prev.endswith("*/"):
            return i + 1
    return 0


def mangled_class(sym):
    """('TKoopa', is_const) for member symbols, (None, False) for free functions."""
    m = re.match(r"(.+?)__(\d+)(.*)$", sym)
    if not m:
        return None, False
    n = int(m.group(2))
    rest = m.group(3)
    cls = rest[:n]
    return cls, rest[n : n + 1] == "C"


def find_function(src, sym, nth=0):
    name = sym.split("__")[0]
    cls, const = mangled_class(sym)
    if name in ("", "__ct", "__dt") or sym.startswith("__"):
        sys.exit("ctors/dtors/operators are not supported by auto base.c: write base.c by hand")
    hits = []
    for hdr, o, c in top_level_blocks(src):
        if ";" in hdr:
            continue
        m = re.search(r"(?:([\w<>:]+)::)?\b" + re.escape(name) + r"\s*\(", hdr)
        if m and (m.group(1) == cls):
            hits.append((hdr, o, c))
    if not hits:
        sys.exit(f"definition of {cls + '::' if cls else ''}{name} not found in the unit source")
    if len(hits) > 1:
        print(f"note: {len(hits)} definitions match, using #{nth} (--nth)", file=sys.stderr)
    return cls, const, hits[nth]


def strip_pp(body):
    """Drop preprocessor lines; keep the retail (non-TARGET_PC) branch."""
    out, stack = [], []  # stack of "keep this branch"
    for line in body.splitlines():
        s = line.strip()
        if s.startswith("#"):
            d = re.sub(r"\s+", " ", s[1:].strip())
            if re.match(r"(ifdef TARGET_PC\b|if defined\(TARGET_PC\)|if TARGET_PC\b)", d):
                stack.append(False)
            elif re.match(r"(ifndef TARGET_PC\b|if !defined\(TARGET_PC\))", d):
                stack.append(True)
            elif d.startswith(("if", "ifdef", "ifndef")):
                stack.append(True)
            elif d.startswith("else"):
                stack[-1] = not stack[-1]
            elif d.startswith("endif") and stack:
                stack.pop()
            continue
        if all(stack):
            out.append(line)
    return "\n".join(out)


# ---------------------------------------------------------------- C++ -> C-parsable aliases
def _balanced(t, i):
    """index after the `>` matching t[i]=='<', or -1 if it does not look like template args."""
    depth, j = 0, i
    while j < len(t):
        if t[j] == "<":
            depth += 1
        elif t[j] == ">":
            depth -= 1
            if depth == 0:
                inner = t[i + 1 : j]
                return j + 1 if re.fullmatch(r"[\w:\s,*&<>]*", inner) and "&&" not in inner else -1
        elif not re.match(r"[\w:\s,*&]", t[j]):
            return -1
        j += 1
    return -1


def hoist_cond_decls(t):
    """C++ `if (T* v = e)` -> `T* v = e; if (v)` (pycparser has no condition declarations)."""
    pat = re.compile(r"(?<!else )\b(if|while)\s*\(")
    pos = 0
    while True:
        m = pat.search(t, pos)
        if not m:
            return t
        depth, j = 1, m.end()
        while j < len(t) and depth:
            depth += {"(": 1, ")": -1}.get(t[j], 0)
            j += 1
        inner = t[m.end() : j - 1]
        d = re.fullmatch(r"\s*((?:const\s+)?[\w:<>]+(?:\s*\*+\s*|\s+))(\w+)\s*=(?!=)\s*(.*)", inner, re.S)
        if d and m.group(1) == "if" and d.group(1).strip() not in KEYWORDS | {"return"}:
            line = t.rfind("\n", 0, m.start()) + 1
            t = (t[:m.start()] + f"{d.group(1).strip()} {d.group(2)} = {d.group(3)};\n"
                 + t[line:m.start()] + f"if ({d.group(2)})" + t[j:])
            pos = m.start()
        else:
            pos = m.end()


def alias_text(t, defs):
    """Replace `A::B`, `T<...>`, `A::B<...>::c` chains by one identifier; record defs[alias] = chunk."""
    out, n, pos = [], len(t), 0
    while pos < n:
        m = TOKEN.match(t, pos)
        tok = m.group(0)
        if not ID.fullmatch(tok):
            out.append(tok)
            pos = m.end()
            continue
        j = m.end()
        while True:
            if tok[0:1].isupper() and j < n and t[j] == "<":
                e = _balanced(t, j)
                if e > 0:
                    j = e
            if t.startswith("::", j) and j + 2 < n and re.match(r"[A-Za-z_~]", t[j + 2]):
                k = ID.match(t, j + 2)
                if not k:
                    break
                tok, j = k.group(0), k.end()
                continue
            break
        chunk = t[pos:j]
        if "::" in chunk or "<" in chunk:
            alias = re.sub(r"\W+", "_", chunk).strip("_")
            defs[alias] = re.sub(r"\s+", " ", chunk)
            out.append(alias)
        else:
            out.append(chunk)
        pos = j
    return "".join(out)


# ---------------------------------------------------------------- base.c generation
def seed_types(text):
    """Identifiers that must be typedef stubs for pycparser (declared types, casts, params)."""
    found = set(BUILTIN_TYPES)
    for m in re.finditer(r"(?:^|[;{}(,])\s*(?:const\s+)?([A-Za-z_]\w*)\s*\**\s*\*?\s*([A-Za-z_]\w*)\s*(?=[=;,)\[])", text):
        if m.group(1) not in KEYWORDS and m.group(2) not in KEYWORDS:
            found.add(m.group(1))
    for m in re.finditer(r"\(\s*(?:const\s+)?([A-Z]\w*)\s*\**\s*\)\s*[\w(&*-]", text):
        found.add(m.group(1))
    for m in re.finditer(r"\b([TJ][A-Z]\w+)\s*\*", text):
        found.add(m.group(1))
    return {f for f in found if f not in KEYWORDS}


def make_base_c(src_text, cls, const, hit, name, perm_dir_path, permuter):
    hdr, o, c = hit
    body = strip_comments(strip_pp(src_text[o:c]))
    hdr = re.sub(r"\b(static|inline|virtual)\b", " ", strip_comments(hdr)).strip()
    m = re.search(r"(?:[\w<>:]+::)?\b" + re.escape(name) + r"\s*\((.*)\)\s*(const)?\s*$", hdr, re.S)
    if not m:
        sys.exit(f"cannot parse header: {hdr!r}")
    ret = hdr[: m.start()].strip() or "void"
    params = m.group(1).strip()
    if params == "void":
        params = ""
    if cls:
        selfp = f"{'const ' if const else ''}{cls}* this_"
        params = selfp + (", " + params if params else "")
    defs = {}
    text = alias_text(hoist_cond_decls(f"{ret} {name}({params})\n{body}\n"), defs)
    text = re.sub(r"\bthis\b", "this_", text)
    seeds = seed_types(text)
    for a, chunk in defs.items():
        if re.search(r"<|^[A-Z]\w*$", chunk) or chunk.split("::")[-1][:1] == "T":
            if not chunk.endswith(")"):
                seeds.add(a)
    sys.path.insert(0, str(permuter))
    from perm_pycparser.c_parser import CParser, ParseError

    stubs = {s for s in seeds if re.search(r"\b" + re.escape(s) + r"\b", text)}
    for _ in range(80):
        pre = "".join(f"typedef int {s};\n" for s in sorted(stubs))
        try:
            CParser().parse(pre + text, "base.c")
            break
        except ParseError as e:
            mm = re.search(r":(\d+):(\d+): before: (.*)$", str(e))
            if not mm:
                raise
            line, col, tok = int(mm.group(1)), int(mm.group(2)), mm.group(3).strip()
            lines = (pre + text).splitlines()
            before = "\n".join(lines[: line - 1] + [lines[line - 1][: col - 1]])
            c1 = re.search(r"([A-Za-z_]\w*)[\s*]+[A-Za-z_]\w*[\s*]*$", before)  # `Type name <err>`
            c2 = re.search(r"([A-Za-z_]\w*)[\s*]*$", before)  # `(Type)<err>`
            new = None
            for c_ in [m_.group(1) for m_ in (c1, c2) if m_] + ([tok] if ID.fullmatch(tok) else []):
                if c_ not in stubs and c_ not in KEYWORDS:
                    new = c_
                    break
            if not new:
                sys.exit(f"pycparser cannot parse the extracted function ({e}); write base.c by hand")
            stubs.add(new)
    else:
        sys.exit("pycparser stub loop did not converge; write base.c by hand")
    (perm_dir_path / "base.c").write_text(pre + text, encoding="utf-8")
    return defs


# ---------------------------------------------------------------- candidate -> C++
def wrap(d, cand_path):
    """Print the full translation unit for one permuter candidate (called from compile.sh)."""
    import json

    meta = json.loads((d / "meta.json").read_text(encoding="utf-8"))
    cand = Path(cand_path).read_text(encoding="utf-8", errors="replace")
    cand = re.sub(r"(?m)^typedef int [A-Za-z_0-9]+;\n?", "", cand)
    name, cls = meta["name"], meta["cls"]
    if cls:
        m = re.search(r"\b" + re.escape(name) + r"\s*\(\s*(?:const\s+)?\w+\s*\*\s*this_\s*(,\s*)?", cand)
        if not m:
            sys.exit("candidate lost its `this_` parameter")
        close = cand.index(")", m.end())
        cand = (
            cand[: m.start()] + f"{cls}::{name}(" + cand[m.end() : close + 1]
            + (" const" if meta["const"] else "") + cand[close + 1 :]
        )
        cand = re.sub(r"\bstatic\b", "", cand[: cand.index("{")]) + cand[cand.index("{") :]
        cand = re.sub(r"\bthis_\b", "this", cand)
    out = (d / "head.h").read_text(encoding="utf-8") + "\n" + (d / "defs.h").read_text(encoding="utf-8")
    out += "\n" + cand + "\n" + (d / "tail.h").read_text(encoding="utf-8")
    sys.stdout.buffer.write(out.encode("utf-8"))
