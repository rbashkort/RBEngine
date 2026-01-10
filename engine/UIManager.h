//
//  UIManager.h rbashkort 01/09/2026
//

#pragma once
#include <RmlUi/Core.h>
#include "ui/RBSystemInterface.h"
#include "ui/RBRenderInterface.h"
#include <string>

class UIManager {
public:
    bool init(int w, int h);
    void shutdown();
    
    void update();
    void render();

    Rml::ElementDocument* loadDocument(const std::string& path);
    bool loadFont(const std::string& path, bool isFallback = false);
    
    void closeDocument(const std::string& path);

    void onMouseMove(int x, int y);
    void onMouseButton(int button, int action); // action: 1=press, 0=release
    void onKey(int key, int action, int mods);
    void onWindowResize(int w, int h);

    Rml::Context* getContext() { return context; }

private:
    Rml::Context* context = nullptr;
    RBSystemInterface systemInterface;
    RBRenderInterface renderInterface;
    
    Rml::Input::KeyIdentifier convertKey(int glfwKey);
    int getKeyModifierState(int glfwMods);
};
