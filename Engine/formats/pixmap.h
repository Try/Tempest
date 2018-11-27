#pragma once

#include <memory>

namespace Tempest {

class Pixmap final {
  public:
    enum class Format : uint8_t {
      RGB    =0,
      RGBA   =1
      };

    Pixmap();
    Pixmap(uint32_t w,uint32_t h,Format frm);
    Pixmap(const char* path);
    Pixmap(const std::string& path);
    Pixmap(const char16_t* path);
    Pixmap(const std::u16string& path);

    Pixmap(Pixmap&& p);
    Pixmap& operator=(Pixmap&& p);

    ~Pixmap();

    bool save(const char* path,const char* ext=nullptr);

    uint32_t w() const;
    uint32_t h() const;

    bool isEmpty() const;

    const void* data() const;
    void*       data();

    Format format() const;

  private:
    struct Impl;
    struct Deleter {
      void operator()(Impl* ptr);
      };

    std::unique_ptr<Impl,Deleter> impl;
  };

}
