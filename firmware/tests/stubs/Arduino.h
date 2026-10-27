#pragma once
#include <cstddef>
#include <cstdint>

constexpr int INPUT = 0, OUTPUT = 1, INPUT_PULLUP = 2, LOW = 0, HIGH = 1;
using portMUX_TYPE = int;
constexpr int portMUX_INITIALIZER_UNLOCKED = 0;
inline void portENTER_CRITICAL(portMUX_TYPE *) {}
inline void portEXIT_CRITICAL(portMUX_TYPE *) {}
inline void pinMode(int, int) {}
inline void delayMicroseconds(unsigned) {}
uint32_t millis();
int digitalRead(int);
void digitalWrite(int, int);
