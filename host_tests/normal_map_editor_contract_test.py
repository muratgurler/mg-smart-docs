#!/usr/bin/env python3
from pathlib import Path

root = Path(__file__).resolve().parents[1]
main_menu_h = (root / 'MainMenuScreen.h').read_text(encoding='utf-8')
main_menu_cpp = (root / 'MainMenuScreen.cpp').read_text(encoding='utf-8')
scan_h = (root / 'ScanScreen.h').read_text(encoding='utf-8')
scan_cpp = (root / 'ScanScreen.cpp').read_text(encoding='utf-8')
touch_controls = (root / 'TouchControlPanel.cpp').read_text(encoding='utf-8')
editor_h = (root / 'MapEditorScreen.h').read_text(encoding='utf-8')
editor_cpp = (root / 'MapEditorScreen.cpp').read_text(encoding='utf-8')
ino = (root / 'MG_Test_ESP32P4_JC1060_Beta_1_0.ino').read_text(encoding='utf-8')

assert 'CustomMapScan' not in main_menu_h
assert 'MainMenuAction::CustomMapScan' not in main_menu_cpp
assert 'ProfileCommand::EditMap' in scan_cpp
assert 'editMapButton_' in scan_h
assert 'DÜZENLE' in scan_cpp
assert 'setScanButtonState(editMapButton_, session_.usesCustomMap())' in scan_cpp
assert 'editMapButton_ = createHeaderButton(526' in scan_cpp
assert 'const lv_coord_t quickX[4] = {850, 72, 168, 312};' in touch_controls
assert '                                    412,' in touch_controls
assert 'begin(const CableMap& source, const CableProfile& profile)' in editor_h
assert 'profile_.activePointCount()' in editor_cpp
assert 'visibleIndices_' in editor_h and 'visibleMask_' in editor_h
assert 'visibleCount_ <= 12U ? 4U' in editor_cpp
assert 'visibleCount_ <= 30U ? 6U' in editor_cpp
assert 'pointEnabledForSide' in editor_cpp
assert 'profile_.sidePointMask(targetSide()' in editor_cpp
assert 'mapEditorScreen.begin(customCableMap, controls.normalProfile())' in ino
assert 'case ScanScreen::ProfileCommand::EditMap:' in ino
assert 'scanSession.setCustomMap(customCableMap);' in ino
assert 'activeScreen = ActiveScreen::Scan;' in ino
assert 'mg::p4::build::kReleaseName' in ino

print('Normal-scan integrated custom-map editor contract passed.')
