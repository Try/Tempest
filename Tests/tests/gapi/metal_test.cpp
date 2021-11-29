#include <Tempest/MetalApi>
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

TEST(MetalApi,MetalApi) {
#if defined(__OSX__)
  GapiTestCommon::init<MetalApi>();
#endif
  }

TEST(MetalApi,Vbo) {
#if defined(__OSX__)
  GapiTestCommon::Vbo<MetalApi>();
#endif
  }

TEST(MetalApi,VboInit) {
#if defined(__OSX__)
  GapiTestCommon::VboInit<MetalApi>();
#endif
  }

TEST(MetalApi,VboDyn) {
#if defined(__OSX__)
  GapiTestCommon::VboDyn<MetalApi>();
#endif
  }

TEST(MetalApi,SsboCopy) {
#if defined(__OSX__)
  GapiTestCommon::SsboCopy<MetalApi,TextureFormat::RGBA8,uint8_t>();
#endif
  }

TEST(MetalApi,Shader) {
#if defined(__OSX__)
  GapiTestCommon::Shader<MetalApi>();
#endif
  }

TEST(MetalApi,Pso) {
#if defined(__OSX__)
  GapiTestCommon::Pso<MetalApi>();
#endif
  }

TEST(MetalApi,Fbo) {
#if defined(__OSX__)
  GapiTestCommon::Fbo<MetalApi>("MetalApi_Fbo.png");
#endif
  }

TEST(MetalApi,Draw) {
#if defined(__OSX__)
  GapiTestCommon::Draw<MetalApi,TextureFormat::RGBA8>  ("MetalApi_Draw_RGBA8.png");
  GapiTestCommon::Draw<MetalApi,TextureFormat::RGB8>   ("MetalApi_Draw_RGB8.png");
  GapiTestCommon::Draw<MetalApi,TextureFormat::RG8>    ("MetalApi_Draw_RG8.png");
  GapiTestCommon::Draw<MetalApi,TextureFormat::R8>     ("MetalApi_Draw_R8.png");
  GapiTestCommon::Draw<MetalApi,TextureFormat::RGBA16> ("MetalApi_Draw_RGBA16.png");
  GapiTestCommon::Draw<MetalApi,TextureFormat::RGB16>  ("MetalApi_Draw_RGB16.png");
  GapiTestCommon::Draw<MetalApi,TextureFormat::RG16>   ("MetalApi_Draw_RG16.png");
  GapiTestCommon::Draw<MetalApi,TextureFormat::R16>    ("MetalApi_Draw_R16.png");
  GapiTestCommon::Draw<MetalApi,TextureFormat::RGBA32F>("MetalApi_Draw_RGBA32F.hdr");
  GapiTestCommon::Draw<MetalApi,TextureFormat::RGB32F> ("MetalApi_Draw_RGB32F.hdr");
  GapiTestCommon::Draw<MetalApi,TextureFormat::RG32F>  ("MetalApi_Draw_RG32F.hdr");
  GapiTestCommon::Draw<MetalApi,TextureFormat::R32F>   ("MetalApi_Draw_R32F.hdr");
#endif
  }

TEST(MetalApi,Viewport) {
#if defined(__OSX__)
  GapiTestCommon::Viewport<MetalApi>("MetalApi_Viewport.png");
#endif
  }

TEST(MetalApi,Ubo) {
#if defined(__OSX__)
  GapiTestCommon::uniforms<MetalApi,TextureFormat::RGBA8>("MetalApi_Uniforms.png");
#endif
  }

TEST(MetalApi,Compute) {
#if defined(__OSX__)
  GapiTestCommon::Compute<MetalApi>();
#endif
  }

TEST(MetalApi,ComputeImage) {
#if defined(__OSX__)
  GapiTestCommon::ComputeImage<MetalApi>("MetalApi_ComputeImage.png");
#endif
  }

TEST(MetalApi,MipMaps) {
#if defined(__OSX__)
  GapiTestCommon::MipMaps<MetalApi,TextureFormat::RGBA8>  ("MetalApi_MipMaps_RGBA8.png");
  GapiTestCommon::MipMaps<MetalApi,TextureFormat::RGBA16> ("MetalApi_MipMaps_RGBA16.png");
  GapiTestCommon::MipMaps<MetalApi,TextureFormat::RGBA32F>("MetalApi_MipMaps_RGBA32.png");
#endif
  }

TEST(MetalApi,S3TC) {
#if defined(__OSX__)
  try {
    MetalApi api{ApiFlags::Validation};
    Device       device(api);

    auto tex = device.texture("data/img/tst-dxt5.dds");
    EXPECT_EQ(tex.format(),TextureFormat::DXT5);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping directx testcase: ", e.what()); else
      throw;
    }
#endif
  }

TEST(MetalApi,DISABLED_TesselationBasic) {
#if defined(__OSX__)
  GapiTestCommon::PsoTess<MetalApi>();
#endif
  }

TEST(MetalApi,SsboWrite) {
#if defined(__OSX__)
  GapiTestCommon::SsboWrite<MetalApi>();
#endif
  }

TEST(MetalApi,PushRemapping) {
#if defined(__OSX__)
  GapiTestCommon::PushRemapping<MetalApi>();
#endif
  }
