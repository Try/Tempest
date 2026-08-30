#pragma once

#include "mtmetalfx.h"

#if defined(TEMPEST_BUILD_METALFX_TEMPORAL)

#include <Tempest/AbstractGraphicsApi>
#include <MetalFX/MetalFX.hpp>

#include <mutex>

#include "nsptr.h"

namespace Tempest {
namespace Detail {

class MtDevice;
class MtTexture;

class MtTemporalScaler final : public AbstractGraphicsApi::TemporalScaler {
  public:
    MtTemporalScaler(MtDevice& device, const TemporalScalerDesc& desc);

    bool isValid() const { return impl!=nullptr; }
    bool belongsTo(const MtDevice& device) const { return owner==&device; }
    bool encode(MTL::CommandBuffer& cmd, MtTexture& input, MtTexture& depth,
                MtTexture& motion, MtTexture& output, const TemporalScalerArgs& args);

  private:
    // A temporal scaler owns one history. The mutex makes validate/set/encode
    // atomic with respect to other CPU threads. Callers must still record and
    // submit history-dependent frames in one explicit order; resetHistory starts
    // a new history and must be submitted before the frames that consume it.
    MtDevice*                  owner = nullptr;
    std::mutex                 sync;
    NsPtr<MTLFX::TemporalScaler> impl;
  };

}
}

#endif
