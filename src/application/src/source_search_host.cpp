#include "bpr/application/source_search_host.hpp"

#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <utility>

namespace bpr::application {
	auto name(const source_search_worker_stage aStage) noexcept -> std::string_view
	{
		switch (aStage) {
		case source_search_worker_stage::queued:           return "queued";
		case source_search_worker_stage::effect_entered:   return "effect-entered";
		case source_search_worker_stage::executing:        return "executing";
		case source_search_worker_stage::executed:         return "executed";
		case source_search_worker_stage::validating:       return "validating";
		case source_search_worker_stage::validated:        return "validated";
		case source_search_worker_stage::observing:        return "observing";
		case source_search_worker_stage::observed:         return "observed";
		case source_search_worker_stage::posting:          return "posting";
		case source_search_worker_stage::posted:           return "posted";
		case source_search_worker_stage::post_failed:      return "post-failed";
		case source_search_worker_stage::decision_entered: return "decision-entered";
		}
		return "unknown";
	}
	namespace {
		auto path_input(std::string_view aInclude, std::string_view aExclude) -> core::source_search_input
		{
			const auto include = core::inspect_source_search_query(aInclude);
			const auto exclude = core::inspect_source_search_query(aExclude);
			if (include == core::source_search_input::invalid_utf8 || exclude == core::source_search_input::invalid_utf8) {
				return core::source_search_input::invalid_utf8;
			}
			if (aInclude.size() > core::maximum_source_search_query_bytes || aExclude.size() > core::maximum_source_search_query_bytes - aInclude.size()) {
				return core::source_search_input::input_limit;
			}
			return core::source_search_input::ready;
		}
		auto valid_match_payload(const source_search_execution& aExecution, std::stop_token aStop) -> bool
		{
			if (aExecution.Corpus && (aExecution.Corpus->Scope != aExecution.Terminal.Request.Scope || aExecution.Corpus->Entries.size() > maximum_git_changed_entries || aExecution.Corpus->OwnedBytes > maximum_git_changed_catalogue_bytes)) {
				return false;
			}
			if (aExecution.Skipped && aExecution.Skipped->size() != aExecution.Terminal.Summary.SkippedSides) {
				return false;
			}
			if (!aExecution.Matches) {
				return true;
			}
			const auto& records = *aExecution.Matches;
			if (records.size() != aExecution.Terminal.Summary.Matches || records.size() > core::maximum_source_search_matches) {
				return false;
			}
			auto        bytes   = records.capacity() * sizeof(source_search_match);
			const auto& scope   = aExecution.Terminal.Request.Scope;
			auto        entries = std::map<core::file_identity, const source_corpus_entry*>{ };
			if (aExecution.Corpus) {
				for (const auto& entry : aExecution.Corpus->Entries) {
					if (!entries.emplace(entry.File, &entry).second) {
						return false;
					}
				}
			}
			for (const auto& record : records) {
				if (aStop.stop_requested() || !record.File.valid() || record.Match.Preview.size() > 960U || core::inspect_source_search_query(record.Match.Preview) == core::source_search_input::invalid_utf8) {
					return false;
				}
				bytes += record.Match.Preview.capacity() + 1U;
				if (bytes > core::source_match_limits{ }.MaximumResultBytes) {
					return false;
				}
				if (!record.Match.Locations[0] && !record.Match.Locations[1]) {
					return false;
				}
				if (scope.Document && scope.Document->File != record.File) {
					return false;
				}
				for (auto index = std::size_t{ 0U }; index < 2U; ++index) {
					const auto& location = record.Match.Locations[index];
					if (!location) {
						continue;
					}
					if (!location->Blob.valid() || location->Span.Begin > location->Span.End || location->Span.End > core::source_match_limits{ }.MaximumBytes
						|| location->Side != (index == 0U ? core::diff_side::left : core::diff_side::right)) {
						return false;
					}
					if (aExecution.Corpus) {
						const auto found = entries.find(record.File);
						if (found == entries.end()) {
							return false;
						}
						const auto& side = index == 0U ? found->second->Left : found->second->Right;
						if (!side || side->Object != location->Blob || (side->Mode != 0100644U && side->Mode != 0100755U)) {
							return false;
						}
					}
					if (scope.Document && location->Blob != (index == 0U ? scope.Document->LeftBlob : scope.Document->RightBlob)) {
						return false;
					}
					if (scope.Selection && (location->Side != scope.Selection->Side || location->Span.Begin < scope.Selection->Span.Begin || location->Span.End > scope.Selection->Span.End)) {
						return false;
					}
				}
			}
			return true;
		}
	}

	struct source_search_host::implementation : std::enable_shared_from_this<implementation>
	{
		enum class command_kind : std::uint8_t { paths,
												 scope,
												 query,
												 open,
												 close,
												 reset,
												 next,
												 previous,
												 select };
		struct command
		{
			command_kind                             Kind = command_kind::open;
			std::optional<core::source_search_scope> Scope;
			std::string                              Query;
			core::source_search_input                Input = core::source_search_input::empty;
			core::source_search_options              Options;
			std::string                              Include;
			std::string                              Exclude;
			std::uint32_t                            Index = 0U;
		};
		struct job
		{
			source_search_work                      Work;
			std::stop_source                        Stop;
			std::mutex                              ProgressMutex;
			std::vector<source_search_match>        Progress;
			core::source_search_summary             ProgressSummary;
			std::shared_ptr<const source_corpus>    Corpus;
			std::chrono::steady_clock::time_point   LastProgress{ };
			bool                                    Posted      = false;
			bool                                    BadProgress = false;
			std::atomic<source_search_worker_stage> Stage{ source_search_worker_stage::queued };
		};

		implementation(decision_executor& aDecisions, effect_executor& aEffects, source_search_execution_port& aExecution, source_search_projection_port& aProjection)
			: Decisions(aDecisions)
			, Effects(aEffects)
			, Execution(aExecution)
			, Projection(aProjection)
		{
		}

		decision_executor&                                             Decisions;
		effect_executor&                                               Effects;
		source_search_execution_port&                                  Execution;
		source_search_projection_port&                                 Projection;
		std::shared_ptr<insights_observer>                             WorkObserver;
		core::source_search_state                                      State;
		std::string                                                    Query;
		std::string                                                    Include;
		std::string                                                    Exclude;
		std::shared_ptr<const source_corpus>                           Corpus;
		std::shared_ptr<const std::vector<source_search_skipped_side>> Skipped;
		bool                                                           RetryAvailable = false;
		std::shared_ptr<const std::vector<source_search_match>>        Matches;
		std::shared_ptr<job>                                           Active;
		std::shared_ptr<job>                                           Queued;
		std::deque<command>                                            Ingress;
		source_search_host_counters                                    Counters;
		std::array<std::atomic_size_t, 8>                              Pipeline{ };
		core::source_search_reason                                     Reason          = core::source_search_reason::none;
		bool                                                           Wrapped         = false;
		std::atomic_size_t                                             PendingProgress = 0U;
		bool                                                           Turning         = false;
		bool                                                           Closed          = false;
		bool                                                           ShutdownPending = false;

		auto projection() const -> source_search_projection
		{
			auto output           = source_search_projection{ };
			output.Scope          = State.Scope;
			output.Query          = Query;
			output.Include        = Include;
			output.Exclude        = Exclude;
			output.Corpus         = Corpus;
			output.Skipped        = Skipped;
			output.RetryAvailable = RetryAvailable;
			output.Options        = State.Options;
			output.Input          = State.Input;
			output.Status         = State.Status;
			output.Summary        = State.Summary;
			output.Generation     = State.Generation;
			output.Matches        = Matches;
			output.CurrentMatch   = State.CurrentMatch;
			output.Reason         = Reason;
			output.Open           = State.Open;
			output.Wrapped        = Wrapped;
			output.NoResults      = core::source_search_no_results(State);
			return output;
		}

		void publish()
		{
			try {
				Projection.publish(projection());
			} catch (...) {
				++Counters.ProjectionFailures;
			}
		}

		void retire_queued()
		{
			if (Queued) {
				++Counters.Terminals;
				const auto output = core::complete_source_search(State, { Queued->Work.Request, core::source_search_status::cancelled, { } });
				if (output.Reason == core::source_search_reason::stale_completion) {
					++Counters.StaleTerminals;
				}
				Queued.reset();
			}
		}

		void perform(const core::source_search_output& aOutput)
		{
			if (aOutput.Reason == core::source_search_reason::stale_completion) {
				return;
			}
			if (aOutput.Work || aOutput.Cancel || aOutput.ClearQuery) {
				auto demand = std::optional<source_search_work>{ };
				if (aOutput.Work && !Closed) {
					demand.emplace();
					demand->Request = *aOutput.Work;
					demand->Query   = Query;
					demand->Include = Include;
					demand->Exclude = Exclude;
				}
				try {
					Execution.update_demand(std::move(demand));
				} catch (...) {
					++Counters.IngressRefusals;
				}
			}
			if (aOutput.Cancel) {
				if (Active && Active->Work.Request == *aOutput.Cancel) {
					Active->Stop.request_stop();
				}
				if (Queued && Queued->Work.Request == *aOutput.Cancel) {
					retire_queued();
				}
			}
			if (aOutput.Work || aOutput.Cancel || aOutput.ClearQuery || (aOutput.Changed && State.Summary.Matches == 0U && !aOutput.Accepted)) {
				Matches.reset();
				Corpus.reset();
				Skipped.reset();
				RetryAvailable = false;
			}
			if (aOutput.ClearQuery) {
				Query.clear();
				Include.clear();
				Exclude.clear();
			}
			Reason  = aOutput.Reason;
			Wrapped = aOutput.Wrapped;
			if (aOutput.Work && !Closed) {
				retire_queued();
				auto pending          = std::make_shared<job>();
				pending->Work.Request = *aOutput.Work;
				pending->Work.Query   = Query;
				pending->Work.Include = Include;
				pending->Work.Exclude = Exclude;
				Queued                = std::move(pending);
				++Counters.Admitted;
			}
			if (aOutput.Changed || aOutput.Reason != core::source_search_reason::none) {
				publish();
			}
		}

		void apply(command aCommand)
		{
			switch (aCommand.Kind) {
			case command_kind::paths:
				if (aCommand.Input == core::source_search_input::ready || aCommand.Input == core::source_search_input::empty) {
					Include = std::move(aCommand.Include);
					Exclude = std::move(aCommand.Exclude);
					perform(core::edit_source_search_query(State, core::inspect_source_search_query(Query), State.Options));
				} else {
					perform(core::edit_source_search_query(State, aCommand.Input, State.Options));
				}
				break;
			case command_kind::scope:
				perform(core::set_source_search_scope(State, aCommand.Scope));
				break;
			case command_kind::query:
				if (aCommand.Input == core::source_search_input::ready || aCommand.Input == core::source_search_input::empty) {
					Query = std::move(aCommand.Query);
				}
				perform(core::edit_source_search_query(State, aCommand.Input, aCommand.Options));
				break;
			case command_kind::open:     perform(core::open_source_search(State)); break;
			case command_kind::close:    perform(core::close_source_search(State)); break;
			case command_kind::reset:    perform(core::reset_source_search(State)); break;
			case command_kind::next:     perform(core::navigate_source_search(State, core::change_navigation_direction::next)); break;
			case command_kind::previous: perform(core::navigate_source_search(State, core::change_navigation_direction::previous)); break;
			case command_kind::select:   perform(core::select_source_search_match(State, aCommand.Index)); break;
			}
		}

		void complete(const std::shared_ptr<job>& aJob, source_search_execution aExecution)
		{
			// The effect executor retains each admitted closure exactly once.
			// Completion remains on the decision lane until Active is retired.
			Turning         = true;
			auto& aTerminal = aExecution.Terminal;
			++Counters.Terminals;
			if (aTerminal.Request != aJob->Work.Request) {
				aTerminal = { aJob->Work.Request, core::source_search_status::failure, { } };
				aExecution.Matches.reset();
			}
			if (aExecution.Matches && (aExecution.Matches->size() != aTerminal.Summary.Matches || aExecution.Matches->size() > core::maximum_source_search_matches)) {
				aTerminal = { aJob->Work.Request, core::source_search_status::failure, { } };
				aExecution.Matches.reset();
			}
			const auto output = core::complete_source_search(State, aTerminal);
			if (output.Accepted) {
				Matches        = std::move(aExecution.Matches);
				Corpus         = std::move(aExecution.Corpus);
				Skipped        = std::move(aExecution.Skipped);
				RetryAvailable = aExecution.RetryAvailable;
			}
			if (output.Reason == core::source_search_reason::stale_completion) {
				++Counters.StaleTerminals;
			}
			Active.reset();
			if (!State.Pending) {
				try {
					Execution.update_demand({ });
				} catch (...) {
					++Counters.IngressRefusals;
				}
			}
			perform(output);
			Turning = false;
			drain();
		}

		void deliver_progress(const std::shared_ptr<job>& aJob)
		{
			--PendingProgress;
			auto summary = core::source_search_summary{ };
			auto records = std::shared_ptr<const std::vector<source_search_match>>{ };
			auto corpus  = std::shared_ptr<const source_corpus>{ };
			{
				std::scoped_lock lock{ aJob->ProgressMutex };
				aJob->Posted = false;
				if (aJob->BadProgress) {
					return;
				}
				summary = aJob->ProgressSummary;
				records = std::make_shared<const std::vector<source_search_match>>(aJob->Progress);
				corpus  = aJob->Corpus;
			}
			Turning           = true;
			const auto output = core::progress_source_search(State, aJob->Work.Request, summary);
			if (output.Accepted) {
				Matches = std::move(records);
				Corpus  = std::move(corpus);
			}
			perform(output);
			Turning = false;
			drain();
		}
		void receive_batch(const std::shared_ptr<job>& aJob, source_search_batch aBatch)
		{
			if (aJob->Stop.stop_requested()) {
				return;
			}
			auto post = false;
			{
				std::scoped_lock lock{ aJob->ProgressMutex };
				if (aJob->BadProgress) {
					return;
				}
				if (aBatch.Request != aJob->Work.Request || aBatch.Matches.size() > 128U || aBatch.Begin != aJob->Progress.size()
					|| aBatch.Summary.Matches != aBatch.Begin + aBatch.Matches.size() || aBatch.Summary.Matches > core::maximum_source_search_matches
					|| !aBatch.Corpus || aBatch.Corpus->Scope != aBatch.Request.Scope) {
					aJob->BadProgress = true;
					return;
				}
				auto progress    = core::source_search_state{ };
				progress.Pending = aJob->Work.Request;
				progress.Summary = aJob->ProgressSummary;
				if (!core::progress_source_search(progress, aBatch.Request, aBatch.Summary).Accepted) {
					aJob->BadProgress = true;
					return;
				}
				auto packet                     = source_search_execution{ core::source_search_terminal{ aBatch.Request, core::source_search_status::partial, aBatch.Summary } };
				packet.Terminal.Summary.Matches = static_cast<std::uint32_t>(aBatch.Matches.size());
				packet.Matches                  = std::make_shared<const std::vector<source_search_match>>(std::move(aBatch.Matches));
				packet.Corpus                   = aBatch.Corpus;
				if (!valid_match_payload(packet, aJob->Stop.get_token())) {
					aJob->BadProgress = true;
					return;
				}
				aJob->Progress.insert(aJob->Progress.end(), packet.Matches->begin(), packet.Matches->end());
				aJob->ProgressSummary = aBatch.Summary;
				aJob->Corpus          = std::move(aBatch.Corpus);
				const auto now        = std::chrono::steady_clock::now();
				if (!aJob->Posted && now - aJob->LastProgress >= std::chrono::milliseconds{ 100 }) {
					aJob->Posted       = true;
					aJob->LastProgress = now;
					post               = true;
				}
			}
			if (post) {
				const auto self = shared_from_this();
				++PendingProgress;
				try {
					Decisions.post([self, aJob] { self->deliver_progress(aJob); });
				} catch (...) {
					--PendingProgress;
					throw;
				}
			}
		}

		void start_queued()
		{
			if (Closed || Active || !Queued) {
				return;
			}
			Active             = std::exchange(Queued, { });
			const auto current = Active;
			const auto host    = shared_from_this();
			try {
				const auto observer = WorkObserver;
				Effects.post([host, current, observer] {
					host->Pipeline[0].fetch_add(1U, std::memory_order_relaxed);
					current->Stage.store(source_search_worker_stage::effect_entered, std::memory_order_relaxed);
					const auto started  = std::chrono::steady_clock::now();
					auto       terminal = source_search_execution{ core::source_search_terminal{ current->Work.Request, core::source_search_status::cancelled, { } } };
					bool       executed = false;
					if (!current->Stop.stop_requested()) {
						try {
							executed = true;
							current->Stage.store(source_search_worker_stage::executing, std::memory_order_relaxed);
							terminal = host->Execution.execute(current->Work, current->Stop.get_token(), [host, current](source_search_batch aBatch) { host->receive_batch(current, std::move(aBatch)); });
							host->Pipeline[1].fetch_add(1U, std::memory_order_relaxed);
							current->Stage.store(source_search_worker_stage::executed, std::memory_order_relaxed);
							{
								std::scoped_lock lock{ current->ProgressMutex };
								if (current->BadProgress) {
									terminal = core::source_search_terminal{ current->Work.Request, core::source_search_status::failure, { } };
								}
							}
							current->Stage.store(source_search_worker_stage::validating, std::memory_order_relaxed);
							if (!valid_match_payload(terminal, current->Stop.get_token())) {
								terminal = source_search_execution{ core::source_search_terminal{ current->Work.Request, current->Stop.stop_requested() ? core::source_search_status::cancelled : core::source_search_status::failure, { } } };
							}
							host->Pipeline[2].fetch_add(1U, std::memory_order_relaxed);
							current->Stage.store(source_search_worker_stage::validated, std::memory_order_relaxed);
						} catch (...) {
							terminal = source_search_execution{ core::source_search_terminal{ current->Work.Request, core::source_search_status::failure, { } } };
						}
					}
					if (observer && executed) {
						current->Stage.store(source_search_worker_stage::observing, std::memory_order_relaxed);
						core::insights_work_sample sample;
						sample.Kind                 = core::insights_work_kind::search;
						sample.DurationMicroseconds = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - started).count());
						observer->record_work(sample);
					}
					host->Pipeline[3].fetch_add(1U, std::memory_order_relaxed);
					current->Stage.store(source_search_worker_stage::observed, std::memory_order_relaxed);
					host->Pipeline[4].fetch_add(1U, std::memory_order_relaxed);
					current->Stage.store(source_search_worker_stage::posting, std::memory_order_relaxed);
					try {
						host->Decisions.post([host, current, completion = std::move(terminal)]() mutable {
							host->Pipeline[7].fetch_add(1U, std::memory_order_relaxed);
							current->Stage.store(source_search_worker_stage::decision_entered, std::memory_order_relaxed);
							host->complete(current, std::move(completion));
						});
						host->Pipeline[5].fetch_add(1U, std::memory_order_relaxed);
						auto expected = source_search_worker_stage::posting;
						static_cast<void>(current->Stage.compare_exchange_strong(expected, source_search_worker_stage::posted, std::memory_order_relaxed));
					} catch (...) {
						host->Pipeline[6].fetch_add(1U, std::memory_order_relaxed);
						current->Stage.store(source_search_worker_stage::post_failed, std::memory_order_relaxed);
						throw;
					}
				});
				++Counters.Scheduled;
			} catch (...) {
				// A refused post retained no closure. Its admitted job still gets
				// one terminal through the required live decision service.
				Decisions.post([host, current] { host->complete(current, core::source_search_terminal{ current->Work.Request, core::source_search_status::failure, { } }); });
			}
		}

		void drain()
		{
			if (Turning) {
				return;
			}
			Turning = true;
			while (ShutdownPending || !Ingress.empty()) {
				if (ShutdownPending) {
					ShutdownPending = false;
					Ingress.clear();
					perform(core::reset_source_search(State));
					retire_queued();
				} else {
					auto next = std::move(Ingress.front());
					Ingress.pop_front();
					apply(std::move(next));
				}
			}
			start_queued();
			Turning = false;
		}

		auto enqueue(command aCommand) -> bool
		{
			if (Closed || Ingress.size() == 64U) {
				++Counters.IngressRefusals;
				return false;
			}
			Ingress.push_back(std::move(aCommand));
			drain();
			return true;
		}

		void shutdown()
		{
			if (Closed) {
				return;
			}
			Closed          = true;
			ShutdownPending = true;
			if (Active) {
				Active->Stop.request_stop();
			}
			drain();
		}
	};

	void source_search_host::observe_work(std::shared_ptr<insights_observer> aObserver) { Implementation->WorkObserver = std::move(aObserver); }

	source_search_host::source_search_host(decision_executor& aDecisions, effect_executor& aEffects, source_search_execution_port& aExecution, source_search_projection_port& aProjection)
		: Implementation(std::make_shared<implementation>(aDecisions, aEffects, aExecution, aProjection))
	{
	}

	source_search_host::~source_search_host() { Implementation->shutdown(); }

	auto source_search_host::set_scope(std::optional<core::source_search_scope> aScope) -> bool
	{
		auto command  = implementation::command{ };
		command.Kind  = implementation::command_kind::scope;
		command.Scope = std::move(aScope);
		return Implementation->enqueue(std::move(command));
	}

	auto source_search_host::set_query(const std::string_view aQuery, const core::source_search_options aOptions) -> bool
	{
		auto command    = implementation::command{ };
		command.Kind    = implementation::command_kind::query;
		command.Input   = core::inspect_source_search_query(aQuery);
		command.Options = aOptions;
		if (command.Input == core::source_search_input::ready) {
			command.Query = aQuery;
		}
		return Implementation->enqueue(std::move(command));
	}

	auto source_search_host::set_paths(std::string_view aInclude, std::string_view aExclude) -> bool
	{
		auto command  = implementation::command{ };
		command.Kind  = implementation::command_kind::paths;
		command.Input = path_input(aInclude, aExclude);
		if (command.Input == core::source_search_input::ready) {
			command.Include = aInclude;
			command.Exclude = aExclude;
		}
		return Implementation->enqueue(std::move(command));
	}
	auto source_search_host::retry() -> bool { return set_query(Implementation->Query, Implementation->State.Options); }

	auto source_search_host::open() -> bool { return Implementation->enqueue({ implementation::command_kind::open, { }, { }, core::source_search_input::empty, { }, { }, { } }); }
	auto source_search_host::close() -> bool { return Implementation->enqueue({ implementation::command_kind::close, { }, { }, core::source_search_input::empty, { }, { }, { } }); }
	auto source_search_host::clear() -> bool { return set_query({ }, Implementation->State.Options); }
	auto source_search_host::reset() -> bool { return Implementation->enqueue({ implementation::command_kind::reset, { }, { }, core::source_search_input::empty, { }, { }, { } }); }
	auto source_search_host::navigate(const core::change_navigation_direction aDirection) -> bool
	{
		return Implementation->enqueue({ aDirection == core::change_navigation_direction::next ? implementation::command_kind::next : implementation::command_kind::previous, { }, { }, core::source_search_input::empty, { }, { }, { } });
	}
	auto source_search_host::select_match(const std::uint32_t aIndex) -> bool
	{
		auto command  = implementation::command{ };
		command.Kind  = implementation::command_kind::select;
		command.Index = aIndex;
		return Implementation->enqueue(std::move(command));
	}
	void source_search_host::shutdown() { Implementation->shutdown(); }
	auto source_search_host::projection() const -> source_search_projection { return Implementation->projection(); }
	auto source_search_host::state() const noexcept -> const core::source_search_state& { return Implementation->State; }
	auto source_search_host::counters() const noexcept -> source_search_host_counters
	{
		auto result = Implementation->Counters;
		for (auto index = std::size_t{ 0U }; index < result.Pipeline.size(); ++index) {
			result.Pipeline[index] = Implementation->Pipeline[index].load(std::memory_order_relaxed);
		}
		if (Implementation->Active) {
			result.ActiveGeneration = Implementation->Active->Work.Request.Generation;
			result.ActiveStage      = Implementation->Active->Stage.load(std::memory_order_relaxed);
			result.ActiveStopped    = Implementation->Active->Stop.stop_requested();
		}
		if (Implementation->Queued) {
			result.QueuedGeneration = Implementation->Queued->Work.Request.Generation;
		}
		return result;
	}
	auto source_search_host::retained_work_packets() const noexcept -> std::size_t { return (Implementation->Active ? 1U : 0U) + (Implementation->Queued ? 1U : 0U); }
	auto source_search_host::is_quiescent() const noexcept -> bool
	{
		return !Implementation->Active && !Implementation->Queued && Implementation->Ingress.empty() && !Implementation->Turning && !Implementation->ShutdownPending && Implementation->PendingProgress.load() == 0U;
	}
}
