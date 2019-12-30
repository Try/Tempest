#pragma once

#include <sstream>
#include <mutex>
#include <cstdint>
#include <thread>

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
    static void write(Mode m, char*& out, size_t& count, int32_t         msg);
    static void write(Mode m, char*& out, size_t& count, uint32_t        msg);
    static void write(Mode m, char*& out, size_t& count, uint64_t        msg);
    static void write(Mode m, char*& out, size_t& count, int64_t         msg);
    static void write(Mode m, char*& out, size_t& count, float           msg);
    static void write(Mode m, char*& out, size_t& count, double          msg);
    static void write(Mode m, char*& out, size_t& count, const void*     msg);
    static void write(Mode m, char*& out, size_t& count, std::thread::id msg);

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
    static void writeUInt(Mode m, char*& out, size_t& count, T msg);
  };
}
