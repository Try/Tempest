#pragma once

#include <Tempest/SystemApi>
#include <Tempest/Color>
#include <Tempest/Matrix4x4>
#include <Tempest/Vec>

#include <initializer_list>
#include <memory>
#include <atomic>
#include <vector>

#include "../utility/dptr.h"
#include "flags.h"

namespace Tempest {
  class PipelineLayout;
  class Pixmap;
  class Color;
  class RenderState;
  class Device;

  namespace Decl {
  enum ComponentType:uint8_t {
    float0, // just trick
    float1,
    float2,
    float3,
    float4,

    int1,
    int2,
    int3,
    int4,

    uint1,
    uint2,
    uint3,
    uint4,

    count
    };

  inline size_t size(Decl::ComponentType t) {
    switch(t) {
      case Decl::float0:
      case Decl::count:
        return 0;
      case Decl::float1:
      case Decl::int1:
      case Decl::uint1:
        return 4;
      case Decl::float2:
      case Decl::int2:
      case Decl::uint2:
        return 8;
      case Decl::float3:
      case Decl::int3:
      case Decl::uint3:
        return 12;
      case Decl::float4:
      case Decl::int4:
      case Decl::uint4:
        return 16;
      }
    return 0;
    }
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

  enum class DeviceType : uint8_t {
    Unknown   = 0,
    Virtual   = 1,
    Cpu       = 2,
    Integrated= 3,
    Discrete  = 4,
    };


  enum  : uint8_t {
    MaxFramebufferAttachments = 8+1,
    MaxBarriers = 64,
    };

  enum Topology : uint8_t {
    Points   =0,
    Lines    =1,
    Triangles=2,
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
    R32F,
    RG32F,
    RGB32F,
    RGBA32F,
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
    static const Sampler2d& bilinear();
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

  class AccelerationStructure;

  struct RtInstance {
    Tempest::Matrix4x4           mat  = Matrix4x4::mkIdentity();
    uint32_t                     id   = 0;
    const AccelerationStructure* blas = nullptr;
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

    inline size_t sizeofIndex(Detail::IndexClass icls) {
      switch(icls) {
        case Detail::IndexClass::i16:
          return 2;
        case Detail::IndexClass::i32:
          return 4;
        }
      return 2;
      }

    class ResourceState;
    }

  class Attachment;
  class ZBuffer;

  enum class AccessOp : uint8_t {
    Discard,
    Preserve,
    Clear,
    };

  template<AccessOp l>
  struct AccessOpT{};

  static const auto Discard  = AccessOpT<AccessOp::Discard>();
  static const auto Preserve = AccessOpT<AccessOp::Preserve>();

  class AttachmentDesc final {
    public:
      AttachmentDesc() = default;

      AttachmentDesc(Attachment& a, AccessOpT<AccessOp::Discard>  l, AccessOpT<AccessOp::Discard>  s):attachment(&a),load(AccessOp::Discard),store(AccessOp::Discard) {(void)l; (void)s;}
      AttachmentDesc(Attachment& a, AccessOpT<AccessOp::Discard>  l, AccessOpT<AccessOp::Preserve> s):attachment(&a),load(AccessOp::Discard),store(AccessOp::Preserve){(void)l; (void)s;}

      AttachmentDesc(Attachment& a, AccessOpT<AccessOp::Preserve> l, AccessOpT<AccessOp::Discard>  s):attachment(&a),load(AccessOp::Preserve),store(AccessOp::Discard) {(void)l; (void)s;}
      AttachmentDesc(Attachment& a, AccessOpT<AccessOp::Preserve> l, AccessOpT<AccessOp::Preserve> s):attachment(&a),load(AccessOp::Preserve),store(AccessOp::Preserve){(void)l; (void)s;}

      AttachmentDesc(Attachment& a, Tempest::Vec4                 l, AccessOpT<AccessOp::Discard>  s):clear(l),attachment(&a),load(AccessOp::Clear),store(AccessOp::Discard) {(void)l; (void)s;}
      AttachmentDesc(Attachment& a, Tempest::Vec4                 l, AccessOpT<AccessOp::Preserve> s):clear(l),attachment(&a),load(AccessOp::Clear),store(AccessOp::Preserve){(void)l; (void)s;}

      AttachmentDesc(Attachment& a, Tempest::Color                l, AccessOpT<AccessOp::Discard>  s):clear(l.r(),l.g(),l.b(),l.a()),attachment(&a),load(AccessOp::Clear),store(AccessOp::Discard)  {(void)l; (void)s;}
      AttachmentDesc(Attachment& a, Tempest::Color                l, AccessOpT<AccessOp::Preserve> s):clear(l.r(),l.g(),l.b(),l.a()),attachment(&a),load(AccessOp::Clear),store(AccessOp::Preserve) {(void)l; (void)s;}

      AttachmentDesc(ZBuffer&    a, AccessOpT<AccessOp::Discard>  l, AccessOpT<AccessOp::Discard>  s):zbuffer(&a),load(AccessOp::Discard),store(AccessOp::Discard)  {(void)l; (void)s;}
      AttachmentDesc(ZBuffer&    a, AccessOpT<AccessOp::Discard>  l, AccessOpT<AccessOp::Preserve> s):zbuffer(&a),load(AccessOp::Discard),store(AccessOp::Preserve) {(void)l; (void)s;}

      AttachmentDesc(ZBuffer&    a, AccessOpT<AccessOp::Preserve> l, AccessOpT<AccessOp::Discard>  s):zbuffer(&a),load(AccessOp::Preserve),store(AccessOp::Discard)  {(void)l; (void)s;}
      AttachmentDesc(ZBuffer&    a, AccessOpT<AccessOp::Preserve> l, AccessOpT<AccessOp::Preserve> s):zbuffer(&a),load(AccessOp::Preserve),store(AccessOp::Preserve) {(void)l; (void)s;}

      AttachmentDesc(ZBuffer&    a, float                         l, AccessOpT<AccessOp::Discard>  s):clear(l,l,l,l),zbuffer(&a),load(AccessOp::Clear),store(AccessOp::Discard) {(void)l; (void)s;}
      AttachmentDesc(ZBuffer&    a, float                         l, AccessOpT<AccessOp::Preserve> s):clear(l,l,l,l),zbuffer(&a),load(AccessOp::Clear),store(AccessOp::Preserve){(void)l; (void)s;}

      Tempest::Vec4 clear      = {};
      Attachment*   attachment = nullptr;
      ZBuffer*      zbuffer    = nullptr;
      AccessOp      load       = AccessOp::Discard;
      AccessOp      store      = AccessOp::Discard;
    };

  class AbstractGraphicsApi {
    protected:
      AbstractGraphicsApi() =default;

    public:
      virtual ~AbstractGraphicsApi()=default;

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
            size_t offsetAlign = 256;
            size_t maxRange    = 0x10000000;
            } ssbo;

          struct {
            size_t offsetAlign = 1;
            size_t maxRange    = 16384;
            } ubo;
          
          struct {
            size_t maxRange    = 128;
            } push;

          struct {
            size_t maxColorAttachments = 1;
            } mrt;

          struct {
            BasicPoint<int,3> maxGroups    = {65535,65535,65535};
            BasicPoint<int,3> maxGroupSize = {128,128,64};
            } compute;

          struct {
            uint32_t maxSize = 4096;
            } tex2d;

          struct {
            bool nonUniformIndexing = false;
            } bindless;

          struct {
            bool rayQuery = false;
            } raytracing;

          struct {
            bool     taskShader         = false;
            bool     meshShader         = false;
            bool     meshShaderEmulated = false;
            uint32_t maxMeshGroups      = 32;
            uint32_t maxMeshGroupSize   = 32;
            } meshlets;

          bool     anisotropy        = false;
          float    maxAnisotropy     = 1.0f;
          bool     tesselationShader = false;
          bool     geometryShader    = false;

          bool     storeAndAtomicVs  = false;
          bool     storeAndAtomicFs  = false;

          bool     hasSamplerFormat(TextureFormat f) const;
          bool     hasAttachFormat (TextureFormat f) const;
          bool     hasDepthFormat  (TextureFormat f) const;
          bool     hasStorageFormat(TextureFormat f) const;

          void     setSamplerFormats(uint64_t t) { smpFormat  = t; }
          void     setAttachFormats (uint64_t t) { attFormat  = t; }
          void     setDepthFormats  (uint64_t t) { dattFormat = t; }
          void     setStorageFormats(uint64_t t) { storFormat = t; }

        private:
          uint64_t smpFormat =0;
          uint64_t attFormat =0;
          uint64_t dattFormat=0;
          uint64_t storFormat=0;
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
        mutable std::atomic_uint_fast32_t counter{0};
        };

      struct Device:NoCopy {
        virtual ~Device()=default;
        virtual void        waitIdle() = 0;
        };
      struct Fence:NoCopy {
        virtual ~Fence()=default;
        virtual void wait() = 0;
        virtual bool wait(uint64_t time) = 0;
        virtual void reset() = 0;
        };
      struct Swapchain:NoCopy {
        virtual ~Swapchain()=default;
        virtual void          reset()=0;
        virtual uint32_t      currentBackBufferIndex()=0;
        virtual uint32_t      imageCount() const=0;
        virtual uint32_t      w() const=0;
        virtual uint32_t      h() const=0;
        };
      struct Texture:Shared  {
        virtual uint32_t      mipCount() const = 0;
        };
      struct Pipeline:Shared {};
      struct CompPipeline:Shared {
        virtual IVec3 workGroupSize() const = 0;
        };
      struct Shader:Shared   {};
      struct Uniforms        {};
      struct PipelineLay:Shared {
        virtual ~PipelineLay()=default;
        virtual size_t descriptorsCount() = 0;
        };
      struct Buffer:Shared   {
        virtual ~Buffer()=default;
        virtual void  update  (const void* data,size_t off,size_t count,size_t sz,size_t alignedSz)=0;
        virtual void  read    (      void* data,size_t off,size_t size)=0;
        };
      struct AccelerationStructure:Shared {

        };
      struct Desc:NoCopy   {
        virtual ~Desc()=default;
        virtual void set    (size_t id, AbstractGraphicsApi::Texture* tex, const Sampler2d& smp, uint32_t mipLevel)=0;
        virtual void set    (size_t id, const Sampler2d& smp)=0;
        virtual void set    (size_t id, AbstractGraphicsApi::Buffer*  buf, size_t offset)=0;
        virtual void setTlas(size_t id, AbstractGraphicsApi::AccelerationStructure*) {}
        virtual void set    (size_t id, AbstractGraphicsApi::Texture** tex, size_t cnt, const Sampler2d& smp, uint32_t mipLevel);
        virtual void set    (size_t id, AbstractGraphicsApi::Buffer**  buf, size_t cnt);
        virtual void ssboBarriers(Detail::ResourceState& res, PipelineStage st);
        };
      struct EmptyDesc : Desc {
        void set(size_t, AbstractGraphicsApi::Texture*, const Sampler2d&, uint32_t){}
        void set(size_t, AbstractGraphicsApi::Buffer*,  size_t){}
        void set(size_t, const Sampler2d& smp){}
        void ssboBarriers(Detail::ResourceState&,PipelineStage){}
        };
      struct BarrierDesc {
        Buffer*        buffer    = nullptr;
        Texture*       texture   = nullptr;
        Swapchain*     swapchain = nullptr;
        uint32_t       swId      = 0;
        uint32_t       mip       = 0;

        ResourceAccess prev      = ResourceAccess::None;
        ResourceAccess next      = ResourceAccess::None;
        bool           discard   = false;
        };
      struct CommandBuffer:NoCopy {
        virtual ~CommandBuffer()=default;

        virtual void beginRendering(const AttachmentDesc* desc, size_t descSize,
                                    uint32_t w, uint32_t h,
                                    const TextureFormat* frm,
                                    AbstractGraphicsApi::Texture** att,
                                    AbstractGraphicsApi::Swapchain** sw, const uint32_t* imgId) = 0;
        virtual void endRendering() = 0;

        virtual void barrier(const BarrierDesc* desc, size_t cnt) = 0;
        virtual void barrier(Texture& tex, ResourceAccess prev, ResourceAccess next, uint32_t mipId);

        virtual void generateMipmap(Texture& image, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) = 0;
        virtual void copy(Buffer& dest, size_t offset, Texture& src, uint32_t width, uint32_t height, uint32_t mip) = 0;

        virtual bool isRecording() const = 0;
        virtual void begin()=0;
        virtual void end()  =0;
        virtual void reset()=0;

        virtual void setPipeline(Pipeline& p)=0;
        virtual void setComputePipeline(CompPipeline& p)=0;

        virtual void setBytes   (Pipeline &p, const void* data, size_t size)=0;
        virtual void setUniforms(Pipeline& p,Desc& u)=0;

        virtual void setBytes   (CompPipeline &p, const void* data, size_t size)=0;
        virtual void setUniforms(CompPipeline& p,Desc& u)=0;

        virtual void setViewport(const Rect& r)=0;
        virtual void setScissor (const Rect& r)=0;

        virtual void draw        (size_t vsize, size_t firstInstance, size_t instanceCount) = 0;
        virtual void draw        (const Buffer& vbo, size_t stride, size_t offset, size_t vertexCount,
                                  size_t firstInstance, size_t instanceCount) = 0;
        virtual void drawIndexed (const Buffer& vbo, size_t stride, size_t voffset,
                                  const Buffer& ibo, Detail::IndexClass cls, size_t ioffset, size_t isize,
                                  size_t firstInstance, size_t instanceCount) = 0;
        virtual void dispatch    (size_t x, size_t y, size_t z) = 0;
        virtual void dispatchMesh(size_t firstInstance, size_t instanceCount);
        };

      using PBuffer       = Detail::DSharedPtr<Buffer*>;
      using PTexture      = Detail::DSharedPtr<Texture*>;
      using PPipeline     = Detail::DSharedPtr<Pipeline*>;
      using PCompPipeline = Detail::DSharedPtr<CompPipeline*>;
      using PShader       = Detail::DSharedPtr<Shader*>;
      using PPipelineLay  = Detail::DSharedPtr<PipelineLay*>;

      virtual std::vector<Props> devices() const = 0;

    protected:
      virtual Device*    createDevice(const char* gpuName)=0;
      virtual void       destroy(Device* d)=0;

      virtual Swapchain* createSwapchain(SystemApi::Window* w,AbstractGraphicsApi::Device *d)=0;

      virtual PPipelineLay
                         createPipelineLayout(Device *d, const Shader* const* sh, size_t count) = 0;

      virtual PPipeline  createPipeline(Device* d, const RenderState &st, Topology tp,
                                        const PipelineLay &ulayImpl, const Shader* const* sh, size_t cnt)=0;

      virtual PCompPipeline createComputePipeline(Device* d,
                                                  const PipelineLay &ulayImpl,
                                                  Shader* shader)=0;

      virtual PShader    createShader(Device *d,const void* source,size_t src_size)=0;

      virtual Fence*     createFence(Device *d)=0;

      virtual CommandBuffer*
                         createCommandBuffer(Device* d)=0;

      virtual Desc*      createDescriptors(Device* d,PipelineLay& layP)=0;

      virtual PBuffer    createBuffer (Device* d, const void *mem, size_t count, size_t sz, size_t alignedSz, MemUsage usage, BufferHeap flg) = 0;
      virtual PTexture   createTexture(Device* d, const Pixmap& p, TextureFormat frm, uint32_t mips) = 0;
      virtual PTexture   createTexture(Device* d, const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm) = 0;
      virtual PTexture   createStorage(Device* d, const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm) = 0;

      virtual AccelerationStructure* createBottomAccelerationStruct(Device* d,
                                                                    Buffer* vbo, size_t vboSz, size_t stride,
                                                                    Buffer* ibo, size_t iboSz, size_t offset, Detail::IndexClass icls);
      virtual AccelerationStructure* createTopAccelerationStruct(Device* d, const RtInstance* geom, AccelerationStructure*const* as, size_t geomSize);

      virtual void       readPixels   (Device* d, Pixmap& out, const PTexture t,
                                       TextureFormat frm, const uint32_t w, const uint32_t h, uint32_t mip, bool storageImg) = 0;
      virtual void       readBytes    (Device* d, Buffer* buf, void* out, size_t size) = 0;

      virtual void       present  (Device *d, Swapchain* sw)=0;
      virtual void       submit   (Device *d, CommandBuffer*  cmd, Fence* fence)=0;

      virtual void       getCaps  (Device *d,Props& caps)=0;

    friend class Tempest::Device;
    };
}
