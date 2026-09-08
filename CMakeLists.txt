cmake_minimum_required(VERSION 3.10)
project(GravitationalParticles CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Подавляет CMake dev-warning про выбор между legacy libGL.so и GLVND
# (libOpenGL.so/libGLX.so) — на современных дистрибутивах (Mesa, NVIDIA)
# GLVND предпочтителен.
set(OpenGL_GL_PREFERENCE GLVND)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 REQUIRED)

add_executable(gravity_particles
    src/main.cpp
    src/Shader.cpp
    src/Framebuffer.cpp
    src/ParticleSystem.cpp
    src/StarField.cpp
)

target_include_directories(gravity_particles PRIVATE include)

target_link_libraries(gravity_particles PRIVATE
    OpenGL::GL
    GLEW::GLEW
    glfw
)

# Копируем шейдеры рядом с исполняемым файлом, чтобы относительные пути работали
# как при сборке в build/, так и при запуске из корня проекта.
add_custom_command(TARGET gravity_particles POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/shaders
            $<TARGET_FILE_DIR:gravity_particles>/shaders
)
