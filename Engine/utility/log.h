#ifndef LOG_H
#define LOG_H

#include <sstream>
#include <mutex>
#include <cstdint>

namespace Tempest{

class Log {
  public:
    enum Mode{
      Info,
      Error,
      Debug
      };

    template< class ... Args >
    static void i(const Args& ... args){
      std::lock_guard<std::mutex> g(mutex);
      (void)g;
      printImpl(Info,buffer,sizeof(buffer)/sizeof(buffer[0]),args...);
      }

    template< class ... Args >
    static void d(const Args& ... args){
      std::lock_guard<std::mutex> g(mutex);
      (void)g;
      printImpl(Debug,buffer,sizeof(buffer)/sizeof(buffer[0]),args...);
      }

    template< class ... Args >
    static void e(const Args& ... args){
      std::lock_guard<std::mutex> g(mutex);
      (void)g;
      printImpl(Error,buffer,sizeof(buffer)/sizeof(buffer[0]),args...);
      }

  private:
    Log(){}

    static void flush(Mode m, char*& msg, size_t& count);
    static void write(Mode m, char*& out, size_t& count, const std::string& msg);
    static void write(Mode m, char*& out, size_t& count, const char*  msg);
    static void write(Mode m, char*& out, size_t& count, char     msg);
    static void write(Mode m, char*& out, size_t& count, int8_t   msg);
    static void write(Mode m, char*& out, size_t& count, uint8_t  msg);
    static void write(Mode m, char*& out, size_t& count, int16_t  msg);
    static void write(Mode m, char*& out, size_t& count, uint16_t msg);
    static void write(Mode m, char*& out, size_t& count, int32_t  msg);
    static void write(Mode m, char*& out, size_t& count, uint32_t msg);
    static void write(Mode m, char*& out, size_t& count, uint64_t msg);
    static void write(Mode m, char*& out, size_t& count, int64_t  msg);
    static void write(Mode m, char*& out, size_t& count, float    msg);
    static void write(Mode m, char*& out, size_t& count, double   msg);
    static void write(Mode m, char*& out, size_t& count, void*    msg);

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

    static char buffer[128];
    static std::mutex mutex;
  };
}

#endif // LOG_H
