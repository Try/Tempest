#include "pixmapcodecdds.h"

#include <Tempest/IDevice>

#include <algorithm>
#include <cstring>
#include <squish.h>
#include "../ddsdef.h"

using namespace Tempest;

PixmapCodecDDS::PixmapCodecDDS() {  
  }

bool PixmapCodecDDS::testFormat(const PixmapCodec::Context &c) const {
  char buf[4]={};
  return c.peek(buf,4)==4 && std::memcmp(buf,"DDS ",4)==0;
  }

uint8_t* PixmapCodecDDS::load(PixmapCodec::Context &c, uint32_t &ow, uint32_t &oh,
                              TextureFormat& frm, uint32_t& mipCnt, size_t& dataSz, uint32_t &bpp) const {
  using namespace Tempest::Detail;

  auto& f = c.device;
  uint8_t head[4]={};
  if(f.read(head,4)!=4)
    return nullptr;

  DDSURFACEDESC2 ddsd={};
  if(f.read(&ddsd,sizeof(ddsd))!=sizeof(ddsd))
    return nullptr;
  ow = ddsd.dwWidth;
  oh = ddsd.dwHeight;

  int compressType = squish::kDxt1;
  switch(ddsd.ddpfPixelFormat.dwFourCC) {
    case FOURCC_DXT1:
      bpp          = 3;
      compressType = squish::kDxt1;
      frm          = TextureFormat::DXT1;
      break;

    case FOURCC_DXT3:
      bpp          = 4;
      compressType = squish::kDxt3;
      frm          = TextureFormat::DXT3;
      break;

    case FOURCC_DXT5:
      bpp          = 4;
      compressType = squish::kDxt5;
      frm          = TextureFormat::DXT5;
      break;

    default:
      return nullptr;
    }

  if( ddsd.dwLinearSize == 0 ) {
    //return false;
    }

  mipCnt            = std::max(1u, ddsd.dwMipMapCount);
  size_t blocksize  = (compressType==squish::kDxt1) ? 8 : 16;
  size_t bufferSize = 0;

  size_t w = size_t(ow), h = size_t(oh);
  for(size_t i=0; i<mipCnt; i++){
    size_t blockcount = ((w+3)/4)*((h+3)/4);
    bufferSize += blockcount*blocksize;
    w = std::max<size_t>(1,w/2);
    h = std::max<size_t>(1,h/2);
    }

  uint8_t* ddsv = reinterpret_cast<uint8_t*>(std::malloc(bufferSize));
  if(!ddsv || f.read(ddsv,bufferSize)!=bufferSize) {
    std::free(ddsv);
    return nullptr;
    }

  bpp    = 0;
  dataSz = bufferSize;
  return ddsv;
  }

bool PixmapCodecDDS::save(ODevice &, const char* /*ext*/, const uint8_t *data, size_t dataSz,
                          uint32_t w, uint32_t h, TextureFormat frm) const {
  return false;
  }
