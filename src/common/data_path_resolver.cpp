#include "common/data_path_resolver.hpp"

#include <array>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <map>
#include <utility>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <sstream>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

namespace {

std::filesystem::path TryCanonical(const std::filesystem::path &path) {
    std::error_code ec;
    auto result = std::filesystem::weakly_canonical(path, ec);
    if (!ec) {
        return result;
    }

    result = std::filesystem::absolute(path, ec);
    if (!ec) {
        return result;
    }

    return path;
}

std::string SanitizePathComponent(std::string_view value) {
    std::string sanitized;
    sanitized.reserve(value.size());

    for (char ch : value) {
        if (std::isalnum(static_cast<unsigned char>(ch)) || ch == '.' || ch == '-' || ch == '_') {
            sanitized.push_back(ch);
        } else {
            sanitized.push_back('_');
        }
    }

    if (sanitized.empty()) {
        sanitized = "server";
    }

    return sanitized;
}

std::filesystem::path ExecutableDirectory() {
#if defined(_WIN32)
    std::array<char, MAX_PATH> buffer{};
    const DWORD length = GetModuleFileNameA(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0 || length == buffer.size()) {
        return std::filesystem::current_path();
    }
    return std::filesystem::path(buffer.data(), buffer.data() + length).parent_path();
#elif defined(__APPLE__)
    std::array<char, PATH_MAX> buffer{};
    uint32_t size = static_cast<uint32_t>(buffer.size());
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        buffer.fill('\0');
        if (size > buffer.size()) {
            return std::filesystem::current_path();
        }
        if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
            return std::filesystem::current_path();
        }
    }
    return TryCanonical(std::filesystem::path(buffer.data())).parent_path();
#else
    std::array<char, PATH_MAX> buffer{};
    const ssize_t length = ::readlink("/proc/self/exe", buffer.data(), buffer.size());
    if (length <= 0 || static_cast<size_t>(length) >= buffer.size()) {
        return std::filesystem::current_path();
    }
    return TryCanonical(std::filesystem::path(buffer.data(), buffer.data() + length)).parent_path();
#endif
}

std::mutex g_dataRootMutex;
std::optional<std::filesystem::path> g_dataRootOverride;
bool g_dataRootInitialized = false;

std::filesystem::path ValidateDataRootCandidate(const std::filesystem::path &path) {
    const auto canonical = TryCanonical(path);
    std::error_code ec;
    if (!std::filesystem::exists(canonical, ec) || !std::filesystem::is_directory(canonical, ec)) {
        throw std::runtime_error("data_path_resolver: Data directory is invalid: " + canonical.string());
    }

    const auto commonConfig = canonical / "common" / "config.json";
    if (!std::filesystem::exists(commonConfig, ec) || !std::filesystem::is_regular_file(commonConfig, ec)) {
        throw std::runtime_error("Invalid data directory: " + canonical.string() + "\n" + commonConfig.string() + " does not exist.");
    }
    return canonical;
}

std::filesystem::path DetectDataRoot(const std::optional<std::filesystem::path> &overridePath) {
    if (overridePath) {
        return ValidateDataRootCandidate(*overridePath);
    }

    const char *envDataDir = std::getenv("BZ3_DATA_DIR");
    if (!envDataDir || *envDataDir == '\0') {
        throw std::runtime_error("BZ3_DATA_DIR environment variable must be set to the game data directory");
    }

    return ValidateDataRootCandidate(envDataDir);
}

struct ConfigCacheState {
    std::mutex mutex;
    bool initialized = false;
    std::vector<bz::data::ConfigLayer> layers;
    nlohmann::json merged = nlohmann::json::object();
    std::unordered_map<std::string, std::size_t> labelIndex;
    std::unordered_map<std::string, std::filesystem::path> assetLookup;
};

ConfigCacheState g_configCache;

const nlohmann::json *ResolveConfigPath(const nlohmann::json &root, const std::string &path) {
    if (path.empty()) {
        return &root;
    }

    const nlohmann::json *current = &root;
    std::size_t position = 0;

    while (position < path.size()) {
        const std::size_t dot = path.find('.', position);
        const bool lastSegment = (dot == std::string::npos);
        const std::string segment = path.substr(position, lastSegment ? std::string::npos : dot - position);
        if (segment.empty()) {
            return nullptr;
        }

        std::string key = segment;
        std::optional<std::size_t> arrayIndex;
        const auto bracketPos = segment.find('[');
        if (bracketPos != std::string::npos) {
            key = segment.substr(0, bracketPos);
            const auto closingPos = segment.find(']', bracketPos);
            if (closingPos == std::string::npos || closingPos != segment.size() - 1) {
                return nullptr;
            }

            const std::string indexText = segment.substr(bracketPos + 1, closingPos - bracketPos - 1);
            if (indexText.empty()) {
                return nullptr;
            }

            try {
                arrayIndex = static_cast<std::size_t>(std::stoul(indexText));
            } catch (...) {
                return nullptr;
            }
        }

        if (!key.empty()) {
            if (!current->is_object()) {
                return nullptr;
            }

            const auto it = current->find(key);
            if (it == current->end()) {
                return nullptr;
            }

            current = &(*it);
        }

        if (arrayIndex.has_value()) {
            if (!current->is_array() || *arrayIndex >= current->size()) {
                return nullptr;
            }

            current = &((*current)[*arrayIndex]);
        }

        if (lastSegment) {
            break;
        }

        position = dot + 1;
    }

    return current;
}

} // namespace

namespace bz::data {

std::filesystem::path ExecutableDirectory() {
    return ::ExecutableDirectory();
}

const std::filesystem::path &DataRoot() {
    static std::once_flag initFlag;
    static std::filesystem::path root;

    std::call_once(initFlag, [] {
        std::optional<std::filesystem::path> overrideCopy;
        {
            std::lock_guard<std::mutex> lock(g_dataRootMutex);
            overrideCopy = g_dataRootOverride;
        }

        root = DetectDataRoot(overrideCopy);

        std::lock_guard<std::mutex> lock(g_dataRootMutex);
        g_dataRootInitialized = true;
    });

    return root;
}

void SetDataRootOverride(const std::filesystem::path &path) {
    std::lock_guard<std::mutex> lock(g_dataRootMutex);
    if (g_dataRootInitialized) {
        throw std::runtime_error("data_path_resolver: Data root already initialized; override must be set earlier");
    }

    g_dataRootOverride = ValidateDataRootCandidate(path);
}

std::filesystem::path Resolve(const std::filesystem::path &relativePath) {
    if (relativePath.is_absolute()) {
        return TryCanonical(relativePath);
    }

    return TryCanonical(DataRoot() / relativePath);
}

std::filesystem::path ResolveWithBase(const std::filesystem::path &baseDir, const std::string &value) {
    std::filesystem::path candidate(value);
    if (!candidate.is_absolute()) {
        candidate = baseDir / candidate;
    }
    return TryCanonical(candidate);
}

std::filesystem::path UserConfigDirectory() {
    static const std::filesystem::path dir = [] {
        std::filesystem::path base;

#if defined(_WIN32)
        if (const char *appData = std::getenv("APPDATA"); appData && *appData) {
            base = appData;
        } else if (const char *userProfile = std::getenv("USERPROFILE"); userProfile && *userProfile) {
            base = std::filesystem::path(userProfile) / "AppData" / "Roaming";
        }
#elif defined(__APPLE__)
        if (const char *home = std::getenv("HOME"); home && *home) {
            base = std::filesystem::path(home) / "Library" / "Application Support";
        }
#else
        if (const char *xdg = std::getenv("XDG_CONFIG_HOME"); xdg && *xdg) {
            base = xdg;
        } else if (const char *home = std::getenv("HOME"); home && *home) {
            base = std::filesystem::path(home) / ".config";
        }
#endif

        if (base.empty()) {
            throw std::runtime_error("Unable to determine user configuration directory: no home path detected");
        }

        return TryCanonical(base / "bz3");
    }();

    return dir;
}

std::filesystem::path EnsureUserConfigFile(const std::string &fileName) {
    const auto configDir = UserConfigDirectory();

    std::error_code ec;
    std::filesystem::create_directories(configDir, ec);
    if (ec) {
        throw std::runtime_error("Failed to create user config directory " + configDir.string() + ": " + ec.message());
    }

    const auto filePath = configDir / fileName;
    if (!std::filesystem::exists(filePath)) {
        std::ofstream stream(filePath);
        if (!stream) {
            throw std::runtime_error("Failed to create user config file " + filePath.string());
        }

        stream << "{}\n";
        if (!stream) {
            throw std::runtime_error("Failed to initialize user config file " + filePath.string());
        }
    } else if (std::filesystem::is_regular_file(filePath)) {
        std::error_code sizeEc;
        const auto fileSize = std::filesystem::file_size(filePath, sizeEc);
        if (!sizeEc && fileSize == 0) {
            std::ofstream stream(filePath, std::ios::trunc);
            if (!stream) {
                throw std::runtime_error("Failed to truncate empty user config file " + filePath.string());
            }

            stream << "{}\n";
            if (!stream) {
                throw std::runtime_error("Failed to initialize truncated user config file " + filePath.string());
            }
        }
    }

    return TryCanonical(filePath);
}

std::filesystem::path EnsureUserWorldsDirectory() {
    const auto baseDir = UserConfigDirectory();
    const auto worldsDir = baseDir / "worlds";

    std::error_code ec;
    std::filesystem::create_directories(worldsDir, ec);
    if (ec) {
        throw std::runtime_error("Failed to create user worlds directory " + worldsDir.string() + ": " + ec.message());
    }

    return TryCanonical(worldsDir);
}

std::filesystem::path EnsureUserWorldDirectoryForServer(const std::string &host, uint16_t port) {
    const auto baseDir = EnsureUserWorldsDirectory();
    const auto sanitizedHost = SanitizePathComponent(host);

    std::ostringstream name;
    name << sanitizedHost << '.' << port;

    const auto serverDir = baseDir / name.str();

    std::error_code ec;
    std::filesystem::create_directories(serverDir, ec);
    if (ec) {
        throw std::runtime_error("Failed to create server world directory " + serverDir.string() + ": " + ec.message());
    }

    return TryCanonical(serverDir);
}

namespace {
std::unordered_map<std::string, std::filesystem::path> BuildAssetLookupFromLayers(const std::vector<ConfigLayer> &layers);
}

bool MergeConfigLayer(const std::string &label,
                      const nlohmann::json &layerJson,
                      const std::filesystem::path &baseDir) {
    std::filesystem::path canonicalBase = TryCanonical(baseDir);
    const std::string resolvedLabel = label.empty() ? canonicalBase.string() : label;

    if (!layerJson.is_object()) {
        spdlog::warn("data_path_resolver: Config layer '{}' ignored because it is not a JSON object", resolvedLabel);
        return false;
    }

    std::lock_guard<std::mutex> lock(g_configCache.mutex);
    if (!g_configCache.initialized) {
        spdlog::warn("data_path_resolver: Config cache not initialized; cannot merge layer '{}'", resolvedLabel);
        return false;
    }

    ConfigLayer newLayer{layerJson, canonicalBase, resolvedLabel};

    auto labelIt = g_configCache.labelIndex.find(resolvedLabel);
    if (labelIt != g_configCache.labelIndex.end()) {
        g_configCache.layers[labelIt->second] = newLayer;
    } else {
        g_configCache.labelIndex[resolvedLabel] = g_configCache.layers.size();
        g_configCache.layers.push_back(newLayer);
    }

    g_configCache.merged = nlohmann::json::object();
    for (const auto &layer : g_configCache.layers) {
        MergeJsonObjects(g_configCache.merged, layer.json);
    }

    g_configCache.assetLookup = BuildAssetLookupFromLayers(g_configCache.layers);

    spdlog::debug("data_path_resolver: Merged config layer '{}' from {}", resolvedLabel, canonicalBase.string());
    return true;
}

bool MergeExternalConfigLayer(const std::filesystem::path &configPath,
                              const std::string &label,
                              spdlog::level::level_enum missingLevel) {
    const auto canonicalPath = TryCanonical(configPath);
    const auto jsonOpt = LoadJsonFile(canonicalPath,
                                      label.empty() ? canonicalPath.string() : label,
                                      missingLevel);
    if (!jsonOpt) {
        return false;
    }

    return MergeConfigLayer(label, *jsonOpt, canonicalPath.parent_path());
}

std::optional<nlohmann::json> LoadJsonFile(const std::filesystem::path &path,
                                           const std::string &label,
                                           spdlog::level::level_enum missingLevel) {
    if (!std::filesystem::exists(path)) {
        spdlog::log(missingLevel, "data_path_resolver: {} not found: {}", label, path.string());
        return std::nullopt;
    }

    std::ifstream stream(path);
    if (!stream) {
        spdlog::error("data_path_resolver: Failed to open {}: {}", label, path.string());
        return std::nullopt;
    }

    try {
        nlohmann::json json;
        stream >> json;
        return json;
    } catch (const std::exception &e) {
        spdlog::error("data_path_resolver: Failed to parse {}: {}", label, e.what());
        return std::nullopt;
    }
}

std::vector<ConfigLayer> LoadConfigLayers(const std::vector<ConfigLayerSpec> &specs) {
    std::vector<ConfigLayer> layers;
    layers.reserve(specs.size());

    for (const auto &spec : specs) {
        const auto absolutePath = Resolve(spec.relativePath);
        const std::string label = spec.label.empty() ? spec.relativePath.string() : spec.label;
        auto jsonOpt = LoadJsonFile(absolutePath, label, spec.missingLevel);
        if (!jsonOpt) {
            if (spec.required) {
                spdlog::error("data_path_resolver: Required config missing: {}", absolutePath.string());
            }
            continue;
        }

        if (!jsonOpt->is_object()) {
            spdlog::warn("data_path_resolver: Config {} is not a JSON object, skipping", absolutePath.string());
            continue;
        }

        layers.push_back({std::move(*jsonOpt), absolutePath.parent_path(), label});
    }

    return layers;
}

void MergeJsonObjects(nlohmann::json &destination, const nlohmann::json &source) {
    if (!destination.is_object() || !source.is_object()) {
        destination = source;
        return;
    }

    for (auto it = source.begin(); it != source.end(); ++it) {
        const auto &key = it.key();
        const auto &value = it.value();

        if (value.is_object() && destination.contains(key) && destination[key].is_object()) {
            MergeJsonObjects(destination[key], value);
        } else {
            destination[key] = value;
        }
    }
}

void CollectAssetEntries(const nlohmann::json &node,
                         const std::filesystem::path &baseDir,
                         std::map<std::string, std::filesystem::path> &assetMap,
                         const std::string &prefix) {
    if (!node.is_object()) {
        return;
    }

    for (const auto &[key, value] : node.items()) {
        const std::string fullKey = prefix.empty() ? key : prefix + "." + key;
        if (value.is_string()) {
            assetMap[fullKey] = ResolveWithBase(baseDir, value.get<std::string>());
        } else if (value.is_object()) {
            CollectAssetEntries(value, baseDir, assetMap, fullKey);
        }
    }
}

namespace {

std::unordered_map<std::string, std::filesystem::path> BuildAssetLookupFromLayers(const std::vector<ConfigLayer> &layers) {
    std::map<std::string, std::filesystem::path> flattened;

    for (const auto &layer : layers) {
        if (!layer.json.is_object()) {
            continue;
        }

        const auto assetsIt = layer.json.find("assets");
        if (assetsIt != layer.json.end()) {
            if (!assetsIt->is_object()) {
                spdlog::warn("data_path_resolver: 'assets' in {} is not an object; skipping", layer.baseDir.string());
            } else {
                CollectAssetEntries(*assetsIt, layer.baseDir, flattened);
            }
        }

        const auto fontsIt = layer.json.find("fonts");
        if (fontsIt != layer.json.end()) {
            if (!fontsIt->is_object()) {
                spdlog::warn("data_path_resolver: 'fonts' in {} is not an object; skipping", layer.baseDir.string());
            } else {
                CollectAssetEntries(*fontsIt, layer.baseDir, flattened, "fonts");
            }
        }
    }

    std::unordered_map<std::string, std::filesystem::path> lookup;
    lookup.reserve(flattened.size() * 2);

    for (const auto &[key, resolvedPath] : flattened) {
        lookup[key] = resolvedPath;

        const auto separator = key.find_last_of('.');
        if (separator != std::string::npos) {
            lookup[key.substr(separator + 1)] = resolvedPath;
        }
    }

    return lookup;
}

} // namespace

std::filesystem::path ResolveConfiguredAsset(const std::string &assetKey,
                                             const std::filesystem::path &defaultRelativePath) {
    const auto defaultPath = defaultRelativePath.empty() ? std::filesystem::path{} : Resolve(defaultRelativePath);

    {
        std::lock_guard<std::mutex> lock(g_configCache.mutex);
        if (g_configCache.initialized) {
            const auto it = g_configCache.assetLookup.find(assetKey);
            if (it != g_configCache.assetLookup.end()) {
                return it->second;
            }
        }
    }

    static std::once_flag fallbackLoadFlag;
    static std::unordered_map<std::string, std::filesystem::path> fallbackLookup;
    std::call_once(fallbackLoadFlag, [] {
        const auto userConfigPath = EnsureUserConfigFile("config.json");

        const std::vector<ConfigLayerSpec> specs = {
            {"common/config.json", "data/common/config.json", spdlog::level::err, false},
            {"client/config.json", "data/client/config.json", spdlog::level::debug, false},
            {userConfigPath, "user config", spdlog::level::debug, false}
        };

        const auto layers = LoadConfigLayers(specs);
        fallbackLookup = BuildAssetLookupFromLayers(layers);
    });

    if (const auto it = fallbackLookup.find(assetKey); it != fallbackLookup.end()) {
        return it->second;
    }

    spdlog::warn("data_path_resolver: Asset '{}' not found in configuration layers, using default.", assetKey);
    return defaultPath;
}

void InitializeConfigCache(const std::vector<ConfigLayerSpec> &specs) {
    std::vector<ConfigLayer> layers = LoadConfigLayers(specs);

    nlohmann::json merged = nlohmann::json::object();
    for (const auto &layer : layers) {
        MergeJsonObjects(merged, layer.json);
    }

    std::unordered_map<std::string, std::size_t> labelIndex;
    labelIndex.reserve(layers.size());
    for (std::size_t i = 0; i < layers.size(); ++i) {
        if (!layers[i].label.empty()) {
            labelIndex[layers[i].label] = i;
        }
    }

    auto assetLookup = BuildAssetLookupFromLayers(layers);

    std::lock_guard<std::mutex> lock(g_configCache.mutex);
    g_configCache.layers = std::move(layers);
    g_configCache.merged = std::move(merged);
    g_configCache.labelIndex = std::move(labelIndex);
    g_configCache.assetLookup = std::move(assetLookup);
    g_configCache.initialized = true;
}

bool ConfigCacheInitialized() {
    std::lock_guard<std::mutex> lock(g_configCache.mutex);
    return g_configCache.initialized;
}

const nlohmann::json &ConfigCacheRoot() {
    std::lock_guard<std::mutex> lock(g_configCache.mutex);
    if (!g_configCache.initialized) {
        static const nlohmann::json empty = nlohmann::json::object();
        return empty;
    }

    return g_configCache.merged;
}

const nlohmann::json *ConfigLayerByLabel(const std::string &label) {
    std::lock_guard<std::mutex> lock(g_configCache.mutex);
    if (!g_configCache.initialized) {
        return nullptr;
    }

    const auto it = g_configCache.labelIndex.find(label);
    if (it == g_configCache.labelIndex.end()) {
        return nullptr;
    }

    return &g_configCache.layers[it->second].json;
}

const nlohmann::json *ConfigValue(const std::string &path) {
    std::lock_guard<std::mutex> lock(g_configCache.mutex);
    if (!g_configCache.initialized) {
        return nullptr;
    }

    return ResolveConfigPath(g_configCache.merged, path);
}

std::optional<nlohmann::json> ConfigValueCopy(const std::string &path) {
    if (const auto *value = ConfigValue(path)) {
        return std::optional<nlohmann::json>(std::in_place, *value);
    }
    return std::nullopt;
}

std::optional<uint16_t> ConfigValueUInt16(const std::string &path) {
    const auto *value = ConfigValue(path);
    if (!value) {
        return std::nullopt;
    }

    auto clampToUint16 = [](long long number) -> std::optional<uint16_t> {
        if (number < 0 || number > std::numeric_limits<uint16_t>::max()) {
            return std::nullopt;
        }
        return static_cast<uint16_t>(number);
    };

    if (value->is_number_unsigned()) {
        return clampToUint16(static_cast<long long>(value->get<unsigned long long>()));
    }

    if (value->is_number_integer()) {
        return clampToUint16(static_cast<long long>(value->get<long long>()));
    }

    if (value->is_string()) {
        try {
            return clampToUint16(std::stoll(value->get<std::string>()));
        } catch (...) {
            return std::nullopt;
        }
    }

    return std::nullopt;
}

std::optional<std::string> ConfigValueString(const std::string &path) {
    const auto *value = ConfigValue(path);
    if (!value || !value->is_string()) {
        return std::nullopt;
    }
    return value->get<std::string>();
}

} // namespace bz::data
