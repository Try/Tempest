#pragma once

#include <Tempest/Widget>

namespace Tempest {

class Button : public Widget {
  public:
    Button();

  protected:
    void mouseDownEvent(Tempest::MouseEvent& e) override;
    void paintEvent    (Tempest::PaintEvent& e) override;

  private:

  };

}
