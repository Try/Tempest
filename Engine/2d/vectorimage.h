#pragma once

#include <Tempest/PaintDevice>
#include <Tempest/VertexBuffer>
#include <Tempest/DescriptorSet>
#include <Tempest/Rect>
#include <Tempest/Sprite>

#include <vector>

namespace Tempest {

template<class T>
class Encoder;

class VectorImage : public Tempest::PaintDevice {
  public:
    VectorImage()=default;

    class Mesh {
      public:
        void update(Device& dev, const VectorImage& src, BufferHeap heap = BufferHeap::Upload);
        void draw  (Encoder<CommandBuffer>& cmd);

      private:
        struct Block {
          size_t                begin = 0;
          size_t                size  = 0;

          DescriptorSet         desc;
          const RenderPipeline* pipeline = nullptr;
          // strong reference to sprite
          Sprite                sprite;
          };
        Tempest::VertexBuffer<Point> vbo;
        std::vector<Block>           blocks;
      };

    uint32_t w() const { return info.w; }
    uint32_t h() const { return info.h; }

    bool     load(const char* path);
    void     clear() override;

  private:
    void   addPoint(const Point& p) override;
    void   commitPoints() override;

    void   beginPaint(bool clear,uint32_t w,uint32_t h) override;
    void   endPaint() override;

    size_t pushState() override;
    void   popState(size_t id) override;

    void   setState(const TexPtr& t, const Color& c, TextureFormat frm, ClampMode clamp) override;
    void   setState(const Sprite& s, const Color& c) override;
    void   setTopology(Topology t) override;
    void   setBlend(const Blend b) override;

    struct SpriteLock {
      std::vector<Sprite> spr;
      void insert(const Sprite& s) {
        for(size_t i=spr.size();i>0;){
          --i;
          Sprite& sb=spr[i];
          if(sb.pageId()  ==s.pageId() &&
             sb.pageRect()==s.pageRect())
            return;
          }
        spr.push_back(s);
        }

      void clear() { spr.clear(); }
      };

    struct Texture {
      TexPtr        brush;
      TextureFormat frm;
      ClampMode     clamp;
      Sprite        sprite; //TODO: dangling sprites

      bool     operator==(const Texture& t) const {
        return brush==t.brush && sprite.pageId()==t.sprite.pageId();
        }
      };

    struct State {
      Topology       tp    = Triangles;
      Blend          blend = NoBlend;
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

      size_t         begin  = 0;
      size_t         size   = 0;
      bool           hasImg = false;
      };

    Topology                    topology=Triangles;

    std::vector<State>          stateStk;
    std::vector<Block>          blocks;
    std::vector<Point>          buf;
    SpriteLock                  slock;

    struct Info {
      uint32_t w=0,h=0;
      };
    Info   info;
    size_t paintScope = 0;

    const RenderPipeline& pipelineOf(Device& dev, const Block& b) const;

    template<class T,T State::*param>
    void setState(const T& t);
  };
}
