cmake_minimum_required(VERSION 3.16)
project(GravityParticles VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(GP_BUILD_TESTS "Собирать модульные тесты" ON)

# Подавляет CMake dev-warning про выбор между legacy libGL.so и GLVND
# (libOpenGL.so/libGLX.so) — на современных дистрибутивах GLVND предпочтителен.
set(OpenGL_GL_PREFERENCE GLVND)

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(glfw3 3.3 REQUIRED)
find_package(Freetype REQUIRED)
find_package(Threads REQUIRED)

# HarfBuzz: CMake-конфиг (vcpkg, Homebrew) или pkg-config (Linux).
find_package(harfbuzz CONFIG QUIET)
if(TARGET harfbuzz::harfbuzz)
    set(GP_HARFBUZZ harfbuzz::harfbuzz)
else()
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(HARFBUZZ QUIET IMPORTED_TARGET harfbuzz)
    endif()
    if(TARGET PkgConfig::HARFBUZZ)
        set(GP_HARFBUZZ PkgConfig::HARFBUZZ)
    else()
        find_path(HARFBUZZ_INCLUDE_DIR hb.h PATH_SUFFIXES harfbuzz)
        find_library(HARFBUZZ_LIBRARY NAMES harfbuzz)
        if(NOT HARFBUZZ_INCLUDE_DIR OR NOT HARFBUZZ_LIBRARY)
            message(FATAL_ERROR
                "HarfBuzz не найден. Установите libharfbuzz-dev (Debian/Ubuntu), "
                "harfbuzz-devel (Fedora), harfbuzz (Arch/Homebrew/vcpkg).")
        endif()
        add_library(gp_harfbuzz INTERFACE)
        target_include_directories(gp_harfbuzz INTERFACE ${HARFBUZZ_INCLUDE_DIR})
        target_link_libraries(gp_harfbuzz INTERFACE ${HARFBUZZ_LIBRARY})
        set(GP_HARFBUZZ gp_harfbuzz)
    endif()
endif()

configure_file(cmake/Version.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h @ONLY)

if(MSVC)
    set(GP_WARNINGS /W4 /permissive- /utf-8)
else()
    set(GP_WARNINGS -Wall -Wextra -Wpedantic)
endif()

# ---------------------------------------------------------------------------
# gp_core — логика без OpenGL: локализация, настройки, пути, шейпинг текста.
# Её же используют модульные тесты.
# ---------------------------------------------------------------------------
add_library(gp_core STATIC
    src/Json.cpp
    src/I18n.cpp
    src/Settings.cpp
    src/Paths.cpp
    src/TextShaper.cpp
)
target_include_directories(gp_core PUBLIC include ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(gp_core PUBLIC Freetype::Freetype ${GP_HARFBUZZ} Threads::Threads)
target_compile_options(gp_core PRIVATE ${GP_WARNINGS})
# Резервный путь к ресурсам — каталог исходников: программа запустится, даже
# если копия ресурсов рядом с исполняемым файлом неполная или устарела.
target_compile_definitions(gp_core PRIVATE GP_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
if(APPLE)
    target_link_libraries(gp_core PUBLIC "-framework CoreFoundation")
endif()

# ---------------------------------------------------------------------------
# Приложение
# ---------------------------------------------------------------------------
add_executable(gravity_particles
    src/main.cpp
    src/App.cpp
    src/Shader.cpp
    src/Framebuffer.cpp
    src/ParticleSystem.cpp
    src/StarField.cpp
    src/BloomRenderer.cpp
    src/UiRenderer.cpp
    src/Ui.cpp
    src/stb_image_write_impl.cpp
)
target_include_directories(gravity_particles SYSTEM PRIVATE third_party/stb)
target_link_libraries(gravity_particles PRIVATE gp_core OpenGL::GL GLEW::GLEW glfw)
target_compile_options(gravity_particles PRIVATE ${GP_WARNINGS})
# Сторонний однофайловый код компилируется без наших строгих предупреждений.
if(MSVC)
    set_source_files_properties(src/stb_image_write_impl.cpp PROPERTIES COMPILE_OPTIONS "/W0")
else()
    set_source_files_properties(src/stb_image_write_impl.cpp PROPERTIES COMPILE_OPTIONS "-w")
endif()

# Ресурсы копируются рядом с исполняемым файлом: программа запускается
# из любой папки, в т.ч. двойным щелчком. Отдельная цель выполняется при
# каждой сборке — правка одного шейдера или перевода тоже попадает в build/.
if(CMAKE_CONFIGURATION_TYPES)
    set(GP_RUNTIME_DIR ${CMAKE_BINARY_DIR}/$<CONFIG>)   # Visual Studio, Xcode
else()
    set(GP_RUNTIME_DIR ${CMAKE_BINARY_DIR})
endif()
add_custom_target(gp_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/shaders ${GP_RUNTIME_DIR}/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/locales ${GP_RUNTIME_DIR}/locales
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets ${GP_RUNTIME_DIR}/assets
    COMMENT "Copying shaders, locales and fonts"
)
add_dependencies(gravity_particles gp_assets)
set_target_properties(gravity_particles PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

include(GNUInstallDirs)
install(TARGETS gravity_particles RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY shaders locales assets DESTINATION ${CMAKE_INSTALL_DATADIR}/gravity-particles)
if(UNIX AND NOT APPLE)
    install(FILES packaging/gravity-particles.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
endif()

# ---------------------------------------------------------------------------
# Тесты: ctest --test-dir build --output-on-failure
# ---------------------------------------------------------------------------
if(GP_BUILD_TESTS)
    enable_testing()
    add_executable(gp_tests
        tests/test_main.cpp
        tests/test_json.cpp
        tests/test_i18n.cpp
        tests/test_settings.cpp
        tests/test_text.cpp
    )
    target_link_libraries(gp_tests PRIVATE gp_core)
    target_compile_options(gp_tests PRIVATE ${GP_WARNINGS})
    target_compile_definitions(gp_tests PRIVATE GP_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
    add_test(NAME gp_tests COMMAND gp_tests)
endif()
