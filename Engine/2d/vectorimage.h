#pragma once

#include <Tempest/PaintDevice>
#include <Tempest/VertexBuffer>
#include <Tempest/Rect>
#include <Tempest/Uniforms>

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

  private:
    void clear() override;
    void addPoint(const Point& p) override;
    void commitPoints() override;

    void beginPaint(bool clear,uint32_t w,uint32_t h) override;
    void endPaint() override;

    void setBrush(const TexPtr& t,const Color& c) override;
    void setTopology(Topology t) override;

    struct Block {
      size_t   begin=0;
      size_t   size =0;
      Topology tp   =Triangles;
      PipePtr  pipeline;
      };

    struct PerFrame {
      Tempest::VertexBuffer<Point> vbo;
      Tempest::Uniforms            ubo;
      bool                         outdated=true;
      };

    TexPtr                      brush;

    std::unique_ptr<PerFrame[]> frame;
    size_t                      frameCount=0;
    size_t                      outdatedCount=0;

    Topology                    topology=Triangles;

    std::vector<Block> blocks;
    std::vector<Point> buf;

    struct Info {
      uint32_t w=0,h=0;
      };
    Info info;

    void makeActual(Device& dev);
  };
}
