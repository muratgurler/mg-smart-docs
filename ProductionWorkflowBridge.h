#pragma once

#include <stddef.h>

#include "ProductionProfileContract.h"
#include "TestWorkflowController.h"

namespace mg::p4 {

struct ProductionBridgeResult {
    bool accepted = false;
    bool ready = false;
    bool autoStartRequested = false;
    ProductionProfileValidation validation{};
    char message[160] = {};
};

class ProductionWorkflowBridge {
public:
    // expectedPr is used when lookup started from a workplace PR barcode.
    // expectedCustomerReference is used when lookup started from a customer
    // barcode. Supplying the origin makes the two identifiers cross-check
    // against the returned server record while both remain stored separately.
    static ProductionBridgeResult apply(TestWorkflowController& workflow,
                                        const ProductionProfileRecord& record,
                                        const char* expectedPr = nullptr,
                                        const char* expectedCustomerReference = nullptr);
};

}  // namespace mg::p4
