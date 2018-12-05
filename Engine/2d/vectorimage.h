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
    void addPoint(const Point& p) override;
    void commitPoints() override;

    void beginPaint(bool clear,uint32_t w,uint32_t h) override;
    void endPaint() override;

    void setBrush(const TexPtr& t,const Color& c) override;
    void setBrush(const Sprite& s,const Color& c) override;
    void setTopology(Topology t) override;

    struct Texture {
      TexPtr   brush;
      Sprite   sprite;

      bool     operator==(const Texture& t) const {
        return brush==t.brush && sprite==t.sprite;
        }
      };

    struct Block {
      size_t   begin=0;
      size_t   size =0;
      Topology tp   =Triangles;
      PipePtr  pipeline;
      bool     hasImg=false;
      Texture  tex;
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

    std::vector<Block>          blocks;
    std::vector<Point>          buf;

    struct Info {
      uint32_t w=0,h=0;
      };
    Info info;

    void makeActual(Device& dev,RenderPass& pass);

    template<class T,T Block::*param>
    void setState(const T& t);
  };
}
