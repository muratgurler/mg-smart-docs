#pragma once

#include <stddef.h>
#include <stdint.h>

#include "ConfigP4.h"

namespace mg::p4 {

enum class CableMapSide : uint8_t {
    A,
    B,
};

// S3-compatible expected cable map.
//
// Every A pin owns a 64-bit mask of expected B pins. B->A is always derived
// from that A->B table. Expected same-side groups (A-A / B-B) are kept in two
// independent symmetric masks. This is the same logical model used by the
// original ESP32-S3 MG Smart Test custom-map implementation.
class CableMap {
public:
    CableMap();

    void clear();
    void setName(const char* name);
    const char* name() const { return name_; }

    void setOneToOne();

    // Cross-side map. One source may have zero, one or many expected targets.
    void setAtoBMask(uint8_t aIndex, uint64_t bMask);
    void connectAtoB(uint8_t aIndex, uint8_t bIndex, bool connected = true);
    uint64_t receiverMaskAtoB(uint8_t aIndex) const;
    uint64_t receiverMaskBtoA(uint8_t bIndex) const;
    void rebuildReverse();

    // Expected same-side electrical groups (A-A or B-B). The source itself is
    // implicit in senderMaskA/B and is not stored in the raw same-side mask.
    void setSameSideConnection(CableMapSide side,
                               uint8_t firstIndex,
                               uint8_t secondIndex,
                               bool connected = true);
    uint64_t sameSideMaskA(uint8_t aIndex) const;
    uint64_t sameSideMaskB(uint8_t bIndex) const;
    uint64_t senderMaskA(uint8_t aIndex) const;
    uint64_t senderMaskB(uint8_t bIndex) const;

    // Persistence/editor bridge. B->A is deliberately not stored; it is
    // regenerated from A->B after load, exactly like the S3 firmware.
    void loadRaw(const uint64_t* aToB,
                 const uint64_t* sameSideA,
                 const uint64_t* sameSideB,
                 const char* name = nullptr);

private:
    static uint64_t bitFor(uint8_t index);
    void normalizeSameSide(uint64_t* map);

    char name_[40]{};
    uint64_t aToB_[kTestPointsPerSide]{};
    uint64_t bToA_[kTestPointsPerSide]{};
    uint64_t sameSideA_[kTestPointsPerSide]{};
    uint64_t sameSideB_[kTestPointsPerSide]{};
};

CableMap makeOneToOneCableMap();

// Built-in first-use demonstration map. Once the operator saves a map from
// the P4 touch editor or web editor, Preferences replaces this default.
CableMap makeDefaultCustomCableMap();

}  // namespace mg::p4
