// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

namespace con
{

/******************************************************************************/
/* defines used for debugging and profiling                                   */
/******************************************************************************/
#ifdef NDEBUG
	#define PROFILE(a)
#else
	#define PROFILE(a) a
#endif

// TODO: Clean this up.
#define LOG(a) a

#define PING_INTERVAL 5.0f

// exponent base
#define RESEND_SCALE_BASE 1.5f

// since spacing is exponential the numbers here shouldn't be too high
// (it's okay to start out quick)
#define RESEND_TIMEOUT_MIN 0.1f
#define RESEND_TIMEOUT_MAX 2.0f
#define RESEND_TIMEOUT_FACTOR 2

} // namespace con
