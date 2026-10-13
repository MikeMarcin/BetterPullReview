#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_SOURCE_SEARCH_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_SOURCE_SEARCH_HOST_HPP

#include "bpr/application/port.hpp"
#include "bpr/core/source_search.hpp"

#include <memory>
#include <stop_token>
#include <string>
#include <string_view>

namespace bpr::application {

	/// \brief Immutable off-lane work; query text never enters interaction state.
	struct source_search_work
	{
		core::source_search_request Request;
		std::string                 Query;
	};

	/// \brief Executes one exact request off the decision lane, with advisory cancellation.
	/// \details M2 supplies the shared matcher and M3 its epoch-backed corpus.
	/// Implementations return one terminal and do not touch core state or Qt.
	class source_search_execution_port
	{
	public:
		virtual ~source_search_execution_port()                                                                                    = default;
		[[nodiscard]] virtual auto execute(const source_search_work& aWork, std::stop_token aStop) -> core::source_search_terminal = 0;
	};

	/// \brief Runtime-only search presentation, with explicit empty/partial/error coverage.
	struct source_search_projection
	{
		std::optional<core::source_search_scope> Scope;
		std::string                              Query;
		core::source_search_options              Options;
		core::source_search_input                Input  = core::source_search_input::empty;
		core::source_search_status               Status = core::source_search_status::idle;
		core::source_search_summary              Summary;
		std::optional<std::uint32_t>             CurrentMatch;
		core::source_search_reason               Reason    = core::source_search_reason::none;
		bool                                     Open      = false;
		bool                                     Wrapped   = false;
		bool                                     NoResults = false;
	};

	class source_search_projection_port
	{
	public:
		virtual ~source_search_projection_port()                          = default;
		virtual void publish(const source_search_projection& aProjection) = 0;
	};

	/// \brief Observable lifecycle counters; no query or source content is logged.
	struct source_search_host_counters
	{
		std::size_t Admitted           = 0U;
		std::size_t Scheduled          = 0U;
		std::size_t Terminals          = 0U;
		std::size_t StaleTerminals     = 0U;
		std::size_t ProjectionFailures = 0U;
		std::size_t IngressRefusals    = 0U;
	};

	/// \brief One independent pane or PR search session, bounded to one effect and one latest replacement.
	/// \details All public operations and inspection run on the decision lane.
	/// Reentrant projection feedback is deferred with a 64-operation ceiling.
	/// Query inputs are admitted before copying, at 4 KiB. Superseded queued work
	/// gets its cancelled terminal; an executing effect owes its terminal even
	/// after clear/close/shutdown. Ports and executors must outlive is_quiescent().
	/// No persistence, cache, pasteboard or source-read capability is added here.
	class source_search_host final
	{
	public:
		source_search_host(decision_executor& aDecisions, effect_executor& aEffects, source_search_execution_port& aExecution, source_search_projection_port& aProjection);
		~source_search_host();
		source_search_host(const source_search_host&)                    = delete;
		auto operator=(const source_search_host&) -> source_search_host& = delete;

		/// \brief Changes exact scope; a same-authority change retains the query.
		auto set_scope(std::optional<core::source_search_scope> aScope) -> bool;
		/// \brief Opens/edits the query; invalid input clears hits and submits no effect.
		auto set_query(std::string_view aQuery, core::source_search_options aOptions = { }) -> bool;
		auto open() -> bool;
		auto close() -> bool;
		auto clear() -> bool;
		auto reset() -> bool;
		auto navigate(core::change_navigation_direction aDirection) -> bool;
		/// \brief Closes ingress immediately, requests stop and drains every admitted terminal.
		void               shutdown();
		[[nodiscard]] auto projection() const -> source_search_projection;
		[[nodiscard]] auto state() const noexcept -> const core::source_search_state&;
		[[nodiscard]] auto counters() const noexcept -> source_search_host_counters;
		[[nodiscard]] auto retained_work_packets() const noexcept -> std::size_t;
		[[nodiscard]] auto is_quiescent() const noexcept -> bool;

	private:
		struct implementation;
		std::shared_ptr<implementation> Implementation;
	};
}

#endif
