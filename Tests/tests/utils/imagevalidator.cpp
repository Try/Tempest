#include "imagevalidator.h"

#include <gtest/gtest.h>

using namespace Tempest;

ImageValidator::Pixel ImageValidator::at(uint32_t x, uint32_t y) const {
  auto d = reinterpret_cast<const uint8_t*>(pm) + ((x+y*width)*bpp);

  switch(frm) {
    case Pixmap::Format::DXT1:
    case Pixmap::Format::DXT3:
    case Pixmap::Format::DXT5:
      assert(false);
      break;
    case Pixmap::Format::R:
      return decode<uint8_t,1>(d);
    case Pixmap::Format::RG:
      return decode<uint8_t,2>(d);
    case Pixmap::Format::RGB:
      return decode<uint8_t,3>(d);
    case Pixmap::Format::RGBA:
      return decode<uint8_t,4>(d);

    case Pixmap::Format::R16:
      return decode<uint16_t,1>(d);
    case Pixmap::Format::RG16:
      return decode<uint16_t,2>(d);
    case Pixmap::Format::RGB16:
      return decode<uint16_t,3>(d);
    case Pixmap::Format::RGBA16:
      return decode<uint16_t,4>(d);

    case Pixmap::Format::R32F:
      return decode<float,1>(d);
    case Pixmap::Format::RG32F:
      return decode<float,2>(d);
    case Pixmap::Format::RGB32F:
      return decode<float,3>(d);
    case Pixmap::Format::RGBA32F:
      return decode<float,4>(d);
    }

  assert(false);
  return Pixel();
  }

template<class T, uint32_t n>
ImageValidator::Pixel ImageValidator::decode(const uint8_t* src) {
  auto t = reinterpret_cast<const T*>(src);
  return decodeT(t,n);
  }

ImageValidator::Pixel ImageValidator::decodeT(const uint8_t* x, uint32_t n) {
  Pixel ret;
  for(uint32_t i=0; i<n; ++i)
    ret.x[i] = x[i]/255.f;
  return ret;
  }

ImageValidator::Pixel ImageValidator::decodeT(const uint16_t* x, uint32_t n) {
  Pixel ret;
  for(uint32_t i=0; i<n; ++i)
    ret.x[i] = x[i]/65535.f;
  return ret;
  }

ImageValidator::Pixel ImageValidator::decodeT(const float* x, uint32_t n) {
  Pixel ret;
  for(uint32_t i=0; i<n; ++i)
    ret.x[i] = x[i];
  return ret;
  }
