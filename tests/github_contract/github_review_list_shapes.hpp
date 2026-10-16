#ifndef BPR_TESTS_GITHUB_CONTRACT_GITHUB_REVIEW_LIST_SHAPES_HPP
#define BPR_TESTS_GITHUB_CONTRACT_GITHUB_REVIEW_LIST_SHAPES_HPP

// [[BPR-1538]]. Reviews and review comments in the complete shape GitHub
// lists them, every field GitHub sends, as captured from
// MikeMarcin/BPR-Test#70 on 2026-10-15. A page of 100 such comments is about
// 380 KB and 6,200 JSON values, so fixtures that list only the fields the
// product reads cannot show whether a real page is read at all. The GitHub
// Enterprise Server shape is the same document under the instance's own
// origin and `/api/v3` (GHES 3.18 REST description, 2022-11-28).

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace bpr::tests::github_shapes {
	/// Where one server's API and web pages live.
	struct server
	{
		/// `https://api.github.com` or `https://HOST/api/v3`.
		std::string Api;
		/// `https://github.com` or `https://HOST`.
		std::string Web;
	};
	[[nodiscard]] inline auto github_com() -> server { return { .Api = "https://api.github.com", .Web = "https://github.com" }; }
	[[nodiscard]] inline auto enterprise(const std::string_view aHost) -> server
	{
		return { .Api = "https://" + std::string{ aHost } + "/api/v3", .Web = "https://" + std::string{ aHost } };
	}

	/// The pull request a page belongs to.
	struct pull
	{
		server        Server;
		std::string   Owner;
		std::string   Name;
		std::uint64_t RepositoryId = 0U;
		std::uint32_t Number       = 0U;

		[[nodiscard]] auto api() const -> std::string { return Server.Api + "/repos/" + Owner + "/" + Name + "/pulls/" + std::to_string(Number); }
		[[nodiscard]] auto web() const -> std::string { return Server.Web + "/" + Owner + "/" + Name + "/pull/" + std::to_string(Number); }
		/// GitHub's `Link` URL for a page of the comment collection: by the
		/// repository's number, not its name.
		[[nodiscard]] auto comments_page(const std::uint32_t aPage) const -> std::string
		{
			return Server.Api + "/repositories/" + std::to_string(RepositoryId) + "/pulls/" + std::to_string(Number) + "/comments?per_page=100&page=" + std::to_string(aPage);
		}
	};

	struct user
	{
		std::uint64_t Id = 0U;
		std::string   NodeId;
		std::string   Login;
	};

	[[nodiscard]] inline auto user_json(const server& aServer, const user& aUser) -> std::string
	{
		const auto api = aServer.Api + "/users/" + aUser.Login;
		return "{\"login\":\"" + aUser.Login + "\",\"id\":" + std::to_string(aUser.Id) + ",\"node_id\":\"" + aUser.NodeId + "\",\"avatar_url\":\"https://avatars.githubusercontent.com/u/"
			+ std::to_string(aUser.Id) + "?v=4\",\"gravatar_id\":\"\",\"url\":\"" + api + "\",\"html_url\":\"" + aServer.Web + "/" + aUser.Login + "\",\"followers_url\":\"" + api
			+ "/followers\",\"following_url\":\"" + api + "/following{/other_user}\",\"gists_url\":\"" + api + "/gists{/gist_id}\",\"starred_url\":\"" + api
			+ "/starred{/owner}{/repo}\",\"subscriptions_url\":\"" + api + "/subscriptions\",\"organizations_url\":\"" + api + "/orgs\",\"repos_url\":\"" + api
			+ "/repos\",\"events_url\":\"" + api + "/events{/privacy}\",\"received_events_url\":\"" + api + "/received_events\",\"type\":\"User\",\"user_view_type\":\"public\",\"site_admin\":false}";
	}

	/// One comment of the collection. `Line` and the rest are GitHub's
	/// one-based numbers; `Line` absent is an outdated comment.
	struct comment
	{
		std::uint64_t                Id     = 0U;
		std::uint64_t                Review = 0U;
		std::string                  Path;
		std::string                  Body;
		user                         Author;
		std::string                  Commit;
		std::string                  OriginalCommit;
		std::optional<std::uint32_t> Line;
		std::optional<std::uint32_t> StartLine;
		std::uint32_t                OriginalLine = 1U;
		std::optional<std::uint32_t> OriginalStartLine;
		std::string                  Side = "RIGHT";
		std::optional<std::uint64_t> InReplyTo;
		/// The diff hunk's lines up to the comment's line.
		std::uint32_t HunkLines = 20U;
		/// [[BPR-1540]]. A server that names no original position.
		bool OmitOriginal = false;
	};

	[[nodiscard]] inline auto number_or_null(const std::optional<std::uint32_t> aValue) -> std::string
	{
		return aValue ? std::to_string(*aValue) : std::string{ "null" };
	}

	[[nodiscard]] inline auto comment_json(const pull& aPull, const comment& aComment) -> std::string
	{
		auto hunk = std::string{ "@@ -0,0 +1," } + std::to_string(aComment.HunkLines) + " @@";
		for (auto line = 1U; line <= aComment.HunkLines; ++line) {
			hunk += "\\n+line " + std::to_string(line) + ": added on the head, with enough text to read like code";
		}
		const auto id     = std::to_string(aComment.Id);
		const auto self   = aPull.Server.Api + "/repos/" + aPull.Owner + "/" + aPull.Name + "/pulls/comments/" + id;
		const auto html   = aPull.web() + "#discussion_r" + id;
		const auto start  = aComment.StartLine || aComment.OriginalStartLine ? "\"" + aComment.Side + "\"" : std::string{ "null" };
		auto       result = "{\"url\":\"" + self + "\",\"pull_request_review_id\":" + std::to_string(aComment.Review) + ",\"id\":" + id + ",\"node_id\":\"PRRC_kwDOTyvof8" + id
			+ "\",\"diff_hunk\":\"" + hunk + "\",\"path\":\"" + aComment.Path + "\",\"commit_id\":\"" + aComment.Commit
			+ (aComment.OmitOriginal ? std::string{ } : "\",\"original_commit_id\":\"" + aComment.OriginalCommit) + "\",\"user\":" + user_json(aPull.Server, aComment.Author) + ",\"body\":\"" + aComment.Body
			+ "\",\"created_at\":\"2026-10-15T23:13:42Z\",\"updated_at\":\"2026-10-15T23:13:42Z\",\"html_url\":\"" + html + "\",\"pull_request_url\":\"" + aPull.api()
			+ "\",\"_links\":{\"self\":{\"href\":\"" + self + "\"},\"html\":{\"href\":\"" + html + "\"},\"pull_request\":{\"href\":\"" + aPull.api() + "\"}},\"reactions\":{\"url\":\"" + self
			+ "/reactions\",\"total_count\":0,\"+1\":0,\"-1\":0,\"laugh\":0,\"hooray\":0,\"confused\":0,\"heart\":0,\"rocket\":0,\"eyes\":0},\"start_line\":" + number_or_null(aComment.StartLine)
			+ (aComment.OmitOriginal ? std::string{ } : ",\"original_start_line\":" + number_or_null(aComment.OriginalStartLine)) + ",\"start_side\":" + start + ",\"line\":" + number_or_null(aComment.Line)
			+ (aComment.OmitOriginal ? std::string{ } : ",\"original_line\":" + std::to_string(aComment.OriginalLine)) + ",\"side\":\"" + aComment.Side + "\",\"author_association\":\"CONTRIBUTOR\",\"original_position\":"
			+ std::to_string(aComment.OriginalLine) + ",\"position\":" + (aComment.Line ? std::to_string(*aComment.Line) : std::string{ "null" }) + ",\"subject_type\":\"line\"";
		if (aComment.InReplyTo) {
			result += ",\"in_reply_to_id\":" + std::to_string(*aComment.InReplyTo);
		}
		return result + "}";
	}

	/// One review of the review list.
	struct review
	{
		std::uint64_t Id = 0U;
		user          Author;
		std::string   Body;
		std::string   State = "COMMENTED";
		std::string   Commit;
	};

	[[nodiscard]] inline auto review_json(const pull& aPull, const review& aReview) -> std::string
	{
		const auto html = aPull.web() + "#pullrequestreview-" + std::to_string(aReview.Id);
		return "{\"id\":" + std::to_string(aReview.Id) + ",\"node_id\":\"PRR_kwDOTyvof8" + std::to_string(aReview.Id) + "\",\"user\":" + user_json(aPull.Server, aReview.Author)
			+ ",\"body\":\"" + aReview.Body + "\",\"state\":\"" + aReview.State + "\",\"html_url\":\"" + html + "\",\"pull_request_url\":\"" + aPull.api()
			+ "\",\"author_association\":\"CONTRIBUTOR\",\"_links\":{\"html\":{\"href\":\"" + html + "\"},\"pull_request\":{\"href\":\"" + aPull.api()
			+ "\"}},\"submitted_at\":\"2026-10-15T23:13:42Z\",\"commit_id\":\"" + aReview.Commit + "\"}";
	}
} // namespace bpr::tests::github_shapes

#endif // BPR_TESTS_GITHUB_CONTRACT_GITHUB_REVIEW_LIST_SHAPES_HPP
