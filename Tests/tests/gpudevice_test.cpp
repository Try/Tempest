#include <Tempest/VulkanApi>
#include <Tempest/Except>
#include <Tempest/Device>
#include <Tempest/Log>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

using namespace testing;
using namespace Tempest;

TEST(main,VulkanApi) {
  try {
    VulkanApi api;
    (void)api;
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping vulkan testcase: ", e.what());
    }
  }
