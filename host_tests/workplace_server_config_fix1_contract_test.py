#!/usr/bin/env python3
from pathlib import Path
import sys

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]

def read(name): return (root / name).read_text(encoding='utf-8')

ino = read('MG_Test_ESP32P4_JC1060_Beta_1_0.ino')
settings_h = read('SettingsScreen.h')
settings_cpp = read('SettingsScreen.cpp')
screen = read('WorkplaceServerSettingsScreen.cpp')
store = read('WorkplaceServerConfigStore.cpp')
config = read('WorkplaceServerConfig.cpp')
http = read('WorkplaceProfileHttpClient.cpp')
adapter = read('ConfigurableWorkplaceJsonAdapter.cpp')
configp4 = read('ConfigP4.h')
run = read('host_tests/run_tests.sh')

checks = [
    ('settings menu exposes workplace server', 'WorkplaceServer' in settings_h and 'WORKPLACE SERVER' in settings_cpp),
    ('dedicated runtime settings screen', 'WorkplaceServerSettingsScreen' in ino and 'ActiveScreen::WorkplaceServerSettings' in ino),
    ('runtime config persists in NVS', 'mg-workplace' in store and 'Preferences' in store),
    ('boot loads saved config', 'workplaceServerConfigStore.load(workplaceServerConfig)' in ino),
    ('save reapplies client immediately', 'workplaceServerConfigStore.save(workplaceServerConfig)' in ino and 'applyWorkplaceServerConfig();' in ino),
    ('reset returns to safe defaults', 'workplaceServerConfigStore.clear()' in ino and 'defaultWorkplaceServerConfig()' in ino),
    ('endpoint remains blank by default', 'kWorkplacePrEndpointTemplate = ""' in configp4),
    ('endpoint requires PR placeholder', 'indexOf("{PR}")' in config),
    ('auth modes include none/basic/bearer/header', all(x in read('WorkplaceServerConfig.h') for x in ['None', 'Basic', 'Bearer', 'CustomHeader'])),
    ('basic auth is emitted', 'Authorization' in http and 'Basic ' in http),
    ('bearer auth is emitted', 'Bearer ' in http),
    ('custom auth header supported', 'customHeaderName' in http and 'addHeader(config_.customHeaderName' in http),
    ('https CA remains mandatory', 'HTTPS workplace endpoint has no CA certificate' in http and 'setInsecure' not in http),
    ('secret is masked in UI', '********' in screen and 'authSecret' not in ''.join(line for line in ino.splitlines() if 'Serial.' in line)),
    ('CA shown as size not contents', 'SET  (' in screen and 'tlsCaPem.c_str()' not in ''.join(line for line in ino.splitlines() if 'Serial.' in line)),
    ('response format can be changed', 'MappedJsonV1' in screen and 'CanonicalJsonV1' in screen),
    ('mapped JSON adapter exists', 'ConfigurableWorkplaceJsonAdapter' in adapter and 'normalizeMappedJson' in adapter),
    ('field map covers net A/B and identity', all(x in read('WorkplaceServerConfig.h') for x in ['productionPr', 'customerReference', 'revision', 'profileId', 'netA', 'netB'])),
    ('invalid duplicate field map is blocked', '*fields[i] == *fields[j]' in config),
    ('incomplete config stays CONFIG_REQUIRED', 'CONFIG_REQUIRED' in screen and 'validateWorkplaceServerConfig' in screen),
    ('manual CA editor supports multiline', 'Field::TlsCa' in screen and 'NL' in screen),
    ('manual URL editor supports braces', '"{"' in screen and '"}"' in screen),
    ('no auto start introduced', 'autoStart=0' in ino and 'autoStartRequested = false' in http),
    ('new executable test registered', 'workplace_server_config_fix1_test' in run),
    ('new contract test registered', 'workplace_server_config_fix1_contract_test.py' in run),
]

failed=[]
for name, ok in checks:
    print(('PASS' if ok else 'FAIL') + ': ' + name)
    if not ok: failed.append(name)
if failed:
    raise SystemExit(f'{len(failed)} checks failed')
print(f'Extra26 Fix1 runtime server config contract: {len(checks)}/{len(checks)} PASS')
