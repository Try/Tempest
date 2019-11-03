#pragma once

#include <Tempest/IDevice>
#include <Tempest/Platform>
#include <string>

namespace Tempest {

class RFile : public Tempest::IDevice {
  public:
    explicit RFile(const char*     path);
    explicit RFile(const std::string& path);
    explicit RFile(const char16_t* path);
    explicit RFile(const std::u16string& path);
    RFile(RFile&& other);
    ~RFile() override;

    RFile& operator = (RFile&& other);

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
