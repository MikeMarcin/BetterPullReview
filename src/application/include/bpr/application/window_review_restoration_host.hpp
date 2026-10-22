#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WINDOW_REVIEW_RESTORATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_WINDOW_REVIEW_RESTORATION_HOST_HPP
#include "bpr/application/port.hpp"
#include "bpr/application/window_review_restoration_ports.hpp"
#include <memory>

namespace bpr::application {
	/// Decision-lane owner. Ports/executors outlive quiescence; all storage runs on the effect lane.
	class window_review_restoration_host final
	{
	public:
		window_review_restoration_host(core::bpr_profile_id aProfile, std::vector<core::bpr_window_context> aContexts,
									   std::vector<std::string> aConfiguredHosts, window_review_restoration_port& aStore,
									   effect_executor& aEffects, decision_executor& aDecision, window_review_restoration_projection_port& aProjection);
		~window_review_restoration_host();
		window_review_restoration_host(const window_review_restoration_host&)                                  = delete;
		auto               operator=(const window_review_restoration_host&) -> window_review_restoration_host& = delete;
		void               start();
		[[nodiscard]] auto attach(core::bpr_window_context aContext) -> bool;
		void               mark_window_ready(core::bpr_window_context aContext);
		void               cancel_restore(core::bpr_window_context aContext);
		void               cancel_startup();
		[[nodiscard]] auto current_startup(core::request_id aRequest) const noexcept -> bool;
		void               complete_startup(core::request_id aRequest, std::expected<core::window_review_authority, core::window_review_failure> aResult);
		[[nodiscard]] auto current_restore(core::bpr_window_context aContext, core::request_id aRequest) const noexcept -> bool;
		void               complete_restore(core::bpr_window_context aContext, core::request_id aRequest, std::expected<void, core::window_review_failure> aResult);
		/// Composition supplies only an exact read already accepted by the window host,
		/// with its clock reading for the recently viewed list.
		void accepted_read(core::bpr_window_context aContext, const core::window_review_destination& aDestination, std::int64_t aViewedAt = 0);
		/// [[BPR-0733]]. The title of a listed destination, once a snapshot showed it.
		void accepted_draft(const core::window_review_destination& aDestination, std::optional<bool> aDraft);
		void accepted_title(const core::window_review_destination& aDestination, std::string_view aTitle);
		/// [[BPR-0733]]. A window's gesture on its recently viewed list.
		void open_recent(core::bpr_window_context aContext, std::size_t aIndex);
		/// \brief A reviewer's removal of one destination from the recently viewed list, persisted.
		void               forget_recent(const core::window_review_destination& aDestination);
		[[nodiscard]] auto recent() const noexcept -> const std::vector<core::window_recent_review>&;
		/// \brief Retains an exact accepted reading position for the current accepted destination.
		void accepted_position(core::bpr_window_context aContext, const core::window_review_destination& aDestination, const core::window_review_position& aPosition);
		/// \brief [[BPR-1131]]. Retains the window's accepted document tabs for its
		/// current accepted destination, persisted with its reading.
		void accepted_tabs(core::bpr_window_context aContext, const core::window_review_destination& aDestination, const core::window_review_tabs& aTabs);
		/// \brief RFC-BPR-0089 ([[BPR-1688]]). The window's commit and file filters.
		void               accepted_filters(core::bpr_window_context aContext, const core::window_review_destination& aDestination, const core::window_review_filters& aFilters);
		void               close(core::bpr_window_context aContext);
		[[nodiscard]] auto window_quiescent(core::bpr_window_context aContext) const noexcept -> bool;
		/// Explicit recovery reloads revision authority; preserves trusted pending records, never retries conflict blindly.
		void               retry_persistence();
		void               shutdown_orderly();
		void               shutdown();
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		[[nodiscard]] auto projection_delivery_failed() const noexcept -> bool;
		/// \brief Every accepted reading record is durably stored, with no failed flush.
		[[nodiscard]] auto durable() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
}
#endif
