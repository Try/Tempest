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

auto vert = device.loadShader("shader/simple_test.vert.sprv");
auto frag = device.loadShader("shader/simple_test.frag.sprv");
auto pso  = device.pipeline<Vertex>(Topology::Triangles,RenderState(),vert,frag);

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
