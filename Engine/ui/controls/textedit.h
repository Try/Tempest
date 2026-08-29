#pragma once

#include <Tempest/AbstractTextInput>

namespace Tempest {

class TextEdit : public Tempest::AbstractTextInput {
  public:
    TextEdit();

    void  setText(std::string_view text) override;
    using AbstractTextInput::setText;
    using AbstractTextInput::text;

    using AbstractTextInput::setFont;
    using AbstractTextInput::font;

    using AbstractTextInput::setTextColor;
    using AbstractTextInput::textColor;
  };

}
