#include "bpr/core/tour_agent_session.hpp"

#include <algorithm>

namespace bpr::core {
	namespace {
		auto trimmed(std::string_view aText) -> std::string_view
		{
			while (!aText.empty() && (aText.front() == ' ' || aText.front() == '\t')) {
				aText.remove_prefix(1U);
			}
			while (!aText.empty() && (aText.back() == ' ' || aText.back() == '\t')) {
				aText.remove_suffix(1U);
			}
			return aText;
		}
		auto contains(const auto& aList, const std::string_view aValue) -> bool { return std::ranges::find(aList, aValue) != aList.end(); }
		/// One shell command read as a pipeline of word lists with its quotes
		/// removed; nullopt for anything a shell would read as more than one
		/// plain pipeline (a redirection, sequence, substitution, expansion,
		/// glob, background job, subshell, comment or second line). Not a
		/// shell: a deliberately small admission check that refuses whatever
		/// it does not read plainly.
		auto pipeline_of(const std::string_view aCommand) -> std::optional<std::vector<std::vector<std::string>>>
		{
			auto pipeline = std::vector<std::vector<std::string>>{ std::vector<std::string>{ } };
			auto word     = std::string{ };
			bool inWord   = false;
			char quote    = '\0';
			for (const char character : aCommand) {
				if (quote == '\'') {
					if (character == '\'') {
						quote = '\0';
					} else {
						word += character;
					}
					continue;
				}
				if (quote == '"') {
					if (character == '$' || character == '`' || character == '\\') {
						return std::nullopt;
					}
					if (character == '"') {
						quote = '\0';
					} else {
						word += character;
					}
					continue;
				}
				if (character == '\'' || character == '"') {
					quote  = character;
					inWord = true;
					continue;
				}
				if (character == ' ' || character == '\t' || character == '|') {
					if (inWord) {
						pipeline.back().push_back(std::move(word));
						word.clear();
						inWord = false;
					}
					if (character == '|') {
						pipeline.emplace_back();
					}
					continue;
				}
				// A `~` only expands at the start of a word (`HEAD~1` is a revision).
				if ((character == '~' && !inWord) || std::string_view{ ";&<>`$(){}\n\r\\*?[]!#" }.find(character) != std::string_view::npos) {
					return std::nullopt;
				}
				word += character;
				inWord = true;
			}
			if (quote != '\0') {
				return std::nullopt;
			}
			if (inWord) {
				pipeline.back().push_back(std::move(word));
			}
			return pipeline;
		}
		auto segment_refusal(const std::vector<std::string>& aList) -> tour_tool_refusal
		{
			if (aList.empty()) {
				return tour_tool_refusal::shell_syntax;
			}
			const auto& program = aList.front();
			if (program.find('=') != std::string::npos) {
				// An environment assignment can redirect a program (GIT_DIR, PAGER).
				return tour_tool_refusal::argument;
			}
			if (!contains(read_only_programs, program)) {
				return tour_tool_refusal::program;
			}
			for (std::size_t index = 1U; index < aList.size(); ++index) {
				const std::string_view word = aList[index];
				if (program == "find" && (word == "-delete" || word.starts_with("-exec") || word.starts_with("-ok") || word.starts_with("-fprint") || word == "-fls")) {
					return tour_tool_refusal::argument;
				}
				if (program == "sort" && (word.starts_with("-o") || word.starts_with("--output"))) {
					return tour_tool_refusal::argument;
				}
				if ((program == "rg" || program == "grep") && (word.starts_with("--pre") || word == "--hostname-bin")) {
					return tour_tool_refusal::argument;
				}
			}
			if (program == "git") {
				// The first word is the command: an option before it may point
				// Git elsewhere (`-C`, `--git-dir`) or run something (`-c`).
				if (aList.size() < 2U || aList[1].starts_with('-')) {
					return tour_tool_refusal::argument;
				}
				if (!contains(read_only_git_commands, aList[1])) {
					return tour_tool_refusal::program;
				}
				for (std::size_t index = 2U; index < aList.size(); ++index) {
					const std::string_view word = aList[index];
					if (word.starts_with("--output") || word.starts_with("--exec-path") || word == "--ext-diff" || word.starts_with("--textconv") || word.starts_with("-O") || word.starts_with("--open-files-in-pager")) {
						return tour_tool_refusal::argument;
					}
				}
			}
			return tour_tool_refusal::none;
		}
		void bound(std::string& aText)
		{
			if (aText.size() <= maximum_tour_transcript_bytes) {
				return;
			}
			auto size = maximum_tour_transcript_bytes;
			while (size > 0U && (static_cast<unsigned char>(aText[size]) & 0xC0U) == 0x80U) {
				--size;
			}
			aText.resize(size);
			aText += "…";
		}
		auto running_run(tour_agent_session& aSession, const tour_agent_run_id aRun) -> tour_agent_run*
		{
			const auto found = std::ranges::find(aSession.Runs, aRun, &tour_agent_run::Id);
			return found == aSession.Runs.end() || found->State != tour_agent_run_state::running ? nullptr : &*found;
		}
		auto nonblank(const std::string_view aText) -> bool
		{
			return std::ranges::any_of(aText, [](const char aCharacter) { return aCharacter != ' ' && aCharacter != '\n' && aCharacter != '\t' && aCharacter != '\r'; });
		}
	}

	auto tools_for(const tour_agent_request aRequest) noexcept -> tour_agent_tools
	{
		switch (aRequest) {
		case tour_agent_request::draft_tour:
		case tour_agent_request::draft_field:
		case tour_agent_request::outline:     return tour_agent_tools::read_only;
		case tour_agent_request::rewrite:     return tour_agent_tools::none;
		}
		return tour_agent_tools::none;
	}

	auto tour_request_of(const writing_hook aHook, const writing_scope aScope) noexcept -> tour_agent_request
	{
		if (aHook == writing_hook::outline) {
			return tour_agent_request::outline;
		}
		if ((aHook == writing_hook::problem || aHook == writing_hook::solution) && aScope == writing_scope::empty_field) {
			return tour_agent_request::draft_field;
		}
		return tour_agent_request::rewrite;
	}

	auto decide_read_only_tool(const std::string_view aTool, const std::string_view aCommand) -> tour_tool_refusal
	{
		if (!contains(read_only_claude_tools, aTool)) {
			return tour_tool_refusal::tool;
		}
		if (aTool != "Bash") {
			return tour_tool_refusal::none;
		}
		const auto pipeline = pipeline_of(aCommand);
		if (!pipeline) {
			return tour_tool_refusal::shell_syntax;
		}
		for (const auto& segment : *pipeline) {
			if (const auto refusal = segment_refusal(segment); refusal != tour_tool_refusal::none) {
				return refusal;
			}
		}
		return tour_tool_refusal::none;
	}

	auto transcript_kind_of(const std::string_view aTool, const std::string_view aCommand) noexcept -> tour_transcript_kind
	{
		if (aTool == "Read") {
			return tour_transcript_kind::read;
		}
		if (aTool == "Grep" || aTool == "Glob") {
			return tour_transcript_kind::searched;
		}
		const auto command = trimmed(aCommand);
		const auto program = command.substr(0U, command.find_first_of(" \t"));
		if (program == "git") {
			return tour_transcript_kind::ran;
		}
		if (program == "cat" || program == "head" || program == "tail" || program == "nl") {
			return tour_transcript_kind::read;
		}
		return tour_transcript_kind::searched;
	}

	auto begin_tour_run(tour_agent_session& aSession, const tour_agent_request aRequest, const tour_agent_tools aTools, const std::string_view aAsked) -> std::optional<tour_agent_run_id>
	{
		if (running_tour_run(aSession)) {
			return std::nullopt;
		}
		if (aSession.Runs.size() >= maximum_tour_agent_runs) {
			aSession.Runs.erase(aSession.Runs.begin());
		}
		auto run   = tour_agent_run{ .Id = tour_agent_run_id{ ++aSession.LastRun }, .Request = aRequest, .Tools = aTools };
		auto asked = std::string{ aAsked };
		bound(asked);
		run.Entries.push_back({ tour_transcript_kind::asked, std::move(asked) });
		aSession.Runs.push_back(std::move(run));
		return aSession.Runs.back().Id;
	}

	auto record_tour_activity(tour_agent_session& aSession, const tour_agent_run_id aRun, const std::vector<tour_transcript_entry>& aEntries) -> bool
	{
		auto* const run = running_run(aSession, aRun);
		if (run == nullptr) {
			return false;
		}
		for (auto entry : aEntries) {
			if (run->Entries.size() >= maximum_tour_transcript_entries) {
				++run->Dropped;
				continue;
			}
			bound(entry.Text);
			run->Entries.push_back(std::move(entry));
		}
		return true;
	}

	auto finish_tour_run(tour_agent_session& aSession, const tour_agent_run_id aRun, const tour_agent_run_state aState, const std::string_view aLast) -> bool
	{
		auto* const run = running_run(aSession, aRun);
		if (run == nullptr || aState == tour_agent_run_state::running) {
			return false;
		}
		run->State = aState;
		if (!aLast.empty()) {
			auto last = std::string{ aLast };
			bound(last);
			// The last line is always kept, even past the bound.
			run->Entries.push_back({ aState == tour_agent_run_state::answered ? tour_transcript_kind::answered : tour_transcript_kind::ended, std::move(last) });
		}
		return true;
	}

	auto running_tour_run(const tour_agent_session& aSession) noexcept -> std::optional<tour_agent_run_id>
	{
		for (const auto& run : aSession.Runs) {
			if (run.State == tour_agent_run_state::running) {
				return run.Id;
			}
		}
		return std::nullopt;
	}

	auto find_tour_run(const tour_agent_session& aSession, const tour_agent_run_id aRun) noexcept -> const tour_agent_run*
	{
		const auto found = std::ranges::find(aSession.Runs, aRun, &tour_agent_run::Id);
		return found == aSession.Runs.end() ? nullptr : &*found;
	}

	auto anchor_tour_stop(const proposed_tour_stop& aStop, const std::vector<tour_anchor_file>& aFiles) -> tour_anchor_verdict
	{
		if (aStop.FirstLine == 0U || aStop.LastLine < aStop.FirstLine || aStop.LastLine - aStop.FirstLine >= maximum_proposed_stop_lines) {
			return tour_anchor_verdict::invalid_lines;
		}
		const auto file = std::ranges::find(aFiles, aStop.Path, &tour_anchor_file::Path);
		if (aStop.Path.empty() || file == aFiles.end()) {
			return tour_anchor_verdict::not_in_pull_request;
		}
		if (!file->OnHead) {
			return tour_anchor_verdict::not_on_head;
		}
		if (!file->HeadLines) {
			return tour_anchor_verdict::unmeasured;
		}
		return aStop.LastLine > *file->HeadLines ? tour_anchor_verdict::beyond_file : tour_anchor_verdict::anchored;
	}

	auto propose_tour(const tour_agent_run_id aRun, const tour_draft_answer& aAnswer, const std::vector<tour_anchor_file>& aFiles, const review_tour& aDraft) -> tour_proposal
	{
		auto proposal = tour_proposal{ .Run = aRun };
		if (aAnswer.Problem && nonblank(*aAnswer.Problem)) {
			proposal.Items.push_back({ .Part = tour_proposal_part::problem, .Text = *aAnswer.Problem, .ReplacesAuthorText = nonblank(aDraft.Problem) });
		}
		if (aAnswer.Solution && nonblank(*aAnswer.Solution)) {
			proposal.Items.push_back({ .Part = tour_proposal_part::solution, .Text = *aAnswer.Solution, .ReplacesAuthorText = nonblank(aDraft.Solution) });
		}
		for (std::size_t index = 0U; index < aAnswer.Stops.size() && index < maximum_proposed_tour_stops; ++index) {
			const auto& stop = aAnswer.Stops[index];
			proposal.Items.push_back({ .Part = tour_proposal_part::stop, .Text = stop.Explanation, .Stop = stop, .Verdict = anchor_tour_stop(stop, aFiles) });
		}
		return proposal;
	}

	auto acceptable(const tour_proposal_item& aItem) noexcept -> bool
	{
		return aItem.Decision == tour_proposal_decision::pending && (aItem.Part != tour_proposal_part::stop || aItem.Verdict == tour_anchor_verdict::anchored);
	}

	auto decide_tour_proposal(tour_proposal& aProposal, const std::size_t aIndex, const tour_proposal_decision aDecision, const std::string_view aText) -> bool
	{
		if (aIndex >= aProposal.Items.size() || aDecision == tour_proposal_decision::pending) {
			return false;
		}
		auto& item = aProposal.Items[aIndex];
		if (item.Decision != tour_proposal_decision::pending) {
			return false;
		}
		if (aDecision == tour_proposal_decision::accepted) {
			if (!acceptable(item) || !nonblank(aText) || aText.size() > maximum_tour_field_bytes || !valid_writing_utf8(aText)) {
				return false;
			}
			item.Text = std::string{ aText };
			if (item.Part == tour_proposal_part::stop) {
				item.Stop.Explanation = item.Text;
			}
		}
		item.Decision = aDecision;
		return true;
	}

	auto tour_stop_of(const proposed_tour_stop& aStop, const git_commit_oid& aHead) -> tour_stop
	{
		return tour_stop{
			.Title       = aStop.Title,
			.Body        = aStop.Explanation,
			.Mapping     = tour_mapping::unavailable,
			.Destination = tour_code_destination{ .Commit = aHead, .Path = aStop.Path, .Side = diff_side::right, .FirstLine = aStop.FirstLine - 1U, .LastLine = aStop.LastLine - 1U },
		};
	}
}
