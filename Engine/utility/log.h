#pragma once

#include <mutex>
#include <cstdint>
#include <thread>
#include <type_traits>
#include <functional>
#include <string>
#include <string_view>

namespace Tempest{

class Log final {
  public:
    enum Mode : uint8_t {
      Info,
      Error,
      Debug
      };

    template< class ... Args >
    static void i(const Args& ... args){
      Context ctx;
      ctx.mode = Info;
      printImpl(ctx,ctx.buffer,sizeof(ctx.buffer),args...);
      }

    template< class ... Args >
    static void d(const Args& ... args){
      Context ctx;
      ctx.mode = Debug;
      printImpl(ctx,ctx.buffer,sizeof(ctx.buffer),args...);
      }

    template< class ... Args >
    static void e(const Args& ... args){
      Context ctx;
      ctx.mode = Error;
      printImpl(ctx,ctx.buffer,sizeof(ctx.buffer),args...);
      }

    static void setOutputCallback(std::function<void(Mode mode, const char* text)> f);

  private:
    struct Context {
      Mode mode;
      char buffer[256];
      };

    struct Globals {
      std::function<void(Log::Mode, const char*)> outFn;
      std::recursive_mutex                        sync;
      };

    Log() = delete;

    static Globals& globals();

    static void flush(Context& ctx, char*& msg, size_t& count);
    static void write(Context& ctx, char*& out, size_t& count, const std::string& msg);
    static void write(Context& ctx, char*& out, size_t& count, std::string_view   msg);
    static void write(Context& ctx, char*& out, size_t& count, const char*        msg);
    static void write(Context& ctx, char*& out, size_t& count, char               msg);
    static void write(Context& ctx, char*& out, size_t& count, int8_t             msg);
    static void write(Context& ctx, char*& out, size_t& count, uint8_t            msg);
    static void write(Context& ctx, char*& out, size_t& count, int16_t            msg);
    static void write(Context& ctx, char*& out, size_t& count, uint16_t           msg);
    static void write(Context& ctx, char*& out, size_t& count, const int32_t&     msg);
    static void write(Context& ctx, char*& out, size_t& count, const uint32_t&    msg);
    static void write(Context& ctx, char*& out, size_t& count, const int64_t&     msg);
    static void write(Context& ctx, char*& out, size_t& count, const uint64_t&    msg);
    static void write(Context& ctx, char*& out, size_t& count, float              msg);
    static void write(Context& ctx, char*& out, size_t& count, double             msg);
    static void write(Context& ctx, char*& out, size_t& count, const void*        msg);
    static void write(Context& ctx, char*& out, size_t& count, std::thread::id    msg);

    template<class T, std::enable_if_t<!std::is_same<T,uint32_t>::value && !std::is_same<T,uint64_t>::value && std::is_same<T,size_t>::value,bool> = true>
    static void write(Context& ctx, char*& out, size_t& count, T         msg) {
      writeUInt(ctx,out,count,msg);
      }

    static void printImpl(Context& ctx, char* out, size_t count);
    template<class T, class ... Args>
    static void printImpl(Context& ctx, char* out, size_t count,
                          const T& t, const Args& ... args){
      write(ctx,out,count,t);
      printImpl(ctx,out,count,args...);
      }

    template< class T >
    static void writeInt(Context& ctx, char*& out, size_t& count, T msg);

    template< class T >
    static void writeUInt(Context& ctx, char*& out, size_t& count, T msg) {
      char sym[32];
      sym[sizeof(sym)-1] = '\0';
      int pos = sizeof(sym)-2;

      while(pos>=0){
        sym[pos] = msg%10+'0';
        msg/=10;
        if(msg==0)
          break;
        --pos;
        }
      write(ctx,out,count,sym+pos);
      }
  };
}
