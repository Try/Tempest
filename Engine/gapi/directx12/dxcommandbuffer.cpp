#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxcommandbuffer.h"
#include "dxdevice.h"

#include "guid.h"

#include <cassert>

using namespace Tempest;
using namespace Tempest::Detail;

DxCommandBuffer::DxCommandBuffer(DxDevice& d)
  : dev(d) {
  dxAssert(d.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, dev.cmdMain.get(), nullptr,
                                       uuid<ID3D12GraphicsCommandList>(), reinterpret_cast<void**>(&impl)));
  impl->Close();
  }

void DxCommandBuffer::begin() {
  dxAssert(impl->Reset(dev.cmdMain.get(),nullptr));
  recording = true;
  }

void DxCommandBuffer::end() {
  dxAssert(impl->Close());
  recording = false;
  }

bool DxCommandBuffer::isRecording() const {
  return recording;
  }

void DxCommandBuffer::beginRenderPass(AbstractGraphicsApi::Fbo* f, AbstractGraphicsApi::Pass* p,
                                      uint32_t width, uint32_t height) {
  assert(0);
  }

void DxCommandBuffer::endRenderPass() {
  assert(0);
  }

void Tempest::Detail::DxCommandBuffer::setPipeline(Tempest::AbstractGraphicsApi::Pipeline& p, uint32_t w, uint32_t h) {
  assert(0);
  }

void DxCommandBuffer::setViewport(const Rect& r) {
  D3D12_VIEWPORT vp={};
  vp.TopLeftX = r.x;
  vp.TopLeftY = r.y;
  vp.Width    = r.w;
  vp.Height   = r.h;
  vp.MinDepth = 0.f;
  vp.MaxDepth = 1.f;

  impl->RSSetViewports(1,&vp);
  }

void DxCommandBuffer::setUniforms(AbstractGraphicsApi::Pipeline& p, AbstractGraphicsApi::Desc& u, size_t offc, const uint32_t* offv) {
  assert(0);
  }

void DxCommandBuffer::exec(const AbstractGraphicsApi::CommandBuffer& buf) {
  assert(0);
  //impl->ExecuteBundle(); //BUNDLE!
  }

void DxCommandBuffer::changeLayout(AbstractGraphicsApi::Swapchain& s, uint32_t id, TextureFormat frm, TextureLayout prev, TextureLayout next) {
  assert(0);
  }

void DxCommandBuffer::changeLayout(AbstractGraphicsApi::Texture& t, TextureFormat frm, TextureLayout prev, TextureLayout next) {
  assert(0);
  }

void DxCommandBuffer::setVbo(const AbstractGraphicsApi::Buffer& b) {
  assert(0);
  }

void DxCommandBuffer::setIbo(const AbstractGraphicsApi::Buffer* b, IndexClass cls) {
  assert(0);
  }

void DxCommandBuffer::draw(size_t offset, size_t vertexCount) {
  impl->DrawInstanced(vertexCount,1,offset,0);
  }

void DxCommandBuffer::drawIndexed(size_t ioffset, size_t isize, size_t voffset) {
  impl->DrawIndexedInstanced(isize,1,ioffset,voffset,0);
  }

#endif
