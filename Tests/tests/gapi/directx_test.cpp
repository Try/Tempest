#include <Tempest/DirectX12Api>
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

TEST(DirectX12Api,DirectX12Api) {
#if defined(_MSC_VER)
  GapiTestCommon::init<DirectX12Api>();
#endif
  }

TEST(DirectX12Api,Vbo) {
#if defined(_MSC_VER)
  GapiTestCommon::vbo<DirectX12Api>();
#endif
  }


TEST(DirectX12Api,Shader) {
#if defined(_MSC_VER)
  GapiTestCommon::shader<DirectX12Api>();
#endif
  }

TEST(DirectX12Api,Pso) {
#if defined(_MSC_VER)
  GapiTestCommon::pso<DirectX12Api>();
#endif
  }

TEST(DirectX12Api,Fbo) {
#if defined(_MSC_VER)
  GapiTestCommon::fbo<DirectX12Api>("DirectX12Api_Fbo.png");
#endif
  }

TEST(DirectX12Api,Draw) {
#if defined(_MSC_VER)
  GapiTestCommon::draw<DirectX12Api,TextureFormat::RGBA8>  ("DirectX12Api_Draw_RGBA8.png");
  GapiTestCommon::draw<DirectX12Api,TextureFormat::RG8>    ("DirectX12Api_Draw_RG8.png");
  GapiTestCommon::draw<DirectX12Api,TextureFormat::R8>     ("DirectX12Api_Draw_R8.png");
  GapiTestCommon::draw<DirectX12Api,TextureFormat::RGBA16> ("DirectX12Api_Draw_RGBA16.png");
  GapiTestCommon::draw<DirectX12Api,TextureFormat::RG16>   ("DirectX12Api_Draw_RG16.png");
  GapiTestCommon::draw<DirectX12Api,TextureFormat::R16>    ("DirectX12Api_Draw_R16.png");
  GapiTestCommon::draw<DirectX12Api,TextureFormat::RGBA32F>("DirectX12Api_Draw_RGBA32F.png");
  GapiTestCommon::draw<DirectX12Api,TextureFormat::RG32F>  ("DirectX12Api_Draw_RG32F.png");
  GapiTestCommon::draw<DirectX12Api,TextureFormat::R32F>   ("DirectX12Api_Draw_R32F.png");
#endif
  }

TEST(DirectX12Api,Compute) {
#if defined(_MSC_VER)
  GapiTestCommon::ssboDispath<DirectX12Api>();
#endif
  }

TEST(DirectX12Api,ComputeImage) {
#if defined(_MSC_VER)
  GapiTestCommon::imageCompute<DirectX12Api>("DirectX12Api_Compute.png");
#endif
  }

TEST(DirectX12Api,MipMaps) {
#if defined(_MSC_VER)
  GapiTestCommon::mipMaps<DirectX12Api,TextureFormat::RGBA8>  ("DirectX12Api_MipMaps_RGBA8.png");
  GapiTestCommon::mipMaps<DirectX12Api,TextureFormat::RGBA16> ("DirectX12Api_MipMaps_RGBA16.png");
  GapiTestCommon::mipMaps<DirectX12Api,TextureFormat::RGBA32F>("DirectX12Api_MipMaps_RGBA32.png");
#endif
  }

TEST(DirectX12Api,S3TC) {
#if defined(_MSC_VER)
  try {
    DirectX12Api api{ApiFlags::Validation};
    Device       device(api);

    auto tex = device.loadTexture("data/img/tst-dxt5.dds");
    EXPECT_EQ(tex.format(),TextureFormat::DXT5);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping directx testcase: ", e.what()); else
      throw;
    }
#endif
  }

TEST(DirectX12Api,DISABLED_TesselationBasic) {
#if defined(_MSC_VER)
  GapiTestCommon::psoTess<DirectX12Api>();
#endif
  }

TEST(DirectX12Api,SsboWrite) {
#if defined(_MSC_VER)
  GapiTestCommon::ssboWriteVs<DirectX12Api>();
#endif
  }

TEST(DirectX12Api,PushRemapping) {
#if defined(_MSC_VER)
  GapiTestCommon::pushConstant<DirectX12Api>();
#endif
  }
