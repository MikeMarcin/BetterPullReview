#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MUTATION_APPLICATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_MUTATION_APPLICATION_HOST_HPP

#include "bpr/application/port.hpp"
#include "bpr/application/review_mutation_coordinator.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <string>

namespace bpr::application {
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

		auto operator==(const review_mutation_terminal&) const -> bool = default;
	};

	class review_mutation_projection_port
	{
	public:
		virtual ~review_mutation_projection_port() = default;

		virtual void publish(const review_mutation_terminal& aTerminal) = 0;
	};

	/// \brief Host-validated scope presented to an opaque execution authority.
	struct review_mutation_execution_scope
	{
		core::application_session_id ApplicationSession;
		core::request_generation     WorkbenchGeneration;
		core::review_mutation_target Target;
		core::github_account_id      ActingIdentity;
		core::file_comparison_digest Comparison;
		core::file_identity          File;
		core::source_line_ref        Anchor;
		core::git_commit_oid         Commit;

		auto operator<=>(const review_mutation_execution_scope&) const = default;
	};

	/// \brief Opaque capability required before a mutation effect can be queued.
	///
	/// \details This interface deliberately has no production issuer in the
	/// current slice. A later policy slice may supply one without changing the
	/// host contract. Environment variables, command-line flags, persisted
	/// values, and Qt gestures cannot construct this capability.
	class review_mutation_execution_authority final
	{
	public:
		review_mutation_execution_authority(
			const review_mutation_execution_authority&) = delete;
		auto operator=(const review_mutation_execution_authority&)
			-> review_mutation_execution_authority& = delete;
		~review_mutation_execution_authority()      = default;

	private:
		explicit review_mutation_execution_authority(
			review_mutation_execution_scope aScope)
			: Scope{ std::move(aScope) }
		{
		}

	public:
		/// \brief Confirms the request lies inside this session/target grant.
		[[nodiscard]] auto permits(
			const review_mutation_execution_scope& aScope) const noexcept -> bool
		{
			return Scope.ApplicationSession == aScope.ApplicationSession
				&& Scope.WorkbenchGeneration == aScope.WorkbenchGeneration
				&& Scope.Target == aScope.Target
				&& Scope.ActingIdentity == aScope.ActingIdentity
				&& aScope.Comparison.valid() && aScope.File.valid()
				&& aScope.Anchor.Comparison == aScope.Comparison
				&& aScope.Anchor.Side == core::diff_side::right
				&& aScope.Commit == Scope.Target.ExpectedHead;
		}

	private:
		friend class review_mutation_application_host;
#if defined(BPR_APPLICATION_TESTING) || defined(BPR_BUILD_TESTING)
		friend auto make_review_mutation_testing_grant(
			core::application_session_id,
			core::request_generation,
			core::review_mutation_target,
			core::github_account_id)
			-> std::shared_ptr<const review_mutation_execution_authority>;
#endif

		review_mutation_execution_scope Scope;
	};

	using review_mutation_execution_grant
		= std::shared_ptr<const review_mutation_execution_authority>;

#if defined(BPR_APPLICATION_TESTING) || defined(BPR_BUILD_TESTING)
	/// \brief Mints the bounded execution authority used only by fixture builds.
	[[nodiscard]] auto make_review_mutation_testing_grant(
		core::application_session_id aSession,
		core::request_generation     aGeneration,
		core::review_mutation_target aTarget,
		core::github_account_id      aActingIdentity)
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
			review_mutation_projection_port&      aProjection);

#if defined(BPR_APPLICATION_TESTING) || defined(BPR_BUILD_TESTING)
		/// \brief Builds the fixture-only path around a hermetic mutation provider.
		review_mutation_application_host(
			review_mutation_port&            aFixtureProvider,
			core::review_mutation_target     aTarget,
			core::github_account_id          aActingIdentity,
			core::application_session_id     aSession,
			core::request_generation         aWorkbenchGeneration,
			review_mutation_execution_grant  aExecutionGrant,
			effect_executor&                 aEffects,
			decision_executor&               aDecisionLane,
			review_mutation_projection_port& aProjection);
#endif

		~review_mutation_application_host();

		review_mutation_application_host(
			const review_mutation_application_host&) = delete;
		auto operator=(const review_mutation_application_host&)
			-> review_mutation_application_host& = delete;

		[[nodiscard]] auto publish(review_mutation_publication_request aRequest)
			-> review_mutation_request_context;
		void cancel(review_mutation_request_context aContext) noexcept;
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
