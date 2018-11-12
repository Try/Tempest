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

    void draw(Device& dev, CommandBuffer& cmd, RenderPass &pass);

  private:
    void clear() override;
    void addPoint(const Point& p) override;
    void commitPoints() override;

    void beginPaint(bool clear,uint32_t w,uint32_t h) override;
    void endPaint() override;

    void setBrush(const Tempest::Texture2d* t,float r,float g,float b,float a) override;

    std::vector<Point> buf;
    struct Block {
      size_t begin=0;
      size_t size =0;

      };

    struct PerFrame {
      Tempest::VertexBuffer<Point> vbo;
      Tempest::Uniforms            ubo;
      bool                         outdated=true;
      };

    using TexPtr=Detail::ResourcePtr<Tempest::Texture2d>;
    TexPtr                      brush;

    std::unique_ptr<PerFrame[]> frame;
    size_t                      frameCount=0;
    size_t                      outdatedCount=0;

    struct Info {
      uint32_t w=0,h=0;
      };
    Info info;

    void makeActual(Device& dev);
  };
}
