#!/usr/bin/env python3
from pathlib import Path
import sys
root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
ble = (root / 'MgBleProfileServer.cpp').read_text(encoding='utf-8')
validator = (root / 'DocumentImportValidator.cpp').read_text(encoding='utf-8')
service = (root / 'PreparedProfileService.cpp').read_text(encoding='utf-8')
checks = {
    'record moved off loopTask stack': 'std::unique_ptr<ProductionProfileRecord>' in ble and 'new (std::nothrow) ProductionProfileRecord' in ble,
    'CableMap moved off loopTask stack': 'std::unique_ptr<CableMap>' in ble and 'new (std::nothrow) CableMap' in ble,
    'allocation failure is fail-closed': 'fail("MEMORY", "Insufficient heap for profile validation", false)' in ble,
    'heap scratch diagnostic exists': '[BLE-PROFILE] validation scratch=HEAP' in ble,
    'terminal validation still precedes PROFILE_READY': ble.index('PreparedProfileService::loadAndValidate') < ble.index('notifyStatus("PROFILE_READY")'),
    'CRC stack buffer reduced': 'uint8_t buffer[256];' in validator and 'uint8_t buffer[1024];' not in validator,
    'large default CableMap temporary removed': 'map = makeDefaultCustomCableMap();' not in service and 'map.clear();' in service,
}
failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"{'PASS' if ok else 'FAIL'}: {name}")
if failed:
    raise SystemExit('Stage03 Fix1 stack contract failed: ' + ', '.join(failed))
print(f"Stage03 Fix1 stack-safety contract: {len(checks)}/{len(checks)} PASS")
