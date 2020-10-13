// Tests for incorrect usages of uninitialized memory in Parlay's algorithms
//
// Note that these tests will produce false positives on some compilers because
// they rely on performing undefined behaviour and hoping that everything works
// out. This is because, in order to track whether an object is initialized or
// uninitialized, we use a special type that writes to a member flag during that
// object's destructor. If this write is actually performed and is visible, we
// can then check this flag when performing uninitialized operations to ensure
// that the memory is indeed uninitialized. Since this flag is volatile, the
// compiler should ensure that it is written to memory and visible after the
// object's destruction, but we can not guarantee this since we are comitting
// undefined behaviour anyway.
//

#include "gtest/gtest.h"

#define PARLAY_DEBUG_UNINITIALIZED
#include <parlay/internal/debug_uninitialized.h>

#include <parlay/primitives.h>
#include <parlay/sequence.h>

#include "sorting_utils.h"

TEST(TestUninitializedMemory, TestIntegerSort) {
  auto s = parlay::sequence<parlay::internal::UninitializedTracker>(10000000);
  parlay::parallel_for(0, 10000000, [&](size_t i) {
    s[i].x = (50021 * i + 61) % (1 << 20);
  });
  auto sorted = parlay::internal::integer_sort(make_slice(s), [](auto s) { return s.x; });
  ASSERT_EQ(s.size(), sorted.size());
  ASSERT_TRUE(std::is_sorted(std::begin(sorted), std::end(sorted), [](auto x, auto y) { return x.x < y.x; }));
}

TEST(TestUninitializedMemory, TestIntegerSortInPlace) {
  auto s = parlay::sequence<parlay::internal::UninitializedTracker>(10000000);
  parlay::parallel_for(0, 10000000, [&](size_t i) {
  s[i].x = (50021 * i + 61) % (1 << 20);
  });
  parlay::internal::integer_sort_inplace(make_slice(s), [](auto s) { return s.x; });
  ASSERT_TRUE(std::is_sorted(std::begin(s), std::end(s), [](auto x, auto y) { return x.x < y.x; }));
}