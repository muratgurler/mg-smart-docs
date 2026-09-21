#include "CableProfile.h"

#include <stdio.h>

namespace mg::p4 {

namespace {

constexpr ConnectorSpec dsub(const char* name,
                             uint8_t contacts,
                             ConnectorGender gender) {
    return ConnectorSpec{
        ConnectorKind::DSub, gender, contacts, name, nullptr};
}

// This order is an operator workflow contract. DB25 is first because it is
// the most frequently used workshop connector. Sub-D has no PE contact; its
// metal shell/drain remains the dedicated dS point.
const CableProfile kProfiles[] = {
    {"Sub-D25", ScanProfileKind::SubD, 25, false, true, false, false, true, true,
     dsub("DB25", 25, ConnectorGender::Male),
     dsub("DB25", 25, ConnectorGender::Female)},
    {"Sub-D9", ScanProfileKind::SubD, 9, false, true, false, false, true, true,
     dsub("DB9", 9, ConnectorGender::Male),
     dsub("DB9", 9, ConnectorGender::Female)},
    {"Sub-D15", ScanProfileKind::SubD, 15, false, true, false, false, true, true,
     dsub("DB15", 15, ConnectorGender::Male),
     dsub("DB15", 15, ConnectorGender::Female)},
    {"Sub-D37", ScanProfileKind::SubD, 37, false, true, false, false, true, true,
     dsub("DB37", 37, ConnectorGender::Male),
     dsub("DB37", 37, ConnectorGender::Female)},
    {"Sub-D44", ScanProfileKind::SubD, 44, false, true, false, false, true, true,
     dsub("DB44", 44, ConnectorGender::Male),
     dsub("DB44", 44, ConnectorGender::Female)},
    {"Sub-D50", ScanProfileKind::SubD, 50, false, true, false, false, true, true,
     dsub("DB50", 50, ConnectorGender::Male),
     dsub("DB50", 50, ConnectorGender::Female)},
    {"Sub-D62", ScanProfileKind::SubD, 62, false, true, false, false, true, true,
     dsub("DB62", 62, ConnectorGender::Male),
     dsub("DB62", 62, ConnectorGender::Female)},
};

constexpr size_t kProfileCount = sizeof(kProfiles) / sizeof(kProfiles[0]);

}  // namespace

bool CableProfile::pointEnabledOnSide(bool sideASelected, uint8_t testIndex) const {
    if (testIndex == kPeTestIndex) {
        return sideASelected ? includePeA : includePeB;
    }
    if (testIndex == kDrainShieldTestIndex) {
        return sideASelected ? includeDrainShieldA : includeDrainShieldB;
    }
    return testIndex >= kFirstSignalTestIndex &&
           testIndex < static_cast<uint8_t>(kFirstSignalTestIndex + signalPinCount);
}

uint64_t CableProfile::sidePointMask(bool sideASelected) const {
    uint64_t mask = 0ULL;
    for (uint8_t index = 0; index < kTestPointsPerSide; ++index) {
        if (pointEnabledOnSide(sideASelected, index)) {
            mask |= (1ULL << index);
        }
    }
    return mask;
}

void CableProfile::syncSpecialPointUnion() {
    includePe = includePeA || includePeB;
    includeDrainShield = includeDrainShieldA || includeDrainShieldB;
}

uint8_t CableProfile::activePointCount() const {
    const uint16_t count = signalPinCount + (includePe ? 1U : 0U) +
                           (includeDrainShield ? 1U : 0U);
    return count > kTestPointsPerSide ? kTestPointsPerSide
                                      : static_cast<uint8_t>(count);
}

bool CableProfile::isValidSlot(uint8_t slot) const {
    return slot < activePointCount();
}

uint8_t CableProfile::slotToTestIndex(uint8_t slot) const {
    if (!isValidSlot(slot)) {
        return 0xFF;
    }
    if (includePe && slot == 0) {
        return kPeTestIndex;
    }

    const uint8_t signalOffset = includePe ? 1U : 0U;
    if (slot < signalOffset + signalPinCount) {
        return static_cast<uint8_t>(slot - signalOffset +
                                    kFirstSignalTestIndex);
    }
    return includeDrainShield ? kDrainShieldTestIndex : 0xFF;
}

uint8_t CableProfile::testIndexToSlot(uint8_t testIndex) const {
    for (uint8_t slot = 0; slot < activePointCount(); ++slot) {
        if (slotToTestIndex(slot) == testIndex) {
            return slot;
        }
    }
    return 0xFF;
}

void CableProfile::formatSlotLabel(uint8_t slot,
                                   char* output,
                                   size_t outputSize) const {
    if (output == nullptr || outputSize == 0) {
        return;
    }
    const uint8_t testIndex = slotToTestIndex(slot);
    if (testIndex == kPeTestIndex) {
        snprintf(output, outputSize, "PE");
    } else if (testIndex == kDrainShieldTestIndex) {
        snprintf(output, outputSize, "dS");
    } else if (testIndex != 0xFF) {
        snprintf(output, outputSize, "%u", testIndex);
    } else {
        snprintf(output, outputSize, "--");
    }
}

const CableProfile& defaultProfile() {
    return kProfiles[0];  // Sub-D25: 25 contacts + dS, never PE.
}

const CableProfile& profileAt(size_t index) {
    return kProfiles[index < kProfileCount ? index : 0];
}

size_t profileCount() {
    return kProfileCount;
}

CableProfile makeNormalProfile(uint8_t signalPinCount,
                               bool includePe,
                               bool includeDrainShield) {
    CableProfile profile{
        "Normal",
        ScanProfileKind::Normal,
        25,
        includePe,
        includeDrainShield,
        includePe,
        includePe,
        includeDrainShield,
        includeDrainShield,
        {ConnectorKind::Custom, ConnectorGender::Neutral, 25, "NORMAL A", nullptr},
        {ConnectorKind::Custom, ConnectorGender::Neutral, 25, "NORMAL B", nullptr},
    };
    setNormalSignalPinCount(profile, signalPinCount);
    return profile;
}

void setNormalSignalPinCount(CableProfile& profile, uint8_t signalPinCount) {
    if (signalPinCount < 1U) {
        signalPinCount = 1U;
    } else if (signalPinCount > kMaximumSignalPins) {
        signalPinCount = kMaximumSignalPins;
    }
    profile.signalPinCount = signalPinCount;
    profile.sideA.contactCount = signalPinCount;
    profile.sideB.contactCount = signalPinCount;
}

uint8_t nextNormalSignalPinPreset(uint8_t currentSignalPinCount) {
    // The center pin-count button advances to the first preset above the
    // current value. Values at or above 60 wrap to 5. The +/- buttons remain
    // available for exact one-pin adjustment between these shortcuts.
    constexpr uint8_t presets[] = {5U, 10U, 20U, 30U, 40U, 50U, 60U};
    for (const uint8_t preset : presets) {
        if (currentSignalPinCount < preset) {
            return preset;
        }
    }
    return presets[0];
}

const char* genderText(ConnectorGender gender) {
    switch (gender) {
        case ConnectorGender::Male:
            return "ERKEK";
        case ConnectorGender::Female:
            return "DISI";
        case ConnectorGender::Neutral:
        default:
            return "OZEL";
    }
}

}  // namespace mg::p4
