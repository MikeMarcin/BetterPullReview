#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_AGENT_RUNTIME_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_AGENT_RUNTIME_HPP

#include "bpr/application/agent_conversation.hpp"
#include "bpr/application/agent_invocation_host.hpp"
#include "bpr/application/agent_queue_host.hpp"
#include "bpr/application/agent_session_runner.hpp"
#include "bpr/application/port.hpp"

#include <chrono>
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
		/// \brief [[BPR-2153]]. Whether the run `aRun` could take a
		/// correction now (`agent_invocation_host::steer_support`).
		[[nodiscard]] virtual auto steer_support(const agent_invocation_projection_identity& aRun) const -> core::agent_steer_support
		{
			static_cast<void>(aRun);
			return core::agent_steer_support::no_connection;
		}
		/// \brief [[BPR-2153]]. Hands a correction to the run
		/// (`agent_invocation_host::steer`); answered once.
		virtual void steer(const agent_invocation_projection_identity& aRun, std::string aText, agent_invocation_host::steer_answer aAnswer)
		{
			static_cast<void>(aRun);
			static_cast<void>(aText);
			if (aAnswer) {
				aAnswer({ .Support = core::agent_steer_support::no_connection });
			}
		}
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
		/// ask's material), sending through the runtime, as run `aRun`. Says
		/// `awaiting` when the turn is sent on a later pass (its material is
		/// read first; `refuse_release` or the turn's dispatch settles it),
		/// `answered` when it was answered with no turn (a slash command), and
		/// otherwise `refused`. A turn dispatched inside the call is `started`
		/// whatever it says: the runtime saw it. Optional: without one, or
		/// with no window attached, the runtime builds a typed message's
		/// request from its snapshot.
		std::function<queued_admission_kind(const core::queued_entry& aEntry, std::uint64_t aRun)> Release;
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
		/// releases, otherwise built from the snapshot. RFC-BPR-0126 AC03: the
		/// admission is keyed by the run the release was admitted as.
		[[nodiscard]] auto release(const core::queued_entry& aEntry) -> queued_admission;
		/// \brief (d) A turn the window composed, sent through the runtime.
		/// Refused while a turn runs or with no live authority. Sent while the
		/// lent review is still being accepted, it waits for that (`ready`) as
		/// the session's running turn, rather than being refused. While a
		/// release awaits its turn, this is that turn and takes its run.
		/// [[BPR-2243]]. Sent after the running turn finished and before its
		/// run retired (the window already shows the answer), it is held and
		/// sent at that retirement, ahead of the queue; one such turn at most.
		[[nodiscard]] auto dispatch(agent_invocation_request aRequest) -> bool;
		/// \brief AC03. The release admitted as `aRun`, awaiting its turn, was
		/// refused before the turn was sent (its material could not be read):
		/// it settles as refused. Nothing happens for any other run.
		void refuse_release(std::uint64_t aRun);
		/// \brief The release awaiting its turn, if any.
		[[nodiscard]] auto awaiting() const -> std::optional<std::uint64_t>;
		/// \brief The run of the turn now running, or of the last one.
		[[nodiscard]] auto last_run() const -> std::uint64_t;
		/// \brief [[BPR-2181]]. The runtime's machinery accepted the review
		/// and may run a turn: a turn that waited for it starts.
		void ready();
		void cancel();
		/// \brief The run of the runtime's turn retired (its machinery is
		/// quiescent again): the runtime is free, and `aDone` is told.
		void retired();
		void on_retired(std::function<void()> aDone);
		/// \brief A release's turn began (the run it was admitted as was
		/// dispatched): `aBegan` is told that run.
		void               on_began(std::function<void(std::uint64_t aRun)> aBegan);
		[[nodiscard]] auto running() const -> bool;
		/// \brief How the last turn's run ended: `failed` when it retired with
		/// no finish (its machinery refused it before it started).
		[[nodiscard]] auto last_finish() const -> core::queued_finish;
		/// \brief [[BPR-2153]], RFC-BPR-0126 AC05. A queued message offered to
		/// the session's running turn: none when it was handed to the run this
		/// runtime runs, otherwise why not, in the Agent tab's words. A
		/// hand-off the connection later finds unwritten goes back to
		/// `aQueue`, first in line, and every attached window is told why.
		[[nodiscard]] auto steer(const core::queued_entry& aEntry, agent_queue_host* aQueue) -> std::optional<std::string>;
		/// \brief [[BPR-2152]] (AC06). What a correction pressed now is bound
		/// to: the running turn's run and the native conversation (BPR's
		/// agent session) it runs in, or the last one known.
		[[nodiscard]] auto bound() const -> core::queued_claim;
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
		void pin_native_conversation(core::agent_profile_id aProfile, const core::agent_revision_binding& aBinding) override;
		void present_native_history(const native_agent_history_projection& aView) override;
		void present_session_catalog(const agent_session_catalog_projection& aProjection) override;

	private:
		template <typename Event>
		void present(Event&& aEvent);
		/// The awaited release ends as `aFinish` with no turn of its own.
		void settle_awaiting(core::queued_finish aFinish);

		std::string                                     Session;
		std::unique_ptr<review_agent_turn_port>         Turns;
		std::optional<review_agent_snapshot>            Snapshot;
		std::map<std::uint64_t, review_agent_presenter> Presenters;
		std::uint64_t                                   LastPresenter = 0U;
		bool                                            Running       = false;
		/// The running turn's question and its answer so far.
		std::optional<agent_conversation_turn> Turn;
		/// [[BPR-2241]]. When the running turn was sent.
		std::chrono::steady_clock::time_point TurnStarted;
		bool                                  TurnShown = false;
		std::optional<core::queued_finish>    Finished;
		/// A turn sent before the machinery was authorized, waiting for it.
		std::optional<agent_invocation_request> Waiting;
		/// [[BPR-2243]]. A turn sent between the running turn's finish and
		/// its retirement, sent when it retires.
		std::optional<agent_invocation_request> Next;
		/// AC03. Runs are numbered by the runtime; a release reserves one.
		std::uint64_t LastRun = 0U;
		std::uint64_t Run     = 0U;
		/// A release whose turn is sent on a later pass, and the window that
		/// will send it.
		std::optional<std::uint64_t>                                                       Awaiting;
		std::optional<std::uint64_t>                                                       AwaitingPresenter;
		bool                                                                               TurnNative = false;
		std::vector<agent_conversation_turn>                                               Unshown;
		std::function<void()>                                                              Done;
		std::function<void(std::uint64_t)>                                                 Began;
		std::function<void(const agent_conversation_key&, const agent_conversation_turn&)> Keep;
		/// [[BPR-2153]]. The run admitted for the running turn, which Steer
		/// names.
		std::optional<agent_invocation_projection_identity> Active;
		/// [[BPR-2152]]. The running turn streamed something: the agent took
		/// its message.
		bool Streamed = false;
		/// [[BPR-2152]]. The native conversation of the last admitted run.
		std::uint64_t Conversation = 0U;
		/// [[BPR-2152]]. Where corrections were claimed: the run's end
		/// settles them there.
		agent_queue_host* SteerQueue = nullptr;
		/// Lets a late steer answer know the runtime is gone.
		std::shared_ptr<bool> Alive = std::make_shared<bool>(true);
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
		/// \brief RFC-BPR-0126 AC15 ([[BPR-2181]]). Stop all, across the
		/// profile: every session's queue pauses as stopped first, so nothing
		/// starts next, then every runtime's turn is stopped, shown in a window
		/// or not.
		void stop_all();
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
		void               began(const std::string& aSession, std::uint64_t aRun);

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
