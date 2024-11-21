#pragma once

#include <Tempest/ODevice>
#include <Tempest/Platform>
#include <string_view>

namespace Tempest {

class WFile : public Tempest::ODevice {
  public:
    explicit WFile(const char*         path);
    explicit WFile(std::string_view    path);
    explicit WFile(const char16_t*     path);
    explicit WFile(std::u16string_view path);
    WFile(WFile&& other);
    ~WFile() override;

    WFile& operator = (WFile&& other);

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
