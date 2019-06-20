#pragma once

#include <Tempest/SystemApi>

#include <initializer_list>
#include <memory>
#include <atomic>

#include "../utility/dptr.h"
#include "flags.h"

namespace Tempest {
  class UniformsLayout;
  class Pixmap;
  class Color;
  class RenderState;

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

  enum class CmdType : uint8_t {
    Primary,
    Secondary
    };

  enum TextureFormat : uint8_t {
    Undefined,
    Alpha,
    RGB8,
    RGBA8,
    RG16,
    Depth16,
    Depth24x8,
    DXT1,
    DXT3,
    DXT5,
    Last
    };

  inline bool isDepthFormat(TextureFormat f){
    return f==TextureFormat::Depth16 || f==TextureFormat::Depth24x8;
    }

  inline bool isCompressedFormat(TextureFormat f){
    return f==TextureFormat::DXT1 || f==TextureFormat::DXT3 || f==TextureFormat::DXT5;
    }

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
    Filter    minFilter=Filter::Linear;
    Filter    magFilter=Filter::Linear;
    Filter    mipFilter=Filter::Linear;

    ClampMode uClamp   =ClampMode::Repeat;
    ClampMode vClamp   =ClampMode::Repeat;

    bool      anisotropic=true;

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
      ~AbstractGraphicsApi()=default;

    public:
      class Caps {
        public:
          bool     rgb8 =false;
          bool     rgba8=false;
          size_t   minUboAligment=1;

          bool     anisotropy=false;
          float    maxAnisotropy=1.0f;

          bool     hasSamplerFormat(TextureFormat f) const;
          bool     hasAttachFormat (TextureFormat f) const;

          void     setSamplerFormats(uint64_t t) { smpFormat = t; }
          void     setAttachFormats (uint64_t t) { attFormat = t; }

        private:
          uint64_t smpFormat=0;
          uint64_t attFormat=0;
        };

      struct Shared {
        virtual ~Shared()=default;
        std::atomic_uint_fast32_t counter{0};
        };

      struct Device       {};
      struct Swapchain    {
        virtual ~Swapchain(){}
        virtual uint32_t      imageCount() const=0;
        virtual uint32_t      w() const=0;
        virtual uint32_t      h() const=0;
        };
      struct Texture:Shared  {
        virtual void setSampler(const Sampler2d& s)=0;
        };
      struct Image           {};
      struct Fbo             {};
      struct Pass:Shared     {};
      struct Pipeline:Shared {};
      struct Shader:Shared   {};
      struct Uniforms        {};
      struct UniformsLay     {
        virtual ~UniformsLay()=default;
        };
      struct Buffer:Shared   {
        virtual ~Buffer()=default;
        virtual void  update(const void* data,size_t off,size_t sz)=0;
        };
      struct Desc            {
        virtual ~Desc()=default;
        virtual void set(size_t id,AbstractGraphicsApi::Texture *tex)=0;
        virtual void set(size_t id,AbstractGraphicsApi::Buffer* buf,size_t offset,size_t size)=0;
        };
      struct CommandBuffer   {
        virtual ~CommandBuffer()=default;
        virtual void begin()=0;
        virtual void end()  =0;
        virtual void beginRenderPass(AbstractGraphicsApi::Fbo* f,
                                     AbstractGraphicsApi::Pass*  p,
                                     uint32_t width,uint32_t height)=0;
        virtual void beginSecondaryPass(AbstractGraphicsApi::Fbo* f,
                                        AbstractGraphicsApi::Pass*  p,
                                        uint32_t width,uint32_t height)=0;
        virtual void endRenderPass()=0;

        virtual void clear       (Image& img,float r, float g, float b, float a)=0;
        virtual void setPipeline (Pipeline& p,uint32_t w,uint32_t h)=0;
        virtual void setViewport (const Rect& r)=0;
        virtual void setUniforms (Pipeline& p,Desc& u, size_t offc, const uint32_t* offv)=0;
        virtual void exec        (const AbstractGraphicsApi::CommandBuffer& buf)=0;
        virtual void changeLayout(Texture& t,TextureLayout prev,TextureLayout next)=0;
        virtual void setVbo      (const Buffer& b)=0;
        virtual void setIbo      (const Buffer* b,Detail::IndexClass cls)=0;
        virtual void draw        (size_t offset,size_t vertexCount)=0;
        virtual void drawIndexed (size_t ioffset, size_t isize, size_t voffset)=0;
        };
      struct CmdPool         {};

      struct Fence           {
        virtual ~Fence()=default;
        virtual void wait() =0;
        virtual void reset()=0;
        };
      struct Semaphore       {};

      using PBuffer   = Detail::DSharedPtr<Buffer*>;
      using PTexture  = Detail::DSharedPtr<Texture*>;
      using PPipeline = Detail::DSharedPtr<Pipeline*>;
      using PPass     = Detail::DSharedPtr<Pass*>;
      using PShader   = Detail::DSharedPtr<Shader*>;

      virtual Device*    createDevice(SystemApi::Window* w)=0;
      virtual void       destroy(Device* d)=0;
      virtual void       waitIdle(Device* d)=0;

      virtual Swapchain* createSwapchain(SystemApi::Window* w,AbstractGraphicsApi::Device *d)=0;
      virtual void       destroy(Swapchain* d)=0;

      virtual PPass      createPass(Device *d,
                                    Swapchain* sw,
                                    TextureFormat zformat,
                                    FboMode in,const Color* clear,
                                    FboMode out,const float* zclear)=0;
      virtual PPass      createPass(Device *d,
                                    TextureFormat clFormat,
                                    TextureFormat zformat,
                                    FboMode in,const Color* clear,
                                    FboMode out,const float* zclear)=0;

      virtual Fbo*       createFbo(Device *d,Swapchain *s,Pass* pass,uint32_t imageId)=0;
      virtual Fbo*       createFbo(Device *d,Swapchain *s,Pass* pass,uint32_t imageId,Texture* zbuf)=0;
      virtual Fbo*       createFbo(Device *d,uint32_t w, uint32_t h,Pass* pass,Texture* cl,Texture* zbuf)=0;
      virtual void       destroy(Fbo* pass)=0;

      virtual std::shared_ptr<AbstractGraphicsApi::UniformsLay>
                         createUboLayout(Device *d,const UniformsLayout&)=0;

      virtual PPipeline  createPipeline(Device* d,
                                        const RenderState &st,
                                        const Tempest::Decl::ComponentType *decl, size_t declSize,
                                        size_t stride,
                                        Topology tp,
                                        const UniformsLayout& ulay,
                                        std::shared_ptr<UniformsLay> &ulayImpl,
                                        const std::initializer_list<Shader*>& sh)=0;

      virtual PShader    createShader(Device *d,const char* source,size_t src_size)=0;

      virtual Fence*     createFence(Device *d)=0;
      virtual void       destroy(Fence* fence)=0;

      virtual Semaphore* createSemaphore(Device *d)=0;
      virtual void       destroy(Semaphore* semaphore)=0;

      virtual CmdPool*   createCommandPool(Device* d)=0;
      virtual void       destroy(CmdPool* cmd)=0;

      virtual CommandBuffer*
                         createCommandBuffer(Device* d,CmdPool* pool,Pass* pass,CmdType type)=0;
      virtual void       destroy(CommandBuffer* cmd)=0;

      virtual PBuffer    createBuffer(Device* d,const void *mem,size_t size,MemUsage usage,BufferFlags flg)=0;

      virtual Desc*      createDescriptors(Device* d,const Tempest::UniformsLayout& p,AbstractGraphicsApi::UniformsLay* layP)=0;
      virtual void       destroy(Desc* cmd)=0;

      virtual PTexture   createTexture(Device* d,const Pixmap& p,TextureFormat frm,uint32_t mips)=0;
      virtual PTexture   createTexture(Device* d,const uint32_t w,const uint32_t h,uint32_t mips, TextureFormat frm)=0;
      //virtual void       destroy(Texture* t)=0;

      virtual uint32_t   nextImage(Device *d,Swapchain* sw,Semaphore* onReady)=0;
      virtual Image*     getImage (Device *d,Swapchain* sw,uint32_t id)=0;
      virtual void       present  (Device *d,Swapchain* sw,uint32_t imageId,const Semaphore *wait)=0;
      virtual void       draw     (Device *d,Swapchain *sw,CommandBuffer*  cmd,Semaphore* wait,Semaphore* onReady,Fence* onReadyCpu)=0;
      virtual void       draw     (Device *d,Swapchain *sw,CommandBuffer** cmd,size_t count,Semaphore* wait,Semaphore* onReady,Fence* onReadyCpu)=0;

      virtual void       getCaps  (Device *d,Caps& caps)=0;
    };
}
