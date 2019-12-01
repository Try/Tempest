#include "vpipeline.h"

#include "vdevice.h"
#include "vframebuffer.h"
#include "vframebufferlayout.h"
#include "vrenderpass.h"
#include "vshader.h"

#include <algorithm>

#include <Tempest/UniformsLayout>
#include <Tempest/RenderState>

using namespace Tempest::Detail;


VPipeline::VUboLayout::VUboLayout(VkDevice dev, const Tempest::UniformsLayout &lay)
  :dev(dev),impl(initUboLayout(dev,lay)){
  }

VPipeline::VUboLayout::VUboLayout(VkDevice dev,VkDescriptorSetLayout lay)
  :dev(dev),impl(lay){
  }

VPipeline::VUboLayout::~VUboLayout() {
  vkDeviceWaitIdle(dev);
  vkDestroyDescriptorSetLayout(dev,impl,nullptr);
  }


VPipeline::VPipeline(){
  }

VPipeline::VPipeline(VDevice& device,
                     const RenderState &st,
                     const Decl::ComponentType *idecl, size_t declSize, size_t stride,
                     Topology tp,
                     const UniformsLayout &ulay,
                     std::shared_ptr<AbstractGraphicsApi::UniformsLay> &ulayImpl,
                     VShader& vert, VShader& frag)
  : device(device.device), st(st), declSize(declSize), stride(stride), tp(tp), vs(&vert), fs(&frag) {
  try {
    if(ulayImpl==nullptr)
      ulayImpl=std::make_shared<VUboLayout>(device.device,initUboLayout(device.device,ulay));

    decl.reset(new Decl::ComponentType[declSize]);
    std::memcpy(decl.get(),idecl,declSize*sizeof(Decl::ComponentType));

    VUboLayout* puLay=reinterpret_cast<VUboLayout*>(ulayImpl.get());
    pipelineLayout   =initLayout(device.device,puLay->impl);
    // instance(pass,width,height);
    }
  catch(...) {
    cleanup();
    throw;
    }
  }

VPipeline::VPipeline(VPipeline &&other) {
  std::swap(device,         other.device);
  std::swap(inst,           other.inst);
  std::swap(pipelineLayout, other.pipelineLayout);
  }

VPipeline::~VPipeline() {
  cleanup();
  }

void VPipeline::operator=(VPipeline &&other) {
  std::swap(device,         other.device);
  std::swap(inst,           other.inst);
  std::swap(pipelineLayout, other.pipelineLayout);
  }

VPipeline::Inst &VPipeline::instance(VFramebufferLayout &lay, uint32_t width, uint32_t height) {
  std::lock_guard<SpinLock> guard(sync);

  for(auto& i:inst)
    if(i.w==width && i.h==height && i.lay.handler==&lay)
      return i;
  VkPipeline val=VK_NULL_HANDLE;
  try {
    val = initGraphicsPipeline(device,pipelineLayout,lay,st,
                               width,height,decl.get(),declSize,stride,
                               tp,*vs.handler,*fs.handler);
    inst.emplace_back(width,height,&lay,val);
    }
  catch(...) {
    if(val!=VK_NULL_HANDLE)
      vkDestroyPipeline(device,val,nullptr);
    throw;
    }
  return inst.back();
  }

void VPipeline::cleanup() {
  if(pipelineLayout==VK_NULL_HANDLE)
    return;
  vkDeviceWaitIdle(device);
  if(pipelineLayout!=VK_NULL_HANDLE)
    vkDestroyPipelineLayout(device,pipelineLayout,nullptr);
  for(auto& i:inst)
    vkDestroyPipeline(device,i.val,nullptr);
  }

VkPipelineLayout VPipeline::initLayout(VkDevice device,VkDescriptorSetLayout uboLay) {
  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
  pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.pSetLayouts            = &uboLay;
  pipelineLayoutInfo.setLayoutCount         = 1;
  pipelineLayoutInfo.pushConstantRangeCount = 0;

  VkPipelineLayout ret;
  vkAssert(vkCreatePipelineLayout(device,&pipelineLayoutInfo,nullptr,&ret));
  return ret;
  }

VkDescriptorSetLayout VPipeline::initUboLayout(VkDevice device, const UniformsLayout &ulay) {
  static const VkDescriptorType types[]={
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    };

  std::vector<VkDescriptorSetLayoutBinding> bind(ulay.size());
  for(size_t i=0;i<bind.size();++i){
    auto& b=bind[i];
    auto& e=ulay[i];

    b.binding         = e.layout;
    b.descriptorCount = 1;
    b.descriptorType  = types[e.cls];

    b.stageFlags      = 0;
    if(e.stage&UniformsLayout::Vertex)
      b.stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
    if(e.stage&UniformsLayout::Fragment)
      b.stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    }

  VkDescriptorSetLayoutCreateInfo info={};
  info.sType       =VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  info.bindingCount=bind.size();
  info.pBindings   =bind.data();

  VkDescriptorSetLayout ret;
  vkAssert(vkCreateDescriptorSetLayout(device,&info,nullptr,&ret));
  return ret;
  }

VkPipeline VPipeline::initGraphicsPipeline(VkDevice device, VkPipelineLayout layout,
                                           const VFramebufferLayout &lay, const RenderState &st,
                                           uint32_t width, uint32_t height,
                                           const Decl::ComponentType *decl, size_t declSize,
                                           size_t stride, Topology tp,
                                           VShader &vert, VShader &frag) {
  VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
  vertShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
  vertShaderStageInfo.module = vert.impl;
  vertShaderStageInfo.pName  = "main";

  VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
  fragShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragShaderStageInfo.module = frag.impl;
  fragShaderStageInfo.pName  = "main";

  VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

  VkVertexInputBindingDescription vk_vertexInputBindingDescription;
  vk_vertexInputBindingDescription.binding   = 0;
  vk_vertexInputBindingDescription.stride    = stride;
  vk_vertexInputBindingDescription.inputRate = VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX;

  static const VkFormat vertFormats[]={
    VkFormat::VK_FORMAT_UNDEFINED,
    VkFormat::VK_FORMAT_R32_SFLOAT,
    VkFormat::VK_FORMAT_R32G32_SFLOAT,
    VkFormat::VK_FORMAT_R32G32B32_SFLOAT,
    VkFormat::VK_FORMAT_R32G32B32A32_SFLOAT,

    VK_FORMAT_A8B8G8R8_UNORM_PACK32,

    VK_FORMAT_R16G16_SNORM,
    VK_FORMAT_R16G16B16A16_SNORM,

    VK_FORMAT_R16G16_SNORM,
    VK_FORMAT_R16G16B16A16_SNORM,
    };

  static const uint32_t vertSize[]={
    0,
    4,
    8,
    12,
    16,

    4,

    4,
    8,

    4,
    8
  };

  uint32_t offset=0;
  std::vector<VkVertexInputAttributeDescription> vsInput(declSize);
  for(size_t i=0;i<declSize;++i){
    auto& loc=vsInput[i];
    loc.location = i;
    loc.binding  = 0;
    loc.format   = vertFormats[decl[i]];
    loc.offset   = offset;

    offset+=vertSize[decl[i]];
    }

  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
  vertexInputInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.pNext = nullptr;
  vertexInputInfo.flags = 0;
  vertexInputInfo.vertexBindingDescriptionCount   = 1;
  vertexInputInfo.pVertexBindingDescriptions      = &vk_vertexInputBindingDescription;
  vertexInputInfo.vertexAttributeDescriptionCount = vsInput.size();
  vertexInputInfo.pVertexAttributeDescriptions    = vsInput.data();

  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
  inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.primitiveRestartEnable = VK_FALSE;
  if(tp==Triangles)
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; else
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;

  VkViewport viewport = {};
  viewport.x        = 0.0f;
  viewport.y        = 0.0f;
  viewport.width    = float(width);
  viewport.height   = float(height);
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  VkRect2D scissor = {};
  scissor.offset = {0, 0};
  scissor.extent = {width,height};

  VkPipelineViewportStateCreateInfo viewportState = {};
  viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.pViewports    = &viewport;
  viewportState.scissorCount  = 1;
  viewportState.pScissors     = &scissor;

  static const VkCullModeFlags cullMode[]={
    VK_CULL_MODE_BACK_BIT,
    VK_CULL_MODE_FRONT_BIT,
    0,
    0
    };

  VkPipelineRasterizationStateCreateInfo rasterizer = {};
  rasterizer.sType                   = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.pNext                   = nullptr;
  rasterizer.flags                   = 0;
  rasterizer.rasterizerDiscardEnable = st.isRasterDiscardEnabled() ? VK_TRUE : VK_FALSE;
  rasterizer.polygonMode             = VkPolygonMode::VK_POLYGON_MODE_FILL;
  rasterizer.cullMode                = cullMode[uint32_t(st.cullFaceMode())];//VkCullModeFlagBits::VK_CULL_MODE_FRONT_BIT;
  rasterizer.frontFace               = VkFrontFace::VK_FRONT_FACE_CLOCKWISE;
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

  static const VkBlendFactor blend[] = {
    VK_BLEND_FACTOR_ZERO,                 //GL_ZERO,
    VK_BLEND_FACTOR_ONE,                  //GL_ONE,
    VK_BLEND_FACTOR_SRC_COLOR,            //GL_SRC_COLOR,
    VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,  //GL_ONE_MINUS_SRC_COLOR,
    VK_BLEND_FACTOR_SRC_ALPHA,            //GL_SRC_ALPHA,
    VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,  //GL_ONE_MINUS_SRC_ALPHA,
    VK_BLEND_FACTOR_DST_ALPHA,            //GL_DST_ALPHA,
    VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,  //GL_ONE_MINUS_DST_ALPHA,
    VK_BLEND_FACTOR_DST_COLOR,            //GL_DST_COLOR,
    VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,  //GL_ONE_MINUS_DST_COLOR,
    VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,   //GL_SRC_ALPHA_SATURATE,
    VK_BLEND_FACTOR_ZERO
    };
  VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
  colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable    = st.hasBlend() ? VK_TRUE : VK_FALSE;
  colorBlendAttachment.dstColorBlendFactor = blend[uint8_t(st.blendDest())];
  colorBlendAttachment.srcColorBlendFactor = blend[uint8_t(st.blendSource())];
  colorBlendAttachment.dstAlphaBlendFactor = colorBlendAttachment.dstColorBlendFactor;
  colorBlendAttachment.srcAlphaBlendFactor = colorBlendAttachment.srcColorBlendFactor;

  VkPipelineColorBlendStateCreateInfo colorBlending = {};
  colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable     = VK_FALSE;
  colorBlending.logicOp           = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount   = 1;
  colorBlending.pAttachments      = &colorBlendAttachment;
  colorBlending.blendConstants[0] = 0.0f;
  colorBlending.blendConstants[1] = 0.0f;
  colorBlending.blendConstants[2] = 0.0f;
  colorBlending.blendConstants[3] = 0.0f;

  static const VkCompareOp zMode[]={
    VK_COMPARE_OP_ALWAYS,
    VK_COMPARE_OP_NEVER,

    VK_COMPARE_OP_GREATER,
    VK_COMPARE_OP_LESS,

    VK_COMPARE_OP_GREATER_OR_EQUAL,
    VK_COMPARE_OP_LESS_OR_EQUAL,

    VK_COMPARE_OP_NOT_EQUAL,
    VK_COMPARE_OP_EQUAL,
    VK_COMPARE_OP_ALWAYS
    };

  VkPipelineDepthStencilStateCreateInfo depthStencil = {};
  depthStencil.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable       = st.zTestMode()!=RenderState::ZTestMode::Always ? VK_TRUE : VK_FALSE;
  depthStencil.depthWriteEnable      = st.isZWriteEnabled() ? VK_TRUE : VK_FALSE;
  depthStencil.depthCompareOp        = zMode[uint32_t(st.zTestMode())];
  depthStencil.depthBoundsTestEnable = VK_FALSE;
  depthStencil.stencilTestEnable     = VK_FALSE;

  VkPipelineDynamicStateCreateInfo dynamic = {};
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  const VkDynamicState dySt[1]={VK_DYNAMIC_STATE_VIEWPORT};
  dynamic.pDynamicStates    = dySt;
  dynamic.dynamicStateCount = 1;

  VkGraphicsPipelineCreateInfo pipelineInfo = {};
  pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount          = 2;
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
  pipelineInfo.renderPass          = lay.impl;
  pipelineInfo.subpass             = 0;
  pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;

  VkPipeline graphicsPipeline=VK_NULL_HANDLE;
  vkAssert(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pipelineInfo,nullptr,&graphicsPipeline));
  return graphicsPipeline;
  }
