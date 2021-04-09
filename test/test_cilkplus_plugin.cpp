#include "gtest/gtest.h"

#include <algorithm>
#include <chrono>
#include <numeric>
#include <thread>

// Use Cilk Plus for scheduling
#define PARLAY_CILKPLUS
#include <parlay/parallel.h>

TEST(TestCilkPlus, TestParDo) {
  int x = 0, y = 0;
  parlay::par_do(
    [&]() { x = 1; },
    [&]() { y = 2; }
  );
  ASSERT_EQ(x, 1);
  ASSERT_EQ(y, 2);
}

TEST(TestCilkPlus, TestParFor) {
  size_t n = 1000;
  std::vector<int> v(n);
  parlay::parallel_for(0, n, [&](auto i) {
    v[i] = i;
  });
  for (size_t i = 0; i < n; i++) {
    ASSERT_EQ(v[i], i);
  }
}

TEST(TestCilkPlus, TestGranularFor) {
  size_t n = 1000;
  std::vector<int> v(n);
  parlay::parallel_for(0, n, [&](auto i) {
    v[i] = i;
  }, 10);
  for (size_t i = 0; i < n; i++) {
    ASSERT_EQ(v[i], i);
  }
}
