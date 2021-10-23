#include <Tempest/DirectX12Api>
#include <Tempest/Except>
#include <Tempest/Device>
#include <Tempest/Fence>
#include <Tempest/Pixmap>
#include <Tempest/Log>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include "gapi_test_common.h"
#include "../utils/renderdoc.h"

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

TEST(DirectX12Api,VboInit) {
#if defined(_MSC_VER)
  GapiTestCommon::vboInit<DirectX12Api>();
#endif
  }

TEST(DirectX12Api,VboDyn) {
#if defined(_MSC_VER)
  GapiTestCommon::vboDyn<DirectX12Api>();
#endif
  }

TEST(DirectX12Api,SsboDyn) {
#if defined(_MSC_VER)
  GapiTestCommon::ssboDyn<DirectX12Api,float>();
#endif
  }

TEST(DirectX12Api,SsboCopy) {
#if defined(_MSC_VER)
  RenderDoc::start();
  // TODO: test more formats
  GapiTestCommon::bufCopy<DirectX12Api,TextureFormat::R8,  uint8_t> ();
  GapiTestCommon::bufCopy<DirectX12Api,TextureFormat::R16, uint16_t>();
  //GapiTestCommon::bufCopy<DirectX12Api,TextureFormat::R32F,float>   ();

  GapiTestCommon::bufCopy<DirectX12Api,TextureFormat::RG8,  uint8_t >();
  GapiTestCommon::bufCopy<DirectX12Api,TextureFormat::RG16, uint16_t>();
  //GapiTestCommon::bufCopy<DirectX12Api,TextureFormat::RG32F,float>   ();

  GapiTestCommon::bufCopy<DirectX12Api,TextureFormat::RGBA8, uint8_t> ();
  //GapiTestCommon::bufCopy<DirectX12Api,TextureFormat::RGBA16,uint16_t>();
  RenderDoc::stop();
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
  GapiTestCommon::draw<DirectX12Api,TextureFormat::RGBA32F>("DirectX12Api_Draw_RGBA32F.hdr");
  GapiTestCommon::draw<DirectX12Api,TextureFormat::RG32F>  ("DirectX12Api_Draw_RG32F.hdr");
  GapiTestCommon::draw<DirectX12Api,TextureFormat::R32F>   ("DirectX12Api_Draw_R32F.hdr");
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
  GapiTestCommon::S3TC<DirectX12Api>("DirectX12Api_S3TC.png");
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

TEST(DirectX12Api,Spirv_DS) {
#if defined(_MSC_VER)
  using namespace Tempest;

  try {
    DirectX12Api api{ApiFlags::Validation};
    Device       device(api);

    auto tese0 = device.loadShader("shader/tess.tese.sprv");
    auto tese1 = device.loadShader("shader/spirv_ds_01.tese.sprv");
    auto tese2 = device.loadShader("shader/spirv_ds_02.tese.sprv");
    auto tese3 = device.loadShader("shader/spirv_ds_03.tese.sprv");
    auto tese4 = device.loadShader("shader/spirv_ds_quad.tese.sprv");
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
#endif
  }

TEST(DirectX12Api,SpirvDefect_Link) {
#if defined(_MSC_VER)
  using namespace Tempest;

  try {
    DirectX12Api api{ApiFlags::Validation};
    Device       device(api);

    auto vbo  = device.vbo(GapiTestCommon::vboData,3);
    auto ibo  = device.ibo(GapiTestCommon::iboData,3);

    auto vert = device.loadShader("shader/link_defect.vert.sprv");
    auto frag = device.loadShader("shader/link_defect.frag.sprv");
    auto pso  = device.pipeline<GapiTestCommon::Vertex>(Topology::Triangles,RenderState(),vert,frag);

    auto tex  = device.attachment(Tempest::TextureFormat::RGBA8,128,128);
    auto cmd  = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer({{tex,Vec4(0,0,1,1),Tempest::Preserve}});
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

TEST(DirectX12Api,SpirvDefect_Loop) {
#if defined(_MSC_VER)
  using namespace Tempest;

  try {
    DirectX12Api api{ApiFlags::Validation};
    Device       device(api);

    auto pso  = device.pipeline(device.loadShader("shader/loop_defect.comp.sprv"));
    (void)pso;
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
#endif
  }
