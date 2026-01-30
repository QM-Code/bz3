#pragma once

#include <string>

#include "ui/models/bindings_model.hpp"

namespace ui {

class BindingsController {
public:
    struct Result {
        bool ok = true;
        std::string status;
        bool statusIsError = false;
    };

    explicit BindingsController(BindingsModel &model);

    Result loadFromConfig();
    Result saveToConfig();
    Result resetToDefaults();

private:
    BindingsModel &model;
};

} // namespace ui
