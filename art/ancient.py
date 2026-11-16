#!/usr/bin/env python3
"""Draws the ancient piece set.

    python3 art/ancient.py            # writes art/ancient/*.svg and the PNGs the game loads
    python3 art/ancient.py --sheet    # also docs/ancient-pieces.png: every piece on every board

Chess before it was chess: chaturanga, the game of the four arms of an Indian
army, and the king they fight for. The pieces are that army, carved small:

    king     the raja, cross-legged on his throne under the royal parasol
    queen    the mantri, his counsellor: a tall robed figure with a staff
    rook     the ratha, a war chariot: one great wheel under a tiered canopy
    bishop   the gaja, an elephant with its driver on its neck
    knight   the ashva, a horse and its rider
    pawn     the padati, a foot soldier with shield and spear

They are carved the way early figures are: stocky, front-on or in flat
profile, the detail cut in with a point rather than modelled. One side is old
ivory, the other blackened wood; both carry what is left of their paint — red
ochre on a cloth or a shield, a little gold on a crown.

Size follows rank, as it does in old carving, where the more a figure matters
the larger it is drawn: foot soldier, horse, elephant, chariot, counsellor,
king. See art/DESIGN.md for the thinking.
"""

import sys

import kit
from kit import AXIS, f, pt

FLOOR = 91.0
GROUND = 85.8   # the top of the slab every figure stands on

LINE = 1.6      # the outline
CUT = 0.9       # what is scratched into a figure

PALETTES = {
    # Old ivory.
    "w": dict(body=("#fcf6e6", "#ece1c6", "#c7b58c"), far="#bcaa82", line="#46321f", cut="#7a5c3a",
              cut_opacity=0.9, dark="#46321f", tusk="#fffaf0", rim=None),
    # Wood gone black with age and handling.
    "b": dict(body=("#5e554c", "#37312c", "#1a1714"), far="#1c1916", line="#0c0a09", cut="#a39378",
              cut_opacity=0.75, dark="#e0d4b8", tusk="#efe5cf", rim="#d3c29a"),
}
RED = ("#c9583c", "#b0452e", "#83301f")     # red ochre
GOLD = ("#e2bd66", "#cba046", "#9c7427")    # what is left of the gilding


def smooth(points, k=1.0):
    """A closed outline through points, rounded the way a carver's knife
    rounds things. A point given as (x, y, 'c') is a corner and stays sharp."""
    pts = [(p[0], p[1], len(p) > 2) for p in points]
    n = len(pts)
    d = f"M{pt(pts[0][0], pts[0][1])}"
    for i in range(n):
        p0, p1, p2, p3 = pts[i - 1], pts[i], pts[(i + 1) % n], pts[(i + 2) % n]
        c1 = (p1[0], p1[1]) if p1[2] else (p1[0] + (p2[0] - p0[0]) * k / 6, p1[1] + (p2[1] - p0[1]) * k / 6)
        c2 = (p2[0], p2[1]) if p2[2] else (p2[0] - (p3[0] - p1[0]) * k / 6, p2[1] - (p3[1] - p1[1]) * k / 6)
        d += f" C{pt(*c1)} {pt(*c2)} {pt(p2[0], p2[1])}"
    return d + " Z"


def poly(points):
    return "M" + " L".join(pt(x, y) for x, y in points) + " Z"


def disc(x, y, r):
    return f"M{pt(x - r, y)} A{f(r)},{f(r)} 0 1 0 {pt(x + r, y)} A{f(r)},{f(r)} 0 1 0 {pt(x - r, y)} Z"


class Carving:
    def __init__(self, colour):
        self.pal = PALETTES[colour]
        self.out = []

    def shape(self, d, kind="body", width=LINE):
        fill = {"body": "url(#body)", "red": "url(#red)", "gold": "url(#gold)", "far": self.pal["far"],
                "tusk": self.pal["tusk"]}[kind]
        self.out.append(f'<path d="{d}" fill="{fill}" stroke="{self.pal["line"]}" stroke-width="{f(width)}" '
                        f'stroke-linejoin="round"/>')

    def cut(self, d, width=CUT):
        self.out.append(f'<path d="{d}" fill="none" stroke="{self.pal["cut"]}" '
                        f'stroke-opacity="{self.pal["cut_opacity"]}" stroke-width="{f(width)}" '
                        f'stroke-linecap="round" stroke-linejoin="round"/>')

    def ink(self, d, width=CUT):
        """A line in the colour of the outline: an edge rather than a scratch."""
        self.out.append(f'<path d="{d}" fill="none" stroke="{self.pal["line"]}" stroke-width="{f(width)}" '
                        f'stroke-linecap="round" stroke-linejoin="round"/>')

    def dot(self, x, y, r):
        self.out.append(f'<circle cx="{f(x)}" cy="{f(y)}" r="{f(r)}" fill="{self.pal["dark"]}"/>')

    def slab(self, half):
        """What the figure stands on."""
        self.shape(f"M{pt(AXIS - half + 1.6, GROUND)} L{pt(AXIS + half - 1.6, GROUND)} "
                   f"Q{pt(AXIS + half, GROUND)} {pt(AXIS + half, GROUND + 1.6)} L{pt(AXIS + half, FLOOR - 1.2)} "
                   f"Q{pt(AXIS + half, FLOOR)} {pt(AXIS + half - 1.2, FLOOR)} L{pt(AXIS - half + 1.2, FLOOR)} "
                   f"Q{pt(AXIS - half, FLOOR)} {pt(AXIS - half, FLOOR - 1.2)} L{pt(AXIS - half, GROUND + 1.6)} "
                   f"Q{pt(AXIS - half, GROUND)} {pt(AXIS - half + 1.6, GROUND)} Z")

    def face(self, x, y, apart=1.9):
        """Two eyes: all the face an early carving has."""
        self.dot(x - apart, y, 0.8)
        self.dot(x + apart, y, 0.8)

    def svg(self, half):
        def grad(name, stops):
            return (f'<linearGradient id="{name}" x1="0.1" y1="0" x2="0.9" y2="1">'
                    f'<stop offset="0" stop-color="{stops[0]}"/><stop offset="0.45" stop-color="{stops[1]}"/>'
                    f'<stop offset="1" stop-color="{stops[2]}"/></linearGradient>')
        head = [kit.svg_open(), "<defs>", grad("body", self.pal["body"]), grad("red", RED), grad("gold", GOLD),
                '<filter id="soft" x="-20%" y="-200%" width="140%" height="500%">'
                '<feGaussianBlur stdDeviation="1.2"/></filter>',
                # The black side is worn pale along every edge, which is what
                # keeps it from sinking into a dark square.
                '<filter id="rim" x="-10%" y="-10%" width="120%" height="120%">'
                '<feMorphology in="SourceAlpha" operator="dilate" radius="0.75" result="wide"/>'
                f'<feFlood flood-color="{self.pal["rim"] or "#000"}" flood-opacity="0.85"/>'
                '<feComposite in2="wide" operator="in" result="edge"/>'
                '<feMerge><feMergeNode in="edge"/><feMergeNode in="SourceGraphic"/></feMerge></filter>',
                "</defs>",
                f'<ellipse cx="{AXIS}" cy="{FLOOR - 0.2}" rx="{half - 0.5}" ry="2.2" fill="#000" opacity="0.3" '
                f'filter="url(#soft)"/>',
                '<g filter="url(#rim)">' if self.pal["rim"] else "<g>"]
        return "\n".join(head + self.out + ["</g>", "</svg>"])


# --- the army -----------------------------------------------------------------

def pawn(colour):
    """The padati: a foot soldier, front-on, shield before him and spear upright."""
    c = Carving(colour)
    # The spear stands behind his hand.
    c.shape(poly([(61.4, GROUND), (61.4, 41.0), (63.2, 41.0), (63.2, GROUND)]), width=1.2)
    c.shape(poly([(62.3, 32.2), (65.5, 40.0), (62.3, 43.0), (59.1, 40.0)]), "gold", 1.2)
    c.shape(poly([(42.6, 66.0), (57.4, 66.0), (60.6, GROUND), (39.4, GROUND)]))          # the cloth round his legs
    c.cut(f"M{pt(50, 71.0)} L{pt(50, GROUND - 1)}")
    c.shape(f"M{pt(40.6, 55.4)} Q{pt(50, 51.0)} {pt(59.4, 55.4)} L{pt(58.4, 68.0)} L{pt(41.6, 68.0)} Z")
    c.shape(poly([(41.9, 64.6), (58.1, 64.6), (58.3, 68.2), (41.7, 68.2)]), "red", 1.2)   # a sash
    c.shape(disc(50, 47.0, 6.2))
    c.shape(f"M{pt(43.6, 45.8)} Q{pt(50, 32.6)} {pt(56.4, 45.8)} Q{pt(50, 42.8)} {pt(43.6, 45.8)} Z")  # helmet
    c.dot(47.6, 48.4, 1.0)
    c.dot(52.4, 48.4, 1.0)
    c.shape(disc(62.3, 60.0, 2.5), width=1.2)                                             # the hand on the spear
    c.shape(disc(40.2, 63.4, 8.4), "red")                                                 # the shield
    c.shape(disc(40.2, 63.4, 2.6), "gold", 1.1)
    c.slab(19.5)
    return c.svg(19.5)


def knight(colour):
    """The ashva: a horse in profile with one foreleg raised, and its rider."""
    c = Carving(colour)
    # The legs on the far side, and the tail.
    c.shape(smooth([(37.5, 59.6), (31.0, 62.6), (27.6, 69.6), (29.0, 76.6, 1), (32.8, 76.0, 1), (32.4, 70.6),
                    (35.0, 67.0), (39.6, 66.0)]), "far", 1.3)
    c.shape(smooth([(62.6, 63.6, 1), (60.4, 74.0), (59.6, GROUND, 1), (64.6, GROUND, 1), (65.2, 76.0),
                    (68.4, 66.6, 1)], 0.6), "far", 1.3)
    c.shape(smooth([(76.6, 49.6), (81.8, 55.6), (83.2, 67.6), (80.6, 78.0, 1), (79.0, 70.0), (79.6, 60.0),
                    (75.8, 53.0)]), width=1.3)
    c.shape(smooth([
        (70.6, GROUND, 1), (76.2, GROUND, 1), (75.6, 76.0), (77.6, 68.0), (75.6, 60.0), (77.6, 52.0),     # hind leg, rump
        (74.0, 46.6), (66.0, 45.6), (56.0, 46.6), (49.0, 44.0),                                          # back
        (44.4, 37.0), (40.0, 30.6), (36.6, 26.6), (35.4, 22.0, 1), (33.0, 26.6),                         # crest, ear
        (30.0, 30.2), (25.6, 37.6), (21.6, 44.0), (20.4, 47.8), (23.0, 50.4), (27.6, 48.6), (32.0, 46.2),  # face
        (34.6, 49.4), (35.2, 56.0), (36.6, 64.0), (36.0, 74.0), (35.6, GROUND, 1), (41.2, GROUND, 1),    # chest, foreleg
        (41.2, 74.0), (42.6, 65.4), (50.0, 63.6), (59.0, 64.0), (65.0, 66.6), (67.2, 71.4), (69.6, 77.4),  # belly
    ]))
    for x, y, dx, dy in ((43.4, 35.6, 3.2, -1.4), (41.0, 32.0, 3.0, -1.8), (38.6, 28.8, 2.8, -2.0),
                         (46.0, 39.6, 3.2, -1.0)):
        c.cut(f"M{pt(x, y)} l{f(dx)},{f(dy)}")                                           # the mane
    c.dot(30.6, 33.6, 1.0)
    c.dot(22.6, 46.6, 0.7)
    c.cut(f"M{pt(21.2, 48.6)} L{pt(25.6, 47.6)}")
    c.ink(f"M{pt(24.6, 40.6)} L{pt(30.4, 45.4)} M{pt(27.6, 43.0)} Q{pt(40, 50)} {pt(52.6, 44.4)}", 1.0)   # bridle, rein
    c.shape(poly([(48.6, 45.4), (64.6, 46.4), (65.6, 57.0), (49.6, 56.6)]), "red", 1.3)   # the saddle-cloth
    c.cut(f"M{pt(50.6, 54.6)} L{pt(64.2, 55.0)}")
    # The rider.
    c.shape(disc(62.0, 40.6, 4.8), "red", 1.3)                                            # his shield, slung behind
    c.shape(disc(62.0, 40.6, 1.5), "gold", 1.0)
    c.shape(poly([(54.6, 45.6), (59.0, 45.6), (57.8, 60.6), (53.4, 60.6)]), width=1.3)    # a leg
    c.shape(f"M{pt(52.6, 46.4)} L{pt(53.6, 34.6)} Q{pt(57, 32.0)} {pt(60.4, 34.6)} L{pt(61.4, 46.6)} Z")
    c.shape(disc(57.0, 29.4, 3.6))
    c.shape(f"M{pt(53.3, 28.8)} Q{pt(57, 19.6)} {pt(60.7, 28.8)} Q{pt(57, 27.0)} {pt(53.3, 28.8)} Z")
    c.face(56.2, 30.2, 1.4)
    c.slab(26.5)
    return c.svg(26.5)


def bishop(colour):
    """The gaja: an elephant in profile, its driver on its neck."""
    c = Carving(colour)
    c.shape(f"M{pt(77.4, 49.0)} Q{pt(81.8, 56.0)} {pt(80.8, 66.6)} L{pt(79.0, 66.6)} Q{pt(79.8, 57.0)} "
            f"{pt(76.4, 51.0)} Z", width=1.3)                                             # the tail
    c.shape(smooth([
        (75.6, GROUND, 1), (76.6, 74.0), (78.6, 62.0), (77.0, 50.0), (71.0, 42.6), (62.0, 40.6), (53.0, 40.0),   # rump, back
        (46.0, 37.0), (40.0, 33.6), (33.6, 35.0), (29.0, 41.0),                                               # head
        (26.6, 50.0), (24.6, 61.0), (24.0, 71.0), (26.0, 77.6), (30.2, 79.2), (31.4, 76.0), (28.8, 73.6),     # trunk
        (28.8, 66.0), (30.6, 58.6), (32.8, 55.4), (35.2, 58.6), (37.0, 62.6),                                 # mouth, chest
        (36.6, 72.0), (36.2, GROUND, 1), (47.2, GROUND, 1), (46.6, 72.0), (47.8, 66.6),                       # foreleg
        (56.0, 65.6), (63.4, 66.6), (64.6, 74.0), (64.6, GROUND, 1),                                          # belly, hind leg
    ]))
    for y in (58.6, 63.6, 68.6):
        c.cut(f"M{pt(25.4, y)} Q{pt(27.4, y + 1.2)} {pt(29.4, y)}")                       # the trunk's rings
    for x in (38.6, 41.6, 44.6, 67.0, 70.0, 73.0):
        c.cut(f"M{pt(x - 0.9, GROUND - 0.6)} Q{pt(x, GROUND - 2.6)} {pt(x + 0.9, GROUND - 0.6)}")   # toenails
    c.shape(smooth([(39.0, 37.6), (46.6, 36.6), (50.2, 43.0), (48.6, 52.0), (43.0, 56.6), (39.6, 50.0),
                    (38.0, 43.0)]), width=1.3)                                            # the ear
    c.cut(f"M{pt(41.6, 40.6)} Q{pt(46.6, 44.6)} {pt(43.6, 52.6)}")
    c.dot(33.8, 42.6, 1.1)
    c.shape(f"M{pt(32.2, 53.4)} Q{pt(26.0, 55.4)} {pt(22.6, 61.8)} Q{pt(28.6, 60.6)} {pt(33.2, 57.0)} Z",
            "tusk", 1.2)
    c.shape(poly([(54.6, 40.2), (72.6, 43.2), (74.6, 57.6), (55.6, 56.0)]), "red", 1.3)   # the cloth on its back
    c.ink(f"M{pt(56.6, 53.6)} L{pt(73.4, 55.0)}", 0.9)
    for x in (58.6, 62.6, 66.6, 70.6):
        c.shape(disc(x, 48.0 + (x - 58.6) * 0.12, 1.0), "gold", 0.8)
    # The driver, with his goad.
    c.ink(f"M{pt(45.6, 34.6)} L{pt(38.0, 27.6)} M{pt(38.0, 27.6)} Q{pt(36.2, 28.6)} {pt(37.0, 30.8)}", 1.1)
    c.shape(f"M{pt(44.6, 39.6)} L{pt(45.4, 30.6)} Q{pt(48.2, 28.4)} {pt(51.0, 30.6)} L{pt(52.0, 39.8)} Z")
    c.shape(disc(48.2, 26.2, 3.3))
    c.shape(f"M{pt(44.8, 25.6)} Q{pt(48.2, 17.6)} {pt(51.6, 25.6)} Q{pt(48.2, 23.8)} {pt(44.8, 25.6)} Z",
            "red", 1.2)
    c.face(47.4, 27.0, 1.3)
    c.slab(28.0)
    return c.svg(28.0)


def rook(colour):
    """The ratha: a war chariot — one great wheel, a car, and over it a canopy
    in tiers, like the roof of a shrine, flying a pennant."""
    c = Carving(colour)
    pal = c.pal
    # The pole the horses are yoked to.
    c.shape(poly([(31.0, 56.6), (20.6, 62.0), (20.0, 60.2), (30.2, 54.8)]), width=1.2)
    c.shape(poly([(19.2, 57.6), (21.8, 57.0), (22.8, 64.2), (20.2, 64.8)]), width=1.2)
    # The canopy's posts, and the man who rides under it.
    for x in (35.0, 62.4):
        c.shape(poly([(x, 38.0), (x + 2.6, 38.0), (x + 2.6, 51.4), (x, 51.4)]), width=1.3)
    c.shape(f"M{pt(44.4, 51.2)} Q{pt(50, 44.6)} {pt(55.6, 51.2)} Z", width=1.3)
    c.shape(disc(50, 43.4, 3.3), width=1.3)
    c.face(50, 44.0, 1.3)
    # The car, its front swept up.
    c.shape(smooth([(29.6, 62.6, 1), (70.4, 62.6, 1), (71.4, 51.0, 1), (34.0, 51.0), (30.2, 46.4), (26.4, 47.4, 1),
                    (27.6, 54.0)], 0.8))
    c.shape(poly([(37.6, 53.6), (68.0, 53.6), (67.6, 60.0), (37.2, 60.0)]), "red", 1.1)
    for x in (43.6, 50.0, 56.4, 62.8):
        c.shape(disc(x, 56.8, 1.2), "gold", 0.8)
    # The canopy: an eave, two tiers, a finial, a pennant.
    c.shape(poly([(50.7, 14.8), (60.0, 16.8), (50.7, 19.4)]), "red", 1.1)
    c.shape(poly([(49.3, 14.2), (50.7, 14.2), (50.7, 20.0), (49.3, 20.0)]), width=1.0)
    c.shape(disc(50, 21.4, 2.6), "gold", 1.2)
    c.shape(poly([(41.6, 28.8), (58.4, 28.8), (54.4, 23.6), (45.6, 23.6)]))
    c.shape(poly([(33.6, 34.4), (66.4, 34.4), (61.0, 28.8), (39.0, 28.8)]))
    c.shape(poly([(28.8, 38.0), (71.2, 38.0), (69.0, 34.2), (31.0, 34.2)]), "red", 1.3)
    c.cut(f"M{pt(42.0, 31.6)} L{pt(58.0, 31.6)}")
    # The wheel: a rim, eight spokes, a hub. What is between the spokes is air.
    wx, wy, r = 50.0, 73.6, 11.4
    spokes = " ".join(f"M{pt(wx, wy)} L{pt(wx + r * 0.84 * cx, wy + r * 0.84 * cy)}"
                      for cx, cy in ((1, 0), (0.707, 0.707), (0, 1), (-0.707, 0.707), (-1, 0), (-0.707, -0.707),
                                     (0, -1), (0.707, -0.707)))
    c.out.append(f'<path d="{spokes}" fill="none" stroke="{pal["line"]}" stroke-width="3.4" stroke-linecap="round"/>')
    c.out.append(f'<path d="{spokes}" fill="none" stroke="{pal["body"][1]}" stroke-width="1.6" '
                 f'stroke-linecap="round"/>')
    c.out.append(f'<circle cx="{wx}" cy="{wy}" r="{f(r - 1.3)}" fill="none" stroke="{pal["line"]}" '
                 f'stroke-width="{f(2.6 + 2 * 0.8)}"/>')
    c.out.append(f'<circle cx="{wx}" cy="{wy}" r="{f(r - 1.3)}" fill="none" stroke="url(#body)" stroke-width="2.6"/>')
    c.shape(disc(wx, wy, 2.9), "gold", 1.2)
    c.slab(26.0)
    return c.svg(26.0)


def queen(colour):
    """The mantri: the king's counsellor, standing, in a long robe and a great
    turban, his staff of office in his hand."""
    c = Carving(colour)
    # The staff.
    c.shape(poly([(31.6, GROUND), (31.6, 24.0), (33.4, 24.0), (33.4, GROUND)]), width=1.2)
    c.shape(disc(32.5, 20.6, 3.6), "gold", 1.2)
    c.shape(poly([(32.5, 12.6), (34.4, 17.0), (30.6, 17.0)]), "gold", 1.1)
    # The robe.
    c.shape(f"M{pt(38.6, 40.0)} Q{pt(50, 33.6)} {pt(61.4, 40.0)} L{pt(66.4, GROUND)} L{pt(33.6, GROUND)} Z")
    c.shape(poly([(38.4, 40.4), (43.6, 37.6), (62.6, 62.0), (61.6, 68.6)]), "red", 1.2)   # a shawl across it
    c.shape(poly([(36.6, 63.4), (63.4, 63.4), (63.9, 67.0), (36.1, 67.0)]), "gold", 1.1)  # a belt
    c.cut(f"M{pt(50, 69.6)} L{pt(50, GROUND - 1)} M{pt(42.6, 70.6)} L{pt(41.0, GROUND - 1)} "
          f"M{pt(57.4, 70.6)} L{pt(59.0, GROUND - 1)}")                                    # the folds of the skirt
    c.shape(f"M{pt(36.6, 52.0)} Q{pt(33.0, 50.0)} {pt(32.6, 46.0)} Q{pt(36.6, 44.6)} {pt(39.6, 47.6)} Z",
            width=1.2)                                                                    # the hand on the staff
    # The head: a beard, and the turban with its jewel and plume.
    c.shape(disc(50, 31.4, 5.5))
    c.out.append(f'<path d="M{pt(46.4, 34.0)} Q{pt(50, 41.6)} {pt(53.6, 34.0)} Q{pt(50, 36.0)} {pt(46.4, 34.0)} Z" '
                 f'fill="{c.pal["dark"]}"/>')
    c.face(50, 31.6, 2.0)
    c.shape(poly([(50, 16.2), (47.4, 12.0), (50, 9.6), (52.6, 12.0)]), "red", 1.1)
    c.shape(f"M{pt(42.6, 30.2)} C{pt(40.2, 20.6)} {pt(45.6, 14.8)} {pt(50, 14.8)} "
            f"C{pt(54.4, 14.8)} {pt(59.8, 20.6)} {pt(57.4, 30.2)} Q{pt(50, 26.4)} {pt(42.6, 30.2)} Z")
    c.cut(f"M{pt(44.0, 24.6)} Q{pt(50, 20.6)} {pt(56.0, 24.6)} M{pt(45.6, 19.6)} Q{pt(50, 17.0)} {pt(54.4, 19.6)}")
    c.shape(disc(50, 25.2, 1.7), "gold", 1.0)
    c.slab(22.5)
    return c.svg(22.5)


def king(colour):
    """The raja: cross-legged on his throne, crowned, under the parasol that
    only a king may sit beneath."""
    c = Carving(colour)
    # The parasol and its pole, behind everything.
    c.shape(poly([(49.2, 16.0), (50.8, 16.0), (50.8, 34.0), (49.2, 34.0)]), width=1.1)
    c.shape(f"M{pt(27.6, 16.4)} Q{pt(50, -3.0)} {pt(72.4, 16.4)} Q{pt(50, 12.4)} {pt(27.6, 16.4)} Z")
    c.cut(f"M{pt(50, 6.6)} L{pt(50, 13.6)} M{pt(40.6, 9.0)} Q{pt(39.0, 12.0)} {pt(38.0, 14.4)} "
          f"M{pt(59.4, 9.0)} Q{pt(61.0, 12.0)} {pt(62.0, 14.4)}")
    c.shape(f"M{pt(27.6, 16.4)} Q{pt(50, 12.4)} {pt(72.4, 16.4)} L{pt(72.0, 19.0)} Q{pt(50, 15.0)} "
            f"{pt(28.0, 19.0)} Z", "red", 1.2)                                            # its fringe
    c.shape(disc(50, 5.4, 2.0), "gold", 1.1)
    # The throne: a back with a rounded head, and the seat.
    c.shape(f"M{pt(33.6, 68.0)} L{pt(33.6, 43.0)} Q{pt(33.6, 31.6)} {pt(50, 31.6)} Q{pt(66.4, 31.6)} "
            f"{pt(66.4, 43.0)} L{pt(66.4, 68.0)} Z")
    c.shape(f"M{pt(37.4, 66.0)} L{pt(37.4, 43.6)} Q{pt(37.4, 35.4)} {pt(50, 35.4)} Q{pt(62.6, 35.4)} "
            f"{pt(62.6, 43.6)} L{pt(62.6, 66.0)} Z", "red", 1.1)
    c.shape(poly([(30.6, 67.6), (69.4, 67.6), (69.4, GROUND), (30.6, GROUND)]))
    c.shape(poly([(30.6, 67.6), (69.4, 67.6), (69.4, 71.4), (30.6, 71.4)]), "gold", 1.1)
    c.cut(f"M{pt(40.6, 74.6)} L{pt(40.6, GROUND - 1.4)} M{pt(50, 74.6)} L{pt(50, GROUND - 1.4)} "
          f"M{pt(59.4, 74.6)} L{pt(59.4, GROUND - 1.4)}")
    # The king: crossed legs, body, arms on his knees.
    c.shape(smooth([(35.6, 64.6), (41.0, 59.6), (50.0, 58.6), (59.0, 59.6), (64.4, 64.6), (59.6, 68.4),
                    (50.0, 67.0), (40.4, 68.4)]))
    c.cut(f"M{pt(43.6, 63.4)} Q{pt(50, 66.0)} {pt(56.4, 63.4)}")
    c.shape(f"M{pt(41.6, 46.6)} Q{pt(50, 42.4)} {pt(58.4, 46.6)} L{pt(57.2, 61.0)} L{pt(42.8, 61.0)} Z")
    c.shape(poly([(44.6, 46.0), (55.4, 46.0), (53.6, 50.4), (46.4, 50.4)]), "gold", 1.0)  # a collar of gold
    for side in (-1, 1):
        x = lambda v: 50 + side * v
        c.shape(smooth([(x(8.2), 47.4), (x(11.6), 52.6), (x(11.0), 60.6), (x(7.6), 61.6), (x(7.6), 53.6)]),
                width=1.2)
    c.shape(disc(50, 39.4, 5.3))
    c.face(50, 39.8, 2.0)
    # The crown: three tiers, narrowing.
    c.shape(poly([(44.4, 36.4), (55.6, 36.4), (54.2, 31.4), (53.0, 31.4), (52.0, 27.0), (51.0, 27.0), (50, 22.6),
                  (49.0, 27.0), (48.0, 27.0), (47.0, 31.4), (45.8, 31.4)]), "gold", 1.2)
    c.slab(24.5)
    return c.svg(24.5)


PIECES = {"P": pawn, "R": rook, "N": knight, "B": bishop, "Q": queen, "K": king}


def svg(letter, colour):
    return PIECES[letter](colour)


def main():
    kit.write_set("ancient", svg)
    if "--sheet" in sys.argv:
        kit.sheet("ancient", kit.ROOT / "docs" / "ancient-pieces.png")


if __name__ == "__main__":
    main()
