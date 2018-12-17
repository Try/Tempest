#pragma once

#include <Tempest/Widget>
#include <Tempest/Sprite>

namespace Tempest {

/** \addtogroup GUI
 *  @{
 */
class Panel : public Tempest::Widget {
  public:
    Panel();

    void setDragable( bool d );
    bool isDragable();

  protected:
    void mouseDownEvent(Tempest::MouseEvent &e);
    void mouseDragEvent(Tempest::MouseEvent &e);
    void mouseMoveEvent(Tempest::MouseEvent &e);
    void mouseUpEvent(Tempest::MouseEvent &e);

    void mouseWheelEvent(Tempest::MouseEvent &e);
    //void gestureEvent   (Tempest::AbstractGestureEvent &e);

    void paintEvent(Tempest::PaintEvent &p);

  private:
    bool mouseTracking, dragable;
    Tempest::Point mpos, oldPos;
  };
/** @}*/

}
