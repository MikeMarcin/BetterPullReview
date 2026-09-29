#include "bpr/core/review_cycle.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

namespace {
	void expect(const bool aValue, const char* aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle failure: " << aWhat << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename TIdentity>
	auto identity(const std::uint8_t aSeed) -> TIdentity
	{
		auto bytes = std::array<std::uint8_t, 32>{ };
		bytes[0]   = aSeed;
		return TIdentity{ bytes };
	}

	auto authority(const std::uint64_t aProfile, const std::uint8_t aIncarnation) -> bpr::core::local_cycle_authority
	{
		return { .Profile = bpr::core::bpr_profile_id{ aProfile }, .Incarnation = identity<bpr::core::cycle_catalog_incarnation>(aIncarnation) };
	}

	auto capture(const std::uint64_t aScopeVersion, const std::uint8_t aMaterial, const std::uint64_t aEntries,
				 const bpr::core::cycle_capture_coverage aCoverage) -> bpr::core::cycle_capture_observation
	{
		return {
			.ScopeVersion = aScopeVersion,
			.Material     = identity<bpr::core::cycle_material_digest>(aMaterial),
			.Entries      = aEntries,
			.Coverage     = aCoverage,
		};
	}

	auto make_cycle(const std::vector<std::string>& aPaths = { }) -> bpr::core::review_cycle_state
	{
		using namespace bpr::core;
		const auto created = create({ .Authority = authority(1, 1) }, { .Authority = authority(1, 1), .Title = "Parser review", .Scope = { .Paths = aPaths } });
		expect(created.has_value(), "create");
		return created->Cycle;
	}

	auto run(const bpr::core::review_cycle_state& aCycle, bpr::core::review_cycle_change aChange) -> bpr::core::review_cycle_outcome
	{
		return bpr::core::apply(aCycle, { .Subject = aCycle.Subject, .ExpectedRevision = aCycle.Revision, .Change = std::move(aChange) });
	}

	void creates_a_cycle_with_no_provider_and_an_empty_initial_revision()
	{
		using namespace bpr::core;
		const auto catalog = review_cycle_catalog{ .Authority = authority(1, 1) };
		const auto created = create(catalog, { .Authority = authority(1, 1), .Title = "Existing code", .Scope = { } });
		expect(created && created->Catalog.NextCycle == 2 && created->Cycle.Subject.Cycle == review_cycle_id{ 1 }, "first cycle id");
		expect(valid(created->Cycle.Subject) && created->Cycle.Scope.Version == 1, "valid subject");

		// Uncaptured is absent, not empty.
		const auto uncaptured = project(created->Cycle);
		expect(!uncaptured.Captured && !uncaptured.InitialEmpty && !uncaptured.Latest && uncaptured.Revisions == 0, "uncaptured");

		// An empty folder, completely enumerated, is an empty R0.
		const auto empty = run(created->Cycle, record_cycle_capture{ capture(1, 7, 0, cycle_capture_coverage::complete) });
		expect(empty.Refusal == cycle_refusal::none && empty.Changed && empty.Captured, "empty capture");
		const auto view = project(empty.Accepted);
		expect(view.Captured && view.InitialEmpty && view.Revisions == 1, "empty R0");
		expect(empty.Accepted.Initial == empty.Accepted.Latest && !empty.Accepted.Initial->Parent, "R0 has no parent");

		// A partial capture with no entries is not an empty tree.
		const auto partial = run(created->Cycle, record_cycle_capture{ capture(1, 7, 0, cycle_capture_coverage::partial) });
		expect(!project(partial.Accepted).InitialEmpty, "partial is not empty");

		// A second cycle gets the next identity, never a reused one.
		const auto second = create(created->Catalog, { .Authority = authority(1, 1), .Title = "Second", .Scope = { } });
		expect(second && second->Cycle.Subject.Cycle == review_cycle_id{ 2 }, "second cycle id");
	}

	void refuses_invalid_foreign_and_exhausted_identities()
	{
		using namespace bpr::core;
		const auto zeroProfile = create({ .Authority = authority(0, 1) }, { .Authority = authority(0, 1), .Title = "x", .Scope = { } });
		expect(!zeroProfile && zeroProfile.error() == cycle_refusal::invalid_identity, "zero profile");
		const auto zeroIncarnation = create({ .Authority = authority(1, 0) }, { .Authority = authority(1, 0), .Title = "x", .Scope = { } });
		expect(!zeroIncarnation && zeroIncarnation.error() == cycle_refusal::invalid_identity, "zero incarnation");
		const auto otherProfile = create({ .Authority = authority(1, 1) }, { .Authority = authority(2, 1), .Title = "x", .Scope = { } });
		expect(!otherProfile && otherProfile.error() == cycle_refusal::invalid_identity, "unrelated profile");
		const auto full = create({ .Authority = authority(1, 1), .NextCycle = maximum_cycle_identity }, { .Authority = authority(1, 1), .Title = "last", .Scope = { } });
		expect(full && full->Cycle.Subject.Cycle.value() == maximum_cycle_identity, "last identity");
		const auto exhausted = create(full->Catalog, { .Authority = authority(1, 1), .Title = "over", .Scope = { } });
		expect(!exhausted && exhausted.error() == cycle_refusal::exhausted, "cycle overflow");

		const auto cycle = make_cycle();
		// Same cycle number under another profile or incarnation is another subject.
		for (const auto& foreign : { authority(2, 1), authority(1, 2) }) {
			auto subject      = cycle.Subject;
			subject.Authority = foreign;
			const auto result = apply(cycle, { .Subject = subject, .ExpectedRevision = cycle.Revision, .Change = rename_review_cycle{ "stolen" } });
			expect(result.Refusal == cycle_refusal::invalid_identity && result.Accepted == cycle, "foreign authority");
		}
		auto regenerated = cycle.Subject;
		++regenerated.Generation;
		expect(apply(cycle, { .Subject = regenerated, .ExpectedRevision = cycle.Revision, .Change = archive_review_cycle{ } }).Refusal == cycle_refusal::invalid_identity, "stale generation");

		auto exhaustedRevisions                = cycle;
		exhaustedRevisions.Next.SourceRevision = maximum_cycle_identity + 1;
		expect(run(exhaustedRevisions, record_cycle_capture{ capture(1, 1, 1, cycle_capture_coverage::complete) }).Refusal == cycle_refusal::exhausted, "revision overflow");

		// References carry their owning cycle; another cycle's id does not resolve.
		const auto own     = cycle_feedback_item_ref{ .Cycle = cycle.Subject.Cycle, .Id = cycle_feedback_item_id{ 3 } };
		const auto foreign = cycle_feedback_item_ref{ .Cycle = review_cycle_id{ 9 }, .Id = cycle_feedback_item_id{ 3 } };
		const auto zero    = cycle_feedback_item_ref{ .Cycle = cycle.Subject.Cycle, .Id = cycle_feedback_item_id{ 0 } };
		expect(belongs_to(own, cycle.Subject) && !belongs_to(foreign, cycle.Subject) && !belongs_to(zero, cycle.Subject), "cross-cycle reference");
		static_assert(!std::is_convertible_v<cycle_round_id, cycle_round_draft_id>, "distinct identity kinds");
		static_assert(!std::is_convertible_v<cycle_source_revision_ref, cycle_round_ref>, "distinct reference kinds");
	}

	void rejects_stale_revisions_and_archived_mutation()
	{
		using namespace bpr::core;
		const auto cycle   = make_cycle();
		const auto renamed = run(cycle, rename_review_cycle{ "Renamed" });
		expect(renamed.Changed && renamed.Accepted.Revision == 2, "rename advances");
		const auto stale = apply(renamed.Accepted, { .Subject = cycle.Subject, .ExpectedRevision = 1, .Change = rename_review_cycle{ "Late" } });
		expect(stale.Refusal == cycle_refusal::conflict && stale.Accepted == renamed.Accepted, "stale write");
		const auto same = run(renamed.Accepted, rename_review_cycle{ "Renamed" });
		expect(same.Refusal == cycle_refusal::none && !same.Changed && same.Accepted.Revision == 2, "same title no-op");
		expect(run(cycle, rename_review_cycle{ "" }).Refusal == cycle_refusal::bounds, "empty title");
		expect(run(cycle, rename_review_cycle{ std::string(maximum_cycle_title_bytes + 1, 't') }).Refusal == cycle_refusal::bounds, "title bound");

		const auto archived = run(renamed.Accepted, archive_review_cycle{ });
		expect(archived.Changed && archived.Accepted.Lifecycle == cycle_lifecycle::archived, "archive");
		expect(run(archived.Accepted, record_cycle_capture{ capture(1, 1, 1, cycle_capture_coverage::complete) }).Refusal == cycle_refusal::archived, "archived capture");
		expect(!begin_round_draft(archived.Accepted, archived.Accepted.Revision), "archived draft");
		const auto again = run(archived.Accepted, archive_review_cycle{ });
		expect(again.Refusal == cycle_refusal::none && !again.Changed, "archive idempotent");
		const auto restored = run(archived.Accepted, unarchive_review_cycle{ });
		expect(restored.Changed && restored.Accepted.Lifecycle == cycle_lifecycle::active && restored.Accepted.Title == "Renamed", "unarchive keeps content");
	}

	void separates_ordering_from_material_equality()
	{
		using namespace bpr::core;
		const auto first = run(make_cycle(), record_cycle_capture{ capture(1, 5, 3, cycle_capture_coverage::complete) });
		const auto again = run(first.Accepted, record_cycle_capture{ capture(1, 5, 3, cycle_capture_coverage::complete) });
		expect(again.Refusal == cycle_refusal::none && !again.Changed && !again.Captured && again.Accepted == first.Accepted, "identical recapture no-op");

		const auto handoff = run(first.Accepted, record_cycle_capture{ .Capture = capture(1, 5, 3, cycle_capture_coverage::complete), .Deliberate = true });
		expect(handoff.Changed && handoff.Captured && handoff.Captured->Id == cycle_source_revision_id{ 2 }, "deliberate observation");
		expect(same_material(*handoff.Accepted.Initial, *handoff.Accepted.Latest) && handoff.Accepted.Initial->Id != handoff.Accepted.Latest->Id, "same bytes, distinct revisions");
		expect(handoff.Accepted.Latest->Parent == cycle_source_revision_id{ 1 } && handoff.Accepted.Initial->Id.Id == cycle_source_revision_id{ 1 }, "parent link");

		const auto edited = run(handoff.Accepted, record_cycle_capture{ capture(1, 6, 3, cycle_capture_coverage::complete) });
		expect(edited.Captured->Id == cycle_source_revision_id{ 3 } && !same_material(*edited.Accepted.Initial, *edited.Accepted.Latest), "changed bytes");
		expect(project(edited.Accepted).Revisions == 3, "revision count");

		expect(run(first.Accepted, record_cycle_capture{ capture(2, 5, 3, cycle_capture_coverage::complete) }).Refusal == cycle_refusal::invalid_capture, "wrong scope version");
		expect(run(first.Accepted, record_cycle_capture{ capture(1, 0, 3, cycle_capture_coverage::complete) }).Refusal == cycle_refusal::invalid_capture, "zero material");
		expect(run(first.Accepted, record_cycle_capture{ capture(1, 5, maximum_cycle_capture_entries + 1, cycle_capture_coverage::complete) }).Refusal == cycle_refusal::invalid_capture, "entry bound");
	}

	void distinguishes_unknown_source_from_deletion()
	{
		using namespace bpr::core;
		const auto scope = cycle_scope_specification{ .Version = 1, .Paths = { "src/parser", "tests/parser_tests.cpp" } };
		expect(scope_contains(scope, "src/parser/lexer.cpp") && scope_contains(scope, "tests/parser_tests.cpp"), "in scope");
		expect(!scope_contains(scope, "src/parsers/x.cpp") && !scope_contains(scope, "src/main.cpp"), "sibling prefix is outside");

		const auto unlisted = cycle_path_observation{ };
		expect(classify_path(scope, cycle_capture_coverage::partial, "src/parser/lexer.cpp", unlisted) == cycle_path_state::unknown, "partial unlisted is unknown");
		expect(classify_path(scope, cycle_capture_coverage::complete, "src/parser/lexer.cpp", unlisted) == cycle_path_state::absent, "complete unlisted is absent");
		expect(classify_path(scope, cycle_capture_coverage::complete, "src/main.cpp", unlisted) == cycle_path_state::outside_scope, "outside scope");
		expect(classify_path(scope, cycle_capture_coverage::partial, "src/parser/a.cpp", { .ExplicitlyDeleted = true }) == cycle_path_state::deleted, "explicit deletion");
		expect(classify_path(scope, cycle_capture_coverage::complete, "src/parser/a.cpp", { .Listed = true, .Unavailable = true }) == cycle_path_state::unavailable, "unavailable");

		const auto bytesA = std::optional{ identity<cycle_material_digest>(1) };
		const auto bytesB = std::optional{ identity<cycle_material_digest>(2) };
		expect(compare_path(cycle_path_state::present, bytesA, cycle_path_state::unknown, std::nullopt) == cycle_path_change::unknown, "partial is not deletion");
		expect(compare_path(cycle_path_state::present, bytesA, cycle_path_state::outside_scope, std::nullopt) == cycle_path_change::unknown, "narrowed scope is not deletion");
		expect(compare_path(cycle_path_state::present, bytesA, cycle_path_state::unavailable, std::nullopt) == cycle_path_change::unknown, "unavailable is not deletion");
		expect(compare_path(cycle_path_state::present, bytesA, cycle_path_state::absent, std::nullopt) == cycle_path_change::deleted, "complete absence is deletion");
		expect(compare_path(cycle_path_state::present, bytesA, cycle_path_state::deleted, std::nullopt) == cycle_path_change::deleted, "explicit deletion");
		expect(compare_path(cycle_path_state::unknown, std::nullopt, cycle_path_state::present, bytesA) == cycle_path_change::unknown, "unknown before is not addition");
		expect(compare_path(cycle_path_state::absent, std::nullopt, cycle_path_state::present, bytesA) == cycle_path_change::added, "addition");
		expect(compare_path(cycle_path_state::present, bytesA, cycle_path_state::present, bytesA) == cycle_path_change::unchanged, "unchanged");
		expect(compare_path(cycle_path_state::present, bytesA, cycle_path_state::present, bytesB) == cycle_path_change::modified, "modified");

		// Narrowing the scope is a new version, and a capture must name it.
		auto cycle          = make_cycle({ "src" });
		cycle               = run(cycle, record_cycle_capture{ capture(1, 1, 10, cycle_capture_coverage::complete) }).Accepted;
		const auto narrowed = run(cycle, change_cycle_scope{ { "src/parser" } });
		expect(narrowed.Changed && narrowed.Accepted.Scope.Version == 2 && narrowed.Accepted.Latest == cycle.Latest, "scope version, history kept");
		expect(run(narrowed.Accepted, record_cycle_capture{ capture(1, 2, 4, cycle_capture_coverage::complete) }).Refusal == cycle_refusal::invalid_capture, "old scope capture");
		expect(run(narrowed.Accepted, record_cycle_capture{ capture(2, 2, 4, cycle_capture_coverage::complete) }).Changed, "new scope capture");
		const auto unchangedScope = run(narrowed.Accepted, change_cycle_scope{ { "src/parser" } });
		expect(!unchangedScope.Changed && unchangedScope.Accepted.Scope.Version == 2, "same scope no-op");
	}

	void validates_scope_paths()
	{
		using namespace bpr::core;
		expect(validate_scope_paths({ }) == cycle_refusal::none, "whole root");
		expect(validate_scope_paths({ "a", "b/c" }) == cycle_refusal::none, "canonical");
		for (const auto& bad : std::vector<std::vector<std::string>>{
				 { "/abs" }, { "a/" }, { "a//b" }, { "a/../b" }, { "./a" }, { "a\\b" }, { "b", "a" }, { "a", "a" }, { "a", "a/b" }, { "" }, { std::string{ "a\0b", 3 } } }) {
			expect(validate_scope_paths(bad) == cycle_refusal::invalid_scope, "invalid scope path");
		}
		expect(validate_scope_paths(std::vector<std::string>(maximum_cycle_scope_paths + 1, "a")) == cycle_refusal::bounds, "path count bound");
		expect(validate_scope_paths({ std::string(maximum_cycle_scope_path_bytes + 1, 'a') }) == cycle_refusal::invalid_scope, "path byte bound");
		const auto refused = create({ .Authority = authority(1, 1) }, { .Authority = authority(1, 1), .Title = "x", .Scope = { .Paths = { "../escape" } } });
		expect(!refused && refused.error() == cycle_refusal::invalid_scope, "create refuses escape");
	}

	void freezes_immutable_rounds()
	{
		using namespace bpr::core;
		const auto cycle = make_cycle();
		const auto begun = begin_round_draft(cycle, cycle.Revision);
		expect(begun && begun->second.Id.Id == cycle_round_draft_id{ 1 } && begun->first.Revision == cycle.Revision + 1, "draft allocated");
		expect(!begin_round_draft(begun->first, cycle.Revision), "stale draft allocation");
		const auto& [withDraft, draft] = *begun;
		const auto digest              = identity<cycle_round_digest>(4);

		const auto frozen = freeze_round(withDraft, draft, draft.Revision, digest, std::nullopt);
		expect(frozen && !frozen->Reused && frozen->Round.Id.Id == cycle_round_id{ 1 } && frozen->Round.DraftRevision == 1, "freeze");
		const auto repeat = freeze_round(frozen->Cycle, draft, draft.Revision, digest, frozen->Round);
		expect(repeat && repeat->Reused && repeat->Round == frozen->Round && repeat->Cycle == frozen->Cycle, "identical refreeze reuses");
		const auto changedPayload = freeze_round(frozen->Cycle, draft, draft.Revision, identity<cycle_round_digest>(5), frozen->Round);
		expect(!changedPayload && changedPayload.error() == cycle_refusal::conflict, "different payload same revision");

		const auto edited = edit_round_draft(draft, draft.Revision);
		expect(edited && edited->Revision == 2 && !edit_round_draft(*edited, 1), "draft edit revision");
		const auto successor = freeze_round(frozen->Cycle, *edited, edited->Revision, identity<cycle_round_digest>(5), frozen->Round);
		expect(successor && !successor->Reused && successor->Round.Id.Id == cycle_round_id{ 2 } && frozen->Round.DraftRevision == 1, "successor round, original unchanged");

		auto foreignDraft     = draft;
		foreignDraft.Id.Cycle = review_cycle_id{ 9 };
		expect(freeze_round(withDraft, foreignDraft, 1, digest, std::nullopt).error() == cycle_refusal::invalid_identity, "foreign draft");
		expect(freeze_round(withDraft, draft, 1, cycle_round_digest{ }, std::nullopt).error() == cycle_refusal::invalid_identity, "zero digest");
	}

	void keeps_claims_apart_from_judgment()
	{
		using namespace bpr::core;
		auto judgment = cycle_item_judgment{ };
		expect(!judgment.Claim && !judgment.Decision, "no response is not not_attempted");
		judgment = record_claim(judgment, cycle_response_claim::addressed);
		expect(judgment.Claim == cycle_response_claim::addressed && !judgment.Decision, "claim is not verification");
		judgment = record_decision(judgment, cycle_reviewer_decision::needs_work);
		judgment = record_claim(judgment, cycle_response_claim::addressed);
		expect(judgment.Decision == cycle_reviewer_decision::needs_work, "later claim keeps decision");
		judgment = record_decision(judgment, std::nullopt);
		expect(!judgment.Decision && judgment.Claim, "explicit clear");
	}

	void keeps_pull_request_authority_whole()
	{
		using namespace bpr::core;
		auto pr        = pull_request_authority{ };
		pr.PullRequest = pull_request_id{ 42 };
		pr.Number      = pull_request_number{ 7 };
		const auto a   = review_subject{ pr };
		const auto b   = review_subject{ make_cycle().Subject };
		expect(std::holds_alternative<pull_request_authority>(a) && std::get<pull_request_authority>(a) == pr, "pull request subject intact");
		expect(std::holds_alternative<local_cycle_subject>(b) && a != b, "cycle subject distinct");
	}
}

int main()
{
	creates_a_cycle_with_no_provider_and_an_empty_initial_revision();
	refuses_invalid_foreign_and_exhausted_identities();
	rejects_stale_revisions_and_archived_mutation();
	separates_ordering_from_material_equality();
	distinguishes_unknown_source_from_deletion();
	validates_scope_paths();
	freezes_immutable_rounds();
	keeps_claims_apart_from_judgment();
	keeps_pull_request_authority_whole();
	return EXIT_SUCCESS;
}
