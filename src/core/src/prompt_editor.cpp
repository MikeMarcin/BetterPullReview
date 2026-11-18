#include "bpr/core/prompt_editor.hpp"

#include <algorithm>
#include <array>
#include <iterator>
#include <utility>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto list(prompt_surfaces& aSurfaces, const prompt_surface aSurface) -> std::vector<std::string>& { return aSurfaces.Lists.at(static_cast<std::size_t>(aSurface)); }

		[[nodiscard]] auto list(const prompt_surfaces& aSurfaces, const prompt_surface aSurface) -> const std::vector<std::string>&
		{
			return aSurfaces.Lists.at(static_cast<std::size_t>(aSurface));
		}

		[[nodiscard]] auto contains(const std::vector<std::string>& aIds, const std::string& aId) -> bool { return std::ranges::find(aIds, aId) != aIds.end(); }

		[[nodiscard]] auto find_macro(std::vector<prompt_macro>& aLibrary, const std::string& aId) -> prompt_macro*
		{
			const auto found = std::ranges::find(aLibrary, aId, &prompt_macro::Id);
			return found == aLibrary.end() ? nullptr : &*found;
		}

		[[nodiscard]] auto find_macro(const std::vector<prompt_macro>& aLibrary, const std::string& aId) -> const prompt_macro*
		{
			const auto found = std::ranges::find(aLibrary, aId, &prompt_macro::Id);
			return found == aLibrary.end() ? nullptr : &*found;
		}

		[[nodiscard]] auto find_layer(const prompt_editor& aEditor) -> const prompt_repository_layer*
		{
			if (aEditor.Repository.empty()) {
				return nullptr;
			}
			const auto found = std::ranges::find(aEditor.Document.Layers, aEditor.Repository, &prompt_repository_layer::Repository);
			return found == aEditor.Document.Layers.end() ? nullptr : &*found;
		}

		[[nodiscard]] auto shipped_macro(const prompt_editor& aEditor, const std::string& aId) -> const prompt_macro*
		{
			const auto found = std::ranges::find_if(aEditor.Shipped, [&aId](const prompt_builtin& aBuiltin) { return aBuiltin.Macro.Id == aId; });
			return found == aEditor.Shipped.end() ? nullptr : &found->Macro;
		}

		[[nodiscard]] auto refused(const prompt_edit_refusal aRefusal, const placement_refusal aPlacement = placement_refusal::none) -> prompt_edit_outcome
		{
			return { .Refusal = aRefusal, .Placement = aPlacement };
		}

		/// The shown layer, edited, back into the document: the app-wide
		/// library and surfaces, or the repository's layer remade from them.
		void write_back(prompt_editor& aEditor, prompt_layered aView)
		{
			auto& document = aEditor.Document;
			if (aEditor.Repository.empty()) {
				document.Library  = std::move(aView.Library);
				document.Surfaces = std::move(aView.Surfaces);
				return;
			}
			auto layer = repository_layer_of(document.Library, document.Surfaces, aView, aEditor.Repository);
			std::erase_if(document.Layers, [&aEditor](const prompt_repository_layer& aLayer) { return aLayer.Repository == aEditor.Repository; });
			if (layer != prompt_repository_layer{ .Repository = aEditor.Repository }) {
				document.Layers.push_back(std::move(layer));
				std::ranges::sort(document.Layers, { }, &prompt_repository_layer::Repository);
			}
		}

		/// Keeps the step an edit made: a run of typing in one prompt is one.
		void record(prompt_editor& aEditor, prompt_edit_document aBefore, const std::optional<std::string>& aPrompt, const bool aTyping)
		{
			if (!(aTyping && aEditor.Typing && aEditor.Typing == aPrompt)) {
				aEditor.Undo.push_back({ .Document = std::move(aBefore), .Prompt = aPrompt });
				if (aEditor.Undo.size() > maximum_prompt_undo_steps) {
					aEditor.Undo.erase(aEditor.Undo.begin());
				}
			}
			aEditor.Typing = aTyping ? aPrompt : std::nullopt;
			aEditor.Redo.clear();
			aEditor.Pending = true;
			aEditor.Held    = false;
		}

		/// Applies `aMutate` to the shown layer. A refusal changes nothing; a
		/// change is written back and kept as an undo step.
		template <typename TMutate>
		auto commit(prompt_editor& aEditor, const std::optional<std::string>& aPrompt, const bool aTyping, TMutate&& aMutate) -> prompt_edit_outcome
		{
			auto view    = prompt_edit_view(aEditor);
			auto outcome = std::forward<TMutate>(aMutate)(view);
			if (outcome.Refusal != prompt_edit_refusal::none) {
				return outcome;
			}
			outcome.Prompt = aPrompt;
			auto before    = aEditor.Document;
			write_back(aEditor, std::move(view));
			if (aEditor.Document == before) {
				return outcome;
			}
			record(aEditor, std::move(before), aPrompt, aTyping);
			outcome.Changed = true;
			return outcome;
		}

		/// Applies `aMutate` to one prompt of the shown layer.
		template <typename TMutate>
		auto commit_prompt(prompt_editor& aEditor, const std::string& aId, const bool aTyping, TMutate&& aMutate) -> prompt_edit_outcome
		{
			return commit(aEditor, aId, aTyping, [&aId, &aMutate](prompt_layered& aView) -> prompt_edit_outcome {
				auto* const macro = find_macro(aView.Library, aId);
				if (macro == nullptr) {
					return refused(prompt_edit_refusal::unknown_prompt);
				}
				return aMutate(*macro);
			});
		}

		[[nodiscard]] auto library_full(const prompt_editor& aEditor, const prompt_layered& aView) -> bool
		{
			if (aEditor.Repository.empty()) {
				return aView.Library.size() >= maximum_prompt_macros;
			}
			// A repository's own prompts are bounded as the app-wide library
			// is, and the two together as `apply_repository_layer` reads them.
			const auto own = aView.Library.size() - std::min(aView.Library.size(), aEditor.Document.Library.size());
			return own >= maximum_prompt_macros || aView.Library.size() >= maximum_prompt_macros * 2U;
		}

		[[nodiscard]] auto identity_taken(const prompt_editor& aEditor, const prompt_layered& aView, const std::string& aId) -> bool
		{
			if (aId.empty() || find_macro(aView.Library, aId) != nullptr || find_macro(aEditor.Document.Library, aId) != nullptr) {
				return true;
			}
			return std::ranges::any_of(aEditor.Document.Layers, [&aId](const prompt_repository_layer& aLayer) { return std::ranges::find(aLayer.Own, aId, &prompt_macro::Id) != aLayer.Own.end(); });
		}

		/// `aText` cut to at most `aBytes`, never inside a UTF-8 character.
		[[nodiscard]] auto cut(std::string aText, const std::size_t aBytes) -> std::string
		{
			if (aText.size() <= aBytes) {
				return aText;
			}
			auto end = aBytes;
			while (end > 0U && (static_cast<unsigned char>(aText[end]) & 0xC0U) == 0x80U) {
				--end;
			}
			aText.resize(end);
			return aText;
		}

		/// One group of a prompt's tree, the list itself included.
		struct group_ref
		{
			prompt_match*                        Match      = nullptr;
			std::vector<prompt_condition>*       Conditions = nullptr;
			std::vector<prompt_condition_group>* Groups     = nullptr;
		};

		[[nodiscard]] auto group_at(prompt_macro_body& aBody, const prompt_condition_path& aPath) -> std::optional<group_ref>
		{
			auto ref = group_ref{ .Match = &aBody.Match, .Conditions = &aBody.Conditions, .Groups = &aBody.Groups };
			for (const auto at : aPath) {
				if (at >= ref.Groups->size()) {
					return std::nullopt;
				}
				auto& group = (*ref.Groups)[at];
				ref         = { .Match = &group.Match, .Conditions = &group.Conditions, .Groups = &group.Groups };
			}
			return ref;
		}

		[[nodiscard]] auto leaf_count(const std::vector<prompt_condition>& aConditions, const std::vector<prompt_condition_group>& aGroups) -> std::size_t
		{
			auto count = aConditions.size();
			for (const auto& group : aGroups) {
				count += leaf_count(group.Conditions, group.Groups);
			}
			return count;
		}

		void copy_field(prompt_macro& aHere, const prompt_macro& aApp, const prompt_field aField)
		{
			switch (aField) {
			case prompt_field::name:   aHere.Body.Name = aApp.Body.Name; break;
			case prompt_field::text:   aHere.Body.Template = aApp.Body.Template; break;
			case prompt_field::answer: aHere.Body.Kind = aApp.Body.Kind; break;
			case prompt_field::conditions:
				aHere.Body.Conditions = aApp.Body.Conditions;
				aHere.Body.Match      = aApp.Body.Match;
				aHere.Body.Groups     = aApp.Body.Groups;
				aHere.Body.WhenUnmet  = aApp.Body.WhenUnmet;
				break;
			case prompt_field::on: aHere.On = aApp.On; break;
			}
		}

		constexpr auto field_names = std::array<std::string_view, 5U>{ "name", "text", "answer", "conditions", "on" };
	} // namespace

	auto prompt_edit_document_of(const prompt_macro_set& aSet) -> prompt_edit_document
	{
		return { .Library = aSet.macros(), .Surfaces = aSet.Surfaces, .Layers = aSet.Repositories };
	}

	auto prompt_macro_differences_of(const std::span<const prompt_builtin> aShipped, const prompt_edit_document& aDocument) -> prompt_macro_differences
	{
		auto differences = prompt_macro_differences{ };
		for (const auto& macro : aDocument.Library) {
			if (!macro.BuiltIn) {
				differences.Own.push_back(macro);
				continue;
			}
			const auto shipped = std::ranges::find_if(aShipped, [&macro](const prompt_builtin& aBuiltin) { return aBuiltin.Macro.Id == macro.Id; });
			if (shipped == aShipped.end()) {
				continue;
			}
			auto change = prompt_builtin_change{ .Id = macro.Id };
			if (macro.Body != shipped->Macro.Body) {
				change.Body           = macro.Body;
				change.WrittenAgainst = shipped->Revision;
			}
			if (macro.On != shipped->Macro.On) {
				change.On = macro.On;
			}
			if (change.Body || change.On) {
				differences.Changes.push_back(std::move(change));
			}
		}
		// A place's order is kept only where it is not what the merge makes
		// without one.
		auto unorderedDifferences    = prompt_macro_differences{ };
		unorderedDifferences.Changes = differences.Changes;
		unorderedDifferences.Own     = differences.Own;
		const auto unordered         = merge_prompt_macros(aShipped, unorderedDifferences).macros();
		const auto ids               = [](const std::vector<prompt_macro>& aMacros, const prompt_place aPlace) {
			auto result = std::vector<std::string>{ };
			for (const auto& macro : aMacros) {
				if (macro.Place == aPlace) {
					result.push_back(macro.Id);
				}
			}
			return result;
		};
		for (auto place = std::size_t{ 0 }; place < prompt_place_count; ++place) {
			const auto here = static_cast<prompt_place>(place);
			if (auto edited = ids(aDocument.Library, here); edited != ids(unordered, here)) {
				auto order  = prompt_place_order{ };
				order.Place = here;
				order.Ids   = std::move(edited);
				differences.Orders.push_back(std::move(order));
			}
		}
		differences.Surfaces     = prompt_surface_changes_of(aDocument.Library, aDocument.Surfaces);
		differences.Repositories = aDocument.Layers;
		return differences;
	}

	auto make_prompt_editor(std::vector<prompt_builtin> aShipped) -> prompt_editor
	{
		auto editor     = prompt_editor{ };
		editor.Document = prompt_edit_document_of(merge_prompt_macros(aShipped, { }));
		editor.Shipped  = std::move(aShipped);
		return editor;
	}

	auto prompt_edit_view(const prompt_editor& aEditor) -> prompt_layered
	{
		const auto* const layer = find_layer(aEditor);
		if (layer == nullptr) {
			return { .Library = aEditor.Document.Library, .Surfaces = aEditor.Document.Surfaces };
		}
		return apply_repository_layer(aEditor.Document.Library, aEditor.Document.Surfaces, *layer);
	}

	auto adopt_prompt_projection(prompt_editor& aEditor, const prompt_macro_set& aSet, const std::uint64_t aRevision, const bool aReadable, const bool aLoaded) -> bool
	{
		aEditor.Loaded   = aLoaded;
		aEditor.Readable = aReadable;
		if (!aReadable && aLoaded && aEditor.Notice == prompt_edit_notice::none) {
			aEditor.Notice = prompt_edit_notice::unreadable;
		} else if (aReadable && aEditor.Notice == prompt_edit_notice::unreadable) {
			aEditor.Notice = prompt_edit_notice::none;
		}
		// Review N1: a write in flight settles first, and waiting edits are
		// not replaced; they are written from the revision they came from,
		// and the store refuses them as stale if another window wrote since.
		if (aEditor.Writing || (aEditor.Pending && !aEditor.Superseded)) {
			return false;
		}
		if (aEditor.Superseded) {
			aEditor.Superseded = false;
			aEditor.Pending    = false;
			aEditor.Held       = false;
		}
		if (aReadable) {
			aEditor.Base = aRevision;
		}
		auto adopted = prompt_edit_document_of(aSet);
		if (adopted == aEditor.Document) {
			return false;
		}
		// Another document: the steps kept were made over the one replaced.
		aEditor.Document = std::move(adopted);
		aEditor.Undo.clear();
		aEditor.Redo.clear();
		aEditor.Typing.reset();
		return true;
	}

	auto next_prompt_save(prompt_editor& aEditor) -> std::optional<prompt_save_request>
	{
		if (!aEditor.Pending || aEditor.Held || aEditor.Writing || !aEditor.Loaded) {
			return std::nullopt;
		}
		aEditor.Writing     = aEditor.Base;
		aEditor.Pending     = false;
		auto request        = prompt_save_request{ };
		request.Differences = prompt_macro_differences_of(aEditor.Shipped, aEditor.Document);
		request.From        = aEditor.Base;
		return request;
	}

	void settle_prompt_save(prompt_editor& aEditor, const prompt_save_outcome aOutcome)
	{
		if (!aEditor.Writing) {
			return;
		}
		const auto from = *aEditor.Writing;
		aEditor.Writing.reset();
		switch (aOutcome) {
		case prompt_save_outcome::stored:
			// The store bumps its revision by one on every write.
			aEditor.Base = from + 1U;
			if (aEditor.Notice == prompt_edit_notice::save_failed || aEditor.Notice == prompt_edit_notice::save_stale) {
				aEditor.Notice = prompt_edit_notice::none;
			}
			break;
		case prompt_save_outcome::failed:
			// Nothing was written: the edits are kept and go with the next.
			aEditor.Pending = true;
			aEditor.Held    = true;
			aEditor.Notice  = prompt_edit_notice::save_failed;
			break;
		case prompt_save_outcome::stale:
			aEditor.Superseded = true;
			aEditor.Notice     = prompt_edit_notice::save_stale;
			break;
		}
	}

	auto prompt_edits_saving(const prompt_editor& aEditor) noexcept -> bool
	{
		return aEditor.Writing.has_value() || (aEditor.Pending && !aEditor.Held);
	}

	auto place_prompt(prompt_editor& aEditor, const std::string& aId, const prompt_surface aSurface, const std::optional<std::size_t> aAt) -> prompt_edit_outcome
	{
		return commit(aEditor, aId, false, [&](prompt_layered& aView) {
			const auto placement = place(aView.Surfaces, aView.Library, aId, aSurface, aAt);
			return placement == placement_refusal::none ? prompt_edit_outcome{ } : refused(prompt_edit_refusal::placement, placement);
		});
	}

	auto drop_prompt(prompt_editor& aEditor, const std::string& aId, const std::optional<prompt_surface> aFrom, const prompt_surface aTo, std::size_t aAt, const bool aCopy) -> prompt_edit_outcome
	{
		return commit(aEditor, aId, false, [&](prompt_layered& aView) {
			if (aFrom && *aFrom == aTo) {
				// Within a surface: the drop line counts the row being moved.
				const auto& placed = list(aView.Surfaces, aTo);
				if (const auto found = std::ranges::find(placed, aId); found != placed.end() && static_cast<std::size_t>(found - placed.begin()) < aAt) {
					--aAt;
				}
			}
			const auto placement = place(aView.Surfaces, aView.Library, aId, aTo, aAt);
			if (placement != placement_refusal::none) {
				return refused(prompt_edit_refusal::placement, placement);
			}
			if (aFrom && *aFrom != aTo && !aCopy) {
				static_cast<void>(remove_placement(aView.Surfaces, aId, *aFrom));
			}
			return prompt_edit_outcome{ };
		});
	}

	auto unplace_prompt(prompt_editor& aEditor, const std::string& aId, const prompt_surface aSurface) -> prompt_edit_outcome
	{
		return commit(aEditor, aId, false, [&](prompt_layered& aView) {
			const auto placement = remove_placement(aView.Surfaces, aId, aSurface);
			return placement == placement_refusal::none ? prompt_edit_outcome{ } : refused(prompt_edit_refusal::placement, placement);
		});
	}

	auto move_prompt(prompt_editor& aEditor, const std::string& aId, const prompt_surface aSurface, const int aBy) -> prompt_edit_outcome
	{
		return commit(aEditor, aId, false, [&](prompt_layered& aView) {
			const auto placement = move_placement(aView.Surfaces, aId, aSurface, aBy);
			return placement == placement_refusal::none ? prompt_edit_outcome{ } : refused(prompt_edit_refusal::placement, placement);
		});
	}

	auto set_prompt_on(prompt_editor& aEditor, const std::string& aId, const bool aOn) -> prompt_edit_outcome
	{
		return commit_prompt(aEditor, aId, false, [aOn](prompt_macro& aMacro) {
			aMacro.On = aOn;
			return prompt_edit_outcome{ };
		});
	}

	auto add_prompt(prompt_editor& aEditor, std::string aId, std::string aName) -> prompt_edit_outcome
	{
		const auto id = aId;
		return commit(aEditor, id, false, [&](prompt_layered& aView) {
			if (identity_taken(aEditor, aView, aId)) {
				return refused(prompt_edit_refusal::identity_taken);
			}
			if (library_full(aEditor, aView)) {
				return refused(prompt_edit_refusal::library_full);
			}
			auto macro      = prompt_macro{ };
			macro.Id        = std::move(aId);
			macro.Place     = prompt_place::agent_tab;
			macro.Body.Name = cut(std::move(aName), maximum_prompt_name_bytes);
			aView.Library.push_back(std::move(macro));
			return prompt_edit_outcome{ };
		});
	}

	auto duplicate_prompt(prompt_editor& aEditor, const std::string& aFrom, std::string aId, std::string aName) -> prompt_edit_outcome
	{
		const auto id = aId;
		return commit(aEditor, id, false, [&](prompt_layered& aView) {
			const auto* const original = find_macro(aView.Library, aFrom);
			if (original == nullptr) {
				return refused(prompt_edit_refusal::unknown_prompt);
			}
			if (identity_taken(aEditor, aView, aId)) {
				return refused(prompt_edit_refusal::identity_taken);
			}
			if (library_full(aEditor, aView)) {
				return refused(prompt_edit_refusal::library_full);
			}
			auto copy    = *original;
			copy.Id      = std::move(aId);
			copy.BuiltIn = false;
			copy.Action  = prompt_action::none;
			copy.Surface = std::nullopt;
			// RFC-BPR-0125 AC24. A copy is the reviewer's own: a new identity
			// and no pack, so a later version of the pack never updates it.
			copy.Pack.reset();
			copy.Body.Name = cut(std::move(aName), maximum_prompt_name_bytes);
			aView.Library.push_back(std::move(copy));
			return prompt_edit_outcome{ };
		});
	}

	auto reset_prompt(prompt_editor& aEditor, const std::string& aId) -> prompt_edit_outcome
	{
		const auto* const original = shipped_macro(aEditor, aId);
		if (original == nullptr) {
			return refused(prompt_edit_refusal::unknown_prompt);
		}
		const auto body = original->Body;
		return commit_prompt(aEditor, aId, false, [&body](prompt_macro& aMacro) {
			aMacro.Body = body;
			return prompt_edit_outcome{ };
		});
	}

	auto delete_prompt(prompt_editor& aEditor, const std::string& aId) -> prompt_edit_outcome
	{
		return commit(aEditor, aId, false, [&](prompt_layered& aView) {
			const auto* const macro = find_macro(aView.Library, aId);
			if (macro == nullptr) {
				return refused(prompt_edit_refusal::unknown_prompt);
			}
			if (macro->BuiltIn) {
				return refused(prompt_edit_refusal::built_in);
			}
			// In a repository only its own can be deleted; an app-wide
			// prompt is turned off there.
			if (!aEditor.Repository.empty() && find_macro(aEditor.Document.Library, aId) != nullptr) {
				return refused(prompt_edit_refusal::inherited);
			}
			for (auto& placed : aView.Surfaces.Lists) {
				std::erase(placed, aId);
			}
			std::erase_if(aView.Library, [&aId](const prompt_macro& aMacro) { return aMacro.Id == aId; });
			return prompt_edit_outcome{ };
		});
	}

	auto restore_built_in_prompts(prompt_editor& aEditor) -> prompt_edit_outcome
	{
		auto outcome = commit(aEditor, std::nullopt, false, [&](prompt_layered& aView) {
			auto builtIns = std::vector<prompt_macro>{ };
			for (const auto& builtin : aEditor.Shipped) {
				builtIns.push_back(builtin.Macro);
				if (auto* const macro = find_macro(aView.Library, builtin.Macro.Id); macro != nullptr) {
					macro->Body = builtin.Macro.Body;
					macro->On   = builtin.Macro.On;
				}
			}
			auto restored = shipped_surfaces(builtIns);
			for (std::size_t surface = 0U; surface < prompt_surface_count; ++surface) {
				for (const auto& id : aView.Surfaces.Lists.at(surface)) {
					if (const auto* const macro = find_macro(aView.Library, id); macro != nullptr && !macro->BuiltIn) {
						restored.Lists.at(surface).push_back(id);
					}
				}
			}
			aView.Surfaces = std::move(restored);
			return prompt_edit_outcome{ };
		});
		return outcome;
	}

	auto edit_prompt(prompt_editor& aEditor, const prompt_macro& aEdited, const bool aTyping) -> prompt_edit_outcome
	{
		return commit_prompt(aEditor, aEdited.Id, aTyping, [&aEdited](prompt_macro& aMacro) {
			aMacro.Body = aEdited.Body;
			aMacro.On   = aEdited.On;
			return prompt_edit_outcome{ };
		});
	}

	void end_prompt_typing(prompt_editor& aEditor) noexcept { aEditor.Typing.reset(); }

	auto prompt_condition_room_of(const prompt_macro& aMacro, const prompt_condition_path& aPath) -> prompt_condition_room
	{
		auto       room  = prompt_condition_room{ };
		auto       body  = aMacro.Body;
		const auto group = group_at(body, aPath);
		if (!group) {
			return room;
		}
		room.Exists    = true;
		room.Full      = leaf_count(body.Conditions, body.Groups) >= maximum_prompt_condition_leaves;
		room.Crowded   = group->Conditions->size() >= maximum_prompt_conditions;
		room.Condition = !room.Full && !room.Crowded;
		// The list, then two levels of groups below it.
		room.Group = aPath.size() + 1U < maximum_prompt_condition_depth && group->Groups->size() < maximum_prompt_conditions;
		for (std::size_t fact = 0U; fact < prompt_fact_count; ++fact) {
			if (prompt_fact_allowed(aMacro.Place, static_cast<prompt_fact>(fact))) {
				room.Facts.push_back(static_cast<prompt_fact>(fact));
			}
		}
		return room;
	}

	auto add_prompt_condition(prompt_editor& aEditor, const std::string& aId, const prompt_condition_path& aPath, const prompt_fact aFact) -> prompt_edit_outcome
	{
		return commit_prompt(aEditor, aId, false, [&aPath, aFact](prompt_macro& aMacro) {
			const auto room = prompt_condition_room_of(aMacro, aPath);
			if (!room.Condition) {
				return refused(prompt_edit_refusal::conditions_full);
			}
			if (std::ranges::find(room.Facts, aFact) == room.Facts.end()) {
				return refused(prompt_edit_refusal::fact_not_allowed);
			}
			auto condition = prompt_condition{ .Fact = aFact };
			if (aFact == prompt_fact::path || aFact == prompt_fact::repository) {
				condition.Patterns = { aFact == prompt_fact::path ? "src/**" : "owner/*" };
			}
			group_at(aMacro.Body, aPath)->Conditions->push_back(std::move(condition));
			return prompt_edit_outcome{ };
		});
	}

	auto add_prompt_condition_group(prompt_editor& aEditor, const std::string& aId, const prompt_condition_path& aPath) -> prompt_edit_outcome
	{
		return commit_prompt(aEditor, aId, false, [&aPath](prompt_macro& aMacro) {
			if (!prompt_condition_room_of(aMacro, aPath).Group) {
				return refused(prompt_edit_refusal::conditions_full);
			}
			// A new group is All of these, changed in its own box.
			group_at(aMacro.Body, aPath)->Groups->push_back({ .Match = prompt_match::all });
			return prompt_edit_outcome{ };
		});
	}

	auto remove_prompt_condition(prompt_editor& aEditor, const std::string& aId, const prompt_condition_path& aPath, const std::size_t aAt) -> prompt_edit_outcome
	{
		return commit_prompt(aEditor, aId, false, [&aPath, aAt](prompt_macro& aMacro) {
			const auto from = group_at(aMacro.Body, aPath);
			if (!from || aAt >= from->Conditions->size()) {
				return refused(prompt_edit_refusal::conditions_full);
			}
			from->Conditions->erase(from->Conditions->begin() + static_cast<std::ptrdiff_t>(aAt));
			return prompt_edit_outcome{ };
		});
	}

	auto remove_prompt_condition_group(prompt_editor& aEditor, const std::string& aId, const prompt_condition_path& aPath, const std::size_t aAt) -> prompt_edit_outcome
	{
		return commit_prompt(aEditor, aId, false, [&aPath, aAt](prompt_macro& aMacro) {
			const auto from = group_at(aMacro.Body, aPath);
			if (!from || aAt >= from->Groups->size()) {
				return refused(prompt_edit_refusal::conditions_full);
			}
			from->Groups->erase(from->Groups->begin() + static_cast<std::ptrdiff_t>(aAt));
			return prompt_edit_outcome{ };
		});
	}

	auto set_prompt_condition(prompt_editor& aEditor, const std::string& aId, const prompt_condition_path& aPath, const std::size_t aAt, prompt_condition aCondition) -> prompt_edit_outcome
	{
		return commit_prompt(aEditor, aId, false, [&aPath, aAt, &aCondition](prompt_macro& aMacro) {
			const auto in = group_at(aMacro.Body, aPath);
			if (!in || aAt >= in->Conditions->size()) {
				return refused(prompt_edit_refusal::conditions_full);
			}
			(*in->Conditions)[aAt] = std::move(aCondition);
			return prompt_edit_outcome{ };
		});
	}

	auto set_prompt_group_match(prompt_editor& aEditor, const std::string& aId, const prompt_condition_path& aPath, const prompt_match aMatch) -> prompt_edit_outcome
	{
		return commit_prompt(aEditor, aId, false, [&aPath, aMatch](prompt_macro& aMacro) {
			const auto in = group_at(aMacro.Body, aPath);
			if (!in) {
				return refused(prompt_edit_refusal::conditions_full);
			}
			*in->Match = aMatch;
			return prompt_edit_outcome{ };
		});
	}

	auto stable_name(const prompt_field aField) noexcept -> std::string_view
	{
		const auto index = static_cast<std::size_t>(aField);
		return index < field_names.size() ? field_names.at(index) : std::string_view{ "name" };
	}

	auto find_prompt_field(const std::string_view aName) noexcept -> std::optional<prompt_field>
	{
		const auto found = std::ranges::find(field_names, aName);
		return found == field_names.end() ? std::nullopt : std::optional{ static_cast<prompt_field>(std::distance(field_names.begin(), found)) };
	}

	auto prompt_fields_differing(const prompt_macro& aApp, const prompt_macro& aHere) -> std::vector<prompt_field>
	{
		auto fields = std::vector<prompt_field>{ };
		if (aApp.Body.Name != aHere.Body.Name) {
			fields.push_back(prompt_field::name);
		}
		if (aApp.Body.Template != aHere.Body.Template) {
			fields.push_back(prompt_field::text);
		}
		if (aApp.Body.Kind != aHere.Body.Kind) {
			fields.push_back(prompt_field::answer);
		}
		// Review N3: the conditions are one override, as
		// `repository_layer_of` writes them.
		if (aApp.Body.Conditions != aHere.Body.Conditions || aApp.Body.Match != aHere.Body.Match || aApp.Body.Groups != aHere.Body.Groups || aApp.Body.WhenUnmet != aHere.Body.WhenUnmet) {
			fields.push_back(prompt_field::conditions);
		}
		if (aApp.On != aHere.On) {
			fields.push_back(prompt_field::on);
		}
		return fields;
	}

	auto use_app_wide_prompt(prompt_editor& aEditor, const std::string& aId) -> prompt_edit_outcome
	{
		const auto* const app = find_macro(aEditor.Document.Library, aId);
		if (aEditor.Repository.empty() || app == nullptr) {
			return refused(prompt_edit_refusal::not_in_repository);
		}
		const auto copy = *app;
		return commit_prompt(aEditor, aId, false, [&copy](prompt_macro& aMacro) {
			aMacro.Body = copy.Body;
			aMacro.On   = copy.On;
			return prompt_edit_outcome{ };
		});
	}

	auto use_app_wide_field(prompt_editor& aEditor, const std::string& aId, const prompt_field aField) -> prompt_edit_outcome
	{
		const auto* const app = find_macro(aEditor.Document.Library, aId);
		if (aEditor.Repository.empty() || app == nullptr) {
			return refused(prompt_edit_refusal::not_in_repository);
		}
		const auto copy = *app;
		return commit_prompt(aEditor, aId, false, [&copy, aField](prompt_macro& aMacro) {
			copy_field(aMacro, copy, aField);
			return prompt_edit_outcome{ };
		});
	}

	auto revert_prompt_placement(prompt_editor& aEditor, const std::string& aId, const prompt_surface aSurface) -> prompt_edit_outcome
	{
		if (aEditor.Repository.empty()) {
			return refused(prompt_edit_refusal::not_in_repository);
		}
		const auto app = list(aEditor.Document.Surfaces, aSurface);
		return commit(aEditor, aId, false, [&](prompt_layered& aView) {
			auto&      here = list(aView.Surfaces, aSurface);
			const auto was  = std::ranges::find(app, aId);
			if (was == app.end()) {
				std::erase(here, aId);
			} else if (!contains(here, aId)) {
				// Back where it is app-wide, after the placements before it there.
				auto at = std::size_t{ 0 };
				for (auto before = app.begin(); before != was; ++before) {
					if (const auto found = std::ranges::find(here, *before); found != here.end()) {
						at = static_cast<std::size_t>(found - here.begin()) + 1U;
					}
				}
				here.insert(here.begin() + static_cast<std::ptrdiff_t>(std::min(at, here.size())), aId);
			}
			return prompt_edit_outcome{ };
		});
	}

	auto revert_prompt_order(prompt_editor& aEditor, const prompt_surface aSurface) -> prompt_edit_outcome
	{
		const auto* const layer = find_layer(aEditor);
		if (layer == nullptr) {
			return refused(prompt_edit_refusal::not_in_repository);
		}
		auto unordered = *layer;
		auto change    = std::ranges::find(unordered.Surfaces, aSurface, &prompt_repository_surface::Surface);
		if (change == unordered.Surfaces.end() || !change->Order) {
			return { };
		}
		change->Order   = std::nullopt;
		const auto list = apply_repository_layer(aEditor.Document.Library, aEditor.Document.Surfaces, unordered).Surfaces.Lists.at(static_cast<std::size_t>(aSurface));
		return commit(aEditor, std::nullopt, false, [&](prompt_layered& aView) {
			aView.Surfaces.Lists.at(static_cast<std::size_t>(aSurface)) = list;
			return prompt_edit_outcome{ };
		});
	}

	auto choose_prompt_repository(prompt_editor& aEditor, std::string aRepository) -> bool
	{
		if (aRepository == aEditor.Repository) {
			return false;
		}
		// Review N2: the history is the layer's own. A step kept over the
		// app-wide prompts must not be undone into a repository, nor the
		// reverse.
		aEditor.Repository = std::move(aRepository);
		aEditor.Undo.clear();
		aEditor.Redo.clear();
		aEditor.Typing.reset();
		return true;
	}

	namespace {
		auto step(prompt_editor& aEditor, std::vector<prompt_edit_step>& aFrom, std::vector<prompt_edit_step>& aTo) -> prompt_edit_outcome
		{
			while (!aFrom.empty()) {
				auto taken = std::move(aFrom.back());
				aFrom.pop_back();
				if (taken.Document == aEditor.Document) {
					continue;
				}
				aTo.push_back({ .Document = std::move(aEditor.Document), .Prompt = taken.Prompt });
				aEditor.Document = std::move(taken.Document);
				aEditor.Typing.reset();
				aEditor.Pending = true;
				aEditor.Held    = false;
				return { .Changed = true, .Prompt = std::move(taken.Prompt) };
			}
			return refused(prompt_edit_refusal::no_step);
		}
	} // namespace

	auto undo_prompt_edit(prompt_editor& aEditor) -> prompt_edit_outcome { return step(aEditor, aEditor.Undo, aEditor.Redo); }

	auto redo_prompt_edit(prompt_editor& aEditor) -> prompt_edit_outcome { return step(aEditor, aEditor.Redo, aEditor.Undo); }

	auto prompt_standing_of(const prompt_editor& aEditor, const prompt_macro& aMacro) -> prompt_standing
	{
		auto standing = prompt_standing{ };
		if (const auto* const shipped = shipped_macro(aEditor, aMacro.Id); shipped != nullptr && aMacro.BuiltIn) {
			standing.Edited = shipped->Body != aMacro.Body;
		}
		if (aEditor.Repository.empty()) {
			return standing;
		}
		const auto* const app = find_macro(aEditor.Document.Library, aMacro.Id);
		if (app == nullptr) {
			standing.OnlyHere = true;
			return standing;
		}
		standing.Differing   = prompt_fields_differing(*app, aMacro);
		standing.OffHere     = app->On && !aMacro.On;
		standing.ChangedHere = app->Body != aMacro.Body;
		return standing;
	}

	auto prompt_surface_standing_of(const prompt_editor& aEditor, const prompt_surface aSurface) -> prompt_surface_standing
	{
		auto standing = prompt_surface_standing{ };
		if (aEditor.Repository.empty()) {
			return standing;
		}
		const auto  view = prompt_edit_view(aEditor);
		const auto& here = list(view.Surfaces, aSurface);
		const auto& app  = list(aEditor.Document.Surfaces, aSurface);
		for (const auto& id : here) {
			if (!contains(app, id)) {
				standing.Added.push_back(id);
			}
		}
		for (const auto& id : app) {
			if (!contains(here, id) && find_macro(view.Library, id) != nullptr) {
				standing.Removed.push_back(id);
			}
		}
		if (const auto* const layer = find_layer(aEditor); layer != nullptr) {
			const auto change = std::ranges::find(layer->Surfaces, aSurface, &prompt_repository_surface::Surface);
			standing.Ordered  = change != layer->Surfaces.end() && change->Order.has_value();
		}
		return standing;
	}

	auto prompt_placement_problems(const prompt_macro& aMacro, const prompt_surfaces& aSurfaces) -> std::vector<prompt_placement_problem>
	{
		auto problems = std::vector<prompt_placement_problem>{ };
		for (const auto surface : surfaces_of(aSurfaces, aMacro.Id)) {
			for (const auto& problem : validate_prompt_macro(placed_on(aMacro, surface))) {
				if (prompt_problem_blocks(problem.Problem)) {
					problems.push_back({ .Surface = surface, .Problem = problem.Problem });
					break;
				}
			}
		}
		return problems;
	}
} // namespace bpr::core
