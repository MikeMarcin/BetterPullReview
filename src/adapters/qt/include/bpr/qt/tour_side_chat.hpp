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
#include <QWidget>

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QTabBar;
class QToolButton;
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

	/// \brief [[BPR-1914]] v003. Side chats with the tour agent, docked at the
	/// window's right edge.
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
		/// \brief The pane wants to be shown (true) or closed (false).
		void set_visibility_dispatch(std::function<void(bool)> aVisibility) { VisibilityDispatch = std::move(aVisibility); }
		/// \brief Opens (or brings forward) the chat about `aThread`, with
		/// `aPassage` quoted above its composer, and puts the keyboard there.
		void open(const core::tour_thread& aThread, std::optional<core::tour_passage> aPassage = std::nullopt);
		/// \brief Closes the chat about `aThread`; its turns stay in the
		/// session, and reopening it shows them again.
		void close(const core::tour_thread& aThread);
		/// \brief The tour as it is now names each part and quotes it.
		void present(const core::tour_agent_session& aSession, const std::optional<core::tour_proposal>& aProposal, std::shared_ptr<const core::review_tour> aDraft);
		/// \brief The tour as it is now, between sessions' presentations.
		void set_draft(std::shared_ptr<const core::review_tour> aDraft);
		void set_running(bool aAny);
		void set_available(bool aReady, const QString& aWhy);
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

		[[nodiscard]] auto open_threads() const noexcept -> const std::vector<core::tour_thread>& { return Open; }
		[[nodiscard]] auto current() const -> std::optional<core::tour_thread>;
		[[nodiscard]] auto tabs() const noexcept -> QTabBar* { return Tabs; }
		[[nodiscard]] auto composer() const noexcept -> QPlainTextEdit* { return Composer; }
		[[nodiscard]] auto send_button() const noexcept -> QPushButton* { return Send; }
		[[nodiscard]] auto update_button() const noexcept -> QPushButton* { return Update; }
		[[nodiscard]] auto add_button() const noexcept -> QToolButton* { return Add; }
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

	private:
		void               rebuild();
		void               refresh_header();
		void               refresh_actions();
		void               show_passage();
		void               send(const QString& aQuestion, bool aUpdate);
		void               fit();
		void               to_bottom();
		[[nodiscard]] auto name_of(const core::tour_thread& aThread) const -> QString;
		[[nodiscard]] auto reader(QWidget* aParent, const std::string& aBody, const std::string& aIdentity) -> markdown_body_view*;
		[[nodiscard]] auto index_of(const core::tour_thread& aThread) const -> int;

		ask_dispatch                                                                AskDispatch;
		std::function<void()>                                                       StopDispatch;
		decide_dispatch                                                             DecideDispatch;
		std::function<void(const core::tour_thread&)>                               RevealDispatch;
		std::function<void(bool)>                                                   VisibilityDispatch;
		core::tour_agent_session                                                    Session;
		std::optional<core::tour_proposal>                                          Proposal;
		std::shared_ptr<const core::review_tour>                                    Draft;
		std::vector<core::tour_thread>                                              Open;
		std::map<std::size_t, core::tour_passage>                                   Passages;
		std::set<std::uint64_t>                                                     Expanded;
		std::map<std::size_t, QString>                                              Edited;
		std::set<std::size_t>                                                       Editing;
		bool                                                                        Running = false;
		bool                                                                        Mine    = false;
		bool                                                                        Ready   = true;
		QString                                                                     Why;
		std::size_t                                                                 Shown = 0U;
		bool                                                                        Stick = false;
		QFont                                                                       CodeFont;
		qreal                                                                       TabStop = 0.0;
		std::optional<markdown_code_theme>                                          CodeTheme;
		std::function<void(std::vector<application::markdown_code_syntax_request>)> CodeDispatch;
		QTabBar*                                                                    Tabs            = nullptr;
		QToolButton*                                                                Add             = nullptr;
		QToolButton*                                                                CopyChat        = nullptr;
		QToolButton*                                                                ClosePane       = nullptr;
		QLabel*                                                                     Subject         = nullptr;
		QLabel*                                                                     Quote           = nullptr;
		QPushButton*                                                                ShowPart        = nullptr;
		QScrollArea*                                                                Scroll          = nullptr;
		QVBoxLayout*                                                                Turns           = nullptr;
		QLabel*                                                                     Empty           = nullptr;
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
