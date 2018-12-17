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

  Widget& b0=w.addWidget(new Widget());
  Widget& b1=w.addWidget(new Widget());

  EXPECT_EQ(b0.h(),b1.h());
  }

TEST(main,LinearLayoutMaxSize) {
  Widget w;
  w.resize(1000,1000);
  w.setLayout(Vertical);

  Widget& b0=w.addWidget(new Widget());
  Widget& b1=w.addWidget(new Widget());
  b1.setMinimumSize(50,50);
  b1.setSizePolicy(Fixed);

  EXPECT_EQ(w.h(),b0.h()+b1.h()+w.spacing());
  EXPECT_EQ(b1.h(),50);
  }
