#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_CONVERSATION_CARDS_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_CONVERSATION_CARDS_VIEW_HPP

#include "bpr/application/github_projection.hpp"
#include "bpr/core/conversation_reactions.hpp"
#include "bpr/core/conversation_timeline.hpp"
#include "bpr/core/diff_model.hpp"
#include "bpr/core/discuss_preferences.hpp"
#include "bpr/core/pull_request_suggestion.hpp"
#include "bpr/core/reference_page.hpp"
#include "bpr/core/reply_draft_standing.hpp"
#include "bpr/core/returning_review.hpp"
#include "bpr/core/review_round_trip.hpp"
#include "bpr/core/review_tour_markdown.hpp"
#include "bpr/core/suggestion_applicability.hpp"
#include "bpr/core/suggestion_draft.hpp"
#include "bpr/core/thread_reply.hpp"
#include "bpr/qt/enhance_control.hpp"
#include "bpr/qt/feedback_label_marks.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/suggest_change_control.hpp"

#include <QAbstractScrollArea>
#include <QColor>
#include <QFont>
#include <QPoint>
#include <QPointer>
#include <QRect>
#include <QString>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

class QAbstractButton;
class QContextMenuEvent;
class QMenu;
class QPlainTextEdit;
class QToolButton;
class QTimer;
class QKeyEvent;
class QLabel;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QResizeEvent;
class QWheelEvent;

class QVariantAnimation;

namespace bpr::qt {
	/// \brief [[BPR-1746]]. Why a thread takes no reply, in the words
	/// the card and the drawer show; empty for a refusal GitHub did not make.
	[[nodiscard]] auto thread_reply_refusal_text(core::thread_reply_refusal aRefusal) -> QString;
	/// \brief [[BPR-1824]]. Why a thread cannot be resolved (or, with
	/// `aResolve` false, reopened), in the words the menu and the panel show;
	/// empty for `none`.
	[[nodiscard]] auto thread_resolution_refusal_text(core::thread_resolution_refusal aRefusal, bool aResolve) -> QString;
	class feedback_label_controls;

	/// \brief Which threads the notes show.
	enum class cards_scope : std::uint8_t {
		/// The threads of the file the comparison shows, at their lines.
		this_file,
		/// Every thread of the pull request, in file order (Discuss).
		whole_pull_request,
	};

	/// \brief What a gesture on a thread asks for.
	enum class card_action : std::uint8_t {
		publish_draft,
		edit_draft,
		delete_draft,
		/// Read this thread up to its latest post ([[BPR-0656]]).
		mark_read,
		mark_unread,
		/// Start a reply on this thread ([[BPR-0649]]). [[BPR-1290]]: the
		/// reply is written in place, at the foot of the open thread, through
		/// the one Reply path; no thread starts a second thread.
		reply,
		/// Return to the code at this thread's line from Discuss ([[BPR-0649]]).
		open_in_code,
		/// Resolve or reopen this thread through the mutation port ([[BPR-0657]]).
		resolve,
		unresolve,
		defect,
		/// [[BPR-1290]]. Reveal the open thread's line in the code, without
		/// changing the modality.
		show_line,
		/// [[BPR-1290]]. Publish the reply written in place.
		publish_reply,
		/// [[BPR-1290]]. Give up the reply written in place.
		cancel_reply,
		/// [[BPR-1212]]. Open the draft written in the drawer in Discuss,
		/// where the full composer is (⌘⇧M since [[BPR-1345]] v002; ⌘⇧↩
		/// before).
		maximize_draft,
		/// [[BPR-1212]]. Leave the draft written in the drawer (Esc, Close):
		/// the window keeps a written draft and drops an empty one.
		close_draft,
		/// [[BPR-1345]]. Keep the draft written in the drawer pending in the
		/// review: Start a review, or Add review comment once one is in
		/// progress. Nothing is published.
		review_draft,
		/// [[BPR-1345]] v002. Finish review (⌘⌥↩) from the draft's field:
		/// the window opens the review's summary when a review is in
		/// progress.
		finish_review,
		/// [[BPR-1374]]. My kept reply, whose thread is gone: its words become
		/// a new comment on its line, still unpublished.
		keep_reply_as_comment,
		/// [[BPR-1374]]. My kept reply's words, copied as written.
		copy_reply,
		/// [[BPR-1374]]. My kept reply is discarded, once the window has asked.
		discard_reply,
		/// [[BPR-1581]]. Show this thread, or the post named, in Discuss.
		show_in_discuss,
		/// [[BPR-1586]]. Show this thread's row, at the post named, in Threads.
		show_in_threads,
		/// [[RFC-BPR-0090]]. The drawer draft's suggestion GitHub cannot apply
		/// becomes text at the same lines (owner, Q3).
		convert_draft,
		/// [[BPR-1733]]. My kept suggestion from an earlier head is written
		/// again at its lines here; the earlier draft goes once it is saved.
		carry_suggestion,
		/// [[BPR-1733]]. My kept suggestion's change, copied as its lines.
		copy_change,
		/// [[BPR-1773]]. Ask the agent about this conversation: the Agent
		/// tab opens with it attached to the next question. Nothing is sent.
		ask_agent,
		/// RFC-BPR-0116 ([[BPR-2074]]). Was this addressed?: the Agent tab
		/// asks it of this conversation. Nothing is resolved or posted.
		was_addressed,
		/// RFC-BPR-0118 ([[BPR-2078]]). Suggest the change on the drawer
		/// draft's lines, or Stop while the agent writes it.
		suggest_change,
		/// RFC-BPR-0118. The agent's change open in the code goes into the
		/// comment (Keep change), is dropped (Discard), the comment goes back
		/// to how it was (Undo), or the panel closes (Done, Dismiss).
		keep_suggested_change,
		discard_suggested_change,
		undo_suggested_change,
		close_suggested_change,
		/// RFC-BPR-0127 ([[BPR-2158]]). My Local comment becomes a draft
		/// of the same kind at the same place, not in the review, or at the
		/// selected line ("Make draft on line N"). Nothing is published.
		make_draft,
		/// RFC-BPR-0127. My draft becomes a Local comment: only on this
		/// computer, never sent.
		keep_local,
		/// RFC-BPR-0127. My Local comment is deleted, once the window has asked.
		delete_local,
		/// RFC-BPR-0127. My Local comment's words, copied as written.
		copy_local,
		/// RFC-BPR-0127. The composer's state control: Local chosen.
		compose_local,
		/// RFC-BPR-0127. The composer's state control: Draft (or Reply) chosen.
		compose_draft,
		/// RFC-BPR-0127. Save the composer's words as a Local comment (⌘↩).
		save_local,
		/// RFC-BPR-0127. My Local comment on another file: the window goes to
		/// the file, then the line, and opens it there.
		open_local_elsewhere,
		/// RFC-BPR-0127. My Local comment's words, edited in place, saved
		/// only on this computer.
		edit_local,
	};

	/// \brief RFC-BPR-0127 ("Where a Local comment can be"). Where the
	/// window found my Local comment against the comparison shown.
	enum class local_comment_place : std::uint8_t {
		/// At its line (or range) in the shown comparison.
		line,
		/// Its line is not in the shown comparison: why is in `Notice`.
		outdated,
		/// On this file as a whole: the file's edge chip.
		whole_file,
		/// On the pull request: the pull request chip, and Overview's
		/// Comments tab.
		pull_request,
		/// On another file: the "Other files" chip.
		other_file,
	};

	/// \brief RFC-BPR-0127 ([[BPR-2158]]). One of my Local comments, as the
	/// window placed it against the comparison shown: only on this computer,
	/// never sent. Nothing here decides where it is.
	struct local_comment_card
	{
		std::uint64_t Id       = 0U;
		std::uint64_t Revision = 0U;
		QString       Body;
		/// The comparison shown, where it is drawn at its line; none for a
		/// comment the comparison does not place.
		std::optional<core::file_comparison_digest> Comparison;
		core::diff_side                             Side = core::diff_side::right;
		/// Its one-based line, and the first of a range.
		std::optional<std::uint32_t> Line;
		std::optional<std::uint32_t> FirstLine;
		/// The thread a Local reply answers, by its provider identity.
		std::optional<std::string> ReplyToThread;
		/// Not at a line of its own here: why (outdated), drawn after the
		/// placed cards as an outdated thread is.
		QString Notice;
		/// What Make draft is offered as: "Make draft", "Make draft on line
		/// N", or empty when it is not offered; and why not.
		QString MakeDraft;
		QString MakeDraftWhy;
		/// Where it stands, and for another file its path and one-based line.
		local_comment_place          Place = local_comment_place::line;
		QString                      Path;
		std::optional<std::uint32_t> PathLine;

		auto operator==(const local_comment_card&) const -> bool = default;
	};

	/// \brief RFC-BPR-0127. What assistive technology hears for each state.
	[[nodiscard]] auto comment_state_words(bool aLocal, bool aDraft, bool aInReview) -> QString;

	/// \brief [[BPR-1733]]. My kept suggestion written on an earlier head of
	/// the file shown, as the core decided at this head (RFC-BPR-0090, "A new
	/// head while editing").
	struct moved_suggestion_card
	{
		/// The draft as it was saved, on its own comparison, and its revision.
		core::review_comment_draft Draft;
		std::uint64_t              Revision = 0U;
		/// The comparison shown, where its note is drawn.
		core::file_comparison_digest Comparison;
		/// The short commit it was written on.
		QString EarlierHead;
		/// Carry to its lines here, or why not, and the change to copy.
		core::suggestion_head_move Move;

		auto operator==(const moved_suggestion_card&) const -> bool = default;
	};

	/// \brief [[BPR-1345]]. Where the draft written in the drawer stands
	/// with the reviewer's review.
	enum class draft_review_state : std::uint8_t {
		/// No review is in progress: Start a review starts one.
		none,
		/// Other drafts are pending: Add review comment adds this one.
		in_progress,
		/// This draft is pending in the review already.
		pending,
	};

	/// \brief The notes' filters ([[BPR-0649]], [[BPR-1290]]).
	struct discuss_filters
	{
		/// Only threads with a post newer than what I have read.
		bool UnreadOnly = false;
		/// Hide resolved threads.
		bool UnresolvedOnly = false;
		/// [[BPR-1089]]. Exactly these conversations, by root identity: an
		/// Overview count's drill-down. None lists every conversation.
		std::optional<std::set<std::string>> Listed;
		/// [[BPR-2171]]. Fold bots: runs of bot comments fold to one row.
		bool FoldBots = false;

		auto operator<=>(const discuss_filters&) const = default;
	};

	/// \brief What the reader has not seen in one thread.
	///
	/// \details [[BPR-0656]]. The thread root's projection identity and the
	/// projection identities of its unread posts, as the ledger host decided
	/// them; a thread absent from the state is read.
	struct thread_read_state
	{
		std::string              Thread;
		std::vector<std::string> UnreadPosts;

		auto operator<=>(const thread_read_state&) const = default;
	};

	/// \brief [[BPR-1810]]. The stop a published tour stop's Previous or Next
	/// reaches among the comments at hand: its thread, number and title.
	struct tour_stop_neighbour
	{
		std::string Thread;
		std::size_t Ordinal = 0U;
		std::string Title;

		auto operator==(const tour_stop_neighbour&) const -> bool = default;
	};

	/// \brief One post as the notes draw it.
	struct card_post
	{
		std::string Id;
		QString     Author;
		QString     Body;
		/// The post's depth beneath the root, one for the first reply.
		std::size_t Depth = 0U;
		bool        Mine  = false;
		/// RFC-BPR-0127. My Local reply inside the thread, after its
		/// published posts: only on this computer.
		bool Local = false;
		/// RFC-BPR-0127. What Make draft is offered as on my Local reply,
		/// or empty with why not.
		QString LocalMakeDraft;
		QString LocalMakeDraftWhy;
		/// Newer than my cursor, as the ledger decided ([[BPR-0656]]).
		bool Unread = false;
		/// "author" for the pull request's author, "reviewer" otherwise.
		QString Role;
		/// Whom the post answers when it answers anything but the thread's
		/// first post ([[BPR-1290]]: deep threads are read in place).
		QString ReplyingTo;
		/// [[BPR-1083]]. The post's label mark, when its body begins with a
		/// recognized Conventional Comments prefix. `Body` is still the whole
		/// body, its header and decorations included, as written.
		std::optional<core::feedback_label> Label;
		/// The label's written decorations, as its mark and rail show them.
		feedback_mark_emphasis Emphasis;
		/// "tbutler, issue (blocking)": the author, then the mark's name.
		QString Name;
		/// [[BPR-1290]]. How many characters of `Body` the recognized header
		/// takes after `Lead`, which the notes set in semibold; zero for a
		/// plain post.
		qsizetype HeaderLength = 0;
		/// [[BPR-1086]]. Where a closed note's words start: a published tour
		/// stop's narrative, after its pinned source and generated heading, so
		/// its label reads beside its mark; zero for any other post.
		qsizetype Lead = 0;
		/// [[BPR-1806]]. The thread's first post read as a published tour
		/// stop, which shows as one: its number and title, its narrative
		/// alone. `Body` stays the whole body as written.
		std::optional<core::published_tour_stop> TourStop;
		/// [[BPR-1806]]. How many stops its tour has, when its stops prove it.
		std::optional<std::size_t> TourStopCount;
		/// [[BPR-1810]]. The stops its Previous and Next reach, when they are
		/// among the comments at hand; the plate steps to their threads.
		std::optional<tour_stop_neighbour> TourPrevious;
		std::optional<tour_stop_neighbour> TourNext;
	};

	/// \brief [[BPR-1806]]. A tour stop's badge: "Stop 6 of 9", or "Stop 6"
	/// while the tour's length is unknown.
	[[nodiscard]] auto tour_stop_badge(const core::published_tour_stop& aStop, std::optional<std::size_t> aCount) -> QString;

	/// \brief [[BPR-1741]]. A review thread's resolution as the viewer
	/// should see it, and whether GitHub lets the viewer resolve or reopen it.
	struct thread_resolution_presentation
	{
		bool Resolved     = false;
		bool Pending      = false;
		bool CanResolve   = true;
		bool CanUnresolve = true;

		auto operator==(const thread_resolution_presentation&) const -> bool = default;
	};

	/// \brief One thread's note: a review thread's root and its replies, or my draft.
	/// \brief RFC-BPR-0128 ([[BPR-2165]]). The agent's claim on a thread from
	/// Was this addressed?, and the lines it read: as the thread started and
	/// as they are now. A claim; nothing is resolved or posted by it.
	struct thread_claim
	{
		core::addressed_claim                Claim = core::addressed_claim::cannot_tell;
		std::optional<core::returning_lines> Then;
		std::optional<core::returning_lines> Now;
		/// The Agent tab turn that answered.
		std::size_t Turn = 0U;

		auto operator==(const thread_claim&) const -> bool = default;
	};
	/// \brief The chip's word: Addressed, Partly, Not addressed, Unclear.
	[[nodiscard]] auto thread_claim_words(core::addressed_claim aClaim) -> QString;

	struct thread_card
	{
		/// The thread root's identity; empty for the draft note.
		std::string Id;
		QString     Kind;
		QString     Verdict;
		std::string CreatedAt;
		/// [[BPR-1779]]. In the conversation's time order (the Pull request
		/// view and Discuss's whole pull request), the review whose bundle the
		/// comment was posted in; empty otherwise or outside any review.
		std::string Bundle;
		/// [[BPR-2171]]. In time order, beside the author: the review's
		/// verdict on its own comment, and when it was written.
		QString HeadVerdict;
		QString When;
		/// [[BPR-1779]]. Its place in that order.
		std::size_t                    TimelineOrder = 0U;
		QString                        DefectBadge;
		QString                        DefectDescription;
		QString                        Path;
		std::optional<core::diff_side> Side;
		/// The one-based line the thread sits on, when it has one.
		std::optional<std::uint32_t> Line;
		/// [[BPR-1564]]. A published thread on several lines: GitHub's
		/// one-based `start_line`, the first of them.
		std::optional<std::uint32_t> StartLine;
		/// [[BPR-1212]] slice 1. My draft on several lines: its one-based
		/// first line; `Line` is its last, where it is anchored.
		std::optional<std::uint32_t> FirstLine;
		bool                         Resolved = false;
		/// [[BPR-1741]]. My resolve or reopen is unanswered; `Resolved`
		/// already shows what I asked.
		bool ResolutionPending = false;
		/// [[BPR-1741]]. GitHub lets me resolve (or reopen) this thread.
		bool ResolutionPermitted = true;
		bool Outdated            = false;
		bool AnchorUnavailable   = false;
		/// [[BPR-1745]]. GitHub reports the thread outdated, and the
		/// core answers it by the thread (`core::thread_reply_route::on_thread`):
		/// Reply is offered without a line, and the reply field says quietly
		/// that the thread's line has changed since it was written.
		bool ReplyOnThread = false;
		/// [[BPR-1746]]. GitHub says this viewer cannot reply in the
		/// thread (`viewerCanReply`), because the conversation is locked or
		/// otherwise: the core's refusal. Reply is not offered, and
		/// `thread_reply_refusal_text` says why.
		core::thread_reply_refusal ReplyRefusal = core::thread_reply_refusal::none;
		bool                       Draft        = false;
		/// RFC-BPR-0127 ([[BPR-2158]]). My Local comment: dashed as a draft
		/// is, marked with the computer, never sent. `Draft` is also set.
		bool               Local = false;
		local_comment_card LocalComment;
		/// RFC-BPR-0127. How many of my Local replies the thread holds.
		std::size_t LocalReplies = 0U;
		/// [[BPR-1345]]. My draft is pending in my review, or held by its
		/// submission.
		bool Pending = false;
		/// [[BPR-1212]]. My draft, open and being written in the drawer: the
		/// composer, not the note. It carries no posts; the composer shows
		/// what the draft editor holds.
		bool Writing = false;
		/// [[BPR-1290]]. GitHub stores the thread as a file comment that its
		/// code link places at a line (RFC-BPR-0022).
		bool FileLevel = false;
		/// [[BPR-1374]]. My unsent reply: on a thread's note, the reply I have
		/// written in it and not sent; on my kept reply's own note, where its
		/// thread stands. None when there is no unsent reply.
		std::optional<core::reply_draft_standing> UnsentReply;
		/// RFC-BPR-0127, owner look v003. On a thread's note, my draft reply
		/// in it and its words, so the thread shows it stands (one draft
		/// reply per thread is why a Local reply cannot become one).
		std::optional<core::review_draft_id> UnsentReplyDraft;
		QString                              UnsentReplyBody;
		/// [[BPR-1374]]. What became of the thread an unsent reply answers, in
		/// a sentence, drawn under the note's name; empty when nothing did.
		QString Notice;
		/// [[BPR-1374]]. My kept reply can become a new comment on its line.
		bool KeepableAsComment = false;
		/// [[BPR-1733]]. My kept suggestion from an earlier head, at the
		/// lines it is carried to here or at its own; `Carryable` when the core
		/// carries it, and `Change` what Copy change copies.
		bool    MovedSuggestion = false;
		bool    Carryable       = false;
		QString Change;
		/// How many of the posts are unread; the first post unread makes the
		/// thread new.
		std::size_t UnreadCount = 0U;
		bool        RootUnread  = false;
		/// The code at the thread's line, when Discuss can read it
		/// ([[BPR-0649]]); empty otherwise.
		QString Excerpt;
		/// [[BPR-1084]]. What a reply in this thread is attached to on
		/// GitHub, which decides whether a suggested change in it is native.
		core::suggestion_placement_observation Suggestion;
		/// True while the thread's line is on the comparison and locatable.
		bool Placed = false;
		/// Stable projection order breaks geometry ties across comparison frames.
		std::size_t ProjectionOrder = 0U;
		/// Where the line is, in viewport coordinates, when placed.
		int TargetY = 0;
		/// Where the note is drawn, in content coordinates.
		QRect                  Rect;
		std::vector<card_post> Posts;
	};

	/// \brief [[BPR-1721]]. The one-based first line of a card on
	/// several lines, or none for one line: my draft's `FirstLine`, or a
	/// published thread's `StartLine`, when it is above the card's `Line`.
	[[nodiscard]] inline auto thread_first_line(const thread_card& aCard) noexcept -> std::optional<std::uint32_t>
	{
		const auto first = aCard.Draft ? aCard.FirstLine : aCard.StartLine;
		return aCard.Line.has_value() && first.has_value() && *first < *aCard.Line ? first : std::nullopt;
	}

	/// \brief [[BPR-1471]]. Where a closed note's meta row draws, in viewport
	/// coordinates, from its right edge leftward. A note that is not a draft
	/// keeps the resolved check's slot whether or not it is resolved, so the
	/// blue new-post count, the reply count and the faces stand in one column.
	struct closed_note_meta final
	{
		/// The green resolved check; empty when the note is not resolved.
		QRectF Resolved;
		/// The blue new-post count; empty when nothing in the note is new. Its
		/// right edge is the same on every closed note ([[BPR-1473]]).
		QRectF NewCount;
		/// "Draft reply" and its pencil, left of the count; empty without one.
		QRectF UnsentReply;
		/// The right edge the reply count and the repliers' faces start from.
		int Replies = 0;
	};

	/// \brief [[BPR-1084]]. What a reply in this conversation would be attached
	/// to on GitHub, from its accepted projection: a file comment (also one its
	/// code link places at a line), an outdated line, no current line, the pull
	/// request's own conversation, or a current line on its side.
	[[nodiscard]] auto suggestion_placement_of(const application::github_conversation_projection& aNode) -> core::suggestion_placement_observation;

	/// \brief [[BPR-1290]]. One thread's marker in the code's gutter and its
	/// tick on the code's scroll bar.
	struct thread_line_mark
	{
		std::uint32_t                       Line = 0U;
		std::optional<core::feedback_label> Label;
		feedback_mark_emphasis              Emphasis;
		bool                                Unread   = false;
		bool                                Resolved = false;
		bool                                Draft    = false;
		/// The open thread: its line is washed and its marker ringed.
		bool Open = false;
		/// [[BPR-1212]]. My draft, being written in the drawer: a blue
		/// bracket in place of the amber wash.
		bool Writing = false;
		/// [[BPR-1212]] slice 1. My draft on several lines: its zero-based
		/// first line, where the bracket starts. [[BPR-1721]]: or a
		/// published thread's, from GitHub's `start_line`.
		std::optional<std::uint32_t> FirstLine;
		/// The note under the pointer: its line is washed lightly.
		bool Hovered = false;
		/// "Line 656: 4 posts, 2 new, resolved".
		QString Description;

		auto operator==(const thread_line_mark&) const -> bool = default;
	};

	/// \brief Answers where a one-based line is, in this view's viewport.
	using card_line_locator = std::function<std::optional<int>(core::diff_side, std::uint32_t)>;
	/// \brief Reports a thread made current and whether the gesture opens it.
	using card_thread_dispatch = std::function<void(const std::string&, bool)>;
	using card_action_dispatch = std::function<void(card_action, const std::string&)>;
	/// \brief Answers the code around a one-based line of a path, for Discuss.
	using card_excerpt_locator
		= std::function<std::optional<QString>(const QString&, core::diff_side, std::uint32_t)>;
	/// \brief [[BPR-1564]]. Answers the exact text of one-based lines first
	/// to last of a path on a side, when that file is the one compared.
	using card_suggestion_locator
		= std::function<std::optional<std::vector<std::string>>(const QString&, core::diff_side, std::uint32_t, std::uint32_t)>;

	/// \brief [[BPR-1290]]. The reply written in place, as the draft editor holds it.
	struct thread_reply_presentation
	{
		/// The thread the draft answers, by root identity.
		std::string Thread;
		QString     Body;
		/// The draft editor's Publish is enabled.
		bool    Publishable = false;
		bool    Editable    = true;
		QString Notice;

		auto operator==(const thread_reply_presentation&) const -> bool = default;
	};

	/// \brief [[BPR-1212]]. The new comment written in the drawer, as the
	/// draft editor holds it.
	struct draft_composer_presentation
	{
		QString Body;
		/// The draft editor's Publish is enabled.
		bool Publishable = false;
		/// The draft can be written here. A draft whose retained text is in
		/// conflict, or that cannot be edited, is read but not written.
		bool Editable = true;
		/// The lines the draft is on, in words: "Line 12", "Lines 10–12".
		QString Lines;
		/// What the draft's suggestion block changes, when it has one.
		std::optional<core::suggested_change> Change;
		/// [[RFC-BPR-0090]]. The change is written in the head pane: `Body`
		/// is the reviewer's words only, and the block lives in the code.
		bool InCode = false;
		/// [[RFC-BPR-0090]]. GitHub cannot apply this suggestion where it goes,
		/// so it is held: the foot offers this button ("Convert to file
		/// comment") in place of the sending ones, and `Notice` says why.
		QString Convert;
		/// What the last Publish came to ([[BPR-1329]]), else why Publish is
		/// unavailable, or empty.
		QString Notice;
		/// [[BPR-2172]]. `Notice` is a caution about where the comment goes
		/// that leaves Publish offered: the composer shows it as a warning
		/// mark in its tool row, its sentence disclosed on press, not as
		/// lines under the row.
		bool NoticeFolds = false;
		/// [[BPR-1329]]. The draft editor offers Publish again as a retry of
		/// the same publication.
		bool Retry = false;
		/// [[BPR-1345]]. Where the draft stands with the review, which
		/// decides its foot: Add single comment and Start a review, or Add
		/// review comment as the main action while a review is in progress.
		draft_review_state Review = draft_review_state::none;
		/// RFC-BPR-0118 ([[BPR-2078]]). Suggest the change: whether it is
		/// offered on these lines, and where a press stands.
		agent_suggestion_presentation AgentSuggestion;
		/// RFC-BPR-0127 ([[BPR-2158]]). The composer opens on Local (N): its
		/// foot is Delete and Save, and nothing in it can send.
		bool Local = false;
		/// RFC-BPR-0127. Why this draft cannot be kept local now (a submitted
		/// review holds it, or it is being published): the Local segment is
		/// disabled and says so. Empty when Local is offered.
		QString LocalRefusal;

		auto operator==(const draft_composer_presentation&) const -> bool = default;
	};

	/// \brief [[BPR-1779]]. A line the conversation's time order draws
	/// between its comments: a review's quiet line over the comments it
	/// bundled, a verdict that carried no comment, or a thread's path and line.
	struct timeline_line
	{
		enum class kind : std::uint8_t {
			/// Who posted the review's comments, when, and the verdict when it
			/// is Approved or Changes requested.
			bundle,
			/// Approved or Changes requested, with nothing posted: who and when.
			verdict,
			/// A thread's file and line, above its comment: opens it in Review.
			path,
			/// The New rule above the first comment not yet read.
			fresh,
			/// [[BPR-2171]]. A run of bot comments folded to one row.
			bots,
		};
		kind Kind = kind::bundle;
		/// The review's identity, or the thread's for a path line.
		std::string Id;
		QString     Author;
		QString     Verdict;
		/// When, in words; empty when GitHub gave no time.
		QString When;
		/// [[BPR-2171]]. The file row's path, named once over its threads.
		QString Path;
		/// [[BPR-2171]]. What a file or bots row folds: how many comments, the
		/// fold's key (empty when it folds nothing) and whether it is open.
		std::size_t Folded = 0U;
		std::string Fold;
		bool        Open = false;
		/// [[BPR-2171]]. A bundle whose review's own comment comes first says
		/// who, the verdict and when on that comment: its line is not drawn.
		bool Merged = false;
		/// Where it is drawn, in content coordinates. A bundle's `Reach` is the
		/// foot of its last comment, which its rail runs down to.
		QRect Rect;
		int   Reach = 0;

		auto operator==(const timeline_line&) const -> bool = default;
	};

	class open_thread_card;
	namespace detail {
		struct note_preview;
	}
	class scoped_prose_editor;

	/// \brief [[BPR-1775]]. Save pressed while editing one of my published
	/// comments: the post as its card names it, the body editing began with,
	/// byte for byte, and the text to save. A gesture only; the composition
	/// decides whether anything is sent.
	struct comment_edit_save
	{
		std::string Post;
		std::string Original;
		std::string Body;

		auto operator==(const comment_edit_save&) const -> bool = default;
	};

	/// \brief [[BPR-1775]]. One comment being edited, as the open card shows
	/// it: the post, the text as typed, the words beside the editor, whether
	/// GitHub is being asked, and whether GitHub's newer text is offered.
	struct comment_edit_presentation
	{
		std::string Post;
		std::string Text;
		QString     Status;
		bool        Saving = false;
		bool        Newer  = false;
		/// RFC-BPR-0127. My Local comment, edited in place: saved only on
		/// this computer, never sent.
		bool Local = false;

		auto operator==(const comment_edit_presentation&) const -> bool = default;
	};

	/// \brief RFC-BPR-0028's Comments tab: this file's threads as notes in a
	/// margin beside the code ([[BPR-0648]], [[BPR-1290]]).
	///
	/// \details A drawn view of the drawer's own forest: it holds no authority
	/// over the conversation and decides nothing about it. Each review thread
	/// is one note hanging from its line: the first post's author, avatar,
	/// label mark and rail ([[BPR-1083]]), two lines of its body as written,
	/// the replies and what is new. One thread at a time opens in place, pinned
	/// exactly at its line, to the whole conversation with Show line, Resolve,
	/// Mark read, More and a reply written at its foot; the notes around it
	/// make room above and below. The drawer moves with the code: the window
	/// relays every applied scroll frame, a wheel over the notes scrolls the
	/// code, and only when the code can go no further do the notes take the
	/// overflow themselves. Threads above or below what the notes show are
	/// chips on the top and bottom edges, nearest first.
	///
	/// In Discuss (`set_feed`) the same notes are a list in file order under
	/// their paths, with their own scrolling, each opening in place.
	///
	/// Opening a note makes it current and reports its root, which the window
	/// turns into the reveal of its line; a gesture on an action reports the
	/// action.
	class conversation_cards_view final : public QAbstractScrollArea
	{
	public:
		explicit conversation_cards_view(QWidget* aParent = nullptr);
		~conversation_cards_view() override;

		conversation_cards_view(const conversation_cards_view&)                    = delete;
		auto operator=(const conversation_cards_view&) -> conversation_cards_view& = delete;
		conversation_cards_view(conversation_cards_view&&)                         = delete;
		auto operator=(conversation_cards_view&&) -> conversation_cards_view&      = delete;

		void               adopt(const std::vector<application::github_conversation_projection>& aConversations);
		[[nodiscard]] auto retained() const noexcept -> const std::vector<application::github_conversation_projection>& { return Retained; }
		/// \brief Adopts the forest and file anchor in one layout.
		void adopt(const std::vector<application::github_conversation_projection>& aConversations,
				   std::optional<core::file_identity>                              aFile);
		void clear();
		void set_scope(cards_scope aScope);
		/// BPR-1603: PR-level comments in place, preserving the file view.
		/// [[BPR-1781]]: the window turns it on while the view is Overview's
		/// Comments tab and off in Review; each keeps its own place.
		void show_pull_request_comments(bool aShown);
		/// \brief [[BPR-1781]]. Told when the reviewer follows the quiet
		/// "Pull request conversation" link beside the code (an empty
		/// identity), or opens a comment on the pull request there (its
		/// identity). Without it the link is not offered.
		void               set_pull_request_link_dispatch(std::function<void(const std::string&)> aDispatch);
		[[nodiscard]] auto pull_request_comments_shown() const noexcept -> bool;
		/// \brief [[BPR-1808]]. The time-ordered conversation's counts, before
		/// and after its filters, and why a filtered list is empty: the core's
		/// summary of the last arrangement.
		[[nodiscard]] auto timeline_summary() const noexcept -> const core::timeline_filter_summary&;
		[[nodiscard]] auto pull_request_comments_chip() const noexcept -> QAbstractButton*;
		[[nodiscard]] auto scope() const noexcept -> cards_scope;
		void               set_anchor_file(std::optional<core::file_identity> aFile);
		void               set_line_locator(card_line_locator aLocator);
		/// \brief My unpublished draft, drawn as a dashed note at its line.
		///
		/// \details A draft that answers a thread is that thread's reply,
		/// written in place, and draws no note of its own.
		void set_draft(
			std::optional<core::review_comment_draft>   aDraft,
			std::optional<core::file_comparison_digest> aCurrentComparison);
		/// \brief [[BPR-1326]]. My other unpublished drafts, each drawn as a
		/// dashed note at its line while it is on this comparison, as the
		/// editor's draft is. The editor's draft is drawn from the editor.
		/// \details The editor holds only the selected line's draft, so a draft
		/// on any other line showed nothing until its line was chosen again.
		void set_saved_drafts(std::vector<core::review_comment_draft> aDrafts);
		/// \brief [[BPR-1374]]. As above, with where each unsent reply stands
		/// against the server's threads, as the core decided. A reply whose
		/// thread is on this file marks that thread's note; a reply whose
		/// thread is gone, or not known, is a note of its own at its line,
		/// with what became of the thread, until it is kept as a comment or
		/// discarded. Nothing here decides a standing.
		void set_saved_drafts(std::vector<core::review_comment_draft> aDrafts, std::vector<core::reply_draft_decision> aReplies);
		/// \brief [[BPR-1374]]. The sentence a kept or marked reply's note
		/// shows for a standing; empty for one that needs none.
		[[nodiscard]] static auto unsent_reply_notice(core::reply_draft_standing aStanding) -> QString;
		/// \brief [[BPR-1374]]. The identity of a kept reply's note.
		[[nodiscard]] static auto kept_reply_identity(core::review_draft_id aDraft) -> std::string;
		/// \brief [[BPR-1733]]. My kept suggestions written on an earlier head
		/// of the file shown, each a note of its own at the lines it is
		/// carried to, or at its own, while that comparison is shown. The
		/// window says what each offers; nothing here decides it.
		void set_moved_suggestions(std::vector<moved_suggestion_card> aMoved);
		/// \brief RFC-BPR-0127 ([[BPR-2158]]). My Local comments: at their
		/// lines in the line's group (after published threads, before my
		/// draft), my Local replies inside their threads' cards, and the
		/// outdated after the placed cards with their reason.
		void               set_local_comments(std::vector<local_comment_card> aComments);
		[[nodiscard]] auto local_comments() const noexcept -> const std::vector<local_comment_card>&;
		/// \brief RFC-BPR-0127. The identity of a Local comment's card, and
		/// the comment a card identity names.
		[[nodiscard]] static auto local_comment_identity(std::uint64_t aId) -> std::string;
		[[nodiscard]] static auto local_comment_of(const std::string& aIdentity) -> std::optional<std::uint64_t>;
		/// \brief RFC-BPR-0127 ("Where a Local comment can be"). The edge
		/// chips for my Local comments on this whole file and on other files,
		/// which the window places beside the pull request link. Each counts
		/// them beside the computer; hidden with none.
		[[nodiscard]] auto local_file_chip() const noexcept -> QAbstractButton*;
		/// \brief RFC-BPR-0127. In a drawer too narrow for their words the
		/// pull request link and the Local comment chips keep only their
		/// glyphs and counts; their words stay their names and tooltips.
		/// \returns Whether any chip changed.
		auto set_edge_chips_compact(bool aCompact) -> bool;
		/// \brief How much wider the shown edge chips are with their words
		/// than as they are drawn now.
		[[nodiscard]] auto edge_chips_saving() const -> int;
		[[nodiscard]] auto local_elsewhere_chip() const noexcept -> QAbstractButton*;
		/// \brief The menu the Other files chip opens, while it is open.
		[[nodiscard]] auto local_elsewhere_menu() const noexcept -> QMenu*;
		/// \brief RFC-BPR-0127. Told when Save is pressed on my Local
		/// comment edited in place: its identity, its revision and the words.
		void set_local_edit_dispatch(std::function<void(std::uint64_t, std::uint64_t, const std::string&)> aDispatch);
		/// \brief [[BPR-1733]]. The identity of a moved suggestion's note.
		[[nodiscard]] static auto moved_suggestion_identity(core::review_draft_id aDraft) -> std::string;
		/// \brief [[BPR-1733]]. What a moved suggestion's note says about where
		/// it was written and what can be done with it.
		[[nodiscard]] static auto moved_suggestion_notice(const moved_suggestion_card& aMoved) -> QString;
		void                      set_viewer_login(const QString& aLogin);
		void                      set_thread_dispatch(card_thread_dispatch aDispatch);
		void                      set_action_dispatch(card_action_dispatch aDispatch);
		using defect_badges = std::map<std::string, std::pair<QString, QString>>;
		void set_defect_badges(defect_badges aBadges);
		void set_context_menu_builder(std::function<void(QMenu&, const std::string&)> aBuilder);
		/// \brief [[BPR-1699]]. Where "Mark all from <login> as read"
		/// goes, from a thread's menu (its first post's author) and from a
		/// post's body menu (that post's author), with the GitHub login.
		void set_author_read_dispatch(std::function<void(const std::string&)> aDispatch);
		/// \brief [[BPR-2171]]. Who this repository's marks say is a bot or
		/// not; GitHub decides the rest.
		void set_author_bots(core::author_bot_marks aMarks);
		/// \brief [[BPR-2171]]. Mark as bot (true) or Not a bot, by login.
		void set_author_bot_dispatch(std::function<void(const std::string&, bool)> aDispatch);
		/// \brief [[BPR-2171]]. Discuss's centred column, in logical pixels;
		/// none for the whole width.
		void               set_column_width(std::optional<int> aWidth);
		[[nodiscard]] auto column_width() const noexcept -> std::optional<int>;
		/// \brief [[BPR-2171]]. Where the list's column is: its left edge and
		/// width in the viewport.
		[[nodiscard]] auto column() const -> std::pair<int, int>;
		/// \brief [[BPR-2171]]. Opens the fold holding a thread (or a post's
		/// thread), so a thread asked for by name is listed.
		void reveal_folded(const std::string& aId) { unfold_for(aId); }
		/// \brief [[BPR-2171]]. Opens or closes a fold by its row's key.
		void               toggle_fold(const std::string& aFold);
		[[nodiscard]] auto unfolded() const noexcept -> const std::set<std::string>&;
		/// \brief [[BPR-2171]]. Comments a closed fold holds, not listed.
		[[nodiscard]] auto folded_count() const noexcept -> std::size_t;
		/// \brief [[BPR-1699]]. How many posts this login wrote that are
		/// new, over every card; what the author's Mark all is offered for.
		[[nodiscard]] auto unread_post_count_from(const QString& aLogin) const -> std::size_t;
		[[nodiscard]] auto thread_menu(const std::string& aIdentity) -> QMenu*;
		void               show_thread_menu(const std::string& aIdentity, QPoint aGlobalPosition = { });
		/// \brief Discuss's list ([[BPR-0649]]): every thread in file order
		/// under its path, each opening in place, with its code excerpt.
		void               set_feed(bool aFeed);
		[[nodiscard]] auto feed() const noexcept -> bool;
		void               set_filters(discuss_filters aFilters);
		[[nodiscard]] auto filters() const noexcept -> discuss_filters;
		void               set_pull_request_author(const QString& aLogin);
		void               set_excerpt_locator(card_excerpt_locator aLocator);
		/// \brief [[BPR-1564]]. Where a suggestion's commented lines come from.
		void set_suggestion_locator(card_suggestion_locator aLocator);
		/// \brief Told after every relayout, so a summary can restate the counts.
		void               set_layout_observer(std::function<void()> aObserver);
		[[nodiscard]] auto thread_count() const noexcept -> std::size_t;
		[[nodiscard]] auto unresolved_count() const noexcept -> std::size_t;
		[[nodiscard]] auto unread_thread_count() const noexcept -> std::size_t;
		/// \brief Threads with unread posts on this file, independent of visible filters.
		[[nodiscard]] auto unread_thread_count(core::file_identity aFile) const -> std::size_t;
		void               set_code_colours(const QColor& aBackground, const QColor& aInk);
		/// \brief [[BPR-1270]]. The excerpt's and inline code's font: the
		/// appearance profile's code font. Until one is supplied, the default
		/// typography's.
		void               set_code_font(const QFont& aFont);
		[[nodiscard]] auto code_font() const -> const QFont&;
		/// \brief [[BPR-1562]]. The review panes' colours, for posts' fenced code.
		void set_code_theme(const markdown_code_theme& aTheme);
		/// \brief [[BPR-1562]]. Asks for the spans of the open thread's code;
		/// answers come back by `apply_markdown_code_syntax`.
		void set_code_syntax_dispatch(std::function<void(std::vector<application::markdown_code_syntax_request>)> aDispatch);
		void apply_markdown_code_syntax(const application::markdown_code_syntax_publication& aSpans);
		/// \brief [[BPR-1802]]. The pull request's repository: the open
		/// thread's permalinks in it read without their owner, and standing
		/// alone they ask through `set_permalink_dispatch` for their lines,
		/// which `apply_markdown_permalink_snippet` shows as snippet boxes.
		void set_permalink_repository(std::optional<core::markdown_repository> aRepository);
		void set_permalink_dispatch(std::function<void(std::vector<application::markdown_blob_snippet_request>)> aDispatch);
		void apply_markdown_permalink_snippet(const application::markdown_blob_snippet_publication& aSnippet);
		void request_permalink_snippets();
		/// \brief Adopts the read state the ledger host decided.
		///
		/// \details [[BPR-0656]]. Until a state arrives every post is drawn
		/// plainly: the notes do not guess what is unread.
		void set_read_state(std::vector<thread_read_state> aState);
		/// \brief [[BPR-0972]]. Adopts one thread's unread posts; empty means read.
		/// \returns The thread's previous unread posts.
		/// \details Lays out again only when that thread's note is shown.
		auto set_thread_read_state(const std::string& aThread, std::vector<std::string> aUnreadPosts)
			-> std::vector<std::string>;
		/// \brief [[BPR-1741]]. The posts' reactions as the viewer
		/// should see them, by GitHub node; a post without an entry shows none.
		void present_reactions(std::map<std::string, core::reaction_summary> aBySubject);
		/// \brief The reactions of one post, named as its card names it.
		[[nodiscard]] auto reactions_of(std::string_view aPost) const -> std::optional<core::reaction_summary>;
		/// \brief A reaction asked for on a post: the post's identity and the reaction.
		void set_reaction_dispatch(std::function<void(const std::string&, core::reaction_content)> aDispatch);
		/// \brief [[BPR-1775]]. Offers Edit on my own published comments, or
		/// withdraws it. The composition offers it in a signed-in read whose
		/// connection edits comments; the core decides each post from the
		/// read's "Mine". Withdrawn while editing, the editor closes and
		/// nothing is sent.
		void               set_comment_edit_offered(bool aOffered);
		[[nodiscard]] auto comment_edit_offered() const noexcept -> bool;
		/// \brief [[BPR-1806]]. Offers Open in tour on a published tour stop,
		/// or withdraws it: offered while the window has the tour to follow.
		void set_tour_open_offered(bool aOffered);
		/// \brief RFC-BPR-0116 ([[BPR-2074]]). The threads Was this addressed?
		/// is offered on, by identity.
		/// \brief RFC-BPR-0128 ([[BPR-2164]]). Fills a thread's Ask menu (its
		/// identity and section title): the open card's Ask and the thread's
		/// right-click Ask. None offers no Ask.
		using ask_menu_filler = std::function<void(QMenu&, const QString& aThread, const QString& aTitle)>;
		/// \brief RFC-BPR-0128 ([[BPR-2165]]). Each thread's claim, shown as a
		/// chip in its header (open) and its meta (closed); a press on the
		/// open card's chip shows the lines then and now.
		void               set_thread_claims(std::map<std::string, thread_claim> aClaims);
		[[nodiscard]] auto thread_claims() const noexcept -> const std::map<std::string, thread_claim>& { return Claims; }
		/// \brief The chip's Open the answer: the Agent tab at that turn.
		void               set_claim_answer_dispatch(std::function<void(std::size_t)> aDispatch) { ClaimAnswerDispatch = std::move(aDispatch); }
		void               set_ask_menu_filler(ask_menu_filler aFiller);
		[[nodiscard]] auto thread_title(const std::string& aThread) const -> QString;
		void               set_addressed_threads(std::set<std::string> aThreads);
		[[nodiscard]] auto addressed_threads() const noexcept -> const std::set<std::string>& { return AddressedThreads; }
		[[nodiscard]] auto tour_open_offered() const noexcept -> bool;
		/// \brief [[BPR-1806]]. Where Open in tour goes: the post's identity
		/// and the stop it was read as. Nothing is opened by the view.
		void set_tour_open_dispatch(std::function<void(const std::string&, const core::published_tour_stop&)> aDispatch);
		/// \brief [[BPR-1810]]. Where a stop plate's Previous or Next goes:
		/// the neighbouring stop's thread, which may be on another file. With
		/// none, a thread among these notes opens in place.
		void set_tour_step_dispatch(std::function<void(const std::string&)> aDispatch);
		/// \brief [[BPR-1810]]. Steps from the open stop to its neighbour's
		/// thread, as the plate's chevron does; false when it has none.
		auto step_tour(bool aForward) -> bool;
		/// \brief [[BPR-1775]]. Whether Edit is offered on this post.
		[[nodiscard]] auto comment_editable(std::string_view aPost) const -> bool;
		/// \brief [[BPR-1775]]. Where Save goes. Nothing is sent by the view.
		void set_comment_edit_dispatch(std::function<void(const comment_edit_save&)> aDispatch);
		/// \brief [[BPR-1775]]. Opens the editor in place of the post's body,
		/// on GitHub's exact body as the read gave it, opening its thread
		/// first when it is not open; false when Edit is not offered on it.
		auto begin_comment_edit(const std::string& aPost) -> bool;
		/// \brief [[BPR-1775]]. The post being edited, if any.
		[[nodiscard]] auto comment_editing() const -> std::optional<std::string>;
		/// \brief [[BPR-1775]]. What the composition says beside the editor;
		/// `aSaving` holds the text and Save while GitHub is asked.
		void present_comment_edit_status(const QString& aText, bool aSaving = false);
		/// \brief [[BPR-1775]]. GitHub's body changed since editing began:
		/// offers to go on editing from `aNewer`. The typed text stays until
		/// the offer is taken.
		void offer_newer_comment(std::string aNewer);
		/// \brief [[BPR-1775]]. Closes the editor, as after a save GitHub
		/// answered or on Cancel; the post then shows what the next read
		/// brings.
		void close_comment_edit();
		/// \brief [[BPR-1775]]. The body editing began with.
		[[nodiscard]] auto comment_edit_original() const -> std::string;
		/// \brief [[BPR-1775]]. The editor's parts, while one is shown.
		[[nodiscard]] auto comment_editor() const -> scoped_prose_editor*;
		[[nodiscard]] auto comment_edit_save_button() const -> QAbstractButton*;
		[[nodiscard]] auto comment_edit_cancel_button() const -> QAbstractButton*;
		[[nodiscard]] auto comment_edit_newer_button() const -> QAbstractButton*;
		[[nodiscard]] auto comment_edit_status() const -> QLabel*;
		/// \brief [[BPR-1741]]. Each thread's resolution as the viewer
		/// should see it and GitHub's permission for it, by thread identity.
		void present_thread_resolutions(std::map<std::string, thread_resolution_presentation> aByThread);
		/// \brief [[BPR-0657]]. Offers Resolve and Unresolve on the threads when the
		/// provider and the server both resolve threads.
		void               set_resolution_offered(bool aOffered);
		[[nodiscard]] auto resolution_offered() const noexcept -> bool;
		/// \brief [[BPR-1824]]. Why the whole pull request's threads cannot be
		/// resolved, or `none`; the menu says it rather than "unavailable".
		void               set_resolution_refusal(core::thread_resolution_refusal aRefusal);
		[[nodiscard]] auto resolution_refusal() const noexcept -> core::thread_resolution_refusal;
		/// \brief The unread threads of the whole pull request, in file order.
		///
		/// \details [[BPR-0972]]. Decided once per adopted forest or read
		/// state and retained, so a scroll frame that asks again walks nothing.
		[[nodiscard]] auto unread_threads_in_file_order() const -> const std::vector<std::string>&;
		/// \brief Owner look v002 ([[BPR-2158]]). Every review thread on a file
		/// with its path, in file and then line order: Previous and Next
		/// comment walk it past the file shown.
		[[nodiscard]] auto threads_in_file_order() const -> std::vector<std::pair<std::string, QString>>;
		/// \brief [[BPR-1601]]. File threads, then PR comments and summaries in Discuss order.
		[[nodiscard]] auto unread_conversations_in_navigation_order() const -> const std::vector<std::string>&;
		/// \brief The zero-based lines of the threads whose first post is unread.
		[[nodiscard]] auto unread_marked_lines(core::diff_side aSide) const -> std::vector<std::uint32_t>;
		[[nodiscard]] auto unread_post_count() const noexcept -> std::size_t;
		/// \brief [[BPR-1290]]. Every shown thread's gutter marker and scroll-bar
		/// tick on one side, by zero-based line, sorted and one per line.
		[[nodiscard]] auto line_marks(core::diff_side aSide) const -> std::vector<thread_line_mark>;
		/// \brief Makes a thread current without reporting it or opening it.
		void set_current_thread(const std::string& aId);
		/// \brief Lays the notes out again against the locator.
		void relayout();
		/// \brief Follows the code: places the notes at their lines again,
		/// retaining measured notes.
		void refresh_geometry();

		/// \brief [[BPR-1290]]. Opens one thread in place, closing any other,
		/// makes it current and reports it as the reviewer's choice.
		void open_thread(const std::string& aId);
		/// \brief [[BPR-1293]]. Opens one thread in place, as `open_thread`
		/// does, with one of its posts picked: washed amber and brought into
		/// view. A post the thread does not hold picks nothing.
		void open_thread_at(const std::string& aId, const std::string& aPost);
		/// \brief The picked post of the open thread, or empty. Opening
		/// another thread, or closing this one, drops it.
		[[nodiscard]] auto picked_post() const noexcept -> const std::string&;
		/// \brief [[BPR-1581]]. Marks the picked post's arrival: its wash is
		/// drawn stronger with an amber ring, held, then dissolves to the
		/// pick's resting wash.
		void mark_arrival();
		/// \brief [[BPR-1581]]. The post whose arrival is marked, and how
		/// strongly now, from 1 to 0; none once it has settled.
		[[nodiscard]] auto arrival_post() const -> std::string;
		[[nodiscard]] auto arrival_strength() const noexcept -> qreal;
		/// \brief Opens a closed thread or closes the open one.
		void toggle_thread(const std::string& aId);
		/// \brief Closes the open thread, keeping it current.
		void               close_thread();
		[[nodiscard]] auto open_thread_id() const noexcept -> const std::string&;
		/// \brief Told whenever the open thread or the note under the pointer
		/// changes, so the gutter and scroll bar can follow.
		void set_marks_observer(std::function<void()> aObserver);
		/// \brief A wheel over the notes scrolls the code: the sink reports
		/// whether the code moved. While the code cannot move, the notes take
		/// the overflow themselves.
		void set_scroll_forward(std::function<bool(QWheelEvent&)> aForward);
		/// \brief Link destinations in a post, with the post's identity, for
		/// the application's admission ([[BPR-0456]]).
		void set_link_dispatch(std::function<void(const std::string&, const std::string&)> aDispatch);
		/// \brief [[BPR-1566]]. Where the open thread's label menu sends About
		/// Conventional Comments. Unset, the menu does not offer it.
		void set_reference_dispatch(std::function<void(core::reference_page)> aDispatch);
		/// \brief RFC-BPR-0084. Asks for a published post's images by post identity.
		void set_image_dispatch(std::function<void(const std::string&, std::vector<core::markdown_image_reference>)> aDispatch);
		/// \brief Shows one image answer in the open thread's post it names.
		void apply_markdown_image(const application::markdown_image_projection& aImage);
		/// \brief Asks again for the open thread's images still loading.
		void request_images();
		/// \brief RFC-BPR-0084 Q3. Shows or hides images in every post, the
		/// open thread's now and any thread opened later.
		void set_images_shown(bool aShown);
		/// \brief [[BPR-1290]]. Shows the reply editor at the foot of the open
		/// thread and gives it focus.
		void begin_reply(const std::string& aThread);
		/// \brief [[BPR-1290]]. The draft editor's reply, so the editor in
		/// place shows and offers what the draft holds.
		void present_reply(std::optional<thread_reply_presentation> aReply);
		/// \brief [[BPR-1290]]. Each edit of the reply written in place.
		void set_reply_text_dispatch(std::function<void(const std::string&, const QString&)> aDispatch);
		/// \brief The editor of the reply in place, while one is shown.
		[[nodiscard]] auto reply_editor() const -> QPlainTextEdit*;
		/// \brief [[BPR-1212]]. Opens my draft in the drawer as a draft thread
		/// at its line, writing, and gives its field focus. The draft is
		/// shown even while it is empty.
		void begin_draft();
		/// \brief [[BPR-1212]]. Stops writing: the draft closes to its note,
		/// or to nothing while it is empty.
		void end_draft();
		/// \brief [[BPR-1212]]. Whether my draft is open and being written.
		[[nodiscard]] auto writing_draft() const noexcept -> bool;
		/// \brief [[BPR-1212]]. What the draft editor holds for the draft.
		void present_draft(std::optional<draft_composer_presentation> aDraft);
		/// \brief [[BPR-1212]]. Each edit of the draft written in the drawer.
		void set_draft_text_dispatch(std::function<void(const QString&)> aDispatch);
		/// \brief [[BPR-1212]]. The draft's field, while it is being written.
		[[nodiscard]] auto draft_editor() const -> QPlainTextEdit*;
		/// \brief [[BPR-2051]]. Where Enhance on the draft or reply written
		/// here goes, and how a run it started is stopped.
		void set_enhance_dispatch(enhance_dispatch aDispatch, enhance_cancel aCancel);
		/// \brief [[BPR-2051]]. The answer to one press; false when no field
		/// shown here asked it.
		auto present_enhancement(const enhance_outcome& aOutcome) -> bool;
		/// \brief [[BPR-2051]]. The open card's Enhance control and panel.
		[[nodiscard]] auto enhance_control() const -> enhance_button*;
		[[nodiscard]] auto enhance_panel() const -> enhance_suggestion*;
		/// \brief [[BPR-1212]]. The identity the draft's note and composer carry.
		[[nodiscard]] static auto draft_identity() -> const std::string&;
		/// \brief The reply's label chip and menu ([[BPR-1083]]'s
		/// controls, as the composer has them), while a reply is shown.
		[[nodiscard]] auto reply_label_controls() const -> feedback_label_controls*;
		/// \brief The open thread's card, while one is open.
		[[nodiscard]] auto open_card() const -> QWidget*;
		/// \brief RFC-BPR-0128. The open thread's identity; empty for none.
		[[nodiscard]] auto open_identity() const noexcept -> const std::string& { return Open; }
		/// \brief The out-of-view chips shown now, top first then bottom.
		[[nodiscard]] auto edge_chips() const -> std::vector<QAbstractButton*>;
		/// \brief What the notes scroll by themselves beyond the code.
		[[nodiscard]] auto overflow() const -> int;
		/// \brief One note's rectangle in viewport coordinates, or empty.
		[[nodiscard]] auto note_rect(const std::string& aId) const -> QRect;
		/// \brief The thread whose note is under the pointer, if any.
		[[nodiscard]] auto hovered_thread() const noexcept -> const std::string&;

		[[nodiscard]] auto cards() const noexcept -> const std::vector<thread_card>&;
		/// \brief [[BPR-1779]]. Whether the notes read in the conversation's
		/// time order: the Pull request view, and Discuss over the whole pull
		/// request. Beside the code and in one file they keep line order.
		[[nodiscard]] auto timeline() const noexcept -> bool;
		/// \brief [[BPR-1779]]. The lines drawn between the comments in
		/// time order, in content coordinates, top to bottom.
		[[nodiscard]] auto timeline_lines() const noexcept -> const std::vector<timeline_line>&;
		[[nodiscard]] auto current_thread() const noexcept -> const std::string&;
		[[nodiscard]] auto layouts() const noexcept -> std::uint64_t;
		/// \brief [[BPR-0972]]. Retained conversation nodes visited by layouts.
		[[nodiscard]] auto layout_nodes_visited() const noexcept -> std::uint64_t;
		/// \brief Number of note text measurements, excluding retained-cache hits.
		[[nodiscard]] auto measurements() const noexcept -> std::uint64_t;
		/// \brief Accepted bodies parsed for marks and narrative boundaries.
		[[nodiscard]] auto body_annotations() const noexcept -> std::uint64_t;
		/// \brief The note under a viewport point.
		[[nodiscard]] auto hit_test(QPoint aViewportPoint) const -> std::optional<std::size_t>;
		/// \brief Lines that carry a thread on a side, zero-based, for the gutter.
		[[nodiscard]] auto marked_lines(core::diff_side aSide) const -> std::vector<std::uint32_t>;
		[[nodiscard]] auto post_count() const noexcept -> std::size_t;
		/// \brief The colour of what is new and the amber of the open thread.
		void               set_colours(const QColor& aAccent, const QColor& aMarker);
		[[nodiscard]] auto accent() const noexcept -> const QColor&;
		[[nodiscard]] auto marker() const noexcept -> const QColor&;
		/// \brief [[BPR-1083]]. The label tints of the theme on screen and the
		/// mark style.
		void               set_label_marks(const feedback_mark_theme& aTheme);
		[[nodiscard]] auto label_marks() const noexcept -> const feedback_mark_theme&;
		/// \brief Where a closed note draws its first post's 16 px mark, in
		/// viewport coordinates: centred under its 18 px avatar, 3 px below
		/// it. Empty for a plain first post.
		[[nodiscard]] auto mark_rect(const thread_card& aCard) const -> QRect;
		/// \brief [[BPR-1471]]. Where a closed note that is not a draft draws
		/// its meta row; all empty for a draft or a note without posts.
		[[nodiscard]] auto closed_meta(const thread_card& aCard) const -> closed_note_meta;
		/// \brief Where a closed note's rail is drawn, in viewport coordinates:
		/// down its left edge, 2 px wide or 4 px when blocking, growing outward.
		[[nodiscard]] auto rail_rect(const thread_card& aCard) const -> QRectF;

	protected:
		void changeEvent(QEvent* aEvent) override;
		void paintEvent(QPaintEvent* aEvent) override;
		void resizeEvent(QResizeEvent* aEvent) override;
		void showEvent(QShowEvent* aEvent) override;
		void hideEvent(QHideEvent* aEvent) override;
		void mousePressEvent(QMouseEvent* aEvent) override;
		void mouseMoveEvent(QMouseEvent* aEvent) override;
		void keyPressEvent(QKeyEvent* aEvent) override;
		void wheelEvent(QWheelEvent* aEvent) override;
		void contextMenuEvent(QContextMenuEvent* aEvent) override;
		void focusInEvent(QFocusEvent* aEvent) override;
		void focusOutEvent(QFocusEvent* aEvent) override;
		/// \brief [[BPR-2158]] correction. N and Shift+N step through the threads with
		/// new posts ahead of the window's N (a Local comment), as a list reads
		/// its own keys ahead of window shortcuts.
		auto event(QEvent* aEvent) -> bool override;
		auto viewportEvent(QEvent* aEvent) -> bool override;
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override;
		/// \brief [[BPR-1290]]. Tab from the notes with a thread open enters
		/// its first post's text, where the conversation is read.
		auto focusNextPrevChild(bool aNext) -> bool override;

	private:
		/// [[BPR-1566]]. Gives an open card's label menu this view's reference dispatch.
		void bind_reference(feedback_label_controls& aLabels);
		/// \brief Ends writing the draft in the drawer when its card is the
		/// open one: called before the open card changes.
		void leave_draft() noexcept;

		struct closed_measure
		{
			QString Body;
			bool    Resolved = false;
			bool    Draft    = false;
			QString Notice;
			int     Height = 0;
			/// [[BPR-1374]]. How many lines the notice takes.
			int NoticeLines = 0;
			/// How many lines of the body the note shows: one when it fits,
			/// otherwise two (one when resolved), the last elided.
			int Lines = 0;
		};
		struct measurement_generation
		{
			int                                   Width = -1;
			std::map<std::string, closed_measure> Notes;
		};

		void               index_retained();
		void               measure_closed(thread_card& aCard, int aWidth);
		void               clear_measurements();
		void               forget_measurements_if(const std::function<bool(const std::string&)>& aForget);
		[[nodiscard]] auto aligned() const noexcept -> bool;
		[[nodiscard]] auto closed_measure_of(const thread_card& aCard) const -> const closed_measure*;
		void               place_cards(bool aForce);
		/// \brief [[BPR-1740]]. Sets the notes' scroll range from the
		/// content's height. The drawer scrolls past its end as the code
		/// panes do ([[BPR-1291]]), until only its last note's last line is
		/// at the top; Discuss's feed ends with its last note at the bottom.
		void set_scroll_range();
		/// \brief [[BPR-1740]]. Whether the open thread hangs from its
		/// line, and so moves with the code; true with none open.
		[[nodiscard]] auto open_thread_hangs() const -> bool;
		void               place_open_card();
		void               sync_edges();
		void               sync_pull_request_edge();
		/// [[BPR-1779]]. The core's items for the roots the notes hold, or for
		/// every root of the forest, as the time order reads them.
		[[nodiscard]] auto timeline_item_of(std::size_t aIndex) const -> core::timeline_item;
		/// [[BPR-1779]]. Puts the cards in the core's time order, keeping only
		/// the comments it lists.
		void arrange_timeline();
		/// [[BPR-1779]]. Lays the cards and their lines out in time order.
		void               place_timeline();
		void               paint_timeline(QPainter& aPainter, const QRect& aClip);
		[[nodiscard]] auto timeline_path_at(QPoint aViewportPoint) const -> const timeline_line*;
		void               rebuild_open_card();
		void               relayout_read_state();
		void               measure_cards();
		bool               GeometryPending = false;
		void               choose(std::size_t aCard, bool aOpen);
		void               set_hovered(const std::string& aId);
		void               notify_marks();
		void               step_new(int aDirection);
		void               ensure_visible(const QRect& aViewportRect);
		/// [[BPR-2173]]. Beside the code, the card just opened is brought
		/// whole into view by the notes' own scroll.
		void reveal_opened(const std::string& aId);
		/// \brief Brings what is being read in an open thread into view: the
		/// notes' own overflow in Discuss, the code (which the drawer follows)
		/// in Review, as a wheel over the notes would.
		void reveal_reading(const QRect& aViewportRect);
		/// \brief Brings a control of the open thread into view, or a text's
		/// caret rather than all of it.
		void               reveal_focus(QWidget& aWidget);
		[[nodiscard]] auto card_index(const std::string& aId) const -> std::optional<std::size_t>;
		[[nodiscard]] auto to_content(QPoint aViewportPoint) const -> QPoint;
		[[nodiscard]] auto to_viewport(QRect aContentRect) const -> QRect;
		[[nodiscard]] auto handle_thread_key(QKeyEvent& aEvent, bool aTyping) -> bool;
		void               paint_closed(QPainter& aPainter, const thread_card& aCard, bool aHovered, bool aFocused) const;

		std::vector<application::github_conversation_projection> Retained;
		/// The file identity of each retained node's path, index for index,
		/// derived once per adopted forest rather than once per relayout.
		std::vector<core::file_identity> RetainedFiles;
		/// [[BPR-0972]]. Per adopted forest: the first node for each identity,
		/// each node's root and depth, and every node by its root's file, so a
		/// layout scoped to one file visits that file's conversations only.
		/// [[BPR-1362]]. Keyed by value: a view into `Retained` dangled once
		/// `Retained` was replaced, and MSVC's clear() re-hashes its keys.
		std::unordered_map<std::string, std::size_t> RetainedById;
		std::vector<std::size_t>                     RetainedRoot;
		std::vector<std::size_t>                     PullRequestRoots;
		/// [[BPR-1779]]. A thread root's first comment: the review it was
		/// posted in and when, by the root's index.
		std::unordered_map<std::size_t, std::pair<std::string, std::string>> ThreadOrigins;
		/// [[BPR-1779]]. The rows the core arranged for the time order, and
		/// the lines laid out between the comments.
		std::vector<core::timeline_row> TimelineRows;
		core::timeline_filter_summary   TimelineSummary;
		std::vector<timeline_line>      TimelineLines;
		/// [[BPR-1779]]. The path line under the pointer, drawn in full ink.
		std::string HoveredPath;
		/// [[BPR-2171]].
		core::author_bot_marks                        BotMarks;
		std::function<void(const std::string&, bool)> AuthorBotDispatch;
		std::optional<int>                            ColumnLimit = 820;
		std::set<std::string>                         Unfolded;
		std::size_t                                   FoldedComments   = 0U;
		std::size_t                                   FoldedThreads    = 0U;
		std::size_t                                   FoldedUnresolved = 0U;
		[[nodiscard]] auto                            card_width_for(const thread_card& aCard) const -> std::pair<int, int>;
		void                                          unfold_for(const std::string& aId);
		/// [[BPR-1779]]. The pull request's comments and their unread, as the
		/// Pull request chip counts them: comments, not reviews.
		mutable std::optional<std::pair<std::size_t, std::size_t>> PullRequestCounts;
		std::vector<std::size_t>                                   RetainedDepth;
		std::vector<std::pair<core::file_identity, std::size_t>>   RetainedByFile;
		/// [[BPR-1083]]. Each retained node's mark, header length and
		/// ([[BPR-1086]]) lead, read once per adopted forest from the accepted
		/// body.
		/// [[BPR-1404]]. One ordered set of exact accepted bodies keeps its
		/// annotations across endpoint placement changes, including a
		/// temporarily empty forest. A different nonempty set replaces it;
		/// authority clear releases it.
		std::vector<std::string>        AnnotatedBodies;
		std::uint64_t                   BodyAnnotations = 0U;
		std::vector<feedback_body_mark> RetainedMarks;
		std::vector<qsizetype>          RetainedHeaders;
		std::vector<qsizetype>          RetainedLeads;
		/// [[BPR-1806]]. Each retained post read as a published tour stop,
		/// and its tour's length when known.
		std::vector<std::optional<core::published_tour_stop>> RetainedTourStops;
		std::vector<std::optional<std::size_t>>               RetainedTourCounts;
		/// [[BPR-1810]]. Where each retained stop's Previous and Next lead,
		/// as retained indexes.
		std::vector<core::published_tour_stop_links> RetainedTourLinks;
		feedback_mark_theme                          MarkTheme;
		/// [[BPR-0972]]. The first read state of each thread.
		std::unordered_map<std::string, std::size_t>                                         ReadStateByThread;
		mutable std::optional<std::vector<std::string>>                                      UnreadThreads;
		mutable std::optional<std::vector<std::string>>                                      UnreadConversations;
		mutable std::map<core::file_identity, std::size_t>                                   UnreadThreadsByFile;
		std::uint64_t                                                                        LayoutNodesVisited = 0U;
		std::vector<thread_card>                                                             Cards;
		std::optional<core::file_identity>                                                   AnchorFile;
		std::optional<core::review_comment_draft>                                            Draft;
		std::vector<core::review_comment_draft>                                              SavedDrafts;
		std::vector<core::reply_draft_decision>                                              UnsentReplies;
		std::vector<moved_suggestion_card>                                                   MovedSuggestions;
		std::vector<local_comment_card>                                                      LocalComments;
		QAbstractButton*                                                                     LocalFileChip      = nullptr;
		QAbstractButton*                                                                     LocalElsewhereChip = nullptr;
		QPointer<QMenu>                                                                      LocalElsewhereMenu;
		std::function<void(std::uint64_t, std::uint64_t, const std::string&)>                LocalEditDispatch;
		void                                                                                 sync_local_chips();
		void                                                                                 open_local_elsewhere_menu();
		std::optional<core::file_comparison_digest>                                          CurrentComparison;
		card_line_locator                                                                    Locator;
		card_thread_dispatch                                                                 ThreadDispatch;
		card_action_dispatch                                                                 ActionDispatch;
		defect_badges                                                                        DefectBadges;
		std::function<void(QMenu&, const std::string&)>                                      ContextMenuBuilder;
		std::function<void(const std::string&)>                                              AuthorReadDispatch;
		std::function<void()>                                                                MarksObserver;
		std::function<bool(QWheelEvent&)>                                                    ScrollForward;
		std::function<void(const std::string&, const std::string&)>                          LinkDispatch;
		std::function<void(core::reference_page)>                                            ReferenceDispatch;
		std::function<void(const std::string&, std::vector<core::markdown_image_reference>)> ImageDispatch;
		bool                                                                                 ImagesShown = true;
		std::function<void(const std::string&, const QString&)>                              ReplyTextDispatch;
		std::optional<thread_reply_presentation>                                             Reply;
		/// [[BPR-1212]]. My draft is open and being written in the drawer.
		bool                                       Writing = false;
		std::optional<draft_composer_presentation> Composer;
		std::function<void(const QString&)>        DraftTextDispatch;
		enhance_dispatch                           EnhanceDispatch;
		enhance_cancel                             EnhanceCancel;
		QString                                    Viewer;
		std::string                                Current;
		bool                                       PullRequestComments = false;
		std::string                                NewConversationBoundary;
		QAbstractButton*                           PullRequestChip = nullptr;
		/// [[BPR-1781]]. Where each mode was left, restored on return.
		struct mode_place
		{
			std::string Open;
			std::string Current;
			int         Scroll = 0;
		};
		mode_place                              LinesPlace;
		mode_place                              ConversationPlace;
		std::function<void(const std::string&)> PullRequestLinkDispatch;
		std::string                             Open;
		/// [[BPR-1293]]. The open thread's picked post, and the one a pick
		/// is about to open with.
		std::string Picked;
		std::string PickedNext;
		/// [[BPR-1581]]. The arrival mark's post and its animation.
		std::string                 ArrivalPost;
		QPointer<QVariantAnimation> ArrivalAnimation;
		bool                        RevealPicked      = false;
		QTimer*                     PickedReveal      = nullptr;
		int                         PickedRevealSteps = 0;
		int                         PickedRevealLast  = 0;
		std::string                 Hovered;
		/// The thread whose reply editor is shown in its open card.
		std::string                                                     Replying;
		std::vector<thread_read_state>                                  ReadState;
		bool                                                            ReadStateKnown    = false;
		bool                                                            Feed              = false;
		bool                                                            ResolutionOffered = false;
		core::thread_resolution_refusal                                 ResolutionRefusal = core::thread_resolution_refusal::no_write_connection;
		std::map<std::string, core::reaction_summary>                   ReactionsBySubject;
		std::map<std::string, thread_resolution_presentation>           ResolutionsByThread;
		std::function<void(const std::string&, core::reaction_content)> ReactionDispatch;
		/// [[BPR-1775]]. Edit on my comments, the comment being edited (its
		/// original, GitHub's newer text when offered) and where Save goes.
		struct comment_edit_session
		{
			std::string                Post;
			std::string                Original;
			std::string                Text;
			std::optional<std::string> Newer;
			QString                    Status;
			bool                       Saving = false;
		};
		bool                                                                      CommentEditOffered = false;
		bool                                                                      TourOpenOffered    = false;
		std::set<std::string>                                                     AddressedThreads;
		ask_menu_filler                                                           AskMenuFiller;
		std::map<std::string, thread_claim>                                       Claims;
		std::function<void(std::size_t)>                                          ClaimAnswerDispatch;
		std::function<void(const std::string&, const core::published_tour_stop&)> TourOpenDispatch;
		/// [[BPR-1810]]. Where a stop's chevrons go, and which chevron the
		/// stepped-to thread's plate takes focus on ("" for none).
		void                                                                        focus_tour_step();
		std::function<void(const std::string&)>                                     TourStepDispatch;
		std::string                                                                 TourStepThread;
		QString                                                                     TourStepFocus;
		std::optional<comment_edit_session>                                         CommentEdit;
		std::function<void(const comment_edit_save&)>                               CommentEditDispatch;
		void                                                                        present_comment_edit();
		void                                                                        save_comment_edit();
		bool                                                                        HasFocus = false;
		discuss_filters                                                             Filters;
		QString                                                                     PullRequestAuthor;
		card_excerpt_locator                                                        ExcerptLocator;
		card_suggestion_locator                                                     SuggestionLocator;
		std::optional<markdown_code_theme>                                          CodeTheme;
		std::function<void(std::vector<application::markdown_code_syntax_request>)> CodeDispatch;
		std::function<void()>                                                       LayoutObserver;
		QFont                                                                       CodeFont;
		QColor                                                                      CodeBackground{ 0xF6, 0xF8, 0xFA };
		QColor                                                                      CodeInk{ 0x1F, 0x23, 0x28 };
		cards_scope                                                                 Scope = cards_scope::this_file;
		QColor                                                                      Accent{ 0x09, 0x69, 0xDA };
		QColor                                                                      Marker{ 0xD2, 0x99, 0x22 };
		int                                                                         ContentHeight = 0;
		std::uint64_t                                                               Layouts       = 0U;
		/// [[BPR-1802]]. The pull request's repository and the permalink lane.
		std::function<void(std::vector<application::markdown_blob_snippet_request>)> PermalinkDispatch;
		std::optional<core::markdown_repository>                                     PermalinkRepository;
		/// [[BPR-0673]]. Two widths, because a scroll bar that comes and goes
		/// alternates the content width; keeping one width re-measured every
		/// note of a large review each time it did.
		measurement_generation Measurements;
		measurement_generation PreviousMeasurements;
		QFont                  MeasurementFont;
		std::uint64_t          MeasurementCount = 0U;
		/// [[BPR-2171]]. Each closed note's preview, by its card, kept while
		/// its body is the same: read once, not at every width.
		struct preview_entry
		{
			QString                                     Body;
			qsizetype                                   Header = 0;
			std::shared_ptr<const detail::note_preview> Preview;
		};
		mutable std::unordered_map<std::string, preview_entry> Previews;
		[[nodiscard]] auto                                     preview_for(const thread_card& aCard) const -> const detail::note_preview&;
		open_thread_card*                                      OpenCard = nullptr;
		std::vector<QAbstractButton*>                          Chips;
		std::size_t                                            ChipsShown = 0U;
		QWidget*                                               BandAbove  = nullptr;
		QWidget*                                               BandBelow  = nullptr;
	};
} // namespace bpr::qt

#endif
