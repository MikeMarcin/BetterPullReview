#include "bpr/core/local_draft_check.hpp"

#include "bpr/core/utf8.hpp"

#include <glaze/glaze.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <ranges>
#include <utility>

namespace bpr::core::check_wire {
	struct step
	{
		std::string Command;
		std::string Shows;
	};
	struct plan
	{
		std::vector<step> Steps;
		bool              Scratch = false;
		bool              Builds  = false;
		std::string       NotCovered;
		std::uint64_t     Candidate = 0;
		std::string       State;
	};
	struct command
	{
		std::string        Command;
		std::optional<int> Exit;
		bool               OutOfPlan = false;
	};
	struct reference
	{
		std::string   Path;
		std::uint32_t Line = 0;
	};
	struct report
	{
		std::string              Outcome;
		std::uint64_t            Candidate = 0;
		std::string              Summary;
		std::vector<command>     Commands;
		std::vector<std::string> Output;
		std::vector<reference>   References;
		std::string              NotCovered;
		std::vector<std::string> Changed;
		plan                     Plan;
	};
	struct check
	{
		std::optional<plan>   Plan;
		std::optional<report> Report;
	};

	// What the agent writes: its own keys, read leniently.
	struct asked_step
	{
		std::string Command;
		std::string Shows;
	};
	struct asked_plan
	{
		std::vector<asked_step>  Commands;
		std::vector<std::string> Needs;
		std::string              NotCovered;
	};
	struct asked_command
	{
		std::string        Command;
		std::optional<int> Exit;
	};
	struct asked_report
	{
		std::string                Outcome;
		std::string                Summary;
		std::vector<asked_command> Commands;
		std::vector<std::string>   Output;
		std::vector<reference>     References;
		std::string                NotCovered;
	};
} // namespace bpr::core::check_wire

namespace cw = bpr::core::check_wire;

template <>
struct glz::meta<cw::step>
{
	using value_type            = cw::step;
	static constexpr auto value = object("command", &value_type::Command, "shows", &value_type::Shows);
};
template <>
struct glz::meta<cw::plan>
{
	using value_type            = cw::plan;
	static constexpr auto value = object("steps", &value_type::Steps, "scratch", &value_type::Scratch, "builds", &value_type::Builds, "not_covered", &value_type::NotCovered, "candidate",
										 &value_type::Candidate, "state", &value_type::State);
};
template <>
struct glz::meta<cw::command>
{
	using value_type            = cw::command;
	static constexpr auto value = object("command", &value_type::Command, "exit", &value_type::Exit, "out_of_plan", &value_type::OutOfPlan);
};
template <>
struct glz::meta<cw::reference>
{
	using value_type            = cw::reference;
	static constexpr auto value = object("path", &value_type::Path, "line", &value_type::Line);
};
template <>
struct glz::meta<cw::report>
{
	using value_type            = cw::report;
	static constexpr auto value = object("outcome", &value_type::Outcome, "candidate", &value_type::Candidate, "summary", &value_type::Summary, "commands", &value_type::Commands, "output",
										 &value_type::Output, "references", &value_type::References, "not_covered", &value_type::NotCovered, "changed", &value_type::Changed, "plan",
										 &value_type::Plan);
};
template <>
struct glz::meta<cw::check>
{
	using value_type            = cw::check;
	static constexpr auto value = object("plan", &value_type::Plan, "report", &value_type::Report);
};
template <>
struct glz::meta<cw::asked_step>
{
	using value_type            = cw::asked_step;
	static constexpr auto value = object("command", &value_type::Command, "shows", &value_type::Shows);
};
template <>
struct glz::meta<cw::asked_plan>
{
	using value_type            = cw::asked_plan;
	static constexpr auto value = object("commands", &value_type::Commands, "needs", &value_type::Needs, "not_covered", &value_type::NotCovered);
};
template <>
struct glz::meta<cw::asked_command>
{
	using value_type            = cw::asked_command;
	static constexpr auto value = object("command", &value_type::Command, "exit", &value_type::Exit);
};
template <>
struct glz::meta<cw::asked_report>
{
	using value_type            = cw::asked_report;
	static constexpr auto value = object("outcome", &value_type::Outcome, "summary", &value_type::Summary, "commands", &value_type::Commands, "output", &value_type::Output, "references",
										 &value_type::References, "not_covered", &value_type::NotCovered);
};

namespace bpr::core {
	namespace {
		constexpr auto all_states   = std::array{ local_check_state::planned, local_check_state::discarded, local_check_state::ran };
		constexpr auto all_outcomes = std::array{ local_check_outcome::passed, local_check_outcome::failed, local_check_outcome::couldnt_tell, local_check_outcome::didnt_run,
												  local_check_outcome::stopped };
		constexpr auto lenient      = glz::opts{ .error_on_unknown_keys = false };
		constexpr auto strict       = glz::opts{ .error_on_unknown_keys = true };

		template <typename TEnum, std::size_t TCount>
		auto enum_from(const std::string_view aName, const std::array<TEnum, TCount>& aAll) -> std::optional<TEnum>
		{
			const auto found = std::ranges::find_if(aAll, [aName](const TEnum aValue) { return name(aValue) == aName; });
			return found == aAll.end() ? std::nullopt : std::optional{ *found };
		}

		auto trimmed(const std::string_view aText) -> std::string_view
		{
			const auto first = aText.find_first_not_of(" \t\r\n");
			if (first == std::string_view::npos) {
				return { };
			}
			const auto last = aText.find_last_not_of(" \t\r\n");
			return aText.substr(first, last - first + 1U);
		}

		/// Valid UTF-8 within `aBytes`, cut at a character boundary.
		auto bounded(const std::string_view aText, const std::size_t aBytes) -> std::string
		{
			auto kept = std::string{ aText.substr(0U, std::min(aText.size(), aBytes)) };
			while (!kept.empty() && !utf8::valid(kept)) {
				kept.pop_back();
			}
			return kept;
		}

		auto line_ok(const std::string_view aText, const std::size_t aBytes) -> bool
		{
			return aText.size() <= aBytes && utf8::valid(aText) && aText.find('\n') == std::string_view::npos;
		}

		/// The body of the one fenced block named `aName`, and the answer
		/// without it; none when there is no closed block.
		struct fenced
		{
			std::string_view Body;
			std::string      Rest;
		};
		auto take_block(const std::string_view aText, const std::string_view aName) -> std::optional<fenced>
		{
			const auto open = aText.find("```" + std::string{ aName });
			if (open == std::string_view::npos) {
				return std::nullopt;
			}
			const auto body  = aText.find('\n', open);
			const auto close = body == std::string_view::npos ? std::string_view::npos : aText.find("```", body + 1U);
			if (close == std::string_view::npos) {
				return std::nullopt;
			}
			auto rest = std::string{ aText.substr(0U, open) } + std::string{ aText.substr(close + 3U) };
			return fenced{ .Body = aText.substr(body + 1U, close - body - 1U), .Rest = std::string{ trimmed(rest) } };
		}

		auto to_wire(const local_check_plan& aPlan) -> cw::plan
		{
			auto out = cw::plan{ .Scratch = aPlan.NeedsScratch, .Builds = aPlan.NeedsBuilds, .NotCovered = aPlan.NotCovered, .Candidate = aPlan.Candidate, .State = std::string{ name(aPlan.State) } };
			for (const auto& step : aPlan.Steps) {
				out.Steps.push_back({ .Command = step.Command, .Shows = step.Shows });
			}
			return out;
		}

		auto from_wire(const cw::plan& aPlan) -> std::optional<local_check_plan>
		{
			const auto state = enum_from(aPlan.State, all_states);
			if (!state) {
				return std::nullopt;
			}
			auto out = local_check_plan{ .NeedsScratch = aPlan.Scratch, .NeedsBuilds = aPlan.Builds, .NotCovered = aPlan.NotCovered, .Candidate = aPlan.Candidate, .State = *state };
			for (const auto& step : aPlan.Steps) {
				out.Steps.push_back({ .Command = step.Command, .Shows = step.Shows });
			}
			return out;
		}

		auto plan_valid(const local_check_plan& aPlan) -> bool
		{
			return aPlan.Steps.size() <= maximum_check_commands && aPlan.Candidate != 0U && aPlan.NotCovered.size() <= maximum_check_prose_bytes && utf8::valid(aPlan.NotCovered)
				&& std::ranges::all_of(aPlan.Steps, [](const local_check_step& aStep) {
					   return !aStep.Command.empty() && line_ok(aStep.Command, maximum_check_command_bytes) && line_ok(aStep.Shows, maximum_check_command_bytes);
				   });
		}

		auto same_command(const std::string_view aLeft, const std::string_view aRight) -> bool { return trimmed(aLeft) == trimmed(aRight); }

		/// Commands the report names, then those only the activity listed.
		auto merge_commands(const std::vector<cw::asked_command>& aReported, const std::vector<std::string>& aObserved, const local_check_plan& aPlan) -> std::vector<local_check_command>
		{
			auto out = std::vector<local_check_command>{ };
			auto add = [&](const std::string_view aCommand, const std::optional<int> aExit) {
				const auto command = bounded(trimmed(aCommand), maximum_check_command_bytes);
				if (command.empty() || out.size() >= maximum_check_commands || std::ranges::any_of(out, [&](const local_check_command& aKept) { return same_command(aKept.Command, command); })) {
					return;
				}
				const auto planned = std::ranges::any_of(aPlan.Steps, [&](const local_check_step& aStep) { return same_command(aStep.Command, command); });
				auto       single  = command;
				std::ranges::replace(single, '\n', ' ');
				out.push_back({ .Command = std::move(single), .Exit = aExit, .OutOfPlan = !planned });
			};
			for (const auto& reported : aReported) {
				add(reported.Command, reported.Exit);
			}
			for (const auto& observed : aObserved) {
				add(observed, std::nullopt);
			}
			return out;
		}

		auto fence_for(const std::vector<std::string>& aLines) -> std::string
		{
			auto fence = std::string{ "```" };
			while (std::ranges::any_of(aLines, [&fence](const std::string& aLine) { return aLine.find(fence) != std::string::npos; })) {
				fence += '`';
			}
			return fence;
		}

		auto code_span(const std::string_view aText) -> std::string
		{
			auto ticks = std::string{ "`" };
			while (aText.find(ticks) != std::string_view::npos) {
				ticks += '`';
			}
			const auto pad = !aText.empty() && (aText.front() == '`' || aText.back() == '`') ? " " : "";
			return ticks + pad + std::string{ aText } + pad + ticks;
		}

		void append_lines(std::string& aText, const std::span<const std::string> aLines, const std::string_view aBullet)
		{
			for (const auto& line : aLines) {
				aText += aBullet;
				aText += line;
				aText += '\n';
			}
		}
	} // namespace

	auto name(const local_check_state aState) noexcept -> std::string_view
	{
		switch (aState) {
		case local_check_state::planned:   return "planned";
		case local_check_state::discarded: return "discarded";
		case local_check_state::ran:       return "ran";
		}
		return "planned";
	}

	auto name(const local_check_outcome aOutcome) noexcept -> std::string_view
	{
		switch (aOutcome) {
		case local_check_outcome::passed:       return "passed";
		case local_check_outcome::failed:       return "failed";
		case local_check_outcome::couldnt_tell: return "couldnt_tell";
		case local_check_outcome::didnt_run:    return "didnt_run";
		case local_check_outcome::stopped:      return "stopped";
		}
		return "couldnt_tell";
	}

	auto outcome_words(const local_check_outcome aOutcome) noexcept -> std::string_view
	{
		switch (aOutcome) {
		case local_check_outcome::passed:       return "Passed";
		case local_check_outcome::failed:       return "Failed";
		case local_check_outcome::couldnt_tell: return "Couldn't tell";
		case local_check_outcome::didnt_run:    return "Didn't run";
		case local_check_outcome::stopped:      return "Stopped";
		}
		return "Couldn't tell";
	}

	auto name(const local_check_block aBlock) noexcept -> std::string_view
	{
		switch (aBlock) {
		case local_check_block::none:         return "none";
		case local_check_block::discarded:    return "discarded";
		case local_check_block::ran:          return "ran";
		case local_check_block::no_candidate: return "no_candidate";
		case local_check_block::running:      return "running";
		case local_check_block::scratch_off:  return "scratch_off";
		case local_check_block::builds_off:   return "builds_off";
		}
		return "none";
	}

	auto valid(const local_check& aCheck) -> bool
	{
		if (aCheck.Plan.has_value() == aCheck.Report.has_value()) {
			return false;
		}
		if (aCheck.Plan) {
			return plan_valid(*aCheck.Plan);
		}
		const auto& report = *aCheck.Report;
		return report.Candidate != 0U && plan_valid(report.Plan) && report.Commands.size() <= maximum_check_commands && report.Output.size() <= maximum_check_output_lines
			&& report.References.size() <= maximum_check_references && report.Changed.size() <= maximum_check_changed_files && report.Summary.size() <= maximum_check_prose_bytes
			&& utf8::valid(report.Summary) && report.NotCovered.size() <= maximum_check_prose_bytes && utf8::valid(report.NotCovered)
			&& std::ranges::all_of(report.Commands, [](const local_check_command& aCommand) { return !aCommand.Command.empty() && line_ok(aCommand.Command, maximum_check_command_bytes); })
			&& std::ranges::all_of(report.Output, [](const std::string& aLine) { return line_ok(aLine, maximum_check_line_bytes); })
			&& std::ranges::all_of(report.References, [](const local_check_reference& aReference) { return !aReference.Path.empty() && line_ok(aReference.Path, maximum_check_line_bytes); })
			&& std::ranges::all_of(report.Changed, [](const std::string& aPath) { return !aPath.empty() && line_ok(aPath, maximum_check_line_bytes); });
	}

	auto encode_local_check(const local_check& aCheck) -> std::string
	{
		auto wire = cw::check{ };
		if (aCheck.Plan) {
			wire.Plan = to_wire(*aCheck.Plan);
		}
		if (aCheck.Report) {
			const auto& report = *aCheck.Report;
			auto        out    = cw::report{ .Outcome    = std::string{ name(report.Outcome) },
											 .Candidate  = report.Candidate,
											 .Summary    = report.Summary,
											 .Output     = report.Output,
											 .NotCovered = report.NotCovered,
											 .Changed    = report.Changed,
											 .Plan       = to_wire(report.Plan) };
			for (const auto& command : report.Commands) {
				out.Commands.push_back({ .Command = command.Command, .Exit = command.Exit, .OutOfPlan = command.OutOfPlan });
			}
			for (const auto& reference : report.References) {
				out.References.push_back({ .Path = reference.Path, .Line = reference.Line });
			}
			wire.Report = std::move(out);
		}
		auto text = std::string{ };
		static_cast<void>(glz::write_json(wire, text));
		return text;
	}

	auto decode_local_check(const std::string_view aText) -> std::optional<local_check>
	{
		auto wire = cw::check{ };
		if (glz::read<strict>(wire, aText)) {
			return std::nullopt;
		}
		auto out = local_check{ };
		if (wire.Plan) {
			out.Plan = from_wire(*wire.Plan);
			if (!out.Plan) {
				return std::nullopt;
			}
		}
		if (wire.Report) {
			const auto outcome = enum_from(wire.Report->Outcome, all_outcomes);
			auto       plan    = from_wire(wire.Report->Plan);
			if (!outcome || !plan) {
				return std::nullopt;
			}
			auto report = local_check_report{ .Outcome    = *outcome,
											  .Candidate  = wire.Report->Candidate,
											  .Summary    = wire.Report->Summary,
											  .Output     = wire.Report->Output,
											  .NotCovered = wire.Report->NotCovered,
											  .Changed    = wire.Report->Changed,
											  .Plan       = std::move(*plan) };
			for (const auto& command : wire.Report->Commands) {
				report.Commands.push_back({ .Command = command.Command, .Exit = command.Exit, .OutOfPlan = command.OutOfPlan });
			}
			for (const auto& reference : wire.Report->References) {
				report.References.push_back({ .Path = reference.Path, .Line = reference.Line });
			}
			out.Report = std::move(report);
		}
		if (!valid(out)) {
			return std::nullopt;
		}
		return out;
	}

	auto grants_for(const std::span<const local_check_grants> aGrants, const std::string_view aAgent) -> local_check_grants
	{
		const auto found = std::ranges::find(aGrants, aAgent, &local_check_grants::Agent);
		return found == aGrants.end() ? local_check_grants{ .Agent = std::string{ aAgent } } : *found;
	}

	auto decide_check_run(const local_check_plan& aPlan, const local_check_run_facts& aFacts) -> local_check_block
	{
		if (aPlan.State == local_check_state::discarded) {
			return local_check_block::discarded;
		}
		if (aPlan.State == local_check_state::ran) {
			return local_check_block::ran;
		}
		if (!aFacts.Candidate) {
			return local_check_block::no_candidate;
		}
		if (aFacts.Running) {
			return local_check_block::running;
		}
		if (aPlan.NeedsScratch && !aFacts.Grants.Scratch) {
			return local_check_block::scratch_off;
		}
		if (aPlan.NeedsBuilds && !aFacts.Grants.Builds) {
			return local_check_block::builds_off;
		}
		return local_check_block::none;
	}

	auto check_block_words(const local_check_block aBlock) -> std::string
	{
		switch (aBlock) {
		case local_check_block::none:         return { };
		case local_check_block::discarded:    return "Discarded.";
		case local_check_block::ran:          return "Ran. Its report follows.";
		case local_check_block::no_candidate: return "This draft has no candidate to check. Refresh to capture one.";
		case local_check_block::running:      return "Another check is running in this draft. Run this one when it ends.";
		case local_check_block::scratch_off:  return "This plan needs Use scratch folder. Turn it on to run it.";
		case local_check_block::builds_off:   return "This plan needs Allow build and run tests. Turn it on to run it.";
		}
		return { };
	}

	auto compose_check_plan_request(const std::string_view aTitle, const std::uint64_t aCandidate, const std::span<const std::string> aPaths, const std::string_view aWords,
									const std::string_view aInstructions, const std::string_view aVersion) -> local_check_request
	{
		auto out         = local_check_request{ };
		out.Instructions = std::string{ aInstructions } + "\n\n(" + std::string{ local_check_purpose } + ", " + std::string{ aVersion } + ")\n\n"
			+ "Answer with a short paragraph, then exactly one fenced block named bpr-check-plan holding JSON:\n"
			  "```bpr-check-plan\n"
			  "{\"commands\":[{\"command\":\"the exact shell command\",\"shows\":\"one line on what it shows\"}],"
			  "\"needs\":[\"scratch\",\"builds\"],\"not_covered\":\"what this plan will not cover\"}\n"
			  "```\n"
			  "List the commands in the order they run. In needs, name scratch when a command writes a script in your scratch folder, and builds when a command builds or runs this "
			  "change's code or tests. Run nothing now: this is a plan. Read only.";
		out.Text = "Plan a check of the local draft \"" + std::string{ aTitle } + "\", candidate " + std::to_string(aCandidate)
			+ ". The working folder is a throwaway copy of exactly that candidate: its committed head and the uncommitted files the author included.\n\n";
		const auto words = trimmed(aWords);
		out.Text += words.empty() ? std::string{ "The author gave no words: choose the checks that matter most for this change." } : "The author asks: " + std::string{ words };
		out.Text += "\n\nChanged in this candidate:\n";
		append_lines(out.Text, aPaths, "- ");
		return out;
	}

	auto compose_check_run_request(const std::string_view aTitle, const std::uint64_t aCandidate, const std::span<const std::string> aPaths, const local_check_plan& aPlan,
								   const std::string_view aInstructions, const std::string_view aVersion) -> local_check_request
	{
		auto out         = local_check_request{ };
		out.Instructions = std::string{ aInstructions } + "\n\n(" + std::string{ local_check_purpose } + ", " + std::string{ aVersion } + ")\n\n"
			+ "Run the plan below in the working folder, in order, and nothing else unless a command cannot run without it. Do not change files Git tracks. Then answer with a short paragraph and "
			  "exactly one fenced block named bpr-check-report holding JSON:\n"
			  "```bpr-check-report\n"
			  "{\"outcome\":\"passed|failed|couldnt_tell|didnt_run\",\"summary\":\"what happened, in a sentence or two\","
			  "\"commands\":[{\"command\":\"the command as run\",\"exit\":0}],\"output\":[\"the output lines that explain a failure\"],"
			  "\"references\":[{\"path\":\"path/in/the/change\",\"line\":12}],\"not_covered\":\"what the check did not cover\"}\n"
			  "```";
		out.Text = "Run the check you planned on the local draft \"" + std::string{ aTitle } + "\", candidate " + std::to_string(aCandidate)
			+ ". The working folder is a throwaway copy of exactly that candidate.\n\nThe plan:\n";
		for (std::size_t index = 0U; index < aPlan.Steps.size(); ++index) {
			out.Text += std::to_string(index + 1U) + ". `" + aPlan.Steps[index].Command + "`: " + aPlan.Steps[index].Shows + '\n';
		}
		if (!aPlan.NotCovered.empty()) {
			out.Text += "\nNot covered: " + aPlan.NotCovered + '\n';
		}
		out.Text += "\nChanged in this candidate:\n";
		append_lines(out.Text, aPaths, "- ");
		return out;
	}

	auto check_plan_question(const std::string_view aWords) -> std::string
	{
		const auto words = trimmed(aWords);
		return words.empty() ? std::string{ "Plan a check (you choose what to check)." } : "Plan a check: " + std::string{ words };
	}

	auto check_run_question(const std::uint64_t aCandidate, const bool aAgain) -> std::string
	{
		return (aAgain ? "Run the same plan again on candidate " : "Run the plan on candidate ") + std::to_string(aCandidate) + ".";
	}

	auto read_check_plan(const std::string_view aAnswer, const std::uint64_t aCandidate) -> local_check_plan_answer
	{
		auto       out   = local_check_plan_answer{ .Prose = std::string{ trimmed(aAnswer) } };
		const auto block = take_block(aAnswer, "bpr-check-plan");
		if (!block) {
			return out;
		}
		auto asked = cw::asked_plan{ };
		if (glz::read<lenient>(asked, block->Body) || asked.Commands.empty()) {
			return out;
		}
		auto plan = local_check_plan{ .NotCovered = bounded(trimmed(asked.NotCovered), maximum_check_prose_bytes), .Candidate = aCandidate };
		for (const auto& step : asked.Commands | std::views::take(maximum_check_commands)) {
			auto command = bounded(trimmed(step.Command), maximum_check_command_bytes);
			auto shows   = bounded(trimmed(step.Shows), maximum_check_command_bytes);
			std::ranges::replace(command, '\n', ' ');
			std::ranges::replace(shows, '\n', ' ');
			if (!command.empty()) {
				plan.Steps.push_back({ .Command = std::move(command), .Shows = std::move(shows) });
			}
		}
		plan.NeedsScratch = std::ranges::find(asked.Needs, "scratch") != asked.Needs.end();
		plan.NeedsBuilds  = std::ranges::find(asked.Needs, "builds") != asked.Needs.end();
		if (plan.Steps.empty()) {
			return out;
		}
		out.Prose = block->Rest;
		out.Plan  = std::move(plan);
		return out;
	}

	auto read_check_report(const std::string_view aAnswer, const local_check_plan& aPlan, const std::uint64_t aCandidate, const local_check_observation& aObserved) -> local_check_report_answer
	{
		auto out             = local_check_report_answer{ .Prose = std::string{ trimmed(aAnswer) } };
		auto plan            = aPlan;
		plan.State           = local_check_state::ran;
		out.Report.Plan      = std::move(plan);
		out.Report.Candidate = aCandidate;
		for (const auto& path : aObserved.Changed | std::views::take(maximum_check_changed_files)) {
			out.Report.Changed.push_back(bounded(path, maximum_check_line_bytes));
		}
		auto       asked  = cw::asked_report{ };
		const auto block  = take_block(aAnswer, "bpr-check-report");
		const auto read   = block && !glz::read<lenient>(asked, block->Body);
		const auto stated = read ? enum_from(asked.Outcome, all_outcomes) : std::nullopt;
		if (read) {
			out.Prose             = block->Rest;
			out.Report.Summary    = bounded(trimmed(asked.Summary), maximum_check_prose_bytes);
			out.Report.NotCovered = bounded(trimmed(asked.NotCovered), maximum_check_prose_bytes);
			for (const auto& line : asked.Output | std::views::take(maximum_check_output_lines)) {
				auto kept = bounded(line, maximum_check_line_bytes);
				std::ranges::replace(kept, '\n', ' ');
				out.Report.Output.push_back(std::move(kept));
			}
			for (const auto& reference : asked.References | std::views::take(maximum_check_references)) {
				auto path = bounded(trimmed(reference.Path), maximum_check_line_bytes);
				if (!path.empty() && path.find('\n') == std::string::npos) {
					out.Report.References.push_back({ .Path = std::move(path), .Line = reference.Line });
				}
			}
		}
		out.Report.Commands = merge_commands(read ? asked.Commands : std::vector<cw::asked_command>{ }, aObserved.Commands, aPlan);
		if (out.Report.NotCovered.empty()) {
			out.Report.NotCovered = aPlan.NotCovered;
		}
		if (aObserved.Stopped) {
			out.Report.Outcome = local_check_outcome::stopped;
		} else if (stated && *stated != local_check_outcome::stopped) {
			out.Report.Outcome = *stated;
		} else {
			out.Report.Outcome = out.Report.Commands.empty() ? local_check_outcome::didnt_run : local_check_outcome::couldnt_tell;
		}
		return out;
	}

	auto check_reference_link(const local_check_reference& aReference) -> std::string
	{
		return "bpr-check:" + aReference.Path + ":" + std::to_string(aReference.Line);
	}

	auto read_check_reference_link(const std::string_view aLink) -> std::optional<local_check_reference>
	{
		constexpr auto scheme = std::string_view{ "bpr-check:" };
		if (!aLink.starts_with(scheme)) {
			return std::nullopt;
		}
		const auto rest  = aLink.substr(scheme.size());
		const auto colon = rest.rfind(':');
		if (colon == std::string_view::npos || colon == 0U) {
			return std::nullopt;
		}
		auto       line   = std::uint32_t{ 0U };
		const auto digits = rest.substr(colon + 1U);
		if (const auto [end, error] = std::from_chars(digits.data(), digits.data() + digits.size(), line); error != std::errc{ } || end != digits.data() + digits.size()) {
			return std::nullopt;
		}
		return local_check_reference{ .Path = std::string{ rest.substr(0U, colon) }, .Line = line };
	}

	auto check_candidate_words(const std::uint64_t aChecked, const std::uint64_t aLatest) -> std::string
	{
		auto words = "Checked candidate " + std::to_string(aChecked);
		if (aLatest > aChecked) {
			words += "; the draft is now at candidate " + std::to_string(aLatest);
		}
		return words;
	}

	auto render_check_report(const local_check_report& aReport, const std::uint64_t aLatest) -> std::string
	{
		auto text = "# " + std::string{ outcome_words(aReport.Outcome) } + "\n\n" + check_candidate_words(aReport.Candidate, aLatest) + ". This is the agent's report of its run.\n\n";
		if (!aReport.Summary.empty()) {
			text += aReport.Summary + "\n\n";
		}
		text += "## Commands\n\n";
		if (aReport.Commands.empty()) {
			text += "Nothing ran.\n";
		}
		for (const auto& command : aReport.Commands) {
			// A word first: a list item that opens on a path-like code span
			// reads as a file reference, which a command is not.
			text += "- Ran " + code_span(command.Command);
			if (command.Exit) {
				text += ", exited " + std::to_string(*command.Exit);
			}
			if (command.OutOfPlan) {
				text += " (not in the plan)";
			}
			text += '\n';
		}
		if (!aReport.Output.empty()) {
			const auto fence = fence_for(aReport.Output);
			text += "\n## Output\n\n" + fence + "text\n";
			append_lines(text, aReport.Output, "");
			text += fence + '\n';
		}
		if (!aReport.References.empty()) {
			text += "\n## Code references\n\n";
			for (const auto& reference : aReport.References) {
				const auto place = reference.Path + (reference.Line == 0U ? std::string{ } : ":" + std::to_string(reference.Line));
				text += "- [" + code_span(place) + "](" + check_reference_link(reference) + ")\n";
			}
		}
		if (!aReport.Changed.empty()) {
			text += "\n## Changed in the check copy\n\nThe run changed these files Git tracks in its copy, so the change was not tested as written:\n\n";
			for (const auto& path : aReport.Changed) {
				text += "- " + code_span(path) + '\n';
			}
		}
		text += "\n## Not covered\n\n" + (aReport.NotCovered.empty() ? std::string{ "The report does not say." } : aReport.NotCovered) + '\n';
		return text;
	}
}
