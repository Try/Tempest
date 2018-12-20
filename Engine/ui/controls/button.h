#pragma once

#include <Tempest/Widget>
#include <Tempest/TextModel>
#include <Tempest/Signal>

namespace Tempest {

class Font;

class Button : public Widget {
  public:
    Button();

    void setText(const char* text);
    void setFont(const Font& f);

    void setIcon(const Sprite& s);

    Signal<void()> onClick;

  protected:
    void mouseDownEvent(Tempest::MouseEvent& e) override;
    void mouseUpEvent  (Tempest::MouseEvent& e) override;
    void paintEvent    (Tempest::PaintEvent& e) override;

  private:
    TextModel textM;
    Sprite    icon;

    void invalidateSizeHint();
  };

}
