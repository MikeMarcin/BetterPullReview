#include "bpr/core/sha1.hpp"

#include <sha1.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <new>
#include <span>
#include <string_view>

/// \file
/// \brief [[BPR-1450]]. The core SHA-1 over sha1collisiondetection, built as
/// Git builds it (cmake/BprSha1dcProvider.cmake). The first-party SHA-1 it
/// replaces is gone; there is no fallback.
namespace bpr::core {
	namespace {
		static_assert(sizeof(SHA1_CTX) <= sha1_stream::context_bytes, "sha1_stream must hold the library's whole context");
		static_assert(alignof(SHA1_CTX) <= alignof(std::uint64_t), "sha1_stream's storage must be aligned for the library's context");

		[[nodiscard]] auto context(std::span<unsigned char, sha1_stream::context_bytes> aStorage) noexcept -> SHA1_CTX*
		{
			return std::launder(reinterpret_cast<SHA1_CTX*>(aStorage.data()));
		}
	}

	auto name(const sha1_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case sha1_refusal::collision_detected:
			return "collision_detected";
		}
		return "unknown";
	}

	sha1_stream::sha1_stream() noexcept
	{
		auto* const state = ::new (static_cast<void*>(Context.data())) SHA1_CTX;
		SHA1DCInit(state);
		// Git's configuration: the plain digest, never a safe-hash substitute;
		// detection itself stays on. SHA1DC_INIT_SAFE_HASH_DEFAULT=0 says the same.
		SHA1DCSetSafeHash(state, 0);
	}

	void sha1_stream::update(const std::span<const std::uint8_t> aBytes) noexcept
	{
		if (aBytes.empty()) {
			return;
		}
		SHA1DCUpdate(context(Context), reinterpret_cast<const char*>(aBytes.data()), aBytes.size());
	}

	auto sha1_stream::finish() noexcept -> sha1_result
	{
		auto digest = sha1_digest{ };
		if (SHA1DCFinal(digest.data(), context(Context)) != 0) {
			return std::unexpected{ sha1_refusal::collision_detected };
		}
		return digest;
	}

	auto sha1(const std::span<const std::uint8_t> aBytes) noexcept -> sha1_result
	{
		auto stream = sha1_stream{ };
		stream.update(aBytes);
		return stream.finish();
	}
}
