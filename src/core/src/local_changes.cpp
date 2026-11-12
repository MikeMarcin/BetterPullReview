#include "bpr/core/local_changes.hpp"

#include "bpr/core/sha256.hpp"
#include "bpr/core/utf8.hpp"

#include <algorithm>
#include <ranges>
#include <set>
#include <utility>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto trimmed(std::string_view aText) -> std::string_view
		{
			constexpr auto blank = std::string_view{ " \t\r\n" };
			const auto     first = aText.find_first_not_of(blank);
			if (first == std::string_view::npos) {
				return { };
			}
			const auto last = aText.find_last_not_of(blank);
			return aText.substr(first, last - first + 1U);
		}

		/// The bytes of lines [aBegin, aEnd) of a document, terminators included.
		void append_lines(std::vector<std::uint8_t>& aOut, const decoded_text_document& aDocument, const std::uint32_t aBegin, const std::uint32_t aEnd)
		{
			for (auto line = aBegin; line < aEnd && line < aDocument.Lines.size(); ++line) {
				const auto& decoded = aDocument.Lines[line];
				const auto  end     = decoded.Terminator.empty() ? decoded.Content.End : decoded.Terminator.End;
				aOut.insert(aOut.end(), aDocument.Bytes.begin() + static_cast<std::ptrdiff_t>(decoded.Content.Begin), aDocument.Bytes.begin() + static_cast<std::ptrdiff_t>(end));
			}
		}

		[[nodiscard]] auto rebuilt(const decoded_text_document& aDocument) -> std::vector<std::uint8_t>
		{
			auto out = std::vector<std::uint8_t>{ };
			out.reserve(aDocument.Bytes.size());
			append_lines(out, aDocument, 0U, static_cast<std::uint32_t>(aDocument.Lines.size()));
			return out;
		}

		/// Takes the lines of `aBase` and, at each hunk in `aTaken`, the
		/// other side's lines instead. Left is the base when `aFromLeft`.
		[[nodiscard]] auto splice(const decoded_text_document& aBase, const decoded_text_document& aOther, std::span<const local_change_hunk> aTaken, const bool aFromLeft)
			-> std::vector<std::uint8_t>
		{
			auto out  = std::vector<std::uint8_t>{ };
			auto next = std::uint32_t{ 0 };
			for (const auto& hunk : aTaken) {
				const auto baseBegin  = aFromLeft ? hunk.LeftBegin : hunk.RightBegin;
				const auto baseEnd    = aFromLeft ? hunk.LeftEnd : hunk.RightEnd;
				const auto otherBegin = aFromLeft ? hunk.RightBegin : hunk.LeftBegin;
				const auto otherEnd   = aFromLeft ? hunk.RightEnd : hunk.LeftEnd;
				append_lines(out, aBase, next, baseBegin);
				append_lines(out, aOther, otherBegin, otherEnd);
				next = baseEnd;
			}
			append_lines(out, aBase, next, static_cast<std::uint32_t>(aBase.Lines.size()));
			return out;
		}

		/// Both sides are text that the decoded lines rebuild byte for byte,
		/// and the changes together rebuild the other side.
		[[nodiscard]] auto exact(const computed_material_diff& aDiff, std::span<const local_change_hunk> aHunks) -> std::optional<local_stage_refusal>
		{
			if (aDiff.Alignment.Content != diff_content_state::text || !aDiff.LeftDocument || !aDiff.RightDocument) {
				return local_stage_refusal::not_text;
			}
			if (rebuilt(*aDiff.LeftDocument) != aDiff.LeftDocument->Bytes || rebuilt(*aDiff.RightDocument) != aDiff.RightDocument->Bytes) {
				return local_stage_refusal::inexact;
			}
			if (splice(*aDiff.LeftDocument, *aDiff.RightDocument, aHunks, true) != aDiff.RightDocument->Bytes) {
				return local_stage_refusal::inexact;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto contains_hunk(std::span<const local_change_hunk> aHunks, const local_change_hunk& aHunk) -> bool
		{
			return std::ranges::find(aHunks, aHunk) != aHunks.end();
		}

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

		/// A single-quoted POSIX shell word when the text needs one.
		[[nodiscard]] auto shell_word(const std::string_view aText) -> std::string
		{
			const auto safe = !aText.empty() && std::ranges::all_of(aText, [](const char aByte) {
				return (aByte >= 'a' && aByte <= 'z') || (aByte >= 'A' && aByte <= 'Z') || (aByte >= '0' && aByte <= '9') || aByte == '/' || aByte == '.' || aByte == '_' || aByte == '-'
					|| aByte == '+' || aByte == ':' || aByte == '@' || aByte == ',';
			});
			if (safe) {
				return std::string{ aText };
			}
			auto out = std::string{ "'" };
			for (const auto byte : aText) {
				if (byte == '\'') {
					out += "'\\''";
				} else {
					out += byte;
				}
			}
			return out + "'";
		}

		/// A branch name Git accepts and this command can carry as one argument.
		[[nodiscard]] auto plain_branch(const std::string_view aBranch) -> bool
		{
			return !aBranch.empty() && aBranch.size() <= 255U && aBranch.front() != '-' && aBranch.find("..") == std::string_view::npos && utf8::valid(aBranch)
				&& std::ranges::none_of(aBranch, [](const char aByte) {
					   const auto byte = static_cast<unsigned char>(aByte);
					   return byte <= 0x20U || byte == 0x7FU || aByte == '~' || aByte == '^' || aByte == ':' || aByte == '?' || aByte == '*' || aByte == '[' || aByte == '\\';
				   });
		}

		/// Splits text into lines without their terminators.
		[[nodiscard]] auto lines_of(const std::string_view aText) -> std::vector<std::string_view>
		{
			auto lines = std::vector<std::string_view>{ };
			auto rest  = aText;
			while (!rest.empty()) {
				const auto end  = rest.find('\n');
				auto       line = rest.substr(0U, end);
				if (!line.empty() && line.back() == '\r') {
					line.remove_suffix(1U);
				}
				lines.push_back(line);
				if (end == std::string_view::npos) {
					break;
				}
				rest.remove_prefix(end + 1U);
			}
			return lines;
		}

		/// The text inside the answer's first fenced block, or the answer.
		[[nodiscard]] auto unfenced(const std::string_view aAnswer) -> std::string_view
		{
			const auto open = aAnswer.find("```");
			if (open == std::string_view::npos) {
				return aAnswer;
			}
			const auto bodyStart = aAnswer.find('\n', open);
			if (bodyStart == std::string_view::npos) {
				return aAnswer;
			}
			const auto close = aAnswer.find("```", bodyStart + 1U);
			return close == std::string_view::npos ? aAnswer.substr(bodyStart + 1U) : aAnswer.substr(bodyStart + 1U, close - bodyStart - 1U);
		}

		[[nodiscard]] auto without_label(std::string_view aLine, const std::string_view aLabel) -> std::string_view
		{
			if (aLine.size() >= aLabel.size() && std::ranges::equal(aLine.substr(0U, aLabel.size()), aLabel, [](const char aLeft, const char aRight) {
					return (aLeft >= 'A' && aLeft <= 'Z' ? static_cast<char>(aLeft - 'A' + 'a') : aLeft) == aRight;
				})) {
				aLine.remove_prefix(aLabel.size());
			}
			return trimmed(aLine);
		}

		/// Cuts text to at most `aLimit` bytes on a UTF-8 boundary.
		[[nodiscard]] auto utf8_prefix(const std::string_view aText, std::size_t aLimit) -> std::string_view
		{
			if (aText.size() <= aLimit) {
				return aText;
			}
			while (aLimit > 0U && (static_cast<unsigned char>(aText[aLimit]) & 0xC0U) == 0x80U) {
				--aLimit;
			}
			return aText.substr(0U, aLimit);
		}
	}

	auto name(const local_file_status aStatus) noexcept -> std::string_view
	{
		switch (aStatus) {
		case local_file_status::added:        return "added";
		case local_file_status::modified:     return "modified";
		case local_file_status::deleted:      return "deleted";
		case local_file_status::renamed:      return "renamed";
		case local_file_status::type_changed: return "type_changed";
		case local_file_status::unmerged:     return "unmerged";
		case local_file_status::untracked:    return "untracked";
		}
		return "modified";
	}

	auto badge(const local_file_status aStatus) noexcept -> char
	{
		switch (aStatus) {
		case local_file_status::added:
		case local_file_status::untracked:    return 'A';
		case local_file_status::modified:     return 'M';
		case local_file_status::deleted:      return 'D';
		case local_file_status::renamed:      return 'R';
		case local_file_status::type_changed: return 'T';
		case local_file_status::unmerged:     return 'U';
		}
		return 'M';
	}

	auto has_unmerged(const local_worktree_status& aStatus) noexcept -> bool
	{
		const auto unmerged = [](const local_status_entry& aEntry) { return aEntry.Status == local_file_status::unmerged; };
		return std::ranges::any_of(aStatus.Staged, unmerged) || std::ranges::any_of(aStatus.Unstaged, unmerged);
	}

	auto uncommitted_paths(const local_worktree_status& aStatus) -> std::vector<std::string>
	{
		auto paths = std::set<std::string>{ };
		for (const auto& entry : aStatus.Staged) {
			paths.insert(entry.Path);
		}
		for (const auto& entry : aStatus.Unstaged) {
			paths.insert(entry.Path);
		}
		return { paths.begin(), paths.end() };
	}

	auto normalized(local_worktree_status aStatus) -> local_worktree_status
	{
		const auto order = [](std::vector<local_status_entry>& aEntries) {
			std::ranges::sort(aEntries, [](const local_status_entry& aLeft, const local_status_entry& aRight) {
				return std::tie(aLeft.Path, aLeft.Original, aLeft.Status) < std::tie(aRight.Path, aRight.Original, aRight.Status);
			});
			const auto duplicate = std::ranges::unique(aEntries);
			aEntries.erase(duplicate.begin(), duplicate.end());
		};
		order(aStatus.Staged);
		order(aStatus.Unstaged);
		return aStatus;
	}

	auto name(const local_comparison aComparison) noexcept -> std::string_view
	{
		switch (aComparison) {
		case local_comparison::uncommitted:        return "uncommitted";
		case local_comparison::not_pushed:         return "not_pushed";
		case local_comparison::whole_pull_request: return "whole_pull_request";
		}
		return "uncommitted";
	}

	auto local_change_comparison(const local_comparison aComparison, const std::string_view aPath, const std::optional<std::span<const std::uint8_t>> aLeft,
								 const std::optional<std::span<const std::uint8_t>> aRight, const diff_options& aOptions) -> std::optional<material_comparison_key>
	{
		const auto options = options_digest(aOptions);
		if ((!aLeft && !aRight) || !options || aPath.empty()) {
			return std::nullopt;
		}
		constexpr auto domain = std::string_view{ "BPR-local-change-comparison-v1" };
		auto           bytes  = std::vector<std::uint8_t>(domain.begin(), domain.end());
		bytes.push_back(0U);
		append_length_prefixed(bytes, as_bytes(name(aComparison)));
		append_length_prefixed(bytes, as_bytes(aPath));
		const auto side = [&bytes](const std::optional<std::span<const std::uint8_t>>& aSide) {
			bytes.push_back(aSide ? 1U : 0U);
			if (aSide) {
				const auto digest = sha256(*aSide);
				bytes.insert(bytes.end(), digest.begin(), digest.end());
			}
		};
		side(aLeft);
		side(aRight);
		bytes.insert(bytes.end(), options->bytes().begin(), options->bytes().end());
		const auto algorithm = std::to_string(supported_diff_algorithm_revision.value());
		append_length_prefixed(bytes, as_bytes(algorithm));
		return material_comparison_key{
			.Comparison   = file_comparison_digest{ sha256(bytes) },
			.LeftPresent  = aLeft.has_value(),
			.RightPresent = aRight.has_value(),
			.Options      = *options,
			.Algorithm    = supported_diff_algorithm_revision,
		};
	}

	auto change_hunks(const computed_material_diff& aDiff) -> std::vector<local_change_hunk>
	{
		auto hunks = std::vector<local_change_hunk>{ };
		auto open  = false;
		for (const auto& segment : aDiff.Alignment.Segments) {
			if (segment.Kind == alignment_kind::equal) {
				open = false;
				continue;
			}
			if (!open) {
				hunks.push_back({ .LeftBegin = segment.Left.Begin, .LeftEnd = segment.Left.End, .RightBegin = segment.Right.Begin, .RightEnd = segment.Right.End });
				open = true;
				continue;
			}
			hunks.back().LeftEnd  = std::max(hunks.back().LeftEnd, segment.Left.End);
			hunks.back().RightEnd = std::max(hunks.back().RightEnd, segment.Right.End);
		}
		return hunks;
	}

	auto name(const local_stage_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case local_stage_refusal::stale:    return "stale";
		case local_stage_refusal::not_text: return "not_text";
		case local_stage_refusal::inexact:  return "inexact";
		}
		return "stale";
	}

	auto apply_change(const computed_material_diff& aDiff, const local_change_hunk& aHunk) -> std::expected<std::vector<std::uint8_t>, local_stage_refusal>
	{
		const auto hunks = change_hunks(aDiff);
		if (const auto refused = exact(aDiff, hunks)) {
			return std::unexpected{ *refused };
		}
		if (!contains_hunk(hunks, aHunk)) {
			return std::unexpected{ local_stage_refusal::stale };
		}
		return splice(*aDiff.LeftDocument, *aDiff.RightDocument, std::span{ &aHunk, 1U }, true);
	}

	auto revert_change(const computed_material_diff& aDiff, const local_change_hunk& aHunk) -> std::expected<std::vector<std::uint8_t>, local_stage_refusal>
	{
		const auto hunks = change_hunks(aDiff);
		if (const auto refused = exact(aDiff, hunks)) {
			return std::unexpected{ *refused };
		}
		if (!contains_hunk(hunks, aHunk)) {
			return std::unexpected{ local_stage_refusal::stale };
		}
		return splice(*aDiff.RightDocument, *aDiff.LeftDocument, std::span{ &aHunk, 1U }, false);
	}

	auto uncommitted_bands(const std::span<const local_change_hunk> aHeadToWorktree, const std::span<const local_change_hunk> aHeadToIndex,
						   const std::span<const local_change_hunk> aIndexToWorktree) -> std::vector<local_change_band>
	{
		// Ranges meet when they overlap; an empty range is the point between
		// two lines and meets a range that touches it.
		const auto meets = [](const std::uint32_t aBegin, const std::uint32_t aEnd, const std::uint32_t aOtherBegin, const std::uint32_t aOtherEnd) {
			if (aBegin == aEnd || aOtherBegin == aOtherEnd) {
				return aBegin <= aOtherEnd && aOtherBegin <= aEnd;
			}
			return aBegin < aOtherEnd && aOtherBegin < aEnd;
		};
		auto bands = std::vector<local_change_band>{ };
		bands.reserve(aHeadToWorktree.size());
		for (const auto& shown : aHeadToWorktree) {
			auto band = local_change_band{ .Shown = shown };
			// Worktree lines are the right side of both the shown change and an unstaged one.
			for (const auto& unstaged : aIndexToWorktree) {
				if (meets(shown.RightBegin, shown.RightEnd, unstaged.RightBegin, unstaged.RightEnd)) {
					band.Unstaged = true;
					band.Stage    = band.Stage.value_or(unstaged);
				}
			}
			// Last-commit lines are the left side of both the shown change and a staged one.
			for (const auto& staged : aHeadToIndex) {
				if (meets(shown.LeftBegin, shown.LeftEnd, staged.LeftBegin, staged.LeftEnd)) {
					band.Staged  = true;
					band.Unstage = band.Unstage.value_or(staged);
				}
			}
			bands.push_back(band);
		}
		return bands;
	}

	auto line_counts(const computed_material_diff& aDiff) -> local_line_counts
	{
		auto counts = local_line_counts{ };
		for (const auto& segment : aDiff.Alignment.Segments) {
			if (segment.Kind != alignment_kind::equal) {
				counts.Removed += segment.Left.End - segment.Left.Begin;
				counts.Added += segment.Right.End - segment.Right.Begin;
			}
		}
		return counts;
	}

	auto name(const local_commit_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case local_commit_refusal::nothing_staged: return "nothing_staged";
		case local_commit_refusal::no_subject:     return "no_subject";
		case local_commit_refusal::detached:       return "detached";
		case local_commit_refusal::unborn:         return "unborn";
		case local_commit_refusal::unmerged:       return "unmerged";
		case local_commit_refusal::amend_pushed:   return "amend_pushed";
		}
		return "nothing_staged";
	}

	auto commit_message(const std::string_view aSubject, const std::string_view aDescription) -> std::string
	{
		// The subject is one line: anything after its first line break belongs
		// to the description.
		const auto subjectText = trimmed(aSubject);
		const auto lineBreak   = subjectText.find('\n');
		auto       subject     = std::string{ trimmed(subjectText.substr(0U, lineBreak)) };
		auto       body        = std::string{ };
		if (lineBreak != std::string_view::npos) {
			body = std::string{ trimmed(subjectText.substr(lineBreak + 1U)) };
		}
		const auto description = trimmed(aDescription);
		if (!description.empty()) {
			body = body.empty() ? std::string{ description } : body + "\n\n" + std::string{ description };
		}
		return body.empty() ? subject + "\n" : subject + "\n\n" + body + "\n";
	}

	auto amend_offered(const local_worktree_status& aStatus, const bool aHeadOnHost) noexcept -> bool
	{
		return aStatus.Head.has_value() && !aStatus.Branch.empty() && !aHeadOnHost;
	}

	auto decide_commit(const local_worktree_status& aStatus, const local_commit_request& aRequest, const bool aHeadOnHost)
		-> std::expected<local_commit_plan, local_commit_refusal>
	{
		if (aStatus.Branch.empty()) {
			return std::unexpected{ local_commit_refusal::detached };
		}
		if (has_unmerged(aStatus)) {
			return std::unexpected{ local_commit_refusal::unmerged };
		}
		if (aRequest.Amend && !aStatus.Head) {
			return std::unexpected{ local_commit_refusal::unborn };
		}
		if (aRequest.Amend && aHeadOnHost) {
			return std::unexpected{ local_commit_refusal::amend_pushed };
		}
		if (!aRequest.Amend && aStatus.Staged.empty()) {
			return std::unexpected{ local_commit_refusal::nothing_staged };
		}
		if (trimmed(aRequest.Subject).empty()) {
			return std::unexpected{ local_commit_refusal::no_subject };
		}
		return local_commit_plan{
			.Message = commit_message(aRequest.Subject, aRequest.Description),
			.Amend   = aRequest.Amend,
			.Files   = static_cast<std::uint32_t>(aStatus.Staged.size()),
		};
	}

	auto name(const local_push_state aState) noexcept -> std::string_view
	{
		switch (aState) {
		case local_push_state::unobserved:  return "unobserved";
		case local_push_state::missing:     return "missing";
		case local_push_state::ahead:       return "ahead";
		case local_push_state::up_to_date:  return "up_to_date";
		case local_push_state::behind:      return "behind";
		case local_push_state::diverged:    return "diverged";
		case local_push_state::unknown_tip: return "unknown_tip";
		}
		return "unobserved";
	}

	auto make_push_command(const std::string_view aDirectory, const std::string_view aUrl, const std::string_view aBranch) -> std::optional<local_push_command>
	{
		if (aDirectory.empty() || aUrl.empty() || aUrl.front() == '-' || !plain_branch(aBranch) || !utf8::valid(aUrl)
			|| std::ranges::any_of(aUrl, [](const char aByte) { return static_cast<unsigned char>(aByte) <= 0x20U; })) {
			return std::nullopt;
		}
		const auto refspec = "refs/heads/" + std::string{ aBranch } + ":refs/heads/" + std::string{ aBranch };
		auto       command = local_push_command{
			.Directory = std::string{ aDirectory },
			.Url       = std::string{ aUrl },
			.Branch    = std::string{ aBranch },
			.Arguments = { "push", "--no-follow-tags", std::string{ aUrl }, refspec },
			.Text      = { },
		};
		command.Text = "git push --no-follow-tags " + shell_word(aUrl) + " " + shell_word(refspec);
		return command;
	}

	auto decide_push(const local_worktree_status& aStatus, const local_hosted_branch& aHosted) -> local_push_decision
	{
		auto decision = local_push_decision{ };
		if (!aHosted.Observed) {
			return decision;
		}
		if (!aHosted.Tip) {
			decision.State = local_push_state::missing;
		} else if (!aHosted.TipKnown) {
			decision.State = local_push_state::unknown_tip;
		} else if (aHosted.Ahead > 0U && aHosted.Behind > 0U) {
			decision.State = local_push_state::diverged;
		} else if (aHosted.Behind > 0U) {
			decision.State = local_push_state::behind;
		} else if (aHosted.Ahead > 0U) {
			decision.State = local_push_state::ahead;
		} else {
			decision.State = local_push_state::up_to_date;
		}
		const auto pushable = decision.State == local_push_state::ahead || decision.State == local_push_state::missing;
		decision.Offered    = pushable && !aStatus.Branch.empty() && aStatus.Head.has_value();
		decision.Default    = decision.Offered && aStatus.Staged.empty() && aStatus.Unstaged.empty();
		return decision;
	}

	auto head_on_host(const local_worktree_status& aStatus, const local_hosted_branch& aHosted) noexcept -> bool
	{
		if (!aStatus.Head || !aHosted.Observed || !aHosted.Tip) {
			return false;
		}
		// Unknown tip: GitHub has commits this worktree lacks, and may have
		// this head too. Amending is refused rather than risk a forced push.
		return !aHosted.TipKnown || aHosted.Ahead == 0U;
	}

	auto name(const local_edit_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case local_edit_refusal::deleted:    return "deleted";
		case local_edit_refusal::not_a_file: return "not_a_file";
		case local_edit_refusal::link:       return "link";
		case local_edit_refusal::submodule:  return "submodule";
		case local_edit_refusal::outside:    return "outside";
		case local_edit_refusal::too_large:  return "too_large";
		case local_edit_refusal::not_utf8:   return "not_utf8";
		case local_edit_refusal::nul_byte:   return "nul_byte";
		case local_edit_refusal::filtered:   return "filtered";
		}
		return "deleted";
	}

	auto decide_editable(const local_edit_facts& aFacts, const std::span<const std::uint8_t> aBytes) -> std::optional<local_edit_refusal>
	{
		if (!aFacts.Inside) {
			return local_edit_refusal::outside;
		}
		if (!aFacts.Exists) {
			return local_edit_refusal::deleted;
		}
		if (aFacts.Link) {
			return local_edit_refusal::link;
		}
		if (aFacts.Submodule) {
			return local_edit_refusal::submodule;
		}
		if (!aFacts.Regular) {
			return local_edit_refusal::not_a_file;
		}
		if (aFacts.Size > maximum_editable_bytes || aBytes.size() > maximum_editable_bytes) {
			return local_edit_refusal::too_large;
		}
		if (aFacts.Filtered) {
			return local_edit_refusal::filtered;
		}
		if (std::ranges::find(aBytes, std::uint8_t{ 0 }) != aBytes.end()) {
			return local_edit_refusal::nul_byte;
		}
		if (!utf8::valid(std::string_view{ reinterpret_cast<const char*>(aBytes.data()), aBytes.size() })) {
			return local_edit_refusal::not_utf8;
		}
		return std::nullopt;
	}

	auto name(const local_save_decision aDecision) noexcept -> std::string_view
	{
		switch (aDecision) {
		case local_save_decision::write:           return "write";
		case local_save_decision::changed_on_disk: return "changed_on_disk";
		case local_save_decision::unchanged:       return "unchanged";
		}
		return "write";
	}

	auto decide_save(const std::span<const std::uint8_t> aRead, const std::span<const std::uint8_t> aDisk, const std::span<const std::uint8_t> aEdited) -> local_save_decision
	{
		if (!std::ranges::equal(aRead, aDisk)) {
			return local_save_decision::changed_on_disk;
		}
		return std::ranges::equal(aDisk, aEdited) ? local_save_decision::unchanged : local_save_decision::write;
	}

	auto line_marks(const computed_material_diff& aDiff) -> std::vector<local_line_mark>
	{
		auto marks = std::vector<local_line_mark>{ };
		for (const auto& hunk : change_hunks(aDiff)) {
			const auto removed = hunk.LeftEnd - hunk.LeftBegin;
			const auto added   = hunk.RightEnd - hunk.RightBegin;
			if (added == 0U) {
				marks.push_back({ .Line = hunk.RightBegin, .Kind = local_line_mark_kind::removed_before, .LeftBegin = hunk.LeftBegin, .LeftEnd = hunk.LeftEnd });
				continue;
			}
			const auto kind = removed == 0U ? local_line_mark_kind::added : local_line_mark_kind::changed;
			for (auto line = hunk.RightBegin; line < hunk.RightEnd; ++line) {
				marks.push_back({ .Line = line, .Kind = kind, .LeftBegin = hunk.LeftBegin, .LeftEnd = hunk.LeftEnd });
			}
		}
		return marks;
	}

	auto compose_commit_message_prompt(const commit_message_request& aRequest) -> std::string
	{
		auto prompt = std::string{ trimmed(aRequest.Instructions) };
		prompt += "\n\nBranch: " + aRequest.Branch + "\n";
		if (!aRequest.RecentSubjects.empty()) {
			prompt += "\nRecent commit subjects on this branch, newest first, for style:\n";
			const auto count = std::min(aRequest.RecentSubjects.size(), maximum_commit_message_subjects);
			for (const auto& subject : std::span{ aRequest.RecentSubjects }.first(count)) {
				prompt += "- " + std::string{ trimmed(subject) } + "\n";
			}
		}
		const auto diff = utf8_prefix(aRequest.StagedDiff, maximum_commit_message_diff_bytes);
		prompt += "\nThe staged change (git diff --cached):\n\n```diff\n";
		prompt += diff;
		if (!diff.empty() && diff.back() != '\n') {
			prompt += "\n";
		}
		prompt += "```\n";
		if (diff.size() < aRequest.StagedDiff.size()) {
			prompt += "\n(" + std::to_string(aRequest.StagedDiff.size() - diff.size()) + " more bytes of the staged change were not sent.)\n";
		}
		return prompt;
	}

	auto read_commit_message_answer(const std::string_view aAnswer) -> std::optional<commit_message_suggestion>
	{
		const auto lines = lines_of(trimmed(unfenced(trimmed(aAnswer))));
		auto       index = std::size_t{ 0 };
		while (index < lines.size() && trimmed(lines[index]).empty()) {
			++index;
		}
		if (index == lines.size()) {
			return std::nullopt;
		}
		auto suggestion    = commit_message_suggestion{ };
		suggestion.Subject = std::string{ utf8_prefix(without_label(lines[index], "subject:"), 200U) };
		++index;
		while (index < lines.size() && trimmed(lines[index]).empty()) {
			++index;
		}
		auto description = std::string{ };
		for (; index < lines.size(); ++index) {
			const auto line = lines[index];
			description += description.empty() ? std::string{ without_label(line, "description:") } : std::string{ line };
			description += "\n";
		}
		suggestion.Description = std::string{ trimmed(description) };
		if (suggestion.Subject.empty()) {
			return std::nullopt;
		}
		return suggestion;
	}
}
