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
  event = CreateEvent(nullptr, TRUE, TRUE, nullptr);
  if(event == nullptr)
    throw std::bad_alloc();
  try {
    dxAssert(dev.device->CreateFence(Ready, D3D12_FENCE_FLAG_NONE,
                                     uuid<ID3D12Fence>(),
                                     reinterpret_cast<void**>(&impl)));
    }
  catch(...) {
    CloseHandle(event);
    throw;
    }
  }

template<class Interface>
DxFenceBase<Interface>::~DxFenceBase() {
  if(event!=nullptr)
    CloseHandle(event);
  }

template<class Interface>
void DxFenceBase<Interface>::wait() {
  dxAssert(impl->SetEventOnCompletion(Ready,event));
  WaitForSingleObjectEx(event, INFINITE, FALSE);
  }

template<class Interface>
void DxFenceBase<Interface>::reset() {
  dxAssert(impl->Signal(Waiting));
  }

#endif
