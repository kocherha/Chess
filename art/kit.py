"""What the two piece sets and the boards are drawn with.

Nothing here knows what a rook looks like. It turns outlines described on a
100 x 100 square into SVG path data, writes the SVGs out as PNGs with
rsvg-convert (librsvg), and lays finished pieces out on a sheet to look at
(that last part needs Pillow).
"""

import pathlib
import subprocess

ROOT = pathlib.Path(__file__).resolve().parent.parent
CONTENT = ROOT / "game" / "Content"
SIZE = 256  # pixels a piece is drawn at; a square on screen is a third of that

AXIS = 50.0  # the vertical line every piece but the knight is symmetric about


def f(value):
    """A number as it is written into a path: two decimals are plenty."""
    return f"{value:.2f}".rstrip("0").rstrip(".")


def pt(x, y):
    return f"{f(x)},{f(y)}"


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
            right.append(f"C{pt(axis + seg[1], seg[2])} {pt(axis + seg[3], seg[4])} "
                         f"{pt(axis + seg[5], seg[6])}")
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


def mirrored(points, axis=AXIS):
    """A closed polygon from the points of its right half, top to bottom or
    bottom to top: the left half is added in the opposite order."""
    left = [(2 * axis - x, y) for x, y in reversed(points) if abs(x - axis) > 1e-6]
    return polygon(list(points) + left)


def circle(x, y, r):
    """A circle as a path, so that it can be clipped, filled and outlined like
    everything else."""
    return (f"M{pt(x - r, y)} A{f(r)},{f(r)} 0 1 0 {pt(x + r, y)} "
            f"A{f(r)},{f(r)} 0 1 0 {pt(x - r, y)} Z")


def svg_open(size=SIZE):
    return f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" width="{size}" height="{size}">'


def render(source, target, size=SIZE):
    subprocess.run(["rsvg-convert", "-w", str(size), "-h", str(size), "-o", str(target), str(source)],
                   check=True)


def write_set(name, draw, letters="KQRBNP"):
    """Writes art/<name>/*.svg and game/Content/pieces/<name>/*.png.
    `draw(letter, colour)` returns the SVG of one piece."""
    svg_dir = ROOT / "art" / name
    png_dir = CONTENT / "pieces" / name
    svg_dir.mkdir(parents=True, exist_ok=True)
    png_dir.mkdir(parents=True, exist_ok=True)
    for colour in "wb":
        for letter in letters:
            source = svg_dir / f"{colour}{letter}.svg"
            source.write_text(draw(letter, colour) + "\n", encoding="utf-8")
            render(source, png_dir / f"{colour}{letter}.png")


# (id, light, dark) of the boards in game/Source/Look.cpp, in the same order.
# A board that is laid from pictures has them in game/Content/boards/<id>/.
BOARDS = [("wood", "#e1c594", "#9e6942"), ("gothic", "#beb7a7", "#5d2127"),
          ("marble", "#e4e6ea", "#8d96a7"), ("emerald", "#ebecd0", "#739552"),
          ("sakura", "#fef4ef", "#f4abc0"), ("leather", "#c8b487", "#573823"),
          ("stone", "#b69768", "#7f422f"), ("pixel", "#e4d8b0", "#66915f")]


def sheet(name, target, cell=128, order="KQRBNP"):
    """Every piece of both colours on each board's two squares, for looking at."""
    from PIL import Image

    png_dir = CONTENT / "pieces" / name
    image = Image.new("RGB", (cell * 12, cell * len(BOARDS)), "#181a1f")
    for row, (board, light, dark) in enumerate(BOARDS):
        for col in range(12):
            colour = "w" if col < 6 else "b"
            is_light = (row + col) % 2 == 0
            tile = CONTENT / "boards" / board / ("light.png" if is_light else "dark.png")
            if tile.exists():
                # The same quarter of the picture the game would show there.
                picture = Image.open(tile).convert("RGB")
                half = picture.width // 2
                box = ((col % 2) * half, (row % 2) * half)
                square = picture.crop((box[0], box[1], box[0] + half, box[1] + half)).resize(
                    (cell, cell), Image.LANCZOS)
            else:
                square = Image.new("RGB", (cell, cell), light if is_light else dark)
            piece = Image.open(png_dir / f"{colour}{order[col % 6]}.png").resize((cell, cell), Image.LANCZOS)
            square.paste(piece, (0, 0), piece)
            image.paste(square, (col * cell, row * cell))
    image.save(target)


def embed(svg_text, x, y, size, uid):
    """One finished piece as part of a larger drawing: its <svg> re-opened at
    a place and a size, and its ids made unique so that two pieces on the same
    sheet do not share a gradient."""
    body = svg_text[svg_text.index(">") + 1:svg_text.rindex("</svg>")]
    body = body.replace('id="', f'id="{uid}-').replace("url(#", f"url(#{uid}-")
    return (f'<svg x="{f(x)}" y="{f(y)}" width="{f(size)}" height="{f(size)}" viewBox="0 0 100 100">'
            f"{body}</svg>")
