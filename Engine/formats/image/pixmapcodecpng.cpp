#include "pixmapcodecpng.h"

#include <Tempest/IDevice>
#include <Tempest/ODevice>

#include <png.h>
#include <cstring>

using namespace Tempest;

struct PixmapCodecPng::Impl {
  IDevice* data = nullptr;
  uint8_t* out  = nullptr;

  Impl(IDevice* idev)
    :data(idev) {
    }

  ~Impl(){
    std::free(out);
    }

  bool readPng(png_structp png_ptr, png_infop info_ptr,
               Pixmap::Format& frm, uint32_t& outW, uint32_t& outH, uint32_t& outBpp) {
    if(setjmp(png_jmpbuf(png_ptr))) {
      // png exception
      return false;
      }
    //png_init_io(png_ptr, data);
    png_set_read_fn  (png_ptr, this, &Impl::read );
    png_set_sig_bytes(png_ptr, 8);

    png_read_info(png_ptr, info_ptr);

    outW = png_get_image_width (png_ptr, info_ptr);
    outH = png_get_image_height(png_ptr, info_ptr);

    png_byte color_type = png_get_color_type(png_ptr, info_ptr);
    png_byte bit_depth  = png_get_bit_depth(png_ptr, info_ptr);
    if(bit_depth!=8)
      return false;

    if( color_type == PNG_COLOR_TYPE_PALETTE )
      png_set_palette_to_rgb(png_ptr);

    color_type = png_get_color_type(png_ptr, info_ptr);

    if( color_type==PNG_COLOR_TYPE_RGB ){
      outBpp = 3;
      frm    = Pixmap::Format::RGB;
      }
    else if( color_type==PNG_COLOR_TYPE_RGB_ALPHA ){
      outBpp = 4;
      frm    = Pixmap::Format::RGBA;
      }
    else if( color_type==PNG_COLOR_TYPE_GRAY ){
      png_set_gray_to_rgb(png_ptr);
      outBpp = 3;
      frm    = Pixmap::Format::RGB;
      }
    else if( color_type==PNG_COLOR_TYPE_GRAY_ALPHA ){
      png_set_gray_to_rgb(png_ptr);
      outBpp = 4;
      frm    = Pixmap::Format::RGBA;
      }
    else {
      return false;
      }

    out = reinterpret_cast<uint8_t*>(malloc(outW*outH*outBpp));
    png_set_interlace_handling(png_ptr);
    png_read_update_info(png_ptr, info_ptr);

    // Based on png_read_image(png_ptr,imgRows)
    int pass =  png_set_interlace_handling(png_ptr);
    for(int j = 0; j < pass; j++) {
      for(uint32_t y=0; y<outH; y++) {
        png_bytep rp = &out[y*outW*outBpp];
        png_read_row(png_ptr, rp, nullptr);
        }
      }

    png_read_end(png_ptr, info_ptr);
    return true;
    }

  static void read( png_structp png_ptr,
                    png_bytep   outBytes,
                    png_size_t  byteCountToRead ){
    if(!png_get_io_ptr(png_ptr)){
      return;
      }

    Impl& r = *reinterpret_cast<Impl*>(png_get_io_ptr(png_ptr));
    if(r.data->read(outBytes, byteCountToRead)!=byteCountToRead) {
      png_error(png_ptr,"unable to read input stream");
      }
    }
  };

static void png_write_data(png_structp png_ptr, png_bytep data, png_size_t length){
  ODevice* f = reinterpret_cast<ODevice*>(png_get_io_ptr(png_ptr));
  f->write(data, length);
  }

static void png_flush(png_structp png_ptr){
  ODevice* f = reinterpret_cast<ODevice*>(png_get_io_ptr(png_ptr));
  f->flush();
  }

PixmapCodecPng::PixmapCodecPng() {
  }

bool PixmapCodecPng::testFormat(const PixmapCodec::Context& c) const {
  png_byte head[8];
  return c.peek(head,8)==8 && png_sig_cmp(head, 0, 8)==0;
  }

uint8_t* PixmapCodecPng::load(PixmapCodec::Context& c, uint32_t& w, uint32_t& h,
                              Pixmap::Format& frm, uint32_t& mipCnt, size_t& dataSz, uint32_t& bpp) const {
  auto& f = c.device;
  png_byte head[8];
  if(f.read(head,8)!=8 || png_sig_cmp(head, 0, 8)!=0)
    return nullptr;

  // initialize stuff
  png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if(png_ptr==nullptr)
    return nullptr;

  png_infop info_ptr = png_create_info_struct(png_ptr);
  if(info_ptr==nullptr) {
    png_destroy_read_struct(&png_ptr, nullptr, nullptr);
    return nullptr;
    }

  // work
  Impl r(&f);
  bool readed = r.readPng(png_ptr,info_ptr,frm,w,h,bpp);

  // cleanup
  png_destroy_info_struct(png_ptr, &info_ptr);
  png_destroy_read_struct(&png_ptr, nullptr, nullptr);

  uint8_t* out = nullptr;
  if(readed) {
    out    = r.out;
    mipCnt = 1;
    dataSz = w*h*bpp;
    r.out = nullptr;
    }
  return out;
  }

bool PixmapCodecPng::save(ODevice& f, const char* ext, const uint8_t* data,
                          size_t /*dataSz*/, uint32_t w, uint32_t h, Pixmap::Format frm) const {
  if(ext!=nullptr && std::strcmp("png",ext)!=0)
    return false;

  uint32_t bpp=0;
  if(frm==Pixmap::Format::RGB) {
    bpp = 3;
    }
  else if(frm==Pixmap::Format::RGBA) {
    bpp = 4;
    }
  else {
    return false;
    }

  png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
  if(png_ptr==nullptr)
    return false;

  png_infop   info_ptr = png_create_info_struct(png_ptr);
  if(info_ptr==nullptr) {
    png_destroy_write_struct(&png_ptr,nullptr);
    return false;
    }

  if( setjmp(png_jmpbuf(png_ptr)) ) {
    png_destroy_info_struct(png_ptr, &info_ptr);
    png_destroy_write_struct(&png_ptr,nullptr);
    return false;
    }

  png_set_write_fn(png_ptr, &f, png_write_data, png_flush);

  // write header
  png_set_IHDR( png_ptr, info_ptr, w, h,
                8,//bit_depth,
                (frm==Pixmap::Format::RGB) ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_RGB_ALPHA,
                PNG_INTERLACE_NONE,
                PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

  png_write_info(png_ptr, info_ptr);

  // write bytes
  {
  const int num_pass = png_set_interlace_handling(png_ptr);
  // Loop through passes
  for(int pass = 0; pass < num_pass; pass++) {
    // Loop through image
    for(uint32_t i=0; i<h; i++) {
      const png_byte* rp = &data[i*w*bpp];
      png_write_row(png_ptr, rp);
      }
    }
  png_write_end(png_ptr, nullptr);
  }

  // cleanup
  png_destroy_info_struct(png_ptr, &info_ptr);
  png_destroy_write_struct(&png_ptr,nullptr);
  return true;
  }
