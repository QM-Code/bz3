#pragma once

namespace ui {

class RenderSettings {
public:
    static constexpr float kMinBrightness = 1.0f;
    static constexpr float kMaxBrightness = 3.0f;

    void loadFromConfig();
    bool saveToConfig() const;
    void reset();
    bool setBrightness(float value, bool fromUser);
    bool setVsync(bool value, bool fromUser);
    float brightness() const;
    bool vsync() const;
    bool consumeDirty();
    void clearDirty();
    static bool eraseFromConfig();

private:
    static float clampBrightness(float value);

    float brightnessValue = 1.0f;
    bool vsyncValue = true;
    bool dirty = false;
};

} // namespace ui
