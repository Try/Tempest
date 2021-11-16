#pragma once

#include <Tempest/Pixmap>
#include <cstdint>

class ImageValidator {
  public:
    ImageValidator(const Tempest::Pixmap& pm):pm(pm),frm(pm.format()),bpp(pm.bpp()){}

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

    const Tempest::Pixmap&   pm;
    Tempest::Pixmap::Format  frm;
    uint32_t                 bpp = 0;
  };

