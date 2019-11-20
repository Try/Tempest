#include <Tempest/Widget>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

using namespace testing;
using namespace Tempest;

TEST(main,Enable) {
  Widget w;

  Widget& b0=w.addWidget(new Widget());
  Widget& b1=w.addWidget(new Widget());

  Widget& b00=b0.addWidget(new Widget());
  Widget& b01=b0.addWidget(new Widget());

  b0.setEnabled(false);
  b01.setEnabled(false);
  EXPECT_FALSE(b0.isEnabled());
  EXPECT_FALSE(b00.isEnabled());

  b0.takeWidget(&b00);
  b0.takeWidget(&b01);

  EXPECT_TRUE (b00.isEnabled());
  EXPECT_FALSE(b01.isEnabled());

  EXPECT_TRUE (b1.isEnabled());

  b0.addWidget(&b00);
  b0.addWidget(&b01);
  EXPECT_FALSE(b00.isEnabled());
  EXPECT_FALSE(b01.isEnabled());
  }
