#include <Tempest/Signal>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

using namespace testing;
using namespace Tempest;

struct Cls {
  mutable std::string log;

  void fun(int,float)         { log+="func()";  }
  void funC(int,float) const  { log+="funcC()"; }

  void funR(int,const float&) { log+="funcR()"; }
  };

struct Cow {
  Signal<void()> sig;
  mutable std::string log;

  void func0(){ log+="func0()"; }
  void func1(){ log+="func1()"; }
  void func2(){ log+="func2()"; }

  void connect(){
    sig.bind(this,&Cow::func2);
    }
  void disconnect(){
    sig.ubind(this,&Cow::func0);
    }
  };

TEST(main,Signal0) {
  Cls test;
  Signal<void(int,float)> sig;

  sig.bind(&test,&Cls::funR);
  sig.bind(&test,&Cls::fun);
  sig.bind(&test,&Cls::funC);

  sig(1,3.14f);

  EXPECT_EQ(test.log,"funcR()func()funcC()");
  }

TEST(main,Signal1) {
  Cls test;
  Signal<void(int,float)> sig;

  sig.bind(&test,&Cls::funR);
  sig.bind(&test,&Cls::fun);
  sig.bind(&test,&Cls::funC);

  sig.ubind(&test,&Cls::fun);

  sig(1,3.14f);

  EXPECT_EQ(test.log,"funcR()funcC()");
  }

TEST(main,SignalCOW0) {
  Cow test;

  test.sig.bind(&test,&Cow::func0);
  test.sig.bind(&test,&Cow::disconnect);
  test.sig.bind(&test,&Cow::func1);
  test.sig();

  EXPECT_EQ(test.log,"func0()func1()");

  test.log.clear();
  test.sig();
  EXPECT_EQ(test.log,"func1()");
  }

TEST(main,SignalCOW1) {
  Cow test;

  test.sig.bind(&test,&Cow::func0);
  test.sig.bind(&test,&Cow::connect);
  test.sig.bind(&test,&Cow::func1);
  test.sig();

  EXPECT_EQ(test.log,"func0()func1()");

  test.log.clear();
  test.sig();
  EXPECT_EQ(test.log,"func0()func1()func2()");
  }
