#include "vpipeline.h"

#include "vdevice.h"
#include "vrenderpass.h"
#include "vshader.h"

#include <algorithm>

using namespace Tempest::Detail;

VPipeline::VPipeline(){
  }

VPipeline::VPipeline(VDevice& device,VRenderPass& pass,uint32_t width,uint32_t height,VShader& vert,VShader& frag)
  : device(device.device) {
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

  /*
  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
  vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.vertexBindingDescriptionCount   = 0;
  vertexInputInfo.vertexAttributeDescriptionCount = 0;*/

  VkVertexInputBindingDescription vk_vertexInputBindingDescription;
  vk_vertexInputBindingDescription.binding   = 0;
  vk_vertexInputBindingDescription.stride    = sizeof(float)*2;
  vk_vertexInputBindingDescription.inputRate = VkVertexInputRate::VK_VERTEX_INPUT_RATE_VERTEX;

  VkVertexInputAttributeDescription vk_vertexInputAttributeDescription;
  vk_vertexInputAttributeDescription.location = 0;
  vk_vertexInputAttributeDescription.binding  = 0;
  vk_vertexInputAttributeDescription.format   = VkFormat::VK_FORMAT_R32G32_SFLOAT;
  vk_vertexInputAttributeDescription.offset   = 0;

  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
  vertexInputInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertexInputInfo.pNext = nullptr;
  vertexInputInfo.flags = 0;
  vertexInputInfo.vertexBindingDescriptionCount   = 1;
  vertexInputInfo.pVertexBindingDescriptions      = &vk_vertexInputBindingDescription;
  vertexInputInfo.vertexAttributeDescriptionCount = 1;
  vertexInputInfo.pVertexAttributeDescriptions    = &vk_vertexInputAttributeDescription;


  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
  inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

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

  VkPipelineRasterizationStateCreateInfo rasterizer = {};
  rasterizer.sType                   = VkStructureType::VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.pNext                   = nullptr;
  rasterizer.flags                   = 0;
  rasterizer.depthClampEnable        = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode             = VkPolygonMode::VK_POLYGON_MODE_FILL;
  rasterizer.cullMode                = VkCullModeFlagBits::VK_CULL_MODE_NONE;
  rasterizer.frontFace               = VkFrontFace::VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterizer.depthBiasEnable         = VK_FALSE;
  rasterizer.depthBiasConstantFactor = 0.0f;
  rasterizer.depthBiasClamp          = 0.0f;
  rasterizer.depthBiasSlopeFactor    = 0.0f;
  rasterizer.lineWidth               = 1.0f;

  VkPipelineMultisampleStateCreateInfo multisampling = {};
  multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable  = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
  colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
  colorBlendAttachment.blendEnable    = VK_FALSE;

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

  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
  pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipelineLayoutInfo.setLayoutCount         = 0;
  pipelineLayoutInfo.pushConstantRangeCount = 0;

  if(vkCreatePipelineLayout(device.device,&pipelineLayoutInfo,nullptr,&pipelineLayout)!=VK_SUCCESS)
    throw std::runtime_error("failed to create pipeline layout!");

  VkGraphicsPipelineCreateInfo pipelineInfo = {};
  pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount          = 2;
  pipelineInfo.pStages             = shaderStages;
  pipelineInfo.pVertexInputState   = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState      = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState   = &multisampling;
  pipelineInfo.pColorBlendState    = &colorBlending;
  pipelineInfo.layout              = pipelineLayout;
  pipelineInfo.renderPass          = pass.renderPass;
  pipelineInfo.subpass             = 0;
  pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;

  if(vkCreateGraphicsPipelines(device.device,VK_NULL_HANDLE,1,&pipelineInfo,nullptr,&graphicsPipeline)!=VK_SUCCESS)
    throw std::runtime_error("failed to create graphics pipeline!");
  }

VPipeline::VPipeline(VPipeline &&other) {
  std::swap(device,other.device);
  std::swap(graphicsPipeline,other.graphicsPipeline);
  std::swap(pipelineLayout,  other.pipelineLayout);
  }

VPipeline::~VPipeline() {
  if(pipelineLayout==VK_NULL_HANDLE)
    return;
  vkDeviceWaitIdle(device);
  vkDestroyPipeline      (device,graphicsPipeline,nullptr);
  vkDestroyPipelineLayout(device,pipelineLayout,nullptr);
  }

void VPipeline::operator=(VPipeline &&other) {
  std::swap(device,other.device);
  std::swap(graphicsPipeline,other.graphicsPipeline);
  std::swap(pipelineLayout,  other.pipelineLayout);
  }
