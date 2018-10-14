#include "../gapi/deviceallocator.h"

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

using namespace testing;
using namespace Tempest::Detail;

struct TestDevice {
  using DeviceMemory=void*;

  DeviceMemory alloc(size_t size, uint32_t /*typeBits*/){
    return std::malloc(size);
    }

  void free(DeviceMemory m){
    std::free(m);
    }
  };

TEST(main, DeviceAllocator) {
  TestDevice device;
  DeviceAllocator<TestDevice> memory(device);

  auto p1 = memory.alloc(64,0);
  memory.free(p1);
  }

TEST(main, DeviceAllocator2) {
  TestDevice device;
  DeviceAllocator<TestDevice> memory(device);

  auto p1 = memory.alloc(64,0);
  auto p2 = memory.alloc(128,0);
  auto p3 = memory.alloc(32,0);
  memory.free(p1);
  memory.free(p2);
  memory.free(p3);
  }

TEST(main, DeviceAllocator3) {
  TestDevice device;
  DeviceAllocator<TestDevice> memory(device);

  auto p1 = memory.alloc(64,0);
  auto p2 = memory.alloc(128,0);
  auto p3 = memory.alloc(32,0);
  memory.free(p2);
  memory.free(p1);
  memory.free(p3);
  }
