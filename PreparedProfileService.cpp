#include "PreparedProfileService.h"

#include <LittleFS.h>
#include <stdio.h>
#include <vector>

#include "MapWebEditor.h"
#include "WorkplaceProfileJsonAdapter.h"

namespace mg::p4 {
namespace {

void fillMissingMobileIdentity(ProductionProfileRecord& record) {
    if (record.productionPr[0] == '\0') {
        snprintf(record.productionPr, sizeof(record.productionPr), "PR-MOBILE");
    }
    if (record.customerReference[0] == '\0') {
        snprintf(record.customerReference, sizeof(record.customerReference), "MOBILE-IMPORT");
    }
    if (record.revision[0] == '\0') {
        snprintf(record.revision, sizeof(record.revision), "UNSPECIFIED");
    }
    if (record.profileId[0] == '\0') {
        snprintf(record.profileId, sizeof(record.profileId), "MOBILE-PROFILE");
    }
}

}  // namespace

bool PreparedProfileService::loadAndValidate(MapWebEditor& portal,
                                             DocumentImportValidator& validator,
                                             ProductionProfileRecord& record,
                                             CableProfile& profile,
                                             CableMap& map,
                                             String& message,
                                             DocumentValidationReport* documentReport) {
    record = {};
    profile = makeNormalProfile();
    // Do not construct a ~2 KiB CableMap return-value temporary
    // on loopTask's call chain. buildRuntimeProfile() replaces the full map on
    // success, so an empty map is the correct low-stack scratch state here.
    map.clear();
    map.setName("CUSTOM MAP");

    const DocumentValidationReport report = validator.validate(portal);
    if (documentReport) *documentReport = report;
    if (!report.ok) {
        message = report.message;
        return false;
    }

    DocumentItemInfo item;
    if (!portal.documentItem(0U, item) || item.kind != DocumentFileKind::Json) {
        message = "Prepared profile JSON is unavailable";
        return false;
    }

    File file = LittleFS.open(item.path, FILE_READ);
    if (!file) {
        message = "Profile JSON cannot be opened";
        return false;
    }
    const size_t size = file.size();
    if (size == 0U || size > 256U * 1024U) {
        file.close();
        message = "Prepared profile JSON size is invalid";
        return false;
    }

    std::vector<char> body(size + 1U, '\0');
    const size_t got = file.readBytes(body.data(), size);
    file.close();
    if (got != size) {
        message = "Prepared profile JSON read failed";
        return false;
    }

    CanonicalWorkplaceJsonAdapter adapter;
    char parseError[160] = {};
    if (!adapter.parse(body.data(), size, record, parseError, sizeof(parseError))) {
        message = String("Profile JSON parse failed: ") + parseError;
        return false;
    }

    // Mobile/manual profiles do not have to originate from the workplace PR
    // lookup. Preserve any supplied identity and fill only truly missing data.
    fillMissingMobileIdentity(record);

    ProductionProfileValidation validation;
    if (!ProductionProfileContract::buildRuntimeProfile(record, profile, map, &validation)) {
        message = String("Electrical profile validation failed: ") + validation.message;
        return false;
    }

    message = "Prepared mobile profile parsed and electrically validated";
    return true;
}

}  // namespace mg::p4
