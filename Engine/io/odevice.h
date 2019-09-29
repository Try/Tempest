#pragma once

#include <cstdint>
#include <cstddef>

namespace Tempest {

class ODevice {
  public:
    ODevice()=default;
    ODevice(const ODevice&)=delete;
    ODevice& operator = (const ODevice&)=delete;

    virtual ~ODevice();
    virtual size_t  write(const void* val,size_t sz)=0;
    virtual bool    flush()=0;
  };

}
