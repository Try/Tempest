#pragma once

#include <Tempest/Platform>
#include <Tempest/Asset>

#include <memory>

namespace Tempest {

class Assets final {
  public:
    Assets(const char *path);
    ~Assets();

    Asset operator[](const char* file) const;

    struct Provider {
      virtual ~Provider(){}
      };

  private:
    struct Directory:Provider {
      Directory(const char* path);
#ifdef __WINDOWS__
      std::wstring path;
#else
      std::string  path;
#endif
      };

    std::unique_ptr<Provider> impl;
  };

}
