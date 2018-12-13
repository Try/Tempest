#pragma once

#include <Tempest/Point>

#include <vector>
#include <cstdint>

namespace Tempest {

template<class MemoryProvider>
class RectAllocator {
  private:
    struct Page;
    struct Node;

  public:
    using Memory=typename MemoryProvider::DeviceMemory;

    explicit RectAllocator(MemoryProvider& device):device(device){}

    RectAllocator(const RectAllocator&)=delete;

    ~RectAllocator(){
      }

    struct Allocation {
      Allocation()=default;
      Allocation(Allocation&& t)
        :owner(t.owner),id(t.id),x(t.x),y(t.y){
        t.owner=nullptr;
        }

      Allocation(const Allocation& t)
        :owner(t.owner),id(t.id),x(t.x),y(t.y){
        if(owner!=nullptr)
          owner->addRef(id,x,y);
        }

      ~Allocation(){
        if(owner!=nullptr)
          owner->free(id,x,y);
        }

      Allocation& operator=(const Allocation& t) {
        if(owner==t.owner && id==t.id) {
          id  =t.id;
          x   =t.x;
          y   =t.y;
          return *this;
          }

        if(owner==nullptr){
          owner=t.owner;
          } else {
          owner->free(id,x,y);
          owner=t.owner;
          }

        id  =t.id;
        x   =t.x;
        y   =t.y;

        if(owner)
          owner->addRef(id,x,y);
        return *this;
        }

      Allocation& operator=(Allocation&& t) {
        std::swap(owner,t.owner);
        id = t.id;
        x  = t.x;
        y  = t.y;
        return *this;
        }

      Memory& memory(){
        return owner->pages[id].memory;
        }

      const Memory& memory() const {
        return owner->pages[id].memory;
        }

      Rect pageRect() const {
        auto& p=owner->pages[id];
        return Rect(int(x),int(y),int(p.w),int(p.h));
        }

      void* pageId() const {
        return &owner[id];
        }

      RectAllocator* owner=nullptr;
      size_t         id   =0;
      uint32_t       x    =0;
      uint32_t       y    =0;
      };

    Allocation alloc(uint32_t iw,uint32_t ih) {
      if(iw==0 || ih==0)
        return Allocation();

      for(size_t i=0;i<pages.size();++i){
        auto a=alloc(pages[i],i,iw,ih);
        if(a.owner)
          return a;
        }
      const uint32_t w=std::max(iw,defPageSize);
      const uint32_t h=std::max(ih,defPageSize);

      pages.emplace_back(*this,w,h);
      auto a=alloc(pages.back(),pages.size()-1,iw,ih);
      if(a.owner)
        return a;
      pages.pop_back();
      throw std::bad_alloc();
      }

  private:
    MemoryProvider&   device;
    std::vector<Page> pages;
    uint32_t          defPageSize=512; //TODO: get-set

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
      Page(RectAllocator& owner,uint32_t w,uint32_t h):owner(owner),node(1),w(w),h(h) {
        node[0] = Node{0,0,w,h,0};
        memory  = owner.device.alloc(w,h);
        }

      Page(Page&&)=default;

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

      size_t find(uint32_t x,uint32_t y) const {
        size_t id=0;
        while(true) {
          const Node* n=&node[id];
          if(n->sub[0]==0) // n is leaf
            return id;

          if(n->x==x && n->y==y) { // sub[0] is always leaf or zero
            id=n->sub[0];
            continue;
            }

          if(n->sub[1]!=0) {
            const Node& nx=node[n->sub[1]];
            if(nx.x<=x && nx.y<=y &&
               x<nx.x+nx.w && y<nx.y+nx.h){
              id=n->sub[1];
              continue;
              }
            }

          id=n->sub[2];
          }
        }

      RectAllocator&   owner;
      Memory            memory={};
      std::vector<Node> node;
      const uint32_t    w=0;
      const uint32_t    h=0;
      };

    Allocation alloc(Page& p,size_t pageId,uint32_t pw,uint32_t ph) {
      size_t sz0=p.node.size();

      try {
        return tryAlloc(p,pageId,pw,ph);
        }
      catch(...) {
        p.node.resize(sz0);
        throw;
        }
      }

    Allocation tryAlloc(Page& p,size_t pageId,uint32_t pw,uint32_t ph) {
      size_t sz0=p.node.size();
      for(size_t i=0;i<sz0;++i){
        Node rd=p.node[i];
        if(rd.usage>0)
          continue;

        if(pw<rd.w && ph<rd.h){
          const uint32_t sq1 = (rd.w-pw)*(rd.h);
          const uint32_t sq2 = (rd.w)*(rd.h-ph);

          rd.sub[0] = p.add(rd.x,rd.y,pw,ph,i);
          if(sq1<sq2) {
            rd.sub[1] = p.add(rd.x+pw,rd.y,   rd.w-pw,rd.h,   i);
            rd.sub[2] = p.add(rd.x,   rd.y+ph,pw,     rd.h-ph,i);
            } else {
            rd.sub[1] = p.add(rd.x,   rd.y+ph,rd.w,   rd.h-ph,i);
            rd.sub[2] = p.add(rd.x+pw,rd.y,   rd.w-pw,ph,     i);
            }

          Allocation a = emplace(p,pageId,rd.sub[0],uint32_t(rd.x),uint32_t(rd.y));
          rd.usage  = 1;
          p.node[i] = rd;
          return a;
          }

        if(pw==rd.w && ph<rd.h) {
          rd.sub[0] = p.add(rd.x, rd.y,    pw, ph,      i);
          rd.sub[1] = p.add(rd.x, rd.y+ph, pw, rd.h-ph, i);

          Allocation a = emplace(p,pageId,rd.sub[0],uint32_t(rd.x),uint32_t(rd.y));
          rd.usage  = 1;
          p.node[i] = rd;
          return a;
          }

        if(pw<rd.w && ph==rd.h){
          rd.sub[0] = p.add(rd.x, rd.y,    pw, ph,      i);
          rd.sub[1] = p.add(rd.x+pw, rd.y, rd.w-pw, ph, i);

          Allocation a = emplace(p,pageId,rd.sub[0],uint32_t(rd.x),uint32_t(rd.y));
          rd.usage  = 1;
          p.node[i] = rd;
          return a;
          }

        if(pw==rd.w && ph==rd.h){
          Allocation a = emplace(p,pageId,i,uint32_t(rd.x),uint32_t(rd.y));
          rd.usage  = 1;
          p.node[i] = rd;
          return a;
          }
        }

      return Allocation();
      }

    Allocation emplace(Page &dest,size_t pageId,size_t nId,uint32_t x, uint32_t y) {
      Node* nx  =&dest.node[nId];
      Node* zero=&dest.node[0];

      while(true) {
        nx->usage++;
        if(nx==zero)
          break;
        nx=&dest.node[nx->parent];
        }

      Allocation a;
      a.owner = this;
      a.id    = pageId;
      a.x     = x;
      a.y     = y;
      return a;
      }

    void addRef(size_t pageId,uint32_t x,uint32_t y) {
      Page& p = pages[pageId];
      p.node[p.find(x,y)].usage++;
      }

    void free(size_t pageId,uint32_t x,uint32_t y) {
      Page& p = pages[pageId];
      free(p,p.find(x,y));
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
