/*
Luantis — Clay Event Bridge
Routes Irrlicht SEvent → Clay pointer state updates.

Note: Does NOT include clay.h. Uses simple float structs instead.
*/

#pragma once

namespace irr { struct SEvent; }

/// Simple 2D vector matching Clay_Vector2 layout
struct ClayEventVec2 {
	float x, y;
};

/// Bridges Irrlicht input events to Clay's pointer state.
/// Clay needs: pointer position, left mouse button state, and scroll delta.
class ClayEventBridge {
public:
	ClayEventBridge();

	/// Feed an Irrlicht event. Returns true if the event was relevant to Clay
	/// (mouse move, click, or scroll) and should potentially be consumed.
	bool onEvent(const irr::SEvent &event);

	/// Get current pointer position for Clay_SetPointerState()
	ClayEventVec2 getPointerPos() const { return m_pointer_pos; }
	bool isPointerDown() const { return m_pointer_down; }

	/// Get accumulated scroll delta for Clay_UpdateScrollContainers()
	ClayEventVec2 getScrollDelta() const { return m_scroll_delta; }

	/// Reset per-frame state (call at start of each frame)
	void resetFrameState();

private:
	ClayEventVec2 m_pointer_pos = {0, 0};
	bool m_pointer_down = false;
	ClayEventVec2 m_scroll_delta = {0, 0};
	bool m_mouse_moved = false;
};
