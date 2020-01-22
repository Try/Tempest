#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>
#include <mutex>
#include <list>

#include "utility/spinlock.h"

namespace Tempest {

class UniformsLayout;

namespace Detail {

class VDescriptorArray;

class VUniformsLay : public AbstractGraphicsApi::UniformsLay {
  public:
    VUniformsLay(VkDevice dev, const UniformsLayout& lay);
    ~VUniformsLay();

    VkDevice                      dev =nullptr;
    VkDescriptorSetLayout         impl=VK_NULL_HANDLE;
    std::vector<VkDescriptorType> hint;
    size_t                        offsetsCnt=0;

  private:
    enum {
      POOL_SIZE=512
      };

    struct Pool {
      VkDescriptorPool impl      = VK_NULL_HANDLE;
      uint16_t         freeCount = POOL_SIZE;
      };

    Detail::SpinLock sync;
    std::list<Pool>  pool;

    void implCreate(const UniformsLayout& lay, VkDescriptorSetLayoutBinding *bind);

  friend class VDescriptorArray;
  };

}
}
