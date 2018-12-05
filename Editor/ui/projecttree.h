#pragma once

#include <Tempest/Texture2d>
#include <Tempest/Sprite>
#include <Tempest/Widget>

#include "resources.h"

class ProjectTree : public Tempest::Widget {
  public:
    ProjectTree();

  private:
    void paintEvent(Tempest::PaintEvent& event) override;

    const Tempest::Sprite& background1 = Resources::get<Tempest::Sprite>("toolbar.png");
    const Tempest::Texture2d& background = Resources::get<Tempest::Texture2d>("toolbar.png");
  };
