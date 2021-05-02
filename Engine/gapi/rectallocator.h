#pragma once

#include <Tempest/Point>

#include <vector>
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <memory>

namespace Tempest {

template<class MemoryProvider>
class RectAllocator {
  private:
    struct Page;
    struct Node;
    template<class T>
    struct Allocator;

  public:
    using Memory=typename MemoryProvider::DeviceMemory;

    explicit RectAllocator(MemoryProvider& device):device(device){}

    RectAllocator(const RectAllocator&)=delete;
    ~RectAllocator(){}

    struct Allocation {
      Allocation()=default;

      Allocation(Allocation&& a)
        :owner(a.owner),node(a.node),pId(a.pId){
        a.owner=nullptr;
        a.node =nullptr;
        a.pId  =0;
        }

      Allocation(const Allocation& a):owner(a.owner),node(a.node),pId(a.pId) {
        if(node!=nullptr)
          node->addref();
        }

      ~Allocation(){
        if(node!=nullptr)
          node->decref();
        }

      Allocation& operator=(const Allocation& a){
        if(a.node!=nullptr)
          a.node->addref();
        if(node!=nullptr)
          node->decref();
        owner=a.owner;
        node =a.node;
        pId  =a.pId;
        return *this;
        }

      Allocation& operator=(Allocation&& a){
        std::swap(owner,a.owner);
        std::swap(node ,a.node);
        std::swap(pId  ,a.pId);
        return *this;
        }

      Memory& memory(){
        return owner->pages[pId].memory;
        }

      const Memory& memory() const {
        return owner->pages[pId].memory;
        }

      Rect pageRect() const {
        auto& p=owner->pages[pId];
        return Rect(int(node->x),int(node->y),int(p.root->w),int(p.root->h));
        }

      Point pos() const {
        return Point(node->x,node->y);
        }

      void* pageId() const {
        return owner==nullptr ? nullptr : &owner->pages[pId];
        }

      RectAllocator* owner=nullptr;
      Node*          node =nullptr;
      size_t         pId  =0;
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
      Node(uint32_t x,uint32_t y,uint32_t w,uint32_t h,Node* owner):x(x),y(y),w(w),h(h),owner(owner){}

      std::atomic<uint32_t> refcount{};

      uint32_t x=0;
      uint32_t y=0;
      uint32_t w=0;
      uint32_t h=0;

      static Allocator<Node>& allocator(){
        static Allocator<Node> alloc;
        return alloc;
        }

      static void* operator new(size_t){
        static Allocator<Node>& alloc=allocator();
        void* ret = alloc.alloc();
        if(!ret)
          throw std::bad_alloc();
        return ret;
        }

      static void operator delete(void* ptr){
        static Allocator<Node>& alloc=allocator();
        return alloc.free(reinterpret_cast<Node*>(ptr));
        }

      Node*    owner=nullptr;
      Node*    sub[3]={};

      void     remove(Node* n){
        if(sub[0]==n)
          sub[0]=nullptr;
        else if(sub[1]==n)
          sub[1]=nullptr;
        else if(sub[2]==n)
          sub[2]=nullptr;
        }

      bool hasLeafs() const {
        return sub[0]!=nullptr || sub[1]!=nullptr || sub[2]!=nullptr;
        }

      void addref() {
        refcount.fetch_add(1,std::memory_order_acq_rel);
        }

      void decref() {
        Node* nx=this;
        if(nx->refcount.fetch_sub(1,std::memory_order_acq_rel)!=1)
          return;

        Node* ow=nx->owner;
        while(true) {
          ow->remove(nx);
          nx=ow;
          if(nx==nullptr || nx->hasLeafs())
            return;
          ow = nx->owner;
          }
        delete this;
        }

      Point pos() const { return Point(x,y); }
      };

    struct Page {
      Page(RectAllocator& owner,uint32_t w,uint32_t h)
        :owner(owner),root(new Node(0,0,w,h,nullptr)) {
        memory = owner.device.alloc(w,h);// std::bad_alloc, if error
        }

      Page(Page&&)=default;

      ~Page(){
        // memory!=null; 100%!!
        owner.device.free(memory);
        }

      RectAllocator&        owner;
      std::unique_ptr<Node> root;
      Memory                memory={};
      };

    Allocation alloc(Page& p,size_t page,uint32_t pw,uint32_t ph) {
      return alloc(*p.root,page,pw,ph);
      }

    Allocation alloc(Node& n,size_t page,uint32_t pw,uint32_t ph){
      if(n.refcount.load(std::memory_order_acquire)>0)
        return Allocation();

      if(n.sub[0] || n.sub[1] || n.sub[2]){
        if(n.w<pw || n.h<ph)
          return Allocation();
        Allocation a;
        if(n.sub[0])
          a=alloc(*n.sub[0],page,pw,ph);
        if(a.owner==nullptr && n.sub[1])
          a=alloc(*n.sub[1],page,pw,ph);
        if(a.owner==nullptr && n.sub[2])
          a=alloc(*n.sub[2],page,pw,ph);
        return a;
        }

      if(pw<n.w && ph<n.h){
        const uint32_t sq1 = (n.w-pw)*(n.h);
        const uint32_t sq2 = (n.w)*(n.h-ph);

        std::unique_ptr<Node> sub0(new Node(n.x,n.y,pw,ph,&n));
        std::unique_ptr<Node> sub1;
        std::unique_ptr<Node> sub2;

        if(sq1<sq2) {
          sub1.reset(new Node(n.x+pw,n.y,   n.w-pw,n.h,   &n));
          sub2.reset(new Node(n.x,   n.y+ph,pw,    n.h-ph,&n));
          } else {
          sub1.reset(new Node(n.x,   n.y+ph,n.w,   n.h-ph,&n));
          sub2.reset(new Node(n.x+pw,n.y,   n.w-pw,ph,    &n));
          }

        n.sub[0] = sub0.release();
        n.sub[1] = sub1.release();
        n.sub[2] = sub2.release();

        return emplace(n.sub[0],page);
        }

      if(pw==n.w && ph<n.h) {
        std::unique_ptr<Node> sub0(new Node(n.x, n.y,    pw, ph,     &n));
        std::unique_ptr<Node> sub1(new Node(n.x, n.y+ph, pw, n.h-ph, &n));

        n.sub[0] = sub0.release();
        n.sub[1] = sub1.release();

        return emplace(n.sub[0],page);
        }

      if(pw<n.w && ph==n.h){
        std::unique_ptr<Node> sub0(new Node(n.x,    n.y, pw,     ph, &n));
        std::unique_ptr<Node> sub1(new Node(n.x+pw, n.y, n.w-pw, ph, &n));

        n.sub[0] = sub0.release();
        n.sub[1] = sub1.release();

        return emplace(n.sub[0],page);
        }

      if(pw==n.w && ph==n.h)
        return emplace(&n,page);

      return Allocation();
      }

    Allocation emplace(Node* nx,size_t page) {
      Allocation a;
      a.owner = this;
      a.node  = nx;
      a.pId   = page;

      nx->addref();
      return a;
      }

    template<class T>
    struct Block {
      Block*                next=nullptr;
      std::atomic<uint32_t> mask{};
      T                     val[32]={};

      T* alloc() noexcept {
        uint32_t v = mask.load(std::memory_order_acquire);
        for(uint32_t i=0;i<32;){
          uint32_t id=(1<<i);
          if((v&id) == 0) {
            if(mask.compare_exchange_strong(v,(v|id),std::memory_order_acq_rel,std::memory_order_acquire)) {
              return &val[i];
              }
            } else {
            ++i;
            continue;
            }
          }
        return nullptr;
        }

      bool free(T* t) noexcept {
        if(t<val)
          return false;
        ptrdiff_t i=t-val;
        if(i>=32)
          return false;
        uint32_t id=(1<<i);
        mask.fetch_and(~id,std::memory_order_release);
        return true;
        }
      };

    template<class T>
    struct Allocator {
      Block<T> node;

      ~Allocator(){
        Block<T>* cur=node.next;
        while( cur!=nullptr ) {
          auto p = cur->next;
          delete cur;
          cur=p;
          }
        }

      T* alloc() noexcept {
        Block<T>* cur=&node;
        while( true ) {
          if(T* ptr=cur->alloc())
            return ptr;
          if(cur->next==nullptr) {
            auto p=new(std::nothrow) Block<T>();
            if(p==nullptr)
              return nullptr;
            cur->next=p;
            return cur->next->alloc();
            } else {
            cur=cur->next;
            }
          }
        }

      void free(T* ptr) noexcept {
        Block<T>* cur=&node;
        while( cur!=nullptr ) {
          if(cur->free(ptr))
            return;
          cur=cur->next;
          }
        }
      };
  };

}
