#!/usr/bin/env python3
"""Draws the Gothic piece set.

    python3 art/gothic.py            # writes art/gothic/*.svg and the PNGs the game loads
    python3 art/gothic.py --sheet    # also docs/gothic-pieces.png: every piece on every board

The set is this file: twelve drawings described as outlines on a 100 x 100
square and written out as SVG. Change a number here, run it, look at the sheet.
The PNGs are made from the SVGs with rsvg-convert (librsvg); the sheet also
needs Pillow.

The idea of the set is the architecture the name comes from. Everything that
is a ball or a curve in a Staunton set is a point here: the pawn is a finial,
the bishop's mitre is a lancet arch, the rook has swallow-tail battlements, the
queen's crown is a row of spires, and the king's is closed over with a dome
that carries a cross with flared arms. Edges are chamfered rather than rounded,
and the windows cut into the pieces are pointed arches.

Light comes from the left on both colours: the pale pieces are shaded down
their right side, the dark ones carry a highlight down their left.
"""

import pathlib
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SVG_DIR = ROOT / "art" / "gothic"
PNG_DIR = ROOT / "game" / "Content" / "pieces" / "gothic"
SIZE = 256  # pixels; the same as the classic set

AXIS = 50.0  # the vertical line every piece but the knight is symmetric about

PALETTES = {
    "w": dict(fill="#ece3cf", tone="#cbbd9f", tone_side="right", line="#1b181e", mark="#1b181e"),
    # Slate rather than black: a truly black piece disappears on a dark square.
    "b": dict(fill="#373243", tone="#595267", tone_side="left", line="#0c0b10", mark="#dbd1b9"),
}
GEM = "#b3222d"
GEM_LINE = "#3d0b10"

LINE = 3.0   # outline
MARK = 1.9   # lines drawn on a piece


# The outlines below are drawn tall and narrow, on a square of their own. This
# is where they are fitted to the square the game gives a piece: a little
# wider, a little shorter, and standing on the same line as the classic set.
WIDEN, SHORTEN, TOP = 1.2, 0.92, 9.0


def place(x, y):
    return AXIS + (x - AXIS) * WIDEN, TOP + (y - 3.5) * SHORTEN


def pt(x, y):
    x, y = place(x, y)
    return f"{x:.2f},{y:.2f}"


def symmetric(profile, axis=AXIS):
    """A closed outline from its right half.

    `profile` runs up the right-hand side: ('M', x, y) to start, then
    ('L', x, y), ('Q', cx, cy, x, y) or ('C', x1, y1, x2, y2, x, y), where x is
    the distance from the axis. The left side is the same thing mirrored.
    """
    ends, right = [], []
    for seg in profile:
        kind = seg[0]
        if kind in "ML":
            right.append(f"{kind}{pt(axis + seg[1], seg[2])}")
            ends.append((seg[1], seg[2]))
        elif kind == "Q":
            right.append(f"Q{pt(axis + seg[1], seg[2])} {pt(axis + seg[3], seg[4])}")
            ends.append((seg[3], seg[4]))
        elif kind == "C":
            right.append(f"C{pt(axis + seg[1], seg[2])} {pt(axis + seg[3], seg[4])} {pt(axis + seg[5], seg[6])}")
            ends.append((seg[5], seg[6]))
        else:
            raise ValueError(kind)

    left = [f"L{pt(axis - ends[-1][0], ends[-1][1])}"]
    for i in range(len(profile) - 1, 0, -1):
        seg, (x, y) = profile[i], ends[i - 1]
        kind = seg[0]
        if kind == "L":
            left.append(f"L{pt(axis - x, y)}")
        elif kind == "Q":
            left.append(f"Q{pt(axis - seg[1], seg[2])} {pt(axis - x, y)}")
        elif kind == "C":
            left.append(f"C{pt(axis - seg[3], seg[4])} {pt(axis - seg[1], seg[2])} {pt(axis - x, y)}")
    return " ".join(right + left) + " Z"


def polygon(points):
    return "M" + " L".join(pt(x, y) for x, y in points) + " Z"


# --- parts shared between pieces ---------------------------------------------

def plinth(half):
    """The foot: a slab with chamfered shoulders and a narrower step on it."""
    slab = symmetric([("M", half, 90), ("L", half, 86.5), ("L", half - 2.5, 84),
                      ("L", half - 5.5, 84), ("L", half - 5.5, 81), ("L", half - 8, 79)])
    return slab


def collar(y, half, rise=3.4):
    """A lozenge-section ring: what separates a body from what stands on it."""
    return polygon([(AXIS + half, y), (AXIS + half - 3.6, y - rise), (AXIS - half + 3.6, y - rise),
                    (AXIS - half, y), (AXIS - half + 3.6, y + rise), (AXIS + half - 3.6, y + rise)])


def skirt(bottom_half, top_half, top_y, belly=0.55):
    """A body that sweeps in from the plinth to a narrow waist: concave, like
    the base of a pinnacle."""
    by = 79.5
    return symmetric([("M", bottom_half, by),
                      ("Q", top_half + (bottom_half - top_half) * (1 - belly), by - (by - top_y) * 0.42,
                       top_half, top_y)])


def lancet(x, top, bottom, half):
    """A pointed-arch window, as an outline."""
    spring = top + half * 1.9
    return (f"M{pt(x - half, bottom)} L{pt(x - half, spring)} "
            f"Q{pt(x - half, top + half * 0.6)} {pt(x, top)} "
            f"Q{pt(x + half, top + half * 0.6)} {pt(x + half, spring)} L{pt(x + half, bottom)} Z")


def diamond(x, y, w, h):
    return polygon([(x, y - h), (x + w, y), (x, y + h), (x - w, y)])


# --- the six pieces -----------------------------------------------------------
# Each returns (parts, marks): `parts` are filled, shaded and outlined, back to
# front — an outline, or (outline, line width) for one that wants a finer line;
# `marks` are drawn on top — ('line', d), ('solid', d), ('gem', d) or
# ('ball', (x, y, radius)).

def pawn():
    # A finial: the bud that ends every pinnacle on a Gothic roof.
    head = symmetric([("M", 0, 46.5), ("C", 7, 46.5, 12.6, 43, 12.6, 37),
                      ("C", 12.6, 29.5, 4.6, 25, 0, 14.5)])
    parts = [plinth(24), skirt(15, 6, 51.5), collar(48.8, 12.5), head]
    return parts, []


def rook():
    shaft = symmetric([("M", 18.5, 79.5), ("L", 14.5, 41), ("L", 0, 41)])
    # Corbelled out, then three merlons with swallow-tail tops.
    head = polygon([(31, 41), (29, 35), (29, 17), (33.5, 21), (38, 17), (38, 26.5), (45.5, 26.5),
                    (45.5, 17), (50, 21), (54.5, 17), (54.5, 26.5), (62, 26.5), (62, 17), (66.5, 21),
                    (71, 17), (71, 35), (69, 41)])
    parts = [plinth(27), shaft, head]
    marks = [("solid", lancet(50, 50, 69, 3.9)),
             ("line", f"M{pt(29.8, 33.6)} L{pt(70.2, 33.6)}")]
    return parts, marks


def bishop():
    mitre = symmetric([("M", 5.5, 48.5), ("C", 9.5, 45, 12.2, 41, 12.2, 36),
                       ("C", 12.2, 28, 5, 22, 0, 13)])
    finial = diamond(50, 10.2, 3.4, 4.6)
    parts = [plinth(27), skirt(17, 6, 53.5), collar(51, 12.5), mitre, finial]
    marks = [("line", f"M{pt(50, 24)} L{pt(50, 42)}"),
             ("line", f"M{pt(45.2, 30.5)} L{pt(54.8, 30.5)}")]
    return parts, marks


def queen():
    # A cup that opens upward into five spires.
    crown = polygon([(41.5, 48), (30.5, 30), (28, 13.5), (36.5, 27), (39, 9.5), (45, 26), (50, 6.5),
                     (55, 26), (61, 9.5), (63.5, 27), (72, 13.5), (69.5, 30), (58.5, 48)])
    parts = [plinth(29), skirt(19, 7.4, 52.5), collar(50, 14), crown]
    marks = [("line", f"M{pt(33.2, 34.5)} Q{pt(50, 39.5)} {pt(66.8, 34.5)}"),
             ("gem", diamond(50, 42.2, 2.3, 3.0)),
             ("line", lancet(50, 60, 77.5, 3.6))]
    for x, y in ((28, 13.5), (39, 9.5), (50, 6.5), (61, 9.5), (72, 13.5)):
        marks.append(("ball", (x, y - 0.4, 2.5)))
    return parts, marks


def king():
    # A closed crown: a band, and over it a dome drawn up to a point, the way
    # a spire is. That is what tells the king from the queen at a glance —
    # hers is open and all spikes — before the cross is even seen.
    crown = symmetric([("M", 9.5, 50), ("L", 17, 37), ("L", 17, 32.5),
                       ("C", 16.5, 25, 5.5, 26, 0, 17)])
    # A cross with flared arms. Its foot is hidden behind the crown; its
    # outline is thinner than the rest, or there would be no room inside it.
    cross = polygon([(47.2, 24), (47.6, 14.6), (40.6, 16.4), (40.6, 6.6), (47.6, 8.4), (45.6, 1.6),
                     (54.4, 1.6), (52.4, 8.4), (59.4, 6.6), (59.4, 16.4), (52.4, 14.6), (52.8, 24)])
    parts = [plinth(29), skirt(19.5, 8.2, 54.5), collar(52, 14.5), (cross, 2.1), crown]
    marks = [("line", f"M{pt(34.2, 37)} L{pt(65.8, 37)}"),
             ("line", f"M{pt(33.6, 32.5)} L{pt(66.4, 32.5)}"),
             # The ribs of the dome.
             ("line", f"M{pt(43.5, 32.5)} Q{pt(44.5, 25)} {pt(50, 19.5)}"),
             ("line", f"M{pt(56.5, 32.5)} Q{pt(55.5, 25)} {pt(50, 19.5)}"),
             ("gem", diamond(50, 43.8, 2.3, 3.0)),
             ("line", lancet(50, 62, 77.5, 3.6))]
    return parts, marks


def knight():
    # The one piece that is not symmetric. An armoured head, looking left, with
    # a mane of flames down the back of the neck.
    horse = polygon([
        (30, 79.5), (27.5, 69), (31.5, 60), (40, 52.5),          # chest, up to the throat
        (33, 56.5), (24.5, 62), (18.5, 60.5), (15.5, 54.5),      # jaw, chin, lip
        (17, 48), (27, 36.5), (37.5, 23), (44.5, 17),            # nose, up the face to the brow
        (46.5, 6), (53.5, 15.5),                                 # ear
        (61, 12.5), (60, 19), (67.5, 18.5), (65.5, 25.5),        # mane
        (73, 27), (70, 34), (77, 37.5), (73, 44.5),
        (79.5, 50), (75, 56), (80.5, 63), (76, 68), (78.5, 79.5),
    ])
    parts = [plinth(27), horse]
    marks = [("solid", polygon([(34, 31.5), (41.5, 28), (39.5, 33)])),      # eye
             ("solid", polygon([(19.3, 51.6), (22.3, 50.6), (21.2, 53.6)])),  # nostril
             ("line", f"M{pt(17.2, 57.6)} L{pt(26, 55.5)}"),               # mouth
             ("line", f"M{pt(44.5, 17)} L{pt(48.5, 30)} L{pt(41, 43)} L{pt(40, 52.5)}"),  # cheek plate
             ("line", f"M{pt(23, 41)} L{pt(31.5, 47.5)}")]                # noseband
    return parts, marks


PIECES = {"P": pawn, "R": rook, "N": knight, "B": bishop, "Q": queen, "K": king}


# --- writing them out ---------------------------------------------------------

def svg(letter, colour):
    pal = PALETTES[colour]
    parts, marks = PIECES[letter]()
    parts = [part if isinstance(part, tuple) else (part, LINE) for part in parts]
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" width="{SIZE}" height="{SIZE}">',
           "<defs>"]
    for i, (d, _) in enumerate(parts):
        out.append(f'<clipPath id="p{i}"><path d="{d}"/></clipPath>')
    out.append("</defs>")

    # Where the second tone falls: a band down one side, a little off the axis
    # so that the two halves are not equal.
    band = ('<rect x="55.5" y="0" width="44.5" height="100"' if pal["tone_side"] == "right"
            else '<rect x="0" y="0" width="43.5" height="100"')

    for i, (d, width) in enumerate(parts):
        out.append(f'<path d="{d}" fill="{pal["fill"]}"/>')
        out.append(f'<g clip-path="url(#p{i})">{band} fill="{pal["tone"]}"/></g>')
        out.append(f'<path d="{d}" fill="none" stroke="{pal["line"]}" stroke-width="{width}" '
                   f'stroke-linejoin="miter" stroke-miterlimit="3"/>')

    for kind, what in marks:
        if kind == "line":
            out.append(f'<path d="{what}" fill="none" stroke="{pal["mark"]}" stroke-width="{MARK}" '
                       f'stroke-linecap="round" stroke-linejoin="round"/>')
        elif kind == "solid":
            out.append(f'<path d="{what}" fill="{pal["mark"]}"/>')
        elif kind == "gem":
            out.append(f'<path d="{what}" fill="{GEM}" stroke="{GEM_LINE}" stroke-width="1.1" '
                       f'stroke-linejoin="round"/>')
        elif kind == "ball":
            (x, y), r = place(what[0], what[1]), what[2]
            out.append(f'<circle cx="{x}" cy="{y}" r="{r}" fill="{pal["fill"]}" stroke="{pal["line"]}" '
                       f'stroke-width="{LINE * 0.7:.2f}"/>')
    out.append("</svg>")
    return "\n".join(out)


def render(source, target, size):
    subprocess.run(["rsvg-convert", "-w", str(size), "-h", str(size), "-o", str(target), str(source)],
                   check=True)


def sheet():
    """Every piece of both colours on each board's two squares, for looking at."""
    from PIL import Image

    # (light, dark) of the boards in game/Source/Look.cpp, in the same order.
    boards = [("#f0d9b5", "#b58863"), ("#cbc3b4", "#702c32"), ("#e4e6ea", "#8d96a7"), ("#ebecd0", "#739552")]
    cell, order = 128, "KQRBNP"
    image = Image.new("RGB", (cell * 12, cell * len(boards)), "#181a1f")
    for row, squares in enumerate(boards):
        for col in range(12):
            colour = "w" if col < 6 else "b"
            square = Image.new("RGB", (cell, cell), squares[(row + col) % 2])
            piece = Image.open(PNG_DIR / f"{colour}{order[col % 6]}.png").resize((cell, cell), Image.LANCZOS)
            square.paste(piece, (0, 0), piece)
            image.paste(square, (col * cell, row * cell))
    image.save(ROOT / "docs" / "gothic-pieces.png")


def main():
    SVG_DIR.mkdir(parents=True, exist_ok=True)
    PNG_DIR.mkdir(parents=True, exist_ok=True)
    for colour in "wb":
        for letter in PIECES:
            source = SVG_DIR / f"{colour}{letter}.svg"
            source.write_text(svg(letter, colour) + "\n", encoding="utf-8")
            render(source, PNG_DIR / f"{colour}{letter}.png", SIZE)
    if "--sheet" in sys.argv:
        sheet()


if __name__ == "__main__":
    main()
