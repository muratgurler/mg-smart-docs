from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

checks = []

def check(name: str, condition: bool):
    checks.append((name, bool(condition)))
    print(("PASS" if condition else "FAIL"), name)

scan_h = (ROOT / "ScanSession.h").read_text(encoding="utf-8")
scan_cpp = (ROOT / "ScanSession.cpp").read_text(encoding="utf-8")
ui_cpp = (ROOT / "ScanScreen.cpp").read_text(encoding="utf-8")
map_h = (ROOT / "MapGraphScreen.h").read_text(encoding="utf-8")
map_cpp = (ROOT / "MapGraphScreen.cpp").read_text(encoding="utf-8")
report_h = (ROOT / "CompletionReportScreen.h").read_text(encoding="utf-8")
report_cpp = (ROOT / "CompletionReportScreen.cpp").read_text(encoding="utf-8")
app = (ROOT / "MG_Test_ESP32P4_JC1060_Beta_1_0.ino").read_text(encoding="utf-8")

check("live sender mask exposed", "displaySenderMask() const" in scan_h)
check("live receiver mask exposed", "displayReceiverMask() const" in scan_h)
check("classified result is kept for live UI", "displayResult() const" in scan_h)
check("scan mark stores full expected masks", "displaySenderMask_ = senderMask;" in scan_cpp and "displayReceiverMask_ = receiverMask;" in scan_cpp)
check("scan result stores full measured masks", "displaySenderMask_ = measurement.senderGroupMask;" in scan_cpp and "displayReceiverMask_ = measurement.actualReceiverMask;" in scan_cpp)
check("large pin numbers use electrical status color", "lv_obj_set_style_text_color(pinAValue_, liveColor" in ui_cpp and "lv_obj_set_style_text_color(pinBValue_, liveColor" in ui_cpp)
check("direction arrow uses live electrical status", "arrowColor = visualColor(liveStatus);" in ui_cpp)
check("live multi-pin path text exists", "formatLiveMask(session_.displaySenderMask()" in ui_cpp and "liveStatusLabel_" in ui_cpp)
check("live resistance is integrated", "displayResistanceMilliOhm" in ui_cpp and "R %.2f mOhm" in ui_cpp)

check("completion report exposes graph action", "Graph," in report_h)
check("test report has show graph button", "GRAFİKLE GÖSTER" in report_cpp and "CompletionReportAction::Graph" in report_cpp)
check("result graph reuses MapGraphScreen", "beginResults(const ScanSession& session)" in map_h and "void MapGraphScreen::beginResults" in map_cpp)
check("result graph supports expected measured combined", all(x in map_h for x in ["Combined", "Expected", "Measured"]))
check("filter threshold is strictly above 30", "profile_.activePointCount() > 30U" in map_cpp)
check("filters include all errors ok open short wrong high-r", all(x in map_h for x in ["All", "Errors", "Ok", "Open", "ShortCircuit", "WrongConnection", "HighResistance"]))
check("scan palette preserved in result graph", all(x in map_cpp for x in ["0x35D273", "0xE9EEF2", "0xFFD34D", "0xF04444", "0xFF922E"]))
check("combined view draws expected before measured", map_cpp.find("Expected geometry") < map_cpp.find("if (resultView_ != ResultGraphView::Expected)"))
check("wrong/open/short graph uses actual receiver masks", "m.actualReceiverMask" in map_cpp and "drawOpenStub" in map_cpp)
check("common net renderer uses branch buses", "drawNet" in map_cpp and "leftJoinX" in map_cpp and "rightJoinX" in map_cpp)
check("graph back returns to completion report", "returnsToCompletionReport()" in app and "completionReportScreen.activate();" in app)
check("completion graph action opens result graph", "mapGraphScreen.beginResults(scanSession);" in app)

# EXTRA31 Fix2 regression: the graph must not become active while its pin labels
# are still empty. The first Combined frame must use the same fully prepared
# state that a later EXPECTED/MEASURED button refresh would use.
build_start = map_cpp.find("void MapGraphScreen::build()")
build_end = map_cpp.find("void MapGraphScreen::formatPin", build_start)
build_body = map_cpp[build_start:build_end]
present_start = map_cpp.find("void MapGraphScreen::presentPreparedScreen()")
present_end = map_cpp.find("void MapGraphScreen::refreshResultControls", present_start)
present_body = map_cpp[present_start:present_end]
check("Fix2 build does not load half-built graph screen", "lv_scr_load(screen_)" not in build_body)
check("Fix2 initial graph refresh precedes presentation", "build();\n    refresh();\n    presentPreparedScreen();" in map_cpp)
check("Fix2 resolves layout before screen load", present_body.find("lv_obj_update_layout(screen_)") < present_body.find("lv_scr_load(screen_)"))
check("Fix2 invalidates complete screen after load", "lv_obj_invalidate(screen_);" in present_body)

# EXTRA31 Fix3 regression: JC1060/P4 full_refresh uses two native framebuffers.
# Both result graphs and editor/profile maps must automatically refresh both
# buffers after first presentation, without requiring operator interaction.
check("Fix3 exposes post-present refresh service", "servicePostPresentRefresh();" in map_h)
check("Stage03 Fix3 schedules two framebuffer refresh passes for every graph", "postPresentRefreshPasses_ = 2U;" in map_cpp and "resultMode_ ? 2U : 0U" not in map_cpp)
check("Fix3 post-present pass refreshes labels", "lv_obj_invalidate(leftLabels_[i])" in map_cpp and "lv_obj_invalidate(rightLabels_[i])" in map_cpp)
check("Fix3 map loop services deferred refresh before LVGL update", app.find("mapGraphScreen.servicePostPresentRefresh();") < app.find("displayPort.update();", app.find("if (activeScreen == ActiveScreen::MapGraph)")))

failed = [name for name, ok in checks if not ok]
if failed:
    raise SystemExit(f"Extra31 result graph contract FAILED: {failed}")
print(f"Extra31 result graph/live scan contract: PASS ({len(checks)} checks)")
