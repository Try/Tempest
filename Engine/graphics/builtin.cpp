#include "builtin.h"

#include <Tempest/Device>
#include <Tempest/PaintDevice>

#include "shaders/shaders.inl"

using namespace Tempest;

Builtin::Builtin(Device &owner)
  :owner(owner) {
  vs = owner.loadShader(reinterpret_cast<const char*>(tex_brush_vert_sprv),sizeof(tex_brush_vert_sprv));
  fs = owner.loadShader(reinterpret_cast<const char*>(tex_brush_frag_sprv),sizeof(tex_brush_frag_sprv));

  ulay.add(0,Tempest::UniformsLayout::Texture,Tempest::UniformsLayout::Fragment);
  //ulay.add(1,Tempest::UniformsLayout::Ubo,    Tempest::UniformsLayout::Fragment);
  }

const RenderPipeline& Builtin::brushTexture2d(RenderPass& pass,uint32_t w,uint32_t h) const {
  if(brush.isEmpty() || brush.w()!=w || brush.h()!=h)
    brush = owner.pipeline<PaintDevice::Point>(pass,w,h,Triangles,ulay,vs,fs);
  //TODO
  return brush;
  }

const RenderPipeline& Builtin::penTexture2d(RenderPass &pass, uint32_t w, uint32_t h) const {
  if(pen.isEmpty() || pen.w()!=w || pen.h()!=h)
    pen = owner.pipeline<PaintDevice::Point>(pass,w,h,Lines,ulay,vs,fs);
  //TODO
  return pen;
  }

void Builtin::reset() const {
  pen  =RenderPipeline();
  brush=RenderPipeline();
  }
