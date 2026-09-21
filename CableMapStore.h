#pragma once

#include "CableMap.h"

namespace mg::p4 {

class CableMapStore {
public:
    bool load(CableMap& map);
    bool save(const CableMap& map);
    void clearSaved();
};

}  // namespace mg::p4
