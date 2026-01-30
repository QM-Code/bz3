#pragma once

#include <string>

#include "ui/models/settings_model.hpp"

namespace ui {

class SettingsController {
public:
    explicit SettingsController(SettingsModel &model);

    std::string getConfiguredLanguage() const;
    bool setLanguage(const std::string &code, std::string *error);
    bool saveHudSettings(std::string *error);
    bool saveRenderSettings(std::string *error);

private:
    SettingsModel &model;
};

} // namespace ui
