#pragma once
#include <stddef.h>
#include <stdint.h>
#include "Arduino.h"
class Preferences {
public:
    bool begin(const char*, bool = false) { return true; }
    void end() {}
    bool getBool(const char*, bool d=false) { return d; }
    uint8_t getUChar(const char*, uint8_t d=0) { return d; }
    uint32_t getUInt(const char*, uint32_t d=0) { return d; }
    String getString(const char*, const String& d=String()) { return d; }
    String getString(const char*, const char* d) { return String(d); }
    size_t putString(const char*, const String& s) { return s.length() + 1U; }
    size_t putUChar(const char*, uint8_t) { return 1U; }
    size_t putUInt(const char*, uint32_t) { return 4U; }
    size_t putBool(const char*, bool) { return 1U; }
    bool clear() { return true; }
};
