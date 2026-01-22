//
//  engine.h rbashkort 31/12/2025
//

#pragma once

//#include <GL/gl.h>
#include "components.h"
#include <GLFW/glfw3.h>

#include <string>
#include <functional>
#include <flecs.h>
#include <map>

#include "UIManager.h"
#include "entity.h"
#include "ecs_world.h"

#include "TextureManager.h"

struct WindowData {
    GLFWwindow* handle;
    std::string title;
    int width, height;
};

struct Vec2 { float x, y; };

class Engine {
public:
    Engine();
    ~Engine(); 

    bool init(int window_w = 800, int window_h = 600, BackGroundColor back = BackGround_BLACK);
    bool createWindow(const char* title);
    void shutdown();

    // Managers
    TextureManager textureManager;
    UIManager ui;

    // Helpers
    ECSWorld& getECS() { return ecs; }
    GLFWwindow* getWindow() const { return window; }

    void SetDebugMode(bool s) { debugMode = s; }
    bool isDebugMode() const { return debugMode; }

    E_Color ReturnColor(ColorRGB c) {return E_Color{c.r, c.g, c.b};}

    // Entity
    Entity createEntity(const std::string& name = "", bool persistent = false);
    Entity getEntity(const std::string& name);
    template<typename T>
    void registerComponent(const char* name = nullptr) {
        if(name) ecs.getWorld().component<T>(name);
        else ecs.getWorld().component<T>();
    }

    // callbacks
    std::function<void(float)> onUpdate;
    std::function<void()> onRender;
    std::function<void()> onInput;

    // Custom ECS systems 
    template<typename... Components>
    flecs::entity addRenderSystem(std::string name, std::function<void(flecs::entity, Components&...)> func);
    template<typename... Components>
    flecs::entity addUpdateSystem(std::string name, std::function<void(flecs::entity, float, Components&...)> func);

    template<typename... Components>
    auto system(const char* name) {
        return ecs.getWorld().system<Components...>(name);
    }

    auto system(const char* name) {
        return ecs.getWorld().system<>(name);
    }


    bool tick();

    // Input Helpers
    bool isKeyDown(int key);     // is down
    bool isKeyPressed(int key);  // is pressed
    bool isKeyReleased(int key); // is released

    // Mouse Helpers
    bool isMouseButtonDown(int btn); // 0 - Left, 1 - Right
    bool isMouseButtonPressed(int btn); // 0 - Left, 1 - Right

    // scene func
    using SceneInitFunc = std::function<void(Engine&)>;

    void registerScene(const std::string& name, SceneInitFunc initFunc);
    void loadScene(const std::string& name);
    void reloadScene(); // current scene
    bool isCurrentScene(std::string name) { return name == currentSceneName; }


    // window funcs
    GLFWwindow* createExtraWindow(int w, int h, const char* title);
    void window_freezeSize(GLFWwindow* window, int w, int h);
    void window_changeSize(GLFWwindow* window, int w, int h, bool funMode = false);
    Vec2 getScreenSize() { return { (float)window_w, (float)window_h }; }

    // FPS
    void MaxFPS(int maxFPS);
    void SetVSync(bool turnOn);
    float getFPS();
private:
    GLFWwindow* window = nullptr;
    ECSWorld ecs;
    bool debugMode = false;

    std::vector<WindowData> windows; // windows[0] is the main window

    // init
    int window_w = 800;
    int window_h = 600;
    BackGroundColor background_color;

    // Scene Managment
    void _performLoadScene(const std::string& name);

    std::map<std::string, SceneInitFunc> scenes;
    std::string currentSceneName;
    std::string pendingSceneLoad = "";

    float currentDt = 0.0f;
    float targetFrameTime = 0.0f;
    bool VSync = false;

    // functions
    void processInput();
    void update(float dt);
    void render();
};
