#include "Mcp23s17PhysicalBackend.h"

#include "V33BoardConfig.h"

namespace mg::p4::v33 {

Mcp23s17PhysicalBackend::Mcp23s17PhysicalBackend()
    : matrix_(transport_), source_(matrix_) {}

bool Mcp23s17PhysicalBackend::begin() {
    // This is the hard safety boundary for Beta 1.0 Fix1. The transport never
    // configures GPIO/SPI until PCB validation AND external pin freeze have
    // both been accepted through V33BoardConfig.h.
    if (!kHardwareEnabled) return false;
    if (!transport_.begin()) return false;
    return matrix_.begin();
}

}  // namespace mg::p4::v33
