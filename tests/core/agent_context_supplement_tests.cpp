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
		using supplement     = core::agent_context_supplement;
		const auto whole     = supplement{ };
		const auto& [version,
					 applicationSession,
					 run,
					 url,
					 previous,
					 lastReviewed,
					 files,
					 defects,
					 selection,
					 notes,
					 drafts] = whole;
		static_assert(std::is_same_v<decltype(version), const std::uint32_t>);
		static_assert(std::is_same_v<
					  decltype(applicationSession),
					  const core::application_session_id>);
		static_assert(std::is_same_v<decltype(run), const core::agent_session>);
		static_assert(std::is_same_v<
					  decltype(selection),
					  const std::optional<core::supplement_semantic_context>>);
		static_assert(std::is_same_v<
					  decltype(notes), const std::vector<core::supplement_note>>);
		static_assert(std::is_same_v<
					  decltype(drafts), const std::vector<core::supplement_draft>>);
		expect(
			version == core::agent_supplement_schema_version
				&& applicationSession.value() == 0U && run.Id.value() == 0U
				&& url.empty() && !previous && !lastReviewed && files.empty()
				&& defects.empty() && !selection && notes.empty() && drafts.empty(),
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

auto main() -> int
{
	test_a_minimal_supplement_preserves_absence();
	test_every_required_local_and_semantic_field_travels_exactly();
	test_absent_and_present_empty_source_text_are_different();
	test_no_field_can_hold_presentation_or_credential_authority();
	test_invalid_or_oversize_context_is_refused_whole();
	std::cout << "agent-context-supplement-tests:pass\n";
	return EXIT_SUCCESS;
}
