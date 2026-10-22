#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_CONVERSATION_REACTIONS_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_CONVERSATION_REACTIONS_HPP

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::core {
	/// \brief [[BPR-PENDING-AR1]]. The eight reactions GitHub offers on a
	/// comment, in the order its web interface shows them.
	enum class reaction_content : std::uint8_t {
		thumbs_up,
		thumbs_down,
		laugh,
		hooray,
		confused,
		heart,
		rocket,
		eyes,
	};

	inline constexpr std::array<reaction_content, 8> reaction_contents{
		reaction_content::thumbs_up,
		reaction_content::thumbs_down,
		reaction_content::laugh,
		reaction_content::hooray,
		reaction_content::confused,
		reaction_content::heart,
		reaction_content::rocket,
		reaction_content::eyes,
	};

	/// \brief How many reactor logins a group keeps: the first ones GitHub
	/// names, the rest counted.
	inline constexpr std::size_t maximum_named_reactors = 5U;

	/// \brief GitHub's `ReactionContent` name: `THUMBS_UP` and so on.
	[[nodiscard]] auto reaction_graphql_name(reaction_content aContent) noexcept -> std::string_view;
	/// \brief The content a `ReactionContent` name names; none for any other text.
	[[nodiscard]] auto reaction_from_graphql_name(std::string_view aName) noexcept -> std::optional<reaction_content>;
	/// \brief The emoji GitHub draws, as UTF-8.
	[[nodiscard]] auto reaction_emoji(reaction_content aContent) noexcept -> std::string_view;
	/// \brief The reaction's name in words, lower case: "thumbs up".
	[[nodiscard]] auto reaction_label(reaction_content aContent) noexcept -> std::string_view;

	/// \brief One reaction on one comment, as GitHub counted it.
	struct reaction_group
	{
		reaction_content Content = reaction_content::thumbs_up;
		/// Everyone who reacted with it, the viewer included.
		std::uint32_t Count = 0U;
		/// Whether the viewer is among them.
		bool ViewerReacted = false;
		/// The first logins GitHub named, at most `maximum_named_reactors`.
		std::vector<std::string> Reactors;
		/// A change the viewer asked for that GitHub has not answered yet.
		bool Pending = false;

		auto operator<=>(const reaction_group&) const = default;
	};

	/// \brief One comment's reactions: only the non-zero groups, in
	/// `reaction_contents` order, each content at most once.
	struct reaction_summary
	{
		/// GitHub's `viewerCanReact`.
		bool                        ViewerCanReact = false;
		std::vector<reaction_group> Groups;

		auto operator<=>(const reaction_summary&) const = default;
	};

	/// \brief The summary in its canonical form: zero groups dropped, groups
	/// in GitHub's order; none when a content appears twice or a count
	/// cannot hold what it says (a viewer reaction in an empty group, more
	/// names than reactions).
	[[nodiscard]] auto canonical_reaction_summary(reaction_summary aSummary) -> std::optional<reaction_summary>;

	/// \brief The group for one content, or null.
	[[nodiscard]] auto find_reaction_group(const reaction_summary& aSummary, reaction_content aContent) noexcept -> const reaction_group*;

	/// \brief Why a reaction is refused before any provider is asked.
	enum class reaction_refusal : std::uint8_t {
		none,
		/// The session reads anonymously; a reaction has no account to act as.
		anonymous_read,
		/// Reactions were not read for this comment, so there is no state to
		/// change and no permission to go by.
		unobserved,
		/// GitHub says this viewer may not react to this comment.
		not_permitted,
		/// A change to the same reaction on the same comment is unanswered.
		in_flight,
	};

	[[nodiscard]] auto name(reaction_refusal aRefusal) noexcept -> std::string_view;

	/// \brief What decides whether the viewer may add or remove a reaction.
	struct reaction_input
	{
		bool AnonymousRead  = false;
		bool Observed       = false;
		bool ViewerCanReact = false;
		bool InFlight       = false;

		auto operator<=>(const reaction_input&) const = default;
	};

	/// \brief Decides a reaction change from its inputs alone, in order:
	/// anonymous read, unobserved, not permitted, in flight.
	[[nodiscard]] constexpr auto decide_reaction(const reaction_input& aInput) noexcept -> reaction_refusal
	{
		if (aInput.AnonymousRead) {
			return reaction_refusal::anonymous_read;
		}
		if (!aInput.Observed) {
			return reaction_refusal::unobserved;
		}
		if (!aInput.ViewerCanReact) {
			return reaction_refusal::not_permitted;
		}
		if (aInput.InFlight) {
			return reaction_refusal::in_flight;
		}
		return reaction_refusal::none;
	}

	/// \brief One change the viewer asked for: a reaction added or removed,
	/// or a thread resolved or reopened. Kept until GitHub answers, and after
	/// that until a read agrees with it.
	struct conversation_change
	{
		/// The comment's identity for a reaction, the thread's for a resolution.
		std::string Subject;
		/// A reaction change's content; unused for a resolution.
		reaction_content Content = reaction_content::thumbs_up;
		/// Add the reaction, or resolve the thread.
		bool Add = true;
		/// GitHub answered that the change took.
		bool Confirmed = false;
		/// How many reads since GitHub's answer disagreed with it. A read
		/// begun before the change can arrive after it; the second one that
		/// disagrees is believed.
		std::uint8_t Disagreements = 0U;

		auto operator<=>(const conversation_change&) const = default;
	};

	/// \brief The viewer's changes that reads have not caught up with.
	struct conversation_overlay
	{
		std::vector<conversation_change> Reactions;
		std::vector<conversation_change> Resolutions;
		/// GitHub's own answer to a reaction write, per comment: newer than
		/// the last read until the next one.
		std::map<std::string, reaction_summary> Answered;

		auto operator<=>(const conversation_overlay&) const = default;
	};

	/// \brief Whether a reaction change to this comment and content is unanswered.
	[[nodiscard]] auto reaction_in_flight(const conversation_overlay& aOverlay, std::string_view aSubject, reaction_content aContent) noexcept -> bool;
	/// \brief Whether a resolution change to this thread is unanswered.
	[[nodiscard]] auto resolution_in_flight(const conversation_overlay& aOverlay, std::string_view aThread) noexcept -> bool;

	/// \brief Records a reaction change the viewer asked for; false, and
	/// nothing changed, while one to the same comment and content is unanswered.
	[[nodiscard]] auto begin_reaction_change(conversation_overlay& aOverlay, std::string aSubject, reaction_content aContent, bool aAdd) -> bool;
	/// \brief GitHub answered that the change took; `aAnswered` is the
	/// comment's reactions as that answer gave them, when it gave them.
	void confirm_reaction_change(conversation_overlay& aOverlay, std::string_view aSubject, reaction_content aContent, std::optional<reaction_summary> aAnswered);
	/// \brief The change did not take, or its outcome is unknown: the
	/// comment shows what was last read.
	void abandon_reaction_change(conversation_overlay& aOverlay, std::string_view aSubject, reaction_content aContent);

	/// \brief Records a resolution change; false while one to the same thread is unanswered.
	[[nodiscard]] auto begin_resolution_change(conversation_overlay& aOverlay, std::string aThread, bool aResolve) -> bool;
	void               confirm_resolution_change(conversation_overlay& aOverlay, std::string_view aThread);
	void               abandon_resolution_change(conversation_overlay& aOverlay, std::string_view aThread);

	/// \brief What one read observed, for reconciling the overlay: each
	/// comment's reactions when they were read, and each thread's resolution.
	struct conversation_observation
	{
		bool                                    ReactionsObserved = false;
		std::map<std::string, reaction_summary> Reactions;
		std::map<std::string, bool>             Resolved;
	};

	/// \brief A new read arrived. GitHub's write answers are dropped (the read
	/// is the newer account of everyone else's reactions); a confirmed change
	/// the read agrees with is done, and one the read disagrees with a second
	/// time is given up. Unanswered changes stay.
	void observe_conversation(conversation_overlay& aOverlay, const conversation_observation& aObservation);

	/// \brief One comment's reactions as the viewer should see them now: the
	/// latest of GitHub's answer and the read, with the viewer's own changes
	/// applied on top. None when neither was read.
	[[nodiscard]] auto project_reaction_summary(const reaction_summary* aObserved, const conversation_overlay& aOverlay, std::string_view aSubject, std::string_view aViewer)
		-> std::optional<reaction_summary>;

	/// \brief A thread's resolution as the viewer should see it now.
	struct projected_resolution
	{
		bool Resolved = false;
		/// The viewer's change is unanswered.
		bool Pending = false;

		auto operator<=>(const projected_resolution&) const = default;
	};

	[[nodiscard]] auto project_thread_resolution(bool aObserved, const conversation_overlay& aOverlay, std::string_view aThread) noexcept -> projected_resolution;

	/// \brief The change a click on a reaction asks for: add it when the
	/// viewer has not reacted with it, remove it when they have.
	[[nodiscard]] auto reaction_toggle(const std::optional<reaction_summary>& aShown, reaction_content aContent) noexcept -> bool;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_CONVERSATION_REACTIONS_HPP
