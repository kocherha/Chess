#!/usr/bin/env python3
"""Draws the bone piece set.

    python3 art/bone.py            # writes art/bone/*.svg and the PNGs the game loads
    python3 art/bone.py --sheet    # also docs/bone-pieces.png: every piece on every board

The set is the kind that stood on English tables before Staunton's: turned
bone, one side left its own colour and the other stained red. A piece is a
string of beads — a domed foot, a ring, a reel, a ring, a barrel, a collar —
with what it is on top: the king a pierced gallery and a cross, the queen a
coronet of balls, the rook a brick tower flying a pennant, the bishop a mitre
split into two horns, the knight a small carved head on a pedestal, the pawn
a ball on a baluster. The barrels of the king and queen are cut with the row
of slanting grains that gave such sets the name "barleycorn".

They are old. The bone has yellowed and cracked along its grain, and dirt has
settled where one bead meets the next.

Height goes by worth: pawn, knight, bishop, rook, queen, king — the rook's
pennant puts it above the bishop without making the tower any less a tower.
See art/DESIGN.md for the thinking.
"""

import sys

import kit
from kit import AXIS, circle, f, pt, symmetric

FLOOR = 90.5  # every piece stands on this line

LINE = 1.5    # the outline
FINE = 0.8    # what is cut into a part
WIDEN = 1.15  # drawn slender, as turned; stood on the board a little broader

PALETTES = {
    # Old bone: not the honey of boxwood but a greyer ivory, brown in the shade.
    "w": dict(
        turn=[(0, "#8a7350"), (0.09, "#b9a47c"), (0.3, "#fdf8ec"), (0.5, "#e9dfc6"), (0.78, "#c0ae88"),
              (0.93, "#967f58"), (1, "#b8a37c")],
        ball=[(0, "#fffcf3"), (0.34, "#eae0c8"), (0.8, "#bca982"), (1, "#937c57")],
        line="#3a2b1c", cut="#6a5132", cut_opacity=0.85, crack="#5b3f22", crack_opacity=0.5, eye="#2c1e10"),
    # The same bone stained with cochineal: a deep red that holds one streak of light.
    "b": dict(
        turn=[(0, "#2c0807"), (0.1, "#581512"), (0.29, "#cf705f"), (0.42, "#96322b"), (0.78, "#601916"),
              (0.93, "#320a09"), (1, "#762720")],
        ball=[(0, "#dd8a7a"), (0.3, "#9c372e"), (0.8, "#5a1613"), (1, "#2c0807")],
        line="#200505", cut="#1c0404", cut_opacity=0.75, crack="#1a0403", crack_opacity=0.55, eye="#f0d9c4"),
}


# --- the turner's vocabulary --------------------------------------------------
# Each returns (kind, outline). 'turn' is lit as a cylinder, 'under' is one
# that also stands in the shade of the bead above it, 'ball' is lit as a ball.

def turned(profile):
    wide = []
    for seg in profile:
        seg = list(seg)
        for i in range(1, len(seg), 2):
            seg[i] *= WIDEN
        wide.append(tuple(seg))
    return symmetric(wide)


def at(x, y):
    return pt(AXIS + (x - AXIS) * WIDEN, y)


def foot(half):
    """A domed foot: a rim, and a low rise to where the stem begins."""
    return ("turn", turned([("M", half - 1.2, FLOOR), ("Q", half, FLOOR, half, FLOOR - 1.3),
                            ("L", half, FLOOR - 2.7), ("Q", half, FLOOR - 4.1, half - 1.5, FLOOR - 4.3),
                            ("C", half - 5.5, FLOOR - 5.1, half * 0.62, FLOOR - 6.5, half * 0.5, FLOOR - 9.3)]))


def ring(y, half, thick):
    """A bead seen edge-on: flat faces, a rounded rim."""
    r = thick / 2
    return ("turn", turned([("M", half - r / WIDEN, y + r), ("Q", half, y + r, half, y),
                            ("Q", half, y - r, half - r / WIDEN, y - r)]))


def reel(bottom, top, half, waist):
    """A stem drawn in at the middle, like a cotton reel."""
    return ("under", turned([("M", half, bottom), ("Q", waist * 2 - half, (bottom + top) / 2, half, top)]))


def barrel(bottom, top, low, belly, high):
    """A body that swells: an urn, or the barrel of a barleycorn king."""
    span = bottom - top
    return ("under", turned([("M", low, bottom),
                             ("C", belly * 1.04, bottom - span * 0.2, belly * 1.04, top + span * 0.34, high, top)]))


def ball(y, r):
    return ("ball", circle(AXIS, y, r))


def band(y, half, sag=1.4):
    """A line turned round a part, seen a little from above."""
    return ("cut", f"M{at(AXIS - half, y)} Q{at(AXIS, y + sag)} {at(AXIS + half, y)}")


def grains(y, half, count, rise=2.6):
    """The barleycorn: a row of slanting cuts round a barrel."""
    step = 2.0 * half / (count - 1)
    return [("cut", f"M{at(AXIS - half + i * step - 0.9, y + rise)} L{at(AXIS - half + i * step + 0.9, y - rise)}")
            for i in range(count)]


def crack(x, top, bottom):
    """A hairline down the grain of old bone: nearly straight, never quite."""
    steps = max(2, int((bottom - top) / 3.2))
    d, lean = f"M{at(x, top)}", (0.5, -0.35, 0.3, -0.5, 0.25, -0.2)
    for i in range(steps):
        d += f" l{f(lean[i % len(lean)] * WIDEN)},{f((bottom - top) / steps)}"
    return ("crack", d)


# --- the six pieces -----------------------------------------------------------
# Each returns (half the width of its foot, parts back to front, marks).

def pawn():
    parts = [foot(18.5), ring(80.4, 7.6, 2.6), barrel(79.2, 65.4, 4.6, 8.2, 4.6), ring(64.4, 8.6, 2.8),
             reel(63.2, 53.0, 3.8, 3.0), ball(44.6, 9.2)]
    return 18.5, parts, [crack(48.6, 68.0, 77.0)]


def rook():
    tower = ("under", turned([("M", 14.2, 79.2), ("L", 12.6, 44.0), ("L", 0, 44.0)]))
    parapet = ("turn", turned([("M", 13.6, 42.0), ("L", 14.8, 40.0), ("L", 14.8, 32.4), ("L", 9.6, 32.4),
                               ("L", 9.6, 36.2), ("L", 3.6, 36.2), ("L", 3.6, 32.4), ("L", 0, 32.4)]))
    # The pennant: what puts a short tower above the bishop.
    pole = ("turn", symmetric([("M", 0.9, 36.0), ("L", 0.9, 18.6)]))
    flag = ("turn", f"M{pt(50.9, 19.4)} L{pt(63.0, 21.6)} L{pt(57.6, 23.4)} L{pt(63.0, 25.4)} L{pt(50.9, 27.2)} Z")
    parts = [foot(24.0), ring(80.4, 15.6, 2.8), tower, ring(43.2, 15.4, 2.8), flag, pole, ball(17.4, 1.8), parapet]
    # Brickwork: courses, and the joints staggered between them.
    marks = [band(y, 13.0 + (79 - y) * -0.04, 1.2) for y in (51.0, 58.0, 65.0, 72.0)]
    for row, y in enumerate((44.6, 51.6, 58.6, 65.6, 72.6)):
        for x in ((43.0, 50.0, 57.0) if row % 2 else (39.6, 46.6, 53.4, 60.4)):
            marks.append(("cut", f"M{at(x, y + 0.9)} L{at(x, y + 6.2)}"))
    marks.append(crack(45.4, 46.0, 62.0))
    return 24.0, parts, marks


def bishop():
    # The mitre as old sets cut it: split down the middle into two horns.
    mitre = ("ball", turned([("M", 5.2, 50.6), ("C", 10.6, 48.4, 12.4, 42.4, 11.6, 37.0), ("L", 9.4, 24.6),
                             ("L", 0, 38.4)]))
    parts = [foot(21.0), ring(80.4, 8.6, 2.8), reel(79.2, 73.4, 5.2, 4.0), ring(72.4, 8.0, 2.6),
             barrel(71.2, 55.4, 5.6, 10.2, 6.0), ring(54.4, 10.4, 3.0), ring(51.6, 7.0, 2.0), mitre]
    marks = [band(46.6, 9.4, 1.2), band(59.6, 9.0, 1.2), crack(52.2, 58.0, 69.0)]
    return 21.0, parts, marks


def knight():
    def h(x, y):
        # The classic set's head, carved small and stood on a pedestal.
        return pt(AXIS + (x - AXIS) * 1.13 * 0.72, 71.6 + (y - 83.8) * 0.72)

    head = ("turn",
            f"M{h(33.4, 83.8)} "
            f"C{h(32.0, 77.6)} {h(33.2, 71.6)} {h(36.6, 66.6)} "
            f"C{h(39.6, 62.4)} {h(42.0, 59.2)} {h(42.2, 55.2)} "
            f"C{h(38.2, 56.2)} {h(33.8, 59.6)} {h(30.6, 63.2)} "
            f"C{h(28.4, 65.8)} {h(24.8, 67.6)} {h(21.6, 66.2)} "
            f"C{h(18.0, 64.6)} {h(16.2, 60.4)} {h(17.4, 56.4)} "
            f"C{h(18.8, 51.8)} {h(23.2, 47.4)} {h(26.6, 42.4)} "
            f"C{h(29.6, 38.0)} {h(32.6, 33.6)} {h(37.2, 29.6)} "
            f"C{h(37.6, 25.8)} {h(39.6, 22.4)} {h(42.8, 19.4)} "
            f"C{h(44.4, 22.4)} {h(45.8, 25.6)} {h(47.8, 27.6)} "
            f"C{h(55.6, 29.4)} {h(61.4, 36.6)} {h(63.2, 46.6)} "
            f"C{h(65.0, 56.4)} {h(65.6, 70.0)} {h(64.4, 83.8)} Z")
    parts = [foot(22.0), ring(80.4, 10.4, 2.8), reel(79.2, 73.6, 6.4, 5.2), head, ring(72.4, 12.4, 2.8)]
    ex, ey = (float(v) for v in h(35.4, 37.0).split(","))
    nx, ny = (float(v) for v in h(20.6, 58.6).split(","))
    marks = [
        ("cut", f"M{h(49.4, 29.8)} C{h(55.4, 32.6)} {h(58.6, 39.2)} {h(59.8, 47.4)} "
                f"C{h(61.4, 57.0)} {h(61.8, 69.6)} {h(61.0, 80.0)}"),
        ("cut", f"M{h(58.6, 40.2)} L{h(61.8, 38.4)}"), ("cut", f"M{h(59.9, 48.4)} L{h(63.5, 47.6)}"),
        ("cut", f"M{h(61.0, 57.4)} L{h(64.7, 57.2)}"), ("cut", f"M{h(61.6, 66.6)} L{h(65.3, 66.8)}"),
        ("cut", f"M{h(42.2, 55.2)} C{h(46.4, 51.6)} {h(46.8, 44.8)} {h(42.8, 40.4)}"),
        ("cut", f"M{h(18.6, 63.6)} Q{h(22.8, 64.2)} {h(26.8, 61.6)}"),
        ("eye", (ex, ey, 1.5, 1.0, -38)), ("eye", (nx, ny, 1.0, 0.7, -50)),
    ]
    return 22.0, parts, marks


def queen():
    cup = ("turn", turned([("M", 5.2, 40.0), ("L", 12.2, 29.0), ("L", 12.2, 26.2), ("L", 0, 26.2)]))
    stalk = ("turn", symmetric([("M", 1.5, 27.0), ("L", 1.3, 17.0)]))
    parts = [foot(22.5), ring(80.4, 9.0, 2.8), reel(79.2, 72.4, 5.4, 4.1), ring(71.4, 8.4, 2.6),
             barrel(70.2, 50.4, 6.0, 12.8, 7.2), ring(49.4, 11.8, 3.0), ring(46.4, 7.8, 2.2),
             reel(45.4, 39.6, 4.8, 3.9), stalk, ball(13.6, 3.3), cup]
    # The coronet: a ball on every point of the rim.
    parts += [("ball", circle(AXIS + x * WIDEN, 24.4, 2.0)) for x in (-10.4, -5.2, 5.2, 10.4)]
    marks = [band(53.4, 10.6), band(66.6, 9.0)] + grains(60.0, 9.2, 6) + [band(31.0, 10.6, 1.2),
                                                                         crack(44.6, 54.0, 66.0)]
    return 22.5, parts, marks


def king():
    # The gallery: a cup with a toothed rim, pierced with little windows.
    gallery = ("turn", turned([("M", 5.4, 36.8), ("L", 12.8, 25.2), ("L", 12.8, 21.2), ("L", 9.4, 21.2),
                               ("L", 9.4, 23.4), ("L", 6.0, 23.4), ("L", 6.0, 21.2), ("L", 2.6, 21.2),
                               ("L", 2.6, 23.4), ("L", 0, 23.4)]))
    cross = ("turn", symmetric([("M", 2.4, 15.6), ("Q", 1.4, 13.2, 1.3, 11.4), ("Q", 3.9, 11.6, 6.4, 12.6),
                                ("L", 6.4, 6.6), ("Q", 3.9, 7.6, 1.3, 7.8), ("Q", 1.5, 5.6, 3.1, 3.6)]))
    parts = [foot(23.5), ring(80.4, 9.4, 2.8), reel(79.2, 71.6, 5.6, 4.2), ring(70.6, 8.6, 2.6),
             barrel(69.4, 47.6, 6.2, 13.6, 7.6), ring(46.6, 12.4, 3.0), ring(43.6, 8.2, 2.2),
             reel(42.6, 36.4, 5.0, 4.0), cross, ball(17.6, 3.0), gallery]
    marks = [band(50.8, 11.2), band(65.6, 9.4)] + grains(58.2, 9.8, 6)
    marks += [("hole", f"M{at(x - 1.1, 33.0)} L{at(x - 1.1, 29.6)} Q{at(x, 27.2)} {at(x + 1.1, 29.6)} "
                       f"L{at(x + 1.1, 33.0)} Z") for x in (44.4, 50.0, 55.6)]
    marks.append(crack(54.6, 51.0, 64.0))
    return 23.5, parts, marks


PIECES = {"P": pawn, "R": rook, "N": knight, "B": bishop, "Q": queen, "K": king}


# --- writing them out ---------------------------------------------------------

def gradient(name, kind, stops):
    if kind == "turn":
        head, tail = f'<linearGradient id="{name}" x1="0" y1="0" x2="1" y2="0">', "</linearGradient>"
    else:
        head, tail = f'<radialGradient id="{name}" cx="0.36" cy="0.3" r="0.78">', "</radialGradient>"
    return head + "".join(f'<stop offset="{offset}" stop-color="{colour}"/>' for offset, colour in stops) + tail


def svg(letter, colour):
    pal = PALETTES[colour]
    half, parts, marks = PIECES[letter]()
    out = [kit.svg_open(), "<defs>",
           gradient("turn", "turn", pal["turn"]),
           gradient("ball", "ball", pal["ball"]),
           # Where one bead meets the next, a hundred years of dirt have settled.
           '<linearGradient id="grime" x1="0" y1="0" x2="0" y2="1">'
           '<stop offset="0" stop-color="#1c0f05" stop-opacity="0.42"/>'
           '<stop offset="0.2" stop-color="#1c0f05" stop-opacity="0"/>'
           '<stop offset="0.86" stop-color="#1c0f05" stop-opacity="0"/>'
           '<stop offset="1" stop-color="#1c0f05" stop-opacity="0.3"/></linearGradient>',
           '<filter id="soft" x="-20%" y="-200%" width="140%" height="500%">'
           '<feGaussianBlur stdDeviation="1.2"/></filter>',
           "</defs>"]

    # The shadow the piece stands in.
    out.append(f'<ellipse cx="{AXIS}" cy="{FLOOR - 0.2}" rx="{half * WIDEN - 0.5:.1f}" ry="2.3" fill="#000" '
               f'opacity="0.3" filter="url(#soft)"/>')

    for kind, d in parts:
        fill = "ball" if kind == "ball" else "turn"
        out.append(f'<path d="{d}" fill="url(#{fill})" stroke="{pal["line"]}" stroke-width="{LINE}" '
                   f'stroke-linejoin="round"/>')
        if kind == "under":
            out.append(f'<path d="{d}" fill="url(#grime)"/>')

    for kind, what in marks:
        if kind == "cut":
            out.append(f'<path d="{what}" fill="none" stroke="{pal["cut"]}" stroke-opacity="{pal["cut_opacity"]}" '
                       f'stroke-width="{FINE}" stroke-linecap="round"/>')
        elif kind == "crack":
            out.append(f'<path d="{what}" fill="none" stroke="{pal["crack"]}" '
                       f'stroke-opacity="{pal["crack_opacity"]}" stroke-width="0.45" stroke-linecap="round" '
                       f'stroke-linejoin="round"/>')
        elif kind == "hole":
            out.append(f'<path d="{what}" fill="{pal["line"]}" fill-opacity="0.85"/>')
        elif kind == "eye":
            x, y, rx, ry, turn = what
            out.append(f'<ellipse cx="{f(x)}" cy="{f(y)}" rx="{rx}" ry="{ry}" '
                       f'transform="rotate({turn} {f(x)} {f(y)})" fill="{pal["eye"]}"/>')
    out.append("</svg>")
    return "\n".join(out)


def main():
    kit.write_set("bone", svg)
    if "--sheet" in sys.argv:
        kit.sheet("bone", kit.ROOT / "docs" / "bone-pieces.png")


if __name__ == "__main__":
    main()
