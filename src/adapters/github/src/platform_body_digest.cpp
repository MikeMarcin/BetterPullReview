#include "platform_body_digest.hpp"

#include <limits>
#include <stdexcept>

#if defined(__APPLE__)
	#include <CommonCrypto/CommonDigest.h>
#endif

namespace bpr::adapters::github {
	void platform_body_digest::digest(const std::span<const std::string_view> aBodies, const std::span<application::body_digest> aDigests) const
	{
		if (aBodies.size() != aDigests.size()) {
			throw std::invalid_argument{ "one digest for each body" };
		}
#if defined(__APPLE__)
		static_assert(CC_SHA256_DIGEST_LENGTH == std::tuple_size_v<application::body_digest>);
		for (std::size_t index = 0U; index < aBodies.size(); ++index) {
			const auto body = aBodies[index];
			// The one-shot call takes a 32-bit length; the read model bounds a
			// body far below it, and a longer one is refused, not truncated.
			if (body.size() > std::numeric_limits<CC_LONG>::max()) {
				throw std::length_error{ "body too long for one CommonCrypto call" };
			}
			CC_SHA256(body.data(), static_cast<CC_LONG>(body.size()), aDigests[index].data());
		}
#else
		application::core_body_digest{ }.digest(aBodies, aDigests);
#endif
	}

	auto platform_body_digest_backend() noexcept -> std::string_view
	{
#if defined(__APPLE__)
		return "CommonCrypto";
#else
		return "core";
#endif
	}
}
