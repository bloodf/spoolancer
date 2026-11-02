#pragma once
#include <cmath>

namespace JsonNumbers {
// Keep the field proxy so assigning a value creates a previously absent JSON key.
template<class Field>
void numberOrNull(Field field, double value) {
  if (std::isfinite(value)) field.set(value);
  else field.set(nullptr);
}
}
