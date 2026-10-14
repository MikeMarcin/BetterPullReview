#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_READ_MODEL_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_READ_MODEL_HPP

#include "bpr/application/github_host_authority.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/github_model.hpp"

#include <algorithm>
#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace bpr::application {
	/// \brief Provides the github text id contract.
	template <typename Tag>
	class github_text_id
	{
	public:
		github_text_id() = default;
		explicit github_text_id(std::string aValue)
			: Value{ std::move(aValue) }
		{
		}

		[[nodiscard]] auto value() const noexcept -> std::string_view
		{
			return Value;
		}

		auto operator<=>(const github_text_id&) const = default;

	private:
		std::string Value;
	};

	struct github_node_id_tag;
	struct github_actor_id_tag;
	struct github_review_id_tag;
	struct github_thread_id_tag;
	struct github_comment_id_tag;
	struct github_check_id_tag;
	struct commit_oid_tag;

	using github_node_id    = github_text_id<github_node_id_tag>;
	using github_actor_id   = github_text_id<github_actor_id_tag>;
	using github_review_id  = github_text_id<github_review_id_tag>;
	using github_thread_id  = github_text_id<github_thread_id_tag>;
	using github_comment_id = github_text_id<github_comment_id_tag>;
	using github_check_id   = github_text_id<github_check_id_tag>;
	using commit_oid        = github_text_id<commit_oid_tag>;

	inline constexpr std::uint32_t maximum_github_line = 2'147'483'647U;

	/// \brief The account type a provider observation stated for an actor (BPR-1321).
	/// \details Taken only from the response's own type field: REST `type`,
	/// GraphQL `__typename`. `unobserved` means the observation carried no
	/// type, so no representation rule applies to it.
	enum class github_actor_kind : std::uint8_t {
		unobserved,
		user,
		bot,
		other,
	};

	/// \brief Describes the github actor record record.
	/// \details A `bot` login is held in the REST form `<name>[bot]`; the
	/// GraphQL decoder appends the suffix to a typed `Bot` login that lacks it.
	struct github_actor_record
	{
		github_actor_id   Actor;
		std::string       Login;
		github_actor_kind Kind = github_actor_kind::unobserved;

		auto operator<=>(const github_actor_record&) const = default;
	};

	enum class github_pull_request_state : std::uint8_t {
		open,
		closed,
		merged,
	};

	enum class github_file_change : std::uint8_t {
		uninitialized,
		added,
		modified,
		removed,
		renamed,
		copied,
		type_changed,
		unchanged,
	};

	enum class github_file_content_kind : std::uint8_t {
		unknown,
		text,
		binary,
	};

	enum class github_file_viewed_state : std::uint8_t {
		unavailable,
		dismissed,
		unviewed,
		viewed,
	};

	enum class github_collection_availability : std::uint8_t {
		unavailable,
		available,
	};

	enum class github_permission_state : std::uint8_t {
		unavailable,
		denied,
		allowed,
	};

	enum class github_review_state : std::uint8_t {
		uninitialized,
		pending,
		commented,
		approved,
		changes_requested,
		dismissed,
	};

	enum class github_review_thread_subject : std::uint8_t {
		line,
		file,
	};

	enum class github_diff_side : std::uint8_t {
		uninitialized,
		left,
		right,
	};

	enum class github_check_status : std::uint8_t {
		uninitialized,
		queued,
		in_progress,
		completed,
		waiting,
		requested,
		pending,
	};

	enum class github_check_conclusion : std::uint8_t {
		uninitialized,
		none,
		success,
		failure,
		neutral,
		cancelled,
		skipped,
		timed_out,
		action_required,
		stale,
	};

	/// \brief Describes the github viewer record record.
	struct github_viewer_record
	{
		core::github_account_id Account;
		github_actor_id         Node;
		std::string             Login;
		std::string             DisplayName;

		auto operator<=>(const github_viewer_record&) const = default;
	};

	/// \brief Describes the github repository record record.
	struct github_repository_record
	{
		core::repository_id Repository;
		github_node_id      Node;
		std::string         Owner;
		std::string         Name;
		std::string         DefaultBranch;
		bool                Private = false;

		auto operator<=>(const github_repository_record&) const = default;
	};

	/// \brief Describes the github pull request record record.
	struct github_pull_request_record
	{
		core::pull_request_id              PullRequest;
		core::repository_id                Repository;
		core::pull_request_number          Number;
		github_node_id                     Node;
		std::string                        Title;
		std::string                        Body;
		github_pull_request_state          State = github_pull_request_state::open;
		bool                               Draft = false;
		std::optional<github_actor_record> Author;
		commit_oid                         BaseOid;
		commit_oid                         HeadOid;
		std::string                        BaseRef;
		std::string                        HeadRef;
		// REST metadata reports the full PR count, while the pull-request
		// commits endpoint can return only a bounded slice. Synthetic and
		// older snapshots may leave this absent.
		std::optional<std::size_t> ReportedCommitCount;

		auto operator<=>(const github_pull_request_record&) const = default;
	};

	/// \brief Describes the github file record record.
	struct github_file_record
	{
		std::string                Path;
		std::optional<std::string> PreviousPath;
		github_file_change         Change      = github_file_change::uninitialized;
		std::uint64_t              Additions   = 0;
		std::uint64_t              Deletions   = 0;
		github_file_content_kind   ContentKind = github_file_content_kind::unknown;
		github_file_viewed_state   ViewedState = github_file_viewed_state::unavailable;

		auto operator<=>(const github_file_record&) const = default;
	};

	/// \brief Describes the github commit record record.
	struct github_commit_record
	{
		commit_oid                         Oid;
		github_node_id                     Node;
		std::optional<github_actor_record> Author;
		std::string                        Headline;

		auto operator<=>(const github_commit_record&) const = default;
	};

	/// \brief Optional raw remote editing/source facts; absent legacy fields confer no import authority.
	struct github_comment_observation
	{
		std::uint64_t                DatabaseId = 0;
		std::string                  WebUrl;
		std::string                  UpdatedAt;
		std::optional<std::string>   RawBody;
		std::string                  ReviewNode;
		std::string                  Commit;
		std::string                  OriginalCommit;
		std::optional<std::uint32_t> StartLine;
		std::optional<std::uint32_t> OriginalStartLine;
		auto                         operator<=>(const github_comment_observation&) const = default;
	};

	/// \brief Describes the github review record record.
	struct github_review_record
	{
		std::uint64_t                      DatabaseId = 0U;
		github_review_id                   Review;
		std::optional<github_actor_record> Author;
		github_review_state                State = github_review_state::uninitialized;
		std::optional<commit_oid>          Commit;
		std::string                        Body;

		github_comment_observation Observation;

		auto operator<=>(const github_review_record&) const = default;
	};

	/// \brief Describes the github review comment record record.
	struct github_review_comment_record
	{
		github_comment_id                  Comment;
		std::optional<github_comment_id>   ReplyTo;
		std::optional<github_actor_record> Author;
		std::string                        Body;
		std::string                        Path;
		std::optional<std::uint32_t>       Line;
		std::optional<std::uint32_t>       OriginalLine;
		bool                               Outdated = false;
		bool                               Deleted  = false;

		github_comment_observation Observation;

		auto operator<=>(const github_review_comment_record&) const = default;
	};

	/// \brief Describes the github review thread record record.
	struct github_review_thread_record
	{
		github_thread_id                          Thread;
		github_review_thread_subject              Subject = github_review_thread_subject::line;
		std::string                               Path;
		std::optional<std::uint32_t>              Line;
		std::optional<std::uint32_t>              OriginalLine;
		github_diff_side                          DiffSide                  = github_diff_side::uninitialized;
		bool                                      Resolved                  = false;
		bool                                      Outdated                  = false;
		github_permission_state                   ViewerResolvePermission   = github_permission_state::unavailable;
		github_permission_state                   ViewerUnresolvePermission = github_permission_state::unavailable;
		std::vector<github_review_comment_record> Comments;

		auto operator<=>(const github_review_thread_record&) const = default;
	};

	/// \brief Describes one inline review conversation read over REST.
	///
	/// \details A signed-out read reaches `/pulls/{n}/comments`, which is a
	/// flat list carrying `in_reply_to_id` and nothing else about structure.
	/// The grouping is therefore reconstructed from that linkage alone: the
	/// root is the comment no other comment is a reply through, and `Replies`
	/// holds the comments naming it, in the order the server returned them.
	/// Position in the response never decides membership.
	///
	/// The type deliberately carries no resolution, viewed, viewer, or
	/// actionability value. Those observations have no anonymous answer and
	/// RFC-BPR-0017 forbids inferring them from ordering or reply linkage, so
	/// there is no field here for one to be written into.
	struct github_review_comment_group_record
	{
		github_review_comment_record              Root;
		std::vector<github_review_comment_record> Replies;

		auto operator<=>(const github_review_comment_group_record&) const
			= default;
	};

	/// \brief Describes the github issue comment record record.
	struct github_issue_comment_record
	{
		github_comment_id                  Comment;
		std::optional<github_actor_record> Author;
		std::string                        Body;
		bool                               Deleted = false;

		github_comment_observation Observation;

		auto operator<=>(const github_issue_comment_record&) const = default;
	};

	/// \brief The provider collection that supplied a check result.
	enum class github_check_source : std::uint8_t { check_run,
													commit_status };

	/// \brief One current result on a known commit; requiredness is observed separately.
	struct github_check_record
	{
		github_check_id              Check;
		std::string                  Name;
		github_check_status          Status     = github_check_status::uninitialized;
		github_check_conclusion      Conclusion = github_check_conclusion::uninitialized;
		github_check_source          Source     = github_check_source::check_run;
		std::string                  Producer;
		std::string                  Description;
		std::string                  DetailsUrl;
		commit_oid                   EvaluatedOid;
		bool                         StatusError = false;
		std::optional<std::uint64_t> AppId;

		auto operator<=>(const github_check_record&) const = default;
	};

	/// \brief Availability of one independently bounded optional observation.
	enum class github_observation_availability : std::uint8_t { unobserved,
																available,
																permission,
																unsupported,
																incomplete,
																failed };
	enum class github_requested_actor_kind : std::uint8_t { unavailable,
															user,
															team };
	/// \brief One outstanding request; code-owner origin is independent of actor identity.
	struct github_review_request_record
	{
		std::string                 Request, Actor, Login, Organization, Name;
		github_requested_actor_kind Kind = github_requested_actor_kind::unavailable;
		std::optional<bool>         AsCodeOwner;
		auto                        operator<=>(const github_review_request_record&) const = default;
	};
	enum class github_merge_assessment : std::uint8_t { unknown,
														behind,
														blocked,
														clean,
														conflicts,
														draft,
														hooks,
														unstable };
	enum class github_review_decision : std::uint8_t { unknown,
													   required,
													   approved,
													   changes_requested };
	/// \brief Server assessment bound to the accepted PR/head/base; grants no merge authority.
	struct github_merge_observation
	{
		github_observation_availability Availability = github_observation_availability::unobserved;
		github_merge_assessment         Assessment   = github_merge_assessment::unknown;
		github_review_decision          Review       = github_review_decision::unknown;
		std::optional<bool>             AutoMerge;
		bool                            Stale                                              = false;
		auto                            operator<=>(const github_merge_observation&) const = default;
	};
	/// \brief A check context required by an applicable branch policy, optionally tied to an app.
	struct github_required_check
	{
		std::string                  Context;
		std::optional<std::uint64_t> AppId;
		auto                         operator<=>(const github_required_check&) const = default;
	};
	/// \brief An applicable server-selected rule. No local CODEOWNERS or rule evaluation.
	struct github_branch_requirement
	{
		std::string                        Type, Source, SourceKind;
		std::uint64_t                      RulesetId = 0;
		std::optional<std::uint32_t>       ApprovalMinimum;
		bool                               CodeOwners = false, StrictChecks = false, LastPushApproval = false, DismissStaleReviews = false;
		std::vector<github_required_check> Checks;
		// Separate team-review rules retain the server's numeric identity and
		// minimum. Their file-pattern applicability is not evaluated locally.
		std::vector<std::pair<std::uint64_t, std::uint32_t>> RequiredTeams;
		auto                                                 operator<=>(const github_branch_requirement&) const = default;
	};
	/// \brief Policy details for the accepted target; availability remains separate per source.
	struct github_branch_policy_observation
	{
		github_observation_availability        RulesAvailability      = github_observation_availability::unobserved;
		github_observation_availability        ProtectionAvailability = github_observation_availability::unobserved;
		std::vector<github_branch_requirement> Rules, Protection;
		bool                                   RulesStale = false, ProtectionStale = false;
		auto                                   operator<=>(const github_branch_policy_observation&) const = default;
	};

	/// \brief Distinguishes complete repository search from a refused result set.
	enum class github_search_availability : std::uint8_t { unavailable,
														   available,
														   result_limit };

	/// \brief Describes the github search result record record.
	struct github_search_result_record
	{
		core::repository_id       Repository;
		github_node_id            PullRequestNode;
		core::pull_request_number Number;
		std::string               Title;
		github_pull_request_state State = github_pull_request_state::open;
		bool                      Draft = false;

		auto operator<=>(const github_search_result_record&) const = default;
	};

	/// \brief Describes the github capability evidence record.
	struct github_capability_evidence
	{
		core::github_capabilities Capabilities;
		core::capability_revision Revision;
		github_read_shape         ReadShape;
		std::uint32_t             ContractVersion = 1;

		auto operator<=>(const github_capability_evidence&) const = default;
	};

	enum class github_rate_limit_resource : std::uint8_t {
		uninitialized,
		core,
		search,
		graphql,
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const github_rate_limit_resource aResource) noexcept -> bool
	{
		switch (aResource) {
		case github_rate_limit_resource::core:
		case github_rate_limit_resource::search:
		case github_rate_limit_resource::graphql:
			return true;
		case github_rate_limit_resource::uninitialized:
			return false;
		}
		return false;
	}

	/// \brief Describes the github rate limit record record.
	struct github_rate_limit_record
	{
		github_rate_limit_resource Resource         = github_rate_limit_resource::uninitialized;
		std::uint64_t              Limit            = 0;
		std::uint64_t              Remaining        = 0;
		std::uint64_t              ResetUnixSeconds = 0;
		bool                       SecondaryLimited = false;

		auto operator<=>(const github_rate_limit_record&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const github_rate_limit_record& aRecord) noexcept -> bool
	{
		return valid(aRecord.Resource)
			&& aRecord.Remaining <= aRecord.Limit
			&& (!aRecord.SecondaryLimited || aRecord.Remaining != 0U);
	}

	/// \brief Describes the github rate limit state record.
	struct github_rate_limit_state
	{
		std::optional<github_rate_limit_record> Core;
		std::optional<github_rate_limit_record> Search;
		std::optional<github_rate_limit_record> Graphql;

		auto operator<=>(const github_rate_limit_state&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] constexpr auto valid(
		const github_rate_limit_state& aState) noexcept -> bool
	{
		return (
				   !aState.Core.has_value()
				   || (valid(*aState.Core)
					   && aState.Core->Resource
						   == github_rate_limit_resource::core))
			&& (!aState.Search.has_value()
				|| (valid(*aState.Search)
					&& aState.Search->Resource
						== github_rate_limit_resource::search))
			&& (!aState.Graphql.has_value()
				|| (valid(*aState.Graphql)
					&& aState.Graphql->Resource
						== github_rate_limit_resource::graphql));
	}

	/// \brief Computes the observe result.
	[[nodiscard]] constexpr auto observe(
		github_rate_limit_state&        aState,
		const github_rate_limit_record& aRecord) noexcept -> bool
	{
		if (!valid(aRecord)) {
			return false;
		}
		switch (aRecord.Resource) {
		case github_rate_limit_resource::core:
			aState.Core = aRecord;
			return true;
		case github_rate_limit_resource::search:
			aState.Search = aRecord;
			return true;
		case github_rate_limit_resource::graphql:
			aState.Graphql = aRecord;
			return true;
		case github_rate_limit_resource::uninitialized:
			return false;
		}
		return false;
	}

	/// \brief Computes the merge result.
	[[nodiscard]] constexpr auto merge(
		github_rate_limit_state&       aDestination,
		const github_rate_limit_state& aSource) noexcept -> bool
	{
		if (!valid(aSource)) {
			return false;
		}
		if (aSource.Core.has_value()) {
			aDestination.Core = aSource.Core;
		}
		if (aSource.Search.has_value()) {
			aDestination.Search = aSource.Search;
		}
		if (aSource.Graphql.has_value()) {
			aDestination.Graphql = aSource.Graphql;
		}
		return true;
	}

	/// \brief Reports whether the has observations condition holds.
	[[nodiscard]] constexpr auto has_observations(
		const github_rate_limit_state& aState) noexcept -> bool
	{
		return aState.Core.has_value() || aState.Search.has_value()
			|| aState.Graphql.has_value();
	}

	/// \brief Computes the find result.
	[[nodiscard]] constexpr auto find(
		const github_rate_limit_state&   aState,
		const github_rate_limit_resource aResource) noexcept
		-> const github_rate_limit_record*
	{
		switch (aResource) {
		case github_rate_limit_resource::core:
			return aState.Core.has_value() ? &*aState.Core : nullptr;
		case github_rate_limit_resource::search:
			return aState.Search.has_value() ? &*aState.Search : nullptr;
		case github_rate_limit_resource::graphql:
			return aState.Graphql.has_value() ? &*aState.Graphql : nullptr;
		case github_rate_limit_resource::uninitialized:
			return nullptr;
		}
		return nullptr;
	}

	/// \brief Reports whether the is limited condition holds.
	[[nodiscard]] constexpr auto is_limited(
		const github_rate_limit_record& aRecord) noexcept -> bool
	{
		return aRecord.Remaining == 0U || aRecord.SecondaryLimited;
	}

	/// \brief Reports whether the is secondary limited condition holds.
	[[nodiscard]] constexpr auto is_secondary_limited(
		const github_rate_limit_state& aState) noexcept -> bool
	{
		return (
				   aState.Core.has_value()
				   && aState.Core->SecondaryLimited)
			|| (aState.Search.has_value()
				&& aState.Search->SecondaryLimited)
			|| (aState.Graphql.has_value()
				&& aState.Graphql->SecondaryLimited);
	}

	/// \brief Reports whether the is limited condition holds.
	[[nodiscard]] constexpr auto is_limited(
		const github_rate_limit_state& aState) noexcept -> bool
	{
		return (aState.Core.has_value() && is_limited(*aState.Core))
			|| (aState.Search.has_value() && is_limited(*aState.Search))
			|| (aState.Graphql.has_value() && is_limited(*aState.Graphql));
	}

	/// \brief Describes the github session read model record.
	struct github_session_read_model
	{
		core::host_profile_id      Profile;
		core::git_host_id          Host;
		github_viewer_record       Viewer;
		github_capability_evidence CapabilityEvidence;
		github_rate_limit_state    RateLimits;
		// The mode this session was accepted in. Last and defaulted so every
		// session written before the mode existed still means the
		// authenticated session it meant, and part of the defaulted
		// comparison so no equality can drop the discriminant. A
		// public-preview session is a session of its own, not an
		// authenticated one with an absent viewer: `validate` refuses a
		// viewer under public preview and demands one under the
		// authenticated mode, so neither can be read as the other.
		core::github_access_mode Mode{ };

		auto operator<=>(const github_session_read_model&) const = default;
	};

	/// \brief Reports whether the viewer record carries no identity at all.
	///
	/// A public-preview session has no reader to name. That is an absence
	/// rather than a zeroed account: every field must be empty, so a session
	/// cannot carry half a viewer.
	[[nodiscard]] inline auto anonymous(
		const github_viewer_record& aViewer) noexcept -> bool
	{
		return aViewer.Account.value() == 0U && aViewer.Node.value().empty()
			&& aViewer.Login.empty() && aViewer.DisplayName.empty();
	}

	/// \brief Reports whether the read shape claims nothing.
	[[nodiscard]] constexpr auto reads_nothing(
		const github_read_shape& aShape) noexcept -> bool
	{
		return !aShape.ReviewThreads && !aShape.ViewedFileState
			&& !aShape.ResolvePermission && !aShape.UnresolvePermission;
	}

	/// \brief Reports the independent bounded timeline observation.
	// Available means the REST timeline was exhausted, not that ordinary push
	// boundaries are known: GitHub does not emit those events.
	enum class github_timeline_availability : std::uint8_t {
		unavailable,
		available,
		refused,
		capped,
		failed,
	};

	/// \brief A force-push occurrence with an exact numeric identity and UTC event time.
	struct github_force_push_event
	{
		std::uint64_t EventId = 0U;
		std::string   AfterHeadOid;
		std::int64_t  CreatedAtUnixMilliseconds = 0;

		auto operator<=>(const github_force_push_event&) const = default;
	};

	/// \brief Describes the github pull request snapshot record.
	/// \brief [[BPR-1447]]. One feedback post's body digest, computed off the
	/// window thread with the snapshot it belongs to.
	struct github_feedback_body_digest
	{
		/// The post as the feedback adapter names it, and its body's length:
		/// a digest is used only for the post and length it was taken from.
		std::string                  Post;
		std::size_t                  Bytes = 0U;
		std::array<std::uint8_t, 32> Digest{ };
	};

	/// \brief [[BPR-1447]]. The SHA-256 of each feedback body of one snapshot,
	/// in the feedback adapter's post order, taken by the host before the
	/// snapshot reached the decision lane.
	/// \details Derived from the snapshot's bodies and shared, never edited: a
	/// copy of the snapshot shares it. It takes no part in comparing
	/// snapshots, which compare the bodies themselves. Whoever changes a body
	/// of a snapshot after digesting it must reset this; an absent or
	/// mismatched entry leaves the core to hash that body itself.
	struct github_feedback_body_digests
	{
		std::shared_ptr<const std::vector<github_feedback_body_digest>> Entries;

		auto operator==(const github_feedback_body_digests& /*aOther*/) const noexcept -> bool { return true; }
		auto operator<=>(const github_feedback_body_digests& /*aOther*/) const noexcept -> std::strong_ordering { return std::strong_ordering::equal; }
	};

	struct github_pull_request_snapshot
	{
		core::host_profile_id                    Profile;
		core::git_host_id                        Host;
		core::github_account_id                  Account;
		github_actor_record                      Viewer;
		core::provider_snapshot_id               Snapshot;
		github_repository_record                 Repository;
		github_pull_request_record               PullRequest;
		std::vector<github_file_record>          Files;
		std::vector<github_commit_record>        Commits;
		std::vector<github_review_record>        Reviews;
		github_collection_availability           ReviewThreadsAvailability = github_collection_availability::unavailable;
		std::vector<github_review_thread_record> ReviewThreads;
		// The REST inline review conversation is a separate observation from
		// the GraphQL threads above and is never written into them: the
		// threads are unavailable on a signed-out read, and an unavailable
		// collection must stay empty. Availability leads with `unavailable`
		// so a snapshot that was never asked cannot read as one that was
		// asked and found nothing.
		github_collection_availability                  ReviewCommentsAvailability = github_collection_availability::unavailable;
		std::vector<github_review_comment_group_record> ReviewCommentGroups;
		std::vector<github_issue_comment_record>        IssueComments;
		std::vector<github_check_record>                Checks;
		github_collection_availability                  ChecksAvailability   = github_collection_availability::available;
		github_collection_availability                  StatusesAvailability = github_collection_availability::unavailable;
		std::vector<github_search_result_record>        SearchResults;
		github_search_availability                      SearchAvailability = github_search_availability::unavailable;
		github_capability_evidence                      CapabilityEvidence;
		github_rate_limit_state                         RateLimits;
		// The mode this snapshot was read in. Defaulted so every
		// snapshot written before the mode existed still means the
		// authenticated read it meant. Under public preview Account and the
		// viewer actor are empty by rule rather than by omission.
		core::github_access_mode             Mode{ };
		github_timeline_availability         TimelineAvailability = github_timeline_availability::unavailable;
		std::vector<github_force_push_event> ForcePushEvents;
		/// [[BPR-1447]]. Feedback body digests the host took off the window
		/// thread; empty when none were.
		github_feedback_body_digests FeedbackBodyDigests;

		github_observation_availability           ReviewRequestsAvailability = github_observation_availability::unobserved;
		std::vector<github_review_request_record> ReviewRequests;
		bool                                      ReviewRequestsStale = false;
		// A complete REST fallback observes actors, including an empty roster,
		// independently of the richer connection's code-owner availability.
		bool                             ReviewRequestActorsObserved = false;
		github_merge_observation         Merge;
		github_branch_policy_observation BranchPolicy;

		auto operator<=>(const github_pull_request_snapshot&) const = default;
	};

	inline constexpr std::size_t maximum_github_pull_request_endpoint_commits = 250U;

	/// \brief Describes the github read limits record.
	struct github_read_limits
	{
		std::size_t MaximumIdentityBytes     = 256U;
		std::size_t MaximumLoginBytes        = 256U;
		std::size_t MaximumDisplayNameBytes  = 1024U;
		std::size_t MaximumTitleBytes        = 1024U;
		std::size_t MaximumBodyBytes         = 256U * 1024U;
		std::size_t MaximumPathBytes         = 4U * 1024U;
		std::size_t MaximumRefBytes          = 256U;
		std::size_t MaximumHeadlineBytes     = 1024U;
		std::size_t MaximumCheckNameBytes    = 1024U;
		std::size_t MaximumFiles             = 3'000U;
		std::size_t MaximumCommits           = maximum_github_pull_request_endpoint_commits;
		std::size_t MaximumReviews           = 10'000U;
		std::size_t MaximumReviewThreads     = 10'000U;
		std::size_t MaximumCommentsPerThread = 10'000U;
		std::size_t MaximumIssueComments     = 10'000U;
		std::size_t MaximumTotalComments     = 50'000U;
		std::size_t MaximumChecks            = 10'000U;
		std::size_t MaximumSearchResults     = 1'000U;
		std::size_t MaximumTotalRecords      = 50'000U;
		std::size_t MaximumTotalTextBytes    = 16U * 1024U * 1024U;

		auto operator<=>(const github_read_limits&) const = default;
	};

	enum class github_read_validation_error : std::uint8_t {
		none,
		invalid_context,
		invalid_limits,
		missing_identity,
		identity_mismatch,
		missing_required_text,
		invalid_text,
		text_too_long,
		invalid_repository,
		invalid_pull_request_number,
		invalid_commit_oid,
		invalid_line,
		invalid_enum,
		invalid_capability_revision,
		invalid_rate_limit,
		collection_too_large,
		duplicate_identity,
		invalid_relationship,
		allocation_limit,
		total_text_too_large,
		// Diagnostic refinements of invalid_relationship; acceptance uses Error.
		relationship_file_previous_path,
		relationship_thread_comment_path,
		relationship_review_group_root,
		relationship_review_group_reply,
		relationship_actor_login,
		relationship_thread_reply,
		relationship_search_repository,
		relationship_search_target_identity,
		relationship_search_target_metadata,
		relationship_timeline_availability,
		relationship_timeline_event,
		relationship_review_comments_availability,
		relationship_review_threads_capability,
		relationship_file_viewed_capability,
		relationship_thread_permission_capability,
		relationship_search_availability,
	};

	/// \brief Collection provenance for a conflicting actor observation, without actor data.
	enum class github_actor_observation_origin : std::uint8_t {
		viewer,
		pull_request,
		commit,
		review,
		thread_comment,
		review_comment_root,
		review_comment_reply,
		issue_comment,
	};

	/// \brief Bounded disagreement facts; neither login nor stable identity leaves validation.
	struct github_actor_login_conflict
	{
		github_actor_observation_origin First;
		github_actor_observation_origin Conflicting;
		bool                            AsciiCaseOnly                                         = false;
		auto                            operator<=>(const github_actor_login_conflict&) const = default;
	};

	/// \brief Describes the github read validation record.
	struct github_read_validation
	{
		github_read_validation_error Error = github_read_validation_error::none;
		/// Fixed diagnostic refinement; never contains provider text or identities.
		github_read_validation_error               Detail = github_read_validation_error::none;
		std::optional<github_actor_login_conflict> ActorConflict;

		[[nodiscard]] constexpr auto valid() const noexcept -> bool
		{
			return Error == github_read_validation_error::none;
		}

		auto operator<=>(const github_read_validation&) const = default;
	};

	namespace detail {
		/// \brief Provides the github text budget contract.
		class github_text_budget
		{
		public:
			explicit github_text_budget(const github_read_limits& aLimits)
				: Limits{ aLimits }
			{
			}

			[[nodiscard]] auto consume(
				const std::string_view aText,
				const std::size_t      aFieldLimit,
				const bool             aRequired = false) noexcept
				-> github_read_validation_error
			{
				if (aRequired && aText.empty()) {
					return github_read_validation_error::missing_required_text;
				}
				if (aText.find('\0') != std::string_view::npos) {
					return github_read_validation_error::invalid_text;
				}
				if (aText.size() > aFieldLimit) {
					return github_read_validation_error::text_too_long;
				}
				if (
					aText.size() > Limits.MaximumTotalTextBytes
					|| Total > Limits.MaximumTotalTextBytes - aText.size()) {
					return github_read_validation_error::total_text_too_large;
				}
				Total += aText.size();
				return github_read_validation_error::none;
			}

		private:
			const github_read_limits& Limits;
			std::size_t               Total = 0;
		};

		[[nodiscard]] inline auto validation(
			const github_read_validation_error aError) noexcept
			-> github_read_validation
		{
			if (aError >= github_read_validation_error::relationship_file_previous_path
				&& aError <= github_read_validation_error::relationship_search_availability) {
				return { .Error = github_read_validation_error::invalid_relationship, .Detail = aError };
			}
			return github_read_validation{ .Error = aError };
		}

		[[nodiscard]] constexpr auto within_security_ceiling(
			const github_read_limits& aLimits) noexcept -> bool
		{
			constexpr github_read_limits ceiling;
			return aLimits.MaximumIdentityBytes <= ceiling.MaximumIdentityBytes
				&& aLimits.MaximumLoginBytes <= ceiling.MaximumLoginBytes
				&& aLimits.MaximumDisplayNameBytes
				<= ceiling.MaximumDisplayNameBytes
				&& aLimits.MaximumTitleBytes <= ceiling.MaximumTitleBytes
				&& aLimits.MaximumBodyBytes <= ceiling.MaximumBodyBytes
				&& aLimits.MaximumPathBytes <= ceiling.MaximumPathBytes
				&& aLimits.MaximumRefBytes <= ceiling.MaximumRefBytes
				&& aLimits.MaximumHeadlineBytes <= ceiling.MaximumHeadlineBytes
				&& aLimits.MaximumCheckNameBytes <= ceiling.MaximumCheckNameBytes
				&& aLimits.MaximumFiles <= ceiling.MaximumFiles
				&& aLimits.MaximumCommits <= ceiling.MaximumCommits
				&& aLimits.MaximumReviews <= ceiling.MaximumReviews
				&& aLimits.MaximumReviewThreads <= ceiling.MaximumReviewThreads
				&& aLimits.MaximumCommentsPerThread
				<= ceiling.MaximumCommentsPerThread
				&& aLimits.MaximumIssueComments <= ceiling.MaximumIssueComments
				&& aLimits.MaximumTotalComments <= ceiling.MaximumTotalComments
				&& aLimits.MaximumChecks <= ceiling.MaximumChecks
				&& aLimits.MaximumSearchResults <= ceiling.MaximumSearchResults
				&& aLimits.MaximumTotalRecords <= ceiling.MaximumTotalRecords
				&& aLimits.MaximumTotalTextBytes <= ceiling.MaximumTotalTextBytes;
		}

		[[nodiscard]] inline auto valid_context(
			const core::github_session_request_context& aContext) noexcept -> bool
		{
			return aContext.Request.value() != 0U
				&& aContext.ApplicationSession.value() != 0U
				&& aContext.Profile.value() != 0U
				&& aContext.IdentityGeneration.value() != 0U;
		}

		[[nodiscard]] inline auto valid_context(
			const core::pull_request_request_context& aContext) noexcept -> bool
		{
			// Two rules, one per mode: public preview names no reader and the
			// authenticated mode names an exact one. Neither context can be
			// read under the other's rule.
			const auto readerAsExpected = aContext.Mode
					== core::github_access_mode::public_preview
				? aContext.Account.value() == 0U
				: aContext.Account.value() != 0U;
			return aContext.Request.value() != 0U
				&& aContext.ApplicationSession.value() != 0U
				&& aContext.Profile.value() != 0U && aContext.Host.value() != 0U
				&& readerAsExpected && core::valid(aContext.Repository)
				&& aContext.Number.valid()
				&& (!aContext.ExpectedRepository.has_value()
					|| aContext.ExpectedRepository->value() != 0U)
				&& (!aContext.ExpectedPullRequest.has_value()
					|| aContext.ExpectedPullRequest->value() != 0U)
				&& aContext.Capabilities.value() != 0U
				&& aContext.IdentityGeneration.value() != 0U
				&& aContext.Generation.value() != 0U;
		}

		/// \brief [[BPR-0988]]. A repository read names an exact account: it has no
		/// signed-out form.
		[[nodiscard]] inline auto valid_context(
			const core::repository_request_context& aContext) noexcept -> bool
		{
			return aContext.Request.value() != 0U
				&& aContext.ApplicationSession.value() != 0U
				&& aContext.Profile.value() != 0U && aContext.Host.value() != 0U
				&& aContext.Account.value() != 0U && core::valid(aContext.Repository)
				&& aContext.Capabilities.value() != 0U
				&& aContext.IdentityGeneration.value() != 0U;
		}

		[[nodiscard]] inline auto ascii_equal_fold(
			const std::string_view aLeft,
			const std::string_view aRight) noexcept -> bool
		{
			if (aLeft.size() != aRight.size()) {
				return false;
			}

			for (std::size_t index = 0; index < aLeft.size(); ++index) {
				const auto fold = [](const char aValue) {
					return aValue >= 'A' && aValue <= 'Z'
						? static_cast<char>(aValue - 'A' + 'a')
						: aValue;
				};
				if (fold(aLeft[index]) != fold(aRight[index])) {
					return false;
				}
			}
			return true;
		}

		[[nodiscard]] inline auto hexadecimal_oid(
			const std::string_view aValue) noexcept -> bool
		{
			if (aValue.size() != 40U && aValue.size() != 64U) {
				return false;
			}
			for (const auto character : aValue) {
				const auto hexadecimal = (character >= '0' && character <= '9')
					|| (character >= 'a' && character <= 'f')
					|| (character >= 'A' && character <= 'F');
				if (!hexadecimal) {
					return false;
				}
			}
			return true;
		}

		[[nodiscard]] constexpr auto valid_enum(
			const github_actor_kind aValue) noexcept -> bool
		{
			switch (aValue) {
			case github_actor_kind::unobserved:
			case github_actor_kind::user:
			case github_actor_kind::bot:
			case github_actor_kind::other:
				return true;
			}
			return false;
		}

		[[nodiscard]] constexpr auto valid_enum(
			const github_pull_request_state aValue) noexcept -> bool
		{
			switch (aValue) {
			case github_pull_request_state::open:
			case github_pull_request_state::closed:
			case github_pull_request_state::merged:
				return true;
			}
			return false;
		}

		[[nodiscard]] constexpr auto valid_enum(
			const github_file_change aValue) noexcept -> bool
		{
			switch (aValue) {
			case github_file_change::added:
			case github_file_change::modified:
			case github_file_change::removed:
			case github_file_change::renamed:
			case github_file_change::copied:
			case github_file_change::type_changed:
			case github_file_change::unchanged:
				return true;
			case github_file_change::uninitialized:
				return false;
			}
			return false;
		}

		[[nodiscard]] constexpr auto valid_enum(
			const github_file_content_kind aValue) noexcept -> bool
		{
			switch (aValue) {
			case github_file_content_kind::unknown:
			case github_file_content_kind::text:
			case github_file_content_kind::binary:
				return true;
			}
			return false;
		}

		[[nodiscard]] constexpr auto valid_enum(
			const github_file_viewed_state aValue) noexcept -> bool
		{
			switch (aValue) {
			case github_file_viewed_state::unavailable:
			case github_file_viewed_state::dismissed:
			case github_file_viewed_state::unviewed:
			case github_file_viewed_state::viewed:
				return true;
			}
			return false;
		}

		[[nodiscard]] constexpr auto valid_enum(
			const github_collection_availability aValue) noexcept -> bool
		{
			return aValue == github_collection_availability::unavailable
				|| aValue == github_collection_availability::available;
		}

		[[nodiscard]] constexpr auto valid_enum(
			const github_permission_state aValue) noexcept -> bool
		{
			return aValue == github_permission_state::unavailable
				|| aValue == github_permission_state::denied
				|| aValue == github_permission_state::allowed;
		}

		[[nodiscard]] constexpr auto valid_enum(
			const github_review_state aValue) noexcept -> bool
		{
			switch (aValue) {
			case github_review_state::pending:
			case github_review_state::commented:
			case github_review_state::approved:
			case github_review_state::changes_requested:
			case github_review_state::dismissed:
				return true;
			case github_review_state::uninitialized:
				return false;
			}
			return false;
		}

		[[nodiscard]] constexpr auto valid_enum(
			const github_review_thread_subject aValue) noexcept -> bool
		{
			switch (aValue) {
			case github_review_thread_subject::line:
			case github_review_thread_subject::file:
				return true;
			}
			return false;
		}

		[[nodiscard]] constexpr auto valid_enum(
			const github_diff_side aValue) noexcept -> bool
		{
			switch (aValue) {
			case github_diff_side::left:
			case github_diff_side::right:
				return true;
			case github_diff_side::uninitialized:
				return false;
			}
			return false;
		}

		[[nodiscard]] constexpr auto valid_enum(
			const github_check_status aValue) noexcept -> bool
		{
			switch (aValue) {
			case github_check_status::queued:
			case github_check_status::in_progress:
			case github_check_status::completed:
			case github_check_status::waiting:
			case github_check_status::requested:
			case github_check_status::pending:
				return true;
			case github_check_status::uninitialized:
				return false;
			}
			return false;
		}

		[[nodiscard]] constexpr auto valid_enum(
			const github_check_conclusion aValue) noexcept -> bool
		{
			switch (aValue) {
			case github_check_conclusion::none:
			case github_check_conclusion::success:
			case github_check_conclusion::failure:
			case github_check_conclusion::neutral:
			case github_check_conclusion::cancelled:
			case github_check_conclusion::skipped:
			case github_check_conclusion::timed_out:
			case github_check_conclusion::action_required:
			case github_check_conclusion::stale:
				return true;
			case github_check_conclusion::uninitialized:
				return false;
			}
			return false;
		}

		[[nodiscard]] constexpr auto valid_enum(
			const core::github_capability_state aValue) noexcept -> bool
		{
			switch (aValue) {
			case core::github_capability_state::unknown:
			case core::github_capability_state::supported:
			case core::github_capability_state::unsupported:
				return true;
			}
			return false;
		}

		[[nodiscard]] inline auto consume_id(
			github_text_budget&       aBudget,
			const std::string_view    aValue,
			const github_read_limits& aLimits) noexcept
			-> github_read_validation_error
		{
			return aBudget.consume(aValue, aLimits.MaximumIdentityBytes, true);
		}

		[[nodiscard]] inline auto validate_author(
			const std::optional<github_actor_record>& aAuthor,
			github_text_budget&                       aBudget,
			const github_read_limits&                 aLimits) noexcept
			-> github_read_validation_error
		{
			if (!aAuthor.has_value()) {
				return github_read_validation_error::none;
			}
			if (aAuthor->Actor.value().empty()) {
				return github_read_validation_error::missing_identity;
			}
			if (!valid_enum(aAuthor->Kind)) {
				return github_read_validation_error::invalid_enum;
			}
			if (const auto error = consume_id(aBudget, aAuthor->Actor.value(), aLimits);
				error != github_read_validation_error::none) {
				return error;
			}
			return aBudget.consume(
				aAuthor->Login,
				aLimits.MaximumLoginBytes,
				true);
		}

		[[nodiscard]] inline auto validate_actor(
			const github_actor_record& aActor,
			github_text_budget&        aBudget,
			const github_read_limits&  aLimits,
			const bool                 aLoginRequired) noexcept
			-> github_read_validation_error
		{
			if (aActor.Actor.value().empty()) {
				return github_read_validation_error::missing_identity;
			}
			if (!valid_enum(aActor.Kind)) {
				return github_read_validation_error::invalid_enum;
			}
			if (const auto error = consume_id(aBudget, aActor.Actor.value(), aLimits);
				error != github_read_validation_error::none) {
				return error;
			}
			return aBudget.consume(
				aActor.Login,
				aLimits.MaximumLoginBytes,
				aLoginRequired);
		}

		[[nodiscard]] inline auto validate_viewer(
			const github_viewer_record& aRecord,
			github_text_budget&         aBudget,
			const github_read_limits&   aLimits) noexcept
			-> github_read_validation_error
		{
			if (aRecord.Account.value() == 0U) {
				return github_read_validation_error::missing_identity;
			}
			if (const auto error = consume_id(aBudget, aRecord.Node.value(), aLimits);
				error != github_read_validation_error::none) {
				return error;
			}
			if (const auto error = aBudget.consume(aRecord.Login, aLimits.MaximumLoginBytes, true);
				error != github_read_validation_error::none) {
				return error;
			}
			return aBudget.consume(
				aRecord.DisplayName,
				aLimits.MaximumDisplayNameBytes);
		}

		[[nodiscard]] inline auto validate_repository(
			const github_repository_record& aRecord,
			github_text_budget&             aBudget,
			const github_read_limits&       aLimits) noexcept
			-> github_read_validation_error
		{
			if (aRecord.Repository.value() == 0U) {
				return github_read_validation_error::missing_identity;
			}
			if (const auto error = consume_id(aBudget, aRecord.Node.value(), aLimits);
				error != github_read_validation_error::none) {
				return error;
			}
			if (!core::valid(
					core::repository_locator{
						.Owner = aRecord.Owner,
						.Name  = aRecord.Name,
					})) {
				return github_read_validation_error::invalid_repository;
			}
			if (const auto error = aBudget.consume(
					aRecord.Owner,
					core::maximum_repository_owner_length,
					true);
				error != github_read_validation_error::none) {
				return error;
			}
			if (const auto error = aBudget.consume(
					aRecord.Name,
					core::maximum_repository_name_length,
					true);
				error != github_read_validation_error::none) {
				return error;
			}
			return aBudget.consume(
				aRecord.DefaultBranch,
				aLimits.MaximumRefBytes,
				true);
		}

		[[nodiscard]] inline auto validate_pull_request(
			const github_pull_request_record& aRecord,
			github_text_budget&               aBudget,
			const github_read_limits&         aLimits) noexcept
			-> github_read_validation_error
		{
			if (
				aRecord.PullRequest.value() == 0U
				|| aRecord.Repository.value() == 0U) {
				return github_read_validation_error::missing_identity;
			}
			if (!aRecord.Number.valid()) {
				return github_read_validation_error::invalid_pull_request_number;
			}
			if (!valid_enum(aRecord.State)) {
				return github_read_validation_error::invalid_enum;
			}
			if (const auto error = consume_id(aBudget, aRecord.Node.value(), aLimits);
				error != github_read_validation_error::none) {
				return error;
			}
			if (const auto error = validate_author(aRecord.Author, aBudget, aLimits);
				error != github_read_validation_error::none) {
				return error;
			}
			if (const auto error = aBudget.consume(aRecord.Title, aLimits.MaximumTitleBytes, true);
				error != github_read_validation_error::none) {
				return error;
			}
			if (const auto error = aBudget.consume(aRecord.Body, aLimits.MaximumBodyBytes);
				error != github_read_validation_error::none) {
				return error;
			}
			if (
				!hexadecimal_oid(aRecord.BaseOid.value())
				|| !hexadecimal_oid(aRecord.HeadOid.value())) {
				return github_read_validation_error::invalid_commit_oid;
			}
			if (const auto error = aBudget.consume(aRecord.BaseOid.value(), 64U, true);
				error != github_read_validation_error::none) {
				return error;
			}
			if (const auto error = aBudget.consume(aRecord.HeadOid.value(), 64U, true);
				error != github_read_validation_error::none) {
				return error;
			}
			if (const auto error = aBudget.consume(aRecord.BaseRef, aLimits.MaximumRefBytes, true);
				error != github_read_validation_error::none) {
				return error;
			}
			return aBudget.consume(
				aRecord.HeadRef,
				aLimits.MaximumRefBytes,
				true);
		}

		[[nodiscard]] inline auto validate_file(
			const github_file_record& aRecord,
			github_text_budget&       aBudget,
			const github_read_limits& aLimits) noexcept
			-> github_read_validation_error
		{
			if (
				!valid_enum(aRecord.Change) || !valid_enum(aRecord.ContentKind)
				|| !valid_enum(aRecord.ViewedState)) {
				return github_read_validation_error::invalid_enum;
			}
			if (
				aRecord.PreviousPath.has_value()
				&& aRecord.Change != github_file_change::renamed
				&& aRecord.Change != github_file_change::copied) {
				return github_read_validation_error::relationship_file_previous_path;
			}
			if (const auto error = aBudget.consume(aRecord.Path, aLimits.MaximumPathBytes, true);
				error != github_read_validation_error::none) {
				return error;
			}
			if (aRecord.PreviousPath.has_value()) {
				return aBudget.consume(
					*aRecord.PreviousPath,
					aLimits.MaximumPathBytes,
					true);
			}
			return github_read_validation_error::none;
		}

		[[nodiscard]] inline auto validate_commit(
			const github_commit_record& aRecord,
			github_text_budget&         aBudget,
			const github_read_limits&   aLimits) noexcept
			-> github_read_validation_error
		{
			if (!hexadecimal_oid(aRecord.Oid.value())) {
				return github_read_validation_error::invalid_commit_oid;
			}
			if (const auto error = aBudget.consume(aRecord.Oid.value(), 64U, true);
				error != github_read_validation_error::none) {
				return error;
			}
			if (const auto error = consume_id(aBudget, aRecord.Node.value(), aLimits);
				error != github_read_validation_error::none) {
				return error;
			}
			if (const auto error = validate_author(aRecord.Author, aBudget, aLimits);
				error != github_read_validation_error::none) {
				return error;
			}
			return aBudget.consume(
				aRecord.Headline,
				aLimits.MaximumHeadlineBytes);
		}

		[[nodiscard]] inline auto validate_comment_observation(const github_comment_observation& aValue, github_text_budget& aBudget, const github_read_limits& aLimits) noexcept -> github_read_validation_error
		{
			for (const auto* text : { &aValue.WebUrl, &aValue.UpdatedAt, &aValue.ReviewNode, &aValue.Commit, &aValue.OriginalCommit }) {
				if (const auto error = aBudget.consume(*text, aLimits.MaximumPathBytes); error != github_read_validation_error::none) {
					return error;
				}
			}
			if (aValue.RawBody) {
				return aBudget.consume(*aValue.RawBody, aLimits.MaximumBodyBytes);
			}
			return github_read_validation_error::none;
		}

		[[nodiscard]] inline auto validate_review(
			const github_review_record& aRecord,
			github_text_budget&         aBudget,
			const github_read_limits&   aLimits) noexcept
			-> github_read_validation_error
		{
			if (const auto error = validate_comment_observation(aRecord.Observation, aBudget, aLimits); error != github_read_validation_error::none) {
				return error;
			}

			if (aRecord.DatabaseId == 0U || !valid_enum(aRecord.State)) {
				return github_read_validation_error::invalid_enum;
			}
			if (const auto error = consume_id(aBudget, aRecord.Review.value(), aLimits);
				error != github_read_validation_error::none) {
				return error;
			}
			if (const auto error = validate_author(aRecord.Author, aBudget, aLimits);
				error != github_read_validation_error::none) {
				return error;
			}
			if (aRecord.Commit.has_value()) {
				if (!hexadecimal_oid(aRecord.Commit->value())) {
					return github_read_validation_error::invalid_commit_oid;
				}
				if (const auto error = aBudget.consume(aRecord.Commit->value(), 64U, true);
					error != github_read_validation_error::none) {
					return error;
				}
			}
			return aBudget.consume(aRecord.Body, aLimits.MaximumBodyBytes);
		}

		[[nodiscard]] inline auto validate_review_comment(
			const github_review_comment_record& aRecord,
			github_text_budget&                 aBudget,
			const github_read_limits&           aLimits) noexcept
			-> github_read_validation_error
		{
			if (const auto error = validate_comment_observation(aRecord.Observation, aBudget, aLimits); error != github_read_validation_error::none) {
				return error;
			}

			const auto invalidLine = [](const std::optional<std::uint32_t>& aLine) {
				return aLine.has_value()
					&& (*aLine == 0U || *aLine > maximum_github_line);
			};
			if (invalidLine(aRecord.Line) || invalidLine(aRecord.OriginalLine)) {
				return github_read_validation_error::invalid_line;
			}
			if (const auto error = consume_id(aBudget, aRecord.Comment.value(), aLimits);
				error != github_read_validation_error::none) {
				return error;
			}
			if (const auto error = validate_author(aRecord.Author, aBudget, aLimits);
				error != github_read_validation_error::none) {
				return error;
			}
			if (aRecord.ReplyTo.has_value()) {
				if (const auto error = consume_id(aBudget, aRecord.ReplyTo->value(), aLimits);
					error != github_read_validation_error::none) {
					return error;
				}
			}
			if (const auto error = aBudget.consume(aRecord.Body, aLimits.MaximumBodyBytes);
				error != github_read_validation_error::none) {
				return error;
			}
			return aBudget.consume(
				aRecord.Path,
				aLimits.MaximumPathBytes,
				true);
		}

		[[nodiscard]] inline auto validate_thread(
			const github_review_thread_record& aRecord,
			github_text_budget&                aBudget,
			const github_read_limits&          aLimits) noexcept
			-> github_read_validation_error
		{
			if (aRecord.Comments.size() > aLimits.MaximumCommentsPerThread) {
				return github_read_validation_error::collection_too_large;
			}
			if (!valid_enum(aRecord.Subject) || !valid_enum(aRecord.DiffSide)) {
				return github_read_validation_error::invalid_enum;
			}
			if (
				!valid_enum(aRecord.ViewerResolvePermission)
				|| !valid_enum(aRecord.ViewerUnresolvePermission)) {
				return github_read_validation_error::invalid_enum;
			}
			const auto invalidLine = [](const std::optional<std::uint32_t>& aLine) {
				return aLine.has_value()
					&& (*aLine == 0U || *aLine > maximum_github_line);
			};
			if ((aRecord.Subject == github_review_thread_subject::line
				 && !aRecord.Outdated && !aRecord.Line.has_value()
				 && aRecord.OriginalLine.has_value())
				|| (aRecord.Subject == github_review_thread_subject::file
					&& (aRecord.Line.has_value()
						|| aRecord.OriginalLine.has_value()))
				|| invalidLine(aRecord.Line) || invalidLine(aRecord.OriginalLine)) {
				return github_read_validation_error::invalid_line;
			}
			if (const auto error = consume_id(aBudget, aRecord.Thread.value(), aLimits);
				error != github_read_validation_error::none) {
				return error;
			}
			if (const auto error = aBudget.consume(aRecord.Path, aLimits.MaximumPathBytes, true);
				error != github_read_validation_error::none) {
				return error;
			}
			for (const auto& comment : aRecord.Comments) {
				const auto hasLine = comment.Line.has_value()
					|| comment.OriginalLine.has_value();
				if (comment.Path != aRecord.Path
					|| (aRecord.Subject == github_review_thread_subject::file
						&& hasLine)) {
					return github_read_validation_error::relationship_thread_comment_path;
				}
				if (const auto error = validate_review_comment(comment, aBudget, aLimits);
					error != github_read_validation_error::none) {
					return error;
				}
			}
			return github_read_validation_error::none;
		}

		/// Validates one reconstructed inline review conversation.
		///
		/// The structural rules are the whole point of the type: a root is a
		/// comment that replies to nothing, and every reply names that root
		/// and no one else. Together they refuse a self-reference, a reply to
		/// a reply, and any cycle, because no reply can name anything but a
		/// root and no root names anything at all. A reply whose target is
		/// missing cannot be represented here at all, so a producer that
		/// observed one must refuse rather than drop it.
		[[nodiscard]] inline auto validate_review_comment_group(
			const github_review_comment_group_record& aRecord,
			github_text_budget&                       aBudget,
			const github_read_limits&                 aLimits) noexcept
			-> github_read_validation_error
		{
			if (aRecord.Replies.size() >= aLimits.MaximumCommentsPerThread) {
				return github_read_validation_error::collection_too_large;
			}
			if (aRecord.Root.ReplyTo.has_value()) {
				return github_read_validation_error::relationship_review_group_root;
			}
			if (const auto error = validate_review_comment(aRecord.Root, aBudget, aLimits);
				error != github_read_validation_error::none) {
				return error;
			}
			for (const auto& reply : aRecord.Replies) {
				if (
					!reply.ReplyTo.has_value()
					|| *reply.ReplyTo != aRecord.Root.Comment
					|| reply.Comment == aRecord.Root.Comment) {
					return github_read_validation_error::relationship_review_group_reply;
				}
				if (const auto error = validate_review_comment(reply, aBudget, aLimits);
					error != github_read_validation_error::none) {
					return error;
				}
			}
			return github_read_validation_error::none;
		}

		[[nodiscard]] inline auto validate_issue_comment(
			const github_issue_comment_record& aRecord,
			github_text_budget&                aBudget,
			const github_read_limits&          aLimits) noexcept
			-> github_read_validation_error
		{
			if (const auto error = validate_comment_observation(aRecord.Observation, aBudget, aLimits); error != github_read_validation_error::none) {
				return error;
			}

			if (const auto error = consume_id(aBudget, aRecord.Comment.value(), aLimits);
				error != github_read_validation_error::none) {
				return error;
			}
			if (const auto error = validate_author(aRecord.Author, aBudget, aLimits);
				error != github_read_validation_error::none) {
				return error;
			}
			return aBudget.consume(aRecord.Body, aLimits.MaximumBodyBytes);
		}

		[[nodiscard]] inline auto validate_check(
			const github_check_record& aRecord,
			github_text_budget&        aBudget,
			const github_read_limits&  aLimits) noexcept
			-> github_read_validation_error
		{
			if (!valid_enum(aRecord.Status) || !valid_enum(aRecord.Conclusion)) {
				return github_read_validation_error::invalid_enum;
			}
			if (
				(
					aRecord.Status == github_check_status::completed
					&& aRecord.Conclusion == github_check_conclusion::none)
				|| (aRecord.Status != github_check_status::completed
					&& aRecord.Conclusion != github_check_conclusion::none)) {
				return github_read_validation_error::invalid_enum;
			}
			if (const auto error = consume_id(aBudget, aRecord.Check.value(), aLimits);
				error != github_read_validation_error::none) {
				return error;
			}
			if ((aRecord.Source != github_check_source::check_run && aRecord.Source != github_check_source::commit_status)
				|| (!aRecord.EvaluatedOid.value().empty() && !hexadecimal_oid(aRecord.EvaluatedOid.value()))) {
				return github_read_validation_error::invalid_enum;
			}
			for (const auto& value : { std::string_view{ aRecord.Producer }, std::string_view{ aRecord.Description }, std::string_view{ aRecord.DetailsUrl } }) {
				if (const auto error = aBudget.consume(value, aLimits.MaximumBodyBytes); error != github_read_validation_error::none) {
					return error;
				}
			}
			return aBudget.consume(
				aRecord.Name,
				aLimits.MaximumCheckNameBytes,
				true);
		}

		[[nodiscard]] inline auto validate_search_result(
			const github_search_result_record& aRecord,
			github_text_budget&                aBudget,
			const github_read_limits&          aLimits) noexcept
			-> github_read_validation_error
		{
			if (aRecord.Repository.value() == 0U) {
				return github_read_validation_error::missing_identity;
			}
			if (!aRecord.Number.valid()) {
				return github_read_validation_error::invalid_pull_request_number;
			}
			if (
				!valid_enum(aRecord.State)
				|| aRecord.State != github_pull_request_state::open) {
				return github_read_validation_error::invalid_enum;
			}
			if (const auto error = consume_id(aBudget, aRecord.PullRequestNode.value(), aLimits);
				error != github_read_validation_error::none) {
				return error;
			}
			return aBudget.consume(
				aRecord.Title,
				aLimits.MaximumTitleBytes,
				true);
		}

		[[nodiscard]] inline auto validate_capabilities(
			const github_capability_evidence& aEvidence) noexcept
			-> github_read_validation_error
		{
			if (aEvidence.Revision.value() == 0U || aEvidence.ContractVersion != 1U) {
				return github_read_validation_error::invalid_capability_revision;
			}
			if (
				!valid_enum(aEvidence.Capabilities.ReviewThreads)
				|| !valid_enum(aEvidence.Capabilities.ViewedFileMutations)
				|| !valid_enum(aEvidence.Capabilities.ResolveThread)
				|| !valid_enum(aEvidence.Capabilities.UnresolveThread)) {
				return github_read_validation_error::invalid_enum;
			}
			return github_read_validation_error::none;
		}

		[[nodiscard]] inline auto validate_rate_limit(
			const github_rate_limit_record& aRateLimit) noexcept
			-> github_read_validation_error
		{
			if (!valid(aRateLimit)) {
				return github_read_validation_error::invalid_rate_limit;
			}
			return github_read_validation_error::none;
		}

		[[nodiscard]] inline auto validate_rate_limits(
			const github_rate_limit_state& aRateLimits,
			const bool                     aRequireCore,
			const bool                     aRequireSearch,
			const bool                     aRequireGraphql) noexcept
			-> github_read_validation_error
		{
			if (
				!valid(aRateLimits)
				|| (aRequireCore && !aRateLimits.Core.has_value())
				|| (aRequireSearch && !aRateLimits.Search.has_value())
				|| (aRequireGraphql && !aRateLimits.Graphql.has_value())) {
				return github_read_validation_error::invalid_rate_limit;
			}
			return github_read_validation_error::none;
		}

		[[nodiscard]] inline auto validate_merge_observations(const github_pull_request_snapshot& aSnapshot, github_text_budget& aBudget, const github_read_limits& aLimits) -> github_read_validation_error
		{
			const auto available = [](github_observation_availability aValue) { return aValue >= github_observation_availability::unobserved && aValue <= github_observation_availability::failed; };
			if (!available(aSnapshot.ReviewRequestsAvailability) || !available(aSnapshot.Merge.Availability) || !available(aSnapshot.BranchPolicy.RulesAvailability) || !available(aSnapshot.BranchPolicy.ProtectionAvailability) || aSnapshot.Merge.Assessment > github_merge_assessment::unstable || aSnapshot.Merge.Review > github_review_decision::changes_requested) {
				return github_read_validation_error::invalid_enum;
			}
			const auto observed = github_observation_availability::available;
			if ((!aSnapshot.BranchPolicy.Rules.empty() && aSnapshot.BranchPolicy.RulesAvailability != observed && !aSnapshot.BranchPolicy.RulesStale) || (!aSnapshot.BranchPolicy.Protection.empty() && aSnapshot.BranchPolicy.ProtectionAvailability != observed && !aSnapshot.BranchPolicy.ProtectionStale) || (aSnapshot.Merge.Availability != observed && !aSnapshot.Merge.Stale && (aSnapshot.Merge.Assessment != github_merge_assessment::unknown || aSnapshot.Merge.Review != github_review_decision::unknown || aSnapshot.Merge.AutoMerge))) {
				return github_read_validation_error::invalid_relationship;
			}

			if (aSnapshot.ReviewRequests.size() > 1000U || aSnapshot.BranchPolicy.Rules.size() > 1000U || aSnapshot.BranchPolicy.Protection.size() > 1000U) {
				return github_read_validation_error::collection_too_large;
			}
			auto identities = std::unordered_set<std::string_view>{ }, actors = std::unordered_set<std::string_view>{ };
			for (const auto& request : aSnapshot.ReviewRequests) {
				if (request.Kind > github_requested_actor_kind::team || !identities.insert(request.Request).second || (!request.Actor.empty() && !actors.insert(request.Actor).second)) {
					return github_read_validation_error::duplicate_identity;
				}
				if (request.AsCodeOwner && aSnapshot.ReviewRequestsAvailability != observed && !aSnapshot.ReviewRequestsStale) {
					return github_read_validation_error::invalid_relationship;
				}
				if (request.Kind == github_requested_actor_kind::user) {
					const auto consistent = [&](const github_actor_record& aActor) { return request.Actor != aActor.Actor.value() || request.Login == aActor.Login; };
					if (!consistent(aSnapshot.Viewer) || (aSnapshot.PullRequest.Author && !consistent(*aSnapshot.PullRequest.Author)) || std::ranges::any_of(aSnapshot.Reviews, [&](const auto& aReview) { return aReview.Author && !consistent(*aReview.Author); })) {
						return github_read_validation_error::invalid_relationship;
					}
				}

				for (const auto& value : { std::string_view{ request.Request }, std::string_view{ request.Actor }, std::string_view{ request.Login }, std::string_view{ request.Organization }, std::string_view{ request.Name } }) {
					if (const auto error = aBudget.consume(value, aLimits.MaximumDisplayNameBytes); error != github_read_validation_error::none) {
						return error;
					}
				}
				if (request.Request.empty() || (request.Kind != github_requested_actor_kind::unavailable && (request.Actor.empty() || request.Login.empty())) || (request.Kind == github_requested_actor_kind::team && request.Organization.empty())) {
					return github_read_validation_error::missing_identity;
				}
			}
			auto requiredChecks = std::size_t{ 0 }, requiredTeams = std::size_t{ 0 };
			for (const auto* rules : { &aSnapshot.BranchPolicy.Rules, &aSnapshot.BranchPolicy.Protection }) {
				for (const auto& rule : *rules) {
					for (const auto& value : { std::string_view{ rule.Type }, std::string_view{ rule.Source }, std::string_view{ rule.SourceKind } }) {
						if (const auto error = aBudget.consume(value, aLimits.MaximumDisplayNameBytes, true); error != github_read_validation_error::none) {
							return error;
						}
					}
					if ((rule.ApprovalMinimum && *rule.ApprovalMinimum > 100U) || rule.Checks.size() > 100U || rule.RequiredTeams.size() > 100U) {
						return github_read_validation_error::collection_too_large;
					}
					requiredChecks += rule.Checks.size();
					requiredTeams += rule.RequiredTeams.size();
					if (requiredChecks > aLimits.MaximumChecks || requiredTeams > 1000U) {
						return github_read_validation_error::collection_too_large;
					}
					for (const auto& check : rule.Checks) {
						if (check.AppId && !*check.AppId) {
							return github_read_validation_error::missing_identity;
						}
						if (const auto error = aBudget.consume(check.Context, aLimits.MaximumCheckNameBytes, true); error != github_read_validation_error::none) {
							return error;
						}
					}
					for (const auto& [identity, minimum] : rule.RequiredTeams) {
						if (!identity || minimum > 100U) {
							return github_read_validation_error::invalid_relationship;
						}
					}
				}
			}
			return github_read_validation_error::none;
		}

		[[nodiscard]] inline auto validate_unique_identities(
			const github_pull_request_snapshot&         aSnapshot,
			std::optional<github_actor_login_conflict>& aConflict)
			-> github_read_validation_error
		{
			struct actor_observation
			{
				std::string_view                Login;
				github_actor_observation_origin Origin;
				github_actor_kind               Kind;
			};
			std::unordered_map<std::string_view, actor_observation> actors;
			actors.reserve(
				aSnapshot.Commits.size() + aSnapshot.Reviews.size()
				+ aSnapshot.ReviewThreads.size() + aSnapshot.IssueComments.size()
				+ 2U);
			actors.emplace(
				aSnapshot.Viewer.Actor.value(),
				actor_observation{ aSnapshot.Viewer.Login, github_actor_observation_origin::viewer, aSnapshot.Viewer.Kind });
			const auto observeActor =
				[&actors, &aConflict](
					const std::optional<github_actor_record>& aAuthor,
					const github_actor_observation_origin     aOrigin)
				-> github_read_validation_error {
				if (!aAuthor.has_value()) {
					return github_read_validation_error::none;
				}
				const auto [iterator, inserted] = actors.try_emplace(
					aAuthor->Actor.value(),
					actor_observation{ aAuthor->Login, aOrigin, aAuthor->Kind });
				if (inserted) {
					return github_read_validation_error::none;
				}
				// A bot login is only comparable with another observation typed
				// as a bot: the decoders' `[bot]` representation rule must not
				// let a differently typed or untyped observation match it.
				const auto& first = iterator->second;
				if (first.Login == aAuthor->Login && first.Kind != aAuthor->Kind
					&& (first.Kind == github_actor_kind::bot || aAuthor->Kind == github_actor_kind::bot)) {
					aConflict = github_actor_login_conflict{ first.Origin, aOrigin, false };
					return github_read_validation_error::relationship_actor_login;
				}
				if (first.Login != aAuthor->Login) {
					const auto foldAscii = [](const unsigned char aByte) {
						return aByte >= 'A' && aByte <= 'Z' ? static_cast<unsigned char>(aByte + ('a' - 'A')) : aByte;
					};
					const auto caseOnly = std::ranges::equal(iterator->second.Login, aAuthor->Login,
															 [&](const unsigned char aLeft, const unsigned char aRight) {
																 return aLeft < 128U && aRight < 128U && foldAscii(aLeft) == foldAscii(aRight);
															 });
					aConflict           = github_actor_login_conflict{ iterator->second.Origin, aOrigin, caseOnly };
					return github_read_validation_error::relationship_actor_login;
				}
				return github_read_validation_error::none;
			};
			if (const auto error = observeActor(aSnapshot.PullRequest.Author, github_actor_observation_origin::pull_request);
				error != github_read_validation_error::none) {
				return error;
			}
			for (const auto& commit : aSnapshot.Commits) {
				if (const auto error = observeActor(commit.Author, github_actor_observation_origin::commit);
					error != github_read_validation_error::none) {
					return error;
				}
			}
			for (const auto& review : aSnapshot.Reviews) {
				if (const auto error = observeActor(review.Author, github_actor_observation_origin::review);
					error != github_read_validation_error::none) {
					return error;
				}
			}
			for (const auto& thread : aSnapshot.ReviewThreads) {
				for (const auto& comment : thread.Comments) {
					if (const auto error = observeActor(comment.Author, github_actor_observation_origin::thread_comment);
						error != github_read_validation_error::none) {
						return error;
					}
				}
			}
			for (const auto& group : aSnapshot.ReviewCommentGroups) {
				if (const auto error = observeActor(group.Root.Author, github_actor_observation_origin::review_comment_root);
					error != github_read_validation_error::none) {
					return error;
				}
				for (const auto& reply : group.Replies) {
					if (const auto error = observeActor(reply.Author, github_actor_observation_origin::review_comment_reply);
						error != github_read_validation_error::none) {
						return error;
					}
				}
			}
			for (const auto& comment : aSnapshot.IssueComments) {
				if (const auto error = observeActor(comment.Author, github_actor_observation_origin::issue_comment);
					error != github_read_validation_error::none) {
					return error;
				}
			}

			std::unordered_set<std::string_view> entityNodeIds;
			entityNodeIds.reserve(
				aSnapshot.Commits.size() + aSnapshot.Reviews.size()
				+ aSnapshot.ReviewThreads.size() + aSnapshot.IssueComments.size()
				+ aSnapshot.Checks.size() + 2U);
			if (!entityNodeIds.emplace(aSnapshot.Repository.Node.value()).second
				|| !entityNodeIds.emplace(aSnapshot.PullRequest.Node.value()).second) {
				return github_read_validation_error::duplicate_identity;
			}
			for (const auto& commit : aSnapshot.Commits) {
				if (!entityNodeIds.emplace(commit.Node.value()).second) {
					return github_read_validation_error::duplicate_identity;
				}
			}

			std::unordered_set<std::string_view> paths;
			paths.reserve(aSnapshot.Files.size());
			for (const auto& file : aSnapshot.Files) {
				if (!paths.emplace(file.Path).second) {
					return github_read_validation_error::duplicate_identity;
				}
			}

			std::unordered_set<std::string_view> commitOids;
			commitOids.reserve(aSnapshot.Commits.size());
			for (const auto& commit : aSnapshot.Commits) {
				if (!commitOids.emplace(commit.Oid.value()).second) {
					return github_read_validation_error::duplicate_identity;
				}
			}

			for (const auto& review : aSnapshot.Reviews) {
				if (!entityNodeIds.emplace(review.Review.value()).second) {
					return github_read_validation_error::duplicate_identity;
				}
			}

			for (const auto& thread : aSnapshot.ReviewThreads) {
				if (!entityNodeIds.emplace(thread.Thread.value()).second) {
					return github_read_validation_error::duplicate_identity;
				}
				std::unordered_set<std::string_view> threadCommentIds;
				threadCommentIds.reserve(thread.Comments.size());
				for (const auto& comment : thread.Comments) {
					if (
						comment.ReplyTo.has_value()
						&& !threadCommentIds.contains(
							comment.ReplyTo->value())) {
						return github_read_validation_error::relationship_thread_reply;
					}
					if (!entityNodeIds.emplace(comment.Comment.value()).second) {
						return github_read_validation_error::duplicate_identity;
					}
					threadCommentIds.emplace(comment.Comment.value());
				}
			}
			for (const auto& group : aSnapshot.ReviewCommentGroups) {
				if (!entityNodeIds.emplace(group.Root.Comment.value()).second) {
					return github_read_validation_error::duplicate_identity;
				}
				for (const auto& reply : group.Replies) {
					if (!entityNodeIds.emplace(reply.Comment.value()).second) {
						return github_read_validation_error::duplicate_identity;
					}
				}
			}
			for (const auto& comment : aSnapshot.IssueComments) {
				if (!entityNodeIds.emplace(comment.Comment.value()).second) {
					return github_read_validation_error::duplicate_identity;
				}
			}

			for (const auto& check : aSnapshot.Checks) {
				if (!entityNodeIds.emplace(check.Check.value()).second) {
					return github_read_validation_error::duplicate_identity;
				}
			}
			for (const auto& actor : actors) {
				if (!entityNodeIds.emplace(actor.first).second) {
					return github_read_validation_error::duplicate_identity;
				}
			}

			std::unordered_set<std::string_view> searchPullRequests;
			std::unordered_set<std::uint32_t>    searchPullRequestNumbers;
			searchPullRequests.reserve(aSnapshot.SearchResults.size());
			searchPullRequestNumbers.reserve(aSnapshot.SearchResults.size());
			for (const auto& result : aSnapshot.SearchResults) {
				if (result.Repository != aSnapshot.Repository.Repository) {
					return github_read_validation_error::relationship_search_repository;
				}
				const auto matchesSnapshotNode   = result.PullRequestNode == aSnapshot.PullRequest.Node;
				const auto matchesSnapshotNumber = result.Number == aSnapshot.PullRequest.Number;
				if (matchesSnapshotNode != matchesSnapshotNumber) {
					return github_read_validation_error::relationship_search_target_identity;
				}
				if (
					matchesSnapshotNode
					&& (result.Title != aSnapshot.PullRequest.Title
						|| result.State != aSnapshot.PullRequest.State
						|| result.Draft != aSnapshot.PullRequest.Draft)) {
					return github_read_validation_error::relationship_search_target_metadata;
				}
				if (
					!matchesSnapshotNode
					&& !entityNodeIds
							.emplace(result.PullRequestNode.value())
							.second) {
					return github_read_validation_error::duplicate_identity;
				}
				if (
					!searchPullRequests
						 .emplace(result.PullRequestNode.value())
						 .second
					|| !searchPullRequestNumbers
							.emplace(result.Number.value())
							.second) {
					return github_read_validation_error::duplicate_identity;
				}
			}
			return github_read_validation_error::none;
		}
	}

	[[nodiscard]] inline auto validate(
		const github_session_read_model&            aModel,
		const core::github_session_request_context& aContext,
		const github_read_limits&                   aLimits = { }) noexcept
		-> github_read_validation
	{
		if (!detail::within_security_ceiling(aLimits)) {
			return detail::validation(
				github_read_validation_error::invalid_limits);
		}
		if (!detail::valid_context(aContext)) {
			return detail::validation(
				github_read_validation_error::invalid_context);
		}
		const auto anonymousMode = aModel.Mode
			== core::github_access_mode::public_preview;
		if (
			aModel.Profile.value() == 0U || aModel.Host.value() == 0U
			|| (!anonymousMode && aModel.Viewer.Account.value() == 0U)
			|| (anonymousMode && !anonymous(aModel.Viewer))) {
			return detail::validation(
				github_read_validation_error::missing_identity);
		}
		if (aModel.Profile != aContext.Profile) {
			return detail::validation(
				github_read_validation_error::identity_mismatch);
		}
		// Public preview probes nothing, so it may claim nothing. A shape
		// that claims a read the mode cannot perform is a malformed session
		// rather than a capability the host merely declines to use.
		if (anonymousMode && !reads_nothing(aModel.CapabilityEvidence.ReadShape)) {
			return detail::validation(github_read_validation_error::invalid_enum);
		}

		detail::github_text_budget budget{ aLimits };
		if (!anonymousMode) {
			if (const auto error = detail::validate_viewer(aModel.Viewer, budget, aLimits);
				error != github_read_validation_error::none) {
				return detail::validation(error);
			}
		}
		if (const auto error = detail::validate_capabilities(aModel.CapabilityEvidence);
			error != github_read_validation_error::none) {
			return detail::validation(error);
		}
		return detail::validation(
			detail::validate_rate_limits(
				aModel.RateLimits,
				false,
				false,
				false));
	}

	[[nodiscard]] inline auto validate(
		const github_pull_request_snapshot&       aSnapshot,
		const core::pull_request_request_context& aContext,
		const github_read_limits&                 aLimits = { })
		-> github_read_validation
	{
		if (!detail::within_security_ceiling(aLimits)) {
			return detail::validation(
				github_read_validation_error::invalid_limits);
		}
		if (!detail::valid_context(aContext)) {
			return detail::validation(
				github_read_validation_error::invalid_context);
		}
		const auto anonymousMode = aSnapshot.Mode
			== core::github_access_mode::public_preview;
		const auto readerAsExpected = anonymousMode
			? (aSnapshot.Account.value() == 0U
			   && aSnapshot.Viewer == github_actor_record{ })
			: aSnapshot.Account.value() != 0U;
		if (
			aSnapshot.Profile.value() == 0U || aSnapshot.Host.value() == 0U
			|| !readerAsExpected || aSnapshot.Snapshot.value() == 0U) {
			return detail::validation(
				github_read_validation_error::missing_identity);
		}
		if (
			aSnapshot.Profile != aContext.Profile
			|| aSnapshot.Host != aContext.Host
			|| aSnapshot.Account != aContext.Account
			|| aSnapshot.PullRequest.Number != aContext.Number
			|| aSnapshot.PullRequest.Repository
				!= aSnapshot.Repository.Repository
			|| !detail::ascii_equal_fold(
				aSnapshot.Repository.Owner,
				aContext.Repository.Owner)
			|| !detail::ascii_equal_fold(
				aSnapshot.Repository.Name,
				aContext.Repository.Name)
			|| (aContext.ExpectedRepository.has_value()
				&& *aContext.ExpectedRepository
					!= aSnapshot.Repository.Repository)
			|| (aContext.ExpectedPullRequest.has_value()
				&& *aContext.ExpectedPullRequest
					!= aSnapshot.PullRequest.PullRequest)
			|| aSnapshot.CapabilityEvidence.Revision
				!= aContext.Capabilities) {
			return detail::validation(
				github_read_validation_error::identity_mismatch);
		}
		if (const auto error = detail::validate_capabilities(aSnapshot.CapabilityEvidence);
			error != github_read_validation_error::none) {
			return detail::validation(error);
		}
		if (
			!detail::valid_enum(aSnapshot.ReviewThreadsAvailability)
			|| !detail::valid_enum(aSnapshot.ReviewCommentsAvailability)) {
			return detail::validation(
				github_read_validation_error::invalid_enum);
		}
		if (aSnapshot.TimelineAvailability != github_timeline_availability::unavailable
			&& aSnapshot.TimelineAvailability != github_timeline_availability::available
			&& aSnapshot.TimelineAvailability != github_timeline_availability::refused
			&& aSnapshot.TimelineAvailability != github_timeline_availability::capped
			&& aSnapshot.TimelineAvailability != github_timeline_availability::failed) {
			return detail::validation(github_read_validation_error::invalid_enum);
		}
		if (aSnapshot.SearchAvailability != github_search_availability::unavailable
			&& aSnapshot.SearchAvailability != github_search_availability::available
			&& aSnapshot.SearchAvailability != github_search_availability::result_limit) {
			return detail::validation(github_read_validation_error::invalid_enum);
		}
		if (aSnapshot.SearchAvailability != github_search_availability::available && !aSnapshot.SearchResults.empty()) {
			return detail::validation(github_read_validation_error::relationship_search_availability);
		}
		if (aSnapshot.TimelineAvailability != github_timeline_availability::available && !aSnapshot.ForcePushEvents.empty()) {
			return detail::validation(github_read_validation_error::relationship_timeline_availability);
		}
		if (aSnapshot.ForcePushEvents.size() > 1000U) {
			return detail::validation(github_read_validation_error::collection_too_large);
		}
		std::unordered_set<std::uint64_t> timelineIdentities;
		for (const auto& event : aSnapshot.ForcePushEvents) {
			if (event.EventId == 0U || !detail::hexadecimal_oid(event.AfterHeadOid) || event.CreatedAtUnixMilliseconds < 0) {
				return detail::validation(github_read_validation_error::relationship_timeline_event);
			}
			if (!timelineIdentities.insert(event.EventId).second) {
				return detail::validation(github_read_validation_error::duplicate_identity);
			}
		}
		// An unavailable conversation was never read, so it can hold nothing.
		// An observed one may legitimately hold nothing: that is the
		// distinction the pair exists to keep, and the only rule the pair
		// needs is that the unavailable side stays empty.
		if (
			aSnapshot.ReviewCommentsAvailability
				== github_collection_availability::unavailable
			&& !aSnapshot.ReviewCommentGroups.empty()) {
			return detail::validation(
				github_read_validation_error::relationship_review_comments_availability);
		}
		const auto& readShape        = aSnapshot.CapabilityEvidence.ReadShape;
		const auto  threadsAvailable = aSnapshot.ReviewThreadsAvailability
			== github_collection_availability::available;
		if (
			threadsAvailable != readShape.ReviewThreads
			|| (!threadsAvailable
				&& !aSnapshot.ReviewThreads.empty())
			|| (aSnapshot.CapabilityEvidence.Capabilities.ReviewThreads
					== core::github_capability_state::supported
				&& !readShape.ReviewThreads)
			|| (aSnapshot.CapabilityEvidence.Capabilities.ReviewThreads
					!= core::github_capability_state::supported
				&& readShape.ReviewThreads)
			|| (aSnapshot.CapabilityEvidence.Capabilities.ViewedFileMutations
					== core::github_capability_state::supported
				&& !readShape.ViewedFileState)
			|| (aSnapshot.CapabilityEvidence.Capabilities.ResolveThread
					== core::github_capability_state::supported
				&& (!readShape.ReviewThreads
					|| !readShape.ResolvePermission))
			|| (aSnapshot.CapabilityEvidence.Capabilities.UnresolveThread
					== core::github_capability_state::supported
				&& (!readShape.ReviewThreads
					|| !readShape.UnresolvePermission))) {
			return detail::validation(
				github_read_validation_error::relationship_review_threads_capability);
		}
		for (const auto& file : aSnapshot.Files) {
			const auto viewedAvailable = file.ViewedState != github_file_viewed_state::unavailable;
			if (viewedAvailable != readShape.ViewedFileState) {
				return detail::validation(
					github_read_validation_error::relationship_file_viewed_capability);
			}
		}
		for (const auto& thread : aSnapshot.ReviewThreads) {
			const auto resolveAvailable = thread.ViewerResolvePermission
				!= github_permission_state::unavailable;
			const auto unresolveAvailable = thread.ViewerUnresolvePermission
				!= github_permission_state::unavailable;
			if (
				resolveAvailable != readShape.ResolvePermission
				|| unresolveAvailable != readShape.UnresolvePermission) {
				return detail::validation(
					github_read_validation_error::relationship_thread_permission_capability);
			}
		}
		if (aSnapshot.Files.size() > aLimits.MaximumFiles
			|| aSnapshot.Commits.size() > aLimits.MaximumCommits
			|| aSnapshot.Reviews.size() > aLimits.MaximumReviews
			|| aSnapshot.ReviewThreads.size() > aLimits.MaximumReviewThreads
			|| aSnapshot.ReviewCommentGroups.size()
				> aLimits.MaximumReviewThreads
			|| aSnapshot.IssueComments.size() > aLimits.MaximumIssueComments
			|| aSnapshot.Checks.size() > aLimits.MaximumChecks
			|| aSnapshot.SearchResults.size() > aLimits.MaximumSearchResults) {
			return detail::validation(
				github_read_validation_error::collection_too_large);
		}
		if (aSnapshot.PullRequest.ReportedCommitCount.has_value()
			&& std::min(*aSnapshot.PullRequest.ReportedCommitCount,
						maximum_github_pull_request_endpoint_commits)
				!= aSnapshot.Commits.size()) {
			return detail::validation(
				github_read_validation_error::invalid_relationship);
		}

		auto totalComments = aSnapshot.IssueComments.size();
		if (totalComments > aLimits.MaximumTotalComments) {
			return detail::validation(
				github_read_validation_error::collection_too_large);
		}
		for (const auto& thread : aSnapshot.ReviewThreads) {
			if (
				thread.Comments.size()
				> aLimits.MaximumTotalComments - totalComments) {
				return detail::validation(
					github_read_validation_error::collection_too_large);
			}
			totalComments += thread.Comments.size();
		}
		for (const auto& group : aSnapshot.ReviewCommentGroups) {
			if (
				group.Replies.size()
				>= aLimits.MaximumTotalComments - totalComments) {
				return detail::validation(
					github_read_validation_error::collection_too_large);
			}
			totalComments += group.Replies.size() + 1U;
		}

		auto       totalRecords = aSnapshot.Files.size();
		const auto addRecords   = [&](const std::size_t aCount) {
			if (
				aCount > aLimits.MaximumTotalRecords
				|| totalRecords > aLimits.MaximumTotalRecords - aCount) {
				return false;
			}
			totalRecords += aCount;
			return true;
		};
		if (
			!addRecords(aSnapshot.ReviewRequests.size())
			|| !addRecords(aSnapshot.BranchPolicy.Rules.size())
			|| !addRecords(aSnapshot.BranchPolicy.Protection.size())
			|| !addRecords(aSnapshot.Commits.size())
			|| !addRecords(aSnapshot.Reviews.size())
			|| !addRecords(aSnapshot.ReviewThreads.size())
			|| !addRecords(aSnapshot.ReviewCommentGroups.size())
			|| !addRecords(totalComments)
			|| !addRecords(aSnapshot.Checks.size())
			|| !addRecords(aSnapshot.SearchResults.size())) {
			return detail::validation(
				github_read_validation_error::collection_too_large);
		}

		detail::github_text_budget budget{ aLimits };
		if (!anonymousMode) {
			if (const auto error = detail::validate_actor(
					aSnapshot.Viewer,
					budget,
					aLimits,
					true);
				error != github_read_validation_error::none) {
				return detail::validation(error);
			}
		}
		if (const auto error = detail::validate_repository(
				aSnapshot.Repository,
				budget,
				aLimits);
			error != github_read_validation_error::none) {
			return detail::validation(error);
		}
		if (const auto error = detail::validate_pull_request(
				aSnapshot.PullRequest,
				budget,
				aLimits);
			error != github_read_validation_error::none) {
			return detail::validation(error);
		}
		if (const auto error = detail::validate_merge_observations(aSnapshot, budget, aLimits); error != github_read_validation_error::none) {
			return detail::validation(error);
		}
		for (const auto& record : aSnapshot.Files) {
			if (const auto error = detail::validate_file(record, budget, aLimits);
				error != github_read_validation_error::none) {
				return detail::validation(error);
			}
		}
		for (const auto& record : aSnapshot.Commits) {
			if (const auto error = detail::validate_commit(record, budget, aLimits);
				error != github_read_validation_error::none) {
				return detail::validation(error);
			}
		}
		for (const auto& record : aSnapshot.Reviews) {
			if (const auto error = detail::validate_review(record, budget, aLimits);
				error != github_read_validation_error::none) {
				return detail::validation(error);
			}
		}
		for (const auto& record : aSnapshot.ReviewThreads) {
			if (const auto error = detail::validate_thread(record, budget, aLimits);
				error != github_read_validation_error::none) {
				return detail::validation(error);
			}
		}
		for (const auto& record : aSnapshot.ReviewCommentGroups) {
			if (const auto error = detail::validate_review_comment_group(record, budget, aLimits);
				error != github_read_validation_error::none) {
				return detail::validation(error);
			}
		}
		for (const auto& record : aSnapshot.IssueComments) {
			if (const auto error = detail::validate_issue_comment(record, budget, aLimits);
				error != github_read_validation_error::none) {
				return detail::validation(error);
			}
		}
		for (const auto& record : aSnapshot.Checks) {
			if (!record.EvaluatedOid.value().empty() && record.EvaluatedOid != aSnapshot.PullRequest.HeadOid) {
				return detail::validation(github_read_validation_error::identity_mismatch);
			}
			if (const auto error = detail::validate_check(record, budget, aLimits);
				error != github_read_validation_error::none) {
				return detail::validation(error);
			}
		}
		for (const auto& record : aSnapshot.SearchResults) {
			if (const auto error = detail::validate_search_result(record, budget, aLimits);
				error != github_read_validation_error::none) {
				return detail::validation(error);
			}
		}
		try {
			std::optional<github_actor_login_conflict> actorConflict;
			if (const auto error = detail::validate_unique_identities(aSnapshot, actorConflict);
				error != github_read_validation_error::none) {
				auto result          = detail::validation(error);
				result.ActorConflict = actorConflict;
				return result;
			}
		} catch (const std::bad_alloc&) {
			return detail::validation(
				github_read_validation_error::allocation_limit);
		}
		return detail::validation(
			detail::validate_rate_limits(
				aSnapshot.RateLimits,
				false,
				false,
				false));
	}
}

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_GITHUB_READ_MODEL_HPP
