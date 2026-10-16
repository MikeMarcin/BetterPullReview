// [[BPR-1557]] The Updates line starts with the pull request's base, the anchor
// Whole PR compares from. The core decides where a compared anchor sits on the
// line, which places each handle can take and where a step goes; the line only
// draws. The base is the left handle's first stop and never the right's; an
// older update's own base keeps its place before that update; an update GitHub
// recorded and this install has not imported is a stop with no anchor yet.

#include "bpr/core/update_line.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	template <typename Identity>
	[[nodiscard]] auto opaque(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32> bytes{ };
		bytes[0]  = aSeed;
		bytes[31] = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	[[nodiscard]] auto epoch(const std::int64_t aOrdinal, const std::uint8_t aAuthority = 7U) -> review_epoch_id
	{
		return review_epoch_id{
			.Authority   = opaque<pull_request_authority_scope_id>(aAuthority),
			.Incarnation = opaque<epoch_catalog_incarnation_id>(9U),
			.Ordinal     = epoch_ordinal{ aOrdinal },
		};
	}

	[[nodiscard]] auto anchor_of(const review_epoch_id& aEpoch, const comparison_anchor_kind aKind) -> comparison_anchor_identity
	{
		return comparison_anchor_identity{ .Authority = aEpoch.Authority, .Incarnation = aEpoch.Incarnation, .Kind = aKind, .Epoch = aEpoch.Ordinal };
	}

	[[nodiscard]] auto base() -> update_line_place
	{
		return update_line_place{ .Kind = update_line_place_kind::pull_request_base };
	}

	[[nodiscard]] auto head(const std::size_t aUpdate) -> update_line_place
	{
		return update_line_place{ .Kind = update_line_place_kind::update_head, .Update = aUpdate };
	}

	[[nodiscard]] auto own_base(const std::size_t aUpdate) -> update_line_place
	{
		return update_line_place{ .Kind = update_line_place_kind::update_base, .Update = aUpdate };
	}

	// Three updates: a retained one, one GitHub recorded and not imported, and
	// the newest, whose base is the pull request's base.
	[[nodiscard]] auto updates() -> const std::array<update_line_update, 3U>&
	{
		static const auto held = std::array<update_line_update, 3U>{
			update_line_update{ .Epoch = epoch(1), .HasBase = true },
			update_line_update{ .Importable = true },
			update_line_update{ .Epoch = epoch(3), .HasBase = true },
		};
		return held;
	}

	[[nodiscard]] auto line() -> update_line_facts
	{
		return update_line_facts{ .PullRequestBase = anchor_of(epoch(3), comparison_anchor_kind::epoch_base), .Updates = updates() };
	}

	auto test_whole_pr_sits_on_the_base_and_the_newest_update() -> void
	{
		const auto facts = line();
		expect(place_on_update_line(facts, *facts.PullRequestBase) == base(),
			   "the whole PR's left side sits on the base at the line's start, not on the newest update");
		expect(place_on_update_line(facts, anchor_of(epoch(3), comparison_anchor_kind::epoch_head)) == head(2U),
			   "the whole PR's right side sits on the newest update");
		expect(place_on_update_line(facts, anchor_of(epoch(1), comparison_anchor_kind::epoch_base)) == own_base(0U),
			   "an older update's own base keeps its place before that update");
		expect(place_on_update_line(facts, anchor_of(epoch(1), comparison_anchor_kind::epoch_head)) == head(0U), "a retained head is on its node");
		expect(!place_on_update_line(facts, anchor_of(epoch(3, 8U), comparison_anchor_kind::epoch_head)),
			   "another authority's anchor is not on the line");
		expect(!place_on_update_line(facts, comparison_anchor_identity{ .Authority = epoch(3).Authority, .Incarnation = epoch(3).Incarnation, .Kind = comparison_anchor_kind::review_commit }),
			   "a review commit is not a place on the line");
		expect(!place_on_update_line(facts, anchor_of(epoch(5), comparison_anchor_kind::epoch_base)),
			   "a paged-out epoch's base is not a place on the line");
	}

	auto test_the_base_is_the_left_handles_first_stop_only() -> void
	{
		const auto facts = line();
		expect(update_line_stops(facts, comparison_side::left) == std::vector{ base(), head(0U), head(1U), head(2U) },
			   "the left handle can take the base and every update's head, an importable one included");
		expect(update_line_stops(facts, comparison_side::right) == std::vector{ head(0U), head(1U), head(2U) },
			   "the right handle never takes the base");
		expect(update_line_anchor(facts, base()) == facts.PullRequestBase, "the base stands for the published anchor");
		expect(!update_line_anchor(facts, head(1U)), "an update not yet imported has no anchor");
		expect(update_line_anchor(facts, own_base(2U)) == anchor_of(epoch(3), comparison_anchor_kind::epoch_base), "an update's own base is its epoch's base");

		// Before the core has named the base, the oldest update's own base is
		// the left handle's first stop, as the line had it before.
		const auto unnamed = update_line_facts{ .Updates = updates() };
		expect(update_line_stops(unnamed, comparison_side::left) == std::vector{ own_base(0U), head(0U), head(1U), head(2U) },
			   "with no published base the oldest update's base leads the left stops");
		expect(!update_line_anchor(unnamed, base()), "with no published base the base place stands for nothing");
	}

	auto test_steps_follow_the_line() -> void
	{
		const auto facts = line();
		expect(step_on_update_line(facts, comparison_side::left, head(0U), -1) == base(), "Left from the oldest head reaches the base");
		expect(!step_on_update_line(facts, comparison_side::left, base(), -1), "nothing is before the base");
		expect(step_on_update_line(facts, comparison_side::left, base(), +1) == head(0U), "Right from the base reaches the oldest update");
		expect(step_on_update_line(facts, comparison_side::left, own_base(2U), -1) == head(1U)
				   && step_on_update_line(facts, comparison_side::left, own_base(2U), +1) == head(2U),
			   "from an update's own base a step goes to the head before or after it");
		expect(!step_on_update_line(facts, comparison_side::right, head(0U), -1), "the right handle stops at the oldest update");
		expect(step_on_update_line(facts, comparison_side::right, std::nullopt, -1) == head(0U)
				   && step_on_update_line(facts, comparison_side::left, std::nullopt, -1) == base(),
			   "with nowhere to start, a step goes to the first stop");
		expect(step_on_update_line(facts, comparison_side::left, std::nullopt, +1) == head(2U), "or to the last one");
	}

	auto test_one_update_keeps_the_handles_apart() -> void
	{
		const auto one   = std::array<update_line_update, 1U>{ update_line_update{ .Epoch = epoch(1), .HasBase = true } };
		const auto facts = update_line_facts{ .PullRequestBase = anchor_of(epoch(1), comparison_anchor_kind::epoch_base), .Updates = one };
		const auto left  = place_on_update_line(facts, *facts.PullRequestBase);
		const auto right = place_on_update_line(facts, anchor_of(epoch(1), comparison_anchor_kind::epoch_head));
		expect(left == base() && right == head(0U) && left != right,
			   "with one update the whole PR is the base to that update: two places, not one node");
	}
} // namespace

int main()
{
	test_whole_pr_sits_on_the_base_and_the_newest_update();
	test_the_base_is_the_left_handles_first_stop_only();
	test_steps_follow_the_line();
	test_one_update_keeps_the_handles_apart();
	std::cout << "BPR-1557 update line tests passed\n";
	return EXIT_SUCCESS;
}
