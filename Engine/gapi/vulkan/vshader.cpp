#if defined(TEMPEST_BUILD_VULKAN)

#include "vshader.h"

#include <Tempest/File>
#include <libspirv/libspirv.h>

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

  fetchBindings(createInfo.pCode,uint32_t(src_size/4));

  if(vkCreateShaderModule(device.device.impl,&createInfo,nullptr,&impl)!=VK_SUCCESS)
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
  }

VShader::VShader(VDevice& device)
  :device(device.device.impl) {
  }

VShader::~VShader() {
  if(impl!=VK_NULL_HANDLE)
    vkDestroyShaderModule(device,impl,nullptr);
  }

void VShader::fetchBindings(const uint32_t *source, size_t size) {
  // TODO: remove spirv-corss?
  spirv_cross::Compiler comp(source, size);
  ShaderReflection::getVertexDecl(vdecl,comp);
  ShaderReflection::getBindings(lay,comp);

  libspirv::Bytecode code(source, size);
  stage = ShaderReflection::getExecutionModel(code);
  if(stage==ShaderReflection::Compute || stage==ShaderReflection::Task || stage==ShaderReflection::Mesh) {
    for(auto& i:code) {
      if(i.op()!=spv::OpExecutionMode)
        continue;
      if(i[2]==spv::ExecutionModeLocalSize) {
        this->comp.wgSize.x = i[3];
        this->comp.wgSize.y = i[4];
        this->comp.wgSize.z = i[5];
        }
      }
    }
  }

#endif
