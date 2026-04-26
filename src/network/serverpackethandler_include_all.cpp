// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

/*
 * Convenience include-all file for the split server packet handlers.
 *
 * Instead of including the original monolithic serverpackethandler.cpp,
 * include this file to pull in all four focused handler files:
 *
 *   server_auth_handler.cpp       — Authentication (Init, Init2, ClientReady, FirstSrp, SrpBytesA, SrpBytesM)
 *   server_encryption_handler.cpp — Encryption (EcdhPubkey)
 *   server_game_handler.cpp       — Game interaction (Interact, InventoryAction, ChatMessage, Damage, PlayerItem, NodeMetaFields, InventoryFields)
 *   server_misc_handler.cpp       — Miscellaneous (Deprecated, RequestMedia, GotBlocks, PlayerPos, DeletedBlocks, RemovedSounds, ModChannel*, HaveMedia, UpdateClientInfo)
 */

#include "server/server_auth_handler.cpp"
#include "server/server_encryption_handler.cpp"
#include "server/server_game_handler.cpp"
#include "server/server_misc_handler.cpp"
