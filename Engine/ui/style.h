#pragma once

#include <Tempest/Timer>
#include <Tempest/Rect>

#include <string>
#include <memory>
#include <cstdint>

namespace Tempest {

struct Margin;
class  Painter;
class  Font;
class  Color;
class  Icon;
class  Sprite;

class  TextModel;
class  WidgetState;

class  Application;
class  Widget;
class  Button;
class  CheckBox;
class  Panel;
class  Label;
class  TextEdit;

class  ScrollBar;

class Style {
  public:
    Style();
    virtual ~Style();

    struct Extra {
      public:
        Extra(const Widget&   owner);
        Extra(const Button&   owner);
        // Extra(const Label&    owner);
        // Extra(const LineEdit& owner);

        const Margin&         margin;
        const Icon&           icon;
        const Font&           font;
        const Color&          fontColor;

      private:
        static const Tempest::Margin emptyMargin;
        static const Tempest::Icon   emptyIcon;
        static const Tempest::Font   emptyFont;
        static const Tempest::Color  emptyColor;
      };

    enum {
      cursorFlashTime=500
      };

    enum Element {
      E_Background,
      E_MenuBackground,
      E_MenuItemBackground,
      E_ArrowUp,
      E_ArrowDown,
      E_ArrowLeft,
      E_ArrowRight,
      E_CentralButton,
      E_Last
      };

    enum TextElement {
      TE_ButtonTitle,
      TE_CheckboxTitle,
      TE_LabelTitle,
      TE_LineEditContent,
      TE_Last
      };

    enum UIIntefaceCategory {
      UIIntefaceUnspecified=0,
      UIIntefacePC,
      UIIntefacePhone,
      UIIntefacePad
      };

    struct UIIntefaceIdiom {
      UIIntefaceIdiom(UIIntefaceCategory category);
      UIIntefaceCategory category;
      bool               touch;
      };
    virtual UIIntefaceIdiom idiom() const;

    // common
    virtual void draw(Painter& p, Widget*   w, Element e, const WidgetState& st, const Rect& r, const Extra& extra) const;
    virtual void draw(Painter& p, Panel *   w, Element e, const WidgetState& st, const Rect& r, const Extra& extra) const;
    virtual void draw(Painter& p, Button*   w, Element e, const WidgetState& st, const Rect& r, const Extra& extra) const;
    virtual void draw(Painter& p, CheckBox* w, Element e, const WidgetState& st, const Rect& r, const Extra& extra) const;
    virtual void draw(Painter& p, Label*    w, Element e, const WidgetState& st, const Rect& r, const Extra& extra) const;
    virtual void draw(Painter& p, TextEdit* w, Element e, const WidgetState& st, const Rect& r, const Extra& extra) const;

    // complex
    virtual void draw(Painter& p, ScrollBar* w, Element e, const WidgetState& st, const Rect& r, const Extra& extra) const;

    // text
    virtual void draw(Painter& p, const std::u16string& text, TextElement e, const WidgetState& st, const Rect& r, const Extra& extra) const;
    virtual void draw(Painter& p, const TextModel&      text, TextElement e, const WidgetState& st, const Rect& r, const Extra& extra) const;

  protected:
    virtual void polish  (Widget& w) const;
    virtual void unpolish(Widget& w) const;

    static const Tempest::Sprite& iconSprite(const Icon& icon,const WidgetState &st, const Rect &r);

  private:
    mutable uint32_t           refCnt   = 0;

    mutable bool               cursorState = false;
    mutable Tempest::TextEdit* focused     = nullptr;
    mutable Tempest::Timer     anim;
    mutable uint32_t           polished    = 0;

    void processAnimation();
    void drawCursor(Painter &p, const WidgetState &st, int x, int h, bool animState) const;

    void implAddRef() const { refCnt++; }
    void implDecRef() const ;

    Style(const Style& )=delete;
    Style& operator=(const Style&)=delete;

  friend class Widget;
  friend class Application;
  };

}
