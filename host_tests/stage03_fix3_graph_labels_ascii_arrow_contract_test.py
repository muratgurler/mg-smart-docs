from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
map_cpp = (ROOT / "MapGraphScreen.cpp").read_text(encoding="utf-8")
map_h = (ROOT / "MapGraphScreen.h").read_text(encoding="utf-8")
app = (ROOT / "MG_Test_ESP32P4_JC1060_Beta_1_0.ino").read_text(encoding="utf-8")

checks = []
def check(name, condition):
    ok = bool(condition)
    checks.append((name, ok))
    print(("PASS" if ok else "FAIL"), name)

check("editor/profile graph also schedules double-buffer refresh",
      "postPresentRefreshPasses_ = 2U;" in map_cpp and
      "postPresentRefreshPasses_ = resultMode_ ? 2U : 0U;" not in map_cpp)
check("post-present refresh re-runs graph label population",
      "refresh();" in map_cpp[map_cpp.find("void MapGraphScreen::servicePostPresentRefresh()"):map_cpp.find("lv_obj_t* MapGraphScreen::makeResultButton")])
check("both left and right pin labels are invalidated",
      "lv_obj_invalidate(leftLabels_[i])" in map_cpp and
      "lv_obj_invalidate(rightLabels_[i])" in map_cpp)
check("graph service runs before display update",
      app.find("mapGraphScreen.servicePostPresentRefresh();") >= 0 and
      app.find("mapGraphScreen.servicePostPresentRefresh();") < app.find("displayPort.update();", app.find("if (activeScreen == ActiveScreen::MapGraph)")))
check("all visible labels receive formatted pin text",
      "formatPin(testIndex, pin, sizeof(pin));" in map_cpp and
      "lv_label_set_text(leftLabels_[i], pin);" in map_cpp and
      "lv_label_set_text(rightLabels_[i], pin);" in map_cpp)
check("graph header contract remains 15 signal plus optional dS/PE capable",
      "profile_.activePointCount()" in map_cpp and 'snprintf(output, outputSize, "dS")' in map_cpp)

# Operator-facing source strings must not contain arrow glyphs absent from the compact fonts.
arrow_re = re.compile(r"[→↔←⇒⇔⟶⟷⟵⇄]")
offenders = []
for path in ROOT.rglob("*"):
    if path.suffix.lower() not in {".cpp", ".h", ".hpp", ".ino", ".c"}:
        continue
    text = path.read_text(encoding="utf-8")
    if arrow_re.search(text):
        offenders.append(path.relative_to(ROOT).as_posix())
check("unsupported Unicode arrows removed from firmware sources", not offenders)
if offenders:
    print("Arrow offenders:", offenders)

failed = [name for name, ok in checks if not ok]
if failed:
    raise SystemExit(f"Stage03 Fix3 graph-label/ASCII-arrow contract FAILED: {failed}")
print(f"Stage03 Fix3 graph-label/ASCII-arrow contract: PASS ({len(checks)} checks)")
