#pragma once

#include <stdint.h>

#include "ScanScreen.h"

namespace mg::p4 {

enum class ControlAction : uint8_t {
    AutoStartPause,
    Step,
    Direction,
    ResetAbort,
    Mute,
    SubDProfile,
    NextCable,
    Retest,
    Mode,
    Standby,
    StopOnError,
    ContinuousMode,
    NormalPinsDecrease,
    NormalPinsIncrease,
    NormalPinsNextPreset,
    TogglePeA,
    ToggleDrainShieldA,
    TogglePeB,
    ToggleDrainShieldB,
    Diagnostic,
};

class FrontPanelController {
public:
    FrontPanelController(ScanSession& session, ScanScreen& screen)
        : session_(session), screen_(screen) {}

    void handle(ControlAction action);
    void selectScanMode(ScanProfileKind kind);
    bool muted() const;
    bool standby() const { return standby_; }
    bool diagnosticRequested() const { return diagnosticRequested_; }
    bool consumeNextCableRequest();
    bool consumeRetestRequest();
    uint8_t mode() const { return mode_; }
    ScanProfileKind scanMode() const { return scanMode_; }
    const CableProfile& normalProfile() const { return normalProfile_; }
    // Loads an operator-approved document/import profile into the existing
    // NORMAL scan path.  The scan semantics stay identical to manual/custom
    // maps; only the profile source changes.
    void loadDocumentProfile(const CableProfile& profile);
    // Generic workflow/cache/retest bridge. Preserves the profile kind instead
    // of forcing every restored profile into the NORMAL path.
    void loadPreparedProfile(const CableProfile& profile);

private:
    void applyNormalProfile();
    void applySubDProfile();
    void profileChanged();

    ScanSession& session_;
    ScanScreen& screen_;
    CableProfile normalProfile_ = makeNormalProfile();
    // Runtime copy makes A/B dS independently switchable in Sub-D mode while
    // the built-in profile table remains immutable.
    CableProfile subDProfile_ = defaultProfile();
    size_t profileIndex_ = 0;  // Sub-D25 default.
    ScanProfileKind scanMode_ = ScanProfileKind::SubD;
    bool standby_ = false;
    bool diagnosticRequested_ = false;
    uint8_t mode_ = 0;
    bool nextCableRequested_ = false;
    bool retestRequested_ = false;
};

}  // namespace mg::p4
