#pragma once

#include <Tempest/ODevice>
#include <vector>

namespace Tempest {

class MemWriter : public ODevice {
  public:
    MemWriter(std::vector<uint8_t>& vec);

    size_t  write(const void* val,size_t sz) override;
    bool    flush() override { return true; }

  private:
    std::vector<uint8_t>& vec;
  };

}
