#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MUTATION_RECOVERY_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MUTATION_RECOVERY_PORT_HPP

#include "bpr/application/review_mutation_port.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>
#include <vector>

namespace bpr::application {
	inline constexpr std::size_t maximum_review_mutation_recovery_records    = 1'024U;
	inline constexpr std::size_t maximum_review_mutation_recovery_path_bytes = 4'096U;
	inline constexpr std::size_t maximum_review_mutation_baseline_comments   = 100U;

	enum class review_mutation_recovery_failure : std::uint8_t {
		unavailable,
		conflict,
		integrity_failure,
		capacity,
	};

	enum class review_mutation_recovery_state : std::uint8_t {
		unresolved,
		succeeded,
	};

	enum class review_mutation_recovery_transmission : std::uint8_t {
		baseline_incomplete,
		prepared,
		possibly_transmitted,
	};

	/// \brief One exact write whose transmission outcome must survive restart.
	struct review_mutation_recovery_record
	{
		std::string                    IdempotencyKey;
		std::array<std::uint8_t, 32>   RequestFingerprint{ };
		std::array<std::uint8_t, 32>   AuthorityFingerprint{ };
		std::string                    Body;
		review_mutation_anchor_binding Anchor;
		/// Null means a migrated attempt that predates durable baseline capture.
		std::optional<std::vector<std::string>> BaselineExactCommentIds;
		review_mutation_recovery_transmission   Transmission
			= review_mutation_recovery_transmission::baseline_incomplete;
		review_mutation_recovery_state State
			= review_mutation_recovery_state::unresolved;
		std::optional<review_mutation_receipt> Receipt;

		auto operator<=>(const review_mutation_recovery_record&) const = default;
	};

	[[nodiscard]] auto valid(
		const review_mutation_recovery_record& aRecord) noexcept -> bool;

	struct review_mutation_recovery_admission
	{
		review_mutation_recovery_record Record;
		bool                            Inserted = false;
	};

	/// \details [[BPR-1706]]. `unresolvable_line` is a first-attempt admission
	/// of a commit-filtered diff's line the core placed where GitHub resolves
	/// no line comment. It binds the exact file-comment wire request and the
	/// authority, and like a native rejection it carries no freshness window:
	/// it was decided from exact facts, not from a capability cache.
	enum class review_fallback_authorization_source : std::uint8_t { none,
																	 native_rejection,
																	 scoped_evidence,
																	 unresolvable_line };
	/// \brief Durable proof of non-creation or first-attempt scoped evidence admission.
	struct review_fallback_authorization
	{
		review_fallback_authorization_source Source = review_fallback_authorization_source::none;
		std::array<std::uint8_t, 32>         ContractDigest{ };
		std::array<std::uint8_t, 32>         ScopeDigest{ };
		std::uint64_t                        Generation                                              = 0U;
		std::uint64_t                        ObservedAt                                              = 0U;
		std::uint64_t                        ExpiresAt                                               = 0U;
		auto                                 operator<=>(const review_fallback_authorization&) const = default;
	};
	/// \brief One parent and at most two deterministic, representation-bound attempts.
	struct review_publication_recovery_record
	{
		std::string                                    Key;
		std::string                                    LegacyKey;
		std::array<std::uint8_t, 32>                   Fingerprint{ };
		core::review_publication_state                 State = core::review_publication_state::initial;
		review_fallback_authorization                  Authorization;
		std::optional<review_mutation_recovery_record> Native;
		std::optional<review_mutation_recovery_record> Fallback;
		auto                                           operator<=>(const review_publication_recovery_record&) const = default;
	};
	/// \brief Domain-separated SHA-256 child identity; revision one binds native wire or permalink/body/file wire.
	[[nodiscard]] auto review_publication_child_key(std::string_view aParent, core::review_publication_representation aRepresentation) -> std::string;
	[[nodiscard]] auto valid(const review_publication_recovery_record& aRecord) noexcept -> bool;
	/// \brief Checks an exact one-step parent/child transition before the store commits it.
	[[nodiscard]] auto valid_publication_transition(const std::optional<review_publication_recovery_record>& aExpected, const review_publication_recovery_record& aNext) -> bool;

	/// \brief Durable admission and resolution for one exact mutation attempt.
	class review_mutation_recovery_port
	{
	public:
		review_mutation_recovery_port()          = default;
		virtual ~review_mutation_recovery_port() = default;

		review_mutation_recovery_port(const review_mutation_recovery_port&) = delete;
		auto operator=(const review_mutation_recovery_port&)
			-> review_mutation_recovery_port&                          = delete;
		review_mutation_recovery_port(review_mutation_recovery_port&&) = delete;
		auto operator=(review_mutation_recovery_port&&)
			-> review_mutation_recovery_port& = delete;

		/// \brief Loads the exact digest-protected logical journal; legacy APIs retain old native rows.
		[[nodiscard]] virtual auto load_publication(const std::string& aKey)
			-> std::expected<std::optional<review_publication_recovery_record>, review_mutation_recovery_failure>
		{
			static_cast<void>(aKey);
			return std::unexpected(review_mutation_recovery_failure::unavailable);
		}
		/// \brief Commits parent, authorization and child witness/receipt atomically by exact compare-and-swap.
		[[nodiscard]] virtual auto advance_publication(const std::optional<review_publication_recovery_record>& aExpected, const review_publication_recovery_record& aNext)
			-> std::expected<review_publication_recovery_record, review_mutation_recovery_failure>
		{
			static_cast<void>(aExpected);
			static_cast<void>(aNext);
			return std::unexpected(review_mutation_recovery_failure::unavailable);
		}
		/// \brief [[BPR-1735]]. Keeps what GitHub answered the POST of the
		/// logical publication's child that may have been sent, replacing any
		/// earlier answer. The publication must exist. Keeping it is best
		/// effort for the caller: a child with no answer kept is reconciled
		/// GET-only, as RFC-BPR-0022 has it.
		[[nodiscard]] virtual auto note_publication_post(const std::string& aKey, const core::review_publication_post& aPost)
			-> std::expected<void, review_mutation_recovery_failure>
		{
			static_cast<void>(aKey);
			static_cast<void>(aPost);
			return std::unexpected(review_mutation_recovery_failure::unavailable);
		}
		/// \brief [[BPR-1735]]. The answer kept for the publication, none when
		/// none was kept.
		[[nodiscard]] virtual auto load_publication_post(const std::string& aKey)
			-> std::expected<std::optional<core::review_publication_post>, review_mutation_recovery_failure>
		{
			static_cast<void>(aKey);
			return std::optional<core::review_publication_post>{ };
		}

		[[nodiscard]] virtual auto load(const std::string& aIdempotencyKey)
			-> std::expected<
				std::optional<review_mutation_recovery_record>,
				review_mutation_recovery_failure> = 0;

		/// \brief Inserts unresolved durably or returns the existing exact row.
		[[nodiscard]] virtual auto admit(
			const review_mutation_recovery_record& aRecord)
			-> std::expected<
				review_mutation_recovery_admission,
				review_mutation_recovery_failure> = 0;

		/// \brief Durably witnesses that the transport created the POST operation.
		[[nodiscard]] virtual auto mark_possibly_transmitted(
			const std::string&                  aIdempotencyKey,
			const std::array<std::uint8_t, 32>& aRequestFingerprint)
			-> std::expected<
				review_mutation_recovery_record,
				review_mutation_recovery_failure> = 0;

		/// \brief Atomically records the exact server-confirmed receipt.
		[[nodiscard]] virtual auto resolve(
			const std::string&                  aIdempotencyKey,
			const std::array<std::uint8_t, 32>& aRequestFingerprint,
			const review_mutation_receipt&      aReceipt)
			-> std::expected<
				review_mutation_recovery_record,
				review_mutation_recovery_failure> = 0;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MUTATION_RECOVERY_PORT_HPP
