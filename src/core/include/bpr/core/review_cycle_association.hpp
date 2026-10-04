#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_ASSOCIATION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_CYCLE_ASSOCIATION_HPP

#include "bpr/core/git_objects.hpp"
#include "bpr/core/github_model.hpp"
#include "bpr/core/review_cycle.hpp"
#include "bpr/core/review_cycle_publication.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief A local cycle's association with the pull request it became
/// (RFC-BPR-0059 and RFC-BPR-0061, BPR-1177).
/// \details Linking is an association, not a terminal state: the cycle keeps
/// every revision, note, round, response and verification it holds, and
/// GitHub is the authority for the pull request's shared facts from then on.
/// An association records only what the provider was observed to say. A
/// pull request this cycle published natively is associated through its
/// journal's link; one the reviewer made with their own Git tools is
/// associated only after a read of exactly that pull request under the
/// reviewer's account, and it is *externally published*, never evidence that
/// native publication ran. Nothing here uploads local history or names a
/// GitHub review event.
namespace bpr::core {
	inline constexpr std::size_t maximum_cycle_associations    = 256U;
	inline constexpr std::size_t maximum_pull_address_bytes    = 2048U;
	inline constexpr std::size_t maximum_association_ref_bytes = 255U;

	enum class cycle_pull_origin : std::uint8_t {
		/// This cycle's native publication created it.
		published,
		/// Made with the reviewer's own tools and linked afterwards.
		external,
	};
	[[nodiscard]] auto name(cycle_pull_origin aOrigin) noexcept -> std::string_view;

	/// \brief A pull request as a reviewer names it: host, owner, repository, number.
	struct cycle_pull_address
	{
		std::string   Host;
		std::string   Owner;
		std::string   Repository;
		std::uint64_t Number                                       = 0;
		auto          operator<=>(const cycle_pull_address&) const = default;
	};

	/// \brief Reads `https://HOST/OWNER/REPOSITORY/pull/NUMBER`, with an
	/// optional trailing path, query or fragment; nothing else is guessed.
	[[nodiscard]] auto parse_pull_address(std::string_view aText) -> std::optional<cycle_pull_address>;
	/// \brief The address's canonical web form.
	[[nodiscard]] auto pull_address_url(const cycle_pull_address& aAddress) -> std::string;

	/// \brief One association, exactly as observed.
	struct cycle_pull_association
	{
		cycle_pull_origin Origin = cycle_pull_origin::external;
		/// The native publication it came from.
		std::optional<cycle_publication_id> Publication;
		cycle_pull_address                  Address;
		std::string                         NodeId;
		std::string                         Url;
		std::string                         State;
		bool                                Draft = false;
		std::string                         HeadRef;
		git_commit_oid                      Head;
		std::string                         BaseRef;
		/// The base branch's head the server reported; absent when unknown.
		std::optional<git_commit_oid> BaseHead;
		/// The account whose read observed it, and its login.
		github_account_id Account;
		std::string       Login;
		/// The cycle's latest capture when it was associated.
		std::optional<cycle_source_revision_id> Revision;
		/// Native only: the observed head or base is not what the plan named.
		bool Drifted                                          = false;
		auto operator<=>(const cycle_pull_association&) const = default;
	};

	/// \brief What a provider read of one pull request returned, before it is admitted.
	struct cycle_pull_observation
	{
		cycle_pull_address            Address;
		std::string                   NodeId;
		std::string                   Url;
		std::string                   State;
		bool                          Draft = false;
		std::string                   HeadRef;
		git_commit_oid                Head;
		std::string                   BaseRef;
		std::optional<git_commit_oid> BaseHead;
		github_account_id             Account;
		std::string                   Login;
		auto                          operator<=>(const cycle_pull_observation&) const = default;
	};

	enum class cycle_link_refusal : std::uint8_t {
		/// The text is not a pull request address.
		address,
		/// The account's host is not the address's host.
		host,
		/// The read returned another pull request, or a malformed one.
		mismatch,
		/// The pull request does not exist, or this account cannot read it.
		not_found,
		/// This cycle is already associated with that pull request.
		already_linked,
		/// More associations than a cycle keeps.
		bounds,
	};
	[[nodiscard]] auto name(cycle_link_refusal aRefusal) noexcept -> std::string_view;

	/// \brief Admits a pull request the reviewer published with their own
	/// tools: only when the provider read returned exactly the pull request
	/// asked for, with a node identity, its own address and a head commit.
	/// Every field comes from the observation; nothing is filled in.
	[[nodiscard]] auto decide_external_link(const cycle_pull_address& aAsked, const cycle_pull_observation& aObserved, std::span<const cycle_pull_association> aExisting,
											std::optional<cycle_source_revision_id> aRevision) -> std::expected<cycle_pull_association, cycle_link_refusal>;

	/// \brief The association a linked native publication carries: its
	/// journal's last observation of the pull request, under the plan's account.
	[[nodiscard]] auto published_association(cycle_publication_id aPublication, const publication_plan& aPlan, const publication_journal& aJournal)
		-> std::optional<cycle_pull_association>;

	/// \brief Two associations name the same pull request.
	[[nodiscard]] auto same_pull_request(const cycle_pull_address& aLeft, const cycle_pull_address& aRight) noexcept -> bool;

	[[nodiscard]] auto encode_association(const cycle_pull_association& aAssociation) -> std::vector<std::uint8_t>;
	[[nodiscard]] auto decode_association(std::span<const std::uint8_t> aBytes) -> std::optional<cycle_pull_association>;

	/// \brief The pull request's description from the reviewer's Problem and
	/// Solution, in the form a Review Tour maps (`insert_tour_brief`), so a
	/// tour prepared on the pull request later finds them. Absent when either
	/// is over its bound or the result is over the publication body bound.
	[[nodiscard]] auto publication_body(std::string_view aProblem, std::string_view aSolution) -> std::optional<std::string>;

	/// \brief A new branch name for publishing revision `aFinal` of cycle
	/// `aCycle`; the reviewer may change it.
	[[nodiscard]] auto suggested_publication_branch(review_cycle_id aCycle, cycle_source_revision_id aFinal) -> std::string;
}

#endif
