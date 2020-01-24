#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Color>

#include "../utility/dptr.h"

namespace Tempest {

class Device;
class CommandBuffer;
class PrimaryCommandBuffer;
template<class T>
class Encoder;

class FboMode final {
  public:
    enum {
      ClearBit = 1<<2,
      };
    enum Mode {
      Discard     = 0,
      PreserveIn  = 1,
      PreserveOut = 1<<1,
      Preserve    = (PreserveIn|PreserveOut),
      Submit      = (1<<3)|PreserveOut,
      };
    FboMode()=default;
    FboMode(Mode m):mode(m){}
    FboMode(Mode m,const float& clr):mode(m | ClearBit),clear(clr){}
    FboMode(Mode m,const Color& clr):mode(m | ClearBit),clear(clr){}

    int   mode {Preserve};
    Color clear{};
  };

inline FboMode::Mode operator | (FboMode::Mode a,const FboMode::Mode& b) {
  return FboMode::Mode(uint8_t(a)|uint8_t(b));
  }

inline FboMode::Mode operator & (FboMode::Mode a,const FboMode::Mode& b) {
  return FboMode::Mode(uint8_t(a)&uint8_t(b));
  }

class RenderPass final {
  public:
    RenderPass()=default;
    RenderPass(RenderPass&& f)=default;
    ~RenderPass();
    RenderPass& operator = (RenderPass&& other)=default;

  private:
    RenderPass(Detail::DSharedPtr<AbstractGraphicsApi::Pass*>&& img);
    Detail::DSharedPtr<AbstractGraphicsApi::Pass*> impl;

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;
  friend class Tempest::Encoder<Tempest::PrimaryCommandBuffer>;
  };

}
