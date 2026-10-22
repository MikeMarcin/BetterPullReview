/// \file
/// \brief Proves exact bounded agent context without presentation authority.

#include "bpr/core/agent_context_supplement.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto identity(const std::uint8_t aSeed) -> Identity
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		bytes.fill(aSeed);
		return Identity{ bytes };
	}

	[[nodiscard]] auto commit(const std::uint8_t aSeed) -> core::git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (auto index = std::size_t{ 0U }; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto session() -> core::agent_session
	{
		return {
			.Id      = core::agent_session_id{ 40U },
			.Profile = core::agent_profile_id{ 1U },
			.Binding = {
				.Host        = core::git_host_id{ 10U },
				.Repository  = core::repository_id{ 20U },
				.PullRequest = core::pull_request_id{ 30U },
				.BaseOid     = commit(1U),
				.HeadOid     = commit(2U),
				.LeftEpoch   = core::epoch_ordinal{ 1U },
				.RightEpoch  = core::epoch_ordinal{ 2U },
			},
		};
	}

	[[nodiscard]] auto minimal_request() -> core::agent_supplement_request
	{
		return {
			.ApplicationSession = core::application_session_id{ 11U },
			.Session            = session(),
			.PullRequestUrl     = "https://github.example/octo/app/pull/30",
		};
	}

	[[nodiscard]] auto defect() -> core::supplement_defect
	{
		const auto anchor = core::source_line_ref{
			.Comparison = identity<core::file_comparison_digest>(5U),
			.Side       = core::diff_side::right,
			.Line       = 17U,
		};
		return {
			.File         = identity<core::file_identity>(3U),
			.Conversation = core::review_conversation_id{ 70U },
			.History      = {
				.Defect = {
					.Id       = core::defect_id{ 80U },
					.Anchor   = anchor,
					.Severity = core::defect_severity::critical,
					.Category = core::defect_category::security,
					.State    = core::defect_state::verified,
				},
				.OriginalAnchor = anchor,
				.Availability   = core::defect_anchor_availability::outdated,
			},
		};
	}

	[[nodiscard]] auto semantic_context()
		-> core::supplement_semantic_context
	{
		const auto comparison = identity<core::file_comparison_digest>(5U);
		const auto segment    = identity<core::alignment_segment_id>(6U);
		const auto group      = identity<core::change_group_id>(7U);
		return {
			.File      = identity<core::file_identity>(3U),
			.Selection = {
				.Comparison    = comparison,
				.Kind          = core::semantic_selection_kind::group,
				.PreferredLine = {
					.Comparison = comparison,
					.Side       = core::diff_side::right,
					.Line       = 17U,
				},
				.Segment = segment,
				.Group   = group,
			},
			.Confidence = core::supplement_mapping_confidence::similar,
			.Segments   = {
				{
					.Id    = segment,
					.Kind  = core::alignment_kind::replacement,
					.Left  = { 16U, 18U },
					.Right = { 16U, 19U },
					.Group = group,
				},
			},
			.Groups = {
				{
					.Id                 = group,
					.Members            = { 0U, 1U },
					.Left               = { 16U, 18U },
					.Right              = { 16U, 19U },
					.First              = { 16U, 16U },
					.Last               = { 18U, 19U },
					.Kind               = core::change_summary_kind::replacement,
					.AddedLines         = 1U,
					.DeletedLines       = 0U,
					.ReplacedLeftLines  = 2U,
					.ReplacedRightLines = 3U,
				},
			},
			.SelectedSourceText = std::string{ "if (ready) { run(); }" },
		};
	}

	void test_a_minimal_supplement_preserves_absence()
	{
		const auto built = core::build_agent_supplement(minimal_request());
		expect(built.has_value(), "a valid minimal request builds");
		const auto expected = core::agent_context_supplement{
			.SchemaVersion      = core::agent_supplement_schema_version,
			.ApplicationSession = core::application_session_id{ 11U },
			.Session            = session(),
			.PullRequestUrl     = "https://github.example/octo/app/pull/30",
		};
		expect(
			*built == expected,
			"every optional value remains absent and every collection remains empty");
	}

	void test_every_required_local_and_semantic_field_travels_exactly()
	{
		auto request             = minimal_request();
		request.PreviousUpload   = commit(3U);
		request.LastReviewedHead = commit(4U);
		request.Files            = {
			{
				.File               = identity<core::file_identity>(3U),
				.Material           = identity<core::file_material_id>(4U),
				.Path               = "src/retry.cpp",
				.Read               = core::file_read_state::read,
				.ChangedSinceReview = true,
				.Accepted           = true,
			},
		};
		request.Defects       = { defect() };
		request.Selection     = semantic_context();
		request.SelectedNotes = {
			{
				.File       = identity<core::file_identity>(3U),
				.Comparison = identity<core::file_comparison_digest>(5U),
				.Text       = "check retry exhaustion",
			},
		};
		request.SelectedDrafts = {
			{
				.Anchor = request.Selection->Selection.PreferredLine,
				.Text   = "Would a bounded retry be safer?",
			},
		};

		const auto built = core::build_agent_supplement(request);
		expect(built.has_value(), "the complete bounded request builds");
		expect(
			built->Session == request.Session
				&& built->PullRequestUrl == request.PullRequestUrl
				&& built->PreviousUpload == request.PreviousUpload
				&& built->LastReviewedHead == request.LastReviewedHead
				&& built->Files == request.Files && built->Defects == request.Defects
				&& built->Selection == request.Selection
				&& built->Notes == request.SelectedNotes
				&& built->Drafts == request.SelectedDrafts,
			"session, PR, revisions, receipts, defect history, semantic source, "
			"notes, and drafts travel without substitution");
	}

	void test_absent_and_present_empty_source_text_are_different()
	{
		auto absent      = minimal_request();
		absent.Selection = semantic_context();
		absent.Selection->SelectedSourceText.reset();
		auto empty                          = absent;
		empty.Selection->SelectedSourceText = std::string{ };

		const auto absentBuilt = core::build_agent_supplement(absent);
		const auto emptyBuilt  = core::build_agent_supplement(empty);
		expect(
			absentBuilt && emptyBuilt && absentBuilt != emptyBuilt
				&& !absentBuilt->Selection->SelectedSourceText
				&& emptyBuilt->Selection->SelectedSourceText.has_value()
				&& emptyBuilt->Selection->SelectedSourceText->empty(),
			"an absent source differs from a deliberately selected blank line");
	}

	void test_no_field_can_hold_presentation_or_credential_authority()
	{
		using supplement    = core::agent_context_supplement;
		const auto whole    = supplement{ };
		const auto& [version,
					 applicationSession,
					 run,
					 url,
					 message,
					 previous,
					 lastReviewed,
					 files,
					 defects,
					 selection,
					 notes,
					 drafts,
					 operation,
					 guidance,
					 brief] = whole;
		static_assert(std::is_same_v<decltype(version), const std::uint32_t>);
		static_assert(std::is_same_v<
					  decltype(applicationSession),
					  const core::application_session_id>);
		static_assert(std::is_same_v<decltype(run), const core::agent_session>);
		static_assert(std::is_same_v<decltype(message), const std::string>);
		static_assert(std::is_same_v<
					  decltype(selection),
					  const std::optional<core::supplement_semantic_context>>);
		static_assert(std::is_same_v<
					  decltype(notes), const std::vector<core::supplement_note>>);
		static_assert(std::is_same_v<
					  decltype(drafts), const std::vector<core::supplement_draft>>);
		static_assert(std::is_same_v<
					  decltype(operation),
					  const std::optional<core::workshop_operation>>);
		// [[BPR-1091]]. Hook text, its shipped version and where it came from:
		// bounded prose, no argv, environment or token.
		static_assert(std::is_same_v<
					  decltype(guidance),
					  const std::optional<core::supplement_feedback_guidance>>);
		[[maybe_unused]] const auto& [guidanceSource, guidanceVersion, guidanceText] = core::supplement_feedback_guidance{ };
		static_assert(std::is_same_v<decltype(guidanceSource), const core::feedback_guidance_source>);
		static_assert(std::is_same_v<decltype(guidanceVersion), const std::string>);
		static_assert(std::is_same_v<decltype(guidanceText), const std::string>);
		// [[BPR-PENDING-AQ5]]. The brief: what the reviewer can read on GitHub,
		// the workspace's kind and the review skill's text; no argv,
		// environment or token.
		static_assert(std::is_same_v<decltype(brief), const std::optional<core::supplement_brief>>);
		[[maybe_unused]] const auto& [briefTitle, briefDescription, briefCut, briefBase, briefFiles, briefFilesOmitted, briefThreads, briefThreadsOmitted, briefWorkspace,
									  briefSkill] = core::supplement_brief{ };
		static_assert(std::is_same_v<decltype(briefSkill), const std::optional<core::brief_review_skill>>);
		[[maybe_unused]] const auto& [skillSource, skillVersion, skillText] = core::brief_review_skill{ };
		static_assert(std::is_same_v<decltype(skillText), const std::string>);
		expect(
			version == core::agent_supplement_schema_version
				&& applicationSession.value() == 0U && run.Id.value() == 0U
				&& url.empty() && message.empty() && !previous && !lastReviewed
				&& files.empty() && defects.empty() && !selection && notes.empty()
				&& drafts.empty() && !operation && !guidance && !brief,
			"the complete type contains only domain identity, bounded source/prose, "
			"and explicit absence - no argv, environment, token, Qt index, or coordinate");

		const auto semantic      = core::supplement_semantic_context{ };
		const auto& [file,
					 selectionValue,
					 confidence,
					 segments,
					 groups,
					 sourceText] = semantic;
		static_assert(
			std::is_same_v<decltype(file), const core::file_identity>);
		static_assert(std::is_same_v<
					  decltype(selectionValue),
					  const core::semantic_selection>);
		static_assert(std::is_same_v<
					  decltype(segments),
					  const std::vector<core::alignment_segment>>);
		static_assert(std::is_same_v<
					  decltype(groups),
					  const std::vector<core::change_group>>);
		expect(
			!file.valid() && !selectionValue.Comparison.valid()
				&& confidence
					== core::supplement_mapping_confidence::unavailable
				&& segments.empty() && groups.empty() && !sourceText,
			"semantic context is exactly domain identity, mappings, and optional source text");
	}

	[[nodiscard]] auto private_request() -> core::agent_supplement_request
	{
		auto request      = minimal_request();
		request.Selection = semantic_context();
		request.Files     = {
			{
				.File     = identity<core::file_identity>(3U),
				.Material = identity<core::file_material_id>(4U),
				.Path     = "src/retry.cpp",
				.Read     = core::file_read_state::read,
			},
		};
		request.SelectedNotes = {
			{
				.File       = identity<core::file_identity>(3U),
				.Comparison = identity<core::file_comparison_digest>(5U),
				.Text       = "check retry exhaustion",
			},
		};
		request.SelectedDrafts = {
			{
				.Anchor = request.Selection->Selection.PreferredLine,
				.Text   = "Would a bounded retry be safer?",
			},
		};
		return request;
	}

	/// \brief Private notes and drafts are opt-in per session, and default off.
	void test_private_prose_travels_only_where_this_session_opted_in()
	{
		using admitted                               = core::agent_private_context;
		[[maybe_unused]] const auto& [notes, drafts] = admitted{ };
		static_assert(std::is_same_v<
					  decltype(notes), const std::vector<core::supplement_note>>);
		static_assert(std::is_same_v<
					  decltype(drafts),
					  const std::vector<core::supplement_draft>>);
		static_assert(
			sizeof(admitted)
				== sizeof(std::vector<core::supplement_note>)
					+ sizeof(std::vector<core::supplement_draft>),
			"agent_private_context gained a field. Admitted prose is the two "
			"collections and nothing that could describe them, so there is no "
			"'the reviewer declined, here is a summary anyway'.");
		expect(
			notes.empty() && drafts.empty(),
			"a default admission carries nothing, so forgetting to opt in fails "
			"toward zero private prose rather than toward all of it");

		const auto request = private_request();
		expect(
			!request.SelectedNotes.empty() && !request.SelectedDrafts.empty(),
			"the review holds one note and one unpublished draft, so the "
			"absences below are about the opt-in and not about an empty review");

		const auto defaulted = core::agent_private_context_optin{
			.Session = request.Session.Id,
		};
		expect(
			!defaulted.Notes && !defaulted.Drafts,
			"an opt-in defaults to off for both");

		const auto withheld = core::build_agent_supplement(request, defaulted);
		expect(withheld.has_value(), "a session that has not opted in still builds");
		expect(
			withheld->Notes.empty() && withheld->Drafts.empty(),
			"and carries zero notes and zero drafts even though both exist for "
			"the review");
		auto expected   = *core::build_agent_supplement(request);
		expected.Notes  = { };
		expected.Drafts = { };
		expect(
			*withheld == expected,
			"the private prose is dropped and nothing else is: the same "
			"identities, receipts, defect history, and semantic selection "
			"travel exactly as they do without the opt-in");

		const auto onlyNotes = core::build_agent_supplement(
			request,
			{ .Session = request.Session.Id, .Notes = true });
		expect(
			onlyNotes && onlyNotes->Notes == request.SelectedNotes
				&& onlyNotes->Drafts.empty(),
			"notes and drafts are separately opt-in, so seeing the comment "
			"being drafted is not the same permission as reading the notes "
			"taken about the author");

		const auto both = core::build_agent_supplement(
			request,
			{ .Session = request.Session.Id, .Notes = true, .Drafts = true });
		expect(
			both && both->Notes == request.SelectedNotes
				&& both->Drafts == request.SelectedDrafts,
			"an explicit opt-in carries exactly what the reviewer selected, "
			"unchanged");

		const auto stale = core::build_agent_supplement(
			request,
			{
				.Session = core::agent_session_id{ request.Session.Id.value() + 1U },
				.Notes   = true,
				.Drafts  = true,
			});
		expect(
			!stale.has_value(),
			"an opt-in made for another session refuses the whole build: a "
			"permission granted to one conversation cannot be spent by the "
			"next one");
		expect(
			core::admit_private_context(
				request.Session,
				{
					.Session = core::agent_session_id{
						request.Session.Id.value() + 1U },
					.Notes  = true,
					.Drafts = true,
				},
				request.SelectedNotes, request.SelectedDrafts)
				== core::agent_private_context{ },
			"and the admission itself carries nothing across sessions");
		expect(
			core::admit_private_context(
				request.Session,
				{ .Session = request.Session.Id, .Notes = true, .Drafts = true },
				request.SelectedNotes,
				request.SelectedDrafts)
				== core::agent_private_context{
					.Notes  = request.SelectedNotes,
					.Drafts = request.SelectedDrafts,
				},
			"while this session's own opt-in admits exactly what was selected, "
			"so the refusal above is about the identity and not about the "
			"admission refusing everything");

		auto oversize = request;
		oversize.SelectedNotes.assign(
			core::maximum_selected_notes + 1U, { .Text = "note" });
		expect(
			!core::build_agent_supplement(oversize, defaulted).has_value(),
			"a malformed request is still refused whole rather than being "
			"quietly emptied by an opt-in that happened to be off");
	}

	/// \brief The asked-for workshop operation travels, or nothing does.
	void test_a_workshop_operation_travels_and_changes_nothing_else()
	{
		expect(
			!core::build_agent_supplement(minimal_request())
				 ->Operation.has_value(),
			"a supplement built outside a workshop request names no "
			"operation, so absent means none was asked for rather than that "
			"one was dropped");

		auto request      = private_request();
		request.Operation = core::workshop_operation::phrase_as_a_question;
		const auto built  = core::build_agent_supplement(request);
		expect(
			built
				&& built->Operation
					== std::optional{
						core::workshop_operation::phrase_as_a_question },
			"the operation the reviewer asked for travels exactly as asked");

		auto withoutOperation = request;
		withoutOperation.Operation.reset();
		auto expected      = *core::build_agent_supplement(withoutOperation);
		expected.Operation = request.Operation;
		expect(
			*built == expected,
			"and changes nothing else: the same identities, receipts, defect "
			"history, and semantic selection travel beside it");

		auto undeclared      = request;
		undeclared.Operation = static_cast<core::workshop_operation>(
			core::workshop_operation_count);
		expect(
			!core::build_agent_supplement(undeclared).has_value(),
			"an operation nobody declared refuses the whole supplement rather "
			"than being cleared, which would ask the agent to revise a comment "
			"without saying how");
	}

	/// \brief A typed question is admitted for its own session, or not at all.
	void test_a_chat_message_is_admitted_only_for_its_own_session()
	{
		using message                               = core::agent_chat_message;
		[[maybe_unused]] const auto& [keyed, typed] = message{ };
		static_assert(
			std::is_same_v<decltype(keyed), const core::agent_session_id>);
		static_assert(std::is_same_v<decltype(typed), const std::string>);
		static_assert(
			sizeof(message)
				== sizeof(core::agent_session_id) + sizeof(std::string),
			"agent_chat_message gained a field. A message is the session it "
			"was typed into and the words that were typed; a note, a draft, or "
			"a selection here would be a second way to attach what the opt-in "
			"refused.");

		expect(
			core::build_agent_supplement(minimal_request())->Message.empty(),
			"a supplement built outside a chat turn carries no message, so an "
			"empty message means nothing was asked rather than that a question "
			"was dropped");

		auto request    = private_request();
		request.Message = message{
			.Session = request.Session.Id,
			.Text    = "what does this retry budget do?",
		};
		const auto built = core::build_agent_supplement(request);
		expect(
			built && built->Message == request.Message->Text,
			"a message typed into this session travels exactly as typed");

		auto withoutMessage = request;
		withoutMessage.Message.reset();
		auto expected    = *core::build_agent_supplement(withoutMessage);
		expected.Message = request.Message->Text;
		expect(
			*built == expected,
			"and changes nothing else: the same identities, receipts, defect "
			"history, and semantic selection travel beside it");

		const auto withheld = core::build_agent_supplement(
			request, { .Session = request.Session.Id });
		expect(
			withheld && withheld->Message == request.Message->Text
				&& withheld->Notes.empty() && withheld->Drafts.empty(),
			"the message is not a channel for the prose the opt-in refused: a "
			"session that opted into neither still carries its own question and "
			"zero notes and zero drafts");

		auto other    = request;
		other.Message = message{
			.Session = core::agent_session_id{ request.Session.Id.value() + 1U },
			.Text    = "what does this retry budget do?",
		};
		expect(
			!core::build_agent_supplement(other).has_value(),
			"an out-of-session message refuses the whole build: a question "
			"typed into one conversation cannot be sent by the next one");
		expect(
			!core::build_agent_supplement(other, { .Session = other.Session.Id })
				 .has_value(),
			"and the gated build refuses it for the same reason, so an opt-in "
			"this session did make cannot admit a message it did not type");

		auto atBound    = request;
		atBound.Message = message{
			.Session = request.Session.Id,
			.Text    = std::string(core::maximum_chat_message_bytes, 'q'),
		};
		expect(
			core::build_agent_supplement(atBound).has_value(),
			"a message exactly at the bound is admitted, so the refusal below "
			"is about the byte past it");

		auto oversize    = request;
		oversize.Message = message{
			.Session = request.Session.Id,
			.Text    = std::string(core::maximum_chat_message_bytes + 1U, 'q'),
		};
		expect(
			!core::build_agent_supplement(oversize).has_value(),
			"an oversized message is refused whole rather than truncated: half "
			"a question is not the question that was asked");
	}

	void test_invalid_or_oversize_context_is_refused_whole()
	{
		auto request = minimal_request();
		request.SelectedNotes.assign(
			core::maximum_selected_notes + 1U, { .Text = "note" });
		expect(
			!core::build_agent_supplement(request),
			"too many selected notes is refused rather than shortened");

		request = minimal_request();
		request.Files.assign(
			core::maximum_supplement_paths + 1U,
			{
				.File     = identity<core::file_identity>(3U),
				.Material = identity<core::file_material_id>(4U),
				.Path     = "src/a.cpp",
			});
		expect(
			!core::build_agent_supplement(request),
			"too many file states is refused rather than implying omitted files are unchanged");

		request         = minimal_request();
		request.Defects = { defect() };
		request.Defects.front().History.OriginalAnchor.Line += 1U;
		expect(
			!core::build_agent_supplement(request),
			"a substituted original defect anchor is refused");

		request           = minimal_request();
		request.Selection = semantic_context();
		request.Selection->Selection.PreferredLine.Comparison
			= identity<core::file_comparison_digest>(9U);
		expect(
			!core::build_agent_supplement(request),
			"semantic source from another comparison is refused");

		request                    = minimal_request();
		request.Files              = { {
			.File     = identity<core::file_identity>(3U),
			.Material = identity<core::file_material_id>(4U),
			.Path     = "src/a.cpp",
		} };
		request.Files.front().Read = static_cast<core::file_read_state>(255U);
		expect(
			!core::build_agent_supplement(request),
			"an unknown receipt state cannot enter the versioned encoding");

		request           = minimal_request();
		request.Selection = semantic_context();
		request.Selection->Segments.clear();
		expect(
			!core::build_agent_supplement(request),
			"a selected semantic segment absent from the permitted set is refused");
	}
} // namespace

namespace {
	/// \brief [[BPR-1091]]. Review-feedback guidance travels only with a
	/// workshop operation, whole and bounded, or the supplement is refused.
	void test_feedback_guidance_travels_only_with_a_workshop_operation()
	{
		auto guidance    = core::supplement_feedback_guidance{ };
		guidance.Source  = core::feedback_guidance_source::profile_override;
		guidance.Version = "review-feedback-v1";
		guidance.Text    = "Team rule: label every comment.\nKeep questions as questions.";

		auto pressed             = private_request();
		pressed.Operation        = core::workshop_operation::clarify;
		pressed.FeedbackGuidance = guidance;
		const auto built         = core::build_agent_supplement(pressed);
		expect(built && built->FeedbackGuidance == guidance, "a workshop request carries its guidance exactly, source and version included");

		auto withoutGuidance = pressed;
		withoutGuidance.FeedbackGuidance.reset();
		auto expected             = *core::build_agent_supplement(withoutGuidance);
		expected.FeedbackGuidance = guidance;
		expect(*built == expected, "and changes nothing else in the supplement");
		expect(!core::build_agent_supplement(withoutGuidance)->FeedbackGuidance.has_value(), "a press without guidance names none");

		auto unasked = pressed;
		unasked.Operation.reset();
		expect(!core::build_agent_supplement(unasked).has_value(), "guidance on a request that is not a workshop press is refused whole, not dropped");

		auto empty                   = pressed;
		empty.FeedbackGuidance->Text = std::string{ };
		expect(!core::build_agent_supplement(empty).has_value(), "empty guidance is not guidance: the helper being off is said by absence");

		auto oversize                   = pressed;
		oversize.FeedbackGuidance->Text = std::string(core::maximum_feedback_guidance_bytes + 1U, 'x');
		expect(!core::build_agent_supplement(oversize).has_value(), "guidance longer than a hook may be is refused whole rather than truncated");
		oversize.FeedbackGuidance->Text.pop_back();
		expect(core::build_agent_supplement(oversize).has_value(), "and the longest hook text a profile may hold still travels");

		auto unnamed                      = pressed;
		unnamed.FeedbackGuidance->Version = std::string{ };
		expect(!core::build_agent_supplement(unnamed).has_value(), "guidance that names no shipped version is refused");

		auto invalid                   = pressed;
		invalid.FeedbackGuidance->Text = std::string{ "\xff\xfe" };
		expect(!core::build_agent_supplement(invalid).has_value(), "guidance that is not UTF-8 is refused");

		auto undeclared                     = pressed;
		undeclared.FeedbackGuidance->Source = static_cast<core::feedback_guidance_source>(2U);
		expect(!core::build_agent_supplement(undeclared).has_value(), "an undeclared source is refused");
		expect(core::stable_name(core::feedback_guidance_source::shipped) == "shipped" && core::stable_name(core::feedback_guidance_source::profile_override) == "profile_override", "the sources have stable names");
	}
} // namespace

auto main() -> int
{
	test_a_minimal_supplement_preserves_absence();
	test_every_required_local_and_semantic_field_travels_exactly();
	test_absent_and_present_empty_source_text_are_different();
	test_no_field_can_hold_presentation_or_credential_authority();
	test_private_prose_travels_only_where_this_session_opted_in();
	test_a_chat_message_is_admitted_only_for_its_own_session();
	test_a_workshop_operation_travels_and_changes_nothing_else();
	test_feedback_guidance_travels_only_with_a_workshop_operation();
	test_invalid_or_oversize_context_is_refused_whole();
	std::cout << "agent-context-supplement-tests:pass\n";
	return EXIT_SUCCESS;
}
