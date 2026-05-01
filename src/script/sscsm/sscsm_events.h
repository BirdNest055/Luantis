// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "sscsm_ievent.h"
#include "debug.h"
#include "irrlichttypes.h"
#include "sscsm_environment.h"
#include "util/serialize.h"
#include <sstream>
#include <vector>

struct SSCSMEventTearDown : public ISSCSMEvent
{
        void exec(SSCSMEnvironment *env) override
        {
                FATAL_ERROR("SSCSMEventTearDown needs to be handled by SSCSMEnvironment::run()");
        }
};

struct SSCSMEventUpdateVFSFiles : public ISSCSMEvent
{
        // pairs are virtual path and file content
        std::vector<std::pair<std::string, std::string>> files;

        void exec(SSCSMEnvironment *env) override
        {
                env->updateVFSFiles(std::move(files));
        }
};

struct SSCSMEventLoadMods : public ISSCSMEvent
{
        // modnames and paths to init.lua file, in load order
        std::vector<std::pair<std::string, std::string>> mods;

        void exec(SSCSMEnvironment *env) override
        {
                env->getScript()->load_mods(mods);
        }
};

struct SSCSMEventOnStep : public ISSCSMEvent
{
        f32 dtime;

        void exec(SSCSMEnvironment *env) override
        {
                env->getScript()->environment_step(dtime);
        }

        // Serialize into a byte buffer using NetworkPacket-style write helpers.
        // Format: [type_tag:u16 = 1] [dtime:f32]
        std::vector<u8> serialize() const
        {
                std::ostringstream os(std::ios::binary);
                writeU16(os, 1); // type tag for OnStep
                writeF32(os, dtime);
                std::string data = os.str();
                return std::vector<u8>(data.begin(), data.end());
        }

        // Deserialize from a byte buffer (after type tag has already been consumed).
        static SSCSMEventOnStep deSerialize(std::istream &is)
        {
                SSCSMEventOnStep evt;
                evt.dtime = readF32(is);
                return evt;
        }
};

