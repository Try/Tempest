#include <Tempest/Widget>

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

TEST(main,LinearLayoutVisibility) {
  Widget w;
  w.resize(300,300);
  w.setSpacing(0);
  w.setLayout(Vertical);

  Widget* b[3]={};
  b[0] = &w.addWidget(new Widget());
  b[1] = &w.addWidget(new Widget());
  b[2] = &w.addWidget(new Widget());

  EXPECT_EQ(b[0]->h(),100);
  EXPECT_EQ(b[0]->h(),b[1]->h());
  EXPECT_EQ(b[0]->h(),b[2]->h());

  // one invisible
  for(size_t i=0;i<3;++i) {
    b[i]->setVisible(false);
    for(size_t r=0;r<3;++r) {
      if(r==i)
        continue;
      EXPECT_EQ(b[r]->h(),150);
      }
    b[i]->setVisible(true);
    }

  // three invisible
  for(size_t i=0;i<3;++i)
    b[i]->setVisible(false);

  // two invisible
  for(size_t i=0;i<3;++i) {
    b[i]->setVisible(true);
    EXPECT_EQ(b[i]->h(),300);
    b[i]->setVisible(false);
    }
  }
