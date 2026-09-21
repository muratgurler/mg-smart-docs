#pragma once

#include "WorkplaceServerConfig.h"

namespace mg::p4 {

class WorkplaceServerConfigStore {
public:
    bool load(WorkplaceServerConfig& config);
    bool save(const WorkplaceServerConfig& config);
    bool clear();
    bool hasSavedConfig() const { return hasSavedConfig_; }
    const char* statusText() const { return statusText_; }

private:
    void setStatus(const char* text);
    bool hasSavedConfig_ = false;
    char statusText_[96] = {};
};

}  // namespace mg::p4
