#include "bpr/core/chat_notice.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto continue_choice(const std::string& aRead, const bool aStarts) -> chat_notice_choice
		{
			return {
				.Action     = chat_notice_action::continue_at,
				.Words      = aRead.empty() ? std::string{ "Continue at its head" } : "Continue at " + aRead,
				.ShortWords = "Continue",
				.Tip        = aRead.empty() ? std::string{ "Keep this conversation and ask about the revision it read" } : "Keep this conversation and ask about " + aRead + ", the revision it read",
				.Starts     = aStarts,
			};
		}
	} // namespace

	auto plan_send_refused_notice(const std::string_view aWhy, const std::string_view aDetail) -> chat_notice
	{
		auto words = "Not sent. " + std::string{ aWhy };
		if (!aDetail.empty()) {
			words += " (" + std::string{ aDetail } + ")";
		}
		return { .Kind = chat_notice_kind::send_refused, .Form = chat_notice_form::strip, .Tone = chat_notice_tone::danger, .Words = std::move(words), .Dismissable = true };
	}

	auto plan_moved_head_notice(const chat_moved_head_facts& aFacts) -> chat_notice
	{
		const auto read   = chat_revision_words(aFacts.Read);
		auto       notice = chat_notice{
			.Kind  = chat_notice_kind::head_moved,
			.Form  = chat_notice_form::card,
			.Tone  = chat_notice_tone::callout,
			.Title = read.empty() ? std::string{ "This session read an earlier head" } : "This session read " + read,
		};
		// Before a question the card only asks: neither choice starts a run.
		// After a refused one, each choice sends the kept message, and the
		// card reports a turn that did not run: it wears the danger tone and
		// closes as a stopped run's card does.
		if (aFacts.Asked) {
			notice.Tone         = chat_notice_tone::danger;
			notice.Dismissable  = true;
			constexpr auto kept = std::string_view{ "Your message is kept and is sent when you choose." };
			notice.Words        = aFacts.Reason.empty() ? std::string{ kept } : aFacts.Reason + " " + std::string{ kept };
		} else {
			notice.Words = aFacts.KeepsFindings ? "The pull request has moved on. Its findings stay at that head." : "The pull request has moved on.";
		}
		if (aFacts.ContinueOffered) {
			notice.Choices.push_back(continue_choice(read, aFacts.Asked));
		}
		// History's New session, under its one rule: offered while it can be
		// chosen, greyed with why while not.
		auto fresh = chat_notice_choice{ .Action = chat_notice_action::new_session, .Words = "New session", .Starts = aFacts.Asked, .Enabled = aFacts.NewSessionRefused.empty() };
		fresh.Tip  = !aFacts.NewSessionRefused.empty() ? aFacts.NewSessionRefused
			: aFacts.Asked                             ? "Keep this conversation in History, start an empty one at the current head and send your message there"
													   : "Keep this conversation in History and start an empty one at the current head. Nothing is sent until you ask.";
		notice.Choices.push_back(std::move(fresh));
		return notice;
	}

	auto chat_moved_head_wait_words(const std::string_view aRead, const bool aContinueOffered) -> std::string
	{
		if (!aContinueOffered) {
			return "Choose New session first.";
		}
		const auto read = chat_revision_words(aRead);
		return read.empty() ? std::string{ "Choose Continue or New session first." } : "Choose Continue at " + read + " or New session first.";
	}

	auto plan_stopped_notice(const chat_stopped_facts& aFacts) -> chat_notice
	{
		auto notice = chat_notice{
			.Kind        = chat_notice_kind::stopped,
			.Form        = chat_notice_form::card,
			.Tone        = chat_notice_tone::danger,
			.Title       = (aFacts.Agent.empty() ? std::string{ "The agent" } : aFacts.Agent) + " stopped before answering",
			.Words       = aFacts.Reason.empty() ? std::string{ "Your message is kept." } : aFacts.Reason + " Your message is kept.",
			.Dismissable = true,
		};
		// A stop that would stop the same way again is not offered a Resend
		// as its fix; the reason says what would change it.
		if (aFacts.ResendOffered) {
			notice.Choices.push_back({ .Action = chat_notice_action::resend, .Words = "Resend", .Tip = "Send your message to the agent again", .Starts = true });
		}
		notice.Choices.push_back({
			.Action = chat_notice_action::toggle_log,
			.Words  = aFacts.LogShown ? "Hide the log" : "Show the log",
			.Tip    = aFacts.LogShown ? "Fold what the agent wrote" : "Show what the agent wrote before it stopped",
			.Aside  = true,
		});
		return notice;
	}

	auto plan_session_notice(const chat_continuation aContinuation, const std::string_view aAgent, std::string aTip) -> chat_notice
	{
		const auto kind  = aContinuation == chat_continuation::carried_over ? chat_notice_kind::carried_over
			: aContinuation == chat_continuation::cannot_continue           ? chat_notice_kind::cannot_continue
																			: chat_notice_kind::said;
		auto       words = chat_session_notice(aContinuation, aAgent);
		auto       tip   = words.empty() ? std::string{ } : std::move(aTip);
		return { .Kind = kind, .Words = std::move(words), .Tip = std::move(tip) };
	}

	auto plan_said_notice(std::string aWords, std::string aTip) -> chat_notice
	{
		return { .Kind = chat_notice_kind::said, .Words = std::move(aWords), .Tip = std::move(aTip) };
	}
}
