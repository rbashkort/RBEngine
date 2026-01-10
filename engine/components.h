//
//  components.h rbashkort 31/12/2025
//

#pragma once

#include <GL/gl.h>
#include <GLFW/glfw3.h>
#include <functional>
#include <flecs.h>
#include <string>

struct BackGroundColor {
    GLfloat r, g, b, a;
};

struct ColorRGB {
    GLfloat r, g, b;
};

// backGround
constexpr BackGroundColor BackGround_RED   {1.0f, 0.0f, 0.0f, 1.0f};
constexpr BackGroundColor BackGround_GREEN {0.0f, 1.0f, 0.0f, 1.0f};
constexpr BackGroundColor BackGround_BLUE  {0.0f, 0.0f, 1.0f, 1.0f};
constexpr BackGroundColor BackGround_BLACK {0.0f, 0.0f, 0.0f, 1.0f};
constexpr BackGroundColor BackGround_WHITE {1.0f, 1.0f, 1.0f, 1.0f};

// Default colors
constexpr ColorRGB E_RED   {1.0f, 0.0f, 0.0f};
constexpr ColorRGB E_GREEN {0.0f, 1.0f, 0.0f};
constexpr ColorRGB E_BLUE  {0.0f, 0.0f, 1.0f};
constexpr ColorRGB E_BLACK {0.0f, 0.0f, 0.0f};
constexpr ColorRGB E_WHITE {1.0f, 1.0f, 1.0f};

// ==== ECS components ====

struct E_Transform {
    float x, y, layer;
    float angle;
    float xScale, yScale;
};
struct E_Velocity { float vx = 0, vy = 0; bool freeze = false; };
struct E_Gravity { float a = 9.81; bool work = true; };
struct E_Color { float r, g, b; };
struct E_Texture { GLuint id; };

struct E_Sprite {
    enum Type { CIRCLE, TRIANGLE, RECTANGLE } type;
    float width;
    float height;
    float radius;
    bool visible = true;
};

struct E_Camera {
    float zoom = 1;
    bool active = true;
};

struct SceneEntity { }; // just for tag that entity'll be one scene comp 

// === Input component ===

struct E_InputState {
    double mouseX, mouseY;

    bool leftDown;
    bool rightDown;
    
    bool leftPressed;
    bool leftReleased;
    bool rightPressed;
    bool rightReleased;

    bool _prevLeftDown, _prevRightDown;
    bool keysDown[350];      // is key down
    bool keysPressed[350];   // is key pressed
    bool keysReleased[350];  // is key realesed in this frame

    bool _prevKeysDown[350]; // prev state
};

struct E_Clickable {
    std::function<void()> onClick;

    bool isHovered = false;
    bool isClicked = false;
};

// === Visual Effects ===

struct E_EffectShadow { float offset = 3; bool work = true; };
struct E_EffectOutline { float length = 2; bool work = true; };
struct E_EffectHover { float offsetX = 1.1; float offsetY = 1.1; bool work = true; };
struct E_EffectTranspare { float alpha = 0.9; bool work = true; };

// === Physics & Collision ===

enum class ColliderType {
    Rect,       // AABB
    Circle,     // Circle
    Triangle    //Polygon     // Any form
};

struct E_Collider {
    ColliderType type = ColliderType::Rect;

    float width = 0, height = 0; 
    float radius = 0;

    bool isStatic = false; // If is true, object is frozen (wall)
    bool isTrigger = false;
    bool active = true;

    int layer = 1;
    // offset from Transform
    float offsetX = 0, offsetY = 0;
};

struct E_CollisionEvent {
    flecs::entity a;
    flecs::entity b;
    bool isTrigger;
};

struct E_Mass {
    float mass = 1.0f;
    float invMass = 1.0f; // 1/mass, for static = 0

    E_Mass() : mass(1.0f), invMass(1.0f) {}
    explicit E_Mass(float m) : mass(m), invMass(m > 0.0f ? 1.0f/m : 0.0f) {}
    
    void setStatic() { mass = 0.0f; invMass = 0.0f; }
};

struct E_PhysicsMaterial {
    float restitution = 0.3f;  // 0 = no bounce, 1 = perfect bounce
    float friction = 0.3f;     // [0..1], for future (not in use yet)
};

// Heshing
struct SpatialGrid {
    int cellSize = 128;};

// === addtional stuff ===
// for getting size of window
struct E_WindowSize {int w,h; };
