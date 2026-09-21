#pragma once

#include "Mcp23s17Hal.h"
#include "ScanSource.h"

namespace mg::p4 {

class Mcp23s17ScanSource final : public ScanSource {
public:
    explicit Mcp23s17ScanSource(digitalhw::Mcp23s17Matrix& matrix)
        : matrix_(matrix) {}

    PhysicalScanObservation observe(ScanDirection direction,
                                    uint8_t testIndex) override;
    bool isDemo() const override { return false; }
    bool ready() const { return matrix_.ready(); }

private:
    digitalhw::Mcp23s17Matrix& matrix_;
};

}  // namespace mg::p4
