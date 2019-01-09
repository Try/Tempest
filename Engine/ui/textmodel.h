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

    class Cursor final {
      private:
        size_t line  =size_t(-1);
        size_t offset=0;

      public:
        bool operator == (const Cursor& c) const {
          return line==c.line && offset==c.offset;
          }

        bool operator != (const Cursor& c) const {
          return line!=c.line || offset!=c.offset;
          }

      friend class TextModel;
      };

    void        setText(const char* text);
    void        setFont(const Font& f);
    const Font& font() const;

    const Size& sizeHint() const;
    Size        wrapSize() const;
    int         w() const { return sizeHint().w; }
    int         h() const { return sizeHint().h; }

    bool        isEmpty()  const;

    void        paint(Painter& p, int x, int y) const;

    Cursor      charAt(const Point& p) const { return charAt(p.x,p.y); }
    Cursor      charAt(int x,int y) const;
    Point       mapToCoords(Cursor c) const;

    void        drawCursor(Painter& p,int x,int y,Cursor c) const;
    void        drawCursor(Painter& p,int x,int y,Cursor s,Cursor e) const;

    bool        isValid(Cursor c) const;

  private:
    void        calcSize() const;
    void        buildIndex();

    struct Sz {
      Size sizeHint;
      int  wrapHeight=0;
      bool actual=false;
      };
    mutable Sz sz;

    struct Line {
      const char* txt =nullptr;
      size_t      size=0;
      };
    std::vector<char> txt;
    std::vector<Line> line;
    Tempest::Font     fnt;
  };

}
