//
//  quickstart.cpp rbashkort 06/01/2026
//

#include "../engine/engine.h"
#include "../engine/ImGuiLayer.h" // optional for ImGUI

#define Window_w 1024
#define Window_h 768
#define Texture GLuint


void Menu(Engine &eng){
    // here ur scene
}

int main (void){
    Engine eng;
    ImGuiLayer gui;
    if (!eng.init(Window_w, Window_h, BackGround_BLACK)) {return -1;}
    if (!eng.createWindow("RBEngine - quickstart")) {return -1;}

    if (!gui.init(eng.getWindow())) { return -1; };

    eng.window_freezeSize(eng.getWindow(), Window_w, Window_h);

    eng.registerScene("Menu", Menu);
    eng.loadScene("Menu");

    eng.onInput  = [&]() { };
    eng.onRender = [&]() { };
    eng.onUpdate = [&](float dt) { };

    while (eng.tick()) { };

    gui.shutdown();
    return 0;
}
