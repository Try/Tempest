#include "builtin.h"

#include <Tempest/Device>
#include <Tempest/PaintDevice>

#include "shaders/shaders.inl"

using namespace Tempest;

Builtin::Builtin(Device &owner)
  :owner(owner) {
  vsE  = owner.loadShader(reinterpret_cast<const char*>(empty_vert_sprv),sizeof(empty_vert_sprv));
  fsE  = owner.loadShader(reinterpret_cast<const char*>(empty_frag_sprv),sizeof(empty_frag_sprv));

  vsT2 = owner.loadShader(reinterpret_cast<const char*>(tex_brush_vert_sprv),sizeof(tex_brush_vert_sprv));
  fsT2 = owner.loadShader(reinterpret_cast<const char*>(tex_brush_frag_sprv),sizeof(tex_brush_frag_sprv));

  brushT2.layout.add(0,Tempest::UniformsLayout::Texture,Tempest::UniformsLayout::Fragment);
  }

const Builtin::Item &Builtin::texture2d(RenderPass& pass,uint32_t w,uint32_t h) const {
  if(brushT2.brush.isEmpty() || brushT2.brush.w()!=w || brushT2.brush.h()!=h) {
    brushT2.pen   = owner.pipeline<PaintDevice::Point>(pass,w,h,Lines,    brushT2.layout,vsT2,fsT2);
    brushT2.brush = owner.pipeline<PaintDevice::Point>(pass,w,h,Triangles,brushT2.layout,vsT2,fsT2);
    }
  //TODO
  return brushT2;
  }

const Builtin::Item &Builtin::empty(RenderPass &pass, uint32_t w, uint32_t h) const {
  if(brushE.brush.isEmpty() || brushE.brush.w()!=w || brushE.brush.h()!=h) {
    brushE.pen   = owner.pipeline<PaintDevice::Point>(pass,w,h,Lines,    brushE.layout,vsE,fsE);
    brushE.brush = owner.pipeline<PaintDevice::Point>(pass,w,h,Triangles,brushE.layout,vsE,fsE);
    }
  //TODO
  return brushE;
  }

void Builtin::reset() const {
  brushT2.brush=RenderPipeline();
  brushT2.pen  =RenderPipeline();
  brushE.brush =RenderPipeline();
  brushE.pen   =RenderPipeline();
  }
