#pragma once

#include <Tempest/Signal>
#include <limits>

namespace Tempest {

class Timer final {
  public:
    Timer();
    Timer( const Timer& )              = delete;
    Timer& operator = ( const Timer& ) = delete;
    ~Timer();

    Signal<void()> timeout;

    void     start( uint64_t t );
    void     stop();
    uint64_t interval() const { return m.interval; }

  private:
    void     setRunning(bool b);
    bool     process(uint64_t now);

    struct {
      uint64_t interval=0;
      uint64_t lastEmit=0;
      bool     running=false;
      } m;

  friend class Application;
  };

}
