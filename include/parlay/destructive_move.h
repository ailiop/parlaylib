// Implementation of destructive move.
//
// Based on Proposal N4304 by Pablo Halpern:
//  http://open-std.org/JTC1/SC22/WG21/docs/papers/2014/n4034.pdf
//
//
//
//

#ifndef PARLAY_DESTRUCTIVE_MOVE_H_
#define PARLAY_DESTRUCTIVE_MOVE_H_

#include <cstring>

#include <type_traits>

#include "parallel.h"
#include "slice.h"

namespace parlay {
  
template <typename T>
struct is_trivially_destructive_movable :
  std::bool_constant<std::is_trivially_move_constructible<T>::value &&
                     std::is_trivially_destructible<T>::value> { };

template <typename T> struct is_nothrow_destructive_movable :
  std::bool_constant<is_trivially_destructive_movable<T>::value ||
                    (std::is_nothrow_move_constructible<T>::value &&
                     std::is_nothrow_destructible<T>::value)> { };

template<typename T>
inline constexpr bool is_trivially_destructive_movable_v = is_trivially_destructive_movable<T>::value;

template<typename T>
inline constexpr bool is_nothrow_destructive_movable_v = is_nothrow_destructive_movable<T>::value;

// Preconditions:
//  - from points to a valid initialized object of type T
//  - to points to uninitialized memory large enough to hold an object of type T
//
// Postconditions:
//  - from points to uninitialized memory
//  - to points to a valid initialized object of type T
template<typename T>
void destructive_move(T* to, T* from) noexcept(is_nothrow_destructive_movable<T>::value) {
  if constexpr (is_trivially_destructive_movable<T>::value) {
    std::memcpy(to, from, sizeof(T));
  }
  else {
    static_assert(std::is_move_constructible<T>::value);
    static_assert(std::is_destructible<T>::value);
    ::new (to) T(std::move(*from));
    from->~T();
  }
}

template<typename T>
void destructive_move_array(T* to, T* from, size_t sz) noexcept(is_nothrow_destructive_movable<T>::value) {
  if constexpr (is_trivially_destructive_movable<T>::value) {
    constexpr size_t chunk_size = 1024 * sizeof(size_t) / sizeof(T);
    const size_t n_chunks = (sz + chunk_size - 1) / chunk_size;
    parallel_for(0, n_chunks, [&](size_t i) {
      size_t n_objects = (std::min)(chunk_size, sz - i * chunk_size);
      size_t n_bytes = sizeof(T) * n_objects;
      std::memcpy(to + i * chunk_size, from + i * chunk_size, n_bytes);
    }, 1);
  }
  else {
    parallel_for(0, sz, [&](size_t i) {
      static_assert(std::is_move_constructible<T>::value);
      static_assert(std::is_destructible<T>::value);
      ::new (&to[i]) T(std::move(from[i]));
      from[i].~T();
    });
  }
}

template<typename InIterator, typename OutIterator>
void destructive_move_slice(slice<OutIterator, OutIterator> to, slice<InIterator, InIterator> from)
    noexcept(is_nothrow_destructive_movable<range_value_type_t<slice<InIterator, InIterator>>>::value) {

  using T = range_value_type_t<slice<InIterator, InIterator>>;
  static_assert(std::is_same_v<T, range_value_type_t<slice<OutIterator, OutIterator>>>);

  assert(to.size() >= from.size());

  // If the slice is backed by a pointer range, we can use
  // the faster memcpy version of destructive move!
  if constexpr (std::is_pointer<InIterator>::value && std::is_pointer<OutIterator>::value) {
    destructive_move_array(to.begin(), from.begin(), from.size());
  }
  // Otherwise, fall back to a parallel for loop
  else {
    parallel_for(0, from.size(), [&](size_t i) {
      static_assert(std::is_move_constructible<T>::value);
      static_assert(std::is_destructible<T>::value);
      ::new (&to[i]) T(std::move(from[i]));
      from[i].~T();
    });
  }
}

}  // namespace parlay

#endif  // PARLAY_DESTRUCTIVE_MOVE_H_
