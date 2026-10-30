#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_LAUNCH_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_LAUNCH_PANEL_HPP

#include "bpr/application/agent_invocation_host.hpp"
#include "bpr/core/agent_chat_scope.hpp"

#include <QString>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <utility>
#include <variant>
#include <vector>

class QAction;
class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QToolButton;

namespace bpr::qt {
	/// \brief Asks for one agent invocation, now.
	struct start_agent_intent
	{
		core::agent_profile_id                         Profile;
		std::vector<application::local_note_id>        IncludedNotes;
		std::optional<application::agent_draft_choice> IncludedDraft;
		/// Whether the reviewer chose their own checkout for this invocation.
		/// Carried on the intent rather than read from a setting, because
		/// BPR-0176 refuses that mode without a choice made now.
		bool ExistingCheckoutChosen = false;

		auto operator<=>(const start_agent_intent&) const = default;
	};

	/// \brief Asks for the running invocation to stop.
	struct cancel_agent_intent
	{
		auto operator<=>(const cancel_agent_intent&) const = default;
	};

	using agent_intent = std::variant<start_agent_intent, cancel_agent_intent>;

	using agent_intent_dispatch = std::function<void(agent_intent)>;

	/// \brief Records directly observable launch-surface activity.
	struct agent_launch_panel_statistics
	{
		std::size_t StartIntents  = 0U;
		std::size_t CancelIntents = 0U;
		std::size_t Disclosures   = 0U;

		auto operator<=>(const agent_launch_panel_statistics&) const = default;
	};

	/// \brief Lets a reviewer start one agent, and see what they are starting.
	///
	/// \details **This widget holds no process and no launcher.** It has no
	/// member that could start anything and no header that would let it: the
	/// only thing leaving here is an intent. That is what makes "nothing
	/// launches an agent except a user asking" a property of the type rather
	/// than a discipline its callers keep — a refresh, a navigation, or new
	/// activity arriving cannot reach a process from here, because there is no
	/// process here to reach.
	///
	/// An agent that started itself would spend the reviewer's money and their
	/// agent's rate limit on work they did not request.
	///
	/// The provider and the scope are shown **before** the start action is
	/// available, not reported after the fact. A reviewer who cannot see which
	/// tool is about to read their repository has not been asked.
	class agent_launch_panel final
		: public QWidget,
		  public application::agent_invocation_projection_port
	{
	public:
		explicit agent_launch_panel(
			agent_intent_dispatch aDispatch = { },
			QWidget*              aParent   = nullptr);

		void set_dispatch(agent_intent_dispatch aDispatch);
		void set_configuration_observer(std::function<void()> aObserver);
		/// \brief Owner decision 2026-10-23: the run time limit the reviewer
		/// set (0 for none), so a run it ends says so plainly.
		void set_run_limit_minutes(std::uint32_t aMinutes) noexcept { RunLimitMinutes = aMinutes; }

		/// \brief Offers the profiles this host permits, and no others.
		///
		/// \details The list is what `core::decide_agent_invocation` already
		/// permitted. This widget runs no policy of its own: a profile absent
		/// here is absent because the host refused it, and one present cannot
		/// be refused again by presentation.
		void offer_profiles(std::span<const core::agent_profile> aProfiles);
		/// \brief [[BPR-1749]], RFC-BPR-0092. The profile setup saved.
		///
		/// \details [[BPR-1823]], owner ruling 2026-10-29: it is the first
		/// pick only. Once an agent is picked (the saved one when offered, else
		/// the first offered) the menu keeps it; neither this nor offering the
		/// profiles again moves it. `core::decide_agent_pick` decides.
		void prefer_profile(std::optional<core::agent_profile_id> aProfile);
		/// \brief [[BPR-1823]]. The agent setup wizard chose this agent now
		/// (the reviewer picked or added it there): the one automatic change.
		void choose_profile_from_setup(core::agent_profile_id aProfile);
		/// \brief [[BPR-1823]]. Reset agents: no choice is kept, so the next
		/// setup's agent is the first pick (a new agent may reuse an old id).
		void forget_choice();
		/// \brief [[BPR-1823]]. The name of the agent chosen but no longer
		/// offered, shown in the menu as unavailable; empty otherwise.
		[[nodiscard]] auto unavailable_agent_name() const -> QString;
		/// \brief The number of agents offered (the unavailable entry is not one).
		[[nodiscard]] auto offered_count() const noexcept -> std::size_t { return OfferedIds.size(); }
		/// \brief Offers only opaque current private-context identities, unchecked.
		void offer_private_context(
			std::span<const application::local_note_id>    aNotes,
			std::optional<application::agent_draft_choice> aDraft);

		void offer(std::span<const core::agent_profile> aProfiles) override;
		void refuse(std::string_view aReason) override;
		void admit(
			const application::agent_invocation_projection_identity& aIdentity) override;
		void start(
			const application::agent_invocation_projection_identity& aIdentity,
			const application::prepared_workspace&                   aWorkspace) override;
		void stream(
			const application::agent_invocation_projection_identity& aIdentity,
			const application::agent_stream_event&                   aEvent) override;
		void finish(
			const application::agent_invocation_projection_identity& aIdentity,
			const application::agent_run_result&                     aResult) override;
		void present_session_catalog(
			const application::agent_session_catalog_projection& aProjection) override;

		/// \brief States what is about to be read, before anything starts.
		void disclose_scope(const QString& aScope);

		/// \brief Reports whether an invocation is running.
		void set_running(bool aRunning);

		[[nodiscard]] auto profile_selector() const noexcept -> QComboBox*;
		[[nodiscard]] auto start_action() const noexcept -> QAction*;
		[[nodiscard]] auto cancel_action() const noexcept -> QAction*;
		[[nodiscard]] auto existing_checkout_action() const noexcept -> QAction*;
		[[nodiscard]] auto disclosure_text() const -> QString;
		[[nodiscard]] auto status_text() const -> QString;
		/// \brief [[BPR-1738]]. Why the last run stopped and what to do,
		/// in a reviewer's words, when there is more to say than its outcome.
		[[nodiscard]] auto stop_reason_text() const -> QString;
		/// \brief [[BPR-1827]]. The log of a run that ended without an
		/// answer: what went wrong in words, how much the agent wrote, the
		/// first line this app could not read and the last lines of standard
		/// error, each bounded. Never says the agent wrote nothing when it
		/// wrote something.
		[[nodiscard]] static auto failure_log(const application::agent_run_result& aResult) -> QString;
		/// \brief [[BPR-1753]], owner look v001. Where the running
		/// request is: its workspace being made, or the agent running.
		enum class phase : std::uint8_t {
			idle,
			preparing,
			running,
		};
		[[nodiscard]] auto current_phase() const noexcept -> phase { return Phase; }
		/// The last line the agent wrote, for a live line while it works.
		[[nodiscard]] auto latest_output_line() const -> QString;
		/// Called whenever the request's phase, output or end changes.
		void add_progress_observer(std::function<void()> aObserver);
		/// [[BPR-1753]], owner look v002. What the last request's agent
		/// reported running: its model, and its cost when the harness said.
		[[nodiscard]] auto last_reported_model() const -> QString { return LastModel; }
		[[nodiscard]] auto last_reported_cost() const -> QString { return LastCost; }
		/// [[BPR-1753]]. How the last request ended, while nothing runs.
		[[nodiscard]] auto last_outcome() const noexcept -> std::optional<application::agent_run_outcome> { return LastOutcome; }
		/// [[BPR-1817]]. How many refusals the host has named here, and the
		/// last one. Counted even while a turn runs, when the status line
		/// keeps saying it runs, so the composer can say a send was refused.
		[[nodiscard]] auto refusal_count() const noexcept -> std::uint64_t { return Refusals; }
		[[nodiscard]] auto last_refusal() const -> QString { return LastRefusal; }
		[[nodiscard]] auto output_text() const -> QString;
		[[nodiscard]] auto session_catalog_text() const -> QString;
		[[nodiscard]] auto start_button() const noexcept -> QToolButton*;
		[[nodiscard]] auto note_selector() const noexcept -> QListWidget*;
		[[nodiscard]] auto draft_selector() const noexcept -> QCheckBox*;
		[[nodiscard]] auto selected_profile() const
			-> std::optional<core::agent_profile_id>;
		[[nodiscard]] auto statistics() const noexcept
			-> agent_launch_panel_statistics;

	private:
		void update_actions();
		/// [[BPR-1823]]. Decides the pick in core and presents it.
		void settle_pick(std::optional<core::agent_profile_id> aChose);

		std::function<void()> ConfigurationObserver;
		std::uint32_t         RunLimitMinutes = 0U;
		agent_intent_dispatch Dispatch;
		QComboBox*            ProfileSelector = nullptr;
		/// [[BPR-1749]]. The first pick, when offered.
		std::optional<core::agent_profile_id> PreferredProfile;
		/// [[BPR-1823]]. Whether setup has said what it saved (possibly none).
		bool SetupKnown = false;
		/// [[BPR-1823]]. What the menu holds, as core decided it.
		core::agent_pick                                        Pick;
		std::vector<core::agent_profile_id>                     OfferedIds;
		std::vector<std::pair<core::agent_profile_id, QString>> KnownNames;
		QString                                                 UnavailableName;
		bool                                                    Settling               = false;
		QAction*                                                StartAction            = nullptr;
		QAction*                                                CancelAction           = nullptr;
		QAction*                                                ExistingCheckoutAction = nullptr;
		QLabel*                                                 Disclosure             = nullptr;
		QListWidget*                                            NoteSelector           = nullptr;
		QCheckBox*                                              DraftSelector          = nullptr;
		QLabel*                                                 SessionCatalog         = nullptr;
		QLabel*                                                 Status                 = nullptr;
		QPlainTextEdit*                                         Output                 = nullptr;
		QToolButton*                                            StartButton            = nullptr;
		QToolButton*                                            CancelButton           = nullptr;
		QToolButton*                                            ExistingCheckoutButton = nullptr;
		bool                                                    Running                = false;
		bool                                                    Disclosed              = false;
		std::optional<application::agent_draft_choice>          OfferedDraft;
		QString                                                 StopReason;
		std::optional<application::agent_run_outcome>           LastOutcome;
		std::uint64_t                                           Refusals = 0U;
		QString                                                 LastRefusal;
		QString                                                 LastModel;
		QString                                                 LastCost;
		phase                                                   Phase = phase::idle;
		std::vector<std::function<void()>>                      ProgressObservers;
		void                                                    progressed();
		std::optional<application::agent_invocation_projection_identity>
			ActiveInvocation;

		agent_launch_panel_statistics Statistics;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_LAUNCH_PANEL_HPP
