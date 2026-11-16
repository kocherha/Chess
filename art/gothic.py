#!/usr/bin/env python3
"""Draws the Gothic piece set.

    python3 art/gothic.py            # writes art/gothic/*.svg and the PNGs the game loads
    python3 art/gothic.py --sheet    # also docs/gothic-pieces.png: every piece on every board

The set is built the way the architecture it is named after is built: a
stepped plinth, a body, a collar, and a head that ends in a point. Where a
Staunton piece has a ball there is a spire; where it has a curve there is a
pointed arch, struck from two centres with a compass, as a mason would. The
pawn is a niche, the rook a tower with swallow-tail battlements, the bishop's
mitre an ogee arch with crockets, the queen's crown five spires, the king's a
ribbed vault under a cross. The knight is the gargoyle of the set: a war-horse
in plate, its mane a row of crockets. Height goes by worth: pawn, knight,
bishop, rook, queen, king, each standing above the one before.

It is drawn like a woodcut: bone and ink, one hard shadow, a line as heavy as
the lead in a window. On the dark pieces the line turns to bone, and they wear
a thin pale rim so that they do not sink into a dark square. Red glass and
dull gold appear in drops and nowhere else. See art/DESIGN.md for the thinking.
"""

import math
import sys

import kit
from kit import AXIS, circle, f, mirrored, polygon, pt, symmetric

FLOOR = 91.0  # every piece stands on this line

LINE = 2.7    # the outline: the lead of the window
MARK = 1.5    # what is drawn on a piece
RIM = 1.25    # the pale edge a dark piece wears outside its outline
SMALL = 0.8   # the size the knight is built at, of the size it is drawn

PALETTES = {
    # Bone, with a cold shadow down the right.
    "w": dict(fill="#ebe4d3", tone="#bfb6a6", tone_side="right", line="#131016", mark="#131016", rim=None),
    # Ink, with what light there is down the left. The marks are bone inlay.
    "b": dict(fill="#1c1823", tone="#3d364b", tone_side="left", line="#08060b", mark="#ddd4c0", rim="#cfc5b0"),
}
GLASS, GLASS_LINE = "#b01822", "#2a070b"
GOLD, GOLD_LINE = "#c49b44", "#2a1c06"


# --- the mason's tools --------------------------------------------------------

def arch(x, spring, half, radius, bottom=None):
    """A pointed arch struck from two centres. `radius` is in units of the
    span: 1 is the equilateral arch, more is a lancet. With `bottom` it is a
    closed window standing on a sill; without, only the two arcs."""
    r = radius * 2 * half
    apex = spring - math.sqrt(r * r - (r - half) ** 2)
    arcs = (f"A{f(r)},{f(r)} 0 0 1 {pt(x, apex)} "
            f"A{f(r)},{f(r)} 0 0 1 {pt(x + half, spring)}")
    if bottom is None:
        return f"M{pt(x - half, spring)} {arcs}"
    return f"M{pt(x - half, bottom)} L{pt(x - half, spring)} {arcs} L{pt(x + half, bottom)} Z"


def apex(spring, half, radius):
    r = radius * 2 * half
    return spring - math.sqrt(r * r - (r - half) ** 2)


def diamond(x, y, w, h):
    return polygon([(x, y - h), (x + w, y), (x, y + h), (x - w, y)])


def plinth(half):
    """The foot: a slab, a chamfer, a narrower step, another chamfer — the base
    of a pier."""
    return mirrored([(AXIS, FLOOR), (AXIS + half, FLOOR), (AXIS + half, 87.6), (AXIS + half - 2.6, 85.2),
                     (AXIS + half - 4.6, 85.2), (AXIS + half - 4.6, 82.4), (AXIS + half - 7.4, 80.0),
                     (AXIS, 80.0)])


def skirt(bottom_half, top_half, top_y):
    """A body that sweeps in from the plinth to a narrow waist: concave, like
    the foot of a pinnacle."""
    by = 80.5
    return symmetric([("M", bottom_half, by),
                      ("Q", top_half + (bottom_half - top_half) * 0.28, by - (by - top_y) * 0.42,
                       top_half, top_y)])


def collar(y, half, rise=3.2):
    """A lozenge-section ring: what separates a body from what stands on it."""
    return polygon([(AXIS + half, y), (AXIS + half - 3.6, y - rise), (AXIS - half + 3.6, y - rise),
                    (AXIS - half, y), (AXIS - half + 3.6, y + rise), (AXIS + half - 3.6, y + rise)])


def crocket(x, y, out, up, side):
    """A leaf hooked up and out of a sloping edge. `side` is +1 on the right."""
    return (f"M{pt(x - side * 1.4, y + 3.0)} Q{pt(x + side * out * 1.1, y + 1.6)} {pt(x + side * out, y - up)} "
            f"Q{pt(x + side * out * 0.3, y - 0.4)} {pt(x - side * 1.0, y - 2.6)} Z")


# --- the six pieces -----------------------------------------------------------
# Each returns (parts, marks). `parts` are filled, shaded and outlined, back to
# front: an outline, or (outline, options) — width= for a finer line, gold=True
# for one cast in metal. `marks` are drawn on top:
#   ('line', d)            a line in the colour of the piece's marks
#   ('solid', d)           a hole: filled with that colour
#   ('glass', d)           a pane of red glass in a lead line
#   ('lead', d)            the lead across a pane
#   ('gem', d)             a cut stone
#   ('bead', (x, y, r))    a gold ball

def pawn():
    # A niche: a pointed arch standing on corbels, with a slit of a window.
    # The smallest thing on the board, as it is the least.
    head = (f"M{pt(43.4, 61.6)} L{pt(39.4, 57.6)} L{pt(39.4, 53.0)} "
            f"A18,18 0 0 1 {pt(50, 36.6)} A18,18 0 0 1 {pt(60.6, 53.0)} "
            f"L{pt(60.6, 57.6)} L{pt(56.6, 61.6)} Z")
    finial = diamond(50, 34.0, 2.9, 4.0)
    parts = [plinth(21), skirt(13.4, 6.2, 65.5), collar(63.6, 11.6), (finial, dict(gold=True, width=1.5)), head]
    marks = [("solid", arch(50, 52.6, 2.5, 1.3, bottom=58.0))]
    return parts, marks


def rook():
    # Taller than the bishop and the knight: it is worth more than either.
    shaft = mirrored([(AXIS, 80.5), (68.4, 80.5), (64.8, 41.5), (AXIS, 41.5)])
    # Corbelled out, then three merlons with swallow-tail tops.
    head = mirrored([(AXIS, 41.5), (66.0, 41.5), (69.4, 36.1), (71.4, 36.1), (71.4, 32.5), (71.4, 15.0),
                     (66.9, 19.7), (62.4, 15.0), (62.4, 25.1), (54.5, 25.1), (54.5, 15.0), (AXIS, 19.7)])
    parts = [plinth(27), shaft, head]
    window = arch(50, 55.0, 4.2, 1.25, bottom=73.5)
    marks = [("line", f"M{pt(29.6, 32.1)} L{pt(70.4, 32.1)}"),
             # The corbel table: a row of little arches under the parapet.
             ("line", " ".join(arch(x, 39.1, 2.6, 1.0) for x in (37.0, 43.5, 50.0, 56.5, 63.0))),
             ("glass", window),
             ("lead", f"M{pt(50, 49.6)} L{pt(50, 73.5)} M{pt(45.8, 61.6)} L{pt(54.2, 61.6)}")]
    return parts, marks


def bishop():
    # The mitre is an ogee arch: out, in, and up to a point. Two pairs of
    # crockets climb its shoulders.
    mitre = symmetric([("M", 5.6, 62.6), ("C", 11.6, 60.0, 14.6, 55.5, 14.6, 50.6),
                       ("C", 14.6, 44.5, 7.2, 41.3, 3.4, 35.8), ("Q", 1.4, 32.8, 0, 28.5)])
    finial = diamond(50, 25.5, 3.0, 4.4)
    leaves = [crocket(50 + s * 11.4, 45.2, 4.4, 4.0, s) for s in (1, -1)]
    leaves += [crocket(50 + s * 4.8, 38.4, 4.2, 4.2, s) for s in (1, -1)]
    parts = [plinth(25), skirt(16.6, 6.6, 67.0), collar(65.0, 12.6)]
    parts += [(leaf, dict(width=1.5)) for leaf in leaves]
    parts += [(finial, dict(gold=True, width=1.5)), mitre]
    marks = [("line", f"M{pt(50, 35.8)} L{pt(50, 44.6)} M{pt(50, 57.4)} L{pt(50, 60.2)}"),
             ("foil", (50, 51.2, 2.6, 4))]
    return parts, marks


def queen():
    # A cup that opens upward into five spires, each with a gold bead.
    tips = [(50, 9.4), (61.6, 12.6), (72.4, 16.6)]
    crown = mirrored([(AXIS, 50.6), (58.6, 50.6), (68.8, 34.0), tips[2], (64.8, 30.2), tips[1],
                      (55.8, 29.0), tips[0]])
    parts = [plinth(27), skirt(18.6, 7.8, 55.0), collar(53.0, 14.0), crown]
    window = arch(50, 67.0, 5.0, 1.0, bottom=77.6)
    marks = [("line", f"M{pt(32.6, 37.0)} Q{pt(50, 42.4)} {pt(67.4, 37.0)}"),
             ("gem", diamond(50, 45.4, 2.3, 3.0)),
             ("glass", window),
             # Y-tracery: a mullion, and two arches of half the span.
             ("lead", f"M{pt(50, 67)} L{pt(50, 77.6)} " + arch(47.5, 67.0, 2.5, 1.0) + " "
                      + arch(52.5, 67.0, 2.5, 1.0))]
    for x, y in tips + [(2 * AXIS - x, y) for x, y in tips[1:]]:
        marks.append(("bead", (x, y - 0.3, 2.3)))
    return parts, marks


def king():
    # A closed crown: a circlet with a fleuron at each end, and between them a
    # vault drawn up to a point. That is what tells the king from the queen at
    # a glance — hers is open and all spikes — before the cross is even seen.
    crown = symmetric([("M", 9.8, 56.4), ("L", 17.2, 46.4), ("L", 17.2, 41.0), ("L", 19.4, 33.4),
                       ("L", 14.0, 38.6), ("C", 13.6, 31.4, 8.4, 26.0, 0, 22.0)])
    # A cross of four spearheads, the lower one the longest. Its foot is hidden
    # behind the crown; its outline is finer than the rest, or there would be
    # no room inside it.
    cross = mirrored([(AXIS, 6.0), (52.7, 10.0), (51.5, 13.2), (55.0, 12.1), (58.8, 14.8), (55.0, 17.5),
                      (51.5, 16.4), (52.9, 21.0), (51.8, 26.0), (AXIS, 26.0)])
    parts = [plinth(27.5), skirt(19.0, 8.8, 61.0), collar(59.0, 14.8), (cross, dict(gold=True, width=1.5)),
             crown]
    rose = circle(50, 71.4, 4.9)
    marks = [("line", f"M{pt(34.2, 46.4)} L{pt(65.8, 46.4)}"),
             ("line", f"M{pt(33.4, 41.0)} L{pt(66.6, 41.0)}"),
             # The ribs of the vault.
             ("line", f"M{pt(44.2, 41.0)} Q{pt(44.8, 31.6)} {pt(50, 24.6)}"),
             ("line", f"M{pt(55.8, 41.0)} Q{pt(55.2, 31.6)} {pt(50, 24.6)}"),
             ("gem", diamond(50, 51.6, 2.2, 2.9)),
             # A rose window, quartered.
             ("glass", rose),
             ("lead", f"M{pt(50, 66.5)} L{pt(50, 76.3)} M{pt(45.1, 71.4)} L{pt(54.9, 71.4)}")]
    return parts, marks


def knight():
    # The one piece that is not symmetric: an armoured head, looking left. The
    # mane is a row of crockets, hooked upward out of the line of the neck.
    crest = [(54.0, 15.6), (59.8, 21.4), (64.0, 29.2), (66.8, 38.2), (68.6, 47.8), (69.6, 57.8),
             (70.0, 68.0), (69.8, 80.5)]
    mane = []
    for (x0, y0), (x1, y1) in zip(crest, crest[1:]):
        # Out to a hook two thirds of the way down the step, then back in.
        dx, dy = x1 - x0, y1 - y0
        length = math.hypot(dx, dy)
        nx, ny = dy / length, -dx / length  # outward: to the right of the way down
        mane += [(x0 + dx * 0.62 + nx * 5.2 - dx * 0.34, y0 + dy * 0.62 + ny * 5.2 - dy * 0.34),
                 (x0 + dx * 0.74, y0 + dy * 0.74), (x1, y1)]
    # Drawn at one size and stood on the plinth smaller: a knight is worth a
    # bishop, and is no taller than one.
    def small(x, y):
        return AXIS + (x - AXIS) * SMALL, 80.5 + (y - 80.5) * SMALL

    def sp(x, y):
        return pt(*small(x, y))

    horse = polygon([small(x, y) for x, y in [
        (33.0, 80.5), (31.0, 70.6), (33.6, 62.4), (39.6, 54.0),      # chest, up to the throat
        (33.0, 57.6), (24.8, 63.2), (18.8, 62.0), (15.4, 56.0),      # jaw, chin, lip
        (16.6, 49.4), (26.4, 37.6), (36.6, 24.4), (43.6, 18.2),      # nose, up the face to the brow
        (46.6, 8.6), (54.0, 15.6),                                   # ear
    ] + mane])
    parts = [plinth(24), horse]
    marks = [("gem", polygon([small(33.4, 32.6), small(41.2, 28.8), small(39.2, 34.2)])),     # eye
             ("solid", polygon([small(19.0, 53.2), small(22.2, 52.0), small(21.0, 55.2)])),   # nostril
             ("line", f"M{sp(16.8, 58.8)} L{sp(26.0, 56.4)}"),                # mouth
             # The chamfron: the plate over the face, and its edge on the cheek.
             ("line", f"M{sp(43.6, 18.2)} L{sp(47.8, 30.6)} L{sp(40.6, 43.4)} L{sp(39.6, 54.0)}"),
             ("line", f"M{sp(22.6, 42.2)} L{sp(31.4, 48.6)}"),
             # The crinet: the lames of the neck.
             ("line", f"M{sp(47.8, 30.6)} Q{sp(56.0, 30.0)} {sp(62.8, 33.4)}"),
             ("line", f"M{sp(43.6, 43.6)} Q{sp(56.0, 41.8)} {sp(66.6, 47.0)}"),
             ("line", f"M{sp(37.8, 57.2)} Q{sp(54.0, 54.6)} {sp(68.2, 61.0)}"),
             ("line", f"M{sp(33.6, 69.4)} Q{sp(52.0, 67.0)} {sp(68.8, 73.2)}")]
    return parts, marks


PIECES = {"P": pawn, "R": rook, "N": knight, "B": bishop, "Q": queen, "K": king}


# --- writing them out ---------------------------------------------------------

def svg(letter, colour):
    pal = PALETTES[colour]
    parts, marks = PIECES[letter]()
    parts = [part if isinstance(part, tuple) else (part, {}) for part in parts]
    out = [kit.svg_open(), "<defs>"]
    for i, (d, _) in enumerate(parts):
        out.append(f'<clipPath id="p{i}"><path d="{d}"/></clipPath>')
    out.append("</defs>")

    # A dark piece's pale rim: every outline drawn once more, wider, underneath.
    if pal["rim"]:
        for d, options in parts:
            width = options.get("width", LINE) + 2 * RIM
            out.append(f'<path d="{d}" fill="none" stroke="{pal["rim"]}" stroke-width="{f(width)}" '
                       f'stroke-linejoin="miter" stroke-miterlimit="3"/>')
        for kind, what in marks:
            if kind == "bead":
                x, y, r = what
                out.append(f'<circle cx="{f(x)}" cy="{f(y)}" r="{f(r + 0.75 + RIM)}" fill="{pal["rim"]}"/>')

    # Where the second tone falls: a band down one side, a little off the axis
    # so that the two halves are not equal.
    band = ('<rect x="56" y="0" width="44" height="100"' if pal["tone_side"] == "right"
            else '<rect x="0" y="0" width="43.5" height="100"')

    for i, (d, options) in enumerate(parts):
        width = options.get("width", LINE)
        if options.get("gold"):
            out.append(f'<path d="{d}" fill="{GOLD}" stroke="{GOLD_LINE}" stroke-width="{width}" '
                       f'stroke-linejoin="miter" stroke-miterlimit="3"/>')
            continue
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
        elif kind == "glass":
            out.append(f'<path d="{what}" fill="{GLASS}" stroke="{GLASS_LINE}" stroke-width="1.5" '
                       f'stroke-linejoin="miter"/>')
        elif kind == "lead":
            out.append(f'<path d="{what}" fill="none" stroke="{GLASS_LINE}" stroke-width="1.1" '
                       f'stroke-linecap="butt"/>')
        elif kind == "gem":
            out.append(f'<path d="{what}" fill="{GLASS}" stroke="{GLASS_LINE}" stroke-width="1.1" '
                       f'stroke-linejoin="round"/>')
        elif kind == "foil":
            # A quatrefoil of glass: four circles, leaded round the outside only.
            x, y, r, lobes = what
            centres = [(x + r * 0.92 * math.cos(math.tau * k / lobes - math.pi / 2),
                        y + r * 0.92 * math.sin(math.tau * k / lobes - math.pi / 2)) for k in range(lobes)]
            for cx, cy in centres:
                out.append(f'<circle cx="{f(cx)}" cy="{f(cy)}" r="{f(r)}" fill="none" stroke="{GLASS_LINE}" '
                           f'stroke-width="2.6"/>')
            for cx, cy in centres:
                out.append(f'<circle cx="{f(cx)}" cy="{f(cy)}" r="{f(r)}" fill="{GLASS}"/>')
        elif kind == "bead":
            x, y, r = what
            out.append(f'<circle cx="{f(x)}" cy="{f(y)}" r="{f(r)}" fill="{GOLD}" stroke="{GOLD_LINE}" '
                       f'stroke-width="1.5"/>')
    out.append("</svg>")
    return "\n".join(out)


def main():
    kit.write_set("gothic", svg)
    if "--sheet" in sys.argv:
        kit.sheet("gothic", kit.ROOT / "docs" / "gothic-pieces.png")


if __name__ == "__main__":
    main()
