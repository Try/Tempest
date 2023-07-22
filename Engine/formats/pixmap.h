#pragma once

#include <memory>
#include <string>

#include <Tempest/AbstractGraphicsApi>

namespace Tempest {

class IDevice;
class ODevice;

class Pixmap final {
  public:
    Pixmap();
    Pixmap(const Pixmap& src, TextureFormat conv);
    Pixmap(uint32_t w, uint32_t h, TextureFormat frm);
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

    TextureFormat format() const;

    static size_t  bppForFormat      (TextureFormat f);
    static size_t  blockSizeForFormat(TextureFormat f);
    static uint8_t componentCount    (TextureFormat f);
    static Size    blockCount        (TextureFormat f, uint32_t w, uint32_t h);

  private:
    struct Impl;
    struct Deleter {
      void operator()(Impl* ptr);
      };

    std::unique_ptr<Impl,Deleter> impl;
  };

}
