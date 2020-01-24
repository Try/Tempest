#include "builtin.h"

#include <Tempest/Device>
#include <Tempest/PaintDevice>

#include "shaders/shaders.inl"

using namespace Tempest;

Builtin::Builtin(Device& owner)
  :owner(owner) {
  stBlend.setBlendSource(RenderState::BlendMode::src_alpha);
  stBlend.setBlendDest  (RenderState::BlendMode::one_minus_src_alpha);

  stAlpha.setBlendSource(RenderState::BlendMode::one);
  stAlpha.setBlendDest  (RenderState::BlendMode::one);

  vsE  = owner.shader(reinterpret_cast<const char*>(empty_vert_sprv),sizeof(empty_vert_sprv));
  fsE  = owner.shader(reinterpret_cast<const char*>(empty_frag_sprv),sizeof(empty_frag_sprv));

  vsT2 = owner.shader(reinterpret_cast<const char*>(tex_brush_vert_sprv),sizeof(tex_brush_vert_sprv));
  fsT2 = owner.shader(reinterpret_cast<const char*>(tex_brush_frag_sprv),sizeof(tex_brush_frag_sprv));

  brushT2.layout.add(0,Tempest::UniformsLayout::Texture,Tempest::UniformsLayout::Fragment);
  }

const Builtin::Item &Builtin::texture2d() const {
  if(brushT2.brush.isEmpty()) {
    brushT2.pen    = owner.pipeline<PaintDevice::Point>(Lines,    stNormal,brushT2.layout,vsT2,fsT2);
    brushT2.brush  = owner.pipeline<PaintDevice::Point>(Triangles,stNormal,brushT2.layout,vsT2,fsT2);

    brushT2.penB   = owner.pipeline<PaintDevice::Point>(Lines,    stBlend,brushT2.layout,vsT2,fsT2);
    brushT2.brushB = owner.pipeline<PaintDevice::Point>(Triangles,stBlend,brushT2.layout,vsT2,fsT2);

    brushT2.penA   = owner.pipeline<PaintDevice::Point>(Lines,    stAlpha,brushT2.layout,vsT2,fsT2);
    brushT2.brushA = owner.pipeline<PaintDevice::Point>(Triangles,stAlpha,brushT2.layout,vsT2,fsT2);
    }
  return brushT2;
  }

const Builtin::Item &Builtin::empty() const {
  if(brushE.brush.isEmpty()) {
    brushE.pen   = owner.pipeline<PaintDevice::Point>(Lines,    stNormal,brushE.layout,vsE,fsE);
    brushE.brush = owner.pipeline<PaintDevice::Point>(Triangles,stNormal,brushE.layout,vsE,fsE);

    brushE.penB   = owner.pipeline<PaintDevice::Point>(Lines,    stBlend,brushE.layout,vsE,fsE);
    brushE.brushB = owner.pipeline<PaintDevice::Point>(Triangles,stBlend,brushE.layout,vsE,fsE);

    brushE.penA   = owner.pipeline<PaintDevice::Point>(Lines,    stAlpha,brushE.layout,vsE,fsE);
    brushE.brushA = owner.pipeline<PaintDevice::Point>(Triangles,stAlpha,brushE.layout,vsE,fsE);
    }
  return brushE;
  }

void Builtin::reset() const {
  brushT2.brush =RenderPipeline();
  brushT2.pen   =RenderPipeline();
  brushT2.brushA=RenderPipeline();
  brushT2.penA  =RenderPipeline();
  brushE.brush  =RenderPipeline();
  brushE.pen    =RenderPipeline();
  brushE.brushA =RenderPipeline();
  brushE.penA   =RenderPipeline();
  }
