from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
report = (root / "CompletionReportScreen.cpp").read_text(encoding="utf-8")

m = re.search(r"void CompletionReportScreen::refreshHeader\(\) \{(.*?)\n\}", report, re.S)
assert m, "refreshHeader() not found"
body = m.group(1)
test_branch = body.split("} else {", 1)[0]

checks = [
    ("test header keeps production PR", '"PR=%s%s"' in test_branch and "snap.identity.productionPr" in test_branch),
    ("demo marker remains visible", '" | DEMO/SIM"' in test_branch),
    ("REF removed from test header", "REF=" not in test_branch),
    ("REV removed from test header", "REV=" not in test_branch),
    ("PROFILE removed from test header", "PROFILE=" not in test_branch),
    ("TEST removed from test header", "TEST=" not in test_branch),
    ("electrical-net-map label removed from test header", "ELECTRICAL NET MAP" not in test_branch),
    ("one-to-one label removed from test header", "ONE-TO-ONE" not in test_branch),
    ("legacy identity formatter no longer used here", "formatIdentity" not in test_branch),
]

failed = [name for name, ok in checks if not ok]
for name, ok in checks:
    print(("PASS" if ok else "FAIL") + ": " + name)
if failed:
    raise SystemExit("Fix9 report-header contract failed: " + ", ".join(failed))
