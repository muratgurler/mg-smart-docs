#pragma once

#include <Arduino.h>

#include "CableMap.h"
#include "CableProfile.h"
#include "DocumentImportValidator.h"
#include "ProductionProfileContract.h"

namespace mg::p4 {

class MapWebEditor;

// Shared transport-neutral validation path for a prepared mobile profile.
// Wi-Fi and BLE both land in MapWebEditor/LittleFS, then this service performs
// the same JSON parse + electrical topology validation before a profile may be
// considered READY.
class PreparedProfileService {
public:
    static bool loadAndValidate(MapWebEditor& portal,
                                DocumentImportValidator& validator,
                                ProductionProfileRecord& record,
                                CableProfile& profile,
                                CableMap& map,
                                String& message,
                                DocumentValidationReport* documentReport = nullptr);
};

}  // namespace mg::p4
