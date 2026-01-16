#include "engine/components/render.hpp"
#include "threepp/loaders/AssimpLoader.hpp"
#include "spdlog/spdlog.h"
#include "engine/user_pointer.hpp"
#include <threepp/materials/ShaderMaterial.hpp>
#include <threepp/materials/MeshBasicMaterial.hpp>
#include <threepp/geometries/CircleGeometry.hpp>
#include <fstream>
#include <iterator>
#include <filesystem>
#include <array>
#include <algorithm>
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

Render::Render(GLFWwindow *window) :
    window(window),
    renderer({1, 1}) {

    scene = threepp::Scene::create();
    radarScene = threepp::Scene::create();

    int fbWidth = 800, fbHeight = 600;
    glfwGetFramebufferSize(window, &fbWidth, &fbHeight);
    if (fbHeight == 0) fbHeight = 1; // guard against zero height
    renderer.setSize({fbWidth, fbHeight});

    camera = threepp::PerspectiveCamera::create(
        CAMERA_FOV,
        static_cast<float>(fbWidth) / static_cast<float>(fbHeight),
        0.1f,
        1000.f
    );
    camera->updateProjectionMatrix();

    // Radar camera + offscreen render target
    {
        constexpr unsigned int radarTexSize = 512;
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
        // Must be RGBA to preserve the shader's alpha output (used when compositing the radar texture).
        opts.format = threepp::Format::RGBA;
        opts.depthBuffer = true;
        opts.stencilBuffer = false;
        radarRenderTarget = std::make_unique<threepp::GLRenderTarget>(radarTexSize, radarTexSize, opts);
    }

    // Setup resize callback
    auto* userPointer = static_cast<GLFWUserPointer*>(glfwGetWindowUserPointer(window));
    userPointer->resizeCallback = [this](int width, int height) {
        this->resizeCallback(width, height);
    };
    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, int width, int height) {
        auto* userPointer = static_cast<GLFWUserPointer*>(glfwGetWindowUserPointer(w));
        userPointer->resizeCallback(width, height);
    });
    
    renderer.setClearColor(threepp::Color(0x3399ff));
    renderer.shadowMap().enabled = true;
    renderer.shadowMap().type = threepp::ShadowMap::PFCSoft;
    renderer.toneMapping = threepp::ToneMapping::ACESFilmic;

    auto light = threepp::AmbientLight::create(0xffffff, 0.5f);
    scene->add(light);

    auto dir = threepp::DirectionalLight::create(threepp::Color(0xffffff), 1.0f); // white, full intensity
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
}

Render::~Render() {
    while (!objects.empty()) {
        destroy(objects.begin()->first);
    }
}

void Render::resizeCallback(int width, int height) {
    renderer.setSize({width, height});
    camera->aspect = static_cast<float>(width) / static_cast<float>(height);
    camera->updateProjectionMatrix();
}

void Render::update() {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    renderer.setSize({width, height});

    // Radar render (offscreen)
    if (radarCamera && radarRenderTarget) {
        constexpr float radarHeightAbovePlayer = 60.0f;

        const glm::vec3 p = radarAnchorPosition;

        // Rotate the radar view around the Y axis to match the player's facing direction,
        // while keeping the camera looking straight down.
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

render_id Render::create(std::string modelPath, float radius) {
    spdlog::trace("Render::create: Loading model from path {}", modelPath);

    // Load model and add to scene
    static render_id nextId = 1;
    render_id id = nextId++;

    threepp::AssimpLoader loader;

    try {
        auto model = loader.load(modelPath);
        spdlog::trace("Render::create: Model loaded successfully from path {}", modelPath);
        model->traverseType<threepp::Mesh>([&](threepp::Mesh& child) {
            child.castShadow = true;
            child.receiveShadow = true;
        });
        scene->add(model);
        spdlog::trace("Render::create: Model added to scene from path {}", modelPath);
        objects[id] = model;

        if (radius > 0.0f) {
            // Only add a circle overlay to the radar (no model clone).
            auto circleGeom = threepp::CircleGeometry::create(radius, 64);
            auto circleMat = threepp::MeshBasicMaterial::create();
            circleMat->color = threepp::Color(0xffffff);
            circleMat->wireframe = true;
            circleMat->transparent = true;
            circleMat->opacity = 1.0f;
            circleMat->depthTest = false;
            circleMat->depthWrite = false;

            auto circleMesh = threepp::Mesh::create(circleGeom, circleMat);
            circleMesh->rotation.x = -1.57079632679f; // -90deg to lay flat in XZ
            circleMesh->renderOrder = 10000; // ensure on top

            auto circleGroup = threepp::Group::create();
            circleGroup->add(circleMesh);

            radarScene->add(circleGroup);
            radarObjects[id] = circleGroup;
        } else {
            // Also add a clone of this model to the radar scene.
            // (A threepp Object3D can only have one parent.)
            auto radarModel = model->clone<threepp::Group>(true);

            // Apply radar shader to every mesh in the radar scene.
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
        spdlog::error("Render::create: Failed to load model at path {}", modelPath);
    }

    spdlog::trace("Render::create: Created object with render_id {}", id);
    
    return id;
}

void Render::destroy(render_id id) {
    auto it = objects.find(id);
    if (it != objects.end()) {
        scene->remove(*(it->second));
        objects.erase(it);

        auto rit = radarObjects.find(id);
        if (rit != radarObjects.end()) {
            radarScene->remove(*(rit->second));
            radarObjects.erase(rit);
        }
    } else {
        spdlog::error("Render::destroy: Invalid render_id {}", id);
    }
}

void Render::setPosition(render_id id, const glm::vec3 &position) {
    auto it = objects.find(id);
    if (it != objects.end()) {
        it->second->position.set(position.x, position.y, position.z);

        auto rit = radarObjects.find(id);
        if (rit != radarObjects.end()) {
            rit->second->position.set(position.x, position.y, position.z);
        }
    } else {
        spdlog::error("Render::setPosition: Invalid render_id {}", id);
    }
}

void Render::setRotation(render_id id, const glm::quat &rotation) {
    auto it = objects.find(id);
    if (it != objects.end()) {
        it->second->quaternion.set(rotation.x, rotation.y, rotation.z, rotation.w);

        auto rit = radarObjects.find(id);
        if (rit != radarObjects.end()) {
            rit->second->quaternion.set(rotation.x, rotation.y, rotation.z, rotation.w);
        }
    } else {
        spdlog::error("Render::setRotation: Invalid render_id {}", id);
    }
}

void Render::setScale(render_id id, const glm::vec3 &scale) {
    auto it = objects.find(id);
    if (it != objects.end()) {
        it->second->scale.set(scale.x, scale.y, scale.z);

        auto rit = radarObjects.find(id);
        if (rit != radarObjects.end()) {
            rit->second->scale.set(scale.x, scale.y, scale.z);
        }
    } else {
        spdlog::error("Render::setScale: Invalid render_id {}", id);
    }
}

void Render::setVisible(render_id id, bool visible) {
    auto it = objects.find(id);
    if (it != objects.end()) {
        it->second->visible = visible;

        auto rit = radarObjects.find(id);
        if (rit != radarObjects.end()) {
            rit->second->visible = visible;
        }
    } else {
        spdlog::error("Render::setVisible: Invalid render_id {}", id);
    }
}

void Render::setTransparency(render_id id, bool transparency) {
    auto it = objects.find(id);
    if (it != objects.end()) {
        // First get the Object3D
        auto *object = it->second.get();
        object->traverse([](threepp::Object3D& obj) {
            // Then traverse to find all Meshes and set their material transparencymodel->traverse([](threepp::Object3D& obj) {
            if (auto mesh = obj.as<threepp::Mesh>()) {
                for (auto& mat : mesh->materials()) {
                    mat->transparent = true;
                    if (true) {
                        mat->alphaTest = 0.01f; // important
                        mat->depthWrite = false; // prevents sorting artifacts
                    } else {
                        mat->alphaTest = 0.0f;
                        mat->depthWrite = true;
                    }
                }
            }
        });
    } else {
        spdlog::error("Render::setTransparency: Invalid render_id {}", id);
    }
}

void Render::setCameraPosition(const glm::vec3 &position) {
    camera->position.set(position.x, position.y, position.z);
    radarAnchorPosition = position;
}

void Render::setCameraRotation(const glm::quat &rotation) {
    camera->quaternion.set(rotation.x, rotation.y, rotation.z, rotation.w);
    radarAnchorRotation = rotation;
}

void Render::setRadarShaderPath(const std::filesystem::path& vertPath,
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
    // threepp stores column-major in elements[col*4 + row]
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            out[c][r] = m.elements[c * 4 + r];
        }
    }
    return out;
}
} // namespace

glm::mat4 Render::getViewProjectionMatrix() const {
    // Ensure matrices are up to date
    const_cast<threepp::PerspectiveCamera*>(camera.get())->updateMatrixWorld();
    threepp::Matrix4 viewProj;
    viewProj.multiplyMatrices(camera->projectionMatrix, camera->matrixWorldInverse);
    return toGlm(viewProj);
}

glm::mat4 Render::getViewMatrix() const {
    const_cast<threepp::PerspectiveCamera*>(camera.get())->updateMatrixWorld();
    return toGlm(camera->matrixWorldInverse);
}

glm::mat4 Render::getProjectionMatrix() const {
    return toGlm(camera->projectionMatrix);
}

glm::vec3 Render::getCameraPosition() const {
    return {camera->position.x, camera->position.y, camera->position.z};
}

glm::vec3 Render::getCameraForward() const {
    threepp::Vector3 dir;
    const_cast<threepp::PerspectiveCamera*>(camera.get())->getWorldDirection(dir);
    return {dir.x, dir.y, dir.z};
}