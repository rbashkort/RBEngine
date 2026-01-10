//
//  ecs_world.h rbashkort 01/01/2026
//

#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>
#include <flecs.h>
#include "components.h" 

// Hashing helper for spatial grid
static inline uint64_t hashCellGlobal(int x, int y) { 
    return (uint64_t((uint32_t)x) << 32) | (uint32_t)y; 
}

class ECSWorld {
public:
    ECSWorld();
    
    // Initializes the world, registers components and systems
    void init();
    
    // Updates the ECS world (ticks systems)
    void update(float dt);
    
    flecs::world& getWorld() { return world; }

    // Render helpers
    void drawSprite(E_Sprite sprite, bool isLineLoop = false);
    
    // Helper to check if mouse is hovering over an entity
    // TODO: Needs to account for Camera position/zoom in future
    bool hoverIt(E_Sprite &s, flecs::entity &e, E_Transform &t);

private:
    flecs::world world;

    // === Spatial Grid & Collision Members ===
    static constexpr int CELL_SIZE = 128;

    struct PairKey {
        flecs::entity_t a, b;
        bool operator==(const PairKey& o) const { return a == o.a && b == o.b; }
    };

    struct PairHash {
        std::size_t operator()(const PairKey& p) const noexcept {
            auto h1 = std::hash<uint64_t>{}(p.a);
            auto h2 = std::hash<uint64_t>{}(p.b);
            return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1 << 6) + (h1 >> 2));
        }
    };

    std::unordered_map<long long, std::vector<flecs::entity_t>> grid_;
    std::vector<flecs::entity_t> bigBodies_;
    std::unordered_set<PairKey, PairHash> testedPairs_;
    
    // Cached query for optimization
    flecs::query<E_Transform, E_Collider> qColliders_;
};
