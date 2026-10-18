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
	/// Private text and the immutable accepted post it answers; no authority.
	struct conversation_reply_draft
	{
		std::string Conversation, Author, Quote, Text;
		auto        operator==(const conversation_reply_draft&) const -> bool = default;
	};
	[[nodiscard]] inline auto valid(const conversation_reply_draft& aDraft) -> bool
	{
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
