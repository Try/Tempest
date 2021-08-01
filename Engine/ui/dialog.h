#pragma once

#include <Tempest/Panel>

namespace Tempest {

class UiOverlay;

class Dialog : public Panel {
  public:
    Dialog();
    ~Dialog() override;

    int  exec();
    void close();

    bool isOpen() const;

    void setModal(bool m);
    bool isModal() const;

    void setPopup(bool p);
    bool isPopup() const;

  protected:
    void         closeEvent  (CloseEvent& e) override;
    void         keyDownEvent(KeyEvent&   e) override;
    void         keyUpEvent  (KeyEvent&   e) override;
    void         paintEvent  (PaintEvent& e) override;
    virtual void paintShadow (PaintEvent& e);
    virtual void showEvent();

  private:
    struct Overlay;
    struct LayShadow;

    Overlay* owner_ov = nullptr;
    bool     modal    = true;
    bool     popup    = false;
  };

}
