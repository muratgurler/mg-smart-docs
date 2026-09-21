#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../TestWorkflowController.h"
#include "../WorkflowArchiveBinaryCodec.h"
#include "../WorkflowArchiveCore.h"

using namespace mg::p4;

int main() {
    WorkflowArchiveCore archive;
    archive.reset();
    TestWorkflowController workflow;
    workflow.attachArchive(archive);

    CableProfile profile = makeNormalProfile(18U, true, true);
    CableMap map;
    map.clear();
    map.setName("PERSIST-COMMON-NET");
    map.connectAtoB(7U, 9U, true);
    map.connectAtoB(7U, 12U, true);
    map.connectAtoB(7U, 14U, true);
    map.setSameSideConnection(CableMapSide::A, 7U, 9U, true);

    TestWorkflowIdentity identity{};
    snprintf(identity.customerReference, sizeof(identity.customerReference), "ASML-REF-42");
    snprintf(identity.revision, sizeof(identity.revision), "D");
    snprintf(identity.profileId, sizeof(identity.profileId), "PROFILE-42-D");
    assert(workflow.prepareProfile(TestProfileSource::DocumentImport, profile, &map, &identity));
    workflow.observeRunState(RunState::Complete, 0U, 100U);
    assert(archive.resultCount() == 1U);
    assert(archive.profileCacheCount() == 1U);

    assert(workflow.prepareRetest());
    workflow.observeRunState(RunState::Complete, 2U, 100U);
    assert(archive.resultCount() == 2U);
    assert(archive.retestCount() == 1U);
    assert(archive.firstAttemptCount() == 1U);
    assert(archive.firstPassCount() == 1U);

    uint8_t blob[kWorkflowArchiveBinaryMaxBytes]{};
    size_t blobSize = 0U;
    assert(WorkflowArchiveBinaryCodec::encode(archive, blob, sizeof(blob), blobSize));
    assert(blobSize > 100U && blobSize < sizeof(blob));

    WorkflowArchiveCore restored;
    restored.reset();
    assert(WorkflowArchiveBinaryCodec::decode(blob, blobSize, restored));
    assert(restored.resultCount() == archive.resultCount());
    assert(restored.profileCacheCount() == archive.profileCacheCount());
    assert(restored.passCount() == archive.passCount());
    assert(restored.failCount() == archive.failCount());
    assert(restored.retestCount() == 1U);

    const WorkflowTestRecord* newest = restored.resultNewest(0U);
    assert(newest != nullptr);
    assert(newest->attempt == 2U);
    assert(newest->verdict == TestWorkflowVerdict::Fail);
    assert(strcmp(newest->identity.customerReference, "ASML-REF-42") == 0);

    const WorkflowCachedProfile* cached = restored.cachedProfileNewest(0U);
    assert(cached != nullptr);
    assert(strcmp(cached->profileName, profile.name) == 0);
    assert(cached->profile.name == cached->profileName);
    assert(cached->profile.sideA.displayName == cached->connectorA);
    assert(cached->profile.sideB.displayName == cached->connectorB);
    assert(cached->mapFingerprint == WorkflowArchiveCore::fingerprintMap(cached->map));
    assert((cached->map.receiverMaskAtoB(7U) & (1ULL << 9U)) != 0ULL);
    assert((cached->map.receiverMaskAtoB(7U) & (1ULL << 12U)) != 0ULL);
    assert((cached->map.sameSideMaskA(7U) & (1ULL << 9U)) != 0ULL);

    // CRC protection must reject corruption without accepting a partial blob.
    const uint8_t saved = blob[blobSize / 2U];
    blob[blobSize / 2U] ^= 0x5AU;
    WorkflowArchiveCore untouched;
    untouched.reset();
    const uint32_t serialBefore = untouched.changeSerial();
    assert(!WorkflowArchiveBinaryCodec::decode(blob, blobSize, untouched));
    assert(untouched.resultCount() == 0U);
    assert(untouched.profileCacheCount() == 0U);
    assert(untouched.changeSerial() == serialBefore);
    blob[blobSize / 2U] = saved;

    printf("Workflow persistence batch9 test passed. bytes=%u results=%u cache=%u\n",
           static_cast<unsigned>(blobSize),
           static_cast<unsigned>(restored.resultCount()),
           static_cast<unsigned>(restored.profileCacheCount()));
    return 0;
}
