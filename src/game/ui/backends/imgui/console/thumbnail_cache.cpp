#include "ui/backends/imgui/console/thumbnail_cache.hpp"

#include <curl/curl.h>
#include <imgui_impl_opengl3_loader.h>
#include <spdlog/spdlog.h>

#include "common/curl_global.hpp"

#include <stb_image.h>

namespace {
using stbi_uc = ::stbi_uc;
using ::stbi_image_free;
using ::stbi_load_from_memory;

size_t CurlWriteToVector(char *ptr, size_t size, size_t nmemb, void *userdata) {
    auto *buffer = static_cast<std::vector<unsigned char> *>(userdata);
    const size_t total = size * nmemb;
    buffer->insert(buffer->end(), reinterpret_cast<unsigned char *>(ptr), reinterpret_cast<unsigned char *>(ptr) + total);
    return total;
}
} // namespace

namespace ui {

ThumbnailCache::~ThumbnailCache() {
    shutdown();
}

ThumbnailTexture *ThumbnailCache::getOrLoad(const std::string &url) {
    if (url.empty()) {
        return nullptr;
    }

    auto it = cache.find(url);
    if (it != cache.end()) {
        if (it->second.textureId != 0 || it->second.failed || it->second.loading) {
            return &it->second;
        }
    }

    ThumbnailTexture &entry = cache[url];
    if (!entry.loading && entry.textureId == 0 && !entry.failed) {
        entry.loading = true;
        queueRequest(url);
    }
    return &entry;
}

void ThumbnailCache::processUploads() {
    std::deque<ThumbnailPayload> localResults;
    {
        std::lock_guard<std::mutex> lock(mutex);
        localResults.swap(results);
    }

    for (auto &payload : localResults) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            inFlight.erase(payload.url);
        }

        auto &entry = cache[payload.url];
        entry.loading = false;

        if (payload.failed || payload.pixels.empty() || payload.width <= 0 || payload.height <= 0) {
            entry.failed = true;
            continue;
        }

        GLuint textureId = 0;
        glGenTextures(1, &textureId);
        glBindTexture(GL_TEXTURE_2D, textureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, payload.width, payload.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, payload.pixels.data());
        glBindTexture(GL_TEXTURE_2D, 0);

        entry.textureId = textureId;
        entry.width = payload.width;
        entry.height = payload.height;
        entry.failed = false;
    }
}

void ThumbnailCache::shutdown() {
    stopWorker();
    clearTextures();
}

void ThumbnailCache::clearTextures() {
    for (auto &[url, thumb] : cache) {
        if (thumb.textureId != 0) {
            glDeleteTextures(1, &thumb.textureId);
        }
    }
    cache.clear();
}

void ThumbnailCache::startWorker() {
    if (worker.joinable()) {
        return;
    }

    workerStop = false;
    worker = std::thread(&ThumbnailCache::workerProc, this);
}

void ThumbnailCache::stopWorker() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        workerStop = true;
        requests.clear();
        inFlight.clear();
        results.clear();
    }
    cv.notify_all();
    if (worker.joinable()) {
        worker.join();
    }
}

void ThumbnailCache::queueRequest(const std::string &url) {
    startWorker();
    std::lock_guard<std::mutex> lock(mutex);
    if (!inFlight.insert(url).second) {
        return;
    }
    requests.push_back(url);
    cv.notify_one();
}

void ThumbnailCache::workerProc() {
    auto fetchBytes = [](const std::string &url, std::vector<unsigned char> &body) {
        if (!bz::net::EnsureCurlGlobalInit()) {
            return false;
        }

        CURL *curlHandle = curl_easy_init();
        if (!curlHandle) {
            return false;
        }

        curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curlHandle, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curlHandle, CURLOPT_TIMEOUT, 5L);
        curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, CurlWriteToVector);
        curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &body);

        CURLcode result = curl_easy_perform(curlHandle);
        long status = 0;
        if (result == CURLE_OK) {
            curl_easy_getinfo(curlHandle, CURLINFO_RESPONSE_CODE, &status);
        }
        curl_easy_cleanup(curlHandle);

        if (result != CURLE_OK || status < 200 || status >= 300 || body.empty()) {
            return false;
        }

        return true;
    };

    while (true) {
        std::string url;
        {
            std::unique_lock<std::mutex> lock(mutex);
            cv.wait(lock, [&]() { return workerStop || !requests.empty(); });
            if (workerStop) {
                return;
            }
            url = std::move(requests.front());
            requests.pop_front();
        }

        ThumbnailPayload payload;
        payload.url = url;
        payload.failed = true;

        std::vector<unsigned char> body;
        if (fetchBytes(url, body)) {
            int width = 0;
            int height = 0;
            int channels = 0;
            stbi_uc *pixels = stbi_load_from_memory(body.data(), static_cast<int>(body.size()), &width, &height, &channels, 4);
            if (pixels && width > 0 && height > 0) {
                payload.width = width;
                payload.height = height;
                payload.failed = false;
                payload.pixels.assign(pixels, pixels + (width * height * 4));
            }
            if (pixels) {
                stbi_image_free(pixels);
            }
        }

        {
            std::lock_guard<std::mutex> lock(mutex);
            results.push_back(std::move(payload));
        }
    }
}

} // namespace ui
