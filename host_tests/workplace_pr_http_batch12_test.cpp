#include "WorkplaceProfileJsonAdapter.h"
#include "WorkplaceProfileLookupCore.h"
#include "TestWorkflowController.h"

#include <assert.h>
#include <string.h>

using namespace mg::p4;

static const char* kGoodJson = R"JSON({
  "productionPr":"PR123456",
  "customerReference":"ASML-REF-98765",
  "revision":"R07",
  "profileId":"ASML-HARTING-6-R07",
  "signalPinCount":6,
  "includePeA":true,
  "includePeB":true,
  "includeDrainShieldA":false,
  "includeDrainShieldB":false,
  "connectorA":{"kind":"HARTING","gender":"MALE","contacts":10,"name":"X1"},
  "connectorB":{"kind":"HARTING","gender":"FEMALE","contacts":10,"name":"X2"},
  "nets":[
    {"A":[0],"B":[0]},
    {"A":[1],"B":[1,2]},
    {"A":[2],"B":[3]},
    {"A":[3,4],"B":[4]},
    {"A":[5],"B":[5]}
  ]
})JSON";

int main() {
    CanonicalWorkplaceJsonAdapter adapter;
    ProductionProfileRecord record{};
    char error[128]{};
    assert(adapter.parse(kGoodJson, strlen(kGoodJson), record, error, sizeof(error)));
    assert(strcmp(record.productionPr, "PR123456") == 0);
    assert(strcmp(record.connectorAName, "X1") == 0);
    assert(record.aToB[1] == ((1ULL << 1U) | (1ULL << 2U)));
    assert(record.sameSideA[3] == (1ULL << 4U));
    assert(record.sameSideA[4] == (1ULL << 3U));
    assert(record.aToB[6] == 0ULL); // NC/SPARE remains unassigned.

    TestWorkflowController workflow;
    workflow.acceptBarcodeClassification("PR123456", true, false, false, true, "", "");
    assert(workflow.snapshot().state == TestWorkflowState::NeedsLookup);
    const auto result = WorkplaceProfileLookupCore::applyResponse(
        workflow, adapter, kGoodJson, strlen(kGoodJson), "PR123456", nullptr, 200);
    assert(result.status == WorkplaceLookupStatus::Ready);
    assert(result.profileReady);
    assert(!result.autoStartRequested);
    assert(workflow.snapshot().state == TestWorkflowState::Ready);
    assert(workflow.startAllowed());
    assert(!workflow.snapshot().autoStartRequested);
    assert(workflow.hasPreparedProfile());
    assert(workflow.map().receiverMaskAtoB(1U) == ((1ULL << 1U) | (1ULL << 2U)));

    TestWorkflowController mismatch;
    mismatch.acceptBarcodeClassification("PR999999", true, false, false, true, "", "");
    const auto mismatchResult = WorkplaceProfileLookupCore::applyResponse(
        mismatch, adapter, kGoodJson, strlen(kGoodJson), "PR999999", nullptr, 200);
    assert(mismatchResult.status == WorkplaceLookupStatus::ProfileBlocked);
    assert(!mismatchResult.profileReady);
    assert(mismatch.snapshot().state == TestWorkflowState::Blocked);
    assert(!mismatch.hasPreparedProfile());

    TestWorkflowController httpFail;
    const auto httpResult = WorkplaceProfileLookupCore::applyResponse(
        httpFail, adapter, kGoodJson, strlen(kGoodJson), "PR123456", nullptr, 404);
    assert(httpResult.status == WorkplaceLookupStatus::HttpError);
    assert(httpFail.snapshot().state == TestWorkflowState::Blocked);

    static const char* overlap = R"JSON({
      "productionPr":"PR123456","customerReference":"REF","revision":"R1","profileId":"P1",
      "signalPinCount":2,"nets":[{"A":[1],"B":[1]},{"A":[1],"B":[2]}]
    })JSON";
    record = {};
    assert(!adapter.parse(overlap, strlen(overlap), record, error, sizeof(error)));

    static const char* badPoint = R"JSON({
      "productionPr":"PR123456","customerReference":"REF","revision":"R1","profileId":"P1",
      "signalPinCount":2,"nets":[{"A":[64],"B":[1]}]
    })JSON";
    record = {};
    assert(!adapter.parse(badPoint, strlen(badPoint), record, error, sizeof(error)));

    return 0;
}
