// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>

#pragma once

#include <ctime>
#include <string>
#include <mutex>

inline struct tm mt_localtime()
{
        // initialize the time zone on first invocation
        static std::once_flag tz_init;
        std::call_once(tz_init, [] {
#ifdef _WIN32
                _tzset();
#else
                tzset();
#endif
                });

        struct tm ret{};
        time_t t = time(NULL);
        // Both localtime_s and localtime_r return zero on success.
        // On failure, ret remains zero-initialized (epoch 1900-01-01).
#ifdef _WIN32
        if (localtime_s(&ret, &t) != 0) {
                // localtime_s failed; ret is already zero-initialized
                return ret;
        }
#else
        struct tm *result = localtime_r(&t, &ret);
        if (!result) {
                // localtime_r failed; ret is already zero-initialized
                return ret;
        }
#endif
        return ret;
}


inline std::string getTimestamp()
{
        const struct tm tm = mt_localtime();
        char cs[20]; // YYYY-MM-DD HH:MM:SS + '\0'
        strftime(cs, 20, "%Y-%m-%d %H:%M:%S", &tm);
        return cs;
}
