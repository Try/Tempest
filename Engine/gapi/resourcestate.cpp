#include "resourcestate.h"

using namespace Tempest;
using namespace Tempest::Detail;


void ResourceState::setLayout(AbstractGraphicsApi::Swapchain& s, uint32_t id, TextureLayout lay, bool preserve) {
  State& img   = findImg(nullptr,&s,id,TextureLayout::Present,preserve);
  img.next     = lay;
  img.preserve = preserve;
  img.outdated = true;
  }

void ResourceState::setLayout(AbstractGraphicsApi::Texture& a, TextureLayout lay, bool preserve) {
  TextureLayout def = TextureLayout::Sampler;
  if(lay==TextureLayout::DepthAttach)
    def = TextureLayout::DepthAttach; // note: no readable depth

  State& img   = findImg(&a,nullptr,0,def,preserve);
  img.next     = lay;
  img.preserve = preserve;
  img.outdated = true;
  }

void ResourceState::setLayout(AbstractGraphicsApi::Buffer& b, BufferLayout lay) {
  BufState& buf = findBuf(&b);
  buf.next      = lay;
  buf.outdated  = true;
  }

void ResourceState::flushLayout(AbstractGraphicsApi::CommandBuffer& cmd) {
  AbstractGraphicsApi::BarrierDesc barrier[128];
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
    if(barrierCnt==128) {
      cmd.changeLayout(barrier,barrierCnt);
      barrierCnt = 0;
      }
    }
  cmd.changeLayout(barrier,barrierCnt);
  }

void ResourceState::flushSSBO(AbstractGraphicsApi::CommandBuffer& cmd) {
  for(auto& buf:bufState) {
    if(!buf.outdated)
      continue;
    if(buf.last!=BufferLayout::Undefined &&
       !(buf.last==BufferLayout::ComputeRead && buf.next==buf.last)) {
      cmd.changeLayout(*buf.buf,buf.last,buf.next);
      }
    buf.last     = buf.next;
    buf.outdated = false;
    }
  }

void ResourceState::finalize(AbstractGraphicsApi::CommandBuffer& cmd) {
  if(imgState.size()==0 && bufState.size()==0)
    return; // early-out
  flushLayout(cmd);
  flushSSBO(cmd);
  imgState.reserve(imgState.size());
  imgState.clear();
  bufState.reserve(bufState.size());
  bufState.clear();
  }

ResourceState::State& ResourceState::findImg(AbstractGraphicsApi::Texture* img, AbstractGraphicsApi::Swapchain* sw, uint32_t id,
                                             TextureLayout def, bool preserve) {
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
  s.next     = TextureLayout::Undefined;
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
  s.last     = BufferLayout::Undefined;
  s.outdated = false;
  bufState.push_back(s);
  return bufState.back();
  }
