#pragma once

#include <Tempest/ODevice>
#include <Tempest/Platform>
#include <string>

namespace Tempest {

class WFile : public Tempest::ODevice {
  public:
    WFile(const char*     path);
    WFile(const std::string& path);
    WFile(const char16_t* path);
    WFile(const std::u16string& path);
    ~WFile() override;

    size_t  write(const void* val,size_t size) override;
    bool    flush() override;

  private:
    void* handle=nullptr;
#ifdef __WINDOWS__
    static void* implOpen(const wchar_t* wstr);
#else
    static void* implOpen(const char* cstr);
#endif
  };

}
