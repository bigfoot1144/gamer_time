#pragma once

#include "render/render_world.h"

class BatchBuilder {
public:
    RenderBatch build(const RenderWorld & render_world) const;
};
