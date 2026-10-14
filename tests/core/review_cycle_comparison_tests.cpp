#include "bpr/core/diff_compute.hpp"
#include "product_diff.hpp"
#include "bpr/core/review_cycle_comparison.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	void expect(const bool aValue, const char* aWhat)
	{
		if (!aValue) {
			std::cerr << "review cycle comparison failure: " << aWhat << '\n';
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

	auto make_cycle() -> review_cycle_state
	{
		const auto authority = local_cycle_authority{ .Profile = bpr_profile_id{ 1 }, .Incarnation = identity<cycle_catalog_incarnation>(1) };
		return create({ .Authority = authority }, { .Authority = authority, .Title = "Views", .Scope = { } })->Cycle;
	}

	auto captured(const review_cycle_state& aCycle, const std::uint8_t aMaterial) -> review_cycle_state
	{
		const auto outcome = apply(aCycle, { .Subject = aCycle.Subject, .ExpectedRevision = aCycle.Revision, .Change = record_cycle_capture{ .Capture = { .ScopeVersion = 1, .Material = identity<cycle_material_digest>(aMaterial), .Entries = 1, .Coverage = cycle_capture_coverage::complete } } });
		expect(outcome.Refusal == cycle_refusal::none, "capture");
		return outcome.Accepted;
	}

	auto id(const std::uint64_t aValue) -> cycle_source_revision_id
	{
		return cycle_source_revision_id{ aValue };
	}

	void resolves_views_exactly()
	{
		const auto empty = make_cycle();
		expect(resolve_comparison(empty, all_cycle_view{ }).error() == cycle_comparison_error::uncaptured, "no views before R0");
		auto       cycle = captured(captured(empty, 1), 2);
		const auto r1    = cycle.Initial->Id;
		const auto r2    = cycle.Latest->Id;
		expect(resolve_comparison(cycle, full_source_view{ id(1) }) == cycle_comparison_pair{ .Right = r1 }, "full source is one revision");
		expect(resolve_comparison(cycle, all_cycle_view{ }) == cycle_comparison_pair{ .Left = r1, .Right = r2 }, "all cycle");
		expect(resolve_comparison(cycle, since_checkpoint_view{ id(1) }) == cycle_comparison_pair{ .Left = r1, .Right = r2 }, "since the reviewer's checkpoint");
		expect(resolve_comparison(cycle, this_round_view{ .Start = id(1) }) == cycle_comparison_pair{ .Left = r1, .Right = r2 }, "this round to latest");
		expect(resolve_comparison(cycle, this_round_view{ .Start = id(1), .Result = id(1) }) == cycle_comparison_pair{ .Left = r1, .Right = r1 }, "this round with a named result");
		expect(resolve_comparison(cycle, revision_pair_view{ id(2), id(1) }) == cycle_comparison_pair{ .Left = r2, .Right = r1 }, "an exact pair in either order");
		expect(resolve_comparison(cycle, full_source_view{ id(3) }).error() == cycle_comparison_error::unknown_revision, "unallocated revision");
		expect(resolve_comparison(cycle, revision_pair_view{ id(0), id(1) }).error() == cycle_comparison_error::unknown_revision, "zero revision");

		// Explicit views never move; semantic views move only on refresh.
		const auto pinned = *open_comparison(cycle, revision_pair_view{ id(1), id(2) });
		const auto alias  = *open_comparison(cycle, all_cycle_view{ });
		const auto round  = *open_comparison(cycle, this_round_view{ .Start = id(1), .Result = id(2) });
		expect(!newer_available(pinned, cycle) && !newer_available(alias, cycle), "nothing newer yet");
		cycle = captured(cycle, 3);
		expect(newer_available(pinned, cycle) && refresh_comparison(pinned, cycle) == pinned, "a pinned pair signals but never retargets");
		expect(newer_available(round, cycle) && refresh_comparison(round, cycle) == round, "a named round result is pinned");
		expect(newer_available(alias, cycle), "the alias would move");
		expect(refresh_comparison(alias, cycle).Pair == cycle_comparison_pair{ .Left = r1, .Right = cycle.Latest->Id }, "refresh moves the alias to the latest");
		expect(explicit_view(full_source_view{ id(1) }) && !explicit_view(since_checkpoint_view{ id(1) }), "explicit views");
	}

	auto file_entry(const std::string& aPath, const std::uint8_t aContent, const std::uint32_t aMode = git_mode_regular) -> manifest_entry
	{
		return { .Path = aPath, .Mode = aMode, .Size = 4, .Content = identity<cycle_material_digest>(aContent), .Profile = { .Kind = material_text_kind::utf8, .Size = 4 } };
	}

	void compares_manifests_path_by_path()
	{
		auto before      = cycle_manifest{ .ScopeVersion = 1, .Scope = { "src" }, .Coverage = cycle_capture_coverage::complete };
		before.Entries   = { file_entry("src/a.txt", 1), file_entry("src/b.sh", 2), file_entry("src/gen/out.txt", 3), file_entry("src/moved.txt", 5), file_entry("src/same.txt", 6) };
		before.Deletions = { "src/was-deleted.txt" };
		auto after       = cycle_manifest{ .ScopeVersion = 2, .Scope = { }, .Coverage = cycle_capture_coverage::complete };
		after.Entries    = { file_entry("docs/new.md", 7), file_entry("src/a.txt", 4), file_entry("src/b.sh", 2, git_mode_executable), file_entry("src/renamed.txt", 5), file_entry("src/same.txt", 6) };
		after.Exclusions = { { .Path = "src/gen", .Directory = true, .Reason = workspace_exclusion_reason::ignored } };

		const auto page = compare_manifests(&before, after, std::nullopt, 100, false);
		const auto find = [&page](const std::string_view aPath) -> const cycle_path_comparison& {
			for (const auto& item : page.Entries) {
				if (item.Path == aPath) {
					return item;
				}
			}
			expect(false, "path on the page");
			return page.Entries.front();
		};
		expect(page.Entries.size() == 8 && !page.Next, "every path of both manifests, once");
		expect(find("src/a.txt").Change == cycle_path_change::modified, "modified");
		expect(find("src/b.sh").Change == cycle_path_change::unchanged && find("src/b.sh").ModeChanged && find("src/b.sh").changed(), "same bytes, mode change");
		expect(find("src/same.txt").Change == cycle_path_change::unchanged && !find("src/same.txt").changed(), "content-equivalent path is unchanged");
		expect(find("src/moved.txt").Change == cycle_path_change::deleted && find("src/renamed.txt").Change == cycle_path_change::added, "a rename is a deletion and an addition");
		expect(find("src/moved.txt").Left->Content == find("src/renamed.txt").Right.Content, "with the same content identity");
		expect(find("docs/new.md").Change == cycle_path_change::unknown && find("docs/new.md").Left->State == cycle_path_state::outside_scope, "an expanded scope is not an addition");
		expect(find("src/gen/out.txt").Right.State == cycle_path_state::unavailable && find("src/gen/out.txt").Change == cycle_path_change::unknown, "an exclusion is not a deletion");
		expect(find("src/was-deleted.txt").Left->State == cycle_path_state::deleted && find("src/was-deleted.txt").Change == cycle_path_change::unchanged, "deleted then absent");

		const auto changedOnly = compare_manifests(&before, after, std::nullopt, 100, true);
		expect(changedOnly.Entries.size() == 6, "changed-only skips unchanged paths");

		// Pages are bounded and resumable.
		auto collected = std::vector<std::string>{ };
		auto cursor    = std::optional<std::string>{ };
		for (auto pages = 0; pages < 10; ++pages) {
			const auto next = compare_manifests(&before, after, cursor, 3, false);
			for (const auto& item : next.Entries) {
				collected.push_back(item.Path);
			}
			if (!next.Next) {
				break;
			}
			cursor = next.Next;
		}
		auto expected = std::vector<std::string>{ };
		for (const auto& item : page.Entries) {
			expected.push_back(item.Path);
		}
		expect(collected == expected, "paging covers every path once, in order");
		expect(compare_manifests(&before, after, std::nullopt, 0, false).Entries.size() == 1, "a zero limit still makes progress");

		const auto full = compare_manifests(nullptr, after, std::nullopt, 100, true);
		expect(full.Entries.size() == 5 && !full.Entries.front().Change && !full.Entries.front().Left, "full source lists one side, no change claims");
	}

	auto bytes(const std::string_view aText) -> std::span<const std::uint8_t>
	{
		return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
	}

	void keys_comparisons_by_cycle_identity()
	{
		const auto cycle = captured(captured(make_cycle(), 1), 2);
		const auto pair  = cycle_comparison_pair{ .Left = cycle.Initial->Id, .Right = cycle.Latest->Id };
		const auto left  = identity<cycle_material_digest>(1);
		const auto right = identity<cycle_material_digest>(2);
		const auto key   = cycle_file_comparison(cycle.Subject, pair, "a.txt", left, right, { });
		expect(key && valid(*key) && key->LeftPresent && key->RightPresent, "key for a pair");
		expect(cycle_file_comparison(cycle.Subject, pair, "a.txt", left, right, { }) == key, "deterministic");
		expect(cycle_file_comparison(cycle.Subject, pair, "b.txt", left, right, { })->Comparison != key->Comparison, "path is part of the identity");
		expect(cycle_file_comparison(cycle.Subject, { .Left = pair.Right, .Right = *pair.Left }, "a.txt", left, right, { })->Comparison != key->Comparison, "pair order is part of the identity");
		expect(!cycle_file_comparison(cycle.Subject, pair, "a.txt", std::nullopt, std::nullopt, { }), "nothing to compare");
		const auto single = cycle_file_comparison(cycle.Subject, { .Right = pair.Right }, "a.txt", left, right, { });
		expect(single && !single->LeftPresent, "full source has no left side");

		// The shared engine: the same alignment as a pull-request comparison
		// of the same bytes, under a different identity.
		const auto before = std::string_view{ "one\ntwo\nthree\n" };
		const auto after  = std::string_view{ "one\n2\nthree\nfour\n" };
		const auto result = bpr::tests::product_material_diff({ .Key = *key, .Options = { }, .LeftBytes = bytes(before), .RightBytes = bytes(after) });
		expect(result.published() && result.Reason == diff_reason_code::none && validate_computed_material_diff(*result.Result).valid(), "material diff computed and valid");
		expect(result.Result->Alignment.LeftLineCount == 3 && result.Result->Alignment.RightLineCount == 4, "line counts");
		expect(result.Result->LeftDocument->Bytes == std::vector<std::uint8_t>(before.begin(), before.end()), "documents hold the exact bytes");
		const auto binary = bpr::tests::product_material_diff({ .Key = *key, .LeftBytes = bytes(std::string_view{ "a\0b", 3 }), .RightBytes = bytes(after) });
		expect(binary.Reason == diff_reason_code::binary_file && binary.published(), "binary through the same rules");
		auto broken       = *key;
		broken.Comparison = { };
		expect(bpr::tests::product_material_diff({ .Key = broken, .LeftBytes = bytes(before), .RightBytes = bytes(after) }).Reason == diff_reason_code::invalid_alignment, "an invalid key is refused");
		expect(!bpr::tests::product_material_diff({ .Key = *key, .LeftBytes = std::nullopt, .RightBytes = bytes(after) }).Result->LeftDocument, "a missing side is reported, not invented");
	}

	void keeps_previous_content_while_loading()
	{
		const auto a                  = cycle_file_view_key{ .Path = "a.txt" };
		const auto b                  = cycle_file_view_key{ .Path = "b.txt" };
		auto [first, requestA]        = request_file({ }, a);
		auto [second, requestB]       = request_file(first, b);
		const auto [late, lateResult] = complete_file(second, requestA, true);
		expect(lateResult == cycle_file_view_result::stale && !late.Shown && late.Pending, "a late completion for a superseded request is dropped");
		const auto [shown, shownResult] = complete_file(late, requestB, true);
		expect(shownResult == cycle_file_view_result::shown && shown.Shown == b && !shown.Pending, "the current request is shown");
		auto [loading, requestC] = request_file(shown, a);
		expect(loading.Shown == b, "the previous content stays while loading");
		const auto [kept, keptResult] = complete_file(loading, requestC, false);
		expect(keptResult == cycle_file_view_result::kept && kept.Shown == b, "a failure keeps the previous content");
		auto [again, requestD] = request_file(kept, a);
		const auto cancelled   = cancel_file(again);
		expect(cancelled.Shown == b && !cancelled.Pending && complete_file(cancelled, requestD, true).second == cycle_file_view_result::stale, "cancellation keeps the previous content");
	}
}

int main()
{
	resolves_views_exactly();
	compares_manifests_path_by_path();
	keys_comparisons_by_cycle_identity();
	keeps_previous_content_while_loading();
	return EXIT_SUCCESS;
}
