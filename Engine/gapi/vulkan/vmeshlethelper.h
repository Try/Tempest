#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/PipelineLayout>

#include "vbuffer.h"

namespace Tempest {
namespace Detail {

class VDevice;

class VMeshletHelper {
  public:
    enum {
      PipelineMemorySize = 16*1024*1024,
      MeshletsMemorySize = 16*1024,
      };
    explicit VMeshletHelper(VDevice& dev);
    ~VMeshletHelper();

  private:
    void                  cleanup();
    VkDescriptorSetLayout initLayout(VDevice& device);
    VkDescriptorPool      initPool(VDevice& device);
    VkDescriptorSet       initDescriptors(VDevice& device, VkDescriptorPool pool, VkDescriptorSetLayout lay);

    VDevice&         dev;
    VBuffer          meshlets, scratch, compacted;

    VkDescriptorSetLayout descLay  = VK_NULL_HANDLE;
    VkDescriptorPool      descPool = VK_NULL_HANDLE;
    VkDescriptorSet       engSet   = VK_NULL_HANDLE;
  };

}
}
