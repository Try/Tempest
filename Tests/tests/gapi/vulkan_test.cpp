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

    auto tex  = device.attachment(TextureFormat::RGBA8,128,128,true);
    auto fbo  = device.frameBuffer(tex);
    auto rp   = device.pass(FboMode(FboMode::PreserveOut,Color(0.f,0.f,1.f)));
    auto sync = device.fence();

    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer(fbo,rp); // clear image
      enc.setFramebuffer(nullptr);
      enc.generateMipmaps(tex);
    }
    device.submit(cmd,sync);
    sync.wait();
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
  }
