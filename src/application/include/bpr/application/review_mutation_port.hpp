#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MUTATION_PORT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MUTATION_PORT_HPP

#include "bpr/application/github_host_authority.hpp"
#include "bpr/application/private_review_store.hpp"
#include "bpr/application/review_synchronization.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/github_model.hpp"
#include "bpr/core/review_publication_policy.hpp"
#include "bpr/core/review_round_trip.hpp"

#include <atomic>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::application {
	/// \brief Names why a mutation could not be performed.
	enum class review_mutation_failure : std::uint8_t {
		unavailable,
		refused_by_server,
		undesignated_target,
		target_moved,
		stale_head,
		unsupported,
		cancelled,
		/// A valid response could not expose the selected target to this account.
		target_unavailable,
		/// The server response could not be decoded under the operation contract.
		malformed_response,
		incompatible_body,
		credential_unavailable,
		/// The credential tool failed before any publication request.
		credential_tool_failed,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(review_mutation_failure aFailure) noexcept
		-> std::string_view;

	/// \brief Reports what a provider can do natively.
	///
	/// \details A provider that cannot resolve threads or record viewed state
	/// must say so, so the degradation is reported rather than silently
	/// skipped. Silently skipping would leave a reviewer believing they had
	/// resolved something.
	struct review_provider_capabilities
	{
		bool ResolvesThreads    = true;
		bool RecordsViewedState = true;

		auto operator<=>(const review_provider_capabilities&) const = default;
	};

	/// \brief Binds one mutation to the complete accepted pull-request identity.
	///
	/// \details Provider database and node identities remain opaque application
	/// values. They accompany the core target so a GitHub adapter can reject a
	/// renamed, replaced, closed, or otherwise mismatched pull request without
	/// exposing provider response records inward.
	struct review_mutation_target_binding
	{
		core::review_mutation_target Semantic;
		github_canonical_hostname    Hostname;
		github_server_contract       ServerContract = github_server_contract::github_com_2022_11_28;
		core::repository_locator     Repository;
		core::pull_request_number    Number;
		std::uint64_t                RepositoryDatabaseId = 0U;
		std::string                  RepositoryNodeId;
		std::uint64_t                PullRequestDatabaseId = 0U;
		std::string                  PullRequestNodeId;
		std::uint64_t                BaseRepositoryDatabaseId = 0U;
		core::git_commit_oid         BaseOid;
		std::string                  BaseRef;
		std::string                  HeadRef;
		bool                         Open = false;
		/// [[BPR-1560]]. The commit a LEFT line is numbered in: the merge base
		/// GitHub's Files changed compares from. `BaseOid` stays the base
		/// GitHub records for the pull request, which the publication checks
		/// it against. Unset in a binding written before the two could differ,
		/// where `BaseOid` is both.
		core::git_commit_oid ComparisonBaseOid;

		auto operator<=>(const review_mutation_target_binding&) const = default;
	};

	/// \brief [[BPR-1560]]. The `ComparisonBaseOid` a binding keeps: none when
	/// the comparison base is the recorded base, so one binding has one
	/// spelling and survives a journal round trip unchanged.
	[[nodiscard]] inline auto distinct_comparison_base(const core::git_commit_oid& aRecordedBase, const core::git_commit_oid& aComparisonBase) noexcept
		-> core::git_commit_oid
	{
		return aComparisonBase == aRecordedBase ? core::git_commit_oid{ } : aComparisonBase;
	}

	/// \brief [[BPR-1560]]. The commit a LEFT line of this target is numbered in.
	[[nodiscard]] inline auto comparison_base(const review_mutation_target_binding& aTarget) noexcept
		-> const core::git_commit_oid&
	{
		return aTarget.ComparisonBaseOid.valid() ? aTarget.ComparisonBaseOid : aTarget.BaseOid;
	}

	/// \brief Opaque lease for one accepted review-mutation authority.
	///
	/// \details The application host may retain and expose this capability, but
	/// only the GitHub adapter can identify and unwrap its concrete transport.
	/// It contains no credential bytes and exposes no provider or Qt network
	/// type across the application boundary. Every retained shared pointer sees
	/// the same monotonic revocation state. Revocation is an admission check, not
	/// asynchronous transport cancellation.
	class review_mutation_transport_lease
	{
	public:
		review_mutation_transport_lease()          = default;
		virtual ~review_mutation_transport_lease() = default;

		review_mutation_transport_lease(const review_mutation_transport_lease&) = delete;
		auto operator=(const review_mutation_transport_lease&)
			-> review_mutation_transport_lease&                            = delete;
		review_mutation_transport_lease(review_mutation_transport_lease&&) = delete;
		auto operator=(review_mutation_transport_lease&&)
			-> review_mutation_transport_lease& = delete;

		/// \brief Reports whether this exact accepted-snapshot authority remains live.
		[[nodiscard]] auto active() const noexcept -> bool
		{
			return Active.load(std::memory_order_acquire);
		}

		/// \brief Permanently refuses future mutation use through every retained copy.
		///
		/// \details This denial-only operation neither stops nor waits for an
		/// already-running effect. The later asynchronous host owns cancellation.
		void revoke() noexcept
		{
			Active.store(false, std::memory_order_release);
		}

	private:
		std::atomic_bool Active = true;
	};

	using review_mutation_transport = std::shared_ptr<review_mutation_transport_lease>;

	/// \brief Mints one opaque mutation lease for the host's accepted snapshot.
	class review_mutation_transport_factory_port
	{
	public:
		review_mutation_transport_factory_port()          = default;
		virtual ~review_mutation_transport_factory_port() = default;

		review_mutation_transport_factory_port(
			const review_mutation_transport_factory_port&) = delete;
		auto operator=(const review_mutation_transport_factory_port&)
			-> review_mutation_transport_factory_port& = delete;
		review_mutation_transport_factory_port(
			review_mutation_transport_factory_port&&) = delete;
		auto operator=(review_mutation_transport_factory_port&&)
			-> review_mutation_transport_factory_port& = delete;

		[[nodiscard]] virtual auto acquire_review_mutation_transport() noexcept
			-> review_mutation_transport = 0;
	};

	/// \brief Binds the authenticated application account to GitHub identity.
	struct review_mutation_acting_identity
	{
		core::github_account_id Account;
		std::uint64_t           DatabaseId = 0U;
		std::string             NodeId;
		std::string             Login;

		auto operator<=>(const review_mutation_acting_identity&) const = default;
	};

	/// \brief Resolves a semantic source line to GitHub review-comment fields.
	///
	/// \details `Semantic` must equal the proposal anchor. `Commit` is the
	/// exact commit whose `Path` and `Side` were accepted by the host. `Semantic.Line`
	/// remains the core's zero-based line index; provider contracts translate it
	/// to their wire representation.
	struct review_mutation_anchor_binding
	{
		core::source_line_ref Semantic;
		std::string           Path;
		core::git_commit_oid  Commit;
		/// RFC-BPR-0089 M4. The zero-based line GitHub is told, on
		/// `Semantic.Side`, for a line of a commit-filtered diff the host placed
		/// (`core::place_filtered_comment`): the head's line, or, on the right,
		/// the line of `Commit`, an earlier commit of the accepted first-parent
		/// chain. Absent for every other comment, whose GitHub line is
		/// `Semantic.Line` at the head.
		std::optional<std::uint32_t> RemoteLine;

		auto operator<=>(const review_mutation_anchor_binding&) const = default;
	};

	/// \brief RFC-BPR-0089 M4. The zero-based line GitHub is told.
	[[nodiscard]] constexpr auto remote_line(const review_mutation_anchor_binding& aAnchor) noexcept -> std::uint32_t
	{
		return aAnchor.RemoteLine.value_or(aAnchor.Semantic.Line);
	}

	/// \brief RFC-BPR-0089 M4. Whether a comment may be written on the
	/// anchor's commit: the head, or an earlier commit only for a right-side
	/// line the host placed from a commit-filtered diff. The host admits that
	/// placement only for a commit of the accepted pull request's first-parent
	/// chain, before anything is written.
	[[nodiscard]] inline auto admitted_anchor_commit(const review_mutation_anchor_binding& aAnchor, const core::git_commit_oid& aHead) noexcept
		-> bool
	{
		return aAnchor.Commit == aHead || (aAnchor.RemoteLine.has_value() && aAnchor.Semantic.Side == core::diff_side::right);
	}

	/// \brief Carries the provider-free authority required by a live request.
	struct review_mutation_live_binding
	{
		review_mutation_target_binding  Target;
		review_mutation_acting_identity ActingIdentity;
		review_mutation_anchor_binding  Anchor;

		auto operator<=>(const review_mutation_live_binding&) const = default;
	};

	/// \brief Checks the complete target binding contract.
	[[nodiscard]] auto valid(
		const review_mutation_target_binding& aBinding) noexcept -> bool;
	/// \brief Checks the complete acting-identity binding contract.
	[[nodiscard]] auto valid(
		const review_mutation_acting_identity& aIdentity) noexcept -> bool;
	/// \brief Checks the source-line binding contract.
	[[nodiscard]] auto valid(
		const review_mutation_anchor_binding& aAnchor) noexcept -> bool;
	/// \brief Checks the complete live binding contract.
	[[nodiscard]] auto valid(
		const review_mutation_live_binding& aBinding) noexcept -> bool;

	/// \brief Exact retained material accepted by the host before publication.
	struct review_mutation_source_binding
	{
		std::optional<workbench_selected_diff_endpoint> Endpoint;
		core::file_comparison_key                       Comparison;
		std::uint32_t                                   LineCount                                                = 0U;
		core::review_placement                          Placement                                                = core::review_placement::unknown;
		auto                                            operator<=>(const review_mutation_source_binding&) const = default;
	};

	enum class review_remote_comment_subject : std::uint8_t { line,
															  file };

	/// \brief Carries one publication to the provider.
	///
	/// \details `IdempotencyKey` is what makes a retry safe. A request whose
	/// answer was lost is indistinguishable from one that never arrived, so the
	/// only way to retry without risking a second comment is for the provider
	/// to recognise the repeat.
	struct review_mutation_request
	{
		core::review_publication_proposal             Proposal;
		core::review_mutation_target                  Target;
		core::github_account_id                       ActingIdentity;
		std::string                                   IdempotencyKey;
		std::optional<review_mutation_live_binding>   LiveBinding;
		std::optional<review_mutation_source_binding> Source;
		core::review_publication_representation       Representation = core::review_publication_representation::native_line;

		auto operator<=>(const review_mutation_request&) const = default;
	};

	/// \brief Reports what the provider did.
	struct review_mutation_receipt
	{
		std::string                             ServerCommentId;
		bool                                    Duplicate      = false;
		core::review_publication_representation Representation = core::review_publication_representation::native_line;

		auto operator<=>(const review_mutation_receipt&) const = default;
	};

	enum class review_lineage_state : std::uint8_t { absent,
													 pending,
													 recorded };
	/// \brief Local correlation only. A refusal never grants or retries a remote write.
	class review_publication_lineage_port
	{
	public:
		virtual ~review_publication_lineage_port() = default;
		/// Called before recovery-journal admission/transmission on the serialized effect lane.
		[[nodiscard]] virtual auto prepare(const review_mutation_request&, std::string_view aLogical, std::string_view aWireBody) -> bool = 0;
		/// Called only after receipt plus exact actor/body/target reconciliation.
		[[nodiscard]] virtual auto complete(const review_mutation_request&, std::string_view aLogical, std::string_view aWireBody, const review_mutation_receipt&) -> bool = 0;
		/// Pending rows are read on the serialized mutation lane. No row grants a write.
		[[nodiscard]] virtual auto pending(const core::review_mutation_target&, core::github_account_id) -> std::vector<private_publication_record> { return { }; }
		/// Records only a separately observed exact server comment.
		[[nodiscard]] virtual auto complete_observed(const private_publication_record&, std::string_view, const review_mutation_receipt&) -> bool { return false; }
	};
	/// \brief Carries both the write outcome and its authoritative server refresh.
	struct review_mutation_submission
	{
		std::expected<review_mutation_receipt, review_mutation_failure> Result{
			std::unexpected(review_mutation_failure::unavailable)
		};
		std::expected<
			std::vector<review_comment_projection>,
			review_mutation_failure>
							 Reconciliation{
								 std::unexpected(review_mutation_failure::unavailable)
							 };
		bool                 MutationAttempted = false;
		review_lineage_state Lineage           = review_lineage_state::absent;

		auto operator==(const review_mutation_submission&) const -> bool = default;
	};

	/// \brief The GitHub mutation surface, as the host sees it.
	/// \brief Asks the provider to resolve or unresolve one review thread.
	///
	/// \details [[BPR-0657]]. `Thread` is the provider's opaque thread
	/// identity as the read side reported it. Bound to the accepted target
	/// and the acting identity like a publication, so a provider serving
	/// another pull request or another account refuses it.
	struct review_thread_resolution_request
	{
		core::review_mutation_target                Target;
		core::github_account_id                     ActingIdentity;
		std::string                                 Thread;
		bool                                        Resolve = true;
		std::optional<review_mutation_live_binding> LiveBinding;

		auto operator<=>(const review_thread_resolution_request&) const = default;
	};

	/// \brief The provider's answer: the thread and the state it now has.
	struct review_thread_resolution_receipt
	{
		std::string Thread;
		bool        Resolved = false;

		auto operator<=>(const review_thread_resolution_receipt&) const = default;
	};

	class review_mutation_port
	{
	public:
		review_mutation_port()                                               = default;
		review_mutation_port(const review_mutation_port&)                    = delete;
		auto operator=(const review_mutation_port&) -> review_mutation_port& = delete;
		review_mutation_port(review_mutation_port&&)                         = delete;
		auto operator=(review_mutation_port&&) -> review_mutation_port&      = delete;
		virtual ~review_mutation_port()                                      = default;

		/// \brief Reads the target as the server currently reports it.
		[[nodiscard]] virtual auto observe_target()
			-> std::expected<core::review_mutation_target, review_mutation_failure>
			= 0;

		/// \brief Reads the target while honouring one caller-owned stop token.
		///
		/// \details The compatibility default keeps bounded fixture providers
		/// source-compatible. Production providers override this path so the same
		/// token reaches every observation and the native transport.
		[[nodiscard]] virtual auto observe_target(std::stop_token aStop)
			-> std::expected<core::review_mutation_target, review_mutation_failure>
		{
			if (aStop.stop_requested()) {
				return std::unexpected(review_mutation_failure::cancelled);
			}
			return observe_target();
		}

		/// \brief Submits one publication.
		[[nodiscard]] virtual auto submit(const review_mutation_request& aRequest)
			-> std::expected<review_mutation_receipt, review_mutation_failure> = 0;

		/// \brief Submits one publication while honouring caller cancellation.
		[[nodiscard]] virtual auto submit(
			const review_mutation_request& aRequest,
			std::stop_token                aStop)
			-> std::expected<review_mutation_receipt, review_mutation_failure>
		{
			if (aStop.stop_requested()) {
				return std::unexpected(review_mutation_failure::cancelled);
			}
			return submit(aRequest);
		}

		/// \brief Submits and returns the server refresh performed after the attempt.
		///
		/// \details Fixture providers keep the compatibility path. The production
		/// provider overrides this operation so a POST and its reconciliation are
		/// one serialized effect and cannot be separated by a terminal callback.
		[[nodiscard]] virtual auto submit_and_reconcile(
			const review_mutation_request& aRequest,
			std::stop_token                aStop)
			-> review_mutation_submission
		{
			return {
				.Result            = submit(aRequest, aStop),
				.MutationAttempted = true,
			};
		}

		/// \brief [[BPR-0657]]. Resolves or unresolves one thread; a provider
		/// without the capability answers `unavailable`.
		[[nodiscard]] virtual auto resolve_thread(
			const review_thread_resolution_request& aRequest,
			std::stop_token                         aStop)
			-> std::expected<review_thread_resolution_receipt, review_mutation_failure>
		{
			static_cast<void>(aRequest);
			static_cast<void>(aStop);
			return std::unexpected(review_mutation_failure::unavailable);
		}
		/// \brief Repairs a previously admitted logical attempt using only reads; never submits.
		[[nodiscard]] virtual auto reconcile_publication(const review_mutation_request&, std::stop_token = { }) -> review_mutation_submission { return { }; }
		/// One read-only pass over retained private publication joins.
		virtual void               repair_pending_publications(std::stop_token = { }) { }
		[[nodiscard]] virtual auto capabilities() const
			-> review_provider_capabilities = 0;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MUTATION_PORT_HPP
