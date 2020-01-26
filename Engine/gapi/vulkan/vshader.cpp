#include "vshader.h"

#include "vdevice.h"

#include <Tempest/File>

using namespace Tempest::Detail;

VShader::VShader(VDevice& device,const void *source,size_t src_size)
  :device(device.device) {
  if(src_size%4!=0)
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);

  VkShaderModuleCreateInfo createInfo = {};
  createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = src_size;
  createInfo.pCode    = reinterpret_cast<const uint32_t*>(source);

  if(vkCreateShaderModule(device.device,&createInfo,nullptr,&impl)!=VK_SUCCESS)
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
  }

VShader::~VShader() {
  vkDestroyShaderModule(device,impl,nullptr);
  }
