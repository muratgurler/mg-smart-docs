#!/usr/bin/env python3
from pathlib import Path

project = Path(__file__).resolve().parents[1]
cpp = (project / "TouchControlPanel.cpp").read_text(encoding="utf-8")
session_cpp = (project / "ScanSession.cpp").read_text(encoding="utf-8")
hdr = (project / "TouchControlPanel.h").read_text(encoding="utf-8")
config = (project / "ConfigP4.h").read_text(encoding="utf-8")


def require(source: str, token: str, where: str) -> None:
    if token not in source:
        raise AssertionError(f"{where}: missing {token!r}")


for token in (
    "kSingleStepHoldThresholdMs = 1500UL",
    "kSingleStepHoldSpeedPercent = 50U",
):
    require(config, token, "ConfigP4.h")

for token in (
    "stepButtonCallback",
    "LV_EVENT_PRESSED",
    "LV_EVENT_PRESSING",
    "LV_EVENT_RELEASED",
    "LV_EVENT_PRESS_LOST",
    "LV_OBJ_FLAG_PRESS_LOCK",
    "beginStepPress()",
    "updateStepHold()",
    "endStepPress(bool releasedNormally)",
    "session_.pause();",
    "session_.setScanSpeedPercent(kSingleStepHoldSpeedPercent);",
    "session_.start();",
    "session_.setScanSpeedPercent(stepSavedSpeedPercent_);",
    "controller_.handle(ControlAction::Step);",
    "setButtonState(quickActionButtons_[1], stepHoldRunning_);",
):
    require(cpp + hdr, token, "TouchControlPanel")

# STEP must not be wired to the old generic CLICKED-only callback. Its own
# callback receives the full press lifecycle, while the other quick actions
# remain ordinary click actions.
require(cpp, "isSingleStep ? stepButtonCallback : actionCallback", "TouchControlPanel.cpp")
require(cpp, "isSingleStep ? LV_EVENT_ALL : LV_EVENT_CLICKED", "TouchControlPanel.cpp")

require(session_cpp, "dirty_ = true;", "ScanSession.cpp")

print("Single-step 1.5 s hold/jog regression contract passed")
