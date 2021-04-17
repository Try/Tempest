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
  GapiTestCommon::vbo<MetalApi>();
#endif
  }

TEST(MetalApi,VboDyn) {
#if defined(__)
  GapiTestCommon::vboDyn<MetalApi>();
#endif
  }

TEST(MetalApi,Shader) {
#if defined(__OSX__)
  GapiTestCommon::shader<MetalApi>();
#endif
  }

TEST(MetalApi,Pso) {
#if defined(__)
  GapiTestCommon::pso<MetalApi>();
#endif
  }

TEST(MetalApi,Fbo) {
#if defined(__)
  GapiTestCommon::fbo<MetalApi>("MetalApi_Fbo.png");
#endif
  }

TEST(MetalApi,Draw) {
#if defined(__)
  GapiTestCommon::draw<MetalApi,TextureFormat::RGBA8>  ("MetalApi_Draw_RGBA8.png");
  GapiTestCommon::draw<MetalApi,TextureFormat::RG8>    ("MetalApi_Draw_RG8.png");
  GapiTestCommon::draw<MetalApi,TextureFormat::R8>     ("MetalApi_Draw_R8.png");
  GapiTestCommon::draw<MetalApi,TextureFormat::RGBA16> ("MetalApi_Draw_RGBA16.png");
  GapiTestCommon::draw<MetalApi,TextureFormat::RG16>   ("MetalApi_Draw_RG16.png");
  GapiTestCommon::draw<MetalApi,TextureFormat::R16>    ("MetalApi_Draw_R16.png");
  GapiTestCommon::draw<MetalApi,TextureFormat::RGBA32F>("MetalApi_Draw_RGBA32F.png");
  GapiTestCommon::draw<MetalApi,TextureFormat::RG32F>  ("MetalApi_Draw_RG32F.png");
  GapiTestCommon::draw<MetalApi,TextureFormat::R32F>   ("MetalApi_Draw_R32F.png");
#endif
  }

TEST(MetalApi,Compute) {
#if defined(__)
  GapiTestCommon::ssboDispath<MetalApi>();
#endif
  }

TEST(MetalApi,ComputeImage) {
#if defined(__)
  GapiTestCommon::imageCompute<MetalApi>("MetalApi_Compute.png");
#endif
  }

TEST(MetalApi,MipMaps) {
#if defined(__)
  GapiTestCommon::mipMaps<MetalApi,TextureFormat::RGBA8>  ("MetalApi_MipMaps_RGBA8.png");
  GapiTestCommon::mipMaps<MetalApi,TextureFormat::RGBA16> ("MetalApi_MipMaps_RGBA16.png");
  GapiTestCommon::mipMaps<MetalApi,TextureFormat::RGBA32F>("MetalApi_MipMaps_RGBA32.png");
#endif
  }

TEST(MetalApi,S3TC) {
#if defined(__)
  try {
    MetalApi api{ApiFlags::Validation};
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

TEST(MetalApi,TesselationBasic) {
#if defined(__)
  GapiTestCommon::psoTess<MetalApi>();
#endif
  }

TEST(MetalApi,SsboWrite) {
#if defined(__)
  GapiTestCommon::ssboWriteVs<MetalApi>();
#endif
  }

TEST(MetalApi,PushRemapping) {
#if defined(__)
  GapiTestCommon::pushConstant<MetalApi>();
#endif
  }

TEST(MetalApi,SpirvDefect) {
#if defined(__)
  using namespace Tempest;

  try {
    MetalApi api{ApiFlags::Validation};
    Device       device(api);

    auto vbo  = device.vbo(GapiTestCommon::vboData,3);
    auto ibo  = device.ibo(GapiTestCommon::iboData,3);

    auto vert = device.loadShader("shader/link_defect.vert.sprv");
    auto frag = device.loadShader("shader/link_defect.frag.sprv");
    auto pso  = device.pipeline<GapiTestCommon::Vertex>(Topology::Triangles,RenderState(),vert,frag);

    auto tex  = device.attachment(Tempest::TextureFormat::RGBA8,128,128);
    auto fbo  = device.frameBuffer(tex);
    auto rp   = device.pass(FboMode(FboMode::PreserveOut,Color(0.f,0.f,1.f)));

    auto cmd  = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer(fbo,rp);
      enc.setUniforms(pso);
      enc.draw(vbo,ibo);
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
#endif
  }
