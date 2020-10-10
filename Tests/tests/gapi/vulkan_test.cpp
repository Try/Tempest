#include <Tempest/VulkanApi>
#include <Tempest/Except>
#include <Tempest/Device>
#include <Tempest/Fence>
#include <Tempest/Pixmap>
#include <Tempest/Log>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include "gapi_test_common.h"

using namespace testing;
using namespace Tempest;


TEST(VulkanApi,VulkanApi) {
  GapiTestCommon::init<VulkanApi>();
  }

TEST(VulkanApi,Vbo) {
  GapiTestCommon::vbo<VulkanApi>();
  }

TEST(VulkanApi,Shader) {
  GapiTestCommon::shader<VulkanApi>();
  }

TEST(VulkanApi,Pso) {
  GapiTestCommon::pso<VulkanApi>();
  }

TEST(VulkanApi,Fbo) {
  GapiTestCommon::fbo<VulkanApi>("VulkanApi_Fbo.png");
  }

TEST(VulkanApi,Draw) {
  GapiTestCommon::draw<VulkanApi>("VulkanApi_Draw.png");
  }

TEST(VulkanApi,Compute) {
  GapiTestCommon::ssboDispath<VulkanApi>();
  }

TEST(VulkanApi,ComputeImage) {
  GapiTestCommon::imageCompute<VulkanApi>("VulkanApi_Compute.png");
  }

TEST(VulkanApi,MipMaps) {
  using namespace Tempest;

  try {
    VulkanApi   api{ApiFlags::Validation};
    Device      device(api);

    auto pm  = device.attachment(TextureFormat::RGBA8,32,32,true);

    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.generateMipmaps(pm);
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }
