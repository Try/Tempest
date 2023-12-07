#if defined(TEMPEST_BUILD_VULKAN)

#include "vmeshshaderemulated.h"

#include <Tempest/File>
#include <libspirv/libspirv.h>

#include "gapi/spirv/meshconverter.h"

#include "vdevice.h"

using namespace Tempest::Detail;

static void debugLog(const char* file, const uint32_t* spv, size_t len) {
  Tempest::WFile f(file);
  f.write(reinterpret_cast<const char*>(spv),len*4);
  }

VTaskShaderEmulated::VTaskShaderEmulated(VDevice& device, const void* source, size_t src_size)
  :VShader(device) {
  if(src_size%4!=0)
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
  fetchBindings(reinterpret_cast<const uint32_t*>(source),src_size/4);

  libspirv::MutableBytecode code{reinterpret_cast<const uint32_t*>(source),src_size/4};
  assert(code.findExecutionModel()==spv::ExecutionModelTaskEXT);

  MeshConverter conv(code);
  conv.exec();

  auto& comp = conv.computeShader();
  // debugLog("mesh_conv.comp.spv", comp.opcodes(), comp.size());
  // std::system("spirv-cross.exe -V .\\mesh_conv.comp.spv");
  // std::system("spirv-val.exe      .\\mesh_conv.comp.spv");

  VkShaderModuleCreateInfo createInfo = {};
  createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = comp.size()*4u;
  createInfo.pCode    = comp.opcodes();
  if(vkCreateShaderModule(device.device.impl,&createInfo,nullptr,&compPass)!=VK_SUCCESS)
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
  }

VTaskShaderEmulated::~VTaskShaderEmulated() {
  vkDestroyShaderModule(device,compPass,nullptr);
  }

VMeshShaderEmulated::VMeshShaderEmulated(VDevice& device, const void *source, size_t src_size)
  :VShader(device) {
  if(src_size%4!=0)
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
  fetchBindings(reinterpret_cast<const uint32_t*>(source),src_size/4);

  libspirv::MutableBytecode code{reinterpret_cast<const uint32_t*>(source),src_size/4};
  assert(code.findExecutionModel()==spv::ExecutionModelMeshEXT);

  MeshConverter conv(code);
  // conv.options.deferredMeshShading = true;
  // conv.options.varyingInSharedMem  = true;
  conv.exec();

  //debugLog("mesh_orig.mesh.spv", reinterpret_cast<const uint32_t*>(source),src_size/4);

  auto& comp = conv.computeShader();

  // debugLog("mesh_conv.comp.spv", comp.opcodes(), comp.size());
  // std::system("spirv-cross.exe -V .\\mesh_conv.comp.spv");
  // std::system("spirv-val.exe      .\\mesh_conv.comp.spv");


  auto& vert = conv.vertexPassthrough();

  // debugLog("mesh_conv.vert.spv", vert.opcodes(), vert.size());
  // std::system("spirv-cross.exe -V .\\mesh_conv.vert.spv");
  // std::system("spirv-val.exe      .\\mesh_conv.vert.spv");


  VkShaderModuleCreateInfo createInfo = {};
  createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = vert.size()*4u;
  createInfo.pCode    = vert.opcodes();
  if(vkCreateShaderModule(device.device.impl,&createInfo,nullptr,&impl)!=VK_SUCCESS)
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);

  createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = comp.size()*4u;
  createInfo.pCode    = comp.opcodes();
  if(vkCreateShaderModule(device.device.impl,&createInfo,nullptr,&compPass)!=VK_SUCCESS)
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
  }

VMeshShaderEmulated::~VMeshShaderEmulated() {
  vkDestroyShaderModule(device,compPass,nullptr);
  }

#endif
