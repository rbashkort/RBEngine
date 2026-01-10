//
//  entity.cpp rbashkot 01/01/2026
//

#include "entity.h"

Entity::Entity(const std::string& name)
    : name(name) {}

Entity::Entity(const std::string& name, flecs::entity rawEnt)
    : name(name), ent(rawEnt) {}

void Entity::create(flecs::world& world) {
    ent = world.entity(name.c_str());
}
