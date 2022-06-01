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

void ResourceState::onUavUsage(uint32_t read, uint32_t write, PipelineStage st) {
  ResourceState::Usage uavUsage;
  uavUsage.read  = read;
  uavUsage.write = write;
  onUavUsage(uavUsage,st);
  }

void ResourceState::onUavUsage(const Usage& u, PipelineStage st) {
  ResourceAccess rd[PipelineStage::S_Count] = {ResourceAccess::None,ResourceAccess::UavReadComp, ResourceAccess::UavReadGr};
  ResourceAccess wr[PipelineStage::S_Count] = {ResourceAccess::None,ResourceAccess::UavWriteComp,ResourceAccess::UavWriteGr};

  for(PipelineStage p = PipelineStage::S_Tranfer; p<PipelineStage::S_Count; p = PipelineStage(p+1)) {
    auto& usagePrev = uavUsage[p];
    if((usagePrev.write & u.write)!=0 ||
       (usagePrev.write & u.read) !=0) {
      // WaW, RaW barrier
      uavPrev      = uavPrev | rd[p];
      uavPrev      = uavPrev | wr[p];
      uavUsage[p]  = Usage();
      }
    else if((usagePrev.read & u.write)!=0) {
      // WaR barrier
      uavPrev          = uavPrev | rd[p];
      uavUsage[p].read = 0;
      }
    }
  uavUsage[st].read  |= u.read;
  uavUsage[st].write |= u.write;
  }

void ResourceState::joinCompute(AbstractGraphicsApi::CommandBuffer& cmd) {
  auto& usage = uavUsage[PipelineStage::S_Compute];
  if(/*uavUsage.read!=0 ||*/ usage.write!=0) {
    // NOTE: VS/FS side effects will require WaR barrier
    if(usage.read !=0)
      uavPrev = uavPrev | ResourceAccess::UavReadComp;
    if(usage.write !=0)
      uavPrev = uavPrev | ResourceAccess::UavWriteComp;
    usage = ResourceState::Usage();
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

  if(uavPrev!=ResourceAccess::None) {
    auto& b = barrier[barrierCnt];
    b.buffer = nullptr;
    b.prev   = uavPrev;
    b.next   = ResourceAccess::UavReadWriteAll; //TODO
    ++barrierCnt;
    uavPrev = ResourceAccess::None;
    }
  emitBarriers(cmd,barrier,barrierCnt);
  }

void ResourceState::finalize(AbstractGraphicsApi::CommandBuffer& cmd) {
  for(auto& i:uavUsage)
    if(i.write!=0)
      uavPrev = ResourceAccess::UavReadWriteAll;

  if(imgState.size()==0 && uavPrev==ResourceAccess::None)
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
  for(auto& i:uavUsage)
    i = ResourceState::Usage();
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
