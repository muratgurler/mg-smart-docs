from pathlib import Path
import sys

root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
core_h = (root / "WorkflowArchiveCore.h").read_text(encoding="utf-8")
core_cpp = (root / "WorkflowArchiveCore.cpp").read_text(encoding="utf-8")
codec_h = (root / "WorkflowArchiveBinaryCodec.h").read_text(encoding="utf-8")
codec_cpp = (root / "WorkflowArchiveBinaryCodec.cpp").read_text(encoding="utf-8")
persist_h = (root / "WorkflowArchivePersistence.h").read_text(encoding="utf-8")
persist_cpp = (root / "WorkflowArchivePersistence.cpp").read_text(encoding="utf-8")
archive_screen = (root / "WorkflowArchiveScreen.cpp").read_text(encoding="utf-8")
archive_screen_h = (root / "WorkflowArchiveScreen.h").read_text(encoding="utf-8")
front_h = (root / "FrontPanelController.h").read_text(encoding="utf-8")
front_cpp = (root / "FrontPanelController.cpp").read_text(encoding="utf-8")
touch = (root / "TouchControlPanel.cpp").read_text(encoding="utf-8")
ino = (root / "MG_Test_ESP32P4_JC1060_Beta_1_0.ino").read_text(encoding="utf-8")

checks = {
    "32 result archive retained": "kWorkflowResultCapacity = 32U" in core_h,
    "8 profile cache retained": "kWorkflowProfileCacheCapacity = 8U" in core_h,
    "binary schema versioned": "kSchemaVersion = 1U" in codec_h and "kMagic" in codec_cpp,
    "CRC32 protected": "crc32" in codec_h and "0xEDB88320U" in codec_cpp,
    "logical oldest-newest serialization": "oldest -> newest" in codec_cpp,
    "map cross and same-side persisted": "receiverMaskAtoB" in codec_cpp and "sameSideMaskA" in codec_cpp and "sameSideMaskB" in codec_cpp,
    "decoded map fingerprint validated": "fingerprintMap(item.map) == item.mapFingerprint" in codec_cpp,
    "bounded binary size": "kWorkflowArchiveBinaryMaxBytes = 32768U" in core_h,
    "LittleFS persistent store": "LittleFS" in persist_cpp and "/mgdata/workflow_archive.bin" in persist_h,
    "no format-on-fail persistence mount": "LittleFS.begin(false)" in persist_cpp,
    "atomic temp rename": "workflow_archive.tmp" in persist_h and "LittleFS.rename" in persist_cpp,
    "deferred flash save": "kDeferredSaveMs = 1500U" in persist_cpp and "dirtySinceMs_" in persist_h,
    "boot archive restore": "workflowArchivePersistence.load" in ino,
    "background persistence tick": "workflowArchivePersistence.tick" in ino,
    "CSV all export": "results_all.csv" in persist_h and "exportAllCsv" in persist_cpp,
    "CSV selected export": "result_selected.csv" in persist_h and "exportRecordCsv" in persist_cpp,
    "CSV includes quality and fingerprint": "quality_warning,map_fingerprint" in persist_cpp and "record.qualityWarning" in core_cpp,
    "report summary mode": "URETIM OZETI" in archive_screen and "PRODUCTION SUMMARY" in archive_screen,
    "report FPY metric": "firstPassCount" in core_h and "firstAttemptCount" in core_h and "Window FPY" in archive_screen,
    "report retest metric": "retestCount" in core_h and "RETEST=%u" in archive_screen,
    "screen all CSV action": "ExportAllResultsCsv" in archive_screen_h and "TUM CSV" in archive_screen,
    "screen record CSV action": "ExportSelectedResultCsv" in archive_screen_h and "KAYDI CSV" in archive_screen,
    "profile load still separate": "LoadSelectedProfile" in archive_screen_h and "PROFILI YUKLE" in archive_screen,
    "front panel next-cable request seam": "consumeNextCableRequest" in front_h and "nextCableRequested_ = true" in front_cpp,
    "front panel retest request seam": "consumeRetestRequest" in front_h and "retestRequested_ = true" in front_cpp,
    "touch next cable gated by complete": "TestWorkflowState::Complete" in touch and "LV_STATE_DISABLED" in touch,
    "same next-cable helper used by reports": 'handleWorkflowNextCableRequest("REPORT_TOUCH")' in ino,
    "same next-cable helper used by front panel": 'handleWorkflowNextCableRequest("FRONT_PANEL")' in ino,
    "same retest helper used by reports": 'handleWorkflowRetestRequest("REPORT_TOUCH")' in ino,
    "front panel request is not silent session reset": "case ControlAction::NextCable" in front_cpp and "nextCableRequested_ = true" in front_cpp,
    "completed record saved before cycle change": "workflowArchivePersistence.saveNow" in ino and "prepareNextCable" in ino,
    "no auto-start invariant retained": "autoStartRequested = false" in (root / "TestWorkflowController.h").read_text(encoding="utf-8") and "waiting START" in ino,
    "GPIO5 untouched by this batch": "GPIO5" not in persist_cpp,
}

failed = [name for name, ok in checks.items() if not ok]
for name, ok in checks.items():
    print(f"[{'PASS' if ok else 'FAIL'}] {name}")
if failed:
    raise SystemExit("Extra23 contract failed: " + ", ".join(failed))
print(f"Extra23 persistence/report batch9 contract passed: {len(checks)}/{len(checks)}")
