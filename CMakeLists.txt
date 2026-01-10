cmake_minimum_required(VERSION 3.16)
project(RBEngine LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Third-party Paths ---
set(FLECS_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/thirdparty/flecs/include)
# Assuming you have a prebuilt lib or building it manually. 
# Better: include flecs.c/.cpp directly if possible, or use find_library.
set(FLECS_LIB_DIR ${CMAKE_SOURCE_DIR}/thirdparty/flecs/lib)

# --- RmlUi Configuration ---
set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
set(RMLUI_TESTS OFF CACHE BOOL "" FORCE)
set(RMLUI_LUA_BINDINGS OFF CACHE BOOL "" FORCE)
# ---------------------------

# Setup Flecs library name based on architecture if needed
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(FLECS_LIB_NAME "libflecs.so")
else()
    set(FLECS_LIB_NAME "libflecs.so")  
endif()

# Add Subdirectories
add_subdirectory(thirdparty/imgui)
add_subdirectory(thirdparty/RmlUi) 
add_subdirectory(engine)

# Main Executable
add_executable(RBEngine src/main.cpp)

# Include Directories
target_include_directories(RBEngine 
    PRIVATE
        ${CMAKE_SOURCE_DIR}/engine
        ${FLECS_INCLUDE_DIR}
)

# Link Libraries
target_link_libraries(RBEngine 
    PRIVATE
        engine_lib
        ${FLECS_LIB_DIR}/${FLECS_LIB_NAME}
        GL
        glfw
        imgui
        RmlUi::Core      # Link RmlUi Core
        RmlUi::Debugger  # Link RmlUi Debugger
)

file(COPY ${CMAKE_SOURCE_DIR}/assets DESTINATION ${CMAKE_BINARY_DIR})

# (only for linux/macos)
#add_custom_command(TARGET RBEngine POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E create_symlink 
#    ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/assets
#)