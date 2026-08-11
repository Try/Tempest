#if defined(TEMPEST_BUILD_VULKAN)

#include "vpipeline.h"

#include "vdevice.h"
#include "vshader.h"

#include <Tempest/RenderState>

using namespace Tempest;
using namespace Tempest::Detail;

bool VPipeline::InstRp::isCompatible(const std::shared_ptr<VFramebufferMap::RenderPass>& dr, VkPipelineLayout pLay, size_t stride) const {
  if(this->stride!=stride)
    return false;
  if(this->pLay!=pLay)
    return false;

  return lay->isCompatible(*dr);
  }

bool VPipeline::InstDr::isCompatible(const VkPipelineRenderingCreateInfoKHR& dr, VkPipelineLayout pLay, size_t stride) const {
  if(this->stride!=stride)
    return false;
  if(this->pLay!=pLay)
    return false;

  if(lay.viewMask!=dr.viewMask)
    return false;
  if(lay.colorAttachmentCount!=dr.colorAttachmentCount)
    return false;
  if(std::memcmp(colorFrm,dr.pColorAttachmentFormats,dr.colorAttachmentCount*sizeof(VkFormat))!=0)
    return false;
  if(lay.depthAttachmentFormat!=dr.depthAttachmentFormat ||
     lay.stencilAttachmentFormat!=dr.stencilAttachmentFormat)
    return false;
  return true;
  }


VPipeline::VPipeline(VDevice& device, Topology tp, const RenderState& st, const VShader** sh, size_t count)
  : device(device), tp(tp), st(st) {
  try {
    const std::vector<Detail::ShaderReflection::Binding>* bindings[5] = {};
    for(size_t i=0; i<count; ++i) {
      if(sh[i]==nullptr)
        continue;
      auto* s = reinterpret_cast<const Detail::VShader*>(sh[i]);
      bindings[i] = &s->lay;
      }
    ShaderReflection::setupLayout(pb, layout, sync, bindings, count);

    for(size_t i=0; i<count; ++i)
      if(sh[i]!=nullptr)
        modules[i] = Detail::DSharedPtr<const VShader*>{sh[i]};

    if(auto vert=findShader(ShaderReflection::Stage::Vertex)) {
      declSize = vert->vert.decl.size();
      decl.reset(new Decl::ComponentType[declSize]);
      std::memcpy(decl.get(), vert->vert.decl.data(), declSize*sizeof(Decl::ComponentType));
      defaultStride = 0;
      for(size_t i=0;i<declSize;++i){
        defaultStride += uint32_t(Decl::size(decl[i]));
        }
      }

    if(auto task=findShader(ShaderReflection::Stage::Task)) {
      wgSize = task->comp.wgSize;
      }
    else if(auto mesh=findShader(ShaderReflection::Stage::Mesh)) {
      wgSize = mesh->comp.wgSize;
      }

    pipelineLayout = device.psoLayouts.findLayout(pb, layout);
    }
  catch(...) {
    cleanup();
    throw;
    }
  }

VPipeline::~VPipeline() {
  cleanup();
  }

VkPipeline VPipeline::instance(const VkPipelineRenderingCreateInfoKHR& info, VkRenderPass pass, VkPipelineLayout pLay, size_t stride) {
  std::lock_guard<SpinLock> guard(syncInst);

  for(auto& i:instDr)
    if(i.isCompatible(info,pLay,stride))
      return i.val;
  VkPipeline val = VK_NULL_HANDLE;
  try {
    val = initGraphicsPipeline(device,pLay,pass,&info,st,
                               decl.get(),declSize,stride,
                               tp,modules);
    instDr.emplace_back(info,pLay,stride,val);
    }
  catch(...) {
    if(val!=VK_NULL_HANDLE)
      vkDestroyPipeline(device.device.impl,val,nullptr);
    throw;
    }
  return instDr.back().val;
  }

IVec3 VPipeline::workGroupSize() const {
  return wgSize;
  }

size_t VPipeline::sizeofBuffer(size_t id, size_t arraylen) const {
  return layout.sizeofBuffer(id, arraylen);
  }

const VShader* VPipeline::findShader(ShaderReflection::Stage sh) const {
  for(auto& i:modules)
    if(i.handler!=nullptr && i.handler->stage==sh) {
      return i.handler;
      }
  return nullptr;
  }

void VPipeline::cleanup() {
  for(auto& i:instRp)
    vkDestroyPipeline(device.device.impl,i.val,nullptr);
  for(auto& i:instDr)
    vkDestroyPipeline(device.device.impl,i.val,nullptr);
  }

VkPipeline VPipeline::initGraphicsPipeline(VDevice& device,
                                           VkPipelineLayout layout, const VkRenderPass rpass,
                                           const VkPipelineRenderingCreateInfoKHR* dynLay,
                                           const RenderState &st,
                                           const Decl::ComponentType *decl, size_t declSize,
                                           size_t stride, Topology tp,
                                           const DSharedPtr<const VShader*>* shaders) {
  SmallArray<VkDescriptorSetAndBindingMappingEXT, MaxBindings> mappings(this->layout.size());
  VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo { VK_STRUCTURE_TYPE_SHADER_DESCRIPTOR_SET_AND_BINDING_MAPPING_INFO_EXT};
  if(device.props.hasDescriptorHeap) {
    mappingInfo.mappingCount = uint32_t(this->layout.size());
    mappingInfo.pMappings    = mappings.get();
    initPushMappings(device, mappingInfo, mappings.get(), pb, this->layout);
    }

  VkPipelineShaderStageCreateInfo shaderStages[5] = {};
  size_t                          stagesCnt       = 0;
  for(size_t i=0; i<5; ++i) {
    if(shaders[i].handler==nullptr)
      continue;

    VkPipelineShaderStageCreateInfo& sh = shaderStages[stagesCnt];
    sh.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    sh.stage  = nativeFormat(shaders[i].handler->stage);
    sh.module = shaders[i].handler->impl;
    sh.pName  = "main";
    if(device.props.hasDescriptorHeap) {
      mappingInfo.pNext = sh.pNext;
      sh.pNext          = &mappingInfo;
      }
    stagesCnt++;
    }

  const bool useTesselation = (findShader(ShaderReflection::Stage::Evaluate)!=nullptr ||
                               findShader(ShaderReflection::Stage::Control) !=nullptr);

  VkVertexInputBindingDescription vertexInputBindingDescription;
  vertexInputBindingDescription.binding   = 0;
  vertexInputBindingDescription.stride    = uint32_t(stride);
  vertexInputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

  SmallArray<VkVertexInputAttributeDescription,16> vsInput(declSize);
  uint32_t offset=0;
  for(size_t i=0;i<declSize;++i){
    auto& loc=vsInput[i];
    loc.location = uint32_t(i);
    loc.binding  = 0;
    loc.format   = nativeFormat(decl[i]);
    loc.offset   = offset;

    offset += uint32_t(Decl::size(decl[i]));
    }

  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
  vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.pNext = nullptr;
  vertexInputInfo.flags = 0;
  vertexInputInfo.vertexBindingDescriptionCount   = (declSize>0 ? 1 : 0);
  vertexInputInfo.pVertexBindingDescriptions      = &vertexInputBindingDescription;
  vertexInputInfo.vertexAttributeDescriptionCount = uint32_t(declSize);
  vertexInputInfo.pVertexAttributeDescriptions    = vsInput.get();

  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
  inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.primitiveRestartEnable = VK_FALSE;
  inputAssembly.topology               = nativeFormat(tp);

  if(findShader(ShaderReflection::Stage::Vertex)!=nullptr) {
    if(useTesselation)
      inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;
    } else {
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    }

  VkPipelineViewportStateCreateInfo viewportState = {};
  viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports    = nullptr;
  viewportState.scissorCount  = 1;
  viewportState.pScissors     = nullptr;

  VkPipelineRasterizationStateCreateInfo rasterizer = {};
  rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.pNext                   = nullptr;
  rasterizer.flags                   = 0;
  rasterizer.rasterizerDiscardEnable = st.isRasterDiscardEnabled() ? VK_TRUE : VK_FALSE;
  rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
  rasterizer.cullMode                = nativeFormat(st.cullFaceMode());
  rasterizer.frontFace               = VK_FRONT_FACE_CLOCKWISE;
  rasterizer.depthClampEnable        = VK_FALSE;
  rasterizer.depthBiasEnable         = VK_FALSE;
  rasterizer.depthBiasConstantFactor = 0.0f;
  rasterizer.depthBiasClamp          = 0.0f;
  rasterizer.depthBiasSlopeFactor    = 0.0f;
  rasterizer.lineWidth               = 1.0f;

  VkPipelineMultisampleStateCreateInfo multisampling = {};
  multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable  = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState blendAtt[MaxFramebufferAttachments] = {};
  uint32_t                            blendAttCount = 0;
  {
  const size_t size = dynLay->colorAttachmentCount;
  for(size_t i=0; i<size; ++i) {
    const VkFormat frm = dynLay->pColorAttachmentFormats[i];
    if(nativeIsDepthFormat(frm))
      continue;
    auto& a = blendAtt[i];
    a.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    a.blendEnable         = st.hasBlend() ? VK_TRUE : VK_FALSE;
    a.dstColorBlendFactor = nativeFormat(st.blendDest());
    a.srcColorBlendFactor = nativeFormat(st.blendSource());
    a.colorBlendOp        = nativeFormat(st.blendOperation());
    a.dstAlphaBlendFactor = a.dstColorBlendFactor;
    a.srcAlphaBlendFactor = a.srcColorBlendFactor;
    a.alphaBlendOp        = a.colorBlendOp;
    ++blendAttCount;
    }
  }

  VkPipelineColorBlendStateCreateInfo colorBlending = {};
  colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable     = VK_FALSE;
  colorBlending.logicOp           = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount   = blendAttCount;
  colorBlending.pAttachments      = blendAtt;
  colorBlending.blendConstants[0] = 0.0f;
  colorBlending.blendConstants[1] = 0.0f;
  colorBlending.blendConstants[2] = 0.0f;
  colorBlending.blendConstants[3] = 0.0f;

  VkPipelineDepthStencilStateCreateInfo depthStencil = {};
  depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable       = st.zTestMode()!=RenderState::ZTestMode::Always ? VK_TRUE : VK_FALSE;
  depthStencil.depthWriteEnable      = st.isZWriteEnabled() ? VK_TRUE : VK_FALSE;
  depthStencil.depthCompareOp        = nativeFormat(st.zTestMode());
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.stencilTestEnable     = VK_FALSE;
  if(depthStencil.depthWriteEnable==VK_TRUE && depthStencil.depthTestEnable==VK_FALSE) {
    // Spec: Depth writes are always disabled when depthTestEnable is VK_FALSE.
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthCompareOp  = VK_COMPARE_OP_ALWAYS;
    }

  VkPipelineTessellationStateCreateInfo tesselation = {};
  tesselation.sType                 = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
  tesselation.flags                 = 0;
  if(tp==Triangles)
    tesselation.patchControlPoints = 3; else
    tesselation.patchControlPoints = 2;

  VkPipelineDynamicStateCreateInfo dynamic = {};
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  const VkDynamicState dySt[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  dynamic.pDynamicStates    = dySt;
  dynamic.dynamicStateCount = 2;

  VkPipelineCreateFlags2CreateInfo createFlags2{VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO};
  createFlags2.flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

  VkGraphicsPipelineCreateInfo pipelineInfo = {};
  pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount          = uint32_t(stagesCnt);
  pipelineInfo.pStages             = shaderStages;
  pipelineInfo.pVertexInputState   = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState      = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState   = &multisampling;
  pipelineInfo.pDepthStencilState  = &depthStencil;
  pipelineInfo.pColorBlendState    = &colorBlending;
  pipelineInfo.pDynamicState       = &dynamic;
  pipelineInfo.layout              = layout;
  pipelineInfo.subpass             = 0;
  pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE; // TODO: dummy default pso

  if(rpass!=VK_NULL_HANDLE) {
    pipelineInfo.renderPass = rpass;
    } else {
    pipelineInfo.pNext      = dynLay;
    }

  if(device.props.hasDescriptorHeap) {
    createFlags2.pNext = pipelineInfo.pNext;
    pipelineInfo.pNext = &createFlags2;
    }

  if(useTesselation) {
    pipelineInfo.pTessellationState = &tesselation;
    // rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
    }

  VkPipeline graphicsPipeline = VK_NULL_HANDLE;
  const auto err = vkCreateGraphicsPipelines(device.device.impl,VK_NULL_HANDLE,1,&pipelineInfo,nullptr,&graphicsPipeline);
  if(err!=VK_SUCCESS)
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);
  return graphicsPipeline;
  }

void VPipeline::initPushMappings(VDevice& device, VkShaderDescriptorSetAndBindingMappingInfoEXT& info, VkDescriptorSetAndBindingMappingEXT* mappings,
                                 const PushBlock& pb, const LayoutDesc& lay) {
  auto nativeFormat = [](ShaderReflection::Class cls) -> VkSpirvResourceTypeFlagsEXT {
    switch (cls) {
      case ShaderReflection::Ubo:     return VK_SPIRV_RESOURCE_TYPE_UNIFORM_BUFFER_BIT_EXT;
      case ShaderReflection::Texture: return VK_SPIRV_RESOURCE_TYPE_COMBINED_SAMPLED_IMAGE_BIT_EXT;
      case ShaderReflection::Image:   return VK_SPIRV_RESOURCE_TYPE_SAMPLED_IMAGE_BIT_EXT;
      case ShaderReflection::Sampler: return VK_SPIRV_RESOURCE_TYPE_SAMPLER_BIT_EXT;
      case ShaderReflection::SsboR:   return VK_SPIRV_RESOURCE_TYPE_READ_ONLY_STORAGE_BUFFER_BIT_EXT;
      case ShaderReflection::SsboRW:  return VK_SPIRV_RESOURCE_TYPE_READ_WRITE_STORAGE_BUFFER_BIT_EXT;
      case ShaderReflection::ImgR:    return VK_SPIRV_RESOURCE_TYPE_READ_ONLY_IMAGE_BIT_EXT;
      case ShaderReflection::ImgRW:   return VK_SPIRV_RESOURCE_TYPE_READ_WRITE_IMAGE_BIT_EXT;
      case ShaderReflection::Tlas:    return VK_SPIRV_RESOURCE_TYPE_ACCELERATION_STRUCTURE_BIT_EXT;
      case ShaderReflection::Push:
      case ShaderReflection::Count:
        return VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
      }
    return VK_SPIRV_RESOURCE_TYPE_ALL_EXT;
    };

  size_t   mappingId = 0;
  uint32_t descOff   = pb.size;
  assert(descOff%4==0); //NOTE: must be multiple of 4
  for(size_t i=0; i<MaxBindings; ++i) {
    if(((1u << i) & lay.active)==0)
      continue;
    auto& m = mappings[mappingId]; ++mappingId;
    m.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_AND_BINDING_MAPPING_EXT;
    m.descriptorSet = 0;
    m.firstBinding  = uint32_t(i);
    m.bindingCount  = 1;
    m.resourceMask  = nativeFormat(lay.bindings[i]);
    m.source        = VK_DESCRIPTOR_MAPPING_SOURCE_HEAP_WITH_PUSH_INDEX_EXT;

    m.sourceData.pushIndex.heapIndexStride        = device.props.resourceDescriptorSize;
    m.sourceData.pushIndex.heapArrayStride        = device.props.resourceDescriptorSize;
    m.sourceData.pushIndex.samplerHeapIndexStride = device.props.samplerDescriptorSize;
    m.sourceData.pushIndex.samplerHeapArrayStride = 0; //device.props.samplerDescriptorSize;

    if(lay.bindings[i]==ShaderReflection::Texture)
      m.sourceData.pushIndex.useCombinedImageSamplerIndex = VK_TRUE;

    m.sourceData.pushIndex.pushOffset = descOff;
    descOff += sizeof(uint32_t);
    }
  info.mappingCount = mappingId;
  }


bool VCompPipeline::Inst::isCompatible(VkPipelineLayout lay) const {
  return dLay==lay;
  }


VCompPipeline::VCompPipeline(VDevice& device, const VShader& comp)
  :device(device), wgSize(comp.comp.wgSize) {
  const std::vector<Detail::ShaderReflection::Binding>* bindings = &comp.lay;
  ShaderReflection::setupLayout(pb, layout, sync, &bindings, 1);

  SmallArray<VkDescriptorSetAndBindingMappingEXT, MaxBindings> mappings(this->layout.size());
  VkShaderDescriptorSetAndBindingMappingInfoEXT mappingInfo { VK_STRUCTURE_TYPE_SHADER_DESCRIPTOR_SET_AND_BINDING_MAPPING_INFO_EXT};
  if(device.props.hasDescriptorHeap) {
    mappingInfo.mappingCount = uint32_t(this->layout.size());
    mappingInfo.pMappings    = mappings.get();
    VPipeline::initPushMappings(device, mappingInfo, mappings.get(), pb, this->layout);
    }
  VkPipelineCreateFlags2CreateInfo createFlags2{VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO};
  createFlags2.flags = VK_PIPELINE_CREATE_2_DESCRIPTOR_HEAP_BIT_EXT;

  pipelineLayout = device.props.hasDescriptorHeap ? VK_NULL_HANDLE : device.psoLayouts.findLayout(pb, layout);
  shader         = Detail::DSharedPtr<const VShader*>{&comp};

  VkDevice dev = device.device.impl;
  try {
    VkComputePipelineCreateInfo info = {};
    info.sType        = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    info.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    info.stage.module = comp.impl;
    info.stage.pName  = "main";
    info.layout       = pipelineLayout;
    if(layout.isUpdateAfterBind() && !device.props.hasDescriptorHeap) {
      info.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;
      }
    if(device.props.hasDescriptorHeap) {
      mappingInfo.pNext = info.stage.pNext;
      info.stage.pNext  = &mappingInfo;

      createFlags2.pNext = info.pNext;
      info.pNext         = &createFlags2;
      }
    const auto err = vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &info, nullptr, &impl);
    if(err!=VK_SUCCESS)
      throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);

    VkDebugMarkerObjectNameInfoEXT nameInfo = {VK_STRUCTURE_TYPE_DEBUG_MARKER_OBJECT_NAME_INFO_EXT};
    nameInfo.objectType   = VK_DEBUG_REPORT_OBJECT_TYPE_PIPELINE_EXT;
    nameInfo.object       = impl;
    nameInfo.pObjectName  = comp.dbgShortName();
    device.vkDebugMarkerSetObjectName(device.device.impl, &nameInfo);
    }
  catch(...) {
    vkDestroyPipelineLayout(dev,pipelineLayout,nullptr);
    throw;
    }
  }

VCompPipeline::~VCompPipeline() {
  if(impl==VK_NULL_HANDLE)
    return;
  VkDevice dev = device.device.impl;
  vkDestroyPipeline(dev,impl,nullptr);
  for(auto& i:inst)
    vkDestroyPipeline(dev,i.val,nullptr);
  }

IVec3 VCompPipeline::workGroupSize() const {
  return wgSize;
  }

size_t VCompPipeline::sizeofBuffer(size_t id, size_t arraylen) const {
  return layout.sizeofBuffer(id, arraylen);
  }

VkPipeline VCompPipeline::instance(VkPipelineLayout pLay) {
  if(!layout.isUpdateAfterBind())
    return impl;

  if(device.props.hasDescriptorHeap)
    return impl; // no need to spam variants for heap-based pipelines

  std::lock_guard<SpinLock> guard(syncInst);
  for(auto& i:inst)
    if(i.isCompatible(pLay))
      return i.val;

  VkDevice   dev = device.device.impl;
  VkPipeline val = VK_NULL_HANDLE;
  try {
    VkComputePipelineCreateInfo info = {};
    info.sType              = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    info.stage.sType        = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage.stage        = VK_SHADER_STAGE_COMPUTE_BIT;
    info.stage.module       = shader.handler->impl;
    info.stage.pName        = "main";
    info.layout             = pLay;
    info.flags              = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
    info.basePipelineHandle = impl;
    info.basePipelineIndex  = -1;
    const auto err = vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &info, nullptr, &val);
    if(err!=VK_SUCCESS)
      throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule);

    inst.emplace_back(pLay,val);
    }
  catch(...) {
    if(val!=VK_NULL_HANDLE)
      vkDestroyPipeline(dev,val,nullptr);
    throw;
    }
  return inst.back().val;
  }

#endif
