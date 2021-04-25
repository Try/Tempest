#include "mtframebuffer.h"

#include <Tempest/RenderPass>

#include "mttexture.h"

#include <Metal/MTLRenderCommandEncoder.h>
#include <mutex>

using namespace Tempest;
using namespace Tempest::Detail;

static MTLLoadAction mkLoadOp(const FboMode& m) {
  if(m.mode&FboMode::ClearBit)
    return MTLLoadActionClear;
  if(m.mode&FboMode::PreserveIn)
    return MTLLoadActionLoad;
  return MTLLoadActionDontCare;
  }

static MTLClearColor mkClearColor(const FboMode& m) {
  MTLClearColor ret;
  ret.red   = m.clear.r();
  ret.green = m.clear.g();
  ret.blue  = m.clear.b();
  ret.alpha = m.clear.a();
  return ret;
  }

static MTLStoreAction mkStoreOp(const FboMode& m) {
  if(m.mode&FboMode::PreserveOut)
    return MTLStoreActionStore;
  return MTLStoreActionDontCare;
  }

MtFramebuffer::MtFramebuffer(MtTexture** clr, size_t clrSize, MtTexture *depth)
  :color(clrSize), depth(depth) {
  for(size_t i=0; i<clrSize; ++i)
    color[i] = clr[i];
  }

MtFramebuffer::~MtFramebuffer() {
  for(auto& i:inst)
    [i.desc release];
  }

MTLRenderPassDescriptor *MtFramebuffer::instance(MtRenderPass &rp) {
  std::lock_guard<SpinLock> guads(sync);
  for(auto& i:inst)
    if(i.pass.handler->isCompatible(rp))
      return i.desc;

  MTLRenderPassDescriptor* desc = [MTLRenderPassDescriptor renderPassDescriptor];

  for(size_t i=0; i<color.size(); ++i) {
    desc.colorAttachments[i].texture        = color[i]->impl.get();
    desc.colorAttachments[i].loadAction     = mkLoadOp    (rp.mode[i]);
    desc.colorAttachments[i].storeAction    = mkStoreOp   (rp.mode[i]);
    desc.colorAttachments[i].clearColor     = mkClearColor(rp.mode[i]);
    }

  if(depth!=nullptr) {
    desc.depthAttachment.texture     = depth->impl.get();
    desc.depthAttachment.loadAction  = mkLoadOp (rp.mode.back());
    desc.depthAttachment.storeAction = mkStoreOp(rp.mode.back());
    desc.depthAttachment.clearDepth  = rp.mode.back().clear.r();
    }

  inst.emplace_back();
  auto& ret = inst.back();
  ret.pass = DSharedPtr<MtRenderPass*>(&rp);
  ret.desc = desc;
  [desc retain];
  return desc;
  }
