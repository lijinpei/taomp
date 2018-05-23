#pragma once

#include <atomic>
#include <cassert>
#include <climits>
#include <new>
#include <stdlib.h>
#include <type_traits>

// when a input argument is marked as TAKE, you don't have ownership of the
// argument after calling the function
#define TAKE(x) x
// when a input argument is marker as BORROW, calling the function has no effect
// on the ownership of that argument
#define BORROW(x) x
// when a return value is marked as GIVE, you are given the ownership of the
// return value after calling the function
#define GIVE(x) x

namespace std {
inline constexpr std::size_t hardware_destructive_interference_size = 64;
inline constexpr std::size_t hardware_constructive_interference_size = 64;
} // namespace std

namespace taomp {

namespace internal {
template <typename T, std::size_t N> struct MaskLeadingZero_impl {
  static T calc(T n) {
    return MaskLeadingZero_impl<T, N - 1>::calc(n | (n >> 1));
  }
};

template <typename T> struct MaskLeadingZero_impl<T, 1> {
  static T calc(T n) { return n; }
};
} // namespace internal

/**Let n be of unsigned type T, let v be the smallest number, which is a power
 * of 2 and not smaller than n, this template return v - 1
 */
template <typename T>
typename std::enable_if<std::is_unsigned<T>::value, T>::type
MaskLeadingZero(T n) {
  return internal::MaskLeadingZero_impl<T, sizeof(T) * CHAR_BIT>::calc(n);
}

template <typename T> T Mask(unsigned bits) { return (T(1) << bits) - T(1); }

template <typename T,
          std::size_t align = std::hardware_destructive_interference_size>
T *aligned_alloc(std::size_t num = 1) {
  auto *ret = reinterpret_cast<T *>(::aligned_alloc(align, sizeof(T) * num));
  assert(ret);
  return ret;
}

} // namespace taomp
