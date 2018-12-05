#include "../gapi/deviceallocator.h"
#include "../gapi/rectalllocator.h"

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

TEST(main, AlasAlloator) {
  TestDevice device;
  Tempest::RectAlllocator<TestDevice> allocator(device);

  auto s1 = allocator.alloc(128,64);
  auto s2 = allocator.alloc(32,32);

  allocator.free(s2);
  allocator.free(s1);
  }
