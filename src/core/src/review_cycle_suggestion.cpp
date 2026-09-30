#include "bpr/core/review_cycle_suggestion.hpp"

#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <numeric>
#include <utility>

namespace bpr::core {
	namespace {
		auto text_of(const std::span<const std::uint8_t> aBytes) -> std::string_view
		{
			return { reinterpret_cast<const char*>(aBytes.data()), aBytes.size() };
		}
		auto bytes_of(const std::string_view aText) -> std::span<const std::uint8_t>
		{
			return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
		}
		auto digest_of(const std::span<const std::uint8_t> aBytes) -> cycle_material_digest
		{
			return cycle_material_digest{ sha256(aBytes) };
		}

		/// The range holds exactly the original bytes.
		auto range_matches(const cycle_suggestion& aSuggestion, const std::span<const std::uint8_t> aFile) -> bool
		{
			return aSuggestion.End <= aFile.size()
				&& text_of(aFile).substr(static_cast<std::size_t>(aSuggestion.Start), static_cast<std::size_t>(aSuggestion.End - aSuggestion.Start)) == aSuggestion.Original;
		}

		auto splice(const std::span<const std::uint8_t> aFile, const cycle_suggestion& aSuggestion) -> std::vector<std::uint8_t>
		{
			const auto start  = static_cast<std::size_t>(aSuggestion.Start);
			const auto end    = static_cast<std::size_t>(aSuggestion.End);
			auto       result = std::vector<std::uint8_t>{ };
			result.reserve(aFile.size() - (end - start) + aSuggestion.Replacement.size());
			result.insert(result.end(), aFile.begin(), aFile.begin() + static_cast<std::ptrdiff_t>(start));
			const auto replacement = bytes_of(aSuggestion.Replacement);
			result.insert(result.end(), replacement.begin(), replacement.end());
			result.insert(result.end(), aFile.begin() + static_cast<std::ptrdiff_t>(end), aFile.end());
			return result;
		}

		/// Conservative v1 conflict rule over original coordinates.
		auto conflicting(const cycle_suggestion& aLeft, const cycle_suggestion& aRight) -> bool
		{
			if (aLeft.insertion() && aRight.insertion()) {
				return aLeft.Start == aRight.Start;
			}
			if (aLeft.insertion()) {
				return aRight.Start <= aLeft.Start && aLeft.Start <= aRight.End;
			}
			if (aRight.insertion()) {
				return aLeft.Start <= aRight.Start && aRight.Start <= aLeft.End;
			}
			return aLeft.Start < aRight.End && aRight.Start < aLeft.End;
		}

		constexpr auto base64_alphabet = std::string_view{ "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/" };

		auto base64(const std::string_view aBytes) -> std::string
		{
			auto result = std::string{ };
			result.reserve((aBytes.size() + 2U) / 3U * 4U);
			for (std::size_t index = 0; index < aBytes.size(); index += 3U) {
				const auto remaining = aBytes.size() - index;
				auto       group     = static_cast<std::uint32_t>(static_cast<unsigned char>(aBytes[index])) << 16U;
				if (remaining > 1U) {
					group |= static_cast<std::uint32_t>(static_cast<unsigned char>(aBytes[index + 1U])) << 8U;
				}
				if (remaining > 2U) {
					group |= static_cast<std::uint32_t>(static_cast<unsigned char>(aBytes[index + 2U]));
				}
				result.push_back(base64_alphabet[(group >> 18U) & 0x3FU]);
				result.push_back(base64_alphabet[(group >> 12U) & 0x3FU]);
				result.push_back(remaining > 1U ? base64_alphabet[(group >> 6U) & 0x3FU] : '=');
				result.push_back(remaining > 2U ? base64_alphabet[group & 0x3FU] : '=');
			}
			return result;
		}

		/// Strict: padded, canonical, nothing but the alphabet.
		auto unbase64(const std::string_view aText) -> std::optional<std::string>
		{
			if (aText.size() % 4U != 0U) {
				return std::nullopt;
			}
			auto result = std::string{ };
			result.reserve(aText.size() / 4U * 3U);
			for (std::size_t index = 0; index < aText.size(); index += 4U) {
				auto group   = std::uint32_t{ 0 };
				auto padding = 0U;
				for (std::size_t offset = 0; offset < 4U; ++offset) {
					const auto symbol = aText[index + offset];
					if (symbol == '=' && index + 4U == aText.size() && offset >= 2U) {
						++padding;
						group <<= 6U;
						continue;
					}
					const auto value = base64_alphabet.find(symbol);
					if (value == std::string_view::npos || padding != 0U) {
						return std::nullopt;
					}
					group = (group << 6U) | static_cast<std::uint32_t>(value);
				}
				// Canonical form: the bits a padding symbol stands for are zero.
				if ((padding == 1U && (group & 0xFFU) != 0U) || (padding == 2U && (group & 0xFFFFU) != 0U)) {
					return std::nullopt;
				}
				result.push_back(static_cast<char>((group >> 16U) & 0xFFU));
				if (padding < 2U) {
					result.push_back(static_cast<char>((group >> 8U) & 0xFFU));
				}
				if (padding < 1U) {
					result.push_back(static_cast<char>(group & 0xFFU));
				}
			}
			return result;
		}

		auto hex(const std::array<std::uint8_t, 32>& aBytes) -> std::string
		{
			constexpr auto digits = std::string_view{ "0123456789abcdef" };
			auto           result = std::string{ };
			for (const auto byte : aBytes) {
				result.push_back(digits[byte >> 4U]);
				result.push_back(digits[byte & 0x0FU]);
			}
			return result;
		}

		auto number(const std::string_view aText) -> std::optional<std::uint64_t>
		{
			auto value = std::uint64_t{ 0 };
			if (aText.empty() || (aText.size() > 1U && aText.front() == '0')) {
				return std::nullopt;
			}
			const auto [end, error] = std::from_chars(aText.data(), aText.data() + aText.size(), value);
			if (error != std::errc{ } || end != aText.data() + aText.size()) {
				return std::nullopt;
			}
			return value;
		}

		/// Splits the wire text into lines, requiring a final newline.
		class wire_lines
		{
		public:
			explicit wire_lines(const std::string_view aText) noexcept
				: Text{ aText }
			{
			}
			/// The fields of the next line if it starts with `aKey`.
			auto next(const std::string_view aKey, const std::size_t aFields) -> std::optional<std::vector<std::string_view>>
			{
				const auto end = Text.find('\n');
				if (end == std::string_view::npos) {
					return std::nullopt;
				}
				const auto line = Text.substr(0, end);
				Text.remove_prefix(end + 1U);
				auto fields = std::vector<std::string_view>{ };
				auto rest   = line;
				while (true) {
					const auto space = rest.find(' ');
					fields.push_back(rest.substr(0, space));
					if (space == std::string_view::npos) {
						break;
					}
					rest.remove_prefix(space + 1U);
				}
				if (fields.size() != aFields + 1U || fields.front() != aKey) {
					return std::nullopt;
				}
				fields.erase(fields.begin());
				return fields;
			}
			[[nodiscard]] auto done() const noexcept -> bool
			{
				return Text.empty();
			}

		private:
			std::string_view Text;
		};

		auto side(const std::string_view aKey, const std::string_view aBytes) -> std::string
		{
			return std::string{ aKey } + " " + std::to_string(aBytes.size()) + " " + hex(sha256(bytes_of(aBytes))) + " " + base64(aBytes) + "\n";
		}
		auto read_side(wire_lines& aLines, const std::string_view aKey) -> std::optional<std::string>
		{
			const auto fields = aLines.next(aKey, 3);
			if (!fields) {
				return std::nullopt;
			}
			const auto size  = number((*fields)[0]);
			auto       bytes = unbase64((*fields)[2]);
			if (!size || !bytes || bytes->size() != *size || hex(sha256(bytes_of(*bytes))) != (*fields)[1]) {
				return std::nullopt;
			}
			return bytes;
		}
	}

	auto valid(const cycle_suggestion& aSuggestion) -> bool
	{
		return valid_cycle_id(aSuggestion.Id.Item.Cycle) && valid_cycle_id(aSuggestion.Id.Item.Id) && valid_cycle_id(aSuggestion.Id.Id) && aSuggestion.Revision > 0
			&& aSuggestion.Revision <= maximum_cycle_identity && valid_cycle_id(aSuggestion.Source) && valid_cycle_path(aSuggestion.Path) && aSuggestion.OriginalBlob.valid()
			&& aSuggestion.Start <= aSuggestion.End && aSuggestion.End - aSuggestion.Start == aSuggestion.Original.size()
			&& aSuggestion.Original.size() <= maximum_cycle_suggestion_bytes && aSuggestion.Replacement.size() <= maximum_cycle_suggestion_bytes
			&& aSuggestion.Intent <= cycle_suggestion_intent::illustrative && aSuggestion.State <= cycle_suggestion_state::observed_already_applied
			&& (aSuggestion.State == cycle_suggestion_state::observed_already_applied) == aSuggestion.AppliedIn.has_value()
			&& (!aSuggestion.AppliedIn || valid_cycle_id(*aSuggestion.AppliedIn));
	}

	auto make_suggestion(const cycle_suggestion_ref& aId, const suggest_change& aChange) -> std::expected<cycle_suggestion, cycle_refusal>
	{
		const auto& selection  = aChange.Selection;
		auto        suggestion = cycle_suggestion{
			.Id           = aId,
			.Source       = selection.Revision,
			.Path         = selection.Path,
			.OriginalBlob = selection.Blob,
			.Start        = selection.Start,
			.End          = selection.End,
			.Original     = selection.Original,
			.Replacement  = aChange.Replacement,
			.Intent       = aChange.Intent,
		};
		if (aChange.Replacement.size() > maximum_cycle_suggestion_bytes || selection.Original.size() > maximum_cycle_suggestion_bytes) {
			return std::unexpected{ cycle_refusal::bounds };
		}
		if (!valid(suggestion)) {
			return std::unexpected{ cycle_refusal::invalid_target };
		}
		// An exact edit that changes nothing suggests nothing.
		if (suggestion.Intent == cycle_suggestion_intent::exact_replacement && suggestion.Replacement == suggestion.Original) {
			return std::unexpected{ cycle_refusal::invalid_target };
		}
		return suggestion;
	}

	auto add_suggestion(const review_cycle_state& aCycle, const cycle_feedback_thread& aThread, const std::uint64_t aExpectedVersion, const suggest_change& aChange,
						const std::size_t aExisting) -> std::expected<added_cycle_suggestion, cycle_refusal>
	{
		if (!valid(aCycle.Subject) || aThread.Id.Cycle != aCycle.Subject.Cycle || !valid(aThread)) {
			return std::unexpected{ cycle_refusal::invalid_identity };
		}
		if (aCycle.Lifecycle == cycle_lifecycle::archived) {
			return std::unexpected{ cycle_refusal::archived };
		}
		if (const auto refusal = private_write_refusal(aThread.Lineage, aExpectedVersion)) {
			switch (*refusal) {
			case private_review_refusal::conflict: return std::unexpected{ cycle_refusal::conflict };
			case private_review_refusal::archived: return std::unexpected{ cycle_refusal::closed };
			default:                               return std::unexpected{ cycle_refusal::bounds };
			}
		}
		if (aExisting >= maximum_cycle_suggestions) {
			return std::unexpected{ cycle_refusal::bounds };
		}
		if (!valid_target(aCycle, aChange.Selection)) {
			return std::unexpected{ cycle_refusal::invalid_target };
		}
		if (aThread.NextSuggestion > maximum_cycle_identity) {
			return std::unexpected{ cycle_refusal::exhausted };
		}
		auto       thread     = aThread;
		const auto id         = cycle_suggestion_ref{ .Item = aThread.Id, .Id = cycle_suggestion_id{ thread.NextSuggestion++ } };
		auto       suggestion = make_suggestion(id, aChange);
		if (!suggestion) {
			return std::unexpected{ suggestion.error() };
		}
		++thread.Lineage.Version;
		return added_cycle_suggestion{ .Thread = std::move(thread), .Suggestion = std::move(*suggestion) };
	}

	auto verify_suggestion(const cycle_suggestion& aSuggestion, const std::span<const std::uint8_t> aFile) -> bool
	{
		return valid(aSuggestion) && digest_of(aFile) == aSuggestion.OriginalBlob && range_matches(aSuggestion, aFile);
	}

	auto change_suggestion(const cycle_suggestion& aSuggestion, const std::uint64_t aExpectedRevision, const cycle_suggestion_change& aChange)
		-> std::expected<cycle_suggestion, cycle_refusal>
	{
		if (!valid(aSuggestion)) {
			return std::unexpected{ cycle_refusal::invalid_identity };
		}
		if (aExpectedRevision != aSuggestion.Revision) {
			return std::unexpected{ cycle_refusal::conflict };
		}
		if (aSuggestion.State == cycle_suggestion_state::observed_already_applied) {
			return std::unexpected{ cycle_refusal::closed };
		}
		if (aSuggestion.Revision >= maximum_cycle_identity) {
			return std::unexpected{ cycle_refusal::exhausted };
		}
		auto next = aSuggestion;
		++next.Revision;
		if (const auto* revise = std::get_if<revise_suggestion>(&aChange)) {
			if (revise->Replacement.size() > maximum_cycle_suggestion_bytes) {
				return std::unexpected{ cycle_refusal::bounds };
			}
			next.Replacement = revise->Replacement;
			next.Intent      = revise->Intent;
			if (next.Intent == cycle_suggestion_intent::exact_replacement && next.Replacement == next.Original) {
				return std::unexpected{ cycle_refusal::invalid_target };
			}
		} else if (const auto* revalidate = std::get_if<revalidate_suggestion>(&aChange)) {
			const auto& candidate = revalidate->Candidate;
			// Only the same original bytes, found in other material.
			if (candidate.Original != aSuggestion.Original || candidate.End - candidate.Start != candidate.Original.size()
				|| (candidate.Blob == aSuggestion.OriginalBlob && candidate.Start == aSuggestion.Start && candidate.Path == aSuggestion.Path)) {
				return std::unexpected{ cycle_refusal::invalid_target };
			}
			next.Source       = candidate.Revision;
			next.Path         = candidate.Path;
			next.OriginalBlob = candidate.Blob;
			next.Start        = candidate.Start;
			next.End          = candidate.End;
		} else {
			next.State     = cycle_suggestion_state::observed_already_applied;
			next.AppliedIn = std::get<observe_suggestion_applied>(aChange).Result;
		}
		if (!valid(next)) {
			return std::unexpected{ cycle_refusal::invalid_target };
		}
		return next;
	}

	auto preview_suggestion(const cycle_suggestion& aSuggestion, const std::span<const std::uint8_t> aFile) -> std::optional<cycle_suggestion_preview>
	{
		if (!verify_suggestion(aSuggestion, aFile)) {
			return std::nullopt;
		}
		auto       after    = splice(aFile, aSuggestion);
		const auto before   = project_lines(aFile, aSuggestion.Start, aSuggestion.End);
		const auto replaced = project_lines(after, aSuggestion.Start, aSuggestion.Start + aSuggestion.Replacement.size());
		if (!before || !replaced) {
			return std::nullopt;
		}
		const auto blob = digest_of(after);
		return cycle_suggestion_preview{ .After = std::move(after), .AfterBlob = blob, .Before = *before, .Replaced = *replaced };
	}

	auto preflight_suggestions(const std::span<const cycle_suggestion> aSuggestions, const std::span<const std::uint8_t> aFile) -> cycle_suggestion_preflight
	{
		auto result = cycle_suggestion_preflight{ };
		if (aSuggestions.empty()) {
			return result;
		}
		if (aSuggestions.size() > maximum_cycle_suggestions) {
			result.Findings.push_back({ .Index = 0, .Issue = cycle_suggestion_issue::invalid });
			return result;
		}
		const auto  blob    = digest_of(aFile);
		const auto& first   = aSuggestions.front();
		auto        checked = std::vector<bool>(aSuggestions.size(), false);
		for (std::size_t index = 0; index < aSuggestions.size(); ++index) {
			const auto& suggestion = aSuggestions[index];
			auto        issue      = std::optional<cycle_suggestion_issue>{ };
			if (!valid(suggestion)) {
				issue = cycle_suggestion_issue::invalid;
			} else if (suggestion.Path != first.Path || suggestion.OriginalBlob != first.OriginalBlob) {
				issue = cycle_suggestion_issue::other_file;
			} else if (blob != suggestion.OriginalBlob) {
				issue = cycle_suggestion_issue::stale_blob;
			} else if (!range_matches(suggestion, aFile)) {
				issue = cycle_suggestion_issue::stale_range;
			} else if (suggestion.Intent == cycle_suggestion_intent::illustrative) {
				issue = cycle_suggestion_issue::illustrative;
			} else if (suggestion.State == cycle_suggestion_state::observed_already_applied) {
				issue = cycle_suggestion_issue::already_applied;
			}
			if (issue) {
				result.Findings.push_back({ .Index = index, .Issue = *issue });
			}
			// Conflicts are decided over every well-formed edit to this file,
			// whatever else is wrong with it.
			checked[index] = valid(suggestion) && suggestion.Path == first.Path && suggestion.OriginalBlob == first.OriginalBlob;
		}
		for (std::size_t left = 0; left < aSuggestions.size(); ++left) {
			for (std::size_t right = left + 1U; right < aSuggestions.size(); ++right) {
				if (checked[left] && checked[right] && conflicting(aSuggestions[left], aSuggestions[right])) {
					result.Findings.push_back({ .Index = left, .Issue = cycle_suggestion_issue::conflict, .With = right });
				}
			}
		}
		if (!result.Findings.empty()) {
			return result;
		}
		// Every coordinate is in the original file; apply from the end back.
		auto order = std::vector<std::size_t>(aSuggestions.size());
		std::iota(order.begin(), order.end(), std::size_t{ 0 });
		std::ranges::sort(order, [&](const std::size_t aLeft, const std::size_t aRight) { return aSuggestions[aLeft].Start > aSuggestions[aRight].Start; });
		auto bytes = std::vector<std::uint8_t>(aFile.begin(), aFile.end());
		for (const auto index : order) {
			bytes = splice(bytes, aSuggestions[index]);
		}
		result.ResultBlob = digest_of(bytes);
		result.Result     = std::move(bytes);
		return result;
	}

	auto observe_application(const cycle_suggestion& aSuggestion, const std::span<const std::uint8_t> aOriginalFile, const cycle_material_digest& aLater)
		-> cycle_application_observation
	{
		if (!verify_suggestion(aSuggestion, aOriginalFile)) {
			return cycle_application_observation::unknown;
		}
		if (aLater == aSuggestion.OriginalBlob) {
			return cycle_application_observation::not_applied;
		}
		return digest_of(splice(aOriginalFile, aSuggestion)) == aLater ? cycle_application_observation::already_applied : cycle_application_observation::unknown;
	}

	auto encode_suggestion_wire(const cycle_suggestion& aSuggestion) -> std::string
	{
		auto text = std::string{ "bpr-suggestion-v1\n" };
		text += "item " + std::to_string(aSuggestion.Id.Item.Cycle.value()) + " " + std::to_string(aSuggestion.Id.Item.Id.value()) + "\n";
		text += "suggestion " + std::to_string(aSuggestion.Id.Id.value()) + " " + std::to_string(aSuggestion.Revision) + "\n";
		text += "source " + std::to_string(aSuggestion.Source.value()) + " " + base64(aSuggestion.Path) + " " + hex(aSuggestion.OriginalBlob.bytes()) + "\n";
		text += "range " + std::to_string(aSuggestion.Start) + " " + std::to_string(aSuggestion.End) + "\n";
		text += "intent " + std::string{ stable_name(aSuggestion.Intent) } + " " + std::string{ stable_name(aSuggestion.State) } + " "
			+ (aSuggestion.AppliedIn ? std::to_string(aSuggestion.AppliedIn->value()) : std::string{ "-" }) + "\n";
		text += side("original", aSuggestion.Original);
		text += side("replacement", aSuggestion.Replacement);
		text += "end\n";
		return text;
	}

	auto decode_suggestion_wire(const std::string_view aText) -> std::optional<cycle_suggestion>
	{
		auto lines = wire_lines{ aText };
		if (!lines.next("bpr-suggestion-v1", 0)) {
			return std::nullopt;
		}
		const auto item       = lines.next("item", 2);
		const auto suggestion = lines.next("suggestion", 2);
		const auto source     = lines.next("source", 3);
		const auto range      = lines.next("range", 2);
		const auto intent     = lines.next("intent", 3);
		if (!item || !suggestion || !source || !range || !intent) {
			return std::nullopt;
		}
		const auto numbers = std::array{ number((*item)[0]), number((*item)[1]), number((*suggestion)[0]), number((*suggestion)[1]), number((*source)[0]), number((*range)[0]), number((*range)[1]) };
		const auto path    = unbase64((*source)[1]);
		if (std::ranges::any_of(numbers, [](const auto& aValue) { return !aValue.has_value(); }) || !path) {
			return std::nullopt;
		}
		auto decoded = cycle_suggestion{
			.Id       = { .Item = { .Cycle = review_cycle_id{ *numbers[0] }, .Id = cycle_feedback_item_id{ *numbers[1] } }, .Id = cycle_suggestion_id{ *numbers[2] } },
			.Revision = *numbers[3],
			.Source   = cycle_source_revision_id{ *numbers[4] },
			.Path     = *path,
			.Start    = *numbers[5],
			.End      = *numbers[6],
		};
		auto blob = std::array<std::uint8_t, 32>{ };
		if ((*source)[2].size() != 64U) {
			return std::nullopt;
		}
		for (std::size_t index = 0; index < blob.size(); ++index) {
			const auto pair = (*source)[2].substr(index * 2U, 2U);
			auto       byte = std::uint8_t{ 0 };
			if (std::ranges::any_of(pair, [](const char aDigit) { return (aDigit < '0' || aDigit > '9') && (aDigit < 'a' || aDigit > 'f'); })
				|| std::from_chars(pair.data(), pair.data() + pair.size(), byte, 16).ptr != pair.data() + pair.size()) {
				return std::nullopt;
			}
			blob.at(index) = byte;
		}
		decoded.OriginalBlob = cycle_material_digest{ blob };
		if ((*intent)[0] == stable_name(cycle_suggestion_intent::illustrative)) {
			decoded.Intent = cycle_suggestion_intent::illustrative;
		} else if ((*intent)[0] != stable_name(cycle_suggestion_intent::exact_replacement)) {
			return std::nullopt;
		}
		if ((*intent)[1] == stable_name(cycle_suggestion_state::observed_already_applied)) {
			decoded.State = cycle_suggestion_state::observed_already_applied;
		} else if ((*intent)[1] != stable_name(cycle_suggestion_state::proposed)) {
			return std::nullopt;
		}
		if ((*intent)[2] != "-") {
			const auto applied = number((*intent)[2]);
			if (!applied) {
				return std::nullopt;
			}
			decoded.AppliedIn = cycle_source_revision_id{ *applied };
		}
		auto original    = read_side(lines, "original");
		auto replacement = read_side(lines, "replacement");
		if (!original || !replacement || !lines.next("end", 0) || !lines.done()) {
			return std::nullopt;
		}
		decoded.Original    = std::move(*original);
		decoded.Replacement = std::move(*replacement);
		if (!valid(decoded)) {
			return std::nullopt;
		}
		return decoded;
	}

	auto stable_name(const cycle_suggestion_intent aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_suggestion_intent::exact_replacement: return "exact_replacement";
		case cycle_suggestion_intent::illustrative:      return "illustrative";
		}
		return "invalid";
	}
	auto stable_name(const cycle_suggestion_state aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_suggestion_state::proposed:                 return "proposed";
		case cycle_suggestion_state::observed_already_applied: return "observed_already_applied";
		}
		return "invalid";
	}
	auto stable_name(const cycle_suggestion_issue aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_suggestion_issue::stale_blob:      return "stale_blob";
		case cycle_suggestion_issue::stale_range:     return "stale_range";
		case cycle_suggestion_issue::illustrative:    return "illustrative";
		case cycle_suggestion_issue::already_applied: return "already_applied";
		case cycle_suggestion_issue::conflict:        return "conflict";
		case cycle_suggestion_issue::other_file:      return "other_file";
		case cycle_suggestion_issue::invalid:         return "invalid";
		}
		return "invalid";
	}
}
