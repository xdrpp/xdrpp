#pragma once

#include <cstddef>
#include <stdexcept>

struct throwing_arm_state {
  inline static bool throw_now = false;
};

template<std::size_t Size>
struct throwing_arm : throwing_arm_state {
  char data[Size]{};

  throwing_arm() {
    if (throw_now)
      throw std::runtime_error("throwing_arm");
  }
  throwing_arm(const throwing_arm &) {
    if (throw_now)
      throw std::runtime_error("throwing_arm");
  }
  throwing_arm(throwing_arm &&) {
    if (throw_now)
      throw std::runtime_error("throwing_arm");
  }
  throwing_arm &operator=(const throwing_arm &) = default;
  throwing_arm &operator=(throwing_arm &&) = default;
};

using throwing_inline_arm = throwing_arm<4>;
using throwing_indirect_arm = throwing_arm<16>;