#pragma once

#include <stddef.h>
#include <stdint.h>

#include "WorkflowArchiveCore.h"

namespace mg::p4 {

class WorkflowArchiveBinaryCodec {
public:
    static constexpr uint16_t kSchemaVersion = 1U;

    static bool encode(const WorkflowArchiveCore& archive,
                       uint8_t* output,
                       size_t outputCapacity,
                       size_t& outputSize);

    // Decode is transactional with respect to syntax/CRC validation: the
    // target archive is not cleared until the entire blob has been validated.
    static bool decode(const uint8_t* data,
                       size_t dataSize,
                       WorkflowArchiveCore& archive);

    static uint32_t crc32(const uint8_t* data, size_t size);
};

}  // namespace mg::p4
