#!/usr/bin/env python3
"""Render the Fix20 JC1060 main menu reference image."""

from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

WIDTH, HEIGHT, SCALE = 1024, 600, 2


def sc(value):
    return round(value * SCALE)


def get_font(size, bold=False):
    name = "DejaVuSans-Bold.ttf" if bold else "DejaVuSans.ttf"
    return ImageFont.truetype(f"/usr/share/fonts/truetype/dejavu/{name}", sc(size))


def label(draw, xy, value, size, color, anchor="mm", bold=False):
    draw.text((sc(xy[0]), sc(xy[1])), value, font=get_font(size, bold),
              fill=color, anchor=anchor)


def button(draw, x, y, title, subtitle, enabled, color):
    outline = "#75A7BD" if enabled else "#52606A"
    draw.rounded_rectangle((sc(x), sc(y), sc(x + 312), sc(y + 118)),
                           radius=sc(12), fill=color, outline=outline,
                           width=sc(2))
    label(draw, (x + 156, y + 38), title, 14,
          "#FFFFFF" if enabled else "#9AA7AE", bold=True)
    label(draw, (x + 156, y + 91), subtitle, 9,
          "#D3E6EF" if enabled else "#7F8C94")


def speaker(draw, x, y):
    draw.rectangle((sc(x), sc(y + 10), sc(x + 6), sc(y + 18)), fill="#F4F8FA")
    draw.polygon([(sc(x + 6), sc(y + 10)), (sc(x + 14), sc(y + 4)),
                  (sc(x + 14), sc(y + 24)), (sc(x + 6), sc(y + 18))], fill="#F4F8FA")
    draw.arc((sc(x + 13), sc(y + 6), sc(x + 28), sc(y + 22)), -55, 55,
             fill="#F4F8FA", width=sc(2))


def globe(draw, x, y):
    draw.ellipse((sc(x), sc(y), sc(x + 28), sc(y + 28)),
                 outline="#EAF6FF", width=sc(2))
    draw.ellipse((sc(x + 7), sc(y + 1), sc(x + 21), sc(y + 27)),
                 outline="#EAF6FF", width=sc(1))
    for offset in (8, 13, 20):
        draw.line((sc(x + 2), sc(y + offset), sc(x + 26), sc(y + offset)),
                  fill="#EAF6FF", width=sc(1 if offset != 13 else 2))


def turkish_flag(draw, x, y):
    width, height = 44, 30
    inner_x, inner_y = x + 1, y + 1
    inner_width, inner_height = 42, 28
    draw.rounded_rectangle((sc(x), sc(y), sc(x + width), sc(y + height)),
                           radius=sc(4), fill="#E30A17",
                           outline="#D9E2E7", width=sc(1))

    # Exact proportions from the supplied 512 x 356.18 SVG path.
    scale_x = inner_width / 512.0
    scale_y = inner_height / 356.18
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


image = Image.new("RGB", (sc(WIDTH), sc(HEIGHT)), "#081119")
draw = ImageDraw.Draw(image)
draw.rectangle((0, 0, sc(WIDTH), sc(82)), fill="#142B3A")
label(draw, (512, 25), "MG SMART TEST - ANA MENÜ", 20, "#EAF6FF", bold=True)
label(draw, (512, 60), "Yapmak istediğiniz işlemi seçin", 13, "#9CC6DC")

items = [
    ("NORMAL KABLO TESTİ", "PE + ayarlanabilir pinler + dS", True, "#147A4B"),
    ("SUB-D KABLO TESTİ", "1..N + dS | Sub-D profili", True, "#315B8A"),
    ("ÖZEL HARİTA", "Henüz hazır değil", False, "#344957"),
    ("DUMMY TESTİ", "Henüz hazır değil", False, "#344957"),
    ("DOKUNMATİK DİAGNOSTİK", "GT911 beş noktalı ekran testi", True, "#167A9A"),
    ("KABLO ÖĞRENME", "Henüz hazır değil", False, "#344957"),
    ("QR / BARKOD İLE TEST", "Henüz hazır değil", False, "#344957"),
    ("RAPORLAR", "Henüz hazır değil", False, "#344957"),
    ("ÇOKLU KONNEKTÖR TESTİ", "Henüz hazır değil", False, "#344957"),
]
for index, item in enumerate(items):
    x = 22 + (index % 3) * 334
    y = 92 + (index // 3) * 136
    button(draw, x, y, *item)

draw.rounded_rectangle((sc(22), sc(512), sc(362), sc(566)), radius=sc(12),
                       fill="#6A4B8A", outline="#75A7BD", width=sc(2))
language_text = "Dil: Türkçe"
language_font = get_font(14, True)
text_width = draw.textlength(language_text, font=language_font) / SCALE
row_width = 28 + 8 + text_width + 8 + 44
row_x = 22 + (340 - row_width) / 2
globe(draw, row_x, 525)
label(draw, (row_x + 28 + 8, 539), language_text, 14, "#FFFFFF",
      anchor="lm", bold=True)
turkish_flag(draw, row_x + 28 + 8 + text_width + 8, 524)
label(draw, (687, 539), "JC1060 hazır | Harici 128 kanal kartı bağlı değil",
      9, "#F1C96A")
draw.rounded_rectangle((sc(970), sc(18), sc(1012), sc(60)), radius=sc(8),
                       fill="#294A5E", outline="#7790A0")
speaker(draw, 979, 25)

output = Path(__file__).resolve().parents[1] / "preview/main_menu_fix20.png"
output.parent.mkdir(parents=True, exist_ok=True)
image.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS).save(output, optimize=True)
print(output)
