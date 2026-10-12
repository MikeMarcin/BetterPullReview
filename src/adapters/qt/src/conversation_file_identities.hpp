#ifndef BPR_SRC_ADAPTERS_QT_SRC_CONVERSATION_FILE_IDENTITIES_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_CONVERSATION_FILE_IDENTITIES_HPP

#include "bpr/application/github_projection.hpp"
#include "bpr/core/diff_model.hpp"

#include <string_view>
#include <unordered_map>
#include <vector>

namespace bpr::qt::detail {
	/// \brief Derives each distinct path once for one immutable conversation list.
	[[nodiscard]] inline auto conversation_file_identities(
		const std::vector<application::github_conversation_projection>& aConversations)
		-> std::vector<core::file_identity>
	{
		auto files = std::vector<core::file_identity>{ };
		files.reserve(aConversations.size());
		auto byPath = std::unordered_map<std::string_view, core::file_identity>{ };
		byPath.reserve(aConversations.size());
		for (const auto& conversation : aConversations) {
			const auto [held, inserted] = byPath.try_emplace(conversation.Path);
			if (inserted) {
				held->second = core::file_identity_of(conversation.Path);
			}
			files.push_back(held->second);
		}
		return files;
	}
}

#endif
