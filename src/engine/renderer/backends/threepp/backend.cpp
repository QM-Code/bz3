#include "renderer/backends/threepp/backend.hpp"

#include "renderer/render.hpp"
#include "spdlog/spdlog.h"
#include "platform/window.hpp"
#include "threepp/loaders/AssimpLoader.hpp"
#include <threepp/geometries/BoxGeometry.hpp>
#include <threepp/geometries/CircleGeometry.hpp>
#include <threepp/materials/LineBasicMaterial.hpp>
#include <threepp/materials/MeshBasicMaterial.hpp>
#include <threepp/materials/ShaderMaterial.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

namespace {
std::string readFileToString(const std::filesystem::path& path) {
    if (path.empty()) return {};

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        spdlog::error("Render: Failed to open file '{}'", path.string());
        return {};
    }

    return std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
}
} // namespace

namespace render_backend {

ThreeppBackend::ThreeppBackend(platform::Window& windowRef)
    : window(&windowRef),
      renderer({1, 1}) {
    spdlog::trace("Render: init start");

    scene = threepp::Scene::create();
    radarScene = threepp::Scene::create();

    int fbWidth = 800, fbHeight = 600;
    window->getFramebufferSize(fbWidth, fbHeight);
    if (fbHeight == 0) fbHeight = 1;
    renderer.setSize({fbWidth, fbHeight});

    camera = threepp::PerspectiveCamera::create(
        CAMERA_FOV,
        static_cast<float>(fbWidth) / static_cast<float>(fbHeight),
        0.1f,
        1000.f
    );
    camera->updateProjectionMatrix();

    {
        constexpr unsigned int radarTexSize = 512 * 2;
        constexpr float radarOrthoHalfSize = 40.0f;
        constexpr float radarNear = 0.1f;
        constexpr float radarFar = 500.0f;

        radarCamera = threepp::OrthographicCamera::create(
            -radarOrthoHalfSize,
            radarOrthoHalfSize,
            radarOrthoHalfSize,
            -radarOrthoHalfSize,
            radarNear,
            radarFar
        );
        radarCamera->updateProjectionMatrix();

        threepp::GLRenderTarget::Options opts;
        opts.format = threepp::Format::RGBA;
        opts.depthBuffer = true;
        opts.stencilBuffer = false;
        radarRenderTarget = std::make_unique<threepp::GLRenderTarget>(radarTexSize, radarTexSize, opts);
    }

    renderer.setClearColor(threepp::Color(0x3399ff));
    renderer.shadowMap().enabled = true;
    renderer.shadowMap().type = threepp::ShadowMap::PFCSoft;
    renderer.toneMapping = threepp::ToneMapping::ACESFilmic;

    auto light = threepp::AmbientLight::create(0xffffff, 0.5f);
    scene->add(light);

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
    scene->add(dir);

    radarMaterial = threepp::ShaderMaterial::create();
    radarMaterial->transparent = true;
    radarMaterial->depthWrite = false;
    radarMaterial->wireframe = false;
    radarMaterial->uniforms.insert_or_assign("playerY", threepp::Uniform(threepp::UniformValue{0.0f}));
    radarMaterial->uniforms.insert_or_assign("jumpHeight", threepp::Uniform(threepp::UniformValue{5.0f}));

    {
        auto geom = threepp::BoxGeometry::create(radarFOVBeamWidth, 0.2f, radarFOVBeamLength);
        auto mat = threepp::MeshBasicMaterial::create();
        mat->color = threepp::Color(0xffffff);
        mat->depthTest = false;
        mat->depthWrite = false;

        radarFOVLeft = threepp::Mesh::create(geom, mat);
        radarFOVRight = threepp::Mesh::create(geom, mat);

        radarFOVLeft->renderOrder = 10000;
        radarFOVRight->renderOrder = 10000;

        radarScene->add(radarFOVLeft);
        radarScene->add(radarFOVRight);

        setRadarFOVLinesAngle(CAMERA_FOV);
    }
}

ThreeppBackend::~ThreeppBackend() {
    while (!objects.empty()) {
        destroy(objects.begin()->first);
    }
}

void ThreeppBackend::resizeCallback(int width, int height) {
    renderer.setSize({width, height});
    camera->aspect = static_cast<float>(width) / static_cast<float>(height);
    camera->updateProjectionMatrix();
}

void ThreeppBackend::update() {
    int width = 0;
    int height = 0;
    if (window) {
        window->getFramebufferSize(width, height);
    }
    if (width <= 0) width = 1;
    if (height <= 0) height = 1;
    renderer.setSize({width, height});

    if (radarCamera && radarRenderTarget) {
        constexpr float radarHeightAbovePlayer = 60.0f;

        const glm::vec3 p = radarAnchorPosition;

        glm::vec3 forward = glm::mat3_cast(radarAnchorRotation) * glm::vec3(0.0f, 0.0f, -1.0f);
        forward.y = 0.0f;
        const float len2 = glm::dot(forward, forward);
        if (len2 < 1e-6f) {
            forward = glm::vec3(0.0f, 0.0f, -1.0f);
        } else {
            forward *= 1.0f / std::sqrt(len2);
        }

        radarCamera->position.set(p.x, p.y + radarHeightAbovePlayer, p.z);
        radarCamera->up.set(forward.x, forward.y, forward.z);
        radarCamera->lookAt(threepp::Vector3(p.x, p.y, p.z));

        if (radarMaterial) {
            radarMaterial->uniforms["playerY"].setValue(threepp::UniformValue{p.y});
        }

        renderer.setRenderTarget(radarRenderTarget.get());
        renderer.setViewport(0, 0, static_cast<int>(radarRenderTarget->width), static_cast<int>(radarRenderTarget->height));
        renderer.setClearColor(threepp::Color(0x101018), 0.0f);
        renderer.clear(true, true, true);
        renderer.render(*radarScene, *radarCamera);
        renderer.setRenderTarget(nullptr);

        if (radarRenderTarget->texture) {
            const auto texId = renderer.getGlTextureId(*radarRenderTarget->texture);
            radarTextureId = texId.value_or(0);
        } else {
            radarTextureId = 0;
        }
    }

    renderer.setRenderTarget(nullptr);
    renderer.setViewport(0, 0, width, height);
    renderer.setClearColor(threepp::Color::skyblue);
    renderer.render(*scene, *camera);
}

render_id ThreeppBackend::create() {
    static render_id nextId = 1;
    render_id id = nextId++;
    return id;
}

render_id ThreeppBackend::create(std::string modelPath, bool addToRadar) {
    render_id id = create();
    setModel(id, modelPath, addToRadar);
    spdlog::trace("Render::create: Created object with render_id {}", id);
    return id;
}

void ThreeppBackend::setModel(render_id id, const std::filesystem::path& modelPath, bool addToRadar) {
    threepp::AssimpLoader loader;
    const auto modelPathStr = modelPath.string();

    try {
        auto model = loader.load(modelPath);
        spdlog::trace("Render::create: Model loaded successfully from path {}", modelPathStr);
        model->traverseType<threepp::Mesh>([&](threepp::Mesh& child) {
            child.castShadow = true;
            child.receiveShadow = true;
        });
        scene->add(model);
        spdlog::trace("Render::create: Model added to scene from path {}", modelPathStr);
        objects[id] = model;

        if (addToRadar) {
            auto radarModel = model->clone<threepp::Group>(true);

            radarModel->traverseType<threepp::Mesh>([&](threepp::Mesh& mesh) {
                mesh.castShadow = false;
                mesh.receiveShadow = false;

                const auto& oldMaterials = mesh.materials();
                if (oldMaterials.size() <= 1) {
                    mesh.setMaterial(radarMaterial);
                } else {
                    std::vector<std::shared_ptr<threepp::Material>> newMaterials;
                    newMaterials.resize(oldMaterials.size(), radarMaterial);
                    mesh.setMaterials(newMaterials);
                }
            });

            radarScene->add(radarModel);
            radarObjects[id] = radarModel;
        }
    } catch (...) {
        spdlog::error("Render::create: Failed to load model at path {}", modelPathStr);
    }
}

void ThreeppBackend::setRadarCircleGraphic(render_id id, float radius) {
    auto circleGeom = threepp::CircleGeometry::create(radius, 64);
    auto circleMat = threepp::MeshBasicMaterial::create();
    circleMat->color = threepp::Color(0xffffff);
    circleMat->wireframe = true;
    circleMat->transparent = true;
    circleMat->opacity = 1.0f;
    circleMat->depthTest = false;
    circleMat->depthWrite = false;

    auto circleMesh = threepp::Mesh::create(circleGeom, circleMat);
    circleMesh->rotation.x = -1.57079632679f;
    circleMesh->renderOrder = 10000;

    auto circleGroup = threepp::Group::create();
    circleGroup->add(circleMesh);

    radarScene->add(circleGroup);
    radarObjects[id] = circleGroup;
}

void ThreeppBackend::setRadarFOVLinesAngle(float fovDegrees) {
    int fbWidth = 1, fbHeight = 1;
    if (window) {
        window->getFramebufferSize(fbWidth, fbHeight);
        if (fbHeight == 0) fbHeight = 1;
    }
    const float aspect = static_cast<float>(fbWidth) / static_cast<float>(fbHeight);

    const float length = 80.0f;
    const float halfVertRad = glm::radians(fovDegrees * 0.5f);
    const float halfHorizRad = std::atan(std::tan(halfVertRad) * aspect);

    radarFOVBeamLength = length;

    if (radarFOVLeft) {
        const glm::quat yaw = glm::angleAxis(halfHorizRad, glm::vec3(0, 1, 0));
        const glm::quat combined = radarAnchorRotation * yaw;
        const glm::vec3 offset = combined * glm::vec3(0.0f, 0.0f, -radarFOVBeamLength * 0.5f);

        radarFOVLeft->quaternion.set(combined.x, combined.y, combined.z, combined.w);
        radarFOVLeft->position.set(radarAnchorPosition.x + offset.x,
                                   radarAnchorPosition.y + offset.y,
                                   radarAnchorPosition.z + offset.z);
    }

    if (radarFOVRight) {
        const glm::quat yaw = glm::angleAxis(-halfHorizRad, glm::vec3(0, 1, 0));
        const glm::quat combined = radarAnchorRotation * yaw;
        const glm::vec3 offset = combined * glm::vec3(0.0f, 0.0f, -radarFOVBeamLength * 0.5f);

        radarFOVRight->quaternion.set(combined.x, combined.y, combined.z, combined.w);
        radarFOVRight->position.set(radarAnchorPosition.x + offset.x,
                                    radarAnchorPosition.y + offset.y,
                                    radarAnchorPosition.z + offset.z);
    }
}

void ThreeppBackend::destroy(render_id id) {
    auto it = objects.find(id);
    if (it != objects.end()) {
        scene->remove(*(it->second));
        objects.erase(it);
    }

    auto rit = radarObjects.find(id);
    if (rit != radarObjects.end()) {
        radarScene->remove(*(rit->second));
        radarObjects.erase(rit);
    }
}

void ThreeppBackend::setPosition(render_id id, const glm::vec3& position) {
    auto rit = radarObjects.find(id);
    if (rit != radarObjects.end()) {
        rit->second->position.set(position.x, position.y, position.z);
    }

    auto it = objects.find(id);
    if (it != objects.end()) {
        it->second->position.set(position.x, position.y, position.z);
    }
}

void ThreeppBackend::setRotation(render_id id, const glm::quat& rotation) {
    auto rit = radarObjects.find(id);
    if (rit != radarObjects.end()) {
        rit->second->quaternion.set(rotation.x, rotation.y, rotation.z, rotation.w);
    }

    auto it = objects.find(id);
    if (it != objects.end()) {
        it->second->quaternion.set(rotation.x, rotation.y, rotation.z, rotation.w);
    }
}

void ThreeppBackend::setScale(render_id id, const glm::vec3& scale) {
    auto rit = radarObjects.find(id);
    if (rit != radarObjects.end()) {
        rit->second->scale.set(scale.x, scale.y, scale.z);
    }

    auto it = objects.find(id);
    if (it != objects.end()) {
        it->second->scale.set(scale.x, scale.y, scale.z);
    }
}

void ThreeppBackend::setVisible(render_id id, bool visible) {
    auto rit = radarObjects.find(id);
    if (rit != radarObjects.end()) {
        rit->second->visible = visible;
    }

    auto it = objects.find(id);
    if (it != objects.end()) {
        it->second->visible = visible;
    }
}

void ThreeppBackend::setTransparency(render_id id, bool transparency) {
    auto it = objects.find(id);
    if (it != objects.end()) {
        auto* object = it->second.get();
        object->traverse([transparency](threepp::Object3D& obj) {
            if (auto mesh = obj.as<threepp::Mesh>()) {
                for (auto& mat : mesh->materials()) {
                    mat->transparent = transparency;
                    mat->alphaTest = 0.01f;
                    mat->depthWrite = false;
                }
            }
        });
    }
}

void ThreeppBackend::setCameraPosition(const glm::vec3& position) {
    camera->position.set(position.x, position.y, position.z);
    radarAnchorPosition = position;
}

void ThreeppBackend::setCameraRotation(const glm::quat& rotation) {
    camera->quaternion.set(rotation.x, rotation.y, rotation.z, rotation.w);
    radarAnchorRotation = rotation;
}

void ThreeppBackend::setRadarShaderPath(const std::filesystem::path& vertPath,
                                        const std::filesystem::path& fragPath) {
    const auto vertSrc = readFileToString(vertPath);
    const auto fragSrc = readFileToString(fragPath);

    radarMaterial->vertexShader = vertSrc;
    radarMaterial->fragmentShader = fragSrc;
    radarMaterial->needsUpdate();
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
    const_cast<threepp::PerspectiveCamera*>(camera.get())->updateMatrixWorld();
    threepp::Matrix4 viewProj;
    viewProj.multiplyMatrices(camera->projectionMatrix, camera->matrixWorldInverse);
    return toGlm(viewProj);
}

glm::mat4 ThreeppBackend::getViewMatrix() const {
    const_cast<threepp::PerspectiveCamera*>(camera.get())->updateMatrixWorld();
    return toGlm(camera->matrixWorldInverse);
}

glm::mat4 ThreeppBackend::getProjectionMatrix() const {
    return toGlm(camera->projectionMatrix);
}

glm::vec3 ThreeppBackend::getCameraPosition() const {
    return {camera->position.x, camera->position.y, camera->position.z};
}

glm::vec3 ThreeppBackend::getCameraForward() const {
    threepp::Vector3 dir;
    const_cast<threepp::PerspectiveCamera*>(camera.get())->getWorldDirection(dir);
    return {dir.x, dir.y, dir.z};
}

} // namespace render_backend
