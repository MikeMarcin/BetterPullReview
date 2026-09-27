#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_TOUR_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_TOUR_HOST_HPP
#include "bpr/application/port.hpp"
#include "bpr/application/review_tour_projection.hpp"
#include "bpr/application/review_tour_store.hpp"
#include "bpr/core/review_tour_import.hpp"
#include <functional>
#include <memory>
namespace bpr::application {
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
		void observe_source(core::tour_code_destination aSource);
		/// Accepted publication attribution is an observation, never an undoable remote action.
		void observe_publication(core::review_tour aPublished);
		void dispatch(const core::tour_command& aCommand);
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
