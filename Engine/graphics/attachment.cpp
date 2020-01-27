#include "attachment.h"

using namespace Tempest;

Attachment::Attachment(AbstractGraphicsApi::Swapchain* sw, uint32_t id) {
  sImpl.swapchain = sw;
  sImpl.id        = id;
  }

int Attachment::w() const {
  if(sImpl.swapchain)
    return int(sImpl.swapchain->w());
  return tImpl.w();
  }

int Attachment::h() const {
  if(sImpl.swapchain)
    return int(sImpl.swapchain->h());
  return tImpl.h();
  }

Size Attachment::size() const {
  if(sImpl.swapchain)
    return Size(int(sImpl.swapchain->w()),int(sImpl.swapchain->h()));
  return tImpl.size();
  }

bool Attachment::isEmpty() const {
  if(sImpl.swapchain)
    return int(sImpl.swapchain->w()> 0 && sImpl.swapchain->h()>0);
  return tImpl.isEmpty();
  }

