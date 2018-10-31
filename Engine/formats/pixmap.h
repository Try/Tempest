#pragma once

#include <memory>

namespace Tempest {

class Pixmap final {
  public:
    Pixmap(const char* path);
    ~Pixmap();

    uint32_t w() const;
    uint32_t h() const;

    const void* data() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl;
  };

}
