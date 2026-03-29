#pragma once

#include "render/render_world.h"

class ProjectionSystem {
public:
    void run(RenderWorld & render_world, int window_width, int window_height) const;
};
