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

struct Vertex {
  float x,y;
  };

namespace Tempest {
template<>
inline VertexBufferDecl vertexBufferDecl<::Vertex>() {
  return {Decl::float2};
  }
}

static const Vertex   vboData[3] = {{-1,-1},{1,-1},{1,1}};
static const uint16_t iboData[3] = {0,1,2};

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
  GapiTestCommon::draw<DirectX12Api>("DirectX12Api_Draw.png");
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

TEST(DirectX12Api,MipMaps) {
#if defined(_MSC_VER)
  using namespace Tempest;

  try {
    DirectX12Api api{ApiFlags::Validation};
    Device       device(api);

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
#endif
  }
