#pragma once

#include <Tempest/Point>

#include <vector>
#include <cstdint>

namespace Tempest {

template<class MemoryProvider>
class RectAlllocator {
  private:
    struct Page;

  public:
    using Memory=typename MemoryProvider::DeviceMemory;
    static const constexpr Memory null=Memory{};

    explicit RectAlllocator(MemoryProvider& device):device(device){}

    RectAlllocator(const RectAlllocator&)=delete;

    ~RectAlllocator(){
      for(auto& i:pages)
        device.free(i.memory);
      }

    struct Allocation {
      Page*    page=nullptr;
      size_t   id  =0;
      uint32_t x   =0;
      uint32_t y   =0;
      };

    Allocation alloc(int iw,int ih) {
      for(auto& i:pages){
        auto a=alloc(i,iw,ih);
        if(a.page)
          return a;
        }
      const int w=std::max(iw,defPageSize);
      const int h=std::max(ih,defPageSize);

      pages.emplace_back(*this,w,h);
      auto a=alloc(pages.back(),iw,ih);
      if(a.page)
        return a;
      throw std::bad_alloc();
      }

    void free(Allocation& a){
      if(a.page)
        free(*a.page,a.id);
      }

  private:
    MemoryProvider&   device;
    std::vector<Page> pages;
    int               defPageSize=512;

    struct Node {
      Node()=default;
      Node(uint32_t x,uint32_t y,uint32_t w,uint32_t h,size_t parent):x(x),y(y),w(w),h(h),parent(parent){}

      uint32_t x=0;
      uint32_t y=0;
      uint32_t w=0;
      uint32_t h=0;

      size_t   parent=0;
      uint32_t usage =0;

      size_t   sub[3]={};

      Point pos() const { return Point(x,y); }
      };

    struct Page {
      Page(RectAlllocator& owner,uint32_t w,uint32_t h):owner(owner),node(1),w(w),h(h) {
        node[0] = Node{0,0,w,h,0};
        memory  = owner.device.alloc(w,h);
        }

      ~Page(){
        // memory!=null; 100%!!
        owner.device.free(memory);
        }

      size_t add(int x,int y,int w,int h,size_t parent){
        size_t sz=node.size();
        node.emplace_back(x,y,w,h,parent);
        return sz;
        }

      void   del(size_t id){
        if(id==0)
          return;
        // swap node[id] <-> node.back() and fix id's
        Node& b       = node.back();
        Node& bParent = node[b.parent];

        for(auto& ip:bParent.sub)
          if(ip+1==node.size()){ //last
            Node& parent=node[node[id].parent];
            for(auto& i:parent.sub)
              if(i==id) {
                ip=id;
                i =0;
                break;
                }
            break;
            }


        node[id]=node.back();
        node.pop_back();
        }

      RectAlllocator&   owner;
      Memory            memory={};
      std::vector<Node> node;
      const uint32_t    w=0;
      const uint32_t    h=0;
      };

    Allocation alloc(Page& p,uint32_t pw,uint32_t ph) {
      size_t sz0=p.node.size();

      try {
        return tryAlloc(p,pw,ph);
        }
      catch(...) {
        p.node.resize(sz0);
        throw;
        }
      }

    Allocation tryAlloc(Page& p,uint32_t pw,uint32_t ph) {
      size_t sz0=p.node.size();
      for(size_t i=0;i<sz0;++i){
        Node rd=p.node[i];
        if(rd.usage>0)
          continue;

        if(pw<rd.w && ph<rd.h){
          const int sq1 = (rd.w-pw)*(rd.h);
          const int sq2 = (rd.w)*(rd.h-ph);

          rd.sub[0] = p.add(rd.x,rd.y,pw,ph,i);
          if(sq1<sq2) {
            rd.sub[1] = p.add(rd.x+pw,rd.y,   rd.w-pw,rd.h,   i);
            rd.sub[2] = p.add(rd.x,   rd.y+ph,pw,     rd.h-ph,i);
            } else {
            rd.sub[1] = p.add(rd.x,   rd.y+ph,rd.w,   rd.h-ph,i);
            rd.sub[2] = p.add(rd.x+pw,rd.y,   rd.w-pw,ph,     i);
            }

          Allocation a = emplace(p,rd.sub[0],uint32_t(rd.x),uint32_t(rd.y));
          rd.usage  = 1;
          p.node[i] = rd;
          return a;
          }

        if(pw==rd.w && ph<rd.h) {
          rd.sub[0] = p.add(rd.x, rd.y,    pw, ph,      i);
          rd.sub[1] = p.add(rd.x, rd.y+ph, pw, rd.h-ph, i);

          Allocation a = emplace(p,rd.sub[0],uint32_t(rd.x),uint32_t(rd.y));
          rd.usage  = 1;
          p.node[i] = rd;
          return a;
          }

        if(pw<rd.w && ph==rd.h){
          rd.sub[0] = p.add(rd.x, rd.y,    pw, ph,      i);
          rd.sub[1] = p.add(rd.x+pw, rd.y, rd.w-pw, ph, i);

          Allocation a = emplace(p,rd.sub[0],uint32_t(rd.x),uint32_t(rd.y));
          rd.usage  = 1;
          p.node[i] = rd;
          return a;
          }

        if(pw==rd.w && ph==rd.h){
          Allocation a = emplace(p,i,uint32_t(rd.x),uint32_t(rd.y));
          rd.usage  = 1;
          p.node[i] = rd;
          return a;
          }
        }

      return Allocation();
      }

    Allocation emplace(Page &dest,size_t nId,uint32_t x, uint32_t y) {
      Node* nx  =&dest.node[nId];
      Node* zero=&dest.node[0];

      while(true) {
        nx->usage++;
        if(nx==zero)
          break;
        nx=&dest.node[nx->parent];
        }

      Allocation a;
      a.page = &dest;
      a.id   = nId;
      a.x    = x;
      a.y    = y;
      return a;
      }

    void free(Page& p,size_t id) {
      p.node[id].usage--;

      while(p.node[id].usage==0) {
        // recursive?
        p.del(p.node[id].sub[0]);
        p.del(p.node[id].sub[1]);
        p.del(p.node[id].sub[2]);

        auto par=p.node[id].parent;
        if(par==id)
          return;
        p.node[par].usage--;
        id = p.node[id].parent;
        }
      }
  };

}
