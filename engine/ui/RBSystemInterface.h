//
//  RBSystemInterface.h rbashkort 01/10/2026
//

#pragma once
#include <RmlUi/Core/SystemInterface.h>
#include <GLFW/glfw3.h>
#include <iostream>

class RBSystemInterface : public Rml::SystemInterface {
public:
    double GetElapsedTime() override {
        return glfwGetTime();
    }

    bool LogMessage(Rml::Log::Type type, const Rml::String& message) override {
        if(type == Rml::Log::LT_ERROR || type == Rml::Log::LT_WARNING)
            std::cout << "[RmlUi] " << message << std::endl;
        return true;
    }
};
