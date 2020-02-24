#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <d3d12.h>
#include "comptr.h"

namespace Tempest {
namespace Detail {

class DxDevice;

template<class Interface>
class DxFenceBase : public Interface {
  public:
    enum State : uint64_t {
      Waiting = 0,
      Ready   = 1
      };
    DxFenceBase(DxDevice& dev);
    ~DxFenceBase() override;

    void wait();
    void reset();

    HANDLE              event=nullptr;
    ComPtr<ID3D12Fence> impl;
  };

using DxFence     = DxFenceBase<AbstractGraphicsApi::Fence>;
using DxSemaphore = DxFenceBase<AbstractGraphicsApi::Semaphore>;

}}
