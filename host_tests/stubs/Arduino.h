#pragma once
#include <stdint.h>
#include <stdarg.h>
#include <stddef.h>
#include <string>
#include <algorithm>
#include <cctype>
#include <cstdlib>

class String {
public:
    String() = default;
    String(const char* s) : s_(s ? s : "") {}
    String(const std::string& s) : s_(s) {}
    String(char c) : s_(1, c) {}
    String(unsigned int v) : s_(std::to_string(v)) {}
    String(unsigned long v) : s_(std::to_string(v)) {}
    String(int v) : s_(std::to_string(v)) {}
    String(long v) : s_(std::to_string(v)) {}

    size_t length() const { return s_.size(); }
    bool isEmpty() const { return s_.empty(); }
    const char* c_str() const { return s_.c_str(); }
    void reserve(size_t n) { s_.reserve(n); }
    char operator[](size_t i) const { return s_[i]; }
    char& operator[](size_t i) { return s_[i]; }

    int indexOf(char c, unsigned int from = 0U) const {
        auto p = s_.find(c, from); return p == std::string::npos ? -1 : static_cast<int>(p);
    }
    int indexOf(const char* text, unsigned int from = 0U) const {
        auto p = s_.find(text ? text : "", from); return p == std::string::npos ? -1 : static_cast<int>(p);
    }
    int indexOf(const String& text, unsigned int from = 0U) const {
        auto p = s_.find(text.s_, from); return p == std::string::npos ? -1 : static_cast<int>(p);
    }
    bool startsWith(const char* prefix) const {
        if (!prefix) return false;
        std::string p(prefix);
        return s_.rfind(p, 0) == 0;
    }
    bool startsWith(const String& prefix) const { return s_.rfind(prefix.s_, 0) == 0; }
    String substring(unsigned int from) const {
        return from <= s_.size() ? String(s_.substr(from)) : String();
    }
    String substring(unsigned int from, unsigned int to) const {
        if (from > s_.size() || to < from) return String();
        return String(s_.substr(from, std::min<size_t>(to, s_.size()) - from));
    }
    void trim() {
        auto first = std::find_if_not(s_.begin(), s_.end(), [](unsigned char c){ return std::isspace(c); });
        auto last = std::find_if_not(s_.rbegin(), s_.rend(), [](unsigned char c){ return std::isspace(c); }).base();
        if (first >= last) s_.clear(); else s_ = std::string(first, last);
    }
    void replace(const String& find, const String& repl) {
        if (find.s_.empty()) return;
        size_t pos = 0;
        while ((pos = s_.find(find.s_, pos)) != std::string::npos) {
            s_.replace(pos, find.s_.size(), repl.s_);
            pos += repl.s_.size();
        }
    }
    void replace(const char* find, const String& repl) { replace(String(find), repl); }

    String& operator+=(const String& o) { s_ += o.s_; return *this; }
    String& operator+=(const char* o) { if (o) s_ += o; return *this; }
    String& operator+=(char c) { s_ += c; return *this; }
    String& operator+=(unsigned int v) { s_ += std::to_string(v); return *this; }
    String& operator+=(unsigned long v) { s_ += std::to_string(v); return *this; }
    String& operator+=(int v) { s_ += std::to_string(v); return *this; }
    String& operator+=(long v) { s_ += std::to_string(v); return *this; }

    friend String operator+(String a, const String& b) { a += b; return a; }
    friend String operator+(String a, const char* b) { a += b; return a; }
    friend String operator+(const char* a, const String& b) { String r(a); r += b; return r; }
    friend String operator+(String a, unsigned int v) { a += v; return a; }
    friend String operator+(String a, unsigned long v) { a += v; return a; }
    friend bool operator==(const String& a, const String& b) { return a.s_ == b.s_; }
    friend bool operator==(const String& a, const char* b) { return a.s_ == (b ? b : ""); }
    friend bool operator==(const char* a, const String& b) { return b == a; }
    friend bool operator!=(const String& a, const String& b) { return !(a == b); }
    friend bool operator!=(const String& a, const char* b) { return !(a == b); }

private:
    std::string s_;
};

class FakeSerialClass {
public:
    void println(const char*) {}
    void println() {}
    int printf(const char*, ...) { return 0; }
    void flush() {}
};

extern FakeSerialClass Serial;
uint32_t millis();
inline void delay(uint32_t) {}
