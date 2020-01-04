#include "frame.h"

using namespace Tempest;

Frame::Frame(){
  }

Frame::Frame(AbstractGraphicsApi::Image *img, AbstractGraphicsApi::Swapchain* sw, uint32_t id)
  :img(img),swapchain(sw),id(id) {
  }

Frame::Frame(Frame &&f) : img(f.img) {
  f.img=nullptr;
  }

Frame::~Frame() {
  }
