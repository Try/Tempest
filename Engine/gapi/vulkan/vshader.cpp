#if defined(TEMPEST_BUILD_VULKAN)

#include "vshader.h"

#include "vdevice.h"

using namespace Tempest::Detail;

VShader::VShader(VDevice& device, const void *source, size_t src_size)
  :Shader(source, src_size), device(device.device.impl) {

  VkShaderModuleCreateInfo createInfo = {};
  createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = src_size;
  createInfo.pCode    = reinterpret_cast<const uint32_t*>(source);

  if(vkCreateShaderModule(device.device.impl,&createInfo,nullptr,&impl)!=VK_SUCCESS)
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
  }

VShader::VShader(VDevice& device)
  :Shader(), device(device.device.impl) {
  }

VShader::~VShader() {
  if(impl!=VK_NULL_HANDLE)
    vkDestroyShaderModule(device,impl,nullptr);
  }


#endif
