#include "vshader.h"

#include "vdevice.h"

#include <Tempest/File>

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
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
  }

VShader::~VShader() {
  vkDeviceWaitIdle(device);
  vkDestroyShaderModule(device,impl,nullptr);
  }

std::unique_ptr<uint32_t[]> VShader::load(const char *filename,uint32_t& size) {
  Tempest::RFile file(filename);

  const size_t fileSize=file.size();
  if(fileSize%4!=0)
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);

  std::unique_ptr<uint32_t[]> buffer(new uint32_t[fileSize/4]);
  file.read(reinterpret_cast<char*>(buffer.get()),fileSize);

  size=uint32_t(fileSize);
  return buffer;
  }
