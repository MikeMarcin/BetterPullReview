#include "bpr/application/source_search_host.hpp"

#include <deque>
#include <functional>
#include <utility>

namespace bpr::application {
	namespace {
		auto valid_match_payload(const source_search_execution& aExecution, std::stop_token aStop) -> bool
		{
			if (!aExecution.Matches) {
				return true;
			}
			const auto& records = *aExecution.Matches;
			if (records.size() != aExecution.Terminal.Summary.Matches || records.size() > core::maximum_source_search_matches) {
				return false;
			}
			auto        bytes = records.capacity() * sizeof(source_search_match);
			const auto& scope = aExecution.Terminal.Request.Scope;
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
		enum class command_kind : std::uint8_t { scope,
												 query,
												 open,
												 close,
												 reset,
												 next,
												 previous };
		struct command
		{
			command_kind                             Kind = command_kind::open;
			std::optional<core::source_search_scope> Scope;
			std::string                              Query;
			core::source_search_input                Input = core::source_search_input::empty;
			core::source_search_options              Options;
		};
		struct job
		{
			source_search_work Work;
			std::stop_source   Stop;
		};

		implementation(decision_executor& aDecisions, effect_executor& aEffects, source_search_execution_port& aExecution, source_search_projection_port& aProjection)
			: Decisions(aDecisions)
			, Effects(aEffects)
			, Execution(aExecution)
			, Projection(aProjection)
		{
		}

		decision_executor&                                      Decisions;
		effect_executor&                                        Effects;
		source_search_execution_port&                           Execution;
		source_search_projection_port&                          Projection;
		core::source_search_state                               State;
		std::string                                             Query;
		std::shared_ptr<const std::vector<source_search_match>> Matches;
		std::shared_ptr<job>                                    Active;
		std::shared_ptr<job>                                    Queued;
		std::deque<command>                                     Ingress;
		source_search_host_counters                             Counters;
		core::source_search_reason                              Reason          = core::source_search_reason::none;
		bool                                                    Wrapped         = false;
		bool                                                    Turning         = false;
		bool                                                    Closed          = false;
		bool                                                    ShutdownPending = false;

		auto projection() const -> source_search_projection
		{
			auto output         = source_search_projection{ };
			output.Scope        = State.Scope;
			output.Query        = Query;
			output.Options      = State.Options;
			output.Input        = State.Input;
			output.Status       = State.Status;
			output.Summary      = State.Summary;
			output.Matches      = Matches;
			output.CurrentMatch = State.CurrentMatch;
			output.Reason       = Reason;
			output.Open         = State.Open;
			output.Wrapped      = Wrapped;
			output.NoResults    = core::source_search_no_results(State);
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
			}
			if (aOutput.ClearQuery) {
				Query.clear();
			}
			Reason  = aOutput.Reason;
			Wrapped = aOutput.Wrapped;
			if (aOutput.Work && !Closed) {
				retire_queued();
				auto pending          = std::make_shared<job>();
				pending->Work.Request = *aOutput.Work;
				pending->Work.Query   = Query;
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
				Matches = std::move(aExecution.Matches);
			}
			if (output.Reason == core::source_search_reason::stale_completion) {
				++Counters.StaleTerminals;
			}
			Active.reset();
			perform(output);
			Turning = false;
			drain();
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
				Effects.post([host, current] {
					auto terminal = source_search_execution{ core::source_search_terminal{ current->Work.Request, core::source_search_status::cancelled, { } } };
					if (!current->Stop.stop_requested()) {
						try {
							terminal = host->Execution.execute(current->Work, current->Stop.get_token());
							if (!valid_match_payload(terminal, current->Stop.get_token())) {
								terminal = source_search_execution{ core::source_search_terminal{ current->Work.Request, current->Stop.stop_requested() ? core::source_search_status::cancelled : core::source_search_status::failure, { } } };
							}
						} catch (...) {
							terminal = source_search_execution{ core::source_search_terminal{ current->Work.Request, core::source_search_status::failure, { } } };
						}
					}
					host->Decisions.post([host, current, completion = std::move(terminal)]() mutable { host->complete(current, std::move(completion)); });
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

	auto source_search_host::open() -> bool { return Implementation->enqueue({ implementation::command_kind::open, { }, { }, core::source_search_input::empty, { } }); }
	auto source_search_host::close() -> bool { return Implementation->enqueue({ implementation::command_kind::close, { }, { }, core::source_search_input::empty, { } }); }
	auto source_search_host::clear() -> bool { return set_query({ }, Implementation->State.Options); }
	auto source_search_host::reset() -> bool { return Implementation->enqueue({ implementation::command_kind::reset, { }, { }, core::source_search_input::empty, { } }); }
	auto source_search_host::navigate(const core::change_navigation_direction aDirection) -> bool
	{
		return Implementation->enqueue({ aDirection == core::change_navigation_direction::next ? implementation::command_kind::next : implementation::command_kind::previous, { }, { }, core::source_search_input::empty, { } });
	}
	void source_search_host::shutdown() { Implementation->shutdown(); }
	auto source_search_host::projection() const -> source_search_projection { return Implementation->projection(); }
	auto source_search_host::state() const noexcept -> const core::source_search_state& { return Implementation->State; }
	auto source_search_host::counters() const noexcept -> source_search_host_counters { return Implementation->Counters; }
	auto source_search_host::retained_work_packets() const noexcept -> std::size_t { return (Implementation->Active ? 1U : 0U) + (Implementation->Queued ? 1U : 0U); }
	auto source_search_host::is_quiescent() const noexcept -> bool
	{
		return !Implementation->Active && !Implementation->Queued && Implementation->Ingress.empty() && !Implementation->Turning && !Implementation->ShutdownPending;
	}
}
