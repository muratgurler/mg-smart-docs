#include "CableMap.h"

#include <string.h>

namespace mg::p4 {

CableMap::CableMap() {
    clear();
    setName("CUSTOM MAP");
}

uint64_t CableMap::bitFor(uint8_t index) {
    return index < kTestPointsPerSide ? (1ULL << index) : 0ULL;
}

void CableMap::clear() {
    memset(aToB_, 0, sizeof(aToB_));
    memset(bToA_, 0, sizeof(bToA_));
    memset(sameSideA_, 0, sizeof(sameSideA_));
    memset(sameSideB_, 0, sizeof(sameSideB_));
}

void CableMap::setName(const char* name) {
    if (name == nullptr) {
        name_[0] = '\0';
        return;
    }
    strncpy(name_, name, sizeof(name_) - 1U);
    name_[sizeof(name_) - 1U] = '\0';
}

void CableMap::setOneToOne() {
    clear();
    for (uint8_t index = 0; index < kTestPointsPerSide; ++index) {
        aToB_[index] = bitFor(index);
    }
    rebuildReverse();
}

void CableMap::setAtoBMask(uint8_t aIndex, uint64_t bMask) {
    if (aIndex >= kTestPointsPerSide) {
        return;
    }
    aToB_[aIndex] = bMask;
    rebuildReverse();
}

void CableMap::connectAtoB(uint8_t aIndex,
                           uint8_t bIndex,
                           bool connected) {
    if (aIndex >= kTestPointsPerSide || bIndex >= kTestPointsPerSide) {
        return;
    }
    const uint64_t bit = bitFor(bIndex);
    if (connected) {
        aToB_[aIndex] |= bit;
    } else {
        aToB_[aIndex] &= ~bit;
    }
    rebuildReverse();
}

uint64_t CableMap::receiverMaskAtoB(uint8_t aIndex) const {
    return aIndex < kTestPointsPerSide ? aToB_[aIndex] : 0ULL;
}

uint64_t CableMap::receiverMaskBtoA(uint8_t bIndex) const {
    return bIndex < kTestPointsPerSide ? bToA_[bIndex] : 0ULL;
}

void CableMap::rebuildReverse() {
    memset(bToA_, 0, sizeof(bToA_));
    for (uint8_t aIndex = 0; aIndex < kTestPointsPerSide; ++aIndex) {
        const uint64_t targets = aToB_[aIndex];
        for (uint8_t bIndex = 0; bIndex < kTestPointsPerSide; ++bIndex) {
            if ((targets & bitFor(bIndex)) != 0ULL) {
                bToA_[bIndex] |= bitFor(aIndex);
            }
        }
    }
}

void CableMap::setSameSideConnection(CableMapSide side,
                                     uint8_t firstIndex,
                                     uint8_t secondIndex,
                                     bool connected) {
    if (firstIndex >= kTestPointsPerSide ||
        secondIndex >= kTestPointsPerSide || firstIndex == secondIndex) {
        return;
    }

    uint64_t* map = side == CableMapSide::A ? sameSideA_ : sameSideB_;
    const uint64_t firstBit = bitFor(firstIndex);
    const uint64_t secondBit = bitFor(secondIndex);
    if (connected) {
        map[firstIndex] |= secondBit;
        map[secondIndex] |= firstBit;
    } else {
        map[firstIndex] &= ~secondBit;
        map[secondIndex] &= ~firstBit;
    }
}

uint64_t CableMap::sameSideMaskA(uint8_t aIndex) const {
    return aIndex < kTestPointsPerSide ? sameSideA_[aIndex] : 0ULL;
}

uint64_t CableMap::sameSideMaskB(uint8_t bIndex) const {
    return bIndex < kTestPointsPerSide ? sameSideB_[bIndex] : 0ULL;
}

uint64_t CableMap::senderMaskA(uint8_t aIndex) const {
    return aIndex < kTestPointsPerSide
               ? (sameSideA_[aIndex] | bitFor(aIndex))
               : 0ULL;
}

uint64_t CableMap::senderMaskB(uint8_t bIndex) const {
    return bIndex < kTestPointsPerSide
               ? (sameSideB_[bIndex] | bitFor(bIndex))
               : 0ULL;
}

void CableMap::normalizeSameSide(uint64_t* map) {
    if (map == nullptr) {
        return;
    }
    for (uint8_t i = 0; i < kTestPointsPerSide; ++i) {
        map[i] &= ~bitFor(i);  // self is implicit, never stored
    }
    for (uint8_t i = 0; i < kTestPointsPerSide; ++i) {
        const uint64_t peers = map[i];
        for (uint8_t j = 0; j < kTestPointsPerSide; ++j) {
            if ((peers & bitFor(j)) != 0ULL && i != j) {
                map[j] |= bitFor(i);
            }
        }
    }
}

void CableMap::loadRaw(const uint64_t* aToB,
                       const uint64_t* sameSideA,
                       const uint64_t* sameSideB,
                       const char* name) {
    clear();
    if (aToB != nullptr) {
        memcpy(aToB_, aToB, sizeof(aToB_));
    }
    if (sameSideA != nullptr) {
        memcpy(sameSideA_, sameSideA, sizeof(sameSideA_));
    }
    if (sameSideB != nullptr) {
        memcpy(sameSideB_, sameSideB, sizeof(sameSideB_));
    }
    normalizeSameSide(sameSideA_);
    normalizeSameSide(sameSideB_);
    rebuildReverse();
    if (name != nullptr && name[0] != '\0') {
        setName(name);
    }
}

CableMap makeOneToOneCableMap() {
    CableMap map;
    map.setName("ONE TO ONE");
    map.setOneToOne();
    return map;
}

CableMap makeDefaultCustomCableMap() {
    CableMap map;
    map.setName("CUSTOM MAP");
    map.setOneToOne();

    // Replace, do not add to, the first three one-to-one routes.
    map.setAtoBMask(1U, 1ULL << 3U);
    map.setAtoBMask(2U, 1ULL << 1U);
    map.setAtoBMask(3U, 1ULL << 2U);
    map.rebuildReverse();
    return map;
}

}  // namespace mg::p4
