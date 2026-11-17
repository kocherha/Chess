#!/usr/bin/env python3
"""Draws the pixel-art piece set.

    python3 art/pixel.py            # writes art/pixel/*.svg and the PNGs the game loads
    python3 art/pixel.py --sheet    # also docs/pixel-pieces.png: every piece on every board

The pieces as an old game console would have drawn them: on a grid of
32 x 32, in five tones a side and one dark line round everything. The Pixel
board (art/boards.py) is drawn on the same grid, so a pixel of a piece and a
pixel of the square under it are the same size and sit on the same lines.

A piece is given as the shape of its body, row by row. The line round it and
the light on it are not drawn by hand but worked out: light comes from the
left and from above, every ledge casts a shadow on what is under it. Only the
few things that cannot be worked out — an eye, the cut in a mitre — are put
in pixel by pixel.

Sizes follow what a piece is worth, as in the other sets: pawn, then knight
and bishop alike, rook, queen, king. See art/DESIGN.md for the thinking.
"""

import sys

import kit

GRID = 32        # a piece is drawn on GRID x GRID
FLOOR = 28       # the row its line stands on, counted from the top
PIXEL = kit.SIZE // GRID

# Deep shadow, shadow, the colour itself, light, highlight; and the line.
PALETTES = {
    "w": dict(line=(40, 32, 52), tones=[(128, 120, 122), (172, 163, 156), (219, 212, 198), (242, 238, 228),
                                        (255, 255, 255)]),
    "b": dict(line=(14, 12, 20), tones=[(32, 29, 46), (48, 45, 68), (72, 68, 98), (101, 97, 132),
                                        (146, 142, 176)]),
}
DEEP, SHADOW, BASE, LIGHT, SHINE = range(5)

# --- the bodies ----------------------------------------------------------------
#
# A row is either how many pixels the body reaches each side of the middle, or,
# where it is not one piece from side to side, its left half spelt out ('#' is
# body); the right half is the same mirrored. The knight alone is spelt out in
# full, being the only piece that is not the same on both sides.

PAWN = [3, 4, 5, 5, 5, 4, 3,         # the ball
        2, 5,                        # neck, collar
        3, 4, 5, 6, 7,               # the skirt
        9, 10, 10]                   # the foot

ROOK = ["......####....##",
        "......####....##",
        "......####....##",           # the battlements
        10, 10, 8,
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7,  # the walls
        8, 9, 10,
        11, 11, 11]                  # the foot

BISHOP = [1, 2, 2, 1,                # the ball
          2, 3, 4, 5, 6, 7, 7, 6, 5,  # the mitre
          6,                         # the collar
          4, 4, 5, 6,                # the stem
          8, 10, 10]                 # the foot

QUEEN = ["..............##",
         "........###...##",
         "..###...###....#",
         "..###....#.....#",
         "...#.....#.....#",
         "...#.....##...##",
         "...##....##...##",
         "....#...####.###",
         "....##..########",
         "....############",
         ".....###########",          # five points with a ball on each
         10, 9,
         10,                         # the ring under the crown
         6, 5, 5, 5, 6, 7, 8,        # the stem
         10, 11, 11]                 # the foot

KING = ["...............#",
        "...............#",
        "............####",
        "...............#",
        "...............#",           # the cross
        ".......###....##",
        "......#####..###",
        ".....#######.###",
        ".....#######.###",           # two lobes and the stem between them
        11, 11, 10, 9, 8,
        9,                           # the ring under the crown
        6, 5, 5, 5, 5, 6, 7, 8,      # the stem
        10, 11, 11]                  # the foot

KNIGHT = [".............##.................",
          "............####................",
          "...........#######..............",
          "..........##########............",
          ".........#############..........",
          "........###############.........",
          ".......#################........",
          "......###################.......",
          ".....####################.......",
          "....######################......",
          "...#######################......",
          "...########.##############......",
          "...#######...##############.....",
          "....#####...###############.....",
          "...........################.....",
          "..........#################.....",
          ".........##################.....",
          "........####################....",
          ".......#####################....",
          ".....########################...",
          ".....########################..."]

# What cannot be worked out: (column, row from the top of the body, what).
# A "seam" is drawn in the dark line itself; a "mark" too on a white piece,
# but on a black one, where a dark eye would not be seen, in its brightest tone.
DETAILS = {
    "B": [(20, 7, "mark"), (19, 8, "mark"), (18, 9, "mark"), (17, 10, "mark")],           # the cut in the mitre
    "N": [(10, 6, "mark"), (11, 6, "mark"), (10, 7, "mark"),                              # the eye
          (5, 11, "mark"),                                                                # the nostril
          (18, 3, "seam"), (19, 4, "seam"), (20, 5, "seam"), (21, 6, "seam"), (21, 7, "seam"),
          (22, 8, "seam"), (22, 9, "seam"), (23, 10, "seam"), (23, 11, "seam"), (23, 12, "seam"),
          (24, 13, "seam"), (24, 14, "seam"), (24, 15, "seam"), (24, 16, "seam")],        # where the mane lies
}

BODIES = {"P": PAWN, "R": ROOK, "B": BISHOP, "Q": QUEEN, "K": KING, "N": KNIGHT}


def body(rows):
    """The body as a grid of booleans, standing on the floor."""
    grid = [[False] * GRID for _ in range(GRID)]
    top = FLOOR - len(rows)             # the line under the last row is the floor
    middle = GRID // 2
    for index, row in enumerate(rows):
        y = top + index
        if isinstance(row, int):
            for x in range(middle - row, middle + row):
                grid[y][x] = True
        elif len(row) == GRID:
            for x, mark in enumerate(row):
                grid[y][x] = mark == "#"
        else:
            for x, mark in enumerate(row):
                if mark == "#":
                    grid[y][x] = grid[y][GRID - 1 - x] = True
    return grid, top


def paint(letter, colour):
    """One piece as GRID x GRID pixels: None where there is nothing, else an RGB colour."""
    palette = PALETTES[colour]
    inside, top = body(BODIES[letter])

    def at(x, y):
        return 0 <= x < GRID and 0 <= y < GRID and inside[y][x]

    tone = [[None] * GRID for _ in range(GRID)]
    last = max(y for y in range(GRID) if any(inside[y]))
    for y in range(GRID):
        x = 0
        while x < GRID:
            if not inside[y][x]:
                x += 1
                continue
            a = x
            while x < GRID and inside[y][x]:
                x += 1
            b, n = x - 1, x - a
            # Light from the left: a lit edge, a shine beside it, shade on the far side.
            for i in range(a, b + 1):
                tone[y][i] = BASE
            if n >= 2:
                tone[y][a] = LIGHT
            if n >= 6:
                tone[y][a + 1] = SHINE
                tone[y][a], tone[y][a + 2] = LIGHT, LIGHT if n >= 10 else BASE
            if n >= 3:
                tone[y][b] = SHADOW
            if n >= 7:
                tone[y][b - 1] = SHADOW
                tone[y][b] = DEEP
            if n >= 12:
                tone[y][b - 2] = SHADOW
    for y in range(GRID):
        for x in range(GRID):
            if tone[y][x] is None:
                continue
            # A ledge casts a shadow on what is under it; what faces up catches the light.
            under_ledge = at(x, y - 1) and ((not at(x - 1, y) and at(x - 1, y - 1) and at(x - 2, y - 1))
                                            or (not at(x + 1, y) and at(x + 1, y - 1) and at(x + 2, y - 1)))
            wide_above = at(x, y - 1) and sum(inside[y - 1]) >= sum(inside[y]) + 4
            if under_ledge or wide_above:
                tone[y][x] = max(DEEP, tone[y][x] - 1)
            elif not at(x, y - 1) and tone[y][x] < SHINE and (at(x - 1, y) or at(x + 1, y)):
                tone[y][x] += 1
            if y == last:
                tone[y][x] = max(DEEP, tone[y][x] - 1)

    pixels = [[None] * GRID for _ in range(GRID)]
    for y in range(GRID):
        for x in range(GRID):
            if tone[y][x] is not None:
                pixels[y][x] = palette["tones"][tone[y][x]]
            elif any(at(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                pixels[y][x] = palette["line"]          # the line round the body
    for x, y, what in DETAILS.get(letter, ()):
        bright = what == "mark" and colour == "b"
        pixels[top + y][x] = palette["tones"][SHINE] if bright else palette["line"]
    return pixels


def svg(letter, colour):
    """The piece as an SVG: one rectangle for each run of the same colour."""
    pixels = paint(letter, colour)
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {GRID} {GRID}" width="{kit.SIZE}" '
           f'height="{kit.SIZE}" shape-rendering="crispEdges">']
    for y, row in enumerate(pixels):
        x = 0
        while x < GRID:
            if row[x] is None:
                x += 1
                continue
            start, colour_here = x, row[x]
            while x < GRID and row[x] == colour_here:
                x += 1
            out.append(f'<rect x="{start}" y="{y}" width="{x - start}" height="1" '
                       f'fill="#{colour_here[0]:02x}{colour_here[1]:02x}{colour_here[2]:02x}"/>')
    out.append("</svg>")
    return "\n".join(out)


def png(letter, colour, target):
    """The piece as the picture the game loads: every pixel PIXEL x PIXEL, nothing smoothed."""
    from PIL import Image

    image = Image.new("RGBA", (GRID, GRID), (0, 0, 0, 0))
    for y, row in enumerate(paint(letter, colour)):
        for x, rgb in enumerate(row):
            if rgb is not None:
                image.putpixel((x, y), rgb + (255,))
    image.resize((kit.SIZE, kit.SIZE), Image.NEAREST).save(target, optimize=True)


def main():
    svg_dir = kit.ROOT / "art" / "pixel"
    png_dir = kit.CONTENT / "pieces" / "pixel"
    svg_dir.mkdir(parents=True, exist_ok=True)
    png_dir.mkdir(parents=True, exist_ok=True)
    for colour in "wb":
        for letter in "KQRBNP":
            (svg_dir / f"{colour}{letter}.svg").write_text(svg(letter, colour) + "\n", encoding="utf-8")
            png(letter, colour, png_dir / f"{colour}{letter}.png")
    if "--sheet" in sys.argv:
        kit.sheet("pixel", kit.ROOT / "docs" / "pixel-pieces.png")


if __name__ == "__main__":
    main()
