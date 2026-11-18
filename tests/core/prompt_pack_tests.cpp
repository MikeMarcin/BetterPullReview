// RFC-BPR-0125 AC24 and AC26 ([[BPR-2149]]), as amended by the owner on
// 2026-11-18: a prompt pack that an import merges in by each prompt's UUID.
// Added, updated in place (On and placements kept), a conflict on a local
// edit, unchanged, dropped from the pack (Remove or Keep), an older pack
// flagged, prompts outside the pack untouched; the exchange writes version 1
// with no prompt and version 2 with a pack; Duplicate drops the link.

#include "bpr/core/agent_configuration.hpp"
#include "bpr/core/prompt_editor.hpp"
#include "bpr/core/prompt_pack.hpp"

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr::core;

	int failures = 0;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			++failures;
		}
	}

	constexpr auto pack_id   = std::string_view{ "0f8e4c2a-1b3d-4e5f-8a9b-0c1d2e3f4a5b" };
	constexpr auto first_id  = std::string_view{ "11111111-1111-4111-8111-111111111111" };
	constexpr auto second_id = std::string_view{ "22222222-2222-4222-9222-222222222222" };
	constexpr auto third_id  = std::string_view{ "33333333-3333-4333-a333-333333333333" };
	constexpr auto other_id  = std::string_view{ "44444444-4444-4444-b444-444444444444" };

	[[nodiscard]] auto body(const std::string& aName, const std::string& aTemplate) -> prompt_macro_body
	{
		auto value     = prompt_macro_body{ };
		value.Name     = aName;
		value.Template = aTemplate;
		return value;
	}

	[[nodiscard]] auto portable(const std::string_view aId, const std::string& aName, const std::string& aTemplate) -> portable_prompt_macro
	{
		return portable_prompt_macro{ .Id = std::string{ aId }, .Place = prompt_place::agent_tab, .Body = body(aName, aTemplate) };
	}

	[[nodiscard]] auto pack(const std::uint32_t aVersion, std::vector<portable_prompt_macro> aMacros) -> decoded_prompt_pack
	{
		auto file = decoded_prompt_pack{ .Pack = { .Id = std::string{ pack_id }, .Label = "Team prompts", .Version = aVersion }, .Macros = { } };
		for (auto& macro : aMacros) {
			file.Macros.push_back({ .Macro = std::move(macro), .Problem = std::nullopt, .Detail = { } });
		}
		return file;
	}

	/// One of the reviewer's own prompts linked to the pack at `aVersion`
	/// with `aName`/`aTemplate` as the pack had it.
	[[nodiscard]] auto linked(const std::string_view aId, const std::string& aName, const std::string& aTemplate, const std::uint32_t aVersion) -> prompt_macro
	{
		auto macro = prompt_macro{ .Id = std::string{ aId }, .Place = prompt_place::agent_tab, .Body = body(aName, aTemplate) };
		macro.Pack = prompt_pack_link{ .Pack = std::string{ pack_id }, .Label = "Team prompts", .Version = aVersion, .Place = prompt_place::agent_tab, .Body = body(aName, aTemplate) };
		return macro;
	}

	[[nodiscard]] auto own(const prompt_macro_differences& aStored, const std::string_view aId) -> std::optional<prompt_macro>
	{
		const auto found = std::ranges::find(aStored.Own, aId, &prompt_macro::Id);
		return found == aStored.Own.end() ? std::nullopt : std::optional{ *found };
	}

	void test_uuids()
	{
		expect(valid_prompt_uuid(first_id) && valid_prompt_uuid(pack_id), "a lowercase version 4 UUID is valid");
		expect(!valid_prompt_uuid("0F8E4C2A-1B3D-4E5F-8A9B-0C1D2E3F4A5B"), "uppercase is not");
		expect(!valid_prompt_uuid("0f8e4c2a-1b3d-1e5f-8a9b-0c1d2e3f4a5b"), "version 1 is not");
		expect(!valid_prompt_uuid("0f8e4c2a-1b3d-4e5f-7a9b-0c1d2e3f4a5b"), "another variant is not");
		expect(!valid_prompt_uuid("own.0123456789abcdef0123456789abcdef"), "an old own identity is not");
		expect(!valid_prompt_uuid("agent-tab.what-could-break"), "a built-in's name is not");
	}

	void test_encode_decode()
	{
		auto macro             = portable(first_id, "Look again", "Look at {pr.title} again.");
		macro.Body.Subject     = prompt_subject::open_file;
		macro.Body.QuickAsk    = true;
		macro.Body.Scopes.File = true;
		macro.Body.Match       = prompt_match::any;
		macro.Body.WhenUnmet   = prompt_when_unmet::hide;
		macro.Body.Conditions  = { prompt_condition{ .Fact = prompt_fact::path, .Negated = true, .Patterns = { "*.cpp", "docs/**" } } };
		auto inner             = prompt_condition_group{ .Match = prompt_match::none, .Conditions = { prompt_condition{ .Fact = prompt_fact::draft } }, .Groups = { } };
		auto outer             = prompt_condition_group{ .Match = prompt_match::all, .Conditions = { prompt_condition{ .Fact = prompt_fact::selected_lines, .Lines = prompt_lines_value::several } }, .Groups = { inner } };
		macro.Body.Groups      = { outer };
		const auto identity    = prompt_pack_identity{ .Id = std::string{ pack_id }, .Label = "Team prompts", .Version = 3U };
		const auto macros      = std::vector{ macro, portable(second_id, "Plain", "Plain text") };
		const auto bytes       = encode_prompt_pack(identity, macros);
		expect(bytes.has_value(), "a pack encodes");
		const auto decoded = bytes ? decode_prompt_pack(*bytes) : std::nullopt;
		expect(decoded && decoded->Pack == identity, "the pack's identity reads back");
		expect(decoded && decoded->Macros.size() == 2U && decoded->Macros[0].Macro == macro && !decoded->Macros[0].Problem, "a prompt reads back exactly, conditions and groups too");
		expect(decoded && decoded->Macros[1].Macro == macros[1], "the second reads back");
		expect(encode_prompt_pack({ .Id = "not-a-uuid", .Label = "x", .Version = 1U }, macros) == std::nullopt, "a pack ID that is not a UUID is not written");
		expect(encode_prompt_pack(identity, std::vector{ portable("own.old", "x", "y") }) == std::nullopt, "a prompt ID that is not a UUID is not written");
		expect(decode_prompt_pack(R"({"id":"bad","label":"L","version":1,"prompts":[]})") == std::nullopt, "a pack ID that is not a UUID makes the pack unreadable");
		expect(decode_prompt_pack(std::string{ R"({"id":")" } + std::string{ pack_id } + R"(","label":"L","version":0,"prompts":[]})") == std::nullopt, "version 0 is unreadable");
	}

	[[nodiscard]] auto one_prompt_file(const std::string& aPrompt) -> std::string
	{
		return std::string{ R"({"id":")" } + std::string{ pack_id } + R"(","label":"L","version":1,"prompts":[)" + aPrompt + "]}";
	}

	void test_unsupported_items()
	{
		const auto prompt = [](const std::string_view aId, const std::string_view aPlace, const std::string_view aTemplate, const std::string_view aFact) {
			auto text = std::string{ R"({"id":")" } + std::string{ aId } + R"(","place":")" + std::string{ aPlace } + R"(","name":"N","template":")" + std::string{ aTemplate }
				+ R"(","about":"pull-request","ask":"question","match":"all","when_unmet":"disable","quick_ask":false)";
			if (!aFact.empty()) {
				text += R"(,"conditions":[{"fact":")" + std::string{ aFact } + R"(","negated":false}])";
			}
			return text + "}";
		};
		const auto place = decode_prompt_pack(one_prompt_file(prompt(first_id, "commit-list", "T", "")));
		expect(place && place->Macros[0].Problem == prompt_pack_item_problem::unsupported && place->Macros[0].Detail == "commit-list", "an unknown place is unsupported and named");
		const auto fact = decode_prompt_pack(one_prompt_file(prompt(first_id, "agent-tab", "T", "moon-phase")));
		expect(fact && fact->Macros[0].Problem == prompt_pack_item_problem::unsupported && fact->Macros[0].Detail == "moon-phase", "an unknown fact is unsupported and named");
		const auto variable = decode_prompt_pack(one_prompt_file(prompt(first_id, "agent-tab", "Look at {pr.colour}", "")));
		expect(variable && variable->Macros[0].Problem == prompt_pack_item_problem::unsupported && variable->Macros[0].Detail == "pr.colour", "an unknown variable is unsupported and named");
		const auto id = decode_prompt_pack(one_prompt_file(prompt("own.0123456789abcdef0123456789abcdef", "agent-tab", "T", "")));
		expect(id && id->Macros[0].Problem == prompt_pack_item_problem::invalid_id, "an ID that is not a UUID is refused as unsupported");
		const auto twice = decode_prompt_pack(one_prompt_file(prompt(first_id, "agent-tab", "T", "") + "," + prompt(first_id, "agent-tab", "U", "")));
		expect(twice && !twice->Macros[0].Problem && twice->Macros[1].Problem == prompt_pack_item_problem::duplicate, "the second of an ID listed twice is refused");
		const auto empty = decode_prompt_pack(one_prompt_file(prompt(first_id, "agent-tab", "", "")));
		expect(empty && empty->Macros[0].Problem == prompt_pack_item_problem::invalid, "an empty template is invalid");
	}

	void test_plan_every_case()
	{
		// Here: one linked and untouched, one linked and edited, one linked
		// that the new version drops, one linked, edited and dropped, one
		// unlinked of the same ID as the file's, and one not from the pack.
		auto stored             = prompt_macro_differences{ };
		auto untouched          = linked(first_id, "First", "First text", 1U);
		untouched.On            = false;
		auto edited             = linked(second_id, "Second", "Second text", 1U);
		edited.Body.Template    = "My own words";
		auto dropped            = linked(third_id, "Third", "Third text", 1U);
		auto droppedEdited      = linked("55555555-5555-4555-8555-555555555555", "Fifth", "Fifth text", 1U);
		droppedEdited.Body.Name = "Fifth, mine";
		const auto mine         = prompt_macro{ .Id = std::string{ other_id }, .Place = prompt_place::agent_tab, .Body = body("Mine", "Mine text") };
		const auto unlinked     = prompt_macro{ .Id = "66666666-6666-4666-9666-666666666666", .Place = prompt_place::agent_tab, .Body = body("Unlinked", "Unlinked text") };
		stored.Own              = { untouched, edited, dropped, droppedEdited, mine, unlinked };
		stored.Surfaces         = { prompt_surface_change{ .Surface = prompt_surface::pull_request_menu, .Ids = { std::string{ first_id } }, .Removed = { } } };
		stored.Orders           = { prompt_place_order{ .Place = prompt_place::agent_tab, .Ids = { std::string{ third_id }, std::string{ first_id } } } };

		const auto added     = std::string{ "77777777-7777-4777-a777-777777777777" };
		auto       unchanged = portable("88888888-8888-4888-b888-888888888888", "Same", "Same text");
		stored.Own.push_back(linked(unchanged.Id, "Same", "Same text", 1U));
		const auto file    = pack(2U, { portable(first_id, "First", "First text, better"), portable(second_id, "Second", "Second text, better"), portable(added, "New", "New text"), unchanged, portable("66666666-6666-4666-9666-666666666666", "Unlinked", "Theirs"), portable("agent-tab.what-could-break", "Built", "In") });
		const auto shipped = shipped_prompt_macros();
		const auto plan    = plan_prompt_pack_import(file, stored, shipped);
		expect(plan.Imported == 1U && !plan.Older, "the version here is the highest linked one; a later file is not older");
		expect(plan.Items.size() == 6U, "one plan per prompt of the file");
		expect(plan.Items[0].Standing == prompt_pack_standing::updated && plan.Items[0].Default, "linked and untouched: updated in place, chosen");
		expect(plan.Items[1].Standing == prompt_pack_standing::conflict && !plan.Items[1].Default, "linked and edited: a conflict, starting at Keep mine");
		expect(plan.Items[2].Standing == prompt_pack_standing::added && plan.Items[2].Default, "no prompt of its ID: added, chosen");
		expect(plan.Items[3].Standing == prompt_pack_standing::unchanged && !plan.Items[3].Default, "equal: unchanged, nothing to choose");
		expect(plan.Items[4].Standing == prompt_pack_standing::conflict, "the same ID never linked to this pack: a conflict, never a silent update");
		expect(plan.Items[5].Problem == prompt_pack_item_problem::built_in_id, "a built-in's ID is refused (decoding refuses it sooner, as no UUID)");
		expect(plan.Dropped.size() == 2U && plan.Dropped[0].Here.Id == third_id && plan.Dropped[0].Default && !plan.Dropped[0].Edited, "a dropped prompt not edited starts at Remove");
		expect(plan.Dropped[1].Edited && !plan.Dropped[1].Default, "a dropped prompt edited starts at Keep");

		// Defaults applied: updated in place with On and placements kept;
		// added; conflict kept; dropped removed with its placements; the
		// edited drop kept and unlinked; the prompt not from the pack as it was.
		const auto choices = default_prompt_pack_choices(plan);
		const auto after   = apply_prompt_pack_import(stored, shipped, file, choices);
		expect(after.has_value(), "the defaults apply");
		if (!after) {
			return;
		}
		const auto first = own(*after, first_id);
		expect(first && first->Body.Template == "First text, better" && !first->On, "updated in place: the file's body, the reviewer's Off kept");
		expect(first && first->Pack && first->Pack->Version == 2U && !prompt_pack_edited(*first), "updated in place: linked at the file's version");
		expect(std::ranges::contains(after->Surfaces[0].Ids, std::string{ first_id }), "updated in place: its placement kept");
		const auto second = own(*after, second_id);
		expect(second && second->Body.Template == "My own words" && second->Pack && second->Pack->Version == 1U, "Keep mine: the reviewer's text and the version it was written against");
		const auto fresh = own(*after, added);
		expect(fresh && fresh->On && fresh->Pack && fresh->Pack->Pack == pack_id && fresh->Body.Template == "New text", "added: on and linked");
		expect(!own(*after, third_id) && !std::ranges::contains(after->Orders[0].Ids, std::string{ third_id }), "removed with its place in order");
		const auto fifth = own(*after, "55555555-5555-4555-8555-555555555555");
		expect(fifth && !fifth->Pack && fifth->Body.Name == "Fifth, mine", "a kept drop is the reviewer's own now");
		expect(own(*after, other_id) == mine, "a prompt not from the pack is untouched");
		expect(own(*after, unchanged.Id)->Pack->Version == 2U, "an unchanged prompt records the file's version");
		expect(own(*after, "66666666-6666-4666-9666-666666666666")->Body.Template == "Unlinked text", "an unchosen conflict keeps the reviewer's text");

		// Replace takes the file's text and links it.
		auto replace = choices;
		replace.Take.push_back(1U);
		const auto replaced = apply_prompt_pack_import(stored, shipped, file, replace);
		expect(replaced && own(*replaced, second_id)->Body.Template == "Second text, better" && !prompt_pack_edited(*own(*replaced, second_id)), "Replace takes the file's prompt");

		// Equal inputs, equal results.
		expect(plan_prompt_pack_import(file, stored, shipped) == plan_prompt_pack_import(file, stored, shipped), "the plan is deterministic");
	}

	void test_older_pack()
	{
		auto stored     = prompt_macro_differences{ };
		stored.Own      = { linked(first_id, "First", "Version 3 text", 3U), linked(second_id, "Second", "Second", 3U) };
		const auto file = pack(2U, { portable(first_id, "First", "Version 2 text") });
		const auto plan = plan_prompt_pack_import(file, stored, shipped_prompt_macros());
		expect(plan.Older && plan.Imported == 3U, "a lower version than the one here is flagged");
		expect(plan.Items[0].Standing == prompt_pack_standing::updated && !plan.Items[0].Default, "an older update starts unchosen");
		expect(plan.Dropped.size() == 1U && !plan.Dropped[0].Default, "an older pack's drop starts at Keep");
	}

	void test_refusals()
	{
		auto stored        = prompt_macro_differences{ };
		stored.Own         = { linked(first_id, "First", "Same", 1U) };
		const auto file    = pack(2U, { portable(first_id, "First", "Same"), portable(second_id, "New", "New") });
		const auto shipped = shipped_prompt_macros();
		expect(apply_prompt_pack_import(stored, shipped, file, { .Take = { 0U }, .Remove = { } }).error() == prompt_pack_apply_refusal::unusable_item, "an unchanged prompt cannot be chosen");
		expect(apply_prompt_pack_import(stored, shipped, file, { .Take = { 9U }, .Remove = { } }).error() == prompt_pack_apply_refusal::unusable_item, "out of range");
		expect(apply_prompt_pack_import(stored, shipped, file, { .Take = { 1U, 1U }, .Remove = { } }).error() == prompt_pack_apply_refusal::duplicate_choice, "chosen twice");
		expect(apply_prompt_pack_import(stored, shipped, file, { .Take = { }, .Remove = { std::string{ first_id } } }).error() == prompt_pack_apply_refusal::unknown_drop, "removing what the pack keeps");
		auto full = prompt_macro_differences{ };
		for (std::size_t index = shipped.size(); index < maximum_prompt_macros; ++index) {
			auto id = std::string{ "own.full-" } + std::to_string(index);
			full.Own.push_back(prompt_macro{ .Id = id, .Place = prompt_place::agent_tab, .Body = body(id, "t") });
		}
		expect(apply_prompt_pack_import(full, shipped, file, { .Take = { 1U }, .Remove = { } }).error() == prompt_pack_apply_refusal::library_full, "a full library refuses all of it");
	}

	void test_export_links_and_renames()
	{
		auto       stored   = prompt_macro_differences{ };
		const auto old      = std::string{ "own.0123456789abcdef0123456789abcdef" };
		stored.Own          = { prompt_macro{ .Id = old, .Place = prompt_place::agent_tab, .Body = body("Old", "Old text") },
								prompt_macro{ .Id = std::string{ first_id }, .Place = prompt_place::agent_tab, .Body = body("Uuid", "Uuid text") },
								prompt_macro{ .Id = std::string{ other_id }, .Place = prompt_place::agent_tab, .Body = body("Not chosen", "x") } };
		stored.Orders       = { prompt_place_order{ .Place = prompt_place::agent_tab, .Ids = { old, std::string{ first_id } } } };
		stored.Surfaces     = { prompt_surface_change{ .Surface = prompt_surface::pull_request_chips, .Ids = { old }, .Removed = { } } };
		stored.Repositories = { prompt_repository_layer{ .Repository = "o/r", .Overrides = { prompt_override{ .Id = old } }, .Own = { }, .Surfaces = { } } };
		const auto identity = prompt_pack_identity{ .Id = std::string{ pack_id }, .Label = "Team", .Version = 1U };
		const auto fresh    = std::vector<std::string>{ std::string{ second_id } };
		const auto chosen   = std::vector<std::string>{ old, std::string{ first_id } };
		const auto exported = export_prompt_pack(stored, identity, chosen, fresh);
		expect(exported.has_value(), "an export of own prompts is made");
		if (!exported) {
			return;
		}
		expect(exported->Macros.size() == 2U && exported->Macros[0].Id == second_id && exported->Macros[1].Id == first_id, "a prompt made before UUIDs gets the minted one; a UUID stays");
		expect(!own(exported->After, old) && own(exported->After, second_id), "the old identity is gone from the library");
		expect(exported->After.Orders[0].Ids[0] == second_id && exported->After.Surfaces[0].Ids[0] == second_id && exported->After.Repositories[0].Overrides[0].Id == second_id,
			   "every reference names the new identity");
		expect(own(exported->After, first_id)->Pack == prompt_pack_link{ .Pack = std::string{ pack_id }, .Label = "Team", .Version = 1U, .Place = prompt_place::agent_tab, .Body = body("Uuid", "Uuid text") },
			   "an exported prompt is linked to the pack at its version");
		expect(!own(exported->After, other_id)->Pack, "a prompt not chosen is untouched");
		expect(export_prompt_pack(stored, identity, chosen, { }) == std::nullopt, "no UUID to give: nothing is exported");
		expect(export_prompt_pack(stored, identity, std::vector<std::string>{ "agent-tab.what-could-break" }, fresh) == std::nullopt, "a built-in is not exported");

		const auto packs = prompt_packs_here(exported->After);
		expect(packs.size() == 1U && packs[0].Pack == identity && packs[0].Members.size() == 2U, "the profile now holds the pack");
		expect(next_prompt_pack(packs[0], { }) == prompt_pack_identity{ .Id = std::string{ pack_id }, .Label = "Team", .Version = 2U }, "the next export is one version on");
	}

	void test_duplicate_drops_the_link()
	{
		auto editor = make_prompt_editor(shipped_prompt_macros());
		auto set    = merge_prompt_macros(shipped_prompt_macros(), prompt_macro_differences{ .Changes = { }, .Own = { linked(first_id, "First", "Text", 1U) }, .Orders = { }, .Surfaces = { }, .Repositories = { } });
		static_cast<void>(adopt_prompt_projection(editor, set, 1U, true, true));
		const auto outcome = duplicate_prompt(editor, std::string{ first_id }, std::string{ second_id }, "First copy");
		expect(outcome.Changed, "an imported prompt duplicates");
		const auto view = prompt_edit_view(editor);
		const auto copy = std::ranges::find(view.Library, second_id, &prompt_macro::Id);
		expect(copy != view.Library.end() && !copy->Pack && copy->Body.Template == "Text", "the copy has the new UUID and no pack");
		const auto original = std::ranges::find(view.Library, first_id, &prompt_macro::Id);
		expect(original != view.Library.end() && original->Pack, "the original keeps its link");
		// An edit through the editor keeps the link, so the import sees it as edited.
		auto edited          = *original;
		edited.Body.Template = "Changed";
		static_cast<void>(edit_prompt(editor, edited));
		const auto differences = prompt_macro_differences_of(shipped_prompt_macros(), editor.Document);
		const auto saved       = own(differences, first_id);
		expect(saved && saved->Pack && prompt_pack_edited(*saved), "a saved edit keeps the link and reads as edited");
	}

	void test_exchange_versions()
	{
		auto configuration       = agent_configuration{ };
		configuration.ExportedBy = "BetterPullReview";
		const auto v1            = encode_agent_configuration(configuration);
		expect(v1 && v1->find("\"version\": 1") != std::string::npos && v1->find("prompt_pack") == std::string::npos, "no prompt chosen: version 1, no pack");
		configuration.PromptPack   = { .Id = std::string{ pack_id }, .Label = "Team", .Version = 4U };
		configuration.PromptMacros = { portable(first_id, "First", "First text") };
		const auto v2              = encode_agent_configuration(configuration);
		expect(v2 && v2->find("\"version\": 2") != std::string::npos, "a pack: version 2");
		expect(v2 && v2->find("\n   \"prompt_pack\": {\n      \"id\": ") != std::string::npos && v2->find("\n         {\n            \"id\": ") != std::string::npos,
			   "the pack is laid out one member a line, at the file's indentation, for editing in place");
		const auto read = v2 ? decode_agent_configuration(*v2, { }) : std::unexpected(configuration_file_refusal::malformed);
		expect(read && read->Version == 2U && read->PromptPack && read->PromptPack->Pack.Version == 4U && read->PromptPack->Macros[0].Macro == configuration.PromptMacros[0], "version 2 reads back");
		const auto oldRead = v1 ? decode_agent_configuration(*v1, { }) : std::unexpected(configuration_file_refusal::malformed);
		expect(oldRead && oldRead->Version == 1U && !oldRead->PromptPack, "version 1 reads as before");
		auto mixed = *v2;
		mixed.replace(mixed.find("\"version\": 2"), 12U, "\"version\": 1");
		expect(decode_agent_configuration(mixed, { }).error() == configuration_file_refusal::malformed, "a pack in a version 1 file is malformed");
		auto newer = *v2;
		newer.replace(newer.find("\"version\": 2"), 12U, "\"version\": 3");
		expect(decode_agent_configuration(newer, { }).error() == configuration_file_refusal::unsupported_version, "version 3 is refused as newer");
	}

	/// AC26: export from one profile, import into a clean one, re-import a
	/// later version: updated in place, other prompts unchanged.
	void test_round_trip()
	{
		const auto shipped         = shipped_prompt_macros();
		auto       author          = prompt_macro_differences{ };
		author.Own                 = { prompt_macro{ .Id = std::string{ first_id }, .Place = prompt_place::agent_tab, .Body = body("Look again", "Look again at {pr.title}.") },
									   prompt_macro{ .Id = std::string{ second_id }, .Place = prompt_place::conversation, .Body = body("Still open?", "Is this still open?") } };
		author.Own[1].Body.Subject = prompt_subject::conversation;
		const auto identity        = prompt_pack_identity{ .Id = std::string{ pack_id }, .Label = "Team", .Version = 1U };
		const auto ids             = std::vector<std::string>{ std::string{ first_id }, std::string{ second_id } };
		auto       one             = export_prompt_pack(author, identity, ids, { });
		auto       file            = agent_configuration{ .Label = { }, .ExportedBy = "A", .Agents = { }, .Prompts = { }, .Workflows = { }, .Identity = { }, .Repositories = { }, .PromptPack = identity, .PromptMacros = one->Macros };
		const auto bytes1          = encode_agent_configuration(file);
		const auto read1           = decode_agent_configuration(*bytes1, { });
		expect(read1 && read1->PromptPack, "version 1 of the pack reads");

		auto       teammate = prompt_macro_differences{ };
		const auto theirs   = prompt_macro{ .Id = std::string{ other_id }, .Place = prompt_place::agent_tab, .Body = body("Theirs", "Their own") };
		teammate.Own        = { theirs };
		const auto plan1    = plan_prompt_pack_import(*read1->PromptPack, teammate, shipped);
		expect(std::ranges::all_of(plan1.Items, [](const prompt_pack_item_plan& aItem) { return aItem.Standing == prompt_pack_standing::added; }), "into a clean profile every prompt is added");
		auto imported = apply_prompt_pack_import(teammate, shipped, *read1->PromptPack, default_prompt_pack_choices(plan1));
		expect(imported && imported->Own.size() == 3U, "both added beside the teammate's own");
		auto turnedOff                                                     = *imported;
		std::ranges::find(turnedOff.Own, second_id, &prompt_macro::Id)->On = false;

		// The author improves one and exports version 2.
		std::ranges::find(one->After.Own, first_id, &prompt_macro::Id)->Body.Template = "Look again, closely, at {pr.title}.";
		const auto packs                                                              = prompt_packs_here(one->After);
		const auto two                                                                = export_prompt_pack(one->After, next_prompt_pack(packs[0], { }), ids, { });
		file.PromptPack                                                               = two->Pack;
		file.PromptMacros                                                             = two->Macros;
		const auto read2                                                              = decode_agent_configuration(*encode_agent_configuration(file), { });
		const auto plan2                                                              = plan_prompt_pack_import(*read2->PromptPack, turnedOff, shipped);
		expect(plan2.Items[0].Standing == prompt_pack_standing::updated && plan2.Items[1].Standing == prompt_pack_standing::unchanged && plan2.Dropped.empty(), "re-import: one update, one unchanged");
		const auto after = apply_prompt_pack_import(turnedOff, shipped, *read2->PromptPack, default_prompt_pack_choices(plan2));
		expect(after && own(*after, first_id)->Body.Template == "Look again, closely, at {pr.title}.", "updated in place");
		expect(after && !own(*after, second_id)->On, "the teammate's Off stays");
		expect(after && own(*after, other_id) == theirs && after->Own.size() == 3U, "the teammate's other prompt is unchanged");
	}
} // namespace

auto main() -> int
{
	test_uuids();
	test_encode_decode();
	test_unsupported_items();
	test_plan_every_case();
	test_older_pack();
	test_refusals();
	test_export_links_and_renames();
	test_duplicate_drops_the_link();
	test_exchange_versions();
	test_round_trip();
	if (failures != 0) {
		std::cerr << "prompt-pack-tests:fail " << failures << '\n';
		return EXIT_FAILURE;
	}
	std::cout << "prompt-pack-tests:pass\n";
	return EXIT_SUCCESS;
}
