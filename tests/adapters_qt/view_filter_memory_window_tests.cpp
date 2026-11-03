// [[BPR-1844]]. Two windows over one profile memory: each pull request opens
// with the last filters chosen in any window, and a choice never reaches a
// window that is already open.
#include "bpr/qt/conversation_finder.hpp"
#include "bpr/qt/github_conversation_panel.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QToolButton>

#include <cstdlib>
#include <iostream>
#include <string_view>
#include <utility>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void process_events()
	{
		for (auto pass = 0; pass < 4; ++pass) {
			QApplication::processEvents();
		}
	}

	/// The profile's memory as the composition holds it, shared by windows.
	struct profile_memory
	{
		core::view_filter_defaults Defaults;
		int                        Told = 0;

		void attach(qt::workbench_window& aWindow)
		{
			aWindow.set_view_filter_memory([this] { return Defaults; },
										   [this](const core::view_filter_remember& aChoice) {
											   Defaults = core::remember(Defaults, aChoice);
											   ++Told;
										   });
			aWindow.publish(application::workbench_projection{ .Behaviour = application::review_behaviour_settings{ } });
		}
	};

	void open_pull_request(qt::workbench_window& aWindow)
	{
		aWindow.publish(application::github_presentation_update{
			.PullRequest = application::github_pull_request_projection{ .ReviewThreadsAvailable = true },
		});
		process_events();
	}

	[[nodiscard]] auto threads_chip(qt::workbench_window& aWindow, const char* const aName) -> QAbstractButton*
	{
		auto* const chip = aWindow.finder()->findChild<QAbstractButton*>(QString::fromLatin1(aName));
		expect(chip != nullptr, "the Threads page has its filter chip");
		return chip;
	}

	[[nodiscard]] auto shows(const qt::github_conversation_panel& aPanel, const core::view_filter_choice aChoice) -> bool
	{
		return aPanel.filter_unread_button()->isChecked() == aChoice.OnlyNew && aPanel.filter_unresolved_button()->isChecked() == aChoice.HideResolved;
	}

	void test_two_windows_diverge_and_the_next_pull_request_opens_with_the_memory()
	{
		constexpr auto onlyNew = core::view_filter_choice{ .OnlyNew = true, .HideResolved = false };
		constexpr auto hide    = core::view_filter_choice{ .OnlyNew = false, .HideResolved = true };
		constexpr auto none    = core::view_filter_choice{ };

		auto                 memory = profile_memory{ };
		qt::workbench_window first;
		qt::workbench_window second;
		memory.attach(first);
		memory.attach(second);
		open_pull_request(first);
		open_pull_request(second);
		auto* const a = first.conversation_panel();
		auto* const b = second.conversation_panel();

		// Review's Comments, beside the code, in the first window.
		first.choose_modality(qt::review_modality::review);
		second.choose_modality(qt::review_modality::review);
		process_events();
		expect(shows(*a, none) && shows(*b, none), "a fresh profile opens unfiltered");
		a->filter_unread_button()->click();
		process_events();
		expect(memory.Defaults.ReviewComments == onlyNew && first.view_filters().ReviewComments == onlyNew, "a press in Review's Comments is remembered and is the window's own");
		expect(shows(*b, none) && second.view_filters() == core::view_filter_state{ }, "the second window, already open, keeps its own filters");

		// Discuss in the second window.
		second.choose_modality(qt::review_modality::discuss);
		process_events();
		b->filter_unresolved_button()->click();
		process_events();
		expect(memory.Defaults.Discuss == hide && shows(*b, hide), "a press in Discuss is remembered");
		first.choose_modality(qt::review_modality::discuss);
		process_events();
		expect(shows(*a, none), "the first window's Discuss is still its own");

		// Threads in the first window.
		first.choose_modality(qt::review_modality::threads);
		process_events();
		threads_chip(first, "threadFinderHideResolved")->click();
		process_events();
		expect(memory.Defaults.Threads == hide && first.finder()->filters().HideResolved, "a press on Threads is remembered");
		second.choose_modality(qt::review_modality::threads);
		process_events();
		expect(!second.finder()->filters().HideResolved, "the second window's Threads is still its own");

		// A window's own change, not the reviewer's, is not remembered.
		const auto told    = memory.Told;
		auto       filters = second.finder()->filters();
		filters.OnlyNew    = true;
		second.finder()->set_filters(std::move(filters));
		process_events();
		expect(second.finder()->filters().OnlyNew && memory.Told == told && !memory.Defaults.Threads->OnlyNew, "a filter the window sets changes the window and leaves the memory");

		// The second window opens another pull request: every view takes the memory.
		open_pull_request(second);
		second.choose_modality(qt::review_modality::threads);
		process_events();
		expect(second.finder()->filters().HideResolved && !second.finder()->filters().OnlyNew, "Threads opens with the last Threads choice, made in the other window");
		second.choose_modality(qt::review_modality::discuss);
		process_events();
		expect(shows(*b, hide), "Discuss opens with the last Discuss choice");
		second.choose_modality(qt::review_modality::review);
		process_events();
		expect(shows(*b, onlyNew), "Review's Comments opens with the last Review Comments choice, made in the other window");
		expect(second.view_filters() == core::view_filter_state{ .Discuss = hide, .Threads = hide, .ReviewComments = onlyNew }, "the window's state is the memory it opened with");

		// And the first window, never reopened, kept what it had.
		first.choose_modality(qt::review_modality::discuss);
		process_events();
		expect(shows(*a, none), "the first window was never pushed the memory");
	}

	void test_the_first_filter_preference_seeds_review_comments_and_a_settings_choice_is_remembered()
	{
		auto                 memory = profile_memory{ };
		qt::workbench_window window;
		window.set_view_filter_memory([&memory] { return memory.Defaults; },
									  [&memory](const core::view_filter_remember& aChoice) { memory.Defaults = core::remember(memory.Defaults, aChoice); });
		window.set_behaviour_dispatch([](const application::review_behaviour_settings&) { });
		window.publish(application::workbench_projection{ .Behaviour = application::review_behaviour_settings{ .CommentsFilter = application::review_comments_filter::unresolved } });
		open_pull_request(window);
		window.choose_modality(qt::review_modality::review);
		process_events();
		auto* const panel = window.conversation_panel();
		expect(shows(*panel, { .OnlyNew = false, .HideResolved = true }), "with nothing remembered, Review's Comments opens on the first-filter preference");

		auto* const unread = window.findChild<QToolButton*>(QStringLiteral("commentsFilterUnread"));
		expect(unread != nullptr, "Settings has the first-filter row");
		unread->click();
		expect(memory.Defaults.ReviewComments == core::view_filter_choice{ .OnlyNew = true, .HideResolved = false }, "choosing the first filter in Settings is remembered for Review's Comments");
	}
}

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_two_windows_diverge_and_the_next_pull_request_opens_with_the_memory();
	test_the_first_filter_preference_seeds_review_comments_and_a_settings_choice_is_remembered();
	std::cout << "view-filter-memory-window:pass\n";
	return EXIT_SUCCESS;
}
