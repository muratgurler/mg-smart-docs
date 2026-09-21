#include "WorkflowArchiveBinaryCodec.h"

#include <string.h>
#include <new>

namespace mg::p4 {
namespace {

constexpr uint8_t kMagic[8] = {'M', 'G', 'A', 'R', '2', '3', 'B', '9'};

class Writer {
public:
    Writer(uint8_t* data, size_t capacity) : data_(data), capacity_(capacity) {}

    bool bytes(const void* src, size_t count) {
        if (!ok_ || src == nullptr || position_ + count > capacity_) {
            ok_ = false;
            return false;
        }
        memcpy(data_ + position_, src, count);
        position_ += count;
        return true;
    }
    bool u8(uint8_t value) { return bytes(&value, 1U); }
    bool u16(uint16_t value) {
        uint8_t b[2] = {static_cast<uint8_t>(value & 0xFFU),
                        static_cast<uint8_t>((value >> 8U) & 0xFFU)};
        return bytes(b, sizeof(b));
    }
    bool u32(uint32_t value) {
        uint8_t b[4] = {
            static_cast<uint8_t>(value & 0xFFU),
            static_cast<uint8_t>((value >> 8U) & 0xFFU),
            static_cast<uint8_t>((value >> 16U) & 0xFFU),
            static_cast<uint8_t>((value >> 24U) & 0xFFU),
        };
        return bytes(b, sizeof(b));
    }
    bool u64(uint64_t value) {
        uint8_t b[8]{};
        for (uint8_t i = 0U; i < 8U; ++i) {
            b[i] = static_cast<uint8_t>((value >> (8U * i)) & 0xFFU);
        }
        return bytes(b, sizeof(b));
    }
    bool fixedText(const char* text, size_t count) {
        if (!ok_ || position_ + count > capacity_) {
            ok_ = false;
            return false;
        }
        memset(data_ + position_, 0, count);
        if (text != nullptr && count > 0U) {
            size_t length = 0U;
            while (length + 1U < count && text[length] != '\0') ++length;
            memcpy(data_ + position_, text, length);
        }
        position_ += count;
        return true;
    }
    bool ok() const { return ok_; }
    size_t size() const { return position_; }

private:
    uint8_t* data_ = nullptr;
    size_t capacity_ = 0U;
    size_t position_ = 0U;
    bool ok_ = true;
};

class Reader {
public:
    Reader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    bool bytes(void* dst, size_t count) {
        if (!ok_ || dst == nullptr || position_ + count > size_) {
            ok_ = false;
            return false;
        }
        memcpy(dst, data_ + position_, count);
        position_ += count;
        return true;
    }
    bool u8(uint8_t& value) { return bytes(&value, 1U); }
    bool u16(uint16_t& value) {
        uint8_t b[2]{};
        if (!bytes(b, sizeof(b))) return false;
        value = static_cast<uint16_t>(b[0]) |
                static_cast<uint16_t>(static_cast<uint16_t>(b[1]) << 8U);
        return true;
    }
    bool u32(uint32_t& value) {
        uint8_t b[4]{};
        if (!bytes(b, sizeof(b))) return false;
        value = static_cast<uint32_t>(b[0]) |
                (static_cast<uint32_t>(b[1]) << 8U) |
                (static_cast<uint32_t>(b[2]) << 16U) |
                (static_cast<uint32_t>(b[3]) << 24U);
        return true;
    }
    bool u64(uint64_t& value) {
        uint8_t b[8]{};
        if (!bytes(b, sizeof(b))) return false;
        value = 0ULL;
        for (uint8_t i = 0U; i < 8U; ++i) {
            value |= static_cast<uint64_t>(b[i]) << (8U * i);
        }
        return true;
    }
    bool fixedText(char* text, size_t count) {
        if (!bytes(text, count)) return false;
        if (count > 0U) text[count - 1U] = '\0';
        return true;
    }
    bool ok() const { return ok_; }
    size_t position() const { return position_; }

private:
    const uint8_t* data_ = nullptr;
    size_t size_ = 0U;
    size_t position_ = 0U;
    bool ok_ = true;
};

bool writeIdentity(Writer& writer, const TestWorkflowIdentity& identity) {
    return writer.fixedText(identity.productionPr, sizeof(identity.productionPr)) &&
           writer.fixedText(identity.customerReference, sizeof(identity.customerReference)) &&
           writer.fixedText(identity.revision, sizeof(identity.revision)) &&
           writer.fixedText(identity.profileId, sizeof(identity.profileId)) &&
           writer.fixedText(identity.sourceCode, sizeof(identity.sourceCode));
}

bool readIdentity(Reader& reader, TestWorkflowIdentity& identity) {
    return reader.fixedText(identity.productionPr, sizeof(identity.productionPr)) &&
           reader.fixedText(identity.customerReference, sizeof(identity.customerReference)) &&
           reader.fixedText(identity.revision, sizeof(identity.revision)) &&
           reader.fixedText(identity.profileId, sizeof(identity.profileId)) &&
           reader.fixedText(identity.sourceCode, sizeof(identity.sourceCode));
}

bool writeRecord(Writer& writer, const WorkflowTestRecord& record) {
    return writer.u32(record.recordId) &&
           writer.u32(record.generation) &&
           writer.u16(record.attempt) &&
           writer.u8(static_cast<uint8_t>(record.source)) &&
           writer.u8(static_cast<uint8_t>(record.verdict)) &&
           writeIdentity(writer, record.identity) &&
           writer.u16(record.electricalErrors) &&
           writer.u8(record.signalPinCount) &&
           writer.u8(record.customMap ? 1U : 0U) &&
           writer.u8(record.qualityWarning ? 1U : 0U) &&
           writer.u64(record.mapFingerprint) &&
           writer.fixedText(record.profileName, sizeof(record.profileName)) &&
           writer.fixedText(record.connectorA, sizeof(record.connectorA)) &&
           writer.fixedText(record.connectorB, sizeof(record.connectorB));
}

bool readRecord(Reader& reader, WorkflowTestRecord& record) {
    uint8_t source = 0U;
    uint8_t verdict = 0U;
    uint8_t customMap = 0U;
    uint8_t qualityWarning = 0U;
    if (!reader.u32(record.recordId) ||
        !reader.u32(record.generation) ||
        !reader.u16(record.attempt) ||
        !reader.u8(source) ||
        !reader.u8(verdict) ||
        !readIdentity(reader, record.identity) ||
        !reader.u16(record.electricalErrors) ||
        !reader.u8(record.signalPinCount) ||
        !reader.u8(customMap) ||
        !reader.u8(qualityWarning) ||
        !reader.u64(record.mapFingerprint) ||
        !reader.fixedText(record.profileName, sizeof(record.profileName)) ||
        !reader.fixedText(record.connectorA, sizeof(record.connectorA)) ||
        !reader.fixedText(record.connectorB, sizeof(record.connectorB))) {
        return false;
    }
    if (source > static_cast<uint8_t>(TestProfileSource::ProfileCache) ||
        verdict > static_cast<uint8_t>(TestWorkflowVerdict::Fail) ||
        customMap > 1U || qualityWarning > 1U ||
        record.recordId == 0U || record.attempt == 0U ||
        record.signalPinCount > kMaximumSignalPins) {
        return false;
    }
    record.source = static_cast<TestProfileSource>(source);
    record.verdict = static_cast<TestWorkflowVerdict>(verdict);
    record.customMap = customMap != 0U;
    record.qualityWarning = qualityWarning != 0U;
    return true;
}

bool writeProfile(Writer& writer, const WorkflowCachedProfile& item) {
    if (!writer.u32(item.cacheId) ||
        !writer.u32(item.lastUseOrdinal) ||
        !writer.u8(static_cast<uint8_t>(item.source)) ||
        !writeIdentity(writer, item.identity) ||
        !writer.u8(item.customMap ? 1U : 0U) ||
        !writer.u64(item.mapFingerprint) ||
        !writer.fixedText(item.profileName, sizeof(item.profileName)) ||
        !writer.fixedText(item.connectorA, sizeof(item.connectorA)) ||
        !writer.fixedText(item.connectorB, sizeof(item.connectorB)) ||
        !writer.u8(static_cast<uint8_t>(item.profile.kind)) ||
        !writer.u8(item.profile.signalPinCount) ||
        !writer.u8(item.profile.includePe ? 1U : 0U) ||
        !writer.u8(item.profile.includeDrainShield ? 1U : 0U) ||
        !writer.u8(item.profile.includePeA ? 1U : 0U) ||
        !writer.u8(item.profile.includePeB ? 1U : 0U) ||
        !writer.u8(item.profile.includeDrainShieldA ? 1U : 0U) ||
        !writer.u8(item.profile.includeDrainShieldB ? 1U : 0U) ||
        !writer.u8(static_cast<uint8_t>(item.profile.sideA.kind)) ||
        !writer.u8(static_cast<uint8_t>(item.profile.sideA.gender)) ||
        !writer.u8(item.profile.sideA.contactCount) ||
        !writer.u8(static_cast<uint8_t>(item.profile.sideB.kind)) ||
        !writer.u8(static_cast<uint8_t>(item.profile.sideB.gender)) ||
        !writer.u8(item.profile.sideB.contactCount) ||
        !writer.fixedText(item.map.name(), 40U)) {
        return false;
    }
    for (uint8_t i = 0U; i < kTestPointsPerSide; ++i) {
        if (!writer.u64(item.map.receiverMaskAtoB(i)) ||
            !writer.u64(item.map.sameSideMaskA(i)) ||
            !writer.u64(item.map.sameSideMaskB(i))) {
            return false;
        }
    }
    return true;
}

bool readProfile(Reader& reader, WorkflowCachedProfile& item) {
    uint8_t source = 0U;
    uint8_t customMap = 0U;
    uint8_t kind = 0U;
    uint8_t includePe = 0U;
    uint8_t includeDs = 0U;
    uint8_t peA = 0U;
    uint8_t peB = 0U;
    uint8_t dsA = 0U;
    uint8_t dsB = 0U;
    uint8_t aKind = 0U;
    uint8_t aGender = 0U;
    uint8_t bKind = 0U;
    uint8_t bGender = 0U;
    char mapName[40]{};
    uint64_t aToB[kTestPointsPerSide]{};
    uint64_t sameA[kTestPointsPerSide]{};
    uint64_t sameB[kTestPointsPerSide]{};

    item = {};
    item.valid = true;
    if (!reader.u32(item.cacheId) ||
        !reader.u32(item.lastUseOrdinal) ||
        !reader.u8(source) ||
        !readIdentity(reader, item.identity) ||
        !reader.u8(customMap) ||
        !reader.u64(item.mapFingerprint) ||
        !reader.fixedText(item.profileName, sizeof(item.profileName)) ||
        !reader.fixedText(item.connectorA, sizeof(item.connectorA)) ||
        !reader.fixedText(item.connectorB, sizeof(item.connectorB)) ||
        !reader.u8(kind) ||
        !reader.u8(item.profile.signalPinCount) ||
        !reader.u8(includePe) ||
        !reader.u8(includeDs) ||
        !reader.u8(peA) ||
        !reader.u8(peB) ||
        !reader.u8(dsA) ||
        !reader.u8(dsB) ||
        !reader.u8(aKind) ||
        !reader.u8(aGender) ||
        !reader.u8(item.profile.sideA.contactCount) ||
        !reader.u8(bKind) ||
        !reader.u8(bGender) ||
        !reader.u8(item.profile.sideB.contactCount) ||
        !reader.fixedText(mapName, sizeof(mapName))) {
        return false;
    }

    const auto boolByte = [](uint8_t value) { return value <= 1U; };
    if (item.cacheId == 0U ||
        source > static_cast<uint8_t>(TestProfileSource::ProfileCache) ||
        customMap > 1U ||
        kind > static_cast<uint8_t>(ScanProfileKind::SubD) ||
        item.profile.signalPinCount > kMaximumSignalPins ||
        !boolByte(includePe) || !boolByte(includeDs) || !boolByte(peA) ||
        !boolByte(peB) || !boolByte(dsA) || !boolByte(dsB) ||
        aKind > static_cast<uint8_t>(ConnectorKind::Custom) ||
        bKind > static_cast<uint8_t>(ConnectorKind::Custom) ||
        aGender > static_cast<uint8_t>(ConnectorGender::Neutral) ||
        bGender > static_cast<uint8_t>(ConnectorGender::Neutral)) {
        return false;
    }

    for (uint8_t i = 0U; i < kTestPointsPerSide; ++i) {
        if (!reader.u64(aToB[i]) || !reader.u64(sameA[i]) || !reader.u64(sameB[i])) {
            return false;
        }
    }

    item.source = static_cast<TestProfileSource>(source);
    item.customMap = customMap != 0U;
    item.profile.name = item.profileName;
    item.profile.kind = static_cast<ScanProfileKind>(kind);
    item.profile.includePe = includePe != 0U;
    item.profile.includeDrainShield = includeDs != 0U;
    item.profile.includePeA = peA != 0U;
    item.profile.includePeB = peB != 0U;
    item.profile.includeDrainShieldA = dsA != 0U;
    item.profile.includeDrainShieldB = dsB != 0U;
    item.profile.sideA.kind = static_cast<ConnectorKind>(aKind);
    item.profile.sideA.gender = static_cast<ConnectorGender>(aGender);
    item.profile.sideA.displayName = item.connectorA;
    item.profile.sideA.frontImage = nullptr;
    item.profile.sideB.kind = static_cast<ConnectorKind>(bKind);
    item.profile.sideB.gender = static_cast<ConnectorGender>(bGender);
    item.profile.sideB.displayName = item.connectorB;
    item.profile.sideB.frontImage = nullptr;
    item.map.loadRaw(aToB, sameA, sameB, mapName);
    return WorkflowArchiveCore::fingerprintMap(item.map) == item.mapFingerprint;
}

}  // namespace

uint32_t WorkflowArchiveBinaryCodec::crc32(const uint8_t* data, size_t size) {
    uint32_t crc = 0xFFFFFFFFU;
    for (size_t i = 0U; i < size; ++i) {
        crc ^= static_cast<uint32_t>(data[i]);
        for (uint8_t bit = 0U; bit < 8U; ++bit) {
            const uint32_t mask = static_cast<uint32_t>(-(static_cast<int32_t>(crc & 1U)));
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return ~crc;
}

bool WorkflowArchiveBinaryCodec::encode(const WorkflowArchiveCore& archive,
                                        uint8_t* output,
                                        size_t outputCapacity,
                                        size_t& outputSize) {
    outputSize = 0U;
    if (output == nullptr || outputCapacity < 32U) return false;

    Writer writer(output, outputCapacity);
    writer.bytes(kMagic, sizeof(kMagic));
    writer.u16(kSchemaVersion);
    writer.u16(static_cast<uint16_t>(archive.resultCount()));
    writer.u16(static_cast<uint16_t>(archive.profileCacheCount()));
    writer.u16(0U);  // reserved

    // Persist logical order oldest -> newest. Ring-buffer physical indexes are
    // intentionally not part of the on-flash ABI.
    for (size_t offset = archive.resultCount(); offset > 0U; --offset) {
        const WorkflowTestRecord* record = archive.resultNewest(offset - 1U);
        if (record == nullptr || !writeRecord(writer, *record)) return false;
    }
    for (size_t offset = archive.profileCacheCount(); offset > 0U; --offset) {
        const WorkflowCachedProfile* item = archive.cachedProfileNewest(offset - 1U);
        if (item == nullptr || !writeProfile(writer, *item)) return false;
    }
    if (!writer.ok() || writer.size() + sizeof(uint32_t) > outputCapacity) return false;

    const uint32_t crc = crc32(output, writer.size());
    if (!writer.u32(crc)) return false;
    outputSize = writer.size();
    return true;
}

bool WorkflowArchiveBinaryCodec::decode(const uint8_t* data,
                                        size_t dataSize,
                                        WorkflowArchiveCore& archive) {
    if (data == nullptr || dataSize < sizeof(kMagic) + 12U ||
        dataSize > kWorkflowArchiveBinaryMaxBytes) {
        return false;
    }

    const size_t crcOffset = dataSize - sizeof(uint32_t);
    Reader crcReader(data + crcOffset, sizeof(uint32_t));
    uint32_t storedCrc = 0U;
    if (!crcReader.u32(storedCrc) || storedCrc != crc32(data, crcOffset)) return false;

    Reader reader(data, crcOffset);
    uint8_t magic[sizeof(kMagic)]{};
    uint16_t schema = 0U;
    uint16_t resultCount = 0U;
    uint16_t profileCount = 0U;
    uint16_t reserved = 0U;
    if (!reader.bytes(magic, sizeof(magic)) || memcmp(magic, kMagic, sizeof(kMagic)) != 0 ||
        !reader.u16(schema) || schema != kSchemaVersion ||
        !reader.u16(resultCount) || resultCount > kWorkflowResultCapacity ||
        !reader.u16(profileCount) || profileCount > kWorkflowProfileCacheCapacity ||
        !reader.u16(reserved)) {
        return false;
    }

    WorkflowTestRecord* results = resultCount > 0U
        ? new (std::nothrow) WorkflowTestRecord[resultCount]{} : nullptr;
    WorkflowCachedProfile* profiles = profileCount > 0U
        ? new (std::nothrow) WorkflowCachedProfile[profileCount]{} : nullptr;
    if ((resultCount > 0U && results == nullptr) ||
        (profileCount > 0U && profiles == nullptr)) {
        delete[] results;
        delete[] profiles;
        return false;
    }

    bool valid = true;
    for (uint16_t i = 0U; i < resultCount && valid; ++i) {
        valid = readRecord(reader, results[i]);
    }
    for (uint16_t i = 0U; i < profileCount && valid; ++i) {
        valid = readProfile(reader, profiles[i]);
    }
    valid = valid && reader.ok() && reader.position() == crcOffset;
    if (!valid) {
        delete[] results;
        delete[] profiles;
        return false;
    }

    archive.reset();
    for (uint16_t i = 0U; i < resultCount; ++i) {
        if (!archive.restoreRecord(results[i])) {
            delete[] results;
            delete[] profiles;
            return false;
        }
    }
    for (uint16_t i = 0U; i < profileCount; ++i) {
        if (!archive.restoreCachedProfile(profiles[i])) {
            delete[] results;
            delete[] profiles;
            return false;
        }
    }
    delete[] results;
    delete[] profiles;
    return true;
}

}  // namespace mg::p4
