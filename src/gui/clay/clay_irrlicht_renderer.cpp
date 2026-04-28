/*
Luantis — Clay Irrlicht Renderer Implementation
Translates Clay render commands to Irrlicht video::IVideoDriver calls.
*/

// This file is compiled with C++20 for Clay designated initializer support
#include <clay.h>

#include "clay_irrlicht_renderer.h"

#include <irrlicht.h>
#include "client/fontengine.h"
#include "log.h"

/// Convert Clay_Color {r,g,b,a in 0.0-1.0} to Irrlicht SColor
static inline irr::video::SColor clayToSColor(Clay_Color c)
{
	return irr::video::SColor(
		(u8)(c.a * 255.0f),
		(u8)(c.r * 255.0f),
		(u8)(c.g * 255.0f),
		(u8)(c.b * 255.0f)
	);
}

/// Convert Clay_BoundingBox {x, y, width, height} to Irrlicht rect
static inline irr::core::rect<irr::s32> clayToRect(const Clay_BoundingBox &bb)
{
	return irr::core::rect<irr::s32>(
		(irr::s32)bb.x,
		(irr::s32)bb.y,
		(irr::s32)(bb.x + bb.width),
		(irr::s32)(bb.y + bb.height)
	);
}

ClayIrrlichtRenderer::ClayIrrlichtRenderer(irr::video::IVideoDriver *driver,
		FontEngine *font_engine)
	: m_driver(driver)
	, m_font_engine(font_engine)
{
}

void ClayIrrlichtRenderer::render(const Clay_RenderCommandArray &commands)
{
	for (u32 i = 0; i < commands.length; i++) {
		const Clay_RenderCommand &cmd = commands.internalArray[i];
		switch (cmd.commandType) {
		case CLAY_RENDER_COMMAND_TYPE_RECTANGLE:
			renderRectangle(cmd);
			break;
		case CLAY_RENDER_COMMAND_TYPE_TEXT:
			renderText(cmd);
			break;
		case CLAY_RENDER_COMMAND_TYPE_IMAGE:
			renderImage(cmd);
			break;
		case CLAY_RENDER_COMMAND_TYPE_BORDER:
			renderBorder(cmd);
			break;
		case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
			renderCustom(cmd);
			break;
		case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
			pushScissor(cmd);
			break;
		case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
			popScissor();
			break;
		default:
			break;
		}
	}

	// Pop any remaining scissor rects (shouldn't happen with well-formed Clay output)
	while (!m_scissor_stack.empty())
		popScissor();
}

void ClayIrrlichtRenderer::renderRectangle(const Clay_RenderCommand &cmd)
{
	irr::video::SColor color = clayToSColor(cmd.renderData.rectangle.backgroundColor);
	irr::core::rect<irr::s32> rect = clayToRect(cmd.boundingBox);
	const irr::core::rect<irr::s32> *clip = m_scissor_stack.empty()
		? nullptr : &m_scissor_stack.back();

	m_driver->draw2DRectangle(color, rect, clip);
}

void ClayIrrlichtRenderer::renderText(const Clay_RenderCommand &cmd)
{
	const Clay_TextRenderData &text_data = cmd.renderData.text;
	irr::video::SColor color = clayToSColor(text_data.textColor);
	irr::core::rect<irr::s32> rect = clayToRect(cmd.boundingBox);
	const irr::core::rect<irr::s32> *clip = m_scissor_stack.empty()
		? nullptr : &m_scissor_stack.back();

	// Get the font from FontEngine based on userData (font spec encoding)
	irr::gui::IGUIFont *font = nullptr;
	if (m_font_engine) {
		u32 font_size = (cmd.userData & 0x7FF);
		if (font_size == 0)
			font_size = 16; // default
		font = m_font_engine->getFont(font_size, FM_Standard);
	}

	if (!font) {
		// Fallback: try Irrlicht's built-in font
		irr::gui::IGUIEnvironment *env = m_driver->getGUIEnvironment();
		if (env)
			font = env->getBuiltInFont();
	}

	if (font) {
		// Clay_StringSlice is not null-terminated, so we need to copy it
		std::string text(text_data.stringContents.chars, text_data.stringContents.length);
		std::wstring wtext = utf8_to_wide(text);
		font->draw(wtext.c_str(), rect, color, false, false, clip);
	}
}

void ClayIrrlichtRenderer::renderImage(const Clay_RenderCommand &cmd)
{
	const Clay_ImageRenderData &img_data = cmd.renderData.image;
	irr::video::SColor tint = clayToSColor(img_data.backgroundColor);
	irr::core::rect<irr::s32> rect = clayToRect(cmd.boundingBox);
	const irr::core::rect<irr::s32> *clip = m_scissor_stack.empty()
		? nullptr : &m_scissor_stack.back();

	// If userData contains an ITexture*, render it
	irr::video::ITexture *texture = reinterpret_cast<irr::video::ITexture*>(cmd.userData);
	if (texture) {
		m_driver->draw2DImage(texture,
			irr::core::position2d<irr::s32>(rect.UpperLeftCorner.X, rect.UpperLeftCorner.Y),
			irr::core::rect<irr::s32>(0, 0, texture->getOriginalSize().Width, texture->getOriginalSize().Height),
			clip, nullptr, true);
	} else {
		// No texture — draw a colored placeholder
		m_driver->draw2DRectangle(tint, rect, clip);
	}
}

void ClayIrrlichtRenderer::renderBorder(const Clay_RenderCommand &cmd)
{
	const Clay_BorderRenderData &border = cmd.renderData.border;
	irr::video::SColor color = clayToSColor(border.color);
	irr::core::rect<irr::s32> rect = clayToRect(cmd.boundingBox);
	const irr::core::rect<irr::s32> *clip = m_scissor_stack.empty()
		? nullptr : &m_scissor_stack.back();

	float top = border.width.top;
	float right = border.width.right;
	float bottom = border.width.bottom;
	float left = border.width.left;

	if (top > 0) {
		m_driver->draw2DRectangle(color,
			irr::core::rect<irr::s32>(rect.UpperLeftCorner.X, rect.UpperLeftCorner.Y,
				rect.LowerRightCorner.X, rect.UpperLeftCorner.Y + (irr::s32)top),
			clip);
	}
	if (bottom > 0) {
		m_driver->draw2DRectangle(color,
			irr::core::rect<irr::s32>(rect.UpperLeftCorner.X, rect.LowerRightCorner.Y - (irr::s32)bottom,
				rect.LowerRightCorner.X, rect.LowerRightCorner.Y),
			clip);
	}
	if (left > 0) {
		m_driver->draw2DRectangle(color,
			irr::core::rect<irr::s32>(rect.UpperLeftCorner.X, rect.UpperLeftCorner.Y,
				rect.UpperLeftCorner.X + (irr::s32)left, rect.LowerRightCorner.Y),
			clip);
	}
	if (right > 0) {
		m_driver->draw2DRectangle(color,
			irr::core::rect<irr::s32>(rect.LowerRightCorner.X - (irr::s32)right, rect.UpperLeftCorner.Y,
				rect.LowerRightCorner.X, rect.LowerRightCorner.Y),
			clip);
	}
}

void ClayIrrlichtRenderer::renderCustom(const Clay_RenderCommand &cmd)
{
	// Custom elements are rendered by their owning IClayPanel
	// For now, draw a debug rectangle
	irr::video::SColor debug_color(128, 255, 0, 255);
	irr::core::rect<irr::s32> rect = clayToRect(cmd.boundingBox);
	const irr::core::rect<irr::s32> *clip = m_scissor_stack.empty()
		? nullptr : &m_scissor_stack.back();
	m_driver->draw2DRectangle(debug_color, rect, clip);
}

void ClayIrrlichtRenderer::pushScissor(const Clay_RenderCommand &cmd)
{
	irr::core::rect<irr::s32> new_rect = clayToRect(cmd.boundingBox);

	if (!m_scissor_stack.empty()) {
		new_rect.clipAgainst(m_scissor_stack.back());
	}

	m_scissor_stack.push_back(new_rect);
}

void ClayIrrlichtRenderer::popScissor()
{
	if (!m_scissor_stack.empty())
		m_scissor_stack.pop_back();
}
