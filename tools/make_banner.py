"""Draws the GitHub banner / social preview image: docs/images/banner.png.

1280 x 640 px, the size GitHub recommends for a repository's social
preview (Settings > General > Social preview). The same image is shown at
the top of the README.

Uses the Popa icon from make_icon.py and Windows fonts (Segoe UI and
Segoe Fluent Icons), so it must run on Windows.

Usage:  python tools/make_banner.py
"""

from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter, ImageFont

from make_icon import draw_full

WIDTH, HEIGHT = 1280, 640
SCALE = 2  # drawn at 2x and reduced at the end, for smooth edges
FONTS = Path("C:/Windows/Fonts")

BACKGROUND_TOP = (15, 18, 42)
BACKGROUND_BOTTOM = (24, 20, 64)
GLOW_INDIGO = (79, 70, 229)
GLOW_TEAL = (6, 182, 212)
TEXT = (255, 255, 255)
TEXT_SOFT = (199, 210, 254)
CHIP_TEXT = (224, 231, 255)
MENU = (44, 44, 44)
MENU_BORDER = (70, 70, 74)
MENU_HIGHLIGHT = (64, 64, 70)
MENU_TEXT = (240, 240, 240)
MENU_SEPARATOR = (75, 75, 80)


def font(name, size):
    return ImageFont.truetype(str(FONTS / name), size * SCALE)


def s(value):
    return int(value * SCALE)


def background():
    img = Image.new("RGB", (s(WIDTH), s(HEIGHT)))
    draw = ImageDraw.Draw(img)
    for y in range(s(HEIGHT)):
        t = y / (s(HEIGHT) - 1)
        color = tuple(round(a + (b - a) * t) for a, b in zip(BACKGROUND_TOP, BACKGROUND_BOTTOM))
        draw.line([(0, y), (s(WIDTH), y)], fill=color)

    # Two soft glows in the icon's colors.
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    g = ImageDraw.Draw(glow)
    g.ellipse([s(-200), s(-260), s(620), s(420)], fill=GLOW_INDIGO + (110,))
    g.ellipse([s(760), s(260), s(1500), s(900)], fill=GLOW_TEAL + (80,))
    glow = glow.filter(ImageFilter.GaussianBlur(s(140)))
    img = img.convert("RGBA")
    img.alpha_composite(glow)
    return img


def keycap(draw, x, y, label, label_font):
    width = draw.textlength(label, font=label_font) + s(36)
    height = s(58)
    draw.rounded_rectangle([x, y + s(5), x + width, y + height + s(5)], radius=s(12), fill=(10, 12, 30))
    draw.rounded_rectangle([x, y, x + width, y + height], radius=s(12), fill=(49, 46, 129),
                           outline=(129, 140, 248), width=s(2))
    draw.text((x + width / 2, y + height / 2 - s(2)), label, font=label_font, fill=TEXT, anchor="mm")
    return x + width


def chip(draw, x, y, label, chip_font):
    width = draw.textlength(label, font=chip_font) + s(32)
    draw.rounded_rectangle([x, y, x + width, y + s(40)], radius=s(20), fill=(255, 255, 255, 22),
                           outline=(255, 255, 255, 60), width=s(1))
    draw.text((x + width / 2, y + s(20)), label, font=chip_font, fill=CHIP_TEXT, anchor="mm")
    return x + width + s(12)


def popup_menu(img):
    """A Windows 11 style dark popup menu, like the one Popa shows."""
    x0, y0, width = s(780), s(118), s(390)
    row = s(52)
    rows = [
        ("\uE774", "Browser", (59, 130, 246), False, False),
        ("\uE943", "Code editor", (16, 185, 129), True, False),
        ("\uEB9F", "Photos", (245, 158, 11), False, False),
        None,
        ("\uE7FC", "Games", (239, 68, 68), False, True),
        ("\uE8B7", "Projects", (234, 179, 8), False, True),
        ("\uE713", "Settings", (148, 163, 184), False, False),
    ]
    height = sum(s(18) if r is None else row for r in rows) + s(16)

    shadow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle([x0 + s(6), y0 + s(18), x0 + width + s(6), y0 + height + s(18)],
                                             radius=s(16), fill=(0, 0, 0, 150))
    img.alpha_composite(shadow.filter(ImageFilter.GaussianBlur(s(24))))

    draw = ImageDraw.Draw(img)
    draw.rounded_rectangle([x0, y0, x0 + width, y0 + height], radius=s(16), fill=MENU, outline=MENU_BORDER,
                           width=s(2))

    icon_font = font("SegoeIcons.ttf", 22)
    label_font = font("segoeui.ttf", 24)
    arrow_font = font("SegoeIcons.ttf", 14)
    y = y0 + s(8)
    for entry in rows:
        if entry is None:
            draw.line([(x0 + s(20), y + s(9)), (x0 + width - s(20), y + s(9))], fill=MENU_SEPARATOR, width=s(2))
            y += s(18)
            continue
        glyph, label, color, highlighted, submenu = entry
        if highlighted:
            draw.rounded_rectangle([x0 + s(8), y + s(3), x0 + width - s(8), y + row - s(3)], radius=s(8),
                                   fill=MENU_HIGHLIGHT)
        # Icon: a rounded colored tile with a Fluent glyph.
        ix, iy = x0 + s(22), y + (row - s(34)) // 2
        draw.rounded_rectangle([ix, iy, ix + s(34), iy + s(34)], radius=s(8), fill=color)
        draw.text((ix + s(17), iy + s(17)), glyph, font=icon_font, fill=TEXT, anchor="mm")
        draw.text((x0 + s(74), y + row / 2), label, font=label_font, fill=MENU_TEXT, anchor="lm")
        if submenu:
            draw.text((x0 + width - s(30), y + row / 2), "\uE76C", font=arrow_font, fill=MENU_TEXT, anchor="mm")
        y += row

    # Mouse pointer at the menu's corner, where the menu opened.
    px, py = x0 - s(26), y0 - s(30)
    pointer = [(0, 0), (0, 34), (9, 26), (15, 40), (21, 37), (15, 24), (27, 24)]
    points = [(px + s(x), py + s(y)) for x, y in pointer]
    draw.polygon(points, fill=TEXT, outline=(15, 18, 42))
    draw.line(points + [points[0]], fill=(15, 18, 42), width=s(2))


def main():
    img = background()
    draw = ImageDraw.Draw(img)

    # Left: icon, name, tagline, hotkey, features.
    icon = draw_full(s(132))
    img.alpha_composite(icon, (s(96), s(92)))
    draw.text((s(250), s(158)), "Popa", font=font("segoeuib.ttf", 96), fill=TEXT, anchor="lm")

    draw.text((s(100), s(290)), "Your programs,", font=font("seguisb.ttf", 44), fill=TEXT, anchor="ls")
    draw.text((s(100), s(344)), "one keystroke away.", font=font("seguisb.ttf", 44), fill=TEXT, anchor="ls")
    draw.text((s(100), s(392)), "A tiny, private launcher for Windows 11.", font=font("segoeui.ttf", 26),
              fill=TEXT_SOFT, anchor="ls")

    key_font = font("seguisb.ttf", 26)
    x = keycap(draw, s(100), s(428), "Win", key_font)
    draw.text((x + s(22), s(457)), "+", font=font("seguisb.ttf", 32), fill=TEXT_SOFT, anchor="mm")
    keycap(draw, x + s(44), s(428), "Z", key_font)

    chip_font = font("segoeui.ttf", 20)
    overlay = Image.new("RGBA", img.size, (0, 0, 0, 0))
    chips = ImageDraw.Draw(overlay)
    x = s(100)
    for label in ["Single 0.6 MB exe", "No telemetry", "Open source", "English · Español"]:
        x = chip(chips, x, s(528), label, chip_font)
    img.alpha_composite(overlay)

    # Right: the menu.
    popup_menu(img)

    out = Path(__file__).resolve().parent.parent / "docs" / "images" / "banner.png"
    out.parent.mkdir(parents=True, exist_ok=True)
    img.resize((WIDTH, HEIGHT), Image.LANCZOS).convert("RGB").save(out, optimize=True)
    print(out, out.stat().st_size, "bytes")


if __name__ == "__main__":
    main()
