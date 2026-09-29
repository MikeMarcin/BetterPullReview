#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_FINDING_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_FINDING_HOST_HPP

#include "bpr/application/agent_finding_delivery.hpp"
#include "bpr/application/review_ledger_port.hpp"
#include "bpr/core/agent_finding.hpp"
#include "bpr/core/agent_finding_promotion.hpp"
#include "bpr/core/agent_session.hpp"
#include "bpr/core/defect_workflow.hpp"
#include "bpr/core/review_round_trip.hpp"

#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>

namespace bpr::application {
	/// \brief The six things a reviewer may ask about one delivered finding.
	///
	/// \details The same six `qt::finding_queue_action` offers, restated here
	/// because the host may not depend on the adapter that presents them. They
	/// are kept in step by `application_agent_finding_composition_contract`,
	/// which sweeps both counts against each other rather than trusting that
	/// two lists written apart stayed together.
	///
	/// Three of them ask for a promotion and `core::promote_agent_finding`
	/// decides every one; the other three produce no artifact at all, which is
	/// why `not_a_promotion` is a refusal here rather than a silent success.
	enum class agent_finding_action : std::uint8_t {
		/// Show the code the finding claims to be about. Changes nothing.
		open_evidence,
		/// Ask the agent about this finding. An explicit chat turn, elsewhere.
		ask_follow_up,
		/// Promote it onto the local draft the request names.
		convert_to_draft_comment,
		/// Promote it onto the local defect the request names.
		convert_to_draft_defect,
		/// Keep it as the reviewer's own durable private note.
		add_to_private_notes,
		/// Rule it out. It produces no artifact and is not a promotion.
		dismiss,
	};

	/// \brief The number of declared finding actions.
	inline constexpr std::size_t agent_finding_action_count = 6U;

	static_assert(
		static_cast<std::size_t>(agent_finding_action::dismiss) + 1U
			== agent_finding_action_count,
		"agent_finding_action_count must cover every declared action");

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(agent_finding_action aAction) noexcept
		-> std::string_view;

	/// \brief Asks for one thing to be done about one exact delivered finding.
	///
	/// \details **There is no verdict here.** The verdict belongs to the
	/// delivery the host is holding, and a request that carried one could
	/// assert that an anchor held when the core had already decided it did
	/// not. The finding is named by the session it arrived in and its position
	/// in that delivery, which is the only identity `core::agent_finding` has.
	///
	/// `Draft` and `Defect` are the local artifacts the reviewer is promoting
	/// *onto*; `core::promote_agent_finding` refuses when the named one is
	/// anchored elsewhere. `Note` and `Attachment` name where a private note
	/// would be written, because allocating an identity is the caller's job
	/// and not a policy this host should own.
	struct agent_finding_action_request
	{
		core::agent_session_id Session;
		/// Its position in the delivery, counted from zero.
		std::size_t          Ordinal = 0U;
		agent_finding_action Action  = agent_finding_action::open_evidence;
		/// Whether the reviewer has already ruled this finding out.
		core::agent_finding_standing Standing
			= core::agent_finding_standing::delivered;
		std::optional<core::review_comment_draft> Draft;
		std::optional<core::defect_record>        Defect;
		local_note_id                             Note;
		std::optional<local_note_attachment>      Attachment;

		auto operator<=>(const agent_finding_action_request&) const = default;
	};

	/// \brief Why an action produced nothing, before promotion was ever asked.
	///
	/// \details Separate from `core::agent_finding_promotion_refusal` on
	/// purpose. These are reasons the host could not put a promotion question
	/// to the core at all; that enumeration is the core's answer once it was
	/// asked. Folding them together would let "this host is holding no
	/// delivery" read as "the anchor did not hold".
	enum class agent_finding_action_refusal : std::uint8_t {
		/// Not a refusal.
		none,
		/// No delivery is being held, so there is no finding to act on.
		nothing_delivered,
		/// A delivery is held, but for another session.
		other_session,
		/// The delivery has no finding at that ordinal.
		unknown_finding,
		/// The action produces no artifact, so no promotion was attempted.
		/// `open_evidence`, `ask_follow_up`, and `dismiss` all arrive here.
		not_a_promotion,
		/// The core produced a note and the durable store refused to keep it,
		/// or no durable store is open for the review at all. Both are the
		/// same fact to a reviewer: the note they asked for does not exist.
		note_not_stored,
	};

	/// \brief The number of declared action refusals.
	inline constexpr std::size_t agent_finding_action_refusal_count = 6U;

	static_assert(
		static_cast<std::size_t>(agent_finding_action_refusal::note_not_stored)
				+ 1U
			== agent_finding_action_refusal_count,
		"agent_finding_action_refusal_count must cover every declared refusal");

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(agent_finding_action_refusal aRefusal) noexcept
		-> std::string_view;

	/// \brief What one asked-for action produced, including nothing and why.
	///
	/// \details **There is no field here a `core::review_publication_proposal`
	/// could occupy**, and none a `review_mutation_target` could either. The
	/// three artifacts are the three local ones `core::agent_finding_promotion`
	/// already names, carried outward unchanged.
	///
	/// A refused promotion is a result, not an absence: `Promotion` carries the
	/// core's own reason, `Finding` carries what the reviewer asked about, and
	/// the projection is told about it exactly as it is told about a success.
	/// A queue that showed nothing after a refused conversion would leave the
	/// reviewer believing a finding had landed on a draft it never reached.
	struct agent_finding_outcome
	{
		core::agent_session_id Session;
		std::size_t            Ordinal = 0U;
		agent_finding_action   Action  = agent_finding_action::open_evidence;
		/// Monotonic identity of this explicit action within the host lifetime.
		/// Re-presenting the same outcome retains this value; asking again gets
		/// the next value even when it names the same finding and action.
		std::uint64_t                ActionGeneration = 0U;
		agent_finding_action_refusal Refusal
			= agent_finding_action_refusal::nothing_delivered;
		/// The core's own answer, and only meaningful once it was asked.
		core::agent_finding_promotion_refusal Promotion
			= core::agent_finding_promotion_refusal::none;
		/// What the reviewer asked about, so a refusal can name it on screen.
		std::optional<agent_delivered_finding>      Subject;
		std::optional<core::review_draft_command>   Draft;
		std::optional<core::defect_command>         Defect;
		std::optional<local_note_record>            Note;
		std::optional<agent_note_promotion_failure> NoteFailure;

		/// \brief Whether exactly one local artifact was produced.
		[[nodiscard]] constexpr auto promoted() const noexcept -> bool
		{
			return Refusal == agent_finding_action_refusal::none
				&& Promotion == core::agent_finding_promotion_refusal::none
				&& (static_cast<std::size_t>(Draft.has_value())
					+ static_cast<std::size_t>(Defect.has_value())
					+ static_cast<std::size_t>(Note.has_value()))
				== 1U;
		}

		auto operator<=>(const agent_finding_outcome&) const = default;
	};

	/// \brief Records directly observable finding-host activity.
	struct agent_finding_host_statistics
	{
		std::uint64_t Deliveries        = 0U;
		std::uint64_t DeliveredFindings = 0U;
		std::uint64_t Actions           = 0U;
		std::uint64_t DraftCommands     = 0U;
		std::uint64_t DefectCommands    = 0U;
		std::uint64_t StoredNotes       = 0U;
		std::uint64_t Dismissals        = 0U;
		/// Every outcome that produced no artifact, refused by this host or by
		/// the core. Counted because "nothing happened" and "the reviewer was
		/// told why nothing happened" are different claims.
		std::uint64_t Refusals = 0U;
		/// Every outcome handed to the projection, refusal included. Equal to
		/// `Actions`, and asserted so, because an outcome that was computed
		/// and not presented is one the reviewer never saw.
		std::uint64_t PresentedOutcomes = 0U;

		auto operator<=>(const agent_finding_host_statistics&) const = default;
	};

	/// \brief Everything the finding host shows, and nothing it decided alone.
	class agent_finding_projection_port
	{
	public:
		agent_finding_projection_port() = default;
		agent_finding_projection_port(const agent_finding_projection_port&)
			= delete;
		auto operator=(const agent_finding_projection_port&)
			-> agent_finding_projection_port&                          = delete;
		agent_finding_projection_port(agent_finding_projection_port&&) = delete;
		auto operator=(agent_finding_projection_port&&)
			-> agent_finding_projection_port&    = delete;
		virtual ~agent_finding_projection_port() = default;

		/// \brief Adopts one run's validated findings, verdicts included.
		virtual void present(const agent_finding_delivery& aDelivery) = 0;
		/// \brief Adopts what one asked-for action produced, or why it did not.
		virtual void present(const agent_finding_outcome& aOutcome) = 0;
		/// \brief Reports that no delivery remains presentable.
		virtual void clear() = 0;
	};

	/// \brief Holds one run's findings and turns a reviewer's ask into one
	/// local artifact, or into a refusal the reviewer can read.
	///
	/// \details Two ports and nothing else: the promoted-note store, which
	/// writes only the note `promoted_note` built, and the projection. There
	/// is **no mutation port, no provider, no coordinator, and no publication
	/// path** reachable from here, which is how RFC-BPR-0012's rule that agent
	/// output can never publish survives composition rather than being restated
	/// in it.
	///
	/// The delivery is held so the verdict is read rather than supplied. That
	/// is the one piece of state this host keeps, and it is what makes an
	/// action request unable to assert an anchor the core refused.
	class agent_finding_host final
	{
	public:
		/// \brief Stores one promoted note in the review open right now.
		///
		/// \details Asked at call time rather than captured, because the ledger
		/// is opened and retired as the reviewer opens pull requests. A host
		/// that captured one would write a promoted note into the review
		/// nobody is looking at. [[BPR-1211]]. The note is written off the
		/// decision lane: `false` means no durable ledger is open, which is a
		/// complete answer and produces `note_not_stored`; otherwise the
		/// answer arrives later, once, on the decision lane.
		using promoted_note_store = std::function<bool(
			const local_note_record&,
			std::function<void(std::expected<local_note_record, local_note_failure>)>)>;

		agent_finding_host(
			promoted_note_store            aStore,
			agent_finding_projection_port& aProjection);

		agent_finding_host(const agent_finding_host&)                    = delete;
		auto operator=(const agent_finding_host&) -> agent_finding_host& = delete;
		agent_finding_host(agent_finding_host&&)                         = delete;
		auto operator=(agent_finding_host&&) -> agent_finding_host&      = delete;
		~agent_finding_host()                                            = default;

		/// \brief Adopts one delivery and presents it. Promotes nothing.
		void deliver(agent_finding_delivery aDelivery);

		/// \brief Forgets the held delivery, and tells the projection so.
		void clear();

		/// \brief Answers one asked-for action, and presents the answer.
		/// \details A note is presented once its durable write answers.
		void act(const agent_finding_action_request& aRequest);

		[[nodiscard]] auto delivered() const noexcept
			-> const std::optional<agent_finding_delivery>&;
		[[nodiscard]] auto statistics() const noexcept
			-> agent_finding_host_statistics;

	private:
		[[nodiscard]] auto locate(const agent_finding_action_request& aRequest)
			const -> std::optional<agent_delivered_finding>;

		void finish(const agent_finding_outcome& aOutcome);

		promoted_note_store                   Store;
		agent_finding_projection_port&        Projection;
		std::optional<agent_finding_delivery> Delivered;
		agent_finding_host_statistics         Statistics;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_FINDING_HOST_HPP
