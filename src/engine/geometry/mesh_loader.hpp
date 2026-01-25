#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <string>

namespace MeshLoader {
    
    struct MeshData {
        std::vector<glm::vec3> vertices;
        std::vector<unsigned int> indices;
    };

    std::vector<MeshData> loadGLB(const std::string &filename);
}
