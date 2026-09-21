#include "DigitalBackboneCore.h"

#include <stdio.h>
#include <string.h>

namespace mg::p4 {
namespace {

constexpr uint64_t bitFor(uint8_t index) {
    return index < kTestPointsPerSide ? (1ULL << index) : 0ULL;
}


void appendText(char* output, size_t outputSize, const char* text) {
    if (output == nullptr || outputSize == 0U || text == nullptr) return;
    const size_t used = strlen(output);
    if (used + 1U >= outputSize) return;
    snprintf(output + used, outputSize - used, "%s", text);
}

void appendNodeList(char* output,
                    size_t outputSize,
                    const char* side,
                    uint64_t mask) {
    bool first = output[0] == '\0';
    for (uint8_t index = 0U; index < kTestPointsPerSide; ++index) {
        if ((mask & bitFor(index)) == 0ULL) continue;
        char token[16]{};
        if (!first) appendText(output, outputSize, ", ");
        if (index == kPeTestIndex) snprintf(token, sizeof(token), "%sPE", side);
        else if (index == kDrainShieldTestIndex) snprintf(token, sizeof(token), "%sdS", side);
        else snprintf(token, sizeof(token), "%s%u", side, static_cast<unsigned>(index));
        appendText(output, outputSize, token);
        first = false;
    }
}

const char* resultWord(DigitalWorkflowState state) {
    switch (state) {
        case DigitalWorkflowState::Complete: return "PASS";
        case DigitalWorkflowState::Fault: return "FAIL";
        case DigitalWorkflowState::Running: return "RUN";
        case DigitalWorkflowState::Paused: return "PAUSE";
        case DigitalWorkflowState::Idle: default: return "READY";
    }
}

}  // namespace

void DigitalSweepRunner::configure(const CableProfile& profile,
                                   ScanSource& source,
                                   const CableMap* expectedMap) {
    profile_ = &profile;
    source_ = &source;
    expectedMap_ = expectedMap;
    reset();
}

void DigitalSweepRunner::reset() {
    state_ = DigitalWorkflowState::Idle;
    stats_ = DigitalSweepStats{};
    reportEntryCount_ = 0U;
    for (auto& entry : reportEntries_) entry = DigitalSweepReportEntry{};
    cursorDirection_ = ScanDirection::AtoB;
    cursorIndex_ = 0U;
    lastStepMs_ = 0U;
    if (profile_ != nullptr) {
        for (uint8_t i = 0U; i < kTestPointsPerSide; ++i) {
            if (pointEnabled(ScanDirection::AtoB, i)) ++stats_.total;
            if (pointEnabled(ScanDirection::BtoA, i)) ++stats_.total;
        }
    }
}

void DigitalSweepRunner::start() {
    if (profile_ == nullptr || source_ == nullptr || stats_.total == 0U) {
        state_ = DigitalWorkflowState::Fault;
        return;
    }
    if (state_ == DigitalWorkflowState::Complete || state_ == DigitalWorkflowState::Fault) {
        reset();
    }
    state_ = DigitalWorkflowState::Running;
}

void DigitalSweepRunner::pauseResume() {
    if (state_ == DigitalWorkflowState::Running) state_ = DigitalWorkflowState::Paused;
    else if (state_ == DigitalWorkflowState::Paused) state_ = DigitalWorkflowState::Running;
    else start();
}

bool DigitalSweepRunner::pointEnabled(ScanDirection direction, uint8_t index) const {
    if (profile_ == nullptr || index >= kTestPointsPerSide) return false;
    return profile_->pointEnabledOnSide(direction == ScanDirection::AtoB, index);
}

uint64_t DigitalSweepRunner::expectedReceiverMask(ScanDirection direction,
                                                   uint8_t source) const {
    if (profile_ == nullptr) return 0ULL;
    uint64_t mask = expectedMap_ == nullptr
                        ? bitFor(source)
                        : (direction == ScanDirection::AtoB
                               ? expectedMap_->receiverMaskAtoB(source)
                               : expectedMap_->receiverMaskBtoA(source));
    const bool receiverIsA = direction == ScanDirection::BtoA;
    return mask & profile_->sidePointMask(receiverIsA);
}

uint64_t DigitalSweepRunner::expectedSenderMask(ScanDirection direction,
                                                 uint8_t source) const {
    if (profile_ == nullptr) return 0ULL;
    uint64_t mask = expectedMap_ == nullptr
                        ? bitFor(source)
                        : (direction == ScanDirection::AtoB
                               ? expectedMap_->senderMaskA(source)
                               : expectedMap_->senderMaskB(source));
    const bool sourceIsA = direction == ScanDirection::AtoB;
    return mask & profile_->sidePointMask(sourceIsA);
}

bool DigitalSweepRunner::nextPoint(ScanDirection& direction, uint8_t& index) const {
    if (profile_ == nullptr) return false;
    ScanDirection d = direction;
    uint8_t start = index;
    for (uint16_t attempt = 0U; attempt < 128U; ++attempt) {
        if (pointEnabled(d, start)) {
            direction = d;
            index = start;
            return true;
        }
        if (start < 63U) {
            ++start;
        } else if (d == ScanDirection::AtoB) {
            d = ScanDirection::BtoA;
            start = 0U;
        } else {
            return false;
        }
    }
    return false;
}

void DigitalSweepRunner::tally(const PhysicalScanObservation& observation,
                               const Measurement& measurement,
                               ScanDirection direction,
                               uint8_t source) {
    ++stats_.completed;
    stats_.lastDirection = direction;
    stats_.lastSource = source;
    stats_.lastMeasurement = measurement;

    const uint64_t expectedSender = expectedSenderMask(direction, source);
    if (reportEntryCount_ < (kTestPointsPerSide * 2U)) {
        auto& entry = reportEntries_[reportEntryCount_++];
        entry.direction = direction;
        entry.source = source;
        entry.expectedReceiverMask = expectedReceiverMask(direction, source);
        entry.expectedSenderMask = expectedSender;
        entry.observation = observation;
        entry.measurement = measurement;
    }
    if (observation.senderGroupMask != expectedSender) ++stats_.sameSideMismatch;

    switch (measurement.result) {
        case ElectricalResult::Ok: ++stats_.ok; break;
        case ElectricalResult::Open: ++stats_.open; break;
        case ElectricalResult::ShortCircuit: ++stats_.shortCircuit; break;
        case ElectricalResult::WrongConnection: ++stats_.wrongConnection; break;
        case ElectricalResult::HighResistance: ++stats_.highResistance; break;
        case ElectricalResult::NotMeasured: break;
    }
}

bool DigitalSweepRunner::step() {
    if (profile_ == nullptr || source_ == nullptr) {
        state_ = DigitalWorkflowState::Fault;
        return false;
    }
    if (state_ == DigitalWorkflowState::Complete || state_ == DigitalWorkflowState::Fault) return false;
    if (state_ == DigitalWorkflowState::Idle) state_ = DigitalWorkflowState::Paused;

    ScanDirection direction = cursorDirection_;
    uint8_t sourceIndex = cursorIndex_;
    if (!nextPoint(direction, sourceIndex)) {
        state_ = stats_.errorCount() == 0U ? DigitalWorkflowState::Complete
                                           : DigitalWorkflowState::Fault;
        return false;
    }

    const PhysicalScanObservation observation = source_->observe(direction, sourceIndex);
    const Measurement measurement = classifyObservation(
        sourceIndex,
        observation,
        expectedReceiverMask(direction, sourceIndex),
        expectedSenderMask(direction, sourceIndex));
    tally(observation, measurement, direction, sourceIndex);

    cursorDirection_ = direction;
    cursorIndex_ = sourceIndex;
    if (cursorIndex_ < 63U) {
        ++cursorIndex_;
    } else if (cursorDirection_ == ScanDirection::AtoB) {
        cursorDirection_ = ScanDirection::BtoA;
        cursorIndex_ = 0U;
    } else {
        cursorIndex_ = 64U;
    }

    if (stats_.completed >= stats_.total) {
        state_ = stats_.errorCount() == 0U ? DigitalWorkflowState::Complete
                                           : DigitalWorkflowState::Fault;
    }
    return true;
}

void DigitalSweepRunner::tick(uint32_t nowMs, uint32_t intervalMs) {
    if (state_ != DigitalWorkflowState::Running) return;
    if (nowMs - lastStepMs_ < intervalMs) return;
    lastStepMs_ = nowMs;
    step();
    if (state_ == DigitalWorkflowState::Paused) state_ = DigitalWorkflowState::Running;
}

const DigitalSweepReportEntry& DigitalSweepRunner::reportEntry(size_t index) const {
    static const DigitalSweepReportEntry empty{};
    return index < reportEntryCount_ ? reportEntries_[index] : empty;
}

uint8_t DigitalSweepRunner::progressPercent() const {
    return stats_.total == 0U ? 0U
        : static_cast<uint8_t>((static_cast<uint32_t>(stats_.completed) * 100U) /
                               stats_.total);
}

void CableLearnEngine::configure(const CableProfile& profile, ScanSource& source) {
    profile_ = &profile;
    source_ = &source;
    reset();
}

void CableLearnEngine::reset() {
    state_ = DigitalWorkflowState::Idle;
    cursorDirection_ = ScanDirection::AtoB;
    cursorIndex_ = 0U;
    completed_ = 0U;
    total_ = 0U;
    lastStepMs_ = 0U;
    memset(aToB_, 0, sizeof(aToB_));
    memset(sameA_, 0, sizeof(sameA_));
    memset(sameB_, 0, sizeof(sameB_));
    learnedMap_.clear();
    learnedMap_.setName("LEARNED DEMO");
    learnedNetCount_ = 0U;
    for (auto& net : learnedNets_) net = CableNet{};
    if (profile_ != nullptr) {
        for (uint8_t i = 0U; i < kTestPointsPerSide; ++i) {
            if (pointEnabled(ScanDirection::AtoB, i)) ++total_;
            if (pointEnabled(ScanDirection::BtoA, i)) ++total_;
        }
    }
}

void CableLearnEngine::start() {
    if (profile_ == nullptr || source_ == nullptr || total_ == 0U) {
        state_ = DigitalWorkflowState::Fault;
        return;
    }
    if (state_ == DigitalWorkflowState::Complete || state_ == DigitalWorkflowState::Fault) reset();
    state_ = DigitalWorkflowState::Running;
}

void CableLearnEngine::pauseResume() {
    if (state_ == DigitalWorkflowState::Running) state_ = DigitalWorkflowState::Paused;
    else if (state_ == DigitalWorkflowState::Paused) state_ = DigitalWorkflowState::Running;
    else start();
}

bool CableLearnEngine::pointEnabled(ScanDirection direction, uint8_t index) const {
    if (profile_ == nullptr || index >= kTestPointsPerSide) return false;
    return profile_->pointEnabledOnSide(direction == ScanDirection::AtoB, index);
}

bool CableLearnEngine::nextPoint(ScanDirection& direction, uint8_t& index) const {
    ScanDirection d = direction;
    uint8_t candidate = index;
    for (uint16_t attempt = 0U; attempt < 128U; ++attempt) {
        if (pointEnabled(d, candidate)) {
            direction = d;
            index = candidate;
            return true;
        }
        if (candidate < 63U) ++candidate;
        else if (d == ScanDirection::AtoB) {
            d = ScanDirection::BtoA;
            candidate = 0U;
        } else return false;
    }
    return false;
}

bool CableLearnEngine::step() {
    if (profile_ == nullptr || source_ == nullptr) {
        state_ = DigitalWorkflowState::Fault;
        return false;
    }
    if (state_ == DigitalWorkflowState::Complete || state_ == DigitalWorkflowState::Fault) return false;
    if (state_ == DigitalWorkflowState::Idle) state_ = DigitalWorkflowState::Paused;

    ScanDirection direction = cursorDirection_;
    uint8_t sourceIndex = cursorIndex_;
    if (!nextPoint(direction, sourceIndex)) {
        finalizeMap();
        state_ = DigitalWorkflowState::Complete;
        return false;
    }

    const PhysicalScanObservation observation = source_->observe(direction, sourceIndex);
    if (direction == ScanDirection::AtoB) {
        aToB_[sourceIndex] |= observation.receiverMask;
        sameA_[sourceIndex] |= observation.senderGroupMask & ~bitFor(sourceIndex);
    } else {
        sameB_[sourceIndex] |= observation.senderGroupMask & ~bitFor(sourceIndex);
        for (uint8_t a = 0U; a < kTestPointsPerSide; ++a) {
            if ((observation.receiverMask & bitFor(a)) != 0ULL) {
                aToB_[a] |= bitFor(sourceIndex);
            }
        }
    }
    ++completed_;

    cursorDirection_ = direction;
    cursorIndex_ = sourceIndex;
    if (cursorIndex_ < 63U) ++cursorIndex_;
    else if (cursorDirection_ == ScanDirection::AtoB) {
        cursorDirection_ = ScanDirection::BtoA;
        cursorIndex_ = 0U;
    } else cursorIndex_ = 64U;

    if (completed_ >= total_) {
        finalizeMap();
        state_ = DigitalWorkflowState::Complete;
    }
    return true;
}

void CableLearnEngine::tick(uint32_t nowMs, uint32_t intervalMs) {
    if (state_ != DigitalWorkflowState::Running) return;
    if (nowMs - lastStepMs_ < intervalMs) return;
    lastStepMs_ = nowMs;
    step();
    if (state_ == DigitalWorkflowState::Paused) state_ = DigitalWorkflowState::Running;
}

void CableLearnEngine::finalizeMap() {
    learnedMap_.loadRaw(aToB_, sameA_, sameB_, "LEARNED DEMO");
    const uint64_t visibleA = profile_ != nullptr ? profile_->sidePointMask(true) : ~0ULL;
    const uint64_t visibleB = profile_ != nullptr ? profile_->sidePointMask(false) : ~0ULL;
    learnedNetCount_ = CableNetGraph::build(learnedMap_, visibleA, visibleB,
                                             learnedNets_, kTestPointsPerSide);
}

uint8_t CableLearnEngine::progressPercent() const {
    return total_ == 0U ? 0U
        : static_cast<uint8_t>((static_cast<uint32_t>(completed_) * 100U) / total_);
}

void CableLearnEngine::formatSummary(char* output, size_t outputSize) const {
    if (output == nullptr || outputSize == 0U) return;
    snprintf(output, outputSize, "%u/%u observations | %u electrical nets",
             static_cast<unsigned>(completed_), static_cast<unsigned>(total_),
             static_cast<unsigned>(learnedNetCount_));
}

void SmartProbeEngine::configure(const CableProfile& profile, const CableMap& physicalMap) {
    profile_ = &profile;
    physicalMap_ = &physicalMap;
    reset();
}

void SmartProbeEngine::reset() {
    state_ = DigitalWorkflowState::Idle;
    selectedSide_ = CableMapSide::A;
    selectedIndex_ = 0U;
    progress_ = 0U;
    lastStepMs_ = 0U;
    found_ = false;
    foundNet_ = CableNet{};
    if (!pointEnabled(selectedSide_, selectedIndex_)) nextEnd();
}

bool SmartProbeEngine::pointEnabled(CableMapSide side, uint8_t index) const {
    if (profile_ == nullptr || index >= kTestPointsPerSide) return false;
    return profile_->pointEnabledOnSide(side == CableMapSide::A, index);
}

bool SmartProbeEngine::findNextEnabled(CableMapSide& side, uint8_t& index) const {
    uint16_t flat = static_cast<uint16_t>(side == CableMapSide::A ? index : 64U + index);
    for (uint16_t offset = 1U; offset <= 128U; ++offset) {
        const uint16_t candidate = static_cast<uint16_t>((flat + offset) % 128U);
        const CableMapSide candidateSide = candidate < 64U ? CableMapSide::A : CableMapSide::B;
        const uint8_t candidateIndex = static_cast<uint8_t>(candidate % 64U);
        if (pointEnabled(candidateSide, candidateIndex)) {
            side = candidateSide;
            index = candidateIndex;
            return true;
        }
    }
    return false;
}

void SmartProbeEngine::nextEnd() {
    CableMapSide side = selectedSide_;
    uint8_t index = selectedIndex_;
    if (findNextEnabled(side, index)) {
        selectedSide_ = side;
        selectedIndex_ = index;
    }
    state_ = DigitalWorkflowState::Idle;
    progress_ = 0U;
    found_ = false;
    foundNet_ = CableNet{};
}

void SmartProbeEngine::start() {
    if (profile_ == nullptr || physicalMap_ == nullptr) {
        state_ = DigitalWorkflowState::Fault;
        return;
    }
    if (state_ == DigitalWorkflowState::Complete || state_ == DigitalWorkflowState::Fault) {
        progress_ = 0U;
        found_ = false;
        foundNet_ = CableNet{};
    }
    state_ = DigitalWorkflowState::Running;
}

void SmartProbeEngine::pauseResume() {
    if (state_ == DigitalWorkflowState::Running) state_ = DigitalWorkflowState::Paused;
    else if (state_ == DigitalWorkflowState::Paused) state_ = DigitalWorkflowState::Running;
    else start();
}

void SmartProbeEngine::evaluateSelected() {
    if (profile_ == nullptr || physicalMap_ == nullptr) {
        state_ = DigitalWorkflowState::Fault;
        return;
    }
    CableNet nets[kTestPointsPerSide]{};
    const size_t count = CableNetGraph::build(*physicalMap_,
                                               profile_->sidePointMask(true),
                                               profile_->sidePointMask(false),
                                               nets, kTestPointsPerSide);
    const uint64_t selectedBit = bitFor(selectedIndex_);
    for (size_t i = 0U; i < count; ++i) {
        const bool match = selectedSide_ == CableMapSide::A
                               ? (nets[i].aMask & selectedBit) != 0ULL
                               : (nets[i].bMask & selectedBit) != 0ULL;
        if (match) {
            found_ = true;
            foundNet_ = nets[i];
            break;
        }
    }
    progress_ = 100U;
    state_ = DigitalWorkflowState::Complete;
}

void SmartProbeEngine::tick(uint32_t nowMs, uint32_t intervalMs) {
    if (state_ != DigitalWorkflowState::Running) return;
    if (nowMs - lastStepMs_ < intervalMs) return;
    lastStepMs_ = nowMs;
    if (progress_ < 75U) {
        progress_ = static_cast<uint8_t>(progress_ + 25U);
    } else {
        evaluateSelected();
    }
}

void SmartProbeEngine::formatSelected(char* output, size_t outputSize) const {
    if (output == nullptr || outputSize == 0U) return;
    const char side = selectedSide_ == CableMapSide::A ? 'A' : 'B';
    if (selectedIndex_ == kPeTestIndex) snprintf(output, outputSize, "%cPE", side);
    else if (selectedIndex_ == kDrainShieldTestIndex) snprintf(output, outputSize, "%cdS", side);
    else snprintf(output, outputSize, "%c%u", side, static_cast<unsigned>(selectedIndex_));
}

void SmartProbeEngine::formatFoundNet(char* output, size_t outputSize) const {
    if (output == nullptr || outputSize == 0U) return;
    output[0] = '\0';
    if (!found_) {
        snprintf(output, outputSize, "No connected net / NC-SPARE candidate");
        return;
    }
    appendNodeList(output, outputSize, "A", foundNet_.aMask);
    appendNodeList(output, outputSize, "B", foundNet_.bMask);
}

void SelfTestDemoEngine::buildPlan() {
    itemCount_ = 0U;
    auto add = [this](const char* name) {
        if (itemCount_ < (sizeof(items_) / sizeof(items_[0]))) {
            items_[itemCount_++] = SelfTestItem{name, SelfTestItemResult::Pending};
        }
    };
    add("SAFE OUTPUTS / CTRL_OE_N");
    add("I2C + SPI BUS");
    add("MCP23S17 HAEN + READBACK");
    add("KELVIN ZERO / ADC OFFSET");
    if (mode_ != SelfTestMode::Fast) {
        add("DIGITAL 128-NODE LOOP");
        add("GLITCH LATCH");
        add("TDR KNOWN DELAY");
        add("PAIR LOOPBACK");
        add("USB-C SAFE / FUSB302");
    }
    if (mode_ == SelfTestMode::Calibration) {
        add("REF SHORT");
        add("REF 10 mOhm");
        add("REF 100 mOhm");
        add("REF 1 Ohm");
        add("CAL DATA CRC / SAVE");
    }
}

void SelfTestDemoEngine::start(SelfTestMode mode) {
    mode_ = mode;
    state_ = DigitalWorkflowState::Running;
    cursor_ = 0U;
    lastStepMs_ = 0U;
    buildPlan();
}

void SelfTestDemoEngine::reset() {
    state_ = DigitalWorkflowState::Idle;
    cursor_ = 0U;
    itemCount_ = 0U;
    lastStepMs_ = 0U;
}

bool SelfTestDemoEngine::step() {
    if (state_ == DigitalWorkflowState::Idle || itemCount_ == 0U) return false;
    if (state_ == DigitalWorkflowState::Complete || state_ == DigitalWorkflowState::Fault) return false;
    if (cursor_ >= itemCount_) {
        state_ = failCount() > 0U ? DigitalWorkflowState::Fault : DigitalWorkflowState::Complete;
        return false;
    }
    // Deterministic demo: all current hardware-independent contract checks pass.
    items_[cursor_].result = SelfTestItemResult::Pass;
    ++cursor_;
    if (cursor_ >= itemCount_) state_ = DigitalWorkflowState::Complete;
    return true;
}

void SelfTestDemoEngine::tick(uint32_t nowMs, uint32_t intervalMs) {
    if (state_ != DigitalWorkflowState::Running) return;
    if (nowMs - lastStepMs_ < intervalMs) return;
    lastStepMs_ = nowMs;
    step();
}

uint8_t SelfTestDemoEngine::progressPercent() const {
    return itemCount_ == 0U ? 0U
        : static_cast<uint8_t>((static_cast<uint32_t>(cursor_) * 100U) / itemCount_);
}

const SelfTestItem& SelfTestDemoEngine::item(size_t index) const {
    static const SelfTestItem empty{};
    return index < itemCount_ ? items_[index] : empty;
}

size_t SelfTestDemoEngine::passCount() const {
    size_t count = 0U;
    for (size_t i = 0U; i < itemCount_; ++i) if (items_[i].result == SelfTestItemResult::Pass) ++count;
    return count;
}

size_t SelfTestDemoEngine::warningCount() const {
    size_t count = 0U;
    for (size_t i = 0U; i < itemCount_; ++i) if (items_[i].result == SelfTestItemResult::Warning) ++count;
    return count;
}

size_t SelfTestDemoEngine::failCount() const {
    size_t count = 0U;
    for (size_t i = 0U; i < itemCount_; ++i) if (items_[i].result == SelfTestItemResult::Fail) ++count;
    return count;
}

const char* SelfTestDemoEngine::currentItemName() const {
    if (itemCount_ == 0U) return "READY";
    if (cursor_ >= itemCount_) return "COMPLETE";
    return items_[cursor_].name != nullptr ? items_[cursor_].name : "-";
}

MultiConnectorDemoEngine::MultiConnectorDemoEngine() {
    profiles_[0] = makeNormalProfile(25U, true, true);
    profiles_[1] = makeNormalProfile(12U, false, true);
    profiles_[2] = makeNormalProfile(8U, true, false);
    profiles_[0].name = "A-B SEGMENT";
    profiles_[1].name = "B-C SEGMENT";
    profiles_[2].name = "C-D SEGMENT";

    for (uint8_t s = 0U; s < 3U; ++s) {
        expected_[s].clear();
        physical_[s].clear();
        const uint64_t specialMask =
            (profiles_[s].includePe ? bitFor(kPeTestIndex) : 0ULL) |
            (profiles_[s].includeDrainShield ? bitFor(kDrainShieldTestIndex) : 0ULL);
        (void)specialMask;
        for (uint8_t i = 1U; i <= profiles_[s].signalPinCount; ++i) {
            expected_[s].connectAtoB(i, i, true);
            physical_[s].connectAtoB(i, i, true);
        }
        if (profiles_[s].includePe) {
            expected_[s].connectAtoB(kPeTestIndex, kPeTestIndex, true);
            physical_[s].connectAtoB(kPeTestIndex, kPeTestIndex, true);
        }
        if (profiles_[s].includeDrainShield) {
            expected_[s].connectAtoB(kDrainShieldTestIndex, kDrainShieldTestIndex, true);
            physical_[s].connectAtoB(kDrainShieldTestIndex, kDrainShieldTestIndex, true);
        }
        sources_[s].setPhysicalMap(&physical_[s]);
        sources_[s].setFaultOverlayEnabled(false);
    }

    // B-C deliberately demonstrates a segment-local mapping fault.
    physical_[1].setAtoBMask(3U, bitFor(4U));
    physical_[1].setAtoBMask(4U, bitFor(3U));

    // C-D demonstrates a legitimate N:N/common-net segment. Every source on
    // a common net must expect the whole connected component on both sides.
    const uint64_t cdCommonB = bitFor(7U) | bitFor(8U);
    expected_[2].setAtoBMask(7U, cdCommonB);
    expected_[2].setAtoBMask(8U, cdCommonB);
    physical_[2].setAtoBMask(7U, cdCommonB);
    physical_[2].setAtoBMask(8U, cdCommonB);
    expected_[2].setSameSideConnection(CableMapSide::A, 7U, 8U, true);
    physical_[2].setSameSideConnection(CableMapSide::A, 7U, 8U, true);
    expected_[2].setSameSideConnection(CableMapSide::B, 7U, 8U, true);
    physical_[2].setSameSideConnection(CableMapSide::B, 7U, 8U, true);

    reset();
}

void MultiConnectorDemoEngine::reset() {
    for (auto& result : results_) result = MultiSegmentResult{};
    selected_ = MultiSegmentId::AB;
    configureSelectedRunner();
}

void MultiConnectorDemoEngine::select(MultiSegmentId segment) {
    captureCurrentResult();
    selected_ = segment;
    configureSelectedRunner();
}

void MultiConnectorDemoEngine::nextSegment() {
    const uint8_t next = static_cast<uint8_t>((indexOf(selected_) + 1U) % 3U);
    select(static_cast<MultiSegmentId>(next));
}

void MultiConnectorDemoEngine::configureSelectedRunner() {
    const uint8_t index = indexOf(selected_);
    runner_.configure(profiles_[index], sources_[index], &expected_[index]);
    const MultiSegmentResult& stored = results_[index];
    if (stored.state == DigitalWorkflowState::Complete || stored.state == DigitalWorkflowState::Fault) {
        // Result history remains in results_; runner is a fresh re-test context.
    }
}

void MultiConnectorDemoEngine::captureCurrentResult() {
    const uint8_t index = indexOf(selected_);
    const DigitalWorkflowState state = runner_.state();
    if (state == DigitalWorkflowState::Complete || state == DigitalWorkflowState::Fault) {
        results_[index].state = state;
        results_[index].stats = runner_.stats();
    }
}

void MultiConnectorDemoEngine::startSelected() {
    runner_.start();
}

void MultiConnectorDemoEngine::pauseResume() {
    runner_.pauseResume();
}

void MultiConnectorDemoEngine::tick(uint32_t nowMs, uint32_t intervalMs) {
    runner_.tick(nowMs, intervalMs);
    captureCurrentResult();
}

const MultiSegmentResult& MultiConnectorDemoEngine::result(MultiSegmentId segment) const {
    return results_[indexOf(segment)];
}

const char* MultiConnectorDemoEngine::segmentName(MultiSegmentId segment) const {
    switch (segment) {
        case MultiSegmentId::AB: return "A-B";
        case MultiSegmentId::BC: return "B-C";
        case MultiSegmentId::CD: return "C-D";
    }
    return "?";
}

uint8_t MultiConnectorDemoEngine::segmentSignalCount(MultiSegmentId segment) const {
    return profiles_[indexOf(segment)].signalPinCount;
}

void MultiConnectorDemoEngine::formatAllResults(char* output, size_t outputSize) const {
    if (output == nullptr || outputSize == 0U) return;
    snprintf(output, outputSize, "A-B:%s | B-C:%s | C-D:%s",
             resultWord(results_[0].state), resultWord(results_[1].state),
             resultWord(results_[2].state));
}

CableMap makeDigitalBackboneDemoMap() {
    CableMap map;
    map.clear();
    map.setName("BACKBONE DEMO NETS");

    // Mostly ordinary point-to-point conductors.
    for (uint8_t i = 1U; i <= 20U; ++i) map.connectAtoB(i, i, true);
    map.connectAtoB(kPeTestIndex, kPeTestIndex, true);
    map.connectAtoB(kDrainShieldTestIndex, kDrainShieldTestIndex, true);

    // A7/A9 are a same-side common splice; the electrical component fans out
    // to B9/B12/B14. Remove the ordinary one-to-one routes that would
    // otherwise merge A12/A14 into this component. This intentionally matches
    // the legacy S3 learn semantics A7 -> B9,B12,B14,A9.
    map.setAtoBMask(9U, 0ULL);
    map.setAtoBMask(12U, 0ULL);
    map.setAtoBMask(14U, 0ULL);
    map.setAtoBMask(7U, bitFor(9U) | bitFor(12U) | bitFor(14U));
    map.setSameSideConnection(CableMapSide::A, 7U, 9U, true);

    // Additional many-to-one example kept as a separate component.
    map.setAtoBMask(17U, 0ULL);
    map.setAtoBMask(18U, 0ULL);
    map.setSameSideConnection(CableMapSide::A, 16U, 17U, true);
    map.setAtoBMask(16U, bitFor(18U));
    return map;
}

DigitalBackboneCore::DigitalBackboneCore() {
    fullProfile_ = makeNormalProfile(kMaximumSignalPins, true, true);
    expectedOneToOne_.setOneToOne();
    demoElectricalMap_ = makeDigitalBackboneDemoMap();

    scanSource_.setFaultOverlayEnabled(true);
    scan_.configure(fullProfile_, scanSource_, &expectedOneToOne_);

    learnSource_.setPhysicalMap(&demoElectricalMap_);
    learnSource_.setFaultOverlayEnabled(false);
    learn_.configure(fullProfile_, learnSource_);

    probe_.configure(fullProfile_, demoElectricalMap_);
}

void DigitalBackboneCore::resetAll() {
    scan_.reset();
    learn_.reset();
    probe_.reset();
    selfTest_.reset();
    multi_.reset();
}

void DigitalBackboneCore::tick(uint32_t nowMs) {
    scan_.tick(nowMs);
    learn_.tick(nowMs);
    probe_.tick(nowMs);
    selfTest_.tick(nowMs);
    multi_.tick(nowMs);
}

}  // namespace mg::p4
