// Effekseer-backed particle system implementation.

#include "render/particle_effect_system.hpp"

#include <Effekseer/Effekseer.h>
#include <EffekseerRendererGL/EffekseerRendererGL.h>

#include "spdlog/spdlog.h"

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <stdexcept>

namespace {
Effekseer::Vector3D toEffekseer(const glm::vec3& v) {
	return Effekseer::Vector3D{v.x, v.y, v.z};
}

Effekseer::Matrix44 toEffekseer(const glm::mat4& m) {
	Effekseer::Matrix44 out{};
	const float* p = glm::value_ptr(m);
	for (int r = 0; r < 4; ++r) {
		for (int c = 0; c < 4; ++c) {
			out.Values[r][c] = p[r * 4 + c];
		}
	}
	return out;
}

glm::vec3 safeForward(const glm::vec3& dir) {
	const float lenSq = glm::dot(dir, dir);
	if (lenSq < 1e-6f) {
		return glm::vec3(0.0f, 0.0f, -1.0f);
	}
	return glm::normalize(dir);
}

std::u16string toU16(const std::string& s) {
	std::u16string out;
	out.reserve(s.size());
	for (unsigned char c : s) {
		out.push_back(static_cast<char16_t>(c));
	}
	return out;
}

std::string u16ToUtf8(const char16_t* s) {
	// Effekseer gives us UTF-16; we assume ASCII paths in this project.
	std::string out;
	for (const char16_t* p = s; *p != 0; ++p) {
		out.push_back(static_cast<char>(*p));
	}
	return out;
}

class RootedFileInterface : public Effekseer::FileInterface {
public:
	explicit RootedFileInterface(std::filesystem::path baseDir)
		: baseDir_(std::move(baseDir)), fallback_(Effekseer::MakeRefPtr<Effekseer::DefaultFileInterface>()) {}

	Effekseer::FileReaderRef OpenRead(const char16_t* path) override {
		std::filesystem::path p(u16ToUtf8(path));
		if (!p.is_absolute()) {
			p = baseDir_ / p;
		}
		const auto u16 = toU16(p.string());
		if (!std::filesystem::exists(p)) {
			spdlog::warn("Effekseer FileInterface: missing '{}'", p.string());
			// Try rebasing known asset folders (Texture/, mqo/) back to the effect directory when exports keep original author paths.
			std::filesystem::path rebased;
			for (auto it = p.begin(); it != p.end(); ++it) {
				if (*it == "Texture" || *it == "mqo") {
					auto suffix = std::filesystem::path{};
					for (auto jt = it; jt != p.end(); ++jt) {
						suffix /= *jt;
					}
					rebased = baseDir_ / suffix;
					break;
				}
			}

			if (!rebased.empty() && std::filesystem::exists(rebased)) {
				spdlog::info("Effekseer FileInterface: rebasing '{}' -> '{}'", p.string(), rebased.string());
				const auto rebasedU16 = toU16(rebased.string());
				spdlog::trace("Effekseer FileInterface: open '{}'", rebased.string());
				return fallback_->OpenRead(rebasedU16.c_str());
			}
		}
		spdlog::trace("Effekseer FileInterface: open '{}'", p.string());
		return fallback_->OpenRead(u16.c_str());
	}

	Effekseer::FileWriterRef OpenWrite(const char16_t* path) override {
		std::filesystem::path p(u16ToUtf8(path));
		if (!p.is_absolute()) {
			p = baseDir_ / p;
		}
		const auto u16 = toU16(p.string());
		return fallback_->OpenWrite(u16.c_str());
	}

private:
	std::filesystem::path baseDir_;
	Effekseer::FileInterfaceRef fallback_;
};

glm::vec3 quatToEulerXYZ(const glm::quat& q) {
	// Convert quaternion to Euler angles (roll=X, pitch=Y, yaw=Z) without glm experimental headers.
	const float ysqr = q.y * q.y;

	const float t0 = +2.0f * (q.w * q.x + q.y * q.z);
	const float t1 = +1.0f - 2.0f * (q.x * q.x + ysqr);
	const float roll = std::atan2(t0, t1);

	float t2 = +2.0f * (q.w * q.y - q.z * q.x);
	t2 = std::clamp(t2, -1.0f, 1.0f);
	const float pitch = std::asin(t2);

	const float t3 = +2.0f * (q.w * q.z + q.x * q.y);
	const float t4 = +1.0f - 2.0f * (ysqr + q.z * q.z);
	const float yaw = std::atan2(t3, t4);

	return {roll, pitch, yaw};
}

const char* toString(Effekseer::LogType type) {
	switch (type) {
	case Effekseer::LogType::Error:
		return "Error";
	case Effekseer::LogType::Warning:
		return "Warning";
	case Effekseer::LogType::Info:
		return "Info";
	default:
		return "Unknown";
	}
}

} // namespace

struct ParticleEffectData {
	Effekseer::ManagerRef manager;
	Effekseer::EffectRef effect;
	Effekseer::Handle handle = -1;
};

struct ParticleEngine::Impl {
	EffekseerRendererGL::RendererRef renderer;
	Effekseer::ManagerRef manager;
	std::unordered_map<std::string, Effekseer::EffectRef> effectCache;

	Impl();
	~Impl() = default;

	std::shared_ptr<ParticleEffectData> createEffect(const std::string& filepath, float sizeFactor);
	void update(float deltaSeconds);
	void render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPosition, const glm::vec3& cameraFront);
};

ParticleEngine::Impl::Impl() {
	// Renderer setup (OpenGL 3 assumed to match threepp usage).
	renderer = EffekseerRendererGL::Renderer::Create(10000, EffekseerRendererGL::OpenGLDeviceType::OpenGL3, true);
	if (renderer.Get() == nullptr) {
		throw std::runtime_error("ParticleEngine: Failed to create Effekseer GL renderer");
	}
	renderer->SetRestorationOfStatesFlag(true);

	// Manager setup.
	manager = Effekseer::Manager::Create(10000);
	if (manager.Get() == nullptr) {
		throw std::runtime_error("ParticleEngine: Failed to create Effekseer manager");
	}

	auto setting = Effekseer::Setting::Create();
	setting->SetCoordinateSystem(Effekseer::CoordinateSystem::RH);
	manager->SetSetting(setting);

	// Connect renderers.
	manager->SetSpriteRenderer(renderer->CreateSpriteRenderer());
	manager->SetRibbonRenderer(renderer->CreateRibbonRenderer());
	manager->SetRingRenderer(renderer->CreateRingRenderer());
	manager->SetTrackRenderer(renderer->CreateTrackRenderer());
	manager->SetModelRenderer(renderer->CreateModelRenderer());

	Effekseer::SetLogger([](Effekseer::LogType type, const std::string& msg) {
		// Map Effekseer log levels into spdlog. Use trace to always surface the message when verbose mode is on.
		spdlog::level::level_enum level = spdlog::level::info;
		if (type == Effekseer::LogType::Warning) level = spdlog::level::warn;
		else if (type == Effekseer::LogType::Error) level = spdlog::level::err;
		else level = spdlog::level::trace;
		spdlog::log(level, "[Effekseer][{}] {}", toString(type), msg);
	});
}

std::shared_ptr<ParticleEffectData> ParticleEngine::Impl::createEffect(const std::string& filepath, float sizeFactor) {
    if (manager.Get() == nullptr) {
        throw std::runtime_error("ParticleEngine: Manager not initialized");
    }

    if (!std::filesystem::exists(filepath)) {
        spdlog::error("ParticleEngine: Effect path does not exist '{}'", filepath);
        throw std::runtime_error("ParticleEngine: Effect file missing");
    }

    const auto filepathAbs = std::filesystem::absolute(filepath);
    const auto filepathAbsStr = filepathAbs.string();

    // Base directory containing efkefc + its textures/models/materials.
    const auto effectDir = filepathAbs.parent_path();
    auto fileInterface = Effekseer::MakeRefPtr<RootedFileInterface>(effectDir);

    // IMPORTANT: set loaders on the MANAGER, not only on Setting.
    manager->SetTextureLoader(renderer->CreateTextureLoader(fileInterface));
    manager->SetModelLoader(renderer->CreateModelLoader(fileInterface));
    manager->SetMaterialLoader(renderer->CreateMaterialLoader(fileInterface));
    manager->SetEffectLoader(Effekseer::Effect::CreateEffectLoader(fileInterface));
    manager->SetCurveLoader(Effekseer::MakeRefPtr<Effekseer::CurveLoader>(fileInterface));

    Effekseer::EffectRef effect;
    if (auto it = effectCache.find(filepathAbsStr); it != effectCache.end()) {
        effect = it->second;
    } else {
        // Use the ABSOLUTE path here so the EffectLoader doesn’t depend on cwd.
        const auto path16 = toU16(filepathAbsStr);
        effect = Effekseer::Effect::Create(manager, path16.c_str());
        if (effect.Get() == nullptr) {
            spdlog::error("ParticleEngine: Failed to load effect '{}'", filepathAbsStr);
            throw std::runtime_error("ParticleEngine: Effect load failure");
        }
        spdlog::trace("ParticleEngine: Loaded effect '{}'", filepathAbsStr);
        effectCache.emplace(filepathAbsStr, effect);
    }

    const Effekseer::Handle handle = manager->Play(effect, 0.0f, 0.0f, 0.0f);
    if (handle < 0) {
        spdlog::error("ParticleEngine: Failed to play effect '{}'", filepathAbsStr);
        throw std::runtime_error("ParticleEngine: Play failure");
    }
	// Uniformly scale the effect if requested.
	manager->SetScale(handle, sizeFactor, sizeFactor, sizeFactor);
    spdlog::trace("ParticleEngine: Playing effect '{}' handle={}", filepathAbsStr, handle);

    auto data = std::make_shared<ParticleEffectData>();
    data->manager = manager;
    data->effect  = effect;
    data->handle  = handle;
    return data;
}


void ParticleEngine::Impl::update(float deltaSeconds) {
	if (manager.Get() == nullptr) return;

	Effekseer::Manager::UpdateParameter params{};
	params.DeltaFrame = deltaSeconds <= 0.0f ? 1.0f : deltaSeconds * 60.0f; // Effekseer works in frames; scale seconds to frames at 60 FPS.
	params.UpdateInterval = 1.0f;
	params.SyncUpdate = true;
	manager->Update(params);
}

void ParticleEngine::Impl::render(const glm::mat4& view,
								   const glm::mat4& projection,
								   const glm::vec3& cameraPosition,
								   const glm::vec3& cameraFront) {
	if (manager.Get() == nullptr || renderer.Get() == nullptr) return;

	Effekseer::Matrix44 efkProj = toEffekseer(projection);
	Effekseer::Matrix44 efkView = toEffekseer(view);
	Effekseer::Matrix44 efkViewProj = toEffekseer(projection * view);

	Effekseer::Manager::DrawParameter drawParam{};
	drawParam.ViewProjectionMatrix = efkViewProj;
	drawParam.ZNear = 0.1f;
	drawParam.ZFar = 1000.0f;
	drawParam.CameraPosition = toEffekseer(cameraPosition);
	drawParam.CameraFrontDirection = toEffekseer(safeForward(cameraFront));
	drawParam.CameraCullingMask = ~0;

	// Feed separate matrices to the renderer.
	renderer->SetProjectionMatrix(efkProj);
	renderer->SetCameraMatrix(efkView);

	renderer->BeginRendering();
	manager->Draw(drawParam);
	renderer->EndRendering();
}

// ParticleEffect -----------------------------------------------------------------

ParticleEffect::ParticleEffect(std::shared_ptr<ParticleEffectData> data) : data_(std::move(data)) {}

void ParticleEffect::setPosition(glm::vec3 position) {
	if (!data_ || data_->manager.Get() == nullptr) {
		spdlog::warn("ParticleEffect: setPosition called on invalid effect");
		return;
	}
	data_->manager->SetLocation(data_->handle, position.x, position.y, position.z);
}

void ParticleEffect::setRotation(glm::quat rotation) {
	if (!data_ || data_->manager.Get() == nullptr) {
		spdlog::warn("ParticleEffect: setRotation called on invalid effect");
		return;
	}
	const glm::vec3 euler = quatToEulerXYZ(rotation);
	data_->manager->SetRotation(data_->handle, euler.x, euler.y, euler.z);
}

void ParticleEffect::stop() {
	if (!data_ || data_->manager.Get() == nullptr) return;
	data_->manager->StopEffect(data_->handle);
	data_.reset();
}

// ParticleEngine -----------------------------------------------------------------

ParticleEngine::ParticleEngine() : impl_(std::make_unique<Impl>()) {}

ParticleEngine::~ParticleEngine() = default;

ParticleEffect ParticleEngine::createEffect(const std::string& filepath, float sizeFactor) {
	return ParticleEffect{impl_->createEffect(filepath, sizeFactor)};
}

void ParticleEngine::update(float deltaSeconds) {
	impl_->update(deltaSeconds);
}

void ParticleEngine::render(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPosition, const glm::vec3& cameraFront) {
	impl_->render(view, projection, cameraPosition, cameraFront);
}
