#!/usr/bin/env python3
"""Render the JC1060 Stage03 1024x600 reference screen without hardware."""

from __future__ import annotations

import argparse
from pathlib import Path

from PIL import Image, ImageColor, ImageDraw, ImageFont


SCALE = 3
WIDTH = 1024
HEIGHT = 600


COLORS = {
    "bg": "#0B1118",
    "header": "#142330",
    "panel": "#101A23",
    "border": "#294052",
    "text": "#EDF4F8",
    "muted": "#AEBFCA",
    "cyan": "#55C8FF",
    "scan": "#28BCE8",
    "ok": "#35D273",
    "open": "#E9EEF2",
    "short": "#FFD34D",
    "wrong": "#F04444",
    "resistance": "#FF922E",
    "untested": "#35434E",
    "metal": "#67717A",
    "gold": "#D5A640",
    "button_idle": "#505A60",
    "button_active": "#16834D",
}


def sc(value: float) -> int:
    return round(value * SCALE)


def font(size: int, bold: bool = False) -> ImageFont.FreeTypeFont:
    name = "DejaVuSans-Bold.ttf" if bold else "DejaVuSans.ttf"
    paths = [
        Path("/usr/share/fonts/truetype/dejavu") / name,
        Path("/usr/local/share/fontss") / name,
    ]
    for path in paths:
        if path.exists():
            return ImageFont.truetype(str(path), sc(size))
    return ImageFont.load_default()


def text(draw: ImageDraw.ImageDraw, xy, value, size, fill, *, anchor="la", bold=False):
    draw.text((sc(xy[0]), sc(xy[1])), value, font=font(size, bold), fill=fill, anchor=anchor)


def legend_row(draw: ImageDraw.ImageDraw):
    entries = [
        (COLORS["untested"], "BEKLİYOR"),
        (COLORS["scan"], "TARAMA"),
        (COLORS["ok"], "DOĞRU"),
        (COLORS["open"], "AÇIK DEVRE"),
        (COLORS["short"], "KISA DEVRE"),
        (COLORS["wrong"], "YANLIŞ BAĞLANTI"),
        (COLORS["resistance"], "YÜKSEK DİRENÇ"),
    ]
    label_font = font(14)
    square = 22
    item_widths = [
        square + 4 + draw.textlength(f": {label}", font=label_font) / SCALE
        for _, label in entries
    ]
    gap = max(2.0, (WIDTH - sum(item_widths)) / (len(entries) + 1))
    x = gap
    for (colour, label), item_width in zip(entries, item_widths):
        rounded(draw, (x, 572, x + square, 594), 2,
                colour, "#C6D1D8", 1)
        text(draw, (x + square + 4, 583), f": {label}", 14,
             COLORS["text"], anchor="lm")
        x += item_width + gap


def rounded(draw: ImageDraw.ImageDraw, box, radius, fill, outline=None, width=1):
    draw.rounded_rectangle(tuple(sc(v) for v in box), sc(radius), fill=fill,
                           outline=outline, width=sc(width))


def button_3d(image: Image.Image, draw: ImageDraw.ImageDraw, box, radius,
              tone="neutral"):
    """Approximate the shared LVGL vertical gradient, bevel and drop shadow."""
    tones = {
        "neutral": ("#68747B", "#3C464C", "#929EA5"),
        "active": ("#27AA67", "#0E6738", "#76E2AA"),
        "sound": ("#294A5E", "#294A5E", "#7790A0"),
    }
    top, bottom, border = tones[tone]
    x0, y0, x1, y1 = (sc(value) for value in box)
    rounded(draw, (box[0] + 1, box[1] + 3,
                   box[2] + 1, box[3] + 3), radius, "#030608")

    width = x1 - x0 + 1
    height = y1 - y0 + 1
    mask = Image.new("L", (width, height), 0)
    mask_draw = ImageDraw.Draw(mask)
    mask_draw.rounded_rectangle((0, 0, width - 1, height - 1),
                                radius=sc(radius), fill=255)
    gradient = Image.new("RGB", (width, height), top)
    gradient_draw = ImageDraw.Draw(gradient)
    top_rgb = ImageColor.getrgb(top)
    bottom_rgb = ImageColor.getrgb(bottom)
    for row in range(height):
        ratio = row / max(1, height - 1)
        colour = tuple(round(a + (b - a) * ratio)
                       for a, b in zip(top_rgb, bottom_rgb))
        gradient_draw.line((0, row, width - 1, row), fill=colour)
    image.paste(gradient, (x0, y0), mask)
    draw.rounded_rectangle((x0, y0, x1, y1), radius=sc(radius),
                           outline=border, width=sc(2))


def rounded_trapezoid(draw: ImageDraw.ImageDraw, x: int, y: int,
                      top_width: int, bottom_width: int, height: int,
                      radius: int, fill: str):
    """Mirror ConnectorFaceRenderer::fillRoundedTrapezoid in the preview."""
    for row in range(height):
        width = top_width + (bottom_width - top_width) * row // max(1, height - 1)
        inset = (top_width - width) // 2
        corner_trim = 0
        if radius > 0 and row < radius:
            distance = radius - row
            corner_trim = (distance * distance + radius - 1) // radius
        elif radius > 0 and row >= height - radius:
            distance = row - (height - radius - 1)
            corner_trim = (distance * distance + radius - 1) // radius
        rounded_width = max(1, width - 2 * corner_trim)
        draw.rectangle((sc(x + inset + corner_trim), sc(y + row),
                        sc(x + inset + corner_trim + rounded_width - 1),
                        sc(y + row)), fill=fill)


def special_points(draw: ImageDraw.ImageDraw, x: int, active_pin: int,
                   status_color: str, include_pe: bool, include_ds: bool):
    points = []
    if include_pe:
        points.append(("PE", 0))
    if include_ds:
        points.append(("dS", 63))
    centers = [x + 94] if len(points) == 1 else [x + 49, x + 139]
    for (label, test_index), cx in zip(points, centers):
        active = active_pin == test_index
        fill = status_color if active else "#26333E"
        rounded(draw, (cx - 29, 216, cx + 29, 241), 13, fill,
                "#FFFFFF" if active else "#647481", 2 if active else 1)
        text(draw, (cx, 228), label, 12, COLORS["text"], anchor="mm", bold=True)


def dsub_geometry(contacts: int):
    if contacts <= 9:
        flange_width = 132
    elif contacts <= 15:
        flange_width = 142
    elif contacts <= 25:
        flange_width = 154
    elif contacts <= 37:
        flange_width = 166
    else:
        flange_width = 168
    rows = 3 if contacts >= 44 else 2
    flange_height = 104 if rows == 3 else 94
    shell_top = flange_width - 28
    return {
        "rows": rows,
        "flange_x": (172 - flange_width) // 2,
        "flange_y": (136 - flange_height) // 2,
        "flange_width": flange_width,
        "flange_height": flange_height,
        "shell_x": (172 - shell_top) // 2,
        "shell_y": (136 - flange_height) // 2 + 10,
        "shell_top": shell_top,
        "shell_bottom": flange_width - 42,
        "shell_height": flange_height - 20,
    }


def dsub_row_counts(contacts: int, rows: int):
    if rows == 2:
        return [(contacts + 1) // 2, contacts // 2]
    if contacts == 50:
        return [17, 16, 17]
    base, remainder = divmod(contacts, 3)
    return [base + (1 if remainder > 0 else 0),
            base + (1 if remainder > 1 else 0), base]


def dsub_face(draw: ImageDraw.ImageDraw, x: int, gender: str,
              active_pin: int, status_color: str, contacts: int,
              include_pe: bool, include_ds: bool):
    rounded(draw, (x, 58, x + 188, 252), 10, COLORS["panel"], COLORS["border"])
    geometry = dsub_geometry(contacts)
    body_x, body_y = x + 8, 68
    flange_x = body_x + geometry["flange_x"]
    flange_y = body_y + geometry["flange_y"]
    flange_width = geometry["flange_width"]
    flange_height = geometry["flange_height"]
    rounded(draw, (flange_x, flange_y,
                   flange_x + flange_width, flange_y + flange_height),
            12, "#D6DCE0")
    rounded(draw, (flange_x + 3, flange_y + 3,
                   flange_x + flange_width - 3, flange_y + flange_height - 3),
            10, "#7A858D")

    shell_x = body_x + geometry["shell_x"]
    shell_y = body_y + geometry["shell_y"]
    shell_top = geometry["shell_top"]
    shell_bottom = geometry["shell_bottom"]
    shell_height = geometry["shell_height"]
    rounded_trapezoid(draw, shell_x, shell_y, shell_top, shell_bottom,
                      shell_height, 9, "#E7EBED")
    rounded_trapezoid(draw, shell_x + 4, shell_y + 4,
                      shell_top - 8, shell_bottom - 8,
                      shell_height - 8, 8, "#929CA3")
    rounded_trapezoid(draw, shell_x + 8, shell_y + 8,
                      shell_top - 16, shell_bottom - 16,
                      shell_height - 16, 10, "#252B30")

    screw_y = flange_y + flange_height / 2
    for cx in (flange_x + 11, flange_x + flange_width - 12):
        for radius, color in ((9, "#E4E8EA"), (6, "#5B656C"), (3, "#11161A")):
            draw.ellipse((sc(cx - radius), sc(screw_y - radius),
                          sc(cx + radius), sc(screw_y + radius)), fill=color)

    rows = geometry["rows"]
    counts = dsub_row_counts(contacts, rows)
    inner_top = shell_top - 16
    inner_bottom = shell_bottom - 16
    inner_height = shell_height - 16
    pin = 1
    for row, count in enumerate(counts):
        desired_pitch = 17 if contacts <= 9 else 14 if contacts <= 15 else \
            9 if contacts <= 25 else 7 if contacts <= 37 else 6
        row_y = shell_y + 8 + inner_height * \
            ((35 if row == 0 else 68) if rows == 2 else (27 + row * 23)) / 100
        relative_y = row_y - (shell_y + 8)
        available = inner_top + (inner_bottom - inner_top) * relative_y / max(1, inner_height - 1)
        maximum_pitch = (available - 4) / (count - 1) if count > 1 else desired_pitch
        pitch = min(desired_pitch, maximum_pitch)
        span = (count - 1) * pitch
        for col in range(count):
            px = body_x + (172 - span) / 2 + col * pitch
            if gender == "DİŞİ":
                px = body_x + 172 - ((172 - span) / 2 + col * pitch)
            is_active = pin == active_pin
            dot_color = status_color if is_active else (COLORS["gold"] if gender == "ERKEK" else "#151A1E")
            dot_size = 9 if contacts <= 15 else 7 if contacts <= 25 else 6 if contacts <= 37 else 5
            radius = dot_size / 2
            draw.ellipse((sc(px - radius), sc(row_y - radius),
                          sc(px + radius), sc(row_y + radius)),
                         fill=dot_color,
                         outline="#FFFFFF" if is_active else "#E0E7EB",
                         width=sc(2 if is_active else 1))
            pin += 1

    special_points(draw, x, active_pin, status_color, include_pe, include_ds)


def generic_face(draw: ImageDraw.ImageDraw, x: int, active_pin: int,
                 status_color: str, contacts: int,
                 include_pe: bool, include_ds: bool):
    rounded(draw, (x, 58, x + 188, 252), 10, COLORS["panel"], COLORS["border"])
    columns = min(16, contacts)
    for pin in range(1, contacts + 1):
        col, row = (pin - 1) % columns, (pin - 1) // columns
        px, py = x + 17 + col * 9, 89 + row * 20
        active = pin == active_pin
        radius = 5 if active else 3.5
        draw.ellipse((sc(px - radius), sc(py - radius), sc(px + radius), sc(py + radius)),
                     fill=status_color if active else "#D5A640",
                     outline="#FFFFFF" if active else "#E0E7EB",
                     width=sc(2 if active else 1))
    special_points(draw, x, active_pin, status_color, include_pe, include_ds)

def arrow(draw: ImageDraw.ImageDraw, direction: str):
    color = "#25D17B"
    if direction == "atob":
        points = [(536, 121), (620, 159), (536, 197), (536, 174),
                  (466, 174), (466, 144), (536, 144)]
    else:
        points = [(488, 121), (404, 159), (488, 197), (488, 174),
                  (558, 174), (558, 144), (488, 144)]
    draw.polygon([(sc(x), sc(y)) for x, y in points], fill=color)


def point_labels(signal_pins: int, include_pe: bool, include_ds: bool):
    return (["PE"] if include_pe else []) + \
        [str(i) for i in range(1, signal_pins + 1)] + \
        (["dS"] if include_ds else [])


def sound_icon(image: Image.Image, draw: ImageDraw.ImageDraw,
               muted: bool = False):
    button_3d(image, draw, (974, 4, 1016, 46), 8, "sound")
    draw.rectangle((sc(982), sc(21), sc(988), sc(29)), fill="#F4F8FA")
    draw.polygon([(sc(988), sc(21)), (sc(996), sc(15)), (sc(996), sc(35)),
                  (sc(988), sc(29))], fill="#F4F8FA")
    draw.arc((sc(995), sc(17), sc(1007), sc(33)), -55, 55, fill="#F4F8FA", width=sc(2))
    if muted:
        draw.line((sc(979), sc(10), sc(1010), sc(38)), fill="#F04444", width=sc(3))


def led_rows(draw: ImageDraw.ImageDraw, labels, current_slot: int,
             destination_slot: int, direction: str):
    active = len(labels)
    left, right = 48, 1010
    pitch = (right - left) / active
    gap = 1 if active <= 27 else 1
    bar_width = max(3, pitch - gap)
    bar_height = 96 if active <= 27 else 76
    y_a = 328 if active <= 27 else 338
    y_b = 451 if active <= 27 else 435
    number_size = 15 if active <= 27 else 10
    number_y = 436 if active <= 27 else 424

    text(draw, (24, y_a + bar_height / 2), "A", 18, COLORS["cyan"], anchor="mm", bold=True)
    text(draw, (24, y_b + bar_height / 2), "B", 18, COLORS["cyan"], anchor="mm", bold=True)

    for slot, label in enumerate(labels):
        cell_left = left + slot * pitch
        x = cell_left + (pitch - bar_width) / 2
        active_a = destination_slot if direction == "btoa" else current_slot
        active_b = current_slot if direction == "btoa" else destination_slot
        fill_a = COLORS["scan"] if slot == active_a else COLORS["untested"]
        fill_b = COLORS["scan"] if slot == active_b else COLORS["untested"]
        rounded(draw, (x, y_a, x + bar_width, y_a + bar_height), 3, fill_a, "#647481", 1)
        text(draw, (cell_left + pitch / 2, number_y), label, number_size, COLORS["text"], anchor="mm", bold=True)
        rounded(draw, (x, y_b, x + bar_width, y_b + bar_height), 3, fill_b, "#647481", 1)


def render(output: Path, direction: str, signal_pins: int, current_signal: int,
           state: str, continuous: bool, mode: str,
           include_pe: bool, include_ds: bool):
    image = Image.new("RGB", (sc(WIDTH), sc(HEIGHT)), COLORS["bg"])
    draw = ImageDraw.Draw(image)

    draw.rectangle((0, 0, sc(WIDTH), sc(50)), fill=COLORS["header"])
    if mode == "subd":
        # Sub-D has no PE contact. dS remains operator-selectable per side;
        # the reference renderer uses the same state on A/B for snapshots.
        include_pe = False
    labels = point_labels(signal_pins, include_pe, include_ds)
    active_points = len(labels)
    running = state == "running"
    quick = [
        (8, 60, "MENÜ", "neutral"),
        (72, 92, "TEK ADIM", "neutral"),
        (168, 140, "HATADA DUR", "neutral" if continuous else "active"),
        (312, 96, "NON-STOP", "active" if continuous else "neutral"),
        (412, 110, "SONRAKİ KABLO", "neutral"),
        (850, 118, "TARA / DURAKLAT",
         "active" if running else "neutral"),
    ]
    for x, width, label, tone in quick:
        button_3d(image, draw, (x, 4, x + width, 44), 8, tone)
        label_size = 10 if x == 850 else 13
        text(draw, (x + width / 2, 24), label, label_size,
             COLORS["text"], anchor="mm", bold=True)
    if mode == "subd":
        button_3d(image, draw, (526, 4, 846, 44), 8, "neutral")
        text(draw, (686, 24), f"Sub-D{signal_pins}", 13,
             COLORS["text"], anchor="mm", bold=True)
    else:
        controls = [(526, 120, "DÜZENLE"),
                    (650, 54, "-"),
                    (708, 80, f"{signal_pins} PIN"),
                    (792, 54, "+")]
        for x, width, value in controls:
            button_3d(image, draw, (x, 4, x + width, 44), 8, "neutral")
            text(draw, (x + width / 2, 24), value, 10 if x == 526 else 13,
                 COLORS["text"], anchor="mm", bold=True)
    sound_icon(image, draw)
    state_label = {"running": "TARANIYOR", "paused": "DURAKLATILDI",
                   "complete": "TAMAMLANDI"}[state]
    text(draw, (512, 65), state_label, 11, "#69D991", anchor="mm", bold=True)

    current_slot = current_signal if include_pe else current_signal - 1
    if mode == "subd":
        dsub_face(draw, 10, "ERKEK", current_signal, COLORS["scan"], signal_pins,
                  include_pe, include_ds)
        dsub_face(draw, 826, "DİŞİ", current_signal, COLORS["scan"], signal_pins,
                  include_pe, include_ds)
        # Only dS is meaningful on a Sub-D shell, so centre one switch below
        # each connector just like firmware refreshProfileControls().
        for x in (59, 875):
            button_3d(image, draw, (x, 255, x + 90, 283), 6,
                      "active" if include_ds else "neutral")
            text(draw, (x + 45, 269), "dS ON" if include_ds else "dS OFF", 10,
                 COLORS["text"], anchor="mm", bold=True)
    else:
        generic_face(draw, 10, current_signal, COLORS["scan"], signal_pins,
                     include_pe, include_ds)
        generic_face(draw, 826, current_signal, COLORS["scan"], signal_pins,
                     include_pe, include_ds)
        side_controls = [
            (10, "PE ON" if include_pe else "PE OFF", include_pe),
            (108, "dS ON" if include_ds else "dS OFF", include_ds),
            (826, "PE ON" if include_pe else "PE OFF", include_pe),
            (924, "dS ON" if include_ds else "dS OFF", include_ds),
        ]
        for x, value, enabled in side_controls:
            button_3d(image, draw, (x, 255, x + 90, 283), 6,
                      "active" if enabled else "neutral")
            text(draw, (x + 45, 269), value, 10, COLORS["text"],
                 anchor="mm", bold=True)

    text(draw, (292, 96), "A", 22, COLORS["cyan"], anchor="mm", bold=True)
    text(draw, (748, 96), "B", 22, COLORS["cyan"], anchor="mm", bold=True)
    text(draw, (307, 159), str(current_signal), 72, COLORS["text"], anchor="mm", bold=True)
    text(draw, (717, 159), str(current_signal), 72, COLORS["text"], anchor="mm", bold=True)
    arrow(draw, direction)
    phase = "A → B TARAMASI" if direction == "atob" else "B → A TARAMASI"
    text(draw, (512, 226), phase, 18, COLORS["muted"], anchor="mm", bold=True)
    text(draw, (230, 264), "TEST HIZI: 80%", 11, "#D6E4EC", anchor="lm", bold=True)
    rounded(draw, (420, 256, 810, 280), 9, "#263746")
    rounded(draw, (420, 256, 732, 280), 9, "#2DA9E8")
    rounded(draw, (720, 252, 744, 284), 12, "#FFD34D", "#FFFFFF", 2)

    draw.rectangle((0, sc(286), sc(1024), sc(288)), fill="#315064")
    total = active_points * 2
    completed = total if state == "complete" else current_slot + (active_points if direction == "btoa" else 0)
    percentage = round(completed * 100 / total)
    text(draw, (14, 307), f"{completed}/{total}   %{percentage}", 11, "#C5D2DB", anchor="lm", bold=True)
    rounded(draw, (155, 300, 1010, 314), 7, "#263746")
    rounded(draw, (155, 300, 155 + 855 * percentage / 100, 314), 7, COLORS["ok"])

    led_rows(draw, labels, current_slot, current_slot, direction)
    legend_row(draw)

    image = image.resize((WIDTH, HEIGHT), Image.Resampling.LANCZOS)
    output.parent.mkdir(parents=True, exist_ok=True)
    image.save(output, optimize=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--direction", choices=("atob", "btoa"), default="atob")
    parser.add_argument("--pins", type=int, default=25)
    parser.add_argument("--current", type=int, default=12)
    parser.add_argument("--state", choices=("running", "paused", "complete"),
                        default="running")
    parser.add_argument("--continuous", action="store_true")
    parser.add_argument("--mode", choices=("subd", "normal"), default="subd")
    parser.add_argument("--no-pe", action="store_true")
    parser.add_argument("--no-ds", action="store_true")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    if not 1 <= args.pins <= 62:
        parser.error("--pins must be between 1 and 62")
    if not 1 <= args.current <= args.pins:
        parser.error("--current must be within the selected signal pins")
    render(args.output, args.direction, args.pins, args.current, args.state,
           args.continuous, args.mode, not args.no_pe, not args.no_ds)


if __name__ == "__main__":
    main()
