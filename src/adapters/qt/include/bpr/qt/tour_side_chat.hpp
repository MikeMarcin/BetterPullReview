#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_TOUR_SIDE_CHAT_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_TOUR_SIDE_CHAT_HPP

#include "bpr/application/markdown_code_syntax.hpp"
#include "bpr/core/review_tour.hpp"
#include "bpr/core/skill_references.hpp"
#include "bpr/core/tour_agent_session.hpp"
#include "bpr/qt/markdown_body_view.hpp"

#include <QFont>
#include <QIcon>
#include <QPointer>
#include <QToolButton>
#include <QWidget>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

class QLabel;
class QPainter;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QHBoxLayout;

class QVBoxLayout;

namespace bpr::qt {
	class skill_completer;
	/// \brief [[BPR-1914]] v003. One question for the tour agent from a side
	/// chat: the chat's part, the passage quoted with it, the words, and
	/// whether it asks for the part rewritten from the chat ("Update this
	/// section from our chat"). Every side chat's composer sends this, through
	/// one dispatch, so a reviewer's own `/skill` references ([[BPR-1916]])
	/// are resolved in one place.
	struct tour_ask
	{
		core::tour_thread                 Thread;
		std::optional<core::tour_passage> Passage;
		QString                           Question;
		bool                              Update = false;
	};

	/// \brief The side chat's glyph: a small speech bubble, in the colour of
	/// `aPalette`'s button text, for the quiet "Open side chat" affordances.
	[[nodiscard]] auto side_chat_icon(const QPalette& aPalette) -> QIcon;

	/// \brief [[BPR-1925]] v002. Draws a part's chat toggle in `aBox`: off, a
	/// speech bubble outlined in the agent accent with the chat's turns
	/// beside it; on, a pill filled with the accent and the bubble and turns
	/// in the ground's colour. The route draws its stops' toggles with it.
	void paint_chat_toggle(QPainter& aPainter, const QRectF& aBox, const QPalette& aPalette, std::size_t aTurns, bool aOn, bool aHovered);
	/// \brief The size `paint_chat_toggle` needs for `aTurns` in `aFont`.
	[[nodiscard]] auto chat_toggle_size(const QFont& aFont, std::size_t aTurns) -> QSize;

	/// \brief [[BPR-1925]] v002, owner on v001: "The mockup had the chat button
	/// on the right side of each of the sections and I prefer that, I also
	/// want the chat button to toggle it open and closed." A part's chat
	/// button, at the right of its header: pressed while that part's chat is
	/// in the column, with the chat's turns. It is a checkable tool button,
	/// so Space and Return work it and it shows the keyboard's focus; its
	/// accessible name says whether it opens or closes the chat.
	class part_chat_toggle final : public QToolButton
	{
	public:
		part_chat_toggle(const QString& aPart, QWidget* aParent = nullptr);
		/// \brief The chat's turns, and whether it is the one in the column.
		void               present(std::size_t aTurns, bool aOn);
		[[nodiscard]] auto turns() const noexcept -> std::size_t { return Turns; }
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override { return sizeHint(); }

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		QString     Part;
		std::size_t Turns = 0U;
	};

	/// \brief [[BPR-1914]] v003, [[BPR-1925]]. Side chats with the tour
	/// agent: the Tour tab's second column, beside the tour.
	///
	/// \details [[BPR-1925]], owner 2026-11-07: "it's kind of weird that the
	/// sidechat is so decoupled from the tour when it's really about the
	/// tour." It no longer docks at the window's edge: the window places this
	/// column beside the tour, in the Tour tab, and it goes with that tab. The
	/// part a chat is about is marked in the tour, each part's glyph opens its
	/// chat, and there are no tabs: one chat shows at a time.
	///
	/// \details The owner rejected v002's inline threads (2026-11-06): "it's
	/// too in the way and noisy. I think I need something more like a side
	/// chat ... On each part I can pop open a side chat, ask some questions and
	/// then ... ask the agent to update the section based on our chat." So the
	/// tour stays as it was, and each part (the tour as a whole, the Problem,
	/// the Solution, a stop) has its own chat here, one tab each, opened from
	/// a small affordance on the part or its context menu, or from "+". The
	/// top says which part a chat is about and Show takes the tour (and a
	/// stop's code in the diff) there. The chat itself is plain: the author's
	/// words in bubbles on the right, the agent's replies as rendered Markdown
	/// on the left with their code highlighted ([[BPR-1919]]), "Worked for Ns"
	/// to see what it read, Copy on each reply and Copy chat for all of it.
	/// One clear action, Update the part from this chat, asks for the part
	/// rewritten from the conversation; the result is the part's proposal,
	/// accepted, edited or discarded right in the chat.
	///
	/// \details Presents `core::tour_agent_session` and `core::tour_proposal`
	/// and sends gestures; it decides and performs nothing.
	class tour_side_chat final : public QWidget
	{
	public:
		using ask_dispatch    = std::function<void(const tour_ask&)>;
		using decide_dispatch = std::function<void(std::size_t, core::tour_proposal_decision, const QString&)>;

		explicit tour_side_chat(QWidget* aParent = nullptr);
		void set_dispatch(ask_dispatch aAsk, std::function<void()> aStop, decide_dispatch aDecide);
		/// \brief Show: brings the tour to the chat's part, and a stop's code
		/// into the diff.
		void set_reveal_dispatch(std::function<void(const core::tour_thread&)> aReveal) { RevealDispatch = std::move(aReveal); }
		/// \brief The column wants to be shown (true) or closed (false).
		void set_visibility_dispatch(std::function<void(bool)> aVisibility) { VisibilityDispatch = std::move(aVisibility); }
		/// \brief Opens (or brings forward) the chat about `aThread`, with
		/// `aPassage` quoted above its composer, and puts the keyboard there
		/// unless `aFocus` is false (the chat following the route).
		void open(const core::tour_thread& aThread, std::optional<core::tour_passage> aPassage = std::nullopt, bool aFocus = true);
		/// \brief Closes the chat about `aThread`; its turns stay in the
		/// session, and reopening it shows them again.
		void close(const core::tour_thread& aThread);
		/// \brief The tour as it is now names each part and quotes it.
		void present(const core::tour_agent_session& aSession, const std::optional<core::tour_proposal>& aProposal, std::shared_ptr<const core::review_tour> aDraft);
		/// \brief The tour as it is now, between sessions' presentations.
		void set_draft(std::shared_ptr<const core::review_tour> aDraft);
		/// \brief [[BPR-1930]]. `aDrafting`: the run is Draft the tour, and the
		/// tour's chat still takes feedback, which waits for its proposal.
		void set_running(bool aAny, bool aDrafting = false);
		void set_available(bool aReady, const QString& aWhy);
		/// \brief [[BPR-1930]]. Feedback kept until the draft proposes, shown
		/// in the tour's chat while it waits.
		[[nodiscard]] auto waiting_feedback() const noexcept -> QWidget* { return WaitingBubble; }
		/// \brief [[BPR-1930]]. The tour chat's line saying where its proposal
		/// waits, for the newest turn that proposed.
		[[nodiscard]] auto in_tour_line() const noexcept -> QLabel* { return InTour; }
		/// \brief [[BPR-1919]]. Replies and proposals set their code as the
		/// diff's comments do.
		void set_code_style(const QFont& aFont, qreal aTabStopDistance, std::optional<markdown_code_theme> aTheme, std::function<void(std::vector<application::markdown_code_syntax_request>)> aDispatch);
		void apply_code_syntax(const application::markdown_code_syntax_publication& aSpans);
		/// \brief The agent accent, from `aGround`'s window.
		void restyle(const QPalette& aGround);
		/// \brief [[BPR-1916]]. The skills `/name` completion offers in the
		/// composer, each with where it came from.
		void               present_skills(std::vector<core::skill_entry> aSkills);
		[[nodiscard]] auto skill_completion() const noexcept -> skill_completer* { return SkillCompletion; }

		/// \brief [[BPR-1925]]. Told when the chat shown, the column's state
		/// or the session changes, so the tour can mark the part and count the
		/// chats.
		void               set_change_observer(std::function<void()> aObserver) { ChangeObserver = std::move(aObserver); }
		[[nodiscard]] auto open_threads() const noexcept -> const std::vector<core::tour_thread>& { return Open; }
		[[nodiscard]] auto current() const -> std::optional<core::tour_thread>;
		/// \brief The parts whose chat was shown in this review, the shown one
		/// among them.
		[[nodiscard]] auto chat_count() const noexcept -> std::size_t { return Open.size(); }
		/// \brief The shown chat's place among `open_threads()`, or -1.
		[[nodiscard]] auto current_index() const noexcept -> int { return Current; }
		void               select(int aIndex);
		/// \brief [[BPR-1925]]. Whether the column is open (it shows while the
		/// Tour tab does), and the part it is about while it is.
		[[nodiscard]] auto column_open() const noexcept -> bool { return ColumnOpen; }
		[[nodiscard]] auto discussed() const -> std::optional<core::tour_thread>;
		/// \brief Opens or closes the column; closed, every chat is kept.
		void set_column_open(bool aOpen);
		/// \brief [[BPR-1925]]. Turns in `aThread`'s chat, for its glyph.
		[[nodiscard]] auto turns_about(const core::tour_thread& aThread) const -> std::size_t;
		/// \brief [[BPR-1925]]. How many parts have a chat, for the Tour tab's
		/// badge (`core::chat_threads`).
		[[nodiscard]] auto chat_parts() const -> std::size_t;
		[[nodiscard]] auto composer() const noexcept -> QPlainTextEdit* { return Composer; }
		[[nodiscard]] auto send_button() const noexcept -> QPushButton* { return Send; }
		[[nodiscard]] auto update_button() const noexcept -> QPushButton* { return Update; }
		[[nodiscard]] auto close_button() const noexcept -> QToolButton* { return ClosePane; }
		[[nodiscard]] auto copy_chat_button() const noexcept -> QToolButton* { return CopyChat; }
		[[nodiscard]] auto show_button() const noexcept -> QPushButton* { return ShowPart; }
		[[nodiscard]] auto subject() const noexcept -> QLabel* { return Subject; }
		[[nodiscard]] auto passage_line() const noexcept -> QLabel* { return PassageLine; }
		[[nodiscard]] auto passage() const -> std::optional<core::tour_passage>;
		/// \brief How many turns the open chat shows.
		[[nodiscard]] auto turn_count() const noexcept -> std::size_t { return Bubbles.size(); }
		/// \brief Turn `aTurn`'s bubble, reply, Worked disclosure and Copy.
		[[nodiscard]] auto bubble(std::size_t aTurn) const -> QWidget*;
		[[nodiscard]] auto reply(std::size_t aTurn) const -> markdown_body_view*;
		[[nodiscard]] auto worked_button(std::size_t aTurn) const -> QToolButton*;
		[[nodiscard]] auto steps(std::size_t aTurn) const -> QWidget*;
		[[nodiscard]] auto copy_reply_button(std::size_t aTurn) const -> QToolButton*;
		/// \brief Proposal item `aIndex`'s Accept and rendered text, while it is
		/// pending and shown in the open chat.
		[[nodiscard]] auto accept_button(std::size_t aIndex) const -> QPushButton*;
		[[nodiscard]] auto proposal_reader(std::size_t aIndex) const -> markdown_body_view*;
		/// \brief The open chat as plain text that reads as Markdown: what Copy
		/// chat puts on the clipboard.
		[[nodiscard]] auto chat_text() const -> QString;
		/// \brief A reply as written, what its Copy puts on the clipboard.
		[[nodiscard]] static auto reply_text(const core::tour_agent_run& aRun) -> QString;

	protected:
		bool eventFilter(QObject* aWatched, QEvent* aEvent) override;
		/// [[BPR-1925]]. The seam with the tour: a line of the agent's accent
		/// down the column's left edge, and the faintest wash beside it.
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		void rebuild();
		void changed();
		/// [[BPR-1914]] v004. The file inline code in a chat is coloured as:
		/// a stop's own file, or the language most of the tour's stops are in.
		[[nodiscard]] auto inline_selector() const -> std::string;
		void               refresh_header();
		void               refresh_actions();
		void               show_passage();
		void               send(const QString& aQuestion, bool aUpdate);
		void               fit();
		void               to_bottom();
		[[nodiscard]] auto name_of(const core::tour_thread& aThread) const -> QString;
		[[nodiscard]] auto reader(QWidget* aParent, const std::string& aBody, const std::string& aIdentity, const std::string& aInlineSelector) -> markdown_body_view*;
		[[nodiscard]] auto index_of(const core::tour_thread& aThread) const -> int;

		ask_dispatch                                                                AskDispatch;
		std::function<void()>                                                       StopDispatch;
		decide_dispatch                                                             DecideDispatch;
		std::function<void(const core::tour_thread&)>                               RevealDispatch;
		std::function<void(bool)>                                                   VisibilityDispatch;
		std::function<void()>                                                       ChangeObserver;
		bool                                                                        ColumnOpen = false;
		core::tour_agent_session                                                    Session;
		std::optional<core::tour_proposal>                                          Proposal;
		std::shared_ptr<const core::review_tour>                                    Draft;
		std::vector<core::tour_thread>                                              Open;
		std::map<std::size_t, core::tour_passage>                                   Passages;
		std::set<std::uint64_t>                                                     Expanded;
		std::map<std::size_t, QString>                                              Edited;
		std::set<std::size_t>                                                       Editing;
		bool                                                                        Running       = false;
		bool                                                                        Drafting      = false;
		QWidget*                                                                    WaitingBubble = nullptr;
		QLabel*                                                                     InTour        = nullptr;
		[[nodiscard]] auto                                                          takes_feedback() const -> bool;
		bool                                                                        Mine  = false;
		bool                                                                        Ready = true;
		QString                                                                     Why;
		std::size_t                                                                 Shown = 0U;
		bool                                                                        Stick = false;
		QFont                                                                       CodeFont;
		qreal                                                                       TabStop = 0.0;
		std::optional<markdown_code_theme>                                          CodeTheme;
		std::function<void(std::vector<application::markdown_code_syntax_request>)> CodeDispatch;
		int                                                                         Current         = -1;
		QLabel*                                                                     Glyph           = nullptr;
		QToolButton*                                                                CopyChat        = nullptr;
		QToolButton*                                                                ClosePane       = nullptr;
		QLabel*                                                                     Subject         = nullptr;
		QLabel*                                                                     Quote           = nullptr;
		QPushButton*                                                                ShowPart        = nullptr;
		QScrollArea*                                                                Scroll          = nullptr;
		QVBoxLayout*                                                                Turns           = nullptr;
		QPushButton*                                                                Update          = nullptr;
		QWidget*                                                                    PassageRow      = nullptr;
		QLabel*                                                                     PassageLine     = nullptr;
		QToolButton*                                                                ClearPassage    = nullptr;
		QPlainTextEdit*                                                             Composer        = nullptr;
		QPushButton*                                                                Send            = nullptr;
		skill_completer*                                                            SkillCompletion = nullptr;
		std::vector<QWidget*>                                                       Bubbles;
		std::vector<markdown_body_view*>                                            Replies;
		std::vector<QToolButton*>                                                   Worked;
		std::vector<QWidget*>                                                       Steps;
		std::vector<QToolButton*>                                                   Copies;
		std::vector<QPointer<markdown_body_view>>                                   Readers;
		std::map<std::size_t, QPushButton*>                                         Accepts;
		std::map<std::size_t, markdown_body_view*>                                  ProposalReaders;
		std::vector<QPointer<QWidget>>                                              Accented;
	};
}

#endif
