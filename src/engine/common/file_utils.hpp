#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace bz::file {

std::vector<uint8_t> ReadFileBytes(const std::filesystem::path &path);

} // namespace bz::file
