// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

// Transitional file that includes all split implementation files.
// This allows the build system to continue compiling a single translation unit
// while the code has been logically split into separate files.

#include "buffered_packet.cpp"
#include "reliable_buffer.cpp"
#include "split_buffer.cpp"
#include "channel.cpp"
#include "peer.cpp"
#include "connection_core.cpp"
