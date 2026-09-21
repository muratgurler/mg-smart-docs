from pathlib import Path
import sys

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
archive_h = (root / "WorkflowArchiveCore.h").read_text(encoding="utf-8")
archive_cpp = (root / "WorkflowArchiveCore.cpp").read_text(encoding="utf-8")
controller_h = (root / "TestWorkflowController.h").read_text(encoding="utf-8")
controller_cpp = (root / "TestWorkflowController.cpp").read_text(encoding="utf-8")
overview = (root / "WorkflowOverviewScreen.cpp").read_text(encoding="utf-8")
history = (root / "WorkflowArchiveScreen.cpp").read_text(encoding="utf-8")
ino = (root / "MG_Test_ESP32P4_JC1060_Beta_1_0.ino").read_text(encoding="utf-8")

checks = {
    "fixed result ring": "kWorkflowResultCapacity = 32U" in archive_h,
    "fixed profile cache": "kWorkflowProfileCacheCapacity = 8U" in archive_h,
    "filesystem-independent archive core": "filesystem-independent" in archive_h,
    "completed result upsert": "upsertCompleted" in archive_h and "findResult" in archive_cpp,
    "late quality no duplicate": "archive_->upsertCompleted" in controller_cpp and "markQualityWarning" in controller_cpp,
    "result generation attempt identity": "generation" in archive_h and "attempt" in archive_h and "TestWorkflowIdentity identity" in archive_h,
    "map fingerprint": "fingerprintMap" in archive_h and "sameSideMaskA" in archive_cpp and "sameSideMaskB" in archive_cpp,
    "profile cache LRU": "lastUseOrdinal" in archive_h and "least-recently-used" in archive_cpp,
    "cache restore explicit source": "ProfileCache" in controller_h and "prepareCachedProfile" in controller_cpp,
    "cache clears PR": "cached.productionPr[0]" in controller_cpp,
    "retest increments attempt": "prepareRetest" in controller_cpp and "attempt + 1U" in controller_cpp,
    "next cable generation": "prepareNextCable" in controller_cpp and "generation += 1U" in controller_cpp,
    "PR next cable requires identifier": "NeedsNewIdentifier" in controller_cpp and "productionPr[0] = '\\0'" in controller_cpp,
    "no auto-start invariant": "autoStartRequested = false" in controller_h and "automatic START" in history,
    "overview records button": "KAYITLAR" in overview and "OpenHistory" in overview,
    "overview retest": "TEKRAR TEST" in overview and "WorkflowOverviewAction::Retest" in overview,
    "overview next cable": "SONRAKI KABLO" in overview and "WorkflowOverviewAction::NextCable" in overview,
    "history results profile modes": "TEST SONUCLARI" in history and "SON KULLANILAN PROFILLER" in history,
    "CSV report record": "formatRecordCsv" in archive_cpp,
    "PASS WARN FAIL totals": "passCount" in archive_cpp and "warningCount" in archive_cpp and "failCount" in archive_cpp,
    "archive wired in setup": "attachArchive(workflowArchive)" in ino,
    "archive screen wired": "ActiveScreen::WorkflowArchive" in ino and "showWorkflowArchive" in ino,
    "cache load scan ready": "loadCachedProfile" in ino and "prepareCachedProfile" in ino and "startScanUi" in ino,
    "retest scan ready": "prepareRetest" in ino and "waiting START" in ino,
    "generic prepared profile bridge": "loadPreparedProfile" in ino,
    "MCP23S17 comment current": "Real MCP23S17 hardware" in ino and "Real MCP23017 hardware" not in ino,
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"[{'PASS' if ok else 'FAIL'}] {name}")
if failed:
    raise SystemExit("Extra22 contract failed: " + ", ".join(failed))
print(f"Extra22 result/archive batch8 contract passed: {len(checks)}/{len(checks)}")
