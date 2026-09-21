#include "Mcp23s17ArduinoTransport.h"

#include "V33BoardConfig.h"

#if __has_include(<Arduino.h>) && __has_include(<SPI.h>)
#include <Arduino.h>
#include <SPI.h>
#define MG_MCP23S17_ARDUINO_SPI_AVAILABLE 1
#else
#define MG_MCP23S17_ARDUINO_SPI_AVAILABLE 0
#endif

namespace mg::p4::digitalhw {

bool Mcp23s17ArduinoTransport::begin() {
    if (!v33::kHardwareEnabled) {
        started_ = false;
        return false;
    }

#if MG_MCP23S17_ARDUINO_SPI_AVAILABLE
    pinMode(v33::kGpioMcpDigCsN, OUTPUT);
    digitalWrite(v33::kGpioMcpDigCsN, HIGH);
    SPI.begin(v33::kGpioSpiSclk,
              v33::kGpioSpiMiso,
              v33::kGpioSpiMosi,
              -1);
    started_ = true;
    return true;
#else
    // Fail closed. Fix1 never falls back to a guessed SPI implementation.
    started_ = false;
    return false;
#endif
}

bool Mcp23s17ArduinoTransport::transferFrame(const uint8_t* tx,
                                             uint8_t* rx,
                                             size_t length,
                                             uint32_t spiHz) {
#if MG_MCP23S17_ARDUINO_SPI_AVAILABLE
    if (!started_ || tx == nullptr || length == 0U) return false;

    SPI.beginTransaction(SPISettings(spiHz, MSBFIRST, SPI_MODE0));
    digitalWrite(v33::kGpioMcpDigCsN, LOW);
    for (size_t i = 0U; i < length; ++i) {
        const uint8_t value = SPI.transfer(tx[i]);
        if (rx != nullptr) rx[i] = value;
    }
    digitalWrite(v33::kGpioMcpDigCsN, HIGH);
    SPI.endTransaction();
    return true;
#else
    (void)tx;
    (void)rx;
    (void)length;
    (void)spiHz;
    return false;
#endif
}

void Mcp23s17ArduinoTransport::delayMicroseconds(uint32_t delayUs) {
#if MG_MCP23S17_ARDUINO_SPI_AVAILABLE
    ::delayMicroseconds(delayUs);
#else
    (void)delayUs;
#endif
}

}  // namespace mg::p4::digitalhw
