#pragma once

#include <cstdint>

namespace Tempest {

class RenderState final {
  public:
    RenderState()=default;

    enum class BlendMode : uint8_t {
      Zero,
      One,
      SrcColor,
      OneMinusSrcColor,
      SrcAlpha,
      OneMinusSrcAlpha,
      DstAlpha,
      OneMinusDstAlpha,
      DstColor,
      OneMinusDstColor,
      SrcAlphaSaturate,
      };

    enum class BlendOp : uint8_t {
      Add,
      Subtract,
      ReverseSubtract,
      Min,
      Max
      };

    enum class ZTestMode : uint8_t {
      Always,
      Never,

      Greater,
      Less,

      GEqual,
      LEqual,

      NOEqual,
      Equal
      };

    enum class CullMode : uint8_t {
      Back=0,
      Front,
      NoCull
      };

    void      setBlendSource(BlendMode s) { blendS=s; }
    void      setBlendDest  (BlendMode d) { blendD=d; }
    void      setBlendOp    (BlendOp   o) { blendO=o; }

    BlendMode blendSource()    const { return blendS; }
    BlendMode blendDest()      const { return blendD; }
    BlendOp   blendOperation() const { return blendO; }

    bool      hasBlend() const { return blendS!=BlendMode::One || blendD!=BlendMode::Zero || !(blendO==BlendOp::Add || blendO==BlendOp::Subtract); }

    void      setZTestMode(ZTestMode z){ zmode=z; }
    ZTestMode zTestMode() const { return zmode; }

    void      setRasterDiscardEnabled(bool e) { discard=e; }
    bool      isRasterDiscardEnabled() const { return discard; }

    void      setZWriteEnabled(bool e) { zdiscard=!e; }
    bool      isZWriteEnabled() const { return !zdiscard; }

    CullMode  cullFaceMode() const { return cull; }
    void      setCullFaceMode(CullMode use) { cull=use; }

  private:
    BlendMode blendS   = BlendMode::One;
    BlendMode blendD   = BlendMode::Zero;
    BlendOp   blendO   = BlendOp::Add;
    ZTestMode zmode    = ZTestMode::Always;
    CullMode  cull     = CullMode::Back;
    bool      discard  = false;
    bool      zdiscard = false;
  };

}
