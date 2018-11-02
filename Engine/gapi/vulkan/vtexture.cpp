#include "vtexture.h"

#include <Tempest/Pixmap>
#include "vdevice.h"

using namespace Tempest::Detail;

VTexture::VTexture(VDevice& dev, const VBuffer &stage, const Pixmap &pm, bool mips)
  :device(dev) {
  VkImageCreateInfo imageInfo = {};
  imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType     = VK_IMAGE_TYPE_2D;
  imageInfo.extent.width  = pm.w();
  imageInfo.extent.height = pm.h();
  imageInfo.extent.depth  = 1;
  imageInfo.mipLevels     = 1;
  imageInfo.arrayLayers   = 1;
  imageInfo.format        = VK_FORMAT_R8G8B8A8_UNORM;
  imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

  vkAssert(vkCreateImage(device.device, &imageInfo, nullptr, &image));

  //dev.allocator.alloc();

  VkMemoryRequirements memRequirements;
  vkGetImageMemoryRequirements(device.device, image, &memRequirements);

  //Detail::VBuffer stage=dev.allocator.alloc(mem,size,usage,BufferFlags::Staging);

  VkMemoryAllocateInfo allocInfo = {};
  allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  allocInfo.allocationSize  = memRequirements.size;
  allocInfo.memoryTypeIndex = device.memoryTypeIndex(memRequirements.memoryTypeBits,VkMemoryPropertyFlagBits::VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  VkDeviceMemory imageMemory=VK_NULL_HANDLE;
  vkAssert(vkAllocateMemory(device.device, &allocInfo, nullptr, &imageMemory));
  vkBindImageMemory(device.device, image, imageMemory, 0);

  //dev.copyBuffer(*this,stage,pm.w()*pm.h()*4);
  }

VTexture::~VTexture() {
  if(image==VK_NULL_HANDLE)
    return;
  vkDeviceWaitIdle(device.device);
  vkDestroyImage(device.device,image,nullptr);
  }
