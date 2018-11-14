#pragma once

#include <memory>

namespace Tempest {

class Pixmap final {
  public:
    Pixmap(const char* path);
    Pixmap(const std::string& path);
    Pixmap(const char16_t* path);
    Pixmap(const std::u16string& path);

    Pixmap(Pixmap&& p);
    Pixmap& operator=(Pixmap&& p);

    ~Pixmap();

    uint32_t w() const;
    uint32_t h() const;

    bool isEmpty() const;

    const void* data() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
  };

}
