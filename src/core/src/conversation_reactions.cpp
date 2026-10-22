#include "bpr/core/conversation_reactions.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto order(const reaction_content aContent) noexcept -> std::size_t
		{
			return static_cast<std::size_t>(aContent);
		}

		[[nodiscard]] auto valid(const reaction_content aContent) noexcept -> bool
		{
			return order(aContent) < reaction_contents.size();
		}

		[[nodiscard]] auto matches(const conversation_change& aChange, const std::string_view aSubject) noexcept -> bool
		{
			return aChange.Subject == aSubject;
		}

		[[nodiscard]] auto matches(const conversation_change& aChange, const std::string_view aSubject, const reaction_content aContent) noexcept -> bool
		{
			return aChange.Subject == aSubject && aChange.Content == aContent;
		}

		/// The viewer's change applied to one group: the count moves by one and
		/// the viewer's login joins or leaves the named reactors.
		void apply(reaction_summary& aSummary, const conversation_change& aChange, const std::string_view aViewer)
		{
			auto group = std::ranges::find(aSummary.Groups, aChange.Content, &reaction_group::Content);
			if (aChange.Add) {
				if (group == aSummary.Groups.end()) {
					aSummary.Groups.push_back(reaction_group{ .Content = aChange.Content });
					group = std::prev(aSummary.Groups.end());
				}
				if (!group->ViewerReacted) {
					group->ViewerReacted = true;
					++group->Count;
					if (!aViewer.empty() && std::ranges::find(group->Reactors, aViewer) == group->Reactors.end()) {
						group->Reactors.insert(group->Reactors.begin(), std::string{ aViewer });
						if (group->Reactors.size() > maximum_named_reactors) {
							group->Reactors.pop_back();
						}
					}
				}
			} else if (group != aSummary.Groups.end() && group->ViewerReacted) {
				group->ViewerReacted = false;
				group->Count         = group->Count > 0U ? group->Count - 1U : 0U;
				if (const auto named = std::ranges::find(group->Reactors, aViewer); !aViewer.empty() && named != group->Reactors.end()) {
					group->Reactors.erase(named);
				}
			}
			if (group != aSummary.Groups.end() && !aChange.Confirmed) {
				group->Pending = true;
			}
		}

		void reconcile(std::vector<conversation_change>& aChanges, const auto& aAgrees)
		{
			std::erase_if(aChanges, [&aAgrees](conversation_change& aChange) {
				if (!aChange.Confirmed) {
					return false;
				}
				const auto agrees = aAgrees(aChange);
				if (!agrees.has_value() || *agrees) {
					return true;
				}
				++aChange.Disagreements;
				return aChange.Disagreements >= 2U;
			});
		}
	} // namespace

	auto reaction_graphql_name(const reaction_content aContent) noexcept -> std::string_view
	{
		switch (aContent) {
		case reaction_content::thumbs_up:   return "THUMBS_UP";
		case reaction_content::thumbs_down: return "THUMBS_DOWN";
		case reaction_content::laugh:       return "LAUGH";
		case reaction_content::hooray:      return "HOORAY";
		case reaction_content::confused:    return "CONFUSED";
		case reaction_content::heart:       return "HEART";
		case reaction_content::rocket:      return "ROCKET";
		case reaction_content::eyes:        return "EYES";
		}
		return { };
	}

	auto reaction_from_graphql_name(const std::string_view aName) noexcept -> std::optional<reaction_content>
	{
		for (const auto content : reaction_contents) {
			if (reaction_graphql_name(content) == aName) {
				return content;
			}
		}
		return std::nullopt;
	}

	auto reaction_emoji(const reaction_content aContent) noexcept -> std::string_view
	{
		switch (aContent) {
		case reaction_content::thumbs_up:   return "\xF0\x9F\x91\x8D";
		case reaction_content::thumbs_down: return "\xF0\x9F\x91\x8E";
		case reaction_content::laugh:       return "\xF0\x9F\x98\x84";
		case reaction_content::hooray:      return "\xF0\x9F\x8E\x89";
		case reaction_content::confused:    return "\xF0\x9F\x98\x95";
		case reaction_content::heart:       return "\xE2\x9D\xA4\xEF\xB8\x8F";
		case reaction_content::rocket:      return "\xF0\x9F\x9A\x80";
		case reaction_content::eyes:        return "\xF0\x9F\x91\x80";
		}
		return { };
	}

	auto reaction_label(const reaction_content aContent) noexcept -> std::string_view
	{
		switch (aContent) {
		case reaction_content::thumbs_up:   return "thumbs up";
		case reaction_content::thumbs_down: return "thumbs down";
		case reaction_content::laugh:       return "laugh";
		case reaction_content::hooray:      return "hooray";
		case reaction_content::confused:    return "confused";
		case reaction_content::heart:       return "heart";
		case reaction_content::rocket:      return "rocket";
		case reaction_content::eyes:        return "eyes";
		}
		return { };
	}

	auto name(const reaction_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case reaction_refusal::none:           return "none";
		case reaction_refusal::anonymous_read: return "anonymous_read";
		case reaction_refusal::unobserved:     return "unobserved";
		case reaction_refusal::not_permitted:  return "not_permitted";
		case reaction_refusal::in_flight:      return "in_flight";
		}
		return { };
	}

	auto canonical_reaction_summary(reaction_summary aSummary) -> std::optional<reaction_summary>
	{
		auto seen = std::array<bool, reaction_contents.size()>{ };
		for (const auto& group : aSummary.Groups) {
			if (!valid(group.Content) || seen[order(group.Content)] || group.Reactors.size() > maximum_named_reactors
				|| group.Reactors.size() > group.Count || (group.ViewerReacted && group.Count == 0U)
				|| std::ranges::any_of(group.Reactors, [](const std::string& aLogin) { return aLogin.empty(); })) {
				return std::nullopt;
			}
			seen[order(group.Content)] = true;
		}
		std::erase_if(aSummary.Groups, [](const reaction_group& aGroup) { return aGroup.Count == 0U; });
		std::ranges::sort(aSummary.Groups, { }, [](const reaction_group& aGroup) { return order(aGroup.Content); });
		return aSummary;
	}

	auto find_reaction_group(const reaction_summary& aSummary, const reaction_content aContent) noexcept -> const reaction_group*
	{
		const auto found = std::ranges::find(aSummary.Groups, aContent, &reaction_group::Content);
		return found == aSummary.Groups.end() ? nullptr : &*found;
	}

	auto reaction_in_flight(const conversation_overlay& aOverlay, const std::string_view aSubject, const reaction_content aContent) noexcept -> bool
	{
		return std::ranges::any_of(aOverlay.Reactions, [&](const conversation_change& aChange) { return !aChange.Confirmed && matches(aChange, aSubject, aContent); });
	}

	auto resolution_in_flight(const conversation_overlay& aOverlay, const std::string_view aThread) noexcept -> bool
	{
		return std::ranges::any_of(aOverlay.Resolutions, [&](const conversation_change& aChange) { return !aChange.Confirmed && matches(aChange, aThread); });
	}

	auto begin_reaction_change(conversation_overlay& aOverlay, std::string aSubject, const reaction_content aContent, const bool aAdd) -> bool
	{
		if (aSubject.empty() || !valid(aContent) || reaction_in_flight(aOverlay, aSubject, aContent)) {
			return false;
		}
		// A confirmed change to the same reaction is superseded by this one.
		std::erase_if(aOverlay.Reactions, [&](const conversation_change& aChange) { return matches(aChange, aSubject, aContent); });
		aOverlay.Reactions.push_back(conversation_change{ .Subject = std::move(aSubject), .Content = aContent, .Add = aAdd });
		return true;
	}

	void confirm_reaction_change(conversation_overlay& aOverlay, const std::string_view aSubject, const reaction_content aContent, std::optional<reaction_summary> aAnswered)
	{
		const auto change = std::ranges::find_if(aOverlay.Reactions, [&](const conversation_change& aChange) { return !aChange.Confirmed && matches(aChange, aSubject, aContent); });
		if (change == aOverlay.Reactions.end()) {
			return;
		}
		change->Confirmed = true;
		if (aAnswered.has_value()) {
			if (auto canonical = canonical_reaction_summary(std::move(*aAnswered)); canonical.has_value()) {
				aOverlay.Answered.insert_or_assign(std::string{ aSubject }, std::move(*canonical));
			}
		}
	}

	void abandon_reaction_change(conversation_overlay& aOverlay, const std::string_view aSubject, const reaction_content aContent)
	{
		std::erase_if(aOverlay.Reactions, [&](const conversation_change& aChange) { return !aChange.Confirmed && matches(aChange, aSubject, aContent); });
	}

	auto begin_resolution_change(conversation_overlay& aOverlay, std::string aThread, const bool aResolve) -> bool
	{
		if (aThread.empty() || resolution_in_flight(aOverlay, aThread)) {
			return false;
		}
		std::erase_if(aOverlay.Resolutions, [&](const conversation_change& aChange) { return matches(aChange, aThread); });
		aOverlay.Resolutions.push_back(conversation_change{ .Subject = std::move(aThread), .Add = aResolve });
		return true;
	}

	void confirm_resolution_change(conversation_overlay& aOverlay, const std::string_view aThread)
	{
		for (auto& change : aOverlay.Resolutions) {
			if (!change.Confirmed && matches(change, aThread)) {
				change.Confirmed = true;
			}
		}
	}

	void abandon_resolution_change(conversation_overlay& aOverlay, const std::string_view aThread)
	{
		std::erase_if(aOverlay.Resolutions, [&](const conversation_change& aChange) { return !aChange.Confirmed && matches(aChange, aThread); });
	}

	void observe_conversation(conversation_overlay& aOverlay, const conversation_observation& aObservation)
	{
		if (aObservation.ReactionsObserved) {
			aOverlay.Answered.clear();
			reconcile(aOverlay.Reactions, [&](const conversation_change& aChange) -> std::optional<bool> {
				const auto read = aObservation.Reactions.find(aChange.Subject);
				if (read == aObservation.Reactions.end()) {
					// The comment is gone from the read; nothing is left to show.
					return std::nullopt;
				}
				const auto* const group = find_reaction_group(read->second, aChange.Content);
				return (group != nullptr && group->ViewerReacted) == aChange.Add;
			});
		}
		reconcile(aOverlay.Resolutions, [&](const conversation_change& aChange) -> std::optional<bool> {
			const auto read = aObservation.Resolved.find(aChange.Subject);
			if (read == aObservation.Resolved.end()) {
				return std::nullopt;
			}
			return read->second == aChange.Add;
		});
	}

	auto project_reaction_summary(const reaction_summary* const aObserved, const conversation_overlay& aOverlay, const std::string_view aSubject, const std::string_view aViewer)
		-> std::optional<reaction_summary>
	{
		auto       summary  = std::optional<reaction_summary>{ };
		const auto answered = aOverlay.Answered.find(std::string{ aSubject });
		if (answered != aOverlay.Answered.end()) {
			summary = answered->second;
		} else if (aObserved != nullptr) {
			summary = *aObserved;
		}
		if (!summary.has_value()) {
			return std::nullopt;
		}
		for (const auto& change : aOverlay.Reactions) {
			if (matches(change, aSubject)) {
				apply(*summary, change, aViewer);
			}
		}
		std::erase_if(summary->Groups, [](const reaction_group& aGroup) { return aGroup.Count == 0U; });
		std::ranges::sort(summary->Groups, { }, [](const reaction_group& aGroup) { return order(aGroup.Content); });
		return summary;
	}

	auto project_thread_resolution(const bool aObserved, const conversation_overlay& aOverlay, const std::string_view aThread) noexcept -> projected_resolution
	{
		auto result = projected_resolution{ .Resolved = aObserved };
		for (const auto& change : aOverlay.Resolutions) {
			if (matches(change, aThread)) {
				result.Resolved = change.Add;
				result.Pending  = !change.Confirmed;
			}
		}
		return result;
	}

	auto reaction_toggle(const std::optional<reaction_summary>& aShown, const reaction_content aContent) noexcept -> bool
	{
		if (!aShown.has_value()) {
			return true;
		}
		const auto* const group = find_reaction_group(*aShown, aContent);
		return group == nullptr || !group->ViewerReacted;
	}
} // namespace bpr::core
