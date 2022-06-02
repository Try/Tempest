#include "resourcestate.h"

using namespace Tempest;
using namespace Tempest::Detail;

void ResourceState::setRenderpass(AbstractGraphicsApi::CommandBuffer& cmd,
                                  const AttachmentDesc* desc, size_t descSize,
                                  const TextureFormat* frm, AbstractGraphicsApi::Texture** att,
                                  AbstractGraphicsApi::Swapchain** sw, const uint32_t* imgId) {
  for(size_t i=0; i<descSize; ++i) {
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
    const bool discard = desc[i].store!=AccessOp::Preserve;
    if(isDepthFormat(frm[i]))
      setLayout(*att[i],ResourceAccess::DepthAttach,discard);
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
    def = ResourceAccess::DepthAttach; // note: no readable depth

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

void ResourceState::onTranferUsage(NonUniqResId read, NonUniqResId write) {
  if(read!=NonUniqResId::I_None)
    uavSrcBarrier = uavSrcBarrier| ResourceAccess::TransferSrc;
  if(write!=NonUniqResId::I_None)
    uavDstBarrier = uavDstBarrier| ResourceAccess::TransferDst;
  }

void ResourceState::onUavUsage(const Usage& u, PipelineStage st) {
  ResourceAccess rd [PipelineStage::S_Count] = {ResourceAccess::UavReadComp, ResourceAccess::UavReadGr};
  ResourceAccess wr [PipelineStage::S_Count] = {ResourceAccess::UavWriteComp,ResourceAccess::UavWriteGr};

  for(PipelineStage p = PipelineStage::S_First; p<PipelineStage::S_Count; p = PipelineStage(p+1)) {
    if((uavRead [st].depend[p] & u.write)!=0 ||
       (uavWrite[st].depend[p] & u.read)!=0) {
      // WaW, RaW barrier - execution+cache
      uavSrcBarrier = uavSrcBarrier | rd[p];
      uavSrcBarrier = uavSrcBarrier | wr[p];

      uavDstBarrier = uavDstBarrier | rd[st];
      uavDstBarrier = uavDstBarrier | wr[st]; // improve?

      uavRead [st].depend[p] = u.read;
      uavWrite[st].depend[p] = u.write;
      }
    else if((uavRead[st].depend[p] & u.write)!=0) {
      // WaR barrier - only exec barrier
      uavSrcBarrier          = uavSrcBarrier | rd[p];
      uavDstBarrier          = uavDstBarrier | wr[st];
      uavRead [st].depend[p] = NonUniqResId::I_None;
      uavWrite[st].depend[p] = u.write;
      }
    else {
      // RaR - no barrier needed
      uavRead [p].depend[st] |= u.read;
      uavWrite[p].depend[st] |= u.write;
      }
    }
  }

void ResourceState::joinCompute(PipelineStage st) {
  ResourceAccess dst[PipelineStage::S_Count] = {ResourceAccess::UavReadWriteComp, ResourceAccess::UavReadWriteGr};

  auto& usage = uavWrite[PipelineStage::S_Compute];
  if(/*uavUsage.read!=0 ||*/ usage.any!=0) {
    // NOTE: VS/FS side effects will require WaR barrier
    if(uavRead[PipelineStage::S_Compute].any!=0)
      uavSrcBarrier = uavSrcBarrier | ResourceAccess::UavReadComp;
    if(uavWrite[PipelineStage::S_Compute].any!=0)
      uavSrcBarrier = uavSrcBarrier | ResourceAccess::UavWriteComp;
    uavDstBarrier = uavDstBarrier | dst[st];

    uavRead [st].depend[PipelineStage::S_Compute] = NonUniqResId::I_None;
    uavWrite[st].depend[PipelineStage::S_Compute] = NonUniqResId::I_None;
    }
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
    i.any = 0;
  for(auto& i:uavWrite)
    i.any = 0;
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
