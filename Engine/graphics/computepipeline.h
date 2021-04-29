#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/PipelineLayout>

#include "../utility/dptr.h"

namespace Tempest {

class Device;
class CommandBuffer;
class PipelineLayout;
template<class T>
class Encoder;

class ComputePipeline final {
  public:
    ComputePipeline()=default;
    ComputePipeline(ComputePipeline&& f)=default;
    ~ComputePipeline()=default;
    ComputePipeline& operator = (ComputePipeline&& other);

    bool isEmpty() const { return impl.handler==nullptr; }
    const PipelineLayout& layout() const { return ulay; }

  private:
    ComputePipeline(Detail::DSharedPtr<AbstractGraphicsApi::CompPipeline*>&& p,
                    Detail::DSharedPtr<AbstractGraphicsApi::PipelineLay*>&&  lay);

    PipelineLayout                                         ulay;
    Detail::DSharedPtr<AbstractGraphicsApi::CompPipeline*> impl;

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;
  friend class Tempest::Encoder<Tempest::CommandBuffer>;

  template<class T>
  friend class Tempest::Detail::ResourcePtr;
  };

}

