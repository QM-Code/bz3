#pragma once

#include "engine/geometry/mesh_loader.hpp"
#include "engine/graphics/device.hpp"
#include <filesystem>
#include <unordered_map>
#include <vector>

namespace graphics {

class ResourceRegistry {
public:
    explicit ResourceRegistry(GraphicsDevice &device) : device_(&device) {}

    MeshId loadMesh(const std::filesystem::path &path) {
        auto it = meshCache_.find(path);
        if (it != meshCache_.end()) {
            return it->second;
        }
        graphics::MeshData meshData;
        const auto meshes = MeshLoader::loadGLB(path.string(), MeshLoader::LoadOptions{});
        if (!meshes.empty()) {
            size_t vertexOffset = 0;
            for (const auto &source : meshes) {
                meshData.vertices.insert(meshData.vertices.end(),
                                         source.vertices.begin(),
                                         source.vertices.end());
                meshData.texcoords.insert(meshData.texcoords.end(),
                                          source.texcoords.begin(),
                                          source.texcoords.end());
                meshData.normals.insert(meshData.normals.end(),
                                        source.normals.begin(),
                                        source.normals.end());
                meshData.indices.reserve(meshData.indices.size() + source.indices.size());
                for (unsigned int idx : source.indices) {
                    meshData.indices.push_back(static_cast<uint32_t>(idx + vertexOffset));
                }
                vertexOffset += source.vertices.size();
            }
        }
        MeshId mesh = device_->createMesh(meshData);
        if (mesh != kInvalidMesh) {
            meshCache_.insert({path, mesh});
        }
        return mesh;
    }

    MaterialId createMaterial(const MaterialDesc &desc) {
        MaterialId material = device_->createMaterial(desc);
        if (material != kInvalidMaterial) {
            materialCache_.push_back(material);
        }
        return material;
    }

    MaterialId getDefaultMaterial() {
        if (defaultMaterial_ != kInvalidMaterial) {
            return defaultMaterial_;
        }
        MaterialDesc desc;
        desc.baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
        desc.depthTest = true;
        desc.depthWrite = true;
        desc.unlit = false;
        desc.doubleSided = false;
        defaultMaterial_ = createMaterial(desc);
        return defaultMaterial_;
    }

private:
    GraphicsDevice *device_;
    std::unordered_map<std::filesystem::path, MeshId> meshCache_;
    std::vector<MaterialId> materialCache_;
    MaterialId defaultMaterial_ = kInvalidMaterial;
};

} // namespace graphics
