/*
Luantis — Clay Irrlicht Renderer
Translates Clay render commands to Irrlicht video::IVideoDriver calls.

Note: Does NOT include clay.h. Uses forward declarations only.
*/

#pragma once

#include <vector>
#include <cstdint>

namespace irr {
namespace video { class IVideoDriver; }
namespace gui { class IGUIFont; }
class SEvent;
}

class FontEngine;

// Forward declarations for Clay types
struct Clay_RenderCommand;
struct Clay_RenderCommandArray;
struct Clay_BoundingBox;
struct Clay_Color;

/// Renders Clay's output command list using Irrlicht's 2D drawing API.
/// Call render() once per frame after Clay_EndLayout().
class ClayIrrlichtRenderer {
public:
	ClayIrrlichtRenderer(irr::video::IVideoDriver *driver, FontEngine *font_engine);

	/// Render all commands in the array.
	void render(const Clay_RenderCommandArray &commands);

private:
	void renderRectangle(const Clay_RenderCommand &cmd);
	void renderText(const Clay_RenderCommand &cmd);
	void renderImage(const Clay_RenderCommand &cmd);
	void renderBorder(const Clay_RenderCommand &cmd);
	void renderCustom(const Clay_RenderCommand &cmd);
	void pushScissor(const Clay_RenderCommand &cmd);
	void popScissor();

	irr::video::IVideoDriver *m_driver;
	FontEngine *m_font_engine;

	/// Stack of clip rects for nested scissor commands
	std::vector<irr::core::rect<irr::s32>> m_scissor_stack;

	/// DPI scaling factor (matches HUD scaling)
	float m_scale_factor = 1.0f;
};
