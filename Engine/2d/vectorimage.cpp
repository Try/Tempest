#include "vectorimage.h"

#include <Tempest/Device>
#include <Tempest/Builtin>

using namespace Tempest;

void VectorImage::beginPaint(bool clear, uint32_t w, uint32_t h) {
  if(clear)
    buf.clear();
  info.w=w;
  info.h=h;
  }

void VectorImage::endPaint() {
  for(size_t i=0;i<frameCount;++i)
    frame[i].outdated=true;
  outdatedCount=frameCount;
  }

void VectorImage::setBrush(const Texture2d *t, float r, float g, float b, float a) {
  brush = t ? Detail::ResourcePtr<Texture2d>(*t) : Detail::ResourcePtr<Texture2d>();
  }

void VectorImage::clear() {
  buf.clear();
  }

void VectorImage::addPoint(const PaintDevice::Point &p) {
  buf.push_back(p);
  }

void VectorImage::commitPoints() {
  }

void VectorImage::makeActual(Device &dev) {
  if(!frame || frameCount!=dev.maxFramesInFlight()) {
    uint8_t count=dev.maxFramesInFlight();
    frame.reset(new PerFrame[count]);
    frameCount=count;
    }

  PerFrame& f=frame[dev.frameId()];
  if(f.outdated) {
    f.vbo=dev.loadVbo(buf,BufferFlags::Static);
    f.ubo=dev.uniforms(dev.builtin().brushTexture2dLayout());
    f.ubo.set(0,brush);

    f.outdated=false;
    outdatedCount--;
    if(outdatedCount==0)
      buf.clear();
    }
  }

void VectorImage::draw(Device & dev, CommandBuffer &cmd, RenderPass& pass) {
  makeActual(dev);

  PerFrame& f=frame[dev.frameId()];

  auto& pipeline=dev.builtin().brushTexture2d(pass,info.w,info.h);
  cmd.setUniforms(pipeline,f.ubo);
  cmd.draw(f.vbo,f.vbo.size());
  }

