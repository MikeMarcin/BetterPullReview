#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_TOUR_AGENT_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_TOUR_AGENT_PANEL_HPP

#include "bpr/core/tour_agent_session.hpp"

#include <QPointer>
#include <QWidget>

#include <functional>
#include <map>
#include <optional>
#include <utility>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QToolButton;
class QVBoxLayout;

namespace bpr::qt {
	/// \brief [[BPR-1900]]. The Tour tab's own agent session, at the top of
	/// the tab: Draft the tour (Stop drafting while it runs), the whole-tour
	/// proposal to accept, edit or discard item by item, and the session's
	/// transcript, which opens only when asked.
	///
	/// \details Presents `core::tour_agent_session` and `core::tour_proposal`
	/// and sends gestures; it decides and performs nothing. The proposal's
	/// texts are editable in place, and what the author typed survives a
	/// presentation of the same proposal.
	class tour_agent_panel final : public QWidget
	{
	public:
		using decide_dispatch = std::function<void(std::size_t, core::tour_proposal_decision, const QString&)>;

		explicit tour_agent_panel(QWidget* aParent = nullptr);
		void set_dispatch(std::function<void()> aDraft, std::function<void()> aStop, decide_dispatch aDecide);
		void present(const core::tour_agent_session& aSession, const std::optional<core::tour_proposal>& aProposal);
		/// \brief A Draft the tour run is running (its button says Stop
		/// drafting), or any run of the session is.
		void set_running(bool aDraftTour, bool aAny);
		/// \brief Usable once the agent is set up; otherwise disabled with why.
		void set_available(bool aReady, const QString& aWhy);
		/// \brief Puts BPR-1892's failure notice for Draft the tour under the
		/// button.
		void set_failure_notice(QWidget* aNotice);
		/// \brief Opens the transcript at `aRun`, or at the newest run.
		void open_transcript(std::optional<core::tour_agent_run_id> aRun = std::nullopt);
		/// \brief The agent accent, from `aGround`'s window.
		void restyle(const QPalette& aGround);

		[[nodiscard]] auto draft_button() const noexcept -> QPushButton* { return Draft; }
		[[nodiscard]] auto transcript_button() const noexcept -> QPushButton* { return TranscriptToggle; }
		[[nodiscard]] auto transcript() const noexcept -> QWidget* { return TranscriptBox; }
		[[nodiscard]] auto proposal() const noexcept -> QWidget* { return ProposalBox; }
		/// \brief The proposal's items at their full height, for a picture.
		[[nodiscard]] auto proposal_items() const -> QWidget*;
		[[nodiscard]] auto status() const noexcept -> QLabel* { return Status; }
		[[nodiscard]] auto session() const noexcept -> const core::tour_agent_session& { return Session; }
		/// \brief The transcript as its lines read, newest run first.
		[[nodiscard]] auto transcript_text() const -> QString;
		/// \brief The proposal item `aIndex`'s editor, while it is pending.
		[[nodiscard]] auto proposal_editor(std::size_t aIndex) const -> QPlainTextEdit*;
		/// \brief Its Accept button, while it is pending.
		[[nodiscard]] auto accept_button(std::size_t aIndex) const -> QPushButton*;

	private:
		void                                   rebuild_proposal();
		void                                   rebuild_transcript();
		void                                   refresh_toggle();
		std::function<void()>                  DraftDispatch;
		std::function<void()>                  StopDispatch;
		decide_dispatch                        DecideDispatch;
		core::tour_agent_session               Session;
		std::optional<core::tour_proposal>     Proposal;
		std::map<std::size_t, QString>         Edited;
		std::map<std::size_t, QPlainTextEdit*> Editors;
		std::map<std::size_t, QPushButton*>    Accepts;
		std::optional<core::tour_agent_run_id> FocusRun;
		bool                                   RunningDraft = false;
		bool                                   RunningAny   = false;
		bool                                   Ready        = true;
		QString                                Why;
		QPushButton*                           Draft            = nullptr;
		QPushButton*                           TranscriptToggle = nullptr;
		QLabel*                                Status           = nullptr;
		QVBoxLayout*                           FailureSlot      = nullptr;
		QWidget*                               ProposalBox      = nullptr;
		QVBoxLayout*                           ProposalItems    = nullptr;
		QLabel*                                ProposalTitle    = nullptr;
		QWidget*                               TranscriptBox    = nullptr;
		QVBoxLayout*                           TranscriptItems  = nullptr;
		QScrollArea*                           TranscriptScroll = nullptr;
		QScrollArea*                           ProposalScroll   = nullptr;
		std::vector<QPointer<QWidget>>         Accented;
		std::vector<QPointer<QWidget>>         Fields;
	};
}

#endif
