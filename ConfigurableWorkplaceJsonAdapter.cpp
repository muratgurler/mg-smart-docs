#include "ConfigurableWorkplaceJsonAdapter.h"

#include <stdio.h>

namespace mg::p4 {
namespace {
void setAdapterError(char* error, size_t errorSize, const char* text) {
    if (error == nullptr || errorSize == 0U) return;
    snprintf(error, errorSize, "%s", text != nullptr ? text : "response mapping failed");
}
}

const char* ConfigurableWorkplaceJsonAdapter::canonicalKeyFor(const String& member) const {
    if (member == fieldMap_.productionPr) return "productionPr";
    if (member == fieldMap_.customerReference) return "customerReference";
    if (member == fieldMap_.revision) return "revision";
    if (member == fieldMap_.profileId) return "profileId";
    if (member == fieldMap_.signalPinCount) return "signalPinCount";
    if (member == fieldMap_.includePeA) return "includePeA";
    if (member == fieldMap_.includePeB) return "includePeB";
    if (member == fieldMap_.includeDrainShieldA) return "includeDrainShieldA";
    if (member == fieldMap_.includeDrainShieldB) return "includeDrainShieldB";
    if (member == fieldMap_.connectorA) return "connectorA";
    if (member == fieldMap_.connectorB) return "connectorB";
    if (member == fieldMap_.nets) return "nets";
    if (member == fieldMap_.netA) return "A";
    if (member == fieldMap_.netB) return "B";
    if (member == fieldMap_.connectorKind) return "kind";
    if (member == fieldMap_.connectorGender) return "gender";
    if (member == fieldMap_.connectorContacts) return "contacts";
    if (member == fieldMap_.connectorName) return "name";
    return nullptr;
}

bool ConfigurableWorkplaceJsonAdapter::normalizeMappedJson(
    const char* body, size_t bodyLength, String& normalized,
    char* error, size_t errorSize) const {
    normalized = String();
    if (body == nullptr || bodyLength == 0U) {
        setAdapterError(error, errorSize, "empty mapped response");
        return false;
    }
    normalized.reserve(bodyLength + 256U);
    size_t i = 0U;
    while (i < bodyLength) {
        if (body[i] != '"') {
            normalized += body[i++];
            continue;
        }

        const size_t quoteStart = i;
        ++i;
        const size_t textStart = i;
        bool escaped = false;
        while (i < bodyLength) {
            if (body[i] == '\\') {
                escaped = true;
                i += 2U;
                continue;
            }
            if (body[i] == '"') break;
            ++i;
        }
        if (i >= bodyLength) {
            setAdapterError(error, errorSize, "unterminated JSON string");
            return false;
        }
        const size_t quoteEnd = i;
        size_t after = quoteEnd + 1U;
        while (after < bodyLength && (body[after] == ' ' || body[after] == '\t' ||
                                      body[after] == '\r' || body[after] == '\n')) ++after;

        const bool isMemberName = after < bodyLength && body[after] == ':';
        if (isMemberName && !escaped) {
            String member;
            member.reserve(quoteEnd - textStart);
            for (size_t n = textStart; n < quoteEnd; ++n) member += body[n];
            const char* canonical = canonicalKeyFor(member);
            if (canonical != nullptr) {
                normalized += '"';
                normalized += canonical;
                normalized += '"';
            } else {
                for (size_t n = quoteStart; n <= quoteEnd; ++n) normalized += body[n];
            }
        } else {
            for (size_t n = quoteStart; n <= quoteEnd; ++n) normalized += body[n];
        }
        i = quoteEnd + 1U;
    }
    return true;
}

bool ConfigurableWorkplaceJsonAdapter::parse(const char* body,
                                              size_t bodyLength,
                                              ProductionProfileRecord& record,
                                              char* error,
                                              size_t errorSize) const {
    if (format_ == WorkplaceResponseFormat::CanonicalJsonV1) {
        return canonical_.parse(body, bodyLength, record, error, errorSize);
    }
    if (format_ != WorkplaceResponseFormat::MappedJsonV1) {
        setAdapterError(error, errorSize, "unsupported workplace response format");
        return false;
    }
    String normalized;
    if (!normalizeMappedJson(body, bodyLength, normalized, error, errorSize)) return false;
    return canonical_.parse(normalized.c_str(), normalized.length(), record, error, errorSize);
}

}  // namespace mg::p4
