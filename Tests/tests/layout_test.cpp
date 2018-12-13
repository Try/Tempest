#include <Tempest/Widget>
#include <Tempest/Button>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

using namespace testing;
using namespace Tempest;

TEST(main,LinearLayout) {
  Widget w;
  w.resize(1000,1000);
  w.setLayout(Vertical);

  Widget* b0=&w.addWidget(new Widget());
  Widget* b1=&w.addWidget(new Widget());

  EXPECT_EQ(b0->h(),b1->h());
  }
