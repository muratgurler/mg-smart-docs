#pragma once

#include <stddef.h>
#include <stdint.h>

#include "Mcp23s17Hal.h"

namespace mg::p4::digitalhw {

// ESP32/Arduino binding for the transport seam. The header intentionally has
// no Arduino/SPI dependency, so the protocol layer remains host-testable and
// projects without the Arduino SPI library can still compile the gated HAL.
class Mcp23s17ArduinoTransport final : public Mcp23s17SpiTransport {
public:
    bool begin();
    bool transferFrame(const uint8_t* tx,
                       uint8_t* rx,
                       size_t length,
                       uint32_t spiHz) override;
    void delayMicroseconds(uint32_t delayUs) override;
    bool started() const { return started_; }

private:
    bool started_ = false;
};

}  // namespace mg::p4::digitalhw
