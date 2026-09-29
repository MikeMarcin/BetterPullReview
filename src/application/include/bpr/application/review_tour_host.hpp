#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_TOUR_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_TOUR_HOST_HPP
#include "bpr/application/port.hpp"
#include "bpr/application/review_tour_projection.hpp"
#include "bpr/application/review_tour_store.hpp"
#include "bpr/core/review_tour_import.hpp"
#include "bpr/core/review_tour_revision.hpp"
#include <expected>
#include <functional>
#include <memory>
namespace bpr::application {
	/// \brief Exact local service request, shared by desktop and terminal presentations.
	/// Publication and assistance require their separate existing authority ports.
	enum class tour_service_operation : std::uint8_t { edit,
													   undo,
													   redo,
													   import_selected,
													   assist_complete,
													   outline_use };
	/// \brief What the active agent returned for one issued ticket, or the author's choice from a proposal.
	/// The agent supplies result text only: target, range and hook were fixed when the ticket was issued.
	struct tour_assist_completion
	{
		std::uint64_t            Id = 0;
		std::string              ResultJson;
		std::int64_t             Now                 = 0;
		std::uint64_t            CurrentHookRevision = 0;
		std::vector<std::size_t> Indices;
		std::size_t              Position = 0;
	};
	struct tour_service_request
	{
		core::tour_command           Command;
		tour_service_operation       Operation   = tour_service_operation::edit;
		std::size_t                  ImportIndex = 0;
		tour_presentation_origin     Origin      = tour_presentation_origin::desktop;
		std::string                  RequestId{ };
		std::array<std::uint8_t, 32> Digest{ };
		tour_assist_completion       Assist{ };
	};
	enum class tour_service_status : std::uint8_t { accepted,
													unchanged,
													refused,
													busy,
													storage_failure };
	/// \brief Admission result; accepted edits are durable only once SavedRevision catches up.
	struct tour_service_result
	{
		tour_service_result() = default;
		tour_service_result(tour_service_status aStatus, core::tour_refusal aRefusal = core::tour_refusal::none, std::uint64_t aRevision = 0, bool aReplayed = false)
			: Status{ aStatus }
			, Refusal{ aRefusal }
			, Revision{ aRevision }
			, Replayed{ aReplayed }
		{ }
		tour_service_status Status   = tour_service_status::busy;
		core::tour_refusal  Refusal  = core::tour_refusal::none;
		std::uint64_t       Revision = 0;
		bool                Replayed = false;
		/// Assist and outline outcomes: a short stable code, a bounded human line, the agent's notes and any proposal.
		std::string               Reason, Detail;
		std::vector<writing_note> Notes;
		std::uint64_t             Proposal = 0;
	};
	/// \brief Decision-lane owner with coalesced off-lane persistence.
	/// Store and queued work belong to this host. Shutdown drains final accepted edits;
	/// completion delivery must remain live until is_quiescent(). No remote effects exist.
	class review_tour_host final
	{
	public:
		using publication = std::function<void(const tour_presentation&)>;
		review_tour_host(core::review_tour aSeed, std::unique_ptr<review_tour_store> aStore, effect_executor& aEffects, decision_executor& aDecision, publication aPublish);
		~review_tour_host();
		void start();
		void observe_imports(std::vector<core::tour_import_candidate> aCandidates);
		void import_selected(std::size_t aIndex);
		void resolve_remote_conflict(std::uint64_t aRevision, bool aUseRemote);
		void observe_comparison(const core::diff_semantic_index&, const core::tour_code_destination&, const core::tour_code_destination&);
		void observe_source(core::tour_code_destination aSource);
		/// Accepted publication attribution is an observation, never an undoable remote action.
		void               observe_publication(core::review_tour aPublished);
		void               dispatch(const core::tour_command& aCommand);
		[[nodiscard]] auto execute(const tour_service_request& aRequest) -> tour_service_result;
		[[nodiscard]] auto request_status(std::string_view aId) const -> std::optional<tour_request_receipt>;
		/// Issue the one live ticket for this draft, superseding any earlier one; journal-only, no revision change.
		[[nodiscard]] auto assist_issue(tour_assist_ticket aTicket, std::int64_t aNow) -> std::expected<tour_assist_ticket, std::string>;
		[[nodiscard]] auto assist_ticket(std::uint64_t aId) const -> std::optional<tour_assist_ticket>;
		[[nodiscard]] auto assist_proposal(std::uint64_t aId) const -> std::optional<tour_assist_proposal>;
		/// Retire every pending ticket and proposal. Explicit closure by the author or the caller.
		void assist_cancel();
		void undo();
		void redo();
		void retry_save();
		/// Explicitly discard local edits after a conflict; the caller presents both copies first.
		void               use_saved_copy();
		void               shutdown();
		[[nodiscard]] auto presentation() const -> tour_presentation;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
}
#endif
