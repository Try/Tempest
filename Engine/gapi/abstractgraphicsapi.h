#pragma once

#include <Tempest/SystemApi>

#include <initializer_list>
#include <memory>
#include <atomic>
#include <vector>

#include "../utility/dptr.h"
#include "flags.h"

namespace Tempest {
  class UniformsLayout;
  class Pixmap;
  class Color;
  class RenderState;
  class FboMode;
  class Device;

  namespace Decl {
  enum ComponentType:uint8_t {
    float0 = 0, // just trick
    float1 = 1,
    float2 = 2,
    float3 = 3,
    float4 = 4,

    color  = 5,
    short2 = 6,
    short4 = 7,

    half2  = 8,
    half4  = 9,
    count
    };
  }

  enum class ApiFlags : uint16_t{
    NoFlags   =0,
    Validation=1
    };

  inline ApiFlags operator | (ApiFlags a, ApiFlags b){
    return ApiFlags(uint16_t(a)|uint16_t(b));
    }

  inline ApiFlags operator & (ApiFlags a, ApiFlags b){
    return ApiFlags(uint16_t(a)&uint16_t(b));
    }

  enum Topology : uint8_t {
    Lines    =1,
    Triangles=2
    };

  enum TextureFormat : uint8_t {
    Undefined,
    R8,
    RG8,
    RGB8,
    RGBA8,
    R16,
    RG16,
    RGB16,
    RGBA16,
    Depth16,
    Depth24x8,
    Depth24S8,
    DXT1,
    DXT3,
    DXT5,
    Last
    };

  inline bool isDepthFormat(TextureFormat f){
    return f==TextureFormat::Depth16 || f==TextureFormat::Depth24x8 || f==TextureFormat::Depth24S8;
    }

  inline bool isCompressedFormat(TextureFormat f){
    return f==TextureFormat::DXT1 || f==TextureFormat::DXT3 || f==TextureFormat::DXT5;
    }

  enum class ComponentSwizzle {
    Identity = 0,
    R,
    G,
    B,
    A
    };

  struct ComponentMapping final {
    ComponentSwizzle r = ComponentSwizzle::Identity;
    ComponentSwizzle g = ComponentSwizzle::Identity;
    ComponentSwizzle b = ComponentSwizzle::Identity;
    ComponentSwizzle a = ComponentSwizzle::Identity;

    bool operator==(const ComponentMapping& other) const {
      return r==other.r && g==other.g && b==other.b && a==other.a;
      }
    bool operator!=(const ComponentMapping& other) const {
      return !(*this==other);
      }
    };

  //! Способы фильтрации текстуры.
  enum class Filter : uint8_t {
    //! ближайшая фильтрация
    Nearest,
    //! линейная фильтрация
    Linear,
    //! Count
    Count
    };

  enum class ClampMode : uint8_t {
    ClampToBorder,
    ClampToEdge,
    MirroredRepeat,
    Repeat,
    Count
    };

  struct Sampler2d final {
    Filter           minFilter=Filter::Linear;
    Filter           magFilter=Filter::Linear;
    Filter           mipFilter=Filter::Linear;

    ClampMode        uClamp   =ClampMode::Repeat;
    ClampMode        vClamp   =ClampMode::Repeat;

    bool             anisotropic=true;
    ComponentMapping mapping;

    static const Sampler2d& anisotrophy();
    static const Sampler2d& trillinear();
    static const Sampler2d& nearest();

    void setClamping(ClampMode c){
      uClamp = c;
      vClamp = c;
      }

    void setFiltration(Filter f){
      minFilter = f;
      magFilter = f;
      mipFilter = f;
      }

    bool operator==(const Sampler2d& s) const {
      return minFilter==s.minFilter &&
             magFilter==s.magFilter &&
             mipFilter==s.mipFilter &&
             uClamp   ==s.uClamp    &&
             vClamp   ==s.vClamp    &&
             anisotropic==s.anisotropic;
      }

    bool operator!=(const Sampler2d& s) const {
      return !(*this==s);
      }
    };

  namespace Detail {
    enum class IndexClass:uint8_t {
      i16=0,
      i32
      };
    template<class T>
    inline IndexClass indexCls() { return IndexClass::i16; }

    template<>
    inline IndexClass indexCls<uint16_t>() { return IndexClass::i16; }

    template<>
    inline IndexClass indexCls<uint32_t>() { return IndexClass::i32; }
    }

  class AbstractGraphicsApi {
    protected:
      AbstractGraphicsApi() =default;

    public:
      virtual ~AbstractGraphicsApi()=default;

      enum DeviceType : uint8_t {
        Unknown   = 0,
        Virtual   = 1,
        Cpu       = 2,
        Integrated= 3,
        Discrete  = 4,
        };

      class Props {
        public:
          char       name[256]={};
          DeviceType type=DeviceType::Unknown;

          struct {
            size_t maxAttribs  = 16;
            size_t maxRange    = 2047;
            } vbo;

          struct {
            size_t maxValue    = 0xFFFFFF;
            } ibo;

          struct {
            size_t offsetAlign = 1;
            size_t maxRange    = 16384;
            } ubo;
          
          struct {
            size_t maxRange    = 128;
            } push;

          bool     anisotropy=false;
          float    maxAnisotropy=1.0f;

          bool     hasSamplerFormat(TextureFormat f) const;
          bool     hasAttachFormat (TextureFormat f) const;
          bool     hasDepthFormat  (TextureFormat f) const;

          void     setSamplerFormats(uint64_t t) { smpFormat  = t; }
          void     setAttachFormats (uint64_t t) { attFormat  = t; }
          void     setDepthFormats  (uint64_t t) { dattFormat = t; }

        private:
          uint64_t smpFormat =0;
          uint64_t attFormat =0;
          uint64_t dattFormat=0;
        };

      struct NoCopy {
        NoCopy()=default;
        virtual ~NoCopy() = default;
        NoCopy(const NoCopy&) = delete;
        NoCopy(NoCopy&&) = delete;
        NoCopy& operator = (const NoCopy&) = delete;
        NoCopy& operator = (NoCopy&&) = delete;
        };

      struct Shared:NoCopy {
        std::atomic_uint_fast32_t counter{0};
        };

      struct Device:NoCopy {
        virtual ~Device()=default;
        virtual const char* renderer() const=0;
        virtual void        waitIdle() = 0;
        };
      struct Fence:NoCopy {
        virtual ~Fence()=default;
        virtual void wait() =0;
        virtual void reset()=0;
        };
      struct Semaphore:NoCopy {
        virtual ~Semaphore()=default;
        };
      struct Swapchain:NoCopy {
        virtual ~Swapchain()=default;
        virtual void          reset()=0;
        virtual uint32_t      nextImage(Semaphore* onReady)=0;
        virtual uint32_t      imageCount() const=0;
        virtual uint32_t      w() const=0;
        virtual uint32_t      h() const=0;
        };
      struct Texture:Shared  {
        };
      struct Fbo:Shared      {
        virtual ~Fbo(){}
        };
      struct FboLayout:Shared      {
        virtual ~FboLayout()=default;
        virtual bool equals(const FboLayout&) const = 0;
        };
      struct Pass:Shared     {
        virtual ~Pass()=default;
        };
      struct Pipeline:Shared {};
      struct Shader:Shared   {};
      struct Uniforms        {};
      struct UniformsLay:Shared {
        virtual ~UniformsLay()=default;
        };
      struct Buffer:Shared   {
        virtual ~Buffer()=default;
        virtual void  update(const void* data,size_t off,size_t count,size_t sz,size_t alignedSz)=0;
        };
      struct Desc:NoCopy   {
        virtual ~Desc()=default;
        virtual void set(size_t id,AbstractGraphicsApi::Texture *tex, const Sampler2d& smp)=0;
        virtual void set(size_t id,AbstractGraphicsApi::Buffer* buf,size_t offset,size_t size,size_t align)=0;
        };
      struct CommandBuffer:NoCopy {
        virtual ~CommandBuffer()=default;
        virtual void beginRenderPass(AbstractGraphicsApi::Fbo* f,
                                     AbstractGraphicsApi::Pass*  p,
                                     uint32_t width,uint32_t height)=0;
        virtual void endRenderPass()=0;

        virtual void changeLayout(Swapchain& s, uint32_t id, TextureFormat frm, TextureLayout prev, TextureLayout next)=0;
        virtual void changeLayout(Texture& t,TextureFormat frm,TextureLayout prev,TextureLayout next)=0;
        virtual bool isRecording() const = 0;
        virtual void begin()=0;
        virtual void end()  =0;
        virtual void setPipeline(Pipeline& p,uint32_t w,uint32_t h)=0;
        virtual void setBytes   (Pipeline &p, const void* data, size_t size)=0;
        virtual void setViewport(const Rect& r)=0;
        virtual void setUniforms(Pipeline& p,Desc& u)=0;

        virtual void setVbo      (const Buffer& b)=0;
        virtual void setIbo      (const Buffer& b,Detail::IndexClass cls)=0;
        virtual void draw        (size_t offset,size_t vertexCount)=0;
        virtual void drawIndexed (size_t ioffset, size_t isize, size_t voffset)=0;
        };

      using PBuffer      = Detail::DSharedPtr<Buffer*>;
      using PTexture     = Detail::DSharedPtr<Texture*>;
      using PPipeline    = Detail::DSharedPtr<Pipeline*>;
      using PPass        = Detail::DSharedPtr<Pass*>;
      using PShader      = Detail::DSharedPtr<Shader*>;
      using PFbo         = Detail::DSharedPtr<Fbo*>;
      using PFboLayout   = Detail::DSharedPtr<FboLayout*>;
      using PUniformsLay = Detail::DSharedPtr<UniformsLay*>;

      virtual std::vector<Props> devices() const = 0;

    protected:
      virtual Device*    createDevice(const char* gpuName)=0;
      virtual void       destroy(Device* d)=0;

      virtual Swapchain* createSwapchain(SystemApi::Window* w,AbstractGraphicsApi::Device *d)=0;

      virtual PPass      createPass(Device *d, const FboMode** att, size_t acount)=0;

      virtual PFbo       createFbo(Device *d,FboLayout* lay,Swapchain *s,uint32_t imageId)=0;
      virtual PFbo       createFbo(Device *d,FboLayout* lay,Swapchain *s,uint32_t imageId,Texture* zbuf)=0;
      virtual PFbo       createFbo(Device *d,FboLayout* lay,uint32_t w, uint32_t h,Texture* cl,Texture* zbuf)=0;
      virtual PFbo       createFbo(Device *d,FboLayout* lay,uint32_t w, uint32_t h,Texture* cl)=0;

      virtual PFboLayout createFboLayout(Device *d, Swapchain *s, TextureFormat *att, size_t attCount)=0;

      virtual PUniformsLay
                         createUboLayout(Device *d,const std::initializer_list<Shader*>& sh)=0;

      virtual PPipeline  createPipeline(Device* d, const RenderState &st,
                                        const Tempest::Decl::ComponentType *decl, size_t declSize,
                                        size_t stride, Topology tp,
                                        const UniformsLay &ulayImpl,
                                        const std::initializer_list<Shader*>& sh)=0;

      virtual PShader    createShader(Device *d,const void* source,size_t src_size)=0;

      virtual Fence*     createFence(Device *d)=0;

      virtual Semaphore* createSemaphore(Device *d)=0;

      virtual CommandBuffer*
                         createCommandBuffer(Device* d)=0;

      virtual Desc*      createDescriptors(Device* d,UniformsLay& layP)=0;

      virtual PBuffer    createBuffer (Device* d,const void *mem,size_t count,size_t sz,size_t alignedSz,MemUsage usage,BufferHeap flg)=0;
      virtual PTexture   createTexture(Device* d,const Pixmap& p,TextureFormat frm,uint32_t mips)=0;
      virtual PTexture   createTexture(Device* d,const uint32_t w,const uint32_t h,uint32_t mips, TextureFormat frm)=0;
      virtual void       readPixels   (AbstractGraphicsApi::Device *d, Pixmap &out,const PTexture t,
                                       TextureLayout lay, TextureFormat frm,
                                       const uint32_t w, const uint32_t h, uint32_t mip) = 0;

      virtual void       present  (Device *d,Swapchain* sw,uint32_t imageId,const Semaphore *wait)=0;

      virtual void       submit   (Device *d,CommandBuffer*  cmd,Semaphore* wait,Semaphore* onReady,Fence* onReadyCpu)=0;
      virtual void       submit   (Device *d,
                                   CommandBuffer** cmd,size_t count,
                                   Semaphore** wait, size_t waitCnt,
                                   Semaphore** done, size_t doneCnt,
                                   Fence* doneCpu)=0;

      virtual void       getCaps  (Device *d,Props& caps)=0;

    friend class Tempest::Device;
    };
}
