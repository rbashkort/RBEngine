cmake_minimum_required(VERSION 3.16)
project(imgui_lib)

set(IMGUI_SOURCES
    imgui.cpp
    imgui_draw.cpp
    imgui_tables.cpp
    imgui_widgets.cpp
    imgui_demo.cpp
    # Backends for GLFW + OpenGL2
    backends/imgui_impl_glfw.cpp
    backends/imgui_impl_opengl2.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})

# Include directories (for #include <imgui.h>)
target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/backends
)

# ImGui need GLFW and OpenGL
find_package(glfw3 REQUIRED)
find_package(OpenGL REQUIRED)

target_link_libraries(imgui PRIVATE glfw OpenGL::GL)

