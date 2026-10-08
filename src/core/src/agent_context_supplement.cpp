#include "bpr/core/agent_context_supplement.hpp"

#include "bpr/core/review_preparation.hpp"

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

		/// \brief Whether this session may carry the supplied typed question.
		///
		/// \details The gate is the message's own session identity, checked
		/// the way `admit_private_context` checks the opt-in's: a message
		/// typed into one conversation is not admitted into the next one just
		/// because both were sent by the same reviewer.
		[[nodiscard]] auto valid_chat_message(
			const agent_session&                     aSession,
			const std::optional<agent_chat_message>& aMessage) noexcept -> bool
		{
			return !aMessage
				|| (aMessage->Session == aSession.Id
					&& aMessage->Text.size() <= maximum_chat_message_bytes);
		}

		/// \brief [[BPR-1091]]. Guidance rides only on a workshop request,
		/// whole, bounded and UTF-8, naming the version it was taken from.
		/// \details The operation is the gate because
		/// `workshop_operation_for` has already decided from the entry point
		/// that this build is a workshop press: guidance on a review, a chat
		/// turn or a tour writing request would be a hook text nobody asked
		/// the agent to apply there.
		[[nodiscard]] auto valid_feedback_guidance(
			const std::optional<workshop_operation>& aOperation,
			const supplement_feedback_guidance&      aGuidance) noexcept -> bool
		{
			return aOperation.has_value() && !aGuidance.Text.empty()
				&& aGuidance.Text.size() <= maximum_feedback_guidance_bytes
				&& valid_writing_utf8(aGuidance.Text) && !aGuidance.Version.empty()
				&& aGuidance.Version.size() <= maximum_feedback_guidance_version_bytes
				&& valid_writing_utf8(aGuidance.Version)
				&& enumerator_at_most(aGuidance.Source, feedback_guidance_source::profile_override);
		}
	} // namespace

	auto stable_name(const feedback_guidance_source aSource) noexcept -> std::string_view
	{
		switch (aSource) {
		case feedback_guidance_source::shipped:
			return "shipped";
		case feedback_guidance_source::profile_override:
			return "profile_override";
		}
		return { };
	}

	auto build_agent_supplement(const agent_supplement_request& aRequest)
		-> std::optional<agent_context_supplement>
	{
		if (
			aRequest.ApplicationSession.value() == 0U
			|| !valid_agent_session(aRequest.Session) || aRequest.PullRequestUrl.empty()
			|| aRequest.PullRequestUrl.size() > maximum_pull_request_url_bytes
			|| aRequest.Files.size() > maximum_supplement_paths
			|| aRequest.Defects.size() > maximum_supplement_defects
			|| aRequest.SelectedNotes.size() > maximum_selected_notes
			|| aRequest.SelectedDrafts.size() > maximum_selected_drafts
			|| (aRequest.PreviousUpload && !aRequest.PreviousUpload->valid())
			|| (aRequest.LastReviewedHead && !aRequest.LastReviewedHead->valid())
			|| !valid_chat_message(aRequest.Session, aRequest.Message)
			|| (aRequest.Operation && !declared(*aRequest.Operation))
			|| (aRequest.FeedbackGuidance && !valid_feedback_guidance(aRequest.Operation, *aRequest.FeedbackGuidance))
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
			.SchemaVersion      = agent_supplement_schema_version,
			.ApplicationSession = aRequest.ApplicationSession,
			.Session            = aRequest.Session,
			.PullRequestUrl     = aRequest.PullRequestUrl,
			.Message            = aRequest.Message ? aRequest.Message->Text
												   : std::string{ },
			.PreviousUpload     = aRequest.PreviousUpload,
			.LastReviewedHead   = aRequest.LastReviewedHead,
			.Files              = aRequest.Files,
			.Defects            = aRequest.Defects,
			.Selection          = aRequest.Selection,
			.Notes              = aRequest.SelectedNotes,
			.Drafts             = aRequest.SelectedDrafts,
			.Operation          = aRequest.Operation,
			.FeedbackGuidance   = aRequest.FeedbackGuidance,
		};
	}

	auto admit_private_context(
		const agent_session&                 aSession,
		const agent_private_context_optin&   aOptIn,
		const std::vector<supplement_note>&  aNotes,
		const std::vector<supplement_draft>& aDrafts) -> agent_private_context
	{
		if (!valid_agent_session(aSession) || aOptIn.Session != aSession.Id) {
			return { };
		}
		return {
			.Notes  = aOptIn.Notes ? aNotes : std::vector<supplement_note>{ },
			.Drafts = aOptIn.Drafts ? aDrafts : std::vector<supplement_draft>{ },
		};
	}

	auto build_agent_supplement(
		const agent_supplement_request&    aRequest,
		const agent_private_context_optin& aOptIn)
		-> std::optional<agent_context_supplement>
	{
		if (!valid_agent_session(aRequest.Session)
			|| aOptIn.Session != aRequest.Session.Id) {
			return std::nullopt;
		}
		// Validated as the request stands, so an oversize or malformed note
		// still refuses the whole supplement rather than being silently
		// dropped by an opt-in that happened to be off.
		auto built = build_agent_supplement(aRequest);
		if (!built.has_value()) {
			return std::nullopt;
		}
		auto admitted = admit_private_context(
			aRequest.Session, aOptIn, aRequest.SelectedNotes,
			aRequest.SelectedDrafts);
		built->Notes  = std::move(admitted.Notes);
		built->Drafts = std::move(admitted.Drafts);
		return built;
	}
} // namespace bpr::core
