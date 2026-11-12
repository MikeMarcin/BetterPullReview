#include "bpr/core/returning_review.hpp"

#include "bpr/core/local_changes.hpp"
#include "bpr/core/sha256.hpp"
#include "bpr/core/utf8.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>

namespace bpr::core {
	namespace {
		void append_length_prefixed(std::vector<std::uint8_t>& aBytes, const std::span<const std::uint8_t> aText)
		{
			for (std::size_t shift = 0U; shift < 64U; shift += 8U) {
				aBytes.push_back(static_cast<std::uint8_t>((static_cast<std::uint64_t>(aText.size()) >> shift) & 0xFFU));
			}
			aBytes.insert(aBytes.end(), aText.begin(), aText.end());
		}

		[[nodiscard]] auto as_bytes(const std::string_view aText) -> std::span<const std::uint8_t>
		{
			return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
		}

		[[nodiscard]] auto index_of(const std::vector<std::string>& aCommits, const std::string& aCommit) -> std::optional<std::size_t>
		{
			const auto found = std::ranges::find(aCommits, aCommit);
			return found == aCommits.end() ? std::nullopt : std::optional{ static_cast<std::size_t>(found - aCommits.begin()) };
		}

		/// The text of one decoded line, without its terminator.
		[[nodiscard]] auto line_text(const decoded_text_document& aDocument, const std::uint32_t aLine) -> std::string_view
		{
			const auto& line = aDocument.Lines[aLine];
			return { reinterpret_cast<const char*>(aDocument.Bytes.data()) + line.Content.Begin, static_cast<std::size_t>(line.Content.End - line.Content.Begin) };
		}

		[[nodiscard]] auto lowered(const std::string_view aText) -> std::string
		{
			auto text = std::string{ aText };
			std::ranges::transform(text, text.begin(), [](const char aCharacter) { return static_cast<char>(std::tolower(static_cast<unsigned char>(aCharacter))); });
			return text;
		}

		[[nodiscard]] auto letter(const char aCharacter) noexcept -> bool
		{
			return std::isalpha(static_cast<unsigned char>(aCharacter)) != 0;
		}

		[[nodiscard]] auto valid_lines(const returning_lines& aLines) -> bool
		{
			return !aLines.Commit.empty() && !aLines.Path.empty() && aLines.FirstLine != 0U && !aLines.Lines.empty() && aLines.Lines.size() <= maximum_returning_excerpt_lines
				&& std::ranges::all_of(aLines.Lines, [](const std::string& aLine) { return aLine.size() <= maximum_returning_line_bytes && utf8::valid(aLine); });
		}
	}

	auto stable_name(const returning_basis aBasis) noexcept -> std::string_view
	{
		switch (aBasis) {
		case returning_basis::review: return "review";
		case returning_basis::seen:   return "seen";
		}
		return "review";
	}

	auto returning_point_of(const latest_upload_facts& aFacts) -> std::optional<returning_point>
	{
		if (aFacts.Authored) {
			return std::nullopt;
		}
		const auto review = aFacts.ReviewedHead.empty() ? std::nullopt : std::optional{ returning_point{ .Head = aFacts.ReviewedHead, .Basis = returning_basis::review } };
		const auto seen   = aFacts.SeenHead.empty() ? std::nullopt : std::optional{ returning_point{ .Head = aFacts.SeenHead, .Basis = returning_basis::seen } };
		if (!review || !seen) {
			return review ? review : seen;
		}
		const auto reviewAt = index_of(aFacts.Commits, review->Head);
		const auto seenAt   = index_of(aFacts.Commits, seen->Head);
		if (reviewAt && seenAt) {
			return *reviewAt > *seenAt ? review : seen;
		}
		if (reviewAt && !seenAt) {
			return review;
		}
		return seen;
	}

	auto stable_name(const latest_upload_state aState) noexcept -> std::string_view
	{
		switch (aState) {
		case latest_upload_state::hidden:      return "hidden";
		case latest_upload_state::reading:     return "reading";
		case latest_upload_state::offered:     return "offered";
		case latest_upload_state::unavailable: return "unavailable";
		}
		return "hidden";
	}

	auto decide_latest_upload(const latest_upload_facts& aFacts) -> latest_upload_offer
	{
		const auto earlier = returning_point_of(aFacts);
		if (!earlier || aFacts.CurrentHead.empty() || earlier->Head == aFacts.CurrentHead) {
			return { };
		}
		switch (aFacts.Material) {
		case returning_material::reading:    return { .State = latest_upload_state::reading, .Earlier = earlier };
		case returning_material::readable:   return { .State = latest_upload_state::offered, .Earlier = earlier };
		case returning_material::unreadable: return { .State = latest_upload_state::unavailable, .Earlier = earlier };
		}
		return { };
	}

	auto proposed_fix_offered(const proposed_fix_facts& aFacts) noexcept -> bool
	{
		return !aFacts.Note && aFacts.ReviewerCommented && !aFacts.LastCommentCommit.empty() && !aFacts.CurrentHead.empty() && aFacts.LastCommentCommit != aFacts.CurrentHead;
	}

	auto stable_name(const addressed_claim aClaim) noexcept -> std::string_view
	{
		switch (aClaim) {
		case addressed_claim::addressed:     return "addressed";
		case addressed_claim::partly:        return "partly";
		case addressed_claim::not_addressed: return "not_addressed";
		case addressed_claim::cannot_tell:   return "cannot_tell";
		}
		return "cannot_tell";
	}

	auto claim_words(const addressed_claim aClaim) noexcept -> std::string_view
	{
		switch (aClaim) {
		case addressed_claim::addressed:     return "Addressed";
		case addressed_claim::partly:        return "Partly";
		case addressed_claim::not_addressed: return "Not addressed";
		case addressed_claim::cannot_tell:   return "Cannot tell";
		}
		return "Cannot tell";
	}

	auto read_addressed_claim(const std::string_view aAnswer) noexcept -> std::optional<addressed_claim>
	{
		auto at = aAnswer.find_first_not_of(" \t\r\n*_#>`");
		if (at == std::string_view::npos) {
			return std::nullopt;
		}
		// The longest form first, so "not addressed" is never read as a
		// leading "addressed" it does not have.
		static constexpr auto forms = std::array{
			std::pair{ std::string_view{ "not addressed" }, addressed_claim::not_addressed },
			std::pair{ std::string_view{ "cannot tell" }, addressed_claim::cannot_tell },
			std::pair{ std::string_view{ "can't tell" }, addressed_claim::cannot_tell },
			std::pair{ std::string_view{ "can\xe2\x80\x99t tell" }, addressed_claim::cannot_tell },
			std::pair{ std::string_view{ "partly" }, addressed_claim::partly },
			std::pair{ std::string_view{ "addressed" }, addressed_claim::addressed },
		};
		const auto rest = aAnswer.substr(at, 16U);
		try {
			const auto head = lowered(rest);
			for (const auto& [form, claim] : forms) {
				if (head.starts_with(form) && (head.size() == form.size() || !letter(head[form.size()]))) {
					return claim;
				}
			}
		} catch (...) {
			return std::nullopt;
		}
		return std::nullopt;
	}

	auto returning_comparison(const std::string_view aPath, const std::optional<std::span<const std::uint8_t>> aEarlier, const std::optional<std::span<const std::uint8_t>> aCurrent,
							  const diff_options& aOptions) -> std::optional<material_comparison_key>
	{
		const auto options = options_digest(aOptions);
		if ((!aEarlier && !aCurrent) || !options || aPath.empty()) {
			return std::nullopt;
		}
		constexpr auto domain = std::string_view{ "BPR-returning-review-comparison-v1" };
		auto           bytes  = std::vector<std::uint8_t>(domain.begin(), domain.end());
		bytes.push_back(0U);
		append_length_prefixed(bytes, as_bytes(aPath));
		const auto side = [&bytes](const std::optional<std::span<const std::uint8_t>>& aSide) {
			bytes.push_back(aSide ? 1U : 0U);
			if (aSide) {
				const auto digest = sha256(*aSide);
				bytes.insert(bytes.end(), digest.begin(), digest.end());
			}
		};
		side(aEarlier);
		side(aCurrent);
		bytes.insert(bytes.end(), options->bytes().begin(), options->bytes().end());
		const auto algorithm = std::to_string(supported_diff_algorithm_revision.value());
		append_length_prefixed(bytes, as_bytes(algorithm));
		return material_comparison_key{
			.Comparison   = file_comparison_digest{ sha256(bytes) },
			.LeftPresent  = aEarlier.has_value(),
			.RightPresent = aCurrent.has_value(),
			.Options      = *options,
			.Algorithm    = supported_diff_algorithm_revision,
		};
	}

	auto render_returning_change(const std::string_view aPath, const computed_material_diff& aDiff) -> std::string
	{
		const auto from   = aDiff.Alignment.Key.LeftPresent ? "a/" + std::string{ aPath } : std::string{ "/dev/null" };
		const auto to     = aDiff.Alignment.Key.RightPresent ? "b/" + std::string{ aPath } : std::string{ "/dev/null" };
		const auto binary = "Binary file " + std::string{ aPath } + " changed\n";
		if (aDiff.Alignment.Content != diff_content_state::text || (aDiff.Alignment.Key.LeftPresent && !aDiff.LeftDocument)
			|| (aDiff.Alignment.Key.RightPresent && !aDiff.RightDocument)) {
			return binary;
		}
		const auto hunks = change_hunks(aDiff);
		if (hunks.empty()) {
			return { };
		}
		static const auto empty      = decoded_text_document{ };
		const auto&       left       = aDiff.LeftDocument ? *aDiff.LeftDocument : empty;
		const auto&       right      = aDiff.RightDocument ? *aDiff.RightDocument : empty;
		const auto        leftCount  = static_cast<std::uint32_t>(left.Lines.size());
		const auto        rightCount = static_cast<std::uint32_t>(right.Lines.size());
		// Hunks whose context would touch are one hunk, as Git's are.
		auto groups = std::vector<std::pair<std::size_t, std::size_t>>{ };
		for (auto index = std::size_t{ 0U }; index < hunks.size(); ++index) {
			if (!groups.empty() && hunks[index].LeftBegin <= hunks[groups.back().second].LeftEnd + 2U * returning_context_lines) {
				groups.back().second = index;
			} else {
				groups.emplace_back(index, index);
			}
		}
		auto       text = "--- " + from + "\n+++ " + to + "\n";
		const auto line = [&text](const char aMark, const std::string_view aLine) {
			text += aMark;
			text += aLine;
			text += '\n';
		};
		for (const auto& [first, last] : groups) {
			const auto leftBegin  = hunks[first].LeftBegin > returning_context_lines ? hunks[first].LeftBegin - returning_context_lines : 0U;
			const auto rightBegin = hunks[first].RightBegin - (hunks[first].LeftBegin - leftBegin);
			const auto leftEnd    = std::min(leftCount, hunks[last].LeftEnd + returning_context_lines);
			const auto rightEnd   = std::min(rightCount, hunks[last].RightEnd + (leftEnd - hunks[last].LeftEnd));
			const auto start      = [](const std::uint32_t aBegin, const std::uint32_t aEnd) { return aEnd == aBegin ? aBegin : aBegin + 1U; };
			text += "@@ -" + std::to_string(start(leftBegin, leftEnd)) + ',' + std::to_string(leftEnd - leftBegin) + " +" + std::to_string(start(rightBegin, rightEnd)) + ','
				+ std::to_string(rightEnd - rightBegin) + " @@\n";
			auto leftAt  = leftBegin;
			auto rightAt = rightBegin;
			for (auto index = first; index <= last; ++index) {
				const auto& hunk = hunks[index];
				for (; leftAt < hunk.LeftBegin && leftAt < leftCount; ++leftAt, ++rightAt) {
					line(' ', line_text(left, leftAt));
				}
				for (auto at = hunk.LeftBegin; at < hunk.LeftEnd && at < leftCount; ++at) {
					line('-', line_text(left, at));
				}
				for (auto at = hunk.RightBegin; at < hunk.RightEnd && at < rightCount; ++at) {
					line('+', line_text(right, at));
				}
				leftAt  = hunk.LeftEnd;
				rightAt = hunk.RightEnd;
			}
			for (; leftAt < leftEnd && leftAt < leftCount; ++leftAt) {
				line(' ', line_text(left, leftAt));
			}
		}
		// The agent's request is UTF-8; text in another encoding is said
		// as a binary change rather than sent as bytes it cannot read.
		return utf8::valid(text) ? text : binary;
	}

	auto bound_returning_changes(std::vector<returning_change_file> aFiles) -> returning_changes
	{
		std::ranges::sort(aFiles, { }, &returning_change_file::Path);
		auto result = returning_changes{ };
		for (const auto& file : aFiles) {
			if (file.Text.empty()) {
				continue;
			}
			if (result.FilesShown < maximum_returning_files && result.Text.size() + file.Text.size() <= maximum_returning_change_bytes) {
				result.Text += file.Text;
				++result.FilesShown;
			} else {
				++result.FilesOmitted;
			}
		}
		return result;
	}

	auto stable_name(const returning_operation aOperation) noexcept -> std::string_view
	{
		switch (aOperation) {
		case returning_operation::latest_upload: return "latest_upload";
		case returning_operation::proposed_fix:  return "proposed_fix";
		}
		return "latest_upload";
	}

	auto returning_excerpt(const std::string_view aCommit, const std::string_view aPath, const std::string_view aText, const std::uint32_t aFirst, const std::uint32_t aLast)
		-> std::optional<returning_lines>
	{
		if (aFirst == 0U || aLast < aFirst || aCommit.empty() || aPath.empty()) {
			return std::nullopt;
		}
		auto lines = std::vector<std::string_view>{ };
		for (auto at = std::size_t{ 0U }; at <= aText.size();) {
			const auto end = aText.find('\n', at);
			if (end == std::string_view::npos) {
				if (at < aText.size()) {
					lines.push_back(aText.substr(at));
				}
				break;
			}
			auto line = aText.substr(at, end - at);
			if (line.ends_with('\r')) {
				line.remove_suffix(1U);
			}
			lines.push_back(line);
			at = end + 1U;
		}
		if (aLast > lines.size()) {
			return std::nullopt;
		}
		const auto first = aFirst > returning_excerpt_context ? aFirst - returning_excerpt_context : 1U;
		auto       last  = std::min<std::uint32_t>(static_cast<std::uint32_t>(lines.size()), aLast + returning_excerpt_context);
		last             = std::min<std::uint32_t>(last, first + static_cast<std::uint32_t>(maximum_returning_excerpt_lines) - 1U);
		auto result      = returning_lines{ .Commit = std::string{ aCommit }, .Path = std::string{ aPath }, .FirstLine = first };
		for (auto line = first; line <= last; ++line) {
			auto text = std::string{ lines[line - 1U] };
			if (text.size() > maximum_returning_line_bytes) {
				text.resize(maximum_returning_line_bytes);
			}
			while (!text.empty() && !utf8::valid(text)) {
				text.pop_back();
			}
			result.Lines.push_back(std::move(text));
		}
		return result;
	}

	auto valid(const supplement_returning& aReturning) -> bool
	{
		if (aReturning.EarlierHead.empty() || aReturning.CurrentHead.empty() || aReturning.EarlierHead == aReturning.CurrentHead) {
			return false;
		}
		switch (aReturning.Operation) {
		case returning_operation::latest_upload:
			return !aReturning.Then && !aReturning.Now && aReturning.Changes.size() <= maximum_returning_change_bytes && aReturning.FilesShown <= maximum_returning_files
				&& utf8::valid(aReturning.Changes);
		case returning_operation::proposed_fix:
			return aReturning.Changes.empty() && aReturning.FilesShown == 0U && aReturning.FilesOmitted == 0U && (!aReturning.Then || valid_lines(*aReturning.Then))
				&& (!aReturning.Now || valid_lines(*aReturning.Now));
		}
		return false;
	}
} // namespace bpr::core
