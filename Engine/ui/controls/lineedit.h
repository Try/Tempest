#pragma once

#include <Tempest/AbstractTextInput>

namespace Tempest {

class LineEdit : public Tempest::AbstractTextInput {
  public:
    LineEdit();

    void  setText(std::string_view text) override;
    using AbstractTextInput::setText;
    using AbstractTextInput::text;

    using AbstractTextInput::setFont;
    using AbstractTextInput::font;

    using AbstractTextInput::setTextColor;
    using AbstractTextInput::textColor;

    Tempest::Signal<void()> onEnter;

  protected:
    void keyDownEvent  (Tempest::KeyEvent &event) override;
    void keyRepeatEvent(Tempest::KeyEvent &event) override;

  private:
    void filterAndSetText(std::string_view src);
  };

}
