#include "ScanSession.h"

namespace mg::p4 {

namespace {

constexpr uint64_t bitFor(uint8_t index) {
    return index < 64U ? (1ULL << index) : 0ULL;
}

uint8_t firstSetIndex(uint64_t mask) {
    for (uint8_t index = 0; index < 64U; ++index) {
        if ((mask & bitFor(index)) != 0ULL) {
            return index;
        }
    }
    return 0xFF;
}

}  // namespace

ScanSession::ScanSession(const CableProfile& profile, ScanSource& source)
    : profile_(&profile), source_(source) {
    reset();
}

void ScanSession::setProfile(const CableProfile& profile) {
    profile_ = &profile;
    reset();
}

void ScanSession::setOneToOneMap() {
    customMap_ = nullptr;
    reset();
}

void ScanSession::setCustomMap(const CableMap& map) {
    customMap_ = &map;
    reset();
}

void ScanSession::clearResults() {
    for (uint8_t slot = 0; slot < kTestPointsPerSide; ++slot) {
        visualA_[slot] = PointVisualStatus::Untested;
        visualB_[slot] = PointVisualStatus::Untested;
        reportAtoB_[slot] = Measurement{};
        reportBtoA_[slot] = Measurement{};
    }
    clearVisuals();
}

void ScanSession::clearVisuals() {
    for (uint8_t slot = 0; slot < kTestPointsPerSide; ++slot) {
        visualA_[slot] = PointVisualStatus::Untested;
        visualB_[slot] = PointVisualStatus::Untested;
    }
    displayTestIndexA_ = 0xFF;
    displayTestIndexB_ = 0xFF;
    activeStatusA_ = PointVisualStatus::Untested;
    activeStatusB_ = PointVisualStatus::Untested;
    displaySenderMask_ = 0ULL;
    displayReceiverMask_ = 0ULL;
    displayResult_ = ElectricalResult::NotMeasured;
    displayResistanceValid_ = false;
    displayResistanceMilliOhm_ = 0.0f;
}

void ScanSession::reset() {
    clearResults();
    runState_ = RunState::Idle;
    direction_ = ScanDirection::AtoB;
    displayDirection_ = ScanDirection::AtoB;
    currentSlot_ = firstEnabledSlot(ScanDirection::AtoB);
    completedMeasurements_ = 0;
    lastStepMs_ = 0;
    restartCycleOnStart_ = false;
    automaticReturnSweep_ = false;
    dirty_ = true;
}

void ScanSession::startOrPause() {
    if (runState_ == RunState::Running) {
        pause();
    } else {
        start();
    }
}

void ScanSession::start() {
    if (restartCycleOnStart_) {
        // NON-STOP + HATADA DUR stopped the production line on a faulty
        // cable. The operator has removed it; START must begin the next cable
        // from A->B/first contact, not resume halfway through the rejected one.
        beginNextContinuousCycle();
    } else if (runState_ == RunState::Complete) {
        // The just-completed reports remain available, but a new operator
        // cycle begins from the A fixture and discards the previous results.
        reset();
    }
    runState_ = RunState::Running;
    markCurrentScanning();
    dirty_ = true;
}

void ScanSession::pause() {
    if (runState_ == RunState::Running) {
        runState_ = RunState::Paused;
        dirty_ = true;
    }
}

void ScanSession::step() {
    if (restartCycleOnStart_) {
        // A manual first step after an error follows the same new-cable rule
        // as START, while leaving the session paused after that one step.
        beginNextContinuousCycle();
    } else if (runState_ == RunState::Complete) {
        reset();
    }
    markCurrentScanning();
    finishCurrentMeasurement();
    if (runState_ != RunState::Complete) {
        runState_ = RunState::Paused;
    }
    dirty_ = true;
}

void ScanSession::toggleDirection() {
    const uint8_t pointCount = profile_->activePointCount();
    if (pointCount == 0) {
        return;
    }

    const uint8_t targetSlot = directionChangeTargetSlot();
    direction_ = direction_ == ScanDirection::AtoB ? ScanDirection::BtoA
                                                    : ScanDirection::AtoB;
    automaticReturnSweep_ = false;
    displayDirection_ = direction_;
    currentSlot_ = targetSlot < pointCount ? targetSlot : currentSlot_;
    if (!slotEnabledForDirection(direction_, currentSlot_)) {
        currentSlot_ = direction_ == ScanDirection::AtoB
                           ? firstEnabledSlot(direction_)
                           : lastEnabledSlot(direction_);
    }
    completedMeasurements_ = 0;
    for (uint8_t slot = 0; slot < profile_->activePointCount(); ++slot) {
        if (reportAtoB_[slot].result != ElectricalResult::NotMeasured) {
            ++completedMeasurements_;
        }
        if (reportBtoA_[slot].result != ElectricalResult::NotMeasured) {
            ++completedMeasurements_;
        }
    }
    clearVisuals();
    if (runState_ == RunState::Running) {
        markCurrentScanning();
    } else {
        setIdleVisual(currentSlot_);
    }
    dirty_ = true;
}

void ScanSession::setStopOnError(bool enabled) {
    // HATADA DUR and NON-STOP are independent production options. Together
    // they scan good cables continuously but stop the line on the first fault.
    stopOnError_ = enabled;
    dirty_ = true;
}

void ScanSession::setContinuousMode(bool enabled) {
    continuousMode_ = enabled;
    dirty_ = true;
}

void ScanSession::setScanSpeedPercent(uint8_t percent) {
    if (percent < kMinimumTestSpeedPercent) {
        percent = kMinimumTestSpeedPercent;
    } else if (percent > kMaximumTestSpeedPercent) {
        percent = kMaximumTestSpeedPercent;
    }

    scanSpeedPercent_ = percent;
    if (percent <= kDefaultTestSpeedPercent) {
        const uint32_t speedPosition =
            static_cast<uint32_t>(percent - kMinimumTestSpeedPercent);
        const uint32_t speedRange =
            kDefaultTestSpeedPercent - kMinimumTestSpeedPercent;
        const uint32_t intervalRange =
            kSlowestScanStepIntervalMs - kDefaultScanStepIntervalMs;
        scanStepIntervalMs_ =
            kSlowestScanStepIntervalMs -
            speedPosition * intervalRange / speedRange;
    } else if (percent <= kRapidTestSpeedPercent) {
        const uint32_t speedPosition =
            static_cast<uint32_t>(percent - kDefaultTestSpeedPercent);
        const uint32_t speedRange =
            kRapidTestSpeedPercent - kDefaultTestSpeedPercent;
        const uint32_t intervalRange =
            kDefaultScanStepIntervalMs - kRapidScanStepIntervalMs;
        scanStepIntervalMs_ =
            kDefaultScanStepIntervalMs -
            speedPosition * intervalRange / speedRange;
    } else {
        const uint32_t speedPosition =
            static_cast<uint32_t>(percent - kRapidTestSpeedPercent);
        const uint32_t speedRange =
            kMaximumTestSpeedPercent - kRapidTestSpeedPercent;
        const uint32_t intervalRange =
            kRapidScanStepIntervalMs - kFastestScanStepIntervalMs;
        scanStepIntervalMs_ =
            kRapidScanStepIntervalMs -
            speedPosition * intervalRange / speedRange;
    }
    // Speed is visible on the scan screen. Mark the session dirty so
    // temporary TEK ADIM jog speed changes and the restored operator setting
    // are reflected immediately even if the scan completed while held.
    dirty_ = true;
}

void ScanSession::tick(uint32_t nowMs) {
    if (runState_ != RunState::Running ||
        nowMs - lastStepMs_ < scanStepIntervalMs_) {
        return;
    }
    lastStepMs_ = nowMs;
    finishCurrentMeasurement();
}

uint64_t ScanSession::activePointMask() const {
    uint64_t mask = 0ULL;
    for (uint8_t slot = 0; slot < profile_->activePointCount(); ++slot) {
        mask |= bitFor(profile_->slotToTestIndex(slot));
    }
    return mask;
}

uint64_t ScanSession::expectedReceiverMask(ScanDirection direction,
                                           uint8_t sourceTestIndex) const {
    uint64_t mask = bitFor(sourceTestIndex);
    if (customMap_ != nullptr) {
        mask = direction == ScanDirection::AtoB
                   ? customMap_->receiverMaskAtoB(sourceTestIndex)
                   : customMap_->receiverMaskBtoA(sourceTestIndex);
    }
    const bool receiverIsA = direction == ScanDirection::BtoA;
    return mask & activePointMask() & profile_->sidePointMask(receiverIsA);
}

uint64_t ScanSession::expectedSenderMask(ScanDirection direction,
                                         uint8_t sourceTestIndex) const {
    uint64_t mask = bitFor(sourceTestIndex);
    if (customMap_ != nullptr) {
        mask = direction == ScanDirection::AtoB
                   ? customMap_->senderMaskA(sourceTestIndex)
                   : customMap_->senderMaskB(sourceTestIndex);
    }
    const bool sourceIsA = direction == ScanDirection::AtoB;
    return mask & activePointMask() & profile_->sidePointMask(sourceIsA);
}

uint8_t ScanSession::expectedDisplayReceiver(ScanDirection direction,
                                             uint8_t sourceTestIndex) const {
    return firstSetIndex(expectedReceiverMask(direction, sourceTestIndex));
}

bool ScanSession::slotEnabledForDirection(ScanDirection direction, uint8_t slot) const {
    if (slot >= profile_->activePointCount()) {
        return false;
    }
    const uint8_t testIndex = profile_->slotToTestIndex(slot);
    const bool sourceIsA = direction == ScanDirection::AtoB;
    return profile_->pointEnabledOnSide(sourceIsA, testIndex);
}

uint8_t ScanSession::firstEnabledSlot(ScanDirection direction) const {
    for (uint8_t slot = 0; slot < profile_->activePointCount(); ++slot) {
        if (slotEnabledForDirection(direction, slot)) {
            return slot;
        }
    }
    return 0U;
}

uint8_t ScanSession::lastEnabledSlot(ScanDirection direction) const {
    const uint8_t count = profile_->activePointCount();
    for (uint8_t offset = 0; offset < count; ++offset) {
        const uint8_t slot = static_cast<uint8_t>(count - 1U - offset);
        if (slotEnabledForDirection(direction, slot)) {
            return slot;
        }
    }
    return 0U;
}

bool ScanSession::nextEnabledSlot(ScanDirection direction,
                                  uint8_t slot,
                                  uint8_t& nextSlot) const {
    const uint8_t count = profile_->activePointCount();
    if (direction == ScanDirection::AtoB) {
        for (uint8_t candidate = static_cast<uint8_t>(slot + 1U);
             candidate < count; ++candidate) {
            if (slotEnabledForDirection(direction, candidate)) {
                nextSlot = candidate;
                return true;
            }
        }
    } else {
        for (int16_t candidate = static_cast<int16_t>(slot) - 1; candidate >= 0; --candidate) {
            if (slotEnabledForDirection(direction, static_cast<uint8_t>(candidate))) {
                nextSlot = static_cast<uint8_t>(candidate);
                return true;
            }
        }
    }
    return false;
}

uint8_t ScanSession::directionChangeTargetSlot() const {
    const uint8_t pointCount = profile_->activePointCount();
    if (pointCount == 0) {
        return 0;
    }

    // Match the physical S3 direction key: reverse at the visible/measured
    // contact, never jump to PE or dS. A wrong connection such as A8 -> B9
    // therefore becomes B9 -> ... when the operator reverses direction.
    uint8_t targetTestIndex = direction_ == ScanDirection::AtoB
                                  ? displayTestIndexB_
                                  : displayTestIndexA_;
    uint8_t targetSlot = profile_->testIndexToSlot(targetTestIndex);
    if (targetSlot == 0xFF) {
        targetSlot = currentSlot_ < pointCount ? currentSlot_
                                               : static_cast<uint8_t>(pointCount - 1U);
    }
    return targetSlot;
}

void ScanSession::setIdleVisual(uint8_t slot) {
    if (slot >= profile_->activePointCount() ||
        !slotEnabledForDirection(direction_, slot)) {
        return;
    }
    const uint8_t sourceIndex = profile_->slotToTestIndex(slot);
    const uint8_t targetIndex = expectedDisplayReceiver(direction_, sourceIndex);
    const uint64_t senderMask = expectedSenderMask(direction_, sourceIndex);
    const uint64_t receiverMask = expectedReceiverMask(direction_, sourceIndex);

    if (direction_ == ScanDirection::AtoB) {
        displayTestIndexA_ = sourceIndex;
        displayTestIndexB_ = targetIndex;
    } else {
        displayTestIndexB_ = sourceIndex;
        displayTestIndexA_ = targetIndex;
    }

    for (uint8_t activeSlot = 0; activeSlot < profile_->activePointCount(); ++activeSlot) {
        const uint8_t index = profile_->slotToTestIndex(activeSlot);
        if ((senderMask & bitFor(index)) != 0ULL) {
            (direction_ == ScanDirection::AtoB ? visualA_[activeSlot]
                                               : visualB_[activeSlot]) =
                PointVisualStatus::Scanning;
        }
        if ((receiverMask & bitFor(index)) != 0ULL) {
            (direction_ == ScanDirection::AtoB ? visualB_[activeSlot]
                                               : visualA_[activeSlot]) =
                PointVisualStatus::Scanning;
        }
    }
    activeStatusA_ = PointVisualStatus::Scanning;
    activeStatusB_ = PointVisualStatus::Scanning;
}

uint8_t ScanSession::currentTestIndex() const {
    return profile_->slotToTestIndex(currentSlot_);
}

uint8_t ScanSession::totalMeasurements() const {
    uint8_t total = 0U;
    for (uint8_t slot = 0; slot < profile_->activePointCount(); ++slot) {
        if (slotEnabledForDirection(ScanDirection::AtoB, slot)) {
            ++total;
        }
        if (slotEnabledForDirection(ScanDirection::BtoA, slot)) {
            ++total;
        }
    }
    return total;
}

uint8_t ScanSession::progressPercent() const {
    const uint8_t total = totalMeasurements();
    return total == 0 ? 0
                      : static_cast<uint8_t>(
                            (static_cast<uint16_t>(completedMeasurements_) *
                             100U) /
                            total);
}

PointVisualStatus ScanSession::statusA(uint8_t slot) const {
    return slot < profile_->activePointCount()
               ? visualA_[slot]
               : PointVisualStatus::Untested;
}

PointVisualStatus ScanSession::statusB(uint8_t slot) const {
    return slot < profile_->activePointCount()
               ? visualB_[slot]
               : PointVisualStatus::Untested;
}

Measurement ScanSession::reportMeasurement(ScanDirection direction,
                                           uint8_t slot) const {
    if (slot >= profile_->activePointCount()) {
        return Measurement{};
    }
    return direction == ScanDirection::AtoB ? reportAtoB_[slot]
                                            : reportBtoA_[slot];
}

ElectricalResult ScanSession::reportResult(ScanDirection direction,
                                           uint8_t slot) const {
    return reportMeasurement(direction, slot).result;
}

uint8_t ScanSession::errorCount(ScanDirection direction) const {
    uint8_t count = 0;
    for (uint8_t slot = 0; slot < profile_->activePointCount(); ++slot) {
        const ElectricalResult result = reportResult(direction, slot);
        if (result != ElectricalResult::NotMeasured &&
            result != ElectricalResult::Ok) {
            ++count;
        }
    }
    return count;
}

bool ScanSession::consumeDirty() {
    const bool value = dirty_;
    dirty_ = false;
    return value;
}

void ScanSession::markCurrentScanning() {
    if (currentSlot_ >= profile_->activePointCount() ||
        !slotEnabledForDirection(direction_, currentSlot_)) {
        return;
    }
    clearVisuals();
    const uint8_t sourceIndex = currentTestIndex();
    const uint8_t targetIndex = expectedDisplayReceiver(direction_, sourceIndex);
    const uint64_t senderMask = expectedSenderMask(direction_, sourceIndex);
    const uint64_t receiverMask = expectedReceiverMask(direction_, sourceIndex);
    displayDirection_ = direction_;
    displaySenderMask_ = senderMask;
    displayReceiverMask_ = receiverMask;
    displayResult_ = ElectricalResult::NotMeasured;
    displayResistanceValid_ = false;
    displayResistanceMilliOhm_ = 0.0f;

    if (direction_ == ScanDirection::AtoB) {
        displayTestIndexA_ = sourceIndex;
        displayTestIndexB_ = targetIndex;
    } else {
        displayTestIndexB_ = sourceIndex;
        displayTestIndexA_ = targetIndex;
    }

    for (uint8_t slot = 0; slot < profile_->activePointCount(); ++slot) {
        const uint8_t index = profile_->slotToTestIndex(slot);
        if ((senderMask & bitFor(index)) != 0ULL) {
            (direction_ == ScanDirection::AtoB ? visualA_[slot]
                                               : visualB_[slot]) =
                PointVisualStatus::Scanning;
        }
        if ((receiverMask & bitFor(index)) != 0ULL) {
            (direction_ == ScanDirection::AtoB ? visualB_[slot]
                                               : visualA_[slot]) =
                PointVisualStatus::Scanning;
        }
    }
    activeStatusA_ = PointVisualStatus::Scanning;
    activeStatusB_ = PointVisualStatus::Scanning;
}

void ScanSession::showMeasurement(uint8_t sourceSlot,
                                  const Measurement& measurement) {
    clearVisuals();
    displayDirection_ = direction_;
    const PointVisualStatus resultVisual = toVisual(measurement.result);
    const uint8_t sourceIndex = profile_->slotToTestIndex(sourceSlot);
    displaySenderMask_ = measurement.senderGroupMask;
    displayReceiverMask_ = measurement.actualReceiverMask;
    displayResult_ = measurement.result;
    displayResistanceValid_ = measurement.resistanceValid;
    displayResistanceMilliOhm_ = measurement.resistanceMilliOhm;

    if (direction_ == ScanDirection::AtoB) {
        displayTestIndexA_ = sourceIndex;
        displayTestIndexB_ = measurement.actualTestIndex;
        activeStatusA_ = resultVisual;
        activeStatusB_ = measurement.actualReceiverMask == 0ULL
                             ? PointVisualStatus::Untested
                             : resultVisual;
    } else {
        displayTestIndexB_ = sourceIndex;
        displayTestIndexA_ = measurement.actualTestIndex;
        activeStatusB_ = resultVisual;
        activeStatusA_ = measurement.actualReceiverMask == 0ULL
                             ? PointVisualStatus::Untested
                             : resultVisual;
    }

    // Unlike the old single-destination P4 demo, render every pin contained
    // in the S3 masks. This matters for expected multi-target maps and shorts.
    for (uint8_t slot = 0; slot < profile_->activePointCount(); ++slot) {
        const uint8_t index = profile_->slotToTestIndex(slot);
        if ((measurement.senderGroupMask & bitFor(index)) != 0ULL) {
            (direction_ == ScanDirection::AtoB ? visualA_[slot]
                                               : visualB_[slot]) = resultVisual;
        }
        if ((measurement.actualReceiverMask & bitFor(index)) != 0ULL) {
            (direction_ == ScanDirection::AtoB ? visualB_[slot]
                                               : visualA_[slot]) = resultVisual;
        }
    }
}

void ScanSession::finishCurrentMeasurement() {
    if (currentSlot_ >= profile_->activePointCount() ||
        !slotEnabledForDirection(direction_, currentSlot_)) {
        runState_ = RunState::Complete;
        dirty_ = true;
        return;
    }

    const uint8_t testIndex = currentTestIndex();
    PhysicalScanObservation observed = source_.observe(direction_, testIndex);
    const uint64_t activeMask = activePointMask();
    observed.senderGroupMask &= activeMask;
    observed.receiverMask &= activeMask;
    const Measurement measurement = classifyObservation(
        testIndex,
        observed,
        expectedReceiverMask(direction_, testIndex),
        expectedSenderMask(direction_, testIndex));
    Measurement& report = direction_ == ScanDirection::AtoB
                              ? reportAtoB_[currentSlot_]
                              : reportBtoA_[currentSlot_];
    const bool firstMeasurement =
        report.result == ElectricalResult::NotMeasured;
    report = measurement;
    if (firstMeasurement && completedMeasurements_ < totalMeasurements()) {
        ++completedMeasurements_;
    }
    showMeasurement(currentSlot_, measurement);
    advanceAfterMeasurement(measurement.result);
    dirty_ = true;
}

void ScanSession::advanceAfterMeasurement(ElectricalResult result) {
    const bool failed = result != ElectricalResult::Ok &&
                        result != ElectricalResult::NotMeasured;

    if (continuousMode_ && stopOnError_ && failed) {
        restartCycleOnStart_ = true;
        runState_ = RunState::Paused;
        return;
    }

    bool hasNextPoint = false;
    uint8_t nextSlot = currentSlot_;

    if (nextEnabledSlot(direction_, currentSlot_, nextSlot)) {
        currentSlot_ = nextSlot;
        hasNextPoint = true;
    } else if (direction_ == ScanDirection::AtoB && kAutoReverseAfterAtoB) {
        direction_ = ScanDirection::BtoA;
        automaticReturnSweep_ = true;
        currentSlot_ = lastEnabledSlot(direction_);
        hasNextPoint = slotEnabledForDirection(direction_, currentSlot_);
    } else if (direction_ == ScanDirection::BtoA && !automaticReturnSweep_) {
        // Manual reverse reached the first enabled B-side point. Continue the
        // same running cycle from the first enabled A-side point.
        direction_ = ScanDirection::AtoB;
        automaticReturnSweep_ = false;
        currentSlot_ = firstEnabledSlot(direction_);
        hasNextPoint = slotEnabledForDirection(direction_, currentSlot_);
    }

    if (!hasNextPoint) {
        if (continuousMode_) {
            beginNextContinuousCycle();
        } else {
            runState_ = RunState::Complete;
            // Logical next-cycle state may return to A->B, but DO NOT overwrite
            // displayDirection_. showMeasurement() has just captured the final
            // B->A point (normally B1 -> A1). Keeping that display snapshot is
            // essential: the completion report is deliberately delayed long
            // enough for the operator to see the first return pin before the
            // screen changes. start()/reset() restores the next cycle to A->B.
            direction_ = ScanDirection::AtoB;
            currentSlot_ = firstEnabledSlot(direction_);
        }
    } else if (stopOnError_ && failed) {
        runState_ = RunState::Paused;
    }
}

void ScanSession::beginNextContinuousCycle() {
    // NON-STOP deliberately has no final aggregate report. Reuse the fixed
    // report storage for the next cable/cycle instead of accumulating data.
    for (uint8_t slot = 0; slot < kTestPointsPerSide; ++slot) {
        reportAtoB_[slot] = Measurement{};
        reportBtoA_[slot] = Measurement{};
    }
    completedMeasurements_ = 0;
    direction_ = ScanDirection::AtoB;
    displayDirection_ = ScanDirection::AtoB;
    currentSlot_ = firstEnabledSlot(direction_);
    automaticReturnSweep_ = false;
    restartCycleOnStart_ = false;
    runState_ = RunState::Running;
}

PointVisualStatus ScanSession::toVisual(ElectricalResult result) {
    switch (result) {
        case ElectricalResult::Ok:
            return PointVisualStatus::Ok;
        case ElectricalResult::Open:
            return PointVisualStatus::Open;
        case ElectricalResult::ShortCircuit:
            return PointVisualStatus::ShortCircuit;
        case ElectricalResult::WrongConnection:
            return PointVisualStatus::WrongConnection;
        case ElectricalResult::HighResistance:
            return PointVisualStatus::HighResistance;
        case ElectricalResult::NotMeasured:
        default:
            return PointVisualStatus::Untested;
    }
}

const char* runStateText(RunState state) {
    switch (state) {
        case RunState::Running:
            return "TARANIYOR";
        case RunState::Paused:
            return "DURAKLATILDI";
        case RunState::Complete:
            return "TAMAMLANDI";
        case RunState::Idle:
        default:
            return "HAZIR";
    }
}

const char* electricalResultText(ElectricalResult result) {
    switch (result) {
        case ElectricalResult::Ok:
            return "OK";
        case ElectricalResult::Open:
            return "ACIK";
        case ElectricalResult::ShortCircuit:
            return "KISA DEVRE";
        case ElectricalResult::WrongConnection:
            return "YANLIS BAGLANTI";
        case ElectricalResult::HighResistance:
            return "YUKSEK DIRENC";
        case ElectricalResult::NotMeasured:
        default:
            return "OLCULMEDI";
    }
}

}  // namespace mg::p4
