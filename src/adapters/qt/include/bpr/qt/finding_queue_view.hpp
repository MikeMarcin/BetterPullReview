#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_FINDING_QUEUE_VIEW_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_FINDING_QUEUE_VIEW_HPP

#include "bpr/application/agent_finding_delivery.hpp"
#include "bpr/core/agent_finding.hpp"
#include "bpr/core/agent_session.hpp"

#include <QString>
#include <QWidget>

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

class QAction;
class QLabel;
class QListWidget;

namespace bpr::qt {
	/// \brief The six things a reviewer may do with one delivered finding.
	///
	/// \details Exactly the list `docs/ux/ai-experience.md` names, and there
	/// is deliberately no seventh. `finding_queue_action_count` and the static
	/// assertion below keep the list and its size in agreement, so an action
	/// added here fails the sweeps that claim to cover every one of them
	/// rather than quietly never being exercised.
	///
	/// Three of these ask for a promotion, and this widget performs none of
	/// them: `core::promote_agent_finding` decides whether a finding may
	/// become a draft, a defect, or a note, and refuses with
	/// `anchor_elsewhere`, `boundary_anchor`, or `unanchored` when it may not.
	/// Every action here is therefore offered against every finding, whatever
	/// its anchor state — the anchor state is *shown* instead, so the reviewer
	/// can see that a finding did not land before asking for anything.
	/// Disabling an action on the anchor state would move that decision into a
	/// widget, and a widget that answered it could disagree with the core.
	enum class finding_queue_action : std::uint8_t {
		/// Show the code the finding claims to be about. Changes nothing.
		open_evidence,
		/// Ask the agent about this finding, which is an explicit chat turn.
		ask_follow_up,
		/// Promote it onto a local draft comment. RFC-BPR-0012's promotion
		/// path decides whether that is legal; this only asks.
		convert_to_draft_comment,
		/// Promote it onto a local defect, on the same terms.
		convert_to_draft_defect,
		/// Keep it as the reviewer's own private note.
		add_to_private_notes,
		/// Rule it out. This produces no artifact, which is why it is not a
		/// promotion choice in `core::agent_finding_promotion_choice`.
		dismiss,
	};

	/// \brief The number of declared finding actions.
	inline constexpr std::size_t finding_queue_action_count = 6U;

	static_assert(
		static_cast<std::size_t>(finding_queue_action::dismiss) + 1U
			== finding_queue_action_count,
		"finding_queue_action_count must cover every declared action");

	/// \brief Returns the stable name for the supplied value.
	///
	/// \details Presentation wording lives at this boundary. The core
	/// publishes no name for a gesture it does not have — these six are a Qt
	/// vocabulary, not a core one.
	[[nodiscard]] auto stable_name(finding_queue_action aAction) noexcept
		-> std::string_view;

	/// \brief Names exactly one delivered finding, and carries it whole.
	///
	/// \details A `core::agent_finding` has no identity field, because the
	/// core deliberately gives one no name of its own: it is a claim about a
	/// place in a frozen comparison, and the place is the identity. What
	/// singles one out is therefore the session it arrived in and its position
	/// in that delivery — the order `deliver_agent_findings` states, which is
	/// the order the payload stated.
	///
	/// The finding and its verdict are copied rather than referenced, so an
	/// intent that has left this widget cannot be re-read against a delivery
	/// that arrived afterwards. They travel together for the reason
	/// `application::agent_delivered_finding` pairs them: a finding without
	/// its verdict invites its reader to decide for itself whether the anchor
	/// held, and that decision has already been made once.
	struct queued_finding
	{
		core::agent_session_id Session;
		/// Its position in the delivery, counted from zero.
		std::size_t                 Ordinal = 0U;
		core::agent_finding         Finding;
		core::agent_finding_verdict Verdict;

		auto operator<=>(const queued_finding&) const = default;
	};

	/// \brief Asks for one thing to be done about one exact finding.
	///
	/// \details The only value that leaves this widget. There is no promotion
	/// result here, no draft, no defect, and no note: those are what the core
	/// makes of this request, and the widget never sees itself make them.
	///
	/// The default action is `open_evidence` because it is the only one of the
	/// six that produces nothing. An intent nobody filled in should ask for
	/// the harmless thing.
	struct finding_action_intent
	{
		finding_queue_action Action = finding_queue_action::open_evidence;
		queued_finding       Subject;

		auto operator<=>(const finding_action_intent&) const = default;
	};

	using finding_queue_intent_dispatch
		= std::function<void(finding_action_intent)>;

	/// \brief Records directly observable finding-queue activity.
	struct finding_queue_view_statistics
	{
		std::uint64_t ActionIntents = 0U;
		std::uint64_t Presentations = 0U;
		std::uint64_t Selections    = 0U;

		auto operator<=>(const finding_queue_view_statistics&) const = default;
	};

	/// \brief Every member the finding queue holds, as one readable value.
	///
	/// \details Declared as one aggregate, and held as the view's only data
	/// member, so that "this widget holds no provider, no endpoint, no store,
	/// and no process" is a list a test reads rather than a claim a reviewer
	/// checks by eye. A guard in `tests/adapters_qt/finding_queue_view_tests.cpp`
	/// binds every member here by name and states its type, so adding one more
	/// member — of any type — fails to compile, and the same test pins
	/// `sizeof(finding_queue_view)` to this aggregate so a member added to the
	/// class outside it fails too.
	///
	/// What is here is a dispatch callback, the delivery it was handed, a
	/// local selection, three counters, and the widgets that show them. There
	/// is nothing to promote with, nothing to write to, and nowhere to send
	/// anything: the only way out of this widget is an intent.
	struct finding_queue_view_members
	{
		/// True only while `present` is writing the accepted delivery into the
		/// controls, so presenting never looks like a reviewer selecting.
		bool Applying = false;

		finding_queue_view_statistics                      Statistics;
		finding_queue_intent_dispatch                      Dispatch;
		std::optional<application::agent_finding_delivery> Shown;
		/// Which row is detailed. Qt-local navigation, and nothing else: it
		/// selects what is read out of the delivery already presented, and no
		/// finding changes because a reviewer looked at it.
		std::optional<std::size_t> Selected;

		QListWidget* Findings    = nullptr;
		QLabel*      Severity    = nullptr;
		QLabel*      Confidence  = nullptr;
		QLabel*      Category    = nullptr;
		QLabel*      Claim       = nullptr;
		QLabel*      Evidence    = nullptr;
		QLabel*      Consequence = nullptr;
		QLabel*      AnchorState = nullptr;

		/// One action per declared enumerator, indexed by it. An array rather
		/// than six named members so a seventh action cannot be declared and
		/// then never offered.
		std::array<QAction*, finding_queue_action_count> Actions{ };
	};

	/// \brief Shows what an agent found, and asks rather than acts.
	///
	/// \details Each of the seven facts the queue discloses — severity,
	/// confidence, category, claim, evidence, consequence, and the anchor
	/// state — is a separate field on screen rather than a rendered sentence.
	/// A reviewer who has to read an anchor state out of a paragraph about a
	/// claim has not been shown either, and a single summary string would let
	/// one of them go missing without anything failing.
	///
	/// The anchor state is disclosed for one reason: a finding whose anchor
	/// the frozen snapshot does not contain is still delivered, because a
	/// reviewer who cannot see what the agent got wrong cannot correct it. The
	/// queue shows that it did not land; it does not hide it, and it does not
	/// guess a nearby line.
	///
	/// Nothing the reviewer does changes what a finding says. Every one of the
	/// six actions emits one `finding_action_intent` naming the exact finding
	/// and waits; the shown delivery moves only when `present` is called with
	/// an accepted one. Selecting a row is the single exception, and it is not
	/// an exception to the rule: it changes which finding is detailed, not
	/// what any finding claims.
	class finding_queue_view final : public QWidget
	{
	public:
		explicit finding_queue_view(
			finding_queue_intent_dispatch aDispatch = { },
			QWidget*                      aParent   = nullptr);

		void set_dispatch(finding_queue_intent_dispatch aDispatch);

		/// \brief Adopts one delivery, and promotes nothing.
		///
		/// \details The selected ordinal survives a delivery that still has a
		/// row at it, which is what keeps an accepted refresh from moving the
		/// reviewer somewhere they did not ask to be.
		void present(const application::agent_finding_delivery& aDelivery);

		/// \brief Details the finding at one ordinal, and emits nothing.
		void select(std::size_t aOrdinal);

		/// \brief [[BPR-1594]]. The delivery shown, which the Agent tab draws
		/// as cards; none before the first.
		[[nodiscard]] auto shown_delivery() const noexcept -> const application::agent_finding_delivery*
		{
			return Members.Shown ? &*Members.Shown : nullptr;
		}
		[[nodiscard]] auto selected_ordinal() const noexcept
			-> std::optional<std::size_t>;
		[[nodiscard]] auto finding_count() const noexcept -> std::size_t;

		/// \brief The queue itself: one row per delivered finding, in order.
		[[nodiscard]] auto findings_text() const -> QString;
		[[nodiscard]] auto severity_text() const -> QString;
		[[nodiscard]] auto confidence_text() const -> QString;
		[[nodiscard]] auto category_text() const -> QString;
		[[nodiscard]] auto claim_text() const -> QString;
		[[nodiscard]] auto evidence_text() const -> QString;
		[[nodiscard]] auto consequence_text() const -> QString;
		/// \brief Whether the anchor held, and when it did not, why.
		[[nodiscard]] auto anchor_state_text() const -> QString;

		[[nodiscard]] auto findings_list() const noexcept -> QListWidget*;
		[[nodiscard]] auto action(finding_queue_action aAction) const noexcept
			-> QAction*;

		[[nodiscard]] auto statistics() const noexcept
			-> finding_queue_view_statistics;

	private:
		void emit_action(finding_queue_action aAction);
		void refresh_details();
		void refresh_enablement();

		[[nodiscard]] auto selected_finding() const
			-> std::optional<queued_finding>;

		finding_queue_view_members Members;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_FINDING_QUEUE_VIEW_HPP
