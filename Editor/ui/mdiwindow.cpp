#include "mdiwindow.h"

#include <Tempest/Painter>
#include <Tempest/Button>

#include "resources.h"

using namespace Tempest;

struct MdiWindow::Top : Widget {
  Top(){
    setLayout(Horizontal);
    setMaximumSize(maxSize().w,27);
    setMargins(0);
    }
  };

MdiWindow::MdiWindow() {
  setMargins(8);
  resize(400,300);
  setDragable(true);
  setLayout(Vertical);

  Top& top = addWidget(new Top);

  top.addWidget(new Widget());

  Button& exp = top.addWidget(new Button());
  exp.setMinimumSize(27,27);
  exp.setSizePolicy(Fixed);
  exp.setIcon(Resources::get<Sprite>("icon/expand.png"));

  Button& close = top.addWidget(new Button());
  close.setMinimumSize(27,27);
  close.setSizePolicy(Fixed);
  close.setIcon(Resources::get<Sprite>("icon/close.png"));
  //btn.setText("x");
  }

void MdiWindow::paintEvent(PaintEvent &e) {
  Painter p(e);

  if( !m.l && !m.r && !m.t && !m.b ){
    p.setPen(Pen(Color(0.278f,0.278f,0.278f,1),Painter::Alpha));
    } else {
    p.setPen(Pen(Color(0.278f,0.278f,0.278f,1),Painter::Add));
    }

  p.drawLine(0,0,w(),0);
  p.drawLine(0,h()-1,w(),h()-1);

  p.drawLine(0,0,0,h());
  p.drawLine(w()-1,0,w()-1,h());

  p.setBrush(Brush(Color(0.19f,0.19f,0.19f,0.96f),Painter::Alpha));
  p.drawRect(1,1,w()-2,h()-2);
  }

void MdiWindow::mouseDownEvent(MouseEvent &e) {
  if(e.button!=Event::ButtonLeft) {
    e.ignore();
    return;
    }
  m.l = e.x<margins().left;
  m.t = e.y<margins().top;
  m.r = e.x>w()-margins().right;
  m.b = e.y>h()-margins().bottom;

  m.mpos   = mapToRoot(e.pos());
  m.origin = Rect(x(),y(),w(),h());

  update();
  }

void MdiWindow::mouseDragEvent(MouseEvent& e) {
  /*
  if( m.container && m.container->showMode() ){
    storedGeometry = rect();
    m.container->setShowMode(false);
    }*/

  Rect  nrect = m.origin;
  Point pos   = mapToRoot(e.pos());
  if( !m.l && !m.r && !m.t && !m.b && isDragable() ){
    nrect.x += (pos.x-m.mpos.x);
    nrect.y += (pos.y-m.mpos.y);
    }

  if( m.l ){
    nrect.x += (pos.x-m.mpos.x);
    nrect.w -= (pos.x-m.mpos.x);
    } else
  if( m.r ){
    nrect.w += (pos.x-m.mpos.x);
    }

  if( m.t ){
    nrect.y += (pos.y-m.mpos.y);
    nrect.h -= (pos.y-m.mpos.y);
    } else
  if( m.b ){
    nrect.h += (pos.y-m.mpos.y);
    }

  if( nrect.w<minSize().w ) {
    if( m.l )
      nrect.x -= (minSize().w-nrect.w);
    nrect.w = minSize().w;
    }
  if( nrect.h<minSize().h ) {
    if( m.t )
      nrect.y -= (minSize().h-nrect.h);
    nrect.h = minSize().h;
    }

  setGeometry(nrect);
  update();
  }

