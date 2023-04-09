![Tempest Logo](icon.png)
=
[![Build status](https://ci.appveyor.com/api/projects/status/github/Try/Tempest?svg=true)](https://ci.appveyor.com/project/Try/Tempest)

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

### Examples
```c++
// offscreen render
VulkanApi api;
Device    device(api);

static const Vertex   vboData[3] = {{-1,-1},{1,-1},{1,1}};
static const uint16_t iboData[3] = {0,1,2};

auto vbo  = device.vbo(vboData,3);
auto ibo  = device.ibo(iboData,3);

auto vert = device.shader("shader/simple_test.vert.sprv");
auto frag = device.shader("shader/simple_test.frag.sprv");
auto pso  = device.pipeline(Topology::Triangles,RenderState(),vert,frag);

auto tex  = device.attachment(format,128,128);

auto cmd  = device.commandBuffer();
{
  auto enc = cmd.startEncoding(device);
  enc.setFramebuffer({{tex,Vec4(0,0,1,1),Tempest::Preserve}});
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

### Ecosystem
During development various issues of Vulkan stack been found, reported and some were fixed. 

#### Contribution 
* - [x] State tracking of VkCopyDescriptorSet for KHR-acceleration-structure  
https://github.com/KhronosGroup/Vulkan-ValidationLayers/pull/4219

* - [x] HLSL: Add mesh shader support  
https://github.com/KhronosGroup/SPIRV-Cross/pull/2052

* - [x] HLSL: Add task(amplification) shader support  
https://github.com/KhronosGroup/SPIRV-Cross/pull/2124

* - [x] Mesh shader: fix implicit index-array size calculation for lines and triangles  
https://github.com/KhronosGroup/glslang/pull/3050

* - [ ] Fix crash in HLSL frontend  
https://github.com/KhronosGroup/glslang/pull/2916

#### Issues

* - [x] [HLSL] SPIRV_Cross_VertexInfo is not relieble  
https://github.com/KhronosGroup/SPIRV-Cross/issues/2032

* - [x] MSL: invalid codegen, when using rayquery  
https://github.com/KhronosGroup/SPIRV-Cross/issues/1910

* - [x] HLSL: link error: Signatures between stages are incompatible  
https://github.com/KhronosGroup/SPIRV-Cross/issues/1691

* - [x] HLSL: error X4503: output TEXCOORD2 used more than once  
https://github.com/KhronosGroup/SPIRV-Cross/issues/1645

* - [ ] MSL: RayQuery implementation is incomplete  
https://github.com/KhronosGroup/SPIRV-Cross/issues/2115

* - [ ] SPIR-V -> HLSL : cross compiling bindings overlap  
https://github.com/KhronosGroup/SPIRV-Cross/issues/2064

* - [ ] Component swizzling of gl_MeshVerticesEXT[].gl_Position, produces invalid code  
https://github.com/KhronosGroup/glslang/issues/3058

* - [ ] HLSL: crash, while compiling HULL shader  
https://github.com/KhronosGroup/glslang/issues/2914

