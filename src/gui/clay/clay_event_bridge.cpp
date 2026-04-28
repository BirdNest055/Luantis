/*
Luantis — Clay Event Bridge Implementation
*/

#include <irrlicht.h>
#include "clay_event_bridge.h"

ClayEventBridge::ClayEventBridge()
{
}

bool ClayEventBridge::onEvent(const irr::SEvent &event)
{
	switch (event.EventType) {
	case irr::EET_MOUSE_INPUT_EVENT:
		switch (event.MouseInput.Event) {
		case irr::EMIE_MOUSE_MOVED:
			m_pointer_pos.x = (float)event.MouseInput.X;
			m_pointer_pos.y = (float)event.MouseInput.Y;
			m_mouse_moved = true;
			return true;
		case irr::EMIE_LMOUSE_PRESSED_DOWN:
			m_pointer_down = true;
			return true;
		case irr::EMIE_LMOUSE_LEFT_UP:
			m_pointer_down = false;
			return true;
		case irr::EMIE_MOUSE_WHEEL:
			m_scroll_delta.y += event.MouseInput.Wheel * -50.0f;
			return true;
		default:
			break;
		}
		break;
	default:
		break;
	}
	return false;
}

void ClayEventBridge::resetFrameState()
{
	m_scroll_delta = {0, 0};
	m_mouse_moved = false;
}
