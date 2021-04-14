#if defined(TEMPEST_BUILD_VULKAN)

#include "vshader.h"

#include <Tempest/File>

#include "vdevice.h"
#include "gapi/shaderreflection.h"

using namespace Tempest::Detail;

VShader::VShader(VDevice& device, const void *source, size_t src_size)
  :device(device.device) {
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
  }

  if(vkCreateShaderModule(device.device,&createInfo,nullptr,&impl)!=VK_SUCCESS)
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
  }

VShader::~VShader() {
  vkDestroyShaderModule(device,impl,nullptr);
  }

#endif
