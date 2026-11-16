#!/usr/bin/env python3
"""Draws the classic piece set.

    python3 art/classic.py            # writes art/classic/*.svg and the PNGs the game loads
    python3 art/classic.py --sheet    # also docs/classic-pieces.png: every piece on every board

The set is the Staunton pattern as it comes off a lathe: boxwood and ebony,
seen from the side, and sized by what a piece is worth: pawn, knight, bishop,
rook, queen, king, each taller than the one before (the rook, which a turner
would make short, stands above the bishop here). Every piece but the knight
is a stack of turned parts — a foot, a bead, a stem, collars, a head — and
each part is described by the right half of its outline, the way a turner's
template is. The knight is the one carved thing in the box and is drawn by
hand.

Nothing is flat. A turned part is a cylinder, and it is lit like one: a bright
band left of the axis, a shadow down the right, a little light coming back at
the very edge. A ball is lit as a ball. The light is the same window, up and
to the left, for both colours. See art/DESIGN.md for the thinking.
"""

import sys

import kit
from kit import AXIS, circle, pt, symmetric

FLOOR = 90.5  # every piece stands on this line

LINE = 1.7    # the outline
FINE = 0.9    # a groove turned into a part

PALETTES = {
    # Boxwood: pale honey, warm in the shade.
    "w": dict(
        turn=[(0, "#9a7340"), (0.09, "#cba970"), (0.3, "#fff9e9"), (0.5, "#f2e0b8"), (0.78, "#cfae78"),
              (0.93, "#a78250"), (1, "#c5a26c")],
        ball=[(0, "#fffbf1"), (0.34, "#f3e2bb"), (0.8, "#c7a46c"), (1, "#a27c4a")],
        line="#3b2815", groove="#6a4a26", groove_opacity=0.85, eye="#2c1c0d"),
    # Ebony: black, with the light held in one narrow streak.
    "b": dict(
        turn=[(0, "#080706"), (0.1, "#1c1714"), (0.29, "#75695f"), (0.42, "#3d3530"), (0.78, "#1c1714"),
              (0.93, "#0b0908"), (1, "#2c2622")],
        ball=[(0, "#93867c"), (0.3, "#4a403a"), (0.8, "#1a1512"), (1, "#0a0807")],
        line="#050403", groove="#9a8c80", groove_opacity=0.5, eye="#d9cbb8"),
}


# --- turned parts -------------------------------------------------------------
# Each is (kind, outline): 'turn' for a part lit as a cylinder, 'stem' for one
# that also stands in the shade of the collar above it, 'ball' for one lit as
# a ball. Drawn in the order given, so a collar lies over the stem it is
# turned on.
#
# The templates below are drawn slender, as a turner would draw them. WIDEN is
# where they are fitted to a square of the board, which wants a piece broader
# than a mantelpiece does.
WIDEN = 1.2
HEAD = 1.13  # the same for the knight's head, which is broad already
SMALL = 0.9  # and the size the knight's head is carved at, of the size it is drawn


def turned(profile):
    wide = []
    for seg in profile:
        seg = list(seg)
        for i in range(1, len(seg), 2):
            seg[i] *= WIDEN
        wide.append(tuple(seg))
    return symmetric(wide)


def at(x, y, widen=WIDEN):
    return pt(AXIS + (x - AXIS) * widen, y)


def disc(y, half, thick):
    """A collar seen edge-on: flat faces, a rounded rim."""
    r = thick / 2
    return turned([("M", half - r / WIDEN, y + r), ("Q", half, y + r, half, y), ("Q", half, y - r, half - r / WIDEN, y - r)])


def foot(half):
    """What every piece stands on: a thick disc, and a bead on top of it."""
    return [("turn", turned([("M", half - 1.4, FLOOR), ("Q", half, FLOOR, half, FLOOR - 1.6),
                             ("L", half, FLOOR - 2.9), ("Q", half, FLOOR - 4.5, half - 1.4, FLOOR - 4.5)])),
            ("turn", disc(84.6, half - 3.0, 3.4))]


def stem(half, neck, neck_y):
    """The body: out of the bead in a concave sweep, up to a narrow neck."""
    drop = 83.6 - neck_y  # a short stem sweeps in sooner
    return ("stem", turned([("M", half - 5.0, 83.6),
                            ("C", half - 6.4, 83.6 - min(5.2, drop * 0.2), neck + 3.0, 83.6 - min(8.1, drop * 0.31),
                             neck + 0.9, neck_y + min(13.0, drop * 0.5)),
                            ("L", neck, neck_y)]))


def groove(y, half, sag=2.2):
    """A line turned round a part, seen a little from above."""
    return ("groove", f"M{at(AXIS - half, y)} Q{at(AXIS, y + sag)} {at(AXIS + half, y)}")


def pawn():
    parts = foot(19.0)
    parts.append(stem(19.0, 5.6, 59.4))
    parts.append(("turn", disc(60.4, 10.4, 3.2)))
    parts.append(("ball", circle(AXIS, 46.4, 11.6)))
    return parts, []


def rook():
    parts = foot(24.0)
    # A tower's shaft is thick and nearly straight — and tall: the rook is
    # worth more than a bishop or a knight and stands above both.
    parts.append(("stem", turned([("M", 19.0, 83.6), ("C", 17.4, 77, 14.4, 62, 13.6, 45)])))
    parts.append(("turn", disc(44.0, 17.2, 3.6)))
    parts.append(("turn", disc(40.6, 14.6, 2.4)))
    # Corbelled out to the battlements: three merlons, the middle one facing us.
    parts.append(("turn", turned([("M", 13.4, 39.6), ("Q", 14.4, 32.4, 18.0, 30.0), ("L", 18.0, 17.5),
                                  ("L", 12.0, 17.5), ("L", 12.0, 23.0), ("L", 5.3, 23.0),
                                  ("L", 5.3, 17.5), ("L", 0, 17.5)])))
    return parts, [groove(30.2, 17.6, 2.0)]


def bishop():
    parts = foot(22.0)
    parts.append(stem(22.0, 6.2, 66.1))
    parts.append(("turn", disc(67.0, 12.0, 3.4)))
    parts.append(("turn", disc(63.8, 9.2, 2.4)))
    parts.append(("ball", circle(AXIS, 28.0, 3.5)))
    mitre = turned([("M", 5.8, 62.6), ("C", 12.0, 60.0, 13.6, 52.8, 12.2, 46.8),
                    ("C", 10.6, 40.2, 5.0, 36.0, 0, 30.4)])
    parts.append(("ball", mitre))
    # The cut of the mitre: a slit sawn down into it from the right.
    marks = [("cut", (mitre, f"M{at(59.6, 37.2)} L{at(51.8, 50.0)}"))]
    return parts, marks


def queen():
    parts = foot(23.5)
    parts.append(stem(23.5, 7.4, 52.5))
    parts.append(("turn", disc(57.0, 9.8, 2.4)))
    parts.append(("turn", disc(53.4, 13.4, 3.6)))
    parts.append(("turn", disc(49.9, 10.4, 2.4)))
    # Behind the coronet: the dome it surrounds, and the ball on top of that.
    parts.append(("ball", circle(AXIS, 14.4, 3.8)))
    parts.append(("ball", turned([("M", 10.6, 28.0), ("C", 10.6, 21.4, 5.4, 17.6, 0, 17.6)])))
    # The coronet: a cup opening upward, its rim cut into points.
    parts.append(("turn", turned([("M", 7.6, 48.8), ("C", 8.0, 43.4, 12.6, 38.0, 15.6, 30.2),
                                  ("L", 16.8, 20.6), ("L", 11.8, 27.6), ("L", 8.5, 19.8),
                                  ("L", 4.2, 27.0), ("L", 0, 19.2)])))
    return parts, [groove(31.4, 15.0, 2.4)]


def king():
    parts = foot(24.5)
    parts.append(stem(24.5, 7.8, 50.5))
    parts.append(("turn", disc(55.0, 10.2, 2.4)))
    parts.append(("turn", disc(51.4, 14.0, 3.6)))
    parts.append(("turn", disc(47.9, 10.8, 2.4)))
    # A cross pattée: four arms that flare. It stands behind the crown's dome.
    parts.append(("turn", symmetric([("M", 3.4, 23.0), ("Q", 1.9, 19.2, 1.8, 15.8), ("Q", 5.2, 16.0, 8.6, 17.6),
                                     ("L", 8.6, 9.4), ("Q", 5.2, 11.0, 1.8, 11.2), ("Q", 2.0, 7.8, 4.2, 4.6)])))
    # The crown: a cup that flares to its rim and is closed over with a dome.
    parts.append(("turn", turned([("M", 8.0, 46.8), ("C", 8.4, 41.6, 12.6, 35.8, 15.5, 29.4),
                                  ("Q", 16.6, 26.8, 14.6, 26.0), ("C", 11.4, 24.8, 6.2, 22.2, 2.8, 21.8)])))
    return parts, [groove(30.4, 14.8, 2.4)]


def knight():
    parts = foot(23.5)

    def h(x, y):
        # Drawn at one size and stood on the bead a little smaller: a knight
        # is worth a bishop, and is about as tall.
        return at(x, 83.8 + (y - 83.8) * SMALL, HEAD * SMALL)

    # The head. Clockwise from the chest: up the throat, along the jaw to the
    # chin, round the muzzle, up the face, the ear, and down the crest.
    head = (f"M{h(33.4, 83.8)} "
            f"C{h(32.0, 77.6)} {h(33.2, 71.6)} {h(36.6, 66.6)} "    # breast
            f"C{h(39.6, 62.4)} {h(42.0, 59.2)} {h(42.2, 55.2)} "    # throat
            f"C{h(38.2, 56.2)} {h(33.8, 59.6)} {h(30.6, 63.2)} "    # jaw
            f"C{h(28.4, 65.8)} {h(24.8, 67.6)} {h(21.6, 66.2)} "    # chin
            f"C{h(18.0, 64.6)} {h(16.2, 60.4)} {h(17.4, 56.4)} "    # muzzle
            f"C{h(18.8, 51.8)} {h(23.2, 47.4)} {h(26.6, 42.4)} "    # nose
            f"C{h(29.6, 38.0)} {h(32.6, 33.6)} {h(37.2, 29.6)} "    # forehead
            f"C{h(37.6, 25.8)} {h(39.6, 22.4)} {h(42.8, 19.4)} "    # ear, front
            f"C{h(44.4, 22.4)} {h(45.8, 25.6)} {h(47.8, 27.6)} "    # ear, back
            f"C{h(55.6, 29.4)} {h(61.4, 36.6)} {h(63.2, 46.6)} "    # crest
            f"C{h(65.0, 56.4)} {h(65.6, 70.0)} {h(64.4, 83.8)} Z")
    parts.insert(1, ("turn", head))  # under the bead it rises from
    marks = [
        # The mane: a band down the crest, and the locks it is cut into.
        ("groove", f"M{h(49.4, 29.8)} C{h(55.4, 32.6)} {h(58.6, 39.2)} {h(59.8, 47.4)} "
                   f"C{h(61.4, 57.0)} {h(61.8, 69.6)} {h(61.0, 81.6)}"),
        ("groove", f"M{h(55.4, 33.6)} L{h(58.0, 30.8)}"),
        ("groove", f"M{h(58.6, 40.2)} L{h(61.8, 38.4)}"),
        ("groove", f"M{h(59.9, 48.4)} L{h(63.5, 47.6)}"),
        ("groove", f"M{h(61.0, 57.4)} L{h(64.7, 57.2)}"),
        ("groove", f"M{h(61.6, 66.6)} L{h(65.3, 66.8)}"),
        ("groove", f"M{h(61.5, 75.4)} L{h(65.0, 76.0)}"),
        # The round of the cheek, the mouth, the inside of the ear.
        ("groove", f"M{h(42.2, 55.2)} C{h(46.4, 51.6)} {h(46.8, 44.8)} {h(42.8, 40.4)}"),
        ("groove", f"M{h(18.6, 63.6)} Q{h(22.8, 64.2)} {h(26.8, 61.6)}"),
        ("groove", f"M{h(42.2, 26.4)} L{h(43.0, 22.6)}"),
        ("eye", (AXIS + (35.4 - AXIS) * HEAD * SMALL, 83.8 + (37.0 - 83.8) * SMALL)),
        ("nostril", (AXIS + (20.6 - AXIS) * HEAD * SMALL, 83.8 + (58.6 - 83.8) * SMALL)),
    ]
    return parts, marks


PIECES = {"P": pawn, "R": rook, "N": knight, "B": bishop, "Q": queen, "K": king}
# Half the width of each piece's foot: the shadow it stands in is as wide.
FOOT = {"P": 19.0, "R": 24.0, "N": 23.5, "B": 22.0, "Q": 23.5, "K": 24.5}


# --- writing them out ---------------------------------------------------------

def gradient(name, kind, stops):
    if kind == "turn":
        head = f'<linearGradient id="{name}" x1="0" y1="0" x2="1" y2="0">'
        tail = "</linearGradient>"
    else:
        head = f'<radialGradient id="{name}" cx="0.36" cy="0.3" r="0.78">'
        tail = "</radialGradient>"
    body = "".join(f'<stop offset="{offset}" stop-color="{colour}"/>' for offset, colour in stops)
    return head + body + tail


def svg(letter, colour):
    pal = PALETTES[colour]
    parts, marks = PIECES[letter]()
    out = [kit.svg_open(), "<defs>",
           gradient("turn", "turn", pal["turn"]),
           gradient("ball", "ball", pal["ball"]),
           # What a collar does to the stem under it.
           '<linearGradient id="under" x1="0" y1="0" x2="0" y2="1">'
           '<stop offset="0" stop-color="#000" stop-opacity="0.34"/>'
           '<stop offset="0.22" stop-color="#000" stop-opacity="0"/></linearGradient>',
           '<filter id="soft" x="-20%" y="-200%" width="140%" height="500%">'
           '<feGaussianBlur stdDeviation="1.2"/></filter>']
    for kind, what in marks:
        if kind == "cut":
            out.append(f'<clipPath id="cut"><path d="{what[0]}"/></clipPath>')
    out.append("</defs>")

    # The shadow the piece stands in.
    out.append(f'<ellipse cx="{AXIS}" cy="{FLOOR - 0.2}" rx="{FOOT[letter] * WIDEN - 0.5:.1f}" ry="2.3" fill="#000" '
               f'opacity="0.3" filter="url(#soft)"/>')

    for kind, d in parts:
        fill = "ball" if kind == "ball" else "turn"
        out.append(f'<path d="{d}" fill="url(#{fill})" stroke="{pal["line"]}" stroke-width="{LINE}" '
                   f'stroke-linejoin="round"/>')
        if kind == "stem":
            out.append(f'<path d="{d}" fill="url(#under)"/>')

    line = (f'fill="none" stroke="{pal["groove"]}" stroke-opacity="{pal["groove_opacity"]}" '
            f'stroke-width="{FINE}" stroke-linecap="round"')
    for kind, what in marks:
        if kind == "groove":
            out.append(f'<path d="{what}" {line}/>')
        elif kind == "cut":
            out.append(f'<path d="{what[1]}" clip-path="url(#cut)" fill="none" stroke="{pal["line"]}" '
                       f'stroke-width="2.1"/>')
        elif kind == "eye":
            x, y = what
            out.append(f'<ellipse cx="{x}" cy="{y}" rx="1.9" ry="1.25" transform="rotate(-38 {x} {y})" '
                       f'fill="{pal["eye"]}"/>')
        elif kind == "nostril":
            x, y = what
            out.append(f'<ellipse cx="{x}" cy="{y}" rx="1.3" ry="0.85" transform="rotate(-50 {x} {y})" '
                       f'fill="{pal["eye"]}"/>')
    out.append("</svg>")
    return "\n".join(out)


def main():
    kit.write_set("classic", svg)
    if "--sheet" in sys.argv:
        kit.sheet("classic", kit.ROOT / "docs" / "classic-pieces.png")


if __name__ == "__main__":
    main()
