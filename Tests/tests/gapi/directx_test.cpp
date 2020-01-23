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
    DirectX12Api   api{ApiFlags::Validation};
    Device         device(api);

    auto vbo = device.loadVbo(vboData,3,BufferFlags::Static);
    auto ibo = device.loadIbo(iboData,3,BufferFlags::Static);
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping vulkan testcase: ", e.what()); else
      throw;
    }
#endif
  }
