// SPDX-FileCopyrightText: 2024 Luanti authors
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include "exceptions.h"
#include <memory>
#include <type_traits>

class SSCSMController;
class Client;

// NOTE: ISSCSMAnswer is a temporary polymorphic base class used solely so that
// answers can be stored in unique_ptr<ISSCSMAnswer>. Once the IPC serialization
// layer is implemented, this will be removed and SerializedSSCSMAnswer will
// become std::vector<u8> (a flat byte buffer). The receiving side already knows
// the answer type, so no type tag is needed for answers (unlike requests).
struct ISSCSMAnswer
{
	virtual ~ISSCSMAnswer() = default;
};

// NOTE: Currently uses unique_ptr<ISSCSMAnswer> as a placeholder for real
// serialization. Once the IPC serialization layer is implemented, this will
// become std::vector<u8> containing the serialized answer bytes. Function
// argument declarations should then be updated to take
// `const SerializedSSCSMAnswer&` (i.e. const std::vector<u8>&).
// Since answers are non-polymorphic (the receiver knows the type), no type
// tag is needed in the serialized format.
using SerializedSSCSMAnswer = std::unique_ptr<ISSCSMAnswer>;

// Request made by the sscsm env to the main env.
struct ISSCSMRequest
{
	virtual ~ISSCSMRequest() = default;

	virtual SerializedSSCSMAnswer exec(Client *client) = 0;
};

// NOTE: Currently uses unique_ptr<ISSCSMRequest> as a placeholder for real
// serialization. Once the IPC serialization layer is implemented, this will
// become std::vector<u8> containing the serialized request bytes. Requests
// are polymorphic (the receiver does not know the type), so a type tag must
// be prepended to the serialized format for dispatch during deserialization.
using SerializedSSCSMRequest = std::unique_ptr<ISSCSMRequest>;

template <typename T>
inline SerializedSSCSMRequest serializeSSCSMRequest(const T &request)
{
	static_assert(std::is_base_of_v<ISSCSMRequest, T>);

	// NOTE: Currently just copies the request object into a unique_ptr.
	// The real implementation will:
	// 1. Write a type tag for T (e.g. a u16 enum value from SSCSMRequestType).
	// 2. Serialize T's fields into the byte stream using sscsm::Serializer<T>{}.
	// The type tag allows the deserializer to dispatch to the correct
	// deserialization routine on the receiving side.

	return std::make_unique<T>(request);
}

template <typename T>
inline T deserializeSSCSMAnswer(SerializedSSCSMAnswer answer_serialized)
{
	static_assert(std::is_base_of_v<ISSCSMAnswer, T>);

	// NOTE: The real implementation will deserialize the byte buffer directly
	// into T without a type tag (answers are non-polymorphic):
	//   return sscsm::Serializer<T>{}.deserialize(answer_serialized);
	// Currently, a dynamic_cast substitutes for deserialization since both
	// sides share the same address space.

	// dynamic cast in place of actual deserialization
	auto ptr = dynamic_cast<T *>(answer_serialized.get());
	if (!ptr) {
		throw SerializationError("deserializeSSCSMAnswer failed");
	}
	return std::move(*ptr);
}

template <typename T>
inline SerializedSSCSMAnswer serializeSSCSMAnswer(T &&answer)
{
	static_assert(std::is_base_of_v<ISSCSMAnswer, T>);

	// NOTE: The real implementation will serialize T into a byte buffer:
	//   return sscsm::Serializer<T>{}.serialize(std::forward<T>(answer));
	// No type tag is needed since the receiver already knows the answer type.

	return std::make_unique<T>(std::move(answer));
}

inline std::unique_ptr<ISSCSMRequest> deserializeSSCSMRequest(SerializedSSCSMRequest request_serialized)
{
	// NOTE: The real implementation will:
	// 1. Read a type tag from the byte stream to determine the concrete request type.
	// 2. Dispatch to the appropriate deserializer (e.g. sscsm::Serializer<SSCSMRequestGetNode>::deserialize()).
	// 3. Return the reconstructed unique_ptr<ISSCSMRequest>.
	// Currently a passthrough since both sides share the same address space.
	return request_serialized;
}
