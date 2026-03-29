#pragma once

class World;

class NavigationSystem {
public:
    void update(World & world, float dt_seconds) const;
};
