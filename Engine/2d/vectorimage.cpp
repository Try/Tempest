#include "vectorimage.h"

#include <Tempest/Device>
#include <Tempest/Builtin>
#include <Tempest/Painter>
#include <Tempest/Event>
#include <Tempest/Encoder>

#define  NANOSVG_IMPLEMENTATION
#include "thirdparty/nanosvg.h"

using namespace Tempest;

void VectorImage::beginPaint(bool clr, uint32_t w, uint32_t h) {
  if(clr || blocks.size()==0)
    clear();
  if(clr) {
    info.w=w;
    info.h=h;
    }
  paintScope++;
  }

void VectorImage::endPaint() {
  paintScope--;
  if(paintScope!=0)
    return;
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

void VectorImage::setState(const TexPtr &t, const Color&, TextureFormat frm, ClampMode clamp) {
  Texture tex={t,frm,clamp,Sprite()};
  setState<Texture,&State::tex>(tex);
  blocks.back().hasImg=bool(t);
  }

void VectorImage::setState(const Sprite &s, const Color&) {
  Texture tex={TexPtr(),TextureFormat::Undefined,ClampMode::Repeat,s};
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

const RenderPipeline& VectorImage::pipelineOf(Device& dev, const VectorImage::Block& b) const {
  const RenderPipeline* p;
  if(b.hasImg) {
    if(b.tp==Triangles){
      if(b.blend==NoBlend)
        p=&dev.builtin().texture2d().brush; else
      if(b.blend==Alpha)
        p=&dev.builtin().texture2d().brushB; else
        p=&dev.builtin().texture2d().brushA;
      } else {
      if(b.blend==NoBlend)
        p=&dev.builtin().texture2d().pen; else
      if(b.blend==Alpha)
        p=&dev.builtin().texture2d().penB; else
        p=&dev.builtin().texture2d().penA;
      }
    } else {
    if(b.tp==Triangles) {
      if(b.blend==NoBlend)
        p=&dev.builtin().empty().brush; else
      if(b.blend==Alpha)
        p=&dev.builtin().empty().brushB; else
        p=&dev.builtin().empty().brushA;
      } else {
      if(b.blend==NoBlend)
        p=&dev.builtin().empty().pen; else
      if(b.blend==Alpha)
        p=&dev.builtin().empty().penB; else
        p=&dev.builtin().empty().penA;
      }
    }
  return *p;
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
          const float* p = &path->pts[i*2]; (void)p;
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


void VectorImage::Mesh::update(Device& dev, const VectorImage& src, BufferHeap heap) {
  if(vbo.size()==src.buf.size())
    vbo.update(src.buf); else
    vbo=dev.vbo(heap,src.buf);

  blocks.resize(src.blocks.size());

  for(size_t i=0;i<blocks.size();++i){
    auto& b  = src.blocks[i];
    auto& ux = blocks[i];

    ux.begin = b.begin;
    ux.size  = b.size;

    auto& p = src.pipelineOf(dev,b);
    ux.pipeline = &p;

    if(T_LIKELY(b.hasImg)) {
      Sampler s = Sampler::bilinear();
      if(b.tex.brush) {
        s.uClamp   = b.tex.clamp;
        s.vClamp   = b.tex.clamp;
        ux.smp     = s;
        ux.format  = b.tex.frm;
        ux.texture = b.tex.brush;
        } else {
        auto& tex = b.tex.sprite.pageRawData(dev); //TODO: oom
        ux.texture = TexPtr(tex);
        ux.smp     = s;
        ux.format  = tex.format();
        ux.sprite  = b.tex.sprite;
        }
      }
    }
  }

void VectorImage::Mesh::draw(Encoder<CommandBuffer>& cmd) const {
  for(size_t i=0;i<blocks.size();++i){
    auto& b = blocks[i];
    if(b.size==0)
      continue;

    if(b.texture) {
      ComponentMapping mapping;
      if(T_UNLIKELY(b.format==TextureFormat::R8 || b.format==TextureFormat::R16 || b.format==TextureFormat::R32F)) {
        mapping.r = ComponentSwizzle::R;
        mapping.g = ComponentSwizzle::R;
        mapping.b = ComponentSwizzle::R;
        }
      else if(T_UNLIKELY(b.format==TextureFormat::RG8 || b.format==TextureFormat::RG16 || b.format==TextureFormat::RG32F)) {
        mapping.r = ComponentSwizzle::R;
        mapping.g = ComponentSwizzle::R;
        mapping.b = ComponentSwizzle::R;
        mapping.a = ComponentSwizzle::G;
        }
      cmd.setBinding(0, b.texture, mapping, b.smp);
      }
    cmd.setPipeline(*b.pipeline);
    cmd.draw(vbo,b.begin,b.size);
    }
  }
