#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MUTATION_APPLICATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MUTATION_APPLICATION_HOST_HPP

#include "bpr/application/insights_observer.hpp"
#include "bpr/application/port.hpp"
#include "bpr/application/review_mutation_coordinator.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/filtered_comment.hpp"
#include "bpr/core/review_publication_policy.hpp"

#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>

namespace bpr::application {

	/// \brief Counts accepted text lines under one exact materialized comparison.
	struct review_mutation_source_material
	{
		core::file_comparison_key    Key;
		std::optional<std::uint32_t> LeftLines;
		std::optional<std::uint32_t> RightLines;
		/// [[BPR-1212]]. The left document is the merge base's version of the
		/// file, so its line numbers are GitHub's `LEFT` lines
		/// (`resolve_github_base_numbering` answered `same` for this key).
		bool BaseNumberedAsGitHub = false;
	};

	/// \brief Host-owned source and changed-file facts for one publication admission.
	/// \details Authority includes the accepted provider snapshot. Material comes from
	/// the diff host, and File from the accepted changed-file catalogue, never Qt.
	/// Placement remains unknown unless complete exact provider evidence exists.
	struct review_mutation_source_observation
	{
		workbench_pull_request_authority Authority;
		workbench_file_observation_entry File;
		review_mutation_target_binding   Target;
		review_mutation_acting_identity  ActingIdentity;
		review_mutation_source_material  Material;
		core::review_placement           Placement = core::review_placement::unknown;
		/// RFC-BPR-0089 M4. For a commit-filtered diff: the facts the core
		/// places a comment on it from (owner decision Q1), from the accepted
		/// commit list and the lines the epoch host read. Absent otherwise.
		std::optional<core::filtered_comment_source> Filtered;
		/// [[BPR-1735]]. For the Whole PR diff, once the merge base is known:
		/// the head, the merge base and, once the epoch host read and compared
		/// the file at both, the merge base to the head, which the core places
		/// a new comment by. Absent on a filtered diff.
		std::optional<core::whole_diff_comment_source> WholeDiff;
		/// [[BPR-1736]]. What the profile has learned about a single comment
		/// outside GitHub's diff on this server and repository, as presented:
		/// supported places such a line natively, unsupported as a file comment
		/// at once, unknown makes one native attempt. Publication checks it
		/// again by the exact key.
		core::off_hunk_creation_capability OffHunkFact = core::off_hunk_creation_capability::unknown;
	};

	/// \brief Reads current bounded source facts on the host's decision lane without effects.
	class review_mutation_source_port
	{
	public:
		virtual ~review_mutation_source_port() = default;
		[[nodiscard]] virtual auto observe_review_source(core::workbench_file_id aFile) const
			-> std::optional<review_mutation_source_observation> = 0;
	};

	/// \brief Exact workbench state bound to one review publication attempt.
	struct review_mutation_publication_request
	{
		core::review_publication_proposal   Proposal;
		core::semantic_selection            Selection;
		workbench_selected_diff_publication SelectedDiff;
		bool                                Acknowledged = false;

		auto operator<=>(const review_mutation_publication_request&) const
			= default;
	};

	/// \brief Stable identity assigned to one asynchronous mutation request.
	struct review_mutation_request_context
	{
		core::request_id             Request;
		core::application_session_id Session;
		core::request_generation     Generation;

		auto operator<=>(const review_mutation_request_context&) const = default;
	};

	/// \brief One exactly-once terminal selected by the mutation host.
	struct review_mutation_terminal
	{
		review_mutation_request_context                                 Context;
		std::expected<review_mutation_receipt, review_mutation_failure> Result;
		std::expected<
			std::vector<review_comment_projection>,
			review_mutation_failure>
							 Reconciliation{
								 std::unexpected(review_mutation_failure::unavailable)
							 };
		bool                 MutationAttempted = false;
		review_lineage_state Lineage           = review_lineage_state::absent;

		auto operator==(const review_mutation_terminal&) const -> bool = default;
	};

	/// \brief [[BPR-0657]]. Asks the host to resolve or unresolve one thread.
	struct review_thread_resolution_publication_request
	{
		std::string Thread;
		bool        Resolve      = true;
		bool        Acknowledged = false;

		auto operator<=>(const review_thread_resolution_publication_request&) const = default;
	};

	/// \brief [[BPR-0657]]. One thread resolution's terminal.
	struct review_thread_resolution_terminal
	{
		review_mutation_request_context                                          Context;
		review_thread_resolution_publication_request                             Request;
		std::expected<review_thread_resolution_receipt, review_mutation_failure> Result{
			std::unexpected(review_mutation_failure::unavailable)
		};

		auto operator==(const review_thread_resolution_terminal&) const -> bool = default;
	};

	/// \brief [[BPR-PENDING-AR1]]. Asks the host to add or remove the viewer's
	/// reaction on one comment or review, named by its GraphQL node.
	struct review_reaction_publication_request
	{
		std::string            Subject;
		core::reaction_content Content = core::reaction_content::thumbs_up;
		bool                   Add     = true;

		auto operator<=>(const review_reaction_publication_request&) const = default;
	};

	/// \brief [[BPR-PENDING-AR1]]. One reaction change's terminal.
	struct review_reaction_terminal
	{
		review_mutation_request_context                                 Context;
		review_reaction_publication_request                             Request;
		std::expected<review_reaction_receipt, review_mutation_failure> Result{
			std::unexpected(review_mutation_failure::unavailable)
		};

		auto operator==(const review_reaction_terminal&) const -> bool = default;
	};

	class review_mutation_projection_port
	{
	public:
		virtual ~review_mutation_projection_port() = default;

		virtual void publish(const review_mutation_terminal& aTerminal) = 0;
		/// \brief [[BPR-0657]]. A sink that does not present resolutions ignores
		/// them; named apart from `publish` so no sink's overload hides it.
		virtual void publish_resolution(const review_thread_resolution_terminal& aTerminal)
		{
			static_cast<void>(aTerminal);
		}
		/// \brief [[BPR-PENDING-AR1]]. A sink that does not present reactions
		/// ignores them.
		virtual void publish_reaction(const review_reaction_terminal& aTerminal)
		{
			static_cast<void>(aTerminal);
		}
	};

	/// \brief [[BPR-0657]]. Host-validated scope of one thread resolution:
	/// the session, generation, target and actor a publication is bound to,
	/// and the thread, with no source line to anchor.
	struct review_mutation_thread_scope
	{
		core::application_session_id    ApplicationSession;
		core::request_generation        WorkbenchGeneration;
		review_mutation_target_binding  Target;
		review_mutation_acting_identity ActingIdentity;
		std::string                     Thread;

		auto operator<=>(const review_mutation_thread_scope&) const = default;
	};

	/// \brief Host-validated scope presented to an opaque execution authority.
	struct review_mutation_execution_scope
	{
		core::application_session_id    ApplicationSession;
		core::request_generation        WorkbenchGeneration;
		review_mutation_target_binding  Target;
		review_mutation_acting_identity ActingIdentity;
		core::file_comparison_digest    Comparison;
		core::file_identity             File;
		core::source_line_ref           Anchor;
		std::string                     ResolvedPath;
		core::git_commit_oid            Commit;

		auto operator<=>(const review_mutation_execution_scope&) const = default;
	};

	/// \brief Opaque capability required before a mutation effect can be queued.
	///
	/// \details Production construction is private to the attended bootstrap or
	/// the normal user-publish issuer. Normal grants bind one complete request
	/// to an exact session, generation, target and actor and expire after five
	/// minutes. The host consumes a grant only after validating retained endpoints.
	class review_mutation_execution_authority;
	using review_mutation_execution_grant
		= std::shared_ptr<review_mutation_execution_authority>;

#if defined(BPR_APPLICATION_TESTING) || defined(BPR_BUILD_TESTING)
	/// \brief Mints the bounded execution authority used only by fixture builds.
	[[nodiscard]] auto make_review_mutation_testing_grant(
		core::application_session_id                       aSession,
		core::request_generation                           aGeneration,
		review_mutation_target_binding                     aTarget,
		review_mutation_acting_identity                    aActingIdentity,
		std::optional<review_mutation_publication_request> aRequest = std::nullopt)
		-> review_mutation_execution_grant;

	/// \brief Mints the production-shaped one-shot carrier for host unit tests.
	[[nodiscard]] auto make_review_mutation_one_shot_testing_grant(
		review_mutation_target_binding        aTarget,
		std::chrono::steady_clock::time_point aExpiresAt)
		-> review_mutation_execution_grant;
#endif

	struct review_mutation_application_host_statistics
	{
		std::uint64_t Attempts                = 0U;
		std::uint64_t EffectsQueued           = 0U;
		std::uint64_t Completions             = 0U;
		std::uint64_t TerminalsPublished      = 0U;
		std::uint64_t RefusedBeforeEffect     = 0U;
		std::uint64_t Superseded              = 0U;
		std::uint64_t StaleTerminalsRejected  = 0U;
		std::uint64_t ShutdownRequestsStopped = 0U;
		/// [[BPR-0657]].
		std::uint64_t ThreadResolutions = 0U;
		/// [[BPR-PENDING-AR1]]. Reaction changes GitHub answered as asked.
		std::uint64_t Reactions = 0U;

		auto operator<=>(const review_mutation_application_host_statistics&) const
			= default;
	};

	/// \brief Serializes authorized review mutation effects and their terminals.
	class review_mutation_application_host final
	{
	public:
		/// \brief Builds the production path with complete live authority binding.
		review_mutation_application_host(
			std::unique_ptr<review_mutation_port> aProvider,
			review_mutation_target_binding        aTarget,
			review_mutation_acting_identity       aActingIdentity,
			core::application_session_id          aSession,
			core::request_generation              aWorkbenchGeneration,
			review_mutation_execution_grant       aExecutionGrant,
			effect_executor&                      aEffects,
			decision_executor&                    aDecisionLane,
			review_mutation_projection_port&      aProjection,
			review_mutation_source_port* aSources = nullptr, insights_review_capture aInsights = { });

#if defined(BPR_APPLICATION_TESTING) || defined(BPR_BUILD_TESTING)
		/// \brief Builds the fixture-only path around a hermetic mutation provider.
		review_mutation_application_host(
			review_mutation_port&            aFixtureProvider,
			review_mutation_target_binding   aTarget,
			review_mutation_acting_identity  aActingIdentity,
			core::application_session_id     aSession,
			core::request_generation         aWorkbenchGeneration,
			review_mutation_execution_grant  aExecutionGrant,
			effect_executor&                 aEffects,
			decision_executor&               aDecisionLane,
			review_mutation_projection_port& aProjection,
			review_mutation_source_port* aSources = nullptr, insights_review_capture aInsights = { });
#endif

		~review_mutation_application_host();

		review_mutation_application_host(
			const review_mutation_application_host&) = delete;
		auto operator=(const review_mutation_application_host&)
			-> review_mutation_application_host& = delete;

		[[nodiscard]] auto publish(review_mutation_publication_request aRequest,
								   review_mutation_execution_grant     aRequestGrant = { })
			-> review_mutation_request_context;
		/// \brief [[BPR-0657]]. Resolves or unresolves one thread under a grant
		/// bound to the accepted target and actor; refused before any effect
		/// when the provider lacks the capability or the grant does not permit
		/// the exact request.
		[[nodiscard]] auto resolve(
			review_thread_resolution_publication_request aRequest,
			review_mutation_execution_grant              aRequestGrant = { })
			-> review_mutation_request_context;
		/// \brief [[BPR-PENDING-AR1]]. Adds or removes the viewer's reaction
		/// under a grant bound to the accepted target and actor, as a
		/// resolution does; refused before any effect when the provider does
		/// not react or the grant does not permit the exact request.
		[[nodiscard]] auto react(
			review_reaction_publication_request aRequest,
			review_mutation_execution_grant     aRequestGrant = { })
			-> review_mutation_request_context;
		/// \brief [[BPR-1345]]. Where a new comment on this selection would go,

		/// decided exactly as Publish decides it and without publishing: the
		/// file's path and the head it is written on, with the anchor.
		/// \details Start a review keeps the answer as the draft's pending
		/// mark, so a review comment is admitted on the same terms as one
		/// published on its own: the current head, the selected file, the
		/// source's lines, and on a base line the evidence that its numbering
		/// is GitHub's. No effect is queued.
		[[nodiscard]] auto comment_destination(const review_mutation_publication_request& aRequest) const
			-> std::expected<review_mutation_anchor_binding, review_mutation_failure>;
		void cancel(review_mutation_request_context aContext) noexcept;
		/// Queues a read-only repair pass on the serialized mutation effect lane.
		void repair_pending_publications();
		void shutdown() noexcept;

		[[nodiscard]] auto designated() const noexcept
			-> const core::review_mutation_target&;
		[[nodiscard]] auto capabilities() const noexcept
			-> review_provider_capabilities;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		[[nodiscard]] auto outstanding_requests() const noexcept -> std::size_t;
		[[nodiscard]] auto statistics() const noexcept
			-> review_mutation_application_host_statistics;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MUTATION_APPLICATION_HOST_HPP
