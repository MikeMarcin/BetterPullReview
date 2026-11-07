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
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QTextBrowser;
class QToolButton;
class QVBoxLayout;

namespace bpr::qt {
	/// \brief [[BPR-1900]]. The Tour tab's own agent session, at the top of
	/// the tab: Draft the tour (Stop drafting while it runs), the whole-tour
	/// proposal to accept, edit or discard item by item, and the session's
	/// transcript, which opens only when asked.
	///
	/// \details [[BPR-1913]], RFC-BPR-0092 T2 amended. The transcript is a
	/// window of its own, beside the main window, so the Tour tab's editing
	/// column keeps its room: the runs on the left, the chosen run as one
	/// selectable text on the right, and Copy run and Copy all runs, which
	/// put plain text on the clipboard. Draft the tour is the tab's one
	/// bordered action; Transcript is a bare action at the row's far end, so
	/// neither reads as a tab.
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
		/// \brief [[BPR-1900]]. Move to nearest change on a stop off the change.
		void set_relocate_dispatch(std::function<void(std::size_t)> aRelocate) { RelocateDispatch = std::move(aRelocate); }
		/// \brief Its Move to nearest change button, while it is offered.
		[[nodiscard]] auto relocate_button(std::size_t aIndex) const -> QPushButton*;
		/// \brief The quiet words under a stop's place, when it has some.
		[[nodiscard]] auto placement_text(std::size_t aIndex) const -> QString;
		void               present(const core::tour_agent_session& aSession, const std::optional<core::tour_proposal>& aProposal);
		/// \brief A Draft the tour run is running (its button says Stop
		/// drafting), or any run of the session is.
		void set_running(bool aDraftTour, bool aAny);
		/// \brief Usable once the agent is set up; otherwise disabled with why.
		void set_available(bool aReady, const QString& aWhy);
		/// \brief Puts BPR-1892's failure notice for Draft the tour under the
		/// button.
		void set_failure_notice(QWidget* aNotice);
		/// \brief Opens the transcript window at `aRun`, or at the newest
		/// run; with `aEntry`, scrolled to that entry of the run.
		void open_transcript(std::optional<core::tour_agent_run_id> aRun = std::nullopt, std::optional<std::size_t> aEntry = std::nullopt);
		/// \brief The agent accent, from `aGround`'s window.
		void restyle(const QPalette& aGround);

		[[nodiscard]] auto draft_button() const noexcept -> QPushButton* { return Draft; }
		[[nodiscard]] auto transcript_button() const noexcept -> QPushButton* { return TranscriptToggle; }
		/// \brief [[BPR-1913]]. The transcript's own window.
		[[nodiscard]] auto transcript() const noexcept -> QWidget* { return TranscriptWindow; }
		/// \brief The shown run's text, selectable across its entries.
		[[nodiscard]] auto transcript_view() const noexcept -> QTextBrowser* { return RunView; }
		/// \brief The transcript's list of runs, newest first.
		[[nodiscard]] auto transcript_runs() const noexcept -> QListWidget* { return RunList; }
		[[nodiscard]] auto copy_run_button() const noexcept -> QPushButton* { return CopyRun; }
		[[nodiscard]] auto copy_all_button() const noexcept -> QPushButton* { return CopyAll; }
		/// \brief [[BPR-1912]]. The proposal's quiet line saying how many
		/// commands Claude ran outside the read-only list.
		[[nodiscard]] auto outside_list() const noexcept -> QLabel* { return OutsideList; }
		/// \brief The run shown in the transcript window, if any.
		[[nodiscard]] auto shown_run() const noexcept -> std::optional<core::tour_agent_run_id> { return ShownRun; }
		[[nodiscard]] auto proposal() const noexcept -> QWidget* { return ProposalBox; }
		/// \brief The proposal's items at their full height, for a picture.
		[[nodiscard]] auto proposal_items() const -> QWidget*;
		[[nodiscard]] auto status() const noexcept -> QLabel* { return Status; }
		/// \brief The quiet line saying how many stops were left out.
		[[nodiscard]] auto left_out() const noexcept -> QLabel* { return LeftOut; }
		[[nodiscard]] auto session() const noexcept -> const core::tour_agent_session& { return Session; }
		/// \brief The transcript as its lines read, newest run first.
		[[nodiscard]] auto transcript_text() const -> QString;
		/// \brief [[BPR-1913]]. What Copy run puts on the clipboard: the run's
		/// request, how it ended, then each step's kind and what it was
		/// asked, with its note, in order, ending with the answer or the
		/// failure's reason. Plain text that reads as Markdown.
		[[nodiscard]] static auto run_text(const core::tour_agent_run& aRun) -> QString;
		/// \brief What Copy all runs puts on the clipboard: every run, oldest
		/// first.
		[[nodiscard]] auto session_text() const -> QString;
		/// \brief The proposal item `aIndex`'s editor, while it is pending.
		[[nodiscard]] auto proposal_editor(std::size_t aIndex) const -> QPlainTextEdit*;
		/// \brief Its Accept button, while it is pending.
		[[nodiscard]] auto accept_button(std::size_t aIndex) const -> QPushButton*;

	private:
		void                                   rebuild_proposal();
		void                                   rebuild_transcript();
		void                                   show_run(std::optional<core::tour_agent_run_id> aRun, std::optional<std::size_t> aEntry);
		void                                   copy(const QString& aText, const QString& aSaid);
		void                                   refresh_toggle();
		std::function<void()>                  DraftDispatch;
		std::function<void()>                  StopDispatch;
		decide_dispatch                        DecideDispatch;
		std::function<void(std::size_t)>       RelocateDispatch;
		std::map<std::size_t, QPushButton*>    Relocates;
		std::map<std::size_t, QLabel*>         Placements;
		core::tour_agent_session               Session;
		std::optional<core::tour_proposal>     Proposal;
		std::map<std::size_t, QString>         Edited;
		std::map<std::size_t, QPlainTextEdit*> Editors;
		std::map<std::size_t, QPushButton*>    Accepts;
		std::optional<core::tour_agent_run_id> FocusRun;
		std::optional<core::tour_agent_run_id> ShownRun;
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
		QLabel*                                LeftOut          = nullptr;
		QLabel*                                OutsideList      = nullptr;
		QWidget*                               TranscriptWindow = nullptr;
		QListWidget*                           RunList          = nullptr;
		QTextBrowser*                          RunView          = nullptr;
		QPushButton*                           CopyRun          = nullptr;
		QPushButton*                           CopyAll          = nullptr;
		QLabel*                                Copied           = nullptr;
		QScrollArea*                           ProposalScroll   = nullptr;
		std::vector<QPointer<QWidget>>         Accented;
		std::vector<QPointer<QWidget>>         Fields;
	};
}

#endif
