#include "game/common/data_path_spec.hpp"

#include "common/data_path_resolver.hpp"
#include <spdlog/spdlog.h>

namespace game_common {

void ConfigureDataPathSpec() {
    bz::data::DataPathSpec spec;
    spec.appName = "bz3";
    spec.dataDirEnvVar = "BZ3_DATA_DIR";
    spec.requiredDataMarker = "common/config.json";
    bz::data::SetDataPathSpec(spec);

    const auto userConfigPath = bz::data::EnsureUserConfigFile("config.json");
    spec.fallbackAssetLayers = {
        {"common/config.json", "data/common/config.json", spdlog::level::err, false},
        {"client/config.json", "data/client/config.json", spdlog::level::debug, false},
        {userConfigPath, "user config", spdlog::level::debug, false}
    };

    bz::data::SetDataPathSpec(spec);
}

} // namespace game_common
