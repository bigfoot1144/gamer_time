#pragma once

#include "render/render_world.h"

class DepthSorter {
public:
    void run(RenderWorld & render_world) const;
};
