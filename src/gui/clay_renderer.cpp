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

#include <irrlicht.h>
#include "IGUIFont.h"
#include "client/fontengine.h"
#include "client/renderingengine.h"
#include "log.h"
#include "porting.h"

#include <cstring>
#include <string>
#include <codecvt>
#include <locale>

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
                        .width = static_cast<float>(text.length) * config->fontSize * 0.6f,
                        .height = static_cast<float>(config->fontSize)
                };
        }

        gui::IGUIFont *font = self->getFont(config->fontId, config->fontSize);
        if (!font) {
                return Clay_Dimensions{
                        .width = static_cast<float>(text.length) * config->fontSize * 0.6f,
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
        // fontId 0 or 3 = FM_Mono (monospace), everything else = FM_Standard.
        // Note: Luantis's FontEngine only has FM_Standard and FM_Mono —
        // no bold/italic variants are available.
        auto mode = FontMode::FM_Standard;
        if (fontId == 3)
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

void ClayIrrlichtRenderer::render(const Clay_RenderCommandArray &commands)
{
        if (!m_driver)
                return;

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

        // Clay's imageData is a transparent pointer — we expect it to be
        // an Irrlicht video::ITexture* that was set during layout.
        // If imageData is null or the texture is invalid, draw a placeholder.
        if (img.imageData) {
                auto *texture = static_cast<video::ITexture *>(img.imageData);
                if (texture) {
                        // Source rect = full texture
                        core::rect<s32> src(
                                0, 0,
                                static_cast<s32>(texture->getOriginalSize().Width),
                                static_cast<s32>(texture->getOriginalSize().Height));

                        // If a tint color is specified, use it; otherwise full opacity
                        video::SColor tint = toSColor(img.backgroundColor);
                        if (img.backgroundColor.a == 0 && img.backgroundColor.r == 0
                                        && img.backgroundColor.g == 0 && img.backgroundColor.b == 0) {
                                tint = video::SColor(255, 255, 255, 255);
                        }

                        // Use alpha-aware draw if a clip rect is present
                        const core::rect<s32> *clipPtr = clipRect ? clipRect : nullptr;
                        m_driver->draw2DImage(texture, dest, src, clipPtr, nullptr, true);
                        return;
                }
        }

        // Placeholder: magenta rect with cross pattern so missing images are visible
        m_driver->draw2DRectangle(
                video::SColor(255, 255, 0, 255), dest, clipRect);
        // Draw an X to make it obviously a placeholder
        m_driver->draw2DLine(
                core::position2di(dest.UpperLeftCorner.X, dest.UpperLeftCorner.Y),
                core::position2di(dest.LowerRightCorner.X, dest.LowerRightCorner.Y),
                video::SColor(255, 0, 0, 0));
        m_driver->draw2DLine(
                core::position2di(dest.LowerRightCorner.X, dest.UpperLeftCorner.Y),
                core::position2di(dest.UpperLeftCorner.X, dest.LowerRightCorner.Y),
                video::SColor(255, 0, 0, 0));
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
                video::SColor outlineColor(80, 128, 128, 128);
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
