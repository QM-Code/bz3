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
enable_language(ASM)

set(THE_FORGE_DIR "${PROJECT_SOURCE_DIR}/third_party/the-forge" CACHE PATH "The Forge root directory")
if(EXISTS "${THE_FORGE_DIR}/Common_3")
    add_library(the_forge_headers INTERFACE)
    target_include_directories(the_forge_headers INTERFACE "${THE_FORGE_DIR}")
else()
    message(STATUS "The Forge not found at ${THE_FORGE_DIR}; clone it to enable Forge integration.")
endif()

set(BZ3_UI_BACKEND "imgui" CACHE STRING "UI backend (imgui or rmlui)")
set_property(CACHE BZ3_UI_BACKEND PROPERTY STRINGS imgui rmlui)
if(BZ3_UI_BACKEND STREQUAL "imgui")
    add_compile_definitions(BZ3_UI_BACKEND_IMGUI)
elseif(BZ3_UI_BACKEND STREQUAL "rmlui")
    add_compile_definitions(BZ3_UI_BACKEND_RMLUI)
else()
    message(FATAL_ERROR "Unknown BZ3_UI_BACKEND value: ${BZ3_UI_BACKEND}")
endif()

set(BZ3_WINDOW_BACKEND "sdl3" CACHE STRING "Window backend (sdl3 or sdl2)")
set_property(CACHE BZ3_WINDOW_BACKEND PROPERTY STRINGS sdl3 sdl2)
if(BZ3_WINDOW_BACKEND STREQUAL "sdl3")
    add_compile_definitions(BZ3_WINDOW_BACKEND_SDL3)
    set(WINDOW_TARGET SDL3::SDL3)
elseif(BZ3_WINDOW_BACKEND STREQUAL "sdl2")
    add_compile_definitions(BZ3_WINDOW_BACKEND_SDL2)
    set(WINDOW_TARGET "")
else()
    message(FATAL_ERROR "Unknown BZ3_WINDOW_BACKEND value: ${BZ3_WINDOW_BACKEND}")
endif()

set(BZ3_PHYSICS_BACKEND "jolt" CACHE STRING "Physics backend (jolt or bullet or physx)")
set_property(CACHE BZ3_PHYSICS_BACKEND PROPERTY STRINGS jolt bullet physx)
if(BZ3_PHYSICS_BACKEND STREQUAL "jolt")
    add_compile_definitions(BZ3_PHYSICS_BACKEND_JOLT)
elseif(BZ3_PHYSICS_BACKEND STREQUAL "bullet")
    add_compile_definitions(BZ3_PHYSICS_BACKEND_BULLET)
elseif(BZ3_PHYSICS_BACKEND STREQUAL "physx")
    add_compile_definitions(BZ3_PHYSICS_BACKEND_PHYSX)
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

set(BZ3_RENDER_BACKEND "bgfx" CACHE STRING "Render backend (diligent|bgfx|forge)")
set_property(CACHE BZ3_RENDER_BACKEND PROPERTY STRINGS diligent bgfx forge)
if(BZ3_RENDER_BACKEND STREQUAL "diligent")
    add_compile_definitions(BZ3_RENDER_BACKEND_DILIGENT)
elseif(BZ3_RENDER_BACKEND STREQUAL "bgfx")
    add_compile_definitions(BZ3_RENDER_BACKEND_BGFX)
elseif(BZ3_RENDER_BACKEND STREQUAL "forge")
    add_compile_definitions(BZ3_RENDER_BACKEND_FORGE)
else()
    message(FATAL_ERROR "Unknown BZ3_RENDER_BACKEND value: ${BZ3_RENDER_BACKEND}")
endif()

if(BZ3_RENDER_BACKEND STREQUAL "forge")
    if(NOT EXISTS "${THE_FORGE_DIR}/Common_3")
        message(FATAL_ERROR "The Forge not found at ${THE_FORGE_DIR}. Clone https://github.com/ConfettiFX/The-Forge.git into third_party/the-forge.")
    endif()
    set(THE_FORGE_GRAPHICS_SOURCES
        ${THE_FORGE_DIR}/Common_3/Graphics/GraphicsConfig.cpp
        ${THE_FORGE_DIR}/Common_3/Graphics/Vulkan/Vulkan.c
        ${THE_FORGE_DIR}/Common_3/Graphics/Vulkan/Vulkan_cxx.cpp
        ${THE_FORGE_DIR}/Common_3/Graphics/Vulkan/VulkanRaytracing.c
        ${THE_FORGE_DIR}/Common_3/Graphics/ThirdParty/OpenSource/volk/volk.c
    )
    set(THE_FORGE_OS_SOURCES
        ${THE_FORGE_DIR}/Common_3/OS/Linux/LinuxLog.c
        ${THE_FORGE_DIR}/Common_3/OS/Linux/LinuxThread.c
        ${THE_FORGE_DIR}/Common_3/OS/Linux/LinuxTime.c
        ${THE_FORGE_DIR}/Common_3/OS/Linux/LinuxFileSystem.c
        ${THE_FORGE_DIR}/Common_3/OS/Linux/LinuxToolsFileSystem.c
    )
    set(THE_FORGE_UTIL_SOURCES
        ${THE_FORGE_DIR}/Common_3/Utilities/Log/Log.c
        ${THE_FORGE_DIR}/Common_3/Utilities/FileSystem/FileSystem.c
        ${THE_FORGE_DIR}/Common_3/Utilities/FileSystem/ToolFileSystem.c
        ${THE_FORGE_DIR}/Common_3/Utilities/FileSystem/UnixFileSystem.c
        ${THE_FORGE_DIR}/Common_3/Utilities/Threading/ThreadSystem.c
        ${THE_FORGE_DIR}/Common_3/Utilities/Timer.c
        ${THE_FORGE_DIR}/Common_3/Utilities/MemoryTracking/MemoryTracking.c
        ${THE_FORGE_DIR}/Common_3/Utilities/Math/Algorithms.c
        ${THE_FORGE_DIR}/Common_3/Utilities/Math/StbDs.c
        ${THE_FORGE_DIR}/Common_3/Resources/ResourceLoader/ResourceLoader.cpp
        ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/lz4/lz4.c
        ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/lz4/lz4hc.c
        ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/bstrlib/bstrlib.c
        ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/bstrlib/buniutil.c
        ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/bstrlib/utf8util.c
        ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/FluidStudios/MemoryManager/mmgr.c
    )
    file(GLOB THE_FORGE_ZSTD_COMMON_SOURCES
        ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/zstd/common/*.c
    )
    file(GLOB THE_FORGE_ZSTD_COMPRESS_SOURCES
        ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/zstd/compress/*.c
    )
    file(GLOB THE_FORGE_ZSTD_DECOMPRESS_SOURCES
        ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/zstd/decompress/*.c
    )
    set(THE_FORGE_ZSTD_ASM_SOURCES
        ${THE_FORGE_DIR}/Common_3/Utilities/ThirdParty/OpenSource/zstd/decompress/huf_decompress_amd64.S
    )
    set_source_files_properties(${THE_FORGE_ZSTD_ASM_SOURCES} PROPERTIES LANGUAGE ASM)
    add_library(the_forge STATIC
        ${THE_FORGE_GRAPHICS_SOURCES}
        ${THE_FORGE_OS_SOURCES}
        ${THE_FORGE_UTIL_SOURCES}
        ${THE_FORGE_ZSTD_COMMON_SOURCES}
        ${THE_FORGE_ZSTD_COMPRESS_SOURCES}
        ${THE_FORGE_ZSTD_DECOMPRESS_SOURCES}
        ${THE_FORGE_ZSTD_ASM_SOURCES}
    )
    target_include_directories(the_forge PUBLIC "${THE_FORGE_DIR}")
    target_compile_definitions(the_forge PRIVATE
        FORGE_EXPLICIT_RENDERER_API
        FORGE_EXPLICIT_RENDERER_API_VULKAN
        ENABLE_MEMORY_TRACKING
    )
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

if(BZ3_RENDER_BACKEND STREQUAL "diligent")
    set(DILIGENT_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
    set(DILIGENT_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
    set(DILIGENT_BUILD_FX OFF CACHE BOOL "" FORCE)
    set(DILIGENT_NO_ARCHIVER ON CACHE BOOL "" FORCE)
    set(DILIGENT_NO_DIRECT3D11 ON CACHE BOOL "" FORCE)
    set(DILIGENT_NO_DIRECT3D12 ON CACHE BOOL "" FORCE)
    set(DILIGENT_NO_OPENGL ON CACHE BOOL "" FORCE)
    set(DILIGENT_NO_GLES ON CACHE BOOL "" FORCE)
    set(DILIGENT_NO_METAL ON CACHE BOOL "" FORCE)
    set(DILIGENT_NO_WEBGPU ON CACHE BOOL "" FORCE)
    set(DILIGENT_NO_GLSLANG OFF CACHE BOOL "" FORCE)
    set(DILIGENT_NO_HLSL OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        DiligentEngine
        GIT_REPOSITORY https://github.com/DiligentGraphics/DiligentEngine.git
        GIT_TAG v2.5.6
        GIT_SUBMODULES "DiligentCore;DiligentTools;DiligentFX;DiligentSamples"
        GIT_SUBMODULES_RECURSE ON
        PATCH_COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/cmake/diligent_wayland_apply.py
    )
    FetchContent_MakeAvailable(DiligentEngine)

    if(TARGET Diligent-GraphicsEngineVk-shared)
        target_link_libraries(Diligent-GraphicsEngineVk-shared
        PRIVATE
            Diligent-GraphicsEngine
            Diligent-GraphicsEngineNextGenBase
            Diligent-Common
            Diligent-GraphicsAccessories
            Diligent-ShaderTools
            Diligent-PlatformInterface
            Diligent-TargetPlatform
        )
    endif()
endif()

FetchContent_Declare(
    enet
    GIT_REPOSITORY https://github.com/zpl-c/enet.git
    GIT_TAG master
)
set(ENET_TEST OFF CACHE BOOL "" FORCE)
set(ENET_SHARED OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(enet)

# No particle system dependency.


# Find external packages
find_package(glm REQUIRED)
if(BZ3_PHYSICS_BACKEND STREQUAL "jolt")
    find_package(Jolt REQUIRED)
elseif(BZ3_PHYSICS_BACKEND STREQUAL "bullet")
    find_package(Bullet CONFIG REQUIRED)
elseif(BZ3_PHYSICS_BACKEND STREQUAL "physx")
    find_package(unofficial-omniverse-physx-sdk CONFIG REQUIRED)
endif()
find_package(assimp REQUIRED)
find_package(cxxopts REQUIRED)
if(BZ3_UI_BACKEND STREQUAL "imgui")
    find_package(imgui REQUIRED)
elseif(BZ3_UI_BACKEND STREQUAL "rmlui")
    find_package(RmlUi CONFIG REQUIRED)
    find_package(md4c REQUIRED)
endif()
if(BZ3_WINDOW_BACKEND STREQUAL "sdl3" OR BZ3_AUDIO_BACKEND STREQUAL "sdlaudio")
    find_package(SDL3 CONFIG REQUIRED)
endif()
find_package(spdlog REQUIRED)
find_package(miniz REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Protobuf REQUIRED)
find_package(CURL REQUIRED)
find_package(OpenSSL REQUIRED)
if(BZ3_RENDER_BACKEND STREQUAL "bgfx")
    find_package(bgfx CONFIG REQUIRED)
    set(BGFX_SHADER_DIR "${PROJECT_SOURCE_DIR}/data/bgfx/shaders")
    set(BGFX_SHADER_BIN_DIR "${PROJECT_SOURCE_DIR}/data/bgfx/shaders/bin")
    set(BGFX_SHADER_BIN_DIR_VK "${BGFX_SHADER_BIN_DIR}/vk")
    file(MAKE_DIRECTORY "${BGFX_SHADER_BIN_DIR_VK}")

    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_HOST_TRIPLET)
        set(_BGFX_SHADERC_HINT "${VCPKG_INSTALLED_DIR}/${VCPKG_HOST_TRIPLET}/tools/bgfx")
    endif()
    if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        set(_BGFX_SHADER_INCLUDE_HINT "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/include")
    endif()
    if(NOT _BGFX_SHADER_INCLUDE_HINT)
        set(_BGFX_SHADER_INCLUDE_HINT "${BGFX_SHADER_DIR}")
    endif()

    find_program(BGFX_SHADERC_EXECUTABLE NAMES shaderc HINTS ${_BGFX_SHADERC_HINT})
    if(NOT BGFX_SHADERC_EXECUTABLE)
        message(WARNING "bgfx shaderc not found; bgfx shaders will not be compiled.")
    endif()

    set(BGFX_SHADER_PROFILE_VK "spirv")
    set(BGFX_SHADER_RAW "")

    function(bz3_bgfx_compile_shader out_file type profile input varying)
        if(NOT BGFX_SHADERC_EXECUTABLE)
            return()
        endif()
        set(_shaderc_args
            -f "${input}"
            -o "${out_file}"
            --type "${type}"
            --platform linux
            --profile "${profile}"
            --varyingdef "${varying}"
            -i "${BGFX_SHADER_DIR}"
            -i "${_BGFX_SHADER_INCLUDE_HINT}"
            -i "${_BGFX_SHADER_INCLUDE_HINT}/bgfx"
        )
        if(BGFX_SHADER_RAW)
            list(APPEND _shaderc_args --raw)
        endif()
        add_custom_command(
            OUTPUT "${out_file}"
            COMMAND "${BGFX_SHADERC_EXECUTABLE}" ${_shaderc_args}
            DEPENDS "${input}" "${varying}"
            COMMENT "Compiling bgfx shader ${input}"
            VERBATIM
        )
    endfunction()

    set(BGFX_VS_BIN_VK "${BGFX_SHADER_BIN_DIR_VK}/vs_triangle.bin")
    set(BGFX_FS_BIN_VK "${BGFX_SHADER_BIN_DIR_VK}/fs_triangle.bin")
    bz3_bgfx_compile_shader("${BGFX_VS_BIN_VK}" vertex "${BGFX_SHADER_PROFILE_VK}" "${BGFX_SHADER_DIR}/vs_triangle.sc" "${BGFX_SHADER_DIR}/varying.def.sc")
    bz3_bgfx_compile_shader("${BGFX_FS_BIN_VK}" fragment "${BGFX_SHADER_PROFILE_VK}" "${BGFX_SHADER_DIR}/fs_triangle.sc" "${BGFX_SHADER_DIR}/varying.def.sc")

    set(BGFX_IMGUI_SHADER_DIR "${BGFX_SHADER_DIR}/imgui")
    set(BGFX_IMGUI_BIN_DIR_VK "${BGFX_SHADER_BIN_DIR_VK}/imgui")
    file(MAKE_DIRECTORY "${BGFX_IMGUI_BIN_DIR_VK}")
    set(BGFX_IMGUI_VS_BIN_VK "${BGFX_IMGUI_BIN_DIR_VK}/vs_imgui.bin")
    set(BGFX_IMGUI_FS_BIN_VK "${BGFX_IMGUI_BIN_DIR_VK}/fs_imgui.bin")
    bz3_bgfx_compile_shader("${BGFX_IMGUI_VS_BIN_VK}" vertex "${BGFX_SHADER_PROFILE_VK}" "${BGFX_IMGUI_SHADER_DIR}/vs_imgui.sc" "${BGFX_IMGUI_SHADER_DIR}/varying.def.sc")
    bz3_bgfx_compile_shader("${BGFX_IMGUI_FS_BIN_VK}" fragment "${BGFX_SHADER_PROFILE_VK}" "${BGFX_IMGUI_SHADER_DIR}/fs_imgui.sc" "${BGFX_IMGUI_SHADER_DIR}/varying.def.sc")

    set(BGFX_MESH_SHADER_DIR "${BGFX_SHADER_DIR}/mesh")
    set(BGFX_MESH_BIN_DIR_VK "${BGFX_SHADER_BIN_DIR_VK}/mesh")
    file(MAKE_DIRECTORY "${BGFX_MESH_BIN_DIR_VK}")
    set(BGFX_MESH_VS_BIN_VK "${BGFX_MESH_BIN_DIR_VK}/vs_mesh.bin")
    set(BGFX_MESH_FS_BIN_VK "${BGFX_MESH_BIN_DIR_VK}/fs_mesh.bin")
    bz3_bgfx_compile_shader("${BGFX_MESH_VS_BIN_VK}" vertex "${BGFX_SHADER_PROFILE_VK}" "${BGFX_MESH_SHADER_DIR}/vs_mesh.sc" "${BGFX_MESH_SHADER_DIR}/varying.def.sc")
    bz3_bgfx_compile_shader("${BGFX_MESH_FS_BIN_VK}" fragment "${BGFX_SHADER_PROFILE_VK}" "${BGFX_MESH_SHADER_DIR}/fs_mesh.sc" "${BGFX_MESH_SHADER_DIR}/varying.def.sc")

    set(BGFX_SKYBOX_SHADER_DIR "${BGFX_SHADER_DIR}/skybox")
    set(BGFX_SKYBOX_BIN_DIR_VK "${BGFX_SHADER_BIN_DIR_VK}/skybox")
    file(MAKE_DIRECTORY "${BGFX_SKYBOX_BIN_DIR_VK}")
    set(BGFX_SKYBOX_VS_BIN_VK "${BGFX_SKYBOX_BIN_DIR_VK}/vs_skybox.bin")
    set(BGFX_SKYBOX_FS_BIN_VK "${BGFX_SKYBOX_BIN_DIR_VK}/fs_skybox.bin")
    bz3_bgfx_compile_shader("${BGFX_SKYBOX_VS_BIN_VK}" vertex "${BGFX_SHADER_PROFILE_VK}" "${BGFX_SKYBOX_SHADER_DIR}/vs_skybox.sc" "${BGFX_SKYBOX_SHADER_DIR}/varying.def.sc")
    bz3_bgfx_compile_shader("${BGFX_SKYBOX_FS_BIN_VK}" fragment "${BGFX_SHADER_PROFILE_VK}" "${BGFX_SKYBOX_SHADER_DIR}/fs_skybox.sc" "${BGFX_SKYBOX_SHADER_DIR}/varying.def.sc")

    set(BGFX_RMLUI_SHADER_DIR "${BGFX_SHADER_DIR}/rmlui")
    set(BGFX_RMLUI_BIN_DIR_VK "${BGFX_SHADER_BIN_DIR_VK}/rmlui")
    file(MAKE_DIRECTORY "${BGFX_RMLUI_BIN_DIR_VK}")
    set(BGFX_RMLUI_VS_BIN_VK "${BGFX_RMLUI_BIN_DIR_VK}/vs_rmlui.bin")
    set(BGFX_RMLUI_FS_TEX_BIN_VK "${BGFX_RMLUI_BIN_DIR_VK}/fs_rmlui_texture.bin")
    set(BGFX_RMLUI_FS_COLOR_BIN_VK "${BGFX_RMLUI_BIN_DIR_VK}/fs_rmlui_color.bin")
    bz3_bgfx_compile_shader("${BGFX_RMLUI_VS_BIN_VK}" vertex "${BGFX_SHADER_PROFILE_VK}" "${BGFX_RMLUI_SHADER_DIR}/vs_rmlui.sc" "${BGFX_RMLUI_SHADER_DIR}/varying.def.sc")
    bz3_bgfx_compile_shader("${BGFX_RMLUI_FS_TEX_BIN_VK}" fragment "${BGFX_SHADER_PROFILE_VK}" "${BGFX_RMLUI_SHADER_DIR}/fs_rmlui_texture.sc" "${BGFX_RMLUI_SHADER_DIR}/varying.def.sc")
    bz3_bgfx_compile_shader("${BGFX_RMLUI_FS_COLOR_BIN_VK}" fragment "${BGFX_SHADER_PROFILE_VK}" "${BGFX_RMLUI_SHADER_DIR}/fs_rmlui_color.sc" "${BGFX_RMLUI_SHADER_DIR}/varying.def.sc")

    if(BGFX_SHADERC_EXECUTABLE)
        add_custom_target(bz3_bgfx_shaders ALL DEPENDS
            "${BGFX_VS_BIN_VK}" "${BGFX_FS_BIN_VK}"
            "${BGFX_IMGUI_VS_BIN_VK}" "${BGFX_IMGUI_FS_BIN_VK}"
            "${BGFX_MESH_VS_BIN_VK}" "${BGFX_MESH_FS_BIN_VK}"
            "${BGFX_SKYBOX_VS_BIN_VK}" "${BGFX_SKYBOX_FS_BIN_VK}"
            "${BGFX_RMLUI_VS_BIN_VK}" "${BGFX_RMLUI_FS_TEX_BIN_VK}" "${BGFX_RMLUI_FS_COLOR_BIN_VK}"
        )
    else()
        add_custom_target(bz3_bgfx_shaders ALL)
    endif()
endif()

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
    ${PROJECT_SOURCE_DIR}/src/engine/common/config_store.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/i18n.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/data_path_resolver.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/data_dir_override.cpp
)
file(GLOB_RECURSE SERVER_SOURCES CONFIGURE_DEPENDS "${PROJECT_SOURCE_DIR}/src/game/server/*.cpp")
list(APPEND SERVER_SOURCES
    ${PROJECT_SOURCE_DIR}/src/game/server/terminal_commands.cpp
    ${PROJECT_SOURCE_DIR}/src/game/server/server_cli_options.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/curl_global.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/config_helpers.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/config_store.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/common/i18n.cpp
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
    ${PROJECT_SOURCE_DIR}/src/game/common/data_path_spec.cpp
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
    ${PROJECT_SOURCE_DIR}/src/game/ui/config.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/ui_config.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/input_mapping.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/controllers/console_controller.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/controllers/console_controller.hpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/controllers/hud_controller.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/controllers/hud_controller.hpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/controllers/bindings_controller.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/controllers/bindings_controller.hpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/controllers/settings_controller.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/controllers/settings_controller.hpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/console/keybindings.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/hud_settings.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/render_settings.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/render_scale.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/models/hud_model.hpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/models/console_model.hpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/models/settings_model.hpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/models/bindings_model.hpp
    ${PROJECT_SOURCE_DIR}/src/engine/audio/audio.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/audio/backend_factory.cpp
)
if(BZ3_WINDOW_BACKEND STREQUAL "sdl3")
    list(APPEND ENGINE_CLIENT_SOURCES ${PROJECT_SOURCE_DIR}/src/engine/platform/backends/window_sdl.cpp)
elseif(BZ3_WINDOW_BACKEND STREQUAL "sdl2")
    list(APPEND ENGINE_CLIENT_SOURCES ${PROJECT_SOURCE_DIR}/src/engine/platform/backends/window_sdl2_stub.cpp)
endif()


if(BZ3_UI_BACKEND STREQUAL "imgui")
    list(APPEND ENGINE_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/backend.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/console/console.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/console/thumbnail_cache.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/console/panels/panel_community.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/console/panels/panel_settings.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/console/panels/panel_bindings.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/console/panels/panel_documentation.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/console/panels/panel_start_server.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/hud/hud.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/hud/chat.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/hud/crosshair.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/hud/fps.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/hud/radar.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/hud/scoreboard.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/hud/dialog.cpp
    )
    if(BZ3_RENDER_BACKEND STREQUAL "bgfx")
        list(APPEND ENGINE_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/platform/renderer_bgfx.cpp
        )
    elseif(BZ3_RENDER_BACKEND STREQUAL "diligent")
        list(APPEND ENGINE_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/platform/renderer_diligent.cpp
        )
    elseif(BZ3_RENDER_BACKEND STREQUAL "forge")
        list(APPEND ENGINE_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/imgui/platform/renderer_forge.cpp
        )
    endif()
elseif(BZ3_UI_BACKEND STREQUAL "rmlui")
    list(APPEND ENGINE_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/backend.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/console/emoji_utils.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/console/console.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/console/modal_dialog.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/hud/hud.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/hud/chat.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/hud/crosshair.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/hud/dialog.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/hud/radar.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/hud/scoreboard.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/translate.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/console/panels/panel.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/console/panels/panel_community.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/console/panels/panel_settings.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/console/panels/panel_bindings.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/console/panels/panel_documentation.cpp
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/console/panels/panel_start_server.cpp
    )
    if(BZ3_RENDER_BACKEND STREQUAL "bgfx")
        list(APPEND ENGINE_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/platform/renderer_bgfx.cpp
        )
    elseif(BZ3_RENDER_BACKEND STREQUAL "diligent")
        list(APPEND ENGINE_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/platform/renderer_diligent.cpp
        )
    elseif(BZ3_RENDER_BACKEND STREQUAL "forge")
        list(APPEND ENGINE_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/game/ui/frontends/rmlui/platform/renderer_forge.cpp
        )
    endif()
endif()

list(APPEND ENGINE_CLIENT_SOURCES
    ${PROJECT_SOURCE_DIR}/src/game/ui/third_party/stb_image_impl.cpp
)
set(ENGINE_SERVER_SOURCES
    ${PROJECT_SOURCE_DIR}/src/game/engine/server_engine.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/geometry/mesh_loader.cpp
    ${PROJECT_SOURCE_DIR}/src/game/ui/third_party/stb_image_impl.cpp
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
    ${PROJECT_SOURCE_DIR}/src/game/common/data_path_spec.cpp
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
elseif(BZ3_PHYSICS_BACKEND STREQUAL "physx")
    list(APPEND PHYSICS_SOURCES
        ${PROJECT_SOURCE_DIR}/src/engine/physics/backends/physx/physics_world_physx.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/physics/backends/physx/rigid_body_physx.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/physics/backends/physx/static_body_physx.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/physics/backends/physx/player_controller_physx.cpp
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

if(BZ3_RENDER_BACKEND STREQUAL "diligent")
    list(APPEND ENGINE_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/engine/graphics/backends/diligent/backend.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/graphics/backends/diligent/ui_bridge.cpp
    )
    if(BZ3_UI_BACKEND STREQUAL "imgui")
        list(APPEND ENGINE_CLIENT_SOURCES
        )
    endif()
elseif(BZ3_RENDER_BACKEND STREQUAL "bgfx")
    list(APPEND ENGINE_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/engine/graphics/backends/bgfx/backend.cpp
    )
    if(BZ3_UI_BACKEND STREQUAL "imgui")
        list(APPEND ENGINE_CLIENT_SOURCES
        )
    endif()
elseif(BZ3_RENDER_BACKEND STREQUAL "forge")
    list(APPEND ENGINE_CLIENT_SOURCES
        ${PROJECT_SOURCE_DIR}/src/engine/graphics/backends/forge/backend.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/graphics/backends/forge/ui_bridge.cpp
        ${PROJECT_SOURCE_DIR}/src/engine/graphics/backends/forge/reload_stubs.cpp
    )
endif()

# Create executables
set(MACOSX_BUNDLE_ICON_FILE "") # Set to your .icns file if you have one
add_executable(bz3 MACOSX_BUNDLE ${CLIENT_SOURCES} ${ENGINE_CLIENT_SOURCES})


if(BZ3_WINDOW_BACKEND STREQUAL "sdl3")
    list(APPEND ENGINE_CLIENT_SOURCES ${PROJECT_SOURCE_DIR}/src/engine/platform/backends/window_sdl.cpp)
elseif(BZ3_WINDOW_BACKEND STREQUAL "sdl2")
    list(APPEND ENGINE_CLIENT_SOURCES ${PROJECT_SOURCE_DIR}/src/engine/platform/backends/window_sdl2_stub.cpp)
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
)

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
    cxxopts::cxxopts
    spdlog::spdlog
    miniz::miniz
    nlohmann_json::nlohmann_json
    proto_lib
    CURL::libcurl
    OpenSSL::Crypto
)
if(TARGET the_forge_headers)
    target_link_libraries(bz3 PRIVATE the_forge_headers)
endif()
if(BZ3_RENDER_BACKEND STREQUAL "diligent")
    target_link_libraries(bz3 PRIVATE Diligent-GraphicsEngineVk-static)
    if(TARGET Diligent-Common)
        target_link_libraries(bz3 PRIVATE Diligent-Common)
    endif()
    if(TARGET Diligent-GraphicsEngine)
        target_link_libraries(bz3 PRIVATE Diligent-GraphicsEngine)
    endif()
    if(TARGET Diligent-GraphicsEngineNextGenBase)
        target_link_libraries(bz3 PRIVATE Diligent-GraphicsEngineNextGenBase)
    endif()
    if(TARGET Diligent-GraphicsAccessories)
        target_link_libraries(bz3 PRIVATE Diligent-GraphicsAccessories)
    endif()
    if(TARGET Diligent-ShaderTools)
        target_link_libraries(bz3 PRIVATE Diligent-ShaderTools)
    endif()
    if(TARGET Diligent-PlatformInterface)
        target_link_libraries(bz3 PRIVATE Diligent-PlatformInterface)
    endif()
    if(TARGET Diligent-TargetPlatform)
        target_link_libraries(bz3 PRIVATE Diligent-TargetPlatform)
    endif()
    if(DEFINED DiligentEngine_SOURCE_DIR)
        target_include_directories(bz3 PRIVATE ${DiligentEngine_SOURCE_DIR})
        target_include_directories(bz3 PRIVATE ${DiligentEngine_SOURCE_DIR}/DiligentCore)
        target_include_directories(bz3 PRIVATE ${DiligentEngine_SOURCE_DIR}/DiligentTools)
    endif()
    if(UNIX AND NOT APPLE)
        target_compile_definitions(bz3 PRIVATE PLATFORM_LINUX=1)
    endif()
elseif(BZ3_RENDER_BACKEND STREQUAL "bgfx")
    target_link_libraries(bz3 PRIVATE bgfx::bgfx)
    add_dependencies(bz3 bz3_bgfx_shaders)
elseif(BZ3_RENDER_BACKEND STREQUAL "forge")
    target_link_libraries(bz3 PRIVATE the_forge)
endif()
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
elseif(BZ3_PHYSICS_BACKEND STREQUAL "physx")
    target_link_libraries(bz3 PRIVATE unofficial::omniverse-physx-sdk::sdk)
    # Ensure static lib link order resolves PhysX character controller deps.
    target_link_libraries(bz3 PRIVATE
        unofficial::omniverse-physx-sdk::PhysXCharacterKinematic
        unofficial::omniverse-physx-sdk::PhysXExtensions
    )
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

if(UNIX AND NOT APPLE)
endif()

if(BZ3_UI_BACKEND STREQUAL "rmlui")
    target_link_libraries(bz3 PRIVATE RmlUi::RmlUi md4c::md4c)
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
elseif(BZ3_PHYSICS_BACKEND STREQUAL "physx")
    target_link_libraries(bz3-server PRIVATE unofficial::omniverse-physx-sdk::sdk)
    # Ensure static lib link order resolves PhysX character controller deps.
    target_link_libraries(bz3-server PRIVATE
        unofficial::omniverse-physx-sdk::PhysXCharacterKinematic
        unofficial::omniverse-physx-sdk::PhysXExtensions
    )
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
    if(BZ3_WINDOW_BACKEND STREQUAL "sdl3")
        set(CPACK_DEBIAN_PACKAGE_DEPENDS "libgl1, libsdl3")
    else()
        set(CPACK_DEBIAN_PACKAGE_DEPENDS "libgl1")
    endif()
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
elseif(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
endif()

include(CPack)
