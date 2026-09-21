#!/usr/bin/env bash
set -euo pipefail

# Regenerates the compact LVGL BPP2 fonts used by the seven-language UI and
# the digits-only 84 px bold font used beside the direction arrow.
# Usage: ./tools/generate_ui_fonts.sh [/path/to/DejaVuSans.ttf] [/path/to/DejaVuSans-Bold.ttf]
project_dir="$(cd "$(dirname "$0")/.." && pwd)"
font_file="${1:-/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf}"
bold_font_file="${2:-/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf}"
symbols='ÇçĞğİıÖöŞşÜüÄäËëÏïßÀàÂâÆæÉéÈèÊêÎîÔôŒœÙùÛûŸÿÁáÍíÓóÚúÑñ¿¡ĄąĆćĘęŁłŃńŚśŹźŻż'

for size in 10 14 20; do
    npx --yes lv_font_conv \
        --font "$font_file" \
        --size "$size" \
        --bpp 2 \
        --format lvgl \
        --range 0x20-0x7e \
        --symbols "$symbols" \
        --font-name "mg_font_${size}" \
        --output "$project_dir/src/fonts/mg_font_${size}.c"
done

npx --yes lv_font_conv \
    --font "$bold_font_file" \
    --size 84 \
    --bpp 2 \
    --format lvgl \
    --symbols=-0123456789EPSd \
    --no-kerning \
    --lv-font-name mg_font_pin_84 \
    --output "$project_dir/src/fonts/mg_font_pin_84.c"
