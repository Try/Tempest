#pragma once

#include <cstddef>
#include <cstdint>

#include "../utility/compiller_hints.h"

namespace Tempest {

class IDevice {
  public:
    IDevice()=default;
    IDevice(const IDevice&)=delete;
    IDevice& operator = (const IDevice&)=delete;

    virtual ~IDevice();
    T_NODISCARD virtual size_t  read (void* to,size_t sz)=0;
    T_NODISCARD virtual size_t  size () const=0;
    T_NODISCARD virtual uint8_t peek ()=0;
    T_NODISCARD virtual size_t  seek (size_t advance)=0;
    T_NODISCARD virtual size_t  unget(size_t advance)=0;
  };

}
