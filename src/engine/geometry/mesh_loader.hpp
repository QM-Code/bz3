#pragma once

#include <vector>
#include <cstdint>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <vector>

namespace MeshLoader {
    
    struct TextureData {
        int width = 0;
        int height = 0;
        int channels = 0;
        std::vector<uint8_t> pixels;
        std::string key;
    };

    struct MeshData {
        std::vector<glm::vec3> vertices;
        std::vector<unsigned int> indices;
        std::vector<glm::vec2> texcoords;
        std::vector<glm::vec3> normals;
        std::shared_ptr<TextureData> albedo;
    };

    struct LoadOptions {
        bool loadTextures = false;
    };

    std::vector<MeshData> loadGLB(const std::string &filename, const LoadOptions& options = {});
}
