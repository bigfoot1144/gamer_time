#pragma once

#include "render/render_world.h"

class FrustumCuller {
public:
    void run(RenderWorld & render_world, int window_width, int window_height) const;
};
