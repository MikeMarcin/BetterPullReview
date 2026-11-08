#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_TOUR_AGENT_PANEL_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_TOUR_AGENT_PANEL_HPP

#include "bpr/core/skill_references.hpp"
#include "bpr/core/tour_agent_session.hpp"
#include "bpr/qt/markdown_body_view.hpp"

#include <QPointer>
#include <QWidget>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <utility>

class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QTextBrowser;
class QToolButton;
class QLayout;
class QVBoxLayout;
class QTextCursor;

namespace bpr::qt {
	namespace detail {
		class markdown_code_store;
		class markdown_code_block_objects;
	}
	class markdown_body_view;
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
	/// \details [[BPR-1914]], RFC-BPR-0092 T1 superseded. The window is the
	/// tour conversation: every turn, oldest first, as one selectable
	/// document (what was asked and about which passage, what it read, and the
	/// reply rendered as Markdown). v002 (owner, 2026-11-06: "how do I do that
	/// if I can't see the tour or the code?"): it is a secondary, copyable
	/// record only. Questions are asked in threads under the part of the tour
	/// they are about. v003: in side chats docked at the window's right edge
	/// (`tour_side_chat`); this panel opens the chat for
	/// the tour as a whole, under Draft the tour, and leaves the parts an ask
	/// turn proposed to that turn's thread.
	/// [[BPR-1915]]. A proposed text shows rendered, as the preview and GitHub
	/// show it; Edit text shows its Markdown source to change before Accept.
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
		/// \brief [[BPR-1925]] v003, owner on v002: "I think this whole section
		/// probably just goes away and we move the accept into the stops
		/// directly". Each pending part of a proposal is offered to `aPlace`,
		/// which puts its box (heading, text as it will read, Edit text,
		/// Discard and Accept) in the tour, on the part it changes; a part it
		/// does not place stays in this panel. `aBegin` is told first, each
		/// time the proposal is laid out again, so the old places are cleared.
		using proposal_placer = std::function<bool(std::size_t, const core::tour_proposal_item&, QWidget*)>;
		void set_proposal_placer(std::function<void()> aBegin, proposal_placer aPlace)
		{
			PlaceBegin = std::move(aBegin);
			Place      = std::move(aPlace);
		}
		/// \brief The proposal's one line: how many parts wait in the tour, and
		/// Accept all.
		[[nodiscard]] auto accept_all_button() const noexcept -> QPushButton* { return AcceptAll; }
		[[nodiscard]] auto proposal_title() const noexcept -> QLabel* { return ProposalTitle; }
		/// \brief [[BPR-1914]] v002. The thread for the tour as a whole, under
		/// Draft the tour; Ask about the tour opens it.
		/// \brief [[BPR-1914]] v003. Opens the side chat about the whole tour.
		void               set_chat_dispatch(std::function<void()> aChat) { ChatDispatch = std::move(aChat); }
		[[nodiscard]] auto chat_button() const noexcept -> QToolButton* { return ChatTour; }
		/// \brief [[BPR-1930]], [[BPR-1931]]. The focus chip's ×, Reset tour
		/// (once confirmed) and Undo reset. The host clears, resets and
		/// restores; this panel only asks.
		void set_shape_dispatch(std::function<void()> aClearFocus, std::function<void()> aReset, std::function<void()> aUndoReset);
		/// \brief [[BPR-1930]]. The tour as it is, to name the stops a
		/// structural part changes by their numbers and titles.
		void set_draft(std::shared_ptr<const core::review_tour> aDraft) { Tour = std::move(aDraft); }
		/// \brief [[BPR-1931]]. What Reset tour would clear, whether it can, and
		/// whether the last reset can still be undone.
		void present_reset(const core::tour_reset_tally& aTally, bool aAvailable, bool aUndoable);
		/// \brief [[BPR-1931]]. The confirmation's sentence: what is lost, and
		/// for a published tour that only the local draft is reset.
		[[nodiscard]] static auto reset_words(const core::tour_reset_tally& aTally) -> QString;
		/// \brief [[BPR-1930]]. The remembered focus, as a chip with ×.
		[[nodiscard]] auto focus_chip() const noexcept -> QWidget* { return FocusChip; }
		[[nodiscard]] auto focus_text() const noexcept -> QLabel* { return FocusText; }
		[[nodiscard]] auto focus_clear_button() const noexcept -> QToolButton* { return FocusClear; }
		[[nodiscard]] auto reset_button() const noexcept -> QToolButton* { return Reset; }
		[[nodiscard]] auto reset_confirm() const noexcept -> QWidget* { return ResetConfirm; }
		[[nodiscard]] auto reset_sentence() const noexcept -> QLabel* { return ResetSentence; }
		[[nodiscard]] auto reset_confirm_button() const noexcept -> QPushButton* { return ResetYes; }
		[[nodiscard]] auto reset_cancel_button() const noexcept -> QPushButton* { return ResetNo; }
		[[nodiscard]] auto reset_done() const noexcept -> QWidget* { return ResetDone; }
		[[nodiscard]] auto undo_reset_button() const noexcept -> QPushButton* { return UndoReset; }
		/// \brief [[BPR-1919]]. Proposed texts' code: font, colours and the
		/// highlighter.
		void set_code_style(const QFont& aFont, qreal aTabStopDistance, std::optional<markdown_code_theme> aTheme, std::function<void(std::vector<application::markdown_code_syntax_request>)> aDispatch);
		void apply_code_syntax(const application::markdown_code_syntax_publication& aSpans);
		/// \brief [[BPR-1915]]. A pending item's rendered text, and its Edit
		/// text button, which shows the Markdown source instead.
		[[nodiscard]] auto proposal_reader(std::size_t aIndex) const -> markdown_body_view*;
		[[nodiscard]] auto edit_button(std::size_t aIndex) const -> QPushButton*;
		/// \brief [[BPR-1914]]. The words a turn's proposal is said in.
		[[nodiscard]] static auto proposed_words(const core::tour_proposal_tally& aTally) -> QString;
		/// \brief [[BPR-1937]]. A part of a revision the core refused, said
		/// in plain words: which part, which stops, and why.
		[[nodiscard]] static auto refused_words(const core::tour_refused_part& aPart) -> QString;
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
		/// \brief [[BPR-1937]]. The parts of the newest revision the core refused.
		[[nodiscard]] auto refused_line() const noexcept -> QLabel* { return RefusedLine; }
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

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;

	private:
		/// [[BPR-1925]]. Conversation and the tour's chat glyph sit at the
		/// right of Draft the tour, or on their own row under it when the
		/// drawer is too narrow for both at their own widths.
		void                             fit_head();
		QLayout*                         Head  = nullptr;
		QWidget*                         Trail = nullptr;
		void                             fit_readers();
		void                             rebuild_proposal();
		void                             rebuild_transcript();
		void                             show_run(std::optional<core::tour_agent_run_id> aRun, std::optional<std::size_t> aEntry);
		void                             copy(const QString& aText, const QString& aSaid);
		void                             append_run(QTextCursor& aCursor, const core::tour_agent_run& aRun, const QString& aMuted, const QString& aCode) const;
		void                             style_transcript_code();
		void                             refresh_toggle();
		std::function<void()>            DraftDispatch;
		std::function<void()>            StopDispatch;
		decide_dispatch                  DecideDispatch;
		std::function<void(std::size_t)> RelocateDispatch;
		std::function<void()>            PlaceBegin;
		proposal_placer                  Place;
		QPushButton*                     AcceptAll = nullptr;
		/// [[BPR-1925]] v003. Each pending part's box, wherever it was placed.
		std::map<std::size_t, QPointer<QWidget>>                                    Boxes;
		std::map<std::size_t, markdown_body_view*>                                  Readers;
		std::map<std::size_t, QPushButton*>                                         EditButtons;
		std::set<std::size_t>                                                       Editing;
		QToolButton*                                                                ChatTour = nullptr;
		std::function<void()>                                                       ChatDispatch;
		QFont                                                                       CodeFont;
		qreal                                                                       TabStop = 0.0;
		std::optional<markdown_code_theme>                                          CodeTheme;
		std::function<void(std::vector<application::markdown_code_syntax_request>)> CodeDispatch;
		std::map<std::size_t, QPushButton*>                                         Relocates;
		std::map<std::size_t, QLabel*>                                              Placements;
		core::tour_agent_session                                                    Session;
		std::optional<core::tour_proposal>                                          Proposal;
		std::map<std::size_t, QString>                                              Edited;
		std::map<std::size_t, QPlainTextEdit*>                                      Editors;
		std::map<std::size_t, QPushButton*>                                         Accepts;
		std::optional<core::tour_agent_run_id>                                      FocusRun;
		std::optional<core::tour_agent_run_id>                                      ShownRun;
		bool                                                                        RunningDraft = false;
		bool                                                                        RunningAny   = false;
		bool                                                                        Ready        = true;
		QString                                                                     Why;
		QPushButton*                                                                Draft            = nullptr;
		QPushButton*                                                                TranscriptToggle = nullptr;
		QLabel*                                                                     Status           = nullptr;
		QVBoxLayout*                                                                FailureSlot      = nullptr;
		QWidget*                                                                    ProposalBox      = nullptr;
		QVBoxLayout*                                                                ProposalItems    = nullptr;
		QLabel*                                                                     ProposalTitle    = nullptr;
		QLabel*                                                                     LeftOut          = nullptr;
		QLabel*                                                                     RefusedLine      = nullptr;
		QLabel*                                                                     OutsideList      = nullptr;
		QWidget*                                                                    TranscriptWindow = nullptr;
		QListWidget*                                                                RunList          = nullptr;
		QTextBrowser*                                                               RunView          = nullptr;
		std::shared_ptr<detail::markdown_code_store>                                TranscriptCode;
		detail::markdown_code_block_objects*                                        TranscriptFrames = nullptr;
		QPushButton*                                                                CopyRun          = nullptr;
		QPushButton*                                                                CopyAll          = nullptr;
		QLabel*                                                                     Copied           = nullptr;
		QScrollArea*                                                                ProposalScroll   = nullptr;
		std::vector<QPointer<QWidget>>                                              Accented;
		std::vector<QPointer<QWidget>>                                              Fields;
		/// [[BPR-1930]]. A structural part's words: what it changes, by the
		/// tour's numbers and titles.
		[[nodiscard]] auto                       stop_number(core::tour_stop_id aStop) const -> int;
		[[nodiscard]] auto                       stop_title(core::tour_stop_id aStop) const -> QString;
		void                                     add_revision_lines(QVBoxLayout* aColumn, const core::tour_proposal_item& aItem, QWidget* aBox);
		std::shared_ptr<const core::review_tour> Tour;
		QVBoxLayout*                             FocusSlot     = nullptr;
		QWidget*                                 FocusChip     = nullptr;
		QLabel*                                  FocusText     = nullptr;
		QToolButton*                             FocusClear    = nullptr;
		QToolButton*                             Reset         = nullptr;
		QWidget*                                 ResetConfirm  = nullptr;
		QLabel*                                  ResetSentence = nullptr;
		QPushButton*                             ResetYes      = nullptr;
		QPushButton*                             ResetNo       = nullptr;
		QWidget*                                 ResetDone     = nullptr;
		QPushButton*                             UndoReset     = nullptr;
		core::tour_reset_tally                   ResetTally;
		bool                                     ResetAvailable = false;
		std::function<void()>                    ClearFocusDispatch;
		std::function<void()>                    ResetDispatch;
		std::function<void()>                    UndoResetDispatch;
		void                                     refresh_shape();
	};
}

#endif
