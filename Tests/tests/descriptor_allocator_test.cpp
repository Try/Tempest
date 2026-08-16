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
  uint32_t maxSize     = 1024;
  };

TEST(main, DescriptorAllocator) {
  TestDevice device;
  DescriptorAllocator<TestDevice> memory(device);

  auto a0 = memory.alloc(23);
  memory.free(a0, 23);
  }
