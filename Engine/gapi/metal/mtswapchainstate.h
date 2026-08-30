#pragma once

#include <cassert>
#include <cstdint>
#include <condition_variable>
#include <mutex>

namespace Tempest::Detail {

/**
 * Serializes swapchain reset/destruction against CPU-side acquire/present
 * operations.  The exclusive side can invalidate the render generation before
 * waiting, while new operations remain blocked until layer reconfiguration is
 * complete.
 */
class MtSwapchainOperationGate final {
  public:
    class Operation final {
      public:
        Operation() = default;
        Operation(const Operation&) = delete;
        Operation& operator=(const Operation&) = delete;

        Operation(Operation&& other) noexcept :gate(other.gate) {
          other.gate = nullptr;
          }
        Operation& operator=(Operation&& other) noexcept {
          if(this==&other)
            return *this;
          finish();
          gate       = other.gate;
          other.gate = nullptr;
          return *this;
          }
        ~Operation() { finish(); }

        void finish() noexcept {
          if(gate==nullptr)
            return;
          auto* owner = gate;
          gate = nullptr;
          owner->finishOperation();
          }

      private:
        explicit Operation(MtSwapchainOperationGate* gate) :gate(gate) {}
        MtSwapchainOperationGate* gate = nullptr;

      friend class MtSwapchainOperationGate;
      };

    class Exclusive final {
      public:
        Exclusive() = default;
        Exclusive(const Exclusive&) = delete;
        Exclusive& operator=(const Exclusive&) = delete;

        Exclusive(Exclusive&& other) noexcept :gate(other.gate) {
          other.gate = nullptr;
          }
        Exclusive& operator=(Exclusive&& other) noexcept {
          if(this==&other)
            return *this;
          release();
          gate       = other.gate;
          other.gate = nullptr;
          return *this;
          }
        ~Exclusive() { release(); }

        void wait() {
          if(gate!=nullptr)
            gate->waitForOperations();
          }

        void release() noexcept {
          if(gate==nullptr)
            return;
          auto* owner = gate;
          gate = nullptr;
          owner->releaseExclusive();
          }

      private:
        explicit Exclusive(MtSwapchainOperationGate* gate) :gate(gate) {}
        MtSwapchainOperationGate* gate = nullptr;

      friend class MtSwapchainOperationGate;
      };

    Operation startOperation() {
      std::unique_lock<std::mutex> guard(sync);
      changed.wait(guard,[this](){ return !exclusive; });
      ++active;
      return Operation(this);
      }

    Exclusive blockNewOperations() {
      std::unique_lock<std::mutex> guard(sync);
      changed.wait(guard,[this](){ return !exclusive; });
      exclusive = true;
      return Exclusive(this);
      }

  private:
    void finishOperation() noexcept {
      std::lock_guard<std::mutex> guard(sync);
      assert(active>0);
      --active;
      changed.notify_all();
      }

    void waitForOperations() {
      std::unique_lock<std::mutex> guard(sync);
      changed.wait(guard,[this](){ return active==0; });
      }

    void releaseExclusive() noexcept {
      std::lock_guard<std::mutex> guard(sync);
      exclusive = false;
      changed.notify_all();
      }

    std::mutex              sync;
    std::condition_variable changed;
    uint32_t                active    = 0;
    bool                    exclusive = false;
  };

/**
 * Small, platform-independent state machine used by MtSwapchain.
 *
 * CAMetalLayer calls deliberately live outside this class (and outside the
 * swapchain lock).  Tickets make their results invalid as soon as reset()
 * starts a new generation.
 */
class MtSwapchainState final {
  public:
    enum class Target:uint8_t {
      Copy,
      Direct,
      };

    enum class Phase:uint8_t {
      Idle,
      Acquiring,
      Ready,
      Presenting,
      };

    struct Ticket {
      uint64_t generation = 0;
      uint64_t serial     = 0;
      uint32_t image      = 0;

      bool operator==(const Ticket& other) const noexcept {
        return generation==other.generation && serial==other.serial &&
               image==other.image;
        }
      bool operator!=(const Ticket& other) const noexcept {
        return !(*this==other);
        }
      };

    struct Acquire {
      enum class Result:uint8_t {
        Start,
        Reuse,
        Busy,
        };

      Result result = Result::Busy;
      Ticket ticket;
      };

    void reset(uint32_t count) noexcept {
      ++generation;
      ++serial;
      images  = count;
      current = 0;
      phase   = Phase::Idle;
      active  = {};
      target  = Target::Copy;
      }

    Acquire beginAcquire() noexcept {
      if(images==0)
        return {};
      if(phase==Phase::Ready)
        return {Acquire::Result::Reuse,active};
      if(phase!=Phase::Idle)
        return {};

      active = {generation,++serial,current};
      phase  = Phase::Acquiring;
      return {Acquire::Result::Start,active};
      }

    bool publish(const Ticket& ticket, Target renderTarget) noexcept {
      if(phase!=Phase::Acquiring || ticket!=active ||
         ticket.generation!=generation || ticket.image!=current)
        return false;
      target = renderTarget;
      phase  = Phase::Ready;
      return true;
      }

    bool cancelAcquire(const Ticket& ticket) noexcept {
      if(phase!=Phase::Acquiring || ticket!=active)
        return false;
      phase  = Phase::Idle;
      active = {};
      return true;
      }

    bool isAcquiring(const Ticket& ticket) const noexcept {
      return phase==Phase::Acquiring && ticket==active &&
             ticket.generation==generation && ticket.image==current;
      }

    bool beginPresent(Ticket& ticket) noexcept {
      if(phase!=Phase::Ready)
        return false;
      phase  = Phase::Presenting;
      ticket = active;
      return true;
      }

    bool presentFailed(const Ticket& ticket) noexcept {
      if(phase!=Phase::Presenting || ticket!=active)
        return false;
      phase = Phase::Ready;
      return true;
      }

    bool presentCommitted(const Ticket& ticket) noexcept {
      if(phase!=Phase::Presenting || ticket!=active || images==0)
        return false;
      current = (current+1)%images;
      phase   = Phase::Idle;
      active  = {};
      return true;
      }

    uint32_t currentImage() const noexcept { return current; }
    uint32_t imageCount()   const noexcept { return images;  }
    uint64_t currentGeneration() const noexcept { return generation; }
    Phase    currentPhase() const noexcept { return phase; }
    Target   currentTarget() const noexcept { return target; }

    static Target chooseTarget(bool directPreferred, bool drawableAvailable,
                               bool drawableSizeMatches) noexcept {
      return directPreferred && drawableAvailable && drawableSizeMatches
             ? Target::Direct : Target::Copy;
      }

  private:
    uint64_t generation = 0;
    uint64_t serial     = 0;
    uint32_t images     = 0;
    uint32_t current    = 0;
    Phase    phase      = Phase::Idle;
    Target   target     = Target::Copy;
    Ticket   active;
  };

}
