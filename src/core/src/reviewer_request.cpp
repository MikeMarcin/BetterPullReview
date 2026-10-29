#include "bpr/core/reviewer_request.hpp"

#include "bpr/core/review_role.hpp"

#include <algorithm>
#include <utility>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto ascii_lower(const char aCharacter) noexcept -> char
		{
			return aCharacter >= 'A' && aCharacter <= 'Z' ? static_cast<char>(aCharacter - 'A' + 'a') : aCharacter;
		}

		/// \brief Whether `aText` holds `aNeedle`, ASCII case folded.
		[[nodiscard]] auto contains_folded(const std::string_view aText, const std::string_view aNeedle) noexcept -> bool
		{
			if (aNeedle.empty()) {
				return true;
			}
			if (aNeedle.size() > aText.size()) {
				return false;
			}
			for (std::size_t start = 0; start + aNeedle.size() <= aText.size(); ++start) {
				auto match = true;
				for (std::size_t index = 0; index < aNeedle.size(); ++index) {
					if (ascii_lower(aText[start + index]) != ascii_lower(aNeedle[index])) {
						match = false;
						break;
					}
				}
				if (match) {
					return true;
				}
			}
			return false;
		}

		[[nodiscard]] auto listed(const std::vector<std::string>& aLogins, const std::string_view aLogin) noexcept -> bool
		{
			return std::ranges::any_of(aLogins, [aLogin](const std::string& aListed) { return same_login(aListed, aLogin); });
		}

		[[nodiscard]] auto matches(const reviewer_candidate& aCandidate, const std::string_view aQuery) noexcept -> bool
		{
			return contains_folded(aCandidate.Login, aQuery) || contains_folded(aCandidate.Name, aQuery);
		}

		void present_rows(reviewer_picker_state& aState)
		{
			aState.Rows = aState.Answer ? reviewer_rows(*aState.Answer, aState.Query) : std::vector<reviewer_row>{ };
		}
	} // namespace

	auto name(const reviewer_standing aStanding) noexcept -> std::string_view
	{
		switch (aStanding) {
		case reviewer_standing::available: return "available";
		case reviewer_standing::requested: return "requested";
		case reviewer_standing::reviewed:  return "reviewed";
		}
		return "unknown";
	}

	auto name(const reviewer_choice_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case reviewer_choice_refusal::none:              return "none";
		case reviewer_choice_refusal::closed:            return "closed";
		case reviewer_choice_refusal::busy:              return "busy";
		case reviewer_choice_refusal::unknown:           return "unknown";
		case reviewer_choice_refusal::already_requested: return "already_requested";
		}
		return "unknown";
	}

	auto name(const reviewer_request_failure aFailure) noexcept -> std::string_view
	{
		switch (aFailure) {
		case reviewer_request_failure::connection:     return "connection";
		case reviewer_request_failure::refused:        return "refused";
		case reviewer_request_failure::target_changed: return "target_changed";
		case reviewer_request_failure::cancelled:      return "cancelled";
		}
		return "unknown";
	}

	auto name(const reviewer_notice aNotice) noexcept -> std::string_view
	{
		switch (aNotice) {
		case reviewer_notice::none:              return "none";
		case reviewer_notice::requested:         return "requested";
		case reviewer_notice::rerequested:       return "rerequested";
		case reviewer_notice::already_requested: return "already_requested";
		case reviewer_notice::not_added:         return "not_added";
		case reviewer_notice::failed:            return "failed";
		case reviewer_notice::busy:              return "busy";
		}
		return "unknown";
	}

	auto reviewer_rows(const reviewer_search_answer& aAnswer, const std::string_view aQuery) -> std::vector<reviewer_row>
	{
		auto rows = std::vector<reviewer_row>{ };
		auto add  = [&](const reviewer_candidate& aCandidate, const bool aSuggested) {
			if (rows.size() >= maximum_reviewer_rows || aCandidate.Login.empty() || aCandidate.NodeId.empty()
				|| same_login(aCandidate.Login, aAnswer.Author) || !matches(aCandidate, aQuery)) {
				return;
			}
			const auto known = std::ranges::find_if(rows, [&](const reviewer_row& aRow) { return same_login(aRow.Candidate.Login, aCandidate.Login); });
			if (known != rows.end()) {
				known->Candidate.Suggested = known->Candidate.Suggested || aSuggested;
				known->Candidate.Commenter = known->Candidate.Commenter || aCandidate.Commenter;
				return;
			}
			auto row                = reviewer_row{ .Candidate = aCandidate };
			row.Candidate.Suggested = aSuggested;
			row.Standing            = listed(aAnswer.Requested, aCandidate.Login) ? reviewer_standing::requested
				: listed(aAnswer.Reviewed, aCandidate.Login)                      ? reviewer_standing::reviewed
																				  : reviewer_standing::available;
			rows.push_back(std::move(row));
		};
		for (const auto& candidate : aAnswer.Suggested) {
			add(candidate, true);
		}
		for (const auto& candidate : aAnswer.Matches) {
			add(candidate, false);
		}
		return rows;
	}

	auto normalize_reviewer_query(const std::string_view aText) -> std::string
	{
		constexpr auto spaces = std::string_view{ " \t\r\n\f\v" };
		auto           text   = aText;
		const auto     first  = text.find_first_not_of(spaces);
		if (first == std::string_view::npos) {
			return { };
		}
		text = text.substr(first, text.find_last_not_of(spaces) - first + 1U);
		if (text.starts_with('@')) {
			text.remove_prefix(1U);
		}
		if (text.size() > maximum_reviewer_query_bytes) {
			auto end = maximum_reviewer_query_bytes;
			// Back off continuation bytes so a character is never split.
			while (end > 0U && (static_cast<unsigned char>(text[end]) & 0xC0U) == 0x80U) {
				--end;
			}
			text = text.substr(0, end);
		}
		return std::string{ text };
	}

	auto open_reviewer_picker(reviewer_picker_state aState) -> reviewer_picker_transition
	{
		if (aState.Open) {
			return { .State = std::move(aState), .Effect = std::monostate{ } };
		}
		aState.Open = true;
		aState.Query.clear();
		aState.Rows.clear();
		aState.Answer.reset();
		aState.SearchFailure.reset();
		aState.Searching = true;
		++aState.Search;
		if (!aState.Pending) {
			aState.Notice = reviewer_notice::none;
			aState.NoticeLogin.clear();
			aState.NoticeFailure.reset();
		}
		auto effect = reviewer_search_effect{ .Search = aState.Search, .Query = { } };
		return { .State = std::move(aState), .Effect = std::move(effect) };
	}

	auto type_reviewer_query(reviewer_picker_state aState, const std::string_view aText) -> reviewer_picker_transition
	{
		auto query = normalize_reviewer_query(aText);
		if (!aState.Open || query == aState.Query) {
			return { .State = std::move(aState), .Effect = std::monostate{ } };
		}
		aState.Query = std::move(query);
		aState.SearchFailure.reset();
		aState.Searching = true;
		++aState.Search;
		// What is known already narrows at once; the answer completes it.
		present_rows(aState);
		auto effect = reviewer_search_effect{ .Search = aState.Search, .Query = aState.Query };
		return { .State = std::move(aState), .Effect = std::move(effect) };
	}

	auto accept_reviewer_search(reviewer_picker_state aState, const std::uint64_t aSearch, reviewer_search_answer aAnswer) -> reviewer_picker_state
	{
		if (!aState.Open || aSearch != aState.Search) {
			return aState;
		}
		aState.Searching = false;
		aState.SearchFailure.reset();
		aState.Answer = std::move(aAnswer);
		present_rows(aState);
		return aState;
	}

	auto fail_reviewer_search(reviewer_picker_state aState, const std::uint64_t aSearch, const reviewer_request_failure aFailure) -> reviewer_picker_state
	{
		if (!aState.Open || aSearch != aState.Search) {
			return aState;
		}
		aState.Searching     = false;
		aState.SearchFailure = aFailure;
		return aState;
	}

	auto choose_reviewer(reviewer_picker_state aState, const std::string_view aLogin) -> reviewer_picker_transition
	{
		const auto refuse = [&](const reviewer_choice_refusal aRefusal) {
			return reviewer_picker_transition{ .State = std::move(aState), .Effect = std::monostate{ }, .Refusal = aRefusal };
		};
		if (!aState.Open) {
			return refuse(reviewer_choice_refusal::closed);
		}
		if (aState.Pending) {
			aState.Notice = reviewer_notice::busy;
			return refuse(reviewer_choice_refusal::busy);
		}
		const auto row = std::ranges::find_if(aState.Rows, [aLogin](const reviewer_row& aRow) { return same_login(aRow.Candidate.Login, aLogin); });
		if (row == aState.Rows.end()) {
			return refuse(reviewer_choice_refusal::unknown);
		}
		if (row->Standing == reviewer_standing::requested) {
			aState.Notice      = reviewer_notice::already_requested;
			aState.NoticeLogin = row->Candidate.Login;
			aState.NoticeFailure.reset();
			return refuse(reviewer_choice_refusal::already_requested);
		}
		++aState.Request;
		aState.Pending     = row->Candidate.Login;
		aState.Notice      = reviewer_notice::none;
		aState.NoticeLogin = row->Candidate.Login;
		aState.NoticeFailure.reset();
		auto effect = reviewer_request_effect{
			.Request   = aState.Request,
			.NodeId    = row->Candidate.NodeId,
			.Login     = row->Candidate.Login,
			.Rerequest = row->Standing == reviewer_standing::reviewed,
		};
		return { .State = std::move(aState), .Effect = std::move(effect) };
	}

	auto finish_reviewer_request(reviewer_picker_state aState, const std::uint64_t aRequest, const reviewer_request_answer& aAnswer) -> reviewer_picker_state
	{
		if (aRequest != aState.Request || !aState.Pending) {
			return aState;
		}
		const auto login = *aState.Pending;
		aState.Pending.reset();
		aState.NoticeLogin = login;
		aState.NoticeFailure.reset();
		const auto row     = std::ranges::find_if(aState.Rows, [&](const reviewer_row& aRow) { return same_login(aRow.Candidate.Login, login); });
		const auto asked   = row != aState.Rows.end() && row->Standing == reviewer_standing::reviewed;
		const auto granted = listed(aAnswer.Requested, login);
		aState.Notice      = !granted ? reviewer_notice::not_added
			: !aAnswer.Written        ? reviewer_notice::already_requested
			: asked                   ? reviewer_notice::rerequested
									  : reviewer_notice::requested;
		if (aState.Answer) {
			aState.Answer->Requested = aAnswer.Requested;
			present_rows(aState);
		}
		return aState;
	}

	auto fail_reviewer_request(reviewer_picker_state aState, const std::uint64_t aRequest, const reviewer_request_failure aFailure) -> reviewer_picker_state
	{
		if (aRequest != aState.Request || !aState.Pending) {
			return aState;
		}
		aState.NoticeLogin = *aState.Pending;
		aState.Pending.reset();
		aState.Notice        = reviewer_notice::failed;
		aState.NoticeFailure = aFailure;
		return aState;
	}

	auto close_reviewer_picker(reviewer_picker_state aState) -> reviewer_picker_state
	{
		aState.Open      = false;
		aState.Searching = false;
		aState.SearchFailure.reset();
		aState.Query.clear();
		aState.Answer.reset();
		aState.Rows.clear();
		return aState;
	}

	auto decide_reviewer_write(const reviewer_search_answer& aObserved, const std::string_view aLogin) noexcept -> reviewer_write_decision
	{
		if (same_login(aObserved.Author, aLogin)) {
			return reviewer_write_decision::author;
		}
		return listed(aObserved.Requested, aLogin) ? reviewer_write_decision::already_requested : reviewer_write_decision::write;
	}
} // namespace bpr::core
