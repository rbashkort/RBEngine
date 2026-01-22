# engine/CMakeLists.txt


add_library(engine_lib STATIC
    engine.cpp
    ecs_world.cpp
    entity.cpp
    TextureManager.cpp 
    UIManager.cpp
    glad.c

    ImGuiLayer.h
)

target_include_directories(engine_lib
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${FLECS_INCLUDE_DIR}
)

find_package(glfw3 REQUIRED)

target_link_libraries(engine_lib
    PUBLIC
        glfw
        GL
        glm
        RmlUi::Core
        RmlUi::Debugger
    PRIVATE
        ${FLECS_LIB_DIR}/${FLECS_LIB_NAME}
)
