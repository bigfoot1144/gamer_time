#include "render/depth_sorter.h"

#include <algorithm>

void DepthSorter::run(RenderWorld & render_world) const {
    std::sort(
        render_world.projected_units.begin(),
        render_world.projected_units.end(),
        [](const ProjectedUnit & lhs, const ProjectedUnit & rhs) {
            if (lhs.depth_key == rhs.depth_key) {
                return lhs.source.id < rhs.source.id;
            }
            return lhs.depth_key < rhs.depth_key;
        }
    );
}
