#include "engine/components/gui/rich_text_renderer.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <vector>

#include <spdlog/spdlog.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_SIZES_H
#include FT_ERRORS_H
#include FT_COLOR_H
#include <freetype/ftmodapi.h>
#ifdef FT_CONFIG_OPTION_SVG
#define BZ3_FREETYPE_HAS_SVG 1
#else
#define BZ3_FREETYPE_HAS_SVG 0
#endif
#include <hb.h>
#include <hb-ft.h>

#if defined(__linux__)
#include <fontconfig/fontconfig.h>
#endif

#include <imgui_impl_opengl3_loader.h>
#if defined(__linux__)
#include <GL/gl.h>
#endif

#ifdef BZ3_HAS_LIBRSVG
#include <librsvg/rsvg.h>
#include <cairo.h>
#include <freetype/otsvg.h>
#include <glib.h>
#endif

namespace {
constexpr int kAtlasPadding = 1;
constexpr std::size_t kSvgCacheLimit = 512;

bool isDebugEmoji(uint32_t codepoint) {
    return codepoint == 0x1F680 || codepoint == 0x1F973;
}

const char *emojiLabel(uint32_t codepoint) {
    switch (codepoint) {
        case 0x1F680: return "U+1F680 (🚀)";
        case 0x1F973: return "U+1F973 (🥳)";
        default: return "emoji";
    }
}

ImTextureID toImTextureId(unsigned int texture) {
    return (ImTextureID)(intptr_t)texture;
}

uint64_t hashSvgKey(const unsigned char *data, std::size_t length, int width, int height) {
    const uint64_t fnvOffset = 1469598103934665603ull;
    const uint64_t fnvPrime = 1099511628211ull;
    uint64_t hash = fnvOffset;
    if (data && length > 0) {
        for (std::size_t i = 0; i < length; ++i) {
            hash ^= static_cast<uint64_t>(data[i]);
            hash *= fnvPrime;
        }
    }
    hash ^= static_cast<uint64_t>(width);
    hash *= fnvPrime;
    hash ^= static_cast<uint64_t>(height);
    hash *= fnvPrime;
    return hash;
}

#ifdef BZ3_HAS_LIBRSVG
gui::RichTextRenderer *g_active_renderer = nullptr;

FT_Error svg_init(FT_Pointer *data_pointer) {
    *data_pointer = nullptr;
    return 0;
}

void svg_free(FT_Pointer *data_pointer) {
    *data_pointer = nullptr;
}

struct SvgSize {
    gint width = 0;
    gint height = 0;
};

void svg_size_func(gint *width, gint *height, gpointer user_data) {
    if (!user_data || !width || !height) {
        return;
    }
    const SvgSize *size = static_cast<const SvgSize *>(user_data);
    *width = size->width;
    *height = size->height;
}

FT_Error svg_render_to_slot(FT_GlyphSlot slot) {
    if (!slot || !slot->other || !slot->bitmap.buffer) {
        return FT_Err_Invalid_Argument;
    }
    FT_SVG_Document doc = reinterpret_cast<FT_SVG_Document>(slot->other);
    if (!doc) {
        return FT_Err_Invalid_Argument;
    }
    const int cacheWidth = static_cast<int>(slot->bitmap.width);
    const int cacheHeight = static_cast<int>(slot->bitmap.rows);
    if (g_active_renderer &&
        g_active_renderer->copySvgCache(doc->svg_document,
                                        doc->svg_document_length,
                                        cacheWidth,
                                        cacheHeight,
                                        slot->bitmap.buffer,
                                        slot->bitmap.pitch)) {
        return 0;
    }

    GError *error = nullptr;
    RsvgHandle *handle = rsvg_handle_new_with_flags(RSVG_HANDLE_FLAG_UNLIMITED);
    if (!handle) {
        spdlog::warn("RichTextRenderer: librsvg failed to create handle.");
        return FT_Err_Invalid_Argument;
    }

    cairo_surface_t *surface = cairo_image_surface_create_for_data(
        slot->bitmap.buffer,
        CAIRO_FORMAT_ARGB32,
        static_cast<int>(slot->bitmap.width),
        static_cast<int>(slot->bitmap.rows),
        slot->bitmap.pitch);
    cairo_t *cr = cairo_create(surface);

    rsvg_handle_set_dpi_x_y(handle, 96.0, 96.0);
    SvgSize *size = static_cast<SvgSize *>(g_malloc0(sizeof(SvgSize)));
    size->width = static_cast<gint>(doc->units_per_EM > 0 ? doc->units_per_EM : slot->bitmap.width);
    size->height = static_cast<gint>(doc->units_per_EM > 0 ? doc->units_per_EM : slot->bitmap.rows);
    rsvg_handle_set_size_callback(handle, svg_size_func, size, g_free);

    if (!rsvg_handle_write(handle,
                           reinterpret_cast<const guchar *>(doc->svg_document),
                           doc->svg_document_length,
                           &error)) {
        if (error) {
            spdlog::warn("RichTextRenderer: librsvg write failed: {}", error->message);
            g_error_free(error);
        }
        g_object_unref(handle);
        return FT_Err_Invalid_Argument;
    }
    if (!rsvg_handle_close(handle, &error)) {
        if (error) {
            spdlog::warn("RichTextRenderer: librsvg close failed: {}", error->message);
            g_error_free(error);
        }
        g_object_unref(handle);
        return FT_Err_Invalid_Argument;
    }

    cairo_set_operator(cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(cr);
    cairo_set_operator(cr, CAIRO_OPERATOR_OVER);

    if (slot->bitmap.width == 0 || slot->bitmap.rows == 0) {
        spdlog::warn("RichTextRenderer: svg_render received zero-sized bitmap.");
    }
    spdlog::info("RichTextRenderer: svg_render size {}x{}, units_per_EM {}, x_ppem {}, y_ppem {}.",
                 slot->bitmap.width,
                 slot->bitmap.rows,
                 doc->units_per_EM,
                 doc->metrics.x_ppem,
                 doc->metrics.y_ppem);

    const double units = doc->units_per_EM > 0 ? static_cast<double>(doc->units_per_EM) : 1.0;
    const double emScaleX = units > 0.0 ? static_cast<double>(doc->metrics.x_ppem) / units : 1.0;
    const double emScaleY = units > 0.0 ? static_cast<double>(doc->metrics.y_ppem) / units : 1.0;
    spdlog::info("RichTextRenderer: svg_render emScale {}x{} (units_per_EM {}).",
                 emScaleX,
                 emScaleY,
                 doc->units_per_EM);
    cairo_scale(cr, emScaleX, emScaleY);

    cairo_matrix_t ftMatrix;
    ftMatrix.xx = static_cast<double>(doc->transform.xx) / 65536.0;
    ftMatrix.xy = static_cast<double>(doc->transform.xy) / 65536.0;
    ftMatrix.yx = static_cast<double>(doc->transform.yx) / 65536.0;
    ftMatrix.yy = static_cast<double>(doc->transform.yy) / 65536.0;
    ftMatrix.x0 = 0.0;
    ftMatrix.y0 = 0.0;
    cairo_transform(cr, &ftMatrix);
    cairo_translate(cr,
                    static_cast<double>(doc->delta.x) / 64.0,
                    -static_cast<double>(doc->delta.y) / 64.0);

    if (!rsvg_handle_render_cairo(handle, cr)) {
        spdlog::warn("RichTextRenderer: librsvg render_cairo returned false.");
    }
    cairo_surface_flush(surface);
    cairo_status_t status = cairo_status(cr);
    if (status != CAIRO_STATUS_SUCCESS) {
        spdlog::warn("RichTextRenderer: cairo status {}", cairo_status_to_string(status));
    }

    if (g_active_renderer) {
        g_active_renderer->storeSvgCache(doc->svg_document,
                                         doc->svg_document_length,
                                         cacheWidth,
                                         cacheHeight,
                                         slot->bitmap.buffer,
                                         slot->bitmap.pitch);
    }

    cairo_destroy(cr);
    cairo_surface_destroy(surface);
    g_object_unref(handle);

    return 0;
}

FT_Error svg_preset_slot(FT_GlyphSlot slot, FT_Bool cache, FT_Pointer *) {
    if (!g_active_renderer || !slot || !slot->other) {
        return FT_Err_Invalid_Argument;
    }
    spdlog::info("RichTextRenderer: svg_preset_slot called (cache={}).", cache ? "true" : "false");
    FT_SVG_Document doc = reinterpret_cast<FT_SVG_Document>(slot->other);
    if (!doc) {
        return FT_Err_Invalid_Argument;
    }

    int targetWidth = std::max(1, static_cast<int>(doc->metrics.x_ppem));
    int targetHeight = std::max(1, static_cast<int>(doc->metrics.y_ppem));
    if (targetWidth == 0) {
        targetWidth = targetHeight;
    }

    const int pitch = targetWidth * 4;
    const std::size_t bytes = static_cast<std::size_t>(pitch * targetHeight);
    unsigned char *buffer = g_active_renderer->allocateSvgBuffer(bytes);
    if (buffer) {
        std::memset(buffer, 0, bytes);
    }

    slot->bitmap.buffer = buffer;
    slot->bitmap.width = targetWidth;
    slot->bitmap.rows = targetHeight;
    slot->bitmap.pitch = pitch;
    slot->bitmap.pixel_mode = FT_PIXEL_MODE_BGRA;
    slot->bitmap.num_grays = 256;
    slot->format = FT_GLYPH_FORMAT_BITMAP;
    slot->bitmap_left = 0;
    slot->bitmap_top = targetHeight;
    slot->advance.x = doc->metrics.x_ppem << 6;
    slot->advance.y = 0;

    return svg_render_to_slot(slot);
}

FT_Error svg_render(FT_GlyphSlot slot, FT_Pointer *) {
    spdlog::info("RichTextRenderer: svg_render called.");
    return svg_render_to_slot(slot);
}

const SVG_RendererHooks kSvgHooks = {
    (SVG_Lib_Init_Func)svg_init,
    (SVG_Lib_Free_Func)svg_free,
    (SVG_Lib_Render_Func)svg_render,
    (SVG_Lib_Preset_Slot_Func)svg_preset_slot
};
#endif

uint64_t packGlyphKey(unsigned int instanceId, unsigned int glyphIndex, bool isColor) {
    return (static_cast<uint64_t>(instanceId) << 32) |
           (static_cast<uint64_t>(glyphIndex) << 1) |
           (isColor ? 1u : 0u);
}

std::string makeFontKey(const std::string &path, int faceIndex) {
    return path + "#" + std::to_string(faceIndex);
}

bool hasColorTables(FT_Face face) {
    if (!face) {
        return false;
    }
    return FT_HAS_COLOR(face) != 0;
}
} // namespace

namespace gui {

struct RichTextRenderer::FontFace {
    std::string path;
    int index = 0;
    FT_Face face = nullptr;
    bool supportsColor = false;
};

struct RichTextRenderer::FontInstance {
    FontFace *owner = nullptr;
    int pixelSize = 0;
    unsigned int id = 0;
    FT_Size ftSize = nullptr;
    int fixedSizeIndex = -1;
    bool ownsSize = false;
    hb_font_t *hbFont = nullptr;
    ~FontInstance() {
        if (hbFont) {
            hb_font_destroy(hbFont);
            hbFont = nullptr;
        }
        if (ownsSize && ftSize && owner && owner->face) {
            FT_Done_Size(ftSize);
            ftSize = nullptr;
        }
    }
};

RichTextRenderer::RichTextRenderer() = default;

RichTextRenderer::~RichTextRenderer() {
    shutdown();
}

bool RichTextRenderer::initialize(const FontSpec &regular,
                                  const FontSpec &title,
                                  const FontSpec &heading,
                                  const FontSpec &emoji) {
    shutdown();

    FT_Library lib = nullptr;
    if (FT_Init_FreeType(&lib) != 0) {
        spdlog::warn("RichTextRenderer: Failed to initialize FreeType.");
        return false;
    }
    ftLibrary = lib;
#ifdef BZ3_HAS_LIBRSVG
    g_active_renderer = this;
    FT_Error hookError = FT_Property_Set(lib, "ot-svg", "svg-hooks", const_cast<SVG_RendererHooks *>(&kSvgHooks));
    if (hookError != 0) {
        spdlog::warn("RichTextRenderer: Failed to set SVG hooks (error {}).", hookError);
    } else {
        spdlog::info("RichTextRenderer: SVG hooks installed.");
    }
#endif

#if defined(__linux__)
    fontconfigReady = initFontconfig();
#endif

    regularFace = loadFace(regular.path, 0);
    titleFace = loadFace(title.path, 0);
    headingFace = loadFace(heading.path, 0);
    if (!emoji.path.empty()) {
        emojiFace = loadFace(emoji.path, 0);
    }

    if (!regularFace) {
        spdlog::warn("RichTextRenderer: Regular font unavailable; renderer disabled.");
        shutdown();
        return false;
    }

    ensureAtlas();
    initialized = true;
    return true;
}

void RichTextRenderer::shutdown() {
    if (atlasTexture != 0) {
        glDeleteTextures(1, &atlasTexture);
        atlasTexture = 0;
    }
    atlasPixels.clear();

    for (auto &pair : faces) {
        if (pair.second && pair.second->face) {
            FT_Done_Face(pair.second->face);
        }
    }
    faces.clear();
    fallbackCache.clear();

    for (auto &entry : glyphs) {
        (void)entry;
    }
    glyphs.clear();
    svgBuffers.clear();
    svgCache.clear();

    if (ftLibrary) {
        FT_Done_FreeType(reinterpret_cast<FT_Library>(ftLibrary));
        ftLibrary = nullptr;
    }

#if defined(__linux__)
    if (fontconfigReady) {
        FcFini();
        fontconfigReady = false;
    }
#endif

    regularFace = nullptr;
    titleFace = nullptr;
    headingFace = nullptr;
    emojiFace = nullptr;
    initialized = false;
#ifdef BZ3_HAS_LIBRSVG
    if (g_active_renderer == this) {
        g_active_renderer = nullptr;
    }
#endif
}

bool RichTextRenderer::isInitialized() const {
    return initialized;
}

float RichTextRenderer::getLineHeight(const TextStyle &style) const {
    FontFace *face = regularFace;
    switch (style.role) {
        case FontRole::Title: face = titleFace ? titleFace : regularFace; break;
        case FontRole::Heading: face = headingFace ? headingFace : regularFace; break;
        default: break;
    }
    if (!face || !face->face) {
        return style.size;
    }
    FT_Face ftFace = face->face;
    FT_Set_Pixel_Sizes(ftFace, 0, static_cast<FT_UInt>(std::max(1.0f, style.size)));
    return static_cast<float>(ftFace->size->metrics.height) / 64.0f;
}

void RichTextRenderer::drawInline(ImDrawList *drawList,
                                  InlineLayout &layout,
                                  const std::string &utf8,
                                  const TextStyle &style,
                    std::vector<Rect> *outRects) {
    if (!initialized || !drawList) {
        return;
    }

    const std::vector<Token> tokens = tokenize(utf8);
    const float lineHeight = getLineHeight(style);
    layout.lineHeight = std::max(layout.lineHeight, lineHeight);
    layout.lineSpacing = lineHeight * 0.25f;

    float baseline = layout.cursor.y + lineHeight;
    float cursorX = layout.cursor.x;

    for (const Token &token : tokens) {
        if (token.type == Token::Type::Newline) {
            layout.cursor.x = layout.start.x;
            layout.cursor.y = baseline + layout.lineSpacing;
            baseline = layout.cursor.y + lineHeight;
            cursorX = layout.cursor.x;
            continue;
        }

        const float tokenWidth = measureTextWithFallback(style.role, style.size, token.text);
        if (layout.maxWidth > 0.0f && token.type == Token::Type::Word &&
            cursorX > layout.start.x &&
            cursorX + tokenWidth > layout.start.x + layout.maxWidth) {
            layout.cursor.x = layout.start.x;
            layout.cursor.y = baseline + layout.lineSpacing;
            baseline = layout.cursor.y + lineHeight;
            cursorX = layout.cursor.x;
        }

        float maxTop = 0.0f;
        float maxBottom = 0.0f;
        const float endX = drawTextWithFallback(drawList,
                                                style.role,
                                                style.size,
                                                baseline,
                                                cursorX,
                                                token.text,
                                                style.color,
                                                maxTop,
                                                maxBottom);
        if (outRects && endX > cursorX && token.type == Token::Type::Word) {
            Rect rect{ImVec2(cursorX, baseline - maxTop),
                      ImVec2(endX, baseline + maxBottom)};
            outRects->push_back(rect);
        }

        cursorX = endX;
        layout.cursor.x = cursorX;
        layout.cursor.y = baseline - lineHeight;
    }
}

RichTextRenderer::FontFace *RichTextRenderer::loadFace(const std::string &path, int faceIndex) {
    if (path.empty() || !ftLibrary) {
        return nullptr;
    }
    const std::string key = makeFontKey(path, faceIndex);
    auto it = faces.find(key);
    if (it != faces.end()) {
        return it->second.get();
    }
    FT_Face face = nullptr;
    if (FT_New_Face(reinterpret_cast<FT_Library>(ftLibrary), path.c_str(), faceIndex, &face) != 0) {
        return nullptr;
    }
    auto entry = std::make_unique<FontFace>();
    entry->path = path;
    entry->index = faceIndex;
    entry->face = face;
    entry->supportsColor = hasColorTables(face);
    FontFace *out = entry.get();
    faces[key] = std::move(entry);
    return out;
}

RichTextRenderer::FontInstance *RichTextRenderer::getInstance(FontFace *face, int pixelSize) {
    if (!face || !face->face) {
        return nullptr;
    }
    const int size = std::max(1, pixelSize);
    // Store instances within face->face via user data.
    struct InstanceHolder {
        std::unordered_map<int, std::unique_ptr<FontInstance>> instances;
    };
    auto *holder = reinterpret_cast<InstanceHolder *>(face->face->generic.data);
    if (!holder) {
        auto *newHolder = new InstanceHolder();
        face->face->generic.data = newHolder;
        face->face->generic.finalizer = [](void *ptr) {
            delete reinterpret_cast<InstanceHolder *>(ptr);
        };
        holder = newHolder;
    }
    auto found = holder->instances.find(size);
    if (found != holder->instances.end()) {
        return found->second.get();
    }
    auto instance = std::make_unique<FontInstance>();
    instance->owner = face;
    instance->pixelSize = size;
    instance->id = nextInstanceId++;
    if (face->face->num_fixed_sizes > 0) {
        int bestIndex = 0;
        long bestDelta = std::labs(face->face->available_sizes[0].height - size);
        for (int i = 1; i < face->face->num_fixed_sizes; ++i) {
            long delta = std::labs(face->face->available_sizes[i].height - size);
            if (delta < bestDelta) {
                bestDelta = delta;
                bestIndex = i;
            }
        }
        instance->fixedSizeIndex = bestIndex;
        FT_Error selectError = FT_Select_Size(face->face, bestIndex);
        if (selectError == 0) {
            instance->ftSize = face->face->size;
            instance->ownsSize = false;
        } else {
            spdlog::warn("RichTextRenderer: FT_Select_Size failed (error {}). Falling back to scalable sizing.", selectError);
            instance->fixedSizeIndex = -1;
        }
    } else {
        if (FT_New_Size(face->face, &instance->ftSize) == 0) {
            instance->ownsSize = true;
            FT_Activate_Size(instance->ftSize);
            FT_Set_Pixel_Sizes(face->face, 0, static_cast<FT_UInt>(size));
        } else {
            FT_Set_Pixel_Sizes(face->face, 0, static_cast<FT_UInt>(size));
        }
    }
    instance->hbFont = hb_ft_font_create_referenced(face->face);
    FontInstance *out = instance.get();
    holder->instances[size] = std::move(instance);
    return out;
}

RichTextRenderer::FontFace *RichTextRenderer::pickFace(FontRole role, uint32_t codepoint) {
    FontFace *primary = regularFace;
    if (role == FontRole::Title && titleFace) {
        primary = titleFace;
    } else if (role == FontRole::Heading && headingFace) {
        primary = headingFace;
    }
    if (!primary) {
        primary = regularFace;
    }

    if (primary && primary->face) {
        if (FT_Get_Char_Index(primary->face, codepoint) != 0) {
            return primary;
        }
    }

    if (emojiFace && emojiFace->face) {
        if (FT_Get_Char_Index(emojiFace->face, codepoint) != 0) {
            return emojiFace;
        }
    }

    auto cached = fallbackCache.find(codepoint);
    if (cached != fallbackCache.end()) {
        return cached->second;
    }

    FontFace *fallback = nullptr;
#if defined(__linux__)
    fallback = findFontconfigFallback(codepoint, isEmojiCodepoint(codepoint));
#endif

    fallbackCache[codepoint] = fallback;
    return fallback ? fallback : primary;
}

RichTextRenderer::Glyph *RichTextRenderer::getGlyph(FontInstance *instance, uint32_t glyphIndex, bool &isColor) {
    if (!instance || !instance->owner) {
        return nullptr;
    }
    isColor = instance->owner->supportsColor;
    if (instance->fixedSizeIndex >= 0) {
        FT_Error selectError = FT_Select_Size(instance->owner->face, instance->fixedSizeIndex);
        if (selectError != 0) {
            return nullptr;
        }
    } else if (instance->ftSize) {
        FT_Activate_Size(instance->ftSize);
    }

    FT_Face face = instance->owner->face;
    if (!face) {
        return nullptr;
    }
    if (instance->owner->supportsColor) {
        FT_Palette_Data palette{};
        if (FT_Palette_Data_Get(face, &palette) == 0 && palette.num_palettes > 0) {
            FT_Palette_Select(face, 0, nullptr);
        }
    }
    const uint64_t key = packGlyphKey(instance->id, glyphIndex, isColor);
    auto it = glyphs.find(key);
    if (it != glyphs.end()) {
        return &it->second;
    }

    FT_Int32 loadFlags = FT_LOAD_DEFAULT;
    if (isColor) {
        loadFlags |= FT_LOAD_COLOR;
    }

    FT_Error loadError = FT_Load_Glyph(face, glyphIndex, loadFlags);
    if (loadError != 0) {
        return nullptr;
    }

    FT_GlyphSlot slot = face->glyph;
    bool needsRender = (slot->format != FT_GLYPH_FORMAT_BITMAP);
    FT_Render_Mode renderMode = FT_RENDER_MODE_NORMAL;
#ifdef FT_RENDER_MODE_BGRA
    if (isColor) {
        renderMode = FT_RENDER_MODE_BGRA;
    }
#endif
    if (needsRender) {
        if (FT_Render_Glyph(slot, renderMode) != 0) {
            return nullptr;
        }
    }

    slot = face->glyph;

    const FT_Bitmap &bitmap = slot->bitmap;
    if ((bitmap.width == 0 || bitmap.rows == 0 || !bitmap.buffer) &&
        slot->format == FT_GLYPH_FORMAT_OUTLINE) {
        if (FT_Render_Glyph(slot, FT_RENDER_MODE_NORMAL) == 0) {
            FT_Render_Glyph(slot, renderMode);
        }
    }

    const FT_Bitmap &bitmapFinal = slot->bitmap;
    if (bitmapFinal.width <= 0 || bitmapFinal.rows <= 0 || !bitmapFinal.buffer) {
        return nullptr;
    }

    const int width = static_cast<int>(bitmapFinal.width);
    const int height = static_cast<int>(bitmapFinal.rows);
    int x = 0;
    int y = 0;
    if (!allocateAtlasRegion(width + kAtlasPadding * 2, height + kAtlasPadding * 2, x, y)) {
        return nullptr;
    }

    std::vector<unsigned char> rgba(static_cast<std::size_t>(width * height * 4), 0);
    const int pitch = bitmapFinal.pitch == 0 ? width * 4 : std::abs(bitmapFinal.pitch);
    const bool flip = bitmapFinal.pitch < 0;

    if (bitmapFinal.pixel_mode == FT_PIXEL_MODE_BGRA && bitmapFinal.num_grays == 256) {
        for (int row = 0; row < height; ++row) {
            const int srcRow = flip ? (height - 1 - row) : row;
            const unsigned char *src = bitmapFinal.buffer + srcRow * pitch;
            for (int col = 0; col < width; ++col) {
                const int dstIndex = (row * width + col) * 4;
                rgba[dstIndex + 2] = src[col * 4 + 0];
                rgba[dstIndex + 1] = src[col * 4 + 1];
                rgba[dstIndex + 0] = src[col * 4 + 2];
                rgba[dstIndex + 3] = src[col * 4 + 3];
            }
        }
    } else {
        const int grayPitch = bitmapFinal.pitch == 0 ? width : std::abs(bitmapFinal.pitch);
        for (int row = 0; row < height; ++row) {
            const int srcRow = flip ? (height - 1 - row) : row;
            const unsigned char *src = bitmapFinal.buffer + srcRow * grayPitch;
            for (int col = 0; col < width; ++col) {
                const unsigned char value = src[col];
                const int dstIndex = (row * width + col) * 4;
                rgba[dstIndex + 0] = 255;
                rgba[dstIndex + 1] = 255;
                rgba[dstIndex + 2] = 255;
                rgba[dstIndex + 3] = value;
            }
        }
    }

    if (!atlasPixels.empty()) {
        const int dstStride = atlasWidth * 4;
        for (int row = 0; row < height; ++row) {
            const int dstY = y + kAtlasPadding + row;
            unsigned char *dst = atlasPixels.data() + (dstY * dstStride) + (x + kAtlasPadding) * 4;
            const unsigned char *src = rgba.data() + row * width * 4;
            std::memcpy(dst, src, static_cast<std::size_t>(width * 4));
        }
        glBindTexture(GL_TEXTURE_2D, atlasTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlasWidth, atlasHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlasPixels.data());
        glBindTexture(GL_TEXTURE_2D, 0);
    }

    Glyph stored;
    stored.texture = atlasTexture;
    stored.width = width;
    stored.height = height;
    stored.bearingX = slot->bitmap_left;
    stored.bearingY = slot->bitmap_top;
    stored.advance = static_cast<float>(slot->advance.x) / 64.0f;
    stored.color = (bitmapFinal.pixel_mode == FT_PIXEL_MODE_BGRA);
    stored.uv0 = ImVec2(static_cast<float>(x + kAtlasPadding) / static_cast<float>(atlasWidth),
                        static_cast<float>(y + kAtlasPadding) / static_cast<float>(atlasHeight));
    stored.uv1 = ImVec2(static_cast<float>(x + kAtlasPadding + width) / static_cast<float>(atlasWidth),
                        static_cast<float>(y + kAtlasPadding + height) / static_cast<float>(atlasHeight));

    auto [insertIt, _] = glyphs.emplace(key, stored);
    return &insertIt->second;
}

void RichTextRenderer::debugLogEmojiGlyph(uint32_t codepoint, FontRole role, float pixelSize) {
    static bool loggedRocket = false;
    static bool loggedParty = false;
    if (codepoint == 0x1F680) {
        if (loggedRocket) {
            return;
        }
        loggedRocket = true;
    } else if (codepoint == 0x1F973) {
        if (loggedParty) {
            return;
        }
        loggedParty = true;
    } else {
        return;
    }

    FontFace *face = pickFace(role, codepoint);
    if (!face || !face->face) {
        spdlog::warn("RichTextRenderer: {} no font face selected.", emojiLabel(codepoint));
        return;
    }

    const uint32_t glyphIndex = FT_Get_Char_Index(face->face, codepoint);
    spdlog::info("RichTextRenderer: {} using face '{}' (color={}, glyphIndex={}, fixedSizes={}, faceFlags=0x{:X}, svgSupport={}).",
                 emojiLabel(codepoint),
                 face->path,
                 face->supportsColor ? "yes" : "no",
                 glyphIndex,
                 face->face->num_fixed_sizes,
                 face->face->face_flags,
                 BZ3_FREETYPE_HAS_SVG ? "yes" : "no");

    if (face->face->num_fixed_sizes > 0) {
        for (int i = 0; i < face->face->num_fixed_sizes; ++i) {
            spdlog::info("RichTextRenderer: {} fixed strike {} height {} width {}.",
                         emojiLabel(codepoint),
                         i,
                         face->face->available_sizes[i].height,
                         face->face->available_sizes[i].width);
        }
    } else {
        spdlog::warn("RichTextRenderer: {} reports no fixed strikes.", emojiLabel(codepoint));
    }

    if (glyphIndex == 0) {
        spdlog::warn("RichTextRenderer: {} missing in face '{}'.", emojiLabel(codepoint), face->path);
        return;
    }

    FontInstance *instance = getInstance(face, static_cast<int>(pixelSize));
    if (!instance) {
        spdlog::warn("RichTextRenderer: {} failed to create instance.", emojiLabel(codepoint));
        return;
    }
    spdlog::info("RichTextRenderer: {} instance size {} fixedIndex {}.",
                 emojiLabel(codepoint),
                 instance->pixelSize,
                 instance->fixedSizeIndex);
    if (instance->ftSize) {
        FT_Activate_Size(instance->ftSize);
    }

    FT_Int32 loadFlags = FT_LOAD_DEFAULT;
    if (face->supportsColor) {
        loadFlags |= FT_LOAD_COLOR;
    }
    FT_Error loadError = FT_Load_Glyph(face->face, glyphIndex, loadFlags);
    if (loadError != 0) {
        const char *errStr = FT_Error_String(loadError);
        if (!errStr) {
            errStr = "unknown";
        }
        spdlog::warn("RichTextRenderer: {} FT_Load_Glyph failed (error {}: {}).",
                     emojiLabel(codepoint),
                     loadError,
                     errStr);
        return;
    }

    FT_GlyphSlot slot = face->face->glyph;
    bool needsRender = (slot->format != FT_GLYPH_FORMAT_BITMAP);
    FT_Render_Mode renderMode = FT_RENDER_MODE_NORMAL;
#ifdef FT_RENDER_MODE_BGRA
    if (face->supportsColor) {
        renderMode = FT_RENDER_MODE_BGRA;
    }
#endif
    if (needsRender) {
        FT_Error renderError = FT_Render_Glyph(slot, renderMode);
        if (renderError != 0) {
            const char *renderStr = FT_Error_String(renderError);
            if (!renderStr) {
                renderStr = "unknown";
            }
            spdlog::warn("RichTextRenderer: {} FT_Render_Glyph failed (error {}: {}).",
                         emojiLabel(codepoint),
                         renderError,
                         renderStr);
        } else {
            spdlog::info("RichTextRenderer: {} FT_Render_Glyph succeeded; bitmap mode {}.",
                         emojiLabel(codepoint),
                         static_cast<int>(slot->bitmap.pixel_mode));
        }
    }

    const FT_Bitmap &bitmap = slot->bitmap;
    if (bitmap.width == 0 || bitmap.rows == 0 || !bitmap.buffer) {
        spdlog::warn("RichTextRenderer: {} bitmap empty (format={}, pitch={}).",
                     emojiLabel(codepoint),
                     static_cast<int>(bitmap.pixel_mode),
                     bitmap.pitch);
        return;
    }

    int nonzeroAlpha = 0;
    int maxAlpha = 0;
    int firstX = -1;
    int firstY = -1;
    std::array<unsigned char, 4> firstColor{};
    const int height = static_cast<int>(bitmap.rows);
    const int width = static_cast<int>(bitmap.width);
    const int pitch = bitmap.pitch == 0 ? width * 4 : std::abs(bitmap.pitch);
    const bool flip = bitmap.pitch < 0;
    int sampleCount = 0;
    std::array<unsigned char, 16> sampleBytes{};

    if (bitmap.pixel_mode == FT_PIXEL_MODE_BGRA) {
        for (int row = 0; row < height; ++row) {
            const int srcRow = flip ? (height - 1 - row) : row;
            const unsigned char *src = bitmap.buffer + srcRow * pitch;
            for (int col = 0; col < width; ++col) {
                if (src[col * 4 + 3] != 0) {
                    ++nonzeroAlpha;
                }
                if (src[col * 4 + 3] > maxAlpha) {
                    maxAlpha = src[col * 4 + 3];
                }
                if (firstX < 0 && src[col * 4 + 3] != 0) {
                    firstX = col;
                    firstY = row;
                    firstColor = {src[col * 4 + 2], src[col * 4 + 1], src[col * 4 + 0], src[col * 4 + 3]};
                }
                if (sampleCount < static_cast<int>(sampleBytes.size())) {
                    sampleBytes[sampleCount++] = src[col * 4 + 3];
                }
            }
        }
    } else if (bitmap.pixel_mode == FT_PIXEL_MODE_GRAY) {
        const int grayPitch = bitmap.pitch == 0 ? width : std::abs(bitmap.pitch);
        for (int row = 0; row < height; ++row) {
            const int srcRow = flip ? (height - 1 - row) : row;
            const unsigned char *src = bitmap.buffer + srcRow * grayPitch;
            for (int col = 0; col < width; ++col) {
                if (src[col] != 0) {
                    ++nonzeroAlpha;
                }
                if (src[col] > maxAlpha) {
                    maxAlpha = src[col];
                }
                if (firstX < 0 && src[col] != 0) {
                    firstX = col;
                    firstY = row;
                    firstColor = {255, 255, 255, src[col]};
                }
                if (sampleCount < static_cast<int>(sampleBytes.size())) {
                    sampleBytes[sampleCount++] = src[col];
                }
            }
        }
    }

    spdlog::info("RichTextRenderer: {} bitmap {}x{} mode={} pitch={} nonzeroAlpha={}.",
                 emojiLabel(codepoint),
                 width,
                 height,
                 static_cast<int>(bitmap.pixel_mode),
                 bitmap.pitch,
                 nonzeroAlpha);
    if (firstX >= 0) {
        spdlog::info("RichTextRenderer: {} first alpha at ({}, {}), color RGBA({}, {}, {}, {}), maxAlpha={}.",
                     emojiLabel(codepoint),
                     firstX,
                     firstY,
                     firstColor[0],
                     firstColor[1],
                     firstColor[2],
                     firstColor[3],
                     maxAlpha);
    }
    if (sampleCount > 0) {
        std::string sample;
        sample.reserve(sampleBytes.size() * 3);
        for (int i = 0; i < sampleCount; ++i) {
            char buf[4];
            std::snprintf(buf, sizeof(buf), "%02X", sampleBytes[i]);
            sample.append(buf);
            if (i + 1 < sampleCount) {
                sample.push_back(' ');
            }
        }
        spdlog::info("RichTextRenderer: {} bitmap sample alpha bytes: {}",
                     emojiLabel(codepoint),
                     sample);
    }
}

float RichTextRenderer::measureRun(FontInstance *instance, const std::string &utf8) const {
    if (!instance || !instance->hbFont) {
        return 0.0f;
    }
    if (instance->fixedSizeIndex >= 0) {
        FT_Select_Size(instance->owner->face, instance->fixedSizeIndex);
    } else if (instance->ftSize) {
        FT_Activate_Size(instance->ftSize);
    }
    hb_buffer_t *buffer = hb_buffer_create();
    hb_buffer_add_utf8(buffer, utf8.c_str(), static_cast<int>(utf8.size()), 0, static_cast<int>(utf8.size()));
    hb_buffer_guess_segment_properties(buffer);
    hb_shape(instance->hbFont, buffer, nullptr, 0);
    unsigned int glyphCount = 0;
    hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(buffer, &glyphCount);
    float width = 0.0f;
    for (unsigned int i = 0; i < glyphCount; ++i) {
        width += static_cast<float>(positions[i].x_advance) / 64.0f;
    }
    hb_buffer_destroy(buffer);
    return width;
}

float RichTextRenderer::drawRun(ImDrawList *drawList,
                                FontInstance *instance,
                                float baselineY,
                                float startX,
                                const std::string &utf8,
                                const ImVec4 &color,
                                float &outMaxTop,
                                float &outMaxBottom) {
    if (!instance || !instance->hbFont) {
        return startX;
    }
    if (instance->fixedSizeIndex >= 0) {
        FT_Select_Size(instance->owner->face, instance->fixedSizeIndex);
    } else if (instance->ftSize) {
        FT_Activate_Size(instance->ftSize);
    }
    hb_buffer_t *buffer = hb_buffer_create();
    hb_buffer_add_utf8(buffer, utf8.c_str(), static_cast<int>(utf8.size()), 0, static_cast<int>(utf8.size()));
    hb_buffer_guess_segment_properties(buffer);
    hb_shape(instance->hbFont, buffer, nullptr, 0);
    unsigned int glyphCount = 0;
    hb_glyph_info_t *infos = hb_buffer_get_glyph_infos(buffer, &glyphCount);
    hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(buffer, &glyphCount);

    float penX = startX;
    float maxTop = 0.0f;
    float maxBottom = 0.0f;

    for (unsigned int i = 0; i < glyphCount; ++i) {
        const uint32_t glyphIndex = infos[i].codepoint;
        bool isColor = false;
        Glyph *glyph = getGlyph(instance, glyphIndex, isColor);
        if (glyph) {
            const float x = penX + static_cast<float>(positions[i].x_offset) / 64.0f + static_cast<float>(glyph->bearingX);
            const float y = baselineY - static_cast<float>(glyph->bearingY) - static_cast<float>(positions[i].y_offset) / 64.0f;
            ImVec2 p0(x, y);
            ImVec2 p1(x + static_cast<float>(glyph->width), y + static_cast<float>(glyph->height));
            ImU32 tint = ImGui::ColorConvertFloat4ToU32(color);
            if (isColor) {
                tint = IM_COL32_WHITE;
            }
            drawList->AddImage(toImTextureId(glyph->texture),
                               p0,
                               p1,
                               glyph->uv0,
                               glyph->uv1,
                               tint);
            maxTop = std::max(maxTop, baselineY - y);
            maxBottom = std::max(maxBottom, p1.y - baselineY);
        }

        penX += static_cast<float>(positions[i].x_advance) / 64.0f;
    }
    hb_buffer_destroy(buffer);
    outMaxTop = std::max(outMaxTop, maxTop);
    outMaxBottom = std::max(outMaxBottom, maxBottom);
    return penX;
}

float RichTextRenderer::drawTextWithFallback(ImDrawList *drawList,
                                             FontRole role,
                                             float pixelSize,
                                             float baselineY,
                                             float startX,
                                             const std::string &utf8,
                                             const ImVec4 &color,
                                             float &outMaxTop,
                                             float &outMaxBottom) {
    std::size_t offset = 0;
    float cursor = startX;

    auto emitRun = [&](FontFace *face, const std::string &segment) {
        if (segment.empty()) {
            return;
        }
        FontInstance *instance = getInstance(face, static_cast<int>(pixelSize));
        if (!instance) {
            return;
        }
        cursor = drawRun(drawList,
                         instance,
                         baselineY,
                         cursor,
                         segment,
                         color,
                         outMaxTop,
                         outMaxBottom);
    };

    std::string segment;
    FontFace *currentFace = nullptr;
    while (offset < utf8.size()) {
        std::size_t start = offset;
        uint32_t codepoint = 0;
        if (!decodeNext(utf8, offset, codepoint)) {
            break;
        }
        if (isDebugEmoji(codepoint)) {
            debugLogEmojiGlyph(codepoint, role, pixelSize);
        }
        FontFace *face = pickFace(role, codepoint);
        if (!currentFace) {
            currentFace = face;
        }
        if (face != currentFace) {
            emitRun(currentFace, segment);
            segment.clear();
            currentFace = face;
        }
        segment.append(utf8.substr(start, offset - start));
    }
    emitRun(currentFace ? currentFace : regularFace, segment);
    return cursor;
}

float RichTextRenderer::measureTextWithFallback(FontRole role, float pixelSize, const std::string &utf8) const {
    std::size_t offset = 0;
    float width = 0.0f;

    auto measureSegment = [&](FontFace *face, const std::string &segment) {
        if (!face || segment.empty()) {
            return;
        }
        FT_Set_Pixel_Sizes(face->face, 0, static_cast<FT_UInt>(std::max(1.0f, pixelSize)));
        hb_font_t *hbFont = hb_ft_font_create_referenced(face->face);
        hb_buffer_t *buffer = hb_buffer_create();
        hb_buffer_add_utf8(buffer, segment.c_str(), static_cast<int>(segment.size()), 0, static_cast<int>(segment.size()));
        hb_buffer_guess_segment_properties(buffer);
        hb_shape(hbFont, buffer, nullptr, 0);
        unsigned int glyphCount = 0;
        hb_glyph_position_t *positions = hb_buffer_get_glyph_positions(buffer, &glyphCount);
        for (unsigned int i = 0; i < glyphCount; ++i) {
            width += static_cast<float>(positions[i].x_advance) / 64.0f;
        }
        hb_buffer_destroy(buffer);
        hb_font_destroy(hbFont);
    };

    std::string segment;
    FontFace *currentFace = nullptr;
    while (offset < utf8.size()) {
        std::size_t start = offset;
        uint32_t codepoint = 0;
        if (!decodeNext(utf8, offset, codepoint)) {
            break;
        }
        FontFace *face = const_cast<RichTextRenderer *>(this)->pickFace(role, codepoint);
        if (!currentFace) {
            currentFace = face;
        }
        if (face != currentFace) {
            measureSegment(currentFace, segment);
            segment.clear();
            currentFace = face;
        }
        segment.append(utf8.substr(start, offset - start));
    }
    measureSegment(currentFace ? currentFace : regularFace, segment);
    return width;
}

std::vector<RichTextRenderer::Token> RichTextRenderer::tokenize(const std::string &utf8) {
    std::vector<Token> out;
    std::string current;
    auto flushWord = [&]() {
        if (!current.empty()) {
            out.push_back(Token{Token::Type::Word, current});
            current.clear();
        }
    };

    for (char ch : utf8) {
        if (ch == '\n') {
            flushWord();
            out.push_back(Token{Token::Type::Newline, "\n"});
        } else if (ch == ' ' || ch == '\t') {
            flushWord();
            out.push_back(Token{Token::Type::Space, std::string(1, ch)});
        } else {
            current.push_back(ch);
        }
    }
    flushWord();

    // Merge consecutive spaces into one token to preserve spacing.
    std::vector<Token> merged;
    for (const auto &token : out) {
        if (merged.empty()) {
            merged.push_back(token);
            continue;
        }
        if (token.type == Token::Type::Space && merged.back().type == Token::Type::Space) {
            merged.back().text.append(token.text);
        } else {
            merged.push_back(token);
        }
    }
    return merged;
}

bool RichTextRenderer::decodeNext(const std::string &utf8, std::size_t &offset, uint32_t &codepoint) {
    if (offset >= utf8.size()) {
        return false;
    }
    unsigned char c = static_cast<unsigned char>(utf8[offset]);
    if (c < 0x80) {
        codepoint = c;
        offset += 1;
        return true;
    }
    int length = 0;
    if ((c & 0xE0) == 0xC0) {
        length = 2;
        codepoint = c & 0x1F;
    } else if ((c & 0xF0) == 0xE0) {
        length = 3;
        codepoint = c & 0x0F;
    } else if ((c & 0xF8) == 0xF0) {
        length = 4;
        codepoint = c & 0x07;
    } else {
        offset += 1;
        return false;
    }
    if (offset + length > utf8.size()) {
        offset += 1;
        return false;
    }
    for (int i = 1; i < length; ++i) {
        unsigned char cc = static_cast<unsigned char>(utf8[offset + i]);
        if ((cc & 0xC0) != 0x80) {
            offset += 1;
            return false;
        }
        codepoint = (codepoint << 6) | (cc & 0x3F);
    }
    offset += length;
    return true;
}

bool RichTextRenderer::isEmojiCodepoint(uint32_t codepoint) {
    return (codepoint >= 0x1F300 && codepoint <= 0x1FAFF);
}

bool RichTextRenderer::initFontconfig() {
#if defined(__linux__)
    return FcInit() != FcFalse;
#else
    return false;
#endif
}

RichTextRenderer::FontFace *RichTextRenderer::findFontconfigFallback(uint32_t codepoint, bool preferColor) {
#if defined(__linux__)
    if (!fontconfigReady) {
        return nullptr;
    }
    FcCharSet *charset = FcCharSetCreate();
    if (!charset) {
        return nullptr;
    }
    FcCharSetAddChar(charset, codepoint);
    FcPattern *pattern = FcPatternCreate();
    if (!pattern) {
        FcCharSetDestroy(charset);
        return nullptr;
    }
    FcPatternAddCharSet(pattern, FC_CHARSET, charset);
    FcPatternAddBool(pattern, FC_SCALABLE, FcTrue);
    FcPatternAddBool(pattern, FC_OUTLINE, FcTrue);
    if (preferColor) {
        FcPatternAddBool(pattern, FC_COLOR, FcTrue);
    }
    FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);

    FcResult result = FcResultNoMatch;
    FcPattern *match = FcFontMatch(nullptr, pattern, &result);
    FontFace *face = nullptr;
    if (match) {
        FcChar8 *file = nullptr;
        int index = 0;
        if (FcPatternGetString(match, FC_FILE, 0, &file) == FcResultMatch &&
            FcPatternGetInteger(match, FC_INDEX, 0, &index) == FcResultMatch) {
            face = loadFace(reinterpret_cast<const char *>(file), index);
        }
        FcPatternDestroy(match);
    }

    FcPatternDestroy(pattern);
    FcCharSetDestroy(charset);
    return face;
#else
    (void)codepoint;
    (void)preferColor;
    return nullptr;
#endif
}

void RichTextRenderer::ensureAtlas() {
    if (atlasTexture != 0) {
        return;
    }
    atlasPixels.assign(static_cast<std::size_t>(atlasWidth * atlasHeight * 4), 0);
    glGenTextures(1, &atlasTexture);
    glBindTexture(GL_TEXTURE_2D, atlasTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, atlasWidth, atlasHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, atlasPixels.data());
    glBindTexture(GL_TEXTURE_2D, 0);
}

bool RichTextRenderer::allocateAtlasRegion(int width, int height, int &outX, int &outY) {
    if (atlasTexture == 0) {
        ensureAtlas();
    }
    if (width > atlasWidth || height > atlasHeight) {
        return false;
    }
    if (atlasCursorX + width > atlasWidth) {
        atlasCursorX = 1;
        atlasCursorY += atlasRowHeight + kAtlasPadding;
        atlasRowHeight = 0;
    }
    if (atlasCursorY + height > atlasHeight) {
        return false;
    }
    outX = atlasCursorX;
    outY = atlasCursorY;
    atlasCursorX += width + kAtlasPadding;
    atlasRowHeight = std::max(atlasRowHeight, height);
    return true;
}

unsigned char *RichTextRenderer::allocateSvgBuffer(std::size_t bytes) {
    if (bytes == 0) {
        return nullptr;
    }
    auto buffer = std::make_unique<unsigned char[]>(bytes);
    unsigned char *ptr = buffer.get();
    svgBuffers.push_back(std::move(buffer));
    return ptr;
}

bool RichTextRenderer::copySvgCache(const unsigned char *svgData,
                                    std::size_t length,
                                    int width,
                                    int height,
                                    unsigned char *dest,
                                    int destPitch) {
    if (!svgData || length == 0 || width <= 0 || height <= 0 || !dest) {
        return false;
    }
    const uint64_t key = hashSvgKey(svgData, length, width, height);
    auto it = svgCache.find(key);
    if (it == svgCache.end()) {
        return false;
    }
    const std::vector<unsigned char> &pixels = it->second;
    const int rowBytes = width * 4;
    if (static_cast<int>(pixels.size()) < rowBytes * height) {
        return false;
    }
    const int pitch = destPitch == 0 ? rowBytes : std::abs(destPitch);
    const bool flip = destPitch < 0;
    for (int row = 0; row < height; ++row) {
        const int dstRow = flip ? (height - 1 - row) : row;
        unsigned char *dst = dest + dstRow * pitch;
        const unsigned char *src = pixels.data() + row * rowBytes;
        std::memcpy(dst, src, static_cast<std::size_t>(rowBytes));
    }
    return true;
}

void RichTextRenderer::storeSvgCache(const unsigned char *svgData,
                                     std::size_t length,
                                     int width,
                                     int height,
                                     const unsigned char *src,
                                     int srcPitch) {
    if (!svgData || length == 0 || width <= 0 || height <= 0 || !src) {
        return;
    }
    if (svgCache.size() >= kSvgCacheLimit) {
        svgCache.clear();
    }
    const uint64_t key = hashSvgKey(svgData, length, width, height);
    const int rowBytes = width * 4;
    const int pitch = srcPitch == 0 ? rowBytes : std::abs(srcPitch);
    const bool flip = srcPitch < 0;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(rowBytes * height));
    for (int row = 0; row < height; ++row) {
        const int srcRow = flip ? (height - 1 - row) : row;
        const unsigned char *srcRowPtr = src + srcRow * pitch;
        unsigned char *dst = pixels.data() + row * rowBytes;
        std::memcpy(dst, srcRowPtr, static_cast<std::size_t>(rowBytes));
    }
    svgCache[key] = std::move(pixels);
}

} // namespace gui
