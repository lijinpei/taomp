#pragma once

#include "utils.hpp"
#include <cassert>
#include <cstdint>
#include <memory>

/**Stolen from llvm:
 * https://github.com/llvm-mirror/llvm/blob/master/include/llvm/ADT/PointerIntPair.h
 * and libcds:
 * https://github.com/khizmax/libcds/blob/master/cds/details/marked_ptr.h
 */

namespace taomp {

namespace internal {
template <unsigned V> struct ConstantLog2 {
  static constexpr int value = ConstantLog2<(V >> 1)>::value + 1;
};
template <> struct ConstantLog2<1> { static constexpr int value = 0; };
} // namespace internal

template <typename PointerTy, unsigned IntBits, typename IntType = unsigned,
          unsigned AlignBits = internal::ConstantLog2<alignof(
              typename std::pointer_traits<PointerTy>::element_type)>::value>
class PointerIntPair {
  static_assert(AlignBits >= IntBits);
  using RawPointerTrait = std::pointer_traits<PointerTy>;
  uintptr_t value;

  static constexpr unsigned BlankBits = AlignBits - IntBits;
  static constexpr auto AlignMask = Mask<uintptr_t>(AlignBits);
  static constexpr auto BlankMask = Mask<uintptr_t>(BlankBits);
  static constexpr uintptr_t IntMask = AlignMask >> BlankBits;
  static constexpr uintptr_t ShiftedIntMask = AlignMask ^ BlankMask;
  static constexpr uintptr_t PointerMask = ~AlignMask;

  static constexpr bool isConstPointer = std::is_const<PointerTy>::value;
  PointerIntPair(uintptr_t up) : value(up) {}

public:
  using element_type = typename RawPointerTrait::element_type;
  using difference_type = typename RawPointerTrait::difference_type;
  PointerIntPair(PointerTy ptr = nullptr, IntType i = 0) {
    value = uintptr_t(ptr) | (uintptr_t(i) << BlankBits);
  }
  /**rebind is not reasonable
   * template <typename V> using rebind<V>;
   */
  IntType getInt() const { return (value & BlankMask) >> BlankBits; }
  std::enable_if<!isConstPointer> setInt(IntType i) {
    assert(i <= IntType(IntMask));
    value = (value & ~ShiftedIntMask) | (i << BlankBits);
  }
  PointerTy getPointer() const { return value & PointerMask; }
  std::enable_if<!isConstPointer> setPointer(PointerTy ptr) {
    uintptr_t uptr = ptr;
    assert(!(uptr & AlignMask));
    value = uptr | (value & AlignMask);
  }
  typename std::enable_if<!isConstPointer>::type setPointerAndInt(PointerTy ptr,
                                                                  IntType i) {
    value = (value & BlankMask) | uintptr_t(ptr) | (uintptr_t(i) << BlankBits);
  }
  void *getOpaqueValue() const { return reinterpret_cast<void *>(value); }
  PointerIntPair operator&(uintptr_t v) { return PointerIntPair(value & v); }
};

} // namespace taomp

