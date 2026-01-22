//
//  example.cpp rbashkort 11/01/2026
//  Showcase of RBEngine features

#include "../engine/engine.h"
#include "../engine/ImGuiLayer.h" 
#include "components.h"
#include <GLFW/glfw3.h>
#include <string>

#define WINDOW_W 1024
#define WINDOW_H 768

// --- Scenes ---

void SceneMenu(Engine &eng){
    for(int i=0; i<250; i++) {
        auto p = eng.createEntity("");
        float r = 1 + rand()%30;
        p.addManyComponents(
            E_Transform{(float)(rand()%WINDOW_W), (float)(rand()%WINDOW_H), 0, 0, 1, 1},
            E_Sprite{E_Sprite::CIRCLE, 0, 0, r},
            E_Color{eng.ReturnColor(E_BLUE)},
            E_Collider{ColliderType::Circle, 0,0,5},
            E_EffectTranspare{0.5f}, // Полупрозрачные
            E_Velocity{(float)(rand()%40 - 20), (float)(rand()%40 - 20)} // Медленно плывут
        );
    }
}

void SceneLevel1(Engine &eng){
    auto Player = eng.createEntity("Player", false);
    
    GLuint texPlayer = eng.textureManager.loadTexture("assets/textures/player.png");
    Player.addManyComponents(
        E_Transform{200, 400, 1, 0, 1, 1},
        E_Sprite{E_Sprite::RECTANGLE, 75, 200, 0},
        E_Texture{texPlayer},
        E_Color{eng.ReturnColor(E_WHITE)},
        E_Camera{1.f, true},
        E_Velocity{0, 0},
        E_Collider{ColliderType::Rect, 75, 200, 0},
        //E_EffectShadow{5.0f},
        //E_EffectOutline{2.0f}
        E_EffectBloom{2}
    );

    // Тестовые препятствия
    auto Box = eng.createEntity("BoxObstacle");
    Box.addManyComponents(
        E_Transform{500, 400, 1, 0, 1, 1},
        E_Sprite{E_Sprite::RECTANGLE, 50, 50, 0},
        E_Color{eng.ReturnColor(E_RED)},
        E_Collider{ColliderType::Rect, 50, 50}, 
        E_EffectHover{1.2f, 1.2f, true},
        E_EffectBloom{2}
    );

    auto Ball = eng.createEntity("BallClickable");
    Ball.addManyComponents(
        E_Transform{700, 300, 1, 0, 1, 1},
        E_Sprite{E_Sprite::CIRCLE, 0, 0, 30},
        E_Color{eng.ReturnColor(E_GREEN)},
        E_Collider{ColliderType::Circle, 0, 0, 30},
        E_Clickable{[&]() {
             printf("Ball clicked! Reloading...\n");
             eng.reloadScene();
        }},
        E_EffectHover{1.1f, 1.1f, false}
    );
}

// --- Main ---

int main(void){
    Engine eng;
    ImGuiLayer gui;
    
    // Init
    if (!eng.init(WINDOW_W, WINDOW_H, BackGround_BLACK)) return -1;
    if (!eng.createWindow("RBEngine v0.1 Showcase")) return -1;
    eng.SetDebugMode(true);

    if (!eng.ui.loadFont("assets/fonts/Arial.ttf")) {
        printf("WARNING: Font not found, UI might look bad.\n");
    }

    if (!gui.init(eng.getWindow())) return -1;

    eng.window_freezeSize(eng.getWindow(), WINDOW_W, WINDOW_H);

    // Register Scenes
    eng.registerScene("Menu", SceneMenu);
    eng.registerScene("Level1", SceneLevel1);

    // Start with Menu
    eng.loadScene("Menu");
    auto* menuDoc = eng.ui.loadDocument("assets/ui/menu.rml");

    // --- UI Event Handler ---
    struct MenuHandler : public Rml::EventListener {
        Engine* e;
        MenuHandler(Engine* e) : e(e) {}
        
        void ProcessEvent(Rml::Event& event) override {
            Rml::String id = event.GetTargetElement()->GetId();
            if (id == "btn_start") {
                e->ui.closeDocument("assets/ui/menu.rml");
                e->loadScene("Level1");
                e->ui.loadDocument("assets/ui/level1.rml");
            } 
            else if (id == "btn_exit") {
                exit(0);
            }
        }
    };
    static MenuHandler menuHandler(&eng);

    if (menuDoc) {
        if (auto btn = menuDoc->GetElementById("btn_start")) 
            btn->AddEventListener("click", &menuHandler);
        if (auto btn = menuDoc->GetElementById("btn_exit")) 
            btn->AddEventListener("click", &menuHandler);
    }

    auto bindMenuEvents = [&]() {
        if (auto doc = eng.ui.getContext()->GetDocument("Main Menu")) { // Имя берется из <title>
             if (auto el = doc->GetElementById("btn_start")) el->AddEventListener("click", &menuHandler);
             if (auto el = doc->GetElementById("btn_exit")) el->AddEventListener("click", &menuHandler);
        }
    };
    bindMenuEvents();

    // --- Game Variables ---
    float player_speed = 200.0f;
    int player_hp = 200;
    bool gameRunning = false;

    // --- Input & Update ---
    
    eng.onInput = [&]() {
        // Player control logic (only if we are in the game)
        if (eng.isCurrentScene("Level1")) {
            auto player = eng.getEntity("Player"); // Looking for player
            E_Camera* camera;
            if (player.is_valid()) {
                auto v = player.getComponentMut<E_Velocity>();
                if(player.hasComponent<E_Camera>()) camera = player.getComponentMut<E_Camera>();
                if (v) {
                    v->vx = 0; v->vy = 0; // speed reduction
                    if(eng.isKeyDown(GLFW_KEY_W)) v->vy = -player_speed;
                    if(eng.isKeyDown(GLFW_KEY_S)) v->vy =  player_speed;
                    if(eng.isKeyDown(GLFW_KEY_A)) v->vx = -player_speed;
                    if(eng.isKeyDown(GLFW_KEY_D)) v->vx =  player_speed;
                }
                if(camera) {
                    if(eng.isKeyPressed(GLFW_KEY_1)) {if(camera->active) { camera->active = false; } else { camera->active = false; } };
                }
                if(eng.isKeyPressed(GLFW_KEY_R)) eng.reloadScene();
            }
        }
    };

    eng.onUpdate = [&](float dt) {
        eng.ui.update();
        if (eng.isCurrentScene("Level1")) {
            if (auto doc = eng.ui.getContext()->GetDocument("level1.rml")) {
                 if(auto fill = doc->GetElementById("health-fill"))
                     fill->SetProperty("width", std::to_string(player_hp) + "%");
                 
                 if(auto score = doc->GetElementById("score-val"))
                     score->SetInnerRML(std::to_string((int)eng.getFPS()));
            }
        } else {
             bindMenuEvents();
        }
    };

    eng.onRender = [&]() {
        eng.ui.render();
        gui.begin();
        if(eng.isDebugMode()) {
            ImGui::Begin("Debug");
            ImGui::Text("FPS: %.1f", eng.getFPS());
            ImGui::Text("Entities: %d", eng.getECS().getWorld().count<SceneEntity>());
            ImGui::End();
        }
        gui.end();
    };

    // --- Loop ---
    while (eng.tick()) {
        // Engine ticks...
    }

    gui.shutdown();
    eng.shutdown();
    return 0;
}
