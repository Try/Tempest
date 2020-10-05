#include <Tempest/DirectX12Api>
#include <Tempest/Except>
#include <Tempest/Device>
#include <Tempest/Fence>
#include <Tempest/Pixmap>
#include <Tempest/Log>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

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
  try {
    DirectX12Api api{ApiFlags::Validation};
    (void)api;
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping directx testcase: ", e.what()); else
      throw;
    }
#endif
  }

TEST(DirectX12Api,Vbo) {
#if defined(_MSC_VER)
  try {
    DirectX12Api api{ApiFlags::Validation};
    Device       device(api);

    auto vbo = device.vbo(vboData,3);
    auto ibo = device.ibo(iboData,3);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping directx testcase: ", e.what()); else
      throw;
    }
#endif
  }


TEST(DirectX12Api,Shader) {
#if defined(_MSC_VER)
  try {
    DirectX12Api api{ApiFlags::Validation};
    Device       device(api);

    auto vert = device.loadShader("shader/simple_test.vert.sprv");
    auto frag = device.loadShader("shader/simple_test.frag.sprv");
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping directx testcase: ", e.what()); else
      throw;
    }
#endif
  }

TEST(DirectX12Api,Pso) {
#if defined(_MSC_VER)
  try {
    DirectX12Api api{ApiFlags::Validation};
    Device       device(api);

    auto vert = device.loadShader("shader/simple_test.vert.sprv");
    auto frag = device.loadShader("shader/simple_test.frag.sprv");
    auto pso  = device.pipeline<Vertex>(Topology::Triangles,RenderState(),vert,frag);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping directx testcase: ", e.what()); else
      throw;
    }
#endif
  }

TEST(DirectX12Api,Fbo) {
#if defined(_MSC_VER)
  try {
    DirectX12Api api{ApiFlags::Validation};
    Device       device(api);

    auto tex = device.attachment(TextureFormat::RGBA8,128,128);
    auto fbo = device.frameBuffer(tex);
    auto rp  = device.pass(FboMode(FboMode::PreserveOut,Color(0.f,0.f,1.f)));

    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer(fbo,rp);
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    auto pm = device.readPixels(tex);
    pm.save("DirectX12Api_Fbo.png");
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping directx testcase: ", e.what()); else
      throw;
    }
#endif
  }

TEST(DirectX12Api,Draw) {
#if defined(_MSC_VER)
  try {
    DirectX12Api api{ApiFlags::Validation};
    Device       device(api);

    auto vbo  = device.vbo(vboData,3);
    auto ibo  = device.ibo(iboData,3);

    auto vert = device.loadShader("shader/simple_test.vert.sprv");
    auto frag = device.loadShader("shader/simple_test.frag.sprv");
    auto pso  = device.pipeline<Vertex>(Topology::Triangles,RenderState(),vert,frag);

    auto tex  = device.attachment(TextureFormat::RGBA8,128,128);
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

    auto pm = device.readPixels(tex);
    pm.save("DirectX12Api_Draw.png");
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping directx testcase: ", e.what()); else
      throw;
    }
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

TEST(DirectX12Api,Compute) {
#if defined(_MSC_VER)
  try {
    DirectX12Api api{ApiFlags::Validation};
    Device       device(api);

    Vec4 inputCpu[3] = {Vec4(0,1,2,3),Vec4(4,5,6,7),Vec4(8,9,10,11)};

    auto input  = device.ssbo<Tempest::Vec4>(inputCpu,3);
    auto output = device.ssbo<Tempest::Vec4>(nullptr, 3);

    auto cs  = device.loadShader("shader/simple_test.comp.sprv");
    auto pso = device.pipeline(cs);

    auto ubo = device.uniforms(pso.layout());
    ubo.set(0,input);
    ubo.set(1,output);

    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setUniforms(pso,ubo);
      enc.dispatch(3,1,1);
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    /*
    Vec4 outputCpu[3] = {};
    output.readback(outputCpu,0,3);
    for(size_t i=0; i<3; ++i)
      EXPECT_EQ(outputCpu[i],inputCpu[i]);
    */
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping vulkan testcase: ", e.what()); else
      throw;
    }
#endif
  }
