#pragma once

#include <cstdint>
#include <filesystem>
#include <glm/glm.hpp>
#include <string>
#include <vector>

namespace graphics {

using EntityId = uint32_t;
using MeshId = uint32_t;
using MaterialId = uint32_t;
using RenderTargetId = uint32_t;
using LayerId = uint32_t;

constexpr RenderTargetId kDefaultRenderTarget = 0;
constexpr MaterialId kInvalidMaterial = 0;
constexpr MeshId kInvalidMesh = 0;
constexpr EntityId kInvalidEntity = 0;

struct MeshData {
    std::vector<glm::vec3> vertices;
    std::vector<uint32_t> indices;
    std::vector<glm::vec2> texcoords;
    std::vector<glm::vec3> normals;
};

struct MaterialDesc {
    std::filesystem::path vertexShaderPath;
    std::filesystem::path fragmentShaderPath;
    glm::vec4 baseColor{1.0f, 1.0f, 1.0f, 1.0f};
    bool unlit = false;
    bool transparent = false;
    bool depthTest = true;
    bool depthWrite = true;
    bool wireframe = false;
    bool doubleSided = false;
};

struct RenderTargetDesc {
    int width = 0;
    int height = 0;
    bool depth = true;
    bool stencil = false;
};

} // namespace graphics
