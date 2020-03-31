#pragma once

#include <Tempest/Widget>
#include <Tempest/TextModel>
#include <Tempest/Icon>
#include <Tempest/Signal>

namespace Tempest {

class Font;

class Button : public Widget {
  public:
    Button();

    using Type=Tempest::WidgetState::ButtonType;
    static constexpr Type T_PushButton      = Type::T_PushButton;
    static constexpr Type T_ToolButton      = Type::T_ToolButton;
    static constexpr Type T_FlatButton      = Type::T_FlatButton;
    static constexpr Type T_CheckableButton = Type::T_CheckableButton;

    using CheckState=Tempest::WidgetState::CheckState;
    static constexpr CheckState Unchecked       =CheckState::Unchecked;
    static constexpr CheckState Checked         =CheckState::Checked;
    static constexpr CheckState PartiallyChecked=CheckState::PartiallyChecked;

    void         setText(const char* text);
    void         setText(const std::string& text);

    void         setFont(const Font& f);
    const Font&  font() const;

    void         setIcon(const Icon& s);
    const Icon&  icon() const { return icn; }

    void         setButtonType(Type t);
    Type         buttonType() const;

    void         setChecked(bool c);
    void         setChecked(WidgetState::CheckState c);

    bool         isPressed() const;
    CheckState   isChecked() const;

    Signal<void()> onClick;

  protected:
    void mouseDownEvent (Tempest::MouseEvent&  e) override;
    void mouseUpEvent   (Tempest::MouseEvent&  e) override;
    void mouseMoveEvent (Tempest::MouseEvent&  e) override;

    void mouseEnterEvent(Tempest::MouseEvent&  e) override;
    void mouseLeaveEvent(Tempest::MouseEvent&  e) override;

    void paintEvent     (Tempest::PaintEvent&  e) override;
    void polishEvent    (Tempest::PolishEvent& e) override;

  private:
    TextModel    textM;
    Icon         icn;

    void         invalidateSizeHint(); //TODO: remove
    void         setPressed(bool p);
    void         showMenu();
  };

}
