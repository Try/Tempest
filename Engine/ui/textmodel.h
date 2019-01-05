#pragma once

#include <Tempest/Font>
#include <Tempest/Size>

#include <vector>
#include <string>

namespace Tempest {

class Painter;

class TextModel final {
  public:
    TextModel()=default;
    TextModel(const char* text);

    void        setText(const char* text);
    void        setFont(const Font& f);
    const Font& font() const;

    const Size& sizeHint() const;
    Size        wrapSize() const;
    int         w() const { return sizeHint().w; }
    int         h() const { return sizeHint().h; }

    bool        isEmpty()  const;

    void        paint(Painter& p, int x, int y) const;

  private:
    void        calcSize() const;

    struct Sz {
      Size sizeHint;
      int  wrapHeight=0;
      bool actual=false;
      };
    mutable Sz sz;

    struct Line {
      const char* txt;
      };
    std::vector<char> text;
    std::vector<Line> line;
    Tempest::Font     fnt;
  };

}
