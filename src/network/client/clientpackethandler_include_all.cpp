// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2015 nerzhul, Loic Blot <loic.blot@unix-experience.fr>

// Transitional include-all file for clientpackethandler split.
// This file includes all 6 split handler files so that existing
// CMakeLists.txt referencing clientpackethandler.cpp can continue
// to work by replacing the reference with this file.
//
// Each included file is a self-contained translation unit with its
// own headers. When CMake is updated to list the 6 files individually,
// this file can be removed.

#include "client/client_auth_handler.cpp"
#include "client/client_encryption_handler.cpp"
#include "client/client_media_handler.cpp"
#include "client/client_game_handler.cpp"
#include "client/client_hud_handler.cpp"
#include "client/client_misc_handler.cpp"
