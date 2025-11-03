cmake_minimum_required(VERSION 3.15)
project(ecosystem)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)

include(FetchContent)
FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
)
FetchContent_MakeAvailable(glfw)

FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.2
)
FetchContent_MakeAvailable(glm)

FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.92.4
)
FetchContent_MakeAvailable(imgui)

# Create ImGui library
add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui PUBLIC
        glfw
        OpenGL::GL
)

# Main executable
add_executable(${PROJECT_NAME}
        src/main.cpp
        src/simulation.cpp
        src/behavior.cpp
        src/behavior.h
        src/entity.cpp
        src/entity.h
        src/species.cpp
        src/species.h
        src/simulation.h
        src/utils.h
        src/utils.cpp
        src/gl_utils.cpp
        src/gl_utils.h
        src/app.cpp
        src/app.h
)

add_library(glad STATIC ${CMAKE_SOURCE_DIR}/external/glad/src/gl.c)
target_include_directories(glad PUBLIC ${CMAKE_SOURCE_DIR}/external/glad/include)

target_link_libraries(${PROJECT_NAME} PRIVATE
        imgui
        glfw
        glm
        glad
        OpenGL::GL
)

# Platform-specific settings
if (WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_IMPL_OPENGL_LOADER_GLAD)
elseif (UNIX AND NOT APPLE)
    target_link_libraries(${PROJECT_NAME} PRIVATE dl pthread)
endif ()