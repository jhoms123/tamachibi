#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
inline float radians(float x) { return x * 3.14159265358979323846f / 180.0f; }
#include <algorithm>
#include <string>
using std::max;
using std::min;
using byte = uint8_t;
using String = std::string;
class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(s))
#define PROGMEM
using PGM_P = const char *;
#define strncpy_P strncpy
#define pgm_read_byte(p) (*reinterpret_cast<const uint8_t *>(p))
#define pgm_read_word(p) (*reinterpret_cast<const uint16_t *>(p))
#define pgm_read_dword(p) (*reinterpret_cast<const uintptr_t *>(p))
extern uint32_t fixtureMillis;
inline uint32_t millis() { return fixtureMillis; }
inline void delay(uint32_t) {}
