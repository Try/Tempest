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
  State& img   = findImg(nullptr,&s,id,ResourceAccess::Present,discard);
  img.next     = lay;
  img.discard  = discard;
  img.outdated = true;
  }

void ResourceState::setLayout(AbstractGraphicsApi::Texture& a, ResourceAccess lay, bool discard) {
  ResourceAccess def = ResourceAccess::Sampler;
  if(lay==ResourceAccess::DepthAttach)
    def = ResourceAccess::DepthAttach; // note: no readable depth

  State& img   = findImg(&a,nullptr,0,def,discard);
  img.next     = lay;
  img.discard  = discard;
  img.outdated = true;
  }

void ResourceState::setLayout(AbstractGraphicsApi::Buffer& buf, ResourceAccess lay) {
  for(auto& i:bufState)
    if(i.buf==&buf) {
      i.next      = lay;
      i.outdated  = true;
      return;
      }

  // read access assumed as default; RaR barrier are equal to NOP
  if(lay==ResourceAccess::ComputeRead)
    return;

  BufState s = {};
  s.buf      = &buf;
  s.last     = ResourceAccess::ComputeRead;
  s.next     = lay;
  s.outdated = true;
  bufState.push_back(s);
  }

void ResourceState::joinCompute() {
  for(auto& i:bufState) {
    if(i.buf==nullptr)
      continue;
    if((i.last & ResourceAccess::ComputeWrite)!=ResourceAccess::ComputeWrite)
      continue;
    i.next     = ResourceAccess::ComputeRead;
    i.outdated = true;
    }
  }

void ResourceState::flush(AbstractGraphicsApi::CommandBuffer& cmd) {
  AbstractGraphicsApi::BarrierDesc barrier[MaxBarriers];
  uint8_t                          barrierCnt = 0;

  for(auto& i:bufState) {
    if(!i.outdated)
      continue;
    if(i.last==ResourceAccess::ComputeRead && i.next==ResourceAccess::ComputeRead)
      continue;

    auto& b = barrier[barrierCnt];
    b.buffer = i.buf;
    b.prev   = i.last;
    b.next   = i.next;
    ++barrierCnt;

    i.last     = i.next;
    i.outdated = false;
    if(barrierCnt==MaxBarriers) {
      emitBarriers(cmd,barrier,barrierCnt);
      barrierCnt = 0;
      }
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
      emitBarriers(cmd,barrier,barrierCnt);
      barrierCnt = 0;
      }
    }

  emitBarriers(cmd,barrier,barrierCnt);
  }

void ResourceState::finalize(AbstractGraphicsApi::CommandBuffer& cmd) {
  if(imgState.size()==0 && bufState.size()==0)
    return; // early-out
  for(auto& i:bufState) {
    if(i.buf==nullptr)
      continue;
    if((i.last & ResourceAccess::ComputeWrite)!=ResourceAccess::ComputeWrite)
      continue;
    i.next     = ResourceAccess::ComputeRead;
    i.outdated = true;
    }
  for(auto& i:imgState) {
    if(i.sw==nullptr)
      continue;
    i.next     = ResourceAccess::Present;
    i.outdated = true;
    }
  flush(cmd);
  imgState.reserve(imgState.size());
  imgState.clear();
  bufState.reserve(bufState.size());
  bufState.clear();
  }

ResourceState::State& ResourceState::findImg(AbstractGraphicsApi::Texture* img, AbstractGraphicsApi::Swapchain* sw, uint32_t id,
                                             ResourceAccess def, bool discard) {
  auto nativeImg = img;
  for(auto& i:imgState) {
    if(i.sw==sw && i.id==id && i.img==nativeImg)
      return i;
    }
  State s={};
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
