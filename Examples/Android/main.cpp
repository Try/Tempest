#include <Tempest/Application>
#include <Tempest/Event>
#include <Tempest/Log>
#include <Tempest/Window>

class Example final : public Tempest::Window {
  private:
    void resizeEvent(Tempest::SizeEvent& event) override {
      Tempest::Log::i("Window resized: ",event.w,"x",event.h);
      Tempest::Window::resizeEvent(event);
      }

    void focusEvent(Tempest::FocusEvent& event) override {
      Tempest::Log::i(event.in ? "Window resumed" : "Window paused");
      Tempest::Window::focusEvent(event);
      }
  };

int main(int, char**) {
  Tempest::Application app;
  Example window;
  return app.exec();
  }
