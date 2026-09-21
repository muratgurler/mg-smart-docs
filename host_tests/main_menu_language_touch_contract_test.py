#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
src = (root / 'MainMenuScreen.cpp').read_text(encoding='utf-8')

required = (
    '                                                504,',
    'compact ? 72 : 118',
    'lv_obj_set_ext_click_area(button, 8);',
    'lv_obj_add_flag(button, LV_OBJ_FLAG_PRESS_LOCK);',
    'lv_obj_clear_flag(row, LV_OBJ_FLAG_CLICKABLE);',
    'lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);',
    'PERMANENT LANGUAGE TOUCH GUARD',
)
for token in required:
    if token not in src:
        raise AssertionError(
            f'MainMenuScreen.cpp: language touch regression guard missing: {token!r}'
        )

for forbidden in (
    'compact ? 54 : 118',
    '                                                512,',
):
    if forbidden in src:
        raise AssertionError(
            f'MainMenuScreen.cpp: old hard-to-touch language selector returned: {forbidden!r}'
        )

print('Main-menu language touch regression guard passed.')
