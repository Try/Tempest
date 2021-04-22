#if defined(TEMPEST_BUILD_VULKAN)

#include "vpipeline.h"

#include "vdevice.h"
#include "vframebuffer.h"
#include "vframebufferlayout.h"
#include "vrenderpass.h"
#include "vshader.h"
#include "vuniformslay.h"

#include <algorithm>

#include <Tempest/UniformsLayout>
#include <Tempest/RenderState>

using namespace Tempest;
using namespace Tempest::Detail;

VPipeline::VPipeline(){
  }

VPipeline::VPipeline(VDevice& device,
                     const RenderState &st, size_t stride, Topology tp, const VUniformsLay& ulay,
                     const VShader* vert, const VShader* ctrl, const VShader* tess, const VShader* geom, const VShader* frag)
  : device(device.device), st(st), stride(stride), tp(tp) {
  try {
    modules[0] = Detail::DSharedPtr<const VShader*>{vert};
    modules[1] = Detail::DSharedPtr<const VShader*>{ctrl};
    modules[2] = Detail::DSharedPtr<const VShader*>{tess};
    modules[3] = Detail::DSharedPtr<const VShader*>{geom};
    modules[4] = Detail::DSharedPtr<const VShader*>{frag};

    if(vert!=nullptr) {
      declSize = vert->vdecl.size();
      decl.reset(new Decl::ComponentType[declSize]);
      std::memcpy(decl.get(),vert->vdecl.data(),declSize*sizeof(Decl::ComponentType));
      }
    pipelineLayout = initLayout(device.device,ulay,pushStageFlags);
    ssboBarriers   = ulay.hasSSBO;
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
  std::swap(ssboBarriers,   other.ssboBarriers);
  }

VPipeline::~VPipeline() {
  cleanup();
  }

VPipeline::Inst &VPipeline::instance(VFramebufferLayout &lay, uint32_t width, uint32_t height) {
  std::lock_guard<SpinLock> guard(sync);

  for(auto& i:inst)
    if(i.w==width && i.h==height && i.lay.handler->isCompatible(lay))
      return i;
  VkPipeline val=VK_NULL_HANDLE;
  try {
    val = initGraphicsPipeline(device,pipelineLayout,lay,st,
                               width,height,decl.get(),declSize,stride,
                               tp,modules);
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
  if(pipelineLayout!=VK_NULL_HANDLE)
    vkDestroyPipelineLayout(device,pipelineLayout,nullptr);
  for(auto& i:inst)
    vkDestroyPipeline(device,i.val,nullptr);
  }

VkPipelineLayout VPipeline::initLayout(VkDevice device, const VUniformsLay& uboLay, VkShaderStageFlags& pushStageFlags) {
  VkPushConstantRange push = {};

  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
  pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.pSetLayouts            = &uboLay.impl;
  pipelineLayoutInfo.setLayoutCount         = 1;
  pipelineLayoutInfo.pushConstantRangeCount = 0;

  if(uboLay.pb.size>0) {
    if(uboLay.pb.stage & ShaderReflection::Vertex)
      pushStageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
    if(uboLay.pb.stage & ShaderReflection::Geometry)
      pushStageFlags |= VK_SHADER_STAGE_GEOMETRY_BIT;
    if(uboLay.pb.stage & ShaderReflection::Control)
      pushStageFlags |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    if(uboLay.pb.stage & ShaderReflection::Evaluate)
      pushStageFlags |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    if(uboLay.pb.stage & ShaderReflection::Fragment)
      pushStageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if(uboLay.pb.stage & ShaderReflection::Compute)
      pushStageFlags |= VK_SHADER_STAGE_COMPUTE_BIT;
    push.stageFlags = pushStageFlags;
    push.offset     = 0;
    push.size       = uint32_t(uboLay.pb.size);

    pipelineLayoutInfo.pPushConstantRanges    = &push;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    }

  VkPipelineLayout ret;
  vkAssert(vkCreatePipelineLayout(device,&pipelineLayoutInfo,nullptr,&ret));
  return ret;
  }

VkPipeline VPipeline::initGraphicsPipeline(VkDevice device, VkPipelineLayout layout,
                                           const VFramebufferLayout &lay, const RenderState &st,
                                           uint32_t width, uint32_t height,
                                           const Decl::ComponentType *decl, size_t declSize,
                                           size_t stride, Topology tp,
                                           const DSharedPtr<const VShader*>* shaders) {
  static const VkShaderStageFlagBits stageBits[] = {
    VK_SHADER_STAGE_VERTEX_BIT,
    VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
    VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
    VK_SHADER_STAGE_GEOMETRY_BIT,
    VK_SHADER_STAGE_FRAGMENT_BIT
    };
  VkPipelineShaderStageCreateInfo shaderStages[5] = {};
  size_t                          stagesCnt       = 0;
  for(size_t i=0; i<5; ++i) {
    if(shaders[i].handler!=nullptr) {
      VkPipelineShaderStageCreateInfo& sh = shaderStages[stagesCnt];
      sh.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
      sh.stage  = stageBits[i];
      sh.module = shaders[i].handler->impl;
      sh.pName  = "main";
      stagesCnt++;
      }
    }

  const bool useTesselation = (shaders[1] || shaders[2]);

  VkVertexInputBindingDescription vertexInputBindingDescription;
  vertexInputBindingDescription.binding   = 0;
  vertexInputBindingDescription.stride    = uint32_t(stride);
  vertexInputBindingDescription.inputRate = VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription                  vsInputsStk[16]={};
  std::unique_ptr<VkVertexInputAttributeDescription> vsInputHeap;
  VkVertexInputAttributeDescription*                 vsInput = vsInputsStk;
  if(declSize>16) {
    vsInputHeap.reset(new VkVertexInputAttributeDescription[declSize]);
    vsInput = vsInputHeap.get();
    }
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
  vertexInputInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.pNext = nullptr;
  vertexInputInfo.flags = 0;
  vertexInputInfo.vertexBindingDescriptionCount   = 1;
  vertexInputInfo.pVertexBindingDescriptions      = &vertexInputBindingDescription;
  vertexInputInfo.vertexAttributeDescriptionCount = uint32_t(declSize);
  vertexInputInfo.pVertexAttributeDescriptions    = vsInput;

  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
  inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.primitiveRestartEnable = VK_FALSE;
  if(tp==Triangles)
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; else
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
  if(useTesselation)
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;

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

  static const VkBlendOp blendOp[] = {
    VK_BLEND_OP_ADD,
    VK_BLEND_OP_SUBTRACT,
    VK_BLEND_OP_REVERSE_SUBTRACT,
    VK_BLEND_OP_MIN,
    VK_BLEND_OP_MAX,
    };

  VkPipelineColorBlendAttachmentState blendAtt[256] = {};
  for(size_t i=0; i<lay.colorCount; ++i) {
    auto& a = blendAtt[i];
    a.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    a.blendEnable         = st.hasBlend() ? VK_TRUE : VK_FALSE;
    a.dstColorBlendFactor = blend[uint8_t(st.blendDest())];
    a.srcColorBlendFactor = blend[uint8_t(st.blendSource())];
    a.colorBlendOp        = blendOp[uint8_t(st.blendOperation())];
    a.dstAlphaBlendFactor = a.dstColorBlendFactor;
    a.srcAlphaBlendFactor = a.srcColorBlendFactor;
    a.alphaBlendOp        = a.colorBlendOp;
    }

  VkPipelineColorBlendStateCreateInfo colorBlending = {};
  colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable     = VK_FALSE;
  colorBlending.logicOp           = VK_LOGIC_OP_COPY;
  colorBlending.attachmentCount   = lay.colorCount;
  colorBlending.pAttachments      = blendAtt;
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

  VkPipelineTessellationStateCreateInfo tesselation = {};
  tesselation.sType                 = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
  tesselation.flags                 = 0;
  if(tp==Triangles)
    tesselation.patchControlPoints = 3; else
    tesselation.patchControlPoints = 2;

  VkPipelineDynamicStateCreateInfo dynamic = {};
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  const VkDynamicState dySt[1]={VK_DYNAMIC_STATE_VIEWPORT};
  dynamic.pDynamicStates    = dySt;
  dynamic.dynamicStateCount = 1;

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
  pipelineInfo.renderPass          = lay.impl;
  pipelineInfo.subpass             = 0;
  pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;

  if(useTesselation) {
    pipelineInfo.pTessellationState = &tesselation;
    // rasterizer.polygonMode = VK_POLYGON_MODE_LINE;
    }

  VkPipeline graphicsPipeline=VK_NULL_HANDLE;
  vkAssert(vkCreateGraphicsPipelines(device,VK_NULL_HANDLE,1,&pipelineInfo,nullptr,&graphicsPipeline));
  return graphicsPipeline;
  }


VCompPipeline::VCompPipeline() {
  }

VCompPipeline::VCompPipeline(VDevice& dev, const VUniformsLay& ulay, VShader& comp)
  :device(dev.device) {
  VkShaderStageFlags pushStageFlags = 0;
  pipelineLayout = VPipeline::initLayout(device,ulay,pushStageFlags);
  ssboBarriers   = ulay.hasSSBO;

  try {
    VkComputePipelineCreateInfo info = {};
    info.sType        = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    info.stage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    info.stage.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    info.stage.module = comp.impl;
    info.stage.pName  = "main";
    info.layout       = pipelineLayout;
    vkAssert(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &info, nullptr, &impl));
    }
  catch(...) {
    vkDestroyPipelineLayout(device,pipelineLayout,nullptr);
    throw;
    }
  }

VCompPipeline::VCompPipeline(VCompPipeline&& other) {
  std::swap(device,         other.device);
  std::swap(impl,           other.impl);
  std::swap(pipelineLayout, other.pipelineLayout);
  std::swap(ssboBarriers,   other.ssboBarriers);
  }

VCompPipeline::~VCompPipeline() {
  if(pipelineLayout==VK_NULL_HANDLE)
    return;
  vkDestroyPipelineLayout(device,pipelineLayout,nullptr);
  vkDestroyPipeline(device,impl,nullptr);
  }

#endif
