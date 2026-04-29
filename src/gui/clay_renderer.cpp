/*
 * Clay Rendering Backend for Irrlicht
 *
 * Translates Clay render commands into Irrlicht 2D draw calls.
 *
 * Part of the Luantis Clay GUI integration (v9.46).
 */

// CLAY_IMPLEMENTATION must be defined in exactly one translation unit
// before including clay.h
#define CLAY_IMPLEMENTATION
#include "clay_renderer.h"

#include <irrlicht.h>
#include "client/fontengine.h"
#include "client/renderingengine.h"
#include "log.h"
#include "porting.h"

#include <cstring>
#include <string>

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void ClayIrrlichtRenderer::init(irr::IrrlichtDevice *device)
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

        irr::gui::IGUIFont *font = self->getFont(config->fontId, config->fontSize);
        if (!font) {
                return Clay_Dimensions{
                        .width = static_cast<float>(text.length) * config->fontSize * 0.6f,
                        .height = static_cast<float>(config->fontSize)
                };
        }

        // Clay does NOT guarantee null-terminated strings
        std::string str(text.chars, text.length);
        auto dim = font->getDimension(str.c_str());

        return Clay_Dimensions{
                .width = static_cast<float>(dim.Width),
                .height = static_cast<float>(dim.Height)
        };
}

// ---------------------------------------------------------------------------
// Font lookup
// ---------------------------------------------------------------------------

irr::gui::IGUIFont *ClayIrrlichtRenderer::getFont(uint16_t fontId, uint16_t fontSize)
{
        if (!m_font_engine)
                return nullptr;

        // Map Clay fontId to FontEngine font mode.
        // We use the fontSize to determine the font size, and fontId to pick
        // the style: 0 = normal, 1 = bold, 2 = italic, 3 = monospace
        auto mode = FontMode::FM_Standard;
        switch (fontId) {
        case 1: mode = FontMode::FM_Bold; break;
        case 2: mode = FontMode::FM_Italic; break;
        case 3: mode = FontMode::FM_Mono; break;
        default: break;
        }

        // FontEngine uses a size parameter; fontSize of 0 means default
        unsigned int size = fontSize > 0 ? fontSize : 0;
        return m_font_engine->getFont(size, mode);
}

// ---------------------------------------------------------------------------
// Main render loop
// ---------------------------------------------------------------------------

void ClayIrrlichtRenderer::render(const Clay_RenderCommandArray &commands)
{
        if (!m_driver)
                return;

        for (uint32_t i = 0; i < commands.length; i++) {
                const Clay_RenderCommand &cmd = commands.internalArray[i];

                switch (cmd.commandType) {
                case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
                        drawRectangle(cmd);
                        break;
                case CLAY_RENDER_COMMAND_TYPE_TEXT:
                        drawText(cmd);
                        break;
                case CLAY_RENDER_COMMAND_TYPE_IMAGE:
                        drawImage(cmd);
                        break;
                case CLAY_RENDER_COMMAND_TYPE_BORDER:
                        drawBorder(cmd);
                        break;
                case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
                        drawCustom(cmd);
                        break;
                case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
                        // Scissoring is handled by Clay's clip system; we'll use
                        // Irrlicht's clip rect support
                        {
                                auto &bb = cmd.boundingBox;
                                m_driver->setClipRect(irr::core::recti(
                                        static_cast<int>(bb.x),
                                        static_cast<int>(bb.y),
                                        static_cast<int>(bb.x + bb.width),
                                        static_cast<int>(bb.y + bb.height)));
                        }
                        break;
                case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
                        m_driver->setClipRect(irr::core::recti());
                        break;
                default:
                        break;
                }
        }
}

// ---------------------------------------------------------------------------
// Individual command renderers
// ---------------------------------------------------------------------------

static irr::video::SColor toSColor(Clay_Color c)
{
        return irr::video::SColor(
                static_cast<irr::u32>(c.a),
                static_cast<irr::u32>(c.r),
                static_cast<irr::u32>(c.g),
                static_cast<irr::u32>(c.b));
}

void ClayIrrlichtRenderer::drawRectangle(const Clay_RenderCommand &cmd)
{
        auto &bb = cmd.boundingBox;
        auto &rect = cmd.renderData.rectangle;
        auto color = toSColor(rect.backgroundColor);

        irr::core::recti dest(
                static_cast<int>(bb.x),
                static_cast<int>(bb.y),
                static_cast<int>(bb.x + bb.width),
                static_cast<int>(bb.y + bb.height));

        // Corner radius — Irrlicht doesn't natively support rounded rects,
        // so for now we draw a simple filled rect. Rounded corners can be
        // added later using a custom shader or pre-rendered texture.
        m_driver->draw2DRectangle(color, dest);
}

void ClayIrrlichtRenderer::drawText(const Clay_RenderCommand &cmd)
{
        auto &bb = cmd.boundingBox;
        auto &td = cmd.renderData.text;

        irr::gui::IGUIFont *font = getFont(td.textElementConfig->fontId,
                td.textElementConfig->fontSize);
        if (!font)
                return;

        // Clay does NOT guarantee null termination
        std::string str(td.stringContents.chars, td.stringContents.length);

        auto color = toSColor(td.textElementConfig->textColor);

        irr::core::position2di pos(
                static_cast<int>(bb.x),
                static_cast<int>(bb.y));

        font->draw(str.c_str(),
                irr::core::recti(pos,
                        irr::core::dimension2di(
                                static_cast<int>(bb.width),
                                static_cast<int>(bb.height))),
                color, false, false);
}

void ClayIrrlichtRenderer::drawImage(const Clay_RenderCommand &cmd)
{
        // Image rendering requires texture lookup from the Clay image config's
        // userData pointer. For now this is a placeholder — image rendering
        // will be implemented when we port dialogs that need images.
        auto &bb = cmd.boundingBox;
        irr::core::recti dest(
                static_cast<int>(bb.x),
                static_cast<int>(bb.y),
                static_cast<int>(bb.x + bb.width),
                static_cast<int>(bb.y + bb.height));

        // Draw a placeholder magenta rect so missing images are visible
        m_driver->draw2DRectangle(
                irr::video::SColor(255, 255, 0, 255), dest);
}

void ClayIrrlichtRenderer::drawBorder(const Clay_RenderCommand &cmd)
{
        auto &bb = cmd.boundingBox;
        auto &bd = cmd.renderData.border;

        irr::core::recti dest(
                static_cast<int>(bb.x),
                static_cast<int>(bb.y),
                static_cast<int>(bb.x + bb.width),
                static_cast<int>(bb.y + bb.height));

        // Draw the four edges
        auto drawLine = [&](int x1, int y1, int x2, int y2, Clay_Color c) {
                m_driver->draw2DLine(
                        irr::core::position2di(x1, y1),
                        irr::core::position2di(x2, y2),
                        toSColor(c));
        };

        int left = static_cast<int>(bb.x);
        int top = static_cast<int>(bb.y);
        int right = static_cast<int>(bb.x + bb.width);
        int bottom = static_cast<int>(bb.y + bb.height);

        if (bd.color.left.r || bd.color.left.g || bd.color.left.b || bd.color.left.a)
                for (int i = 0; i < bd.width.left; i++)
                        drawLine(left + i, top, left + i, bottom, bd.color.left);

        if (bd.color.right.r || bd.color.right.g || bd.color.right.b || bd.color.right.a)
                for (int i = 0; i < bd.width.right; i++)
                        drawLine(right - 1 - i, top, right - 1 - i, bottom, bd.color.right);

        if (bd.color.top.r || bd.color.top.g || bd.color.top.b || bd.color.top.a)
                for (int i = 0; i < bd.width.top; i++)
                        drawLine(left, top + i, right, top + i, bd.color.top);

        if (bd.color.bottom.r || bd.color.bottom.g || bd.color.bottom.b || bd.color.bottom.a)
                for (int i = 0; i < bd.width.bottom; i++)
                        drawLine(left, bottom - 1 - i, right, bottom - 1 - i, bd.color.bottom);
}

void ClayIrrlichtRenderer::drawCustom(const Clay_RenderCommand &cmd)
{
        // Custom elements can be used for inventory slots, 3D model previews, etc.
        // Will be implemented in future versions.
}
