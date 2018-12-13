#pragma once

#include <Tempest/Widget>
#include <Tempest/TextModel>

namespace Tempest {

class Font;

class Button : public Widget {
  public:
    Button();

    void setText(const char* text);
    void setFont(const Font& f);

  protected:
    void mouseDownEvent(Tempest::MouseEvent& e) override;
    void paintEvent    (Tempest::PaintEvent& e) override;

  private:
    TextModel textM;
  };

}
