#include "vectorimage.h"

#include <Tempest/Device>
#include <Tempest/Builtin>
#include <Tempest/Painter>
#include <Tempest/Event>

#define  NANOSVG_IMPLEMENTATION
#include "thirdparty/nanosvg.h"

using namespace Tempest;

void VectorImage::beginPaint(bool clr, uint32_t w, uint32_t h) {
  if(clr || blocks.size()==0)
    clear();
  info.w=w;
  info.h=h;
  }

void VectorImage::endPaint() {
  for(size_t i=0;i<frameCount;++i)
    frame[i].outdated=true;
  outdatedCount=frameCount;
  }

size_t VectorImage::pushState() {
  size_t sz=stateStk.size();
  stateStk.push_back(blocks.back());
  return sz;
  }

void VectorImage::popState(size_t id) {
  State& s = stateStk[id];
  State& b = reinterpret_cast<State&>(blocks.back());
  if(b==s)
    return;
  if(blocks.back().size==0) {
    b=s;
    } else {
    blocks.emplace_back(s);
    blocks.back().begin =buf.size();
    blocks.back().size  =0;
    }
  stateStk.resize(id);
  }

template<class T,T VectorImage::State::*param>
void VectorImage::setState(const T &t) {
  // blocks.size()>0, see VectorImage::clear()
  if(blocks.back().*param==t)
    return;

  if(blocks.back().size==0){
    blocks.back().*param=t;
    return;
    }

  blocks.push_back(blocks.back());
  blocks.back().begin =buf.size();
  blocks.back().size  =0;
  blocks.back().*param=t;
  }

void VectorImage::setState(const TexPtr &t,const Color&) {
  Texture tex={t,Sprite()};
  setState<Texture,&State::tex>(tex);
  blocks.back().hasImg=bool(t);
  }

void VectorImage::setState(const Sprite &s, const Color&) {
  Texture tex={TexPtr(),s};
  setState<Texture,&State::tex>(tex);
  blocks.back().hasImg=!s.isEmpty();

  slock.insert(s);
  }

void VectorImage::setTopology(Topology t) {
  setState<Topology,&State::tp>(t);
  }

void VectorImage::setBlend(const Painter::Blend b) {
  setState<Painter::Blend,&State::blend>(b);
  }

void VectorImage::clear() {
  buf.clear();
  blocks.resize(1);
  blocks.back()=Block();
  stateStk.clear();
  slock.clear();
  }

void VectorImage::addPoint(const PaintDevice::Point &p) {
  buf.push_back(p);
  blocks.back().size++;
  }

void VectorImage::commitPoints() {
  blocks.resize(blocks.size());

  while(blocks.size()>1){
    if(blocks.back().size!=0)
      return;
    blocks.pop_back();
    }
  }

void VectorImage::makeActual(Device &dev,RenderPass& pass) {
  if(!frame || frameCount!=dev.maxFramesInFlight()) {
    uint8_t count=dev.maxFramesInFlight();
    frame.reset(new PerFrame[count]);
    frameCount=count;
    }

  PerFrame& f=frame[dev.frameId()];
  if(f.outdated) {
    f.vbo=dev.loadVbo(buf,BufferFlags::Static);
    f.blocks.reserve(blocks.size());
    f.blocks.clear();

    for(auto& i:blocks){
      Uniforms ux;
      if(i.hasImg) {
        ux=dev.uniforms(dev.builtin().texture2d(pass,info.w,info.h).layout);
        if(i.tex.brush)
          ux.set(0,i.tex.brush); else
          ux.set(0,i.tex.sprite.pageRawData(dev)); //TODO: oom
        } else {
        ux=dev.uniforms(dev.builtin().empty(pass,info.w,info.h).layout);
        }
      f.blocks.push_back(std::move(ux));
      }

    f.outdated=false;
    outdatedCount--;
    if(outdatedCount==0)
      clear();
    }
  }

void VectorImage::draw(Device & dev, CommandBuffer &cmd, RenderPass& pass) {
  makeActual(dev,pass);

  PerFrame& f=frame[dev.frameId()];

  for(size_t i=0;i<blocks.size();++i){
    auto& b=blocks[i];
    auto& u=f.blocks[i];

    if(b.size==0)
      continue;

    if(!b.pipeline) {
      const RenderPipeline* p;
      if(b.hasImg) {
        if(b.tp==Triangles){
          if(b.blend==NoBlend)
            p=&dev.builtin().texture2d(pass,info.w,info.h).brush; else
          if(b.blend==Alpha)
            p=&dev.builtin().texture2d(pass,info.w,info.h).brushB; else
            p=&dev.builtin().texture2d(pass,info.w,info.h).brushA;
          } else {
          if(b.blend==NoBlend)
            p=&dev.builtin().texture2d(pass,info.w,info.h).pen; else
          if(b.blend==Alpha)
            p=&dev.builtin().texture2d(pass,info.w,info.h).penB; else
            p=&dev.builtin().texture2d(pass,info.w,info.h).penA;
          }
        } else {
        if(b.tp==Triangles) {
          if(b.blend==NoBlend)
            p=&dev.builtin().empty(pass,info.w,info.h).brush; else
          if(b.blend==Alpha)
            p=&dev.builtin().empty(pass,info.w,info.h).brushB; else
            p=&dev.builtin().empty(pass,info.w,info.h).brushA;
          } else {
          if(b.blend==NoBlend)
            p=&dev.builtin().empty(pass,info.w,info.h).pen; else
          if(b.blend==Alpha)
            p=&dev.builtin().empty(pass,info.w,info.h).penB; else
            p=&dev.builtin().empty(pass,info.w,info.h).penA;
          }
        }
      b.pipeline=PipePtr(*p);
      }

    if(b.hasImg)
      cmd.setUniforms(b.pipeline,u); else
      cmd.setUniforms(b.pipeline);
    cmd.draw(f.vbo,b.begin,b.size);
    }
  }

bool VectorImage::load(const char *file) {
  NSVGimage* image = nsvgParseFromFile(file,"px",96);
  if(image==nullptr)
    return false;

  try {
    VectorImage img;
    //PaintEvent  event(img,int(image->width),int(image->height));
    //Painter     p(event);

    for(NSVGshape* shape=image->shapes; shape!=nullptr; shape=shape->next) {
      for(NSVGpath* path=shape->paths; path!=nullptr; path=path->next) {
        for(int i=0; i<path->npts-1; i+=3) {
          const float* p = &path->pts[i*2];
          //drawCubicBez(p[0],p[1], p[2],p[3], p[4],p[5], p[6],p[7]);
          }
        }
      }

    *this=std::move(img);
    }
  catch(...){
    nsvgDelete(image);
    return false;
    }
  nsvgDelete(image);
  return true;
  }

