//
//  engine.cpp rbashkort 31/12/2025
//


#include "engine.h"
#include "components.h"
#include "entity.h"

#include <GLFW/glfw3.h>
#include <cstdio>
#include <flecs.h>
#include <iostream>
#include <chrono>
#include <GL/gl.h>
#include <thread>

Engine::Engine() {}

Engine::~Engine() { shutdown(); }

// ================= Init and shutdown ================= 

bool Engine::init(int w, int h, BackGroundColor c) {
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);

    window_w = w; window_h = h; background_color = c;
    ui.init(window_w, window_h);

    // init ecs
    ecs.init();  // ECS components and base systems
    ecs.getWorld().set<E_WindowSize>({window_w, window_h});

    return true;
}

void Engine::shutdown() {
    if (window) glfwDestroyWindow(window);
    glfwTerminate();
}
// ================= Window ================= 
bool Engine::createWindow(const char* title) {
    if(debugMode) printf("[Engine] Creating window\n");
    window = glfwCreateWindow(window_w, window_h, title, nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    glfwSetWindowUserPointer(window, this);

    glfwSetCursorPosCallback(window, [](GLFWwindow* win, double x, double y) {
        Engine* eng = (Engine*)glfwGetWindowUserPointer(win);
        if (eng) eng->ui.onMouseMove((int)x, (int)y);
    });

    // MOUSE BUTTON
    glfwSetMouseButtonCallback(window, [](GLFWwindow* win, int button, int action, int mods) {
        Engine* eng = (Engine*)glfwGetWindowUserPointer(win);
        if (eng) eng->ui.onMouseButton(button, action);
    });

    // KEYBOARD
    glfwSetKeyCallback(window, [](GLFWwindow* win, int key, int scancode, int action, int mods) {
        Engine* eng = (Engine*)glfwGetWindowUserPointer(win);
        if (eng) eng->ui.onKey(key, action, mods);
    });

    // WINDOW RESIZE
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* win, int w, int h) {
        Engine* eng = (Engine*)glfwGetWindowUserPointer(win);
        if (eng) {
            glViewport(0, 0, w, h);
            eng->ui.onWindowResize(w, h);
        }
    });

    printf("OpenGL version: %s\n", glGetString(GL_VERSION));
    printf("GL_VENDOR: %s\n", glGetString(GL_VENDOR));

    if(debugMode) printf("[Engine] Debug Mode is ON\n");

    return true;
}

void Engine::window_freezeSize(GLFWwindow* win, int w, int h){
    window_changeSize(win, w, h);
    glfwSetWindowSizeLimits(win, w, h, w, h);
    glfwSetWindowAttrib(window, GLFW_RESIZABLE, GLFW_FALSE);
}

void Engine::window_changeSize(GLFWwindow* win, int w, int h, bool funMode) {
    if (!win) win = window; // if null, just change main window
    glfwSetWindowSize(win, w, h);

    // update Ortho
    if (win == window) {
        window_w = w;
        window_h = h;
        ecs.getWorld().set<E_WindowSize>({window_w, window_h});
        // For fun can comment it 
        if(!funMode) glViewport(0, 0, w, h);
    }
}

// ================= FPS =================

void Engine::SetVSync(bool turnOn) {
    VSync = turnOn;
    glfwSwapInterval(turnOn ? 1 : 0);
}

void Engine::MaxFPS(int maxFPS) {
    if (maxFPS <= 0) {
        targetFrameTime = 0.0f; // Without any limits
    } else {
        targetFrameTime = 1.0f / (float)maxFPS;
    }
    // just bc it'll confilt
    if (maxFPS > 0 && VSync) {
        SetVSync(false);
    }
}

float Engine::getFPS() { return 1.f / currentDt; }

// ================= Time and input ================= 

bool Engine::tick() {
    if (glfwWindowShouldClose(window)) return false;

    static auto lastTime = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> delta = now - lastTime;

    // === FPS LIMIT LOGIC ===
    if (targetFrameTime > 0.0f) {
        if (delta.count() < targetFrameTime) {
            // Too early to draw a frame 
            // sleep for optimazing CPU
            float sleepSeconds = targetFrameTime - delta.count();
            if (sleepSeconds > 0.002f) {
                 std::this_thread::sleep_for(std::chrono::duration<float>(sleepSeconds));
            }

            now = std::chrono::high_resolution_clock::now();
            delta = now - lastTime;

            while (delta.count() < targetFrameTime) {
                now = std::chrono::high_resolution_clock::now();
                delta = now - lastTime;
            }
        }
    }
    // ============================

    lastTime = now;
    float dt = delta.count();
    
    // Safe from the big dt(for ex., when move the window)
    if (dt > 0.1f) dt = 0.1f;
    
    currentDt = dt;

    processInput();
    if (onInput) onInput();

    render();

    update(dt);
    if (onUpdate) onUpdate(dt);

    if (onRender) onRender();

    glfwSwapBuffers(window);
    glfwPollEvents();

    // safe reload scene
    if (!pendingSceneLoad.empty()) {
        _performLoadScene(pendingSceneLoad);
        pendingSceneLoad = "";
    }

    return true;
}

void Engine::update(float dt) {
    ecs.update(dt);
}

void Engine::render() {
    glClearColor(background_color.r, background_color.g, background_color.b, background_color.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST); // for the Z layers 
    glDepthFunc(GL_LEQUAL);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, window_w, window_h, 0, -100, 100);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

// ================= Custom ECS systems ================= 

template<typename... Components>
flecs::entity Engine::addRenderSystem(std::string name, std::function<void(flecs::entity, Components&...)> func) {
    return ecs.getWorld().system<Components...>(name.c_str())
        .each(func)
        .term_at(0).moved().add(EcsOnStore);
}

template<typename... Components>
flecs::entity Engine::addUpdateSystem(std::string name, std::function<void(flecs::entity, float, Components&...)> func) {
    return ecs.getWorld().system<Components...>(name.c_str())
        .each([func,name](flecs::entity e, Components&... comps) {
            float dt = e.world().delta_time();
            func(e, dt, comps...);
        });
}

// ================= Entity ================= 

Entity Engine::createEntity(const std::string& name, bool persistent) {
    auto& world = ecs.getWorld();
    flecs::entity raw = name.empty() ? world.entity() : world.entity(name.c_str());
    
    // if persistent is true -> will on the screen in every scene
    if(!persistent) raw.add<SceneEntity>(); 
    
    return Entity(name, raw);
}
Entity Engine::getEntity(const std::string& name) {
    auto e = ecs.getWorld().lookup(name.c_str());
    return Entity(name, e); // Возвращаем обертку
}

// ================= Scenes ================= 

void Engine::registerScene(const std::string& name, SceneInitFunc initFunc) {
    scenes[name] = initFunc;
}

void Engine::loadScene(const std::string& name) {
    if (scenes.find(name) == scenes.end()) {
        std::cerr << "[Engine] Scene not found: " << name << std::endl;
        return;
    }

    // clear scene
    // deleting entities with teg
    ecs.getWorld().delete_with<SceneEntity>(); 

    // init new scene
    //if(currentSceneName == name) { printf("[Engine] Scene already loaded"); return; };
    currentSceneName = name;
    scenes[name](*this);

    pendingSceneLoad = name;
    
    printf("[Engine] Loaded scene: %s\n", name.c_str());
}

void Engine::reloadScene() {
    if (!currentSceneName.empty()) {
        pendingSceneLoad = currentSceneName;
    }
}

void Engine::_performLoadScene(const std::string& name) {
     if (scenes.find(name) == scenes.end()) {
        std::cerr << "[Engine] Scene not found: " << name << std::endl;
        return;
    }

    ecs.getWorld().delete_with<SceneEntity>(); 
    currentSceneName = name;
    scenes[name](*this);
    
    printf("[Engine] Loaded scene: %s\n", name.c_str());
}

// ================= Input ================= 

void Engine::processInput() {
    E_InputState& input = ecs.getWorld().get_mut<E_InputState>(); 

    // --- MOUSE ---
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    bool curLeft = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
    bool curRight = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);

    input.leftPressed = curLeft && !input._prevLeftDown;
    input.leftReleased = !curLeft && input._prevLeftDown;
    input.rightPressed = curRight && !input._prevRightDown;
    input.rightReleased = !curRight && input._prevRightDown;

    input.mouseX = mx;
    input.mouseY = my;
    input.leftDown = curLeft;
    input.rightDown = curRight;
    input._prevLeftDown = curLeft;
    input._prevRightDown = curRight;

    // --- KEYBOARD ---
    // 350 keys its many, but it anyway faster
    for (int key = 0; key < 350; ++key) {
        // for the simple API it'll check every 350 keys its like < 1 microsec

        bool isDown = (glfwGetKey(window, key) == GLFW_PRESS);

        input.keysPressed[key] = isDown && !input._prevKeysDown[key];
        input.keysReleased[key] = !isDown && input._prevKeysDown[key];
        input.keysDown[key] = isDown;

        input._prevKeysDown[key] = isDown;
    }

    if (input.keysPressed[GLFW_KEY_GRAVE_ACCENT]) { 
        // idk what add here
    }
}


//Keyboard
bool Engine::isKeyDown(int key) {
    if (key < 0 || key >= 350) return false;
    if (ecs.getWorld().has<E_InputState>()) {
        return ecs.getWorld().get<E_InputState>().keysDown[key];
    }
    return false;
}

bool Engine::isKeyPressed(int key) {
    if (key < 0 || key >= 350) return false;
    if (ecs.getWorld().has<E_InputState>()) {
        return ecs.getWorld().get<E_InputState>().keysPressed[key];
    }
    return false;
}

bool Engine::isKeyReleased(int key) {
    if (key < 0 || key >= 350) return false;
    if (ecs.getWorld().has<E_InputState>()) {
        return ecs.getWorld().get<E_InputState>().keysReleased[key];
    }
    return false;
}

// Mouse

bool Engine::isMouseButtonDown(int btn)
{
    if(btn < 0 || btn >= 2) return false;
     if (ecs.getWorld().has<E_InputState>()) {
        switch (btn) {
            case 0: return ecs.getWorld().get<E_InputState>().leftDown;
            case 1: return ecs.getWorld().get<E_InputState>().rightDown;
        }
        return ecs.getWorld().get<E_InputState>().leftDown;
    }
    return false;
}
bool Engine::isMouseButtonPressed(int btn)
{
    if(btn < 0 || btn >= 2) return false;
     if (ecs.getWorld().has<E_InputState>()) {
        switch (btn) {
            case 0: return ecs.getWorld().get<E_InputState>().leftPressed;
            case 1: return ecs.getWorld().get<E_InputState>().rightPressed;
        }
        return ecs.getWorld().get<E_InputState>().leftDown;
    }
    return false;
}
