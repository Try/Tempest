#include <Tempest/VulkanApi>
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

TEST(VulkanApi,VulkanApi) {
  try {
    VulkanApi api;
    (void)api;
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping vulkan testcase: ", e.what()); else
      throw;
    }
  }

TEST(VulkanApi,Vbo) {
  try {
    VulkanApi api{ApiFlags::Validation};
    Device    device(api);

    auto vbo = device.vbo(vboData,3);
    auto ibo = device.ibo(iboData,3);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping vulkan testcase: ", e.what()); else
      throw;
    }
  }

TEST(VulkanApi,Shader) {
  try {
    VulkanApi api{ApiFlags::Validation};
    Device    device(api);

    auto vert = device.loadShader("shader/simple_test.vert.sprv");
    auto frag = device.loadShader("shader/simple_test.frag.sprv");
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping vulkan testcase: ", e.what()); else
      throw;
    }
  }

TEST(VulkanApi,Fbo) {
  try {
    VulkanApi api{ApiFlags::Validation};
    Device    device(api);

    auto tex = device.attachment(TextureFormat::RGBA8,128,128);
    auto fbo = device.frameBuffer(tex);
    auto rp  = device.pass(FboMode(FboMode::PreserveOut,Color(0.f,0.f,1.f)));

    auto cmd = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setPass(fbo,rp);
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    auto pm = device.readPixels(tex);
    pm.save("VulkanApi_Fbo.png");
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping vulkan testcase: ", e.what()); else
      throw;
    }
  }

TEST(VulkanApi,Draw) {
  try {
    VulkanApi api{ApiFlags::Validation};
    Device    device(api);

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
      enc.setPass(fbo,rp);
      enc.setUniforms(pso);
      enc.draw(vbo,ibo);
    }

    auto sync = device.fence();
    device.submit(cmd,sync);
    sync.wait();

    auto pm = device.readPixels(tex);
    pm.save("VulkanApi_Draw.png");
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping vulkan testcase: ", e.what()); else
      throw;
    }
  }
