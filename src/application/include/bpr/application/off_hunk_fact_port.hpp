#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_OFF_HUNK_FACT_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_OFF_HUNK_FACT_PORT_HPP

#include "bpr/core/off_hunk_fact.hpp"

#include <compare>
#include <cstdint>
#include <expected>
#include <optional>
#include <string>

namespace bpr::application {
	enum class review_mutation_recovery_failure : std::uint8_t;

	/// \brief [[BPR-1736]]. Whom and where a learned off-hunk fact is about:
	/// the profile, the canonical host, the acting account and the
	/// repository's database ID (GitHub.com rolled its web feature out per
	/// repository). One entry is kept per operation in each scope.
	struct off_hunk_fact_scope
	{
		std::uint64_t Profile = 0U;
		std::string   Host;
		std::uint64_t Account              = 0U;
		std::uint64_t RepositoryDatabaseId = 0U;

		auto operator<=>(const off_hunk_fact_scope&) const = default;
	};

	/// \brief [[BPR-1736]]. The exact key a fact holds under. A changed
	/// profile revision, product, server version, API version or classifier
	/// revision is another key: the kept entry is forgotten, never reused.
	struct off_hunk_fact_key
	{
		off_hunk_fact_scope           Scope;
		core::off_hunk_fact_operation Operation       = core::off_hunk_fact_operation::single_comment;
		std::uint64_t                 ProfileRevision = 0U;
		core::github_server_product   Product         = core::github_server_product::github_com;
		/// GitHub Enterprise Server's `installed_version`, as the answer that
		/// produced the evidence named it; empty on GitHub.com.
		std::string   ServerVersion;
		std::uint32_t ApiVersion         = 20221128U;
		std::uint32_t ClassifierRevision = 0U;

		auto operator<=>(const off_hunk_fact_key&) const = default;
	};

	inline constexpr std::size_t maximum_off_hunk_fact_text_bytes = 4'096U;

	/// \brief [[BPR-1736]]. One learned fact and the comment that taught it,
	/// for Support details: the repository as `owner/name`, the pull request,
	/// the head path and the zero-based line.
	struct off_hunk_fact
	{
		core::off_hunk_fact_observation Observation;
		std::string                     Repository;
		std::uint64_t                   PullRequest = 0U;
		std::string                     Path;
		std::uint32_t                   Line = 0U;

		auto operator<=>(const off_hunk_fact&) const = default;
	};

	/// \brief [[BPR-1736]]. A kept entry with the key it was recorded under.
	struct off_hunk_fact_entry
	{
		off_hunk_fact_key Key;
		off_hunk_fact     Fact;

		auto operator<=>(const off_hunk_fact_entry&) const = default;
	};

	[[nodiscard]] auto valid(const off_hunk_fact_scope& aScope) noexcept -> bool;
	[[nodiscard]] auto valid(const off_hunk_fact_key& aKey) noexcept -> bool;
	[[nodiscard]] auto valid(const off_hunk_fact& aFact) noexcept -> bool;

	/// \brief [[BPR-1736]]. The durable learned off-hunk facts of one profile.
	///
	/// \details Kept so one refused attempt is paid once per key rather than
	/// once per session. The store keeps facts; it decides nothing: the caller
	/// asks the core whether a kept fact is still current. At most
	/// `maximum_entries` are kept, the oldest observation forgotten first.
	class off_hunk_fact_port
	{
	public:
		static constexpr std::size_t maximum_entries = 1'024U;

		off_hunk_fact_port()                                             = default;
		off_hunk_fact_port(const off_hunk_fact_port&)                    = delete;
		auto operator=(const off_hunk_fact_port&) -> off_hunk_fact_port& = delete;
		off_hunk_fact_port(off_hunk_fact_port&&)                         = delete;
		auto operator=(off_hunk_fact_port&&) -> off_hunk_fact_port&      = delete;
		virtual ~off_hunk_fact_port()                                    = default;

		/// \brief The fact kept under exactly `aKey`; none when the scope's
		/// entry for the operation was kept under any other part of the key.
		[[nodiscard]] virtual auto load(const off_hunk_fact_key& aKey) -> std::expected<std::optional<off_hunk_fact>, review_mutation_recovery_failure> = 0;
		/// \brief The scope's entry for the operation under whatever server it
		/// was kept: what a presentation that has not read the server's version
		/// shows. Publication decides by the exact key.
		[[nodiscard]] virtual auto load_scope(const off_hunk_fact_scope& aScope, core::off_hunk_fact_operation aOperation)
			-> std::expected<std::optional<off_hunk_fact_entry>, review_mutation_recovery_failure> = 0;
		/// \brief Replaces the scope's entry for the key's operation.
		[[nodiscard]] virtual auto record(const off_hunk_fact_key& aKey, const off_hunk_fact& aFact) -> std::expected<void, review_mutation_recovery_failure> = 0;
		/// \brief Forgets every entry of the key's profile, host and account
		/// kept under another profile revision, product, server version, API
		/// version or classifier revision: the server named by `aKey` is the
		/// one now answering.
		[[nodiscard]] virtual auto forget_other_servers(const off_hunk_fact_key& aKey) -> std::expected<void, review_mutation_recovery_failure> = 0;
	};

	/// \brief [[BPR-1736]]. The scope's current fact for a single comment's
	/// Publish, as a presentation sees it at `aNowUtcMs`: unknown when nothing
	/// is kept, the store fails or the kept fact expired.
	[[nodiscard]] auto presented_off_hunk_fact(off_hunk_fact_port* aFacts, const off_hunk_fact_scope& aScope, std::int64_t aNowUtcMs) -> core::off_hunk_creation_capability;

	/// \brief [[BPR-1736]]. The scope's current fact for a review (decision
	/// D3): the newer of the single comment's and the review's, each current.
	[[nodiscard]] auto presented_review_off_hunk_fact(off_hunk_fact_port* aFacts, const off_hunk_fact_scope& aScope, std::int64_t aNowUtcMs)
		-> core::off_hunk_creation_capability;

	/// \brief [[BPR-1736]]. A review sent with a comment outside the diff, under
	/// a supported fact, came back refused for an unplaceable line: GitHub
	/// withdrew support. Records unsupported for both operations under the
	/// supported entry's key, at `aNowUtcMs`, taught by `aComment` (its
	/// observation is replaced); false when the scope's review fact is not
	/// supported or the store fails.
	[[nodiscard]] auto demote_off_hunk_fact(off_hunk_fact_port* aFacts, const off_hunk_fact_scope& aScope, off_hunk_fact aComment, std::int64_t aNowUtcMs) -> bool;
}
#endif
