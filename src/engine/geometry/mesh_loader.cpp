#include "karma/geometry/mesh_loader.hpp"
#include <vector>
#include <glm/glm.hpp>
#include <string>
#include <filesystem>
#include <unordered_map>
#include <cstdlib>
#include <stb_image.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/matrix3x3.h>

namespace {
struct TextureCache {
    std::unordered_map<std::string, std::shared_ptr<MeshLoader::TextureData>> entries;
};

std::shared_ptr<MeshLoader::TextureData> loadTextureFromMemory(const unsigned char* data,
                                                               int dataSize,
                                                               const std::string& key,
                                                               TextureCache& cache) {
    if (auto it = cache.entries.find(key); it != cache.entries.end()) {
        return it->second;
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load_from_memory(data, dataSize, &width, &height, &channels, 4);
    if (!pixels || width <= 0 || height <= 0) {
        if (pixels) {
            stbi_image_free(pixels);
        }
        return nullptr;
    }

    auto texture = std::make_shared<MeshLoader::TextureData>();
    texture->width = width;
    texture->height = height;
    texture->channels = 4;
    texture->pixels.assign(pixels, pixels + static_cast<size_t>(width * height * 4));
    texture->key = key;
    stbi_image_free(pixels);
    cache.entries[key] = texture;
    return texture;
}

std::shared_ptr<MeshLoader::TextureData> loadTextureFromFile(const std::filesystem::path& path,
                                                             const std::string& key,
                                                             TextureCache& cache) {
    if (auto it = cache.entries.find(key); it != cache.entries.end()) {
        return it->second;
    }
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load(path.string().c_str(), &width, &height, &channels, 4);
    if (!pixels || width <= 0 || height <= 0) {
        if (pixels) {
            stbi_image_free(pixels);
        }
        return nullptr;
    }

    auto texture = std::make_shared<MeshLoader::TextureData>();
    texture->width = width;
    texture->height = height;
    texture->channels = 4;
    texture->pixels.assign(pixels, pixels + static_cast<size_t>(width * height * 4));
    texture->key = key;
    stbi_image_free(pixels);
    cache.entries[key] = texture;
    return texture;
}

std::shared_ptr<MeshLoader::TextureData> loadEmbeddedTexture(const aiTexture* texture,
                                                             const std::string& key,
                                                             TextureCache& cache) {
    if (!texture) {
        return nullptr;
    }
    if (auto it = cache.entries.find(key); it != cache.entries.end()) {
        return it->second;
    }

    if (texture->mHeight == 0) {
        const auto* data = reinterpret_cast<const unsigned char*>(texture->pcData);
        return loadTextureFromMemory(data, static_cast<int>(texture->mWidth), key, cache);
    }

    const int width = static_cast<int>(texture->mWidth);
    const int height = static_cast<int>(texture->mHeight);
    if (width <= 0 || height <= 0) {
        return nullptr;
    }

    auto textureData = std::make_shared<MeshLoader::TextureData>();
    textureData->width = width;
    textureData->height = height;
    textureData->channels = 4;
    textureData->pixels.resize(static_cast<size_t>(width * height * 4));
    textureData->key = key;

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const aiTexel& texel = texture->pcData[y * width + x];
            const size_t index = static_cast<size_t>((y * width + x) * 4);
            textureData->pixels[index + 0] = texel.r;
            textureData->pixels[index + 1] = texel.g;
            textureData->pixels[index + 2] = texel.b;
            textureData->pixels[index + 3] = texel.a;
        }
    }

    cache.entries[key] = textureData;
    return textureData;
}

std::shared_ptr<MeshLoader::TextureData> loadMaterialTexture(const aiScene* scene,
                                                             const aiMaterial* material,
                                                             const std::filesystem::path& baseDir,
                                                             const std::filesystem::path& modelPath,
                                                             TextureCache& cache) {
    if (!scene || !material) {
        return nullptr;
    }

    aiString texPath;
    if (material->GetTexture(aiTextureType_BASE_COLOR, 0, &texPath) != AI_SUCCESS) {
        material->GetTexture(aiTextureType_DIFFUSE, 0, &texPath);
    }
    if (texPath.length == 0) {
        return nullptr;
    }

    const std::string rawPath = texPath.C_Str();
    if (!rawPath.empty() && rawPath[0] == '*') {
        const int index = std::atoi(rawPath.c_str() + 1);
        if (index < 0 || index >= static_cast<int>(scene->mNumTextures)) {
            return nullptr;
        }
        const std::string key = modelPath.string() + ":embedded:" + std::to_string(index);
        return loadEmbeddedTexture(scene->mTextures[index], key, cache);
    }

    const std::filesystem::path resolved = baseDir / rawPath;
    const std::string key = resolved.string();
    if (!std::filesystem::exists(resolved)) {
        return nullptr;
    }
    return loadTextureFromFile(resolved, key, cache);
}
} // namespace

namespace {
void appendMeshData(const aiScene* scene,
                    const aiMesh* mesh,
                    const aiMatrix4x4& transform,
                    const std::filesystem::path& baseDir,
                    const std::filesystem::path& modelPath,
                    const MeshLoader::LoadOptions& options,
                    TextureCache& textureCache,
                    std::vector<MeshLoader::MeshData>& outMeshes) {
    if (!mesh) {
        return;
    }
    MeshLoader::MeshData data;
    data.vertices.reserve(mesh->mNumVertices);
    data.texcoords.reserve(mesh->mNumVertices);
    data.normals.reserve(mesh->mNumVertices);
    aiMatrix3x3 normalMatrix = aiMatrix3x3(transform);
    normalMatrix = normalMatrix.Inverse().Transpose();
    for (unsigned int v = 0; v < mesh->mNumVertices; ++v) {
        const auto &vert = mesh->mVertices[v];
        const aiVector3D pos = transform * vert;
        data.vertices.emplace_back(pos.x, pos.y, pos.z);

        if (mesh->HasTextureCoords(0)) {
            const auto& uv = mesh->mTextureCoords[0][v];
            data.texcoords.emplace_back(uv.x, uv.y);
        } else {
            data.texcoords.emplace_back(0.0f, 0.0f);
        }

        if (mesh->HasNormals()) {
            const auto& n = mesh->mNormals[v];
            const aiVector3D nn = normalMatrix * n;
            aiVector3D nnn = nn;
            nnn.Normalize();
            data.normals.emplace_back(nnn.x, nnn.y, nnn.z);
        } else {
            data.normals.emplace_back(0.0f, 1.0f, 0.0f);
        }
    }

    for (unsigned int f = 0; f < mesh->mNumFaces; ++f) {
        aiFace &face = mesh->mFaces[f];
        if (face.mNumIndices != 3) continue;
        data.indices.push_back(face.mIndices[0]);
        data.indices.push_back(face.mIndices[1]);
        data.indices.push_back(face.mIndices[2]);
    }

    if (options.loadTextures && mesh->mMaterialIndex < scene->mNumMaterials) {
        const aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
        data.albedo = loadMaterialTexture(scene, material, baseDir, modelPath, textureCache);
    }

    outMeshes.push_back(std::move(data));
}

void traverseNode(const aiScene* scene,
                  const aiNode* node,
                  const aiMatrix4x4& parentTransform,
                  const std::filesystem::path& baseDir,
                  const std::filesystem::path& modelPath,
                  const MeshLoader::LoadOptions& options,
                  TextureCache& textureCache,
                  std::vector<MeshLoader::MeshData>& outMeshes) {
    if (!node) {
        return;
    }
    const aiMatrix4x4 current = parentTransform * node->mTransformation;

    for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
        const unsigned int meshIndex = node->mMeshes[i];
        if (meshIndex < scene->mNumMeshes) {
            appendMeshData(scene, scene->mMeshes[meshIndex], current, baseDir, modelPath, options, textureCache, outMeshes);
        }
    }

    for (unsigned int i = 0; i < node->mNumChildren; ++i) {
        traverseNode(scene, node->mChildren[i], current, baseDir, modelPath, options, textureCache, outMeshes);
    }
}
} // namespace

namespace MeshLoader {
    std::vector<MeshData> loadGLB(const std::string &filename, const LoadOptions& options) {
        std::vector<MeshData> meshes;

        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(filename,
            aiProcess_Triangulate |
            aiProcess_JoinIdenticalVertices |
            aiProcess_GenNormals
        );

        if (!scene) return meshes;

        TextureCache textureCache;
        const std::filesystem::path modelPath(filename);
        const std::filesystem::path baseDir = modelPath.parent_path();

        traverseNode(scene, scene->mRootNode, aiMatrix4x4(), baseDir, modelPath, options, textureCache, meshes);

        return meshes;
    }

}
