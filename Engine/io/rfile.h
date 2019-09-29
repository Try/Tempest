#pragma once

#include <Tempest/IDevice>
#include <string>

namespace Tempest {

class RFile : public Tempest::IDevice {
  public:
    RFile(const char*     path);
    RFile(const std::string& path);
    RFile(const char16_t* path);
    RFile(const std::u16string& path);
    ~RFile() override;

    size_t  read(void* to,size_t size) override;
    size_t  size() const override;

    uint8_t peek() override;
    size_t  seek(size_t advance) override;

  private:
    void* handle=nullptr;
#ifdef __WINDOWS__
    static void* implOpen(const wchar_t* wstr);
#else
    static void* implOpen(const char* cstr);
#endif
  };

}
