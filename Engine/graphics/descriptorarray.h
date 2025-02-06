#pragma once

#include <Tempest/AbstractGraphicsApi>

namespace Tempest {

class DescriptorArray {
  public:
    DescriptorArray() = default;
    DescriptorArray(DescriptorArray&&);
    ~DescriptorArray();
    DescriptorArray& operator=(DescriptorArray&&);

    bool isEmpty() const { return impl.handler==nullptr; }

  private:
    DescriptorArray(Device& dev, AbstractGraphicsApi::DescArray* desc);

    Detail::DPtr<AbstractGraphicsApi::DescArray*> impl;

  friend class Tempest::Device;
  friend class Tempest::Encoder<Tempest::CommandBuffer>;
  };

}
