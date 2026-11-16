#!/usr/bin/env python3
"""Draws the anime piece set.

    python3 art/anime.py            # writes art/anime/*.svg and the PNGs the game loads
    python3 art/anime.py --sheet    # also docs/anime-pieces.png: every piece on every board

These are chess pieces first: the six silhouettes anyone knows — a pawn's
ball, a rook's battlements, a knight's head, a bishop's cut mitre, a queen's
pointed coronet, a king's crown and cross — on a foot and a stem, as on any
board. What makes them anime is how they are drawn, and that each has a face.

The drawing is cel animation's: flat colour, one hard shadow down the right,
a single plum line with rounded joins, and round every piece a white edge,
like a sticker's, so that it stands clear of any square. The proportions are
a chibi's — short, broad, with a head too large for the body.

The faces are six characters, not one: the pawn is wide-eyed, the knight
bright and pleased with itself, the bishop serene with its eyes shut, the
rook stern under level brows, the queen all eyelashes, the king calm behind a
small moustache. The light side is cream with blue eyes, the dark side violet
with red ones; crowns are gold on both.

Height goes by worth: pawn, knight, bishop, rook, queen, king. See
art/DESIGN.md for the thinking.
"""

import sys

import kit
from kit import AXIS, f, pt, symmetric

FLOOR = 91.0

LINE = 1.9    # the outline
FINE = 1.2    # lines inside a piece
RIM = 1.5     # the white edge outside the outline
WIDEN = 1.14  # how much broader than drawn every piece but the knight stands

BLUSH = "#ff8fa3"
GOLD, GOLD_SHADE = "#ffd75e", "#f3a83a"

PALETTES = {
    "w": dict(body="#fffdf6", shade="#cdd6f0", shine=None, line="#3a2a44", face="#3a2a44",
              accent="#4aa8ff", accent_shade="#2b78d8", iris=("#1b55c4", "#86d6ff")),
    "b": dict(body="#51467c", shade="#352c58", shine="#8b7cd0", line="#1c1530", face="#fff3fb",
              accent="#ff4f72", accent_shade="#c9274b", iris=("#9c1030", "#ff93a8")),
}

# The right side of everything is in shadow: beyond a line that leans a
# little, so that the shadow is not a ruled stripe.
SHADOW = f"M{pt(57, 0)} L{pt(100, 0)} L{pt(100, 100)} L{pt(61, 100)} Z"


class Figure:
    """A drawing in the making: shapes back to front, each with its outline,
    and the white edge that goes round all of them."""

    def __init__(self, pal):
        self.pal = pal
        self.defs, self.rims, self.body = [], [], []
        self.clips = 0

    def shape(self, d, fill=None, shade=None, width=LINE, rim=True):
        """A filled, outlined shape with its shadow. Body colours unless told."""
        fill = fill or self.pal["body"]
        shade = shade or (self.pal["shade"] if fill == self.pal["body"] else None)
        if rim:
            self.rims.append(f'<path d="{d}" fill="none" stroke="#fff" stroke-width="{f(width + 2 * RIM)}" '
                             f'stroke-linejoin="round"/>')
        self.body.append(f'<path d="{d}" fill="{fill}"/>')
        if shade:
            self.inside(d, f'<path d="{SHADOW}" fill="{shade}"/>')
        self.body.append(f'<path d="{d}" fill="none" stroke="{self.pal["line"]}" stroke-width="{f(width)}" '
                         f'stroke-linejoin="round"/>')

    def inside(self, d, what):
        """Something drawn only where the shape `d` is."""
        self.clips += 1
        self.defs.append(f'<clipPath id="c{self.clips}"><path d="{d}"/></clipPath>')
        self.body.append(f'<g clip-path="url(#c{self.clips})">{what}</g>')

    def line(self, d, colour=None, width=FINE):
        self.body.append(f'<path d="{d}" fill="none" stroke="{colour or self.pal["line"]}" '
                         f'stroke-width="{f(width)}" stroke-linecap="round" stroke-linejoin="round"/>')

    def raw(self, what):
        self.body.append(what)

    def shine(self, x, y, rx, ry, turn=24):
        """The one hard highlight, where a dark piece has one."""
        if self.pal["shine"]:
            self.raw(f'<ellipse cx="{f(x)}" cy="{f(y)}" rx="{f(rx)}" ry="{f(ry)}" fill="{self.pal["shine"]}" '
                     f'transform="rotate({turn} {f(x)} {f(y)})"/>')

    def svg(self, widen=WIDEN):
        # Drawn slender and fitted to the square here, like the classic set.
        group = f'<g transform="translate({AXIS} 0) scale({widen} 1) translate(-{AXIS} 0)">'
        return "\n".join([kit.svg_open(), "<defs>"] + self.defs + ["</defs>", group] + self.rims + self.body
                         + ["</g>", "</svg>"])


# --- what the pieces share ----------------------------------------------------

def circle(x, y, r):
    """A ball. Narrower than it is tall as drawn, so that it is round once
    the piece has been broadened."""
    rx = r / WIDEN
    return f"M{pt(x - rx, y)} A{f(rx)},{f(r)} 0 1 0 {pt(x + rx, y)} A{f(rx)},{f(r)} 0 1 0 {pt(x - rx, y)} Z"


def pedestal(fig, half, neck, neck_y, collar):
    """A foot, a stem that sweeps in to a neck, and the collar on it: what
    stands under every head but the knight's."""
    fig.shape(symmetric([("M", neck, neck_y), ("Q", neck + (half - 4 - neck) * 0.3, neck_y + (85 - neck_y) * 0.62,
                                                half - 4, 85.5)]))
    foot(fig, half)
    fig.shape(symmetric([("M", collar - 2, neck_y + 0.6), ("Q", collar, neck_y + 0.6, collar, neck_y - 1.4),
                         ("Q", collar, neck_y - 3.4, collar - 2, neck_y - 3.4)]))


def foot(fig, half):
    fig.shape(symmetric([("M", half - 3.2, FLOOR), ("Q", half, FLOOR, half, 87.6), ("Q", half, 84.2, half - 3.2, 84.2)]))


def eye(fig, uid, x, y, rx=2.9, ry=3.7, lid=0.0, lashes=0):
    """An eye: white, an iris that is darker at the top, a pupil, two lights
    and one heavy lash. `lid` lowers the lid over that much of it; `lashes`
    is which side the eyelashes flick out on (-1 left, +1 right, 0 none)."""
    pal = fig.pal
    top, bottom = pal["iris"]
    fig.defs.append(f'<linearGradient id="{uid}" x1="0" y1="0" x2="0" y2="1">'
                    f'<stop offset="0" stop-color="{top}"/><stop offset="1" stop-color="{bottom}"/>'
                    f'</linearGradient>')
    white = (f"M{pt(x - rx, y)} A{f(rx)},{f(ry)} 0 1 0 {pt(x + rx, y)} A{f(rx)},{f(ry)} 0 1 0 {pt(x - rx, y)} Z")
    lid_y = y - ry + 2 * ry * lid
    fig.raw(f'<path d="{white}" fill="#fff"/>')
    fig.inside(white,
               f'<ellipse cx="{f(x)}" cy="{f(y + 0.4)}" rx="{f(rx * 0.84)}" ry="{f(ry * 0.92)}" fill="url(#{uid})"/>'
               f'<ellipse cx="{f(x)}" cy="{f(y + 0.3)}" rx="{f(rx * 0.36)}" ry="{f(ry * 0.46)}" fill="#2a1c36"/>'
               f'<circle cx="{f(x - rx * 0.34)}" cy="{f(y - ry * 0.36)}" r="{f(rx * 0.36)}" fill="#fff"/>'
               f'<circle cx="{f(x + rx * 0.36)}" cy="{f(y + ry * 0.46)}" r="{f(rx * 0.18)}" fill="#fff"/>'
               + (f'<rect x="{f(x - rx - 1)}" y="{f(y - ry - 1)}" width="{f(2 * rx + 2)}" '
                  f'height="{f(lid_y - (y - ry) + 1)}" fill="{pal["body"]}"/>' if lid else ""))
    if lid:
        fig.line(f"M{pt(x - rx - 0.6, lid_y)} L{pt(x + rx + 0.6, lid_y)}", "#2a1c36", 1.5)
    else:
        fig.line(f"M{pt(x - rx - 0.5, y - ry * 0.3)} Q{pt(x, y - ry * 1.42)} {pt(x + rx + 0.5, y - ry * 0.3)}",
                 "#2a1c36", 1.5)
    if lashes:
        ex = x + lashes * (rx + 0.4)
        fig.line(f"M{pt(ex, y - ry * 0.3)} L{pt(ex + lashes * 2.4, y - ry * 0.95)}", "#2a1c36", 1.3)


def blush(fig, x, y, apart):
    for side in (-1, 1):
        fig.raw(f'<ellipse cx="{f(x + side * apart)}" cy="{f(y)}" rx="2.7" ry="1.5" fill="{BLUSH}" '
                f'fill-opacity="0.62"/>')


def smile(fig, x, y, half=1.7, depth=1.5):
    fig.line(f"M{pt(x - half, y)} Q{pt(x, y + depth)} {pt(x + half, y)}", fig.pal["face"], 1.1)


def gem(fig, x, y, r, fill):
    fig.raw(f'<circle cx="{f(x)}" cy="{f(y)}" r="{f(r)}" fill="{fill}" stroke="{fig.pal["line"]}" '
            f'stroke-width="0.9"/>')
    fig.raw(f'<circle cx="{f(x - r * 0.3)}" cy="{f(y - r * 0.3)}" r="{f(r * 0.3)}" fill="#fff"/>')


# --- the six pieces -----------------------------------------------------------

def pawn(colour):
    fig = Figure(PALETTES[colour])
    pedestal(fig, 19.5, 7.0, 64.8, 11.6)
    fig.shape(circle(50, 47.2, 14.2))
    fig.shine(42.4, 39.8, 1.7, 3.8)
    # Wide-eyed, and a little surprised to be here.
    eye(fig, "l", 44.6, 47.6, 3.1, 4.3)
    eye(fig, "r", 55.4, 47.6, 3.1, 4.3)
    blush(fig, 50, 53.2, 8.8)
    fig.raw(f'<ellipse cx="50" cy="54.2" rx="1.0" ry="1.25" fill="{fig.pal["face"]}"/>')
    return fig.svg()


def rook(colour):
    fig = Figure(PALETTES[colour])
    # The shaft, straight-sided as a tower's is.
    fig.shape(symmetric([("M", 14.8, 58.0), ("Q", 15.6, 73.0, 19.4, 85.5)]))
    foot(fig, 23.5)
    fig.shape(symmetric([("M", 17.0, 58.8), ("Q", 19.0, 58.8, 19.0, 56.8), ("Q", 19.0, 54.8, 17.0, 54.8)]))
    # The turret: corbelled out, and three merlons along the top.
    turret = symmetric([("M", 14.6, 55.0), ("Q", 15.2, 50.0, 19.6, 48.2), ("L", 19.6, 17.0), ("L", 13.2, 17.0),
                        ("L", 13.2, 23.4), ("L", 5.6, 23.4), ("L", 5.6, 17.0), ("L", 0, 17.0)])
    fig.shape(turret)
    fig.shine(34.6, 31.0, 1.5, 4.2, 0)
    fig.line(f"M{pt(30.4, 27.6)} L{pt(69.6, 27.6)}", width=1.0)
    # Stern: level lids, brows drawn down toward the middle, a straight mouth.
    eye(fig, "l", 42.6, 38.0, 3.5, 3.9, lid=0.34)
    eye(fig, "r", 57.4, 38.0, 3.5, 3.9, lid=0.34)
    fig.line(f"M{pt(38.2, 31.4)} L{pt(46.4, 33.8)} M{pt(61.8, 31.4)} L{pt(53.6, 33.8)}", fig.pal["face"], 1.6)
    blush(fig, 50, 43.6, 11.4)
    fig.line(f"M{pt(48.0, 44.8)} L{pt(52.0, 44.8)}", fig.pal["face"], 1.2)
    return fig.svg()


def knight(colour):
    fig = Figure(PALETTES[colour])
    pal = fig.pal

    def h(x, y):
        # The head is drawn at one size and stood on the foot at another.
        return pt(AXIS + (x - AXIS) * 1.1 * 0.92, 84.4 + (y - 84.4) * 0.92)

    # The mane: spikes of hair down the crest, in the side's colour.
    mane = [(46.5, 27.0), (54.6, 20.6), (56.0, 29.2), (64.6, 26.6), (62.2, 36.4), (71.0, 36.0), (64.2, 45.6),
            (73.4, 47.6), (65.6, 55.6), (74.6, 59.6), (66.2, 66.0), (74.0, 71.6), (65.8, 76.4), (65.4, 84.4),
            (56.0, 84.4), (54.0, 60.0), (50.0, 40.0)]
    fig.shape("M" + " L".join(h(x, y) for x, y in mane) + " Z", pal["accent"], pal["accent_shade"])
    head = (f"M{h(33.4, 84.4)} "
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
            f"C{h(65.0, 56.4)} {h(65.6, 70.0)} {h(64.4, 84.4)} Z")
    fig.shape(head)
    foot(fig, 25.5)
    # The inside of the ear, the nostril, and a mouth turned up at the corner.
    fig.line(f"M{h(42.4, 26.6)} L{h(43.0, 22.8)}", width=1.0)
    nx, ny = (float(v) for v in h(20.8, 59.0).split(","))
    fig.raw(f'<ellipse cx="{f(nx)}" cy="{f(ny)}" rx="1.1" ry="0.75" fill="{pal["face"]}" '
            f'transform="rotate(-50 {f(nx)} {f(ny)})"/>')
    fig.line(f"M{h(19.0, 63.6)} Q{h(23.0, 65.4)} {h(27.0, 61.4)}", pal["face"], 1.1)
    # One large bright eye, and the colour in its cheek.
    ex, ey = (float(v) for v in h(34.4, 40.4).split(","))
    eye(fig, "e", ex, ey, 3.9, 4.9)
    bx, by = (float(v) for v in h(33.6, 50.0).split(","))
    fig.raw(f'<ellipse cx="{f(bx)}" cy="{f(by)}" rx="3.0" ry="1.6" fill="{BLUSH}" fill-opacity="0.62"/>')
    return fig.svg(widen=1.0)


def bishop(colour):
    fig = Figure(PALETTES[colour])
    pal = fig.pal
    pedestal(fig, 21.5, 7.6, 66.5, 13.0)
    fig.shape(circle(50, 26.4, 3.3))
    mitre = symmetric([("M", 6.6, 63.2), ("C", 14.8, 60.6, 17.6, 53.4, 16.0, 46.6),
                       ("C", 14.0, 38.6, 6.4, 34.6, 0, 28.6)])
    fig.shape(mitre)
    fig.shine(39.6, 44.6, 1.6, 4.0)
    # The cut of the mitre: a slit sawn down into it from the right.
    fig.inside(mitre, f'<path d="M{pt(60.6, 33.6)} L{pt(54.2, 42.6)}" fill="none" stroke="{pal["line"]}" '
                      f'stroke-width="2.2"/>')
    # Serene: the eyes are shut, and smiling.
    for x in (43.4, 56.6):
        fig.line(f"M{pt(x - 3.4, 52.4)} Q{pt(x, 47.6)} {pt(x + 3.4, 52.4)}", pal["face"], 1.6)
    blush(fig, 50, 55.4, 10.4)
    smile(fig, 50, 56.0)
    return fig.svg()


def queen(colour):
    fig = Figure(PALETTES[colour])
    pal = fig.pal
    pedestal(fig, 23.5, 8.4, 60.0, 14.6)
    # Behind the coronet: the jewel on top of it.
    fig.shape(circle(50, 14.0, 3.9), pal["accent"], pal["accent_shade"])
    # The coronet: five points, in gold.
    fig.shape(symmetric([("M", 13.6, 35.6), ("L", 18.8, 21.4), ("L", 12.8, 28.4), ("L", 9.4, 18.6),
                         ("L", 4.7, 27.4), ("L", 0, 17.4)]), GOLD, GOLD_SHADE)
    head = symmetric([("M", 7.6, 57.0), ("C", 9.6, 50.0, 19.6, 48.8, 19.6, 40.8),
                      ("C", 19.6, 36.4, 17.0, 34.2, 14.4, 33.0)])
    fig.shape(head)
    fig.shine(36.4, 40.2, 1.6, 3.8)
    # All eyelashes, and sure of herself.
    eye(fig, "l", 42.8, 43.2, 3.4, 4.2, lashes=-1)
    eye(fig, "r", 57.2, 43.2, 3.4, 4.2, lashes=1)
    blush(fig, 50, 48.8, 11.6)
    smile(fig, 50, 49.8, 2.0, 1.8)
    return fig.svg()


def king(colour):
    fig = Figure(PALETTES[colour])
    pal = fig.pal
    pedestal(fig, 24.5, 9.0, 58.0, 15.6)
    # The cross stands behind the crown's dome.
    fig.shape(f"M{pt(48.2, 18.6)} L{pt(48.2, 12.2)} L{pt(44.4, 12.2)} L{pt(44.4, 8.6)} L{pt(48.2, 8.6)} "
              f"L{pt(48.2, 5.4)} L{pt(51.8, 5.4)} L{pt(51.8, 8.6)} L{pt(55.6, 8.6)} L{pt(55.6, 12.2)} "
              f"L{pt(51.8, 12.2)} L{pt(51.8, 18.6)} Z", GOLD, GOLD_SHADE, width=1.5)
    # The crown: a band that flares, closed over with a dome.
    crown = symmetric([("M", 14.2, 32.6), ("L", 17.8, 22.8), ("Q", 18.4, 20.4, 16.0, 19.8),
                       ("C", 12.0, 18.6, 6.0, 16.8, 3.0, 16.4)])
    fig.shape(crown, GOLD, GOLD_SHADE)
    fig.line(f"M{pt(33.4, 24.6)} Q{pt(50, 27.6)} {pt(66.6, 24.6)}", width=1.0)
    gem(fig, 50, 29.2, 1.9, pal["accent"])
    head = symmetric([("M", 8.6, 55.0), ("C", 10.8, 48.0, 20.4, 46.6, 20.4, 38.6),
                      ("C", 20.4, 34.0, 17.6, 31.8, 15.0, 30.6)])
    fig.shape(head)
    fig.shine(35.8, 38.4, 1.6, 3.8)
    # Calm: heavy brows, lids half down, and a small moustache.
    eye(fig, "l", 42.6, 41.4, 3.4, 4.0, lid=0.2)
    eye(fig, "r", 57.4, 41.4, 3.4, 4.0, lid=0.2)
    fig.line(f"M{pt(38.6, 35.6)} L{pt(46.2, 35.2)} M{pt(61.4, 35.6)} L{pt(53.8, 35.2)}", pal["face"], 1.8)
    blush(fig, 50, 46.8, 12.0)
    fig.line(f"M{pt(50, 47.6)} Q{pt(47.4, 50.2)} {pt(44.4, 47.8)} M{pt(50, 47.6)} Q{pt(52.6, 50.2)} "
             f"{pt(55.6, 47.8)}", pal["face"], 1.5)
    return fig.svg()


PIECES = {"P": pawn, "R": rook, "N": knight, "B": bishop, "Q": queen, "K": king}


def svg(letter, colour):
    return PIECES[letter](colour)


def main():
    kit.write_set("anime", svg)
    if "--sheet" in sys.argv:
        kit.sheet("anime", kit.ROOT / "docs" / "anime-pieces.png")


if __name__ == "__main__":
    main()
