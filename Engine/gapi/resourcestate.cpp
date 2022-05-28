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

void ResourceState::onUavUsage(uint64_t read, uint64_t write) {
  ResourceState::Usage uavUsage;
  uavUsage.read  = read;
  uavUsage.write = write;
  onUavUsage(uavUsage);
  }

void ResourceState::onUavUsage(const Usage& u) {
  if((uavUsage.write & u.read) !=0 ||
     (uavUsage.write & u.write)!=0 ){
    // RaW, WaW barrier
    needUavRBarrier = true;
    needUavWBarrier = true;
    uavUsage        = u;
    }
  else if((uavUsage.read & u.write)!=0) {
    // WaR barrier
    needUavRBarrier = true;
    uavUsage        = u;
    }
  else {
    uavUsage.read  |= u.read;
    uavUsage.write |= u.write;
    }
  }

void ResourceState::joinCompute(AbstractGraphicsApi::CommandBuffer& cmd) {
  if(/*uavUsage.read!=0 ||*/ uavUsage.write!=0) {
    // NOTE: VS/FS side effects will require WaR barrier
    needUavRBarrier = (uavUsage.read !=0);
    needUavWBarrier = (uavUsage.write!=0);
    uavUsage        = ResourceState::Usage();
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

  if(needUavRBarrier || needUavWBarrier) {
    auto& b = barrier[barrierCnt];
    b.buffer = nullptr;
    if(needUavWBarrier)
      b.prev = ResourceAccess::UavReadWrite; else
      b.prev = ResourceAccess::UavRead;
    b.next   = ResourceAccess::UavReadWrite;
    ++barrierCnt;
    needUavRBarrier = false;
    needUavWBarrier = false;
    }
  emitBarriers(cmd,barrier,barrierCnt);
  }

void ResourceState::finalize(AbstractGraphicsApi::CommandBuffer& cmd) {
  if(imgState.size()==0 && needUavRBarrier==false && needUavWBarrier==false)
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
  needUavRBarrier = false;
  needUavWBarrier = false;
  uavUsage        = ResourceState::Usage();
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
