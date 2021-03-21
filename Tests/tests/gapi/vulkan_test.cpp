#include <Tempest/VulkanApi>
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


TEST(VulkanApi,VulkanApi) {
  GapiTestCommon::init<VulkanApi>();
  }

TEST(VulkanApi,Vbo) {
  GapiTestCommon::vbo<VulkanApi>();
  }

TEST(VulkanApi,Shader) {
  GapiTestCommon::shader<VulkanApi>();
  }

TEST(VulkanApi,Pso) {
  GapiTestCommon::pso<VulkanApi>();
  }

TEST(VulkanApi,Fbo) {
  GapiTestCommon::fbo<VulkanApi>("VulkanApi_Fbo.png");
  }

TEST(VulkanApi,Draw) {
  GapiTestCommon::draw<VulkanApi,TextureFormat::RGBA8>  ("VulkanApi_Draw_RGBA8.png");
  GapiTestCommon::draw<VulkanApi,TextureFormat::RG8>    ("VulkanApi_Draw_RG8.png");
  GapiTestCommon::draw<VulkanApi,TextureFormat::R8>     ("VulkanApi_Draw_R8.png");
  GapiTestCommon::draw<VulkanApi,TextureFormat::RGBA16> ("VulkanApi_Draw_RGBA16.png");
  GapiTestCommon::draw<VulkanApi,TextureFormat::RG16>   ("VulkanApi_Draw_RG16.png");
  GapiTestCommon::draw<VulkanApi,TextureFormat::R16>    ("VulkanApi_Draw_R16.png");
  GapiTestCommon::draw<VulkanApi,TextureFormat::RGBA32F>("VulkanApi_Draw_RGBA32F.png");
  GapiTestCommon::draw<VulkanApi,TextureFormat::RG32F>  ("VulkanApi_Draw_RG32F.png");
  GapiTestCommon::draw<VulkanApi,TextureFormat::R32F>   ("VulkanApi_Draw_R32F.png");
  }

TEST(VulkanApi,Compute) {
  GapiTestCommon::ssboDispath<VulkanApi>();
  }

TEST(VulkanApi,ComputeImage) {
  GapiTestCommon::imageCompute<VulkanApi>("VulkanApi_Compute.png");
  }

TEST(VulkanApi,MipMaps) {
  GapiTestCommon::mipMaps<VulkanApi,TextureFormat::RGBA8>  ("VulkanApi_MipMaps_RGBA8.png");
  GapiTestCommon::mipMaps<VulkanApi,TextureFormat::RGBA16> ("VulkanApi_MipMaps_RGBA16.png");
  GapiTestCommon::mipMaps<VulkanApi,TextureFormat::RGBA32F>("VulkanApi_MipMaps_RGBA32.png");
  }

TEST(VulkanApi,TesselationBasic) {
  GapiTestCommon::psoTess<VulkanApi>();
  }

TEST(VulkanApi,SsboWrite) {
  GapiTestCommon::ssboWriteVs<VulkanApi>();
  }

TEST(VulkanApi,PushRemapping) {
  GapiTestCommon::pushConstant<VulkanApi>();
  }
