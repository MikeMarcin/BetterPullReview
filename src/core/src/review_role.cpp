#include "bpr/core/review_role.hpp"

#include <algorithm>
#include <cstddef>
#include <string_view>

namespace bpr::core {
	namespace {
		[[nodiscard]] constexpr auto lower(const char aCharacter) noexcept -> char
		{
			return aCharacter >= 'A' && aCharacter <= 'Z' ? static_cast<char>(aCharacter - 'A' + 'a') : aCharacter;
		}

		[[nodiscard]] constexpr auto alphanumeric(const char aCharacter) noexcept -> bool
		{
			return (aCharacter >= 'a' && aCharacter <= 'z') || (aCharacter >= 'A' && aCharacter <= 'Z') || (aCharacter >= '0' && aCharacter <= '9');
		}

		/// \brief Whether a line opens or closes a fenced code block: up to three
		/// spaces, then three or more backticks or tildes.
		[[nodiscard]] auto fence(std::string_view aLine) -> bool
		{
			auto indent = std::size_t{ 0U };
			while (indent < aLine.size() && indent < 3U && aLine[indent] == ' ') {
				++indent;
			}
			const auto rest = aLine.substr(indent);
			return rest.starts_with("```") || rest.starts_with("~~~");
		}

		/// \brief Searches one line outside its inline code spans.
		[[nodiscard]] auto line_mentions(const std::string_view aLine, const std::string_view aLogin) -> bool
		{
			auto index = std::size_t{ 0U };
			while (index < aLine.size()) {
				if (aLine[index] == '`') {
					// A code span closes at the next run of the same length; an
					// unclosed run is literal backticks.
					auto run = index;
					while (run < aLine.size() && aLine[run] == '`') {
						++run;
					}
					const auto ticks = aLine.substr(index, run - index);
					auto       close = aLine.find(ticks, run);
					while (close != std::string_view::npos && close + ticks.size() < aLine.size() && aLine[close + ticks.size()] == '`') {
						close = aLine.find(ticks, close + ticks.size() + 1U);
					}
					index = close == std::string_view::npos ? run : close + ticks.size();
					continue;
				}
				if (aLine[index] == '@') {
					const auto before = index == 0U ? ' ' : aLine[index - 1U];
					const auto start  = index + 1U;
					if (!alphanumeric(before) && before != '_' && before != '@' && start + aLogin.size() <= aLine.size()
						&& same_login(aLine.substr(start, aLogin.size()), aLogin)) {
						const auto after = start + aLogin.size() < aLine.size() ? aLine[start + aLogin.size()] : ' ';
						if (!alphanumeric(after) && after != '-' && after != '/') {
							return true;
						}
					}
				}
				++index;
			}
			return false;
		}
	} // namespace

	auto same_login(const std::string_view aLeft, const std::string_view aRight) noexcept -> bool
	{
		return std::ranges::equal(aLeft, aRight, [](const char aOne, const char aOther) { return lower(aOne) == lower(aOther); });
	}

	auto mentions_login(const std::string_view aMarkdown, const std::string_view aLogin) -> bool
	{
		if (aLogin.empty()) {
			return false;
		}
		auto fenced = false;
		auto start  = std::size_t{ 0U };
		while (start <= aMarkdown.size()) {
			const auto end  = aMarkdown.find('\n', start);
			auto       line = aMarkdown.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
			if (line.ends_with('\r')) {
				line.remove_suffix(1U);
			}
			if (fence(line)) {
				fenced = !fenced;
			} else if (!fenced && line_mentions(line, aLogin)) {
				return true;
			}
			if (end == std::string_view::npos) {
				break;
			}
			start = end + 1U;
		}
		return false;
	}

	auto decide_review_roles(const review_role_facts& aFacts) -> review_roles
	{
		auto result = review_roles{ .ViewerKnown = !aFacts.Viewer.empty() };
		if (!result.ViewerKnown) {
			return result;
		}
		auto& roles = result.Roles;
		if (same_login(aFacts.Viewer, aFacts.Author)) {
			roles.push_back({ .Kind = review_role_kind::author });
		}
		auto direct  = std::optional<review_role>{ };
		auto mine    = std::optional<review_role>{ };
		auto unknown = std::optional<review_role>{ };
		for (const auto& request : aFacts.Requests) {
			if (!request.Team) {
				if (same_login(request.Name, aFacts.Viewer)) {
					direct            = direct.value_or(review_role{ .Kind = review_role_kind::requested });
					direct->CodeOwner = direct->CodeOwner || request.CodeOwner;
				}
				continue;
			}
			if (request.Membership == team_membership::not_member) {
				continue;
			}
			auto& role = request.Membership == team_membership::member ? mine : unknown;
			role       = role.value_or(review_role{ .Kind = review_role_kind::team_requested, .Membership = request.Membership, .Reason = request.Reason });
			role->Teams.push_back(request.Name);
			role->CodeOwner = role->CodeOwner || request.CodeOwner;
		}
		if (direct) {
			roles.push_back(std::move(*direct));
		}
		// A team I am in is my request; only without one is a team whose
		// membership GitHub would not say named instead.
		if (mine) {
			mine->Reason = team_membership_reason::unavailable;
			roles.push_back(std::move(*mine));
		} else if (unknown) {
			roles.push_back(std::move(*unknown));
		}
		if (aFacts.LatestReview) {
			roles.push_back({ .Kind = review_role_kind::reviewed, .Review = aFacts.LatestReview });
		}
		if (aFacts.Mention) {
			roles.push_back({ .Kind = review_role_kind::mentioned, .Mention = aFacts.Mention });
		}
		return result;
	}

	auto decide_status_attention(const status_attention_facts& aFacts) -> std::vector<status_attention>
	{
		auto result = std::vector<status_attention>{ };
		if (aFacts.Conflicts) {
			result.push_back({ .Kind = status_attention_kind::conflicts, .Count = 1U });
		}
		if (!aFacts.FailedChecks.empty()) {
			result.push_back({ .Kind = status_attention_kind::failed_checks, .Count = aFacts.FailedChecks.size(), .Names = aFacts.FailedChecks });
		}
		auto waiting = std::vector<std::string>{ };
		for (const auto& name : aFacts.WaitingOn) {
			if (!name.empty() && !same_login(name, aFacts.Viewer)) {
				waiting.push_back(name);
			}
		}
		if (!waiting.empty()) {
			result.push_back({ .Kind = status_attention_kind::waiting_on, .Count = waiting.size(), .Names = std::move(waiting) });
		}
		if (aFacts.RepliesToMe != 0U) {
			result.push_back({ .Kind = status_attention_kind::replies_to_me, .Count = aFacts.RepliesToMe });
		}
		return result;
	}
} // namespace bpr::core
