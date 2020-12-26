![Tempest Logo](icon.png)
=
[![Build status](https://ci.appveyor.com/api/projects/status/github/Try/MoltenTempest?svg=true)](https://ci.appveyor.com/project/Try/MoltenTempest)

Crossplatform 3d engine.
(work in progress)

Tempest is an open-source, simple, cross-platform graphics engine written in modern C++14.  
Main idea behind this engine is to provide a low-level GPU-programming concepts, like Ubo, Vbo, Ssbo, in convenient C++ packaging, with RAII, types and templates. 

### Features
* Multiplatform (Windows, Linux, 32bit and 64bit)
* Multibackend (Vulkan 1.0, DirectX12)
* Multithreaded command buffers and thread safety
* Build-in 2d graphics support
* Build-in UI library

### Concept mapping
| Tempest           | Vulkan                                                 | DirectX
|-------------------|--------------------------------------------------------|-----------
| Device            | VkDevice                                               | ID3D12Device
| Swapchain         | VkSwapchain                                            | IDXGISwapChain3 
| Shader            |                                                        | ID3D12Resource
| VertexBuffer<T>   | VkBuffer + VK_BUFFER_USAGE_VERTEX_BUFFER_BIT           | ID3D12Resource 
| IndexBuffer<T>    | VkBuffer + VK_BUFFER_USAGE_INDEX_BUFFER_BIT            | ID3D12Resource 
| UniformBuffer<T>  | VkBuffer + VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT          | ID3D12Resource 
| StorageBuffer<T>  | VkBuffer + VK_BUFFER_USAGE_STORAGE_BUFFER_BIT          | ID3D12Resource + D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS 
| Uniforms          | VkDescriptorSet                                        | ID3D12DescriptorHeap[]
| Attachment        | VkImage  + VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT         | ID3D12Resource + D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
| ZBuffer           | VkImage  + VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | ID3D12Resource + D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
| StorageImage      | VkImage  +VK_IMAGE_USAGE_STORAGE_BIT                   | ID3D12Resource + D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
| FrameBuffer       | VkFramebuffer                                          | ID3D12DescriptorHeap + D3D12_DESCRIPTOR_HEAP_TYPE_RTV
| RenderPass        | VkRenderPass                                           | ClearRenderTargetView/ClearDepthStencilView/DiscardResource
| RenderPipeline<V> | VkPipeline[]                                           | ID3D12PipelineState
| ComputePipeline   | VkPipeline                                             | ID3D12PipelineState
| Fence             | VkFence                                                | n/a
| Semaphore         | VkSemaphore                                            | n/a
| CommandBuffer     | VkCommandPool                                          | ID3D12CommandList

### Examples
```c++
// offscreen render
VulkanApi api;
Device    device(api);

static const Vertex   vboData[3] = {{-1,-1},{1,-1},{1,1}};
static const uint16_t iboData[3] = {0,1,2};

auto vbo  = device.vbo(vboData,3);
auto ibo  = device.ibo(iboData,3);

auto vert = device.loadShader("shader/simple_test.vert.sprv");
auto frag = device.loadShader("shader/simple_test.frag.sprv");
auto pso  = device.pipeline<Vertex>(Topology::Triangles,RenderState(),vert,frag);

auto tex  = device.attachment(format,128,128);
auto fbo  = device.frameBuffer(tex);
auto rp   = device.pass(FboMode(FboMode::PreserveOut,Color(0.f,0.f,1.f)));

auto cmd  = device.commandBuffer();
{
  auto enc = cmd.startEncoding(device);
  enc.setFramebuffer(fbo,rp);
  enc.setUniforms(pso);
  enc.draw(vbo,ibo);
}

auto sync = device.fence();
device.submit(cmd,sync);
sync.wait();

// save image to file
auto pm = device.readPixels(tex);
pm.save(outImg);
```
