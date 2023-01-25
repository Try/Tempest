#include <Tempest/VulkanApi>
#include <Tempest/Except>
#include <Tempest/Device>
#include <Tempest/Fence>
#include <Tempest/Pixmap>
#include <Tempest/Log>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include "gapi_test_common.h"
#include "gapi_test_sync.h"

using namespace testing;
using namespace Tempest;

TEST(VulkanApi,VulkanApi) {
#if !defined(__OSX__)
  GapiTestCommon::init<VulkanApi>();
#endif
  }

TEST(VulkanApi,Vbo) {
#if !defined(__OSX__)
  GapiTestCommon::Vbo<VulkanApi>();
#endif
  }

TEST(VulkanApi,VboInit) {
#if !defined(__OSX__)
  GapiTestCommon::VboInit<VulkanApi>();
#endif
  }

TEST(VulkanApi,VboDyn) {
#if !defined(__OSX__)
  GapiTestCommon::VboDyn<VulkanApi>();
#endif
  }

TEST(VulkanApi,SsboDyn) {
#if !defined(__OSX__)
  GapiTestCommon::SsboDyn<VulkanApi,float>();
#endif
  }

TEST(VulkanApi,SsboCopy) {
#if !defined(__OSX__)
  GapiTestCommon::SsboCopy<VulkanApi,TextureFormat::RGBA8,uint8_t>();
#endif
  }

TEST(VulkanApi,Shader) {
#if !defined(__OSX__)
  GapiTestCommon::Shader<VulkanApi>();
#endif
  }

TEST(VulkanApi,Pso) {
#if !defined(__OSX__)
  GapiTestCommon::Pso<VulkanApi>();
#endif
  }

TEST(VulkanApi,PsoInconsistentVaryings) {
#if !defined(__OSX__)
  GapiTestCommon::PsoInconsistentVaryings<VulkanApi>();
#endif
  }

TEST(VulkanApi,Fbo) {
#if !defined(__OSX__)
  GapiTestCommon::Fbo<VulkanApi>("VulkanApi_Fbo.png");
#endif
  }

TEST(VulkanApi,Draw) {
#if !defined(__OSX__)
  GapiTestCommon::Draw<VulkanApi,TextureFormat::RGBA8>  ("VulkanApi_Draw_RGBA8.png");
  GapiTestCommon::Draw<VulkanApi,TextureFormat::RGB8>   ("VulkanApi_Draw_RGB8.png");
  GapiTestCommon::Draw<VulkanApi,TextureFormat::RG8>    ("VulkanApi_Draw_RG8.png");
  GapiTestCommon::Draw<VulkanApi,TextureFormat::R8>     ("VulkanApi_Draw_R8.png");
  GapiTestCommon::Draw<VulkanApi,TextureFormat::RGBA16> ("VulkanApi_Draw_RGBA16.png");
  GapiTestCommon::Draw<VulkanApi,TextureFormat::RGB16>  ("VulkanApi_Draw_RGB16.png");
  GapiTestCommon::Draw<VulkanApi,TextureFormat::RG16>   ("VulkanApi_Draw_RG16.png");
  GapiTestCommon::Draw<VulkanApi,TextureFormat::R16>    ("VulkanApi_Draw_R16.png");
  GapiTestCommon::Draw<VulkanApi,TextureFormat::RGBA32F>("VulkanApi_Draw_RGBA32F.hdr");
  GapiTestCommon::Draw<VulkanApi,TextureFormat::RGB32F> ("VulkanApi_Draw_RGB32F.hdr");
  GapiTestCommon::Draw<VulkanApi,TextureFormat::RG32F>  ("VulkanApi_Draw_RG32F.hdr");
  GapiTestCommon::Draw<VulkanApi,TextureFormat::R32F>   ("VulkanApi_Draw_R32F.hdr");
#endif
  }

TEST(VulkanApi,InstanceIndex) {
#if !defined(__OSX__)
  GapiTestCommon::InstanceIndex<VulkanApi>("VulkanApi_InstanceIndex.png");
#endif
  }

TEST(VulkanApi,Viewport) {
#if !defined(__OSX__)
  GapiTestCommon::Viewport<VulkanApi>("VulkanApi_Viewport.png");
#endif
  }

TEST(VulkanApi,Uniforms) {
#if !defined(__OSX__)
  GapiTestCommon::Uniforms<VulkanApi>("VulkanApi_Uniforms_UBO.png", true);
  GapiTestCommon::Uniforms<VulkanApi>("VulkanApi_Uniforms_SSBO.png",false);
#endif
  }

TEST(VulkanApi,SSBOReadOnly) {
#if !defined(__OSX__)
  GapiTestCommon::SSBOReadOnly<VulkanApi>(true);
  GapiTestCommon::SSBOReadOnly<VulkanApi>(false);
#endif
  }

TEST(VulkanApi,UnboundSsbo) {
#if !defined(__OSX__)
  GapiTestCommon::UnboundSsbo<VulkanApi>();
#endif
  }

TEST(VulkanApi,SbboOverlap) {
#if !defined(__OSX__)
  GapiTestCommon::SbboOverlap<VulkanApi>();
#endif
  }

TEST(VulkanApi,Compute) {
#if !defined(__OSX__)
  GapiTestCommon::Compute<VulkanApi>();
#endif
  }

TEST(VulkanApi,ComputeImage) {
#if !defined(__OSX__)
  GapiTestCommon::ComputeImage<VulkanApi>("VulkanApi_ComputeImage.png");
#endif
  }

TEST(VulkanApi,DispathToDraw) {
#if !defined(__OSX__)
  GapiTestSync::DispathToDraw<VulkanApi>("VulkanApi_DispathToDraw.png");
  GapiTestSync::DrawToDispath<VulkanApi>();
#endif
  }

TEST(VulkanApi,MipMaps) {
#if !defined(__OSX__)
  GapiTestCommon::MipMaps<VulkanApi,TextureFormat::RGBA8>  ("VulkanApi_MipMaps_RGBA8.png");
  GapiTestCommon::MipMaps<VulkanApi,TextureFormat::RGBA16> ("VulkanApi_MipMaps_RGBA16.png");
  GapiTestCommon::MipMaps<VulkanApi,TextureFormat::RGBA32F>("VulkanApi_MipMaps_RGBA32.hdr");
#endif
  }

TEST(VulkanApi,S3TC) {
#if !defined(__OSX__)
  GapiTestCommon::S3TC<VulkanApi>("VulkanApi_S3TC.png");
#endif
  }

TEST(VulkanApi,PsoTess) {
#if !defined(__OSX__)
  GapiTestCommon::PsoTess<VulkanApi>();
#endif
  }

TEST(VulkanApi,TesselationBasic) {
#if !defined(__OSX__)
  GapiTestCommon::TesselationBasic<VulkanApi>("VulkanApi_TesselationBasic.png");
#endif
  }

TEST(VulkanApi,GeomBasic) {
#if !defined(__OSX__)
  GapiTestCommon::GeomBasic<VulkanApi>("VulkanApi_GeomBasic.png");
#endif
  }

TEST(VulkanApi,SsboWrite) {
#if !defined(__OSX__)
  GapiTestCommon::SsboWrite<VulkanApi>();
#endif
  }

TEST(VulkanApi,ComponentSwizzle) {
#if !defined(__OSX__)
  GapiTestCommon::ComponentSwizzle<VulkanApi>();
#endif
  }

TEST(VulkanApi,PushRemapping) {
#if !defined(__OSX__)
  GapiTestCommon::PushRemapping<VulkanApi>();
#endif
  }

TEST(VulkanApi,ArrayedTextures) {
#if !defined(__OSX__)
  GapiTestCommon::ArrayedTextures<VulkanApi>("VulkanApi_ArrayedTextures.png");
#endif
  }

TEST(VulkanApi,ArrayedImages) {
#if !defined(__OSX__)
  GapiTestCommon::ArrayedImages<VulkanApi>("VulkanApi_ArrayedImages.png");
#endif
  }

TEST(VulkanApi,ArrayedSsbo) {
#if !defined(__OSX__)
  GapiTestCommon::ArrayedSsbo<VulkanApi>("VulkanApi_ArrayedSsbo.png");
#endif
  }

TEST(VulkanApi,Bindless) {
#if !defined(__OSX__)
  GapiTestCommon::Bindless<VulkanApi>("VulkanApi_Bindless.png");
#endif
  }

TEST(VulkanApi,Bindless2) {
#if !defined(__OSX__)
  GapiTestCommon::Bindless2<VulkanApi>("VulkanApi_Bindless2.png");
#endif
  }

TEST(VulkanApi,Blas) {
#if !defined(__OSX__)
  GapiTestCommon::Blas<VulkanApi>();
#endif
  }

TEST(VulkanApi,TlasEmpty) {
#if !defined(__OSX__)
  GapiTestCommon::TlasEmpty<VulkanApi>();
#endif
  }

TEST(VulkanApi,RayQuery) {
#if !defined(__OSX__)
  GapiTestCommon::RayQuery<VulkanApi>("VulkanApi_RayQuery.png");
#endif
  }

TEST(VulkanApi,RayQueryFace) {
#if !defined(__OSX__)
  GapiTestCommon::RayQueryFace<VulkanApi>("VulkanApi_RayQueryFace.png");
#endif
  }

TEST(VulkanApi,MeshShader) {
#if !defined(__OSX__)
  GapiTestCommon::MeshShader<VulkanApi>("VulkanApi_MeshShader.png");
#endif
  }

TEST(VulkanApi,MeshShaderEmulated) {
#if !defined(__OSX__)
  GapiTestCommon::MeshShaderEmulated<VulkanApi>("VulkanApi_MeshShaderEmulated.png");
#endif
  }

TEST(VulkanApi,DISABLED_MeshComputePrototype) {
#if !defined(__OSX__)
  GapiTestCommon::MeshComputePrototype<VulkanApi>("VulkanApi_MeshComputePrototype.png");
#endif
  }
