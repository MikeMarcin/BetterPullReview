#include "bpr/core/prompt_pack.hpp"

#include "bpr/core/prompt_surfaces.hpp"
#include "bpr/core/utf8.hpp"

#include <glaze/glaze.hpp>

#include <algorithm>
#include <iterator>
#include <set>
#include <tuple>
#include <utility>

namespace bpr::core::detail {
	/// RFC-BPR-0125 AC24. A prompt pack as glaze reads and writes it. A
	/// condition carries only the value its fact reads; a list left empty is
	/// left out.
	struct wire_pack_condition
	{
		std::string                             Fact;
		bool                                    Negated = false;
		std::optional<std::string>              Lines;
		std::optional<std::vector<std::string>> Patterns;
		std::optional<std::string>              Activity;
	};
	/// The deepest group: the prompt's list, then two levels of groups.
	struct wire_pack_inner_group
	{
		std::string                      Match;
		std::vector<wire_pack_condition> Conditions;
	};
	struct wire_pack_group
	{
		std::string                                       Match;
		std::vector<wire_pack_condition>                  Conditions;
		std::optional<std::vector<wire_pack_inner_group>> Groups;
	};
	struct wire_pack_macro
	{
		std::string                                     Id;
		std::string                                     Place;
		std::string                                     Name;
		std::string                                     Template;
		std::string                                     About;
		std::string                                     Ask;
		std::string                                     Match;
		std::string                                     WhenUnmet;
		bool                                            QuickAsk = false;
		std::optional<std::vector<std::string>>         Scopes;
		std::optional<std::vector<wire_pack_condition>> Conditions;
		std::optional<std::vector<wire_pack_group>>     Groups;
	};
	struct wire_pack
	{
		std::string                  Id;
		std::string                  Label;
		std::uint32_t                Version = 0U;
		std::vector<wire_pack_macro> Prompts;
	};
} // namespace bpr::core::detail

template <>
struct glz::meta<bpr::core::detail::wire_pack_condition>
{
	using value_type            = bpr::core::detail::wire_pack_condition;
	static constexpr auto value = object("fact", &value_type::Fact, "negated", &value_type::Negated, "lines", &value_type::Lines, "patterns", &value_type::Patterns, "activity", &value_type::Activity);
};
template <>
struct glz::meta<bpr::core::detail::wire_pack_inner_group>
{
	using value_type            = bpr::core::detail::wire_pack_inner_group;
	static constexpr auto value = object("match", &value_type::Match, "conditions", &value_type::Conditions);
};
template <>
struct glz::meta<bpr::core::detail::wire_pack_group>
{
	using value_type            = bpr::core::detail::wire_pack_group;
	static constexpr auto value = object("match", &value_type::Match, "conditions", &value_type::Conditions, "groups", &value_type::Groups);
};
template <>
struct glz::meta<bpr::core::detail::wire_pack_macro>
{
	using value_type            = bpr::core::detail::wire_pack_macro;
	static constexpr auto value = object("id", &value_type::Id, "place", &value_type::Place, "name", &value_type::Name, "template", &value_type::Template, "about", &value_type::About, "ask",
										 &value_type::Ask, "match", &value_type::Match, "when_unmet", &value_type::WhenUnmet, "quick_ask", &value_type::QuickAsk, "scopes", &value_type::Scopes,
										 "conditions", &value_type::Conditions, "groups", &value_type::Groups);
};
template <>
struct glz::meta<bpr::core::detail::wire_pack>
{
	using value_type            = bpr::core::detail::wire_pack;
	static constexpr auto value = object("id", &value_type::Id, "label", &value_type::Label, "version", &value_type::Version, "prompts", &value_type::Prompts);
};

namespace bpr::core {
	namespace {
		constexpr auto pack_read_options = glz::opts{ .error_on_unknown_keys = true };
		// Pretty, so a person or an agent can edit the pack in place.
		constexpr auto pack_write_options = glz::opts{ .prettify = true };

		[[nodiscard]] auto plain_line(const std::string_view aText) noexcept -> bool
		{
			return utf8::valid(aText) && std::ranges::none_of(aText, [](const char aByte) { return static_cast<unsigned char>(aByte) < 0x20U || aByte == 0x7F; });
		}

		[[nodiscard]] auto valid_label(const std::string_view aLabel) noexcept -> bool
		{
			return !aLabel.empty() && aLabel.size() <= maximum_prompt_pack_label_bytes && plain_line(aLabel);
		}

		[[nodiscard]] auto lines_name(const prompt_lines_value aValue) noexcept -> std::string_view
		{
			switch (aValue) {
			case prompt_lines_value::any:     return "any";
			case prompt_lines_value::one:     return "one";
			case prompt_lines_value::several: return "several";
			}
			return "any";
		}

		[[nodiscard]] auto activity_name(const prompt_activity_value aValue) noexcept -> std::string_view
		{
			switch (aValue) {
			case prompt_activity_value::replies_or_pushes: return "replies-or-pushes";
			case prompt_activity_value::replies:           return "replies";
			case prompt_activity_value::pushes:            return "pushes";
			}
			return "replies-or-pushes";
		}

		/// The value of `Enum` whose stable name is `aName`, among `aCount`.
		template <typename Enum, typename Name>
		[[nodiscard]] auto named(const std::string_view aName, const std::size_t aCount, Name aNameOf) -> std::optional<Enum>
		{
			for (std::size_t index = 0U; index < aCount; ++index) {
				if (aNameOf(static_cast<Enum>(index)) == aName) {
					return static_cast<Enum>(index);
				}
			}
			return std::nullopt;
		}

		template <typename Enum>
		[[nodiscard]] auto by_stable_name(const std::string_view aName, const std::size_t aCount) -> std::optional<Enum>
		{
			return named<Enum>(aName, aCount, [](const Enum aValue) { return stable_name(aValue); });
		}

		constexpr std::size_t subject_count    = 9U;
		constexpr std::size_t ask_count        = 2U;
		constexpr std::size_t match_count      = 3U;
		constexpr std::size_t when_unmet_count = 2U;

		[[nodiscard]] auto wire_condition_of(const prompt_condition& aCondition) -> detail::wire_pack_condition
		{
			auto wire = detail::wire_pack_condition{ .Fact = std::string{ stable_name(aCondition.Fact) }, .Negated = aCondition.Negated };
			switch (aCondition.Fact) {
			case prompt_fact::selected_lines:  wire.Lines = std::string{ lines_name(aCondition.Lines) }; break;
			case prompt_fact::path:
			case prompt_fact::repository:      wire.Patterns = aCondition.Patterns; break;
			case prompt_fact::thread_activity: wire.Activity = std::string{ activity_name(aCondition.Activity) }; break;
			default:                           break;
			}
			return wire;
		}

		/// Reads one condition; the unknown name, if any, goes to `aUnknown`.
		[[nodiscard]] auto condition_of(const detail::wire_pack_condition& aWire, std::string& aUnknown) -> prompt_condition
		{
			auto       condition = prompt_condition{ .Negated = aWire.Negated };
			const auto fact      = by_stable_name<prompt_fact>(aWire.Fact, prompt_fact_count);
			if (!fact) {
				aUnknown = aWire.Fact;
				return condition;
			}
			condition.Fact = *fact;
			if (aWire.Lines) {
				const auto lines = named<prompt_lines_value>(*aWire.Lines, 3U, lines_name);
				if (!lines) {
					aUnknown = *aWire.Lines;
				}
				condition.Lines = lines.value_or(prompt_lines_value::any);
			}
			if (aWire.Activity) {
				const auto activity = named<prompt_activity_value>(*aWire.Activity, 3U, activity_name);
				if (!activity) {
					aUnknown = *aWire.Activity;
				}
				condition.Activity = activity.value_or(prompt_activity_value::replies_or_pushes);
			}
			condition.Patterns = aWire.Patterns.value_or(std::vector<std::string>{ });
			return condition;
		}

		template <typename Wire>
		[[nodiscard]] auto wire_conditions_of(const std::vector<prompt_condition>& aConditions) -> Wire
		{
			auto conditions = Wire{ };
			for (const auto& condition : aConditions) {
				conditions.push_back(wire_condition_of(condition));
			}
			return conditions;
		}

		[[nodiscard]] auto wire_macro_of(const portable_prompt_macro& aMacro) -> std::optional<detail::wire_pack_macro>
		{
			const auto& body = aMacro.Body;
			auto        wire = detail::wire_pack_macro{ };
			wire.Id          = aMacro.Id;
			wire.Place       = std::string{ stable_name(aMacro.Place) };
			wire.Name        = body.Name;
			wire.Template    = body.Template;
			wire.About       = std::string{ stable_name(body.Subject) };
			wire.Ask         = std::string{ stable_name(body.Kind) };
			wire.Match       = std::string{ stable_name(body.Match) };
			wire.WhenUnmet   = std::string{ stable_name(body.WhenUnmet) };
			wire.QuickAsk    = body.QuickAsk;
			if (body.Scopes.any()) {
				wire.Scopes.emplace();
				for (std::size_t scope = 0U; scope < prompt_scope_count; ++scope) {
					if (body.Scopes.has(static_cast<prompt_scope>(scope))) {
						wire.Scopes->emplace_back(stable_name(static_cast<prompt_scope>(scope)));
					}
				}
			}
			if (!body.Conditions.empty()) {
				wire.Conditions = wire_conditions_of<std::vector<detail::wire_pack_condition>>(body.Conditions);
			}
			if (!body.Groups.empty()) {
				wire.Groups.emplace();
				for (const auto& group : body.Groups) {
					auto outer = detail::wire_pack_group{ .Match = std::string{ stable_name(group.Match) }, .Conditions = wire_conditions_of<std::vector<detail::wire_pack_condition>>(group.Conditions), .Groups = { } };
					if (!group.Groups.empty()) {
						outer.Groups.emplace();
						for (const auto& inner : group.Groups) {
							// Deeper than the editor allows: such a prompt does not
							// validate and is not exported.
							if (!inner.Groups.empty()) {
								return std::nullopt;
							}
							outer.Groups->push_back({ .Match = std::string{ stable_name(inner.Match) }, .Conditions = wire_conditions_of<std::vector<detail::wire_pack_condition>>(inner.Conditions) });
						}
					}
					wire.Groups->push_back(std::move(outer));
				}
			}
			return wire;
		}

		/// A prompt as the file has it, and the first name this version lacks.
		[[nodiscard]] auto decoded_macro_of(const detail::wire_pack_macro& aWire) -> decoded_prompt_macro
		{
			auto  item      = decoded_prompt_macro{ };
			auto& macro     = item.Macro;
			auto& body      = macro.Body;
			auto  unknown   = std::string{ };
			macro.Id        = aWire.Id;
			body.Name       = aWire.Name;
			body.Template   = aWire.Template;
			body.QuickAsk   = aWire.QuickAsk;
			const auto note = [&unknown](const bool aKnown, const std::string& aName) {
				if (!aKnown && unknown.empty()) {
					unknown = aName.empty() ? std::string{ "\"\"" } : aName;
				}
			};
			const auto place   = by_stable_name<prompt_place>(aWire.Place, prompt_place_count);
			const auto subject = by_stable_name<prompt_subject>(aWire.About, subject_count);
			const auto ask     = by_stable_name<prompt_ask>(aWire.Ask, ask_count);
			const auto match   = by_stable_name<prompt_match>(aWire.Match, match_count);
			const auto unmet   = by_stable_name<prompt_when_unmet>(aWire.WhenUnmet, when_unmet_count);
			note(place.has_value(), aWire.Place);
			note(subject.has_value(), aWire.About);
			note(ask.has_value(), aWire.Ask);
			note(match.has_value(), aWire.Match);
			note(unmet.has_value(), aWire.WhenUnmet);
			macro.Place    = place.value_or(prompt_place::agent_tab);
			body.Subject   = subject.value_or(prompt_subject::pull_request);
			body.Kind      = ask.value_or(prompt_ask::question);
			body.Match     = match.value_or(prompt_match::all);
			body.WhenUnmet = unmet.value_or(prompt_when_unmet::disable);
			for (const auto& name : aWire.Scopes.value_or(std::vector<std::string>{ })) {
				const auto scope = by_stable_name<prompt_scope>(name, prompt_scope_count);
				note(scope.has_value(), name);
				if (scope) {
					body.Scopes.set(*scope, true);
				}
			}
			const auto conditions = [&unknown](const std::vector<detail::wire_pack_condition>& aConditions) {
				auto result = std::vector<prompt_condition>{ };
				for (const auto& wire : aConditions) {
					auto missing = std::string{ };
					result.push_back(condition_of(wire, missing));
					if (!missing.empty() && unknown.empty()) {
						unknown = std::move(missing);
					}
				}
				return result;
			};
			body.Conditions = conditions(aWire.Conditions.value_or(std::vector<detail::wire_pack_condition>{ }));
			for (const auto& wire : aWire.Groups.value_or(std::vector<detail::wire_pack_group>{ })) {
				auto       group  = prompt_condition_group{ };
				const auto gmatch = by_stable_name<prompt_match>(wire.Match, match_count);
				note(gmatch.has_value(), wire.Match);
				group.Match      = gmatch.value_or(prompt_match::all);
				group.Conditions = conditions(wire.Conditions);
				for (const auto& innerWire : wire.Groups.value_or(std::vector<detail::wire_pack_inner_group>{ })) {
					auto       inner  = prompt_condition_group{ };
					const auto imatch = by_stable_name<prompt_match>(innerWire.Match, match_count);
					note(imatch.has_value(), innerWire.Match);
					inner.Match      = imatch.value_or(prompt_match::all);
					inner.Conditions = conditions(innerWire.Conditions);
					group.Groups.push_back(std::move(inner));
				}
				body.Groups.push_back(std::move(group));
			}
			if (!unknown.empty()) {
				item.Problem = prompt_pack_item_problem::unsupported;
				item.Detail  = std::move(unknown);
				return item;
			}
			if (!valid_prompt_uuid(macro.Id)) {
				item.Problem = prompt_pack_item_problem::invalid_id;
				return item;
			}
			const auto checked = prompt_macro{ .Id = macro.Id, .Place = macro.Place, .Body = body };
			for (const auto& problem : validate_prompt_macro(checked)) {
				if (!prompt_problem_blocks(problem.Problem)) {
					continue;
				}
				switch (problem.Problem) {
				// What a later version may allow names something this one lacks.
				case prompt_problem::unknown_variable:
				case prompt_problem::other_place_variable:
				case prompt_problem::fact_not_for_place:
				case prompt_problem::subject_not_for_place:
				case prompt_problem::kind_not_for_subject:
				case prompt_problem::quick_ask_not_for_subject:
					item.Problem = prompt_pack_item_problem::unsupported;
					item.Detail  = problem.Detail.empty() ? std::string{ stable_name(problem.Problem) } : problem.Detail;
					break;
				default: item.Problem = prompt_pack_item_problem::invalid; break;
				}
				break;
			}
			return item;
		}

		[[nodiscard]] auto own_macro(prompt_macro_differences& aStored, const std::string_view aId) -> prompt_macro*
		{
			const auto found = std::ranges::find(aStored.Own, aId, &prompt_macro::Id);
			return found == aStored.Own.end() ? nullptr : &*found;
		}

		[[nodiscard]] auto own_macro(const prompt_macro_differences& aStored, const std::string_view aId) -> const prompt_macro*
		{
			const auto found = std::ranges::find(aStored.Own, aId, &prompt_macro::Id);
			return found == aStored.Own.end() ? nullptr : &*found;
		}

		[[nodiscard]] auto linked_to(const prompt_macro& aMacro, const std::string_view aPack) noexcept -> bool { return aMacro.Pack && aMacro.Pack->Pack == aPack; }

		[[nodiscard]] auto link_of(const prompt_pack_identity& aPack, const prompt_place aPlace, const prompt_macro_body& aBody) -> prompt_pack_link
		{
			return prompt_pack_link{ .Pack = aPack.Id, .Label = aPack.Label, .Version = aPack.Version, .Place = aPlace, .Body = aBody };
		}

		void rename_in(std::vector<std::string>& aIds, const std::string_view aFrom, const std::string& aTo)
		{
			std::ranges::replace(aIds, std::string{ aFrom }, aTo);
		}

		/// Every reference the differences make to `aFrom` names `aTo` instead.
		void rename_prompt(prompt_macro_differences& aStored, const std::string& aFrom, const std::string& aTo)
		{
			for (auto& own : aStored.Own) {
				if (own.Id == aFrom) {
					own.Id = aTo;
				}
			}
			for (auto& order : aStored.Orders) {
				rename_in(order.Ids, aFrom, aTo);
			}
			for (auto& surface : aStored.Surfaces) {
				rename_in(surface.Ids, aFrom, aTo);
				rename_in(surface.Removed, aFrom, aTo);
			}
			for (auto& layer : aStored.Repositories) {
				for (auto& change : layer.Overrides) {
					if (change.Id == aFrom) {
						change.Id = aTo;
					}
				}
				for (auto& surface : layer.Surfaces) {
					rename_in(surface.Added, aFrom, aTo);
					rename_in(surface.Removed, aFrom, aTo);
					if (surface.Order) {
						rename_in(*surface.Order, aFrom, aTo);
					}
				}
			}
		}

		/// A removed prompt goes with its placements and its place in order.
		void remove_prompt(prompt_macro_differences& aStored, const std::string& aId)
		{
			std::erase_if(aStored.Own, [&aId](const prompt_macro& aMacro) { return aMacro.Id == aId; });
			for (auto& order : aStored.Orders) {
				std::erase(order.Ids, aId);
			}
			for (auto& surface : aStored.Surfaces) {
				std::erase(surface.Ids, aId);
				std::erase(surface.Removed, aId);
			}
			for (auto& layer : aStored.Repositories) {
				std::erase_if(layer.Overrides, [&aId](const prompt_override& aOverride) { return aOverride.Id == aId; });
				for (auto& surface : layer.Surfaces) {
					std::erase(surface.Added, aId);
					std::erase(surface.Removed, aId);
					if (surface.Order) {
						std::erase(*surface.Order, aId);
					}
				}
			}
		}
	} // namespace

	auto valid_prompt_uuid(const std::string_view aText) noexcept -> bool
	{
		if (aText.size() != 36U) {
			return false;
		}
		for (std::size_t index = 0U; index < aText.size(); ++index) {
			const auto byte = aText[index];
			if (index == 8U || index == 13U || index == 18U || index == 23U) {
				if (byte != '-') {
					return false;
				}
			} else if (!((byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'f'))) {
				return false;
			}
		}
		const auto variant = aText[19U];
		return aText[14U] == '4' && (variant == '8' || variant == '9' || variant == 'a' || variant == 'b');
	}

	auto stable_name(const prompt_pack_item_problem aProblem) noexcept -> std::string_view
	{
		switch (aProblem) {
		case prompt_pack_item_problem::unsupported: return "unsupported";
		case prompt_pack_item_problem::invalid_id:  return "invalid_id";
		case prompt_pack_item_problem::duplicate:   return "duplicate";
		case prompt_pack_item_problem::built_in_id: return "built_in_id";
		case prompt_pack_item_problem::invalid:     return "invalid";
		}
		return "invalid";
	}

	auto describe(const prompt_pack_item_problem aProblem) noexcept -> std::string_view
	{
		switch (aProblem) {
		case prompt_pack_item_problem::unsupported: return "Uses something this version of BetterPullReview does not have.";
		case prompt_pack_item_problem::invalid_id:  return "Its ID is not a UUID.";
		case prompt_pack_item_problem::duplicate:   return "Listed twice in the file; only the first is used.";
		case prompt_pack_item_problem::built_in_id: return "Has the ID of a built-in prompt.";
		case prompt_pack_item_problem::invalid:     return "Has a value the prompt editor refuses.";
		}
		return "Cannot be imported.";
	}

	auto stable_name(const prompt_pack_standing aStanding) noexcept -> std::string_view
	{
		switch (aStanding) {
		case prompt_pack_standing::added:     return "added";
		case prompt_pack_standing::updated:   return "updated";
		case prompt_pack_standing::conflict:  return "conflict";
		case prompt_pack_standing::unchanged: return "unchanged";
		case prompt_pack_standing::refused:   return "refused";
		}
		return "refused";
	}

	auto stable_name(const prompt_pack_apply_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case prompt_pack_apply_refusal::unusable_item:    return "unusable_item";
		case prompt_pack_apply_refusal::duplicate_choice: return "duplicate_choice";
		case prompt_pack_apply_refusal::unknown_drop:     return "unknown_drop";
		case prompt_pack_apply_refusal::library_full:     return "library_full";
		}
		return "unusable_item";
	}

	auto encode_prompt_pack(const prompt_pack_identity& aPack, const std::span<const portable_prompt_macro> aMacros) -> std::optional<std::string>
	{
		if (!valid_prompt_uuid(aPack.Id) || !valid_label(aPack.Label) || aPack.Version == 0U || aMacros.empty() || aMacros.size() > maximum_prompt_pack_macros) {
			return std::nullopt;
		}
		auto wire = detail::wire_pack{ .Id = aPack.Id, .Label = aPack.Label, .Version = aPack.Version, .Prompts = { } };
		for (const auto& macro : aMacros) {
			auto item = valid_prompt_uuid(macro.Id) ? wire_macro_of(macro) : std::nullopt;
			if (!item) {
				return std::nullopt;
			}
			wire.Prompts.push_back(std::move(*item));
		}
		auto bytes = std::string{ };
		if (glz::write<pack_write_options>(wire, bytes)) {
			return std::nullopt;
		}
		return bytes;
	}

	auto decode_prompt_pack(const std::string_view aJson) -> std::optional<decoded_prompt_pack>
	{
		auto wire = detail::wire_pack{ };
		if (glz::read<pack_read_options>(wire, aJson) || !valid_prompt_uuid(wire.Id) || !valid_label(wire.Label) || wire.Version == 0U || wire.Prompts.size() > maximum_prompt_pack_macros) {
			return std::nullopt;
		}
		auto pack = decoded_prompt_pack{ .Pack = { .Id = wire.Id, .Label = wire.Label, .Version = wire.Version }, .Macros = { } };
		auto ids  = std::set<std::string>{ };
		for (const auto& macro : wire.Prompts) {
			auto item = decoded_macro_of(macro);
			if (!ids.insert(macro.Id).second && !item.Problem) {
				item.Problem = prompt_pack_item_problem::duplicate;
			}
			pack.Macros.push_back(std::move(item));
		}
		return pack;
	}

	auto prompt_pack_edited(const prompt_macro& aMacro) noexcept -> bool { return aMacro.Pack && (aMacro.Place != aMacro.Pack->Place || aMacro.Body != aMacro.Pack->Body); }

	auto prompt_packs_here(const prompt_macro_differences& aStored) -> std::vector<prompt_pack_here>
	{
		auto packs = std::vector<prompt_pack_here>{ };
		for (const auto& own : aStored.Own) {
			if (!own.Pack) {
				continue;
			}
			auto found = std::ranges::find_if(packs, [&own](const prompt_pack_here& aHere) { return aHere.Pack.Id == own.Pack->Pack; });
			if (found == packs.end()) {
				packs.push_back({ .Pack = { .Id = own.Pack->Pack, .Label = own.Pack->Label, .Version = own.Pack->Version }, .Members = { } });
				found = std::prev(packs.end());
			} else if (own.Pack->Version > found->Pack.Version) {
				found->Pack.Version = own.Pack->Version;
				found->Pack.Label   = own.Pack->Label;
			}
			found->Members.push_back(own.Id);
		}
		std::ranges::sort(packs, [](const prompt_pack_here& aLeft, const prompt_pack_here& aRight) {
			return std::tie(aLeft.Pack.Label, aLeft.Pack.Id) < std::tie(aRight.Pack.Label, aRight.Pack.Id);
		});
		return packs;
	}

	auto next_prompt_pack(const prompt_pack_here& aHere, std::string aLabel) -> prompt_pack_identity
	{
		return prompt_pack_identity{ .Id = aHere.Pack.Id, .Label = aLabel.empty() ? aHere.Pack.Label : std::move(aLabel), .Version = aHere.Pack.Version + 1U };
	}

	auto export_prompt_pack(const prompt_macro_differences& aStored, const prompt_pack_identity& aPack, const std::span<const std::string> aChosen, const std::span<const std::string> aFresh)
		-> std::optional<prompt_pack_export>
	{
		if (!valid_prompt_uuid(aPack.Id) || !valid_label(aPack.Label) || aPack.Version == 0U || aChosen.empty() || aChosen.size() > maximum_prompt_pack_macros) {
			return std::nullopt;
		}
		for (const auto& id : aChosen) {
			const auto* const own = own_macro(aStored, id);
			if (own == nullptr || own->BuiltIn || own->Action != prompt_action::none || std::ranges::count(aChosen, id) != 1) {
				return std::nullopt;
			}
		}
		auto result  = prompt_pack_export{ .Pack = aPack, .Macros = { }, .After = aStored };
		auto fresh   = aFresh.begin();
		auto renamed = std::vector<std::pair<std::string, std::string>>{ };
		for (const auto& own : aStored.Own) {
			if (!std::ranges::contains(aChosen, own.Id) || valid_prompt_uuid(own.Id)) {
				continue;
			}
			// A UUID already in use here, or handed out twice, is no new identity.
			while (fresh != aFresh.end() && (!valid_prompt_uuid(*fresh) || own_macro(result.After, *fresh) != nullptr)) {
				++fresh;
			}
			if (fresh == aFresh.end()) {
				return std::nullopt;
			}
			rename_prompt(result.After, own.Id, *fresh);
			renamed.emplace_back(own.Id, *fresh);
			++fresh;
		}
		for (auto& own : result.After.Own) {
			const auto before = std::ranges::find(renamed, own.Id, &std::pair<std::string, std::string>::second);
			const auto id     = before == renamed.end() ? own.Id : before->first;
			if (!std::ranges::contains(aChosen, id)) {
				continue;
			}
			own.Pack = link_of(aPack, own.Place, own.Body);
			result.Macros.push_back({ .Id = own.Id, .Place = own.Place, .Body = own.Body });
		}
		if (!encode_prompt_pack(result.Pack, result.Macros)) {
			return std::nullopt;
		}
		return result;
	}

	auto plan_prompt_pack_import(const decoded_prompt_pack& aFile, const prompt_macro_differences& aStored, const std::span<const prompt_builtin> aShipped) -> prompt_pack_import_plan
	{
		auto plan = prompt_pack_import_plan{ };
		for (const auto& own : aStored.Own) {
			if (linked_to(own, aFile.Pack.Id)) {
				plan.Imported = std::max(plan.Imported.value_or(0U), own.Pack->Version);
			}
		}
		plan.Older = plan.Imported && aFile.Pack.Version < *plan.Imported;
		for (const auto& item : aFile.Macros) {
			auto& entry = plan.Items.emplace_back();
			if (item.Problem) {
				entry.Problem = item.Problem;
				continue;
			}
			if (std::ranges::any_of(aShipped, [&item](const prompt_builtin& aBuiltin) { return aBuiltin.Macro.Id == item.Macro.Id; })) {
				entry.Problem = prompt_pack_item_problem::built_in_id;
				continue;
			}
			const auto* const here = own_macro(aStored, item.Macro.Id);
			if (here == nullptr) {
				entry.Standing = prompt_pack_standing::added;
				entry.Default  = true;
				continue;
			}
			entry.Here = *here;
			if (here->Place == item.Macro.Place && here->Body == item.Macro.Body) {
				entry.Standing = prompt_pack_standing::unchanged;
			} else if (linked_to(*here, aFile.Pack.Id) && !prompt_pack_edited(*here)) {
				entry.Standing = prompt_pack_standing::updated;
				entry.Default  = !plan.Older;
			} else {
				entry.Standing = prompt_pack_standing::conflict;
			}
		}
		for (const auto& own : aStored.Own) {
			if (!linked_to(own, aFile.Pack.Id) || std::ranges::any_of(aFile.Macros, [&own](const decoded_prompt_macro& aItem) { return aItem.Macro.Id == own.Id; })) {
				continue;
			}
			const auto edited = prompt_pack_edited(own);
			plan.Dropped.push_back({ .Here = own, .Edited = edited, .Default = !edited && !plan.Older });
		}
		return plan;
	}

	auto default_prompt_pack_choices(const prompt_pack_import_plan& aPlan) -> prompt_pack_choices
	{
		auto choices = prompt_pack_choices{ };
		for (std::size_t index = 0U; index < aPlan.Items.size(); ++index) {
			if (aPlan.Items[index].Default) {
				choices.Take.push_back(index);
			}
		}
		for (const auto& drop : aPlan.Dropped) {
			if (drop.Default) {
				choices.Remove.push_back(drop.Here.Id);
			}
		}
		return choices;
	}

	auto apply_prompt_pack_import(const prompt_macro_differences& aStored, const std::span<const prompt_builtin> aShipped, const decoded_prompt_pack& aFile, const prompt_pack_choices& aChoices)
		-> std::expected<prompt_macro_differences, prompt_pack_apply_refusal>
	{
		const auto plan  = plan_prompt_pack_import(aFile, aStored, aShipped);
		auto       taken = std::set<std::size_t>{ };
		for (const auto index : aChoices.Take) {
			if (index >= plan.Items.size()) {
				return std::unexpected(prompt_pack_apply_refusal::unusable_item);
			}
			const auto standing = plan.Items[index].Standing;
			if (plan.Items[index].Problem || standing == prompt_pack_standing::unchanged || standing == prompt_pack_standing::refused) {
				return std::unexpected(prompt_pack_apply_refusal::unusable_item);
			}
			if (!taken.insert(index).second) {
				return std::unexpected(prompt_pack_apply_refusal::duplicate_choice);
			}
		}
		auto removed = std::set<std::string>{ };
		for (const auto& id : aChoices.Remove) {
			if (std::ranges::none_of(plan.Dropped, [&id](const prompt_pack_drop_plan& aDrop) { return aDrop.Here.Id == id; })) {
				return std::unexpected(prompt_pack_apply_refusal::unknown_drop);
			}
			if (!removed.insert(id).second) {
				return std::unexpected(prompt_pack_apply_refusal::duplicate_choice);
			}
		}
		auto after = aStored;
		for (std::size_t index = 0U; index < plan.Items.size(); ++index) {
			const auto& item = aFile.Macros[index].Macro;
			const auto  link = link_of(aFile.Pack, item.Place, item.Body);
			if (taken.contains(index)) {
				if (auto* const here = own_macro(after, item.Id); here != nullptr) {
					here->Place = item.Place;
					here->Body  = item.Body;
					here->Pack  = link;
				} else {
					after.Own.push_back(prompt_macro{ .Id = item.Id, .Place = item.Place, .Body = item.Body, .On = true, .Pack = link });
				}
			} else if (plan.Items[index].Standing == prompt_pack_standing::unchanged) {
				// Equal to the file already: it now stands at the file's version.
				if (auto* const here = own_macro(after, item.Id); here != nullptr && linked_to(*here, aFile.Pack.Id) && here->Pack->Version < aFile.Pack.Version) {
					here->Pack = link;
				}
			}
		}
		for (const auto& drop : plan.Dropped) {
			if (removed.contains(drop.Here.Id)) {
				remove_prompt(after, drop.Here.Id);
			} else if (auto* const here = own_macro(after, drop.Here.Id); here != nullptr) {
				here->Pack.reset();
			}
		}
		if (aShipped.size() + after.Own.size() > maximum_prompt_macros) {
			return std::unexpected(prompt_pack_apply_refusal::library_full);
		}
		return after;
	}
} // namespace bpr::core
