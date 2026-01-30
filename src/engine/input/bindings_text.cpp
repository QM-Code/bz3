#include "engine/input/bindings_text.hpp"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace input::bindings {
namespace {

std::string TrimCopy(const std::string &text) {
    auto begin = text.begin();
    auto end = text.end();
    while (begin != end && std::isspace(static_cast<unsigned char>(*begin))) {
        ++begin;
    }
    while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1)))) {
        --end;
    }
    return std::string(begin, end);
}

} // namespace

bool IsMouseBindingName(std::string_view name) {
    std::string upper;
    upper.reserve(name.size());
    for (const char ch : name) {
        upper.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    }
    if (upper.rfind("MOUSE", 0) == 0) {
        return true;
    }
    if (upper.size() >= 6 && upper.substr(upper.size() - 6) == "_MOUSE") {
        return true;
    }
    return false;
}

std::string JoinBindings(const std::vector<std::string> &entries) {
    std::ostringstream oss;
    bool first = true;
    for (const auto &item : entries) {
        if (!first) {
            oss << ", ";
        }
        oss << item;
        first = false;
    }
    return oss.str();
}

std::vector<std::string> SplitBindings(const std::string &text) {
    std::vector<std::string> entries;
    std::stringstream stream(text);
    std::string token;
    while (std::getline(stream, token, ',')) {
        std::string trimmed = TrimCopy(token);
        if (!trimmed.empty()) {
            entries.push_back(trimmed);
        }
    }
    return entries;
}

} // namespace input::bindings
