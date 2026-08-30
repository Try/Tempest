#include <gtest/gtest.h>

#include "../../../Engine/gapi/metal/mtswapchainstate.h"

#include <atomic>
#include <chrono>
#include <future>
#include <thread>

using namespace Tempest::Detail;
using namespace std::chrono_literals;

TEST(MetalSwapchainGate,AcquireResetOrdering) {
  MtSwapchainOperationGate gate;
  auto acquire = gate.startOperation();

  std::promise<void> resetStarted;
  std::promise<void> resetPassed;
  std::promise<void> releaseReset;
  auto releaseResetFuture = releaseReset.get_future();
  auto resetPassedFuture  = resetPassed.get_future();
  std::thread reset([&](){
    auto exclusive = gate.blockNewOperations();
    resetStarted.set_value();
    exclusive.wait();
    resetPassed.set_value();
    releaseResetFuture.wait();
    });

  resetStarted.get_future().wait();
  EXPECT_EQ(resetPassedFuture.wait_for(0ms),std::future_status::timeout);

  std::promise<void> secondAcquireStarted;
  auto secondAcquireFuture = secondAcquireStarted.get_future();
  std::thread secondAcquire([&](){
    auto operation = gate.startOperation();
    secondAcquireStarted.set_value();
    });
  EXPECT_EQ(secondAcquireFuture.wait_for(10ms),std::future_status::timeout);

  acquire.finish();
  EXPECT_EQ(resetPassedFuture.wait_for(1s),std::future_status::ready);
  EXPECT_EQ(secondAcquireFuture.wait_for(10ms),std::future_status::timeout);

  releaseReset.set_value();
  reset.join();
  EXPECT_EQ(secondAcquireFuture.wait_for(1s),std::future_status::ready);
  secondAcquire.join();
  }

TEST(MetalSwapchainGate,PresentRegistersSubmissionBeforeResetPasses) {
  MtSwapchainOperationGate gate;
  auto present = gate.startOperation();
  std::atomic_bool submitted{false};
  std::atomic_bool resetObservedSubmit{false};
  std::promise<void> resetStarted;

  std::thread reset([&](){
    auto exclusive = gate.blockNewOperations();
    resetStarted.set_value();
    exclusive.wait();
    resetObservedSubmit.store(submitted.load(std::memory_order_acquire),
                              std::memory_order_release);
    });

  resetStarted.get_future().wait();
  submitted.store(true,std::memory_order_release);
  present.finish();
  reset.join();
  EXPECT_TRUE(resetObservedSubmit.load(std::memory_order_acquire));
  }

TEST(MetalSwapchainGate,ResetWaitsForAllFramesInFlight) {
  MtSwapchainOperationGate gate;
  auto first  = gate.startOperation();
  auto second = gate.startOperation();
  std::promise<void> resetStarted;
  std::promise<void> resetPassed;
  auto resetPassedFuture = resetPassed.get_future();

  std::thread reset([&](){
    auto exclusive = gate.blockNewOperations();
    resetStarted.set_value();
    exclusive.wait();
    resetPassed.set_value();
    });

  resetStarted.get_future().wait();
  first.finish();
  EXPECT_EQ(resetPassedFuture.wait_for(10ms),std::future_status::timeout);
  second.finish();
  EXPECT_EQ(resetPassedFuture.wait_for(1s),std::future_status::ready);
  reset.join();
  }

TEST(MetalSwapchainGate,CompletionOwnsResourcesUntilRelease) {
  auto resource = std::make_shared<int>(42);
  std::weak_ptr<int> lifetime = resource;
  auto completion = [keepAlive=resource]() mutable {
    keepAlive.reset();
    };
  resource.reset();
  EXPECT_FALSE(lifetime.expired());
  completion();
  EXPECT_TRUE(lifetime.expired());
  }

TEST(MetalSwapchainState,SingleAcquireAndReuse) {
  MtSwapchainState state;
  state.reset(3);

  const auto first = state.beginAcquire();
  EXPECT_EQ(first.result,MtSwapchainState::Acquire::Result::Start);
  EXPECT_EQ(first.ticket.image,0u);
  EXPECT_EQ(state.beginAcquire().result,MtSwapchainState::Acquire::Result::Busy);

  ASSERT_TRUE(state.publish(first.ticket,MtSwapchainState::Target::Direct));
  const auto reused = state.beginAcquire();
  EXPECT_EQ(reused.result,MtSwapchainState::Acquire::Result::Reuse);
  EXPECT_EQ(reused.ticket,first.ticket);
  EXPECT_EQ(state.currentTarget(),MtSwapchainState::Target::Direct);
  }

TEST(MetalSwapchainState,CopyFallbackCanBePublished) {
  MtSwapchainState state;
  state.reset(2);
  const auto acquire = state.beginAcquire();

  ASSERT_TRUE(state.publish(acquire.ticket,MtSwapchainState::Target::Copy));
  EXPECT_EQ(state.currentTarget(),MtSwapchainState::Target::Copy);
  EXPECT_EQ(state.currentImage(),0u);
  }

TEST(MetalSwapchainState,MissingOrWrongDrawableSelectsCopyFallback) {
  EXPECT_EQ(MtSwapchainState::chooseTarget(true,false,false),
            MtSwapchainState::Target::Copy);
  EXPECT_EQ(MtSwapchainState::chooseTarget(true,true,false),
            MtSwapchainState::Target::Copy);
  EXPECT_EQ(MtSwapchainState::chooseTarget(true,true,true),
            MtSwapchainState::Target::Direct);
  EXPECT_EQ(MtSwapchainState::chooseTarget(false,true,true),
            MtSwapchainState::Target::Copy);
  }

TEST(MetalSwapchainState,ResetInvalidatesAcquireAndPresent) {
  MtSwapchainState state;
  state.reset(2);
  const auto acquiring = state.beginAcquire();
  state.reset(3);
  EXPECT_FALSE(state.publish(acquiring.ticket,MtSwapchainState::Target::Direct));
  EXPECT_EQ(state.currentImage(),0u);
  EXPECT_EQ(state.imageCount(),3u);

  const auto acquired = state.beginAcquire();
  ASSERT_TRUE(state.publish(acquired.ticket,MtSwapchainState::Target::Direct));
  MtSwapchainState::Ticket presenting;
  ASSERT_TRUE(state.beginPresent(presenting));
  state.reset(2);
  EXPECT_FALSE(state.presentCommitted(presenting));
  EXPECT_EQ(state.currentImage(),0u);
  }

TEST(MetalSwapchainState,IndexAdvancesOnlyAfterCommittedPresent) {
  MtSwapchainState state;
  state.reset(2);
  const auto acquired = state.beginAcquire();
  ASSERT_TRUE(state.publish(acquired.ticket,MtSwapchainState::Target::Direct));

  MtSwapchainState::Ticket firstPresent;
  ASSERT_TRUE(state.beginPresent(firstPresent));
  ASSERT_TRUE(state.presentFailed(firstPresent));
  EXPECT_EQ(state.currentImage(),0u);

  MtSwapchainState::Ticket retry;
  ASSERT_TRUE(state.beginPresent(retry));
  ASSERT_TRUE(state.presentCommitted(retry));
  EXPECT_EQ(state.currentImage(),1u);

  MtSwapchainState::Ticket duplicate;
  EXPECT_FALSE(state.beginPresent(duplicate));
  EXPECT_FALSE(state.presentCommitted(retry));
  EXPECT_EQ(state.currentImage(),1u);
  }

TEST(MetalSwapchainState,CancelledAcquireIsRetryable) {
  MtSwapchainState state;
  state.reset(2);
  const auto failed = state.beginAcquire();
  ASSERT_TRUE(state.cancelAcquire(failed.ticket));

  const auto retry = state.beginAcquire();
  EXPECT_EQ(retry.result,MtSwapchainState::Acquire::Result::Start);
  EXPECT_NE(retry.ticket.serial,failed.ticket.serial);
  ASSERT_TRUE(state.publish(retry.ticket,MtSwapchainState::Target::Copy));
  }
