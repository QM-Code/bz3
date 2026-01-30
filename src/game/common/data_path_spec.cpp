#include "game/common/data_path_spec.hpp"

#include "common/data_path_resolver.hpp"
#include <spdlog/spdlog.h>

namespace game_common {

void ConfigureDataPathSpec() {
    karma::data::DataPathSpec spec;
    spec.appName = "bz3";
    spec.dataDirEnvVar = "KARMA_DATA_DIR";
    spec.requiredDataMarker = "common/config.json";
    karma::data::SetDataPathSpec(spec);

    const auto userConfigPath = karma::data::EnsureUserConfigFile("config.json");
    spec.fallbackAssetLayers = {
        {"common/config.json", "data/common/config.json", spdlog::level::err, false},
        {"client/config.json", "data/client/config.json", spdlog::level::debug, false},
        {userConfigPath, "user config", spdlog::level::debug, false}
    };

    karma::data::SetDataPathSpec(spec);
}

} // namespace game_common
