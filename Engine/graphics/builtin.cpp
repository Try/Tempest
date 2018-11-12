#include "builtin.h"

#include <Tempest/Device>
#include <Tempest/PaintDevice>

using namespace Tempest;

Builtin::Builtin(Device &owner)
  :owner(owner) {
  vs = owner.loadShader("shader/tex_brush.vert");
  fs = owner.loadShader("shader/tex_brush.frag");

  ulay.add(0,Tempest::UniformsLayout::Texture,Tempest::UniformsLayout::Fragment);
  //ulay.add(1,Tempest::UniformsLayout::Ubo,    Tempest::UniformsLayout::Fragment);
  }

const RenderPipeline &Builtin::brushTexture2d(RenderPass& pass,uint32_t w,uint32_t h) const {
  if(brush.isEmpty() || brush.w()!=w || brush.h()!=h)
    brush = owner.pipeline<PaintDevice::Point>(pass,w,h,ulay,vs,fs);
  //TODO
  return brush;
  }
