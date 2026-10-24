#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_OFF_HUNK_FACT_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_OFF_HUNK_FACT_HPP

#include "bpr/core/filtered_comment.hpp"
#include "bpr/core/review_publication_policy.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

/// [[BPR-1736]] (owner, 2026-10-22, D1 to D4). Whether GitHub accepts a line
/// comment outside its own diff, learned from GitHub's answer.
///
/// No GitHub API says whether a host accepts a line comment more than
/// `github_comment_context_lines` from a change, and neither the host's name
/// nor its version is a sound positive signal: GitHub.com has let every user
/// comment on any line in its web page since January 2026 and its API still
/// refuses it (BPR-Test #125). The one reliable signal is GitHub's own answer
/// to a reviewer's single comment the core placed outside the diff. The core
/// decides here, as pure functions of explicit facts, what an answer teaches,
/// how long the lesson holds and where a comment goes under it. The host keeps
/// the fact, keyed by profile, account, host, server version, API version,
/// operation, repository and classifier revision, and passes it back in; the
/// core never reads it, so equal inputs give equal placements.
///
/// Version is a cache key, never evidence: nothing here sets the fact from a
/// host name, product, version, schema or a readable comment.
namespace bpr::core {
	/// \brief The product a GitHub host runs, as the profile names it.
	enum class github_server_product : std::uint8_t {
		github_com               = 1U,
		github_enterprise_server = 2U,
	};

	/// \brief The write a fact is learned for and applies to.
	///
	/// \details Only a single comment's Publish is ever a probe: GitHub refuses
	/// a review whole and names no comment. Decision D3 lets the single
	/// comment's evidence place a line in a review too, and a review refused
	/// for an unplaceable line demotes both.
	enum class off_hunk_fact_operation : std::uint8_t {
		single_comment = 0U,
		review         = 1U,
	};

	/// \brief Where a kept fact came from: an observed answer, never a
	/// recorded contract or a version (decision D2).
	enum class off_hunk_fact_source : std::uint8_t {
		observed_acceptance = 1U,
		observed_refusal    = 2U,
	};

	/// \brief How long a fact holds on GitHub.com, which has no version and
	/// deploys continuously: 14 days.
	inline constexpr std::int64_t off_hunk_fact_lifetime_github_com_ms = std::int64_t{ 14 } * 24 * 60 * 60 * 1000;
	/// \brief How long a fact holds on GitHub Enterprise Server at most, a
	/// changed `installed_version` ending it sooner: 30 days.
	inline constexpr std::int64_t off_hunk_fact_lifetime_enterprise_ms = std::int64_t{ 30 } * 24 * 60 * 60 * 1000;
	/// \brief The longest server version a fact is keyed by.
	inline constexpr std::size_t maximum_github_server_version_bytes = 32U;

	/// \brief Whether `aVersion` is a version a fact may be keyed by.
	///
	/// \details GitHub.com has none: only the empty version. GitHub Enterprise
	/// Server's is its `installed_version`, two to four dot-separated decimal
	/// parts of one to five digits ("3.20.8"). Anything else is unparsable, so
	/// the fact is unknown and nothing is recorded.
	[[nodiscard]] auto valid_github_server_version(github_server_product aProduct, std::string_view aVersion) noexcept -> bool;

	/// \brief One kept fact: what GitHub answered, and when, in UTC
	/// milliseconds since the Unix epoch.
	struct off_hunk_fact_observation
	{
		off_hunk_creation_capability State         = off_hunk_creation_capability::unknown;
		off_hunk_fact_source         Source        = off_hunk_fact_source::observed_refusal;
		std::int64_t                 ObservedUtcMs = 0;

		auto operator<=>(const off_hunk_fact_observation&) const = default;
	};

	/// \brief Whether the observation is in range: supported from an
	/// acceptance, unsupported from a refusal, at a time that is not negative.
	[[nodiscard]] auto valid(const off_hunk_fact_observation& aObservation) noexcept -> bool;

	/// \brief The fact a kept observation gives at `aNowUtcMs`: its state while
	/// it is valid, not observed later than now, and younger than its
	/// product's lifetime; `unknown` otherwise or with none.
	[[nodiscard]] auto current_off_hunk_fact(github_server_product aProduct, const std::optional<off_hunk_fact_observation>& aObservation,
											 std::int64_t aNowUtcMs) noexcept -> off_hunk_creation_capability;

	/// \brief The newer of two current observations, by when each was observed;
	/// on a tie the refusal. A review consults the single comment's and its
	/// own (decision D3), and a refusal after an acceptance demotes at once.
	[[nodiscard]] auto newer_off_hunk_fact(const std::optional<off_hunk_fact_observation>& aLeft,
										   const std::optional<off_hunk_fact_observation>& aRight) noexcept -> std::optional<off_hunk_fact_observation>;

	/// \brief What one GitHub answer to a native comment at a line the core
	/// placed outside the diff says about that operation on that server.
	enum class off_hunk_answer : std::uint8_t {
		/// Timeout, lost or malformed answer, 5xx, rate limit, permission, an
		/// unrelated 422: no evidence, and the fact is unchanged.
		unclear,
		/// The comment was created at that line and exactly parsed.
		created,
		/// A fully received 422 whose error names the comment's line.
		refused_naming_line,
	};

	/// \brief The observation an answer records, none for an unclear one.
	[[nodiscard]] auto learn_off_hunk_fact(off_hunk_answer aAnswer, std::int64_t aNowUtcMs) noexcept -> std::optional<off_hunk_fact_observation>;

	/// \brief Where a line comment on GitHub's own diff goes, given the
	/// learned fact.
	enum class github_comment_route : std::uint8_t {
		/// Inside a hunk, or placement unknown: a native line comment as today.
		native,
		/// Outside every hunk, where GitHub has accepted such a comment: native.
		native_off_hunk,
		/// Outside every hunk, where GitHub has refused one: a file comment with
		/// a permalink, with no native attempt.
		off_hunk_file,
		/// Outside every hunk, nothing learned: a single Publish makes one native
		/// attempt, and its answer teaches the fact.
		off_hunk_unknown,
		/// A range not wholly inside one hunk where nothing says GitHub takes
		/// it: refused, since a range has no file comment.
		refused,
	};

	/// \brief [[BPR-1736]]. The owner's rule (D1): natively placeable within
	/// `github_comment_context_lines` of a change, or anywhere once GitHub has
	/// accepted such a comment for this key.
	///
	/// \details
	/// ```text
	/// unknown placement or in_diff        -> native
	/// outside, fact supported             -> native_off_hunk
	/// outside, fact unsupported           -> off_hunk_file
	/// outside, fact unknown               -> off_hunk_unknown
	/// range outside, fact supported       -> native_off_hunk
	/// range outside, otherwise            -> refused
	/// ```
	[[nodiscard]] auto route_github_comment(github_diff_placement aPlacement, off_hunk_creation_capability aFact) noexcept -> github_comment_route;

	/// \brief [[BPR-1736]]. Whether a comment pending in a review is held back:
	/// a review never probes, so a line outside the diff is carried only when
	/// GitHub has accepted such a comment (D3).
	[[nodiscard]] auto holds_review_comment(github_diff_placement aPlacement, off_hunk_creation_capability aFact) noexcept -> bool;
}
#endif
