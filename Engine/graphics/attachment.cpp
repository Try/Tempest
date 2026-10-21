#include "attachment.h"

using namespace Tempest;

Attachment::Attachment(Attachment&& other)
  :tImpl(std::move(other.tImpl)),sImpl(other.sImpl),pImpl(std::move(other.pImpl)) {
  other.sImpl = {};
  }

Attachment& Attachment::operator=(Attachment&& other) {
  Attachment tmp(std::move(other));
  std::swap(tImpl,tmp.tImpl);
  std::swap(sImpl,tmp.sImpl);
  std::swap(pImpl,tmp.pImpl);
  return *this;
  }

Attachment::Attachment(AbstractGraphicsApi::Swapchain* sw, uint32_t id) {
  sImpl.swapchain = sw;
  sImpl.id        = id;
  }

int Attachment::w() const {
  if(pImpl)
    return int(pImpl.handler->width);
  if(sImpl.swapchain)
    return int(sImpl.swapchain->w());
  return tImpl.w();
  }

int Attachment::h() const {
  if(pImpl)
    return int(pImpl.handler->height);
  if(sImpl.swapchain)
    return int(sImpl.swapchain->h());
  return tImpl.h();
  }

Size Attachment::size() const {
  if(pImpl)
    return Size(int(pImpl.handler->width),int(pImpl.handler->height));
  if(sImpl.swapchain)
    return Size(int(sImpl.swapchain->w()),int(sImpl.swapchain->h()));
  return tImpl.size();
  }

bool Attachment::isEmpty() const {
  if(pImpl)
    return false;
  if(sImpl.swapchain)
    return sImpl.swapchain->w()==0 || sImpl.swapchain->h()==0;
  return tImpl.isEmpty();
  }

