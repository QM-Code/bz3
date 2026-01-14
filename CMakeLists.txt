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
find_package(glfw3 REQUIRED)
find_package(glm REQUIRED)
find_package(Bullet REQUIRED)
find_package(assimp REQUIRED)
find_package(OpenGL REQUIRED)
find_package(cxxopts REQUIRED)
find_package(imgui REQUIRED)
find_package(spdlog REQUIRED)
find_package(miniz REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(Protobuf REQUIRED)
find_package(CURL REQUIRED)

# Compile protobuf files
set(PROTO_FILES ${PROJECT_SOURCE_DIR}/src/protos/messages.proto)
set(PROTO_OUTPUT_DIR ${CMAKE_BINARY_DIR}/generated)
file(MAKE_DIRECTORY ${PROTO_OUTPUT_DIR})

# Generate protobuf sources
protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${PROTO_FILES})

# Create a library for proto files
add_library(proto_lib STATIC ${PROTO_SRCS} ${PROTO_HDRS})
target_link_libraries(proto_lib PUBLIC protobuf::libprotobuf)
target_include_directories(proto_lib PUBLIC ${CMAKE_BINARY_DIR})

# Gather sources
file(GLOB_RECURSE CLIENT_SOURCES "${PROJECT_SOURCE_DIR}/src/client/*.cpp")
list(APPEND CLIENT_SOURCES
    ${PROJECT_SOURCE_DIR}/src/client/server/server_browser_controller.cpp
    ${PROJECT_SOURCE_DIR}/src/common/config_helpers.cpp
    ${PROJECT_SOURCE_DIR}/src/common/data_path_resolver.cpp
    ${PROJECT_SOURCE_DIR}/src/common/data_dir_override.cpp
)
file(GLOB_RECURSE SERVER_SOURCES "${PROJECT_SOURCE_DIR}/src/server/*.cpp")
list(APPEND SERVER_SOURCES
    ${PROJECT_SOURCE_DIR}/src/server/terminal_commands.cpp
    ${PROJECT_SOURCE_DIR}/src/server/server_cli_options.cpp
    ${PROJECT_SOURCE_DIR}/src/common/config_helpers.cpp
    ${PROJECT_SOURCE_DIR}/src/common/data_path_resolver.cpp
    ${PROJECT_SOURCE_DIR}/src/common/data_dir_override.cpp
)
## Separate engine sources for client and server to avoid over-linking
set(ENGINE_CLIENT_SOURCES
    ${PROJECT_SOURCE_DIR}/src/engine/client_engine.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/mesh_loader.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/components/client_network.cpp
    ${PROJECT_SOURCE_DIR}/src/network/enet_transport.cpp
    ${PROJECT_SOURCE_DIR}/src/network/transport_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/network/proto_codec.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/components/render.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/components/input.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/components/gui.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/components/gui/server_browser.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/physics/physics_world.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/physics/rigid_body.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/physics/player_controller.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/physics/compound_body.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/components/audio.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/components/particle_effect_system.cpp
)
set(ENGINE_SERVER_SOURCES
    ${PROJECT_SOURCE_DIR}/src/engine/server_engine.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/mesh_loader.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/components/server_network.cpp
    ${PROJECT_SOURCE_DIR}/src/network/enet_transport.cpp
    ${PROJECT_SOURCE_DIR}/src/network/transport_factory.cpp
    ${PROJECT_SOURCE_DIR}/src/network/proto_codec.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/physics/physics_world.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/physics/rigid_body.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/physics/player_controller.cpp
    ${PROJECT_SOURCE_DIR}/src/engine/physics/compound_body.cpp
)

# Create executables
set(MACOSX_BUNDLE_ICON_FILE "") # Set to your .icns file if you have one
add_executable(bz3 MACOSX_BUNDLE ${CLIENT_SOURCES} ${ENGINE_CLIENT_SOURCES})

# Handle GLFW target for portability
if(TARGET glfw3::glfw)
    set(GLFW_TARGET glfw3::glfw)
else()
    set(GLFW_TARGET glfw)
endif()

# Handle OpenGL target for portability
if(TARGET OpenGL::OpenGL)
    set(OPENGL_TARGET OpenGL::OpenGL)
else()
    set(OPENGL_TARGET ${OPENGL_LIBRARIES})
endif()

add_executable(bz3-server ${SERVER_SOURCES} ${ENGINE_SERVER_SOURCES})

# Include directories
target_include_directories(bz3 PRIVATE
    ${PROJECT_SOURCE_DIR}/src/
    ${PROJECT_SOURCE_DIR}/src/client
    ${BULLET_INCLUDE_DIRS}
    ${enet_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/libs/effekseer/src
    ${PROJECT_SOURCE_DIR}/libs/effekseer/src/Effekseer
    ${PROJECT_SOURCE_DIR}/include/client
)
target_include_directories(bz3-server PRIVATE
    ${PROJECT_SOURCE_DIR}/src/
    ${PROJECT_SOURCE_DIR}/src/server
    ${BULLET_INCLUDE_DIRS}
    ${enet_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(bz3 PRIVATE
    ${GLFW_TARGET}
    glm::glm
    ${BULLET_LIBRARIES}
    enet_static
    assimp::assimp
    ${OPENGL_TARGET}
    threepp::threepp
    cxxopts::cxxopts
    imgui::imgui
    spdlog::spdlog
    miniz::miniz
    nlohmann_json::nlohmann_json
    proto_lib
    CURL::libcurl
    Effekseer
    EffekseerRendererGL
)

target_link_libraries(bz3-server PRIVATE
    glm::glm
    ${BULLET_LIBRARIES}
    enet_static
    assimp::assimp
    pybind11::embed
    cxxopts::cxxopts
    spdlog::spdlog
    miniz::miniz
    nlohmann_json::nlohmann_json
    proto_lib
)

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
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libgl1, libglfw3")
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
elseif(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
endif()

include(CPack)
