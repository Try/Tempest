#include "../gapi/descriptorallocator.h"

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include <memory>

using namespace testing;
using namespace Tempest::Detail;

struct TestDevice {
  ~TestDevice() {
    if(mem!=nullptr)
      std::free(mem);
    }

  void     flush(){}
  uint32_t size() const { return sz; }

  void   realloc(uint32_t size) {
    auto next = std::realloc(mem, size);
    if(next==nullptr)
      throw std::bad_alloc();
    mem = next;
    sz  = size;
    }

  void*    mem = nullptr;
  uint32_t sz  = 0;

  uint32_t elementSize = 32;
  uint32_t reserveSize = 64;
  uint32_t maxSize     = 4096;
  };

TEST(main, DescriptorAllocator) {
  TestDevice device;
  DescriptorAllocator<TestDevice> memory(device);

  auto a0 = memory.alloc(23);
  auto a1 = memory.alloc(27);
  memory.free(a0, 23);
  memory.free(a1, 27);
  }

TEST(main, DescriptorAllocator2) {
  TestDevice device;
  DescriptorAllocator<TestDevice> memory(device);

  auto a0 = memory.alloc(100);
  auto a1 = memory.alloc(20);
  auto a2 = memory.alloc(6);
  memory.free(a0, 100);
  memory.free(a2, 6);
  memory.free(a1, 20);
  }

TEST(main, DescriptorAllocator3) {
  {
    TestDevice device;
    DescriptorAllocator<TestDevice> memory(device);

    auto a0 = memory.alloc(10);
    auto a1 = memory.alloc(10);
    auto a2 = memory.alloc(10);

    memory.free(a0, 10);
    memory.free(a2, 10);
    memory.free(a1, 10);

    EXPECT_NO_THROW(auto big = memory.alloc(30));
  }
  {
    TestDevice device;
    DescriptorAllocator<TestDevice> memory(device);

    auto a0 = memory.alloc(10);
    auto a1 = memory.alloc(10);
    memory.free(a0, 10);
    EXPECT_NO_THROW(memory.free(a1, 10));
  }
  }
