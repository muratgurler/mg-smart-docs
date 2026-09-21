#include "WorkplaceProfileJsonAdapter.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

namespace mg::p4 {
namespace {

void setError(char* error, size_t errorSize, const char* text) {
    if (error == nullptr || errorSize == 0U) return;
    snprintf(error, errorSize, "%s", text != nullptr ? text : "JSON parse error");
}

bool iequals(const char* a, const char* b) {
    if (a == nullptr || b == nullptr) return false;
    while (*a && *b) {
        if (toupper(static_cast<unsigned char>(*a)) !=
            toupper(static_cast<unsigned char>(*b))) return false;
        ++a; ++b;
    }
    return *a == '\0' && *b == '\0';
}

class JsonCursor {
public:
    JsonCursor(const char* begin, size_t length) : p_(begin), end_(begin + length) {}

    void ws() { while (p_ < end_ && isspace(static_cast<unsigned char>(*p_))) ++p_; }
    bool eof() { ws(); return p_ >= end_; }

    bool take(char c) {
        ws();
        if (p_ >= end_ || *p_ != c) return false;
        ++p_;
        return true;
    }

    bool string(char* out, size_t outSize) {
        ws();
        if (p_ >= end_ || *p_ != '"' || out == nullptr || outSize == 0U) return false;
        ++p_;
        size_t n = 0U;
        while (p_ < end_) {
            char c = *p_++;
            if (c == '"') {
                out[n] = '\0';
                return true;
            }
            if (c == '\\') {
                if (p_ >= end_) return false;
                const char esc = *p_++;
                switch (esc) {
                    case '"': c = '"'; break;
                    case '\\': c = '\\'; break;
                    case '/': c = '/'; break;
                    case 'b': c = '\b'; break;
                    case 'f': c = '\f'; break;
                    case 'n': c = '\n'; break;
                    case 'r': c = '\r'; break;
                    case 't': c = '\t'; break;
                    default: return false; // \u is intentionally not accepted for identifiers.
                }
            }
            if (n + 1U >= outSize) return false;
            out[n++] = c;
        }
        return false;
    }

    bool boolean(bool& value) {
        ws();
        if (remainingStarts("true")) { p_ += 4; value = true; return true; }
        if (remainingStarts("false")) { p_ += 5; value = false; return true; }
        return false;
    }

    bool uintValue(uint32_t& value) {
        ws();
        if (p_ >= end_ || !isdigit(static_cast<unsigned char>(*p_))) return false;
        uint32_t v = 0U;
        while (p_ < end_ && isdigit(static_cast<unsigned char>(*p_))) {
            const uint32_t digit = static_cast<uint32_t>(*p_ - '0');
            if (v > 429496729U || (v == 429496729U && digit > 5U)) return false;
            v = v * 10U + digit;
            ++p_;
        }
        value = v;
        return true;
    }

    bool skipValue() {
        ws();
        if (p_ >= end_) return false;
        if (*p_ == '"') {
            char sink[2]{};
            // Stream-skip a string without imposing the normal output limit.
            ++p_;
            while (p_ < end_) {
                char c = *p_++;
                if (c == '"') return true;
                if (c == '\\') {
                    if (p_ >= end_) return false;
                    ++p_;
                }
            }
            (void)sink;
            return false;
        }
        if (*p_ == '{') {
            ++p_;
            ws();
            if (p_ < end_ && *p_ == '}') { ++p_; return true; }
            for (;;) {
                char key[64]{};
                if (!string(key, sizeof(key)) || !take(':') || !skipValue()) return false;
                ws();
                if (p_ < end_ && *p_ == '}') { ++p_; return true; }
                if (!take(',')) return false;
            }
        }
        if (*p_ == '[') {
            ++p_;
            ws();
            if (p_ < end_ && *p_ == ']') { ++p_; return true; }
            for (;;) {
                if (!skipValue()) return false;
                ws();
                if (p_ < end_ && *p_ == ']') { ++p_; return true; }
                if (!take(',')) return false;
            }
        }
        if (remainingStarts("true")) { p_ += 4; return true; }
        if (remainingStarts("false")) { p_ += 5; return true; }
        if (remainingStarts("null")) { p_ += 4; return true; }
        if (*p_ == '-' || isdigit(static_cast<unsigned char>(*p_))) {
            if (*p_ == '-') ++p_;
            bool digit = false;
            while (p_ < end_ && isdigit(static_cast<unsigned char>(*p_))) { ++p_; digit = true; }
            if (!digit) return false;
            if (p_ < end_ && *p_ == '.') {
                ++p_;
                bool frac = false;
                while (p_ < end_ && isdigit(static_cast<unsigned char>(*p_))) { ++p_; frac = true; }
                if (!frac) return false;
            }
            if (p_ < end_ && (*p_ == 'e' || *p_ == 'E')) {
                ++p_;
                if (p_ < end_ && (*p_ == '+' || *p_ == '-')) ++p_;
                bool exp = false;
                while (p_ < end_ && isdigit(static_cast<unsigned char>(*p_))) { ++p_; exp = true; }
                if (!exp) return false;
            }
            return true;
        }
        return false;
    }

private:
    bool remainingStarts(const char* text) const {
        const size_t n = strlen(text);
        return static_cast<size_t>(end_ - p_) >= n && strncmp(p_, text, n) == 0;
    }

    const char* p_;
    const char* end_;
};

ConnectorKind parseKind(const char* text) {
    if (iequals(text, "SUBD") || iequals(text, "D-SUB") || iequals(text, "DSUB")) return ConnectorKind::DSub;
    if (iequals(text, "HARTING")) return ConnectorKind::Harting;
    if (iequals(text, "LEMO")) return ConnectorKind::Lemo;
    return ConnectorKind::Custom;
}

ConnectorGender parseGender(const char* text) {
    if (iequals(text, "MALE") || iequals(text, "M")) return ConnectorGender::Male;
    if (iequals(text, "FEMALE") || iequals(text, "F")) return ConnectorGender::Female;
    return ConnectorGender::Neutral;
}

bool parseConnector(JsonCursor& j,
                    ConnectorKind& kind,
                    ConnectorGender& gender,
                    uint8_t& contacts,
                    char* name,
                    size_t nameSize) {
    if (!j.take('{')) return false;
    if (j.take('}')) return true;
    for (;;) {
        char key[32]{};
        if (!j.string(key, sizeof(key)) || !j.take(':')) return false;
        if (strcmp(key, "kind") == 0) {
            char text[24]{};
            if (!j.string(text, sizeof(text))) return false;
            kind = parseKind(text);
        } else if (strcmp(key, "gender") == 0) {
            char text[20]{};
            if (!j.string(text, sizeof(text))) return false;
            gender = parseGender(text);
        } else if (strcmp(key, "contacts") == 0) {
            uint32_t v = 0U;
            if (!j.uintValue(v) || v > 64U) return false;
            contacts = static_cast<uint8_t>(v);
        } else if (strcmp(key, "name") == 0) {
            if (!j.string(name, nameSize)) return false;
        } else if (!j.skipValue()) {
            return false;
        }
        if (j.take('}')) return true;
        if (!j.take(',')) return false;
    }
}

bool parsePointArray(JsonCursor& j, uint64_t& mask) {
    mask = 0ULL;
    if (!j.take('[')) return false;
    if (j.take(']')) return true;
    for (;;) {
        uint32_t point = 0U;
        if (!j.uintValue(point) || point >= kTestPointsPerSide) return false;
        const uint64_t bit = 1ULL << static_cast<uint8_t>(point);
        if ((mask & bit) != 0ULL) return false;
        mask |= bit;
        if (j.take(']')) return true;
        if (!j.take(',')) return false;
    }
}

bool parseNet(JsonCursor& j, uint64_t& aMask, uint64_t& bMask) {
    aMask = 0ULL;
    bMask = 0ULL;
    bool haveA = false;
    bool haveB = false;
    if (!j.take('{')) return false;
    if (j.take('}')) return false;
    for (;;) {
        char key[16]{};
        if (!j.string(key, sizeof(key)) || !j.take(':')) return false;
        if (strcmp(key, "A") == 0 || strcmp(key, "a") == 0) {
            if (!parsePointArray(j, aMask)) return false;
            haveA = true;
        } else if (strcmp(key, "B") == 0 || strcmp(key, "b") == 0) {
            if (!parsePointArray(j, bMask)) return false;
            haveB = true;
        } else if (!j.skipValue()) {
            return false;
        }
        if (j.take('}')) break;
        if (!j.take(',')) return false;
    }
    return (haveA || haveB) && (aMask != 0ULL || bMask != 0ULL);
}

bool applyNet(ProductionProfileRecord& record,
              uint64_t aMask,
              uint64_t bMask,
              uint64_t& assignedA,
              uint64_t& assignedB) {
    // Each physical point belongs to one connected-component only. Overlap
    // means the server response is ambiguous and must not silently merge nets.
    if ((assignedA & aMask) != 0ULL || (assignedB & bMask) != 0ULL) return false;
    assignedA |= aMask;
    assignedB |= bMask;

    for (uint8_t a = 0U; a < kTestPointsPerSide; ++a) {
        const uint64_t aBit = 1ULL << a;
        if ((aMask & aBit) == 0ULL) continue;
        record.aToB[a] |= bMask;
        record.sameSideA[a] |= (aMask & ~aBit);
    }
    for (uint8_t b = 0U; b < kTestPointsPerSide; ++b) {
        const uint64_t bBit = 1ULL << b;
        if ((bMask & bBit) == 0ULL) continue;
        record.sameSideB[b] |= (bMask & ~bBit);
    }
    return true;
}

bool parseNets(JsonCursor& j, ProductionProfileRecord& record) {
    uint64_t assignedA = 0ULL;
    uint64_t assignedB = 0ULL;
    if (!j.take('[')) return false;
    if (j.take(']')) return true;
    for (;;) {
        uint64_t aMask = 0ULL;
        uint64_t bMask = 0ULL;
        if (!parseNet(j, aMask, bMask) || !applyNet(record, aMask, bMask, assignedA, assignedB)) return false;
        if (j.take(']')) return true;
        if (!j.take(',')) return false;
    }
}

}  // namespace

bool CanonicalWorkplaceJsonAdapter::parse(const char* body,
                                          size_t bodyLength,
                                          ProductionProfileRecord& record,
                                          char* error,
                                          size_t errorSize) const {
    record = {};
    if (error != nullptr && errorSize > 0U) error[0] = '\0';
    if (body == nullptr || bodyLength == 0U) {
        setError(error, errorSize, "empty server response");
        return false;
    }

    snprintf(record.connectorAName, sizeof(record.connectorAName), "X1");
    snprintf(record.connectorBName, sizeof(record.connectorBName), "X2");

    JsonCursor j(body, bodyLength);
    if (!j.take('{')) {
        setError(error, errorSize, "response is not a JSON object");
        return false;
    }
    if (j.take('}')) {
        setError(error, errorSize, "empty JSON object");
        return false;
    }

    bool haveNets = false;
    for (;;) {
        char key[48]{};
        if (!j.string(key, sizeof(key)) || !j.take(':')) {
            setError(error, errorSize, "invalid JSON member");
            return false;
        }

        if (strcmp(key, "productionPr") == 0) {
            if (!j.string(record.productionPr, sizeof(record.productionPr))) goto bad_value;
        } else if (strcmp(key, "customerReference") == 0) {
            if (!j.string(record.customerReference, sizeof(record.customerReference))) goto bad_value;
        } else if (strcmp(key, "revision") == 0) {
            if (!j.string(record.revision, sizeof(record.revision))) goto bad_value;
        } else if (strcmp(key, "profileId") == 0) {
            if (!j.string(record.profileId, sizeof(record.profileId))) goto bad_value;
        } else if (strcmp(key, "signalPinCount") == 0) {
            uint32_t v = 0U;
            if (!j.uintValue(v) || v > kMaximumSignalPins) goto bad_value;
            record.signalPinCount = static_cast<uint8_t>(v);
        } else if (strcmp(key, "includePeA") == 0) {
            if (!j.boolean(record.includePeA)) goto bad_value;
        } else if (strcmp(key, "includePeB") == 0) {
            if (!j.boolean(record.includePeB)) goto bad_value;
        } else if (strcmp(key, "includeDrainShieldA") == 0) {
            if (!j.boolean(record.includeDrainShieldA)) goto bad_value;
        } else if (strcmp(key, "includeDrainShieldB") == 0) {
            if (!j.boolean(record.includeDrainShieldB)) goto bad_value;
        } else if (strcmp(key, "connectorA") == 0) {
            if (!parseConnector(j, record.connectorKindA, record.connectorGenderA,
                                record.connectorContactsA, record.connectorAName,
                                sizeof(record.connectorAName))) goto bad_value;
        } else if (strcmp(key, "connectorB") == 0) {
            if (!parseConnector(j, record.connectorKindB, record.connectorGenderB,
                                record.connectorContactsB, record.connectorBName,
                                sizeof(record.connectorBName))) goto bad_value;
        } else if (strcmp(key, "nets") == 0) {
            if (!parseNets(j, record)) goto bad_value;
            haveNets = true;
        } else if (!j.skipValue()) {
            goto bad_value;
        }

        if (j.take('}')) break;
        if (!j.take(',')) {
            setError(error, errorSize, "expected ',' or '}'");
            return false;
        }
    }

    if (!j.eof()) {
        setError(error, errorSize, "trailing data after JSON object");
        return false;
    }
    if (!haveNets) {
        setError(error, errorSize, "missing nets array");
        return false;
    }
    return true;

bad_value:
    setError(error, errorSize, "invalid value in canonical profile JSON");
    return false;
}

}  // namespace mg::p4
