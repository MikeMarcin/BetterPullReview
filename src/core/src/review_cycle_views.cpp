#include "bpr/core/review_cycle_views.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <system_error>

namespace bpr::core {
	namespace {
		auto allocated(const review_cycle_state& aCycle, const cycle_source_revision_id aRevision) -> bool
		{
			return valid_cycle_id(aRevision) && aRevision.value() < aCycle.Next.SourceRevision;
		}

		auto problem_of(const cycle_comparison_error aError) -> cycle_view_problem
		{
			return aError == cycle_comparison_error::uncaptured ? cycle_view_problem::uncaptured : cycle_view_problem::unknown_revision;
		}

		/// The named view as the comparison selector it stands for now.
		auto selector_of(const review_cycle_state& aCycle, const cycle_view_facts& aFacts, const cycle_view_request& aRequest)
			-> std::expected<cycle_comparison_selector, cycle_view_problem>
		{
			if (const auto* round = std::get_if<this_round_request>(&aRequest)) {
				const auto bounds = std::ranges::find(aFacts.Rounds, round->Round, &cycle_round_bounds::Round);
				if (!valid_cycle_id(round->Round) || bounds == aFacts.Rounds.end()) {
					return std::unexpected(cycle_view_problem::unknown_round);
				}
				return this_round_view{ .Start = bounds->Start, .Result = round->Result };
			}
			if (std::holds_alternative<since_last_review_request>(aRequest)) {
				if (!aFacts.LastReview) {
					return std::unexpected(aCycle.Latest ? cycle_view_problem::no_review_checkpoint : cycle_view_problem::uncaptured);
				}
				return since_checkpoint_view{ .Checkpoint = aFacts.LastReview->Revision };
			}
			if (std::holds_alternative<all_cycle_request>(aRequest)) {
				return all_cycle_view{ };
			}
			const auto& pair = std::get<chosen_pair_request>(aRequest);
			return revision_pair_view{ .Left = pair.Left, .Right = pair.Right };
		}

		auto step_between(const cycle_revision_source& aBefore, const cycle_revision_source& aAfter) -> cycle_revision_step
		{
			if (!aBefore.Repository || !aAfter.Repository) {
				return cycle_revision_step::no_repository;
			}
			if (aBefore.HeadReference != aAfter.HeadReference || !aAfter.HeadReference) {
				// A detached head is its own line of work unless it did not move.
				if (!aAfter.HeadReference && !aBefore.HeadReference && aBefore.HeadCommit == aAfter.HeadCommit) {
					return cycle_revision_step::same_head;
				}
				return cycle_revision_step::branch_changed;
			}
			return aBefore.HeadCommit == aAfter.HeadCommit ? cycle_revision_step::same_head : cycle_revision_step::head_moved;
		}
	}

	auto stable_name(const cycle_checkpoint_problem aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_checkpoint_problem::uncaptured:       return "uncaptured";
		case cycle_checkpoint_problem::unknown_revision: return "unknown_revision";
		case cycle_checkpoint_problem::unchanged:        return "unchanged";
		}
		return "unknown";
	}

	auto stable_name(const cycle_view_problem aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_view_problem::uncaptured:           return "uncaptured";
		case cycle_view_problem::unknown_revision:     return "unknown_revision";
		case cycle_view_problem::no_review_checkpoint: return "no_review_checkpoint";
		case cycle_view_problem::unknown_round:        return "unknown_round";
		}
		return "unknown";
	}

	auto stable_name(const cycle_revision_step aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_revision_step::initial:           return "initial";
		case cycle_revision_step::same_head:         return "same_head";
		case cycle_revision_step::head_moved:        return "head_moved";
		case cycle_revision_step::branch_changed:    return "branch_changed";
		case cycle_revision_step::no_repository:     return "no_repository";
		case cycle_revision_step::previous_unlisted: return "previous_unlisted";
		}
		return "unknown";
	}

	auto mark_reviewed(const review_cycle_state& aCycle, const std::optional<cycle_review_checkpoint>& aLatest, const cycle_source_revision_id aRevision)
		-> std::expected<cycle_review_checkpoint, cycle_checkpoint_problem>
	{
		if (!aCycle.Latest) {
			return std::unexpected(cycle_checkpoint_problem::uncaptured);
		}
		if (!allocated(aCycle, aRevision)) {
			return std::unexpected(cycle_checkpoint_problem::unknown_revision);
		}
		if (aLatest && aLatest->Revision == aRevision) {
			return std::unexpected(cycle_checkpoint_problem::unchanged);
		}
		const auto ordinal = aLatest ? aLatest->Ordinal + 1U : 1U;
		if (ordinal > maximum_cycle_identity) {
			return std::unexpected(cycle_checkpoint_problem::unknown_revision);
		}
		return cycle_review_checkpoint{ .Ordinal = ordinal, .Revision = aRevision };
	}

	auto round_start(const std::string_view aSourceRevision) -> std::optional<cycle_source_revision_id>
	{
		if (aSourceRevision.size() < 2U || aSourceRevision.front() != 'R' || aSourceRevision[1] == '0') {
			return std::nullopt;
		}
		auto       value  = std::uint64_t{ 0 };
		const auto digits = aSourceRevision.substr(1);
		const auto parsed = std::from_chars(digits.data(), digits.data() + digits.size(), value);
		if (parsed.ec != std::errc{ } || parsed.ptr != digits.data() + digits.size() || value == 0U || value > maximum_cycle_identity) {
			return std::nullopt;
		}
		return cycle_source_revision_id{ value };
	}

	auto pinned(const cycle_view_request& aRequest) noexcept -> bool
	{
		if (const auto* round = std::get_if<this_round_request>(&aRequest)) {
			return round->Result.has_value();
		}
		return std::holds_alternative<chosen_pair_request>(aRequest);
	}

	auto resolve_view(const review_cycle_state& aCycle, const cycle_view_facts& aFacts, const cycle_view_request& aRequest)
		-> std::expected<cycle_comparison_pair, cycle_view_problem>
	{
		const auto selector = selector_of(aCycle, aFacts, aRequest);
		if (!selector) {
			return std::unexpected(selector.error());
		}
		const auto resolved = resolve_comparison(aCycle, *selector);
		if (!resolved) {
			return std::unexpected(problem_of(resolved.error()));
		}
		return *resolved;
	}

	auto view_newer_available(const review_cycle_state& aCycle, const cycle_view_facts& aFacts, const cycle_view_request& aRequest, const cycle_comparison_pair& aShown)
		-> bool
	{
		if (!aCycle.Latest) {
			return false;
		}
		if (pinned(aRequest)) {
			return aCycle.Latest->Id.Id > aShown.Right.Id;
		}
		const auto resolved = resolve_view(aCycle, aFacts, aRequest);
		return resolved && *resolved != aShown;
	}

	auto diff_rows(const computed_material_diff& aDiff) -> std::vector<cycle_diff_row>
	{
		const auto text = [](const std::optional<decoded_text_document>& aDocument, const std::uint32_t aLine) -> std::string {
			if (!aDocument || aLine >= aDocument->Lines.size()) {
				return { };
			}
			const auto& span = aDocument->Lines[aLine].Content;
			return { aDocument->Bytes.begin() + static_cast<std::ptrdiff_t>(span.Begin), aDocument->Bytes.begin() + static_cast<std::ptrdiff_t>(span.End) };
		};
		auto rows = std::vector<cycle_diff_row>{ };
		for (const auto& segment : aDiff.Alignment.Segments) {
			if (segment.Kind == alignment_kind::equal) {
				for (auto offset = std::uint32_t{ 0 }; offset < segment.Right.End - segment.Right.Begin; ++offset) {
					rows.push_back({ .Kind = cycle_diff_row_kind::context, .LeftLine = segment.Left.Begin + offset + 1U, .RightLine = segment.Right.Begin + offset + 1U, .Text = text(aDiff.RightDocument, segment.Right.Begin + offset) });
				}
				continue;
			}
			for (auto line = segment.Left.Begin; line < segment.Left.End; ++line) {
				rows.push_back({ .Kind = cycle_diff_row_kind::removed, .LeftLine = line + 1U, .Text = text(aDiff.LeftDocument, line) });
			}
			for (auto line = segment.Right.Begin; line < segment.Right.End; ++line) {
				rows.push_back({ .Kind = cycle_diff_row_kind::added, .RightLine = line + 1U, .Text = text(aDiff.RightDocument, line) });
			}
		}
		return rows;
	}

	auto describe_history(const std::span<const cycle_revision_source> aRevisions, const std::span<const cycle_imported_revision> aImported) -> cycle_history
	{
		auto history = cycle_history{ };
		for (std::size_t index = 0; index < aRevisions.size(); ++index) {
			const auto& source   = aRevisions[index];
			const auto& revision = source.Revision;
			auto        entry    = cycle_history_entry{
				.Revision      = revision.Id.Id,
				.Parent        = revision.Parent,
				.HeadReference = source.HeadReference,
				.HeadCommit    = source.HeadCommit,
				.Coverage      = revision.Capture.Coverage,
				.Quality       = revision.Capture.Quality,
			};
			const auto* previous = index == 0U ? nullptr : &aRevisions[index - 1U];
			if (previous != nullptr && previous->Revision.Id.Id.value() + 1U != revision.Id.Id.value()) {
				// Captures between these two were not supplied.
				history.Unlisted += revision.Id.Id.value() - previous->Revision.Id.Id.value() - 1U;
			}
			if (!revision.Parent) {
				entry.Step = cycle_revision_step::initial;
			} else {
				const auto expected = cycle_source_revision_id{ revision.Id.Id.value() - 1U };
				entry.ParentGap     = *revision.Parent != expected;
				// Without the capture before it, its head is not known here.
				entry.Step         = previous != nullptr ? step_between(*previous, source) : cycle_revision_step::previous_unlisted;
				entry.SameMaterial = previous != nullptr && previous->Revision.Id.Id == *revision.Parent && same_material(previous->Revision, revision);
			}
			for (const auto& imported : aImported) {
				if (imported.Local == revision.Id.Id) {
					entry.Imported.push_back(imported);
				}
			}
			std::ranges::sort(entry.Imported);
			history.Entries.push_back(std::move(entry));
		}
		return history;
	}
}
