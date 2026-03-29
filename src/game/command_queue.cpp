#include "game/command_queue.h"

#include "game/world.h"

#include <utility>

void CommandQueue::push(MoveCommand command) {
    commands_.push(std::move(command));
}

void CommandQueue::apply(World & world) {
    while (!commands_.empty()) {
        const MoveCommand command = std::move(commands_.front());
        commands_.pop();

        for (UnitId unit_id : command.units) {
            if (UnitComponent * unit = world.try_unit(unit_id)) {
                unit->has_move_target = true;
                unit->move_target = command.destination;
            }
        }
    }
}
