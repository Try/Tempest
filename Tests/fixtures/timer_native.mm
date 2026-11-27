#include <Tempest/Application>
#include <Tempest/Dialog>
#include <Tempest/SystemApi>
#include <Tempest/Timer>

#import <AppKit/AppKit.h>
#import <CoreFoundation/CoreFoundation.h>

#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>

namespace {
struct Slot final {
  std::function<void()> callback;
  void invoke() { callback(); }
  };

class Fixture final {
  public:
    Fixture() {
      Tempest::Application::isRunning(); // Complete the actual AppKit startup.
      watchdog = CFRunLoopTimerCreateWithHandler(kCFAllocatorDefault,
        CFAbsoluteTimeGetCurrent()+3.,0,0,0,^(CFRunLoopTimerRef) {
          failed("native watchdog fired");
          if(child!=nullptr)
            child->close();
          if(parent!=nullptr)
            parent->close();
          Tempest::SystemApi::exit();
          });
      CFRunLoopAddTimer(CFRunLoopGetMain(),watchdog,kCFRunLoopCommonModes);
      }

    ~Fixture() {
      CFRunLoopTimerInvalidate(watchdog);
      CFRelease(watchdog);
      }

    void expect(bool condition, const char* message) {
      if(!condition)
        failed(message);
      }

    void bind(Tempest::Timer& timer, Slot& slot) {
      timer.timeout.bind(&slot,&Slot::invoke);
      timer.start(0);
      }

    void poll() {
      Tempest::Timer timer;
      unsigned calls = 0;
      Slot slot{[&] { ++calls; }};
      bind(timer,slot);
      for(unsigned i=0;i<4;++i) {
        [NSApp postEvent:[NSEvent otherEventWithType:NSEventTypeApplicationDefined
          location:NSZeroPoint modifierFlags:0 timestamp:0 windowNumber:0
          context:nil subtype:0 data1:0 data2:0] atStart:YES];
        Tempest::Application::processEvents();
        expect(calls==i+1,"one zero-interval timer callback per native poll");
        }
      timer.stop();
      Tempest::Application::processEvents();
      expect(calls==4,"stopped timer stays inactive");
      }

    void mutation() {
      auto first = std::make_unique<Tempest::Timer>();
      auto removed = std::make_unique<Tempest::Timer>();
      Tempest::Timer survivor, added;
      unsigned firstCalls=0, removedCalls=0, survivorCalls=0, addedCalls=0;
      Slot firstSlot{[&] {
        ++firstCalls;
        first.reset();
        removed.reset();
        added.start(0);
        }};
      Slot removedSlot{[&] { ++removedCalls; }};
      Slot survivorSlot{[&] { ++survivorCalls; }};
      Slot addedSlot{[&] { ++addedCalls; }};
      added.timeout.bind(&addedSlot,&Slot::invoke);
      bind(*first,firstSlot);
      bind(*removed,removedSlot);
      bind(survivor,survivorSlot);
      Tempest::Application::processEvents();
      expect(firstCalls==1 && removedCalls==0,"self/next destruction cancels only the removed timers");
      expect(survivorCalls==1 && addedCalls<=1,"mutation neither skips the survivor nor repeats the added timer");
      const auto addedBefore = addedCalls;
      Tempest::Application::processEvents();
      expect(survivorCalls==2 && addedCalls==addedBefore+1,"new registration stays active on the next poll");
      survivor.stop();
      added.stop();

      Tempest::Timer restart, next;
      unsigned restarts=0, nextCalls=0;
      Slot restartSlot{[&] {
        ++restarts;
        restart.stop();
        next.stop();
        next.start(0);
        }};
      Slot nextSlot{[&] { ++nextCalls; }};
      bind(restart,restartSlot);
      bind(next,nextSlot);
      Tempest::Application::processEvents();
      expect(restarts==1 && nextCalls<=1,"stop/restart cannot duplicate a callback");
      const auto nextBefore = nextCalls;
      Tempest::Application::processEvents();
      expect(nextCalls==nextBefore+1,"stop/restart of the next timer keeps its new registration");
      }

    void nested() {
      Tempest::Application application;
      auto opener = std::make_unique<Tempest::Timer>();
      auto removed = std::make_unique<Tempest::Timer>();
      Tempest::Timer survivor, added;
      Tempest::Dialog parentDialog, childDialog;
      parent = &parentDialog;
      child = &childDialog;
      unsigned calls=0, removedCalls=0, survivorCalls=0, addedCalls=0;
      unsigned depth=0, maxDepth=0;
      bool childReturned=false, parentReturned=false;
      Slot openerSlot{[&] {
        ++calls;
        ++depth;
        if(depth>maxDepth)
          maxDepth = depth;
        if(depth==1) {
          parentDialog.exec();
          parentReturned = true;
          survivor.stop();
          added.stop();
          Tempest::SystemApi::exit();
          } else if(depth==2) {
          childDialog.exec();
          childReturned = true;
          parentDialog.close();
          } else {
          childDialog.close();
          // Mutate the registry while all three timer sweeps are suspended.
          opener.reset();
          removed.reset();
          added.start(0);
          }
        --depth;
        }};
      Slot removedSlot{[&] { ++removedCalls; }};
      Slot survivorSlot{[&] { ++survivorCalls; }};
      Slot addedSlot{[&] { ++addedCalls; }};
      added.timeout.bind(&addedSlot,&Slot::invoke);
      bind(*opener,openerSlot);
      bind(*removed,removedSlot);
      bind(survivor,survivorSlot);
      // The baseline enters the first dialog from exec, then its watchdog
      // detects the missing timer dispatch inside Dialog::exec/processEvents.
      expect(application.exec()==0,"nested application loop exits normally");
      expect(parentReturned && childReturned,"both real modal dialogs returned");
      expect(calls==3 && maxDepth==3,"the same timer reenters both modal event loops");
      expect(removedCalls==0,"nested removal cancels the pending callback");
      expect(survivorCalls==1 && addedCalls<=1,"nested sweep supersedes the suspended outer sweeps");
      parent = nullptr;
      child = nullptr;
      }

    void reentry() {
      // Repeat across millisecond boundaries: a newly started zero timer can
      // share the suspended sweep's timestamp, exposing stale-cursor replay.
      for(unsigned attempt=0;attempt<32;++attempt)
        reentryOnce();
      }

    void reentryOnce() {
      Tempest::Timer opener, survivor, addedA, addedB;
      bool entered=false;
      unsigned survivorCalls=0, addedACalls=0, addedBCalls=0;
      Slot openerSlot{[&] {
        if(entered) {
          opener.stop();
          return;
          }
        entered = true;
        Tempest::Application::processEvents();
        // Both registrations belong to the next sweep. A shared cursor can
        // resume midway through them after the nested sweep overwrites it.
        addedA.start(0);
        addedB.start(0);
        }};
      Slot survivorSlot{[&] { ++survivorCalls; }};
      Slot addedASlot{[&] { ++addedACalls; }};
      Slot addedBSlot{[&] { ++addedBCalls; }};
      addedA.timeout.bind(&addedASlot,&Slot::invoke);
      addedB.timeout.bind(&addedBSlot,&Slot::invoke);
      bind(opener,openerSlot);
      bind(survivor,survivorSlot);
      const auto before = Tempest::Application::tickCount();
      while(Tempest::Application::tickCount()==before) {}
      Tempest::Application::processEvents();
      expect(entered && survivorCalls==1,"nested poll processes the surviving timer once");
      expect(addedACalls==0 && addedBCalls==0,"suspended outer sweep never resumes at a stale cursor");
      Tempest::Application::processEvents();
      expect(survivorCalls==2 && addedACalls==1 && addedBCalls==1,
             "the next poll processes both newly registered timers");
      }

    void exception() {
      Tempest::Timer opener, throwing, survivor;
      unsigned openerCalls=0, survivorCalls=0;
      bool entered=false, caught=false;
      Slot openerSlot{[&] {
        ++openerCalls;
        if(entered) {
          opener.stop();
          return;
          }
        entered = true;
        Tempest::Application::processEvents();
        }};
      Slot throwingSlot{[&] {
        throwing.stop();
        throw std::runtime_error("timer fixture");
        }};
      Slot survivorSlot{[&] { ++survivorCalls; }};
      bind(opener,openerSlot);
      bind(throwing,throwingSlot);
      bind(survivor,survivorSlot);
      try {
        Tempest::Application::processEvents();
        } catch(const std::runtime_error&) {
        caught = true;
        }
      expect(caught && openerCalls==2 && survivorCalls==0,"nested callback exception propagates");
      Tempest::Application::processEvents();
      Tempest::Application::processEvents();
      expect(survivorCalls==2,"timer traversal recovers after nested exception unwinding");
      }

    void exit(bool native) {
      Tempest::Application application;
      Tempest::Timer quit, survivor;
      unsigned quitCalls=0, survivorCalls=0, atExit=0;
      Slot quitSlot{[&] {
        ++quitCalls;
        Tempest::SystemApi::exit();
        }};
      Slot survivorSlot{[&] { ++survivorCalls; }};
      if(native) {
        auto exit = [&] {
          atExit = survivorCalls;
          Tempest::SystemApi::exit();
          };
        dispatch_async(dispatch_get_main_queue(),^{
          exit();
          });
        } else {
        bind(quit,quitSlot);
        }
      bind(survivor,survivorSlot);
      expect(application.exec()==0,"application exits normally");
      expect(!Tempest::Application::isRunning(),"native exit flag is set");
      expect(native ? survivorCalls==atExit : quitCalls==1 && survivorCalls==0,
             "exit prevents remaining timer callbacks");
      const auto count = survivorCalls;
      Tempest::Application::processEvents();
      Tempest::Application::processEvents();
      expect(survivorCalls==count,"no timer callbacks after Application::exec returns");
      }

    int result(const char* mode) const {
      std::printf("RESULT timer-native mode=%s failures=%u\n",mode,failures);
      return failures==0 ? 0 : 1;
      }

  private:
    void failed(const char* message) {
      ++failures;
      std::fprintf(stderr,"FAIL: %s\n",message);
      }

    CFRunLoopTimerRef watchdog = nullptr;
    Tempest::Dialog* parent = nullptr;
    Tempest::Dialog* child = nullptr;
    unsigned failures = 0;
  };
}

int main(int argc, char** argv) {
  if(argc!=2)
    return 2;
  @autoreleasepool {
    Fixture fixture;
    if(std::strcmp(argv[1],"poll")==0)
      fixture.poll();
    else if(std::strcmp(argv[1],"mutation")==0)
      fixture.mutation();
    else if(std::strcmp(argv[1],"nested")==0)
      fixture.nested();
    else if(std::strcmp(argv[1],"reentry")==0)
      fixture.reentry();
    else if(std::strcmp(argv[1],"exception")==0)
      fixture.exception();
    else if(std::strcmp(argv[1],"exit")==0)
      fixture.exit(false);
    else if(std::strcmp(argv[1],"native-exit")==0)
      fixture.exit(true);
    else
      return 2;
    return fixture.result(argv[1]);
    }
  }
