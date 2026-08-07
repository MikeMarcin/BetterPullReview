/// \file
/// \brief Proves the supplement carries only what the reviewer chose.

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

	[[nodiscard]] auto commit(const std::uint8_t aSeed) -> core::git_commit_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (auto index = std::size_t{ 0U }; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_commit_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto binding() -> core::agent_revision_binding
	{
		return {
			.Host        = core::git_host_id{ 10U },
			.Repository  = core::repository_id{ 20U },
			.PullRequest = core::pull_request_id{ 30U },
			.BaseOid     = commit(1U),
			.HeadOid     = commit(2U),
			.LeftEpoch   = core::epoch_ordinal{ 1 },
			.RightEpoch  = core::epoch_ordinal{ 2 },
		};
	}

	/// \brief Nothing private travels unless the reviewer named it.
	///
	/// \details The whole supplement is read, not one field. A test that
	/// checked `Notes.empty()` alone would pass against a supplement that had
	/// put the reviewer's draft somewhere else.
	void test_a_default_supplement_carries_nothing_private()
	{
		const auto built = core::build_agent_supplement({ .Binding = binding() });
		expect(built.has_value(), "a minimal request builds");

		// Every field, by name. Adding one and forgetting it here is a compile
		// error on the aggregate below, not a silent omission.
		const auto expected = core::agent_context_supplement{
			.SchemaVersion      = core::agent_supplement_schema_version,
			.Binding            = binding(),
			.LastReviewedHead   = std::nullopt,
			.ChangedSinceReview = { },
			.Defects            = { },
			.SelectedFile       = std::nullopt,
			.Notes              = { },
		};
		expect(
			*built == expected,
			"a supplement built without explicit selection equals the empty "
			"one, field for field - there is nowhere else a note could be");
	}

	/// \brief A selected note travels, and only a selected one.
	void test_only_selected_notes_travel()
	{
		auto request = core::agent_supplement_request{ .Binding = binding() };
		request.SelectedNotes.push_back({ .Text = "check the retry path" });

		const auto built = core::build_agent_supplement(request);
		expect(built.has_value(), "the request builds");
		expect(
			built->Notes.size() == 1U
				&& built->Notes.front().Text == "check the retry path",
			"a note the reviewer selected reaches the agent");

		// And the same request without it carries none, so the assertion above
		// is about the selection rather than about notes always appearing.
		request.SelectedNotes.clear();
		const auto without = core::build_agent_supplement(request);
		expect(
			without.has_value() && without->Notes.empty(),
			"and the same request without the selection carries no note");
	}

	/// \brief Every supplement carries the identity it is about.
	void test_every_supplement_carries_its_identity()
	{
		const auto built = core::build_agent_supplement({ .Binding = binding() });
		expect(built.has_value(), "the supplement builds");
		expect(
			built->SchemaVersion == core::agent_supplement_schema_version
				&& built->SchemaVersion != 0U,
			"the schema version is carried and is not the zero a default would "
			"leave");
		expect(
			built->Binding.Host == binding().Host
				&& built->Binding.Repository == binding().Repository
				&& built->Binding.PullRequest == binding().PullRequest
				&& built->Binding.BaseOid == binding().BaseOid
				&& built->Binding.HeadOid == binding().HeadOid
				&& built->Binding.LeftEpoch == binding().LeftEpoch
				&& built->Binding.RightEpoch == binding().RightEpoch,
			"host, repository, pull request, both OIDs, and the epoch pair all "
			"travel");
	}

	/// \brief No field of the supplement can hold a credential.
	void test_no_field_can_hold_a_credential()
	{
		using supplement = core::agent_context_supplement;
		static_assert(
			std::is_same_v<decltype(supplement::SchemaVersion), std::uint32_t>);
		static_assert(std::is_same_v<
					  decltype(supplement::Binding),
					  core::agent_revision_binding>);
		static_assert(std::is_same_v<
					  decltype(supplement::LastReviewedHead),
					  std::optional<core::git_commit_oid>>);
		static_assert(std::is_same_v<
					  decltype(supplement::ChangedSinceReview),
					  std::vector<core::file_identity>>);
		static_assert(std::is_same_v<
					  decltype(supplement::Defects),
					  std::vector<core::supplement_defect>>);
		static_assert(std::is_same_v<
					  decltype(supplement::SelectedFile),
					  std::optional<core::file_identity>>);
		static_assert(std::is_same_v<
					  decltype(supplement::Notes),
					  std::vector<core::supplement_note>>);
		// Every field, bound by name. A structured binding over an aggregate
		// requires the exact count, so adding a field stops this compiling and
		// the next person has to come here and say why theirs cannot hold a
		// credential or an unselected note.
		//
		// Deliberately not a `sizeof` sum: that has to be hand-computed with
		// padding, which is fragile in the direction that matters — it fails
		// for the wrong reason on a layout change and could pass for a field
		// swapped with one of equal size.
		const auto whole = supplement{ };
		const auto& [version, revision, lastHead, changed, defects, selected, notes]
			= whole;
		static_assert(std::is_same_v<decltype(version), const std::uint32_t>);
		static_assert(std::is_same_v<
					  decltype(revision),
					  const core::agent_revision_binding>);
		static_assert(std::is_same_v<
					  decltype(notes),
					  const std::vector<core::supplement_note>>);
		expect(
			version == core::agent_supplement_schema_version && notes.empty()
				&& !lastHead.has_value() && changed.empty() && defects.empty()
				&& !selected.has_value(),
			"a default supplement carries its schema version and nothing "
			"else");

		// The request is the only source, and it has no credential field
		// either — so there is nothing upstream for one to arrive from.
		using request = core::agent_supplement_request;
		static_assert(std::is_same_v<
					  decltype(request::SelectedNotes),
					  std::vector<core::supplement_note>>);
		static_assert(
			std::is_same_v<decltype(core::supplement_note::Text), std::string>,
			"a note is text the reviewer wrote; giving it a key and a value "
			"would make it somewhere a secret fits");
	}

	/// \brief An oversize request is refused whole rather than truncated.
	void test_an_oversize_request_is_refused()
	{
		auto request = core::agent_supplement_request{ .Binding = binding() };
		request.SelectedNotes.assign(
			core::maximum_selected_notes + 1U, { .Text = "note" });
		expect(
			!core::build_agent_supplement(request).has_value(),
			"too many notes is refused rather than shortened");

		request.SelectedNotes.assign(1U, { .Text = "note" });
		expect(
			core::build_agent_supplement(request).has_value(),
			"and a request inside the bound still builds, so the refusal is "
			"about the bound");

		auto paths = core::agent_supplement_request{ .Binding = binding() };
		paths.ChangedSinceReview.assign(
			core::maximum_supplement_paths + 1U, core::file_identity{ });
		expect(
			!core::build_agent_supplement(paths).has_value(),
			"a shortened list of changed files would tell an agent that a file "
			"it was not shown is unchanged, so it is refused whole");
	}
} // namespace

auto main() -> int
{
	test_a_default_supplement_carries_nothing_private();
	test_only_selected_notes_travel();
	test_every_supplement_carries_its_identity();
	test_no_field_can_hold_a_credential();
	test_an_oversize_request_is_refused();
	std::cout << "agent-context-supplement-tests:pass\n";
	return EXIT_SUCCESS;
}
