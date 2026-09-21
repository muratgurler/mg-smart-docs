#pragma once
#include <stddef.h>
#include <stdint.h>

#define FILE_READ "r"
#define FILE_WRITE "w"

class File {
public:
    explicit operator bool() const { return true; }
    size_t size() const { return 0U; }
    size_t read(uint8_t*, size_t) { return 0U; }
    size_t write(const uint8_t*, size_t size) { return size; }
    size_t print(const char*) { return 0U; }
    size_t print(char) { return 0U; }
    void flush() {}
    void close() {}
};

class LittleFSClass {
public:
    bool begin(bool = false) { return true; }
    bool exists(const char*) const { return false; }
    bool mkdir(const char*) { return true; }
    File open(const char*, const char*) { return File{}; }
    bool remove(const char*) { return true; }
    bool rename(const char*, const char*) { return true; }
};

extern LittleFSClass LittleFS;
