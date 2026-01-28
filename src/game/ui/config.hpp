#pragma once

#include <string>

namespace ui {
namespace config {

float GetRequiredFloat(const char* path);
std::string GetRequiredString(const char* path);
bool GetRequiredBool(const char* path);

} // namespace config
} // namespace ui
