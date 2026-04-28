/*
Luantis — Clay UI Integration
Copyright (C) Luantis contributors

This file is part of Luantis.

Clay (C Layout) is a high-performance 2D UI layout library by nicbarker.
It provides flexbox-like layout, scrolling, floating elements, and
outputs renderer-agnostic render commands that we translate to Irrlicht
driver calls.

This header is the master include for all Clay integration classes.
It does NOT include clay.h (which requires C++20).
*/

#pragma once

#include <string>
#include <cstdint>

/// Forward declaration of Clay element ID (defined in clay.h)
struct Clay_ElementId;

/// Interface for a Clay-based UI panel.
/// Each panel is a self-contained UI that declares its layout between
/// Clay_BeginLayout() / Clay_EndLayout() and handles its own events.
class IClayPanel {
public:
	virtual ~IClayPanel() = default;

	/// Unique string ID for this panel (e.g. "pause_menu", "voice_overlay")
	virtual const std::string &getId() const = 0;

	/// Whether this panel should be rendered this frame
	virtual bool isVisible() const = 0;
	virtual void setVisible(bool v) = 0;

	/// Called between Clay_BeginLayout() and Clay_EndLayout().
	/// Use CLAY() / CLAY_TEXT() macros to declare the UI tree.
	virtual void buildLayout() = 0;

	/// Called after Clay_EndLayout() with the full render command array.
	/// Use for custom post-layout processing (e.g. binding actions to buttons).
	virtual void onLayoutComplete() = 0;

	/// Feed a mouse click event. Returns true if the panel consumed it.
	/// @param element_id  The Clay element ID that was clicked (if any)
	virtual bool handleClick(const Clay_ElementId &element_id) = 0;
};
