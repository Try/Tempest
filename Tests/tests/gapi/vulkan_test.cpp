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
  GapiTestCommon::draw<VulkanApi>("VulkanApi_Draw.png");
  }

TEST(VulkanApi,Compute) {
  GapiTestCommon::ssboDispath<VulkanApi>();
  }

TEST(VulkanApi,ComputeImage) {
  GapiTestCommon::imageCompute<VulkanApi>("VulkanApi_Compute.png");
  }
