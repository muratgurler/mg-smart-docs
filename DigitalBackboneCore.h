#pragma once

#include <stddef.h>
#include <stdint.h>

#include "CableMap.h"
#include "CableNetGraph.h"
#include "CableProfile.h"
#include "ScanSource.h"

namespace mg::p4 {

enum class DigitalWorkflowState : uint8_t {
    Idle,
    Running,
    Paused,
    Complete,
    Fault,
};

struct DigitalSweepReportEntry {
    ScanDirection direction = ScanDirection::AtoB;
    uint8_t source = 0xFF;
    uint64_t expectedReceiverMask = 0ULL;
    uint64_t expectedSenderMask = 0ULL;
    PhysicalScanObservation observation{};
    Measurement measurement{};
};

struct DigitalSweepStats {
    uint16_t total = 0;
    uint16_t completed = 0;
    uint16_t ok = 0;
    uint16_t open = 0;
    uint16_t shortCircuit = 0;
    uint16_t wrongConnection = 0;
    uint16_t highResistance = 0;
    uint16_t sameSideMismatch = 0;
    ScanDirection lastDirection = ScanDirection::AtoB;
    uint8_t lastSource = 0xFF;
    Measurement lastMeasurement{};

    uint16_t errorCount() const {
        return static_cast<uint16_t>(open + shortCircuit + wrongConnection +
                                     highResistance);
    }
};

// Shared 128-node digital sweep engine. It is deliberately independent from
// LVGL and from MCP23S17: it consumes the ScanSource contract, so DEMO and
// the Fix1 MCP23S17 physical backend share exactly the same classifier.
class DigitalSweepRunner {
public:
    void configure(const CableProfile& profile,
                   ScanSource& source,
                   const CableMap* expectedMap = nullptr);
    void reset();
    void start();
    void pauseResume();
    bool step();
    void tick(uint32_t nowMs, uint32_t intervalMs = 35U);

    DigitalWorkflowState state() const { return state_; }
    const DigitalSweepStats& stats() const { return stats_; }
    size_t reportEntryCount() const { return reportEntryCount_; }
    const DigitalSweepReportEntry& reportEntry(size_t index) const;
    uint8_t progressPercent() const;

private:
    bool pointEnabled(ScanDirection direction, uint8_t index) const;
    uint64_t expectedReceiverMask(ScanDirection direction, uint8_t source) const;
    uint64_t expectedSenderMask(ScanDirection direction, uint8_t source) const;
    bool nextPoint(ScanDirection& direction, uint8_t& index) const;
    void tally(const PhysicalScanObservation& observation,
               const Measurement& measurement,
               ScanDirection direction,
               uint8_t source);

    const CableProfile* profile_ = nullptr;
    ScanSource* source_ = nullptr;
    const CableMap* expectedMap_ = nullptr;
    DigitalWorkflowState state_ = DigitalWorkflowState::Idle;
    DigitalSweepStats stats_{};
    DigitalSweepReportEntry reportEntries_[kTestPointsPerSide * 2U]{};
    size_t reportEntryCount_ = 0U;
    ScanDirection cursorDirection_ = ScanDirection::AtoB;
    uint8_t cursorIndex_ = 0U;
    uint32_t lastStepMs_ = 0U;
};

class CableLearnEngine {
public:
    void configure(const CableProfile& profile, ScanSource& source);
    void reset();
    void start();
    void pauseResume();
    bool step();
    void tick(uint32_t nowMs, uint32_t intervalMs = 28U);

    DigitalWorkflowState state() const { return state_; }
    uint8_t progressPercent() const;
    uint16_t completedObservations() const { return completed_; }
    uint16_t totalObservations() const { return total_; }
    const CableMap& learnedMap() const { return learnedMap_; }
    size_t learnedNetCount() const { return learnedNetCount_; }
    bool hasReviewableMap() const { return state_ == DigitalWorkflowState::Complete; }
    void formatSummary(char* output, size_t outputSize) const;

private:
    bool pointEnabled(ScanDirection direction, uint8_t index) const;
    bool nextPoint(ScanDirection& direction, uint8_t& index) const;
    void finalizeMap();

    const CableProfile* profile_ = nullptr;
    ScanSource* source_ = nullptr;
    DigitalWorkflowState state_ = DigitalWorkflowState::Idle;
    ScanDirection cursorDirection_ = ScanDirection::AtoB;
    uint8_t cursorIndex_ = 0U;
    uint16_t completed_ = 0U;
    uint16_t total_ = 0U;
    uint32_t lastStepMs_ = 0U;
    uint64_t aToB_[kTestPointsPerSide]{};
    uint64_t sameA_[kTestPointsPerSide]{};
    uint64_t sameB_[kTestPointsPerSide]{};
    CableMap learnedMap_{};
    CableNet learnedNets_[kTestPointsPerSide]{};
    size_t learnedNetCount_ = 0U;
};

class SmartProbeEngine {
public:
    void configure(const CableProfile& profile, const CableMap& physicalMap);
    void reset();
    void start();
    void pauseResume();
    void nextEnd();
    void tick(uint32_t nowMs, uint32_t intervalMs = 90U);

    DigitalWorkflowState state() const { return state_; }
    uint8_t progressPercent() const { return progress_; }
    CableMapSide selectedSide() const { return selectedSide_; }
    uint8_t selectedIndex() const { return selectedIndex_; }
    bool foundNet() const { return found_; }
    const CableNet& found() const { return foundNet_; }
    void formatSelected(char* output, size_t outputSize) const;
    void formatFoundNet(char* output, size_t outputSize) const;

private:
    bool pointEnabled(CableMapSide side, uint8_t index) const;
    bool findNextEnabled(CableMapSide& side, uint8_t& index) const;
    void evaluateSelected();

    const CableProfile* profile_ = nullptr;
    const CableMap* physicalMap_ = nullptr;
    DigitalWorkflowState state_ = DigitalWorkflowState::Idle;
    CableMapSide selectedSide_ = CableMapSide::A;
    uint8_t selectedIndex_ = 0U;
    uint8_t progress_ = 0U;
    uint32_t lastStepMs_ = 0U;
    bool found_ = false;
    CableNet foundNet_{};
};

enum class SelfTestMode : uint8_t { Fast, Full, Calibration };
enum class SelfTestItemResult : uint8_t { Pending, Pass, Warning, Fail };

struct SelfTestItem {
    const char* name = nullptr;
    SelfTestItemResult result = SelfTestItemResult::Pending;
};

class SelfTestDemoEngine {
public:
    void start(SelfTestMode mode);
    void reset();
    void tick(uint32_t nowMs, uint32_t intervalMs = 420U);
    bool step();

    DigitalWorkflowState state() const { return state_; }
    SelfTestMode mode() const { return mode_; }
    uint8_t progressPercent() const;
    size_t itemCount() const { return itemCount_; }
    const SelfTestItem& item(size_t index) const;
    size_t passCount() const;
    size_t warningCount() const;
    size_t failCount() const;
    const char* currentItemName() const;

private:
    void buildPlan();

    SelfTestMode mode_ = SelfTestMode::Fast;
    DigitalWorkflowState state_ = DigitalWorkflowState::Idle;
    SelfTestItem items_[16]{};
    size_t itemCount_ = 0U;
    size_t cursor_ = 0U;
    uint32_t lastStepMs_ = 0U;
};

enum class MultiSegmentId : uint8_t { AB = 0, BC = 1, CD = 2 };

struct MultiSegmentResult {
    DigitalWorkflowState state = DigitalWorkflowState::Idle;
    DigitalSweepStats stats{};
};

class MultiConnectorDemoEngine {
public:
    MultiConnectorDemoEngine();
    void reset();
    void select(MultiSegmentId segment);
    void nextSegment();
    void startSelected();
    void pauseResume();
    void tick(uint32_t nowMs, uint32_t intervalMs = 30U);

    MultiSegmentId selected() const { return selected_; }
    DigitalWorkflowState state() const { return runner_.state(); }
    uint8_t progressPercent() const { return runner_.progressPercent(); }
    const DigitalSweepStats& currentStats() const { return runner_.stats(); }
    const MultiSegmentResult& result(MultiSegmentId segment) const;
    const char* segmentName(MultiSegmentId segment) const;
    uint8_t segmentSignalCount(MultiSegmentId segment) const;
    void formatAllResults(char* output, size_t outputSize) const;

private:
    static uint8_t indexOf(MultiSegmentId segment) {
        return static_cast<uint8_t>(segment);
    }
    void configureSelectedRunner();
    void captureCurrentResult();

    MultiSegmentId selected_ = MultiSegmentId::AB;
    CableProfile profiles_[3]{};
    CableMap expected_[3]{};
    CableMap physical_[3]{};
    DemoScanSource sources_[3]{};
    MultiSegmentResult results_[3]{};
    DigitalSweepRunner runner_{};
};

// Coherent demo/hardware seam for the first digital-core batch.
class DigitalBackboneCore {
public:
    DigitalBackboneCore();
    void resetAll();
    void tick(uint32_t nowMs);

    DigitalSweepRunner& scan() { return scan_; }
    CableLearnEngine& learn() { return learn_; }
    SmartProbeEngine& probe() { return probe_; }
    SelfTestDemoEngine& selfTest() { return selfTest_; }
    MultiConnectorDemoEngine& multi() { return multi_; }

    const CableMap& demoElectricalMap() const { return demoElectricalMap_; }
    const CableProfile& fullProfile() const { return fullProfile_; }

private:
    CableProfile fullProfile_{};
    CableMap expectedOneToOne_{};
    CableMap demoElectricalMap_{};
    DemoScanSource scanSource_{};
    DemoScanSource learnSource_{};
    DigitalSweepRunner scan_{};
    CableLearnEngine learn_{};
    SmartProbeEngine probe_{};
    SelfTestDemoEngine selfTest_{};
    MultiConnectorDemoEngine multi_{};
};

CableMap makeDigitalBackboneDemoMap();

}  // namespace mg::p4
