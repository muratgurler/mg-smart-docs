#pragma once

#include <Arduino.h>
#include <stdint.h>

namespace mg::p4 {

class MapWebEditor;

struct DocumentValidationReport {
    bool ok = false;
    uint8_t fileCount = 0U;
    uint8_t jsonCount = 0U;
    size_t totalBytes = 0U;
    int16_t failedIndex = -1;
    String message;
};

class DocumentImportValidator {
public:
    DocumentValidationReport validate(MapWebEditor& portal);

private:
    static uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t length);
    static bool validateJsonEnvelope(const String& path, String& reason);
};

}  // namespace mg::p4
