from pathlib import Path
import sys

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
h = (root / "WorkplaceProfileHttpClient.h").read_text(encoding="utf-8")
c = (root / "WorkplaceProfileHttpClient.cpp").read_text(encoding="utf-8")
a = (root / "WorkplaceProfileJsonAdapter.cpp").read_text(encoding="utf-8")
core = (root / "WorkplaceProfileLookupCore.cpp").read_text(encoding="utf-8")
cfg = (root / "ConfigP4.h").read_text(encoding="utf-8")
app = (root / "MG_Test_ESP32P4_JC1060_Beta_1_0.ino").read_text(encoding="utf-8")
checks = {
    "endpoint_has_PR_placeholder_contract": "{PR}" in h,
    "endpoint_default_not_guessed": 'kWorkplacePrEndpointTemplate = ""' in cfg,
    "percent_encoded_PR": "percentEncode(productionPr)" in c,
    "real_HTTP_GET": "http.GET()" in c,
    "requires_company_network": "ethernetHasIp()" in c and "wifiConnected()" in c,
    "document_AP_isolated": "documentApActive()" in c,
    "https_requires_CA": "TlsConfigurationRequired" in c and "setCACert" in c,
    "never_set_insecure": "setInsecure" not in c,
    "response_size_guard": "ResponseTooLarge" in c and "maxResponseBytes" in c,
    "canonical_connected_component_nets": "applyNet" in a and "sameSideA" in a and "sameSideB" in a,
    "duplicate_component_point_rejected": "assignedA & aMask" in a and "assignedB & bMask" in a,
    "bridge_is_single_ready_gate": "ProductionWorkflowBridge::apply" in core,
    "auto_start_stays_false": "autoStartRequested = false" in core and "autoStartRequested = false" in c,
    "http_failure_blocks": "markBlocked" in core,
    "application_owns_real_client": "WorkplaceProfileHttpClient workplaceProfileClient" in app,
    "application_configures_real_client": "workplaceProfileClient.configure" in app,
    "boot_reports_config_state": "[WORKPLACE] PR HTTP=" in app,
}
failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"{'PASS' if ok else 'FAIL'} {name}")
if failed:
    raise SystemExit("Batch12 contract failures: " + ", ".join(failed))
print(f"Extra26 workplace PR HTTP batch12 contract: {len(checks)}/{len(checks)} PASS")
