#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../TestWorkflowController.h"
#include "../WorkflowArchiveCore.h"

using namespace mg::p4;

int main() {
    WorkflowArchiveCore archive;
    archive.reset();
    TestWorkflowController workflow;
    workflow.attachArchive(archive);

    CableProfile manual = makeNormalProfile(12U, true, true);
    assert(workflow.prepareManual(manual, false, nullptr));
    assert(workflow.snapshot().state == TestWorkflowState::Ready);
    assert(workflow.snapshot().attempt == 1U);
    assert(archive.profileCacheCount() == 1U);

    assert(workflow.markStartRequest());
    workflow.observeRunState(RunState::Running, 0U, 15U);
    workflow.observeRunState(RunState::Complete, 0U, 100U);
    assert(archive.resultCount() == 1U);
    const WorkflowTestRecord* first = archive.resultNewest(0U);
    assert(first != nullptr);
    assert(first->verdict == TestWorkflowVerdict::Pass);
    assert(first->attempt == 1U);

    // A late process/quality evaluation must update, not duplicate, this run.
    workflow.markQualityWarning(true);
    assert(archive.resultCount() == 1U);
    first = archive.resultNewest(0U);
    assert(first->verdict == TestWorkflowVerdict::Warning);
    assert(first->qualityWarning);

    const uint32_t generation = workflow.snapshot().generation;
    assert(workflow.prepareRetest());
    assert(workflow.snapshot().state == TestWorkflowState::Ready);
    assert(workflow.snapshot().generation == generation);
    assert(workflow.snapshot().attempt == 2U);
    workflow.observeRunState(RunState::Complete, 2U, 100U);
    assert(archive.resultCount() == 2U);
    const WorkflowTestRecord* second = archive.resultNewest(0U);
    assert(second != nullptr);
    assert(second->attempt == 2U);
    assert(second->verdict == TestWorkflowVerdict::Fail);

    assert(workflow.prepareNextCable() == NextCableDisposition::ReadySameProfile);
    assert(workflow.snapshot().state == TestWorkflowState::Ready);
    assert(workflow.snapshot().generation == generation + 1U);
    assert(workflow.snapshot().attempt == 1U);
    assert(!workflow.snapshot().autoStartRequested);

    // Production-PR next cable must require a new identifier instead of
    // silently reusing the previous DUT identity.
    TestWorkflowIdentity id{};
    (void)id;
    CableProfile server = makeNormalProfile(25U, true, true);
    assert(workflow.acceptNetworkResolvedProfile("PR000123", "ASML-REF-77", "C", "P-77-C", server, nullptr));
    workflow.observeRunState(RunState::Complete, 0U, 100U);
    assert(workflow.prepareNextCable() == NextCableDisposition::NeedsNewIdentifier);
    assert(workflow.snapshot().state == TestWorkflowState::NeedsLookup);
    assert(!workflow.snapshot().profileValid);
    assert(workflow.snapshot().identity.productionPr[0] == '\0');
    assert(!workflow.startAllowed());

    // Cache can restore a profile, but it becomes an explicit PROFILE CACHE
    // source and still waits for operator START.
    TestProfileSource cachedSource = TestProfileSource::None;
    TestWorkflowIdentity cachedId{};
    const CableProfile* cachedProfile = nullptr;
    const CableMap* cachedMap = nullptr;
    bool customMap = false;
    assert(archive.loadCachedProfile(0U, cachedSource, cachedId, cachedProfile, cachedMap, customMap));
    assert(cachedProfile != nullptr && cachedMap != nullptr);
    assert(workflow.prepareCachedProfile(*cachedProfile, *cachedMap, customMap, cachedId));
    assert(workflow.snapshot().source == TestProfileSource::ProfileCache);
    assert(workflow.snapshot().state == TestWorkflowState::Ready);
    assert(workflow.snapshot().identity.productionPr[0] == '\0');
    assert(!workflow.snapshot().autoStartRequested);
    assert(workflow.startAllowed());

    char csv[512]{};
    second = archive.resultNewest(0U);
    assert(second != nullptr);
    archive.formatRecordCsv(*second, csv, sizeof(csv));
    assert(strstr(csv, "PASS") != nullptr || strstr(csv, "FAIL") != nullptr || strstr(csv, "WARNING") != nullptr);

    printf("Workflow archive batch8 test passed. records=%u cache=%u\n",
           static_cast<unsigned>(archive.resultCount()),
           static_cast<unsigned>(archive.profileCacheCount()));
    return 0;
}
