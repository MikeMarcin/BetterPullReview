#include "bpr/core/agent_question.hpp"

#include <algorithm>

namespace bpr::core {
	auto stable_name(const agent_question_subject aSubject) noexcept -> std::string_view
	{
		switch (aSubject) {
		case agent_question_subject::pull_request: return "pull_request";
		case agent_question_subject::file:         return "file";
		case agent_question_subject::lines:        return "lines";
		case agent_question_subject::conversation: return "conversation";
		}
		return { };
	}

	auto agent_question_statement(const agent_question_subject aSubject) noexcept -> std::string_view
	{
		switch (aSubject) {
		case agent_question_subject::pull_request:
			return "The question concerns the whole pull request; every changed file is in scope. "
				   "Where the reviewer is looking does not narrow it.";
		case agent_question_subject::file:
			return "The question concerns the file named in subject.path; the rest of the change is context.";
		case agent_question_subject::lines:
			return "The question concerns the lines in subject.text, from subject.path; the rest of the change is context.";
		case agent_question_subject::conversation:
			return "The question concerns the conversation in conversation.*.";
		}
		return { };
	}

	auto decide_agent_question(const agent_question_facts& aFacts) -> std::optional<agent_question>
	{
		if (aFacts.Returning) {
			switch (*aFacts.Returning) {
			case returning_operation::latest_upload: return agent_question{ .Subject = agent_question_subject::pull_request, .EntryPoint = agent_entry_point::analyze_latest_upload };
			case returning_operation::proposed_fix:
				if (!aFacts.ConversationAttached) {
					return std::nullopt;
				}
				return agent_question{ .Subject = agent_question_subject::conversation, .EntryPoint = agent_entry_point::evaluate_proposed_fix };
			}
			return std::nullopt;
		}
		if (aFacts.ConversationAttached) {
			return agent_question{ .Subject = agent_question_subject::conversation, .EntryPoint = agent_entry_point::ask_about_conversation };
		}
		if (aFacts.Suggested) {
			switch (*aFacts.Suggested) {
			case agent_suggested_subject::pull_request:
				return agent_question{
					.Subject    = agent_question_subject::pull_request,
					.EntryPoint = aFacts.Review ? agent_entry_point::run_ai_review : agent_entry_point::open_full_agent_session,
				};
			case agent_suggested_subject::open_file:
				if (aFacts.OpenFile.empty()) {
					return std::nullopt;
				}
				return agent_question{
					.Subject    = agent_question_subject::file,
					.EntryPoint = aFacts.Review ? agent_entry_point::run_ai_review : agent_entry_point::open_full_agent_session,
					.File       = aFacts.OpenFile,
				};
			case agent_suggested_subject::selected_lines:
				if (!aFacts.LinesSelected) {
					return std::nullopt;
				}
				return agent_question{ .Subject = agent_question_subject::lines, .EntryPoint = agent_entry_point::ask_about_selection };
			}
			return std::nullopt;
		}
		if (aFacts.LinesAttached && aFacts.LinesSelected) {
			return agent_question{ .Subject = agent_question_subject::lines, .EntryPoint = agent_entry_point::ask_about_selection };
		}
		return agent_question{ .Subject = agent_question_subject::pull_request, .EntryPoint = agent_entry_point::open_full_agent_session };
	}

	auto valid(const supplement_subject& aSubject) -> bool
	{
		if (aSubject.Path.size() > maximum_subject_path_bytes || aSubject.Text.size() > maximum_subject_text_bytes || aSubject.InScope.size() > maximum_subject_files
			|| std::ranges::any_of(aSubject.InScope, [](const std::string& aPath) { return aPath.empty() || aPath.size() > maximum_subject_path_bytes; })) {
			return false;
		}
		const auto noLines = aSubject.FirstLine == 0U && aSubject.LastLine == 0U && !aSubject.Before && aSubject.Text.empty();
		switch (aSubject.Subject) {
		case agent_question_subject::pull_request: return aSubject.Path.empty() && noLines;
		case agent_question_subject::file:         return !aSubject.Path.empty() && noLines && aSubject.InScope.empty();
		case agent_question_subject::lines:
			return !aSubject.Path.empty() && aSubject.FirstLine != 0U && aSubject.FirstLine <= aSubject.LastLine && aSubject.InScope.empty();
		case agent_question_subject::conversation: return aSubject.Path.empty() && noLines && aSubject.InScope.empty();
		}
		return false;
	}

	auto valid(const supplement_looking_at& aLooking) -> bool
	{
		return !aLooking.Path.empty() && aLooking.Path.size() <= maximum_subject_path_bytes && (!aLooking.Line || *aLooking.Line != 0U);
	}
} // namespace bpr::core
