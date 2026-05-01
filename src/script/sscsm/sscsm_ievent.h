// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <memory>
#include <type_traits>

class SSCSMEnvironment;

// Event triggered from the main env for the SSCSM env.
struct ISSCSMEvent
{
	virtual ~ISSCSMEvent() = default;

	// Note: No return value (difference to ISSCSMRequest). These are not callbacks
	// that you can run at arbitrary locations, because the untrusted code could
	// then clobber your local variables.
	virtual void exec(SSCSMEnvironment *cntrl) = 0;
};

// NOTE: Currently uses unique_ptr<ISSCSMEvent> as a placeholder for real serialization.
// Once the IPC serialization layer is implemented, this will become std::string (or
// std::vector<u8>) containing the serialized byte stream. The unique_ptr approach works
// only because both the main process and the SSCSM process share the same address space.
// Once they run in separate processes, we need true serialization with type tags and
// byte-level marshalling. See sscsm_irequest.h for the full serialization design.
using SerializedSSCSMEvent = std::unique_ptr<ISSCSMEvent>;

template <typename T>
inline SerializedSSCSMEvent serializeSSCSMEvent(const T &event)
{
	static_assert(std::is_base_of_v<ISSCSMEvent, T>);

	// NOTE: Currently just copies the event object into a unique_ptr.
	// The real implementation will serialize T into a byte stream using
	// sscsm::Serializer<T>{} with a type tag prefix for deserialization.
	return std::make_unique<T>(event);
}

inline std::unique_ptr<ISSCSMEvent> deserializeSSCSMEvent(SerializedSSCSMEvent event_serialized)
{
	// NOTE: Currently a no-op passthrough. The real implementation will:
	// 1. Read a type tag from the byte stream to determine the concrete event type.
	// 2. Dispatch to the appropriate deserializer (e.g. sscsm::Serializer<SSCSMEventStep>::deserialize()).
	// 3. Return the reconstructed unique_ptr<ISSCSMEvent>.
	return event_serialized;
}
