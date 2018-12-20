#pragma once

#include <Tempest/Panel>

class MdiWindow : public Tempest::Panel {
  public:
    MdiWindow();

  private:
    struct Top;
    void paintEvent    (Tempest::PaintEvent& e);
    void mouseDownEvent(Tempest::MouseEvent& e);
    void mouseDragEvent(Tempest::MouseEvent& e);

    void close();

    struct {
      bool l=false;
      bool r=false;
      bool t=false;
      bool b=false;

      Tempest::Point mpos;
      Tempest::Rect  origin;
      } m;
  };


