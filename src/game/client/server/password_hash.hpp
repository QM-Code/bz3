#pragma once

#include <string>

namespace client::auth {
bool HashPasswordPBKDF2Sha256(const std::string &password,
                              const std::string &salt,
                              std::string &outHexDigest);
}
