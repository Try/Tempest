#include "resourcestate.h"

using namespace Tempest;
using namespace Tempest::Detail;

ResourceState::ResourceState() {
  fillReads();
  }

void ResourceState::setRenderpass(AbstractGraphicsApi::CommandBuffer& cmd,
                                  const AttachmentDesc* desc, size_t descSize,
                                  const TextureFormat* frm, AbstractGraphicsApi::Texture** att,
                                  AbstractGraphicsApi::Swapchain** sw, const uint32_t* imgId) {
  for(size_t i=0; i<descSize; ++i) {
    const bool discard = desc[i].load!=AccessOp::Preserve;
    if(isDepthFormat(frm[i]) && desc[i].load==AccessOp::Readonly)
      setLayout(*att[i],ResourceLayout::DepthReadOnly,discard);
    if(isDepthFormat(frm[i]))
      setLayout(*att[i],ResourceLayout::DepthAttach,discard);
    else if(frm[i]==TextureFormat::Undefined)
      setLayout(*sw[i],imgId[i],ResourceLayout::ColorAttach,discard);
    else {
      setLayout(*att[i],ResourceLayout::ColorAttach,discard);
      }
    }
  flush(cmd);
  for(size_t i=0; i<descSize; ++i) {
    const bool discard = desc[i].store!=AccessOp::Preserve;
    if(isDepthFormat(frm[i]))
      setLayout(*att[i],ResourceLayout::Default,discard);
    else if(frm[i]==TextureFormat::Undefined)
      setLayout(*sw[i],imgId[i],ResourceLayout::Default,discard);
    else
      setLayout(*att[i],ResourceLayout::Default,discard);
    }
  }

void ResourceState::setLayout(AbstractGraphicsApi::Swapchain& s, uint32_t id, ResourceLayout lay, bool discard) {
  ImgState& img   = findImg(nullptr,&s,id,discard);
  img.next     = lay;
  img.discard  = discard;
  img.outdated = (img.next!=img.last);
  }

void ResourceState::setLayout(AbstractGraphicsApi::Texture& a, ResourceLayout lay, bool discard) {
  ImgState& img = findImg(&a,nullptr,0,discard);
  img.next     = lay;
  img.discard  = discard;
  img.outdated = (img.next!=img.last);
  }

void ResourceState::forceLayout(AbstractGraphicsApi::Texture& a, ResourceLayout lay) {
  ImgState& img = findImg(&a,nullptr,0,false);
  img.last     = lay;
  img.next     = lay;
  img.discard  = false;
  img.outdated = false;
  }

void ResourceState::onTranferUsage(NonUniqResId read, NonUniqResId write, bool host) {
  ResourceState::Usage u = {read, write, false};
  onUavUsage(u, PipelineStage::S_Transfer, host);
  }

void ResourceState::onDrawUsage(NonUniqResId id, AccessOp loadOp) {
  ResourceState::Usage u = {};
  if(loadOp==AccessOp::Readonly)
    u.read  = id; else
    u.write = id;
  onUavUsage(u, PipelineStage::S_Graphics, false);
  }

void ResourceState::onUavUsage(NonUniqResId read, NonUniqResId write, PipelineStage st, bool host) {
  ResourceState::Usage u = {read, write, false};
  onUavUsage(u, st, host);
  }

void ResourceState::onUavUsage(const Usage& u, PipelineStage st, bool host) {
  const SyncStage rd[PipelineStage::S_Count] = {SyncStage::TransferSrc, SyncStage::Indirect, SyncStage::RtAsRead,  SyncStage::ComputeRead,  SyncStage::GraphicsRead };
  const SyncStage wr[PipelineStage::S_Count] = {SyncStage::TransferDst, SyncStage::None,     SyncStage::RtAsWrite, SyncStage::ComputeWrite, SyncStage::GraphicsWrite};
  const SyncStage hv = (host ? SyncStage::TransferHost : SyncStage::None);

  for(PipelineStage p = PipelineStage::S_First; p<PipelineStage::S_Count; p = PipelineStage(p+1)) {
    if((uavWrite[st].depend[p] & u.write)!=0) {
      // WaW - execution+cache
      uavSrcBarrier = uavSrcBarrier | rd[p] | wr[p];
      uavDstBarrier = uavDstBarrier | rd[st] | wr[st];

      uavRead [st].depend[p] = NonUniqResId::I_None;
      uavWrite[st].depend[p] = NonUniqResId::I_None;
      }
    if((uavWrite[st].depend[p] & u.read)!=0) {
      // RaW barrier - execution+cache
      uavSrcBarrier = uavSrcBarrier | rd[p] | wr[p];
      uavDstBarrier = uavDstBarrier | rd[st];

      uavRead [st].depend[p] = NonUniqResId::I_None;
      uavWrite[st].depend[p] = NonUniqResId::I_None;
      }
    else if((uavRead[st].depend[p] & u.write)!=0) {
      // WaR barrier - only exec barrier
      uavSrcBarrier = uavSrcBarrier | rd[p];
      uavDstBarrier = uavDstBarrier | wr[st];

      uavRead [st].depend[p] = NonUniqResId::I_None;
      uavWrite[st].depend[p] = NonUniqResId::I_None;
      }
    else {
      // RaR - no barrier needed
      }
    }

  if(host) {
    uavDstBarrier = uavDstBarrier | hv;
    }

  if(u.speculative)
    return;

  for(PipelineStage p = PipelineStage::S_First; p<PipelineStage::S_Count; p = PipelineStage(p+1)) {
    uavRead [p].depend[st] |= u.read;
    uavWrite[p].depend[st] |= u.write;
    }
  }

void ResourceState::joinWriters(PipelineStage st) {
  ResourceState::Usage u = {NonUniqResId(-1), NonUniqResId::I_None, false};
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

  if(imgState.size()==0 && bufState.size()==0 && uavSrcBarrier==SyncStage::None)
    return; // early-out

  for(auto& i:imgState) {
    i.next     = ResourceLayout::Default;
    i.outdated = (i.next!=i.last);
    }
  for(auto& i:bufState) {
    if(i.buf==nullptr)
      continue;
    i.next     = ResourceLayout::Default; //fixme: buffer-layout is DX only hack
    i.outdated = true;
    }
  flush(cmd);
  imgState.reserve(imgState.size());
  imgState.clear();
  bufState.reserve(bufState.size());
  bufState.clear();
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
      r = NonUniqResId(-1);
  }

ResourceState::ImgState& ResourceState::findImg(AbstractGraphicsApi::Texture* img, AbstractGraphicsApi::Swapchain* sw, uint32_t id, bool discard) {
  auto nativeImg = img;
  for(auto& i:imgState) {
    if(i.sw==sw && i.id==id && i.img==nativeImg)
      return i;
    }
  ImgState s={};
  s.sw       = sw;
  s.id       = id;
  s.img      = img;
  s.last     = ResourceLayout::Default;
  s.next     = ResourceLayout::Default;
  s.discard  = discard;
  s.outdated = false;
  imgState.push_back(s);
  return imgState.back();
  }

ResourceState::BufState& ResourceState::findBuf(const AbstractGraphicsApi::Buffer* buf) {
  for(auto& i:bufState) {
    if(i.buf==buf)
      return i;
    }
  BufState s={};
  s.buf      = buf;
  s.last     = ResourceLayout::Default;
  s.next     = ResourceLayout::Default;
  s.outdated = false;
  bufState.push_back(s);
  return bufState.back();
  }

void ResourceState::flush(AbstractGraphicsApi::CommandBuffer& cmd) {
  AbstractGraphicsApi::BarrierDesc barrier[MaxBarriers];
  uint8_t                          barrierCnt = 0;

  AbstractGraphicsApi::SyncDesc d;
  if(uavDstBarrier!=SyncStage::None) {
    d.prev = uavSrcBarrier;
    d.next = uavDstBarrier;
    uavSrcBarrier = SyncStage::None;
    uavDstBarrier = SyncStage::None;
    }

  for(auto& i:imgState) {
    if(!i.outdated)
      continue;
    auto& b = barrier[barrierCnt];
    b.swapchain = i.sw;
    b.swId      = i.id;
    b.texture   = i.img;
    b.mip       = uint32_t(-1);
    b.prev      = i.last;
    b.next      = i.next;
    b.discard   = i.discard;
    ++barrierCnt;

    i.last     = i.next;
    i.outdated = false;
    if(barrierCnt==MaxBarriers) {
      emitBarriers(cmd,d,barrier,barrierCnt);
      barrierCnt = 0;
      }
    }

  for(auto& i:bufState) {
    if(!i.outdated)
      continue;
    auto& b = barrier[barrierCnt];
    b.buffer    = i.buf;
    b.prev      = i.last;
    b.next      = i.next;
    ++barrierCnt;

    i.last     = i.next;
    i.outdated = false;
    if(barrierCnt==MaxBarriers) {
      emitBarriers(cmd,d,barrier,barrierCnt);
      barrierCnt = 0;
      }
    }

  emitBarriers(cmd,d,barrier,barrierCnt);
  }

void ResourceState::emitBarriers(AbstractGraphicsApi::CommandBuffer& cmd, AbstractGraphicsApi::SyncDesc& d, AbstractGraphicsApi::BarrierDesc* desc, size_t cnt) {
  if(cnt==0 && d.next==SyncStage::None)
    return;
  std::sort(desc,desc+cnt,[](const AbstractGraphicsApi::BarrierDesc& l, const AbstractGraphicsApi::BarrierDesc& r) {
    if(l.prev<r.prev)
      return true;
    if(l.prev>r.prev)
      return false;
    return l.next<r.next;
    });
  cmd.barrier(d,desc,cnt);
  }
