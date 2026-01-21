#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>

#include <imgui.h>

namespace gui {

class RichTextRenderer {
public:
    struct FontSpec {
        std::string path;
        float size = 0.0f;
    };

    enum class FontRole {
        Regular,
        Title,
        Heading
    };

    struct TextStyle {
        FontRole role = FontRole::Regular;
        float size = 0.0f;
        ImVec4 color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    };

    struct InlineLayout {
        ImVec2 start;
        ImVec2 cursor;
        float maxWidth = 0.0f;
        float lineHeight = 0.0f;
        float lineSpacing = 0.0f;
    };

    struct Rect {
        ImVec2 min;
        ImVec2 max;
    };

    RichTextRenderer();
    ~RichTextRenderer();

    bool initialize(const FontSpec &regular,
                    const FontSpec &title,
                    const FontSpec &heading,
                    const FontSpec &emoji);
    void shutdown();
    bool isInitialized() const;

    float getLineHeight(const TextStyle &style) const;

    void drawInline(ImDrawList *drawList,
                    InlineLayout &layout,
                    const std::string &utf8,
                    const TextStyle &style,
                    std::vector<Rect> *outRects = nullptr);

    unsigned char *allocateSvgBuffer(std::size_t bytes);
    bool copySvgCache(const unsigned char *svgData,
                      std::size_t length,
                      int width,
                      int height,
                      unsigned char *dest,
                      int destPitch);
    void storeSvgCache(const unsigned char *svgData,
                       std::size_t length,
                       int width,
                       int height,
                       const unsigned char *src,
                       int srcPitch);

private:
    struct FontFace;
    struct FontInstance;
    struct Glyph {
        unsigned int texture = 0;
        int width = 0;
        int height = 0;
        int bearingX = 0;
        int bearingY = 0;
        float advance = 0.0f;
        bool color = false;
        ImVec2 uv0{};
        ImVec2 uv1{};
    };

    FontFace *loadFace(const std::string &path, int faceIndex);
    FontInstance *getInstance(FontFace *face, int pixelSize);
    FontFace *pickFace(FontRole role, uint32_t codepoint);
    Glyph *getGlyph(FontInstance *instance, uint32_t glyphIndex, bool &isColor);
    void debugLogEmojiGlyph(uint32_t codepoint, FontRole role, float pixelSize);
    float measureRun(FontInstance *instance, const std::string &utf8) const;
    float drawRun(ImDrawList *drawList,
                  FontInstance *instance,
                  float baselineY,
                  float startX,
                  const std::string &utf8,
                  const ImVec4 &color,
                  float &outMaxTop,
                  float &outMaxBottom);
    float drawTextWithFallback(ImDrawList *drawList,
                               FontRole role,
                               float pixelSize,
                               float baselineY,
                               float startX,
                               const std::string &utf8,
                               const ImVec4 &color,
                               float &outMaxTop,
                               float &outMaxBottom);
    float measureTextWithFallback(FontRole role, float pixelSize, const std::string &utf8) const;

    struct Token {
        enum class Type { Word, Space, Newline } type = Type::Word;
        std::string text;
    };

    static std::vector<Token> tokenize(const std::string &utf8);
    static bool decodeNext(const std::string &utf8, std::size_t &offset, uint32_t &codepoint);
    static bool isEmojiCodepoint(uint32_t codepoint);

    bool initFontconfig();
    FontFace *findFontconfigFallback(uint32_t codepoint, bool preferColor);

    void ensureAtlas();
    bool allocateAtlasRegion(int width, int height, int &outX, int &outY);
    bool initialized = false;

    FontFace *regularFace = nullptr;
    FontFace *titleFace = nullptr;
    FontFace *headingFace = nullptr;
    FontFace *emojiFace = nullptr;

    void *ftLibrary = nullptr;
    unsigned int nextInstanceId = 1;

    unsigned int atlasTexture = 0;
    int atlasWidth = 2048;
    int atlasHeight = 2048;
    int atlasCursorX = 1;
    int atlasCursorY = 1;
    int atlasRowHeight = 0;
    std::vector<unsigned char> atlasPixels;

    std::unordered_map<std::string, std::unique_ptr<FontFace>> faces;
    std::unordered_map<uint64_t, Glyph> glyphs;
    std::unordered_map<uint32_t, FontFace *> fallbackCache;
    std::vector<std::unique_ptr<unsigned char[]>> svgBuffers;
    std::unordered_map<uint64_t, std::vector<unsigned char>> svgCache;

    bool fontconfigReady = false;
};

} // namespace gui
