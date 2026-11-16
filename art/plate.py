#!/usr/bin/env python3
"""Draws docs/looks.png: the two looks side by side, as a plate.

    python3 art/plate.py

Left, the classic look; right, the Gothic one. Each half shows six pieces at
size with the lines they were constructed on — the turner's axis and floor,
the mason's two centres — and below them the four ranks a game starts from,
on that look's own board. It needs what the other scripts need (rsvg-convert,
numpy, Pillow) and has been run after them: it reads the board pictures from
game/Content/boards/. The lettering asks for Arsenal SC, Lora and IBM Plex
Mono and falls back to whatever serif and monospace the machine has.
"""

import subprocess
import tempfile

import numpy as np
from PIL import Image

import classic
import gothic
import kit

W, H = 3000, 2000
PANEL = W // 2
MARGIN = 110
CONTENT = PANEL - 2 * MARGIN

NAMES = {"K": "король", "Q": "ферзь", "R": "тура", "B": "слон", "N": "кінь", "P": "пішак"}
NUMERALS = {"K": "I", "Q": "II", "R": "III", "B": "IV", "N": "V", "P": "VI"}

LOOKS = [
    dict(name="classic", draw=classic.svg, floor=classic.FLOOR, board="wood",
         title="ТОЧЕНЕ СВІТЛО", sub="класичний вигляд", plate="PL. I",
         paper=(236, 227, 207), ink="#3b2815", accent="#9a7340",
         frame="#342015", trim="#cdaa70",
         swatches=["#fff9e9", "#cfae78", "#3d3530", "#e1c594", "#9e6942"],
         source="art/classic.py · art/boards.py"),
    dict(name="gothic", draw=gothic.svg, floor=gothic.FLOOR, board="gothic",
         title="СТРІЛЧАСТА НІЧ", sub="ґотичний вигляд", plate="PL. II",
         paper=(21, 18, 26), ink="#ddd4c0", accent="#c49b44",
         frame="#0c0a0e", trim="#9e7c42",
         swatches=["#ebe4d3", "#1c1823", "#b01822", "#c49b44", "#5d2127"],
         source="art/gothic.py · art/boards.py"),
]

HERO = 400                 # a piece in the upper part, in pixels
ROWS = [(250, "KQR", "w"), (720, "BNP", "b")]
SQUARE = 150               # a square of the board below
BOARD_TOP = 1200
RANKS = ["RNBQKBNR", "PPPPPPPP", "PPPPPPPP", "RNBQKBNR"]   # top to bottom
RANK_COLOURS = "bbww"


def top_of(name, colour, letter):
    """How high a piece stands, in its own units: the first row of its picture
    that is not empty."""
    picture = Image.open(kit.CONTENT / "pieces" / name / f"{colour}{letter}.png")
    return picture.getbbox()[1] * 100.0 / picture.height


def text(x, y, what, size, family, fill, anchor="start", spacing=0, opacity=1.0, style=""):
    return (f'<text x="{x:.1f}" y="{y:.1f}" font-family="{family}" font-size="{size}" fill="{fill}" '
            f'fill-opacity="{opacity}" text-anchor="{anchor}" letter-spacing="{spacing}" {style}>{what}</text>')


SMALL_CAPS = "'Arsenal SC', 'Lora', Georgia, serif"
ITALIC = "'Lora', Georgia, serif"
MONO = "'IBM Plex Mono', 'DejaVu Sans Mono', monospace"


def panel(look, left, uid):
    out = []
    ink, accent = look["ink"], look["accent"]
    x0 = left + MARGIN
    x1 = x0 + CONTENT

    def line(xa, ya, xb, yb, width=1.5, opacity=0.38, dash="", colour=ink):
        dash = f' stroke-dasharray="{dash}"' if dash else ""
        out.append(f'<line x1="{xa:.1f}" y1="{ya:.1f}" x2="{xb:.1f}" y2="{yb:.1f}" stroke="{colour}" '
                   f'stroke-width="{width}" stroke-opacity="{opacity}"{dash}/>')

    # The head of the plate.
    out.append(text(x0, 140, look["title"], 46, SMALL_CAPS, ink, spacing=15))
    out.append(text(x1, 140, look["plate"], 20, MONO, ink, anchor="end", spacing=3, opacity=0.6))
    line(x0, 172, x1, 172, 1.5, 0.55)
    out.append(text(x0, 208, look["sub"], 25, ITALIC, ink, opacity=0.7, style='font-style="italic"'))

    # Six pieces at size, and what they were constructed on.
    scale = HERO / 100.0
    column = CONTENT / 3.0
    for top, letters, colour in ROWS:
        floor = top + look["floor"] * scale
        line(x0, floor, x1, floor, 1.5, 0.5)
        for i, letter in enumerate(letters):
            cx = x0 + column * (i + 0.5)
            px = cx - HERO / 2
            peak = top + top_of(look["name"], colour, letter) * scale

            # The axis, and the height.
            line(cx, top - 6, cx, floor + 22, 1.2, 0.34, "9 7")
            dim = cx + 182
            line(dim, peak, dim, floor, 1.2, 0.42)
            line(dim - 9, peak, dim + 9, peak, 1.2, 0.6)
            line(dim - 9, floor, dim + 9, floor, 1.2, 0.6)
            out.append(text(dim + 14, (peak + floor) / 2 + 6, f"{look['floor'] - (peak - top) / scale:.0f}", 18,
                            MONO, ink, opacity=0.6))

            if look["name"] == "gothic" and letter == "P":
                # The mason's construction of the pawn: two centres, two arcs.
                r, spring = 27 * scale, top + 49 * scale
                for centre in (px + (60.6 - 27) * scale, px + (39.4 + 27) * scale):
                    out.append(f'<circle cx="{centre:.1f}" cy="{spring:.1f}" r="{r:.1f}" fill="none" '
                               f'stroke="{accent}" stroke-width="1.3" stroke-opacity="0.55"/>')
                    out.append(f'<circle cx="{centre:.1f}" cy="{spring:.1f}" r="3.6" fill="{accent}"/>')
                line(px + 14 * scale, spring, px + 86 * scale, spring, 1.3, 0.55, "", accent)
            if look["name"] == "classic" and letter == "P":
                # The turner's construction of the pawn: the ball, and where it sits.
                cy, r = top + 43.6 * scale, 12.0 * scale
                out.append(f'<circle cx="{cx:.1f}" cy="{cy:.1f}" r="{r + 16:.1f}" fill="none" stroke="{accent}" '
                           f'stroke-width="1.3" stroke-opacity="0.7" stroke-dasharray="3 6"/>')
                line(cx - r - 44, cy, cx + r + 44, cy, 1.3, 0.7, "", accent)

            out.append(kit.embed(look["draw"](letter, colour), px, top, HERO, f"{uid}h{letter}"))
            out.append(text(cx, floor + 58, f"{NUMERALS[letter]} · {NAMES[letter]}", 23, SMALL_CAPS, ink,
                            anchor="middle", spacing=5, opacity=0.78))

    # The four ranks a game starts from, on this look's board.
    bx = left + (PANEL - 8 * SQUARE) / 2
    frame, thread = 15, 4
    out.append(f'<rect x="{bx - frame}" y="{BOARD_TOP - frame}" width="{8 * SQUARE + 2 * frame}" '
               f'height="{4 * SQUARE + 2 * frame}" rx="3" fill="{look["frame"]}"/>')
    out.append(f'<rect x="{bx - thread}" y="{BOARD_TOP - thread}" width="{8 * SQUARE + 2 * thread}" '
               f'height="{4 * SQUARE + 2 * thread}" fill="{look["trim"]}"/>')
    # The squares themselves are pasted in afterwards, under this drawing; a
    # hole is left for them here.
    out.append(f'<rect x="{bx}" y="{BOARD_TOP}" width="{8 * SQUARE}" height="{4 * SQUARE}" fill="#000" '
               f'class="hole"/>')
    pieces = []
    for row, (rank, colour) in enumerate(zip(RANKS, RANK_COLOURS)):
        for col, letter in enumerate(rank):
            pieces.append(kit.embed(look["draw"](letter, colour), bx + col * SQUARE, BOARD_TOP + row * SQUARE,
                                    SQUARE, f"{uid}b{row}{col}"))

    # The foot: what it is made of, and where it is drawn.
    fy = BOARD_TOP + 4 * SQUARE + 70
    for i, swatch in enumerate(look["swatches"]):
        out.append(f'<rect x="{x0 + i * 62}" y="{fy}" width="44" height="44" fill="{swatch}" stroke="{ink}" '
                   f'stroke-opacity="0.45" stroke-width="1.2"/>')
    out.append(text(x1, fy + 30, look["source"], 19, MONO, ink, anchor="end", spacing=1.5, opacity=0.55))
    return out, pieces, (int(bx), BOARD_TOP)


def squares(look):
    """The board under the four ranks, laid the way the game lays it."""
    tiles = {kind: Image.open(kit.CONTENT / "boards" / look["board"] / f"{kind}.png").convert("RGB")
             for kind in ("light", "dark")}
    half = tiles["light"].width // 2
    board = Image.new("RGB", (8 * SQUARE, 4 * SQUARE))
    for shown, row in enumerate((7, 6, 1, 0)):      # the ranks, top to bottom, as rows of the board
        for col in range(8):
            pick = (col * 73 + row * 151 + col * row * 31) % 16
            tile = tiles["light" if (row + col) % 2 else "dark"].crop(
                ((pick & 1) * half, ((pick >> 1) & 1) * half, (pick & 1) * half + half,
                 ((pick >> 1) & 1) * half + half))
            if pick & 4:
                tile = tile.transpose(Image.FLIP_LEFT_RIGHT)
            if pick & 8:
                tile = tile.transpose(Image.FLIP_TOP_BOTTOM)
            board.paste(tile.resize((SQUARE, SQUARE), Image.LANCZOS), (col * SQUARE, shown * SQUARE))
    return board


def render(body):
    with tempfile.TemporaryDirectory() as folder:
        source, target = f"{folder}/plate.svg", f"{folder}/plate.png"
        with open(source, "w", encoding="utf-8") as file:
            file.write(f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}">'
                       + "\n".join(body) + "</svg>")
        subprocess.run(["rsvg-convert", "--unlimited", "-o", target, source], check=True)
        return Image.open(target).convert("RGBA")


def main():
    rng = np.random.default_rng(7)
    base = np.zeros((H, W, 3), dtype=np.float32)
    drawing, pieces, boards = [], [], []
    for index, look in enumerate(LOOKS):
        left = index * PANEL
        # Paper: the colour, and a little tooth.
        base[:, left:left + PANEL] = np.array(look["paper"], dtype=np.float32)
        base[:, left:left + PANEL] += rng.normal(0, 1.6, (H, PANEL, 1)).astype(np.float32)
        lines, on_board, at = panel(look, left, f"l{index}")
        drawing += lines
        pieces += on_board
        boards.append((squares(look), at))

    plate = Image.fromarray(np.clip(base, 0, 255).astype(np.uint8), "RGB").convert("RGBA")
    under = render([line for line in drawing if 'class="hole"' not in line])
    plate.alpha_composite(under)
    for board, at in boards:
        plate.paste(board, at)
    plate.alpha_composite(render(pieces))
    plate.convert("RGB").save(kit.ROOT / "docs" / "looks.png", optimize=True)


if __name__ == "__main__":
    main()
