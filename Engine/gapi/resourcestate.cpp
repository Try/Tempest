#include "resourcestate.h"

using namespace Tempest;
using namespace Tempest::Detail;

ResourceState::ResourceState() {
  fillReads();
  }

void ResourceState::beginRendering(AbstractGraphicsApi::CommandBuffer& cmd, const Detail::FrameBufferDesc& desc) {
  fbo = desc;
  for(size_t i=0;; ++i) {
    if(fbo.att[i]==nullptr && fbo.sw[i]==nullptr)
      break;
    const bool discard = fbo.desc[i].load!=AccessOp::Preserve;
    if(isDepthFormat(fbo.frm[i]) && fbo.desc[i].load==AccessOp::Readonly)
      setLayout(*fbo.att[i],ResourceLayout::DepthReadOnly,0,false);
    else if(isDepthFormat(fbo.frm[i]))
      setLayout(*fbo.att[i],ResourceLayout::DepthAttach,0,discard);
    else if(fbo.frm[i]==TextureFormat::Undefined)
      setLayout(*fbo.sw[i],fbo.imgId[i],ResourceLayout::ColorAttach,discard);
    else
      setLayout(*fbo.att[i],ResourceLayout::ColorAttach,0,discard);
    }
  }

void ResourceState::endRendering(AbstractGraphicsApi::CommandBuffer& cmd) {
  for(size_t i=0;; ++i) {
    if(fbo.att[i]==nullptr && fbo.sw[i]==nullptr)
      break;

    const bool discard = fbo.desc[i].store!=AccessOp::Preserve;
    if(isDepthFormat(fbo.frm[i]) && fbo.desc[i].load==AccessOp::Readonly)
      setLayout(*fbo.att[i],ResourceLayout::Default,0,false);
    else if(isDepthFormat(fbo.frm[i]))
      setLayout(*fbo.att[i],ResourceLayout::Default,0,false);
    else if(fbo.frm[i]==TextureFormat::Undefined)
      setLayout(*fbo.sw[i],fbo.imgId[i],ResourceLayout::Default,discard);
    else
      setLayout(*fbo.att[i],ResourceLayout::Default,0,discard);
    }
  }

void ResourceState::setLayout(AbstractGraphicsApi::Swapchain& s, uint32_t id, ResourceLayout lay, bool discard) {
  ImgState& img = findImg(nullptr,&s,id,discard);
  img.next     = lay;
  img.discard  = discard;
  }

void ResourceState::setLayout(AbstractGraphicsApi::Texture& a, ResourceLayout lay, uint32_t mip, bool discard) {
  if(mip!=AllMips) {
    ImgState& img = findImg(&a,nullptr,mip,discard);
    img.next    = lay;
    img.discard = discard;
    return;
    }

  for(uint32_t i=0, numMip = a.mipCount(); i<numMip; ++i) {
    ImgState& img = findImg(&a,nullptr,i,discard);
    img.next    = lay;
    img.discard = discard;
    }
  }

void ResourceState::forceLayout(AbstractGraphicsApi::Texture& a, ResourceLayout lay) {
  for(uint32_t i=0, numMip = a.mipCount(); i<numMip; ++i) {
    ImgState& img = findImg(&a,nullptr,i,false);
    img.last    = lay;
    img.next    = lay;
    img.discard = false;
    }
  }

void ResourceState::onTranferUsage(NonUniqResId read, NonUniqResId write, bool host) {
  ResourceState::Usage u = {read, write, false};
  onUavUsage(u, PipelineStage::S_Transfer, host);
  }

void ResourceState::onUavUsage(NonUniqResId read, NonUniqResId write, PipelineStage st, bool host) {
  ResourceState::Usage u = {read, write, false};
  onUavUsage(u, st, host);
  }

static SyncStage toSyncStage(PipelineStage s, bool read) {
  const auto GraphicsFbo = (SyncStage::GraphicsDraw | SyncStage::GraphicsDepth);
  switch(s) {
    case S_Transfer: return read ? SyncStage::TransferSrc  : SyncStage::TransferDst;
    case S_Indirect: return read ? SyncStage::Indirect     : SyncStage::None;
    case S_RtAs:     return read ? SyncStage::RtAsRead     : SyncStage::RtAsWrite;
    case S_Compute:  return read ? SyncStage::ComputeRead  : SyncStage::ComputeWrite;
    case S_Graphics: return read ? SyncStage::GraphicsRead : SyncStage::GraphicsWrite;
    case S_Draw:     return read ? GraphicsFbo : GraphicsFbo;
    case S_Count:    return SyncStage::None;
    }
  return SyncStage::None;
  }

void ResourceState::onUavUsage(const Usage& u, PipelineStage st, bool host) {
  for(PipelineStage p = PipelineStage::S_First; p<PipelineStage::S_Count; p = PipelineStage(p+1)) {
    if((uavWrite[st].depend[p] & u.write)!=0) {
      // WaW - execution+cache
      uavSrcBarrier = uavSrcBarrier | toSyncStage(p,  true) | toSyncStage(p,  false);
      uavDstBarrier = uavDstBarrier | toSyncStage(st, true) | toSyncStage(st, false);

      uavRead [st].depend[p] = NonUniqResId::I_None;
      uavWrite[st].depend[p] = NonUniqResId::I_None;
      }
    else if((uavWrite[st].depend[p] & u.read)!=0) {
      // RaW barrier - execution+cache
      uavSrcBarrier = uavSrcBarrier | toSyncStage(p,  true) | toSyncStage(p,  false);
      uavDstBarrier = uavDstBarrier | toSyncStage(st, true);

      uavRead [st].depend[p] = NonUniqResId::I_None;
      uavWrite[st].depend[p] = NonUniqResId::I_None;
      }
    else if((uavRead[st].depend[p] & u.write)!=0) {
      // WaR barrier - only exec barrier
      uavSrcBarrier = uavSrcBarrier | toSyncStage(p,  true);
      uavDstBarrier = uavDstBarrier | toSyncStage(st, false);

      uavRead [st].depend[p] = NonUniqResId::I_None;
      uavWrite[st].depend[p] = NonUniqResId::I_None;
      }
    else {
      // RaR - no barrier needed
      }
    }

  if(host) {
    uavDstBarrier = uavDstBarrier | SyncStage::TransferHost;
    }

  if(u.speculative)
    return;

  for(PipelineStage p = PipelineStage::S_First; p<PipelineStage::S_Count; p = PipelineStage(p+1)) {
    uavRead [p].depend[st] |= u.read;
    uavWrite[p].depend[st] |= u.write;
    }
  }

void ResourceState::joinWriters(PipelineStage st) {
  ResourceState::Usage u = {NonUniqResId::I_All, NonUniqResId::I_None, false};
  u.speculative = true;
  onUavUsage(u, st);
  }

void ResourceState::clearReaders() {
  for(auto& i:uavRead)
    for(auto& r:i.depend)
      r = NonUniqResId::I_None;
  }

void ResourceState::finalize(AbstractGraphicsApi::CommandBuffer& cmd) {
  for(PipelineStage p = PipelineStage::S_First; p<PipelineStage::S_Count; p = PipelineStage(p+1)) {
    joinWriters(p);
    }

  if(imgState.size()==0 && uavSrcBarrier==SyncStage::None) {
    fillReads();
    return; // early-out
    }

  for(auto& i:imgState) {
    i.next = ResourceLayout::Default;
    }
  flush(cmd);
  imgState.reserve(imgState.size());
  imgState.clear();
  uavSrcBarrier = SyncStage::None;
  uavDstBarrier = SyncStage::None;

  for(auto& i:uavRead)
    i = Stage();
  for(auto& i:uavWrite)
    i = Stage();
  fillReads();
  }

void ResourceState::fillReads() {
  // assume that previous command buffer may read anything
  for(auto& i:uavRead)
    for(auto& r:i.depend)
      r = NonUniqResId::I_All;
  }

ResourceState::ImgState& ResourceState::findImg(AbstractGraphicsApi::Texture* img, AbstractGraphicsApi::Swapchain* sw, uint32_t id, bool discard) {
  auto nativeImg = img;
  for(auto& i:imgState) {
    if(i.sw==sw && i.id==id && i.img==nativeImg)
      return i;
    }
  ImgState s = {};
  s.sw      = sw;
  s.id      = id;
  s.img     = img;
  s.last    = ResourceLayout::Default;
  s.next    = ResourceLayout::Default;
  s.discard = discard;
  imgState.push_back(s);
  return imgState.back();
  }

void ResourceState::flush(AbstractGraphicsApi::CommandBuffer& cmd) {
  AbstractGraphicsApi::BarrierDesc barrier[MaxBarriers];
  uint8_t                          barrierCnt = 0;

  for(auto& i:imgState) {
    const auto nonUniqId = (i.sw!=nullptr) ? i.sw->syncId() : i.img->syncId();

    if(i.next==ResourceLayout::ColorAttach || i.next==ResourceLayout::DepthAttach) {
      /* Use cases:
       *   read/idle -> draw, will act as WaR barrier
       *   draw      -> draw, is WaW barrier
       */
      onUavUsage(NonUniqResId::I_None, nonUniqId, S_Draw);
      }
    else if(i.last==ResourceLayout::ColorAttach || i.last==ResourceLayout::DepthAttach) {
      /* Use cases:
       *   draw -> read, will act as RaW barrier
       */
      onUavUsage(nonUniqId, NonUniqResId::I_None, S_Draw);
      }

    /*
    if(i.last!=i.next) {
      if(i.next==ResourceLayout::TransferSrc || i.next==ResourceLayout::TransferDst) {
        // transition from something -> transfer is write opration in Vulkan
        onUavUsage(NonUniqResId::I_None, nonUniqId, S_Transfer);
        }
      else if(i.last==ResourceLayout::TransferSrc || i.last==ResourceLayout::TransferDst) {
        // transition from transfer -> else is write opration in Vulkan
        onUavUsage(NonUniqResId::I_None, nonUniqId, S_Transfer);
        }
      }
    */
    }

  AbstractGraphicsApi::SyncDesc d;
  if(uavDstBarrier!=SyncStage::None) {
    d.prev = uavSrcBarrier;
    d.next = uavDstBarrier;
    uavSrcBarrier = SyncStage::None;
    uavDstBarrier = SyncStage::None;
    }

  for(auto& i:imgState) {
    if(i.next==i.last)
      continue;
    auto& b = barrier[barrierCnt];
    b.swapchain = i.sw;
    b.swId      = i.sw!=nullptr  ? i.id : uint32_t(-1);
    b.texture   = i.img;
    b.mip       = i.img!=nullptr ? i.id : uint32_t(-1);
    b.prev      = i.last;
    b.next      = i.next;
    b.discard   = i.discard;
    ++barrierCnt;

    i.last      = i.next;
    if(barrierCnt==MaxBarriers) {
      emitBarriers(cmd,d,barrier,barrierCnt);
      barrierCnt = 0;
      }
    }

  std::erase_if(imgState, [](const ImgState& img) {
    return img.last==ResourceLayout::Default;
    });

  emitBarriers(cmd,d,barrier,barrierCnt);
  }

void ResourceState::emitBarriers(AbstractGraphicsApi::CommandBuffer& cmd, AbstractGraphicsApi::SyncDesc& d, AbstractGraphicsApi::BarrierDesc* desc, size_t cnt) {
  if(cnt==0 && d.next==SyncStage::None)
    return;
  std::sort(desc,desc+cnt,[](const AbstractGraphicsApi::BarrierDesc& l, const AbstractGraphicsApi::BarrierDesc& r) {
    return std::tie(l.texture, l.mip, l.swapchain, l.swId) < std::tie(r.texture, r.mip, r.swapchain, r.swId);
    });
  cmd.barrier(d,desc,cnt);
  }
