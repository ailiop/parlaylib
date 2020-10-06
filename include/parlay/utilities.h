
#ifndef PARLAY_UTILITIES_H_
#define PARLAY_UTILITIES_H_

#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>

#include <atomic>
#include <new>
#include <type_traits>
#include <utility>

#include "destructive_move.h"
#include "parallel.h"

#ifdef PARLAY_DEBUG_UNINITIALIZED
#include "internal/debug_uninitialized.h"
#endif  // PARLAY_DEBUG_UNINITIALIZED

namespace parlay {

template <typename Lf, typename Rf>
static void par_do_if(bool do_parallel, Lf left, Rf right, bool cons = false) {
  if (do_parallel)
    par_do(left, right, cons);
  else {
    left();
    right();
  }
}

template <typename Lf, typename Mf, typename Rf>
inline void par_do3(Lf left, Mf mid, Rf right) {
  auto left_mid = [&]() { par_do(left, mid); };
  par_do(left_mid, right);
}

template <typename Lf, typename Mf, typename Rf>
static void par_do3_if(bool do_parallel, Lf left, Mf mid, Rf right) {
  if (do_parallel)
    par_do3(left, mid, right);
  else {
    left();
    mid();
    right();
  }
}

template <class T>
size_t log2_up(T);

struct empty {};

typedef uint32_t flags;
const flags no_flag = 0;
const flags fl_sequential = 1;
const flags fl_debug = 2;
const flags fl_time = 4;
const flags fl_conservative = 8;
const flags fl_inplace = 16;

template <typename T>
inline void assign_uninitialized(T& a, const T& b) {
#ifdef PARLAY_DEBUG_UNINITIALIZED
  if constexpr (std::is_same_v<T, debug_uninitialized>) {
    // Ensure that a is uninitialized
    assert(a.x == -1 && "Assigning to initialized memory with assign_uninitialized");
  }
#endif  // PARLAY_DEBUG_UNINITIALIZED
  new (static_cast<T*>(std::addressof(a))) T(b);
}

template <typename T>
inline auto assign_uninitialized(T& a, T&& b) -> typename std::enable_if_t<std::is_rvalue_reference_v<T&&>> {
#ifdef PARLAY_DEBUG_UNINITIALIZED
  if constexpr (std::is_same_v<T, debug_uninitialized>) {
    // Ensure that a is uninitialized
    assert(a.x == -1 && "Assigning to initialized memory with assign_uninitialized");
  }
#endif  // PARLAY_DEBUG_UNINITIALIZED
  new (static_cast<T*>(std::addressof(a))) T(std::move(b));  // NOLINT: b is guaranteed to be an rvalue reference
}

template <typename T>
inline void move_uninitialized(T& a, T& b) {
#ifdef PARLAY_DEBUG_UNINITIALIZED
  if constexpr (std::is_same_v<T, debug_uninitialized>) {
    // Ensure that a is uninitialized
    assert(a.x == -1 && "Assigning to initialized memory with move_uninitialized");
  }
#endif  // PARLAY_DEBUG_UNINITIALIZED
  new (static_cast<T*>(std::addressof(a))) T(std::move(b));
}


// a 32-bit hash function
inline uint32_t hash32(uint32_t a) {
  a = (a + 0x7ed55d16) + (a << 12);
  a = (a ^ 0xc761c23c) ^ (a >> 19);
  a = (a + 0x165667b1) + (a << 5);
  a = (a + 0xd3a2646c) ^ (a << 9);
  a = (a + 0xfd7046c5) + (a << 3);
  a = (a ^ 0xb55a4f09) ^ (a >> 16);
  return a;
}

inline uint32_t hash32_2(uint32_t a) {
  uint32_t z = (a + 0x6D2B79F5UL);
  z = (z ^ (z >> 15)) * (z | 1UL);
  z ^= z + (z ^ (z >> 7)) * (z | 61UL);
  return z ^ (z >> 14);
}

inline uint32_t hash32_3(uint32_t a) {
  uint32_t z = a + 0x9e3779b9;
  z ^= z >> 15;  // 16 for murmur3
  z *= 0x85ebca6b;
  z ^= z >> 13;
  z *= 0xc2b2ae3d;  // 0xc2b2ae35 for murmur3
  return z ^ (z >> 16);
}

// from numerical recipes
inline uint64_t hash64(uint64_t u) {
  uint64_t v = u * 3935559000370003845ul + 2691343689449507681ul;
  v ^= v >> 21;
  v ^= v << 37;
  v ^= v >> 4;
  v *= 4768777513237032717ul;
  v ^= v << 20;
  v ^= v >> 41;
  v ^= v << 5;
  return v;
}

// a slightly cheaper, but possibly not as good version
// based on splitmix64
inline uint64_t hash64_2(uint64_t x) {
  x = (x ^ (x >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
  x = (x ^ (x >> 27)) * UINT64_C(0x94d049bb133111eb);
  x = x ^ (x >> 31);
  return x;
}


template <typename T, typename EV>
inline void write_add(std::atomic<T>* a, EV b) {
  T newV, oldV;
  do {
    oldV = a->load();
    newV = oldV + b;
  } while (!std::atomic_compare_exchange_weak(a, &oldV, newV));
}

template <typename T, typename F>
inline bool write_min(std::atomic<T>* a, T b, F less) {
  T c;
  bool r = 0;
  do {
    c = a->load();
  } while (less(b, c) && !(r = std::atomic_compare_exchange_weak(a, &c, b)));
  return r;
}

template <typename T, typename F>
inline bool write_max(std::atomic<T>* a, T b, F less) {
  T c;
  bool r = 0;
  do {
    c = a->load();
  } while (less(c, b) && !(r = std::atomic_compare_exchange_weak(a, &c, b)));
  return r;
}

// returns the log base 2 rounded up (works on ints or longs or unsigned
// versions)
template <class T>
size_t log2_up(T i) {
  assert(i > 0);
  size_t a = 0;
  T b = i - 1;
  while (b > 0) {
    b = b >> 1;
    a++;
  }
  return a;
}

inline size_t granularity(size_t n) {
  return (n > 100) ? static_cast<size_t>(ceil(pow(n, 0.5))) : 100;
}

/* For inplace sorting / merging, we sometimes need to move values
   around and sometimes we want to make copies. We use tag dispatch
   to choose between moving and copying, so that the move algorithm
   can be written agnostic to which one it uses. We also account for
   moving / copying between uninitialized memory.
   
   Usage:
     assign_dispatch(dest, val, tag_type())

   Tag types:
    move_assign_tag:           The value is moved assigned into dest from val
    uninitialized_move_tag:    The value is move constructed from val into dest
    copy_assign_tag:           The value is copy assigned into dest from val
    uninitialized_copy_tag:    The value is copy constructed from val into dest
    destructive_move_tag:      The value is destructively moved from val into dest
   
*/   

struct move_assign_tag {};
struct uninitialized_move_tag {};
struct copy_assign_tag {};
struct uninitialized_copy_tag {};
struct destructive_move_tag {};

// Move dispatch -- move val into dest
template<typename T>
void assign_dispatch(T& dest, T& val, move_assign_tag) {
  dest = std::move(val);
}

// Copy dispatch -- copy val into dest
template<typename T>
void assign_dispatch(T& dest, const T& val, copy_assign_tag) {
  dest = val;
}

// Uninitialized move dispatch -- move construct dest with val
template<typename T>
void assign_dispatch(T& dest, T& val, uninitialized_move_tag) {
  assign_uninitialized(dest, std::move(val));
}

// Uninitialized copy dispatch -- copy initialize dest with val
template<typename T>
void assign_dispatch(T& dest, const T& val, uninitialized_copy_tag) {
  assign_uninitialized(dest, val);
}

// Destructive move dispatch -- destructively move val into dest
template<typename T>
void assign_dispatch(T& dest, T& val, destructive_move_tag) {
  destructive_move(&dest, &val);
}


}  // namespace parlay

#endif  // PARLAY_UTILITIES_H_
