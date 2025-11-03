#pragma once

#include <Tempest/AbstractGraphicsApi>

#include "../utility/dptr.h"

namespace Tempest {

class Device;
class CommandBuffer;
template<class T>
class Encoder;

class ComputePipeline final {
  public:
    ComputePipeline()=default;
    ComputePipeline(ComputePipeline&& f)=default;
    ~ComputePipeline()=default;
    ComputePipeline& operator = (ComputePipeline&& other);

    bool isEmpty() const { return impl.handler==nullptr; }

    IVec3  workGroupSize() const;
    size_t sizeofBuffer(size_t layoutBind, size_t arraylen = 0) const;

  private:
    ComputePipeline(Detail::DSharedPtr<AbstractGraphicsApi::CompPipeline*>&& p);
    Detail::DSharedPtr<AbstractGraphicsApi::CompPipeline*> impl;

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;
  friend class Tempest::Encoder<Tempest::CommandBuffer>;

  template<class T>
  friend class Tempest::Detail::ResourcePtr;
  };

}

