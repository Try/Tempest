#pragma once

#include <Tempest/Texture2d>
#include <Tempest/Widget>

#include "resources.h"

class ProjectTree : public Tempest::Widget {
  public:
    ProjectTree();

  private:
    void paintEvent(Tempest::PaintEvent& event) override;

    const Tempest::Texture2d& background = Resources::get<Tempest::Texture2d>("toolbar.png");
  };
