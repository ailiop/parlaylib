#include "gtest/gtest.h"

#include <parlay/destructive_move.h>

// A type that is not trivially destructively movable because
// it keeps a pointer to an object inside itself
struct NotTriviallyDestructiveMovable {
  int x;
  int* px;
  NotTriviallyDestructiveMovable(int _x) : x(_x), px(&x) { }
  NotTriviallyDestructiveMovable(const NotTriviallyDestructiveMovable& other) : x(other.x), px(&x) { }
  NotTriviallyDestructiveMovable(NotTriviallyDestructiveMovable&& other) noexcept : x(other.x), px(&x) { }
};

// A type that is trivially destructively movable because it is
// trivially movable and trivally destructible
struct TriviallyDestructiveMovable {
  int x;
  TriviallyDestructiveMovable(int _x) : x(_x) { }
  TriviallyDestructiveMovable(const TriviallyDestructiveMovable&) = default;
  TriviallyDestructiveMovable(TriviallyDestructiveMovable&&) = default;
  ~TriviallyDestructiveMovable() = default;
};

// A type that we annotate as trivially destructively movable,
// even though it is not deducible as such by the compiler
struct MyTriviallyDestructiveMovable {
  int* x;
  MyTriviallyDestructiveMovable(int _x) : x(new int(_x)) { }
  MyTriviallyDestructiveMovable(const MyTriviallyDestructiveMovable& other) : x(nullptr) {
    if (other.x != nullptr) {
      x = new int(*(other.x));
    }
  }
  MyTriviallyDestructiveMovable(MyTriviallyDestructiveMovable&& other) : x(other.x) { other.x = nullptr; }
  ~MyTriviallyDestructiveMovable() {
    if (x != nullptr) {
      *x = -1;
      delete x;
      x = nullptr;
    }
  }
};

namespace parlay {
  
// Mark the type MyTriviallyDestructiveMovable as trivially destructively movable
template<>
struct is_trivially_destructive_movable<MyTriviallyDestructiveMovable> : public std::true_type { };
  
}


static_assert(!parlay::is_trivially_destructive_movable_v<NotTriviallyDestructiveMovable>);
static_assert(parlay::is_trivially_destructive_movable_v<TriviallyDestructiveMovable>);
static_assert(parlay::is_trivially_destructive_movable_v<MyTriviallyDestructiveMovable>);


TEST(TestDestructiveMove, TestNotTriviallyDestructiveMovable) {
  std::aligned_storage<sizeof(NotTriviallyDestructiveMovable), alignof(NotTriviallyDestructiveMovable)>::type a, b;
  NotTriviallyDestructiveMovable* from = std::launder(reinterpret_cast<NotTriviallyDestructiveMovable*>(&a));
  NotTriviallyDestructiveMovable* to = std::launder(reinterpret_cast<NotTriviallyDestructiveMovable*>(&b));
  // -- Both from and to point to uninitialized memory
  
  new (from) NotTriviallyDestructiveMovable(42);
  ASSERT_EQ(from->x, 42);
  ASSERT_EQ(from->px, &(from->x));
  // -- Now from points to a valid object, and to points to uninitialized memory
  
  parlay::destructive_move(to, from);
  ASSERT_EQ(to->x, 42);
  ASSERT_EQ(to->px, &(to->x));
  // -- Now to points to a valid object, and from points to uninitialized memory
  
  to->~NotTriviallyDestructiveMovable();
  // -- Both from and to point to uninitialized memory
}


TEST(TestDestructiveMove, TestTriviallyDestructiveMovable) {
  std::aligned_storage<sizeof(TriviallyDestructiveMovable), alignof(TriviallyDestructiveMovable)>::type a, b;
  TriviallyDestructiveMovable* from = std::launder(reinterpret_cast<TriviallyDestructiveMovable*>(&a));
  TriviallyDestructiveMovable* to = std::launder(reinterpret_cast<TriviallyDestructiveMovable*>(&b));
  // -- Both from and to point to uninitialized memory
  
  new (from) TriviallyDestructiveMovable(42);
  ASSERT_EQ(from->x, 42);
  // -- Now from points to a valid object, and to points to uninitialized memory
  
  parlay::destructive_move(to, from);
  ASSERT_EQ(to->x, 42);
  // -- Now to points to a valid object, and from points to uninitialized memory
  
  to->~TriviallyDestructiveMovable();
  // -- Both from and to point to uninitialized memory
}


TEST(TestDestructiveMove, TestCustomTriviallyDestructiveMovable) {
  std::aligned_storage<sizeof(MyTriviallyDestructiveMovable), alignof(MyTriviallyDestructiveMovable)>::type a, b;
  MyTriviallyDestructiveMovable* from = std::launder(reinterpret_cast<MyTriviallyDestructiveMovable*>(&a));
  MyTriviallyDestructiveMovable* to = std::launder(reinterpret_cast<MyTriviallyDestructiveMovable*>(&b));
  // -- Both from and to point to uninitialized memory
  
  new (from) MyTriviallyDestructiveMovable(42);
  ASSERT_EQ(*(from->x), 42);
  // -- Now from points to a valid object, and to points to uninitialized memory
  
  parlay::destructive_move(to, from);
  ASSERT_EQ(*(to->x), 42);
  // -- Now to points to a valid object, and from points to uninitialized memory
  
  to->~MyTriviallyDestructiveMovable();
  // -- Both from and to point to uninitialized memory
}


TEST(TestDestructiveMove, TestNotTriviallyDestructiveMovableArray) {
  constexpr size_t N = 100000;
  std::vector<std::aligned_storage<sizeof(NotTriviallyDestructiveMovable), alignof(NotTriviallyDestructiveMovable)>::type> a(N), b(N);
  NotTriviallyDestructiveMovable* from = std::launder(reinterpret_cast<NotTriviallyDestructiveMovable*>(a.data()));
  NotTriviallyDestructiveMovable* to = std::launder(reinterpret_cast<NotTriviallyDestructiveMovable*>(b.data()));
  // -- Both from and to point to uninitialized memory
  
  for (size_t i = 0; i < N; i++) {
    new (&from[i]) NotTriviallyDestructiveMovable(i);
  }
  for (size_t i = 0; i < N; i++) {
    ASSERT_EQ(from[i].x, i);
    ASSERT_EQ(from[i].px, &(from[i].x));
  }
  // -- Now from points to an array of valid objects, and to points to uninitialized memory
  
  parlay::destructive_move_array(to, from, N);
  for (size_t i = 0; i < N; i++) {
    ASSERT_EQ(to[i].x, i);
    ASSERT_EQ(to[i].px, &(to[i].x));
  }
  // -- Now to points to an array of valid objects, and from points to uninitialized memory
  
  for (size_t i = 0; i < N; i++) {
    to[i].~NotTriviallyDestructiveMovable();
  }
  // -- Both from and to point to uninitialized memory
}

TEST(TestDestructiveMove, TestTriviallyDestructiveMovableArray) {
  constexpr size_t N = 100000;
  std::vector<std::aligned_storage<sizeof(TriviallyDestructiveMovable), alignof(TriviallyDestructiveMovable)>::type> a(N), b(N);
  TriviallyDestructiveMovable* from = std::launder(reinterpret_cast<TriviallyDestructiveMovable*>(a.data()));
  TriviallyDestructiveMovable* to = std::launder(reinterpret_cast<TriviallyDestructiveMovable*>(b.data()));
  // -- Both from and to point to uninitialized memory
  
  for (size_t i = 0; i < N; i++) {
    new (&from[i]) TriviallyDestructiveMovable(i);
  }
  for (size_t i = 0; i < N; i++) {
    ASSERT_EQ(from[i].x, i);
  }
  // -- Now from points to an array of valid objects, and to points to uninitialized memory
  
  parlay::destructive_move_array(to, from, N);
  for (size_t i = 0; i < N; i++) {
    ASSERT_EQ(to[i].x, i);
  }
  // -- Now to points to an array of valid objects, and from points to uninitialized memory
  
  for (size_t i = 0; i < N; i++) {
    to[i].~TriviallyDestructiveMovable();
  }
  // -- Both from and to point to uninitialized memory
}

TEST(TestDestructiveMove, TestCustomTriviallyDestructiveMovableArray) {
  constexpr size_t N = 100000;
  std::vector<std::aligned_storage<sizeof(MyTriviallyDestructiveMovable), alignof(MyTriviallyDestructiveMovable)>::type> a(N), b(N);
  MyTriviallyDestructiveMovable* from = std::launder(reinterpret_cast<MyTriviallyDestructiveMovable*>(a.data()));
  MyTriviallyDestructiveMovable* to = std::launder(reinterpret_cast<MyTriviallyDestructiveMovable*>(b.data()));
  // -- Both from and to point to uninitialized memory
  
  for (size_t i = 0; i < N; i++) {
    new (&from[i]) MyTriviallyDestructiveMovable(i);
  }
  for (size_t i = 0; i < N; i++) {
    ASSERT_EQ(*(from[i].x), i);
  }
  // -- Now from points to an array of valid objects, and to points to uninitialized memory
  
  parlay::destructive_move_array(to, from, N);
  for (size_t i = 0; i < N; i++) {
    ASSERT_EQ(*(to[i].x), i);
  }
  // -- Now to points to an array of valid objects, and from points to uninitialized memory
  
  for (size_t i = 0; i < N; i++) {
    to[i].~MyTriviallyDestructiveMovable();
  }
  // -- Both from and to point to uninitialized memory
}
