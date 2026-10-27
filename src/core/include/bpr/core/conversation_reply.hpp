#ifndef BPR_CORE_CONVERSATION_REPLY_HPP
#define BPR_CORE_CONVERSATION_REPLY_HPP
#include <optional>
#include <string>
#include <string_view>
namespace bpr::core {
	enum class conversation_reply_action { open,
										   edit,
										   send,
										   close };
	/// [[BPR-1781]]. The conversation key of a new comment on the pull request
	/// itself, written on Overview's Comments tab: it answers no post, so it
	/// has no author or quote and is sent as its text alone.
	inline constexpr std::string_view new_pull_request_comment = "pull-request:new-comment";
	/// [[BPR-1773]], owner look 2026-10-27 ("let you turn anything into a
	/// note or draft comment"). Where Draft comment on an agent's answer
	/// writes it: the reply of the review thread the question was about; a
	/// line comment on the lines selected when it was asked; otherwise a new
	/// comment on the pull request. Never hidden, so every answer can be one.
	enum class answer_draft_place : unsigned char { thread_reply,
													line_comment,
													pull_request_comment };
	[[nodiscard]] constexpr auto answer_draft_place_for(const bool aAskedAboutThread, const bool aAskedOnLines) noexcept -> answer_draft_place
	{
		if (aAskedAboutThread) {
			return answer_draft_place::thread_reply;
		}
		return aAskedOnLines ? answer_draft_place::line_comment : answer_draft_place::pull_request_comment;
	}
	/// Private text and the immutable accepted post it answers; no authority.
	struct conversation_reply_draft
	{
		std::string Conversation, Author, Quote, Text;
		auto        operator==(const conversation_reply_draft&) const -> bool = default;
	};
	[[nodiscard]] inline auto valid(const conversation_reply_draft& aDraft) -> bool
	{
		// A new comment answers no post: it carries no author or quote.
		if (aDraft.Conversation == new_pull_request_comment && (!aDraft.Author.empty() || !aDraft.Quote.empty())) {
			return false;
		}
		return !aDraft.Conversation.empty() && aDraft.Conversation.size() <= 256U
			&& aDraft.Author.size() <= 256U && aDraft.Quote.size() <= 16384U && aDraft.Text.size() <= 32768U;
	}
	[[nodiscard]] inline auto conversation_reply_can_send(const conversation_reply_draft& aDraft) -> bool
	{
		return valid(aDraft) && aDraft.Text.find_first_not_of(" \t\r\n") != std::string::npos;
	}
	[[nodiscard]] inline auto render_conversation_reply(const conversation_reply_draft& aDraft) -> std::optional<std::string>
	{
		if (!valid(aDraft)) {
			return std::nullopt;
		}
		if (aDraft.Conversation == new_pull_request_comment) {
			return aDraft.Text;
		}
		auto result = std::string{ "In reply to " } + aDraft.Author + ":\n\n";
		auto quote  = std::string_view{ aDraft.Quote };
		do {
			const auto end = quote.find('\n');
			result += "> ";
			result += quote.substr(0, end);
			result += '\n';
			if (end == std::string_view::npos) {
				break;
			}
			quote.remove_prefix(end + 1);
		} while (!quote.empty());
		result += '\n';
		result += aDraft.Text;
		return result;
	}
}
#endif
