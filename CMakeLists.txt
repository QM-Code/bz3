cmake_minimum_required(VERSION 3.20)

set(_VCPKG_TOOLCHAIN "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake")
if (EXISTS "${_VCPKG_TOOLCHAIN}")
    set(CMAKE_TOOLCHAIN_FILE "${_VCPKG_TOOLCHAIN}" CACHE FILEPATH "vcpkg toolchain" FORCE)
else()
    message(STATUS "vcpkg toolchain not found at ${_VCPKG_TOOLCHAIN}; configuring without vcpkg")
endif()

project(bz3)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BZ3_UI_BACKEND "imgui" CACHE STRING "UI backend (imgui or rmlui)")
set_property(CACHE BZ3_UI_BACKEND PROPERTY STRINGS imgui rmlui)
if(BZ3_UI_BACKEND STREQUAL "imgui")
    add_compile_definitions(BZ3_UI_BACKEND_IMGUI)
elseif(BZ3_UI_BACKEND STREQUAL "rmlui")
    add_compile_definitions(BZ3_UI_BACKEND_RMLUI)
else()
    message(FATAL_ERROR "Unknown BZ3_UI_BACKEND value: ${BZ3_UI_BACKEND}")
endif()

set(BZ3_WINDOW_BACKEND "glfw" CACHE STRING "Window backend (glfw or sdl)")
set_property(CACHE BZ3_WINDOW_BACKEND PROPERTY STRINGS glfw sdl)
if(BZ3_WINDOW_BACKEND STREQUAL "glfw")
    add_compile_definitions(BZ3_WINDOW_BACKEND_GLFW)
    find_package(glfw3 REQUIRED)
    if(TARGET glfw3::glfw)
        set(WINDOW_TARGET glfw3::glfw)
    else()
        set(WINDOW_TARGET glfw)
    endif()
elseif(BZ3_WINDOW_BACKEND STREQUAL "sdl")
    add_compile_definitions(BZ3_WINDOW_BACKEND_SDL)
    set(WINDOW_TARGET SDL3::SDL3)
else()
    message(FATAL_ERROR "Unknown BZ3_WINDOW_BACKEND value: ${BZ3_WINDOW_BACKEND}")
endif()

set(BZ3_PHYSICS_BACKEND "jolt" CACHE STRING "Physics backend (jolt or bullet)")
set_property(CACHE BZ3_PHYSICS_BACKEND PROPERTY STRINGS jolt bullet)
if(BZ3_PHYSICS_BACKEND STREQUAL "jolt")
    add_compile_definitions(BZ3_PHYSICS_BACKEND_JOLT)
elseif(BZ3_PHYSICS_BACKEND STREQUAL "bullet")
    add_compile_definitions(BZ3_PHYSICS_BACKEND_BULLET)
else()
    message(FATAL_ERROR "Unknown BZ3_PHYSICS_BACKEND value: ${BZ3_PHYSICS_BACKEND}")
endif()

set(BZ3_AUDIO_BACKEND "sdlaudio" CACHE STRING "Audio backend (miniaudio or sdlaudio)")
set_property(CACHE BZ3_AUDIO_BACKEND PROPERTY STRINGS miniaudio sdlaudio)
if(BZ3_AUDIO_BACKEND STREQUAL "miniaudio")
    add_compile_definitions(BZ3_AUDIO_BACKEND_MINIAUDIO)
elseif(BZ3_AUDIO_BACKEND STREQUAL "sdlaudio")
    add_compile_definitions(BZ3_AUDIO_BACKEND_SDL)
else()
    message(FATAL_ERROR "Unknown BZ3_AUDIO_BACKEND value: ${BZ3_AUDIO_BACKEND}")
endif()

set(BZ3_NETWORK_BACKEND "enet" CACHE STRING "Network backend (enet)")
set_property(CACHE BZ3_NETWORK_BACKEND PROPERTY STRINGS enet)
if(BZ3_NETWORK_BACKEND STREQUAL "enet")
    add_compile_definitions(BZ3_NETWORK_BACKEND_ENET)
else()
    message(FATAL_ERROR "Unknown BZ3_NETWORK_BACKEND value: ${BZ3_NETWORK_BACKEND}")
endif()

set(BZ3_WORLD_BACKEND "fs" CACHE STRING "World backend (fs)")
set_property(CACHE BZ3_WORLD_BACKEND PROPERTY STRINGS fs)
if(BZ3_WORLD_BACKEND STREQUAL "fs")
    add_compile_definitions(BZ3_WORLD_BACKEND_FS)
else()
    message(FATAL_ERROR "Unknown BZ3_WORLD_BACKEND value: ${BZ3_WORLD_BACKEND}")
endif()

set(BZ3_RENDER_BACKEND "threepp" CACHE STRING "Render backend (threepp)")
set_property(CACHE BZ3_RENDER_BACKEND PROPERTY STRINGS threepp)
if(BZ3_RENDER_BACKEND STREQUAL "threepp")
    add_compile_definitions(BZ3_RENDER_BACKEND_THREEPP)
else()
    message(FATAL_ERROR "Unknown BZ3_RENDER_BACKEND value: ${BZ3_RENDER_BACKEND}")
endif()

set(CMAKE_BUILD_TYPE Release)
set(BUILD_SHARED_LIBS OFF)

include(FetchContent)

find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

FetchContent_Declare(
    pybind11
    GIT_REPOSITORY https://github.com/pybind/pybind11.git
    GIT_TAG v2.11.1
)
FetchContent_MakeAvailable(pybind11)

FetchContent_Declare(
    threepp
    GIT_REPOSITORY https://github.com/markaren/threepp.git
    GIT_TAG master
)
set(THREEPP_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(THREEPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(THREEPP_WITH_AUDIO OFF CACHE BOOL "" FORCE)
set(THREEPP_USE_EXTERNAL_GLFW ON CACHE BOOL "" FORCE)
set(THREEPP_FETCH_ASSIMP OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(threepp)

FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/zpl-c/enet.git
    GIT_TAG master
)
set(ENET_TEST OFF CACHE BOOL "" FORCE)
set(ENET_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(enet)

# Effekseer (embedded copy under libs/effekseer)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/libs/effekseer/cmake)
include(FilterFolder)
set(BUILD_EXAMPLES OFF CACHE BOOL "Disable Effekseer examples" FORCE)
set(BUILD_VIEWER OFF CACHE BOOL "Disable Effekseer viewer" FORCE)
set(BUILD_EDITOR OFF CACHE BOOL "Disable Effekseer editor" FORCE)
set(BUILD_TEST OFF CACHE BOOL "Disable Effekseer tests" FORCE)
set(BUILD_GL ON CACHE BOOL "Enable Effekseer OpenGL renderer" FORCE)
set(BUILD_VULKAN OFF CACHE BOOL "Disable Effekseer Vulkan backend" FORCE)
set(USE_OPENAL OFF CACHE BOOL "Prefer host audio path instead of OpenAL" FORCE)
add_subdirectory(${PROJECT_SOURCE_DIR}/libs/effekseer/src)


# Find external packages
find_package(glm REQUIRED)
if(BZ3_PHYSICS_BACKEND STREQUAL "jolt")
    find_package(Jolt REQUIRED)
elseif(BZ3_PHYSICS_BACKEND STREQUAL "bullet")
    find_package(Bullet CONFIG REQUIRED)
endif()
find_package(assimp REQUIRED)
find_package(OpenGL REQUIRED)
find_package(cxxopts REQUIRED)
if(BZ3_UI_BACKEND STREQUAL "imgui")
    find_package(imgui REQUIRED)
elseif(BZ3_UI_BACKEND STREQUAL "rmlui")
    find_package(RmlUi CONFIG REQUIRED)
    find_package(md4c REQUIRED)
endif()
if(BZ3_WINDOW_BACKEND STREQUAL "sdl" OR BZ3_AUDIO_BACKEND STREQUAL "sdlaudio")
    find_package(SDL3 CONFIG REQUIRED)
endif()
find_package(spdlog REQUIRED)
find_package(miniz REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Protobuf REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)

# Compile protobuf files
set(PROTO_FILES ${PROJECT_SOURCE_DIR}/src/game/protos/messages.proto)
set(PROTO_OUTPUT_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_OUTPUT_DIR})

# Generate protobuf sources
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# Create a library for proto files
add_library(proto_lib STATIC ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(proto_lib PUBLIC protobuf::libprotobuf)
target_include_directories(proto_lib PUBLIC ${CMAKE_BINARY_DIR})

# Gather sources
file(GLOB_RECURSE CLIENT_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/game/client/*.cpp")
list(APPEND CLIENT_SOURCES
    ${PROJECT_SOURCE_DIR}/src/game/client/actor.cpp
    ${PROJECT_SOURCE_DIR}/src/game/client/server/community_browser_controller.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/curl_global.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/config_helpers.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/data_path_resolver.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/data_dir_override.cpp
)
file(GLOB_RECURSE SERVER_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/game/server/*.cpp")
list(APPEND SERVER_SOURCES
    ${PROJECT_SOURCE_DIR}/src/game/server/terminal_commands.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/server_cli_options.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/curl_global.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/config_helpers.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/data_path_resolver.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/data_dir_override.cpp
)
## Separate engine sources for client and server to avoid over-linking
set(ENGINE_CLIENT_SOURCES
    ${PROJECT_SOURCE_DIR}/src/game/engine/client_engine.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/geometry/mesh_loader.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/world/content.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/world/backend_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/world/backends/fs/backend.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/network/enet_transport.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/network/transport_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/client_network.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/server_network.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/backend_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/proto_codec.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/backends/enet/client_backend.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/backends/enet/server_backend.cpp
    ${PROJECT_SOURCE_DIR}/src/game/world/config.cpp
    ${PROJECT_SOURCE_DIR}/src/game/renderer/render.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/graphics/device.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/graphics/backend_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/input/input.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/input/mapping/binding.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/input/mapping/map.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/input/mapping/mapper.cpp
    ${PROJECT_SOURCE_DIR}/src/game/input/bindings.cpp
    ${PROJECT_SOURCE_DIR}/src/game/input/state.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/platform/window_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/system.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/backend_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/third_party/stb_image_impl.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/audio/audio.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/audio/backend_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/game/renderer/particle_effect_system.cpp
)
if(BZ3_WINDOW_BACKEND STREQUAL "glfw")
    list(APPEND ENGINE_CLIENT_SOURCES ${PROJECT_SOURCE_DIR}/src/engine/platform/backends/window_glfw.cpp)
elseif(BZ3_WINDOW_BACKEND STREQUAL "sdl")
    list(APPEND ENGINE_CLIENT_SOURCES ${PROJECT_SOURCE_DIR}/src/engine/platform/backends/window_sdl.cpp)
endif()


if(BZ3_UI_BACKEND STREQUAL "imgui")
    list(APPEND ENGINE_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/imgui/backend.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/imgui/console/console.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/imgui/console/thumbnail_cache.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/imgui/console/panels/panel_community.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/imgui/console/panels/panel_settings.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/imgui/console/panels/panel_documentation.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/imgui/console/panels/panel_start_server.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/imgui/console/panels/panel_themes.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/imgui/hud/hud.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/imgui/hud/chat.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/imgui/hud/crosshair.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/imgui/hud/fps.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/imgui/hud/radar.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/imgui/hud/scoreboard.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/imgui/hud/spawn_hint.cpp
    )
elseif(BZ3_UI_BACKEND STREQUAL "rmlui")
    list(APPEND ENGINE_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/rmlui/backend.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/rmlui/platform/RmlUi_Renderer_GL3.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/rmlui/console/emoji_utils.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/rmlui/console/console.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/rmlui/console/modal_dialog.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/rmlui/hud/hud.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/rmlui/hud/chat.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/rmlui/hud/dialog.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/rmlui/hud/radar.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/rmlui/hud/scoreboard.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/rmlui/console/panels/panel.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/rmlui/console/panels/panel_community.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/rmlui/console/panels/panel_settings.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/rmlui/console/panels/panel_documentation.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/rmlui/console/panels/panel_start_server.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/backends/rmlui/console/panels/panel_themes.cpp
    )
endif()
set(ENGINE_SERVER_SOURCES
    ${PROJECT_SOURCE_DIR}/src/game/engine/server_engine.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/geometry/mesh_loader.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/world/content.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/world/backend_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/world/backends/fs/backend.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/network/enet_transport.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/network/transport_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/client_network.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/server_network.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/backend_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/proto_codec.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/backends/enet/client_backend.cpp
    ${PROJECT_SOURCE_DIR}/src/game/net/backends/enet/server_backend.cpp
    ${PROJECT_SOURCE_DIR}/src/game/world/config.cpp
)

set(PHYSICS_SOURCES
    ${PROJECT_SOURCE_DIR}/src/engine/physics/backend_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/physics/physics_world.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/physics/rigid_body.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/physics/static_body.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/physics/player_controller.cpp
)
if(BZ3_PHYSICS_BACKEND STREQUAL "jolt")
    list(APPEND PHYSICS_SOURCES
        ${PROJECT_SOURCE_DIR}/src/engine/physics/backends/jolt/physics_world_jolt.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/physics/backends/jolt/rigid_body_jolt.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/physics/backends/jolt/static_body_jolt.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/physics/backends/jolt/player_controller_jolt.cpp
    )
elseif(BZ3_PHYSICS_BACKEND STREQUAL "bullet")
    list(APPEND PHYSICS_SOURCES
        ${PROJECT_SOURCE_DIR}/src/engine/physics/backends/bullet/physics_world_bullet.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/physics/backends/bullet/rigid_body_bullet.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/physics/backends/bullet/static_body_bullet.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/physics/backends/bullet/player_controller_bullet.cpp
    )
endif()

list(APPEND ENGINE_CLIENT_SOURCES ${PHYSICS_SOURCES})
list(APPEND ENGINE_SERVER_SOURCES ${PHYSICS_SOURCES})

if(BZ3_AUDIO_BACKEND STREQUAL "miniaudio")
    list(APPEND ENGINE_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/engine/audio/backends/miniaudio/backend.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/audio/backends/miniaudio/clip.cpp
    )
elseif(BZ3_AUDIO_BACKEND STREQUAL "sdlaudio")
    list(APPEND ENGINE_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/engine/audio/backends/sdl/backend.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/audio/backends/sdl/clip.cpp
    )
endif()

if(BZ3_RENDER_BACKEND STREQUAL "threepp")
    list(APPEND ENGINE_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/engine/graphics/backends/threepp/backend.cpp
    )
endif()

# Create executables
set(MACOSX_BUNDLE_ICON_FILE "") # Set to your .icns file if you have one
add_executable(bz3 MACOSX_BUNDLE ${CLIENT_SOURCES} ${ENGINE_CLIENT_SOURCES})


# Handle OpenGL target for portability
if(TARGET OpenGL::GL)
    set(OPENGL_TARGET OpenGL::GL)
elseif(TARGET OpenGL::OpenGL)
    set(OPENGL_TARGET OpenGL::OpenGL)
else()
    set(OPENGL_TARGET ${OPENGL_LIBRARIES})
endif()
if(BZ3_WINDOW_BACKEND STREQUAL "glfw")
    list(APPEND ENGINE_CLIENT_SOURCES ${PROJECT_SOURCE_DIR}/src/engine/platform/backends/window_glfw.cpp)
elseif(BZ3_WINDOW_BACKEND STREQUAL "sdl")
    list(APPEND ENGINE_CLIENT_SOURCES ${PROJECT_SOURCE_DIR}/src/engine/platform/backends/window_sdl.cpp)
endif()


if(BZ3_UI_BACKEND STREQUAL "imgui")
    find_package(Freetype REQUIRED)
    find_package(harfbuzz REQUIRED)
endif()
if(UNIX AND NOT APPLE)
    find_package(Fontconfig REQUIRED)
endif()
find_package(PkgConfig REQUIRED)
if(BZ3_UI_BACKEND STREQUAL "imgui")
    pkg_check_modules(LIBRSVG REQUIRED IMPORTED_TARGET librsvg-2.0)
endif()

add_executable(bz3-server ${SERVER_SOURCES} ${ENGINE_SERVER_SOURCES})

# Include directories
target_include_directories(bz3 PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/engine
    ${PROJECT_SOURCE_DIR}/src/game
    ${enet_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/libs/effekseer/src
    ${PROJECT_SOURCE_DIR}/libs/effekseer/src/Effekseer
)

if(DEFINED threepp_SOURCE_DIR)
    target_include_directories(bz3 PRIVATE ${threepp_SOURCE_DIR}/src/external/glad)
endif()
target_include_directories(bz3-server PRIVATE
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/engine
    ${PROJECT_SOURCE_DIR}/src/game
    ${enet_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(bz3 PRIVATE
    ${WINDOW_TARGET}
    glm::glm
    enet_static
    assimp::assimp
    ${OPENGL_TARGET}
    threepp::threepp
    cxxopts::cxxopts
    spdlog::spdlog
    miniz::miniz
    nlohmann_json::nlohmann_json
    proto_lib
    CURL::libcurl
    OpenSSL::Crypto
    Effekseer
    EffekseerRendererGL
)
if(BZ3_PHYSICS_BACKEND STREQUAL "jolt")
    target_link_libraries(bz3 PRIVATE Jolt::Jolt)
elseif(BZ3_PHYSICS_BACKEND STREQUAL "bullet")
    if(TARGET Bullet::Bullet)
        target_link_libraries(bz3 PRIVATE Bullet::Bullet)
    elseif(DEFINED BULLET_LIBRARIES)
        target_link_libraries(bz3 PRIVATE ${BULLET_LIBRARIES})
    endif()
    if(DEFINED BULLET_INCLUDE_DIRS)
        target_include_directories(bz3 PRIVATE ${BULLET_INCLUDE_DIRS})
    endif()
endif()
if(BZ3_UI_BACKEND STREQUAL "imgui")
    target_link_libraries(bz3 PRIVATE
        imgui::imgui
        Freetype::Freetype
        harfbuzz::harfbuzz
        PkgConfig::LIBRSVG
    )
    target_compile_definitions(bz3 PRIVATE BZ3_HAS_LIBRSVG IMGUI_USE_WCHAR32)
    if(UNIX AND NOT APPLE)
        target_link_libraries(bz3 PRIVATE Fontconfig::Fontconfig)
    endif()
    # ImGui: enable full Unicode range (required for emoji codepoints > U+FFFF)
    if(TARGET imgui::imgui)
        target_compile_definitions(imgui::imgui INTERFACE IMGUI_USE_WCHAR32)
    endif()
endif()

if(BZ3_UI_BACKEND STREQUAL "rmlui")
    target_link_libraries(bz3 PRIVATE RmlUi::RmlUi md4c::md4c)
    target_compile_definitions(bz3 PRIVATE RMLUI_GL3_CUSTOM_LOADER)
endif()

if(BZ3_AUDIO_BACKEND STREQUAL "sdlaudio")
    target_link_libraries(bz3 PRIVATE SDL3::SDL3)
endif()

target_link_libraries(bz3-server PRIVATE
    glm::glm
    enet_static
    assimp::assimp
    pybind11::embed
    cxxopts::cxxopts
    spdlog::spdlog
    miniz::miniz
    nlohmann_json::nlohmann_json
    proto_lib
    CURL::libcurl
)
if(BZ3_PHYSICS_BACKEND STREQUAL "jolt")
    target_link_libraries(bz3-server PRIVATE Jolt::Jolt)
elseif(BZ3_PHYSICS_BACKEND STREQUAL "bullet")
    if(TARGET Bullet::Bullet)
        target_link_libraries(bz3-server PRIVATE Bullet::Bullet)
    elseif(DEFINED BULLET_LIBRARIES)
        target_link_libraries(bz3-server PRIVATE ${BULLET_LIBRARIES})
    endif()
    if(DEFINED BULLET_INCLUDE_DIRS)
        target_include_directories(bz3-server PRIVATE ${BULLET_INCLUDE_DIRS})
    endif()
endif()

# System libraries (platform-specific)
if (UNIX AND NOT APPLE)
    target_link_libraries(bz3 PRIVATE pthread dl m)
    target_link_libraries(bz3-server PRIVATE pthread dl m)
elseif(APPLE)
    find_library(COCOA Cocoa)
    find_library(IOKIT IOKit)
    find_library(COREVIDEO CoreVideo)
    target_link_libraries(bz3 PRIVATE ${COCOA} ${IOKIT} ${COREVIDEO})
    target_link_libraries(bz3-server PRIVATE ${COCOA})
elseif(WIN32)
    target_link_libraries(bz3 PRIVATE ws2_32)
    target_link_libraries(bz3-server PRIVATE ws2_32)
endif()

# Define install prefix for runtime data lookup
target_compile_definitions(bz3 PRIVATE
    INSTALL_DATA_DIR="${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}/data"
)
target_compile_definitions(bz3-server PRIVATE
    INSTALL_DATA_DIR="${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}/data"
)

install(TARGETS bz3 BUNDLE DESTINATION .)
install(TARGETS bz3-server
    RUNTIME DESTINATION bin
)

# Install data folder
install(DIRECTORY ${PROJECT_SOURCE_DIR}/data/
    DESTINATION share/${PROJECT_NAME}/data
)

# CPack configuration
set(CPACK_PACKAGE_NAME "bz3")
set(CPACK_PACKAGE_VERSION "1.0.0")
set(CPACK_PACKAGE_VENDOR "Quinn Carmack")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "BZFlag remade")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "bz3")

# Linux-specific generators
if(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Quinn Carmack <quinncarmack@gmail.com>")
    if(BZ3_WINDOW_BACKEND STREQUAL "glfw")
        set(CPACK_DEBIAN_PACKAGE_DEPENDS "libgl1, libglfw3")
    else()
        set(CPACK_DEBIAN_PACKAGE_DEPENDS "libgl1, libsdl3")
    endif()
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
elseif(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
endif()

include(CPack)
