#include "builtin.h"

#include <Tempest/Device>
#include <Tempest/PaintDevice>

#include "builtin_shader.h"

using namespace Tempest;

Builtin::Builtin(Device& device)
  : device(device) {
  static bool internalShaders = true;
  if(internalShaders) {
    brushE  = mkShaderSet(false);
    brushT2 = mkShaderSet(true);
    }
  }

Builtin::Item Builtin::mkShaderSet(bool textures) {
  Tempest::Shader vs, fs;
  if(textures) {
    vs = device.shader(tex_brush_vert_sprv,sizeof(tex_brush_vert_sprv));
    fs = device.shader(tex_brush_frag_sprv,sizeof(tex_brush_frag_sprv));
    } else {
    vs = device.shader(empty_vert_sprv,    sizeof(empty_vert_sprv));
    fs = device.shader(empty_frag_sprv,    sizeof(empty_frag_sprv));
    }

  RenderState stNormal, stBlend, stAlpha;
  stNormal.setZWriteEnabled(false);

  stBlend.setBlendSource  (RenderState::BlendMode::SrcAlpha);
  stBlend.setBlendDest    (RenderState::BlendMode::OneMinusSrcAlpha);
  stBlend.setZWriteEnabled(false);

  stAlpha.setBlendSource  (RenderState::BlendMode::One);
  stAlpha.setBlendDest    (RenderState::BlendMode::One);
  stAlpha.setZWriteEnabled(false);

  Item ret;
  ret.pen    = device.pipeline<PaintDevice::Point>(Lines,    stNormal,vs,fs);
  ret.brush  = device.pipeline<PaintDevice::Point>(Triangles,stNormal,vs,fs);

  ret.penB   = device.pipeline<PaintDevice::Point>(Lines,    stBlend,vs,fs);
  ret.brushB = device.pipeline<PaintDevice::Point>(Triangles,stBlend,vs,fs);

  ret.penA   = device.pipeline<PaintDevice::Point>(Lines,    stAlpha,vs,fs);
  ret.brushA = device.pipeline<PaintDevice::Point>(Triangles,stAlpha,vs,fs);
  return ret;
  }
