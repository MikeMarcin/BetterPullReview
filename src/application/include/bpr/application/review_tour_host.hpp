#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_TOUR_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_TOUR_HOST_HPP
#include "bpr/application/port.hpp"
#include "bpr/application/review_tour_projection.hpp"
#include "bpr/application/review_tour_store.hpp"
#include "bpr/core/review_tour_import.hpp"
#include "bpr/core/review_tour_revision.hpp"
#include <functional>
#include <memory>
namespace bpr::application {
	/// \brief Exact local service request, shared by desktop and terminal presentations.
	/// Publication and assistance require their separate existing authority ports.
	enum class tour_service_operation : std::uint8_t { edit,
													   undo,
													   redo,
													   import_selected };
	struct tour_service_request
	{
		core::tour_command     Command;
		tour_service_operation Operation   = tour_service_operation::edit;
		std::size_t            ImportIndex = 0;
	};
	enum class tour_service_status : std::uint8_t { accepted,
													unchanged,
													refused,
													busy,
													storage_failure };
	/// \brief Admission result; accepted edits are durable only once SavedRevision catches up.
	struct tour_service_result
	{
		tour_service_status Status   = tour_service_status::busy;
		core::tour_refusal  Refusal  = core::tour_refusal::none;
		std::uint64_t       Revision = 0;
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
		void               undo();
		void               redo();
		void               retry_save();
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
