#include <Tempest/Application>
#include <Tempest/Device>
#include <Tempest/Event>
#include <Tempest/Fence>
#include <Tempest/Log>
#include <Tempest/VulkanApi>
#include <Tempest/Window>

#include <optional>

class Example final : public Tempest::Window {
  public:
    Example(Tempest::Device& device) : device(device) {
      }

    ~Example() override {
      device.waitIdle();
      }

  private:
    void render() override {
      try {
        if(!swapchain) {
          swapchain.emplace(device,hwnd());
          resetPending = false;
          }
        else if(resetPending) {
          device.waitIdle();
          swapchain->reset();
          resetPending = false;
          }
        fence.wait();
        {
        auto enc = commands.startEncoding(device);
        const float blue = float(Tempest::Application::tickCount()%2000)/2000.f;
        enc.setFramebuffer({{(*swapchain)[swapchain->currentImage()],Tempest::Vec4(0,0,blue,1),Tempest::Preserve}});
        }
        fence = device.submit(commands);
        device.present(*swapchain);
        }
      catch(const Tempest::SwapchainSuboptimal&) {
        resetPending = true;
        }
      }

    void resizeEvent(Tempest::SizeEvent& event) override {
      Tempest::Log::i("Window resized: ",event.w,"x",event.h);
      resetPending = true;
      Tempest::Window::resizeEvent(event);
      }

    void focusEvent(Tempest::FocusEvent& event) override {
      Tempest::Log::i(event.in ? "Window resumed" : "Window paused");
      Tempest::Window::focusEvent(event);
      }

    Tempest::Device&       device;
    std::optional<Tempest::Swapchain> swapchain;
    Tempest::CommandBuffer commands;
    Tempest::Fence         fence;
    bool                   resetPending = false;
  };

int main(int, char**) {
  Tempest::Application app;
  Tempest::VulkanApi api;
  Tempest::Device device(api);
  Example window(device);
  return app.exec();
  }
