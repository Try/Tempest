#pragma once

#include <Tempest/Button>

namespace Tempest {

class CheckBox : public Button {
  public:
    CheckBox();
    ~CheckBox();

  protected:
    void paintEvent(Tempest::PaintEvent&  e) override;
  };

}

