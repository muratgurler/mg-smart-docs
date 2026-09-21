#include "WorkflowArchiveCore.h"

#include <stdio.h>
#include <string.h>

namespace mg::p4 {
namespace {

uint64_t mix64(uint64_t hash, uint64_t value) {
    constexpr uint64_t kPrime = 1099511628211ULL;
    for (uint8_t i = 0U; i < 8U; ++i) {
        hash ^= static_cast<uint8_t>((value >> (i * 8U)) & 0xFFU);
        hash *= kPrime;
    }
    return hash;
}

bool sameText(const char* a, const char* b) {
    const char* aa = a != nullptr ? a : "";
    const char* bb = b != nullptr ? b : "";
    return strcmp(aa, bb) == 0;
}

}  // namespace

void WorkflowArchiveCore::copyText(char* dst, size_t dstSize, const char* src) {
    if (dst == nullptr || dstSize == 0U) return;
    snprintf(dst, dstSize, "%s", src != nullptr ? src : "");
}

void WorkflowArchiveCore::bindProfileStrings(WorkflowCachedProfile& item) {
    item.profile.name = item.profileName;
    item.profile.sideA.displayName = item.connectorA;
    item.profile.sideB.displayName = item.connectorB;
}

void WorkflowArchiveCore::markChanged() {
    ++changeSerial_;
    if (changeSerial_ == 0U) changeSerial_ = 1U;
}

void WorkflowArchiveCore::reset() {
    for (auto& result : results_) result = {};
    for (auto& profile : profiles_) profile = {};
    resultCount_ = 0U;
    resultWriteIndex_ = 0U;
    profileCacheCount_ = 0U;
    nextRecordId_ = 1U;
    nextCacheId_ = 1U;
    useOrdinal_ = 1U;
    markChanged();
}

uint64_t WorkflowArchiveCore::fingerprintMap(const CableMap& map) {
    uint64_t hash = 1469598103934665603ULL;
    for (uint8_t i = 0U; i < kTestPointsPerSide; ++i) {
        hash = mix64(hash, map.receiverMaskAtoB(i));
        hash = mix64(hash, map.sameSideMaskA(i));
        hash = mix64(hash, map.sameSideMaskB(i));
    }
    return hash;
}

int WorkflowArchiveCore::findResult(uint32_t generation, uint16_t attempt) const {
    for (size_t i = 0U; i < kWorkflowResultCapacity; ++i) {
        if (results_[i].recordId != 0U &&
            results_[i].generation == generation &&
            results_[i].attempt == attempt) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool WorkflowArchiveCore::upsertCompleted(const TestWorkflowSnapshot& snapshot,
                                          const CableProfile& profile,
                                          const CableMap& map,
                                          bool qualityWarning) {
    if (snapshot.state != TestWorkflowState::Complete ||
        snapshot.verdict == TestWorkflowVerdict::None ||
        !snapshot.profileValid || snapshot.attempt == 0U) {
        return false;
    }

    int index = findResult(snapshot.generation, snapshot.attempt);
    if (index < 0) {
        index = static_cast<int>(resultWriteIndex_);
        resultWriteIndex_ = (resultWriteIndex_ + 1U) % kWorkflowResultCapacity;
        if (resultCount_ < kWorkflowResultCapacity) ++resultCount_;
        results_[static_cast<size_t>(index)] = {};
        results_[static_cast<size_t>(index)].recordId = nextRecordId_++;
    }

    WorkflowTestRecord& record = results_[static_cast<size_t>(index)];
    record.generation = snapshot.generation;
    record.attempt = snapshot.attempt;
    record.source = snapshot.source;
    record.verdict = snapshot.verdict;
    record.identity = snapshot.identity;
    record.electricalErrors = snapshot.electricalErrors;
    record.signalPinCount = snapshot.signalPinCount;
    record.customMap = snapshot.customMap;
    record.qualityWarning = qualityWarning;
    record.mapFingerprint = fingerprintMap(map);
    copyText(record.profileName, sizeof(record.profileName), profile.name);
    copyText(record.connectorA, sizeof(record.connectorA), profile.sideA.displayName);
    copyText(record.connectorB, sizeof(record.connectorB), profile.sideB.displayName);
    markChanged();
    return true;
}

bool WorkflowArchiveCore::cacheKeyMatches(const WorkflowCachedProfile& item,
                                          TestProfileSource source,
                                          const TestWorkflowIdentity& identity,
                                          const CableProfile& profile,
                                          bool customMap,
                                          uint64_t mapFingerprint) {
    if (!item.valid || item.source != source || item.customMap != customMap ||
        item.mapFingerprint != mapFingerprint) {
        return false;
    }
    if (identity.profileId[0] != '\0' || item.identity.profileId[0] != '\0') {
        return sameText(item.identity.profileId, identity.profileId) &&
               sameText(item.identity.revision, identity.revision);
    }
    return sameText(item.profileName, profile.name) &&
           item.profile.signalPinCount == profile.signalPinCount &&
           sameText(item.connectorA, profile.sideA.displayName) &&
           sameText(item.connectorB, profile.sideB.displayName);
}

int WorkflowArchiveCore::findCache(TestProfileSource source,
                                   const TestWorkflowIdentity& identity,
                                   const CableProfile& profile,
                                   bool customMap,
                                   uint64_t mapFingerprint) const {
    for (size_t i = 0U; i < kWorkflowProfileCacheCapacity; ++i) {
        if (cacheKeyMatches(profiles_[i], source, identity, profile, customMap, mapFingerprint)) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

bool WorkflowArchiveCore::cacheProfile(TestProfileSource source,
                                       const TestWorkflowIdentity& identity,
                                       const CableProfile& profile,
                                       const CableMap& map,
                                       bool customMap) {
    const uint64_t fingerprint = fingerprintMap(map);
    int index = findCache(source, identity, profile, customMap, fingerprint);
    if (index < 0) {
        // Reuse an empty slot first, otherwise evict least-recently-used.
        uint32_t oldestUse = UINT32_MAX;
        size_t oldestIndex = 0U;
        for (size_t i = 0U; i < kWorkflowProfileCacheCapacity; ++i) {
            if (!profiles_[i].valid) {
                oldestIndex = i;
                oldestUse = 0U;
                break;
            }
            if (profiles_[i].lastUseOrdinal < oldestUse) {
                oldestUse = profiles_[i].lastUseOrdinal;
                oldestIndex = i;
            }
        }
        index = static_cast<int>(oldestIndex);
        if (!profiles_[oldestIndex].valid && profileCacheCount_ < kWorkflowProfileCacheCapacity) {
            ++profileCacheCount_;
        }
        profiles_[oldestIndex] = {};
        profiles_[oldestIndex].valid = true;
        profiles_[oldestIndex].cacheId = nextCacheId_++;
    }

    WorkflowCachedProfile& item = profiles_[static_cast<size_t>(index)];
    item.source = source;
    item.identity = identity;
    item.customMap = customMap;
    item.mapFingerprint = fingerprint;
    item.lastUseOrdinal = useOrdinal_++;
    item.profile = profile;
    item.map = map;
    copyText(item.profileName, sizeof(item.profileName), profile.name);
    copyText(item.connectorA, sizeof(item.connectorA), profile.sideA.displayName);
    copyText(item.connectorB, sizeof(item.connectorB), profile.sideB.displayName);
    bindProfileStrings(item);
    markChanged();
    return true;
}

const WorkflowTestRecord* WorkflowArchiveCore::resultNewest(size_t offset) const {
    if (offset >= resultCount_) return nullptr;
    const size_t newest = (resultWriteIndex_ + kWorkflowResultCapacity - 1U) % kWorkflowResultCapacity;
    const size_t index = (newest + kWorkflowResultCapacity - offset) % kWorkflowResultCapacity;
    return results_[index].recordId != 0U ? &results_[index] : nullptr;
}

size_t WorkflowArchiveCore::cacheIndexNewest(size_t offset) const {
    if (offset >= profileCacheCount_) return kWorkflowProfileCacheCapacity;
    // Tiny fixed cache: selection sort by lastUseOrdinal is clearer and avoids
    // an STL dependency on the embedded build.
    uint32_t ceiling = UINT32_MAX;
    size_t selected = kWorkflowProfileCacheCapacity;
    for (size_t rank = 0U; rank <= offset; ++rank) {
        uint32_t best = 0U;
        selected = kWorkflowProfileCacheCapacity;
        for (size_t i = 0U; i < kWorkflowProfileCacheCapacity; ++i) {
            const auto& item = profiles_[i];
            if (!item.valid || item.lastUseOrdinal >= ceiling) continue;
            if (item.lastUseOrdinal > best) {
                best = item.lastUseOrdinal;
                selected = i;
            }
        }
        if (selected == kWorkflowProfileCacheCapacity) return selected;
        ceiling = profiles_[selected].lastUseOrdinal;
    }
    return selected;
}

const WorkflowCachedProfile* WorkflowArchiveCore::cachedProfileNewest(size_t offset) const {
    const size_t index = cacheIndexNewest(offset);
    return index < kWorkflowProfileCacheCapacity ? &profiles_[index] : nullptr;
}

bool WorkflowArchiveCore::loadCachedProfile(size_t newestOffset,
                                            TestProfileSource& source,
                                            TestWorkflowIdentity& identity,
                                            const CableProfile*& profile,
                                            const CableMap*& map,
                                            bool& customMap) {
    const size_t index = cacheIndexNewest(newestOffset);
    if (index >= kWorkflowProfileCacheCapacity) return false;
    WorkflowCachedProfile& item = profiles_[index];
    item.lastUseOrdinal = useOrdinal_++;
    bindProfileStrings(item);
    markChanged();
    source = item.source;
    identity = item.identity;
    profile = &item.profile;
    map = &item.map;
    customMap = item.customMap;
    return true;
}

bool WorkflowArchiveCore::restoreRecord(const WorkflowTestRecord& record) {
    if (record.recordId == 0U || record.attempt == 0U ||
        record.source > TestProfileSource::ProfileCache ||
        record.verdict > TestWorkflowVerdict::Fail ||
        record.signalPinCount > kMaximumSignalPins) {
        return false;
    }

    WorkflowTestRecord copy = record;
    const size_t index = resultWriteIndex_;
    results_[index] = copy;
    resultWriteIndex_ = (resultWriteIndex_ + 1U) % kWorkflowResultCapacity;
    if (resultCount_ < kWorkflowResultCapacity) ++resultCount_;
    if (copy.recordId >= nextRecordId_) nextRecordId_ = copy.recordId + 1U;
    markChanged();
    return true;
}

bool WorkflowArchiveCore::restoreCachedProfile(const WorkflowCachedProfile& item) {
    if (!item.valid || item.cacheId == 0U ||
        item.source > TestProfileSource::ProfileCache ||
        item.profile.signalPinCount > kMaximumSignalPins) {
        return false;
    }

    size_t index = kWorkflowProfileCacheCapacity;
    for (size_t i = 0U; i < kWorkflowProfileCacheCapacity; ++i) {
        if (!profiles_[i].valid) {
            index = i;
            break;
        }
    }
    if (index >= kWorkflowProfileCacheCapacity) return false;

    WorkflowCachedProfile& dst = profiles_[index];
    dst = item;
    copyText(dst.profileName, sizeof(dst.profileName), item.profileName);
    copyText(dst.connectorA, sizeof(dst.connectorA), item.connectorA);
    copyText(dst.connectorB, sizeof(dst.connectorB), item.connectorB);
    bindProfileStrings(dst);
    dst.profile.sideA.frontImage = nullptr;
    dst.profile.sideB.frontImage = nullptr;
    ++profileCacheCount_;
    if (dst.cacheId >= nextCacheId_) nextCacheId_ = dst.cacheId + 1U;
    if (dst.lastUseOrdinal >= useOrdinal_) useOrdinal_ = dst.lastUseOrdinal + 1U;
    markChanged();
    return true;
}

uint16_t WorkflowArchiveCore::passCount() const {
    uint16_t count = 0U;
    for (size_t i = 0U; i < kWorkflowResultCapacity; ++i)
        if (results_[i].recordId != 0U && results_[i].verdict == TestWorkflowVerdict::Pass) ++count;
    return count;
}

uint16_t WorkflowArchiveCore::warningCount() const {
    uint16_t count = 0U;
    for (size_t i = 0U; i < kWorkflowResultCapacity; ++i)
        if (results_[i].recordId != 0U && results_[i].verdict == TestWorkflowVerdict::Warning) ++count;
    return count;
}

uint16_t WorkflowArchiveCore::failCount() const {
    uint16_t count = 0U;
    for (size_t i = 0U; i < kWorkflowResultCapacity; ++i)
        if (results_[i].recordId != 0U && results_[i].verdict == TestWorkflowVerdict::Fail) ++count;
    return count;
}

uint16_t WorkflowArchiveCore::firstAttemptCount() const {
    uint16_t count = 0U;
    for (size_t i = 0U; i < kWorkflowResultCapacity; ++i) {
        if (results_[i].recordId != 0U && results_[i].attempt == 1U) ++count;
    }
    return count;
}

uint16_t WorkflowArchiveCore::firstPassCount() const {
    uint16_t count = 0U;
    for (size_t i = 0U; i < kWorkflowResultCapacity; ++i) {
        if (results_[i].recordId != 0U && results_[i].attempt == 1U &&
            results_[i].verdict == TestWorkflowVerdict::Pass) {
            ++count;
        }
    }
    return count;
}

uint16_t WorkflowArchiveCore::retestCount() const {
    uint16_t count = 0U;
    for (size_t i = 0U; i < kWorkflowResultCapacity; ++i) {
        if (results_[i].recordId != 0U && results_[i].attempt > 1U) ++count;
    }
    return count;
}

void WorkflowArchiveCore::formatRecord(const WorkflowTestRecord& record,
                                       char* output,
                                       size_t outputSize,
                                       bool turkish) const {
    if (output == nullptr || outputSize == 0U) return;
    snprintf(output, outputSize,
             "#%lu | %s | %s | %s=%u | %s=%u | attempt=%u",
             static_cast<unsigned long>(record.recordId),
             TestWorkflowController::sourceText(record.source, turkish),
             TestWorkflowController::verdictText(record.verdict),
             turkish ? "hata" : "errors", static_cast<unsigned>(record.electricalErrors),
             turkish ? "pin" : "pins", static_cast<unsigned>(record.signalPinCount),
             static_cast<unsigned>(record.attempt));
}

void WorkflowArchiveCore::formatRecordCsv(const WorkflowTestRecord& record,
                                          char* output,
                                          size_t outputSize) const {
    if (output == nullptr || outputSize == 0U) return;
    snprintf(output, outputSize,
             "%lu,%lu,%u,%u,%s,%u,%u,\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",%s,%08lX%08lX",
             static_cast<unsigned long>(record.recordId),
             static_cast<unsigned long>(record.generation),
             static_cast<unsigned>(record.attempt),
             static_cast<unsigned>(record.source),
             TestWorkflowController::verdictText(record.verdict),
             static_cast<unsigned>(record.electricalErrors),
             static_cast<unsigned>(record.signalPinCount),
             record.identity.productionPr,
             record.identity.customerReference,
             record.identity.revision,
             record.identity.profileId,
             record.profileName,
             record.customMap ? "NET" : "1:1",
             record.qualityWarning ? "YES" : "NO",
             static_cast<unsigned long>((record.mapFingerprint >> 32U) & 0xFFFFFFFFULL),
             static_cast<unsigned long>(record.mapFingerprint & 0xFFFFFFFFULL));
}

}  // namespace mg::p4
