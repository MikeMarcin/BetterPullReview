#include "bpr/core/agent_context_supplement.hpp"

#include "bpr/core/review_preparation.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
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
			|| (aRequest.Brief && !valid_brief(*aRequest.Brief))
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
			.Brief              = aRequest.Brief,
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

	namespace {
		/// The longest prefix of `aText` within `aBound` that ends on a
		/// character boundary.
		[[nodiscard]] auto cut_text(const std::string_view aText, const std::size_t aBound) -> std::string
		{
			if (aText.size() <= aBound) {
				return std::string{ aText };
			}
			auto size = aBound;
			while (size > 0U && !writing_boundary(aText, size)) {
				--size;
			}
			return std::string{ aText.substr(0U, size) };
		}

		[[nodiscard]] auto thread_bytes(const brief_thread& aThread) noexcept -> std::size_t
		{
			auto bytes = aThread.Path.size();
			for (const auto& comment : aThread.Comments) {
				bytes += comment.Author.size() + comment.Body.size();
			}
			return bytes;
		}

		[[nodiscard]] auto valid_skill(const brief_review_skill& aSkill) -> bool
		{
			return enumerator_at_most(aSkill.Source, review_skill_source::profile_copy) && inspect_review_skill(aSkill.Text).has_value();
		}
	} // namespace

	auto stable_name(const brief_file_status aStatus) noexcept -> std::string_view
	{
		switch (aStatus) {
		case brief_file_status::added:
			return "added";
		case brief_file_status::modified:
			return "modified";
		case brief_file_status::removed:
			return "removed";
		case brief_file_status::renamed:
			return "renamed";
		case brief_file_status::copied:
			return "copied";
		case brief_file_status::type_changed:
			return "type_changed";
		case brief_file_status::other:
			return "other";
		}
		return { };
	}

	auto stable_name(const brief_workspace aWorkspace) noexcept -> std::string_view
	{
		switch (aWorkspace) {
		case brief_workspace::linked_clone_history:
			return "linked_clone_history";
		case brief_workspace::private_copy:
			return "private_copy";
		case brief_workspace::private_snapshot:
			return "private_snapshot";
		}
		return { };
	}

	auto bound_review_brief(review_brief_input aInput) -> std::optional<supplement_brief>
	{
		if (!enumerator_at_most(aInput.Workspace, brief_workspace::private_snapshot) || (aInput.Skill && !valid_skill(*aInput.Skill))) {
			return std::nullopt;
		}
		auto brief = supplement_brief{
			.Title      = cut_text(aInput.Title, maximum_brief_title_bytes),
			.BaseBranch = cut_text(aInput.BaseBranch, maximum_brief_branch_bytes),
			.Workspace  = aInput.Workspace,
			.Skill      = std::move(aInput.Skill),
		};
		if (aInput.Description.size() <= maximum_brief_description_bytes) {
			brief.Description = std::move(aInput.Description);
		} else {
			const auto window         = std::string_view{ aInput.Description }.substr(0U, maximum_brief_description_bytes);
			const auto line           = window.rfind('\n');
			brief.Description         = line == std::string_view::npos ? cut_text(aInput.Description, maximum_brief_description_bytes)
																	   : std::string{ window.substr(0U, line + 1U) };
			brief.DescriptionCutBytes = aInput.Description.size() - brief.Description.size();
		}
		for (auto& file : aInput.Files) {
			if (brief.Files.size() == maximum_brief_files || file.Path.empty() || file.Path.size() > maximum_brief_path_bytes
				|| (file.PreviousPath && file.PreviousPath->size() > maximum_brief_path_bytes) || !enumerator_at_most(file.Status, brief_file_status::other)) {
				++brief.FilesOmitted;
				continue;
			}
			brief.Files.push_back(std::move(file));
		}
		std::ranges::stable_partition(aInput.Threads, [](const brief_thread& aThread) { return !aThread.Resolved; });
		auto budget = maximum_brief_thread_bytes;
		for (auto& thread : aInput.Threads) {
			if (thread.Comments.empty()) {
				continue;
			}
			for (auto& comment : thread.Comments) {
				comment.Author = cut_text(comment.Author, maximum_brief_login_bytes);
			}
			const auto bytes = thread_bytes(thread);
			if (thread.Path.size() > maximum_brief_path_bytes || bytes > budget) {
				++brief.ThreadsOmitted;
				continue;
			}
			budget -= bytes;
			brief.Threads.push_back(std::move(thread));
		}
		// [[BPR-PENDING-AQ6]]. The newest turns are the ones a follow-up
		// leans on, so the cut falls on the oldest.
		auto room = maximum_brief_conversation_bytes;
		auto kept = std::size_t{ 0U };
		for (auto turn = aInput.Conversation.rbegin(); turn != aInput.Conversation.rend(); ++turn) {
			turn->Asked      = cut_text(turn->Asked, maximum_brief_turn_bytes);
			turn->Answered   = cut_text(turn->Answered, maximum_brief_turn_bytes);
			const auto bytes = turn->Asked.size() + turn->Answered.size();
			if (kept == maximum_brief_turns || bytes > room) {
				break;
			}
			room -= bytes;
			++kept;
		}
		brief.ConversationOmitted = aInput.Conversation.size() - kept;
		brief.Conversation.assign(std::make_move_iterator(aInput.Conversation.end() - static_cast<std::ptrdiff_t>(kept)), std::make_move_iterator(aInput.Conversation.end()));
		return brief;
	}

	auto valid_brief(const supplement_brief& aBrief) -> bool
	{
		if (aBrief.Title.size() > maximum_brief_title_bytes || aBrief.Description.size() > maximum_brief_description_bytes
			|| aBrief.BaseBranch.size() > maximum_brief_branch_bytes || aBrief.Files.size() > maximum_brief_files
			|| !enumerator_at_most(aBrief.Workspace, brief_workspace::private_snapshot) || (aBrief.Skill && !valid_skill(*aBrief.Skill))) {
			return false;
		}
		if (std::ranges::any_of(aBrief.Files, [](const brief_changed_file& aFile) {
				return aFile.Path.empty() || aFile.Path.size() > maximum_brief_path_bytes || (aFile.PreviousPath && aFile.PreviousPath->size() > maximum_brief_path_bytes)
					|| !enumerator_at_most(aFile.Status, brief_file_status::other);
			})) {
			return false;
		}
		auto bytes = std::size_t{ 0U };
		for (const auto& thread : aBrief.Threads) {
			if (thread.Comments.empty() || thread.Path.size() > maximum_brief_path_bytes
				|| std::ranges::any_of(thread.Comments, [](const brief_comment& aComment) { return aComment.Author.size() > maximum_brief_login_bytes; })) {
				return false;
			}
			bytes += thread_bytes(thread);
			if (bytes > maximum_brief_thread_bytes) {
				return false;
			}
		}
		if (aBrief.Conversation.size() > maximum_brief_turns) {
			return false;
		}
		auto said = std::size_t{ 0U };
		for (const auto& turn : aBrief.Conversation) {
			if (turn.Asked.size() > maximum_brief_turn_bytes || turn.Answered.size() > maximum_brief_turn_bytes) {
				return false;
			}
			said += turn.Asked.size() + turn.Answered.size();
		}
		return said <= maximum_brief_conversation_bytes;
	}
} // namespace bpr::core
