#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_ROLE_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_REVIEW_ROLE_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-1589]]. My role in a review, and what needs my attention.
///
/// \details RFC-BPR-0085 D7. The owner, 2026-10-17: "It's also very important
/// to know my role in this review. Is my review required? Am I just tagged? Am
/// I the author? Do I have no role?" These decide that from accepted facts
/// only. Nothing here claims my review is required: GitHub's code-owner
/// marking is carried as GitHub gave it, and a configured approval minimum
/// says nothing about whose approval is missing (RFC-BPR-0069).
namespace bpr::core {
	/// \brief What my latest submitted review said.
	enum class my_review_state : std::uint8_t {
		commented,
		approved,
		changes_requested,
		dismissed,
	};

	/// \brief Where I was mentioned.
	enum class mention_place : std::uint8_t {
		description,
		comment,
	};

	/// \brief One role, from strongest to weakest.
	enum class review_role_kind : std::uint8_t {
		author,
		/// A review requested from me by name.
		requested,
		/// A review requested from a team I am, or may be, in.
		team_requested,
		reviewed,
		mentioned,
	};

	/// \brief One outstanding review request as GitHub reports it.
	struct review_role_request
	{
		/// A login, or `organization/slug` for a team.
		std::string Name;
		bool        Team = false;
		/// GitHub marked the request as a code owner's.
		bool CodeOwner = false;

		auto operator<=>(const review_role_request&) const = default;
	};

	/// \brief What the role is decided from.
	struct review_role_facts
	{
		/// My login; empty when the viewer is not known.
		std::string                      Viewer;
		std::string                      Author;
		std::vector<review_role_request> Requests;
		/// The teams I am observed to be in, as `organization/slug`; absent
		/// when membership was not observed, which is every snapshot today.
		std::optional<std::vector<std::string>> ViewerTeams;
		/// My latest submitted review, if any.
		std::optional<my_review_state> LatestReview;
		/// Where someone else mentioned me first: the description before a
		/// comment.
		std::optional<mention_place> Mention;
	};

	/// \brief One role I hold.
	struct review_role
	{
		review_role_kind Kind = review_role_kind::author;
		/// `requested` and `team_requested`: GitHub marked a request a code
		/// owner's.
		bool CodeOwner = false;
		/// `team_requested`: the requested teams, in GitHub's order.
		std::vector<std::string> Teams;
		/// `team_requested`: whether my membership was observed. When it was
		/// not, the team is named and I "may be a member".
		bool MembershipObserved = false;
		/// `reviewed`: what my latest review said.
		std::optional<my_review_state> Review;
		/// `mentioned`: where.
		std::optional<mention_place> Mention;

		auto operator<=>(const review_role&) const = default;
	};

	/// \brief Every role I hold, strongest first.
	struct review_roles
	{
		/// False when the viewer is not known: then nothing is said.
		bool ViewerKnown = false;
		/// Empty with a known viewer is "no role".
		std::vector<review_role> Roles;

		auto operator<=>(const review_roles&) const = default;
	};

	/// \brief Decides my roles: author, then a direct request, then a team
	/// request, then my latest review, then a mention.
	[[nodiscard]] auto decide_review_roles(const review_role_facts& aFacts) -> review_roles;

	/// \brief Whether two GitHub logins or team names are the same; GitHub
	/// compares them without regard to ASCII case.
	[[nodiscard]] auto same_login(std::string_view aLeft, std::string_view aRight) noexcept -> bool;

	/// \brief Whether Markdown text mentions a login as GitHub links it.
	///
	/// \details An `@` that starts the text or follows a character that is
	/// not a letter, digit, `_`, `` ` `` or another `@`, then the login in any
	/// case, then anything but a letter, digit, `-` or `/` (`@org/team` is a
	/// team's mention). Fenced code blocks and inline code spans are not
	/// searched, as GitHub does not link mentions there.
	[[nodiscard]] auto mentions_login(std::string_view aMarkdown, std::string_view aLogin) -> bool;

	/// \brief One thing that needs my attention.
	enum class status_attention_kind : std::uint8_t {
		conflicts,
		failed_checks,
		waiting_on,
		replies_to_me,
	};

	struct status_attention
	{
		status_attention_kind Kind  = status_attention_kind::conflicts;
		std::size_t           Count = 0U;
		/// The failed checks' or the awaited reviewers' names, in order.
		std::vector<std::string> Names;

		auto operator<=>(const status_attention&) const = default;
	};

	/// \brief What the attention list is decided from.
	struct status_attention_facts
	{
		std::string Viewer;
		/// GitHub reports merge conflicts.
		bool Conflicts = false;
		/// Each failed check's name, once.
		std::vector<std::string> FailedChecks;
		/// Outstanding review requests, code owners first; a request of mine
		/// is my role, not something I wait on.
		std::vector<std::string> WaitingOn;
		std::size_t              RepliesToMe = 0U;
	};

	/// \brief Decides the attention list in RFC-BPR-0085's order: conflicts,
	/// failed checks, who is waited on, replies to me. Only what applies.
	[[nodiscard]] auto decide_status_attention(const status_attention_facts& aFacts) -> std::vector<status_attention>;
} // namespace bpr::core

#endif
