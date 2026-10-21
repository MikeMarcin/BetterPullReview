#include "bpr/core/home_ignore.hpp"
#include "bpr/core/review_queue_attention.hpp"

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

	[[nodiscard]] auto ref(const std::int64_t aNumber, const std::string& aAccount = "reviewer") -> core::pull_request_ref
	{
		auto value       = core::pull_request_ref{ };
		value.Host       = "github.com";
		value.Account    = aAccount;
		value.Repository = "octo/app";
		value.Number     = aNumber;
		return value;
	}

	[[nodiscard]] auto item(const std::int64_t aNumber, const core::attention_bucket aBucket, const bool aIgnored) -> core::queue_item
	{
		auto value    = core::queue_item{ };
		value.Ref     = ref(aNumber);
		value.Bucket  = aBucket;
		value.Ignored = aIgnored;
		return value;
	}

	[[nodiscard]] auto ignore(const core::pull_request_ref& aRef) -> core::home_ignore_intent
	{
		return { .Ref = aRef, .Action = core::home_ignore_action::ignore };
	}

	[[nodiscard]] auto unignore(const core::pull_request_ref& aRef) -> core::home_ignore_intent
	{
		return { .Ref = aRef, .Action = core::home_ignore_action::unignore };
	}

	constexpr auto every_bucket = std::array{
		core::attention_bucket::initial_review_requested,
		core::attention_bucket::changed_since_my_review,
		core::attention_bucket::new_comments_or_author_replies,
		core::attention_bucket::fixes_awaiting_verification,
		core::attention_bucket::authored_awaiting_reviewer_action,
		core::attention_bucket::observed_with_new_activity,
		core::attention_bucket::quiet,
		core::attention_bucket::detail_unknown,
	};

	void test_an_ignored_item_is_in_ignored_whatever_its_bucket()
	{
		for (const auto bucket : every_bucket) {
			auto active = item(1, bucket, false);
			expect(core::home_section_for(active) == core::home_section_for(active.Membership, bucket),
				   "an item that is not ignored keeps its membership's section");
			expect(core::home_section_for(active) != core::home_queue_section::ignored, "no bucket alone is ignored");
			auto ignored = item(1, bucket, true);
			expect(core::home_section_for(ignored) == core::home_queue_section::ignored, "an ignored item is in Ignored");
			expect(ignored.Bucket == bucket, "ignoring leaves the bucket, and so attention, as it was");
		}
		auto mine                = item(2, core::attention_bucket::quiet, true);
		mine.Membership.Authored = true;
		expect(core::home_section_for(mine) == core::home_queue_section::ignored, "an ignored pull request of mine is in Ignored too");
		expect(static_cast<int>(core::home_queue_section::ignored) > static_cast<int>(core::home_queue_section::following),
			   "Ignored is the last section");
	}

	void test_ignored_items_sort_after_every_other_item()
	{
		const auto items = std::vector{
			item(1, core::attention_bucket::initial_review_requested, true),
			item(9, core::attention_bucket::quiet, false),
			item(3, core::attention_bucket::quiet, true),
			item(2, core::attention_bucket::initial_review_requested, false),
			item(4, core::attention_bucket::detail_unknown, false),
		};
		const auto ordered = core::ordered_queue(items);
		const auto numbers = [&ordered] {
			auto values = std::vector<std::int64_t>{ };
			for (const auto& entry : ordered) {
				values.push_back(entry.Ref.Number);
			}
			return values;
		}();
		expect(numbers == std::vector<std::int64_t>{ 2, 9, 4, 1, 3 },
			   "the active items keep bucket-then-identity order, and the ignored follow in the same order");
		expect(core::queue_precedes(items[1], items[0]) && !core::queue_precedes(items[0], items[1]),
			   "the most urgent ignored item still follows the quietest active one");
	}

	void test_ignore_and_unignore_are_typed_transitions()
	{
		auto list  = core::home_ignore_list{ };
		auto first = list.apply(ignore(ref(7)), core::home_tracking::open);
		expect(first == core::home_ignore_delta{ .Ref = ref(7), .Change = core::home_ignore_change::ignored },
			   "ignoring an open pull request is one ignored delta");
		expect(list.contains(ref(7)) && list.size() == 1U, "the list holds it");
		expect(!list.apply(ignore(ref(7)), core::home_tracking::open).has_value(), "ignoring it again changes nothing");
		expect(!list.contains(ref(7, "second")), "the same number under another account is not ignored");
		expect(!list.apply(ignore(ref(8)), core::home_tracking::closed).has_value(), "a closed pull request is not on Home to ignore");
		expect(!list.apply(ignore(ref(8)), core::home_tracking::left).has_value(), "nor is one that left Home");
		expect(!list.apply(ignore(ref(0)), core::home_tracking::open).has_value(), "an unusable reference is refused");
		expect(!list.apply(unignore(ref(8)), core::home_tracking::open).has_value(), "unignoring what is not ignored changes nothing");
		const auto back = list.apply(unignore(ref(7)), core::home_tracking::left);
		expect(back == core::home_ignore_delta{ .Ref = ref(7), .Change = core::home_ignore_change::unignored },
			   "unignoring is accepted wherever the pull request is");
		expect(list.size() == 0U, "and it is gone from the list");
		expect(core::name(core::home_ignore_change::pruned_closed) == "pruned, closed", "the change has a stable name");
	}

	void test_closing_or_leaving_home_prunes_the_ignore()
	{
		auto list = core::home_ignore_list{ };
		for (const auto number : { 1, 2, 3, 4 }) {
			static_cast<void>(list.apply(ignore(ref(number)), core::home_tracking::open));
		}
		const auto observations = std::vector<core::home_tracking_observation>{
			{ .Ref = ref(4), .Tracking = core::home_tracking::left },
			{ .Ref = ref(2), .Tracking = core::home_tracking::closed },
			{ .Ref = ref(3), .Tracking = core::home_tracking::open },
			{ .Ref = ref(9), .Tracking = core::home_tracking::closed },
			{ .Ref = ref(4), .Tracking = core::home_tracking::closed },
			{ .Ref = ref(2), .Tracking = core::home_tracking::left },
		};
		const auto deltas = list.observe(observations);
		expect(deltas == std::vector<core::home_ignore_delta>{
				   { .Ref = ref(2), .Change = core::home_ignore_change::pruned_closed },
				   { .Ref = ref(4), .Change = core::home_ignore_change::pruned_closed },
			   },
			   "each closed pull request is pruned once, in identity order, and closed wins over left");
		expect(list.contains(ref(1)) && list.contains(ref(3)) && list.size() == 2U,
			   "an open one and one with no observation keep their ignore");
		expect(list.observe(observations).empty(), "observing again prunes nothing more");
		const auto left = list.observe(std::vector<core::home_tracking_observation>{ { .Ref = ref(1), .Tracking = core::home_tracking::left } });
		expect(left == std::vector<core::home_ignore_delta>{ { .Ref = ref(1), .Change = core::home_ignore_change::pruned_left } },
			   "a pull request that left Home for another reason is pruned as left");
		expect(list.apply(ignore(ref(2)), core::home_tracking::open).has_value(), "a pruned pull request that comes back can be ignored afresh");
	}

	void test_a_restored_list_is_normalized_and_bounded()
	{
		const auto restored = std::vector{ ref(5), ref(1), ref(5), ref(-2), core::pull_request_ref{ }, ref(3, "second") };
		const auto list     = core::home_ignore_list{ restored };
		expect(list.size() == 3U, "duplicates and unusable references are dropped");
		expect(list.refs()[0] == ref(1) && list.refs()[1] == ref(5) && list.refs()[2] == ref(3, "second"),
			   "the restored list is in identity order");
		expect(list == core::home_ignore_list{ std::vector{ ref(3, "second"), ref(5), ref(1) } }, "equal inputs make equal lists in any order");

		auto many = std::vector<core::pull_request_ref>{ };
		many.reserve(core::maximum_home_ignores + 1U);
		for (auto number = std::int64_t{ 1 }; number <= static_cast<std::int64_t>(core::maximum_home_ignores) + 1; ++number) {
			many.push_back(ref(number));
		}
		auto full = core::home_ignore_list{ many };
		expect(full.size() == core::maximum_home_ignores, "a restored list keeps the bound");
		expect(!full.apply(ignore(ref(0x7fffffff)), core::home_tracking::open).has_value(), "a full list refuses another ignore");
	}
} // namespace

auto main() -> int
{
	test_an_ignored_item_is_in_ignored_whatever_its_bucket();
	test_ignored_items_sort_after_every_other_item();
	test_ignore_and_unignore_are_typed_transitions();
	test_closing_or_leaving_home_prunes_the_ignore();
	test_a_restored_list_is_normalized_and_bounded();
	return EXIT_SUCCESS;
}
