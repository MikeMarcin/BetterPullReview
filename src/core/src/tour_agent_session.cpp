#include "bpr/core/tour_agent_session.hpp"

#include <algorithm>
#include <array>
#include <string>

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
		/// One shell command read as a list of commands (joined by `|`, `&&`,
		/// `||` or `;`), each a word list with its quotes removed; nullopt for
		/// anything a shell would read as more than plain commands: a
		/// redirection other than to `/dev/null` (or `2>&1`), a background
		/// job, a substitution or expansion, a subshell or a second line. A
		/// glob only lists file names, so it is admitted. Not a shell: a
		/// deliberately small admission check that refuses whatever it does
		/// not read plainly. Claude Code splits a compound command the same
		/// way before it matches each part against its allowed prefixes, so a
		/// chain it allows is read here too ([[BPR-1900]], installed Claude
		/// 2.1.310 chains `git show --stat HEAD && git show HEAD | head`).
		auto pipeline_of(const std::string_view aCommand) -> std::optional<std::vector<std::vector<std::string>>>
		{
			static constexpr std::array<std::string_view, 9> silenced = { "2>&1", "2>/dev/null", "1>/dev/null", "&>/dev/null", ">/dev/null", "2> /dev/null", "1> /dev/null", "&> /dev/null", "> /dev/null" };
			auto                                             pipeline = std::vector<std::vector<std::string>>{ std::vector<std::string>{ } };
			auto                                             word     = std::string{ };
			bool                                             inWord   = false;
			char                                             quote    = '\0';
			const auto                                       close    = [&] {
				if (inWord) {
					pipeline.back().push_back(std::move(word));
					word.clear();
					inWord = false;
				}
			};
			for (std::size_t at = 0U; at < aCommand.size(); ++at) {
				const char character = aCommand[at];
				if (quote == '\'') {
					if (character == '\'') {
						quote = '\0';
					} else {
						word += character;
					}
					continue;
				}
				if (quote == '"') {
					// A backslash keeps the next character literal (`"a\|b"` is
					// a grep alternation); `$` and a backquote still expand.
					if (character == '\\' && at + 1U < aCommand.size() && aCommand[at + 1U] != '\n') {
						const auto next = aCommand[++at];
						if (next != '$' && next != '`' && next != '"' && next != '\\') {
							word += '\\';
						}
						word += next;
						continue;
					}
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
				// Outside quotes a backslash makes the next character literal.
				if (character == '\\' && at + 1U < aCommand.size() && aCommand[at + 1U] != '\n' && aCommand[at + 1U] != '\r') {
					word += aCommand[++at];
					inWord = true;
					continue;
				}
				if (!inWord) {
					const auto rest  = aCommand.substr(at);
					const auto found = std::ranges::find_if(silenced, [&rest](const std::string_view aRedirection) {
						return rest.starts_with(aRedirection) && (rest.size() == aRedirection.size() || std::string_view{ " \t|;&" }.find(rest[aRedirection.size()]) != std::string_view::npos);
					});
					if (found != silenced.end()) {
						at += found->size() - 1U;
						continue;
					}
				}
				if (character == ' ' || character == '\t') {
					close();
					continue;
				}
				if (character == '|' || character == ';' || character == '&') {
					close();
					const auto doubled = at + 1U < aCommand.size() && aCommand[at + 1U] == character;
					if (character == '&' && !doubled) {
						// A background job.
						return std::nullopt;
					}
					if (doubled && character != ';') {
						++at;
					}
					if (pipeline.back().empty()) {
						return std::nullopt;
					}
					pipeline.emplace_back();
					continue;
				}
				if (std::string_view{ "<>`$(){}\n\r\\!#" }.find(character) != std::string_view::npos) {
					return std::nullopt;
				}
				word += character;
				inWord = true;
			}
			if (quote != '\0') {
				return std::nullopt;
			}
			close();
			// A trailing `;` ends the last command; nothing else may.
			if (pipeline.size() > 1U && pipeline.back().empty() && !aCommand.empty() && aCommand.find_last_not_of(" \t") != std::string_view::npos && aCommand[aCommand.find_last_not_of(" \t")] == ';') {
				pipeline.pop_back();
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
				if (program == "diff" && word.starts_with("--to-file")) {
					return tour_tool_refusal::argument;
				}
			}
			if (program == "sed") {
				// Print only: `-n`, and a script of line addresses ending in `p`.
				auto quiet  = false;
				auto script = std::string_view{ };
				for (std::size_t index = 1U; index < aList.size(); ++index) {
					const std::string_view word = aList[index];
					if (word == "-n" || word == "--quiet") {
						quiet = true;
					} else if (word.starts_with('-')) {
						return tour_tool_refusal::argument;
					} else if (script.empty()) {
						script = word;
					}
				}
				const auto lines = !script.empty() && script.back() == 'p' && std::ranges::all_of(script.substr(0U, script.size() - 1U), [](const char aCharacter) { return (aCharacter >= '0' && aCharacter <= '9') || aCharacter == ',' || aCharacter == '$'; });
				if (!quiet || !lines) {
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
		case tour_agent_request::outline:
		case tour_agent_request::ask:         return tour_agent_tools::read_only;
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
		// [[BPR-1912]]. A program outside the read-only list that Claude ran
		// keeps the kind of what it does: one that shows a file reads, one
		// that finds searches, and anything else ran.
		static constexpr std::array<std::string_view, 11> reading   = { "cat", "head", "tail", "nl", "sed", "awk", "less", "more", "od", "column", "jq" };
		static constexpr std::array<std::string_view, 6>  searching = { "rg", "grep", "find", "ls", "tree", "fd" };
		if (contains(reading, program)) {
			return tour_transcript_kind::read;
		}
		if (contains(searching, program)) {
			return tour_transcript_kind::searched;
		}
		return tour_transcript_kind::ran;
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
			if (entry.Note == tour_transcript_note::ran_outside_list) {
				++run->OutsideList;
			}
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
			proposal.Items.push_back({ .Part = tour_proposal_part::problem, .Text = *aAnswer.Problem, .ReplacesAuthorText = nonblank(aDraft.Problem), .Run = aRun });
		}
		if (aAnswer.Solution && nonblank(*aAnswer.Solution)) {
			proposal.Items.push_back({ .Part = tour_proposal_part::solution, .Text = *aAnswer.Solution, .ReplacesAuthorText = nonblank(aDraft.Solution), .Run = aRun });
		}
		auto stops = std::vector<tour_proposal_item>{ };
		for (std::size_t index = 0U; index < aAnswer.Stops.size() && index < maximum_proposed_tour_stops; ++index) {
			const auto& stop    = aAnswer.Stops[index];
			const auto  verdict = anchor_tour_stop(stop, aFiles);
			if (verdict != tour_anchor_verdict::anchored) {
				proposal.LeftOut.push_back({ stop, verdict });
				continue;
			}
			const auto& file = *std::ranges::find(aFiles, stop.Path, &tour_anchor_file::Path);
			stops.push_back({ .Part = tour_proposal_part::stop, .Text = stop.Explanation, .Stop = stop, .Verdict = verdict, .Placement = place_tour_stop(stop, file), .Run = aRun });
		}
		// On the change first, then near it, then off it; the agent's order
		// within each.
		std::ranges::stable_sort(stops, { }, &tour_proposal_item::Placement);
		proposal.Items.insert(proposal.Items.end(), std::make_move_iterator(stops.begin()), std::make_move_iterator(stops.end()));
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

	auto parse_patch_head_hunks(const std::string_view aPatch) -> std::optional<std::vector<tour_line_span>>
	{
		auto       hunks  = std::vector<tour_line_span>{ };
		const auto number = [](std::string_view& aText) -> std::optional<std::uint32_t> {
			auto value  = std::uint64_t{ 0U };
			auto digits = std::size_t{ 0U };
			while (!aText.empty() && aText.front() >= '0' && aText.front() <= '9' && digits < 10U) {
				value = value * 10U + static_cast<std::uint64_t>(aText.front() - '0');
				aText.remove_prefix(1U);
				++digits;
			}
			if (digits == 0U || value > 0xFFFF'FFFFU) {
				return std::nullopt;
			}
			return static_cast<std::uint32_t>(value);
		};
		auto rest = aPatch;
		while (!rest.empty()) {
			const auto end  = rest.find('\n');
			auto       line = rest.substr(0U, end);
			rest            = end == std::string_view::npos ? std::string_view{ } : rest.substr(end + 1U);
			if (!line.starts_with("@@ -")) {
				continue;
			}
			const auto plus = line.find(" +");
			if (plus == std::string_view::npos) {
				return std::nullopt;
			}
			line.remove_prefix(plus + 2U);
			const auto first = number(line);
			auto       count = std::optional<std::uint32_t>{ 1U };
			if (!line.empty() && line.front() == ',') {
				line.remove_prefix(1U);
				count = number(line);
			}
			if (!first || !count || !line.starts_with(" @@")) {
				return std::nullopt;
			}
			if (*count == 0U) {
				continue;
			}
			if (hunks.size() >= maximum_patch_hunks || *first == 0U || *count - 1U > 0xFFFF'FFFFU - *first) {
				return std::nullopt;
			}
			hunks.push_back({ *first, *first + *count - 1U });
		}
		return hunks;
	}

	auto place_tour_stop(const proposed_tour_stop& aStop, const tour_anchor_file& aFile) -> tour_stop_placement
	{
		if (!aFile.Hunks) {
			return tour_stop_placement::on_change;
		}
		const auto inside = std::ranges::any_of(*aFile.Hunks, [&aStop](const tour_line_span& aHunk) { return aStop.FirstLine >= aHunk.First && aStop.LastLine <= aHunk.Last; });
		if (inside) {
			return tour_stop_placement::on_change;
		}
		return aStop.FirstLine == aStop.LastLine ? tour_stop_placement::near_change : tour_stop_placement::off_change;
	}

	auto relocate_tour_stop(tour_proposal& aProposal, const std::size_t aIndex, const std::vector<tour_anchor_file>& aFiles) -> bool
	{
		if (aIndex >= aProposal.Items.size()) {
			return false;
		}
		auto& item = aProposal.Items[aIndex];
		if (item.Part != tour_proposal_part::stop || item.Decision != tour_proposal_decision::pending) {
			return false;
		}
		const auto file = std::ranges::find(aFiles, item.Stop.Path, &tour_anchor_file::Path);
		if (file == aFiles.end() || !file->Hunks || file->Hunks->empty()) {
			return false;
		}
		const auto distance = [&item](const tour_line_span& aHunk) -> std::uint64_t {
			if (item.Stop.LastLine < aHunk.First) {
				return aHunk.First - item.Stop.LastLine;
			}
			if (item.Stop.FirstLine > aHunk.Last) {
				return item.Stop.FirstLine - aHunk.Last;
			}
			return 0U;
		};
		const auto& hunk    = *std::ranges::min_element(*file->Hunks, { }, distance);
		const auto  length  = std::min(item.Stop.LastLine - item.Stop.FirstLine, hunk.Last - hunk.First);
		const auto  first   = std::clamp(item.Stop.FirstLine, hunk.First, hunk.Last - length);
		item.Stop.FirstLine = first;
		item.Stop.LastLine  = first + length;
		item.Placement      = tour_stop_placement::on_change;
		return true;
	}

	auto left_out_entries(const tour_proposal& aProposal) -> std::vector<tour_transcript_entry>
	{
		auto entries = std::vector<tour_transcript_entry>{ };
		for (const auto& [stop, verdict] : aProposal.LeftOut) {
			const auto why   = verdict == tour_anchor_verdict::not_in_pull_request ? "not a file of this pull request"
				: verdict == tour_anchor_verdict::not_on_head                      ? "a file this pull request deletes"
				: verdict == tour_anchor_verdict::beyond_file                      ? "past the end of the file"
				: verdict == tour_anchor_verdict::unmeasured                       ? "the file's lines could not be counted"
																				   : "not lines a stop can point at";
			const auto lines = stop.FirstLine == stop.LastLine ? "line " + std::to_string(stop.FirstLine) : "lines " + std::to_string(stop.FirstLine) + " to " + std::to_string(stop.LastLine);
			entries.push_back({ tour_transcript_kind::left_out, "\"" + stop.Title + "\", " + stop.Path + ", " + lines + ": " + why });
		}
		return entries;
	}

	auto note_finished_tour_run(tour_agent_session& aSession, const tour_agent_run_id aRun, const std::vector<tour_transcript_entry>& aEntries) -> bool
	{
		const auto found = std::ranges::find(aSession.Runs, aRun, &tour_agent_run::Id);
		if (found == aSession.Runs.end() || found->State == tour_agent_run_state::running) {
			return false;
		}
		for (auto entry : aEntries) {
			if (found->Entries.size() >= maximum_tour_transcript_entries + 8U) {
				++found->Dropped;
				continue;
			}
			bound(entry.Text);
			found->Entries.push_back(std::move(entry));
		}
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

	namespace {
		/// Cut to `aLimit` bytes at a character boundary.
		void cut(std::string& aText, const std::size_t aLimit)
		{
			if (aText.size() <= aLimit) {
				return;
			}
			auto size = aLimit;
			while (size > 0U && (static_cast<unsigned char>(aText[size]) & 0xC0U) == 0x80U) {
				--size;
			}
			aText.resize(size);
		}
		auto turn_bytes(const tour_conversation_turn& aTurn) -> std::size_t
		{
			return aTurn.Asked.size() + aTurn.Reply.size() + (aTurn.Passage ? aTurn.Passage->Text.size() + aTurn.Passage->StopTitle.size() : 0U);
		}
	}

	auto replay(const tour_agent_session& aSession, const std::optional<tour_thread>& aThread) -> tour_conversation;

	auto begin_tour_ask(tour_agent_session& aSession, const tour_thread& aThread, const std::optional<tour_passage>& aPassage, const std::string_view aQuestion, const bool aUpdate) -> std::optional<tour_agent_run_id>
	{
		if (!nonblank(aQuestion) || aQuestion.size() > maximum_tour_question_bytes || !valid_writing_utf8(aQuestion)) {
			return std::nullopt;
		}
		if (aPassage
			&& (!nonblank(aPassage->Text) || aPassage->Text.size() > maximum_tour_passage_bytes || !valid_writing_utf8(aPassage->Text) || aPassage->StopTitle.size() > maximum_tour_passage_bytes || !valid_writing_utf8(aPassage->StopTitle)
				|| aPassage->Path.size() > maximum_tour_path_bytes || !valid_writing_utf8(aPassage->Path) || aPassage->LastLine < aPassage->FirstLine)) {
			return std::nullopt;
		}
		const auto run = begin_tour_run(aSession, tour_agent_request::ask, tools_for(tour_agent_request::ask), aQuestion);
		if (run) {
			auto& started    = aSession.Runs.back();
			started.Passage  = aPassage;
			started.Question = std::string{ aQuestion };
			started.Thread   = aThread;
			started.Update   = aUpdate;
		}
		return run;
	}

	auto answer_tour_ask(tour_agent_session& aSession, const tour_agent_run_id aRun, const std::string_view aReply, const tour_proposal_tally& aProposed) -> bool
	{
		auto* const run = running_run(aSession, aRun);
		if (run == nullptr || run->Request != tour_agent_request::ask) {
			return false;
		}
		run->Reply = std::string{ aReply };
		cut(run->Reply, maximum_tour_reply_bytes);
		run->Proposed = aProposed;
		return finish_tour_run(aSession, aRun, tour_agent_run_state::answered, run->Reply);
	}

	auto tour_conversation_of(const tour_agent_session& aSession) -> tour_conversation
	{
		return replay(aSession, std::nullopt);
	}

	auto tour_conversation_of(const tour_agent_session& aSession, const tour_thread& aThread) -> tour_conversation { return replay(aSession, aThread); }

	auto time_tour_run(tour_agent_session& aSession, const tour_agent_run_id aRun, const std::uint32_t aSeconds) -> bool
	{
		const auto found = std::ranges::find(aSession.Runs, aRun, &tour_agent_run::Id);
		if (found == aSession.Runs.end() || found->State == tour_agent_run_state::running) {
			return false;
		}
		found->Seconds = aSeconds;
		return true;
	}

	auto keep_thread_part(tour_proposal aProposal, const tour_thread& aThread) -> tour_proposal
	{
		if (aThread.Kind == tour_thread_kind::tour) {
			return aProposal;
		}
		std::erase_if(aProposal.Items, [&aThread](const tour_proposal_item& aItem) {
			switch (aThread.Kind) {
			case tour_thread_kind::problem:  return aItem.Part != tour_proposal_part::problem;
			case tour_thread_kind::solution: return aItem.Part != tour_proposal_part::solution;
			case tour_thread_kind::stop:     return aItem.Part != tour_proposal_part::stop_text || aItem.StopId != aThread.Stop;
			case tour_thread_kind::tour:     return false;
			}
			return true;
		});
		aProposal.LeftOut.clear();
		return aProposal;
	}

	auto replay(const tour_agent_session& aSession, const std::optional<tour_thread>& aThread) -> tour_conversation
	{
		auto turns = std::vector<tour_conversation_turn>{ };
		for (const auto& run : aSession.Runs) {
			if (run.State != tour_agent_run_state::answered) {
				continue;
			}
			// [[BPR-1914]] v003. A side chat replays its own turns; the
			// tour's chat also has the whole-tour drafts.
			if (aThread && (run.Request == tour_agent_request::ask ? !(run.Thread == *aThread) : aThread->Kind != tour_thread_kind::tour)) {
				continue;
			}
			if (run.Request == tour_agent_request::ask) {
				turns.push_back({ run.Question, run.Passage, run.Reply, run.Thread });
			} else if (run.Request == tour_agent_request::draft_tour) {
				const auto answered = std::ranges::find(run.Entries, tour_transcript_kind::answered, &tour_transcript_entry::Kind);
				turns.push_back({ "Draft the tour", std::nullopt, answered == run.Entries.end() ? std::string{ } : answered->Text, tour_thread{ } });
			}
		}
		auto conversation = tour_conversation{ };
		auto bytes        = std::size_t{ 0U };
		auto first        = turns.size();
		while (first > 0U && turns.size() - first < maximum_replayed_tour_turns && bytes + turn_bytes(turns[first - 1U]) <= maximum_replayed_tour_bytes) {
			bytes += turn_bytes(turns[first - 1U]);
			--first;
		}
		conversation.Dropped = first;
		conversation.Turns.assign(std::make_move_iterator(turns.begin() + static_cast<std::ptrdiff_t>(first)), std::make_move_iterator(turns.end()));
		return conversation;
	}

	auto propose_tour_changes(const tour_agent_run_id aRun, const tour_ask_answer& aAnswer, const std::vector<tour_anchor_file>& aFiles, const review_tour& aDraft) -> tour_proposal
	{
		auto proposal = propose_tour(aRun, aAnswer.Changes, aFiles, aDraft);
		auto seen     = std::vector<std::size_t>{ };
		for (const auto& rewrite : aAnswer.StopTexts) {
			if (rewrite.Stop == 0U || rewrite.Stop > aDraft.Stops.size() || !nonblank(rewrite.Explanation) || std::ranges::find(seen, rewrite.Stop) != seen.end()) {
				continue;
			}
			seen.push_back(rewrite.Stop);
			const auto& stop = aDraft.Stops[rewrite.Stop - 1U];
			auto        item = tour_proposal_item{ .Part = tour_proposal_part::stop_text, .Text = rewrite.Explanation, .ReplacesAuthorText = nonblank(stop.Body), .StopId = stop.Id, .Run = aRun };
			item.Stop.Title  = stop.Title;
			proposal.Items.push_back(std::move(item));
		}
		return proposal;
	}

	auto tally_of(const tour_proposal& aProposal) noexcept -> tour_proposal_tally
	{
		auto tally = tour_proposal_tally{ };
		for (const auto& item : aProposal.Items) {
			switch (item.Part) {
			case tour_proposal_part::problem:   tally.Problem = true; break;
			case tour_proposal_part::solution:  tally.Solution = true; break;
			case tour_proposal_part::stop:      ++tally.Stops; break;
			case tour_proposal_part::stop_text: ++tally.StopTexts; break;
			}
		}
		return tally;
	}

	auto merge_tour_proposal(const std::optional<tour_proposal>& aCurrent, tour_proposal aIncoming) -> std::optional<tour_proposal>
	{
		if (aIncoming.Items.empty()) {
			return aCurrent;
		}
		if (aCurrent) {
			const auto superseded = [&aIncoming](const tour_proposal_item& aItem) {
				return std::ranges::any_of(aIncoming.Items, [&aItem](const tour_proposal_item& aNew) {
					return aNew.Part == aItem.Part && (aItem.Part == tour_proposal_part::problem || aItem.Part == tour_proposal_part::solution || (aItem.Part == tour_proposal_part::stop_text && aNew.StopId == aItem.StopId));
				});
			};
			for (const auto& item : aCurrent->Items) {
				if (item.Decision == tour_proposal_decision::pending && !superseded(item)) {
					aIncoming.Items.push_back(item);
				}
			}
		}
		return aIncoming;
	}

	auto thread_runs(const tour_agent_session& aSession, const tour_thread& aThread) -> std::vector<const tour_agent_run*>
	{
		auto runs = std::vector<const tour_agent_run*>{ };
		for (const auto& run : aSession.Runs) {
			if (run.Request == tour_agent_request::ask && run.Thread == aThread) {
				runs.push_back(&run);
			}
		}
		return runs;
	}

	auto chat_threads(const tour_agent_session& aSession) -> std::vector<tour_thread>
	{
		auto threads = std::vector<tour_thread>{ };
		for (const auto& run : aSession.Runs) {
			if (run.Request == tour_agent_request::ask && std::ranges::find(threads, run.Thread) == threads.end()) {
				threads.push_back(run.Thread);
			}
		}
		return threads;
	}

	auto proposed_in_thread(const tour_agent_session& aSession, const tour_proposal_item& aItem) noexcept -> bool
	{
		const auto* const run = find_tour_run(aSession, aItem.Run);
		return run != nullptr && run->Request == tour_agent_request::ask;
	}
}
