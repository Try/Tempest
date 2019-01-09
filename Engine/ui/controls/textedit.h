#pragma once

#include <Tempest/TextModel>
#include <Tempest/Timer>
#include <Tempest/Widget>

namespace Tempest {

class TextEdit : public Tempest::Widget {
  public:
    TextEdit();

    void setText(const char* text);
    void setFont(const Font& f);

  protected:
    void mouseDownEvent(Tempest::MouseEvent& e) override;
    void mouseDragEvent(Tempest::MouseEvent& e) override;

    void paintEvent    (Tempest::PaintEvent& e) override;

  private:
    TextModel         textM;
    TextModel::Cursor selS,selE;

    Timer             anim;

    void      invalidateSizeHint();
  };

}
