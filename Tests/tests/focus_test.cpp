#include <Tempest/Widget>
#include <Tempest/Button>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

using namespace testing;
using namespace Tempest;

TEST(main,Focus0) {
  Widget w;

  Widget& b0=w.addWidget(new Widget());
  Widget& b1=w.addWidget(new Widget());

  Widget& b00=b0.addWidget(new Widget());
  Widget& b01=b0.addWidget(new Widget()); (void)b01;

  Widget& b10=b1.addWidget(new Widget());

  b00.setFocus(true);
  EXPECT_TRUE(b00.hasFocus());
  b10.setFocus(true);
  EXPECT_FALSE(b00.hasFocus());
  EXPECT_TRUE (b10.hasFocus());
  }

TEST(main,Focus1) {
  Widget w;

  Widget& b0=w .addWidget(new Widget());
  Widget& b1=b0.addWidget(new Widget());(void)b1;
  Widget& b2=b1.addWidget(new Widget());
  Widget& b3=b2.addWidget(new Widget());(void)b3;

  b2.setFocus(true);
  EXPECT_TRUE(b2.hasFocus());
  b0.setFocus(true);
  EXPECT_FALSE(b2.hasFocus());
  EXPECT_TRUE (b0.hasFocus());
  }
