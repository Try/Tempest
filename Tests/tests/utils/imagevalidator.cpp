#include "imagevalidator.h"

#include <gtest/gtest.h>

using namespace Tempest;

ImageValidator::Pixel ImageValidator::at(uint32_t x, uint32_t y) const {
  auto d = reinterpret_cast<const uint8_t*>(pm) + ((x+y*width)*bpp);

  switch(frm) {
    case TextureFormat::Undefined:
    case TextureFormat::Last:
      assert(false);
      break;
    case TextureFormat::R8:
      return decode<uint8_t,1>(d);
    case TextureFormat::RG8:
      return decode<uint8_t,2>(d);
    case TextureFormat::RGB8:
      return decode<uint8_t,3>(d);
    case TextureFormat::RGBA8:
      return decode<uint8_t,4>(d);

    case TextureFormat::R16:
      return decode<uint16_t,1>(d);
    case TextureFormat::RG16:
      return decode<uint16_t,2>(d);
    case TextureFormat::RGB16:
      return decode<uint16_t,3>(d);
    case TextureFormat::RGBA16:
      return decode<uint16_t,4>(d);

    case TextureFormat::R32U:
      return decode<uint32_t,1>(d);
    case TextureFormat::RG32U:
      return decode<uint32_t,2>(d);
    case TextureFormat::RGB32U:
      return decode<uint32_t,3>(d);
    case TextureFormat::RGBA32U:
      return decode<uint32_t,4>(d);

    case TextureFormat::R32F:
      return decode<float,1>(d);
    case TextureFormat::RG32F:
      return decode<float,2>(d);
    case TextureFormat::RGB32F:
      return decode<float,3>(d);
    case TextureFormat::RGBA32F:
      return decode<float,4>(d);

    case TextureFormat::Depth16:
      return decode<uint16_t,1>(d);
    case TextureFormat::Depth24x8:
    case TextureFormat::Depth24S8:
      return decodeD24(reinterpret_cast<const uint32_t*>(d));
    case TextureFormat::Depth32F:
      return decode<float,1>(d);

    case TextureFormat::DXT1:
    case TextureFormat::DXT3:
    case TextureFormat::DXT5:
      assert(false);
      break;

    case TextureFormat::R11G11B10UF:
    case TextureFormat::RGBA16F:
      assert(false);
      break;
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

ImageValidator::Pixel ImageValidator::decodeT(const uint32_t* x, uint32_t n) {
  Pixel ret;
  for(uint32_t i=0; i<n; ++i)
    ret.x[i] = float(x[i]);
  return ret;
  }

ImageValidator::Pixel ImageValidator::decodeT(const float* x, uint32_t n) {
  Pixel ret;
  for(uint32_t i=0; i<n; ++i)
    ret.x[i] = x[i];
  return ret;
  }

ImageValidator::Pixel ImageValidator::decodeD24(const uint32_t* x) {
  Pixel ret = {};
  ret.x[0] = float(x[0] & 0x00FFFFFF)/float(0xFFFFFF);
  ret.x[1] = float((x[0] & 0xFF000000) >> 24);
  return ret;
  }
