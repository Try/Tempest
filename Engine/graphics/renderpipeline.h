#pragma once

#include <Tempest/AbstractGraphicsApi>

#include "../utility/dptr.h"

#include "pipelinelayout.h"

namespace Tempest {

class Device;
class CommandBuffer;

template<class T>
class Encoder;

class RenderPipeline final {
  public:
    RenderPipeline()=default;
    RenderPipeline(RenderPipeline&& f)=default;
    ~RenderPipeline() = default;
    RenderPipeline& operator = (RenderPipeline&& other);

    bool isEmpty() const { return impl.handler==nullptr; }

    size_t sizeofBuffer(size_t layoutBind, size_t arraylen = 0) const;

  private:
    RenderPipeline(Detail::DSharedPtr<AbstractGraphicsApi::Pipeline*>&&    p,
                   Detail::DSharedPtr<AbstractGraphicsApi::PipelineLay*>&& lay);

    PipelineLayout                                     ulay;
    Detail::DSharedPtr<AbstractGraphicsApi::Pipeline*> impl;

  friend class Tempest::Device;
  friend class Tempest::Encoder<Tempest::CommandBuffer>;

  template<class T>
  friend class Tempest::Detail::ResourcePtr;
  };

}
