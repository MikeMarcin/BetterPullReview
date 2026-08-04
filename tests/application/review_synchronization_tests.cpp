#include "bpr/application/review_synchronization.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto comment(
		std::string         aId,
		std::string         aBody,
		const std::uint64_t aEdit     = 1U,
		const bool          aResolved = false)
		-> application::review_comment_projection
	{
		return application::review_comment_projection{
			.ServerId    = std::move(aId),
			.Body        = std::move(aBody),
			.EditVersion = aEdit,
			.Resolved    = aResolved,
		};
	}

	[[nodiscard]] auto kind_of(
		const application::reconciliation_result& aResult,
		const std::string_view                    aId)
		-> application::reconciliation_kind
	{
		const auto found = std::ranges::find(
			aResult.Entries,
			aId,
			&application::reconciliation_entry::ServerId);
		expect(found != aResult.Entries.end(), "the identity has an entry");
		return found->Kind;
	}

	void test_an_externally_created_comment_appears()
	{
		const auto local    = std::array{ comment("a", "mine") };
		const auto observed = std::array{
			comment("a", "mine"),
			comment("b", "someone else wrote this"),
		};

		const auto result = application::reconcile(local, observed, { });

		expect(result.Projection.size() == 2U, "the projection grows to two");
		expect(
			kind_of(result, "b") == application::reconciliation_kind::added,
			"the unknown comment is reported added");
		expect(
			kind_of(result, "a") == application::reconciliation_kind::unchanged,
			"the untouched comment is not reported changed");
		expect(
			result.Projection.at(1).Body == "someone else wrote this",
			"the added comment carries the server's text");
	}

	// GitHub keeps the identity across an edit, so identity alone cannot tell a
	// reader that the text changed under them.
	void test_an_external_edit_updates_rather_than_duplicating()
	{
		const auto local    = std::array{ comment("a", "before", 1U) };
		const auto observed = std::array{ comment("a", "after", 2U) };

		const auto result = application::reconcile(local, observed, { });

		expect(
			result.Projection.size() == 1U,
			"an edit updates one comment rather than making a second");
		expect(
			result.Projection.front().Body == "after"
				&& result.Projection.front().EditVersion == 2U,
			"the server's text and version win");
		expect(
			kind_of(result, "a") == application::reconciliation_kind::updated,
			"the edit is reported as an update");
	}

	void test_an_external_deletion_removes_the_comment()
	{
		const auto local = std::array{
			comment("a", "kept"),
			comment("b", "deleted elsewhere"),
		};
		const auto observed = std::array{ comment("a", "kept") };

		const auto result = application::reconcile(local, observed, { });

		expect(
			result.Projection.size() == 1U
				&& result.Projection.front().ServerId == "a",
			"a comment the server no longer reports leaves the projection");
		expect(
			kind_of(result, "b") == application::reconciliation_kind::removed,
			"the deletion is reported");
	}

	void test_resolution_and_reopening_round_trip()
	{
		const auto open     = std::array{ comment("a", "text", 1U, false) };
		const auto resolved = std::array{ comment("a", "text", 1U, true) };

		const auto toResolved = application::reconcile(open, resolved, { });
		expect(
			toResolved.Projection.front().Resolved
				&& kind_of(toResolved, "a")
					== application::reconciliation_kind::updated,
			"an externally resolved thread reaches the projection");

		const auto toReopened = application::reconcile(resolved, open, { });
		expect(
			!toReopened.Projection.front().Resolved
				&& kind_of(toReopened, "a")
					== application::reconciliation_kind::updated,
			"reopening reaches it too, in the other direction");
	}

	// The rule this slice exists for: a local assumption never overwrites an
	// external change. The conflict is surfaced, not resolved.
	void test_a_local_edit_never_overwrites_an_external_change()
	{
		const auto local    = std::array{ comment("a", "what I had", 1U) };
		const auto observed = std::array{ comment("a", "what they wrote", 2U) };
		const auto editing  = std::array{ std::string{ "a" } };

		const auto result = application::reconcile(local, observed, editing);

		expect(
			result.Projection.front().Body == "what they wrote"
				&& result.Projection.front().EditVersion == 2U,
			"the server's text becomes the projection, not the local edit");
		expect(
			kind_of(result, "a") == application::reconciliation_kind::conflicted,
			"the collision is reported as a conflict rather than an update");
		expect(
			result.conflicts() == std::vector<std::string>{ "a" },
			"the conflict is listed so a person can decide");
	}

	void test_a_local_edit_on_an_unchanged_comment_is_not_a_conflict()
	{
		const auto same    = std::array{ comment("a", "text", 1U) };
		const auto editing = std::array{ std::string{ "a" } };

		const auto result = application::reconcile(same, same, editing);

		expect(
			kind_of(result, "a") == application::reconciliation_kind::unchanged,
			"holding an unpublished edit is not itself a conflict");
		expect(
			result.conflicts().empty(),
			"nothing is reported for a comment the server did not touch");
	}

	void test_a_refresh_of_the_same_state_is_identical_not_merely_equivalent()
	{
		const auto local = std::array{
			comment("c", "third"),
			comment("a", "first"),
			comment("b", "second"),
		};
		const auto observed = std::array{
			comment("b", "second"),
			comment("c", "third"),
			comment("a", "first"),
		};

		const auto first  = application::reconcile(local, observed, { });
		const auto second = application::reconcile(local, observed, { });

		expect(first == second, "two refreshes of one state agree exactly");
		expect(
			first.Projection.at(0).ServerId == "a"
				&& first.Projection.at(1).ServerId == "b"
				&& first.Projection.at(2).ServerId == "c",
			"the projection is ordered by identity rather than by arrival");
		expect(
			std::ranges::all_of(
				first.Entries,
				[](const application::reconciliation_entry& aEntry) {
					return aEntry.Kind
						== application::reconciliation_kind::unchanged;
				}),
			"a refresh that changed nothing reports nothing changed");
	}

	void test_every_enumerator_names_itself()
	{
		expect(
			application::name(application::reconciliation_kind::conflicted)
					== "conflicted"
				&& application::name(application::reconciliation_kind::removed)
					== "removed"
				&& application::name(application::reconciliation_kind::added)
					== "added",
			"every reconciliation kind names itself");
	}
} // namespace

auto main() -> int
{
	test_an_externally_created_comment_appears();
	test_an_external_edit_updates_rather_than_duplicating();
	test_an_external_deletion_removes_the_comment();
	test_resolution_and_reopening_round_trip();
	test_a_local_edit_never_overwrites_an_external_change();
	test_a_local_edit_on_an_unchanged_comment_is_not_a_conflict();
	test_a_refresh_of_the_same_state_is_identical_not_merely_equivalent();
	test_every_enumerator_names_itself();

	std::cout << "review-synchronization-tests:pass\n";
	return EXIT_SUCCESS;
}
