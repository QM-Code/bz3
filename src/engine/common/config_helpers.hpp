#pragma once

#include <initializer_list>
#include <string>

#include <cstdint>

namespace bz::config {

bool ReadBoolConfig(std::initializer_list<const char*> paths, bool defaultValue);
uint16_t ReadUInt16Config(std::initializer_list<const char*> paths, uint16_t defaultValue);
float ReadFloatConfig(std::initializer_list<const char*> paths, float defaultValue);
std::string ReadStringConfig(const char *path, const std::string &defaultValue);

} // namespace bz::config
