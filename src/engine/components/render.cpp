#include "engine/components/render.hpp"
#include "threepp/loaders/AssimpLoader.hpp"
#include "spdlog/spdlog.h"
#include "engine/user_pointer.hpp"
#include <array>

Render::Render(GLFWwindow *window) :
    window(window),
    renderer({1, 1}) {

    scene = threepp::Scene::create();

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
}

Render::~Render() {
    for (auto& [id, object] : objects) {
        destroy(id);
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
    renderer.setRenderTarget(nullptr);
    renderer.setViewport(0, 0, width, height);
    renderer.setClearColor(threepp::Color::skyblue);
    renderer.render(*scene, *camera);
}

render_id Render::create(std::string modelPath) {
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
    } else {
        spdlog::error("Render::destroy: Invalid render_id {}", id);
    }
}

void Render::setPosition(render_id id, const glm::vec3 &position) {
    auto it = objects.find(id);
    if (it != objects.end()) {
        it->second->position.set(position.x, position.y, position.z);
    } else {
        spdlog::error("Render::setPosition: Invalid render_id {}", id);
    }
}

void Render::setRotation(render_id id, const glm::quat &rotation) {
    auto it = objects.find(id);
    if (it != objects.end()) {
        it->second->quaternion.set(rotation.x, rotation.y, rotation.z, rotation.w);
    } else {
        spdlog::error("Render::setRotation: Invalid render_id {}", id);
    }
}

void Render::setScale(render_id id, const glm::vec3 &scale) {
    auto it = objects.find(id);
    if (it != objects.end()) {
        it->second->scale.set(scale.x, scale.y, scale.z);
    } else {
        spdlog::error("Render::setScale: Invalid render_id {}", id);
    }
}

void Render::setVisible(render_id id, bool visible) {
    auto it = objects.find(id);
    if (it != objects.end()) {
        it->second->visible = visible;
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
}

void Render::setCameraRotation(const glm::quat &rotation) {
    camera->quaternion.set(rotation.x, rotation.y, rotation.z, rotation.w);
}