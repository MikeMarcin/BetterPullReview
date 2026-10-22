#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_LAUNCH_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_LAUNCH_PANEL_HPP

#include "bpr/application/agent_invocation_host.hpp"

#include <QString>
#include <QWidget>

#include <cstddef>
#include <functional>
#include <optional>
#include <span>
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

		/// \brief Offers the profiles this host permits, and no others.
		///
		/// \details The list is what `core::decide_agent_invocation` already
		/// permitted. This widget runs no policy of its own: a profile absent
		/// here is absent because the host refused it, and one present cannot
		/// be refused again by presentation.
		void offer_profiles(std::span<const core::agent_profile> aProfiles);
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
		/// \brief [[BPR-PENDING-AO]]. Why the last run stopped and what to do,
		/// in a reviewer's words, when there is more to say than its outcome.
		[[nodiscard]] auto stop_reason_text() const -> QString;
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

		std::function<void()>                          ConfigurationObserver;
		agent_intent_dispatch                          Dispatch;
		QComboBox*                                     ProfileSelector        = nullptr;
		QAction*                                       StartAction            = nullptr;
		QAction*                                       CancelAction           = nullptr;
		QAction*                                       ExistingCheckoutAction = nullptr;
		QLabel*                                        Disclosure             = nullptr;
		QListWidget*                                   NoteSelector           = nullptr;
		QCheckBox*                                     DraftSelector          = nullptr;
		QLabel*                                        SessionCatalog         = nullptr;
		QLabel*                                        Status                 = nullptr;
		QPlainTextEdit*                                Output                 = nullptr;
		QToolButton*                                   StartButton            = nullptr;
		QToolButton*                                   CancelButton           = nullptr;
		QToolButton*                                   ExistingCheckoutButton = nullptr;
		bool                                           Running                = false;
		bool                                           Disclosed              = false;
		std::optional<application::agent_draft_choice> OfferedDraft;
		QString                                        StopReason;
		std::optional<application::agent_invocation_projection_identity>
			ActiveInvocation;

		agent_launch_panel_statistics Statistics;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_AGENT_LAUNCH_PANEL_HPP
