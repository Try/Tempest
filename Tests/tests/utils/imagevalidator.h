#pragma once

#include <Tempest/Pixmap>
#include <cstdint>

class ImageValidator {
  public:
    ImageValidator(const Tempest::Pixmap& pm)
      :pm(pm.data()),frm(pm.format()),bpp(pm.bpp()),width(pm.w()) {
      }
    ImageValidator(const std::vector<Tempest::Vec4>& pm, uint32_t w)
      :pm(pm.data()),frm(Tempest::Pixmap::Format::RGBA32F),bpp(16),width(w) {
      }

    struct Pixel {
      float x[4] = {};
      };

    Pixel at(uint32_t x, uint32_t y) const;

  private:
    template<class T, uint32_t n>
    static Pixel decode(const uint8_t* src);

    static Pixel decodeT(const uint8_t*  x, uint32_t n);
    static Pixel decodeT(const uint16_t* x, uint32_t n);
    static Pixel decodeT(const float*    x, uint32_t n);

    const void*              pm    = nullptr;
    Tempest::Pixmap::Format  frm   = Tempest::Pixmap::Format::RGBA32F;
    uint32_t                 bpp   = 0;
    uint32_t                 width = 0;
  };

