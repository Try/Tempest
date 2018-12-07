#pragma once

#include <cstdint>

namespace Tempest {

class RenderState final {
  public:
    RenderState()=default;

    //! Режим альфа смешивания.
    enum class BlendMode : uint8_t {
      zero,                 //GL_ZERO,
      one,                  //GL_ONE,
      src_color,            //GL_SRC_COLOR,
      one_minus_src_color,  //GL_ONE_MINUS_SRC_COLOR,
      src_alpha,            //GL_SRC_ALPHA,
      one_minus_src_alpha,  //GL_ONE_MINUS_SRC_ALPHA,
      dst_alpha,            //GL_DST_ALPHA,
      one_minus_dst_alpha,  //GL_ONE_MINUS_DST_ALPHA,
      dst_color,            //GL_DST_COLOR,
      one_minus_dst_color,  //GL_ONE_MINUS_DST_COLOR,
      src_alpha_saturate,   //GL_SRC_ALPHA_SATURATE,
      Count
      };

    void setBlendSource(BlendMode s) { blendS=s; }
    void setBlendDest  (BlendMode d) { blendD=d; }

    BlendMode blendSource() const { return blendS; }
    BlendMode blendDest()   const { return blendD; }

    bool hasBlend() const { return blendS!=BlendMode::one || blendD!=BlendMode::zero; }

  private:
    BlendMode blendS=BlendMode::one;
    BlendMode blendD=BlendMode::zero;
  };

}
