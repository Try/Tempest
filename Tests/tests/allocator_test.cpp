#include "../gapi/deviceallocator.h"

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

using namespace testing;
using namespace Tempest::Detail;

struct TestDevice {
  using DeviceMemory=void*;

  DeviceMemory alloc(size_t size, uint32_t /*typeId*/){
    return std::malloc(size);
    }

  void free(DeviceMemory m,size_t /*size*/,uint32_t /*typeId*/){
    std::free(m);
    }
  };

TEST(main, DeviceAllocator) {
  TestDevice device;
  DeviceAllocator<TestDevice> memory(device);

  auto p1 = memory.alloc(64,1,0);
  memory.free(p1);
  }

TEST(main, DeviceAllocator2) {
  TestDevice device;
  DeviceAllocator<TestDevice> memory(device);

  auto p1 = memory.alloc(64, 1,0);
  auto p2 = memory.alloc(128,1,0);
  auto p3 = memory.alloc(32, 1,0);
  memory.free(p1);
  memory.free(p2);
  memory.free(p3);
  }

TEST(main, DeviceAllocator3) {
  TestDevice device;
  DeviceAllocator<TestDevice> memory(device);

  auto p1 = memory.alloc(64, 1,0);
  auto p2 = memory.alloc(128,1,0);
  auto p3 = memory.alloc(32, 1,0);
  memory.free(p2);
  memory.free(p1);
  memory.free(p3);
  }

TEST(main, DeviceAllocatorMergeBlock) {
  TestDevice device;
  DeviceAllocator<TestDevice> memory(device);

  auto p1 = memory.alloc(64, 1,0);
  auto p2 = memory.alloc(64, 1,0);
  auto p3 = memory.alloc(64, 1,0);
  auto p4 = memory.alloc(64, 1,0);
  auto p5 = memory.alloc(64, 1,0);
  memory.free(p4);
  memory.free(p2);
  memory.free(p3);
  memory.free(p1);
  memory.free(p5);
  }

TEST(main, DeviceAllocatorAlign0) {
  TestDevice device;
  DeviceAllocator<TestDevice> memory(device);

  size_t big=(DeviceAllocator<TestDevice>::DEFAULT_PAGE_SIZE-((64+5-1)/5)*5);
  auto p1 = memory.alloc(64,  4,0);
  auto p2 = memory.alloc(big, 5,0);
  auto p3 = memory.alloc(32,  6,0);
  memory.free(p2);
  memory.free(p1);
  memory.free(p3);
  }

TEST(main, DeviceAllocatorAlign1) {
  TestDevice device;
  DeviceAllocator<TestDevice> memory(device);

  size_t big=(DeviceAllocator<TestDevice>::DEFAULT_PAGE_SIZE-((64+5-1)/5)*5)-1;
  auto p1 = memory.alloc(64,  4,0);
  auto p2 = memory.alloc(big, 5,0);
  auto p3 = memory.alloc(32,  6,0);
  memory.free(p2);
  memory.free(p1);
  memory.free(p3);
  }
