// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013-2017 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2017 celeron55, Loic Blot <loic.blot@unix-experience.fr>

#pragma once

#include "util/serialize.h"

namespace con
{

/******************************************************************************/
/* defines used for debugging and profiling                                   */
/******************************************************************************/
#ifdef NDEBUG
#define PROFILE(a)
#undef DEBUG_CONNECTION_KBPS
#else
#define PROFILE(a) a
//#define DEBUG_CONNECTION_KBPS
#undef DEBUG_CONNECTION_KBPS
#endif

// TODO: Clean this up.
#define LOG(a) a

#define INIT_PHASE_MIN_TIMEOUT 5.0f

#define MAX_NEW_PEERS_PER_SEC 30

/******************************************************************************/
/* Connection Threads                                                         */
/******************************************************************************/

#define MPPI_SETTING "max_packets_per_iteration"

static inline session_t readPeerId(const u8 *packetdata)
{
        return readU16(&packetdata[4]);
}
static inline u8 readChannel(const u8 *packetdata)
{
        return readU8(&packetdata[6]);
}

} // namespace con
