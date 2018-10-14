#include "frame.h"

using namespace Tempest;

Frame::Frame(){
  }

Frame::Frame(Device &dev, AbstractGraphicsApi::Image *img,uint32_t id)
  :dev(&dev),img(img),id(id) {
  }

Frame::Frame(Frame &&f) : dev(f.dev), img(f.img) {
  f.img=nullptr;
  }

Frame::~Frame() {
  }
