#pragma once

#include <cstddef>
#include <cstdint>

namespace Tempest {

class IDevice {
  public:
    IDevice()=default;
    IDevice(const IDevice&)=delete;
    IDevice& operator = (const IDevice&)=delete;

    virtual ~IDevice();
    virtual size_t  read(void* to,size_t sz)=0;
    virtual size_t  size() const=0;
    virtual uint8_t peek()=0;
    virtual size_t  seek(size_t advance)=0;
  };

}
