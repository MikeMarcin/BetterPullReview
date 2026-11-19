#include "bpr/qt/prompt_words.hpp"

#include <QObject>

namespace bpr::qt {
	auto prompt_fact_name(const core::prompt_fact aFact) -> QString
	{
		using enum core::prompt_fact;
		switch (aFact) {
		case author_is_me:            return QObject::tr("Pull request is mine");
		case draft:                   return QObject::tr("Pull request is a draft");
		case checks_failing:          return QObject::tr("A check failed");
		case repository:              return QObject::tr("Repository matches");
		case pushes_since_review:     return QObject::tr("New pushes since my review");
		case file_open:               return QObject::tr("A file is open");
		case path:                    return QObject::tr("File path matches");
		case selected_lines:          return QObject::tr("Lines are selected");
		case file_unresolved_threads: return QObject::tr("File has unresolved threads");
		case file_unreviewed:         return QObject::tr("I haven't reviewed this file");
		case takes_replies:           return QObject::tr("Thread takes replies");
		case thread_unresolved:       return QObject::tr("Thread is unresolved");
		case thread_activity:         return QObject::tr("New replies or pushes since the comment");
		case replies_since_mine:      return QObject::tr("Replies since my comment");
		case empty_chat:              return QObject::tr("Chat is empty");
		}
		return { };
	}

	auto prompt_fact_group(const core::prompt_fact aFact) -> QString
	{
		using enum core::prompt_fact;
		switch (aFact) {
		case author_is_me:
		case draft:
		case checks_failing:
		case repository:
		case pushes_since_review:     return QObject::tr("Pull request");
		case file_open:
		case path:
		case selected_lines:
		case file_unresolved_threads:
		case file_unreviewed:         return QObject::tr("File");
		case takes_replies:
		case thread_unresolved:
		case thread_activity:
		case replies_since_mine:      return QObject::tr("Thread");
		case empty_chat:              return QObject::tr("Chat");
		}
		return { };
	}

	auto prompt_condition_reason(const core::prompt_condition& aCondition) -> QString
	{
		using enum core::prompt_fact;
		// [[BPR-2185]] (review N3): the core says what the unmet condition
		// says about the window; these are its words.
		const auto reading = core::prompt_unmet_reading_of(aCondition);
		const auto present = reading.Present;
		switch (reading.Fact) {
		case author_is_me:            return present ? QObject::tr("This is your pull request.") : QObject::tr("This is not your pull request.");
		case draft:                   return present ? QObject::tr("This pull request is a draft.") : QObject::tr("This pull request is not a draft.");
		case checks_failing:          return present ? QObject::tr("A check has failed.") : QObject::tr("No check has failed.");
		case pushes_since_review:     return present ? QObject::tr("Commits followed your review.") : QObject::tr("No commits since your review.");
		case file_unresolved_threads: return present ? QObject::tr("This file has unresolved threads.") : QObject::tr("No unresolved threads on this file.");
		case file_unreviewed:         return present ? QObject::tr("You haven't reviewed this file yet.") : QObject::tr("You have already reviewed this file.");
		case thread_unresolved:       return present ? QObject::tr("This thread is unresolved.") : QObject::tr("This thread is resolved.");
		case thread_activity:
			switch (reading.Activity) {
			case core::prompt_activity_value::replies: return present ? QObject::tr("Replies followed the comment.") : QObject::tr("No replies since the comment.");
			case core::prompt_activity_value::pushes:  return present ? QObject::tr("Pushes followed the comment.") : QObject::tr("No pushes since the comment.");
			default:                                   return present ? QObject::tr("Replies or pushes followed the comment.") : QObject::tr("No replies or pushes since the comment.");
			}
		case replies_since_mine: return present ? QObject::tr("Someone replied after your comment.") : QObject::tr("No replies since your comment.");
		// [[BPR-2179]] look prompts-v003: any condition may be the reason a
		// disabled row gives, so each has words.
		case file_open:      return present ? QObject::tr("A file is open.") : QObject::tr("No file is open.");
		case path:           return present ? QObject::tr("This file's path matches.") : QObject::tr("This file's path doesn't match.");
		case selected_lines: return present ? QObject::tr("Lines are selected.") : QObject::tr("No lines are selected.");
		case repository:     return present ? QObject::tr("This repository matches.") : QObject::tr("This repository doesn't match.");
		case takes_replies:  return present ? QObject::tr("This thread takes replies.") : QObject::tr("This thread takes no replies.");
		case empty_chat:     return present ? QObject::tr("The chat is empty.") : QObject::tr("The chat isn't empty.");
		}
		return { };
	}

	auto prompt_problem_words(const core::prompt_problem aProblem) -> QString
	{
		using enum core::prompt_problem;
		switch (aProblem) {
		case no_name:                     return QObject::tr("it has no name");
		case name_too_long:               return QObject::tr("its name is too long");
		case name_not_one_line:           return QObject::tr("its name is more than one line");
		case no_template:                 return QObject::tr("it has no text");
		case template_too_long:           return QObject::tr("its text is too long");
		case unknown_variable:            return QObject::tr("it uses a placeholder that doesn't exist");
		case other_place_variable:        return QObject::tr("it uses a placeholder this surface can't fill");
		case subject_not_for_place:       return QObject::tr("it asks about something this surface doesn't hold");
		case kind_not_for_subject:        return QObject::tr("it answers as findings, which this surface can't");
		case too_many_conditions:         return QObject::tr("it has too many conditions");
		case fact_not_for_place:          return QObject::tr("a condition reads something this surface doesn't hold");
		case no_patterns:                 return QObject::tr("a condition has no pattern");
		case too_many_patterns:           return QObject::tr("a condition has too many patterns");
		case pattern_too_long:            return QObject::tr("a pattern is too long");
		case bad_pattern:                 return QObject::tr("a pattern can't match anything");
		case conditions_contradict:       return QObject::tr("its conditions can't all hold");
		case duplicate_name:              return QObject::tr("another prompt has its name");
		case credential_prefix:           return QObject::tr("its text holds what looks like a credential");
		case quick_ask_not_for_subject:   return QObject::tr("an Ask menu can't ask about what it is about");
		case quick_ask_without_scope:     return QObject::tr("it is in no Ask menu's scope");
		case scope_narrower_than_subject: return QObject::tr("its menu is narrower than what it asks about");
		}
		return { };
	}

	auto prompt_condition_unmet_line(const core::prompt_condition& aCondition) -> QString
	{
		return QObject::tr("When it is the reason the prompt is unmet: \u201C%1\u201D").arg(prompt_condition_reason(aCondition));
	}

	auto prompt_surface_name(const core::prompt_surface aSurface) -> QString
	{
		using enum core::prompt_surface;
		switch (aSurface) {
		case pull_request_menu:  return QObject::tr("Pull request menu");
		case pull_request_chips: return QObject::tr("Pull request chips");
		case file_menu:          return QObject::tr("File menu");
		case file_chips:         return QObject::tr("File chips");
		case thread_menu:        return QObject::tr("Thread menu");
		case thread_chips:       return QObject::tr("Thread chips");
		case tour_stop:          return QObject::tr("Tour stop chips");
		case whole_tour:         return QObject::tr("Whole tour chips");
		case commits_menu:       return QObject::tr("Commits selection menu");
		}
		return { };
	}
} // namespace bpr::qt
