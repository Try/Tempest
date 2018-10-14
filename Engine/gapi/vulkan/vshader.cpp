#include "vshader.h"

#include "vdevice.h"

#include <fstream>

using namespace Tempest::Detail;

VShader::VShader(VDevice& device,const char *file)
  :device(device.device) {
  uint32_t size=0;
  auto code=load(file,size);

  VkShaderModuleCreateInfo createInfo = {};
  createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = size;
  createInfo.pCode    = code.get();

  if(vkCreateShaderModule(device.device,&createInfo,nullptr,&impl)!=VK_SUCCESS)
    throw std::runtime_error("failed to create shader module!");
  }

VShader::~VShader() {
  vkDeviceWaitIdle(device);
  vkDestroyShaderModule(device,impl,nullptr);
  }

std::unique_ptr<uint32_t[]> VShader::load(const char *filename,uint32_t& size) {
  std::ifstream file(filename, std::ios::ate | std::ios::binary);
  if(!file.is_open())
    throw std::runtime_error("failed to open file!");

  size_t fileSize=size_t(file.tellg());
  std::unique_ptr<uint32_t[]> buffer(new uint32_t[fileSize/4]);

  file.seekg(0);
  file.read(reinterpret_cast<char*>(buffer.get()),int(fileSize));
  file.close();

  size=uint32_t(fileSize);

  return buffer;
  }
