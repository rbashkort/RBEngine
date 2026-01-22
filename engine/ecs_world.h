//
//  ecs_world.h rbashkort 01/01/2026
//

#pragma once

#include "components.h" 
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>
#include <flecs.h> 

// Hashing helper for spatial grid
static inline uint64_t hashCellGlobal(int x, int y) { 
    return (uint64_t((uint32_t)x) << 32) | (uint32_t)y; 
}

class ECSWorld {
public:
    ECSWorld();
    
    // Initializes the world, registers components and systems
    void init();
    void initRender();
    
    // Updates the ECS world (ticks systems)
    void update(float dt);
    
    flecs::world& getWorld() { return world; }

    // Render helpers
    void drawSprite(E_Sprite sprite, bool isLineLoop = false);

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
    flecs::query<E_Transform, E_Camera> qCamera_;

    // OpenGL Render Data
    unsigned int VAO = 0, VBO = 0, EBO = 0; // Vertex Array, Buffer, Element Buffer
    unsigned int shaderProgram = 0;

    unsigned int VAO_Rect = 0, VBO_Rect = 0;
    unsigned int VAO_Circle = 0, VBO_Circle = 0;
    unsigned int VAO_Tri = 0, VBO_Tri = 0;
    int numCircleVerts = 32;

    
    // Shader Uniform Locations
    int uColorLoc = -1;
    int uModelLoc = -1;
    int uProjLoc = -1;
    int uViewLoc = -1;
    int uUseTexLoc = -1;
    int uTextureLoc = -1;
    int uBloomLoc = -1;

    unsigned int createShader(const char* vShader, const char* fShader);
};
