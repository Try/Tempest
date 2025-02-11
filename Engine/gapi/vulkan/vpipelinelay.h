#pragma once

#include <Tempest/AbstractGraphicsApi>

namespace Tempest {
namespace Detail {

//TODO: remove
class VPipelineLay : public AbstractGraphicsApi::PipelineLay {
  public:
    VPipelineLay();
    ~VPipelineLay();
  };

}
}
