#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace karma::file {

std::vector<uint8_t> ReadFileBytes(const std::filesystem::path &path);

} // namespace karma::file
