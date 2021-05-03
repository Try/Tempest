#pragma once

#include <sstream>
#include <mutex>
#include <cstdint>
#include <thread>
#include <type_traits>

namespace Tempest{

class Log final {
  public:
    template< class ... Args >
    static void i(const Args& ... args){
      Context& c = getCtx();
      std::lock_guard<std::mutex> g(c.mutex);
      (void)g;
      printImpl(Info,c.buffer,sizeof(c.buffer),args...);
      }

    template< class ... Args >
    static void d(const Args& ... args){
      Context& c = getCtx();
      std::lock_guard<std::mutex> g(c.mutex);
      (void)g;
      printImpl(Debug,c.buffer,sizeof(c.buffer),args...);
      }

    template< class ... Args >
    static void e(const Args& ... args){
      Context& c = getCtx();
      std::lock_guard<std::mutex> g(c.mutex);
      (void)g;
      printImpl(Error,c.buffer,sizeof(c.buffer),args...);
      }

  private:
    enum Mode {
      Info,
      Error,
      Debug
      };

    struct Context {
      char buffer[128];
      std::mutex mutex;
      };

    Log() = delete;

    static Context& getCtx();

    static void flush(Mode m, char*& msg, size_t& count);
    static void write(Mode m, char*& out, size_t& count, const std::string& msg);
    static void write(Mode m, char*& out, size_t& count, const char*     msg);
    static void write(Mode m, char*& out, size_t& count, char            msg);
    static void write(Mode m, char*& out, size_t& count, int8_t          msg);
    static void write(Mode m, char*& out, size_t& count, uint8_t         msg);
    static void write(Mode m, char*& out, size_t& count, int16_t         msg);
    static void write(Mode m, char*& out, size_t& count, uint16_t        msg);
    static void write(Mode m, char*& out, size_t& count, const int32_t&  msg);
    static void write(Mode m, char*& out, size_t& count, const uint32_t& msg);
    static void write(Mode m, char*& out, size_t& count, const int64_t&  msg);
    static void write(Mode m, char*& out, size_t& count, const uint64_t& msg);
    static void write(Mode m, char*& out, size_t& count, float           msg);
    static void write(Mode m, char*& out, size_t& count, double          msg);
    static void write(Mode m, char*& out, size_t& count, const void*     msg);
    static void write(Mode m, char*& out, size_t& count, std::thread::id msg);

    template<class T, std::enable_if_t<!std::is_same<T,uint32_t>::value && !std::is_same<T,uint64_t>::value && std::is_same<T,size_t>::value,bool> = true>
    static void write(Mode m, char*& out, size_t& count, T               msg) {
      writeUInt(m,out,count,msg);
      }

    static void printImpl(Mode m,char* out, size_t count);
    template<class T, class ... Args>
    static void printImpl(Mode m, char* out, size_t count,
                          const T& t, const Args& ... args){
      write(m,out,count,t);
      printImpl(m,out,count,args...);
      }

    template< class T >
    static void writeInt(Mode m, char*& out, size_t& count, T msg);

    template< class T >
    static void writeUInt(Mode m, char*& out, size_t& count, T msg) {
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
      write(m,out,count,sym+pos);
      }
  };
}
