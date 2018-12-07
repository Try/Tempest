#pragma once

#include <Tempest/PaintDevice>
#include <Tempest/VertexBuffer>
#include <Tempest/Rect>
#include <Tempest/Uniforms>
#include <Tempest/Sprite>

#include <vector>

namespace Tempest {

class Device;
class CommandBuffer;

class VectorImage : public Tempest::PaintDevice {
  public:
    VectorImage()=default;

    uint32_t w() const { return info.w; }
    uint32_t h() const { return info.h; }

    void     draw(Device& dev, CommandBuffer& cmd, RenderPass &pass);
    bool     load(const char* path);
    void     clear() override;

  private:
    void   addPoint(const Point& p) override;
    void   commitPoints() override;

    void   beginPaint(bool clear,uint32_t w,uint32_t h) override;
    void   endPaint() override;

    size_t pushState() override;
    void   popState(size_t id) override;

    void   setBrush(const TexPtr& t,const Color& c) override;
    void   setBrush(const Sprite& s,const Color& c) override;
    void   setTopology(Topology t) override;
    void   setBlend(const Blend b) override;

    struct Texture {
      TexPtr   brush;
      Sprite   sprite; //TODO: dangling sprites

      bool     operator==(const Texture& t) const {
        return brush==t.brush && sprite.pageId()==t.sprite.pageId();
        }
      };

    struct State {
      Topology       tp    =Triangles;
      Blend          blend =NoBlend;
      Texture        tex;

      bool operator == (const State& s) const {
        return tp==s.tp && blend==s.blend && tex==s.tex;
        }
      };

    struct Block : State {
      Block()=default;
      Block(Block&&)=default;
      Block(const Block&)=default;
      Block(const State& s):State(s){}

      Block& operator=(const Block&)=default;

      size_t         begin=0;
      size_t         size =0;
      PipePtr        pipeline;

      bool           hasImg=false;
      };

    struct PerFrame {
      Tempest::VertexBuffer<Point> vbo;
      std::vector<Uniforms>        blocks;
      bool                         outdated=true;
      };

    std::unique_ptr<PerFrame[]> frame;
    size_t                      frameCount=0;
    size_t                      outdatedCount=0;

    Topology                    topology=Triangles;

    std::vector<State>          stateStk;
    std::vector<Block>          blocks;
    std::vector<Point>          buf;

    struct Info {
      uint32_t w=0,h=0;
      };
    Info info;

    void makeActual(Device& dev,RenderPass& pass);

    template<class T,T State::*param>
    void setState(const T& t);
  };
}
