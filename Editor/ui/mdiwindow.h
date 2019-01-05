#pragma once

#include <Tempest/Panel>

class MdiWindow : public Tempest::Panel {
  public:
    class Content : public Tempest::Widget {

      };

    MdiWindow();

    static MdiWindow* mkWindow(Content* c);

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

      Tempest::Point   mpos;
      Tempest::Rect    origin;

      Tempest::Widget* content=nullptr;
      } m;
  };


