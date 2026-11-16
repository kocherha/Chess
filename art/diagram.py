#!/usr/bin/env python3
"""Draws the diagram piece set.

    python3 art/diagram.py            # writes art/diagram/*.svg and the PNGs the game loads
    python3 art/diagram.py --sheet    # also docs/diagram-pieces.png: every piece on every board

The pieces of a printed chess diagram: flat, black and white, nothing but an
outline and a few lines inside it. White is paper inside an ink line; Black is
solid ink with the same lines left white. That convention is as old as chess
books and belongs to nobody; these particular outlines were drawn here, from
nothing, for this game.

A black piece also gets a hairline of paper round it, the way a figure cut
out of a printed page would, so that it stands off a dark square.

Sizes follow what a piece is worth, as in the other sets, though more gently,
as on a diagram: pawn, then knight and bishop alike, rook, queen, king. See
art/DESIGN.md for the thinking.
"""

import sys

import kit
from kit import AXIS, f, pt

FLOOR = 90.0

INK = "#141414"
PAPER = "#fdfdfb"
OUTLINE = 2.7    # the line round a piece
INNER = 1.9      # the lines inside one
INSET = 3.4      # how far short of the edge a white line on a black piece stops


def smooth(points, k=1.0):
    """A closed outline through points, rounded. A point given as
    (x, y, 'c') is a corner and stays sharp."""
    pts = [(p[0], p[1], len(p) > 2) for p in points]
    n = len(pts)
    d = f"M{pt(pts[0][0], pts[0][1])}"
    for i in range(n):
        p0, p1, p2, p3 = pts[i - 1], pts[i], pts[(i + 1) % n], pts[(i + 2) % n]
        c1 = (p1[0], p1[1]) if p1[2] else (p1[0] + (p2[0] - p0[0]) * k / 6, p1[1] + (p2[1] - p0[1]) * k / 6)
        c2 = (p2[0], p2[1]) if p2[2] else (p2[0] - (p3[0] - p1[0]) * k / 6, p2[1] - (p3[1] - p1[1]) * k / 6)
        d += f" C{pt(*c1)} {pt(*c2)} {pt(p2[0], p2[1])}"
    return d + " Z"


def open_curve(points, k=1.0):
    """A line through points, rounded, not closed."""
    n = len(points)
    d = f"M{pt(*points[0])}"
    for i in range(n - 1):
        p0, p1, p2, p3 = points[max(i - 1, 0)], points[i], points[i + 1], points[min(i + 2, n - 1)]
        c1 = (p1[0] + (p2[0] - p0[0]) * k / 6, p1[1] + (p2[1] - p0[1]) * k / 6)
        c2 = (p2[0] - (p3[0] - p1[0]) * k / 6, p2[1] - (p3[1] - p1[1]) * k / 6)
        d += f" C{pt(*c1)} {pt(*c2)} {pt(*p2)}"
    return d


def sym(half, k=1.0):
    """A closed, rounded outline from its right half: points (dx, y[, 'c'])
    from the top of the piece to the bottom, dx being the distance from the
    axis. The left half is the same thing mirrored."""
    right = [(AXIS + p[0], p[1]) + tuple(p[2:]) for p in half]
    left = [(AXIS - p[0], p[1]) + tuple(p[2:]) for p in reversed(half) if p[0] > 1e-6]
    return smooth(right + left, k)


class Figure:
    def __init__(self, colour):
        self.white = colour == "w"
        self.out = []

    def shape(self, d):
        fill = PAPER if self.white else INK
        self.out.append(f'<path d="{d}" fill="{fill}" stroke="{INK}" stroke-width="{f(OUTLINE)}" '
                        f'stroke-linejoin="round"/>')

    def line(self, d, width=INNER):
        """A line inside a piece: ink on a white one, paper on a black one."""
        self.out.append(f'<path d="{d}" fill="none" stroke="{INK if self.white else PAPER}" '
                        f'stroke-width="{f(width)}" stroke-linecap="round" stroke-linejoin="round"/>')

    def mark(self, d):
        """A small filled detail: ink on a white piece, paper on a black one."""
        self.out.append(f'<path d="{d}" fill="{INK if self.white else PAPER}"/>')

    def across(self, y, half, sag=0.0, width=INNER):
        """A line from one edge to the other at height y, where the piece is
        `half` wide each side. On a white piece it is a seam and meets the
        outline; on a black one it stops short, or it would cut the piece in
        two. `sag` bows it downward in the middle."""
        reach = half if self.white else half - INSET
        self.line(f"M{pt(AXIS - reach, y)} Q{pt(AXIS, y + 2 * sag)} {pt(AXIS + reach, y)}", width)

    def rim(self, top, half):
        """What a crown stands on: the rim of it, seen a little from above,
        so that its lines and its lower edge are bowed."""
        low = FLOOR - 2.4
        self.shape(f"M{pt(AXIS - half, top)} L{pt(AXIS + half, top)} "
                   f"C{pt(AXIS + half + 0.6, top + 8)} {pt(AXIS + half - 0.4, low - 5)} {pt(AXIS + half - 2.6, low)} "
                   f"Q{pt(AXIS, FLOOR + 2.4)} {pt(AXIS - half + 2.6, low)} "
                   f"C{pt(AXIS - half + 0.4, low - 5)} {pt(AXIS - half - 0.6, top + 8)} {pt(AXIS - half, top)} Z")
        self.across(top + 6.6, half + 0.1, 1.4)
        self.across(top + 13.8, half - 0.4, 1.6)

    def bell(self, top, neck, half):
        """What a pawn and a bishop stand on: a skirt spreading onto a plate."""
        self.shape(kit.symmetric([("M", 0, FLOOR), ("L", half, FLOOR), ("L", half, FLOOR - 5.4),
                                  ("C", half - 6.5, FLOOR - 7.5, neck + 2.6, FLOOR - 15, neck, top),
                                  ("L", 0, top)]))
        self.across(FLOOR - 5.6, half - 0.4)

    def svg(self):
        head = [kit.svg_open(), "<defs>",
                '<filter id="rim" x="-10%" y="-10%" width="120%" height="120%">'
                '<feMorphology in="SourceAlpha" operator="dilate" radius="0.8" result="wide"/>'
                f'<feFlood flood-color="{PAPER}" flood-opacity="0.92"/>'
                '<feComposite in2="wide" operator="in" result="edge"/>'
                '<feMerge><feMergeNode in="edge"/><feMergeNode in="SourceGraphic"/></feMerge></filter>',
                "</defs>", "<g>" if self.white else '<g filter="url(#rim)">']
        return "\n".join(head + self.out + ["</g>", "</svg>"])


# --- the pieces ---------------------------------------------------------------

def pawn(colour):
    g = Figure(colour)
    # A skirt, shoulders, a ball.
    g.bell(60.0, 8.0, 22.5)
    g.shape(sym([(0, 50.6), (9.0, 51.6), (15.0, 55.4), (15.0, 59.4), (9.0, 62.6), (0, 63.4)]))
    g.shape(kit.circle(AXIS, 38.0, 11.0))
    return g.svg()


def rook(colour):
    g = Figure(colour)
    # One outline: battlements, a neck, straight walls, a spreading foot in two steps.
    top, notch = 11.4, 18.6
    g.shape(kit.mirrored([(AXIS, top), (AXIS + 4.6, top), (AXIS + 4.6, notch), (AXIS + 11.0, notch),
                          (AXIS + 11.0, top), (AXIS + 20.5, top), (AXIS + 20.5, 28.0), (AXIS + 15.0, 35.0),
                          (AXIS + 16.0, 67.0), (AXIS + 21.0, 74.0), (AXIS + 21.0, 81.0), (AXIS + 25.0, 81.0),
                          (AXIS + 25.0, FLOOR), (AXIS, FLOOR)]))
    g.across(28.0, 20.3)
    g.across(35.0, 14.8)
    g.across(67.0, 15.8)
    g.across(74.0, 20.8)
    g.across(81.0, 20.8 if g.white else 24.6)
    return g.svg()


def bishop(colour):
    g = Figure(colour)
    g.bell(66.0, 7.5, 24.0)
    # The collar under the mitre, the mitre, its ball.
    g.shape(sym([(0, 58.4), (10.0, 59.2), (16.4, 62.0), (16.4, 65.4), (10.0, 68.0), (0, 68.8)]))
    g.shape(sym([(0, 23.6, 1), (7.6, 29.6), (14.2, 38.0), (17.0, 47.0), (15.0, 55.0), (10.6, 60.6, 1)]))
    g.shape(kit.circle(AXIS, 19.4, 4.2))
    # The cross cut in the mitre.
    g.line(f"M{pt(AXIS, 35.4)} L{pt(AXIS, 51.6)} M{pt(AXIS - 6.6, 43.0)} L{pt(AXIS + 6.6, 43.0)}", INNER + 0.6)
    return g.svg()


def knight(colour):
    g = Figure(colour)
    # A horse's head and neck in profile, looking left.
    g.shape(smooth([
        (29.0, FLOOR, 1), (30.6, 82.6), (35.6, 73.0), (41.0, 65.0), (44.4, 58.6), (43.0, 54.6),      # chest, throat
        (37.0, 56.6), (30.6, 61.0), (25.6, 65.6), (20.6, 67.6), (15.6, 65.4), (12.6, 59.6),           # jaw, chin, lips
        (13.2, 53.0), (16.6, 45.6), (21.6, 37.6), (26.6, 30.0), (31.0, 23.6), (34.0, 20.6),           # nose, forehead
        (38.0, 15.4, 1), (43.4, 21.6),                                                               # ear
        (50.6, 19.8), (59.6, 21.6), (68.6, 27.4), (76.4, 36.6), (81.8, 48.6), (84.8, 62.0),           # poll, crest
        (86.0, 76.0), (86.0, FLOOR, 1),                                                              # back of the neck
    ]))
    # The mane: the line where it lies on the neck, and locks falling from it.
    g.line(open_curve([(46.0, 25.6), (57.0, 26.4), (67.0, 33.0), (74.0, 44.0), (78.0, 58.0), (79.6, 72.0),
                       (79.8, FLOOR - 5.0)]))
    # The cheek, the eye with its brow, the nostril, the mouth.
    g.line(open_curve([(33.6, 40.6), (40.0, 44.6), (42.6, 51.0)]), INNER - 0.2)
    g.mark(smooth([(23.8, 38.6, 1), (27.6, 33.6), (33.0, 33.0, 1), (30.0, 38.0)]))
    g.line(open_curve([(22.8, 33.6), (27.4, 29.8), (33.4, 29.4)]), INNER - 0.5)
    g.line(open_curve([(16.0, 59.6), (17.2, 57.0), (19.8, 56.4)]), INNER - 0.2)
    g.line(open_curve([(15.6, 65.0), (21.0, 64.0), (26.0, 61.0)]), INNER - 0.4)
    return g.svg()


def queen(colour):
    g = Figure(colour)
    # The crown: five points fanning from the rim, a ball on each.
    tips = [(0.0, 15.6), (16.6, 20.6), (30.6, 31.6)]
    half = [(0.0, 15.6), (8.4, 46.0), (16.6, 20.6), (22.2, 49.4), (30.6, 31.6), (20.6, 66.6)]
    g.shape(kit.mirrored([(AXIS + x, y) for x, y in half]))
    for x, y in tips:
        for side in ((-1, 1) if x else (1,)):
            g.shape(kit.circle(AXIS + side * x, y - 3.4, 4.3))
    g.across(57.6, 22.4, 1.6)
    g.rim(66.0, 20.6)
    return g.svg()


def king(colour):
    g = Figure(colour)
    # The cross.
    g.shape(kit.polygon([(47.9, 3.4), (52.1, 3.4), (52.1, 8.6), (57.6, 8.6), (57.6, 12.8), (52.1, 12.8),
                         (52.1, 23.0), (47.9, 23.0), (47.9, 12.8), (42.4, 12.8), (42.4, 8.6), (47.9, 8.6)]))
    # The crown: two full lobes, and between them the leaf the cross stands on.
    g.shape(sym([(0, 38.0, 1), (7.0, 30.0), (17.0, 26.0), (27.0, 29.6), (32.0, 40.0), (29.4, 53.0),
                 (21.0, 66.6, 1)]))
    g.shape(sym([(0, 21.0, 1), (5.0, 27.0), (7.4, 35.0), (5.6, 44.0), (0, 53.0, 1)]))
    # A line inside each lobe, following it round.
    for side in (-1, 1):
        g.line(open_curve([(AXIS + side * x, y) for x, y in
                           ((8.6, 60.0), (19.0, 55.0), (25.0, 46.0), (25.0, 38.6), (19.6, 33.4), (12.6, 34.6))]),
               INNER - 0.2)
    if not g.white:
        # On a black king the leaf would be lost in the crown: its two sides, in paper.
        for side in (-1, 1):
            g.line(open_curve([(AXIS + side * x, y) for x, y in ((1.6, 47.6), (4.2, 41.0), (4.4, 34.0), (2.6, 28.6))]),
                   INNER - 0.4)
    g.rim(66.0, 21.0)
    return g.svg()


PIECES = {"P": pawn, "R": rook, "N": knight, "B": bishop, "Q": queen, "K": king}


def svg(letter, colour):
    return PIECES[letter](colour)


def main():
    kit.write_set("diagram", svg)
    if "--sheet" in sys.argv:
        kit.sheet("diagram", kit.ROOT / "docs" / "diagram-pieces.png")


if __name__ == "__main__":
    main()
