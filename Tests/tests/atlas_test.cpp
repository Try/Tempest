#include "../gapi/deviceallocator.h"
#include "../gapi/rectallocator.h"

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

using namespace testing;
using namespace Tempest::Detail;

struct TestDevice {
  using DeviceMemory=void*;

  DeviceMemory alloc(uint32_t w,uint32_t h){
    return std::malloc(w*h);
    }

  void free(DeviceMemory m){
    std::free(m);
    }
  };

using Allocation=typename Tempest::RectAllocator<TestDevice>::Allocation;

TEST(main, AlasAlloator0) {
  TestDevice device;
  Tempest::RectAllocator<TestDevice> allocator(device);

  auto s1 = allocator.alloc(128,64);
  auto s2 = allocator.alloc(32,32);

  s2=Allocation();
  s1=Allocation();
  }

TEST(main, AlasAlloator1) {
  TestDevice device;
  Tempest::RectAllocator<TestDevice> allocator(device);

  auto s1 = allocator.alloc(128,64);
  auto s2 = s1;

  s2=Allocation();
  s1=Allocation();
  }
