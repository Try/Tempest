#pragma once

#include <memory>
#include <string>

#include <Tempest/AbstractGraphicsApi>

namespace Tempest {

class IDevice;
class ODevice;

class Pixmap final {
  public:
    enum class Format : uint8_t {
      R       = 0,
      RG      = 1,
      RGB     = 2,
      RGBA    = 3,
      R16     = 4,
      RG16    = 5,
      RGB16   = 6,
      RGBA16  = 7,
      R32F    = 8,
      RG32F   = 9,
      RGB32F  = 10,
      RGBA32F = 11,
      DXT1    = 12,
      DXT3    = 13,
      DXT5    = 14,
      };

    Pixmap();
    Pixmap(const Pixmap& src,Format conv);
    Pixmap(uint32_t w,uint32_t h,Format frm);
    Pixmap(const char* path);
    Pixmap(const std::string& path);
    Pixmap(const char16_t* path);
    Pixmap(const std::u16string& path);
    Pixmap(IDevice& input);

    Pixmap(const Pixmap& src);
    Pixmap(Pixmap&& p);
    Pixmap& operator=(Pixmap&& p);
    Pixmap& operator=(const Pixmap& p);

    ~Pixmap();

    void        save(const char* path, const char* ext=nullptr) const;
    void        save(ODevice&    fout, const char *ext=nullptr) const;

    uint32_t    w()   const;
    uint32_t    h()   const;
    uint32_t    bpp() const;
    uint32_t    mipCount() const;

    bool        isEmpty() const;

    const void* data() const;
    void*       data();
    size_t      dataSize() const;

    Format      format() const;

    static size_t        bppForFormat   (Format f);
    static uint8_t       componentCount (Format f);
    static TextureFormat toTextureFormat(Format f);
    static Format        toPixmapFormat (TextureFormat f);

  private:
    struct Impl;
    struct Deleter {
      void operator()(Impl* ptr);
      };

    std::unique_ptr<Impl,Deleter> impl;
  };

}
