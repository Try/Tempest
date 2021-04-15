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
#if defined(_MSC_VER)
  GapiTestCommon::init<MetalApi>();
#endif
  }

TEST(MetalApi,Vbo) {
#if defined(_MSC_VER)
  GapiTestCommon::vbo<MetalApi>();
#endif
  }

TEST(MetalApi,VboDyn) {
#if defined(_MSC_VER)
  GapiTestCommon::vboDyn<MetalApi>();
#endif
  }

TEST(MetalApi,Shader) {
#if defined(_MSC_VER)
  GapiTestCommon::shader<MetalApi>();
#endif
  }

TEST(MetalApi,Pso) {
#if defined(_MSC_VER)
  GapiTestCommon::pso<MetalApi>();
#endif
  }

TEST(MetalApi,Fbo) {
#if defined(_MSC_VER)
  GapiTestCommon::fbo<MetalApi>("MetalApi_Fbo.png");
#endif
  }

TEST(MetalApi,Draw) {
#if defined(_MSC_VER)
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
#if defined(_MSC_VER)
  GapiTestCommon::ssboDispath<MetalApi>();
#endif
  }

TEST(MetalApi,ComputeImage) {
#if defined(_MSC_VER)
  GapiTestCommon::imageCompute<MetalApi>("MetalApi_Compute.png");
#endif
  }

TEST(MetalApi,MipMaps) {
#if defined(_MSC_VER)
  GapiTestCommon::mipMaps<MetalApi,TextureFormat::RGBA8>  ("MetalApi_MipMaps_RGBA8.png");
  GapiTestCommon::mipMaps<MetalApi,TextureFormat::RGBA16> ("MetalApi_MipMaps_RGBA16.png");
  GapiTestCommon::mipMaps<MetalApi,TextureFormat::RGBA32F>("MetalApi_MipMaps_RGBA32.png");
#endif
  }

TEST(MetalApi,S3TC) {
#if defined(_MSC_VER)
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

TEST(MetalApi,DISABLED_TesselationBasic) {
#if defined(_MSC_VER)
  GapiTestCommon::psoTess<MetalApi>();
#endif
  }

TEST(MetalApi,SsboWrite) {
#if defined(_MSC_VER)
  GapiTestCommon::ssboWriteVs<MetalApi>();
#endif
  }

TEST(MetalApi,PushRemapping) {
#if defined(_MSC_VER)
  GapiTestCommon::pushConstant<MetalApi>();
#endif
  }

TEST(MetalApi,SpirvDefect) {
#if defined(_MSC_VER)
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
