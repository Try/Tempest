#include <Tempest/DirectX12Api>
#include <Tempest/Except>
#include <Tempest/Device>
#include <Tempest/Fence>
#include <Tempest/Pixmap>
#include <Tempest/Log>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include "gapi_test_common.h"
#include "gapi_test_sync.h"

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
  GapiTestCommon::Vbo<DirectX12Api>();
#endif
  }

TEST(DirectX12Api,VboInit) {
#if defined(_MSC_VER)
  GapiTestCommon::VboInit<DirectX12Api>();
#endif
  }

TEST(DirectX12Api,VboDyn) {
#if defined(_MSC_VER)
  GapiTestCommon::VboDyn<DirectX12Api>();
#endif
  }

TEST(DirectX12Api,SsboDyn) {
#if defined(_MSC_VER)
  GapiTestCommon::SsboDyn<DirectX12Api,float>();
#endif
  }

TEST(DirectX12Api,SsboEmpty) {
#if defined(_MSC_VER)
  GapiTestCommon::SsboEmpty<DirectX12Api>();
#endif
  }

TEST(DirectX12Api,SsboCopy) {
#if defined(_MSC_VER)
  // TODO: test more formats
  GapiTestCommon::SsboCopy<DirectX12Api,TextureFormat::R8,  uint8_t> ();
  GapiTestCommon::SsboCopy<DirectX12Api,TextureFormat::R16, uint16_t>();
  //GapiTestCommon::SsboCopy<DirectX12Api,TextureFormat::R32F,float>   ();

  GapiTestCommon::SsboCopy<DirectX12Api,TextureFormat::RG8,  uint8_t >();
  GapiTestCommon::SsboCopy<DirectX12Api,TextureFormat::RG16, uint16_t>();
  //GapiTestCommon::SsboCopy<DirectX12Api,TextureFormat::RG32F,float>   ();

  GapiTestCommon::SsboCopy<DirectX12Api,TextureFormat::RGBA8, uint8_t> ();
  //GapiTestCommon::SsboCopy<DirectX12Api,TextureFormat::RGBA16,uint16_t>();
#endif
  }

TEST(DirectX12Api,ArrayLength) {
#if defined(_MSC_VER)
  GapiTestCommon::ArrayLength<DirectX12Api>();
#endif
  }

TEST(DirectX12Api,NonSampledTexture) {
#if defined(_MSC_VER)
  // NOTE[textureLod]: "Sampling non-float textures is not supported in HLSL SM < 6.7."
  GapiTestCommon::NonSampledTexture<DirectX12Api>("DirectX12Api_NonSampledTexture.png");
#endif
  }

TEST(DirectX12Api,Shader) {
#if defined(_MSC_VER)
  GapiTestCommon::Shader<DirectX12Api>();
#endif
  }

TEST(DirectX12Api,Pso) {
#if defined(_MSC_VER)
  GapiTestCommon::Pso<DirectX12Api>();
#endif
  }

TEST(DirectX12Api,DISABLED_PsoInconsistentVaryings) {
#if defined(_MSC_VER)
  GapiTestCommon::PsoInconsistentVaryings<DirectX12Api>();
#endif
  }

TEST(DirectX12Api,Fbo) {
#if defined(_MSC_VER)
  GapiTestCommon::Fbo<DirectX12Api>("DirectX12Api_Fbo.png");
#endif
  }

TEST(DirectX12Api,Draw) {
#if defined(_MSC_VER)
  GapiTestCommon::Draw<DirectX12Api,TextureFormat::RGBA8>  ("DirectX12Api_Draw_RGBA8.png");
  GapiTestCommon::Draw<DirectX12Api,TextureFormat::RGB8>   ("DirectX12Api_Draw_RGB8.png");
  GapiTestCommon::Draw<DirectX12Api,TextureFormat::RG8>    ("DirectX12Api_Draw_RG8.png");
  GapiTestCommon::Draw<DirectX12Api,TextureFormat::R8>     ("DirectX12Api_Draw_R8.png");
  GapiTestCommon::Draw<DirectX12Api,TextureFormat::RGBA16> ("DirectX12Api_Draw_RGBA16.png");
  GapiTestCommon::Draw<DirectX12Api,TextureFormat::RGB16>  ("DirectX12Api_Draw_RGB16.png");
  GapiTestCommon::Draw<DirectX12Api,TextureFormat::RG16>   ("DirectX12Api_Draw_RG16.png");
  GapiTestCommon::Draw<DirectX12Api,TextureFormat::R16>    ("DirectX12Api_Draw_R16.png");
  GapiTestCommon::Draw<DirectX12Api,TextureFormat::RGBA32F>("DirectX12Api_Draw_RGBA32F.hdr");
  GapiTestCommon::Draw<DirectX12Api,TextureFormat::RGB32F> ("DirectX12Api_Draw_RGB32F.hdr");
  GapiTestCommon::Draw<DirectX12Api,TextureFormat::RG32F>  ("DirectX12Api_Draw_RG32F.hdr");
  GapiTestCommon::Draw<DirectX12Api,TextureFormat::R32F>   ("DirectX12Api_Draw_R32F.hdr");
#endif
  }

TEST(DirectX12Api,DepthWrite) {
#if defined(_MSC_VER)
  GapiTestCommon::DepthWrite<DirectX12Api>("DirectX12Api_DepthWrite.png");
#endif
  }

TEST(DirectX12Api,InstanceIndex) {
#if defined(_MSC_VER)
  GapiTestCommon::InstanceIndex<DirectX12Api>("DirectX12Api_InstanceIndex.png");
#endif
  }

TEST(DirectX12Api,Viewport) {
#if defined(_MSC_VER)
  GapiTestCommon::Viewport<DirectX12Api>("DirectX12Api_Viewport.png");
#endif
  }

TEST(DirectX12Api,Uniforms) {
#if defined(_MSC_VER)
  GapiTestCommon::Uniforms<DirectX12Api>("DirectX12Api_Uniforms_UBO.png", true);
  GapiTestCommon::Uniforms<DirectX12Api>("DirectX12Api_Uniforms_SSBO.png",false);
#endif
  }

TEST(DirectX12Api,Compute) {
#if defined(_MSC_VER)
  GapiTestCommon::Compute<DirectX12Api>();
#endif
  }

TEST(DirectX12Api,ComputeImage) {
#if defined(_MSC_VER)
  GapiTestCommon::ComputeImage<DirectX12Api>("DirectX12Api_ComputeImage.png");
#endif
  }

TEST(DirectX12Api,ComputeImage3dAs2d) {
#if defined(_MSC_VER)
  GapiTestCommon::ComputeImage3dAs2d<DirectX12Api>("DirectX12Api_ComputeImage3dAs2d.png");
#endif
  }

TEST(DirectX12Api,AtomicImage) {
#if defined(_MSC_VER)
  GapiTestCommon::AtomicImage<DirectX12Api>("DirectX12Api_AtomicImage.png");
#endif
  }

TEST(DirectX12Api,AtomicImage3D) {
#if defined(_MSC_VER)
  GapiTestCommon::AtomicImage3D<DirectX12Api>("DirectX12Api_AtomicImage3D.png");
#endif
  }

TEST(DirectX12Api,DispathToDraw) {
#if defined(_MSC_VER)
  GapiTestSync::DispathToDraw<DirectX12Api>("DirectX12Api_DispathToDraw.png");
  GapiTestSync::DrawToDispath<DirectX12Api>();
#endif
  }


TEST(DirectX12Api,MipMaps) {
#if defined(_MSC_VER)
  GapiTestCommon::MipMaps<DirectX12Api,TextureFormat::RGBA8>  ("DirectX12Api_MipMaps_RGBA8.png");
  GapiTestCommon::MipMaps<DirectX12Api,TextureFormat::RGBA16> ("DirectX12Api_MipMaps_RGBA16.png");
  GapiTestCommon::MipMaps<DirectX12Api,TextureFormat::RGBA32F>("DirectX12Api_MipMaps_RGBA32.hdr");
#endif
  }

TEST(DirectX12Api,S3TC) {
#if defined(_MSC_VER)
  GapiTestCommon::S3TC<DirectX12Api>("DirectX12Api_S3TC.png");
#endif
  }

TEST(DirectX12Api,SsboWrite) {
#if defined(_MSC_VER)
  GapiTestCommon::SsboWrite<DirectX12Api>();
#endif
  }

TEST(DirectX12Api,SSBOReadOnly) {
#if defined(_MSC_VER)
  GapiTestCommon::SSBOReadOnly<DirectX12Api>(true);
  GapiTestCommon::SSBOReadOnly<DirectX12Api>(false);
#endif
  }

TEST(DirectX12Api,UnboundSsbo) {
#if defined(_MSC_VER)
  GapiTestCommon::UnboundSsbo<DirectX12Api>();
#endif
  }

TEST(DirectX12Api,ComponentSwizzle) {
#if defined(_MSC_VER)
  GapiTestCommon::ComponentSwizzle<DirectX12Api>();
#endif
  }

TEST(DirectX12Api,PushRemapping) {
#if defined(_MSC_VER)
  GapiTestCommon::PushRemapping<DirectX12Api>();
#endif
  }

TEST(DirectX12Api,PushRemappingGr) {
#if defined(_MSC_VER)
  GapiTestCommon::PushRemappingGr<DirectX12Api>("DirectX12Api_PushRemappingGr.png");
#endif
  }

TEST(DirectX12Api,ArrayedTextures) {
#if defined(_MSC_VER)
  GapiTestCommon::ArrayedTextures<DirectX12Api>("DirectX12Api_ArrayedTextures.png");
#endif
  }

TEST(DirectX12Api,ArrayedImages) {
#if defined(_MSC_VER)
  GapiTestCommon::ArrayedImages<DirectX12Api>("DirectX12Api_ArrayedImages.png");
#endif
  }

TEST(DirectX12Api,ArrayedSsbo) {
#if defined(_MSC_VER)
  GapiTestCommon::ArrayedSsbo<DirectX12Api>("DirectX12Api_ArrayedSsbo.png");
#endif
  }

TEST(DirectX12Api,Bindless) {
#if defined(_MSC_VER)
  GapiTestCommon::Bindless<DirectX12Api>("DirectX12Api_Bindless.png");
#endif
  }

TEST(DirectX12Api,Bindless2) {
#if defined(_MSC_VER)
  GapiTestCommon::Bindless2<DirectX12Api>("DirectX12Api_Bindless2.png");
#endif
  }

TEST(DirectX12Api,UnusedDescriptor) {
#if defined(_MSC_VER)
  GapiTestCommon::UnusedDescriptor<DirectX12Api>("DirectX12Api_UnusedDescriptor.png");
#endif
  }

TEST(DirectX12Api,Blas) {
#if defined(_MSC_VER)
  GapiTestCommon::Blas<DirectX12Api>();
#endif
  }

TEST(DirectX12Api,TlasEmpty) {
#if defined(_MSC_VER)
  GapiTestCommon::TlasEmpty<DirectX12Api>();
#endif
  }

TEST(DirectX12Api,RayQuery) {
#if defined(_MSC_VER)
  GapiTestCommon::RayQuery<DirectX12Api>("DirectX12Api_RayQuery.png");
#endif
  }

TEST(DirectX12Api,RayQueryFace) {
#if defined(_MSC_VER)
  GapiTestCommon::RayQueryFace<DirectX12Api>("DirectX12Api_RayQueryFace.png");
#endif
  }

TEST(DirectX12Api,MeshShader) {
#if defined(_MSC_VER)
  GapiTestCommon::MeshShader<DirectX12Api>("DirectX12Api_MeshShader.png");
#endif
  }

TEST(DirectX12Api,DispathIndirect) {
#if defined(_MSC_VER)
  GapiTestCommon::DispathIndirect<DirectX12Api>();
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

    auto vert = device.shader("shader/link_defect.vert.sprv");
    auto frag = device.shader("shader/link_defect.frag.sprv");
    auto pso  = device.pipeline(Topology::Triangles,RenderState(),vert,frag);

    auto tex  = device.attachment(Tempest::TextureFormat::RGBA8,128,128);
    auto cmd  = device.commandBuffer();
    {
      auto enc = cmd.startEncoding(device);
      enc.setFramebuffer({{tex,Vec4(0,0,1,1),Tempest::Preserve}});
      enc.setPipeline(pso);
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

TEST(DirectX12Api,DISABLED_SpirvDefect_Loop) {
#if defined(_MSC_VER)
  using namespace Tempest;

  try {
    DirectX12Api api{ApiFlags::Validation};
    Device       device(api);

    auto pso  = device.pipeline(device.shader("shader/loop_defect.comp.sprv"));
    (void)pso;
    }
  catch(std::system_error& e) {
    if(e.code()==Tempest::GraphicsErrc::NoDevice)
      Log::d("Skipping graphics testcase: ", e.what()); else
      throw;
    }
#endif
  }
