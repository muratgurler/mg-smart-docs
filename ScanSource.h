#pragma once

#include <stdint.h>

namespace mg::p4 {

class CableMap;

enum class ScanDirection : uint8_t {
    AtoB,
    BtoA,
};

enum class ElectricalResult : uint8_t {
    NotMeasured,
    Ok,
    Open,
    ShortCircuit,
    WrongConnection,
    HighResistance,
};

// One scan result, intentionally carrying both the compact UI fields and the
// masks used by the original ESP32-S3 physical scan logic. The masks let a
// real MCP23S17 source use the same classifier without inventing a
// second set of rules.
struct Measurement {
    ElectricalResult result = ElectricalResult::NotMeasured;
    uint8_t expectedTestIndex = 0xFF;  // first/lowest expected receiver
    uint8_t actualTestIndex = 0xFF;    // first/lowest reached receiver
    uint64_t expectedReceiverMask = 0;
    uint64_t actualReceiverMask = 0;
    uint64_t senderGroupMask = 0;      // includes the driven sender itself

    // Optional Kelvin/low-ohm measurement attached to this exact scan point.
    // resistanceValid=false means the digital continuity result is valid but
    // no trustworthy resistance number was acquired for the row.
    bool resistanceValid = false;
    float resistanceMilliOhm = 0.0f;
    float resistanceLimitMilliOhm = 0.0f;
};

// Raw electrical observation corresponding to the S3 TestController scan:
// while one sender pin is driven LOW, read every pin on the sender side and
// every pin on the receiver side. highResistance is an extra P4/V3.3 analog
// property; the S3 digital classification itself is mask based.
struct PhysicalScanObservation {
    uint64_t senderGroupMask = 0;
    uint64_t receiverMask = 0;
    bool highResistance = false;

    // Real hardware may fill these from the Kelvin/ADS122C04 path. They are
    // deliberately optional so UI-only/digital-only builds report N/A rather
    // than fabricating a resistance value.
    bool resistanceValid = false;
    float resistanceMilliOhm = 0.0f;
    float resistanceLimitMilliOhm = 0.0f;
};

// Full S3-compatible classifier for ONE_TO_ONE and CUSTOM_MAP modes.
// expectedReceiverMask may contain zero, one or multiple receiver pins.
// expectedSenderMask contains the source itself plus any expected A-A/B-B
// group. Priority deliberately matches old TestController::analyzeResults().
Measurement classifyObservation(uint8_t sourceTestIndex,
                                const PhysicalScanObservation& observed,
                                uint64_t expectedReceiverMask,
                                uint64_t expectedSenderMask);

// Convenience wrapper for the normal one-to-one mode.
Measurement classifyOneToOneObservation(uint8_t sourceTestIndex,
                                        const PhysicalScanObservation& observed);

class ScanSource {
public:
    virtual ~ScanSource() = default;

    // Real hardware implementations should return the raw masks read from the
    // scanner. Expected-map comparison belongs to ScanSession, exactly as the
    // S3 TestController separates physical readout from map analysis.
    virtual PhysicalScanObservation observe(ScanDirection direction,
                                            uint8_t testIndex) = 0;

    // Kept as a convenience/backward-compatible one-to-one measurement API.
    virtual Measurement measure(ScanDirection direction, uint8_t testIndex);
    virtual bool isDemo() const = 0;
};


class RoutedScanSource final : public ScanSource {
public:
    explicit RoutedScanSource(ScanSource& demoSource)
        : demoSource_(demoSource) {}

    PhysicalScanObservation observe(ScanDirection direction,
                                    uint8_t testIndex) override;
    bool isDemo() const override;

    void attachPhysical(ScanSource* physicalSource) {
        physicalSource_ = physicalSource;
    }
    void preferPhysical(bool enabled) { preferPhysical_ = enabled; }
    bool physicalAttached() const { return physicalSource_ != nullptr; }
    bool usingPhysical() const {
        return preferPhysical_ && physicalSource_ != nullptr;
    }

private:
    ScanSource& activeSource() const;

    ScanSource& demoSource_;
    ScanSource* physicalSource_ = nullptr;
    bool preferPhysical_ = false;
};

class DemoScanSource final : public ScanSource {
public:
    PhysicalScanObservation observe(ScanDirection direction,
                                    uint8_t testIndex) override;
    bool isDemo() const override { return true; }

    // nullptr = original one-to-one demo cable. A non-null map makes the
    // virtual physical cable follow that custom map; the same deliberate demo
    // faults (8/9 swap, 15 open, 18 high-R, 22/23 short) are then overlaid.
    void setPhysicalMap(const CableMap* map) { physicalMap_ = map; }
    const CableMap* physicalMap() const { return physicalMap_; }
    // Learning / multi-connector demos can disable the legacy deliberate
    // fault overlay and expose the exact passive electrical graph instead.
    void setFaultOverlayEnabled(bool enabled) { faultOverlayEnabled_ = enabled; }
    bool faultOverlayEnabled() const { return faultOverlayEnabled_; }
    void setSpecialCrossConnections(bool peConnectedAcross,
                                    bool drainShieldConnectedAcross) {
        peConnectedAcross_ = peConnectedAcross;
        drainShieldConnectedAcross_ = drainShieldConnectedAcross;
    }

private:
    const CableMap* physicalMap_ = nullptr;
    bool peConnectedAcross_ = true;
    bool drainShieldConnectedAcross_ = true;
    bool faultOverlayEnabled_ = true;
};

}  // namespace mg::p4
