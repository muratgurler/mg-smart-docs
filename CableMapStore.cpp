#include "CableMapStore.h"

#include <Preferences.h>

namespace mg::p4 {
namespace {
constexpr const char* kNamespace = "mg_cmap";
constexpr const char* kVersionKey = "ver";
constexpr const char* kNameKey = "name";
constexpr const char* kAbKey = "ab";
constexpr const char* kAaKey = "aa";
constexpr const char* kBbKey = "bb";
constexpr uint32_t kStoreVersion = 1U;
constexpr size_t kArrayBytes = kTestPointsPerSide * sizeof(uint64_t);
}

bool CableMapStore::load(CableMap& map) {
    Preferences prefs;
    if (!prefs.begin(kNamespace, true)) {
        return false;
    }
    const uint32_t version = prefs.getUInt(kVersionKey, 0U);
    if (version != kStoreVersion || prefs.getBytesLength(kAbKey) != kArrayBytes ||
        prefs.getBytesLength(kAaKey) != kArrayBytes ||
        prefs.getBytesLength(kBbKey) != kArrayBytes) {
        prefs.end();
        return false;
    }

    uint64_t ab[kTestPointsPerSide]{};
    uint64_t aa[kTestPointsPerSide]{};
    uint64_t bb[kTestPointsPerSide]{};
    prefs.getBytes(kAbKey, ab, sizeof(ab));
    prefs.getBytes(kAaKey, aa, sizeof(aa));
    prefs.getBytes(kBbKey, bb, sizeof(bb));
    const String name = prefs.getString(kNameKey, "CUSTOM MAP");
    prefs.end();

    map.loadRaw(ab, aa, bb, name.c_str());
    return true;
}

bool CableMapStore::save(const CableMap& map) {
    Preferences prefs;
    if (!prefs.begin(kNamespace, false)) {
        return false;
    }

    uint64_t ab[kTestPointsPerSide]{};
    uint64_t aa[kTestPointsPerSide]{};
    uint64_t bb[kTestPointsPerSide]{};
    for (uint8_t i = 0; i < kTestPointsPerSide; ++i) {
        ab[i] = map.receiverMaskAtoB(i);
        aa[i] = map.sameSideMaskA(i);
        bb[i] = map.sameSideMaskB(i);
    }

    const bool ok = prefs.putUInt(kVersionKey, kStoreVersion) == sizeof(uint32_t) &&
                    prefs.putString(kNameKey, map.name()) > 0U &&
                    prefs.putBytes(kAbKey, ab, sizeof(ab)) == sizeof(ab) &&
                    prefs.putBytes(kAaKey, aa, sizeof(aa)) == sizeof(aa) &&
                    prefs.putBytes(kBbKey, bb, sizeof(bb)) == sizeof(bb);
    prefs.end();
    return ok;
}

void CableMapStore::clearSaved() {
    Preferences prefs;
    if (prefs.begin(kNamespace, false)) {
        prefs.clear();
        prefs.end();
    }
}

}  // namespace mg::p4
