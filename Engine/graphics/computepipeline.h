#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/UniformsLayout>

#include "../utility/dptr.h"

namespace Tempest {

class Device;
class CommandBuffer;
class UniformsLayout;
template<class T>
class Encoder;

class ComputePipeline final {
  public:
    ComputePipeline()=default;
    ComputePipeline(ComputePipeline&& f)=default;
    ~ComputePipeline()=default;
    ComputePipeline& operator = (ComputePipeline&& other);

    bool isEmpty() const { return impl.handler==nullptr; }
    const UniformsLayout& layout() const { return ulay; }

  private:
    ComputePipeline(Detail::DSharedPtr<AbstractGraphicsApi::CompPipeline*>&& p,
                    Detail::DSharedPtr<AbstractGraphicsApi::UniformsLay*>&&  lay);

    UniformsLayout                                         ulay;
    Detail::DSharedPtr<AbstractGraphicsApi::CompPipeline*> impl;

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;
  friend class Tempest::Encoder<Tempest::CommandBuffer>;

  template<class T>
  friend class Tempest::Detail::ResourcePtr;
  };

}

