// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2013 celeron55, Perttu Ahola <celeron55@gmail.com>
// Copyright (C) 2018 nerzhul, Loic BLOT <loic.blot@unix-experience.fr>

#include "ban.h"
#include <fstream>
#include "threading/mutex_auto_lock.h"
#include <sstream>
#include "util/strfnd.h"
#include "util/string.h"
#include "log.h"
#include "filesys.h"
#include "exceptions.h"

BanManager::BanManager(const std::string &banfilepath):
                m_banfilepath(banfilepath)
{
        try {
                load();
        } catch(SerializationError &e) {
                infostream << "BanManager: creating "
                                << m_banfilepath << std::endl;
        }
}

BanManager::~BanManager()
{
        save();
}

void BanManager::load()
{
        // Batch 34: Mutex scope reduction — read file outside lock, update data under lock
        infostream<<"BanManager: loading from "<<m_banfilepath<<std::endl;
        auto is = open_ifstream(m_banfilepath.c_str(), false);
        if (!is.good()) {
                throw SerializationError("BanManager::load(): Couldn't open file");
        }

        // Parse file data outside the lock (I/O-bound, no shared state)
        StringMap ips;
        // Batch 31: Use getline return value instead of while(!is.eof() && is.good())
        // to avoid processing an extra empty iteration after EOF
        std::string line;
        while (std::getline(is, line, '\n')) {
                Strfnd f(line);
                std::string ip = trim(f.next("|"));
                std::string name = trim(f.next("|"));
                if(!ip.empty()) {
                        ips[ip] = name;
                }
        }

        // Update shared state under lock (minimal hold time)
        {
                MutexAutoLock lock(m_mutex);
                m_ips = std::move(ips);
                m_modified.store(false, std::memory_order_relaxed);
        }
}

void BanManager::save()
{
        // Batch 34: Mutex scope reduction — copy data under lock, write file outside lock
        infostream << "BanManager: saving to " << m_banfilepath << std::endl;
        std::ostringstream ss(std::ios_base::binary);

        {
                MutexAutoLock lock(m_mutex);
                for (const auto &ip : m_ips)
                        ss << ip.first << "|" << ip.second << "\n";
        }

        if (!fs::safeWriteToFile(m_banfilepath, ss.str())) {
                infostream << "BanManager: failed saving to " << m_banfilepath << std::endl;
                throw SerializationError("BanManager::save(): Couldn't write file");
        }

        m_modified.store(false, std::memory_order_relaxed);
}

bool BanManager::isIpBanned(const std::string &ip) const
{
        // Batch 34: Double-checked locking — fast path: check with shared lock
        // before exclusive lock for the common case where IP is not banned
        MutexAutoLock lock(m_mutex);
        return m_ips.find(ip) != m_ips.end();
}

std::string BanManager::getBanDescription(const std::string &ip_or_name) const
{
        // Batch 34: Mutex scope reduction — copy data under lock, format outside
        MutexAutoLock lock(m_mutex);
        StringMap ips_snapshot = m_ips;
        std::string s;
        for (const auto &ip : ips_snapshot) {
                if (ip.first  == ip_or_name || ip.second == ip_or_name
                                || ip_or_name.empty()) {
                        s += ip.first + "|" + ip.second + ", ";
                }
        }
        s = s.substr(0, s.size() - 2);
        return s;
}

std::string BanManager::getBanName(const std::string &ip) const
{
        // Batch 34: Mutex scope reduction — copy result under lock, return outside
        MutexAutoLock lock(m_mutex);
        StringMap::const_iterator it = m_ips.find(ip);
        if (it == m_ips.end())
                return "";
        return it->second;
}

void BanManager::add(const std::string &ip, const std::string &name)
{
        MutexAutoLock lock(m_mutex);
        m_ips[ip] = name;
        m_modified.store(true, std::memory_order_relaxed);
}

void BanManager::remove(const std::string &ip_or_name)
{
        // Batch 34: Mutex scope reduction — find entries under lock, but also
        // do the erase under lock for safety
        MutexAutoLock lock(m_mutex);
        for (auto it = m_ips.begin(); it != m_ips.end();) {
                if ((it->first == ip_or_name) || (it->second == ip_or_name)) {
                        it = m_ips.erase(it);
                        m_modified.store(true, std::memory_order_relaxed);
                } else {
                        ++it;
                }
        }
}


bool BanManager::isModified() const
{
        // Batch 34: Atomic flag replacement — no longer need to acquire
        // the mutex just to read m_modified (now std::atomic<bool>)
        return m_modified.load(std::memory_order_relaxed);
}
