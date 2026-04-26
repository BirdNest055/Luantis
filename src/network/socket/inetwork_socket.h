// Luanti
// SPDX-License-Identifier: LGPL-2.1-or-later
// Copyright (C) 2026 Luanti contributors

#pragma once

#include "network/address.h"
#include <cstddef>

/// Abstract interface for network socket operations.
///
/// Decouples the transport layer from concrete UDP socket implementation,
/// enabling mock sockets for testing connection logic without real I/O.
class INetworkSocket
{
public:
    virtual ~INetworkSocket() = default;

    /// Bind the socket to an address.
    virtual void Bind(Address addr) = 0;

    /// Send data to an address.
    virtual void Send(const Address& addr, const void* data, size_t len) = 0;

    /// Receive data from any sender.
    /// @param sender  [out] The address of the sender
    /// @param data    Buffer for received data
    /// @param maxlen  Maximum buffer size
    /// @return Number of bytes received, or -1 on error
    virtual int Receive(Address& sender, void* data, size_t maxlen) = 0;

    /// Get the number of bytes sent since last call.
    virtual unsigned int SendCount() = 0;

    /// Get the number of bytes received since last call.
    virtual unsigned int ReceiveCount() = 0;

    /// Increment the drop counter (for testing).
    virtual void incrementDropCount() = 0;
};
