#include "bpr/application/agent_program_resolution.hpp"

#include <algorithm>
#include <array>
#include <utility>

namespace bpr::application {
	namespace {
		/// The shipped profiles name their program with no folder; a test
		/// profile, or anything else that names its program by path, is left
		/// as it is.
		[[nodiscard]] auto bare_program(const std::string_view aExecutable) noexcept -> bool
		{
			return !aExecutable.empty() && aExecutable.find_first_of("/\\") == std::string_view::npos;
		}

		[[nodiscard]] auto refused(const agent_launch_request& aRequest, std::string aMessage) -> agent_run_result
		{
			return { .Session = aRequest.Session, .Outcome = agent_run_outcome::failed_to_start, .StandardError = std::move(aMessage), .Run = aRequest.Run, .Metadata = aRequest.Metadata };
		}

		/// [[BPR-2259]]. Takes this app's `--settings` pair out of a launch.
		void drop_writing_settings(std::vector<std::string>& aArguments)
		{
			const auto pair = std::ranges::search(aArguments, std::array<std::string_view, 2>{ "--settings", claude_writing_settings });
			aArguments.erase(pair.begin(), pair.end());
		}
	} // namespace

	auto describe_program_refusal(const core::setup_agent aAgent, const std::string_view aPath, const core::agent_wrapper_refusal aRefusal) -> std::string
	{
		const auto* const owner = aAgent == core::setup_agent::codex ? "Codex program " : "Plain-text harness program ";
		return owner + std::string{ aPath } + ": " + std::string{ core::describe(aRefusal) };
	}

	void agent_program_source::set(const core::setup_agent aAgent, std::string aPath)
	{
		const std::scoped_lock lock{ Mutex };
		if (aAgent == core::setup_agent::codex) {
			Codex = std::move(aPath);
		} else if (aAgent == core::setup_agent::plain_text_harness) {
			PlainText = std::move(aPath);
		}
	}

	auto agent_program_source::current(const core::setup_agent aAgent) const -> std::string
	{
		const std::scoped_lock lock{ Mutex };
		if (aAgent == core::setup_agent::codex) {
			return Codex;
		}
		if (aAgent == core::setup_agent::plain_text_harness) {
			return PlainText;
		}
		return { };
	}

	located_agent_process::located_agent_process(agent_process_port& aInner, std::shared_ptr<const agent_program_source> aSource, agent_wrapper_inspector aInspect,
												 agent_program_locator aLocate)
		: Inner{ aInner }
		, Source{ std::move(aSource) }
		, Inspect{ std::move(aInspect) }
		, Locate{ std::move(aLocate) }
	{ }

	auto located_agent_process::run(const agent_launch_request& aRequest, const int aTimeoutMilliseconds, const agent_output_sink& aSink, std::stop_token aStop)
		-> agent_run_result
	{
		const auto agent = core::setup_agent_of(aRequest.Protocol);
		if (agent == core::setup_agent::claude_code || !bare_program(aRequest.Executable)) {
			return Inner.run(aRequest, aTimeoutMilliseconds, aSink, std::move(aStop));
		}
		auto saved = Source ? Source->current(agent) : std::string{ };
		if (!saved.empty()) {
			// Inspected again at every launch: a program removed or no longer
			// runnable since it was saved ends the request and starts nothing.
			auto refusal = std::optional{ core::agent_wrapper_refusal::missing };
			try {
				if (Inspect) {
					refusal = Inspect(saved);
				}
			} catch (...) { }
			if (refusal) {
				return refused(aRequest, describe_program_refusal(agent, saved, *refusal));
			}
			auto located       = aRequest;
			located.Executable = std::move(saved);
			return Inner.run(located, aTimeoutMilliseconds, aSink, std::move(aStop));
		}
		if (agent == core::setup_agent::plain_text_harness) {
			return refused(aRequest, std::string{ plain_text_harness_unset_message });
		}
		// Looked for again at every launch: a `codex` removed or replaced since
		// setup found it is not the one that runs.
		auto found = std::optional<core::agent_search_candidate>{ };
		try {
			if (Locate) {
				found = Locate(core::agent_program::codex);
			}
		} catch (...) { }
		if (!found) {
			return refused(aRequest, std::string{ codex_not_found_message });
		}
		auto located       = aRequest;
		located.Executable = std::move(found->Path);
		return Inner.run(located, aTimeoutMilliseconds, aSink, std::move(aStop));
	}

	void located_agent_process::cancel() noexcept { Inner.cancel(); }

	void agent_entry_source::set(std::vector<core::agent_entry> aEntries)
	{
		const std::scoped_lock lock{ Mutex };
		Entries = std::move(aEntries);
	}

	auto agent_entry_source::current() const -> std::vector<core::agent_entry>
	{
		const std::scoped_lock lock{ Mutex };
		return Entries;
	}

	agent_entry_process::agent_entry_process(agent_process_port& aDirect, agent_process_port& aOthers, std::shared_ptr<const agent_entry_source> aSource,
											 agent_wrapper_inspector aInspect, claude_locator aClaude, agent_program_locator aLocate)
		: Direct{ aDirect }
		, Others{ aOthers }
		, Source{ std::move(aSource) }
		, Inspect{ std::move(aInspect) }
		, Claude{ std::move(aClaude) }
		, Locate{ std::move(aLocate) }
	{ }

	auto agent_entry_process::run(const agent_launch_request& aRequest, const int aTimeoutMilliseconds, const agent_output_sink& aSink, std::stop_token aStop)
		-> agent_run_result
	{
		if (!core::is_agent_entry_profile(aRequest.Profile)) {
			return Others.run(aRequest, aTimeoutMilliseconds, aSink, std::move(aStop));
		}
		const auto  entries = Source ? Source->current() : std::vector<core::agent_entry>{ };
		const auto* entry   = core::entry_of_profile(entries, aRequest.Profile);
		if (entry == nullptr || entry->Protocol != aRequest.Protocol) {
			return refused(aRequest, std::string{ agent_entry_removed_message });
		}
		auto program = entry->Program;
		if (program.empty()) {
			// A popular agent's own program, looked for again at every launch.
			try {
				if (entry->Preset == core::setup_agent::claude_code && Claude) {
					if (auto found = Claude()) {
						program = std::move(found->Path);
					}
				} else if (entry->Preset == core::setup_agent::codex && Locate) {
					if (auto found = Locate(core::agent_program::codex)) {
						program = std::move(found->Path);
					}
				}
			} catch (...) { }
			if (program.empty()) {
				return refused(aRequest, entry->Preset == core::setup_agent::claude_code ? std::string{ claude_not_found_message } : std::string{ codex_not_found_message });
			}
		} else {
			auto refusal = std::optional{ core::agent_wrapper_refusal::missing };
			try {
				if (Inspect) {
					refusal = Inspect(program);
				}
			} catch (...) { }
			if (refusal) {
				return refused(aRequest, entry->Name + " program " + program + ": " + std::string{ core::describe(*refusal) });
			}
		}
		auto launched       = aRequest;
		launched.Executable = std::move(program);
		// [[BPR-2259]] (owner, 2026-11-18: Enhance said Claude isn't signed in
		// although their custom agent had just drafted the comment). Claude
		// keeps only the last `--settings` it is given, and a custom agent's
		// sign-in can ride in its own: in its arguments, which come first, or
		// in a wrapper that puts its own before the ones it passes on. This
		// app's writing settings then replaced it, and the run got a 401. A
		// custom agent's writing and read-only runs keep its own settings, as
		// its Agent tab runs do; the tool and MCP checks still read Claude's
		// init frame, and auto-memory stays off through the environment.
		if (!entry->Preset && launched.Protocol == core::agent_protocol::claude_stream_json) {
			drop_writing_settings(launched.Arguments);
		}
		launched.Arguments.insert(launched.Arguments.begin(), entry->Arguments.begin(), entry->Arguments.end());
		return Direct.run(launched, aTimeoutMilliseconds, aSink, std::move(aStop));
	}

	void agent_entry_process::cancel() noexcept { Others.cancel(); }
} // namespace bpr::application
