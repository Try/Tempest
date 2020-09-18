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
    bool wait(uint64_t timeout);
    bool waitValue(UINT64 val, DWORD timeout = INFINITE);
    void reset();

    void signal(ID3D12CommandQueue& queue);
    void signal(ID3D12CommandQueue& queue,UINT64 val);

    ComPtr<ID3D12Fence> impl;
    HANDLE              event=nullptr;
  };

using DxFence     = DxFenceBase<AbstractGraphicsApi::Fence>;
using DxSemaphore = DxFenceBase<AbstractGraphicsApi::Semaphore>;

}}
