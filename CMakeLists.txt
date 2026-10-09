cmake_minimum_required(VERSION 3.10)
project(VolcanoX CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(VolcanoX
    src/main.cpp
    src/Shader.cpp
    src/Geometry.cpp
    src/SceneObjects.cpp
    src/NewObjects.cpp
    src/Environment.cpp
    src/Texture.cpp
    src/Effects.cpp
)

target_link_libraries(VolcanoX PRIVATE glfw GLEW::GLEW OpenGL::GL)

# Shaders are loaded from disk at runtime relative to the working directory,
# so make sure they sit next to the built executable.
add_custom_command(TARGET VolcanoX POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:VolcanoX>/shaders
)
