#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_AGENT_RUNTIME_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_AGENT_RUNTIME_HPP

#include "bpr/application/agent_conversation.hpp"
#include "bpr/application/agent_invocation_host.hpp"
#include "bpr/application/agent_queue_host.hpp"
#include "bpr/application/agent_session_runner.hpp"
#include "bpr/application/port.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

/// \file
/// \brief RFC-BPR-0126 AC01 ([[BPR-2181]] slice 2). The profile's agent
/// runtime: one per review session, owned by the profile rather than by any
/// window. The owner, 2026-11-14: "Queues should be managed by something like
/// a session manager at a higher level and have nothing to do with the UI,
/// the UI is just a presentation layer". A runtime runs its session's turns
/// on agent machinery the profile's wiring composed for it, builds a released
/// message's request from the review snapshot it holds, keeps the turns it ran
/// while no window showed it, and lives while its session's queue can drain
/// or a turn runs, even after its last window closed. Windows attach to
/// present its turns and send their own turns through it; none is its
/// transport.
namespace bpr::application {
	/// \brief The agent machinery one review's turns run on: an agent
	/// invocation host over ports that outlive every window. Its turn events
	/// reach the projection the runtime gave its composition; its run's
	/// retirement reaches the runtime's `retired`.
	class review_agent_turn_port
	{
	public:
		virtual ~review_agent_turn_port()                        = default;
		virtual void dispatch(agent_invocation_request aRequest) = 0;
		virtual void cancel()                                    = 0;
		/// \brief No run, workspace or helper of it is outstanding.
		[[nodiscard]] virtual auto is_quiescent() const -> bool = 0;
		virtual void               shutdown()                   = 0;
		/// \brief [[BPR-2181]]. The review's authority the machinery reads it
		/// under (the lease the profile lent it) is live. No turn starts
		/// without it.
		[[nodiscard]] virtual auto authorized() const -> bool = 0;
		/// \brief [[BPR-2181]]. A live authority was lent and the machinery
		/// is still accepting the review it names: `authorized` follows
		/// shortly, so a turn sent now waits rather than being refused.
		[[nodiscard]] virtual auto accepting() const -> bool { return false; }
		/// \brief [[BPR-2181]]. Tries again to remove a workspace whose removal
		/// was refused, as a window's shutdown poll does for its own host.
		virtual void retry_cleanup() { }
	};

	/// \brief The review a runtime builds a released message's request from:
	/// the request a window composed for that review's Agent tab, with
	/// everything but the question. Adopted from the last window that showed
	/// the review; the runtime keeps it after that window closes.
	struct review_agent_snapshot
	{
		agent_invocation_request Request;
		/// [[BPR-2181]]. The pull request's conversation the Agent tab keeps
		/// this agent's turns in: where a turn no window saw is written.
		std::optional<agent_conversation_key> Conversation;

		auto operator<=>(const review_agent_snapshot&) const = default;
	};

	/// \brief A window presenting a runtime's turns.
	struct review_agent_presenter
	{
		/// Shown every turn event while attached. Optional.
		agent_invocation_projection_port* Projection = nullptr;
		/// Releases a queued message with the window's whole context (a quick
		/// ask's material), sending through the runtime. Optional: without one,
		/// or with no window attached, the runtime builds a typed message's
		/// request from its snapshot.
		std::function<bool(const core::queued_entry& aEntry)> Release;
		/// Whether the window can release one now (its agent set up, its tab
		/// free). Optional: without one, it always can.
		std::function<bool()> Ready;
		/// [[BPR-2181]]. Told each time the runtime's machinery accepted the
		/// review (its turns then run under that review's identity). Optional.
		std::function<void()> Accepted;

		[[nodiscard]] auto releases() const -> bool { return Release && (!Ready || Ready()); }
	};

	/// \brief RFC-BPR-0126 AC02 ([[BPR-2181]] slice 3). How a released
	/// message's run ended, as the queue reads it: answered or set aside, it
	/// went; cancelled, it was stopped; any other ending failed. The core maps
	/// that to the queue's pause (`core::pause_for`).
	[[nodiscard]] auto queued_finish_of(const agent_run_result& aResult) noexcept -> core::queued_finish;

	/// \brief Bounds the turns a runtime keeps while no window shows them.
	inline constexpr std::size_t maximum_unshown_turns = maximum_conversation_turns;

	class review_agent_runtime final : public agent_invocation_projection_port
	{
	public:
		explicit review_agent_runtime(std::string aSession);
		~review_agent_runtime() override;
		review_agent_runtime(const review_agent_runtime&)                    = delete;
		auto operator=(const review_agent_runtime&) -> review_agent_runtime& = delete;

		/// \brief The composed machinery; set once, before any turn.
		void bind(std::unique_ptr<review_agent_turn_port> aTurns);
		/// \brief (b) The review's latest snapshot; replaces the previous one.
		void               adopt(review_agent_snapshot aSnapshot);
		[[nodiscard]] auto snapshot() const -> const std::optional<review_agent_snapshot>&;

		[[nodiscard]] auto attach(review_agent_presenter aPresenter) -> std::uint64_t;
		void               detach(std::uint64_t aPresenter);
		[[nodiscard]] auto presented() const -> bool;

		/// \brief What the session's agent can take for `aFront` (the message
		/// that goes next): busy while a turn runs or its run has not retired;
		/// not set up when nothing could send it or the review's authority is
		/// not live.
		[[nodiscard]] auto state(const core::queued_entry* aFront) const -> core::queue_agent;
		/// \brief Releases one queued message: through an attached window that
		/// releases, otherwise built from the snapshot. Whether a turn started.
		[[nodiscard]] auto release(const core::queued_entry& aEntry) -> bool;
		/// \brief (d) A turn the window composed, sent through the runtime.
		/// Refused while a turn runs or with no live authority. Sent while the
		/// lent review is still being accepted, it waits for that (`ready`) as
		/// the session's running turn, rather than being refused.
		[[nodiscard]] auto dispatch(agent_invocation_request aRequest) -> bool;
		/// \brief [[BPR-2181]]. The runtime's machinery accepted the review
		/// and may run a turn: a turn that waited for it starts.
		void ready();
		void cancel();
		/// \brief The run of the runtime's turn retired (its machinery is
		/// quiescent again): the runtime is free, and `aDone` is told.
		void               retired();
		void               on_retired(std::function<void()> aDone);
		[[nodiscard]] auto running() const -> bool;
		/// \brief How the last turn's run ended: `failed` when it retired with
		/// no finish (its machinery refused it before it started).
		[[nodiscard]] auto last_finish() const -> core::queued_finish;
		[[nodiscard]] auto is_quiescent() const -> bool;
		void               shutdown();
		/// \brief Retries a refused workspace removal of its machinery, and
		/// settles a turn whose run has retired since.
		void retry_cleanup();

		/// \brief (c) The turns that ran while no window showed the session,
		/// oldest first, and that no native history keeps; a window that
		/// attaches adds them to the session's conversation, which is then
		/// their one transcript, and they stay until `forget_unshown`.
		[[nodiscard]] auto unshown_turns() const -> const std::vector<agent_conversation_turn>&;
		void               forget_unshown();
		/// \brief [[BPR-2181]]. Where a turn no window saw is also written, so
		/// a quit before a window attaches loses none: given the snapshot's
		/// conversation and the turn. Set once, by the registry.
		void on_unshown(std::function<void(const agent_conversation_key& aConversation, const agent_conversation_turn& aTurn)> aKeep);

		[[nodiscard]] auto session() const -> const std::string&;

		// agent_invocation_projection_port: every event goes to the attached
		// windows; a turn none shows is kept.
		void offer(std::span<const core::agent_profile> aProfiles) override;
		void refuse(std::string_view aReason) override;
		void admit(const agent_invocation_projection_identity& aIdentity) override;
		void start(const agent_invocation_projection_identity& aIdentity, const prepared_workspace& aWorkspace) override;
		void stream(const agent_invocation_projection_identity& aIdentity, const agent_stream_event& aEvent) override;
		void finish(const agent_invocation_projection_identity& aIdentity, const agent_run_result& aResult) override;
		void deliver_findings(const agent_invocation_projection_identity& aIdentity, const agent_finding_delivery& aDelivery) override;
		void private_changed(const agent_invocation_projection_identity& aIdentity, const private_review_delta& aDelta) override;
		void private_refused(const agent_invocation_projection_identity& aIdentity, const private_run_error& aError) override;
		void restore_native_session(const core::agent_session& aSession) override;
		void present_native_history(const native_agent_history_projection& aView) override;
		void present_session_catalog(const agent_session_catalog_projection& aProjection) override;

	private:
		template <typename Event>
		void present(Event&& aEvent);

		std::string                                     Session;
		std::unique_ptr<review_agent_turn_port>         Turns;
		std::optional<review_agent_snapshot>            Snapshot;
		std::map<std::uint64_t, review_agent_presenter> Presenters;
		std::uint64_t                                   LastPresenter = 0U;
		bool                                            Running       = false;
		/// The running turn's question and its answer so far.
		std::optional<agent_conversation_turn> Turn;
		bool                                   TurnShown = false;
		std::optional<core::queued_finish>     Finished;
		/// A turn sent before the machinery was authorized, waiting for it.
		std::optional<agent_invocation_request>                                            Waiting;
		bool                                                                               TurnNative = false;
		std::vector<agent_conversation_turn>                                               Unshown;
		std::function<void()>                                                              Done;
		std::function<void(const agent_conversation_key&, const agent_conversation_turn&)> Keep;
	};

	struct review_agent_runtime_dependencies
	{
		/// Where the profile's queues are; the registry keeps a runtime while
		/// its session's queue can drain.
		agent_queue_host* Queue = nullptr;
		/// The profile's turn runner, which the registry offers its runtimes to.
		agent_session_runner* Runner = nullptr;
		/// Where a retired runtime is let go: never inside its own callback.
		decision_executor* DecisionLane = nullptr;
		/// Composes one session's agent machinery, its turn events to
		/// `aProjection` and its retirement to `aRetired`. None: the session has
		/// no agent (an agent-free build or an unusable setup).
		std::function<std::unique_ptr<review_agent_turn_port>(const std::string& aSession, agent_invocation_projection_port& aProjection, std::function<void()> aRetired)> Compose;
		/// Whether a session is one the registry runs (the Agent tab's, not a
		/// tour's, which a window still runs until slice 6).
		std::function<bool(const std::string& aSession)> Runs;
		/// [[BPR-2181]]. Optional: the profile's conversation store, where a
		/// turn no window saw joins its pull request's conversation, and the
		/// serial lane those writes run on, off the decision lane. Without
		/// both, such a turn lives only until a window takes it.
		agent_conversation_store_port* Conversations       = nullptr;
		effect_executor*               ConversationEffects = nullptr;
	};

	/// \brief The profile's runtimes, one per review session.
	class review_agent_runtimes final
	{
	public:
		explicit review_agent_runtimes(review_agent_runtime_dependencies aDependencies);
		~review_agent_runtimes();
		review_agent_runtimes(const review_agent_runtimes&)                    = delete;
		auto operator=(const review_agent_runtimes&) -> review_agent_runtimes& = delete;

		/// \brief The session's runtime, made when it has none. None when the
		/// session has no agent or the registry is shutting down.
		[[nodiscard]] auto open(const std::string& aSession) -> review_agent_runtime*;
		[[nodiscard]] auto find(const std::string& aSession) const -> review_agent_runtime*;
		/// \brief A window stops presenting `aSession`: its runtime stays while
		/// its queue can drain or a turn runs.
		void detach(const std::string& aSession, std::uint64_t aPresenter);
		/// \brief [[BPR-2181]]. The review's lent authority ended (the account
		/// or its identity changed, the pull request became unreadable, or
		/// the profile closed): the session's queue pauses with
		/// `core::queue_pause::lost_access` and its running turn is
		/// cancelled. Nothing starts until a window lends it a live
		/// authority again and the reviewer resumes.
		void authority_ended(const std::string& aSession);
		/// \brief The sessions with a runtime.
		[[nodiscard]] auto sessions() const -> std::vector<std::string>;
		void               shutdown();
		[[nodiscard]] auto is_quiescent() const -> bool;
		/// \brief Retries every runtime's refused workspace removal, retiring
		/// ones included: the profile's shutdown poll calls it while they are
		/// not quiescent.
		void retry_cleanup();

	private:
		[[nodiscard]] auto front(const std::string& aSession) const -> const core::queued_entry*;
		[[nodiscard]] auto holds_work(const std::string& aSession) const -> bool;
		void               settle(const std::string& aSession);
		void               retired(const std::string& aSession);

		review_agent_runtime_dependencies                            Dependencies;
		std::map<std::string, std::unique_ptr<review_agent_runtime>> Runtimes;
		/// Runtimes let go and waiting to be destroyed off their callback.
		std::vector<std::unique_ptr<review_agent_runtime>> Retiring;
		std::optional<std::uint64_t>                       Offered;
		std::optional<std::uint64_t>                       Subscription;
		std::shared_ptr<bool>                              Alive   = std::make_shared<bool>(true);
		bool                                               Closing = false;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_AGENT_RUNTIME_HPP
