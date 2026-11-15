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
		const auto negated = aCondition.Negated;
		switch (aCondition.Fact) {
		case author_is_me:            return negated ? QObject::tr("This is your pull request.") : QObject::tr("This is not your pull request.");
		case draft:                   return negated ? QObject::tr("This pull request is a draft.") : QObject::tr("This pull request is not a draft.");
		case checks_failing:          return negated ? QObject::tr("A check has failed.") : QObject::tr("No check has failed.");
		case pushes_since_review:     return negated ? QObject::tr("Commits followed your review.") : QObject::tr("No commits since your review.");
		case file_unresolved_threads: return negated ? QObject::tr("This file has unresolved threads.") : QObject::tr("No unresolved threads on this file.");
		case file_unreviewed:         return negated ? QObject::tr("You haven't reviewed this file yet.") : QObject::tr("You have already reviewed this file.");
		case thread_unresolved:       return negated ? QObject::tr("This thread is unresolved.") : QObject::tr("This thread is resolved.");
		case thread_activity:
			switch (aCondition.Activity) {
			case core::prompt_activity_value::replies: return QObject::tr("No replies since the comment.");
			case core::prompt_activity_value::pushes:  return QObject::tr("No pushes since the comment.");
			default:                                   return QObject::tr("No replies or pushes since the comment.");
			}
		case replies_since_mine: return negated ? QObject::tr("Someone replied after your comment.") : QObject::tr("No replies since your comment.");
		// [[BPR-2179]] look prompts-v003: any condition may be the reason a
		// disabled row gives, so each has words.
		case file_open:      return negated ? QObject::tr("A file is open.") : QObject::tr("No file is open.");
		case path:           return negated ? QObject::tr("This file's path matches.") : QObject::tr("This file's path doesn't match.");
		case selected_lines: return negated ? QObject::tr("Lines are selected.") : QObject::tr("No lines are selected.");
		case repository:     return negated ? QObject::tr("This repository matches.") : QObject::tr("This repository doesn't match.");
		case takes_replies:  return negated ? QObject::tr("This thread takes replies.") : QObject::tr("This thread takes no replies.");
		case empty_chat:     return negated ? QObject::tr("The chat is empty.") : QObject::tr("The chat isn't empty.");
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
		case tour_part:          return QObject::tr("Tour part chips");
		case whole_tour:         return QObject::tr("Whole tour chips");
		}
		return { };
	}
} // namespace bpr::qt
