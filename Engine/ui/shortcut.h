#pragma once

#include <Tempest/Event>
#include <Tempest/Signal>

namespace Tempest {

class Widget;

class Shortcut final {
  public:
    Shortcut() = default;

    Shortcut(Widget& w,
             Event::Modifier md,
             KeyEvent::KeyType key);
    Shortcut(const Shortcut& sc) = delete;
    Shortcut(Shortcut&& sc );
    ~Shortcut();

    Shortcut& operator = (const Shortcut& sc ) = delete;
    Shortcut& operator = (Shortcut&& sc );

    void setKey     (KeyEvent::KeyType key);
    void setKey     (uint32_t          key);
    void setModifier(Event::Modifier   mod);

    void setEnable(bool e);
    bool isEnable() const;

    KeyEvent::KeyType key()      const;
    uint32_t          lkey()     const;
    Event::Modifier   modifier() const;

    Signal<void()>    onActivated;

  private:
    struct M {
      Event::Modifier   modifier = Event::M_NoModifier;
      KeyEvent::KeyType key      = Event::K_NoKey;
      uint32_t          lkey     = 0;
      Widget*           owner    = nullptr;
      bool              disabled = false;
      } m;

  friend class Widget;
  };

}

