#include <Tempest/Point>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

using namespace testing;
using namespace Tempest;

TEST(main, Point) {
  Point a{1,1},b{3,4};

  auto sum = a+b;
  auto sub = a-b;
  auto mul = b*5;
  auto div = mul/5;
  auto neg = -b;

  EXPECT_EQ(sum,(Point{4,5}));
  EXPECT_EQ(sub,(Point{-2,-3}));
  EXPECT_EQ(mul,(Point{15,20}));
  EXPECT_EQ(div,b);
  EXPECT_EQ(neg,(Point{-3,-4}));
  }
