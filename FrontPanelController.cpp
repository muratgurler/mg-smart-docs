#include "FrontPanelController.h"

#include "P4Sound.h"

namespace mg::p4 {

void FrontPanelController::handle(ControlAction action) {
    // The mapping deliberately mirrors the physical S3 front-panel functions.
    // Final GPIO binding belongs to the enclosure/PCB input board.
    switch (action) {
        case ControlAction::AutoStartPause:
            session_.startOrPause();
            break;
        case ControlAction::Step:
            session_.step();
            break;
        case ControlAction::Direction:
            session_.toggleDirection();
            break;
        case ControlAction::ResetAbort:
            session_.reset();
            break;
        case ControlAction::Mute:
            toggleP4SoundMuted();
            screen_.refreshSoundState();
            break;
        case ControlAction::SubDProfile:
            if (scanMode_ == ScanProfileKind::SubD) {
                profileIndex_ = (profileIndex_ + 1U) % profileCount();
                subDProfile_ = profileAt(profileIndex_);
                applySubDProfile();
            }
            break;
        case ControlAction::NextCable:
            // Workflow-level action: do not silently reset the current DUT.
            // Touch and future physical buttons both enter the same request
            // seam; the top-level controller decides whether the next cable
            // may reuse the profile or needs a fresh PR/barcode.
            nextCableRequested_ = true;
            session_.pause();
            break;
        case ControlAction::Retest:
            retestRequested_ = true;
            session_.pause();
            break;
        case ControlAction::Mode:
            selectScanMode(scanMode_ == ScanProfileKind::SubD
                               ? ScanProfileKind::Normal
                               : ScanProfileKind::SubD);
            break;
        case ControlAction::Standby:
            standby_ = !standby_;
            if (standby_) {
                session_.pause();
            }
            break;
        case ControlAction::StopOnError:
            session_.setStopOnError(!session_.stopOnError());
            break;
        case ControlAction::ContinuousMode:
            session_.setContinuousMode(!session_.continuousMode());
            break;
        case ControlAction::NormalPinsDecrease:
            if (scanMode_ == ScanProfileKind::Normal &&
                normalProfile_.signalPinCount > 1U) {
                setNormalSignalPinCount(
                    normalProfile_,
                    static_cast<uint8_t>(normalProfile_.signalPinCount - 1U));
                applyNormalProfile();
            }
            break;
        case ControlAction::NormalPinsIncrease:
            if (scanMode_ == ScanProfileKind::Normal &&
                normalProfile_.signalPinCount < kMaximumSignalPins) {
                setNormalSignalPinCount(
                    normalProfile_,
                    static_cast<uint8_t>(normalProfile_.signalPinCount + 1U));
                applyNormalProfile();
            }
            break;
        case ControlAction::NormalPinsNextPreset:
            if (scanMode_ == ScanProfileKind::Normal) {
                setNormalSignalPinCount(
                    normalProfile_,
                    nextNormalSignalPinPreset(normalProfile_.signalPinCount));
                applyNormalProfile();
            }
            break;
        case ControlAction::TogglePeA:
            if (scanMode_ == ScanProfileKind::Normal) {
                normalProfile_.includePeA = !normalProfile_.includePeA;
                normalProfile_.syncSpecialPointUnion();
                applyNormalProfile();
            }
            break;
        case ControlAction::ToggleDrainShieldA:
            if (scanMode_ == ScanProfileKind::Normal) {
                normalProfile_.includeDrainShieldA =
                    !normalProfile_.includeDrainShieldA;
                normalProfile_.syncSpecialPointUnion();
                applyNormalProfile();
            } else {
                subDProfile_.includeDrainShieldA =
                    !subDProfile_.includeDrainShieldA;
                subDProfile_.syncSpecialPointUnion();
                applySubDProfile();
            }
            break;
        case ControlAction::TogglePeB:
            if (scanMode_ == ScanProfileKind::Normal) {
                normalProfile_.includePeB = !normalProfile_.includePeB;
                normalProfile_.syncSpecialPointUnion();
                applyNormalProfile();
            }
            break;
        case ControlAction::ToggleDrainShieldB:
            if (scanMode_ == ScanProfileKind::Normal) {
                normalProfile_.includeDrainShieldB =
                    !normalProfile_.includeDrainShieldB;
                normalProfile_.syncSpecialPointUnion();
                applyNormalProfile();
            } else {
                subDProfile_.includeDrainShieldB =
                    !subDProfile_.includeDrainShieldB;
                subDProfile_.syncSpecialPointUnion();
                applySubDProfile();
            }
            break;
        case ControlAction::Diagnostic:
            diagnosticRequested_ = true;
            session_.pause();
            break;
    }
}

void FrontPanelController::selectScanMode(ScanProfileKind kind) {
    scanMode_ = kind;
    mode_ = scanMode_ == ScanProfileKind::SubD ? 1U : 0U;
    if (scanMode_ == ScanProfileKind::SubD) {
        session_.setProfile(subDProfile_);
    } else {
        session_.setProfile(normalProfile_);
    }
    profileChanged();
}

void FrontPanelController::loadDocumentProfile(const CableProfile& profile) {
    normalProfile_ = profile;
    scanMode_ = ScanProfileKind::Normal;
    mode_ = 0U;
    applyNormalProfile();
}

void FrontPanelController::loadPreparedProfile(const CableProfile& profile) {
    if (profile.kind == ScanProfileKind::SubD) {
        subDProfile_ = profile;
        scanMode_ = ScanProfileKind::SubD;
        mode_ = 1U;
        applySubDProfile();
    } else {
        normalProfile_ = profile;
        scanMode_ = ScanProfileKind::Normal;
        mode_ = 0U;
        applyNormalProfile();
    }
}


bool FrontPanelController::consumeNextCableRequest() {
    const bool requested = nextCableRequested_;
    nextCableRequested_ = false;
    return requested;
}

bool FrontPanelController::consumeRetestRequest() {
    const bool requested = retestRequested_;
    retestRequested_ = false;
    return requested;
}

bool FrontPanelController::muted() const {
    return p4SoundMuted();
}

void FrontPanelController::applyNormalProfile() {
    session_.setProfile(normalProfile_);
    profileChanged();
}

void FrontPanelController::applySubDProfile() {
    session_.setProfile(subDProfile_);
    profileChanged();
}

void FrontPanelController::profileChanged() {
    if (screen_.root() != nullptr) {
        screen_.onProfileChanged();
    }
}

}  // namespace mg::p4
