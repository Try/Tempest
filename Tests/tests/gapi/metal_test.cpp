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

TEST(MetalApi,SsboEmpty) {
#if defined(__OSX__)
  GapiTestCommon::SsboEmpty<MetalApi>();
#endif
  }

TEST(MetalApi,ArrayLength) {
#if defined(__OSX__)
  GapiTestCommon::ArrayLength<MetalApi>();
#endif
  }

TEST(MetalApi,NonSampledTexture) {
#if defined(__OSX__)
  GapiTestCommon::NonSampledTexture<MetalApi>("MetalApi_NonSampledTexture.png");
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

TEST(MetalApi,PsoInconsistentVaryings) {
#if defined(__OSX__)
  GapiTestCommon::PsoInconsistentVaryings<MetalApi>();
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

TEST(MetalApi,DepthWrite) {
#if defined(__OSX__)
  GapiTestCommon::DepthWrite<MetalApi>("MetalApi_DepthWrite.png");
#endif
  }

TEST(MetalApi,InstanceIndex) {
#if defined(__OSX__)
  GapiTestCommon::InstanceIndex<MetalApi>("MetalApi_InstanceIndex.png");
#endif
  }

TEST(MetalApi,Viewport) {
#if defined(__OSX__)
  GapiTestCommon::Viewport<MetalApi>("MetalApi_Viewport.png");
#endif
  }

TEST(MetalApi,Uniforms) {
#if defined(__OSX__)
  GapiTestCommon::Uniforms<MetalApi>("MetalApi_Uniforms_UBO.png", true);
  GapiTestCommon::Uniforms<MetalApi>("MetalApi_Uniforms_SSBO.png",false);
#endif
  }

TEST(MetalApi,SsboOverlap) {
#if defined(__OSX__)
  GapiTestCommon::SsboOverlap<MetalApi>();
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

TEST(MetalApi,AtomicImage) {
#if defined(__OSX__)
  GapiTestCommon::AtomicImage<MetalApi>("MetalApi_AtomicImage.png");
#endif
  }

TEST(MetalApi,AtomicImage3D) {
#if defined(__OSX__)
  GapiTestCommon::AtomicImage3D<MetalApi>("MetalApi_AtomicImage3D.png");
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

TEST(MetalApi,PsoTess) {
#if defined(__OSX__)
  GapiTestCommon::PsoTess<MetalApi>();
#endif
  }

TEST(MetalApi,DISABLED_TesselationBasic) {
#if defined(__OSX__)
  GapiTestCommon::TesselationBasic<MetalApi>("MetalApi_TesselationBasic.png");
#endif
  }

TEST(MetalApi,SsboWrite) {
#if defined(__OSX__)
  GapiTestCommon::SsboWrite<MetalApi>();
#endif
  }

TEST(MetalApi,UnboundSsbo) {
#if defined(__OSX__)
  GapiTestCommon::UnboundSsbo<MetalApi>();
#endif
  }

TEST(MetalApi,ComponentSwizzle) {
#if defined(__OSX__)
  GapiTestCommon::ComponentSwizzle<MetalApi>();
#endif
  }

TEST(MetalApi,PushRemapping) {
#if defined(__OSX__)
  GapiTestCommon::PushRemapping<MetalApi>();
#endif
  }

TEST(MetalApi,Bindless) {
#if defined(__OSX__)
  GapiTestCommon::Bindless<MetalApi>("MetalApi_Bindless.png");
#endif
  }

TEST(MetalApi,Bindless2) {
#if defined(__OSX__)
  GapiTestCommon::Bindless2<MetalApi>("MetalApi_Bindless2.png");
#endif
  }

TEST(MetalApi,UnusedDescriptor) {
#if defined(__OSX__)
  GapiTestCommon::UnusedDescriptor<MetalApi>("MetalApi_UnusedDescriptor.png");
#endif
  }

TEST(MetalApi,Blas) {
#if defined(__OSX__)
  GapiTestCommon::Blas<MetalApi>();
#endif
  }

TEST(MetalApi,RayQuery) {
#if defined(__OSX__)
  GapiTestCommon::RayQuery<MetalApi>("MetalApi_RayQuery.png");
#endif
  }

TEST(MetalApi,DISABLED_RayQueryFace) {
#if defined(__OSX__)
  GapiTestCommon::RayQueryFace<MetalApi>("MetalApi_RayQueryFace.png");
#endif
  }

TEST(MetalApi,DISABLED_MeshShader) {
#if defined(__OSX__)
  GapiTestCommon::MeshShader<MetalApi>("MetalApi_MeshShader.png");
#endif
  }

TEST(MetalApi,DispathIndirect) {
#if defined(__OSX__)
  GapiTestCommon::DispathIndirect<MetalApi>();
#endif
  }
