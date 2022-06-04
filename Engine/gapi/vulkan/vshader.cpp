#if defined(TEMPEST_BUILD_VULKAN)

#include "vshader.h"

#include <Tempest/File>

#include "vdevice.h"
#include "gapi/shaderreflection.h"

using namespace Tempest::Detail;

VShader::VShader(VDevice& device, const void *source, size_t src_size)
  :device(device.device.impl) {
  if(src_size%4!=0)
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);

  VkShaderModuleCreateInfo createInfo = {};
  createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = src_size;
  createInfo.pCode    = reinterpret_cast<const uint32_t*>(source);

  {
  spirv_cross::Compiler comp(createInfo.pCode,uint32_t(src_size/4));
  ShaderReflection::getVertexDecl(vdecl,comp);
  ShaderReflection::getBindings(lay,comp);

  stage = ShaderReflection::getExecutionModel(comp);
  if(stage==ShaderReflection::Compute) {
    this->comp.wgSize.x = comp.get_execution_mode_argument(spv::ExecutionModeLocalSize,0);
    this->comp.wgSize.y = comp.get_execution_mode_argument(spv::ExecutionModeLocalSize,1);
    this->comp.wgSize.z = comp.get_execution_mode_argument(spv::ExecutionModeLocalSize,2);
    }
  }

  if(vkCreateShaderModule(device.device.impl,&createInfo,nullptr,&impl)!=VK_SUCCESS)
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
  }

VShader::~VShader() {
  vkDestroyShaderModule(device,impl,nullptr);
  }

#endif
