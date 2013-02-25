#pragma once
#ifndef SUFFIXES_OVHW4ZWV
#define SUFFIXES_OVHW4ZWV


namespace elle {
namespace suffix {

namespace {
  template<char... Bits>
  struct __checkbits
  {
    static const bool valid = false;
  };
  
  template<char High, char... Bits>
  struct __checkbits<High, Bits...>
  {
    static const bool valid = (High == '0' || High == '1')
                   && __checkbits<Bits...>::valid;
  };
  
  template<char High>
  struct __checkbits<High>
  {
    static const bool valid = (High == '0' || High == '1');
  };
}

  template<char... Bits>
  constexpr std::bitset<sizeof...(Bits)>
  operator"" _bits() noexcept
  {
    static_assert(__checkbits<Bits...>::valid, "invalid digit in binary string");
    return std::bitset<sizeof...(Bits)>((char const []){Bits..., '\0'});
  }

} /* suffix */
} /* elle */

#endif /* end of include guard: SUFFIXES_OVHW4ZWV */
