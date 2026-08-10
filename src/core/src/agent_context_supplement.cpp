#include "bpr/core/agent_context_supplement.hpp"

#include <algorithm>
#include <ranges>

namespace bpr::core {
	namespace {
		template <typename Value>
		[[nodiscard]] constexpr auto enumerator_at_most(
			const Value aValue, const Value aMaximum) noexcept -> bool
		{
			return static_cast<std::uint64_t>(aValue)
				<= static_cast<std::uint64_t>(aMaximum);
		}

		[[nodiscard]] constexpr auto valid_line_identity(
			const source_line_ref& aLine) noexcept -> bool
		{
			return aLine.Comparison.valid()
				&& enumerator_at_most(aLine.Side, diff_side::right);
		}

		[[nodiscard]] auto valid_session(const agent_session& aSession) noexcept
			-> bool
		{
			const auto& binding = aSession.Binding;
			return aSession.Id.value() != 0U && aSession.Profile.value() != 0U
				&& binding.Host.value() != 0U
				&& binding.Repository.value() != 0U
				&& binding.PullRequest.value() != 0U && binding.BaseOid.valid()
				&& binding.HeadOid.valid() && binding.LeftEpoch.value() != 0U
				&& binding.RightEpoch.value() != 0U;
		}

		[[nodiscard]] auto valid_file(
			const supplement_file_state& aFile) noexcept -> bool
		{
			return aFile.File.valid() && aFile.Material.valid() && !aFile.Path.empty()
				&& aFile.Path.size() <= maximum_identified_path_bytes
				&& enumerator_at_most(aFile.Read, file_read_state::read)
				&& !(aFile.Accepted && aFile.Excluded);
		}

		[[nodiscard]] auto valid_defect(
			const supplement_defect& aDefect) noexcept -> bool
		{
			const auto& history = aDefect.History;
			return aDefect.File.valid() && aDefect.Conversation.value() != 0U
				&& history.Defect.Id.value() != 0U
				&& valid_line_identity(history.Defect.Anchor)
				&& history.Defect.Anchor == history.OriginalAnchor
				&& enumerator_at_most(
					   history.Defect.Severity, defect_severity::critical)
				&& enumerator_at_most(
					   history.Defect.Category, defect_category::other)
				&& enumerator_at_most(
					   history.Defect.State, defect_state::accepted_as_is)
				&& enumerator_at_most(
					   history.Availability,
					   defect_anchor_availability::thread_deleted);
		}

		[[nodiscard]] constexpr auto valid_segment(
			const alignment_segment& aSegment) noexcept -> bool
		{
			if (
				!aSegment.Id.valid()
				|| !enumerator_at_most(
					aSegment.Kind, alignment_kind::right_only)
				|| aSegment.Left.Begin > aSegment.Left.End
				|| aSegment.Right.Begin > aSegment.Right.End
				|| (aSegment.Group && !aSegment.Group->valid())) {
				return false;
			}
			switch (aSegment.Kind) {
			case alignment_kind::equal:
				return !aSegment.Left.empty() && !aSegment.Right.empty()
					&& aSegment.Left.extent() == aSegment.Right.extent();
			case alignment_kind::replacement:
				return !aSegment.Left.empty() && !aSegment.Right.empty();
			case alignment_kind::left_only:
				return !aSegment.Left.empty() && aSegment.Right.empty();
			case alignment_kind::right_only:
				return aSegment.Left.empty() && !aSegment.Right.empty();
			}
			return false;
		}

		[[nodiscard]] auto valid_selection(
			const supplement_semantic_context& aContext) noexcept -> bool
		{
			if (
				!aContext.File.valid() || !aContext.Selection.Comparison.valid()
				|| aContext.Selection.PreferredLine.Comparison
					!= aContext.Selection.Comparison
				|| !valid_line_identity(aContext.Selection.PreferredLine)
				|| !enumerator_at_most(
					aContext.Selection.Kind, semantic_selection_kind::group)
				|| (aContext.Selection.Kind == semantic_selection_kind::group
					&& (!aContext.Selection.Segment || !aContext.Selection.Group))
				|| !enumerator_at_most(
					aContext.Confidence,
					supplement_mapping_confidence::unavailable)
				|| (aContext.Selection.Segment
					&& !aContext.Selection.Segment->valid())
				|| (aContext.Selection.Group && !aContext.Selection.Group->valid())
				|| aContext.Segments.size() > maximum_semantic_segments
				|| aContext.Groups.size() > maximum_semantic_groups
				|| (aContext.SelectedSourceText
					&& aContext.SelectedSourceText->size()
						> maximum_selected_source_bytes)) {
				return false;
			}
			const auto invalidSegment = std::ranges::any_of(
				aContext.Segments, [](const alignment_segment& aSegment) {
					return !valid_segment(aSegment);
				});
			const auto invalidGroup = std::ranges::any_of(
				aContext.Groups, [&aContext](const change_group& aGroup) {
					return !aGroup.Id.valid() || aGroup.Members.Begin > aGroup.Members.End
						|| aGroup.Members.End > aContext.Segments.size()
						|| aGroup.Left.Begin > aGroup.Left.End
						|| aGroup.Right.Begin > aGroup.Right.End
						|| !enumerator_at_most(
							aGroup.Kind, change_summary_kind::mixed);
				});
			const auto missingSegment = aContext.Selection.Segment
				&& std::ranges::none_of(
											aContext.Segments,
											[&aContext](const alignment_segment& aSegment) {
												return aSegment.Id == *aContext.Selection.Segment;
											});
			const auto missingGroup = aContext.Selection.Group
				&& std::ranges::none_of(
										  aContext.Groups, [&aContext](const change_group& aGroup) {
											  return aGroup.Id == *aContext.Selection.Group;
										  });
			const auto unknownSegmentGroup = std::ranges::any_of(
				aContext.Segments, [&aContext](const alignment_segment& aSegment) {
					return aSegment.Group
						&& std::ranges::none_of(
							   aContext.Groups,
							   [&aSegment](const change_group& aGroup) {
								   return aGroup.Id == *aSegment.Group;
							   });
				});
			return !invalidSegment && !invalidGroup && !missingSegment
				&& !missingGroup && !unknownSegmentGroup;
		}

		[[nodiscard]] auto valid_note(const supplement_note& aNote) noexcept -> bool
		{
			return aNote.Text.size() <= maximum_note_bytes
				&& aNote.File.has_value() == aNote.Comparison.has_value()
				&& (!aNote.File || (aNote.File->valid() && aNote.Comparison->valid()));
		}

		[[nodiscard]] auto valid_draft(const supplement_draft& aDraft) noexcept
			-> bool
		{
			return aDraft.Text.size() <= maximum_draft_bytes
				&& (!aDraft.Anchor || valid_line_identity(*aDraft.Anchor));
		}
	} // namespace

	auto build_agent_supplement(const agent_supplement_request& aRequest)
		-> std::optional<agent_context_supplement>
	{
		if (
			!valid_session(aRequest.Session) || aRequest.PullRequestUrl.empty()
			|| aRequest.PullRequestUrl.size() > maximum_pull_request_url_bytes
			|| aRequest.Files.size() > maximum_supplement_paths
			|| aRequest.Defects.size() > maximum_supplement_defects
			|| aRequest.SelectedNotes.size() > maximum_selected_notes
			|| aRequest.SelectedDrafts.size() > maximum_selected_drafts
			|| (aRequest.PreviousUpload && !aRequest.PreviousUpload->valid())
			|| (aRequest.LastReviewedHead && !aRequest.LastReviewedHead->valid())
			|| (aRequest.Selection && !valid_selection(*aRequest.Selection))) {
			return std::nullopt;
		}
		if (
			std::ranges::any_of(aRequest.Files, [](const auto& aFile) {
				return !valid_file(aFile);
			})
			|| std::ranges::any_of(aRequest.Defects, [](const auto& aDefect) {
				   return !valid_defect(aDefect);
			   })
			|| std::ranges::any_of(aRequest.SelectedNotes, [](const auto& aNote) {
				   return !valid_note(aNote);
			   })
			|| std::ranges::any_of(aRequest.SelectedDrafts, [](const auto& aDraft) {
				   return !valid_draft(aDraft);
			   })) {
			return std::nullopt;
		}

		return agent_context_supplement{
			.SchemaVersion    = agent_supplement_schema_version,
			.Session          = aRequest.Session,
			.PullRequestUrl   = aRequest.PullRequestUrl,
			.PreviousUpload   = aRequest.PreviousUpload,
			.LastReviewedHead = aRequest.LastReviewedHead,
			.Files            = aRequest.Files,
			.Defects          = aRequest.Defects,
			.Selection        = aRequest.Selection,
			.Notes            = aRequest.SelectedNotes,
			.Drafts           = aRequest.SelectedDrafts,
		};
	}
} // namespace bpr::core
