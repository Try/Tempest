#include "pixmapcodechdr.h"

#include <Tempest/IDevice>

#include <algorithm>
#include <cstring>

using namespace Tempest;

static void rgbe2float(float& red, float& green, float& blue, uint8_t rgbe[4]) {
  if(rgbe[3]) {
    // nonzero pixel
    float f = float(std::ldexp(1.0,rgbe[3]-(int)(128+8)));
    red   = rgbe[0] * f;
    green = rgbe[1] * f;
    blue  = rgbe[2] * f;
    } else {
    red = green = blue = 0.0;
    }
  }


PixmapCodecHDR::PixmapCodecHDR() {
  }

bool PixmapCodecHDR::testFormat(const PixmapCodec::Context &c) const {
  char buf[11]={};
  return c.peek(buf,11)==11 && std::memcmp(buf,"#?RADIANCE\n",11)==0;
  }

uint8_t* PixmapCodecHDR::load(PixmapCodec::Context &c, uint32_t &ow, uint32_t &oh,
                              Pixmap::Format& frm, uint32_t& mipCnt, size_t& dataSz, uint32_t &bpp) const {
  char buf[256] = {};
  if(!readToken(c.device,buf,256))
    return nullptr;
  if(std::strcmp(buf,"#?RADIANCE")!=0)
    return nullptr;

  // header
  for(;;) {
    if(!readToken(c.device,buf,256))
      return nullptr;
    if(buf[0]=='\0')
      break;
    // comment
    if(buf[0]=='#')
      continue;
    // internal format
    if(std::memcmp(buf,"FORMAT=",7)==0 && std::strcmp(buf,"FORMAT=32-bit_rle_rgbe")!=0)
      return nullptr;
    }

  if(!readToken(c.device,buf,256))
    return nullptr;

  int width = 0, height = 0;
  std::sscanf(buf,"-Y %d +X %d", &height, &width);
  if(width<=0 || height<=0)
    return nullptr;

  bpp    = 3*sizeof(float);
  dataSz = width*height*bpp;
  float* pixels = (float*)std::malloc(dataSz);
  if(!readDataRLE(c.device,pixels,width,height)) {
    std::free(pixels);
    return nullptr;
    }

  frm    = Pixmap::Format::RGB32F;
  ow     = width;
  oh     = height;
  mipCnt = 1;
  return reinterpret_cast<uint8_t*>(pixels);
  }

bool PixmapCodecHDR::save(ODevice &, const char* /*ext*/, const uint8_t*, size_t,
                          uint32_t, uint32_t, Pixmap::Format) const {
  return false;
  }

bool PixmapCodecHDR::readToken(IDevice& d, char* out, size_t maxSz) {
  size_t sz = d.read(out,maxSz);
  for(size_t i=0; i<sz; ++i) {
    if(out[i]=='\0' || out[i]=='\n') {
      d.unget(sz-i-1);
      for(; i<sz; ++i)
        out[i] = '\0';
      return true;
      }
    }
  return false;
  }

bool PixmapCodecHDR::readData(IDevice& d, float* data, size_t count) {
  uint8_t* rgbe = reinterpret_cast<uint8_t*>(data) + count*sizeof(float)*3 - count*4;
  if(d.read(rgbe,count*4)!=count*4)
    return false;

  for(size_t i=0; i<count; ++i) {
    float rgba[3];
    rgbe2float(rgba[0],rgba[1],rgba[2],rgbe+i*4);
    std::memcpy(data+i*3, rgba, 3*sizeof(float));
    }

  return true;
  }

bool PixmapCodecHDR::readDataRLE(IDevice& d, float* data, size_t width, size_t height) {
  if(width<8 || width>0x7fff)
    return readData(d,data,width*height);

  uint8_t* buffer = (uint8_t*)std::malloc(width*4);
  if(buffer==nullptr)
    return false;

  for(size_t h=0; h<height; ++h) {
    uint8_t rgbe[4];
    if(d.read(rgbe,4)!=4) {
      std::free(buffer);
      return false;
      }

    if(rgbe[0]!=2 || rgbe[1]!=2 || (rgbe[2] & 0x80)!=0) {
      // non compressed
      rgbe2float(data[0],data[1],data[2],rgbe);
      std::free(buffer);
      return readData(d,data+3,width*height-1);
      }

    const size_t len = (size_t(rgbe[2])<<8) | size_t(rgbe[3]);
    if(len!=width) {
      std::free(buffer);
      return false;
      }

    for(int i=0; i<4; i++) {
      uint8_t* ptr     = buffer + (i+0)*width;
      uint8_t* ptr_end = buffer + (i+1)*width;

      while(ptr < ptr_end) {
        uint8_t buf[2];
        if(d.read(buf,2)!=2) {
          std::free(buffer);
          return false;
          }

        const bool isRun = (buf[0]>128);
        const int  count = (isRun ? (buf[0]-128) : buf[0]);
        if(count==0 || ptr_end-ptr<count) {
          std::free(buffer);
          return false;
          }

        if(isRun) {
          std::memset(ptr,buf[1],count);
          ptr += count;
          } else {
          *ptr = buf[1];
          if(count>1) {
            if(d.read(ptr+1,count-1)!=size_t(count-1)) {
              std::free(buffer);
              return false;
              }
            }
          ptr += count;
          }
        }
      }

    for(size_t i=0; i<width; ++i) {
      rgbe[0] = buffer[i+0*width];
      rgbe[1] = buffer[i+1*width];
      rgbe[2] = buffer[i+2*width];
      rgbe[3] = buffer[i+3*width];
      rgbe2float(data[0],data[1],data[2],rgbe);
      data += 3;
      }
    }
  free(buffer);
  return true;
  }
