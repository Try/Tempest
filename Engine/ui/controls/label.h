#pragma once

#include <Tempest/Widget>
#include <Tempest/TextModel>
#include <Tempest/Color>

namespace Tempest {

class Label : public Widget {
  public:
    Label();

    void          setFont(const Font& f);
    const Font&   font() const;

    void          setTextColor(const Color& c);
    const Color&  textColor() const;

    void          setText(const char*        t);
    void          setText(const std::string& t);

  protected:
    void          paintEvent(Tempest::PaintEvent &e);

  private:
    void          invalidateSizeHint();

    TextModel     textM;
    Color         textCl;
  };

}
