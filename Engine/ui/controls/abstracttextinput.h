#pragma once

#include <Tempest/TextModel>
#include <Tempest/UndoStack>
#include <Tempest/Timer>
#include <Tempest/Widget>
#include <Tempest/Color>
#include <Tempest/Shortcut>

namespace Tempest {

class AbstractTextInput : public Tempest::Widget {
  public:
    TextModel::Cursor selectionStart() const;
    TextModel::Cursor selectionEnd()   const;

    void 	            setUndoRedoEnabled(bool enable);
    bool              isUndoRedoEnabled() const;

    void              setTextColor(const Color& c);
    const Color&      textColor() const;

    Signal<void(const TextModel&)> onTextChanged;
    Signal<void(const TextModel&)> onTextEdited;

  protected:
    AbstractTextInput();

    virtual void      setText(const char* text);
    void              setText(const std::string& text);
    const TextModel&  text() const;

    void              setFont(const Font& f);
    const Font&       font() const;

    void              mouseDownEvent(Tempest::MouseEvent& e) override;
    void              mouseDragEvent(Tempest::MouseEvent& e) override;
    void              mouseUpEvent  (Tempest::MouseEvent& e) override;

    void              keyDownEvent  (Tempest::KeyEvent &event) override;
    void              keyRepeatEvent(Tempest::KeyEvent &event) override;
    void              keyUpEvent    (Tempest::KeyEvent &event) override;

    void              paintEvent    (Tempest::PaintEvent& e) override;
    void              focusEvent    (Tempest::FocusEvent& e) override;

    void              invalidateSizeHint();

  private:
    enum {
      StkDepth = 64
      };
    TextModel         textM;
    UndoStack<TextModel> stk;
    bool              undoEnable=true;

    TextModel::Cursor selS,selE;
    Shortcut          scUndo, scRedo;

    Font              fnt;
    bool              fntInUse=false;
    Color             fntCl;
    Timer             anim;
    bool              cursorState = false;

    void              keyEventImpl(KeyEvent& k);

    void              onRepaintCursor();
    void              updateFont();
    void              undo();
    void              redo();
    void              adjustSelection();
  };

}
