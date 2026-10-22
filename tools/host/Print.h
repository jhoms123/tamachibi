#pragma once
#include "Arduino.h"
class Print {
public:
  virtual ~Print() {};
  virtual size_t write(uint8_t) = 0;
  size_t write(const uint8_t *s, size_t n) {
    size_t out = 0;
    while (n--)
      out += write(*s++);
    return out;
  }
  size_t print(char *s) { return print(static_cast<const char *>(s)); }
  size_t print(const char *s) {
    return s ? write(reinterpret_cast<const uint8_t *>(s), strlen(s)) : 0;
  }
  size_t print(const __FlashStringHelper *s) {
    return print(reinterpret_cast<const char *>(s));
  }
  size_t print(const String &s) { return print(s.c_str()); }
  size_t print(char c) { return write(static_cast<uint8_t>(c)); }
  template <typename T> size_t print(T value) {
    return print(std::to_string(value));
  }
  size_t println() { return print('\n'); }
  template <typename T> size_t println(T s) { return print(s) + println(); }
};
