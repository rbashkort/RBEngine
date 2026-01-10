//
//  entity.h rbashkort 01/01/2026
//

#pragma once

#include <flecs.h>
#include <string>
#include <iostream>
#include <type_traits>

class Entity {
public:
    explicit Entity(const std::string& name);
    Entity(const std::string& name, flecs::entity rawEnt);
    ~Entity() = default;

    void draw() const;
    void create(flecs::world& world);

    template<typename T>
    void addTag() {
        if (!ent.is_alive()) return;
        ent.add<T>();
    }

    template<typename T>
    void addComponent(const T& component);

    template<typename T>
    void addComponent(T&& component);

    template<typename... Components>
    void addManyComponents(Components&&... comps);

    template<typename T>
    void removeComponent();

    template<typename T>
    bool hasComponent() const;

    template<typename T>
    const T* getComponent() const;

    template<typename T>
    T* getComponentMut() { return &ent.get_mut<T>(); }

    template<typename T>
    T* getComponentTryMut() { return ent.try_get_mut<T>(); }

    bool is_valid() { return ent.is_valid(); }
    bool is_alive() { return ent.is_alive(); }
    bool is_entity() { return ent.is_entity(); }
    bool is_pair() { return ent.is_pair(); }
    bool is_wildcard() { return ent.is_wildcard(); }

    flecs::entity getRawEntity() const;

protected:
    std::string name;
    flecs::entity ent;
};

template <typename T>
void Entity::addComponent(const T& comp)
{
    if (!ent.is_alive()) {
        std::cerr << "[Engine] Entity not created or is dead\n";
        return;
    }

    if constexpr (std::is_empty_v<T>) {
        ent.add<T>();
    } else {
        ent.set<T>(comp);
    }
}

template<typename T>
void Entity::addComponent(T&& comp)
{
    if (!ent.is_alive()) {
        std::cerr << "[Engine] Entity not created or is dead\n";
        return;
    }

    using U = std::decay_t<T>;

    if constexpr (std::is_empty_v<U>) {
        ent.add<U>();
    } else {
        ent.set<U>(std::forward<T>(comp));
    }
}

template<typename... Components>
void Entity::addManyComponents(Components&&... comps)
{
    if (!ent.is_alive()) {
        std::cerr << "Entity not created or is dead\n";
        return;
    }
    (addComponent(std::forward<Components>(comps)), ...);
}
template<typename T>
void Entity::removeComponent()
{
    if (!ent.is_alive()) return;
    ent.remove<T>();
}

template<typename T>
bool Entity::hasComponent() const
{
    if (!ent.is_alive()) return false;
    return ent.has<T>();
}

template<typename T>
const T* Entity::getComponent() const
{
    if (!ent.is_alive()) return nullptr;
    return ent.get<T>();
}

inline flecs::entity Entity::getRawEntity() const
{
    return ent;
}

