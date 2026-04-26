// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

/*
 * Server packet handler — Game interaction methods
 *
 * handleCommand_Interact, handleCommand_InventoryAction, handleCommand_ChatMessage,
 * handleCommand_Damage, handleCommand_PlayerItem, handleCommand_NodeMetaFields,
 * handleCommand_InventoryFields
 *
 * Helper functions: checkInteractDistance, getWieldedItem
 */

#include "chatmessage.h"
#include "server.h"
#include "serverenvironment.h"
#include "log.h"
#include "emerge.h"
#include "itemdef.h"
#include "mapblock.h"
#include "nodedef.h"
#include "remoteplayer.h"
#include "rollback_interface.h"
#include "scripting_server.h"
#include "serialization.h"
#include "settings.h"
#include "tool.h"
#include "network/connection.h"
#include "network/networkpacket.h"
#include "network/networkprotocol.h"
#include "network/serveropcodes.h"
#include "server/player_sao.h"
#include "server/serverinventorymgr.h"
#include "util/pointedthing.h"

#include <algorithm>

bool Server::checkInteractDistance(RemotePlayer *player, const f32 d, const std::string &what)
{
	ItemStack selected_item, hand_item;
	const ItemStack &tool_item = player->getWieldedItem(&selected_item, &hand_item);
	f32 max_d = BS * getToolRange(tool_item, hand_item, m_itemdef);

	// Cube diagonal * 1.5 for maximal supported node extents:
	// sqrt(3) * 1.5 ≅ 2.6
	if (d > max_d + 2.6f * BS) {
		actionstream << "Player " << player->getName()
				<< " tried to access " << what
				<< " from too far: "
				<< "d=" << d << ", max_d=" << max_d
				<< "; ignoring." << std::endl;
		// Call callbacks
		m_script->on_cheat(player->getPlayerSAO(), "interacted_too_far");
		return false;
	}
	return true;
}

// Tiny helper to retrieve the selected item into an std::optional
static inline void getWieldedItem(const PlayerSAO *playersao, std::optional<ItemStack> &ret)
{
	ret = ItemStack();
	playersao->getWieldedItem(&(*ret));
}

void Server::handleCommand_Interact(NetworkPacket *pkt)
{
	/*
		[0] u16 command
		[2] u8 action
		[3] u16 item
		[5] u32 length of the next item (plen)
		[9] serialized PointedThing
		[9 + plen] player position information
	*/

	InteractAction action;
	u16 item_i;

	*pkt >> (u8 &)action;
	*pkt >> item_i;

	std::istringstream tmp_is(pkt->readLongString(), std::ios::binary);
	PointedThing pointed;
	pointed.deSerialize(tmp_is);

	verbosestream << "TOSERVER_INTERACT: action=" << (int)action << ", item="
			<< item_i << ", pointed=" << pointed.dump() << std::endl;

	session_t peer_id = pkt->getPeerId();
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player) {
		warningstream << FUNCTION_NAME << ": player is null" << std::endl;
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if (!playersao) {
		warningstream << FUNCTION_NAME << ": player SAO is null" << std::endl;
		return;
	}

	if (playersao->isDead()) {
		actionstream << "Server: " << player->getName()
				<< " tried to interact while dead; ignoring." << std::endl;
		if (pointed.type == POINTEDTHING_NODE) {
			// Re-send block to revert change on client-side
			RemoteClient *client = getClient(peer_id);
			v3s16 blockpos = getNodeBlockPos(pointed.node_undersurface);
			client->SetBlockNotSent(blockpos);
		}
		// Call callbacks
		m_script->on_cheat(playersao, "interacted_while_dead");
		return;
	}

	process_PlayerPos(player, playersao, pkt);

	v3f player_pos = playersao->getLastGoodPosition();

	// Update wielded item
	if (player->getMaxHotbarItemcount() == 0) {
		return; // ignore silently
	} else if (item_i >= player->getMaxHotbarItemcount()) {
		actionstream << "Player " << player->getName()
			<< " tried to access item=" << item_i
			<< " out of hotbar_itemcount="
			<< player->getMaxHotbarItemcount()
			<< "; ignoring." << std::endl;
		return;
	}

	player->setWieldIndex(item_i);

	// Get pointed to object (NULL if not POINTEDTYPE_OBJECT)
	ServerActiveObject *pointed_object = NULL;
	if (pointed.type == POINTEDTHING_OBJECT) {
		pointed_object = m_env->getActiveObject(pointed.object_id);
		if (pointed_object == NULL) {
			verbosestream << "TOSERVER_INTERACT: "
				"pointed object is NULL" << std::endl;
			return;
		}

	}

	/*
		Make sure the player is allowed to do it
	*/
	if (!checkPriv(player->getName(), "interact")) {
		actionstream << player->getName() << " attempted to interact with " <<
				pointed.dump() << " without 'interact' privilege" << std::endl;

		if (pointed.type != POINTEDTHING_NODE)
			return;

		// Re-send block to revert change on client-side
		RemoteClient *client = getClient(peer_id);
		// Digging completed -> under
		if (action == INTERACT_DIGGING_COMPLETED) {
			v3s16 blockpos = getNodeBlockPos(pointed.node_undersurface);
			client->SetBlockNotSent(blockpos);
		}
		// Placement -> above
		else if (action == INTERACT_PLACE) {
			v3s16 blockpos = getNodeBlockPos(pointed.node_abovesurface);
			client->SetBlockNotSent(blockpos);
		}
		return;
	}

	/*
		Check that target is reasonably close
	*/
	static thread_local const u32 anticheat_flags =
		g_settings->getFlagStr("anticheat_flags", flagdesc_anticheat, nullptr);

	if ((action == INTERACT_START_DIGGING || action == INTERACT_DIGGING_COMPLETED ||
			action == INTERACT_PLACE || action == INTERACT_USE) &&
			(anticheat_flags & AC_INTERACTION) && !isSingleplayer()) {
		v3f target_pos = player_pos;
		if (pointed.type == POINTEDTHING_NODE) {
			target_pos = intToFloat(pointed.node_undersurface, BS);
		} else if (pointed.type == POINTEDTHING_OBJECT) {
			if (playersao->getId() == pointed_object->getId()) {
				actionstream << "Server: " << player->getName()
					<< " attempted to interact with themselves" << std::endl;
				m_script->on_cheat(playersao, "interacted_with_self");
				return;
			}
			target_pos = pointed_object->getBasePosition();
		}
		float d = playersao->getEyePosition().getDistanceFrom(target_pos);

		if (!checkInteractDistance(player, d, pointed.dump())) {
			if (pointed.type == POINTEDTHING_NODE) {
				// Re-send block to revert change on client-side
				RemoteClient *client = getClient(peer_id);
				v3s16 blockpos = getNodeBlockPos(pointed.node_undersurface);
				client->SetBlockNotSent(blockpos);
			}
			return;
		}
	}

	/*
		If something goes wrong, this player is to blame
	*/
	RollbackScopeActor rollback_scope(m_rollback,
			"player:" + player->getName());

	switch (action) {
	// Start digging or punch object
	case INTERACT_START_DIGGING: {
		if (pointed.type == POINTEDTHING_NODE) {
			MapNode n(CONTENT_IGNORE);
			bool pos_ok;

			v3s16 p_under = pointed.node_undersurface;
			n = m_env->getMap().getNode(p_under, &pos_ok);
			if (!pos_ok) {
				infostream << "Server: Not punching: Node not found. "
					"Adding block to emerge queue." << std::endl;
				m_emerge->enqueueBlockEmerge(peer_id,
					getNodeBlockPos(pointed.node_abovesurface), false);
			}

			if (n.getContent() != CONTENT_IGNORE)
				m_script->node_on_punch(p_under, n, playersao, pointed);

			// Cheat prevention
			playersao->noCheatDigStart(p_under);

			return;
		}

		// Skip if the object can't be interacted with anymore
		if (pointed.type != POINTEDTHING_OBJECT || pointed_object->isGone())
			return;

		ItemStack selected_item, hand_item;
		ItemStack tool_item = playersao->getWieldedItem(&selected_item, &hand_item);
		const ToolCapabilities &toolcap =
				tool_item.getToolCapabilities(m_itemdef, &hand_item);
		v3f dir = (pointed_object->getBasePosition() -
				(playersao->getBasePosition() + playersao->getEyeOffset())
					).normalize();
		float time_from_last_punch =
			playersao->resetTimeFromLastPunch();

		u32 wear = pointed_object->punch(dir, toolcap, playersao,
				time_from_last_punch, tool_item.wear);

		// Callback may have changed item, so get it again
		playersao->getWieldedItem(&selected_item);
		bool changed = selected_item.addWear(wear, m_itemdef);
		if (changed)
			playersao->setWieldedItem(selected_item);

		return;
	} // action == INTERACT_START_DIGGING

	case INTERACT_STOP_DIGGING:
		// Nothing to do
		return;

	case INTERACT_DIGGING_COMPLETED: {
		// Only digging of nodes
		if (pointed.type != POINTEDTHING_NODE)
			return;
		bool pos_ok;
		v3s16 p_under = pointed.node_undersurface;
		MapNode n = m_env->getMap().getNode(p_under, &pos_ok);
		if (!pos_ok) {
			infostream << "Server: Not finishing digging: Node not found. "
				"Adding block to emerge queue." << std::endl;
			m_emerge->enqueueBlockEmerge(peer_id,
				getNodeBlockPos(pointed.node_abovesurface), false);
		}

		/* Cheat prevention */
		bool is_valid_dig = true;
		if ((anticheat_flags & AC_DIGGING) && !isSingleplayer()) {
			v3s16 nocheat_p = playersao->getNoCheatDigPos();
			float nocheat_t = playersao->getNoCheatDigTime();
			playersao->noCheatDigEnd();
			// If player didn't start digging this, ignore dig
			if (nocheat_p != p_under) {
				infostream << "Server: " << player->getName()
						<< " started digging "
						<< nocheat_p << " and completed digging "
						<< p_under << "; not digging." << std::endl;
				is_valid_dig = false;
				// Call callbacks
				m_script->on_cheat(playersao, "finished_unknown_dig");
			}

			// Get player's wielded item
			// See also: Game::handleDigging
			ItemStack selected_item, hand_item;
			ItemStack &tool_item = player->getWieldedItem(&selected_item, &hand_item);

			// Get diggability and expected digging time
			DigParams params = getDigParams(m_nodedef->get(n).groups,
					&tool_item.getToolCapabilities(m_itemdef, &hand_item),
					tool_item.wear);
			// If can't dig, try hand
			if (!params.diggable) {
				params = getDigParams(m_nodedef->get(n).groups,
					&hand_item.getToolCapabilities(m_itemdef));
			}
			// If can't dig, ignore dig
			if (!params.diggable) {
				infostream << "Server: " << player->getName()
						<< " completed digging " << p_under
						<< ", which is not diggable with tool; not digging."
						<< std::endl;
				is_valid_dig = false;
				// Call callbacks
				m_script->on_cheat(playersao, "dug_unbreakable");
			}
			// Check digging time
			// If already invalidated, we don't have to
			if (!is_valid_dig) {
				// Well not our problem then
			}
			// Clean and long dig
			else if (params.time > 2.0 && nocheat_t * 1.2 > params.time) {
				// All is good, but grab time from pool; don't care if
				// it's actually available
				playersao->getDigPool().grab(params.time);
			}
			// Short or laggy dig
			// Try getting the time from pool
			else if (playersao->getDigPool().grab(params.time)) {
				// All is good
			}
			// Dig not possible
			else {
				infostream << "Server: " << player->getName()
						<< " completed digging " << p_under
						<< "too fast; not digging." << std::endl;
				is_valid_dig = false;
				// Call callbacks
				m_script->on_cheat(playersao, "dug_too_fast");
			}
		}

		/* Actually dig node */

		if (is_valid_dig && n.getContent() != CONTENT_IGNORE)
			m_script->node_on_dig(p_under, n, playersao);

		v3s16 blockpos = getNodeBlockPos(p_under);
		RemoteClient *client = getClient(peer_id);
		// Send unusual result (that is, node not being removed)
		if (m_env->getMap().getNode(p_under).getContent() != CONTENT_AIR)
			// Re-send block to revert change on client-side
			client->SetBlockNotSent(blockpos);
		else
			client->ResendBlockIfOnWire(blockpos);

		return;
	} // action == INTERACT_DIGGING_COMPLETED

	// Place block or right-click object
	case INTERACT_PLACE: {
		std::optional<ItemStack> selected_item;
		getWieldedItem(playersao, selected_item);

		const bool had_prediction = !selected_item->getDefinition(m_itemdef).
			node_placement_prediction.empty();

		if (pointed.type == POINTEDTHING_OBJECT) {
			// Right click object

			// Skip if object can't be interacted with anymore
			if (pointed_object->isGone())
				return;

			actionstream << player->getName() << " right-clicks object "
					<< pointed.object_id << ": "
					<< pointed_object->getDescription() << std::endl;

			// Do stuff
			if (m_script->item_OnSecondaryUse(selected_item, playersao, pointed)) {
				if (selected_item.has_value() && playersao->setWieldedItem(*selected_item))
					SendInventory(player, true);
			}

			// on_secondary_use might have removed the object
			if (pointed_object->isGone())
				return;

			pointed_object->rightClick(playersao);
		} else if (m_script->item_OnPlace(selected_item, playersao, pointed)) {
			// Placement was handled in lua

			// Apply returned ItemStack
			if (selected_item.has_value() && playersao->setWieldedItem(*selected_item))
				SendInventory(player, true);
		}

		if (pointed.type != POINTEDTHING_NODE)
			return;

		getClient(peer_id)->m_time_from_building = 0;

		// If item has node placement prediction, always send the
		// blocks to make sure the client knows what exactly happened
		RemoteClient *client = getClient(peer_id);
		v3s16 blockpos = getNodeBlockPos(pointed.node_abovesurface);
		v3s16 blockpos2 = getNodeBlockPos(pointed.node_undersurface);
		if (had_prediction) {
			client->SetBlockNotSent(blockpos);
			if (blockpos2 != blockpos)
				client->SetBlockNotSent(blockpos2);
		} else {
			client->ResendBlockIfOnWire(blockpos);
			if (blockpos2 != blockpos)
				client->ResendBlockIfOnWire(blockpos2);
		}

		return;
	} // action == INTERACT_PLACE

	case INTERACT_USE: {
		std::optional<ItemStack> selected_item;
		getWieldedItem(playersao, selected_item);

		actionstream << player->getName() << " uses " << selected_item->name
				<< ", pointing at " << pointed.dump() << std::endl;

		if (m_script->item_OnUse(selected_item, playersao, pointed)) {
			// Apply returned ItemStack
			if (selected_item.has_value() && playersao->setWieldedItem(*selected_item))
				SendInventory(player, true);
		}

		return;
	}

	// Rightclick air
	case INTERACT_ACTIVATE: {
		std::optional<ItemStack> selected_item;
		getWieldedItem(playersao, selected_item);

		actionstream << player->getName() << " activates "
				<< selected_item->name << std::endl;

		pointed.type = POINTEDTHING_NOTHING; // can only ever be NOTHING

		if (m_script->item_OnSecondaryUse(selected_item, playersao, pointed)) {
			// Apply returned ItemStack
			if (selected_item.has_value() && playersao->setWieldedItem(*selected_item))
				SendInventory(player, true);
		}

		return;
	}

	default:
		warningstream << "Server: Invalid action " << action << std::endl;

	}
}

void Server::handleCommand_InventoryAction(NetworkPacket* pkt)
{
	session_t peer_id = pkt->getPeerId();
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player) {
		warningstream << FUNCTION_NAME << ": player is null" << std::endl;
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if (!playersao) {
		warningstream << FUNCTION_NAME << ": player SAO is null" << std::endl;
		return;
	}

	// Strip command and create a stream
	std::string datastring(pkt->getString(0), pkt->getSize());
	std::istringstream is(datastring, std::ios_base::binary);
	// Create an action
	std::unique_ptr<InventoryAction> a(InventoryAction::deSerialize(is));
	if (!a) {
		infostream << "TOSERVER_INVENTORY_ACTION: "
				<< "InventoryAction::deSerialize() returned NULL"
				<< std::endl;
		return;
	}

	// If something goes wrong, this player is to blame
	RollbackScopeActor rollback_scope(m_rollback,
			"player:" + player->getName());

	/*
		Note: Always set inventory not sent, to repair cases
		where the client made a bad prediction.
	*/

	auto mark_player_inv_list_dirty = [this](const InventoryLocation &loc,
			const std::string &list_name) {

		// Undo the client prediction of the affected list. See `clientApply`.
		if (loc.type != InventoryLocation::PLAYER)
			return;

		Inventory *inv = m_inventory_mgr->getInventory(loc);
		if (!inv)
			return;

		InventoryList *list = inv->getList(list_name);
		if (!list)
			return;

		list->setModified(true);
	};

	const bool player_has_interact = checkPriv(player->getName(), "interact");

	auto check_inv_access = [player, player_has_interact, this] (
			const InventoryLocation &loc) -> bool {

		// Players without interact may modify their own inventory
		if (!player_has_interact && loc.type != InventoryLocation::PLAYER) {
			infostream << "Cannot modify foreign inventory: "
					<< "No interact privilege" << std::endl;
			return false;
		}

		switch (loc.type) {
		case InventoryLocation::CURRENT_PLAYER:
			// Only used internally on the client, never sent
			return false;
		case InventoryLocation::PLAYER:
			// Allow access to own inventory in all cases
			return loc.name == player->getName();
		case InventoryLocation::NODEMETA:
			{
				// Check for out-of-range interaction
				v3f node_pos   = intToFloat(loc.p, BS);
				v3f player_pos = player->getPlayerSAO()->getEyePosition();
				f32 d = player_pos.getDistanceFrom(node_pos);
				return checkInteractDistance(player, d, "inventory");
			}
		case InventoryLocation::DETACHED:
			return getInventoryMgr()->checkDetachedInventoryAccess(loc, player->getName());
		default:
			return false;
		}
	};

	/*
		Handle restrictions and special cases of the move action
	*/
	if (a->getType() == IAction::Move) {
		IMoveAction *ma = (IMoveAction*)a.get();

		ma->from_inv.applyCurrentPlayer(player->getName());
		ma->to_inv.applyCurrentPlayer(player->getName());

		m_inventory_mgr->setInventoryModified(ma->from_inv);
		mark_player_inv_list_dirty(ma->from_inv, ma->from_list);
		bool inv_different = ma->from_inv != ma->to_inv;
		if (inv_different)
			m_inventory_mgr->setInventoryModified(ma->to_inv);
		if (inv_different || ma->from_list != ma->to_list)
			mark_player_inv_list_dirty(ma->to_inv, ma->to_list);

		if (!check_inv_access(ma->from_inv) ||
				!check_inv_access(ma->to_inv))
			return;

		/*
			Disable moving items out of craftpreview
		*/
		if (ma->from_list == "craftpreview") {
			infostream << "Ignoring IMoveAction from "
					<< (ma->from_inv.dump()) << ":" << ma->from_list
					<< " to " << (ma->to_inv.dump()) << ":" << ma->to_list
					<< " because src is " << ma->from_list << std::endl;
			return;
		}

		/*
			Disable moving items into craftresult and craftpreview
		*/
		if (ma->to_list == "craftpreview" || ma->to_list == "craftresult") {
			infostream << "Ignoring IMoveAction from "
					<< (ma->from_inv.dump()) << ":" << ma->from_list
					<< " to " << (ma->to_inv.dump()) << ":" << ma->to_list
					<< " because dst is " << ma->to_list << std::endl;
			return;
		}
	}
	/*
		Handle restrictions and special cases of the drop action
	*/
	else if (a->getType() == IAction::Drop) {
		IDropAction *da = (IDropAction*)a.get();

		da->from_inv.applyCurrentPlayer(player->getName());

		m_inventory_mgr->setInventoryModified(da->from_inv);
		mark_player_inv_list_dirty(da->from_inv, da->from_list);

		/*
			Disable dropping items out of craftpreview
		*/
		if (da->from_list == "craftpreview") {
			infostream << "Ignoring IDropAction from "
					<< (da->from_inv.dump()) << ":" << da->from_list
					<< " because src is " << da->from_list << std::endl;
			return;
		}

		// Disallow dropping items if not allowed to interact
		if (!player_has_interact || !check_inv_access(da->from_inv))
			return;

		// Disallow dropping items if dead
		if (playersao->isDead()) {
			infostream << "Ignoring IDropAction from "
					<< (da->from_inv.dump()) << ":" << da->from_list
					<< " because player is dead." << std::endl;
			return;
		}
	}
	/*
		Handle restrictions and special cases of the craft action
	*/
	else if (a->getType() == IAction::Craft) {
		ICraftAction *ca = (ICraftAction*)a.get();

		ca->craft_inv.applyCurrentPlayer(player->getName());

		m_inventory_mgr->setInventoryModified(ca->craft_inv);
		// Note: `ICraftAction::clientApply` is empty, thus nothing to revert.

		// Disallow crafting if not allowed to interact
		if (!player_has_interact) {
			infostream << "Cannot craft: "
					<< "No interact privilege" << std::endl;
			return;
		}

		if (!check_inv_access(ca->craft_inv))
			return;
	} else {
		// Unknown action. Ignored.
		return;
	}

	// Do the action
	a->apply(m_inventory_mgr.get(), playersao, this);
}

void Server::handleCommand_ChatMessage(NetworkPacket* pkt)
{
	std::wstring message;
	*pkt >> message;

	session_t peer_id = pkt->getPeerId();
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player) {
		warningstream << FUNCTION_NAME << ": player is null" << std::endl;
		return;
	}

	const auto &name = player->getName();

	std::wstring answer_to_sender = handleChat(name, message, true, player);
	if (!answer_to_sender.empty()) {
		// Send the answer to sender
		SendChatMessage(peer_id, ChatMessage(CHATMESSAGE_TYPE_SYSTEM,
			answer_to_sender));
	}
}

void Server::handleCommand_Damage(NetworkPacket* pkt)
{
	u16 damage;

	*pkt >> damage;

	session_t peer_id = pkt->getPeerId();
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player) {
		warningstream << FUNCTION_NAME << ": player is null" << std::endl;
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if (!playersao) {
		warningstream << FUNCTION_NAME << ": player SAO is null" << std::endl;
		return;
	}

	if (!playersao->isImmortal()) {
		if (playersao->isDead()) {
			verbosestream << "Server: "
				"Ignoring damage as player " << player->getName()
				<< " is already dead" << std::endl;
			return;
		}

		actionstream << player->getName() << " damaged by "
				<< (int)damage << " hp at " << (playersao->getBasePosition() / BS)
				<< std::endl;

		PlayerHPChangeReason reason(PlayerHPChangeReason::FALL);
		playersao->setHP((s32)playersao->getHP() - (s32)damage, reason, true);
	}
}

void Server::handleCommand_PlayerItem(NetworkPacket* pkt)
{
	if (pkt->getSize() < 2)
		return;

	session_t peer_id = pkt->getPeerId();
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player) {
		warningstream << FUNCTION_NAME << ": player is null" << std::endl;
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if (!playersao) {
		warningstream << FUNCTION_NAME << ": player SAO is null" << std::endl;
		return;
	}

	u16 item;

	*pkt >> item;

	if (player->getMaxHotbarItemcount() == 0) {
		return; // ignore silently
	} else if (item >= player->getMaxHotbarItemcount()) {
		actionstream << "Player " << player->getName()
			<< " tried to access item=" << item
			<< " out of hotbar_itemcount="
			<< player->getMaxHotbarItemcount()
			<< "; ignoring." << std::endl;
		return;
	}

	playersao->getPlayer()->setWieldIndex(item);
}

void Server::handleCommand_NodeMetaFields(NetworkPacket* pkt)
{
	session_t peer_id = pkt->getPeerId();
	RemotePlayer *player = m_env->getPlayer(peer_id);
	if (!player) {
		warningstream << FUNCTION_NAME << ": player is null" << std::endl;
		return;
	}

	PlayerSAO *playersao = player->getPlayerSAO();
	if (!playersao) {
		warningstream << FUNCTION_NAME << ": player SAO is null" << std::endl;
		return;
	}

	v3s16 p;
	std::string formname;
	StringMap fields;

	*pkt >> p >> formname;

	if (!pkt_read_formspec_fields(pkt, fields)) {
		warningstream << "Too large formspec fields! Ignoring for pos="
			<< p << ", player=" << player->getName() << std::endl;
		return;
	}

	// If something goes wrong, this player is to blame
	RollbackScopeActor rollback_scope(m_rollback,
			"player:" + player->getName());

	// Check the target node for rollback data; leave others unnoticed
	RollbackNode rn_old(&m_env->getMap(), p, this);

	m_script->node_on_receive_fields(p, formname, fields, playersao);

	// Report rollback data
	RollbackNode rn_new(&m_env->getMap(), p, this);
	if (rollback() && rn_new != rn_old) {
		RollbackAction action;
		action.setSetNode(p, rn_old, rn_new);
		rollback()->reportAction(action);
	}
}

void Server::handleCommand_InventoryFields(NetworkPacket* pkt)
{
	session_t peer_id = pkt->getPeerId();
	RemotePlayer *player = m_env->getPlayer(peer_id);

	if (!player)
		return;
	PlayerSAO *playersao = player->getPlayerSAO();
	if (!playersao)
		return;

	std::string client_formspec_name;
	StringMap fields;

	*pkt >> client_formspec_name;

	if (!pkt_read_formspec_fields(pkt, fields)) {
		warningstream << "Too large formspec fields! Ignoring for formname=\""
			<< client_formspec_name << "\", player=" << player->getName() << std::endl;
		return;
	}

	if (client_formspec_name.empty()) { // pass through inventory submits
		m_script->on_playerReceiveFields(playersao, client_formspec_name, fields);
		return;
	}

	// verify that we displayed the formspec to the user
	const auto it = m_formspec_state_data.find(peer_id);
	if (it != m_formspec_state_data.end()) {
		const auto &server_formspec_name = it->second;
		if (client_formspec_name == server_formspec_name) {
			// delete state if formspec was closed
			auto it2 = fields.find("quit");
			if (it2 != fields.end() && it2->second == "true")
				m_formspec_state_data.erase(it);

			m_script->on_playerReceiveFields(playersao, client_formspec_name, fields);
			return;
		}
		actionstream << player->getName()
			<< " submitted formspec ('" << client_formspec_name
			<< "') but the name of the formspec doesn't match the"
			" expected name ('" << server_formspec_name << "')";

	} else {
		actionstream << player->getName()
			<< " submitted formspec ('" << client_formspec_name
			<< "') but server hasn't sent formspec to client";
	}
	actionstream << ", possible exploitation attempt" << std::endl;
}
