#pragma once

#include <Tempest/File>

#include <string>
#include <memory>

namespace Tempest {

class FontElement final {
  public:
    FontElement(const char* file);

  private:
    struct Impl;
    std::shared_ptr<Impl> ptr;
  };

class Font final {
  public:
    Font(const char* file);

  private:
    FontElement fnt[2][2];
  };
}
