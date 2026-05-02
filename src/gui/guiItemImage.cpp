// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#include "guiItemImage.h"
#include "client/client.h"
#include "drawItemStack.h"
#include "inventory.h"
#include <IGUIFont.h>
#include "GUITheme.h"

GUIItemImage::GUIItemImage(gui::IGUIEnvironment *env, gui::IGUIElement *parent,
        s32 id, const core::rect<s32> &rectangle, const std::string &item_name,
        gui::IGUIFont *font, Client *client) :
        gui::IGUIElement(gui::EGUIET_ELEMENT, env, parent, id, rectangle),
        m_item_name(item_name), m_font(font), m_client(client), m_label(core::stringw())
{
}

void GUIItemImage::draw()
{
        if (!IsVisible)
                return;

        if (!m_client) {
                IGUIElement::draw();
                return;
        }

        IItemDefManager *idef = m_client->idef();
        ItemStack item;
        item.deSerialize(m_item_name, idef);
        // Viewport rectangle on screen
        core::rect<s32> rect = core::rect<s32>(AbsoluteRect);
        drawItemStack(Environment->getVideoDriver(), m_font, item, rect,
                        &AbsoluteClippingRect, m_client, IT_ROT_NONE);
        // Batch 32: null check for font before using it to draw label
        if (m_font) {
                video::SColor color = GUITheme::Colors::IMAGE_DRAW_DEFAULT;
                m_font->draw(m_label, rect, color, true, true, &AbsoluteClippingRect);
        }

        IGUIElement::draw();
}
