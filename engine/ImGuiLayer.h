//
//  ImGuiLayer.h rbashkort 02/01/2026
//

#pragma once

#include "../thirdparty/imgui/imgui.h"
#include "../thirdparty/imgui/backends/imgui_impl_glfw.h"
#include "../thirdparty/imgui/backends/imgui_impl_opengl2.h"

class ImGuiLayer {
public:
    bool init(GLFWwindow* window) {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;
        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL2_Init();
        return true;
    }

    void begin() {
        ImGui_ImplOpenGL2_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void end() {
        ImGui::Render();
        ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
    }

    void shutdown() {
        ImGui_ImplOpenGL2_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }
};

