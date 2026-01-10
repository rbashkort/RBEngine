//
//  example.cpp rbashkort 31/12/2025
//  There ur code of game

#include <GLFW/glfw3.h>
#define WINDOW_W 800
#define WINDOW_H 600

#include "../engine/engine.h"
#include "../engine/ImGuiLayer.h" // optinal

// example of creating scenes
void SceneMenu(Engine& eng) {
    Entity title = eng.createEntity("Title");
    title.addComponent(E_Transform{200,350,1,0,1,1});
    title.addComponent(E_Sprite{E_Sprite::RECTANGLE, 200, 50, 0});
    title.addComponent(eng.ReturnColor(E_GREEN));
    title.addComponent(E_Clickable{[&](){
        eng.loadScene("Level1");
    }});
    title.addComponent(E_EffectShadow{5.f});
    title.addComponent(E_EffectHover{});
    title.addComponent(E_EffectTranspare{0.7f});
    title.addComponent(E_EffectOutline{1.f});
}

void SceneLevel1(Engine& eng) {
    GLuint tex1 = eng.textureManager.loadTexture("assets/textures/player.png");
    
    Entity player = eng.createEntity("Player");
    player.addComponent(E_Transform{100,100,1,0,1,1});
    player.addComponent(E_Sprite{E_Sprite::RECTANGLE, 75, 200, 0});
    player.addComponent(E_Texture{tex1});
    player.addComponent(E_EffectOutline{});
    player.addComponent(E_EffectShadow{});
    player.addComponent(E_EffectTranspare{0.75});
    //player.addComponent(eng.ReturnColor(E_RED));

    Entity ball = eng.createEntity("Ball");
    ball.addComponent(E_Transform{400,300,1,0,1,1});
    ball.addComponent(E_Sprite{E_Sprite::CIRCLE, 0, 0, 32});
    //ball.addComponent(E_Velocity{10, 0});
    ball.addComponent(eng.ReturnColor(E_BLUE));
    ball.addComponent(E_Clickable{[&](){
        eng.loadScene("Menu");
    }});
    ball.addComponent(E_EffectOutline{0.5f});
    ball.addComponent(E_EffectHover{});
    ball.addComponent(E_EffectTranspare{0.9f});
    ball.addComponent(E_EffectShadow{});
}

// there the main of everything
int main() {
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    // initing engine, create window, can set debug mode
    Engine eng;
    //eng.SetDebugMode(true); //uncomment for engine debug
    
    if (!eng.init(WINDOW_W, WINDOW_H, BackGround_RED)) { printf("[Engine] init ka-bum\n"); return -1; }
    if (!eng.createWindow("RBEngine")) { printf("[Engine] create window ka-bum\n"); return -1; }
    eng.window_freezeSize(eng.getWindow(), WINDOW_W, WINDOW_H);

    //initing ImGUI helper for example for UI debug
    ImGuiLayer gui;
    if(!gui.init(eng.getWindow())) { printf("[ImGuiLayer] GUI init ka-bum\n"); return -1; }
    bool showDebug = false;

    eng.registerScene("Menu", SceneMenu);
    eng.registerScene("Level1", SceneLevel1);

    eng.loadScene("Menu");

    GLuint tex1 = eng.textureManager.loadTexture("assets/textures/player.png");

    // example of custom input
    eng.onInput = [&]() {
        if (eng.isKeyPressed(GLFW_KEY_1)) { if(!eng.isCurrentScene("Menu"))   eng.loadScene("Menu"); }
        if (eng.isKeyPressed(GLFW_KEY_2)) { if(!eng.isCurrentScene("Level1")) eng.loadScene("Level1"); }
        if (eng.isKeyPressed(GLFW_KEY_R)) { eng.reloadScene(); }
        if (eng.isKeyPressed(GLFW_KEY_GRAVE_ACCENT)) { if(showDebug) showDebug = false; else showDebug = true;}
    };

    // custom update
    eng.onUpdate = [&](float dt) {

    };
    
    // cusotm render
    eng.onRender = [&]() {
        // there u can add ui like ImGUI
        if (showDebug) {
            gui.begin();

            ImGui::Begin("Engine Debug");
            ImGui::Text("FPS: %.1f", eng.getFPS());

            if (ImGui::Button("Reload Scene")) {
                eng.reloadScene();
            }

            ImGui::End();

            gui.end();
        }
    };

    // cycle
    while (eng.tick()) {
        //There do anything what u want
    }

    gui.shutdown();
    return 0;
}
