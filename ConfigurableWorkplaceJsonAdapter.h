#pragma once

#include "WorkplaceProfileJsonAdapter.h"
#include "WorkplaceServerConfig.h"

namespace mg::p4 {

class ConfigurableWorkplaceJsonAdapter final : public WorkplaceProfileResponseAdapter {
public:
    void configure(WorkplaceResponseFormat format, const WorkplaceJsonFieldMap& fieldMap) {
        format_ = format;
        fieldMap_ = fieldMap;
    }

    WorkplaceResponseFormat format() const { return format_; }
    const WorkplaceJsonFieldMap& fieldMap() const { return fieldMap_; }

    bool parse(const char* body,
               size_t bodyLength,
               ProductionProfileRecord& record,
               char* error,
               size_t errorSize) const override;

private:
    const char* canonicalKeyFor(const String& member) const;
    bool normalizeMappedJson(const char* body, size_t bodyLength, String& normalized,
                             char* error, size_t errorSize) const;

    WorkplaceResponseFormat format_ = WorkplaceResponseFormat::CanonicalJsonV1;
    WorkplaceJsonFieldMap fieldMap_{};
    CanonicalWorkplaceJsonAdapter canonical_{};
};

}  // namespace mg::p4
