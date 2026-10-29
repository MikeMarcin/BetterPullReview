// [[BPR-1805]]. Whether the open tour stop is read or written, without Qt.
#include "bpr/core/tour_stop_view.hpp"

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
	using namespace bpr;
	using core::tour_stop_view_event;
	using mode = core::tour_stop_view_mode;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto stop(const std::uint64_t aId, std::string aTitle, std::string aBody) -> core::tour_stop
	{
		auto value  = core::tour_stop{ };
		value.Id    = core::tour_stop_id{ aId };
		value.Title = std::move(aTitle);
		value.Body  = std::move(aBody);
		return value;
	}

	[[nodiscard]] auto open(const core::tour_stop_view& aCurrent, const core::tour_stop& aStop) -> core::tour_stop_view
	{
		return core::decide_tour_stop_view(aCurrent, { .Stop = &aStop, .Event = tour_stop_view_event::open });
	}

	void test_a_written_stop_opens_to_read()
	{
		const auto written = stop(1, "Cache the parse", "The parse is **cached**.");
		const auto view    = open({ }, written);
		expect(view.Stop == written.Id && view.Mode == mode::read, "a written stop opens as its rendered explanation");
		const auto titled = stop(2, "Untitled body", "Body only");
		expect(open(view, titled).Mode == mode::read, "another written stop opens to read too");
	}

	void test_an_unwritten_stop_opens_to_write()
	{
		const auto blank = stop(1, { }, { });
		expect(open({ }, blank).Mode == mode::edit, "a new, empty stop opens to be written");
		const auto skeleton = stop(2, "Suggested title", " \n\t");
		expect(open({ }, skeleton).Mode == mode::edit, "a titled stop with only whitespace to read opens to be written");
		expect(core::tour_stop_unwritten(skeleton) && !core::tour_stop_unwritten(stop(3, { }, "x")), "unwritten is an explanation of whitespace only");
	}

	void test_going_back_reads_again()
	{
		const auto first  = stop(1, "First", "One");
		const auto second = stop(2, "Second", "Two");
		auto       view   = open({ }, first);
		view              = core::decide_tour_stop_view(view, { .Stop = &first, .Event = tour_stop_view_event::begin_edit });
		expect(view.Mode == mode::edit, "the pencil opens the editor");
		view = open(view, second);
		expect(view.Stop == second.Id && view.Mode == mode::read, "moving to another stop reads it");
		view = open(view, first);
		expect(view.Stop == first.Id && view.Mode == mode::read, "going back to a stop that was being written reads it, the owner's report");
	}

	void test_presenting_the_open_stop_again_keeps_its_mode()
	{
		const auto written = stop(1, "Cache the parse", "Body");
		auto       view    = core::decide_tour_stop_view(open({ }, written), { .Stop = &written, .Event = tour_stop_view_event::begin_edit });
		const auto saved   = stop(1, "Cache the parse", "Body, edited");
		expect(open(view, saved) == view, "a save presenting the stop again keeps it open for writing");
		view = core::decide_tour_stop_view(view, { .Stop = &saved, .Event = tour_stop_view_event::finish_edit });
		expect(view.Mode == mode::read, "Done reads the stop");
		expect(open(view, saved) == view, "and a later presentation keeps it read");
		const auto emptied = stop(1, "Cache the parse", { });
		expect(open(view, emptied).Mode == mode::read, "the open stop's mode is the author's: emptying it elsewhere does not open the editor");
	}

	void test_unsaved_text_keeps_the_editor_open()
	{
		const auto saved = stop(1, "Title", "Saved body");
		auto       view  = core::decide_tour_stop_view(open({ }, saved), { .Stop = &saved, .Event = tour_stop_view_event::begin_edit });
		const auto held  = core::tour_stop_held_text{ .Title = "Title", .Body = "Saved body and much more than the limit" };
		expect(core::tour_stop_text_unsaved({ .Stop = &saved, .Held = held }), "text that differs from the stop is unsaved");
		expect(!core::tour_stop_text_unsaved({ .Stop = &saved, .Held = core::tour_stop_held_text{ .Title = "Title", .Body = "Saved body" } }), "text equal to the stop is saved");
		expect(core::decide_tour_stop_view(view, { .Stop = &saved, .Event = tour_stop_view_event::finish_edit, .Held = held }).Mode == mode::edit, "Done keeps unsaved text in view");
		expect(core::decide_tour_stop_view(view, { .Stop = &saved, .Event = tour_stop_view_event::finish_edit, .Held = core::tour_stop_held_text{ .Title = "Changed", .Body = "Saved body" } }).Mode == mode::edit, "an unsaved title counts too");
		expect(core::decide_tour_stop_view(view, { .Stop = &saved, .Event = tour_stop_view_event::finish_edit, .Composing = true }).Mode == mode::edit, "preedit text keeps the editor open");
		const auto read = core::tour_stop_view{ .Stop = saved.Id, .Mode = mode::read };
		expect(core::decide_tour_stop_view(read, { .Stop = &saved, .Event = tour_stop_view_event::open, .Held = held }).Mode == mode::edit, "a stop opened with unsaved text shows the text, never the reader");
		view = core::decide_tour_stop_view(view, { .Stop = &saved, .Event = tour_stop_view_event::finish_edit, .Held = core::tour_stop_held_text{ .Title = "Title", .Body = "Saved body" } });
		expect(view.Mode == mode::read, "once taken, Done reads");
	}

	void test_a_read_only_draft_is_read()
	{
		const auto blank = stop(1, { }, { });
		const auto view  = core::decide_tour_stop_view({ }, { .Stop = &blank, .Event = tour_stop_view_event::begin_edit, .Editable = false });
		expect(view.Stop == blank.Id && view.Mode == mode::read, "a draft that cannot be written is read, even with nothing written");
	}

	void test_no_stop_is_nothing_open()
	{
		const auto view = core::tour_stop_view{ .Stop = core::tour_stop_id{ 4 }, .Mode = mode::edit };
		expect(core::decide_tour_stop_view(view, { }) == core::tour_stop_view{ }, "no stop: nothing is open");
		expect(!core::tour_stop_text_unsaved({ .Composing = true }), "no stop has no unsaved text");
	}

	void test_stable_names()
	{
		expect(core::stable_name(mode::read) == "read" && core::stable_name(mode::edit) == "edit", "mode names");
		expect(core::stable_name(tour_stop_view_event::open) == "open" && core::stable_name(tour_stop_view_event::begin_edit) == "begin_edit" && core::stable_name(tour_stop_view_event::finish_edit) == "finish_edit", "event names");
	}
}

auto main() -> int
{
	test_a_written_stop_opens_to_read();
	test_an_unwritten_stop_opens_to_write();
	test_going_back_reads_again();
	test_presenting_the_open_stop_again_keeps_its_mode();
	test_unsaved_text_keeps_the_editor_open();
	test_a_read_only_draft_is_read();
	test_no_stop_is_nothing_open();
	test_stable_names();
	std::cout << "tour-stop-view:pass\n";
	return EXIT_SUCCESS;
}
