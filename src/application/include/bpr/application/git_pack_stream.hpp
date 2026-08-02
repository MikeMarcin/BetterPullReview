#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GIT_PACK_STREAM_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GIT_PACK_STREAM_HPP

#include "bpr/core/epoch_model.hpp"

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>

namespace bpr::application {
	inline constexpr std::size_t git_artifact_identity_size = 32U;

	enum class git_artifact_role : std::uint8_t {
		completed_spool = 1U,
		installed_pack  = 2U,
		installed_index = 3U,
	};

	/// \brief Describes the git artifact identity record.
	struct git_artifact_identity
	{
		std::array<std::uint8_t, git_artifact_identity_size> Bytes{ };

		auto operator<=>(const git_artifact_identity&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const git_artifact_identity& aIdentity) noexcept -> bool
	{
		if (
			aIdentity.Bytes[0] != 1U
			|| aIdentity.Bytes[1] != 1U
			|| aIdentity.Bytes[2] == 0U) {
			return false;
		}
		for (std::size_t index = 3U; index < 8U; ++index) {
			if (aIdentity.Bytes[index] != 0U) {
				return false;
			}
		}
		bool exactIdentityNonzero = false;
		for (std::size_t index = 8U; index < aIdentity.Bytes.size(); ++index) {
			exactIdentityNonzero = exactIdentityNonzero || aIdentity.Bytes[index] != 0U;
		}
		return exactIdentityNonzero;
	}

	/// \brief Describes the git artifact fact record.
	struct git_artifact_fact
	{
		git_artifact_role     Role = git_artifact_role::completed_spool;
		git_artifact_identity Identity;
		std::uint64_t         Bytes = 0U;

		auto operator<=>(const git_artifact_fact&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const git_artifact_fact& aRtifact,
		const git_artifact_role  aExpectedRole) noexcept -> bool
	{
		return aRtifact.Role == aExpectedRole
			&& valid(aRtifact.Identity) && aRtifact.Bytes != 0U;
	}

	/// \brief Describes the git pack oid record.
	struct git_pack_oid
	{
		core::git_hash_algorithm      Algorithm = core::git_hash_algorithm::invalid;
		std::array<std::uint8_t, 32U> Digest{ };

		auto operator<=>(const git_pack_oid&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(const git_pack_oid& aPack) noexcept -> bool
	{
		const auto digestSize = aPack.Algorithm == core::git_hash_algorithm::sha1
			? 20U
			: aPack.Algorithm == core::git_hash_algorithm::sha256 ? 32U
																  : 0U;
		if (digestSize == 0U) {
			return false;
		}
		bool nonzero = false;
		for (std::size_t index = 0U; index < digestSize; ++index) {
			nonzero = nonzero || aPack.Digest[index] != 0U;
		}
		for (std::size_t index = digestSize; index < aPack.Digest.size(); ++index) {
			if (aPack.Digest[index] != 0U) {
				return false;
			}
		}
		return nonzero;
	}

	/// \brief Describes the completed git pack receipt record.
	struct completed_git_pack_receipt
	{
		core::epoch_operation_id Operation;
		git_pack_oid             ExpectedPack;
		git_pack_oid             ObservedPack;
		git_artifact_fact        Spool{
			.Role = git_artifact_role::completed_spool
		};
		std::array<std::uint8_t, 32U> OpaqueToken{ };

		auto operator<=>(const completed_git_pack_receipt&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const completed_git_pack_receipt& aReceipt) noexcept -> bool
	{
		bool tokenNonzero = false;
		for (const auto byte : aReceipt.OpaqueToken) {
			tokenNonzero = tokenNonzero || byte != 0U;
		}
		return aReceipt.Operation.valid() && valid(aReceipt.ExpectedPack)
			&& valid(aReceipt.ObservedPack)
			&& aReceipt.ExpectedPack.Algorithm
			== aReceipt.ObservedPack.Algorithm
			&& valid(
				   aReceipt.Spool,
				   git_artifact_role::completed_spool)
			&& tokenNonzero;
	}

	/// \brief Reports whether the agrees condition holds.
	[[nodiscard]] constexpr auto agrees(
		const completed_git_pack_receipt& aReceipt) noexcept -> bool
	{
		return valid(aReceipt)
			&& aReceipt.ExpectedPack == aReceipt.ObservedPack;
	}

	enum class git_pack_stream_failure : std::uint8_t {
		invalid_request,
		invalid_state,
		input_limit,
		storage,
		malformed_pack,
		cancelled,
	};

	template <typename Value>
	using git_pack_stream_result = std::expected<Value, git_pack_stream_failure>;

	/// \brief Provides the git pack stream sink contract.
	class git_pack_stream_sink
	{
	public:
		git_pack_stream_sink()                            = default;
		git_pack_stream_sink(const git_pack_stream_sink&) = delete;
		auto operator=(const git_pack_stream_sink&)
			-> git_pack_stream_sink&                 = delete;
		git_pack_stream_sink(git_pack_stream_sink&&) = delete;
		auto operator=(git_pack_stream_sink&&)
			-> git_pack_stream_sink&    = delete;
		virtual ~git_pack_stream_sink() = default;

		[[nodiscard]] virtual auto append(
			std::span<const std::byte> aBytes,
			std::stop_token            aStop) -> git_pack_stream_result<void> = 0;
		[[nodiscard]] virtual auto finish(std::stop_token aStop)
			-> git_pack_stream_result<completed_git_pack_receipt> = 0;
		virtual auto abort() noexcept -> void                     = 0;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GIT_PACK_STREAM_HPP
