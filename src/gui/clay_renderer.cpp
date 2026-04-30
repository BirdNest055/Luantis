/*
 * Clay Rendering Backend for Irrlicht
 *
 * Translates Clay render commands into Irrlicht 2D draw calls.
 * Clay outputs a flat array of render commands (rectangles, text, images,
 * custom elements, etc.) and this renderer composites them onto the screen
 * using Irrlicht's 2D drawing primitives.
 *
 * Part of the Luantis Clay GUI integration (v9.46).
 */

// CLAY_IMPLEMENTATION must be defined in exactly one translation unit
// before including clay.h
#define CLAY_IMPLEMENTATION
#include "clay_renderer.h"

#include "clay_theme.h"

#include <irrlicht.h>
#include "IGUIFont.h"
#include "client/fontengine.h"
#include "client/renderingengine.h"
#include "client/guiscalingfilter.h"
#include "log.h"
#include "porting.h"

#include <cstring>
#include <string>
#include <codecvt>
#include <locale>
#include <fstream>
#include <sstream>

// Render command logging for visual verification.
// When CLAY_RENDER_LOG is defined, each frame's render commands are
// written to /tmp/clay_render_log.jsonl for offline rendering to PNG.
// This is controlled by the environment variable CLAY_RENDER_LOG=1.
static bool g_clay_render_log_enabled = []() {
        const char *env = std::getenv("CLAY_RENDER_LOG");
        return env && std::string(env) == "1";
}();
static int g_clay_render_log_frame = 0;

// Helper: pack Theme constexpr uint32_t (ARGB) into Irrlicht SColor
static video::SColor packedToSColor(uint32_t packed)
{
        return video::SColor(
                (packed >> 24) & 0xFF,
                (packed >> 16) & 0xFF,
                (packed >>  8) & 0xFF,
                (packed      ) & 0xFF);
}

// Irrlicht types in this fork are in global sub-namespaces:
//   core::  video::  gui::  io::  scene::
// NOT wrapped in an outer irr:: namespace.

// ---------------------------------------------------------------------------
// UTF-8 → wstring helper (correctly handles multi-byte sequences)
// ---------------------------------------------------------------------------

static std::wstring utf8ToWstring(const std::string &utf8)
{
        if (utf8.empty())
                return std::wstring();

        // Fast path: if all characters are ASCII, no conversion needed
        bool all_ascii = true;
        for (unsigned char c : utf8) {
                if (c > 127) {
                        all_ascii = false;
                        break;
                }
        }
        if (all_ascii)
                return std::wstring(utf8.begin(), utf8.end());

        // Multi-byte UTF-8: use codecvt for proper conversion
        // Note: codecvt is deprecated in C++17 but still the standard way
        // until C++26 provides a replacement.
        try {
                std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
                return conv.from_bytes(utf8);
        } catch (const std::exception &) {
                // Fallback: byte-by-byte widening (will mangle non-ASCII but won't crash)
                return std::wstring(utf8.begin(), utf8.end());
        }
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void ClayIrrlichtRenderer::init(IrrlichtDevice *device)
{
        m_driver = device->getVideoDriver();
}

void ClayIrrlichtRenderer::setFontEngine(FontEngine *engine)
{
        m_font_engine = engine;
}

// ---------------------------------------------------------------------------
// Text measurement — hot path, must be fast
// ---------------------------------------------------------------------------

Clay_Dimensions ClayIrrlichtRenderer::measureText(Clay_StringSlice text,
        Clay_TextElementConfig *config, void *userData)
{
        auto *self = static_cast<ClayIrrlichtRenderer *>(userData);
        if (!self || !self->m_font_engine) {
                // Fallback: rough monospace estimate
                return Clay_Dimensions{
                        .width = static_cast<float>(text.length) * config->fontSize * Theme::Renderer::fallbackTextWidthRatio,
                        .height = static_cast<float>(config->fontSize)
                };
        }

        gui::IGUIFont *font = self->getFont(config->fontId, config->fontSize);
        if (!font) {
                return Clay_Dimensions{
                        .width = static_cast<float>(text.length) * config->fontSize * Theme::Renderer::fallbackTextWidthRatio,
                        .height = static_cast<float>(config->fontSize)
                };
        }

        // Clay does NOT guarantee null-terminated strings
        std::string str(text.chars, text.length);
        // Irrlicht's IGUIFont::getDimension() expects wchar_t
        std::wstring wstr = utf8ToWstring(str);
        auto dim = font->getDimension(wstr.c_str());

        return Clay_Dimensions{
                .width = static_cast<float>(dim.Width),
                .height = static_cast<float>(dim.Height)
        };
}

// ---------------------------------------------------------------------------
// Font lookup
// ---------------------------------------------------------------------------

gui::IGUIFont *ClayIrrlichtRenderer::getFont(uint16_t fontId, uint16_t fontSize)
{
        if (!m_font_engine)
                return nullptr;

        // Map Clay fontId to FontEngine font mode.
        // Theme::Fonts::standardFontId = FM_Standard, Theme::Fonts::monoFontId = FM_Mono.
        // Note: Luantis's FontEngine only has FM_Standard and FM_Mono —
        // no bold/italic variants are available.
        auto mode = FontMode::FM_Standard;
        if (fontId == Theme::Fonts::monoFontId)
                mode = FontMode::FM_Mono;

        // FontEngine uses a size parameter; fontSize of 0 means default
        unsigned int size = fontSize > 0 ? fontSize : 0;
        return m_font_engine->getFont(size, mode);
}

// ---------------------------------------------------------------------------
// Color conversion (needed before render loop)
// ---------------------------------------------------------------------------

static video::SColor toSColor(Clay_Color c)
{
        // Clay_Color uses float fields conventionally in 0-255 range
        // Clamp to prevent garbage colors from out-of-range values
        auto clamp = [](float v) -> u32 {
                int i = static_cast<int>(v);
                return static_cast<u32>(i < 0 ? 0 : (i > 255 ? 255 : i));
        };
        return video::SColor(clamp(c.a), clamp(c.r), clamp(c.g), clamp(c.b));
}

// ---------------------------------------------------------------------------
// Main render loop
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Render command logging (JSON lines format for Python renderer)
// ---------------------------------------------------------------------------

static std::string escapeJson(const std::string &s)
{
        std::string out;
        out.reserve(s.size() + 8);
        for (char c : s) {
                switch (c) {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:
                        if (static_cast<unsigned char>(c) < 0x20) {
                                char buf[8];
                                snprintf(buf, sizeof(buf), "\\u%04x", c);
                                out += buf;
                        } else {
                                out += c;
                        }
                }
        }
        return out;
}

static void logRenderCommands(const Clay_RenderCommandArray &commands, int frameNum)
{
        // Only log a few frames to avoid huge files
        if (frameNum > Theme::Renderer::renderLogMaxFrames)
                return;

        std::ofstream log(Theme::Renderer::renderLogPath, std::ios::app);
        if (!log.is_open())
                return;

        log << "{\"frame\":" << frameNum << ",\"commands\":[";

        for (int32_t i = 0; i < commands.length; i++) {
                const Clay_RenderCommand &cmd = commands.internalArray[i];
                auto &bb = cmd.boundingBox;

                if (i > 0) log << ",";

                log << "{\"type\":" << static_cast<int>(cmd.commandType);
                log << ",\"x\":" << static_cast<int>(bb.x);
                log << ",\"y\":" << static_cast<int>(bb.y);
                log << ",\"w\":" << static_cast<int>(bb.width);
                log << ",\"h\":" << static_cast<int>(bb.height);

                switch (cmd.commandType) {
                case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
                        log << ",\"bg\":["
                            << static_cast<int>(cmd.renderData.rectangle.backgroundColor.r) << ","
                            << static_cast<int>(cmd.renderData.rectangle.backgroundColor.g) << ","
                            << static_cast<int>(cmd.renderData.rectangle.backgroundColor.b) << ","
                            << static_cast<int>(cmd.renderData.rectangle.backgroundColor.a) << "]";
                        break;
                case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                        auto &td = cmd.renderData.text;
                        std::string text(td.stringContents.chars, td.stringContents.length);
                        log << ",\"text\":\"" << escapeJson(text) << "\"";
                        log << ",\"textColor\":["
                            << static_cast<int>(td.textColor.r) << ","
                            << static_cast<int>(td.textColor.g) << ","
                            << static_cast<int>(td.textColor.b) << ","
                            << static_cast<int>(td.textColor.a) << "]";
                        log << ",\"fontSize\":" << static_cast<int>(td.fontSize);
                        log << ",\"fontId\":" << static_cast<int>(td.fontId);
                        break;
                }
                case CLAY_RENDER_COMMAND_TYPE_BORDER: {
                        auto &bd = cmd.renderData.border;
                        log << ",\"borderColor\":["
                            << static_cast<int>(bd.color.r) << ","
                            << static_cast<int>(bd.color.g) << ","
                            << static_cast<int>(bd.color.b) << ","
                            << static_cast<int>(bd.color.a) << "]";
                        log << ",\"borderWidth\":{"
                            << "\"l\":" << bd.width.left
                            << ",\"r\":" << bd.width.right
                            << ",\"t\":" << bd.width.top
                            << ",\"b\":" << bd.width.bottom << "}";
                        break;
                }
                case CLAY_RENDER_COMMAND_TYPE_IMAGE:
                        log << ",\"hasImageData\":" << (cmd.renderData.image.imageData ? "true" : "false");
                        break;
                case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
                        log << ",\"customBg\":["
                            << static_cast<int>(cmd.renderData.custom.backgroundColor.r) << ","
                            << static_cast<int>(cmd.renderData.custom.backgroundColor.g) << ","
                            << static_cast<int>(cmd.renderData.custom.backgroundColor.b) << ","
                            << static_cast<int>(cmd.renderData.custom.backgroundColor.a) << "]";
                        break;
                case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START:
                        log << ",\"overlayColor\":["
                            << static_cast<int>(cmd.renderData.overlayColor.color.r) << ","
                            << static_cast<int>(cmd.renderData.overlayColor.color.g) << ","
                            << static_cast<int>(cmd.renderData.overlayColor.color.b) << ","
                            << static_cast<int>(cmd.renderData.overlayColor.color.a) << "]";
                        break;
                default:
                        break;
                }

                log << "}";
        }

        log << "]}" << std::endl;
}

void ClayIrrlichtRenderer::render(const Clay_RenderCommandArray &commands)
{
        if (!m_driver)
                return;

        // Log render commands if enabled (for visual verification)
        if (g_clay_render_log_enabled) {
                logRenderCommands(commands, g_clay_render_log_frame++);
        }

        // Track clip rect state across render commands
        core::rect<s32> currentClip;
        bool hasClip = false;

        // Track overlay color state (stack for nested overlays)
        // When active, we draw a semi-transparent rectangle over the
        // bounding box of each subsequent element to simulate tinting.
        struct OverlayState {
                Clay_Color color;
        };
        std::vector<OverlayState> overlayStack;
        bool hasOverlay = false;
        Clay_Color activeOverlayColor = {0, 0, 0, 0};

        for (int32_t i = 0; i < commands.length; i++) {
                const Clay_RenderCommand &cmd = commands.internalArray[i];

                switch (cmd.commandType) {
                case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
                        drawRectangle(cmd, hasClip ? &currentClip : nullptr);
                        break;
                case CLAY_RENDER_COMMAND_TYPE_TEXT:
                        drawText(cmd, hasClip ? &currentClip : nullptr);
                        break;
                case CLAY_RENDER_COMMAND_TYPE_IMAGE:
                        drawImage(cmd, hasClip ? &currentClip : nullptr);
                        break;
                case CLAY_RENDER_COMMAND_TYPE_BORDER:
                        drawBorder(cmd, hasClip ? &currentClip : nullptr);
                        break;
                case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
                        drawCustom(cmd, hasClip ? &currentClip : nullptr);
                        break;
                case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
                        {
                                auto &bb = cmd.boundingBox;
                                currentClip = core::rect<s32>(
                                        static_cast<s32>(bb.x),
                                        static_cast<s32>(bb.y),
                                        static_cast<s32>(bb.x + bb.width),
                                        static_cast<s32>(bb.y + bb.height));
                                hasClip = true;
                        }
                        break;
                case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
                        hasClip = false;
                        break;
                case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START:
                        {
                                auto &oc = cmd.renderData.overlayColor;
                                overlayStack.push_back({oc.color});
                                activeOverlayColor = oc.color;
                                hasOverlay = true;
                        }
                        break;
                case CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_END:
                        if (!overlayStack.empty()) {
                                overlayStack.pop_back();
                        }
                        if (overlayStack.empty()) {
                                hasOverlay = false;
                                activeOverlayColor = {0, 0, 0, 0};
                        } else {
                                activeOverlayColor = overlayStack.back().color;
                        }
                        break;
                default:
                        break;
                }

                // Apply overlay color tinting: draw a semi-transparent rect
                // over the element's bounding box. This is a simple but
                // effective approach that works with Irrlicht's 2D API
                // without requiring shader-based color multiplication.
                if (hasOverlay && cmd.commandType != CLAY_RENDER_COMMAND_TYPE_SCISSOR_START
                        && cmd.commandType != CLAY_RENDER_COMMAND_TYPE_SCISSOR_END
                        && cmd.commandType != CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_START
                        && cmd.commandType != CLAY_RENDER_COMMAND_TYPE_OVERLAY_COLOR_END
                        && activeOverlayColor.a > 0) {
                        auto &bb = cmd.boundingBox;
                        core::rect<s32> overlayRect(
                                static_cast<s32>(bb.x),
                                static_cast<s32>(bb.y),
                                static_cast<s32>(bb.x + bb.width),
                                static_cast<s32>(bb.y + bb.height));
                        m_driver->draw2DRectangle(
                                toSColor(activeOverlayColor),
                                overlayRect,
                                hasClip ? &currentClip : nullptr);
                }
        }
}

// ---------------------------------------------------------------------------
// Individual command renderers
// ---------------------------------------------------------------------------

void ClayIrrlichtRenderer::drawRectangle(const Clay_RenderCommand &cmd,
        void *clipRectPtr)
{
        auto *clipRect = static_cast<core::rect<s32> *>(clipRectPtr);
        auto &bb = cmd.boundingBox;
        auto &rect = cmd.renderData.rectangle;
        auto color = toSColor(rect.backgroundColor);

        core::rect<s32> dest(
                static_cast<s32>(bb.x),
                static_cast<s32>(bb.y),
                static_cast<s32>(bb.x + bb.width),
                static_cast<s32>(bb.y + bb.height));

        m_driver->draw2DRectangle(color, dest, clipRect);
}

void ClayIrrlichtRenderer::drawText(const Clay_RenderCommand &cmd,
        void *clipRectPtr)
{
        auto *clipRect = static_cast<core::rect<s32> *>(clipRectPtr);
        auto &bb = cmd.boundingBox;
        auto &td = cmd.renderData.text;

        gui::IGUIFont *font = getFont(td.fontId, td.fontSize);
        if (!font)
                return;

        // Clay does NOT guarantee null termination
        std::string str(td.stringContents.chars, td.stringContents.length);

        // Irrlicht's IGUIFont::draw() expects wchar_t, so convert
        std::wstring wstr = utf8ToWstring(str);

        auto color = toSColor(td.textColor);

        core::position2di pos(
                static_cast<s32>(bb.x),
                static_cast<s32>(bb.y));

        font->draw(wstr.c_str(),
                core::rect<s32>(pos,
                        core::dimension2di(
                                static_cast<s32>(bb.width),
                                static_cast<s32>(bb.height))),
                color, false, false, clipRect);
}

void ClayIrrlichtRenderer::drawImage(const Clay_RenderCommand &cmd,
        void *clipRectPtr)
{
        auto *clipRect = static_cast<core::rect<s32> *>(clipRectPtr);
        auto &bb = cmd.boundingBox;
        auto &img = cmd.renderData.image;
        core::rect<s32> dest(
                static_cast<s32>(bb.x),
                static_cast<s32>(bb.y),
                static_cast<s32>(bb.x + bb.width),
                static_cast<s32>(bb.y + bb.height));

        if (!img.imageData)
                goto draw_placeholder;

        {
                // Check if this is a ClayImageInfo (9-slice) or raw ITexture*
                auto *info = static_cast<uint8_t *>(img.imageData);
                bool is9Slice = (info[0] == CLAY_IMAGE_TAG_9SLICE);

                video::ITexture *texture = nullptr;
                int middleInset = Theme::Renderer::nineSliceInset;

                if (is9Slice) {
                        auto *imgInfo = static_cast<ClayImageInfo *>(img.imageData);
                        texture = imgInfo->texture;
                        middleInset = imgInfo->middleInset;
                } else {
                        texture = static_cast<video::ITexture *>(img.imageData);
                }

                if (!texture)
                        goto draw_placeholder;

                // Source rect = full texture
                core::rect<s32> src(
                        0, 0,
                        static_cast<s32>(texture->getOriginalSize().Width),
                        static_cast<s32>(texture->getOriginalSize().Height));

                // Tint color handling
                video::SColor tint = toSColor(img.backgroundColor);
                if (img.backgroundColor.a == 0 && img.backgroundColor.r == 0
                                && img.backgroundColor.g == 0 && img.backgroundColor.b == 0) {
                        tint = packedToSColor(Theme::Renderer::defaultImageTint);
                }

                if (is9Slice) {
                        // 9-slice rendering: stretch the middle, keep corners fixed
                        core::rect<s32> middle(
                                middleInset, middleInset,
                                static_cast<s32>(texture->getOriginalSize().Width) - middleInset,
                                static_cast<s32>(texture->getOriginalSize().Height) - middleInset);
                        draw2DImage9Slice(m_driver, texture, dest, src, middle, clipRect);
                } else {
                        // Simple stretched image with DPI-aware scaling
                        draw2DImageFilterScaled(m_driver, texture, dest, src, clipRect, nullptr, true);
                }
                return;
        }

draw_placeholder:
        // Placeholder: magenta rect with cross pattern so missing images are visible
        m_driver->draw2DRectangle(
                packedToSColor(Theme::Renderer::placeholderFillColor), dest, clipRect);
        // Draw an X to make it obviously a placeholder
        m_driver->draw2DLine(
                core::position2di(dest.UpperLeftCorner.X, dest.UpperLeftCorner.Y),
                core::position2di(dest.LowerRightCorner.X, dest.LowerRightCorner.Y),
                packedToSColor(Theme::Renderer::placeholderXColor));
        m_driver->draw2DLine(
                core::position2di(dest.LowerRightCorner.X, dest.UpperLeftCorner.Y),
                core::position2di(dest.UpperLeftCorner.X, dest.LowerRightCorner.Y),
                packedToSColor(Theme::Renderer::placeholderXColor));
}

void ClayIrrlichtRenderer::drawBorder(const Clay_RenderCommand &cmd,
        void *clipRectPtr)
{
        auto &bb = cmd.boundingBox;
        auto &bd = cmd.renderData.border;

        // Draw the four edges using the shared border color
        auto drawLine = [&](int x1, int y1, int x2, int y2, Clay_Color c) {
                m_driver->draw2DLine(
                        core::position2di(x1, y1),
                        core::position2di(x2, y2),
                        toSColor(c));
        };

        int left = static_cast<int>(bb.x);
        int top = static_cast<int>(bb.y);
        int right = static_cast<int>(bb.x + bb.width);
        int bottom = static_cast<int>(bb.y + bb.height);

        // Clay v0.14: border color is shared across all sides (Clay_BorderRenderData.color)
        if (bd.width.left)
                for (int i = 0; i < bd.width.left; i++)
                        drawLine(left + i, top, left + i, bottom, bd.color);

        if (bd.width.right)
                for (int i = 0; i < bd.width.right; i++)
                        drawLine(right - 1 - i, top, right - 1 - i, bottom, bd.color);

        if (bd.width.top)
                for (int i = 0; i < bd.width.top; i++)
                        drawLine(left, top + i, right, top + i, bd.color);

        if (bd.width.bottom)
                for (int i = 0; i < bd.width.bottom; i++)
                        drawLine(left, bottom - 1 - i, right, bottom - 1 - i, bd.color);
}

void ClayIrrlichtRenderer::drawCustom(const Clay_RenderCommand &cmd,
        void *clipRectPtr)
{
        auto *clipRect = static_cast<core::rect<s32> *>(clipRectPtr);
        auto &bb = cmd.boundingBox;
        auto &custom = cmd.renderData.custom;

        core::rect<s32> dest(
                static_cast<s32>(bb.x),
                static_cast<s32>(bb.y),
                static_cast<s32>(bb.x + bb.width),
                static_cast<s32>(bb.y + bb.height));

        // Draw the backgroundColor fill if specified (non-zero alpha)
        if (custom.backgroundColor.a > 0) {
                m_driver->draw2DRectangle(
                        toSColor(custom.backgroundColor), dest, clipRect);
        }

        // Draw a subtle border outline to make custom elements visible
        // for debugging when no backgroundColor is set.
        // Irrlicht doesn't have draw2DRectangleOutline, so we draw
        // four thin lines around the bounding box.
        if (custom.backgroundColor.a == 0) {
                video::SColor outlineColor = packedToSColor(Theme::Renderer::debugOutlineColor);
                m_driver->draw2DLine(
                        core::position2di(dest.UpperLeftCorner.X, dest.UpperLeftCorner.Y),
                        core::position2di(dest.LowerRightCorner.X, dest.UpperLeftCorner.Y),
                        outlineColor);
                m_driver->draw2DLine(
                        core::position2di(dest.LowerRightCorner.X, dest.UpperLeftCorner.Y),
                        core::position2di(dest.LowerRightCorner.X, dest.LowerRightCorner.Y),
                        outlineColor);
                m_driver->draw2DLine(
                        core::position2di(dest.LowerRightCorner.X, dest.LowerRightCorner.Y),
                        core::position2di(dest.UpperLeftCorner.X, dest.LowerRightCorner.Y),
                        outlineColor);
                m_driver->draw2DLine(
                        core::position2di(dest.UpperLeftCorner.X, dest.LowerRightCorner.Y),
                        core::position2di(dest.UpperLeftCorner.X, dest.UpperLeftCorner.Y),
                        outlineColor);
        }

        // Note: cornerRadius for custom elements is tracked but not yet
        // rendered due to Irrlicht's lack of native rounded-rect support.
        // A proper implementation would use stencil clipping or a
        // shader-based approach for rounded corners.
        (void)custom.cornerRadius;
}
