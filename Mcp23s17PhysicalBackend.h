#pragma once

#include "Mcp23s17ArduinoTransport.h"
#include "Mcp23s17Hal.h"
#include "Mcp23s17ScanSource.h"

namespace mg::p4::v33 {

class Mcp23s17PhysicalBackend {
public:
    Mcp23s17PhysicalBackend();

    bool begin();
    bool ready() const { return source_.ready(); }
    ScanSource* scanSource() { return ready() ? &source_ : nullptr; }
    const digitalhw::Mcp23s17MatrixStatus& status() const {
        return matrix_.status();
    }

private:
    digitalhw::Mcp23s17ArduinoTransport transport_{};
    digitalhw::Mcp23s17Matrix matrix_;
    Mcp23s17ScanSource source_;
};

}  // namespace mg::p4::v33
