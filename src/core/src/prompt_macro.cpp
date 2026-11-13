#include "bpr/core/prompt_macro.hpp"

#include "bpr/core/agent_context_supplement.hpp"
#include "bpr/core/review_cycle_ignore.hpp"
#include "bpr/core/skill_references.hpp"
#include "bpr/core/tour_agent_session.hpp"

#include <algorithm>
#include <array>
#include <charconv>

namespace bpr::core {
	static_assert(maximum_prompt_expansion_bytes == maximum_chat_message_bytes, "an expansion is bounded as a chat message is");
	static_assert(maximum_prompt_expansion_bytes == maximum_tour_question_bytes, "an expansion is bounded as a tour question is");
	static_assert(static_cast<std::size_t>(prompt_place::tour_whole) + 1U == prompt_place_count, "prompt_place_count covers every place");
	static_assert(static_cast<std::size_t>(prompt_fact::empty_chat) + 1U == prompt_fact_count, "prompt_fact_count covers every fact");
	static_assert(static_cast<std::size_t>(prompt_variable::cursor) + 1U == prompt_variable_count, "prompt_variable_count covers every variable");

	namespace {
		constexpr auto variable_names = std::array<std::string_view, prompt_variable_count>{
			"pr.number",
			"pr.title",
			"pr.url",
			"pr.author",
			"pr.base",
			"pr.head",
			"repo",
			"file",
			"file.name",
			"lines",
			"lines.first",
			"lines.last",
			"conversation.file",
			"stop.number",
			"stop.title",
			"part",
			"cursor",
		};

		auto is_tour(const prompt_place aPlace) noexcept -> bool
		{
			return aPlace == prompt_place::tour_stop || aPlace == prompt_place::tour_part || aPlace == prompt_place::tour_whole;
		}

		auto is_lower(const char aValue) noexcept -> bool { return aValue >= 'a' && aValue <= 'z'; }
		auto is_digit(const char aValue) noexcept -> bool { return aValue >= '0' && aValue <= '9'; }
		auto is_alnum(const char aValue) noexcept -> bool { return is_lower(aValue) || is_digit(aValue) || (aValue >= 'A' && aValue <= 'Z'); }
		auto is_name_byte(const char aValue) noexcept -> bool { return is_lower(aValue) || is_digit(aValue) || aValue == '.' || aValue == '-' || aValue == '_'; }

		auto decimal(const std::uint64_t aValue) -> std::string
		{
			auto       buffer = std::array<char, 24>{ };
			const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), aValue);
			return { buffer.data(), result.ptr };
		}

		/// The length of a line break or control character at `aIndex`, or 0:
		/// C0 controls and DEL, C1 controls (NEL among them), and U+2028 and
		/// U+2029.
		auto control_length(const std::string_view aText, const std::size_t aIndex) noexcept -> std::size_t
		{
			const auto byte = static_cast<unsigned char>(aText[aIndex]);
			if (byte < 0x20U || byte == 0x7FU) {
				return 1U;
			}
			const auto next = [&](const std::size_t aOffset) { return aIndex + aOffset < aText.size() ? static_cast<unsigned char>(aText[aIndex + aOffset]) : 0U; };
			if (byte == 0xC2U && next(1U) >= 0x80U && next(1U) <= 0x9FU) {
				return 2U;
			}
			if (byte == 0xE2U && next(1U) == 0x80U && (next(2U) == 0xA8U || next(2U) == 0xA9U)) {
				return 3U;
			}
			return 0U;
		}

		/// A value as one line: each line break and control character
		/// becomes one space.
		auto append_one_line(std::string& aOut, const std::string_view aValue) -> void
		{
			auto index = std::size_t{ 0 };
			while (index < aValue.size()) {
				if (const auto length = control_length(aValue, index); length != 0U) {
					aOut.push_back(' ');
					index += length;
				} else {
					aOut.push_back(aValue[index]);
					++index;
				}
			}
		}

		auto has_control(const std::string_view aText) noexcept -> bool
		{
			for (auto index = std::size_t{ 0 }; index < aText.size(); ++index) {
				if (control_length(aText, index) != 0U) {
					return true;
				}
			}
			return false;
		}

		auto blank(const std::string_view aText) noexcept -> bool
		{
			return std::ranges::all_of(aText, [](const char aValue) { return aValue == ' ' || aValue == '\t' || aValue == '\n' || aValue == '\r'; });
		}

		auto ordered(const prompt_line_range aRange) noexcept -> prompt_line_range
		{
			return { .First = std::min(aRange.First, aRange.Last), .Last = std::max(aRange.First, aRange.Last) };
		}

		auto stop_known(const prompt_facts& aFacts) noexcept -> bool { return aFacts.Thread && aFacts.Thread->StopNumber; }
		auto part_known(const prompt_facts& aFacts) noexcept -> bool { return aFacts.Thread && !aFacts.Thread->Part.empty(); }

		/// The need a variable implies, if any.
		auto variable_need(const prompt_variable aVariable) noexcept -> std::optional<prompt_unmet>
		{
			switch (aVariable) {
			case prompt_variable::pr_number:
			case prompt_variable::pr_title:
			case prompt_variable::pr_url:
			case prompt_variable::pr_author:
			case prompt_variable::pr_base:
			case prompt_variable::pr_head:           return prompt_unmet::needs_pull_request;
			case prompt_variable::repo:              return prompt_unmet::needs_repository;
			case prompt_variable::file:
			case prompt_variable::file_name:         return prompt_unmet::needs_file;
			case prompt_variable::lines:
			case prompt_variable::lines_first:
			case prompt_variable::lines_last:        return prompt_unmet::needs_lines;
			case prompt_variable::conversation_file: return prompt_unmet::needs_conversation_file;
			case prompt_variable::stop_number:
			case prompt_variable::stop_title:        return prompt_unmet::needs_stop;
			case prompt_variable::part:              return prompt_unmet::needs_part;
			case prompt_variable::cursor:            return std::nullopt;
			}
			return std::nullopt;
		}

		auto subject_need(const prompt_subject aSubject) noexcept -> std::optional<prompt_unmet>
		{
			switch (aSubject) {
			case prompt_subject::open_file:      return prompt_unmet::needs_file;
			case prompt_subject::selected_lines: return prompt_unmet::needs_lines;
			case prompt_subject::selected_text:  return prompt_unmet::needs_selected_text;
			case prompt_subject::pull_request:
			case prompt_subject::conversation:
			case prompt_subject::stop:
			case prompt_subject::part:
			case prompt_subject::tour:           return std::nullopt;
			}
			return std::nullopt;
		}

		auto need_met(const prompt_unmet aNeed, const prompt_place aPlace, const prompt_facts& aFacts) noexcept -> bool
		{
			switch (aNeed) {
			case prompt_unmet::needs_file:              return aFacts.OpenFile.has_value();
			case prompt_unmet::needs_lines:             return aFacts.Lines.has_value();
			case prompt_unmet::needs_conversation_file: return aFacts.ConversationFile.has_value();
			case prompt_unmet::needs_selected_text:     return aFacts.Selection && aFacts.Selection->Place == aPlace;
			case prompt_unmet::needs_stop:              return stop_known(aFacts);
			case prompt_unmet::needs_part:              return part_known(aFacts);
			case prompt_unmet::needs_pull_request:      return aFacts.PullRequest.has_value();
			case prompt_unmet::needs_repository:        return aFacts.Repository.has_value();
			case prompt_unmet::off:
			case prompt_unmet::invalid:
			case prompt_unmet::condition:
			case prompt_unmet::expansion_too_long:      return true;
			}
			return true;
		}

		/// The value of a variable; nothing when the facts lack it.
		auto variable_value(const prompt_variable aVariable, const prompt_facts& aFacts) -> std::optional<std::string>
		{
			const auto& pr = aFacts.PullRequest;
			switch (aVariable) {
			case prompt_variable::pr_number: return pr ? std::optional{ decimal(pr->Number) } : std::nullopt;
			case prompt_variable::pr_title:  return pr ? std::optional{ pr->Title } : std::nullopt;
			case prompt_variable::pr_url:    return pr ? std::optional{ pr->Url } : std::nullopt;
			case prompt_variable::pr_author: return pr ? std::optional{ pr->Author } : std::nullopt;
			case prompt_variable::pr_base:   return pr ? std::optional{ pr->Base } : std::nullopt;
			case prompt_variable::pr_head:   return pr ? std::optional{ pr->Head } : std::nullopt;
			case prompt_variable::repo:      return aFacts.Repository;
			case prompt_variable::file:      return aFacts.OpenFile;
			case prompt_variable::file_name: {
				if (!aFacts.OpenFile) {
					return std::nullopt;
				}
				const auto slash = aFacts.OpenFile->find_last_of('/');
				return slash == std::string::npos ? *aFacts.OpenFile : aFacts.OpenFile->substr(slash + 1U);
			}
			case prompt_variable::lines: {
				if (!aFacts.Lines) {
					return std::nullopt;
				}
				const auto range = ordered(*aFacts.Lines);
				// U+2013 EN DASH, as the chips have always written a range.
				return range.First == range.Last ? "line " + decimal(range.First) : "lines " + decimal(range.First) + "\xE2\x80\x93" + decimal(range.Last);
			}
			case prompt_variable::lines_first:       return aFacts.Lines ? std::optional{ decimal(ordered(*aFacts.Lines).First) } : std::nullopt;
			case prompt_variable::lines_last:        return aFacts.Lines ? std::optional{ decimal(ordered(*aFacts.Lines).Last) } : std::nullopt;
			case prompt_variable::conversation_file: return aFacts.ConversationFile;
			case prompt_variable::stop_number:       return stop_known(aFacts) ? std::optional{ decimal(*aFacts.Thread->StopNumber) } : std::nullopt;
			case prompt_variable::stop_title:        return stop_known(aFacts) ? std::optional{ aFacts.Thread->StopTitle } : std::nullopt;
			case prompt_variable::part:              return part_known(aFacts) ? std::optional{ aFacts.Thread->Part } : std::nullopt;
			case prompt_variable::cursor:            return std::string{ };
			}
			return std::nullopt;
		}

		/// A pattern as `.gitignore` reads it: no slash, the last segment;
		/// a slash, the whole path, with a leading slash dropped and a
		/// trailing one meaning everything below.
		auto pattern_matches(const std::string_view aPattern, const std::string_view aPath, const bool aCaseFold) -> bool
		{
			if (aPattern.find('/') == std::string_view::npos) {
				const auto slash = aPath.find_last_of('/');
				return wildcard_match(aPattern, slash == std::string_view::npos ? aPath : aPath.substr(slash + 1U), aCaseFold);
			}
			auto pattern = std::string{ aPattern.starts_with('/') ? aPattern.substr(1U) : aPattern };
			if (pattern.ends_with('/')) {
				pattern += "**";
			}
			return wildcard_match(pattern, aPath, aCaseFold);
		}

		auto any_pattern_matches(const std::vector<std::string>& aPatterns, const std::string_view aPath, const bool aCaseFold) -> bool
		{
			return std::ranges::any_of(aPatterns, [&](const std::string& aPattern) { return pattern_matches(aPattern, aPath, aCaseFold); });
		}

		/// A yes-or-no fact under "is" or "is not"; unknown holds for neither.
		auto known_is(const std::optional<bool> aFact, const bool aNegated) noexcept -> bool { return aFact.has_value() && *aFact != aNegated; }

		auto lines_match(const prompt_lines_value aValue, const std::optional<prompt_line_range>& aLines) noexcept -> bool
		{
			if (!aLines) {
				return false;
			}
			switch (aValue) {
			case prompt_lines_value::any:     return true;
			case prompt_lines_value::one:     return aLines->First == aLines->Last;
			case prompt_lines_value::several: return aLines->First != aLines->Last;
			}
			return false;
		}

		auto variables_of(const prompt_template_scan& aScan) -> std::vector<prompt_variable>
		{
			auto found = std::vector<prompt_variable>{ };
			for (const auto& piece : aScan.Pieces) {
				if (piece.Variable) {
					found.push_back(*piece.Variable);
				}
			}
			return found;
		}

		/// The needs a macro implies, in order: its About, then the name's
		/// variables, then the template's.
		auto implied_needs(const prompt_macro& aMacro, const prompt_template_scan& aName, const prompt_template_scan& aTemplate) -> std::vector<prompt_unmet>
		{
			auto       needs = std::vector<prompt_unmet>{ };
			const auto add   = [&needs](const std::optional<prompt_unmet> aNeed) {
				if (aNeed && std::ranges::find(needs, *aNeed) == needs.end()) {
					needs.push_back(*aNeed);
				}
			};
			add(subject_need(aMacro.Body.Subject));
			for (const auto variable : variables_of(aName)) {
				add(variable_need(variable));
			}
			for (const auto variable : variables_of(aTemplate)) {
				add(variable_need(variable));
			}
			return needs;
		}

		auto starts_word(const std::string_view aText, const std::size_t aIndex) noexcept -> bool { return aIndex == 0U || !is_alnum(aText[aIndex - 1U]); }

		/// Whether `aText` holds a well-known credential prefix at the start
		/// of a word.
		auto holds_credential_prefix(const std::string_view aText) -> bool
		{
			constexpr auto prefixes = std::array<std::string_view, 4>{ "ghp_", "github_pat_", "sk-", "-----BEGIN" };
			return std::ranges::any_of(prefixes, [aText](const std::string_view aPrefix) {
				for (auto at = aText.find(aPrefix); at != std::string_view::npos; at = aText.find(aPrefix, at + 1U)) {
					if (starts_word(aText, at)) {
						return true;
					}
				}
				return false;
			});
		}

		/// The states a macro can show in, narrowed by each condition and
		/// implied need; an empty state means it can never show.
		class reachable_states
		{
		public:
			/// \returns False once some fact has no state left.
			auto narrow(const prompt_condition& aCondition) -> bool
			{
				switch (aCondition.Fact) {
				case prompt_fact::file_open:      keep(File, aCondition.Negated ? no : yes); break;
				case prompt_fact::path:           keep(File, yes); break;
				case prompt_fact::selected_lines: {
					auto allowed = lines_states(aCondition.Lines);
					if (aCondition.Negated) {
						allowed = static_cast<std::uint8_t>(~allowed & all_lines);
					}
					Lines &= allowed;
					break;
				}
				case prompt_fact::draft:
				case prompt_fact::author_is_me:
				case prompt_fact::checks_failing:
				case prompt_fact::takes_replies:
				case prompt_fact::empty_chat:     keep(Flags[static_cast<std::size_t>(aCondition.Fact)], aCondition.Negated ? no : yes); break;
				case prompt_fact::repository:     break;
				}
				return possible();
			}

			auto narrow(const prompt_unmet aNeed) -> bool
			{
				if (aNeed == prompt_unmet::needs_file) {
					keep(File, yes);
				} else if (aNeed == prompt_unmet::needs_lines) {
					Lines &= static_cast<std::uint8_t>(one_line | several_lines);
				}
				return possible();
			}

		private:
			static constexpr std::uint8_t no            = 1U;
			static constexpr std::uint8_t yes           = 2U;
			static constexpr std::uint8_t no_lines      = 1U;
			static constexpr std::uint8_t one_line      = 2U;
			static constexpr std::uint8_t several_lines = 4U;
			static constexpr std::uint8_t all_lines     = no_lines | one_line | several_lines;

			static auto lines_states(const prompt_lines_value aValue) noexcept -> std::uint8_t
			{
				switch (aValue) {
				case prompt_lines_value::any:     return one_line | several_lines;
				case prompt_lines_value::one:     return one_line;
				case prompt_lines_value::several: return several_lines;
				}
				return 0U;
			}

			static auto keep(std::uint8_t& aState, const std::uint8_t aValue) noexcept -> void { aState &= aValue; }

			[[nodiscard]] auto possible() const noexcept -> bool
			{
				return File != 0U && Lines != 0U && std::ranges::all_of(Flags, [](const std::uint8_t aState) { return aState != 0U; });
			}

			std::uint8_t                                File  = no | yes;
			std::uint8_t                                Lines = all_lines;
			std::array<std::uint8_t, prompt_fact_count> Flags = [] {
				auto flags = std::array<std::uint8_t, prompt_fact_count>{ };
				flags.fill(no | yes);
				return flags;
			}();
		};

		auto problem(const prompt_problem aProblem) -> prompt_macro_problem { return { .Problem = aProblem }; }

		auto append_refusals(std::vector<prompt_macro_problem>& aProblems, const prompt_template_scan& aScan, const bool aInName) -> void
		{
			for (const auto& refusal : aScan.Refusals) {
				aProblems.push_back({
					.Problem = refusal.Reason == prompt_template_refusal_reason::unknown_variable ? prompt_problem::unknown_variable : prompt_problem::other_place_variable,
					.InName  = aInName,
					.Offset  = refusal.Offset,
					.Length  = refusal.Length,
					.Detail  = refusal.Name,
				});
			}
		}

		auto validate_condition(std::vector<prompt_macro_problem>& aProblems, const prompt_place aPlace, const prompt_condition& aCondition, const std::size_t aIndex) -> void
		{
			if (!prompt_fact_allowed(aPlace, aCondition.Fact)) {
				aProblems.push_back({ .Problem = prompt_problem::fact_not_for_place, .Condition = aIndex });
			}
			if (aCondition.Fact != prompt_fact::path && aCondition.Fact != prompt_fact::repository) {
				return;
			}
			if (aCondition.Patterns.empty()) {
				aProblems.push_back({ .Problem = prompt_problem::no_patterns, .Condition = aIndex });
			}
			if (aCondition.Patterns.size() > maximum_prompt_patterns) {
				aProblems.push_back({ .Problem = prompt_problem::too_many_patterns, .Condition = aIndex, .Excess = aCondition.Patterns.size() - maximum_prompt_patterns });
			}
			for (const auto& pattern : aCondition.Patterns) {
				if (pattern.size() > maximum_prompt_pattern_bytes) {
					aProblems.push_back({ .Problem = prompt_problem::pattern_too_long, .Condition = aIndex, .Excess = pattern.size() - maximum_prompt_pattern_bytes, .Detail = pattern });
				} else if (pattern.empty() || !wildcard_pattern_valid(pattern)) {
					aProblems.push_back({ .Problem = prompt_problem::bad_pattern, .Condition = aIndex, .Detail = pattern });
				}
			}
		}

		/// The first condition or implied need that makes the macro
		/// unreachable, if any.
		auto contradiction(const prompt_macro& aMacro, const std::vector<prompt_unmet>& aNeeds) -> std::optional<std::size_t>
		{
			auto states = reachable_states{ };
			for (const auto need : aNeeds) {
				states.narrow(need);
			}
			for (auto index = std::size_t{ 0 }; index < aMacro.Body.Conditions.size(); ++index) {
				if (!states.narrow(aMacro.Body.Conditions[index])) {
					return index;
				}
			}
			return std::nullopt;
		}

		auto offer_of(const prompt_macro& aMacro, std::string aLabel, prompt_expansion aExpansion) -> prompt_offer
		{
			return {
				.Id        = aMacro.Id,
				.Label     = std::move(aLabel),
				.Expansion = std::move(aExpansion),
				.Place     = aMacro.Place,
				.Subject   = aMacro.Body.Subject,
				.Kind      = aMacro.Body.Kind,
			};
		}
	} // namespace

	auto stable_name(const prompt_place aPlace) noexcept -> std::string_view
	{
		switch (aPlace) {
		case prompt_place::agent_tab:    return "agent-tab";
		case prompt_place::conversation: return "conversation";
		case prompt_place::tour_stop:    return "tour-stop";
		case prompt_place::tour_part:    return "tour-part";
		case prompt_place::tour_whole:   return "tour";
		}
		return "unknown";
	}

	auto stable_name(const prompt_subject aSubject) noexcept -> std::string_view
	{
		switch (aSubject) {
		case prompt_subject::pull_request:   return "pull-request";
		case prompt_subject::open_file:      return "open-file";
		case prompt_subject::selected_lines: return "selected-lines";
		case prompt_subject::conversation:   return "conversation";
		case prompt_subject::stop:           return "stop";
		case prompt_subject::part:           return "part";
		case prompt_subject::selected_text:  return "selected-text";
		case prompt_subject::tour:           return "tour";
		}
		return "unknown";
	}

	auto stable_name(const prompt_ask aKind) noexcept -> std::string_view
	{
		switch (aKind) {
		case prompt_ask::question: return "question";
		case prompt_ask::review:   return "review";
		}
		return "unknown";
	}

	auto prompt_subject_allowed(const prompt_place aPlace, const prompt_subject aSubject) noexcept -> bool
	{
		switch (aPlace) {
		case prompt_place::agent_tab:    return aSubject == prompt_subject::pull_request || aSubject == prompt_subject::open_file || aSubject == prompt_subject::selected_lines;
		case prompt_place::conversation: return aSubject == prompt_subject::conversation;
		case prompt_place::tour_stop:    return aSubject == prompt_subject::stop || aSubject == prompt_subject::selected_text;
		case prompt_place::tour_part:    return aSubject == prompt_subject::part || aSubject == prompt_subject::selected_text;
		case prompt_place::tour_whole:   return aSubject == prompt_subject::tour;
		}
		return false;
	}

	auto prompt_ask_allowed(const prompt_place aPlace, const prompt_subject aSubject, const prompt_ask aKind) noexcept -> bool
	{
		if (!prompt_subject_allowed(aPlace, aSubject)) {
			return false;
		}
		return aKind == prompt_ask::question || (aPlace == prompt_place::agent_tab && (aSubject == prompt_subject::pull_request || aSubject == prompt_subject::open_file));
	}

	auto stable_name(const prompt_fact aFact) noexcept -> std::string_view
	{
		switch (aFact) {
		case prompt_fact::file_open:      return "file-open";
		case prompt_fact::selected_lines: return "selected-lines";
		case prompt_fact::path:           return "path";
		case prompt_fact::draft:          return "draft";
		case prompt_fact::author_is_me:   return "author-is-me";
		case prompt_fact::checks_failing: return "checks-failing";
		case prompt_fact::repository:     return "repository";
		case prompt_fact::takes_replies:  return "takes-replies";
		case prompt_fact::empty_chat:     return "empty-chat";
		}
		return "unknown";
	}

	auto prompt_fact_allowed(const prompt_place aPlace, const prompt_fact aFact) noexcept -> bool
	{
		switch (aFact) {
		case prompt_fact::file_open:
		case prompt_fact::selected_lines:
		case prompt_fact::path:           return aPlace == prompt_place::agent_tab;
		case prompt_fact::draft:
		case prompt_fact::author_is_me:
		case prompt_fact::checks_failing:
		case prompt_fact::repository:     return true;
		case prompt_fact::takes_replies:  return aPlace == prompt_place::conversation;
		case prompt_fact::empty_chat:     return aPlace == prompt_place::agent_tab || is_tour(aPlace);
		}
		return false;
	}

	auto stable_name(const prompt_variable aVariable) noexcept -> std::string_view
	{
		const auto index = static_cast<std::size_t>(aVariable);
		return index < variable_names.size() ? variable_names[index] : std::string_view{ "unknown" };
	}

	auto find_prompt_variable(const std::string_view aName) noexcept -> std::optional<prompt_variable>
	{
		const auto found = std::ranges::find(variable_names, aName);
		if (found == variable_names.end()) {
			return std::nullopt;
		}
		return static_cast<prompt_variable>(found - variable_names.begin());
	}

	auto prompt_variable_allowed(const prompt_place aPlace, const prompt_variable aVariable) noexcept -> bool
	{
		switch (aVariable) {
		case prompt_variable::pr_number:
		case prompt_variable::pr_title:
		case prompt_variable::pr_url:
		case prompt_variable::pr_author:
		case prompt_variable::pr_base:
		case prompt_variable::pr_head:
		case prompt_variable::repo:
		case prompt_variable::cursor:            return true;
		case prompt_variable::file:
		case prompt_variable::file_name:
		case prompt_variable::lines:
		case prompt_variable::lines_first:
		case prompt_variable::lines_last:        return aPlace == prompt_place::agent_tab;
		case prompt_variable::conversation_file: return aPlace == prompt_place::conversation;
		case prompt_variable::stop_number:
		case prompt_variable::stop_title:
		case prompt_variable::part:              return is_tour(aPlace);
		}
		return false;
	}

	auto prompt_variables(const prompt_place aPlace) -> std::vector<prompt_variable>
	{
		auto variables = std::vector<prompt_variable>{ };
		for (auto index = std::size_t{ 0 }; index < prompt_variable_count; ++index) {
			if (const auto variable = static_cast<prompt_variable>(index); prompt_variable_allowed(aPlace, variable)) {
				variables.push_back(variable);
			}
		}
		return variables;
	}

	auto scan_prompt_template(const std::string_view aText, const prompt_place aPlace) -> prompt_template_scan
	{
		auto       scan    = prompt_template_scan{ };
		auto       literal = std::string{ };
		const auto flush   = [&] {
			if (!literal.empty()) {
				scan.Pieces.push_back({ .Literal = std::move(literal) });
				literal.clear();
			}
		};
		auto index = std::size_t{ 0 };
		while (index < aText.size()) {
			if (aText[index] != '{') {
				literal.push_back(aText[index++]);
				continue;
			}
			if (index + 1U < aText.size() && aText[index + 1U] == '{') {
				literal.push_back('{');
				index += 2U;
				continue;
			}
			auto close = index + 1U;
			if (close < aText.size() && is_lower(aText[close])) {
				while (close < aText.size() && is_name_byte(aText[close])) {
					++close;
				}
			}
			if (close == index + 1U || close >= aText.size() || aText[close] != '}') {
				literal.push_back(aText[index++]);
				continue;
			}
			const auto name     = aText.substr(index + 1U, close - index - 1U);
			const auto whole    = aText.substr(index, close - index + 1U);
			const auto variable = find_prompt_variable(name);
			if (variable && prompt_variable_allowed(aPlace, *variable)) {
				flush();
				scan.Pieces.push_back({ .Variable = variable });
			} else {
				scan.Refusals.push_back({
					.Reason = variable ? prompt_template_refusal_reason::other_place_variable : prompt_template_refusal_reason::unknown_variable,
					.Offset = index,
					.Length = whole.size(),
					.Name   = std::string{ name },
				});
				literal += whole;
			}
			index = close + 1U;
		}
		flush();
		return scan;
	}

	auto expand_prompt_template(const prompt_template_scan& aScan, const prompt_facts& aFacts) -> std::optional<prompt_expansion>
	{
		if (!aScan.Refusals.empty()) {
			return std::nullopt;
		}
		auto expansion = prompt_expansion{ };
		for (const auto& piece : aScan.Pieces) {
			if (!piece.Variable) {
				expansion.Text += piece.Literal;
				continue;
			}
			if (*piece.Variable == prompt_variable::cursor) {
				if (!expansion.Cursor) {
					expansion.Cursor = expansion.Text.size();
				}
				continue;
			}
			const auto value = variable_value(*piece.Variable, aFacts);
			if (!value) {
				return std::nullopt;
			}
			const auto begin = expansion.Text.size();
			append_one_line(expansion.Text, *value);
			expansion.Values.push_back({ .Begin = begin, .End = expansion.Text.size() });
		}
		return expansion;
	}

	auto prompt_skill_text(const prompt_expansion& aExpansion) -> std::string
	{
		// A value touches a reference when it overlaps it, or ends where it
		// starts or starts where it ends: then the slash starts a word, or
		// the name ends, only because of the value. An empty value counts,
		// since its absence may join a reference.
		const auto touchesValue = [&aExpansion](const skill_reference& aReference) {
			return std::ranges::any_of(aExpansion.Values, [&aReference](const prompt_value_span& aValue) { return aValue.Begin <= aReference.End && aReference.Begin <= aValue.End; });
		};
		auto text = std::string{ };
		for (const auto& reference : find_skill_references(aExpansion.Text)) {
			if (touchesValue(reference)) {
				continue;
			}
			if (!text.empty()) {
				text.push_back(' ');
			}
			text.append(aExpansion.Text, reference.Begin, reference.End - reference.Begin);
		}
		return text;
	}

	auto suggested_subject_of(const prompt_subject aSubject) noexcept -> std::optional<agent_suggested_subject>
	{
		switch (aSubject) {
		case prompt_subject::pull_request:   return agent_suggested_subject::pull_request;
		case prompt_subject::open_file:      return agent_suggested_subject::open_file;
		case prompt_subject::selected_lines: return agent_suggested_subject::selected_lines;
		case prompt_subject::conversation:
		case prompt_subject::stop:
		case prompt_subject::part:
		case prompt_subject::selected_text:
		case prompt_subject::tour:           break;
		}
		return std::nullopt;
	}

	auto valid(const prompt_turn_name& aName) noexcept -> bool
	{
		return !aName.Asked.empty() && !aName.Name.empty() && aName.Asked.size() <= maximum_prompt_expansion_bytes && aName.Name.size() <= maximum_prompt_turn_name_bytes && aName.Asked != aName.Name;
	}

	auto prompt_turn_names::remember(prompt_turn_name aName) -> bool
	{
		if (!valid(aName)) {
			return false;
		}
		if (!Names.empty() && Names.back() == aName) {
			return false;
		}
		std::erase_if(Names, [&aName](const prompt_turn_name& aKept) { return aKept.Asked == aName.Asked; });
		Names.push_back(std::move(aName));
		if (Names.size() > maximum_prompt_turn_names) {
			Names.erase(Names.begin(), Names.begin() + static_cast<std::ptrdiff_t>(Names.size() - maximum_prompt_turn_names));
		}
		return true;
	}

	void prompt_turn_names::adopt(const std::vector<prompt_turn_name>& aNames)
	{
		auto adopted = std::vector<prompt_turn_name>{ };
		for (const auto& name : aNames) {
			const auto newer = std::ranges::any_of(Names, [&name](const prompt_turn_name& aKept) { return aKept.Asked == name.Asked; });
			if (!valid(name) || newer) {
				continue;
			}
			std::erase_if(adopted, [&name](const prompt_turn_name& aKept) { return aKept.Asked == name.Asked; });
			adopted.push_back(name);
		}
		adopted.insert(adopted.end(), std::make_move_iterator(Names.begin()), std::make_move_iterator(Names.end()));
		Names = std::move(adopted);
		if (Names.size() > maximum_prompt_turn_names) {
			Names.erase(Names.begin(), Names.begin() + static_cast<std::ptrdiff_t>(Names.size() - maximum_prompt_turn_names));
		}
	}

	auto prompt_turn_names::recall(const std::string_view aAsked) const -> std::optional<std::string>
	{
		const auto found = std::ranges::find(Names, aAsked, &prompt_turn_name::Asked);
		return found != Names.end() ? std::optional{ found->Name } : std::nullopt;
	}

	auto stable_name(const prompt_unmet aUnmet) noexcept -> std::string_view
	{
		switch (aUnmet) {
		case prompt_unmet::off:                     return "off";
		case prompt_unmet::invalid:                 return "invalid";
		case prompt_unmet::needs_file:              return "needs-file";
		case prompt_unmet::needs_lines:             return "needs-lines";
		case prompt_unmet::needs_conversation_file: return "needs-conversation-file";
		case prompt_unmet::needs_selected_text:     return "needs-selected-text";
		case prompt_unmet::needs_stop:              return "needs-stop";
		case prompt_unmet::needs_part:              return "needs-part";
		case prompt_unmet::needs_pull_request:      return "needs-pull-request";
		case prompt_unmet::needs_repository:        return "needs-repository";
		case prompt_unmet::condition:               return "condition";
		case prompt_unmet::expansion_too_long:      return "expansion-too-long";
		}
		return "unknown";
	}

	auto prompt_condition_holds(const prompt_condition& aCondition, const prompt_facts& aFacts) -> bool
	{
		switch (aCondition.Fact) {
		case prompt_fact::file_open:      return aFacts.OpenFile.has_value() != aCondition.Negated;
		case prompt_fact::selected_lines: return lines_match(aCondition.Lines, aFacts.Lines) != aCondition.Negated;
		case prompt_fact::path:           return aFacts.OpenFile && any_pattern_matches(aCondition.Patterns, *aFacts.OpenFile, false) != aCondition.Negated;
		case prompt_fact::repository:     return aFacts.Repository && any_pattern_matches(aCondition.Patterns, *aFacts.Repository, true) != aCondition.Negated;
		case prompt_fact::draft:          return known_is(aFacts.Draft, aCondition.Negated);
		case prompt_fact::author_is_me:   return known_is(aFacts.AuthorIsMe, aCondition.Negated);
		case prompt_fact::checks_failing: return known_is(aFacts.ChecksFailing, aCondition.Negated);
		case prompt_fact::takes_replies:  return known_is(aFacts.TakesReplies, aCondition.Negated);
		case prompt_fact::empty_chat:     return known_is(aFacts.ChatEmpty, aCondition.Negated);
		}
		return false;
	}

	auto evaluate_prompt_macro(const prompt_macro& aMacro, const prompt_facts& aFacts) -> prompt_evaluation
	{
		if (!aMacro.On) {
			return { .Unmet = prompt_unmet::off };
		}
		if (!prompt_macro_valid(aMacro)) {
			return { .Unmet = prompt_unmet::invalid };
		}
		const auto name = scan_prompt_template(aMacro.Body.Name, aMacro.Place);
		const auto body = scan_prompt_template(aMacro.Body.Template, aMacro.Place);
		for (const auto need : implied_needs(aMacro, name, body)) {
			if (!need_met(need, aMacro.Place, aFacts)) {
				return { .Unmet = need };
			}
		}
		for (auto index = std::size_t{ 0 }; index < aMacro.Body.Conditions.size(); ++index) {
			if (!prompt_condition_holds(aMacro.Body.Conditions[index], aFacts)) {
				return { .Unmet = prompt_unmet::condition, .Condition = index };
			}
		}
		auto label     = expand_prompt_template(name, aFacts);
		auto expansion = expand_prompt_template(body, aFacts);
		if (!label || !expansion) {
			// Every value a variable needs was checked above.
			return { .Unmet = prompt_unmet::invalid };
		}
		if (expansion->Text.size() > maximum_prompt_expansion_bytes) {
			return { .Unmet = prompt_unmet::expansion_too_long };
		}
		return { .Offer = offer_of(aMacro, std::move(label->Text), std::move(*expansion)) };
	}

	auto decide_prompt_offers(const std::span<const prompt_macro> aOrdered, const prompt_facts& aFacts) -> prompt_offers
	{
		auto       offers   = prompt_offers{ };
		auto       shown    = std::vector<prompt_offer>{ };
		const auto consider = [&](const prompt_macro& aMacro, const prompt_facts& aAgainst, const bool aElsewhere) {
			auto evaluation = evaluate_prompt_macro(aMacro, aAgainst);
			if (!evaluation.Offer) {
				offers.Withheld.push_back({ .Id = aMacro.Id, .Unmet = evaluation.Unmet, .Condition = evaluation.Condition });
				return;
			}
			if (aElsewhere) {
				evaluation.Offer->Label += " in " + aAgainst.Thread->Part;
			}
			shown.push_back(std::move(*evaluation.Offer));
		};
		// Text selected in a tour part: that part's place's macros about it
		// lead, with that part's names.
		const auto selectionPlace = aFacts.Selection ? std::optional{ aFacts.Selection->Place } : std::nullopt;
		if (aFacts.Selection) {
			auto against         = aFacts;
			against.Thread       = aFacts.Selection;
			const auto elsewhere = aFacts.Selection != aFacts.Thread;
			for (const auto& macro : aOrdered) {
				if (macro.Place == *selectionPlace && macro.Body.Subject == prompt_subject::selected_text) {
					consider(macro, against, elsewhere);
				}
			}
		}
		for (const auto& macro : aOrdered) {
			const auto takenAbove = macro.Body.Subject == prompt_subject::selected_text && selectionPlace == macro.Place;
			if (macro.Place == aFacts.Place && !takenAbove) {
				consider(macro, aFacts, false);
			}
		}
		const auto chips = std::min(shown.size(), maximum_prompt_chips);
		offers.Chips.assign(std::make_move_iterator(shown.begin()), std::make_move_iterator(shown.begin() + static_cast<std::ptrdiff_t>(chips)));
		offers.More.assign(std::make_move_iterator(shown.begin() + static_cast<std::ptrdiff_t>(chips)), std::make_move_iterator(shown.end()));
		return offers;
	}

	auto stable_name(const prompt_problem aProblem) noexcept -> std::string_view
	{
		switch (aProblem) {
		case prompt_problem::no_name:               return "no-name";
		case prompt_problem::name_too_long:         return "name-too-long";
		case prompt_problem::name_not_one_line:     return "name-not-one-line";
		case prompt_problem::no_template:           return "no-template";
		case prompt_problem::template_too_long:     return "template-too-long";
		case prompt_problem::unknown_variable:      return "unknown-variable";
		case prompt_problem::other_place_variable:  return "other-place-variable";
		case prompt_problem::subject_not_for_place: return "subject-not-for-place";
		case prompt_problem::kind_not_for_subject:  return "kind-not-for-subject";
		case prompt_problem::too_many_conditions:   return "too-many-conditions";
		case prompt_problem::fact_not_for_place:    return "fact-not-for-place";
		case prompt_problem::no_patterns:           return "no-patterns";
		case prompt_problem::too_many_patterns:     return "too-many-patterns";
		case prompt_problem::pattern_too_long:      return "pattern-too-long";
		case prompt_problem::bad_pattern:           return "bad-pattern";
		case prompt_problem::conditions_contradict: return "conditions-contradict";
		case prompt_problem::duplicate_name:        return "duplicate-name";
		case prompt_problem::credential_prefix:     return "credential-prefix";
		}
		return "unknown";
	}

	auto prompt_problem_blocks(const prompt_problem aProblem) noexcept -> bool
	{
		return aProblem != prompt_problem::conditions_contradict && aProblem != prompt_problem::duplicate_name && aProblem != prompt_problem::credential_prefix;
	}

	auto validate_prompt_macro(const prompt_macro& aMacro, const std::span<const prompt_macro> aSiblings) -> std::vector<prompt_macro_problem>
	{
		auto        problems = std::vector<prompt_macro_problem>{ };
		const auto& body     = aMacro.Body;
		if (blank(body.Name)) {
			problems.push_back(problem(prompt_problem::no_name));
		} else if (has_control(body.Name)) {
			problems.push_back(problem(prompt_problem::name_not_one_line));
		}
		if (body.Name.size() > maximum_prompt_name_bytes) {
			problems.push_back({ .Problem = prompt_problem::name_too_long, .Excess = body.Name.size() - maximum_prompt_name_bytes });
		}
		const auto name = scan_prompt_template(body.Name, aMacro.Place);
		append_refusals(problems, name, true);
		if (blank(body.Template)) {
			problems.push_back(problem(prompt_problem::no_template));
		}
		if (body.Template.size() > maximum_prompt_template_bytes) {
			problems.push_back({ .Problem = prompt_problem::template_too_long, .Excess = body.Template.size() - maximum_prompt_template_bytes });
		}
		const auto text = scan_prompt_template(body.Template, aMacro.Place);
		append_refusals(problems, text, false);
		if (!prompt_subject_allowed(aMacro.Place, body.Subject)) {
			problems.push_back(problem(prompt_problem::subject_not_for_place));
		} else if (!prompt_ask_allowed(aMacro.Place, body.Subject, body.Kind)) {
			problems.push_back(problem(prompt_problem::kind_not_for_subject));
		}
		if (body.Conditions.size() > maximum_prompt_conditions) {
			problems.push_back({ .Problem = prompt_problem::too_many_conditions, .Excess = body.Conditions.size() - maximum_prompt_conditions });
		}
		for (auto index = std::size_t{ 0 }; index < body.Conditions.size(); ++index) {
			validate_condition(problems, aMacro.Place, body.Conditions[index], index);
		}
		if (const auto index = contradiction(aMacro, implied_needs(aMacro, name, text))) {
			problems.push_back({ .Problem = prompt_problem::conditions_contradict, .Condition = *index });
		}
		const auto duplicate = std::ranges::any_of(aSiblings, [&aMacro](const prompt_macro& aOther) {
			return aOther.Id != aMacro.Id && aOther.Place == aMacro.Place && aOther.Body.Name == aMacro.Body.Name;
		});
		if (duplicate && !blank(body.Name)) {
			problems.push_back(problem(prompt_problem::duplicate_name));
		}
		if (holds_credential_prefix(body.Name) || holds_credential_prefix(body.Template)) {
			problems.push_back(problem(prompt_problem::credential_prefix));
		}
		return problems;
	}

	auto prompt_macro_valid(const prompt_macro& aMacro) -> bool
	{
		return std::ranges::none_of(validate_prompt_macro(aMacro), [](const prompt_macro_problem& aProblem) { return prompt_problem_blocks(aProblem.Problem); });
	}

	auto parse_prompt_patterns(const std::string_view aText) -> std::vector<std::string>
	{
		auto patterns = std::vector<std::string>{ };
		auto start    = std::size_t{ 0 };
		while (start <= aText.size()) {
			const auto comma = std::min(aText.find(',', start), aText.size());
			auto       entry = aText.substr(start, comma - start);
			while (!entry.empty() && (entry.front() == ' ' || entry.front() == '\t')) {
				entry.remove_prefix(1U);
			}
			while (!entry.empty() && (entry.back() == ' ' || entry.back() == '\t')) {
				entry.remove_suffix(1U);
			}
			if (!entry.empty()) {
				patterns.emplace_back(entry);
			}
			start = comma + 1U;
		}
		return patterns;
	}

	auto prompt_macro_set::macros() const -> std::vector<prompt_macro>
	{
		auto result = std::vector<prompt_macro>{ };
		result.reserve(Entries.size());
		for (const auto& entry : Entries) {
			result.push_back(entry.Macro);
		}
		return result;
	}

	namespace {
		/// One place's entries in the stored order; built-ins the order does
		/// not name follow the place's last built-in, own macros go last.
		auto order_place(std::vector<prompt_set_entry> aPool, const prompt_place_order* aOrder) -> std::vector<prompt_set_entry>
		{
			auto       placed = std::vector<prompt_set_entry>{ };
			auto       taken  = std::vector<bool>(aPool.size(), false);
			const auto take   = [&](const std::size_t aIndex) -> prompt_set_entry& {
				taken[aIndex] = true;
				return aPool[aIndex];
			};
			if (aOrder != nullptr) {
				for (const auto& id : aOrder->Ids) {
					const auto found = std::ranges::find(aPool, id, [](const prompt_set_entry& aEntry) -> const std::string& { return aEntry.Macro.Id; });
					if (found != aPool.end() && !taken[static_cast<std::size_t>(found - aPool.begin())]) {
						placed.push_back(std::move(take(static_cast<std::size_t>(found - aPool.begin()))));
					}
				}
			}
			for (auto index = std::size_t{ 0 }; index < aPool.size(); ++index) {
				if (taken[index] || !aPool[index].Macro.BuiltIn) {
					continue;
				}
				const auto lastBuiltIn = std::find_if(placed.rbegin(), placed.rend(), [](const prompt_set_entry& aEntry) { return aEntry.Macro.BuiltIn; });
				placed.insert(lastBuiltIn.base(), std::move(take(index)));
			}
			for (auto index = std::size_t{ 0 }; index < aPool.size(); ++index) {
				if (!taken[index]) {
					placed.push_back(std::move(take(index)));
				}
			}
			return placed;
		}

		auto apply_change(const prompt_builtin& aShipped, const prompt_builtin_change& aChange) -> prompt_set_entry
		{
			auto entry = prompt_set_entry{ .Macro = aShipped.Macro };
			if (aChange.Body) {
				entry.Macro.Body = *aChange.Body;
				entry.Changed    = *aChange.Body != aShipped.Macro.Body;
				entry.Outdated   = entry.Changed && aChange.WrittenAgainst < aShipped.Revision;
			}
			if (aChange.On) {
				entry.Macro.On = *aChange.On;
			}
			return entry;
		}
	} // namespace

	auto merge_prompt_macros(const std::span<const prompt_builtin> aShipped, const prompt_macro_differences& aStored) -> prompt_macro_set
	{
		auto       set     = prompt_macro_set{ };
		auto       entries = std::vector<prompt_set_entry>{ };
		auto       ids     = std::vector<std::string>{ };
		const auto known   = [&ids](const std::string& aId) { return std::ranges::find(ids, aId) != ids.end(); };
		for (const auto& builtin : aShipped) {
			const auto change = std::ranges::find(aStored.Changes, builtin.Macro.Id, &prompt_builtin_change::Id);
			entries.push_back(change == aStored.Changes.end() ? prompt_set_entry{ .Macro = builtin.Macro } : apply_change(builtin, *change));
			entries.back().Macro.BuiltIn = true;
			ids.push_back(builtin.Macro.Id);
		}
		for (auto index = std::size_t{ 0 }; index < aStored.Changes.size(); ++index) {
			const auto& id    = aStored.Changes[index].Id;
			const auto  first = std::ranges::find(aStored.Changes, id, &prompt_builtin_change::Id);
			if (!known(id) || static_cast<std::size_t>(first - aStored.Changes.begin()) != index) {
				set.Dropped.push_back(id);
			}
		}
		for (const auto& own : aStored.Own) {
			if (own.BuiltIn || own.Id.empty() || known(own.Id) || entries.size() >= maximum_prompt_macros) {
				set.Dropped.push_back(own.Id);
				continue;
			}
			entries.push_back({ .Macro = own });
			ids.push_back(own.Id);
		}
		for (auto place = std::size_t{ 0 }; place < prompt_place_count; ++place) {
			const auto here = static_cast<prompt_place>(place);
			auto       pool = std::vector<prompt_set_entry>{ };
			for (const auto& entry : entries) {
				if (entry.Macro.Place == here) {
					pool.push_back(entry);
				}
			}
			const auto order = std::ranges::find(aStored.Orders, here, &prompt_place_order::Place);
			for (auto& entry : order_place(std::move(pool), order == aStored.Orders.end() ? nullptr : &*order)) {
				set.Entries.push_back(std::move(entry));
			}
		}
		return set;
	}
} // namespace bpr::core
