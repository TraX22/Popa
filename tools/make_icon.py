"""Draws the Popa icon and writes src/Popa/Popa.ico.

The icon is a Windows 11 style rounded tile with a small popup menu card on
it: three rows, each with a colored app icon, the middle one highlighted.
Every size is drawn separately (not scaled from 256 px) so the small sizes
stay crisp; sizes up to 24 px use a simplified design without the card.

Usage:  python tools/make_icon.py [--preview preview.png]
Requires Pillow (pip install pillow).
"""

import struct
import sys
from io import BytesIO
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

SIZES = [16, 20, 24, 32, 40, 48, 64, 96, 128, 256]
SUPERSAMPLE = 8  # draw at 8x and downscale: cheap antialiasing

TILE_TOP_LEFT = (79, 70, 229)      # indigo
TILE_BOTTOM_RIGHT = (6, 182, 212)  # teal
CARD = (255, 255, 255)
BAR = (203, 213, 225)
HIGHLIGHT = (224, 231, 255)
HIGHLIGHT_BAR = (99, 102, 241)
APP_COLORS = [(245, 158, 11), (16, 185, 129), (239, 68, 68)]


def gradient(size):
    """Diagonal gradient from top-left to bottom-right."""
    img = Image.new("RGB", (size, size))
    px = img.load()
    for y in range(size):
        for x in range(size):
            t = (x + y) / (2 * (size - 1))
            px[x, y] = tuple(round(a + (b - a) * t) for a, b in zip(TILE_TOP_LEFT, TILE_BOTTOM_RIGHT))
    return img


def tile(canvas, s, margin, radius):
    """Pastes the gradient tile with rounded corners onto the canvas."""
    mask = Image.new("L", canvas.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        [margin * s, margin * s, canvas.size[0] - margin * s - 1, canvas.size[1] - margin * s - 1],
        radius=radius * s, fill=255)
    canvas.paste(gradient(canvas.size[0]), (0, 0), mask)


def draw_full(size):
    """Detailed design, coordinates in a 256-unit grid."""
    s = size * SUPERSAMPLE / 256
    big = size * SUPERSAMPLE
    canvas = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    tile(canvas, s, 12, 56)

    def box(x0, y0, x1, y1):
        return [x0 * s, y0 * s, x1 * s, y1 * s]

    # Soft shadow under the card.
    shadow = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle(box(58, 66, 206, 210), radius=22 * s, fill=(15, 23, 42, 90))
    shadow = shadow.filter(ImageFilter.GaussianBlur(8 * s))
    canvas.alpha_composite(shadow)

    d = ImageDraw.Draw(canvas)
    d.rounded_rectangle(box(52, 56, 204, 200), radius=22 * s, fill=CARD)

    rows = [(90, 150), (128, 176), (166, 132)]  # (center y, bar end x)
    for i, (cy, bar_end) in enumerate(rows):
        selected = i == 1
        if selected:
            d.rounded_rectangle(box(62, cy - 17, 194, cy + 17), radius=10 * s, fill=HIGHLIGHT)
        d.rounded_rectangle(box(74, cy - 11, 96, cy + 11), radius=6 * s, fill=APP_COLORS[i])
        d.rounded_rectangle(box(108, cy - 5, bar_end, cy + 5), radius=5 * s,
                            fill=HIGHLIGHT_BAR if selected else BAR)
    return canvas.resize((size, size), Image.LANCZOS)


def draw_small(size):
    """Simplified design for 16-24 px: tile and three white rows, no card."""
    s = size * SUPERSAMPLE / 16
    big = size * SUPERSAMPLE
    canvas = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    tile(canvas, s, 0.5, 3.5)
    d = ImageDraw.Draw(canvas)
    for i, cy in enumerate([4.5, 8, 11.5]):
        alpha = 255 if i == 1 else 200
        d.rounded_rectangle([3 * s, (cy - 1.25) * s, 5.5 * s, (cy + 1.25) * s], radius=0.6 * s,
                            fill=APP_COLORS[i] + (255,))
        end = [10.5, 13, 9.5][i]
        d.rounded_rectangle([7 * s, (cy - 0.9) * s, end * s, (cy + 0.9) * s], radius=0.9 * s,
                            fill=(255, 255, 255, alpha))
    return canvas.resize((size, size), Image.LANCZOS)


def bmp_entry(img):
    """32-bit BMP icon image: BITMAPINFOHEADER, bottom-up BGRA pixels, AND mask.

    The AND mask is all zeros; with 32-bit images Windows uses the alpha
    channel instead. The header height is doubled because it covers both
    the color pixels and the mask, a quirk of the ICO format.
    """
    w, h = img.size
    header = struct.pack("<IiiHHIIiiII", 40, w, h * 2, 1, 32, 0, 0, 0, 0, 0, 0)
    pixels = bytearray()
    for y in range(h - 1, -1, -1):
        for x in range(w):
            r, g, b, a = img.getpixel((x, y))
            pixels += bytes((b, g, r, a))
    mask_row = ((w + 31) // 32) * 4
    return header + bytes(pixels) + bytes(mask_row * h)


def png_entry(img):
    out = BytesIO()
    img.save(out, format="PNG", optimize=True)
    return out.getvalue()


def write_ico(images, path):
    # Sizes up to 64 px as BMP (read by every Windows API), larger as PNG
    # (much smaller; supported since Windows Vista).
    blobs = [png_entry(img) if img.size[0] > 64 else bmp_entry(img) for img in images]
    data = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    for img, blob in zip(images, blobs):
        w = img.size[0]
        data += struct.pack("<BBBBHHII", w % 256, w % 256, 0, 0, 1, 32, len(blob), offset)
        offset += len(blob)
    Path(path).write_bytes(data + b"".join(blobs))


def main():
    images = [draw_small(n) if n <= 24 else draw_full(n) for n in SIZES]
    root = Path(__file__).resolve().parent.parent
    write_ico(images, root / "src" / "Popa" / "Popa.ico")

    if "--preview" in sys.argv:
        out = sys.argv[sys.argv.index("--preview") + 1]
        sheet = Image.new("RGBA", (sum(SIZES) + 10 * len(SIZES) + 10, 276), (243, 243, 243, 255))
        x = 10
        for img in images:
            sheet.alpha_composite(img, (x, 10))
            x += img.size[0] + 10
        dark = Image.new("RGBA", sheet.size, (32, 32, 32, 255))
        x = 10
        for img in images:
            dark.alpha_composite(img, (x, 10))
            x += img.size[0] + 10
        both = Image.new("RGBA", (sheet.size[0], sheet.size[1] * 2))
        both.paste(sheet, (0, 0))
        both.paste(dark, (0, sheet.size[1]))
        both.save(out)


if __name__ == "__main__":
    main()
