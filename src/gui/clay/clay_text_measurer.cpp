/*
Luantis — Clay Text Measurer Implementation
Compiled with C++20 for Clay designated initializer support.
*/

#include <clay.h>

#include "clay_text_measurer.h"

#include "client/fontengine.h"
#include "log.h"

ClayTextMeasurer::ClayTextMeasurer(FontEngine *font_engine)
	: m_font_engine(font_engine)
{
}

void ClayTextMeasurer::install()
{
	Clay_SetMeasureTextFunction(&ClayTextMeasurer::measureText,
		reinterpret_cast<void*>(this));
}

Clay_Dimensions ClayTextMeasurer::measureText(Clay_StringSlice text,
		Clay_TextElementConfig *config, void *userData)
{
	auto *measurer = reinterpret_cast<ClayTextMeasurer*>(userData);

	u32 font_size = 16;
	u32 font_mode = 0;
	bool bold = false, italic = false;
	if (config) {
		decodeFontSpec(config->userData, font_size, font_mode, bold, italic);
	}

	irr::gui::IGUIFont *font = nullptr;
	if (measurer->m_font_engine && font_size > 0) {
		FontMode mode = font_mode == 1 ? FM_Mono : FM_Standard;
		font = measurer->m_font_engine->getFont(font_size, mode);
	}

	if (!font) {
		return Clay_Dimensions {
			.width = (float)text.length * (float)font_size * 0.6f,
			.height = (float)font_size
		};
	}

	std::string utf8(text.chars, text.length);
	std::wstring wide = utf8_to_wide(utf8);

	irr::core::dimension2d<u32> dim = font->getDimension(wide.c_str());
	return Clay_Dimensions {
		.width = (float)dim.Width,
		.height = (float)dim.Height
	};
}

uintptr_t ClayTextMeasurer::encodeFontSpec(uint32_t size, uint32_t mode, bool bold, bool italic)
{
	return (uintptr_t)(size & 0x7FF)
		| ((uintptr_t)(mode & 0x3) << 11)
		| ((uintptr_t)(bold ? 1 : 0) << 13)
		| ((uintptr_t)(italic ? 1 : 0) << 14);
}

void ClayTextMeasurer::decodeFontSpec(uintptr_t encoded, uint32_t &size, uint32_t &mode, bool &bold, bool &italic)
{
	size = encoded & 0x7FF;
	mode = (encoded >> 11) & 0x3;
	bold = (encoded >> 13) & 0x1;
	italic = (encoded >> 14) & 0x1;
}
