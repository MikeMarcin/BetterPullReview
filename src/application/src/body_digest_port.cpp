#include "bpr/application/body_digest_port.hpp"

#include "bpr/core/sha256.hpp"

#include <stdexcept>

namespace bpr::application {
	void core_body_digest::digest(const std::span<const std::string_view> aBodies, const std::span<body_digest> aDigests) const
	{
		if (aBodies.size() != aDigests.size()) {
			throw std::invalid_argument{ "one digest for each body" };
		}
		for (std::size_t index = 0U; index < aBodies.size(); ++index) {
			const auto body = aBodies[index];
			aDigests[index] = core::sha256({ reinterpret_cast<const std::uint8_t*>(body.data()), body.size() });
		}
	}
}
