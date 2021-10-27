#include "resourcestate.h"

using namespace Tempest;
using namespace Tempest::Detail;


void ResourceState::setLayout(AbstractGraphicsApi::Swapchain& s, uint32_t id, ResourceLayout lay, bool preserve) {
  State& img   = findImg(nullptr,&s,id,ResourceLayout::Present,preserve);
  img.next     = lay;
  img.preserve = preserve;
  img.outdated = true;
  }

void ResourceState::setLayout(AbstractGraphicsApi::Texture& a, ResourceLayout lay, bool preserve) {
  ResourceLayout def = ResourceLayout::Sampler;
  if(lay==ResourceLayout::DepthAttach)
    def = ResourceLayout::DepthAttach; // note: no readable depth

  State& img   = findImg(&a,nullptr,0,def,preserve);
  img.next     = lay;
  img.preserve = preserve;
  img.outdated = true;
  }

void ResourceState::setLayout(AbstractGraphicsApi::Buffer& b, ResourceLayout lay) {
  BufState& buf = findBuf(&b);
  buf.next      = lay;
  buf.outdated  = true;
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
    b.prev      = i.last;
    b.next      = i.next;
    b.preserve  = i.preserve;
    ++barrierCnt;

    i.last     = i.next;
    i.outdated = false;
    if(barrierCnt==MaxBarriers) {
      cmd.barrier(barrier,barrierCnt);
      barrierCnt = 0;
      }
    }

  for(auto& i:bufState) {
    if(!i.outdated)
      continue;

    if(i.last!=ResourceLayout::Undefined &&
       !(i.last==ResourceLayout::ComputeRead && i.next==i.last)) {
      auto& b = barrier[barrierCnt];
      b.buffer = i.buf;
      b.prev   = i.last;
      b.next   = i.next;
      ++barrierCnt;
      }

    i.last     = i.next;
    i.outdated = false;
    if(barrierCnt==MaxBarriers) {
      cmd.barrier(barrier,barrierCnt);
      barrierCnt = 0;
      }
    }
  cmd.barrier(barrier,barrierCnt);
  }

void ResourceState::finalize(AbstractGraphicsApi::CommandBuffer& cmd) {
  if(imgState.size()==0 && bufState.size()==0)
    return; // early-out
  flush(cmd);
  imgState.reserve(imgState.size());
  imgState.clear();
  bufState.reserve(bufState.size());
  bufState.clear();
  }

ResourceState::State& ResourceState::findImg(AbstractGraphicsApi::Texture* img, AbstractGraphicsApi::Swapchain* sw, uint32_t id,
                                             ResourceLayout def, bool preserve) {
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
  s.next     = ResourceLayout::Undefined;
  s.preserve = preserve;
  s.outdated = false;
  imgState.push_back(s);
  return imgState.back();
  }

ResourceState::BufState& ResourceState::findBuf(AbstractGraphicsApi::Buffer* buf) {
  for(auto& i:bufState)
    if(i.buf==buf)
      return i;
  BufState s={};
  s.buf      = buf;
  s.last     = ResourceLayout::Undefined;
  s.outdated = false;
  bufState.push_back(s);
  return bufState.back();
  }
