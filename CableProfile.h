#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ConfigP4.h"

namespace mg::p4 {

enum class ConnectorKind : uint8_t {
    DSub,
    Rj45,
    Usb,
    Lemo,
    Harting,
    Custom,
};

enum class ConnectorGender : uint8_t {
    Male,
    Female,
    Neutral,
};

enum class ScanProfileKind : uint8_t {
    Normal,
    SubD,
};

struct ConnectorSpec {
    ConnectorKind kind;
    ConnectorGender gender;
    uint8_t contactCount;
    const char* displayName;
    // Optional future LVGL image asset. A null value selects the exact vector
    // front-face renderer included in this package.
    const void* frontImage;
};

struct CableProfile {
    const char* name;
    ScanProfileKind kind;
    uint8_t signalPinCount;
    // Union flags kept for the existing map/bar UI: true when PE/dS exists
    // on at least one side. Side-specific flags are authoritative.
    bool includePe;
    bool includeDrainShield;
    bool includePeA;
    bool includePeB;
    bool includeDrainShieldA;
    bool includeDrainShieldB;
    ConnectorSpec sideA;
    ConnectorSpec sideB;

    uint8_t activePointCount() const;
    uint8_t slotToTestIndex(uint8_t slot) const;
    uint8_t testIndexToSlot(uint8_t testIndex) const;
    bool isValidSlot(uint8_t slot) const;
    bool isSubD() const { return kind == ScanProfileKind::SubD; }
    bool pointEnabledOnSide(bool sideASelected, uint8_t testIndex) const;
    uint64_t sidePointMask(bool sideASelected) const;
    void syncSpecialPointUnion();
    void formatSlotLabel(uint8_t slot, char* output, size_t outputSize) const;
};

const CableProfile& defaultProfile();
const CableProfile& profileAt(size_t index);
size_t profileCount();
CableProfile makeNormalProfile(uint8_t signalPinCount = 25,
                               bool includePe = true,
                               bool includeDrainShield = true);
void setNormalSignalPinCount(CableProfile& profile, uint8_t signalPinCount);
uint8_t nextNormalSignalPinPreset(uint8_t currentSignalPinCount);

const char* genderText(ConnectorGender gender);

}  // namespace mg::p4
