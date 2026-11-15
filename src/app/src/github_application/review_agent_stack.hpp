#ifndef BPR_APP_GITHUB_APPLICATION_REVIEW_AGENT_STACK_HPP
#define BPR_APP_GITHUB_APPLICATION_REVIEW_AGENT_STACK_HPP
#if defined(BPR_AGENT_INTEGRATIONS)
	#include "bpr/adapters/agent/agent_process_launcher.hpp"
	#include "bpr/adapters/sqlite/private_review_store.hpp"
	#include "bpr/adapters/sqlite/review_ledger_store.hpp"
	#include "bpr/application/agent_invocation_host.hpp"
	#include "bpr/application/agent_program_resolution.hpp"
	#include "bpr/application/agent_run_diagnosis.hpp"
	#include "bpr/application/claude_process_wrapper.hpp"
	#include "bpr/application/diagnostic_log_port.hpp"
	#include "bpr/application/epoch_application_host.hpp"
	#include "bpr/application/github_application_host.hpp"
	#include "bpr/application/native_agent_session.hpp"
	#include "bpr/application/port.hpp"
	#include "bpr/application/private_review_application_host.hpp"
	#include "bpr/application/profile_epoch_actor_registry.hpp"
	#include "bpr/application/review_agent_runtime.hpp"
	#include "bpr/application/review_ledger_application_host.hpp"
	#include "bpr/core/agent_profile.hpp"
	#include "bpr/core/agent_session.hpp"

	#include <cstddef>
	#include <filesystem>
	#include <functional>
	#include <memory>
	#include <mutex>
	#include <optional>
	#include <string>
	#include <utility>
	#include <vector>

	#if defined(BPR_BUILD_TESTING)
		#include "github_application/agent_composition.hpp"
	#endif

/// \file
/// \brief RFC-BPR-0126 AC01 ([[BPR-2181]] slice 2). The agent machinery one
/// review's turns run on, owned by the profile rather than by a window: an
/// epoch host fed the review a window lent it, its own ledger host for
/// session admission, its own private-run store, its own process chain and
/// the agent invocation host over them. Nothing here borrows a window's
/// port, so a turn runs on after every window showing the review closed.
namespace bpr::app {
	/// \brief What a window hands the profile when it first opens a review's
	/// runtime: the agent setup it composed, as values and shared sources,
	/// and where the review's stores are. None of it refers to the window.
	struct review_agent_stack_seed
	{
		std::vector<core::agent_profile>                            Profiles;
		std::vector<core::agent_host_policy>                        Policies;
		std::shared_ptr<application::claude_process_wrapper_source> ClaudeWrapper;
		std::shared_ptr<application::agent_program_source>          Programs;
		std::shared_ptr<application::agent_entry_source>            Entries;
		application::claude_locator                                 LocateClaude;
		application::agent_program_locator                          LocateProgram;
		adapters::sqlite::review_ledger_configuration               Ledger;
		adapters::sqlite::private_review_configuration              Private;
		std::filesystem::path                                       ScratchRoot;
	#if defined(BPR_BUILD_TESTING)
		/// A fixture run's program admission ([[BPR-2177]]); empty for none.
		std::function<std::optional<std::string>(const std::string&)> Admission;
	#endif
	};

	/// \brief The profile's ports and lanes a stack runs on; all outlive it.
	struct review_agent_stack_ports
	{
		application::profile_epoch_actor_registry&        Actors;
		application::epoch_catalog_port&                  Catalogs;
		application::epoch_entropy_port&                  Entropy;
		application::epoch_clock_port&                    Clock;
		application::effect_executor&                     EpochEffects;
		application::effect_executor&                     AgentEffects;
		application::effect_executor&                     LedgerEffects;
		application::decision_executor&                   DecisionLane;
		std::mutex&                                       LedgerFile;
		std::filesystem::path                             WorkspaceParent;
		application::native_agent_session_store_port*     NativeSessions = nullptr;
		std::shared_ptr<application::diagnostic_log_port> Log;
	};

	class review_agent_stack final
		: public application::review_agent_turn_port,
		  public application::agent_session_catalog_port
	{
	public:
		/// \brief Composes one review's stack. None when the profile's epoch
		/// registry admits no further profile-owned consumer.
		[[nodiscard]] static auto make(core::application_session_id aSession, review_agent_stack_seed aSeed, const review_agent_stack_ports& aPorts,
									   application::agent_invocation_projection_port& aProjection, std::function<void()> aRetired, std::function<void()> aReady)
			-> std::unique_ptr<review_agent_stack>;
		~review_agent_stack() override;
		review_agent_stack(const review_agent_stack&)                    = delete;
		auto operator=(const review_agent_stack&) -> review_agent_stack& = delete;

		/// \brief [[BPR-2181]]. The review's authority lent in this stack's own
		/// name: its handoff goes to the stack's epoch host, and its lease gates
		/// every turn. A new loan replaces the last.
		void lend(application::lent_review_authority aLent, core::agent_revision_binding aFor);
		/// \brief A live loan was lent for the window's review at `aBinding`.
		[[nodiscard]] auto lent_for(const core::agent_revision_binding& aBinding) const -> bool;
		[[nodiscard]] auto session() const noexcept -> core::application_session_id;
		[[nodiscard]] auto liveness() const -> std::weak_ptr<bool> { return Alive; }
		[[nodiscard]] auto statistics() const -> application::agent_invocation_host_statistics;
	#if defined(BPR_BUILD_TESTING)
		/// \brief The programs this stack's launcher was asked to start.
		[[nodiscard]] auto invocations() const -> std::size_t { return Probe != nullptr ? Probe->invocations() : 0U; }
	#endif

		// review_agent_turn_port. A turn the window composed names the window's
		// application session; it runs under this stack's.
		void               dispatch(application::agent_invocation_request aRequest) override;
		void               cancel() override;
		[[nodiscard]] auto is_quiescent() const -> bool override;
		void               shutdown() override;
		[[nodiscard]] auto authorized() const -> bool override;
		[[nodiscard]] auto accepting() const -> bool override;
		void               retry_cleanup() override;
		/// \brief The review the stack's epoch host accepted and its agent host
		/// observes, with the stack's own session: the identity every turn of
		/// it carries. None before the lent review is accepted.
		[[nodiscard]] auto observed_review() const -> std::optional<std::pair<core::pull_request_authority, core::agent_revision_binding>>;
		/// \brief [[BPR-2153]]. Steering goes to the stack's invocation host.
		[[nodiscard]] auto steer_support(const application::agent_invocation_projection_identity& aRun) const -> core::agent_steer_support override;
		void               steer(const application::agent_invocation_projection_identity& aRun, std::string aText, application::agent_invocation_host::steer_answer aAnswer) override;

		// agent_session_catalog_port, over the stack's own ledger host.
		void admit(const application::agent_session_admission&                                                aAdmission,
				   std::function<void(std::expected<core::agent_session, application::ledger_store_failure>)> aDone) override;
		void session_catalog(std::function<void(std::expected<application::agent_session_catalog_state, application::ledger_store_failure>)> aDone) override;

	private:
		struct epoch_observer;
		struct receipt_sink;
		review_agent_stack(core::application_session_id aSession, review_agent_stack_seed aSeed, const review_agent_stack_ports& aPorts, std::function<void()> aRetired, std::function<void()> aReady);
		void observe_review();
		/// Closes the epoch and ledger hosts once the agent host is quiescent.
		void close_review_hosts();

		core::application_session_id                Session;
		review_agent_stack_seed                     Seed;
		application::decision_executor&             DecisionLane;
		std::function<void()>                       Retired;
		std::function<void()>                       Ready;
		std::shared_ptr<bool>                       Alive = std::make_shared<bool>(true);
		std::optional<core::agent_revision_binding> Observed;
		std::optional<core::agent_revision_binding> LentFor;
		bool                                        Closing           = false;
		bool                                        ReviewHostsClosed = false;
		application::lent_review_lease              Lease;
		// Declared in dependency order: each is destroyed before what it holds.
		std::unique_ptr<adapters::agent::agent_process_launcher> Launcher;
	#if defined(BPR_BUILD_TESTING)
		std::unique_ptr<composition_agent_process_launcher_probe> Probe;
	#endif
		std::unique_ptr<application::wrapped_claude_process>         ClaudeProcess;
		std::unique_ptr<application::located_agent_process>          LocatedProcess;
		std::unique_ptr<application::agent_entry_process>            EntryProcess;
		std::unique_ptr<application::diagnosed_agent_process>        DiagnosedProcess;
		std::unique_ptr<receipt_sink>                                Receipts;
		std::unique_ptr<application::review_ledger_application_host> Ledger;
		std::unique_ptr<epoch_observer>                              EpochProjection;
		std::unique_ptr<application::epoch_application_host>         Epoch;
		/// The private-run store this stack opened, on an agent effect worker
		/// under the ledger-file gate, and the runs over it.
		std::unique_ptr<application::private_review_store_port> PrivateStore;
		std::unique_ptr<application::scoped_private_agent_runs> PrivateRuns;
		std::unique_ptr<application::agent_invocation_host>     Agent;
	};
} // namespace bpr::app
#endif
#endif
