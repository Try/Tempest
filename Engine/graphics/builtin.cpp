#include "builtin.h"

#include <Tempest/Device>
#include <Tempest/PaintDevice>

#include "builtin_shader.h"

using namespace Tempest;

Builtin::Builtin(Device& owner)
  :owner(owner) {
  stBlend.setBlendSource  (RenderState::BlendMode::SrcAlpha);
  stBlend.setBlendDest    (RenderState::BlendMode::OneMinusSrcAlpha);
  stBlend.setZWriteEnabled(false);

  stAlpha.setBlendSource  (RenderState::BlendMode::One);
  stAlpha.setBlendDest    (RenderState::BlendMode::One);
  stAlpha.setZWriteEnabled(false);

  static bool internalShaders = true;
  if(internalShaders) {
    vsE  = owner.shader(empty_vert_sprv,sizeof(empty_vert_sprv));
    fsE  = owner.shader(empty_frag_sprv,sizeof(empty_frag_sprv));

    vsT2 = owner.shader(tex_brush_vert_sprv,sizeof(tex_brush_vert_sprv));
    fsT2 = owner.shader(tex_brush_frag_sprv,sizeof(tex_brush_frag_sprv));
    }
  }

const Builtin::Item &Builtin::texture2d() const {
  if(brushT2.brush.isEmpty()) {
    brushT2.pen    = owner.pipeline<PaintDevice::Point>(Lines,    stNormal,vsT2,fsT2);
    brushT2.brush  = owner.pipeline<PaintDevice::Point>(Triangles,stNormal,vsT2,fsT2);

    brushT2.penB   = owner.pipeline<PaintDevice::Point>(Lines,    stBlend,vsT2,fsT2);
    brushT2.brushB = owner.pipeline<PaintDevice::Point>(Triangles,stBlend,vsT2,fsT2);

    brushT2.penA   = owner.pipeline<PaintDevice::Point>(Lines,    stAlpha,vsT2,fsT2);
    brushT2.brushA = owner.pipeline<PaintDevice::Point>(Triangles,stAlpha,vsT2,fsT2);
    }
  return brushT2;
  }

const Builtin::Item &Builtin::empty() const {
  if(brushE.brush.isEmpty()) {
    brushE.pen   = owner.pipeline<PaintDevice::Point>(Lines,    stNormal,vsE,fsE);
    brushE.brush = owner.pipeline<PaintDevice::Point>(Triangles,stNormal,vsE,fsE);

    brushE.penB   = owner.pipeline<PaintDevice::Point>(Lines,    stBlend,vsE,fsE);
    brushE.brushB = owner.pipeline<PaintDevice::Point>(Triangles,stBlend,vsE,fsE);

    brushE.penA   = owner.pipeline<PaintDevice::Point>(Lines,    stAlpha,vsE,fsE);
    brushE.brushA = owner.pipeline<PaintDevice::Point>(Triangles,stAlpha,vsE,fsE);
    }
  return brushE;
  }
