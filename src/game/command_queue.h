#pragma once

#include "game/components.h"

#include <queue>

class World;

class CommandQueue {
public:
    void push(MoveCommand command);
    void apply(World & world);
    bool empty() const {
        return commands_.empty();
    }

private:
    std::queue<MoveCommand> commands_;
};
