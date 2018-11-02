#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>

namespace Tempest {

class Pixmap;

namespace Detail {

class VDevice;
class VBuffer;

class VTexture : public AbstractGraphicsApi::Texture {
  public:
    VTexture(VDevice &dev,const VBuffer& stage,const Pixmap& pm,bool mips);
    ~VTexture();

  private:
    VDevice& device;
    VkImage  image =VK_NULL_HANDLE;
  };

}}
