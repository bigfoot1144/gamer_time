#pragma once

#include "render/render_world.h"

#include <string>
#include <vector>

class World;

class RenderExtractor {
public:
    RenderWorld build(
        const World & world,
        const CameraState & camera,
        const std::vector<std::string> & ai_events,
        std::string overlay_text
    ) const;
};
