#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ITEM_SIDE_CHAT_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_ITEM_SIDE_CHAT_HPP

#include "bpr/application/markdown_code_syntax.hpp"
#include "bpr/core/agent_follow_up.hpp"
#include "bpr/core/chat_transcript.hpp"
#include "bpr/core/skill_references.hpp"
#include "bpr/qt/chat_composer.hpp"
#include "bpr/qt/chat_header_view.hpp"

#include <QElapsedTimer>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QToolButton;

/// \file
/// \brief [[BPR-2322]] (owner, 2026-11-20: "I think we should instead make it
/// use something akin to the side chat from the tour onto the report tab").
/// The side chat about one review item: a report's finding or a finding card,
/// a draft, a thread. It is the column beside the report (or the code), built
/// from the Tour tab's side chat parts (`side_chat_parts.hpp`) and wearing its
/// look: the seam, the item at the top with Copy chat and Close, the
/// reviewer's words in bubbles on the right, the agent's replies as Markdown
/// on the left with the one ending line, a proposal in the chat beside what it
/// replaces, and the composer with the item as its chip. RFC-BPR-0130 stage 3:
/// its conversation is drawn by the one transcript (`chat_transcript_view`)
/// from `core::project_follow_up`, the proposal as the turn's attachment.
///
/// \details It replaces [[BPR-2286]]'s follow-up window. Its conversation is
/// `core::follow_up_state`, held by `application::follow_up_host`: one chat a
/// item, each round a fresh run, and the one thing it can apply (Update
/// suggested comment, Update draft or Draft reply) used only on Use. It runs
/// nothing and writes nothing: each press leaves as an intent.
namespace bpr::qt {
	class markdown_body_view;
	class agent_run_status_line;
	class chat_transcript_view;
	class skill_completer;

	/// \brief What a press asks for.
	struct item_side_chat_intents
	{
		std::function<void(const QString& aQuestion)> Ask;
		std::function<void(core::follow_up_apply)>    Propose;
		std::function<void(std::uint64_t aProposal)>  Use;
		std::function<void(std::uint64_t aProposal)>  Discard;
		std::function<void()>                         Stop;
		/// The column closed; the chat is kept for its item.
		std::function<void()> Closed;
		/// Show: the item's place in the code.
		std::function<void(const core::follow_up_subject&)> Reveal;
	};

	/// \brief The words for one apply action: "Update suggested comment",
	/// "Update draft", "Draft reply".
	[[nodiscard]] auto item_chat_apply_words(core::follow_up_apply aApply) -> QString;
	/// \brief Why a step did nothing, in the person's words; empty for none.
	[[nodiscard]] auto item_chat_refusal_words(core::follow_up_refusal aRefusal) -> QString;
	/// \brief What the chat is about: "Suggested comment at renderer.cpp:12–13".
	[[nodiscard]] auto item_chat_subject_words(const core::follow_up_subject& aSubject) -> QString;

	class item_side_chat final : public QWidget
	{
	public:
		explicit item_side_chat(QWidget* aParent = nullptr);

		void set_intents(item_side_chat_intents aIntents);
		void present_skills(std::vector<core::skill_entry> aSkills);
		/// \brief The replies' code is coloured as the other chats' is: their
		/// code asks the window's highlighter through `aDispatch`, which
		/// answers by `apply_code_syntax`.
		void set_code_syntax_dispatch(std::function<void(std::vector<application::markdown_code_syntax_request>)> aDispatch);
		void apply_code_syntax(const application::markdown_code_syntax_publication& aSpans);
		/// \brief Shows `aState`: the item, its rounds, a run going, a
		/// proposal, why the last step did nothing (`aFailure` when a run
		/// failed). `aAgent` names the agent in the ending lines.
		void present(const core::follow_up_state& aState, const QString& aAgent, const QString& aFailure);
		/// \brief Says what came of a Use (or why it was not used).
		void show_status(const QString& aText);
		/// \brief Opens the column with the keyboard in its composer, or
		/// closes it; closed, every item's chat is kept by the host.
		void               set_column_open(bool aOpen);
		[[nodiscard]] auto column_open() const noexcept -> bool { return ColumnOpen; }
		/// \brief [[BPR-2337]]. Whether the tab the open column belongs to is
		/// the one shown: off, the column hides and keeps its chat, its width
		/// and its words; on again, it shows as it was. Opening shows it.
		void               set_tab_shown(bool aShown);
		[[nodiscard]] auto tab_shown() const noexcept -> bool { return TabShown; }
		/// \brief Told each time the column is opened (after it shows) or
		/// closed.
		void set_open_observer(std::function<void(bool aOpen)> aObserver) { OpenObserver = std::move(aObserver); }
		/// \brief The column's width: the preferred width, else the default,
		/// within the bounds the page beside it allows.
		[[nodiscard]] auto sizeHint() const -> QSize override;

		/// \brief [[BPR-2333]], owner at attended review bbbd62f8: "By default
		/// its taking up too much space and I need to be able to resize it".
		/// The narrowest the column drags to, in logical pixels.
		static constexpr int minimum_column_width = 260;
		/// \brief The width it opens at with no preference, beside a page wide
		/// enough; beside a narrower page, three tenths of it.
		static constexpr int default_column_width = 320;
		/// \brief The width it opens at with no preference, beside this page.
		[[nodiscard]] auto default_width() const -> int;
		/// \brief The widest it drags to beside this page: six tenths of it,
		/// so the report or the code keeps the larger share.
		[[nodiscard]] auto maximum_width() const -> int;
		/// \brief Adopts this window's stored width, or none for the
		/// default; reports nothing back.
		void               set_preferred_width(std::optional<std::uint32_t> aWidth);
		[[nodiscard]] auto preferred_width() const noexcept -> std::optional<std::uint32_t> { return PreferredWidth; }
		/// \brief Told the width an explicit drag chose, or none for Reset.
		void set_width_sink(std::function<void(std::optional<std::uint32_t>)> aSink);
		/// \brief The seam at the column's left edge: a drag resizes the
		/// column and a double-click returns it to the default width.
		[[nodiscard]] auto resize_handle() const noexcept -> QWidget* { return ResizeHandle; }
		/// \brief The handle's gestures: a width within the bounds, kept and
		/// reported, or Reset to the default, reported as none.
		void resize_to(int aWidth);
		void reset_width();

		[[nodiscard]] auto composer() const noexcept -> QPlainTextEdit* { return Composer; }
		[[nodiscard]] auto send_button() const noexcept -> QPushButton* { return Send; }
		[[nodiscard]] auto apply_button() const noexcept -> QPushButton* { return ApplyButton; }
		/// \brief The proposal's Use and Discard while one is shown; null otherwise.
		[[nodiscard]] auto use_button() const noexcept -> QPushButton* { return UseButton; }
		[[nodiscard]] auto discard_button() const noexcept -> QPushButton* { return DiscardButton; }
		[[nodiscard]] auto proposal_card() const noexcept -> QWidget* { return ProposalCard; }
		[[nodiscard]] auto proposal_text() const -> QString;
		[[nodiscard]] auto close_button() const noexcept -> QToolButton* { return ClosePane; }
		[[nodiscard]] auto copy_chat_button() const noexcept -> QToolButton* { return CopyChat; }
		[[nodiscard]] auto show_button() const noexcept -> QPushButton* { return ShowItem; }
		[[nodiscard]] auto status_text() const -> QString;
		/// \brief What the chat is about and where, in one phrase, as Copy
		/// conversation heads its text ("Suggested comment at queue.cpp:12").
		[[nodiscard]] auto subject_text() const -> QString;
		/// \brief RFC-BPR-0130. The one header row every chat wears: here the
		/// kind of item and its place, Show, Copy conversation and close.
		[[nodiscard]] auto header() const noexcept -> chat_header_view* { return Header; }
		/// \brief The item as the chat quotes it at its top.
		[[nodiscard]] auto item_text() const -> QString;
		/// \brief The item's chip in the composer.
		[[nodiscard]] auto chip_text() const -> QString;
		/// \brief The one transcript the chat's turns are drawn in.
		[[nodiscard]] auto transcript() const noexcept -> chat_transcript_view* { return Transcript; }
		/// \brief The reviewer's questions and the agent's answers shown, in order.
		[[nodiscard]] auto questions() const -> QStringList;
		[[nodiscard]] auto answers() const -> QStringList;
		/// \brief Round `aRound`'s ending line; null past the rounds shown.
		[[nodiscard]] auto status_line(std::size_t aRound) const -> agent_run_status_line*;
		[[nodiscard]] auto running() const noexcept -> bool { return Running; }
		[[nodiscard]] auto shown_key() const -> std::string { return Shown ? Shown->Subject.Key : std::string{ }; }
		/// \brief The chat as plain text that reads as Markdown: what Copy
		/// chat puts on the clipboard.
		[[nodiscard]] auto chat_text() const -> QString;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;
		bool eventFilter(QObject* aWatched, QEvent* aEvent) override;

	private:
		void               send();
		void               present_lead();
		void               restyle();
		void               fit_chip();
		[[nodiscard]] auto make_proposal(const core::chat_follow_up_proposal& aProposal, QWidget* aParent) -> QWidget*;

		item_side_chat_intents               Intents;
		std::optional<core::follow_up_state> Shown;
		QString                              Agent;
		bool                                 Running    = false;
		bool                                 ColumnOpen = false;
		bool                                 TabShown   = true;
		std::function<void(bool aOpen)>      OpenObserver;
		/// The running request's clock, and each answered round's time: how
		/// long a run took is what this column saw of it.
		QElapsedTimer                            Clock;
		std::vector<std::optional<std::int64_t>> Seconds;
		chat_header_view*                        Header      = nullptr;
		QPushButton*                             ShowItem    = nullptr;
		QToolButton*                             CopyChat    = nullptr;
		QToolButton*                             ClosePane   = nullptr;
		chat_transcript_view*                    Transcript  = nullptr;
		QLabel*                                  ItemHeading = nullptr;
		QLabel*                                  ItemQuote   = nullptr;
		QLabel*                                  Invite      = nullptr;
		QLabel*                                  Status      = nullptr;
		chat_composer*                           Field       = nullptr;
		QLabel*                                  Chip        = nullptr;
		QString                                  ChipWords;
		QPlainTextEdit*                          Composer    = nullptr;
		QPushButton*                             Send        = nullptr;
		QPushButton*                             ApplyButton = nullptr;
		/// The proposal's card while one waits; the transcript owns it.
		QPointer<QWidget>                                                           ProposalCard;
		QPointer<QLabel>                                                            ProposalText;
		QPointer<QPushButton>                                                       UseButton;
		QPointer<QPushButton>                                                       DiscardButton;
		std::function<void(std::vector<application::markdown_code_syntax_request>)> CodeDispatch;
		QWidget*                                                                    ResizeHandle = nullptr;
		/// The page the column stands beside, whose width bounds it.
		QPointer<QWidget>                                 Page;
		std::optional<std::uint32_t>                      PreferredWidth;
		std::function<void(std::optional<std::uint32_t>)> WidthSink;
		skill_completer*                                  SkillCompletion = nullptr;
	};
} // namespace bpr::qt

#endif
