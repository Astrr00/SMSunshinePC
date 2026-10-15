#!/usr/bin/env python3
"""Generate original flat-vector controller button glyphs.

Output: assets/glyphs/<set>/<button>.png, 128x128 RGBA8, sets xbox, playstation, switch, gamecube, keyboard.
Geometry is generic (discs, squares, triangles, stroke letters); no vendor artwork is copied.
The keyboard set draws keycaps labelled with the default key of each pad input (src/sdk/pad_keys.cpp).
Shapes are signed-distance fields evaluated on a 2x2 supersample grid per pixel.
Run: python tools/gen_glyphs.py [style ...]   (no argument: every set; a style name rewrites only that set)
"""
import functools
import math
import os
import struct
import sys
import zlib

SIZE = 128
SS = 2  # supersample samples per axis
C = SIZE / 2

BUTTONS = ["a", "b", "x", "y", "z", "start", "l", "r",
           "dpad_up", "dpad_down", "dpad_left", "dpad_right",
           "c_up", "c_down", "c_left", "c_right", "stick_l", "stick_r"]
FACE = ("a", "b", "x", "y")

WHITE = (255, 255, 255)
DARK = (44, 44, 50)
GREY = (120, 120, 130)

STYLES = {
    "xbox": dict(face="disc", col=dict(a=(76, 175, 80), b=(229, 57, 53), x=(30, 136, 229), y=(253, 216, 53)),
                 util=DARK, ink=WHITE, cdisc=(253, 216, 53), z=GREY),
    "playstation": dict(face="outline", col=dict(a=(92, 140, 230), b=(230, 90, 100), x=(220, 120, 200), y=(90, 190, 150)),
                        util=DARK, ink=WHITE, cdisc=(150, 150, 165), z=GREY),
    "switch": dict(face="chip", col=dict(a=(230, 70, 60), b=(240, 190, 50), x=(60, 130, 230), y=(80, 190, 110)),
                   util=(70, 70, 78), ink=WHITE, cdisc=(70, 70, 78), z=GREY),
    "gamecube": dict(face="gc", col=dict(a=(40, 160, 110), b=(200, 60, 60), x=(90, 80, 170), y=(90, 80, 170)),
                     util=(90, 80, 170), ink=WHITE, cdisc=(230, 200, 60), z=(90, 80, 170)),
    "keyboard": dict(face="keycap", col={}, util=(70, 70, 78), ink=(44, 44, 50), cdisc=(242, 242, 244), z=GREY),
}

# Keycap colours (keyboard set only).
KEY_TOP = (242, 242, 244)
KEY_EDGE = (150, 150, 160)
KEY_LEGEND = (44, 44, 50)
KEY_BG = (70, 70, 78)

# Default keyboard key per pad input, from src/sdk/pad_keys.cpp: A Space, B E, X Q, Y F, Start Enter, Z Z,
# L Left Shift, R Left Ctrl, D-pad arrows, C-stick IJKL, left stick WASD (cluster), stick_r IJKL (cluster).
KEYBOARD_KEYS = {
    "a": "SPACE", "b": "E", "x": "Q", "y": "F", "start": "ENTER", "z": "Z", "l": "SHIFT", "r": "CTRL",
    "dpad_up": "UP", "dpad_down": "DOWN", "dpad_left": "LEFT", "dpad_right": "RIGHT",
    "c_up": "I", "c_down": "K", "c_left": "J", "c_right": "L",
}

# Stroke letters in normalized coords (u right, v down), each polyline in [-1, 1].
LETTERS = {
    "A": [[(-1, 1), (0, -1), (1, 1)], [(-0.5, 0.25), (0.5, 0.25)]],
    "B": [[(-0.75, -1), (-0.75, 1)],
          [(-0.75, -1), (0.25, -1), (0.6, -0.6), (0.25, -0.1), (-0.75, -0.1)],
          [(-0.75, -0.1), (0.35, -0.1), (0.7, 0.45), (0.35, 1), (-0.75, 1)]],
    "X": [[(-1, -1), (1, 1)], [(1, -1), (-1, 1)]],
    "Y": [[(-1, -1), (0, 0.1), (1, -1)], [(0, 0.1), (0, 1)]],
    "Z": [[(-0.9, -1), (0.9, -1), (-0.9, 1), (0.9, 1)]],
    "L": [[(-0.5, -1), (-0.5, 1), (0.7, 1)]],
    "R": [[(-0.7, 1), (-0.7, -1), (0.25, -1), (0.6, -0.6), (0.25, -0.1), (-0.7, -0.1)], [(-0.1, -0.1), (0.7, 1)]],
    "C": [[(0.8, -0.75), (0.4, -1), (-0.4, -1), (-0.8, -0.5), (-0.8, 0.5), (-0.4, 1), (0.4, 1), (0.8, 0.75)]],
    "D": [[(-0.8, -1), (-0.8, 1), (0.2, 1), (0.8, 0.5), (0.8, -0.5), (0.2, -1), (-0.8, -1)]],
    "E": [[(0.8, -1), (-0.8, -1), (-0.8, 1), (0.8, 1)], [(-0.8, 0), (0.4, 0)]],
    "F": [[(0.8, -1), (-0.8, -1), (-0.8, 1)], [(-0.8, 0), (0.4, 0)]],
    "I": [[(-0.5, -1), (0.5, -1)], [(0, -1), (0, 1)], [(-0.5, 1), (0.5, 1)]],
    "J": [[(0.5, -1), (0.5, 0.5), (0, 1), (-0.5, 1), (-0.6, 0.5)]],
    "K": [[(-0.7, -1), (-0.7, 1)], [(0.7, -1), (-0.7, 0.1)], [(-0.2, -0.2), (0.7, 1)]],
    "Q": [[(-0.8, -1), (0.8, -1), (0.8, 1), (-0.8, 1), (-0.8, -1)], [(0.2, 0.2), (0.9, 1.1)]],
    "S": [[(0.8, -0.9), (0.4, -1), (-0.4, -1), (-0.8, -0.5), (-0.4, 0), (0.4, 0), (0.8, 0.5), (0.4, 1), (-0.4, 1), (-0.8, 0.9)]],
    "T": [[(-0.9, -1), (0.9, -1)], [(0, -1), (0, 1)]],
    "W": [[(-1, -1), (-0.5, 1), (0, -0.1), (0.5, 1), (1, -1)]],
}


def sd_disc(px, py, cx, cy, r):
    return math.hypot(px - cx, py - cy) - r


def sd_ring(px, py, cx, cy, r, w):
    return abs(math.hypot(px - cx, py - cy) - r) - w / 2


def sd_rbox(px, py, cx, cy, hw, hh, r):
    qx = abs(px - cx) - (hw - r)
    qy = abs(py - cy) - (hh - r)
    return math.hypot(max(qx, 0), max(qy, 0)) + min(max(qx, qy), 0) - r


def dist_seg(px, py, ax, ay, bx, by):
    vx, vy = bx - ax, by - ay
    wx, wy = px - ax, py - ay
    t = max(0.0, min(1.0, (wx * vx + wy * vy) / (vx * vx + vy * vy)))
    return math.hypot(wx - vx * t, wy - vy * t)


def sd_strokes(px, py, segs, w):
    return min(dist_seg(px, py, ax, ay, bx, by) for (ax, ay), (bx, by) in segs) - w / 2


def sd_poly(px, py, pts):
    d = float("inf")
    inside = False
    n = len(pts)
    for i in range(n):
        ax, ay = pts[i]
        bx, by = pts[(i + 1) % n]
        d = min(d, dist_seg(px, py, ax, ay, bx, by))
        if (ay > py) != (by > py) and px < (bx - ax) * (py - ay) / (by - ay) + ax:
            inside = not inside
    return -d if inside else d


def sd_poly_ring(px, py, pts, w):
    return abs(sd_poly(px, py, pts)) - w / 2


def letter_segs(ch, cx, cy, hh):
    hw = hh * 0.8
    segs = []
    for poly in LETTERS[ch]:
        pts = [(cx + u * hw, cy + v * hh) for u, v in poly]
        segs.extend(zip(pts, pts[1:]))
    return segs


def letter(ch, cx, cy, hh, w):
    return functools.partial(sd_strokes, segs=letter_segs(ch, cx, cy, hh), w=w)


def key_text(text, cx, cy, hh, w):
    # letters side by side, centred on cx
    adv = hh * 0.8 * 2.5
    x0 = cx - (len(text) - 1) * adv / 2
    return [letter(ch, x0 + i * adv, cy, hh, w) for i, ch in enumerate(text)]


def arrow(dx, dy, cx=C, cy=C, size=30):
    # filled triangle pointing along (dx, dy)
    tip = (cx + dx * size, cy + dy * size)
    base = (cx - dx * size * 0.6, cy - dy * size * 0.6)
    side = (-dy * size * 0.75, dx * size * 0.75)
    pts = [tip, (base[0] + side[0], base[1] + side[1]), (base[0] - side[0], base[1] - side[1])]
    return functools.partial(sd_poly, pts=pts)


def face_layers(style, btn):
    s = STYLES[style]
    col = s["col"][btn]
    kind = s["face"]
    if kind == "disc":
        return [(functools.partial(sd_disc, cx=C, cy=C, r=56), col),
                (letter(btn.upper(), C, C, 30, 13), s["ink"])]
    if kind == "chip":
        return [(functools.partial(sd_rbox, cx=C, cy=C, hw=56, hh=56, r=24), DARK),
                (letter(btn.upper(), C, C, 32, 14), col)]
    if kind == "outline":
        if btn == "a":  # cross
            segs = [((34, 34), (94, 94)), ((94, 34), (34, 94))]
            return [(functools.partial(sd_strokes, segs=segs, w=16), col)]
        if btn == "b":  # circle
            return [(functools.partial(sd_ring, cx=C, cy=C, r=40, w=13), col)]
        if btn == "x":  # square
            return [(lambda px, py: abs(sd_rbox(px, py, C, C, 38, 38, 4)) - 6.5, col)]
        pts = [(C, 26), (C + 40, 96), (C - 40, 96)]  # triangle
        return [(functools.partial(sd_poly_ring, pts=pts, w=13), col)]
    # gc: round A/B, rounded-square X/Y
    if btn in ("a", "b"):
        r = 56 if btn == "a" else 44
        return [(functools.partial(sd_disc, cx=C, cy=C, r=r), col),
                (letter(btn.upper(), C, C, 30 if btn == "a" else 24, 13), WHITE)]
    return [(functools.partial(sd_rbox, cx=C, cy=C, hw=56, hh=56, r=24), col),
            (letter(btn.upper(), C, C, 30, 13), WHITE)]


def util_layers(style, btn):
    s = STYLES[style]
    util = s["util"]
    if btn.startswith("dpad_"):
        d = {"dpad_up": (0, -1), "dpad_down": (0, 1), "dpad_left": (-1, 0), "dpad_right": (1, 0)}[btn]
        return [(functools.partial(sd_rbox, cx=C, cy=C, hw=56, hh=56, r=24), util),
                (arrow(*d, size=34), WHITE)]
    if btn.startswith("c_"):
        d = {"c_up": (0, -1), "c_down": (0, 1), "c_left": (-1, 0), "c_right": (1, 0)}[btn]
        return [(functools.partial(sd_disc, cx=C, cy=C, r=56), s["cdisc"]),
                (arrow(*d, size=30), DARK)]
    if btn == "start":
        bars = [((34, 46), (94, 46)), ((34, 64), (94, 64)), ((34, 82), (94, 82))]
        return [(functools.partial(sd_rbox, cx=C, cy=C, hw=56, hh=34, r=17), util),
                (functools.partial(sd_strokes, segs=bars, w=10), WHITE)]
    if btn in ("l", "r"):
        return [(functools.partial(sd_rbox, cx=C, cy=C, hw=56, hh=40, r=20), util),
                (letter(btn.upper(), C, C, 28, 13), WHITE)]
    if btn == "z":
        return [(functools.partial(sd_rbox, cx=C, cy=C, hw=56, hh=56, r=24), s["z"]),
                (letter("Z", C, C, 30, 13), WHITE)]
    # stick_l / stick_r
    return [(functools.partial(sd_ring, cx=C, cy=C, r=50, w=14), util),
            (functools.partial(sd_disc, cx=C, cy=C, r=34), util),
            (letter(btn[-1].upper(), C, C, 22, 10), WHITE)]


def keycap(cx, cy, s, legend):
    # keycap body of half-size s: darker edge shifted down, lighter top, legend layers on top
    r = s * 0.36
    layers = [(functools.partial(sd_rbox, cx=cx, cy=cy + s * 0.06, hw=s, hh=s, r=r), KEY_EDGE),
              (functools.partial(sd_rbox, cx=cx, cy=cy - s * 0.04, hw=s - 3, hh=s - 4, r=r - 2), KEY_TOP)]
    return layers + [(fn, KEY_LEGEND) for fn in legend]


def keyboard_layers(btn):
    if btn in ("stick_l", "stick_r"):
        # four keys in a cross: up, left, down, right
        keys = "WASD" if btn == "stick_l" else "IJKL"
        layers = [(functools.partial(sd_rbox, cx=C, cy=C, hw=56, hh=56, r=24), KEY_BG)]
        for ch, (ox, oy) in zip(keys, ((0, -27), (-27, 0), (0, 27), (27, 0))):
            layers += keycap(C + ox, C + oy, 17, [letter(ch, C + ox, C + oy, 11, 7)])
        return layers
    key = KEYBOARD_KEYS[btn]
    if key in ("UP", "DOWN", "LEFT", "RIGHT"):
        d = {"UP": (0, -1), "DOWN": (0, 1), "LEFT": (-1, 0), "RIGHT": (1, 0)}[key]
        legend = [arrow(*d, cx=C, cy=C, size=30)]
    elif key == "SPACE":
        legend = [functools.partial(sd_rbox, cx=C, cy=C + 6, hw=40, hh=6, r=5)]
    elif key == "ENTER":
        segs = [((C + 22, C - 24), (C + 22, C + 8)), ((C + 22, C + 8), (C - 20, C + 8))]
        legend = [functools.partial(sd_strokes, segs=segs, w=10),
                  functools.partial(sd_poly, pts=[(C - 34, C + 8), (C - 18, C - 4), (C - 18, C + 20)])]
    elif key == "SHIFT":
        legend = [arrow(0, -1, cx=C, cy=C - 4, size=30),
                  functools.partial(sd_rbox, cx=C, cy=C + 26, hw=11, hh=12, r=3)]
    elif len(key) == 1:
        legend = key_text(key, C, C, 30, 11)
    else:
        legend = key_text(key, C, C, 12, 8)
    return keycap(C, C, 54, legend)


def layers_for(style, btn):
    if style == "keyboard":
        return keyboard_layers(btn)
    if btn in FACE:
        return face_layers(style, btn)
    return util_layers(style, btn)


def render(layers):
    out = bytearray(SIZE * SIZE * 4)
    n = SS * SS
    for py in range(SIZE):
        for px in range(SIZE):
            cnt = 0
            sr = sg = sb = 0
            for sy in range(SS):
                for sx in range(SS):
                    x = px + (sx + 0.5) / SS
                    y = py + (sy + 0.5) / SS
                    top = None
                    for fn, rgb in layers:
                        if fn(x, y) <= 0:
                            top = rgb
                    if top is not None:
                        cnt += 1
                        sr += top[0]
                        sg += top[1]
                        sb += top[2]
            o = (py * SIZE + px) * 4
            if cnt:
                out[o:o + 4] = bytes((round(sr / cnt), round(sg / cnt), round(sb / cnt), round(255 * cnt / n)))
    return bytes(out)


def write_png(path, rgba):
    raw = b"".join(b"\x00" + rgba[y * SIZE * 4:(y + 1) * SIZE * 4] for y in range(SIZE))

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)

    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(raw, 9))
           + chunk(b"IEND", b""))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(png)


def main(argv):
    root = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "glyphs")
    styles = argv or list(STYLES)
    count = 0
    for style in styles:
        for btn in BUTTONS:
            write_png(os.path.join(root, style, btn + ".png"), render(layers_for(style, btn)))
            count += 1
    print("wrote %d glyphs to %s" % (count, os.path.normpath(root)))


if __name__ == "__main__":
    main(sys.argv[1:])
