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
    if(desc[i].load==AccessOp::Readonly)
      continue;
    const bool discard = desc[i].load!=AccessOp::Preserve;
    if(isDepthFormat(frm[i]))
      setLayout(*att[i],ResourceAccess::DepthAttach,discard);
    else if(frm[i]==TextureFormat::Undefined)
      setLayout(*sw[i],imgId[i],ResourceAccess::ColorAttach,discard);
    else
      setLayout(*att[i],ResourceAccess::ColorAttach,discard);
    }
  flush(cmd);
  for(size_t i=0; i<descSize; ++i) {
    if(desc[i].load==AccessOp::Readonly)
      continue;
    const bool discard = desc[i].store!=AccessOp::Preserve;
    if(isDepthFormat(frm[i]))
      setLayout(*att[i],ResourceAccess::DepthReadOnly,discard);
    else if(frm[i]==TextureFormat::Undefined)
      setLayout(*sw[i],imgId[i],ResourceAccess::ColorAttach,discard); // execution barrier
    else
      setLayout(*att[i],ResourceAccess::Sampler,discard);
    }
  }

void ResourceState::setLayout(AbstractGraphicsApi::Swapchain& s, uint32_t id, ResourceAccess lay, bool discard) {
  ImgState& img   = findImg(nullptr,&s,id,ResourceAccess::Present,discard);
  img.next     = lay;
  img.discard  = discard;
  img.outdated = true;
  }

void ResourceState::setLayout(AbstractGraphicsApi::Texture& a, ResourceAccess lay, bool discard) {
  ResourceAccess def = ResourceAccess::Sampler;
  if(lay==ResourceAccess::DepthAttach)
    def = ResourceAccess::DepthReadOnly;

  ImgState& img = findImg(&a,nullptr,0,def,discard);
  img.next     = lay;
  img.discard  = discard;
  img.outdated = true;
  }

void ResourceState::forceLayout(AbstractGraphicsApi::Texture& img) {
  for(auto& i:imgState) {
    if(i.sw==nullptr && i.id==0 && i.img==&img) {
      i.last     = i.next;
      i.outdated = false;
      return;
      }
    }
  }

void ResourceState::onTranferUsage(NonUniqResId read, NonUniqResId write, bool host) {
  ResourceState::Usage u = {read, write, false};
  onUavUsage(u, PipelineStage::S_Transfer, host);
  }

void ResourceState::onUavUsage(NonUniqResId read, NonUniqResId write, PipelineStage st) {
  ResourceState::Usage u = {read, write, false};
  onUavUsage(u, st, false);
  }

void ResourceState::onUavUsage(const Usage& u, PipelineStage st, bool host) {
  const ResourceAccess rd[PipelineStage::S_Count] = {ResourceAccess::TransferSrc, ResourceAccess::RtAsRead,  ResourceAccess::UavReadComp,  ResourceAccess::UavReadGr, ResourceAccess::Indirect};
  const ResourceAccess wr[PipelineStage::S_Count] = {ResourceAccess::TransferDst, ResourceAccess::RtAsWrite, ResourceAccess::UavWriteComp, ResourceAccess::UavWriteGr, ResourceAccess::None};
  const ResourceAccess hv = (host ? ResourceAccess::TransferHost : ResourceAccess::None);

  for(PipelineStage p = PipelineStage::S_First; p<PipelineStage::S_Count; p = PipelineStage(p+1)) {
    if((uavWrite[st].depend[p] & u.write)!=0 ||
       (uavWrite[st].depend[p] & u.read)!=0) {
      // WaW, RaW barrier - execution+cache
      uavSrcBarrier = uavSrcBarrier | rd[p] | wr[p];
      uavDstBarrier = uavDstBarrier | rd[st] | wr[st];

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

    if(host) {
      uavDstBarrier = uavDstBarrier | hv;
      }
    }

  for(PipelineStage p = PipelineStage::S_First; p<PipelineStage::S_Count; p = PipelineStage(p+1)) {
    uavRead [p].depend[st] |= u.read;
    uavWrite[p].depend[st] |= u.write;
    }
  }

void ResourceState::joinWriters(PipelineStage st) {
  ResourceState::Usage u = {NonUniqResId(-1), NonUniqResId::I_None, false};
  onUavUsage(u, st);
  }

void ResourceState::clearReaders() {
  for(auto& i:uavRead)
    for(auto& r:i.depend)
      r = NonUniqResId::I_None;
  }

void ResourceState::flush(AbstractGraphicsApi::CommandBuffer& cmd) {
  AbstractGraphicsApi::BarrierDesc barrier[MaxBarriers];
  uint8_t                          barrierCnt = 0;

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
      emitBarriers(cmd,barrier,barrierCnt);
      barrierCnt = 0;
      }
    }

  if(uavSrcBarrier!=ResourceAccess::None) {
    auto& b = barrier[barrierCnt];
    b.buffer = nullptr;
    b.prev   = uavSrcBarrier;
    b.next   = uavDstBarrier;
    ++barrierCnt;
    uavSrcBarrier = ResourceAccess::None;
    uavDstBarrier = ResourceAccess::None;
    }
  emitBarriers(cmd,barrier,barrierCnt);
  }

void ResourceState::finalize(AbstractGraphicsApi::CommandBuffer& cmd) {
  for(PipelineStage p = PipelineStage::S_First; p<PipelineStage::S_Count; p = PipelineStage(p+1)) {
    joinWriters(p);
    }

  if(imgState.size()==0 && uavSrcBarrier==ResourceAccess::None)
    return; // early-out

  for(auto& i:imgState) {
    if(i.sw==nullptr)
      continue;
    i.next     = ResourceAccess::Present;
    i.outdated = true;
    }
  flush(cmd);
  imgState.reserve(imgState.size());
  imgState.clear();
  uavSrcBarrier = ResourceAccess::None;
  uavDstBarrier = ResourceAccess::None;

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

ResourceState::ImgState& ResourceState::findImg(AbstractGraphicsApi::Texture* img, AbstractGraphicsApi::Swapchain* sw, uint32_t id,
                                                ResourceAccess def, bool discard) {
  auto nativeImg = img;
  for(auto& i:imgState) {
    if(i.sw==sw && i.id==id && i.img==nativeImg)
      return i;
    }
  ImgState s={};
  s.sw       = sw;
  s.id       = id;
  s.img      = img;
  s.last     = def;
  s.next     = ResourceAccess::Sampler;
  s.discard  = discard;
  s.outdated = false;
  imgState.push_back(s);
  return imgState.back();
  }

void ResourceState::emitBarriers(AbstractGraphicsApi::CommandBuffer& cmd, AbstractGraphicsApi::BarrierDesc* desc, size_t cnt) {
  if(cnt==0)
    return;
  std::sort(desc,desc+cnt,[](const AbstractGraphicsApi::BarrierDesc& l, const AbstractGraphicsApi::BarrierDesc& r) {
    if(l.prev<r.prev)
      return true;
    if(l.prev>r.prev)
      return false;
    return l.next<r.next;
    });
  cmd.barrier(desc,cnt);
  }
