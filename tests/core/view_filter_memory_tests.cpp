// [[BPR-1844]]. Discuss, Threads and Review's Comments open with the profile's
// last choice; each window keeps its own filters; the stored row degrades
// view by view.
#include "bpr/core/view_filter_memory.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
	using namespace bpr::core;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	constexpr auto only_new = view_filter_choice{ .OnlyNew = true, .HideResolved = false };
	constexpr auto hide     = view_filter_choice{ .OnlyNew = false, .HideResolved = true };
	constexpr auto both     = view_filter_choice{ .OnlyNew = true, .HideResolved = true };

	void test_opening()
	{
		const auto fresh = open_view_filters({ }, { });
		expect(fresh == view_filter_state{ }, "with nothing remembered every view opens unfiltered");

		const auto first = open_view_filters({ }, hide);
		expect(first.ReviewComments == hide && first.Discuss == view_filter_choice{ } && first.Threads == view_filter_choice{ },
			   "Review's Comments falls back to the first-filter preference; the others to nothing");

		auto defaults           = view_filter_defaults{ };
		defaults.Discuss        = only_new;
		defaults.Threads        = both;
		defaults.ReviewComments = view_filter_choice{ };
		const auto remembered   = open_view_filters(defaults, hide);
		expect(remembered.Discuss == only_new && remembered.Threads == both && remembered.ReviewComments == view_filter_choice{ },
			   "a remembered choice wins over the preference, even a choice of nothing filtered");
	}

	void test_choosing()
	{
		const auto state  = open_view_filters({ }, { });
		const auto chosen = choose_view_filter(state, filter_view::threads, only_new);
		expect(chosen.State.Threads == only_new && chosen.State.Discuss == view_filter_choice{ } && chosen.State.ReviewComments == view_filter_choice{ },
			   "a choice changes only its own view");
		expect(chosen.Remember == view_filter_remember{ .View = filter_view::threads, .Choice = only_new }, "and is remembered for that view");
		expect(filters_of(chosen.State, filter_view::threads) == only_new && filters_of(chosen.State, filter_view::discuss) == view_filter_choice{ },
			   "filters_of reads one view");

		const auto again = choose_view_filter(chosen.State, filter_view::threads, only_new);
		expect(again.State == chosen.State && again.Remember.has_value(), "choosing what the window shows is still remembered: the last to choose decides");

		const auto shown = present_view_filter(chosen.State, filter_view::threads, view_filter_choice{ });
		expect(shown.Threads == view_filter_choice{ }, "a change the window makes by itself is shown");

		const auto unknown = choose_view_filter(state, static_cast<filter_view>(9), both);
		expect(unknown.State == state && !unknown.Remember, "an unknown view changes nothing and remembers nothing");
	}

	void test_memory_is_per_view_last_writer()
	{
		// Window A opens, then window B; both from the same memory.
		auto       memory = view_filter_defaults{ };
		const auto a0     = open_view_filters(memory, { });
		const auto b0     = open_view_filters(memory, { });

		// A chooses on Discuss; B, which never saw that, chooses on Threads.
		const auto a1 = choose_view_filter(a0, filter_view::discuss, hide);
		memory        = remember(memory, *a1.Remember);
		const auto b1 = choose_view_filter(b0, filter_view::threads, both);
		memory        = remember(memory, *b1.Remember);
		expect(memory.Discuss == hide && memory.Threads == both && !memory.ReviewComments, "B's Threads choice does not undo A's Discuss choice");

		// Neither window's filters moved because of the other.
		expect(a1.State.Threads == view_filter_choice{ } && b1.State.Discuss == view_filter_choice{ }, "open windows keep their own filters");

		// B then chooses Discuss too: the last writer wins for that view.
		const auto b2 = choose_view_filter(b1.State, filter_view::discuss, only_new);
		memory        = remember(memory, *b2.Remember);
		expect(memory.Discuss == only_new && a1.State.Discuss == hide, "the memory takes B's later choice; A still shows its own");

		// A new window, or a pull request opened in any window, takes the memory.
		const auto c = open_view_filters(memory, { });
		expect(c.Discuss == only_new && c.Threads == both && c.ReviewComments == view_filter_choice{ }, "the next pull request opens with the memory");
	}

	void test_stored_form()
	{
		for (const auto choice : { view_filter_choice{ }, only_new, hide, both }) {
			expect(read_stored_view_filter(stored_view_filter(choice)) == choice, "every choice round-trips through its stored value");
		}
		expect(stored_view_filter(both) == 3 && stored_view_filter(hide) == 2, "the stored bits are fixed");
		expect(!read_stored_view_filter(4) && !read_stored_view_filter(-1), "a value no version wrote is unknown");

		auto defaults           = view_filter_defaults{ };
		defaults.Discuss        = both;
		defaults.ReviewComments = view_filter_choice{ };
		const auto row          = stored_view_filter_memory(defaults);
		expect(row.Version == view_filter_memory_version && row.Discuss == 3 && !row.Threads && row.ReviewComments == 0, "a view with nothing remembered is stored as none");
		const auto read = read_view_filter_memory(row);
		expect(read.Defaults == defaults && !read.Recovered && read.Writable, "the stored row reads back as the memory");
	}

	void test_damaged_and_versioned_rows()
	{
		const auto none = read_view_filter_memory(std::nullopt);
		expect(none == view_filter_memory_read{ }, "nothing stored reads as nothing remembered, writable");

		auto damaged     = stored_view_filter_row{ };
		damaged.Discuss  = 7;
		damaged.Threads  = 1;
		const auto mixed = read_view_filter_memory(damaged);
		expect(!mixed.Defaults.Discuss && mixed.Defaults.Threads == only_new && mixed.Recovered && mixed.Writable,
			   "an unknown value leaves only its own view with nothing remembered, and is reported");

		auto       older = stored_view_filter_row{ .Version = 0, .Discuss = 3, .Threads = 3, .ReviewComments = 3 };
		const auto o     = read_view_filter_memory(older);
		expect(o.Defaults == view_filter_defaults{ } && o.Recovered && o.Writable, "a version older than any known reads as nothing remembered and is rewritten");

		auto       newer = stored_view_filter_row{ .Version = view_filter_memory_version + 1, .Discuss = 3 };
		const auto n     = read_view_filter_memory(newer);
		expect(n.Defaults == view_filter_defaults{ } && !n.Writable && !n.Recovered, "a newer build's row is read as nothing and kept for that build");
	}
}

auto main() -> int
{
	test_opening();
	test_choosing();
	test_memory_is_per_view_last_writer();
	test_stored_form();
	test_damaged_and_versioned_rows();
	std::cout << "view filter memory: ok\n";
	return EXIT_SUCCESS;
}
