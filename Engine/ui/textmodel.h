#pragma once

#include <Tempest/Font>
#include <Tempest/Size>

#include <vector>
#include <string>

namespace Tempest {

class TextModel final {
  public:
    TextModel()=default;
    TextModel(const char* text);

    void setFont(const Font& f);

    const Size& sizeHint() const;

  private:
    void calcSize() const;

    struct Sz {
      Size sizeHint;
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
