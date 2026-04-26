// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#pragma once

#include "irrlichttypes.h"

/**
 * Encryption log verbosity levels.
 * Higher values include all lower values.
 *
 * v9.23: Defined in a separate lightweight header so that encryption_log.h
 * can use the enum values in macros without pulling in settings.h.
 */
enum EncryptionLogLevel : int
{
        ENC_LOG_NONE = 0,    // No encryption log output
        ENC_LOG_ERROR = 1,   // Only errors
        ENC_LOG_ACTION = 2,  // Errors + activation/security/disable events
        ENC_LOG_TRACE = 3,   // Everything including per-packet trace
};
