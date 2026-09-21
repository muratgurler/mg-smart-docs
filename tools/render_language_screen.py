#!/usr/bin/env python3
"""Render the Fix20 language/flag screen reference image."""

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

WIDTH, HEIGHT, SCALE = 1024, 600, 2


def sc(value):
    return round(value * SCALE)


def font(size, bold=False):
    name = "DejaVuSans-Bold.ttf" if bold else "DejaVuSans.ttf"
    return ImageFont.truetype(f"/usr/share/fonts/truetype/dejavu/{name}", sc(size))


def text(draw, xy, value, size, color, anchor="mm", bold=False):
    draw.text((sc(xy[0]), sc(xy[1])), value, font=font(size, bold),
              fill=color, anchor=anchor)


def flag(draw, x, y, code):
    box = (sc(x), sc(y), sc(x + 44), sc(y + 30))
    inner_x, inner_y = x + 1, y + 1
    draw.rounded_rectangle(box, radius=sc(4), fill="#FFFFFF",
                           outline="#D9E2E7", width=sc(1))
    if code == "tr":
        draw.rounded_rectangle(box, radius=sc(4), fill="#E30A17",
                               outline="#D9E2E7", width=sc(1))
        scale_x, scale_y = 42 / 512.0, 28 / 356.18
        outer_cx = inner_x + 178.084 * scale_x
        outer_cy = inner_y + 178.090 * scale_y
        outer_rx = 89.047 * scale_x
        outer_ry = 89.047 * scale_y
        draw.ellipse((sc(outer_cx - outer_rx), sc(outer_cy - outer_ry),
                      sc(outer_cx + outer_rx), sc(outer_cy + outer_ry)),
                     fill="#FFFFFF")
        inner_cx = inner_x + 200.345 * scale_x
        inner_cy = inner_y + 178.090 * scale_y
        inner_rx = 71.237 * scale_x
        inner_ry = 71.237 * scale_y
        draw.ellipse((sc(inner_cx - inner_rx), sc(inner_cy - inner_ry),
                      sc(inner_cx + inner_rx), sc(inner_cy + inner_ry)),
                     fill="#E30A17")
        star_raster = ((23.700, 9.800), (24.761, 12.539),
                       (27.694, 12.702), (25.418, 14.558),
                       (26.169, 17.398), (23.700, 15.806),
                       (21.231, 17.398), (21.982, 14.558),
                       (19.706, 12.702), (22.639, 12.539))
        star = [(sc(inner_x + px), sc(inner_y + py))
                for px, py in star_raster]
        draw.polygon(star, fill="#FFFFFF")
    elif code == "en":
        draw.rectangle((sc(inner_x), sc(inner_y), sc(inner_x + 42), sc(inner_y + 28)), fill="#234A91")
        draw.rectangle((sc(inner_x + 17), sc(inner_y), sc(inner_x + 25), sc(inner_y + 28)), fill="#FFFFFF")
        draw.rectangle((sc(inner_x), sc(inner_y + 9), sc(inner_x + 42), sc(inner_y + 19)), fill="#FFFFFF")
        draw.rectangle((sc(inner_x + 19), sc(inner_y), sc(inner_x + 23), sc(inner_y + 28)), fill="#D92332")
        draw.rectangle((sc(inner_x), sc(inner_y + 11), sc(inner_x + 42), sc(inner_y + 17)), fill="#D92332")
    elif code in {"nl", "de", "es", "pl"}:
        palettes = {
            "nl": ("#AE1C28", "#FFFFFF", "#21468B"),
            "de": ("#111111", "#DD1F2D", "#FFCE00"),
            "es": ("#AA151B", "#F1BF00", "#AA151B"),
            "pl": ("#FFFFFF", "#FFFFFF", "#DC143C"),
        }
        heights = (9, 10, 9) if code != "pl" else (0, 14, 14)
        offset = 0
        for color, height in zip(palettes[code], heights):
            if height:
                draw.rectangle((sc(inner_x), sc(inner_y + offset), sc(inner_x + 42), sc(inner_y + offset + height)), fill=color)
                offset += height
    elif code == "fr":
        for index, color in enumerate(("#1D3F8F", "#FFFFFF", "#ED2939")):
            draw.rectangle((sc(inner_x + index * 14), sc(inner_y), sc(inner_x + (index + 1) * 14), sc(inner_y + 28)), fill=color)


def speaker(draw, x, y):
    draw.rectangle((sc(x), sc(y + 10), sc(x + 6), sc(y + 18)), fill="#F4F8FA")
    draw.polygon([(sc(x + 6), sc(y + 10)), (sc(x + 14), sc(y + 4)),
                  (sc(x + 14), sc(y + 24)), (sc(x + 6), sc(y + 18))], fill="#F4F8FA")
    draw.arc((sc(x + 13), sc(y + 6), sc(x + 28), sc(y + 22)), -55, 55,
             fill="#F4F8FA", width=sc(2))


image = Image.new("RGB", (sc(WIDTH), sc(HEIGHT)), "#081119")
draw = ImageDraw.Draw(image)
text(draw, (512, 34), "DİL SEÇİN", 20, "#EAF6FF", bold=True)
languages = [("en", "English"), ("nl", "Nederlands"), ("tr", "Türkçe"),
             ("de", "Deutsch"), ("fr", "Français"), ("es", "Español"),
             ("pl", "Polski")]
for index, (code, name) in enumerate(languages):
    col, row = index % 2, index // 2
    x, y = 104 + col * 426, 82 + row * 106
    draw.rounded_rectangle((sc(x), sc(y), sc(x + 390), sc(y + 84)),
                           radius=sc(12), fill="#16834D" if code == "tr" else "#245A78")
    flag(draw, x + 20, y + 27, code)
    text(draw, (x + 74, y + 42), name, 20, "#FFFFFF", anchor="lm", bold=True)
draw.rounded_rectangle((sc(744), sc(506), sc(920), sc(572)),
                       radius=sc(10), fill="#596772")
text(draw, (832, 539), "GERİ", 20, "#FFFFFF", bold=True)
draw.rounded_rectangle((sc(970), sc(18), sc(1012), sc(60)),
                       radius=sc(8), fill="#294A5E", outline="#7790A0")
speaker(draw, 979, 25)

output = Path(__file__).resolve().parents[1] / "preview/language_screen_flags_fix20.png"
output.parent.mkdir(parents=True, exist_ok=True)
image.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS).save(output, optimize=True)
print(output)
