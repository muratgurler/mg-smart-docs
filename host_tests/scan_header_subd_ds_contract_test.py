#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
scan = (root / "ScanScreen.cpp").read_text(encoding="utf-8")
controller = (root / "FrontPanelController.cpp").read_text(encoding="utf-8")
controller_h = (root / "FrontPanelController.h").read_text(encoding="utf-8")

# The free strip is x=526..846. Normal mode must use it without the old PE/dS
# header holes; Sub-D gets one wide profile button. Keep these exact geometry
# contracts so later feature merges cannot silently shrink the controls again.
required_scan = [
    'profileButton_ = createHeaderButton(526,\n                                        320,',
    'normalMinusButton_ = createHeaderButton(650,\n                                            54,',
    'normalCountButton_ = createHeaderButton(708,\n                                             80,',
    'normalPlusButton_ = createHeaderButton(792,\n                                           54,',
    'editMapButton_ = createHeaderButton(526,\n                                        120,',
    'lv_obj_set_pos(dsToggleButtonA_, subD ? 59 : 108, 255);',
    'lv_obj_set_pos(dsToggleButtonB_, subD ? 875 : 924, 255);',
    'lv_obj_clear_flag(dsToggleButtonA_, LV_OBJ_FLAG_HIDDEN);',
    'lv_obj_clear_flag(dsToggleButtonB_, LV_OBJ_FLAG_HIDDEN);',
]
for needle in required_scan:
    assert needle in scan, f"ScanScreen layout/dS regression: missing {needle!r}"

assert '                                    110,' in (root / 'TouchControlPanel.cpp').read_text(encoding='utf-8')

# PE stays a Normal-cable special point; Sub-D shell/drain remains independently
# switchable on A and B.
assert 'peToggleButtonA_, peToggleButtonB_' in scan
assert 'subDProfile_ = profileAt(profileIndex_);' in controller
assert controller.count('applySubDProfile();') >= 3
assert 'CableProfile subDProfile_ = defaultProfile();' in controller_h

for member in ('subDProfile_.includeDrainShieldA', 'subDProfile_.includeDrainShieldB'):
    assert member in controller, f"Sub-D dS toggle regression: missing {member}"

print('Scan header fill + Sub-D A/B dS toggle contract passed')
