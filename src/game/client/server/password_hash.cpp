#include "client/server/password_hash.hpp"

#include <openssl/evp.h>
#include <vector>

namespace client::auth {

namespace {
std::string toHexLower(const unsigned char *bytes, std::size_t size) {
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.resize(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        unsigned char value = bytes[i];
        out[i * 2] = kHex[(value >> 4) & 0x0F];
        out[i * 2 + 1] = kHex[value & 0x0F];
    }
    return out;
}
} // namespace

bool HashPasswordPBKDF2Sha256(const std::string &password,
                              const std::string &salt,
                              std::string &outHexDigest) {
    constexpr int kIterations = 100000;
    constexpr std::size_t kDigestLen = 32;
    std::vector<unsigned char> digest(kDigestLen, 0);

    int ok = PKCS5_PBKDF2_HMAC(
        password.data(),
        static_cast<int>(password.size()),
        reinterpret_cast<const unsigned char *>(salt.data()),
        static_cast<int>(salt.size()),
        kIterations,
        EVP_sha256(),
        static_cast<int>(digest.size()),
        digest.data());
    if (!ok) {
        return false;
    }

    outHexDigest = toHexLower(digest.data(), digest.size());
    return true;
}

} // namespace client::auth
