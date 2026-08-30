#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace Tempest {
namespace Detail {

class MtSha256 final {
  public:
    using Digest = std::array<uint8_t,32>;

    MtSha256();

    void   update(const void* data, size_t size);
    void   update(std::string_view data);
    Digest finalize();

    static Digest hash(const void* data, size_t size);
    static Digest hash(std::string_view data);

  private:
    void transform(const uint8_t block[64]);

    uint32_t state[8] = {};
    uint8_t  pending[64] = {};
    uint64_t byteCount = 0;
    size_t   pendingSize = 0;
    bool     finalized = false;
  };

}
}
