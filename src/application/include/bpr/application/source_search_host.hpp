#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_SOURCE_SEARCH_HOST_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_SOURCE_SEARCH_HOST_HPP

#include "bpr/application/port.hpp"
#include "bpr/application/source_corpus_port.hpp"
#include "bpr/core/source_matcher.hpp"
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
		std::string                 Include;
		std::string                 Exclude;
	};

	/// \brief One exact source occurrence; locations retain both proven sides.
	struct source_search_match
	{
		core::file_identity       File;
		core::source_match_record Match;
	};
	/// \brief Honest per-side qualification; absent sides are not skipped material.
	struct source_search_skipped_side
	{
		core::file_identity File;
		core::diff_side     Side   = core::diff_side::left;
		diff_blob_status    Reason = diff_blob_status::failure;
	};
	/// \brief One ordered publication batch, never over 128 records.
	struct source_search_batch
	{
		core::source_search_request          Request;
		core::source_search_summary          Summary;
		std::vector<source_search_match>     Matches;
		std::shared_ptr<const source_corpus> Corpus;
		std::uint32_t                        Begin = 0U;
	};
	using source_search_batch_sink = std::function<void(source_search_batch)>;
	struct source_search_execution
	{
		core::source_search_terminal                                   Terminal;
		std::shared_ptr<const std::vector<source_search_match>>        Matches;
		std::shared_ptr<const source_corpus>                           Corpus;
		std::shared_ptr<const std::vector<source_search_skipped_side>> Skipped;
		bool                                                           RetryAvailable = false;
		std::size_t                                                    PeakOwnedBound = 0U;
		source_search_execution(core::source_search_terminal aTerminal)
			: Terminal(std::move(aTerminal))
		{ }
	};
	/// \brief Execute the shared matcher for an exact, already accepted pane material.
	/// \details This performs no reads. A PR corpus execution compiles once and
	/// calls the same core entry matcher across its exact ordered catalogue.
	[[nodiscard]] auto execute_source_pane_match(const source_search_work& aWork, const core::source_match_entry& aEntry, std::stop_token aStop, core::source_match_limits aLimits = { }) -> source_search_execution;

	/// \brief Executes one exact request off the decision lane, with advisory cancellation.
	/// \details M2 supplies the shared matcher and M3 its epoch-backed corpus.
	/// Implementations return one terminal and do not touch core state or Qt.
	class source_search_execution_port
	{
	public:
		virtual ~source_search_execution_port()                                                                               = default;
		[[nodiscard]] virtual auto execute(const source_search_work& aWork, std::stop_token aStop) -> source_search_execution = 0;
		/// \brief Optional streaming seam; the default preserves ordinary pane workers.
		[[nodiscard]] virtual auto execute(const source_search_work& aWork, std::stop_token aStop, const source_search_batch_sink& aPublish) -> source_search_execution
		{
			static_cast<void>(aPublish);
			return execute(aWork, aStop);
		}
		/// \brief Latest demand is installed before cancelling a replaced matcher.
		virtual void update_demand(std::optional<source_search_work> aWork) { static_cast<void>(aWork); }
	};

	/// \brief Runtime-only search presentation, with explicit empty/partial/error coverage.
	struct source_search_projection
	{
		std::optional<core::source_search_scope>                       Scope;
		std::string                                                    Query;
		std::string                                                    Include;
		std::string                                                    Exclude;
		core::source_search_options                                    Options;
		core::source_search_input                                      Input  = core::source_search_input::empty;
		core::source_search_status                                     Status = core::source_search_status::idle;
		core::source_search_summary                                    Summary;
		std::shared_ptr<const std::vector<source_search_match>>        Matches;
		std::optional<std::uint32_t>                                   CurrentMatch;
		std::shared_ptr<const source_corpus>                           Corpus;
		std::shared_ptr<const std::vector<source_search_skipped_side>> Skipped;
		bool                                                           RetryAvailable = false;
		core::source_search_reason                                     Reason         = core::source_search_reason::none;
		bool                                                           Open           = false;
		bool                                                           Wrapped        = false;
		bool                                                           NoResults      = false;
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
		auto set_paths(std::string_view aInclude, std::string_view aExclude) -> bool;
		auto retry() -> bool;
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
