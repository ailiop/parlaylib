#include "gtest/gtest.h"

#include <algorithm>
#include <deque>
#include <numeric>

#define PARLAY_DEBUG_UNINITIALIZED
#include <parlay/internal/debug_uninitialized.h>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include "sorting_utils.h"

TEST(TestIntegerSort, TestUninitialized) {
  auto s = parlay::sequence<parlay::debug_uninitialized>::uninitialized(10000000);
  parlay::parallel_for(0, 10000000, [&](size_t i) {
    s[i].x = (50021 * i + 61) % (1 << 20);
  });
  auto sorted = parlay::integer_sort(s, [](auto s) { return s.x; });
  ASSERT_EQ(s.size(), sorted.size());
}

namespace parlay {
  // Specialize std::unique_ptr to be considered trivially destructive movable
  template<typename T>
  struct is_trivially_relocatable<std::unique_ptr<T>> : public std::true_type { };
}

TEST(TestIntegerSort, TestIntegerSortInplaceUniquePtr) {
  auto s = parlay::tabulate(100000, [](size_t i) {
    return std::make_unique<int>((50021 * i + 61) % (1 << 20));
  });
  auto sorted = parlay::tabulate(100000, [](size_t i) {
    return std::make_unique<int>((50021 * i + 61) % (1 << 20));
  });
  std::sort(std::begin(sorted), std::end(sorted), [](const auto& p1, const auto& p2) {
    return *p1 < *p2;
  });
  parlay::integer_sort_inplace(s, [](const auto& p) { return *p; });
  ASSERT_EQ(s.size(), sorted.size());
  for (size_t i = 0; i < 100000; i++) {
    ASSERT_EQ(*s[i], *sorted[i]);
  }
}

// HeapInt is both copyable and trivially destructive movable
struct HeapInt {
  int* x;
  HeapInt(int _x) : x(new int(_x)) { }
  ~HeapInt() { if (x != nullptr) delete x; }
  HeapInt(const HeapInt& other) : x(new int(*(other.x))) { }
  HeapInt(HeapInt&& other) : x(other.x) {
    other.x = nullptr;
  }
  HeapInt& operator=(const HeapInt& other) {
    if (x != nullptr) delete x;
    x = new int(*(other.x));
    return *this;
  }
  HeapInt& operator=(HeapInt&& other) {
    if (x != nullptr) delete x;
    x = other.x;
    other.x = nullptr;
    return *this;
  }
  int value() const {
    assert(x != nullptr);
    return *x;
  }
};

namespace parlay {
  // Specialize std::unique_ptr to be considered trivially relocatable
  template<>
  struct is_trivially_relocatable<HeapInt> : public std::true_type { };
}

TEST(TestIntegerSort, TestIntegerSortCopyAndDestructiveMove) {
  auto s = parlay::tabulate(100000, [](size_t i) {
    return HeapInt((50021 * i + 61) % (1 << 20));
  });
  auto real_sorted = parlay::tabulate(100000, [](size_t i) {
    return HeapInt((50021 * i + 61) % (1 << 20));
  });
  std::sort(std::begin(real_sorted), std::end(real_sorted), [](const auto& p1, const auto& p2) {
    return p1.value() < p2.value();
  });
  auto sorted = parlay::integer_sort(s, [](const auto& p) { return p.value(); });
  ASSERT_EQ(sorted.size(), s.size());
  for (size_t i = 0; i < 100000; i++) {
    ASSERT_EQ(real_sorted[i].value(), sorted[i].value());
  }
}
