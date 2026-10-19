#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_SYNTAX_CLASSIFICATION_APPLICATION_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_SYNTAX_CLASSIFICATION_APPLICATION_HOST_HPP
#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/application/insights_observer.hpp"
#include "bpr/application/port.hpp"
namespace bpr::application {
	/// \brief Owns one window's syntax capability, request lifecycle and bounded immutable cache.
	/// \details Decision-lane operations only. Referenced ports/executors remain alive until
	/// shutdown and is_quiescent; theme preferences never enter this owner.
	class syntax_classification_application_host final
	{
	public:
		syntax_classification_application_host(syntax_classification_port&, effect_executor&, decision_executor&,
											   appearance_projection_port&, core::application_session_id, appearance_host_limits = { });
		~syntax_classification_application_host();
		syntax_classification_application_host(const syntax_classification_application_host&)                                  = delete;
		auto               operator=(const syntax_classification_application_host&) -> syntax_classification_application_host& = delete;
		void               classify(token_span_cache_key aKey);
		void               cancel();
		void               shutdown();
		[[nodiscard]] auto cached(const token_span_cache_key&) const -> shared_token_spans;
		[[nodiscard]] auto cached_classifications() const noexcept -> std::size_t;
		[[nodiscard]] auto cache_hits() const noexcept -> std::size_t;
		[[nodiscard]] auto classification_submissions() const noexcept -> std::size_t;
		[[nodiscard]] auto coalesced_classifications() const noexcept -> std::size_t;
		[[nodiscard]] auto classification_failures() const noexcept -> std::size_t;
		[[nodiscard]] auto classification_terminals() const noexcept -> std::size_t;
		[[nodiscard]] auto cache_evictions() const noexcept -> std::size_t;
		[[nodiscard]] auto outstanding_requests() const noexcept -> std::size_t;
		[[nodiscard]] auto rejected_terminals() const noexcept -> std::size_t;
		[[nodiscard]] auto last_rejection() const noexcept -> appearance_terminal_rejection;
		[[nodiscard]] auto is_closing() const noexcept -> bool;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;
		[[nodiscard]] auto projection_delivery_failed() const noexcept -> bool;

		/// \brief Attaches a passive profile observer; existing work and admission are unchanged.
		void observe_work(std::shared_ptr<insights_observer> aObserver);

	private:
		struct implementation;
		std::unique_ptr<implementation> Implementation;
	};
}
#endif
