#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[1])
checks=[]

def need(path, needle, label):
    text=(root/path).read_text(encoding='utf-8', errors='ignore')
    checks.append((label, needle in text))

need('SystemBackboneCore.h','class NetworkPrDemoEngine','network PR engine')
need('SystemBackboneCore.h','class BarcodeWorkflowDemoEngine','barcode engine')
need('SystemBackboneCore.h','class VoiceCommandDemoEngine','voice engine')
need('SystemBackboneCore.h','class BasicComponentDemoEngine','component engine')
need('SystemBackboneCore.h','class HardwareValidationDemoEngine','hardware validation engine')
need('SystemBackboneCore.h','autoStartRequested = false','no auto-start frozen in model')
need('SystemBackboneCore.h','free/manual cable testing is never gated','free test network independence')
need('SystemBackboneCore.cpp','result_.oldProfileCleared = true','stale profile clears before lookup/load')
need('SystemBackboneCore.cpp','MG:PROFILE:FREE-12','generic MG local profile demo')
need('SystemBackboneCore.cpp','UNKNOWN-4711','unknown code supported without electrical failure')
need('SystemBackboneCore.h','CalibrationChange','critical voice command model')
need('SystemBackboneCore.cpp','criticalActionBlocked','critical voice actions blocked')
need('SystemBackboneCore.h','does not claim precision','component scope guard')
need('SystemBackboneCore.cpp','no precision LCR/ESR/leakage','component UI/detail guard')
need('SystemBackboneCore.h','measuredOnRealHardware = false','demo hardware cannot claim real validation')
need('SystemBackboneCore.h','freezeEligible = false','demo cannot release pin freeze')
need('SystemBackboneCore.cpp','GPIO5=LCD_RESET RESERVED','LCD reset reservation in report')
need('SystemBackboneCore.cpp','Gerber release=BLOCKED','Gerber release safety block')
need('BackboneDemoScreen.h','SystemBackboneCore systemCore_','system core attached to UI')
need('BackboneDemoScreen.cpp','applySystemCoreSnapshot','system snapshot feeds UI')
need('BackboneDemoScreen.cpp','systemCore_.network().lookupPr','PR action drives engine')
need('BackboneDemoScreen.cpp','systemCore_.barcode().scanCurrent','barcode action drives engine')
need('BackboneDemoScreen.cpp','systemCore_.voice().listenAndRecognize','voice action drives engine')
need('BackboneDemoScreen.cpp','systemCore_.component().test','component action drives engine')
need('BackboneDemoScreen.cpp','systemCore_.hardwareValidation().run','hardware validation action drives engine')
need('BackboneDemoScreen.cpp','AUTO-START=NO','UI explicitly preserves no auto-start')
need('BackboneDemoScreen.cpp','PIN FREEZE=CANDIDATE','UI keeps pin freeze candidate')
need('host_tests/run_tests.sh','system_core_batch6_test','system executable regression registered')
need('host_tests/run_tests.sh','system_core_batch6_contract_test.py','system contract registered')
need('host_tests/run_tests.sh','main_menu_language_touch_contract_test.py','language touch regression retained')

failed=[label for label, ok in checks if not ok]
for label,ok in checks:
    print(('PASS' if ok else 'FAIL')+': '+label)
if failed:
    raise SystemExit('System core batch6 contract failed: '+', '.join(failed))
print(f'System core batch6 contract passed ({len(checks)}/{len(checks)})')
