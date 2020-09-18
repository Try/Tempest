#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxfence.h"

#include "dxdevice.h"
#include "guid.h"

namespace Tempest {
namespace Detail {
  template class DxFenceBase<AbstractGraphicsApi::Fence>;
  template class DxFenceBase<AbstractGraphicsApi::Semaphore>;
}
}

using namespace Tempest;
using namespace Tempest::Detail;

template<class Interface>
DxFenceBase<Interface>::DxFenceBase(DxDevice& dev) {
  dxAssert(dev.device->CreateFence(Ready, D3D12_FENCE_FLAG_NONE,
                                   uuid<ID3D12Fence>(),
                                   reinterpret_cast<void**>(&impl)));
  event = CreateEvent(nullptr, TRUE, TRUE, nullptr);
  if(event == nullptr)
    throw std::bad_alloc();
  }

template<class Interface>
DxFenceBase<Interface>::~DxFenceBase() {
  if(event!=nullptr)
    CloseHandle(event);
  }

template<class Interface>
void DxFenceBase<Interface>::wait() {
  waitValue(Ready);
  }

template<class Interface>
bool DxFenceBase<Interface>::wait(uint64_t timeout) {
  if(timeout>INFINITE)
    timeout = INFINITE;
  return waitValue(Ready,DWORD(timeout));
  }

template<class Interface>
bool DxFenceBase<Interface>::waitValue(UINT64 val, DWORD timeout) {
  UINT64 v = impl->GetCompletedValue();
  if(val==v)
    return true;
  if(timeout==0)
    return false;
  dxAssert(impl->SetEventOnCompletion(val,event));
  DWORD ret = WaitForSingleObjectEx(event, timeout, FALSE);
  if(ret==WAIT_OBJECT_0)
    return true;
  if(ret==WAIT_TIMEOUT)
    return false;
  dxAssert(GetLastError());
  return false;
  }

template<class Interface>
void DxFenceBase<Interface>::reset() {
  ResetEvent(event);
  dxAssert(impl->Signal(Waiting));
  }

template<class Interface>
void DxFenceBase<Interface>::signal(ID3D12CommandQueue& queue) {
  signal(queue,DxFence::Ready);
  }

template<class Interface>
void DxFenceBase<Interface>::signal(ID3D12CommandQueue& queue, UINT64 val) {
  dxAssert(queue.Signal(impl.get(),val));
  }

#endif
