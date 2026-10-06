#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_DRAFT_EDITOR_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_DRAFT_EDITOR_HPP

#include "bpr/application/review_mutation_port.hpp"
#include "bpr/application/review_synchronization.hpp"
#include "bpr/core/agent_chat_scope.hpp"
#include "bpr/core/review_ledger.hpp"
#include "bpr/core/review_round_trip.hpp"
#include "bpr/core/revisioned_review_draft.hpp"
#include "bpr/qt/feedback_label_marks.hpp"

#include <QString>
#include <QWidget>

#include <array>
#include <compare>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

class QAction;
class QLabel;
class QTimer;
class QPlainTextEdit;

namespace bpr::qt {
	class feedback_label_controls;

	/// \brief Names why no review-draft projection remains presentable.
	enum class review_draft_editor_clear_reason : std::uint8_t {
		selection_cleared,
		selection_invalidated,
		composition_retired,
	};

	/// \brief Records directly observable editor activity.
	struct review_draft_editor_statistics
	{
		std::uint64_t DraftIntents    = 0U;
		std::uint64_t ReceiptIntents  = 0U;
		std::uint64_t Presentations   = 0U;
		std::uint64_t MutationUpdates = 0U;
		/// How many proposed revisions have been shown beside an original.
		std::uint64_t WorkshopPresentations = 0U;
		/// How many were rejected. Rejection emits nothing, so without a
		/// count it would be indistinguishable from never having been wired.
		std::uint64_t RejectedRevisions = 0U;
		/// How many comment-workshop operations were asked for and dispatched.
		/// An unwired editor asks nobody anything and counts nothing, exactly
		/// as an unwired draft dispatch emits nothing and counts nothing.
		std::uint64_t WorkshopRequests = 0U;

		auto operator<=>(const review_draft_editor_statistics&) const = default;
	};

	/// \brief One proposed revision of a comment the reviewer already wrote.
	///
	/// \details The workshop's whole subject, and it is two strings rather
	/// than one diff. `Original` is the reviewer's own text as it stood when
	/// the workshop ran, and `Proposed` is what came back; they are separate
	/// members here and separate widgets on screen, so a proposal that
	/// overwrote the original would have nowhere to hide.
	///
	/// `Draft` is the draft the proposal was made against. The editor emits
	/// against that identity rather than against whatever it happens to be
	/// showing, and refuses when the two disagree — a revision applied to the
	/// wrong comment is worse than one applied to none.
	///
	/// Both are plain text. **No Markdown renderer exists anywhere in this
	/// tree**, and the before/after view is the editor's existing plain-text
	/// presentation for that reason; the decision and its cost are recorded in
	/// `docs/verification/RFC-BPR-0012.md` rather than left to be inferred
	/// from the absence of one.
	struct comment_workshop_proposal
	{
		core::review_draft_id Draft;
		std::string           Original;
		std::string           Proposed;
		/// Which of the six operations produced this after, and absent when
		/// none was asked for. Shown rather than held: a before/after that did
		/// not say which operation ran would leave the reviewer comparing two
		/// texts without the question that separates them.
		std::optional<core::workshop_operation> Operation;

		auto operator<=>(const comment_workshop_proposal&) const = default;
	};

	/// \brief One comment-workshop operation the reviewer asked for.
	///
	/// \details The editor's third and last outbound value, and it names the
	/// draft rather than describing it: the reviewer asked for this operation
	/// on the comment they were looking at, and a request that carried only
	/// the operation would be answered against whichever draft the composition
	/// happened to hold by the time it arrived.
	///
	/// There is no field here a revision, a body, or a provider could occupy.
	/// The widget asks; it does not propose, and it has nothing to propose
	/// with.
	struct comment_workshop_request
	{
		core::review_draft_id    Draft;
		core::workshop_operation Operation = core::workshop_operation::clarify;

		auto operator<=>(const comment_workshop_request&) const = default;
	};

	/// \brief One asked-for turn, ended.
	///
	/// \details BPR-0538. The counterpart of `comment_workshop_proposal`: the
	/// turn a press started is over, so the six controls are open again
	/// whether or not it produced anything. `Reason` is why it produced
	/// nothing, and is empty when it produced an answer that is still waiting
	/// for the reviewer's conversion gesture — the editor says "no revision"
	/// only when there is none coming.
	///
	/// The draft is named for the reason the request names it: a settlement
	/// that arrived after the reviewer moved to another comment must not
	/// reopen controls, or write a reason, over a draft it was never about.
	struct comment_workshop_settlement
	{
		core::review_draft_id Draft;
		std::string           Reason;

		auto operator<=>(const comment_workshop_settlement&) const = default;
	};

	using review_draft_dispatch = std::function<void(core::review_draft_command)>;
	using file_receipt_dispatch = std::function<void(core::file_receipt_command)>;
	using comment_workshop_dispatch
		= std::function<void(comment_workshop_request)>;

	/// \brief Every member the review-draft editor holds, as one readable value.
	///
	/// \details Declared as one aggregate, and held as the editor's only data
	/// member, so that "this widget holds no provider, no endpoint, no store,
	/// and no process" is a list a test reads rather than a claim a reviewer
	/// checks by eye. A guard in
	/// `tests/adapters_qt/review_draft_editor_tests.cpp` binds every member
	/// here by name and states its type, so adding one more member — of any
	/// type — fails to compile, and the same test pins
	/// `sizeof(review_draft_editor)` to this aggregate so a member added to
	/// the class outside it fails too.
	///
	/// What is here is two presentation flags, three dispatch callbacks, the
	/// accepted projections, seven counters, and the widgets that show them.
	/// The only way out is a semantic command or a workshop request.
	struct revisioned_draft_editor_state
	{
		std::function<void(core::revisioned_review_draft_command)> Dispatch;
		std::optional<core::revisioned_review_draft>               Accepted;
		std::optional<core::revisioned_review_draft>               Replacement;
		std::optional<core::revisioned_review_draft_command>       Candidate;
		bool                                                       Conflict                      = false;
		bool                                                       Selected                      = false;
		bool                                                       DispatchingWorkshopAcceptance = false;
		/// [[BPR-1083]]. An edit is with the draft owner and unanswered; a
		/// change made meanwhile waits for its answer (`Deferred`) and then
		/// goes on the accepted revision, so a label shortcode replaced the
		/// moment its closing colon is typed is not refused as stale.
		bool     InFlight = false;
		bool     Deferred = false;
		QTimer*  Flush    = nullptr;
		QAction* Reload   = nullptr;
		QAction* Reapply  = nullptr;
	};
	struct review_draft_editor_members
	{
		/// True only while `present` is writing an accepted body into the
		/// editor, so presenting never looks like the reviewer typing.
		bool ApplyingPresentation = false;

		/// True from a workshop press until that press is answered, cleared,
		/// or its draft goes away. One press, one turn: while it is true the
		/// six controls are disabled and say why, so a second press is refused
		/// on the surface that asked rather than queued behind the first.
		bool WorkshopAsked = false;

		review_draft_dispatch DraftDispatch;
		file_receipt_dispatch ReceiptDispatch;
		/// Where an asked-for operation goes. Absent by default and set by the
		/// composition that owns the workshop host, so an editor nobody wired
		/// asks for nothing rather than reaching for a host of its own.
		comment_workshop_dispatch WorkshopDispatch;
		/// Qt composition notification after accepted presentation changes.
		std::function<void()> PresentationObserver;

		QPlainTextEdit* Body           = nullptr;
		QLabel*         TargetLabel    = nullptr;
		QLabel*         ReplyTarget    = nullptr;
		QLabel*         MutationStatus = nullptr;
		/// The reviewer's own text, shown beside the proposal rather than
		/// replaced by it. Read-only: the editable original is `Body`.
		QPlainTextEdit* WorkshopOriginal = nullptr;
		/// What the agent proposed instead. A separate widget, so a single
		/// buffer that overwrote the original is a visible mistake.
		QPlainTextEdit* WorkshopProposal = nullptr;
		/// Which operation was asked for, and which one produced what is
		/// shown. Separate from both texts because it is neither of them.
		QLabel* WorkshopOperation = nullptr;

		QAction* Publish          = nullptr;
		bool     RetryPublication = false;
		QAction* Discard          = nullptr;
		QAction* MarkRead         = nullptr;
		QAction* Accept           = nullptr;
		QAction* Exclude          = nullptr;
		QAction* AcceptRevision   = nullptr;
		QAction* RejectRevision   = nullptr;
		/// One action per declared operation, indexed by the enumerator, and
		/// exactly as many as there are operations. A seventh operation grows
		/// this array rather than being offered from somewhere else, and an
		/// operation with no control here fails the test that names all six.
		std::array<QAction*, core::workshop_operation_count> WorkshopOperations{ };

		/// [[BPR-1083]]. The label chip, its menu, the decoration toggles and
		/// the leading shortcodes. They edit `Body`'s text and nothing else, so
		/// a label leaves this widget the way typed text does.
		feedback_label_controls* Labels = nullptr;

		std::optional<core::review_comment_draft> ShownDraft;
		std::optional<core::file_receipt>         ShownReceipt;
		std::optional<comment_workshop_proposal>  ShownProposal;
		review_draft_editor_statistics            Statistics;
		revisioned_draft_editor_state             Revisioned;
	};

	/// \brief Composes a review comment and asks for it to be published.
	///
	/// \details The editor is given two dispatch callbacks and nothing else.
	/// There is no provider to hold and no endpoint to reach: the only way out
	/// of this widget is a semantic command, which is what makes "a widget
	/// never calls a mutation endpoint" true by construction rather than by
	/// review.
	///
	/// Nothing the user does changes what the editor shows. Every action emits
	/// an intent and waits; the displayed state moves only when `present` is
	/// called with an accepted projection. A widget that updated itself
	/// optimistically would show a comment as published while the request was
	/// still in flight — and would keep showing it if the request failed.
	///
	/// The comment workshop lives here rather than beside the editor because
	/// it is the same rule applied to a second pair of texts. Accepting a
	/// proposed revision emits one `review_draft_command` and writes nothing:
	/// the reviewer's original stays byte-identical until an accepted edit
	/// projection comes back. Rejecting emits nothing at all and leaves it
	/// byte-identical for good.
	class review_draft_editor final : public QWidget
	{
	public:
		using draft_dispatch   = review_draft_dispatch;
		using receipt_dispatch = file_receipt_dispatch;

		review_draft_editor(
			draft_dispatch   aDraftDispatch,
			receipt_dispatch aReceiptDispatch,
			QWidget*         aParent = nullptr);

		/// \brief Adopts one accepted draft projection.
		void present(const core::review_comment_draft& aDraft);
		void set_revisioned_dispatch(std::function<void(core::revisioned_review_draft_command)> aDispatch);
		void set_presentation_observer(std::function<void()> aObserver);
		void present(const core::revisioned_review_draft& aDraft);
		void present_conflict(const core::revisioned_draft_outcome& aOutcome);
		/// Acknowledges only the exact command the host accepted, including no-op edits.
		void               acknowledge(const core::revisioned_review_draft_command& aCommand, const core::revisioned_review_draft& aAccepted);
		void               acknowledge_published_body(const core::revisioned_review_draft& aPublished);
		[[nodiscard]] auto reload_draft_action() const noexcept -> QAction*;
		[[nodiscard]] auto reapply_draft_action() const noexcept -> QAction*;
		[[nodiscard]] auto rejected_candidate() const noexcept -> const std::optional<core::revisioned_review_draft_command>&;
		/// \brief True only during the synchronous dispatch from Accept revision.
		[[nodiscard]] auto workshop_accepting(
			const core::revisioned_review_draft_command& aCommand) const -> bool;

		/// \brief Adopts one accepted receipt projection.
		void present(const core::file_receipt& aReceipt);
		/// \brief Shows one proposed revision beside the original, and applies
		/// neither.
		///
		/// \details Presenting a proposal emits nothing and changes no draft.
		/// It fills two separate read-only surfaces; the editable body is left
		/// exactly as the reviewer left it, which is what makes rejection a
		/// no-op rather than an undo.
		void present(const comment_workshop_proposal& aProposal);
		/// \brief Reopens the six after the turn one press started has ended.
		///
		/// \details BPR-0538. A press shuts the six and says a turn is
		/// running; when that turn ends the statement stops being true, so the
		/// controls reopen here rather than waiting for a proposal that may
		/// never arrive. A turn that produced nothing says so where a proposal
		/// would have said which operation produced it — a reviewer shown
		/// nothing cannot tell a turn that answered nothing from one still
		/// thinking.
		void present(const comment_workshop_settlement& aSettlement);
		/// \brief Presents the provider's exact supported degradation.
		void present(application::review_provider_capabilities aCapabilities);
		/// \brief Presents one typed mutation refusal.
		void present(application::review_mutation_failure aFailure);
		/// \brief Presents an accepted provider receipt after server reconciliation.
		void present(
			const application::review_mutation_receipt& aReceipt,
			const application::reconciliation_result&   aReconciliation);
		/// \brief Clears every selection-bound projection without emitting intents.
		void present(review_draft_editor_clear_reason aReason);

		/// \brief Describes the target a publication would be sent to.
		///
		/// \details Shown before publishing rather than after, because the
		/// reviewer is agreeing to a specific pull request at a specific
		/// revision under a specific account, and cannot agree to something
		/// they were not shown.
		void describe_target(const QString& aDescription);

		[[nodiscard]] auto body_editor() const noexcept -> QPlainTextEdit*;
		/// \brief [[BPR-1083]]. The label chip, menu, toggles and shortcodes.
		[[nodiscard]] auto label_controls() const noexcept -> feedback_label_controls*;
		/// \brief The label tints of the theme on screen and the mark style.
		void set_label_marks(const feedback_mark_theme& aTheme);
		/// \brief The label helper (completion and shortcode replacement), off
		/// by default; the Settings section that owns it is BPR-1088.
		void               set_label_helper(bool aEnabled);
		[[nodiscard]] auto target_label() const noexcept -> QLabel*;
		/// \brief [[BPR-0657]]. Names the thread the shown draft answers; shown only while it does.
		[[nodiscard]] auto reply_target_label() const noexcept -> QLabel*;
		void               describe_reply_target(const QString& aDescription);
		[[nodiscard]] auto mutation_status_label() const noexcept -> QLabel*;
		[[nodiscard]] auto mutation_status_text() const -> QString;
		[[nodiscard]] auto publish_action() const noexcept -> QAction*;
		void               allow_publication_retry(bool);
		[[nodiscard]] auto discard_action() const noexcept -> QAction*;
		[[nodiscard]] auto mark_read_action() const noexcept -> QAction*;
		[[nodiscard]] auto accept_action() const noexcept -> QAction*;
		[[nodiscard]] auto exclude_action() const noexcept -> QAction*;

		/// \brief The widget showing the reviewer's own text in the workshop.
		[[nodiscard]] auto workshop_original_editor() const noexcept
			-> QPlainTextEdit*;
		/// \brief The widget showing the proposed revision. A different one.
		[[nodiscard]] auto workshop_proposal_editor() const noexcept
			-> QPlainTextEdit*;
		[[nodiscard]] auto workshop_original_text() const -> QString;
		[[nodiscard]] auto workshop_proposal_text() const -> QString;
		[[nodiscard]] auto accept_revision_action() const noexcept -> QAction*;
		[[nodiscard]] auto reject_revision_action() const noexcept -> QAction*;

		/// \brief The control that asks for one exact declared operation.
		///
		/// \details Indexed by the enumerator, so there is one control per
		/// operation and no way to ask for an operation there is no control
		/// for. An undeclared value has no control and returns none.
		[[nodiscard]] auto workshop_action(core::workshop_operation aOperation)
			const noexcept -> QAction*;
		[[nodiscard]] auto clarify_action() const noexcept -> QAction*;
		[[nodiscard]] auto shorten_action() const noexcept -> QAction*;
		[[nodiscard]] auto make_actionable_action() const noexcept -> QAction*;
		[[nodiscard]] auto phrase_as_a_question_action() const noexcept
			-> QAction*;
		[[nodiscard]] auto make_more_direct_action() const noexcept -> QAction*;
		[[nodiscard]] auto check_the_premise_action() const noexcept -> QAction*;
		/// \brief The label naming the operation asked for or shown.
		[[nodiscard]] auto workshop_operation_label() const noexcept -> QLabel*;
		[[nodiscard]] auto workshop_operation_text() const -> QString;

		/// \brief Names where an asked-for operation is answered.
		///
		/// \details Set by the composition rather than taken at construction,
		/// because the workshop host is composed later than the window that
		/// places this widget. Unset, every operation control is still shown
		/// and still disabled by the same rule — there is no state in which a
		/// control the reviewer can press asks nobody anything.
		void set_workshop_dispatch(comment_workshop_dispatch aDispatch);

		[[nodiscard]] auto shown_draft() const noexcept
			-> const std::optional<core::review_comment_draft>&;
		[[nodiscard]] auto shown_receipt() const noexcept
			-> const std::optional<core::file_receipt>&;
		[[nodiscard]] auto shown_proposal() const noexcept
			-> const std::optional<comment_workshop_proposal>&;
		[[nodiscard]] auto statistics() const noexcept
			-> review_draft_editor_statistics;

	private:
		void render_draft(const core::review_comment_draft& aDraft);
		void settle_in_flight();
		void emit_draft(core::review_draft_command_kind aKind);
		void emit_receipt(core::file_receipt_command_kind aKind);
		void accept_revision();
		void reject_revision();
		void request_workshop(core::workshop_operation aOperation);
		void clear_workshop();
		void refresh_enablement();

		review_draft_editor_members Members;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_DRAFT_EDITOR_HPP
