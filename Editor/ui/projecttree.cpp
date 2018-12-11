#include "projecttree.h"

#include <Tempest/Button>
#include <Tempest/Painter>
#include <Tempest/SizePolicy>

#include <Tempest/Brush>

using namespace Tempest;

ProjectTree::ProjectTree() {
  setSizeHint(200,0);
  setSizePolicy(Fixed,Expanding);
  setLayout(Vertical);

  addWidget(new Button());
  addWidget(new Button());
  addWidget(new Button());
  }

void ProjectTree::paintEvent(PaintEvent &e) {
  Painter p(e);

  p.setBrush(background);
  int bw   = int(background.w());
  int bh   = int(background.h());
  int wpad = bw/3;
  int hpad = bh/3;

  p.drawRect(0,0,w()-wpad,hpad,    0,0,bw-wpad,hpad);
  p.drawRect(w()-wpad,0,wpad,hpad, bw-wpad,0,bw,hpad);

  p.drawRect(0,h()-hpad,w()-wpad,hpad,
             0,bh-hpad,bw-wpad,bh);
  p.drawRect(w()-wpad,h()-hpad,wpad,hpad,
             bw-wpad,bh-hpad,bw,bh);

  //cen
  p.drawRect(0,hpad, w()-wpad,h()-2*hpad,
             0,hpad, bw-wpad,bh-hpad);

  p.drawRect(w()-wpad,hpad, wpad,h()-2*hpad,
             bw-wpad,hpad, bw,bh-hpad);
  }
