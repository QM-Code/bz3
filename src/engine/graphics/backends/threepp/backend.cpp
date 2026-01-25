#include "engine/graphics/backends/threepp/backend.hpp"

#include "platform/window.hpp"
#include "spdlog/spdlog.h"

#include <threepp/loaders/AssimpLoader.hpp>
#include <threepp/materials/MeshBasicMaterial.hpp>
#include <threepp/materials/MeshStandardMaterial.hpp>
#include <threepp/materials/ShaderMaterial.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

namespace {
std::string readFileToString(const std::filesystem::path& path) {
    if (path.empty()) {
        return {};
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        spdlog::error("Graphics: Failed to open file '{}'", path.string());
        return {};
    }

    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}

threepp::Color toThreeColor(const glm::vec4& color) {
    const float r = std::clamp(color.r, 0.0f, 1.0f);
    const float g = std::clamp(color.g, 0.0f, 1.0f);
    const float b = std::clamp(color.b, 0.0f, 1.0f);
    return threepp::Color(r, g, b);
}

} // namespace

namespace graphics_backend {

ThreeppBackend::ThreeppBackend(platform::Window& windowRef)
    : window(&windowRef),
      renderer({1, 1}) {
    spdlog::trace("Graphics: init start");

    int fbWidth = 800, fbHeight = 600;
    window->getFramebufferSize(fbWidth, fbHeight);
    if (fbHeight == 0) fbHeight = 1;
    renderer.setSize({fbWidth, fbHeight});

    perspectiveCamera_ = threepp::PerspectiveCamera::create(
        60.0f,
        static_cast<float>(fbWidth) / static_cast<float>(fbHeight),
        0.1f,
        1000.f
    );
    perspectiveCamera_->updateProjectionMatrix();
    activeCamera_ = perspectiveCamera_;

    renderer.setClearColor(threepp::Color(0x3399ff));
    renderer.shadowMap().enabled = true;
    renderer.shadowMap().type = threepp::ShadowMap::PFCSoft;
    renderer.toneMapping = threepp::ToneMapping::ACESFilmic;

    defaultMaterial_ = threepp::MeshStandardMaterial::create();
    defaultMaterial_->color = threepp::Color(0xffffff);

    auto mainScene = sceneForLayer(0);
    auto light = threepp::AmbientLight::create(0xffffff, 0.5f);
    mainScene->add(light);

    auto dir = threepp::DirectionalLight::create(threepp::Color(0xffffff), 1.0f);
    dir->position.set(150, 50, 150);
    dir->castShadow = true;
    dir->shadow->mapSize.set(2048, 2048);
    auto shadowCam = dynamic_cast<threepp::OrthographicCamera*>(dir->shadow->camera.get());
    shadowCam->left   = -50;
    shadowCam->right  =  50;
    shadowCam->top    =  50;
    shadowCam->bottom = -50;
    shadowCam->updateProjectionMatrix();
    mainScene->add(dir);
}

ThreeppBackend::~ThreeppBackend() {
    for (auto& [id, record] : entities_) {
        if (record.object) {
            auto scene = sceneForLayer(record.layer);
            scene->remove(*record.object);
        }
    }
    entities_.clear();
}

void ThreeppBackend::beginFrame() {
    // No-op for threepp
}

void ThreeppBackend::endFrame() {
    // No-op for threepp
}

void ThreeppBackend::resize(int width, int height) {
    if (height == 0) {
        height = 1;
    }
    renderer.setSize({width, height});
    if (perspectiveCamera_) {
        perspectiveCamera_->aspect = static_cast<float>(width) / static_cast<float>(height);
        perspectiveCamera_->updateProjectionMatrix();
    }
}

std::shared_ptr<threepp::Scene> ThreeppBackend::sceneForLayer(graphics::LayerId layer) {
    auto it = scenes_.find(layer);
    if (it != scenes_.end()) {
        return it->second;
    }

    auto scene = threepp::Scene::create();
    scenes_[layer] = scene;
    return scene;
}

std::shared_ptr<threepp::Material> ThreeppBackend::createMaterialInstance(const graphics::MaterialDesc& desc) const {
    if (desc.unlit) {
        auto material = threepp::MeshBasicMaterial::create();
        material->color = toThreeColor(desc.baseColor);
        material->transparent = desc.transparent;
        material->depthTest = desc.depthTest;
        material->depthWrite = desc.depthWrite;
        material->wireframe = desc.wireframe;
        material->side = desc.doubleSided ? threepp::Side::Double : threepp::Side::Front;
        return material;
    }

    if (!desc.vertexShaderPath.empty() && !desc.fragmentShaderPath.empty()) {
        auto shader = threepp::ShaderMaterial::create();
        shader->vertexShader = readFileToString(desc.vertexShaderPath);
        shader->fragmentShader = readFileToString(desc.fragmentShaderPath);
        shader->transparent = desc.transparent;
        shader->depthTest = desc.depthTest;
        shader->depthWrite = desc.depthWrite;
        shader->wireframe = desc.wireframe;
        shader->side = desc.doubleSided ? threepp::Side::Double : threepp::Side::Front;
        shader->uniforms.insert_or_assign("baseColor", threepp::Uniform(threepp::UniformValue{toThreeColor(desc.baseColor)}));
        shader->needsUpdate();
        return shader;
    }

    auto material = threepp::MeshStandardMaterial::create();
    material->color = toThreeColor(desc.baseColor);
    material->transparent = desc.transparent;
    material->depthTest = desc.depthTest;
    material->depthWrite = desc.depthWrite;
    material->wireframe = desc.wireframe;
    material->side = desc.doubleSided ? threepp::Side::Double : threepp::Side::Front;
    return material;
}

std::shared_ptr<threepp::Material> ThreeppBackend::materialForId(graphics::MaterialId material) const {
    if (material == graphics::kInvalidMaterial) {
        return defaultMaterial_;
    }
    auto it = materials_.find(material);
    if (it == materials_.end()) {
        return defaultMaterial_;
    }
    return it->second;
}

graphics::EntityId ThreeppBackend::createEntity(graphics::LayerId layer) {
    const graphics::EntityId id = nextEntityId++;
    auto group = threepp::Group::create();
    sceneForLayer(layer)->add(group);
    entities_[id] = {layer, group};
    return id;
}

graphics::EntityId ThreeppBackend::createModelEntity(const std::filesystem::path& modelPath,
                                                     graphics::LayerId layer,
                                                     graphics::MaterialId materialOverride) {
    graphics::EntityId id = createEntity(layer);
    setEntityModel(id, modelPath, materialOverride);
    return id;
}

graphics::EntityId ThreeppBackend::createMeshEntity(graphics::MeshId mesh,
                                                    graphics::LayerId layer,
                                                    graphics::MaterialId materialOverride) {
    graphics::EntityId id = createEntity(layer);
    setEntityMesh(id, mesh, materialOverride);
    return id;
}

void ThreeppBackend::setEntityModel(graphics::EntityId entity,
                                    const std::filesystem::path& modelPath,
                                    graphics::MaterialId materialOverride) {
    auto it = entities_.find(entity);
    if (it == entities_.end()) {
        return;
    }

    auto scene = sceneForLayer(it->second.layer);
    glm::vec3 prevPos{};
    glm::quat prevRot{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 prevScale{1.0f};
    if (it->second.object) {
        prevPos = {it->second.object->position.x, it->second.object->position.y, it->second.object->position.z};
        prevRot = {it->second.object->quaternion.w, it->second.object->quaternion.x, it->second.object->quaternion.y, it->second.object->quaternion.z};
        prevScale = {it->second.object->scale.x, it->second.object->scale.y, it->second.object->scale.z};
        scene->remove(*it->second.object);
    }

    threepp::AssimpLoader loader;
    try {
        auto model = loader.load(modelPath);
        model->traverseType<threepp::Mesh>([&](threepp::Mesh& child) {
            child.castShadow = true;
            child.receiveShadow = true;
            if (materialOverride != graphics::kInvalidMaterial) {
                auto overrideMaterial = materialForId(materialOverride);
                const auto& oldMaterials = child.materials();
                if (oldMaterials.size() <= 1) {
                    child.setMaterial(overrideMaterial);
                } else {
                    std::vector<std::shared_ptr<threepp::Material>> newMaterials;
                    newMaterials.resize(oldMaterials.size(), overrideMaterial);
                    child.setMaterials(newMaterials);
                }
            }
        });
        scene->add(model);
        model->position.set(prevPos.x, prevPos.y, prevPos.z);
        model->quaternion.set(prevRot.x, prevRot.y, prevRot.z, prevRot.w);
        model->scale.set(prevScale.x, prevScale.y, prevScale.z);
        it->second.object = model;
    } catch (...) {
        spdlog::error("Graphics: Failed to load model at path {}", modelPath.string());
    }
}

void ThreeppBackend::setEntityMesh(graphics::EntityId entity,
                                   graphics::MeshId mesh,
                                   graphics::MaterialId materialOverride) {
    auto it = entities_.find(entity);
    if (it == entities_.end()) {
        return;
    }

    auto scene = sceneForLayer(it->second.layer);
    glm::vec3 prevPos{};
    glm::quat prevRot{1.0f, 0.0f, 0.0f, 0.0f};
    glm::vec3 prevScale{1.0f};
    if (it->second.object) {
        prevPos = {it->second.object->position.x, it->second.object->position.y, it->second.object->position.z};
        prevRot = {it->second.object->quaternion.w, it->second.object->quaternion.x, it->second.object->quaternion.y, it->second.object->quaternion.z};
        prevScale = {it->second.object->scale.x, it->second.object->scale.y, it->second.object->scale.z};
        scene->remove(*it->second.object);
    }

    auto meshIt = meshes_.find(mesh);
    if (meshIt == meshes_.end()) {
        return;
    }

    auto material = materialForId(materialOverride);
    auto newMesh = threepp::Mesh::create(meshIt->second, material);
    scene->add(newMesh);
    newMesh->position.set(prevPos.x, prevPos.y, prevPos.z);
    newMesh->quaternion.set(prevRot.x, prevRot.y, prevRot.z, prevRot.w);
    newMesh->scale.set(prevScale.x, prevScale.y, prevScale.z);
    it->second.object = newMesh;
}

void ThreeppBackend::destroyEntity(graphics::EntityId entity) {
    auto it = entities_.find(entity);
    if (it == entities_.end()) {
        return;
    }

    auto scene = sceneForLayer(it->second.layer);
    if (it->second.object) {
        scene->remove(*it->second.object);
    }
    entities_.erase(it);
}

graphics::MeshId ThreeppBackend::createMesh(const graphics::MeshData& mesh) {
    const graphics::MeshId id = nextMeshId++;
    auto geometry = threepp::BufferGeometry::create();

    std::vector<float> verts;
    verts.reserve(mesh.vertices.size() * 3);
    for (const auto& v : mesh.vertices) {
        verts.push_back(v.x);
        verts.push_back(v.y);
        verts.push_back(v.z);
    }

    geometry->setAttribute("position", threepp::FloatBufferAttribute::create(verts, 3));
    if (!mesh.indices.empty()) {
        geometry->setIndex(mesh.indices);
    }
    geometry->computeVertexNormals();

    meshes_[id] = geometry;
    return id;
}

void ThreeppBackend::destroyMesh(graphics::MeshId mesh) {
    meshes_.erase(mesh);
}

graphics::MaterialId ThreeppBackend::createMaterial(const graphics::MaterialDesc& material) {
    const graphics::MaterialId id = nextMaterialId++;
    materials_[id] = createMaterialInstance(material);
    return id;
}

void ThreeppBackend::updateMaterial(graphics::MaterialId material, const graphics::MaterialDesc& desc) {
    auto it = materials_.find(material);
    if (it == materials_.end()) {
        return;
    }
    it->second = createMaterialInstance(desc);
}

void ThreeppBackend::destroyMaterial(graphics::MaterialId material) {
    materials_.erase(material);
}

void ThreeppBackend::setMaterialFloat(graphics::MaterialId material, std::string_view name, float value) {
    if (material == graphics::kInvalidMaterial) {
        return;
    }
    auto it = materials_.find(material);
    if (it == materials_.end()) {
        return;
    }
    auto shader = std::dynamic_pointer_cast<threepp::ShaderMaterial>(it->second);
    if (!shader) {
        return;
    }
    shader->uniforms.insert_or_assign(std::string(name), threepp::Uniform(threepp::UniformValue{value}));
    shader->needsUpdate();
}

graphics::RenderTargetId ThreeppBackend::createRenderTarget(const graphics::RenderTargetDesc& desc) {
    const graphics::RenderTargetId id = nextTargetId++;
    threepp::GLRenderTarget::Options opts;
    opts.format = threepp::Format::RGBA;
    opts.depthBuffer = desc.depth;
    opts.stencilBuffer = desc.stencil;
    targets_[id] = std::make_unique<threepp::GLRenderTarget>(desc.width, desc.height, opts);
    return id;
}

void ThreeppBackend::destroyRenderTarget(graphics::RenderTargetId target) {
    targets_.erase(target);
}

void ThreeppBackend::renderLayer(graphics::LayerId layer, graphics::RenderTargetId target) {
    auto scene = sceneForLayer(layer);

    int width = 1;
    int height = 1;
    if (window) {
        window->getFramebufferSize(width, height);
    }
    if (width <= 0) width = 1;
    if (height <= 0) height = 1;

    if (target == graphics::kDefaultRenderTarget) {
        renderer.setRenderTarget(nullptr);
        renderer.setViewport(0, 0, width, height);
        renderer.setClearColor(threepp::Color::skyblue);
        if (activeCamera_) {
            renderer.render(*scene, *activeCamera_);
        }
        return;
    }

    auto it = targets_.find(target);
    if (it == targets_.end()) {
        return;
    }

    auto& rt = it->second;
    renderer.setRenderTarget(rt.get());
    renderer.setViewport(0, 0, static_cast<int>(rt->width), static_cast<int>(rt->height));
    renderer.setClearColor(threepp::Color(0x101018), 0.0f);
    renderer.clear(true, true, true);
    if (activeCamera_) {
        renderer.render(*scene, *activeCamera_);
    }
    renderer.setRenderTarget(nullptr);
}

unsigned int ThreeppBackend::getRenderTargetTextureId(graphics::RenderTargetId target) const {
    if (target == graphics::kDefaultRenderTarget) {
        return 0;
    }
    auto it = targets_.find(target);
    if (it == targets_.end()) {
        return 0;
    }
    if (!it->second->texture) {
        return 0;
    }
    const auto texId = renderer.getGlTextureId(*it->second->texture);
    return texId.value_or(0);
}

void ThreeppBackend::setBrightness(float brightness) {
    const float clamped = std::clamp(brightness, 0.2f, 3.0f);
    renderer.toneMappingExposure = clamped;
}

void ThreeppBackend::setPosition(graphics::EntityId entity, const glm::vec3& position) {
    auto it = entities_.find(entity);
    if (it == entities_.end() || !it->second.object) {
        return;
    }
    it->second.object->position.set(position.x, position.y, position.z);
}

void ThreeppBackend::setRotation(graphics::EntityId entity, const glm::quat& rotation) {
    auto it = entities_.find(entity);
    if (it == entities_.end() || !it->second.object) {
        return;
    }
    it->second.object->quaternion.set(rotation.x, rotation.y, rotation.z, rotation.w);
}

void ThreeppBackend::setScale(graphics::EntityId entity, const glm::vec3& scale) {
    auto it = entities_.find(entity);
    if (it == entities_.end() || !it->second.object) {
        return;
    }
    it->second.object->scale.set(scale.x, scale.y, scale.z);
}

void ThreeppBackend::setVisible(graphics::EntityId entity, bool visible) {
    auto it = entities_.find(entity);
    if (it == entities_.end() || !it->second.object) {
        return;
    }
    it->second.object->visible = visible;
}

void ThreeppBackend::setTransparency(graphics::EntityId entity, bool transparency) {
    auto it = entities_.find(entity);
    if (it == entities_.end() || !it->second.object) {
        return;
    }
    it->second.object->traverse([transparency](threepp::Object3D& obj) {
        if (auto mesh = obj.as<threepp::Mesh>()) {
            for (auto& mat : mesh->materials()) {
                mat->transparent = transparency;
                mat->alphaTest = 0.01f;
                mat->depthWrite = !transparency;
            }
        }
    });
}

void ThreeppBackend::setCameraPosition(const glm::vec3& position) {
    if (activeCamera_) {
        activeCamera_->position.set(position.x, position.y, position.z);
    }
}

void ThreeppBackend::setCameraRotation(const glm::quat& rotation) {
    if (activeCamera_) {
        activeCamera_->quaternion.set(rotation.x, rotation.y, rotation.z, rotation.w);
    }
}

void ThreeppBackend::setPerspective(float fovDegrees, float aspect, float nearPlane, float farPlane) {
    if (!perspectiveCamera_) {
        perspectiveCamera_ = threepp::PerspectiveCamera::create(fovDegrees, aspect, nearPlane, farPlane);
    } else {
        perspectiveCamera_->fov = fovDegrees;
        perspectiveCamera_->aspect = aspect;
        perspectiveCamera_->nearPlane = nearPlane;
        perspectiveCamera_->farPlane = farPlane;
    }
    perspectiveCamera_->updateProjectionMatrix();
    activeCamera_ = perspectiveCamera_;
}

void ThreeppBackend::setOrthographic(float left, float right, float top, float bottom, float nearPlane, float farPlane) {
    if (!orthoCamera_) {
        orthoCamera_ = threepp::OrthographicCamera::create(left, right, top, bottom, nearPlane, farPlane);
    } else {
        orthoCamera_->left = left;
        orthoCamera_->right = right;
        orthoCamera_->top = top;
        orthoCamera_->bottom = bottom;
        orthoCamera_->nearPlane = nearPlane;
        orthoCamera_->farPlane = farPlane;
    }
    orthoCamera_->updateProjectionMatrix();
    activeCamera_ = orthoCamera_;
}

namespace {
glm::mat4 toGlm(const threepp::Matrix4& m) {
    glm::mat4 out{1.0f};
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            out[c][r] = m.elements[c * 4 + r];
        }
    }
    return out;
}
} // namespace

glm::mat4 ThreeppBackend::getViewProjectionMatrix() const {
    if (!activeCamera_) {
        return glm::mat4(1.0f);
    }
    activeCamera_->updateMatrixWorld();
    threepp::Matrix4 viewProj;
    viewProj.multiplyMatrices(activeCamera_->projectionMatrix, activeCamera_->matrixWorldInverse);
    return toGlm(viewProj);
}

glm::mat4 ThreeppBackend::getViewMatrix() const {
    if (!activeCamera_) {
        return glm::mat4(1.0f);
    }
    activeCamera_->updateMatrixWorld();
    return toGlm(activeCamera_->matrixWorldInverse);
}

glm::mat4 ThreeppBackend::getProjectionMatrix() const {
    if (!activeCamera_) {
        return glm::mat4(1.0f);
    }
    return toGlm(activeCamera_->projectionMatrix);
}

glm::vec3 ThreeppBackend::getCameraPosition() const {
    if (!activeCamera_) {
        return {};
    }
    return {activeCamera_->position.x, activeCamera_->position.y, activeCamera_->position.z};
}

glm::vec3 ThreeppBackend::getCameraForward() const {
    if (!activeCamera_) {
        return {0.0f, 0.0f, -1.0f};
    }
    threepp::Vector3 dir;
    activeCamera_->getWorldDirection(dir);
    return {dir.x, dir.y, dir.z};
}

} // namespace graphics_backend
