#pragma once
#include "Arduino.h"
#include "NetworkClient.h"
#include "NetworkClientSecure.h"
class HTTPClient {
public:
    void setConnectTimeout(uint32_t) {}
    void setTimeout(uint32_t) {}
    void setUserAgent(const char*) {}
    bool begin(NetworkClient&, const String&) { return true; }
    bool begin(NetworkClientSecure&, const String&) { return true; }
    void addHeader(const String&, const String&) {}
    void addHeader(const char*, const char*) {}
    void addHeader(const char*, const String&) {}
    int GET() { return 200; }
    int getSize() { return 0; }
    String getString() { return String("{}"); }
    void end() {}
    static String errorToString(int) { return String("error"); }
};
