#pragma once

#include <Tempest/Font>
#include <Tempest/Size>
#include <Tempest/UndoStack>

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

        bool operator < (const Cursor& c) const {
          if(line<c.line)
            return true;
          if(line>c.line)
            return false;
          return offset<c.offset;
          }

        bool operator <= (const Cursor& c) const {
          if(line<c.line)
            return true;
          if(line>c.line)
            return false;
          return offset<=c.offset;
          }

        bool operator > (const Cursor& c) const {
          return !(*this<=c);
          }

        bool operator >= (const Cursor& c) const {
          return !(*this<c);
          }

      friend class TextModel;
      };

    using Command = UndoStack<TextModel>::Command;

    class CommandInsert : public Command {
      public:
        CommandInsert(const char* txt,Cursor where);
        void redo(TextModel &subj) override;
        void undo(TextModel &subj) override;
      private:
        Cursor      where;
        std::string txt;
        char        txtShort[3]={};
      };

    class CommandReplace : public Command {
      public:
        CommandReplace(const char* txt,Cursor beg,Cursor end);
        void redo(TextModel &subj) override;
        void undo(TextModel &subj) override;
      private:
        Cursor      begin;
        Cursor      end;
        std::string txt;
        char        txtShort[3]={};

        std::string prev;
      };

    class CommandErase : public Command {
      public:
        CommandErase(Cursor beg,Cursor end);
        void redo(TextModel &subj) override;
        void undo(TextModel &subj) override;
      private:
        Cursor      begin;
        Cursor      end;
        std::string prev;
        char        prevShort[3]={};
      };

    void        setText(const char* text);

    void        insert(const char* txt,Cursor where);
    void        erase  (Cursor s, Cursor e);
    void        erase  (Cursor s, size_t count);
    void        replace(const char* txt,Cursor s,Cursor e);
    void        fetch  (Cursor s,Cursor e, std::string& buf);
    void        fetch  (Cursor s,Cursor e, char* buf);

    Cursor      advance(Cursor src, int32_t offset) const;

    void        setFont(const Font& f);
    const Font& font() const;

    const Size& sizeHint() const;
    Size        wrapSize() const;
    int         w() const { return sizeHint().w; }
    int         h() const { return sizeHint().h; }

    bool        isEmpty()  const;

    void        paint(Painter& p, const Color& color, int x, int y) const;
    void        paint(Painter& p, const Font& fnt, const Color& color, int x, int y) const;

    Cursor      charAt(const Point& p) const { return charAt(p.x,p.y); }
    Cursor      charAt(int x,int y) const;
    Point       mapToCoords(Cursor c) const;
    const char* c_str() const;

    void        drawCursor(Painter& p,int x,int y,Cursor c) const;
    void        drawCursor(Painter& p,int x,int y,Cursor s,Cursor e) const;
    bool        isValid(Cursor c) const;
    Cursor      clamp(const Cursor& c) const;

  private:
    struct Sz {
      Size sizeHint;
      int  wrapHeight=0;
      bool actual=false;
      };

    struct Line {
      const char* txt =nullptr;
      size_t      size=0;
      };

    size_t      cursorCast(Cursor c) const;
    Cursor      cursorCast(size_t c) const;

    void        calcSize() const;
    Sz          calcSize(const Font& fnt) const;
    void        buildIndex();

    mutable Sz        sz;
    std::vector<char> txt;
    std::vector<Line> line;
    Tempest::Font     fnt;
  };

}
