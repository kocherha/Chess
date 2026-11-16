#!/usr/bin/env python3
"""Makes the squares of the boards that are laid from pictures.

    python3 art/boards.py     # writes game/Content/boards/<id>/light.png and dark.png

Needs numpy and Pillow. Everything is generated from a fixed seed, so running
it again gives the same pictures.

Each picture is 512 x 512 and holds four squares, two by two: four different
cuts of the same material. The game picks one of the four for each square of
the board and may turn it over, so that no two neighbours are the same piece
of wood or stone — which is what makes a board look laid rather than printed.

    wood     maple and walnut, grain running across, each square its own veneer
    gothic   limestone and oxblood flags, worn at the edges, with a petal cut
             into every corner: where four flags meet the petals make a
             quatrefoil round a brass stud
    sakura   cream and cherry-blossom pink, flat as a cel, with a few blossoms
             and loose petals fallen on every square
    leather  an old tooled board: vellum and dark calf, each square ruled round
             with a line — gilt on the dark, blind on the pale — foxed, scuffed
             and cracked by a couple of centuries of play

See art/DESIGN.md for the thinking.
"""

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

import kit

CELL = 256      # one square, in pixels
SCALE = 3       # everything is drawn this many times larger and reduced


def noise(rng, shape, rows, cols):
    """Smooth noise: a small random grid stretched to `shape`. More rows than
    columns gives streaks that run across."""
    small = rng.random((rows, cols)).astype(np.float32)
    image = Image.fromarray((small * 255).astype(np.uint8), "L").resize((shape[1], shape[0]), Image.BICUBIC)
    return np.asarray(image, dtype=np.float32) / 255.0 - 0.5


def colour(value, stops):
    """Maps a field of 0..1 through a ramp of colours."""
    value = np.clip(value, 0.0, 1.0)
    xs = np.array([s[0] for s in stops], dtype=np.float32)
    out = np.empty(value.shape + (3,), dtype=np.float32)
    for channel in range(3):
        ys = np.array([s[1][channel] for s in stops], dtype=np.float32)
        out[..., channel] = np.interp(value, xs, ys)
    return out


def save(squares, path):
    """Four squares, each (CELL*SCALE)^2 x 3 floats, as one picture."""
    size = CELL * SCALE
    sheet = np.zeros((size * 2, size * 2, 3), dtype=np.float32)
    for index, square in enumerate(squares):
        row, col = divmod(index, 2)
        sheet[row * size:(row + 1) * size, col * size:(col + 1) * size] = square
    image = Image.fromarray(np.clip(sheet, 0, 255).astype(np.uint8), "RGB")
    image = image.resize((CELL * 2, CELL * 2), Image.LANCZOS)
    path.parent.mkdir(parents=True, exist_ok=True)
    image.save(path, optimize=True)


# --- wood ---------------------------------------------------------------------

def veneer(rng, ramp, contrast, pores, tone):
    """One square of veneer, the grain running across it."""
    size = CELL * SCALE
    shape = (size, size)

    # The growth rings, cut along the trunk: bands that run across and wander
    # only a little — this is straight-grained timber, chosen for a board.
    wander = noise(rng, shape, 4, 3) * 0.05 + noise(rng, shape, 7, 2) * 0.025
    y = np.linspace(0.0, 1.0, size, dtype=np.float32)[:, None] + wander
    rings = (np.sin(y * rng.uniform(40, 56) + rng.uniform(0, 6.28)) * 0.5
             + np.sin(y * rng.uniform(96, 130) + rng.uniform(0, 6.28)) * 0.3)
    grain = (rings * 0.42
             + noise(rng, shape, 90, 2) * 0.8      # broad streaks
             + noise(rng, shape, 240, 3) * 0.6     # fine ones
             + noise(rng, shape, 3, 3) * 0.35)     # the figure: light moving in the wood
    value = 0.5 + grain * contrast + tone

    # Pores: short dark dashes along the grain.
    dashes = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(dashes)
    for _ in range(pores):
        x, py = rng.integers(0, size), rng.integers(0, size)
        length = int(rng.integers(8, 40) * SCALE / 2)
        draw.line([(x, py), (x + length, py + int(rng.integers(-1, 2)))], fill=int(rng.integers(60, 150)), width=SCALE - 1)
    dashes = np.asarray(dashes.filter(ImageFilter.GaussianBlur(SCALE * 0.4)), dtype=np.float32) / 255.0
    value -= dashes * 0.2

    square = colour(value, ramp)

    # The joint: a hairline of shadow where this square meets the next.
    edge = np.ones(shape, dtype=np.float32)
    width = int(SCALE * 1.6)
    edge[:width, :] = edge[-width:, :] = edge[:, :width] = edge[:, -width:] = 0.86
    return square * edge[..., None]


def wood():
    rng = np.random.default_rng(1849)
    maple = [(0.0, (196, 160, 106)), (0.35, (219, 188, 136)), (0.6, (230, 204, 156)), (1.0, (241, 221, 180))]
    walnut = [(0.0, (104, 62, 36)), (0.35, (146, 94, 58)), (0.62, (171, 117, 75)), (1.0, (196, 143, 96))]
    folder = kit.CONTENT / "boards" / "wood"
    save([veneer(rng, maple, 0.3, 240, tone) for tone in (0.0, 0.035, -0.03, 0.01)], folder / "light.png")
    save([veneer(rng, walnut, 0.42, 400, tone) for tone in (0.0, -0.04, 0.035, 0.015)], folder / "dark.png")


# --- stone --------------------------------------------------------------------

def flag(rng, ramp, mottle, cut, lit, stud, tone):
    """One flagstone: mottled, its edge worn down, a petal cut into each corner."""
    size = CELL * SCALE
    shape = (size, size)

    value = (0.5 + tone
             + noise(rng, shape, 4, 4) * 0.5 * mottle       # clouds
             + noise(rng, shape, 13, 13) * 0.42 * mottle
             + noise(rng, shape, 46, 46) * 0.3 * mottle
             + (rng.random(shape).astype(np.float32) - 0.5) * 0.16 * mottle)   # grit

    # Veins: a few faint wandering lines.
    veins = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(veins)
    for _ in range(3):
        x, y = float(rng.integers(0, size)), float(rng.integers(0, size))
        angle = rng.uniform(0, 6.28)
        points = [(x, y)]
        for _ in range(40):
            angle += rng.normal(0, 0.28)
            x, y = x + np.cos(angle) * size / 30, y + np.sin(angle) * size / 30
            points.append((x, y))
        draw.line(points, fill=int(rng.integers(70, 150)), width=SCALE)
    veins = np.asarray(veins.filter(ImageFilter.GaussianBlur(SCALE * 0.9)), dtype=np.float32) / 255.0
    value += veins * 0.22 * (1 if lit > 0.5 else -1) * mottle

    square = colour(value, ramp)

    # What is carved: a mask of grooves, then light on one lip and shadow on
    # the other, as if lit from the top left.
    carve = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(carve)
    r, d, w = 15.0 * SCALE, 18.5 * SCALE, int(2.2 * SCALE)
    for cx, cy in ((d, d), (size - d, d), (d, size - d), (size - d, size - d)):
        draw.ellipse([cx - r, cy - r, cx + r, cy + r], outline=255, width=w)
    groove = np.asarray(carve.filter(ImageFilter.GaussianBlur(SCALE * 0.5)), dtype=np.float32) / 255.0
    shift = int(SCALE * 1.3)
    lip = np.roll(groove, (shift, shift), axis=(0, 1)) - groove      # bright below-right of a groove
    square *= (1.0 - groove * cut)[..., None]
    square += (np.clip(lip, 0, 1) * 255 * 0.1 * lit)[..., None]

    # The worn edge: darker toward the joint, unevenly, and a bevel that
    # catches the light along the top and left.
    ys, xs = np.mgrid[0:size, 0:size].astype(np.float32)
    border = np.minimum(np.minimum(xs, ys), np.minimum(size - 1 - xs, size - 1 - ys)) / SCALE
    ragged = border + noise(rng, shape, 40, 40) * 5.0
    wear = np.clip(1.0 - ragged / 9.0, 0.0, 1.0)
    square *= (1.0 - wear * 0.32)[..., None]
    bevel = 3.2
    top_left = ((np.minimum(xs, ys) / SCALE) < bevel).astype(np.float32)
    bottom_right = ((np.minimum(size - 1 - xs, size - 1 - ys) / SCALE) < bevel).astype(np.float32)
    square += (top_left * 255 * 0.07 * lit)[..., None]
    square *= (1.0 - bottom_right * 0.3)[..., None]

    # The stud: a quarter of a brass boss in each corner, so that four flags
    # make a whole one where they meet.
    boss = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(boss)
    ring = Image.new("L", (size, size), 0)
    draw_ring = ImageDraw.Draw(ring)
    rb = 8.6 * SCALE
    for cx, cy in ((0, 0), (size, 0), (0, size), (size, size)):
        draw_ring.ellipse([cx - rb - 2.4 * SCALE, cy - rb - 2.4 * SCALE, cx + rb + 2.4 * SCALE, cy + rb + 2.4 * SCALE],
                          fill=255)
        draw.ellipse([cx - rb, cy - rb, cx + rb, cy + rb], fill=255)
    ring = np.asarray(ring, dtype=np.float32)[..., None] / 255.0
    boss = np.asarray(boss, dtype=np.float32)[..., None] / 255.0
    square = square * (1 - ring) + np.array([14, 10, 12], dtype=np.float32) * ring
    square = square * (1 - boss) + np.array(stud, dtype=np.float32) * boss
    return square


def gothic():
    rng = np.random.default_rng(1163)
    limestone = [(0.0, (160, 152, 136)), (0.4, (190, 182, 165)), (0.62, (203, 196, 180)), (1.0, (222, 216, 202))]
    oxblood = [(0.0, (62, 18, 24)), (0.4, (88, 30, 36)), (0.62, (103, 38, 44)), (1.0, (128, 54, 58))]
    brass = (171, 136, 74)
    folder = kit.CONTENT / "boards" / "gothic"
    save([flag(rng, limestone, 0.5, 0.2, 0.8, brass, tone) for tone in (0.0, 0.04, -0.035, 0.015)],
         folder / "light.png")
    save([flag(rng, oxblood, 0.62, 0.3, 0.4, brass, tone) for tone in (0.0, -0.04, 0.04, 0.015)],
         folder / "dark.png")


# --- blossom ------------------------------------------------------------------

def petal(cx, cy, angle, length, width):
    """One petal as a polygon: an ellipse with its far end nicked, pointing
    away from (cx, cy) at `angle`."""
    points = []
    for step in range(40):
        t = step / 40.0 * 2 * np.pi
        along = length * 0.5 * (1 + np.cos(t))
        across = width * 0.5 * np.sin(t)
        # The nick that makes it a cherry petal rather than any petal.
        if abs(t) < 0.5 or abs(t - 2 * np.pi) < 0.5:
            along -= length * 0.12 * (1 - min(abs(t), abs(t - 2 * np.pi)) / 0.5)
        points.append((cx + along * np.cos(angle) - across * np.sin(angle),
                       cy + along * np.sin(angle) + across * np.cos(angle)))
    return points


def cel(rng, base, petals, heart, tint):
    """One square of the sakura board: flat colour, a breath of a gradient,
    and what the tree has dropped on it."""
    size = CELL * SCALE
    ys, xs = np.mgrid[0:size, 0:size].astype(np.float32)
    slope = ((xs + ys) / (2 * size) - 0.5) * -10.0 + tint      # lighter toward the top left
    square = np.empty((size, size, 3), dtype=np.float32)
    for channel in range(3):
        square[..., channel] = base[channel] + slope
    image = Image.fromarray(np.clip(square, 0, 255).astype(np.uint8), "RGB")
    draw = ImageDraw.Draw(image)

    # Two or three whole blossoms, kept out of the middle where a piece stands.
    for _ in range(int(rng.integers(2, 4))):
        while True:
            cx, cy = rng.uniform(0.06, 0.94) * size, rng.uniform(0.06, 0.94) * size
            if np.hypot(cx - size / 2, cy - size / 2) > size * 0.33:
                break
        radius = rng.uniform(17, 27) * SCALE
        turn = rng.uniform(0, 2 * np.pi)
        for k in range(5):
            draw.polygon(petal(cx, cy, turn + k * 2 * np.pi / 5, radius, radius * 0.62), fill=petals)
        r = radius * 0.13
        draw.ellipse([cx - r, cy - r, cx + r, cy + r], fill=heart)
    # And a few loose petals anywhere.
    for _ in range(int(rng.integers(3, 6))):
        cx, cy = rng.uniform(0.04, 0.96) * size, rng.uniform(0.04, 0.96) * size
        length = rng.uniform(11, 17) * SCALE
        draw.polygon(petal(cx, cy, rng.uniform(0, 2 * np.pi), length, length * 0.6), fill=petals)
    return np.asarray(image, dtype=np.float32)


def sakura():
    rng = np.random.default_rng(404)
    folder = kit.CONTENT / "boards" / "sakura"
    save([cel(rng, (255, 245, 240), (255, 228, 233), (255, 208, 218), tint) for tint in (0, 1.5, -1.5, 0.5)],
         folder / "light.png")
    save([cel(rng, (244, 170, 192), (249, 190, 208), (236, 148, 176), tint) for tint in (0, -2, 2, 1)],
         folder / "dark.png")


# --- leather ------------------------------------------------------------------

def hide(rng, ramp, grain, gilt, tone):
    """One square of an old tooled board."""
    size = CELL * SCALE
    shape = (size, size)
    ys, xs = np.mgrid[0:size, 0:size].astype(np.float32)

    value = (0.5 + tone
             + noise(rng, shape, 3, 3) * 0.5 * grain           # stains: where a hand has rested
             + noise(rng, shape, 9, 9) * 0.36 * grain
             + noise(rng, shape, 60, 60) * 0.3 * grain         # the grain of the skin
             + (rng.random(shape).astype(np.float32) - 0.5) * 0.14 * grain)
    # Wear: the middle of a square, where pieces stand and slide, is rubbed paler.
    middle = np.hypot(xs - size / 2, ys - size / 2) / (size / 2)
    value += np.clip(1.0 - middle * 1.25, 0.0, 1.0) * 0.12 * (1 if gilt else -0.4)

    # Foxing: small brown spots.
    spots = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(spots)
    for _ in range(int(rng.integers(5, 12))):
        x, y, r = rng.integers(0, size), rng.integers(0, size), rng.uniform(1.5, 5.5) * SCALE
        draw.ellipse([x - r, y - r, x + r, y + r], fill=int(rng.integers(60, 170)))
    spots = np.asarray(spots.filter(ImageFilter.GaussianBlur(SCALE * 1.6)), dtype=np.float32) / 255.0
    value -= spots * 0.2

    # Craquelure: a few fine cracks that wander and fork.
    cracks = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(cracks)
    for _ in range(int(rng.integers(2, 5))):
        x, y = float(rng.integers(0, size)), float(rng.integers(0, size))
        angle = rng.uniform(0, 6.28)
        points = [(x, y)]
        for _ in range(int(rng.integers(14, 40))):
            angle += rng.normal(0, 0.42)
            x, y = x + np.cos(angle) * size / 34, y + np.sin(angle) * size / 34
            points.append((x, y))
        draw.line(points, fill=int(rng.integers(110, 220)), width=max(1, SCALE - 1))
    cracks = np.asarray(cracks.filter(ImageFilter.GaussianBlur(SCALE * 0.35)), dtype=np.float32) / 255.0
    value -= cracks * 0.3

    square = colour(value, ramp)

    # The tooled line: ruled round the square a little way in. On dark leather
    # it was gilded, and the gold has worn off in places; on vellum it is only
    # pressed in.
    inset, width = 15.0 * SCALE, 2.0 * SCALE
    edge = np.minimum(np.minimum(xs, ys), np.minimum(size - 1 - xs, size - 1 - ys))
    line = np.clip(1.0 - np.abs(edge - inset) / (width / 2), 0.0, 1.0)
    if gilt:
        worn = np.clip(0.8 + noise(rng, shape, 22, 22) * 1.5, 0.0, 1.0)      # where the gold is left
        alpha = (line * worn * 0.86)[..., None]
        square = square * (1 - alpha) + np.array([196, 160, 86], dtype=np.float32) * alpha
        square *= (1.0 - np.clip(1.0 - np.abs(edge - inset - width) / (width / 2), 0, 1) * 0.3)[..., None]
    else:
        square *= (1.0 - line * 0.2)[..., None]
        lip = np.clip(1.0 - np.abs(edge - inset - width * 0.9) / (width / 2), 0.0, 1.0)
        square += (lip * 255 * 0.05)[..., None]

    # The seam: where two skins meet, darker and a little sunk.
    seam = np.clip(1.0 - edge / (4.0 * SCALE), 0.0, 1.0)
    square *= (1.0 - seam * 0.34)[..., None]
    return square


def leather():
    rng = np.random.default_rng(1783)
    vellum = [(0.0, (162, 138, 94)), (0.4, (198, 177, 131)), (0.62, (212, 193, 149)), (1.0, (229, 214, 177))]
    calf = [(0.0, (48, 28, 18)), (0.4, (78, 49, 31)), (0.62, (94, 61, 39)), (1.0, (124, 86, 56))]
    folder = kit.CONTENT / "boards" / "leather"
    save([hide(rng, vellum, 0.5, False, tone) for tone in (0.0, 0.03, -0.035, 0.015)], folder / "light.png")
    save([hide(rng, calf, 0.6, True, tone) for tone in (0.0, -0.035, 0.035, 0.01)], folder / "dark.png")


# --- stone --------------------------------------------------------------------

def slab(rng, ramp, speckle, wear, tone):
    """One square of the stone board: a slab of sandstone cut by hand a very
    long time ago — bedded, pitted, its middle worn smooth by the pieces and
    its edges chipped where it meets the next."""
    size = CELL * SCALE
    shape = (size, size)
    ys, xs = np.mgrid[0:size, 0:size].astype(np.float32) / size

    # The stone itself: faint beds laid down as sand, clouds of iron, grit.
    beds = np.sin((ys + noise(rng, shape, 5, 4) * 0.16) * rng.uniform(22, 34) + rng.uniform(0, 6.28))
    value = (0.5 + tone
             + beds * 0.035
             + noise(rng, shape, 4, 4) * 0.34
             + noise(rng, shape, 13, 13) * 0.2
             + noise(rng, shape, 60, 60) * 0.12
             + (rng.random(shape).astype(np.float32) - 0.5) * speckle)

    # Where pieces have stood and slid for centuries the stone is paler and
    # smoother; toward the edges it keeps its dirt.
    middle = np.clip(1.0 - np.hypot(xs - 0.5, ys - 0.5) / 0.62, 0.0, 1.0)
    value += (middle - 0.45) * wear

    # Pits: small dark hollows, each with a lit lip below it.
    pits = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(pits)
    for _ in range(int(rng.integers(26, 44))):
        x, y = rng.uniform(0, size), rng.uniform(0, size)
        r = rng.uniform(0.7, 2.6) * SCALE
        draw.ellipse([x - r, y - r * 0.8, x + r, y + r * 0.8], fill=int(rng.integers(110, 255)))
    pits = np.asarray(pits.filter(ImageFilter.GaussianBlur(SCALE * 0.5)), dtype=np.float32) / 255.0
    value -= pits * 0.3
    value += np.roll(pits, int(SCALE * 1.4), axis=0) * 0.1

    # A crack or two: thin, wandering, from one edge inward.
    cracks = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(cracks)
    for _ in range(int(rng.integers(0, 3))):
        side = int(rng.integers(0, 4))
        t = rng.uniform(0.1, 0.9) * size
        x, y = [(t, 0.0), (t, size - 1.0), (0.0, t), (size - 1.0, t)][side]
        angle = [1.57, -1.57, 0.0, 3.14][side] + rng.uniform(-0.5, 0.5)
        points = [(x, y)]
        for _ in range(int(rng.integers(5, 12))):
            angle += rng.uniform(-0.45, 0.45)
            x, y = x + np.cos(angle) * size * 0.05, y + np.sin(angle) * size * 0.05
            points.append((x, y))
        draw.line(points, fill=int(rng.integers(120, 210)), width=SCALE - 1)
    cracks = np.asarray(cracks.filter(ImageFilter.GaussianBlur(SCALE * 0.35)), dtype=np.float32) / 255.0
    value -= cracks * 0.26

    square = colour(value, ramp)

    # The joint: the slab does not reach its neighbour everywhere. Its edge
    # wanders and its corners are knocked off; what shows between is dark,
    # packed earth.
    edge = np.minimum(np.minimum(xs, 1.0 - xs), np.minimum(ys, 1.0 - ys))
    corner = np.minimum(np.minimum(np.hypot(xs, ys), np.hypot(1.0 - xs, ys)),
                        np.minimum(np.hypot(xs, 1.0 - ys), np.hypot(1.0 - xs, 1.0 - ys)))
    ragged = 0.011 + noise(rng, shape, 22, 22) * 0.012 + noise(rng, shape, 6, 6) * 0.008
    gap = np.clip((ragged - edge) / 0.006, 0.0, 1.0)
    gap = np.maximum(gap, np.clip((0.05 + noise(rng, shape, 9, 9) * 0.07 - corner) / 0.008, 0.0, 1.0))
    bevel = np.clip(1.0 - (edge - ragged) / 0.02, 0.0, 1.0) * (1.0 - gap)     # the worn arris
    lit = np.where((ys < 0.5) | (xs < 0.5), 1.0, -1.0) * 0.5 + 0.5           # light from the top left
    square *= (1.0 + bevel * (lit * 0.14 - 0.08))[..., None]
    earth = np.array(ramp[0][1], dtype=np.float32) * 0.42
    return square * (1.0 - gap[..., None]) + earth * gap[..., None]


def stone():
    rng = np.random.default_rng(600)
    buff = [(0.0, (142, 111, 72)), (0.4, (181, 149, 102)), (0.62, (195, 163, 114)), (1.0, (216, 187, 139))]
    red = [(0.0, (88, 42, 30)), (0.4, (124, 64, 46)), (0.62, (139, 74, 53)), (1.0, (162, 94, 68))]
    folder = kit.CONTENT / "boards" / "stone"
    save([slab(rng, buff, 0.1, 0.1, tone) for tone in (0.0, 0.04, -0.035, 0.015)], folder / "light.png")
    save([slab(rng, red, 0.12, 0.12, tone) for tone in (0.0, -0.04, 0.04, 0.015)], folder / "dark.png")


def main():
    wood()
    gothic()
    sakura()
    leather()
    stone()


if __name__ == "__main__":
    main()
