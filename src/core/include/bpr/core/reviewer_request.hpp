#ifndef BPR_CORE_REVIEWER_REQUEST_HPP
#define BPR_CORE_REVIEWER_REQUEST_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

/// \file
/// \brief [[BPR-1804]]. The reviewer picker in Overview's People section:
/// what it shows, which search answer it keeps, and whether choosing a
/// person asks GitHub for a review.
///
/// \details The owner, 2026-10-28: "Can I add a review from the people
/// section of the overview page? The website gives a nice people search for
/// that". The picker is a small state machine. Every gesture is a pure
/// transition that may name one effect: a search, identified by a number so
/// a late answer to an older query is dropped, or one review request,
/// identified the same way. The host performs effects, debounces typing and
/// cancels; Qt presents the state and keeps the keyboard highlight.
namespace bpr::core {
	/// \brief The longest query sent, in bytes; longer text is cut at a
	/// character boundary. GitHub logins are at most 39 characters.
	inline constexpr std::size_t maximum_reviewer_query_bytes = 64U;
	/// \brief The most people the picker lists.
	inline constexpr std::size_t maximum_reviewer_rows = 30U;

	/// \brief One person GitHub offers as a reviewer.
	struct reviewer_candidate
	{
		/// GitHub's node identity, the one a request names.
		std::string NodeId;
		std::string Login;
		/// The profile name; empty when GitHub has none.
		std::string Name;
		/// GitHub suggests them (they changed or commented on these lines).
		bool Suggested = false;
		/// GitHub says they have commented on the pull request.
		bool Commenter = false;

		auto operator<=>(const reviewer_candidate&) const = default;
	};

	/// \brief What one search answered, as the provider read it.
	struct reviewer_search_answer
	{
		/// The pull request's author; GitHub requests no review from them.
		std::string Author;
		/// GitHub's suggested reviewers, which a query does not filter.
		std::vector<reviewer_candidate> Suggested;
		/// The people who may be asked that match the query, in GitHub's
		/// order.
		std::vector<reviewer_candidate> Matches;
		/// The logins with an open review request.
		std::vector<std::string> Requested;
		/// The logins with a submitted review.
		std::vector<std::string> Reviewed;

		auto operator<=>(const reviewer_search_answer&) const = default;
	};

	/// \brief Where one listed person stands.
	enum class reviewer_standing : std::uint8_t {
		/// No review is requested or in.
		available,
		/// A review request is open.
		requested,
		/// They reviewed; choosing them asks again (re-request).
		reviewed,
	};

	[[nodiscard]] auto name(reviewer_standing aStanding) noexcept -> std::string_view;

	struct reviewer_row
	{
		reviewer_candidate Candidate;
		reviewer_standing  Standing = reviewer_standing::available;

		auto operator<=>(const reviewer_row&) const = default;
	};

	/// \brief The rows a search answer gives for a query: GitHub's
	/// suggestions that match first, then the other matches in GitHub's order,
	/// each login once (logins compare without ASCII case), never the author,
	/// at most `maximum_reviewer_rows`.
	[[nodiscard]] auto reviewer_rows(const reviewer_search_answer& aAnswer, std::string_view aQuery) -> std::vector<reviewer_row>;

	/// \brief The query as it is sent: surrounding spaces and one leading `@`
	/// dropped, cut to `maximum_reviewer_query_bytes` at a character boundary.
	[[nodiscard]] auto normalize_reviewer_query(std::string_view aText) -> std::string;

	/// \brief Why choosing a person asks nothing of GitHub.
	enum class reviewer_choice_refusal : std::uint8_t {
		none,
		/// The picker is closed.
		closed,
		/// A request is already waiting for GitHub's answer.
		busy,
		/// The login is not one the picker lists.
		unknown,
		/// A review request is already open for them.
		already_requested,
	};

	[[nodiscard]] auto name(reviewer_choice_refusal aRefusal) noexcept -> std::string_view;

	/// \brief Why a request did not end with the person asked.
	enum class reviewer_request_failure : std::uint8_t {
		/// The connection failed or could not tell what happened.
		connection,
		/// GitHub refused: the viewer may not request reviews here.
		refused,
		/// The pull request changed or is no longer the one in view.
		target_changed,
		/// The request was stopped.
		cancelled,
	};

	[[nodiscard]] auto name(reviewer_request_failure aFailure) noexcept -> std::string_view;

	/// \brief What the picker last has to say about a choice.
	enum class reviewer_notice : std::uint8_t {
		none,
		/// GitHub has the request.
		requested,
		/// GitHub has the request again, after a review.
		rerequested,
		/// GitHub already had a request open; nothing was sent.
		already_requested,
		/// GitHub answered but did not add them: they cannot review here (not
		/// a collaborator with access).
		not_added,
		/// The request failed; `reviewer_request_failure` says how.
		failed,
		/// A request is still waiting for GitHub.
		busy,
	};

	[[nodiscard]] auto name(reviewer_notice aNotice) noexcept -> std::string_view;

	/// \brief The picker as the host keeps it and Qt presents it.
	struct reviewer_picker_state
	{
		bool Open = false;
		/// The normalized query of the latest search asked.
		std::string Query;
		/// The identity of the latest search asked; 0 before any.
		std::uint64_t Search = 0U;
		/// The latest search has not answered.
		bool Searching = false;
		/// The latest search failed.
		std::optional<reviewer_request_failure> SearchFailure;
		/// The latest answer, kept so a later query reorders without asking.
		std::optional<reviewer_search_answer> Answer;
		std::vector<reviewer_row>             Rows;
		/// The identity of the latest request asked; 0 before any.
		std::uint64_t Request = 0U;
		/// The login a request is waiting on.
		std::optional<std::string>              Pending;
		reviewer_notice                         Notice = reviewer_notice::none;
		std::string                             NoticeLogin;
		std::optional<reviewer_request_failure> NoticeFailure;

		auto operator==(const reviewer_picker_state&) const -> bool = default;
	};

	/// \brief Ask for the people matching `Query`.
	struct reviewer_search_effect
	{
		std::uint64_t Search = 0U;
		std::string   Query;

		auto operator<=>(const reviewer_search_effect&) const = default;
	};

	/// \brief Ask GitHub for one review.
	struct reviewer_request_effect
	{
		std::uint64_t Request = 0U;
		std::string   NodeId;
		std::string   Login;
		/// They reviewed already: this asks again.
		bool Rerequest = false;

		auto operator<=>(const reviewer_request_effect&) const = default;
	};

	using reviewer_picker_effect = std::variant<std::monostate, reviewer_search_effect, reviewer_request_effect>;

	struct reviewer_picker_transition
	{
		reviewer_picker_state  State;
		reviewer_picker_effect Effect;
		/// Set when a choice asked nothing of GitHub.
		reviewer_choice_refusal Refusal = reviewer_choice_refusal::none;
	};

	/// \brief Opens the picker on an empty query and asks for GitHub's
	/// suggestions and the first people. Already open, it asks nothing.
	[[nodiscard]] auto open_reviewer_picker(reviewer_picker_state aState) -> reviewer_picker_transition;

	/// \brief The query changed. A new normalized query is a new search, and
	/// the rows already known are filtered at once; the same one asks nothing.
	[[nodiscard]] auto type_reviewer_query(reviewer_picker_state aState, std::string_view aText) -> reviewer_picker_transition;

	/// \brief A search answered. Only the latest search's answer is kept.
	[[nodiscard]] auto accept_reviewer_search(reviewer_picker_state aState, std::uint64_t aSearch, reviewer_search_answer aAnswer) -> reviewer_picker_state;

	/// \brief A search failed. Only the latest search's failure is shown.
	[[nodiscard]] auto fail_reviewer_search(reviewer_picker_state aState, std::uint64_t aSearch, reviewer_request_failure aFailure) -> reviewer_picker_state;

	/// \brief A person was chosen: one request when they may be asked, or a
	/// refusal in order closed, busy, unknown, already requested.
	[[nodiscard]] auto choose_reviewer(reviewer_picker_state aState, std::string_view aLogin) -> reviewer_picker_transition;

	/// \brief What a request's provider answered: the logins GitHub now has
	/// a request open for, and whether a write was sent.
	struct reviewer_request_answer
	{
		std::vector<std::string> Requested;
		bool                     Written = false;

		auto operator<=>(const reviewer_request_answer&) const = default;
	};

	/// \brief A request answered. Only the latest request's answer is kept:
	/// requested (or re-requested) when GitHub lists the login, already
	/// requested when nothing was sent and it does, not added otherwise.
	[[nodiscard]] auto finish_reviewer_request(reviewer_picker_state aState, std::uint64_t aRequest, const reviewer_request_answer& aAnswer) -> reviewer_picker_state;

	/// \brief A request failed.
	[[nodiscard]] auto fail_reviewer_request(reviewer_picker_state aState, std::uint64_t aRequest, reviewer_request_failure aFailure) -> reviewer_picker_state;

	/// \brief Closes the picker. A request in flight still finishes, and its
	/// notice is kept for the next opening.
	[[nodiscard]] auto close_reviewer_picker(reviewer_picker_state aState) -> reviewer_picker_state;

	/// \brief The provider's decision just before a write: whether GitHub,
	/// asked first, already has the request open, or the person is the
	/// author, whom GitHub never asks.
	enum class reviewer_write_decision : std::uint8_t {
		write,
		already_requested,
		author,
	};

	[[nodiscard]] auto decide_reviewer_write(const reviewer_search_answer& aObserved, std::string_view aLogin) noexcept -> reviewer_write_decision;
} // namespace bpr::core

#endif
