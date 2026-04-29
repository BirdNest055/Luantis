/*
 * Clay Responsive Layout Utilities
 *
 * Provides breakpoint detection, font/padding scaling, and responsive
 * sizing helpers for Clay-based UI panels. Uses Clay_GetLayoutDimensions()
 * to query the current viewport at layout time.
 *
 * Breakpoint thresholds:
 *   Mobile:  < 600px
 *   Tablet:  600–1023px
 *   Desktop: >= 1024px
 *
 * Part of the Luantis Clay GUI integration (v9.47).
 */

#pragma once

#include "clay.h"

#include <algorithm>
#include <cstdint>

// ---------------------------------------------------------------------------
// Breakpoint system
// ---------------------------------------------------------------------------

enum class ClayBreakpoint {
	Mobile,   // < 600px wide
	Tablet,   // 600–1023px
	Desktop,  // >= 1024px
};

// Thresholds (pixels) — can be tuned per game requirements
constexpr float CLAY_BREAKPOINT_MOBILE_MAX  = 600.0f;
constexpr float CLAY_BREAKPOINT_TABLET_MAX  = 1024.0f;

/**
 * Get the current viewport breakpoint by querying Clay's layout dimensions.
 * Safe to call inside buildLayout() — Clay_GetLayoutDimensions() is valid
 * between Clay_BeginLayout() and Clay_EndLayout().
 */
inline ClayBreakpoint clayGetBreakpoint()
{
	Clay_Dimensions dims = Clay_GetLayoutDimensions();
	if (dims.width < CLAY_BREAKPOINT_MOBILE_MAX)
		return ClayBreakpoint::Mobile;
	if (dims.width < CLAY_BREAKPOINT_TABLET_MAX)
		return ClayBreakpoint::Tablet;
	return ClayBreakpoint::Desktop;
}

/**
 * Get the current viewport dimensions.
 * Convenience wrapper around Clay_GetLayoutDimensions().
 */
inline Clay_Dimensions clayGetViewport()
{
	return Clay_GetLayoutDimensions();
}

// ---------------------------------------------------------------------------
// Scaling factors per breakpoint
// ---------------------------------------------------------------------------

/**
 * Scale a padding value by the current breakpoint's density factor.
 * Mobile uses tighter padding to preserve screen real estate.
 */
inline float clayScalePadding(float basePx)
{
	switch (clayGetBreakpoint()) {
	case ClayBreakpoint::Mobile:  return basePx * 0.75f;
	case ClayBreakpoint::Tablet:  return basePx * 0.90f;
	case ClayBreakpoint::Desktop: return basePx * 1.00f;
	}
	return basePx;
}

/**
 * Scale a font size by the current breakpoint.
 * Mobile gets slightly smaller text, desktop stays at base size.
 * The DPI scale factor is applied separately in the renderer.
 */
inline uint16_t clayScaleFontSize(uint16_t basePx)
{
	float factor = 1.0f;
	switch (clayGetBreakpoint()) {
	case ClayBreakpoint::Mobile:  factor = 0.85f; break;
	case ClayBreakpoint::Tablet:  factor = 0.95f; break;
	case ClayBreakpoint::Desktop: factor = 1.00f; break;
	}
	return static_cast<uint16_t>(std::max(8.0f, basePx * factor));
}

/**
 * Scale a corner radius value by the current breakpoint.
 */
inline float clayScaleCornerRadius(float basePx)
{
	switch (clayGetBreakpoint()) {
	case ClayBreakpoint::Mobile:  return basePx * 0.75f;
	case ClayBreakpoint::Tablet:  return basePx * 0.90f;
	case ClayBreakpoint::Desktop: return basePx * 1.00f;
	}
	return basePx;
}

/**
 * Scale a child gap value by the current breakpoint.
 */
inline float clayScaleGap(float basePx)
{
	switch (clayGetBreakpoint()) {
	case ClayBreakpoint::Mobile:  return basePx * 0.75f;
	case ClayBreakpoint::Tablet:  return basePx * 0.90f;
	case ClayBreakpoint::Desktop: return basePx * 1.00f;
	}
	return basePx;
}

// ---------------------------------------------------------------------------
// Responsive sizing helpers
// ---------------------------------------------------------------------------

/**
 * Get a responsive content column width for dialog-like panels.
 * Returns a Clay_SizingAxis that grows between min and max.
 * On mobile, the max is reduced to prevent overflow.
 */
inline Clay_SizingAxis clayDialogContentWidth(float minW = 200.0f, float maxW = 400.0f)
{
	auto bp = clayGetBreakpoint();
	float effectiveMax = (bp == ClayBreakpoint::Mobile)
		? std::min(maxW, static_cast<float>(clayGetViewport().width) * 0.92f)
		: maxW;
	float effectiveMin = std::min(minW, effectiveMax);
	return CLAY_SIZING_GROW(effectiveMin, effectiveMax);
}

/**
 * Get a responsive button width that fills its parent within bounds.
 */
inline Clay_SizingAxis clayButtonWidth(float minW = 200.0f, float maxW = 400.0f)
{
	return CLAY_SIZING_GROW(minW, maxW);
}

/**
 * Get a responsive button height with min/max touch target bounds.
 * Mobile gets taller touch targets.
 */
inline Clay_SizingAxis clayButtonHeight(float baseH = 50.0f)
{
	auto bp = clayGetBreakpoint();
	float minH = (bp == ClayBreakpoint::Mobile) ? 44.0f : baseH;
	float maxH = baseH * 1.2f;
	return CLAY_SIZING_FIT(minH, maxH);
}

/**
 * Get responsive title height.
 */
inline Clay_SizingAxis clayTitleHeight(float baseH = 60.0f)
{
	return CLAY_SIZING_FIT(baseH * 0.7f, baseH * 1.3f);
}

// ---------------------------------------------------------------------------
// Padding helpers
// ---------------------------------------------------------------------------

/**
 * Create a Clay_Padding with all sides scaled by breakpoint.
 */
inline Clay_Padding clayPadAll(float base)
{
	float s = clayScalePadding(base);
	auto v = static_cast<uint16_t>(s);
	return Clay_Padding{v, v, v, v};
}

/**
 * Create a Clay_Padding with horizontal and vertical scaled values.
 */
inline Clay_Padding clayPadHV(float baseH, float baseV)
{
	float sh = clayScalePadding(baseH);
	float sv = clayScalePadding(baseV);
	return Clay_Padding{
		static_cast<uint16_t>(sh),
		static_cast<uint16_t>(sh),
		static_cast<uint16_t>(sv),
		static_cast<uint16_t>(sv)};
}

/**
 * Create a Clay_Padding with individually specified sides, all scaled.
 */
inline Clay_Padding clayPad(float left, float right, float top, float bottom)
{
	return Clay_Padding{
		static_cast<uint16_t>(clayScalePadding(left)),
		static_cast<uint16_t>(clayScalePadding(right)),
		static_cast<uint16_t>(clayScalePadding(top)),
		static_cast<uint16_t>(clayScalePadding(bottom))};
}

// ---------------------------------------------------------------------------
// Corner radius helpers
// ---------------------------------------------------------------------------

/**
 * Create a Clay_CornerRadius with all corners scaled by breakpoint.
 */
inline Clay_CornerRadius clayRadiusAll(float base)
{
	float s = clayScaleCornerRadius(base);
	auto v = static_cast<uint16_t>(s);
	return Clay_CornerRadius{v, v, v, v};
}

/**
 * Create a Clay_CornerRadius with top corners rounded, bottom flat.
 */
inline Clay_CornerRadius clayRadiusTop(float base)
{
	float s = clayScaleCornerRadius(base);
	auto v = static_cast<uint16_t>(s);
	return Clay_CornerRadius{v, v, 0, 0};
}

/**
 * Create a Clay_CornerRadius with bottom corners rounded, top flat.
 */
inline Clay_CornerRadius clayRadiusBottom(float base)
{
	float s = clayScaleCornerRadius(base);
	auto v = static_cast<uint16_t>(s);
	return Clay_CornerRadius{0, 0, v, v};
}
