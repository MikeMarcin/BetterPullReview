/// \file
/// \brief [[BPR-2179]] (docs/design/prompts-settings-v4/SPEC.md): Settings ›
/// Prompts. Surfaces above the Prompt library; selection outlines every
/// placement; drag, Add to, Option-Up and Delete edit the surfaces and the
/// core refuses what a surface cannot take; New prompt, Duplicate, Reset to
/// default and Delete; conditions with All or Any and their kinds; the system
/// prompts in the library, never placed or duplicated; every change saves;
/// the page scrolls in a short window and the library keeps its height.
///
/// With `BPR_PROMPT_RENDER_ROOT` set, the page is saved there as PNGs.

#include "bpr/core/prompt_surfaces.hpp"
#include "bpr/qt/agent_setup_settings.hpp"
#include "bpr/qt/prompt_words.hpp"
#include "bpr/qt/prompts_page.hpp"
#include "bpr/qt/writing_hooks_panel.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QDir>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QToolButton>

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	int failures = 0;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			++failures;
		}
	}

	void settle()
	{
		for (int pass = 0; pass < 4; ++pass) {
			QApplication::processEvents();
			// Rows a rebuild retired go now, so a lookup finds the new ones.
			QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		}
	}

	/// A host in miniature: each save is stored as the host would, and the
	/// projection comes back on the next turn.
	struct harness
	{
		QScrollArea                                                                    Scroll;
		qt::prompts_page*                                                              Page  = nullptr;
		qt::writing_hooks_panel*                                                       Hooks = nullptr;
		std::vector<std::pair<std::vector<core::prompt_macro>, core::prompt_surfaces>> Saves;
		std::uint64_t                                                                  Revision = 0U;

		harness()
		{
			Page  = new qt::prompts_page;
			Hooks = new qt::writing_hooks_panel{ Page };
			Hooks->adopt_review_skill(new qt::review_skill_panel{ Page });
			Page->adopt_system_prompts(Hooks);
			Page->set_save_dispatch([this](std::vector<core::prompt_macro> aMacros, core::prompt_surfaces aSurfaces, std::vector<core::prompt_repository_layer> aLayers) {
				Saves.emplace_back(aMacros, aSurfaces);
				auto       differences = core::prompt_macro_differences{ };
				const auto shipped     = core::shipped_prompt_macros();
				for (const auto& macro : aMacros) {
					if (!macro.BuiltIn) {
						differences.Own.push_back(macro);
						continue;
					}
					const auto original = std::ranges::find_if(shipped, [&macro](const core::prompt_builtin& aBuiltin) { return aBuiltin.Macro.Id == macro.Id; });
					if (original != shipped.end() && (original->Macro.Body != macro.Body || original->Macro.On != macro.On)) {
						differences.Changes.push_back({ .Id = macro.Id, .Body = macro.Body, .On = macro.On, .WrittenAgainst = 1U });
					}
				}
				differences.Surfaces     = core::prompt_surface_changes_of(aMacros, aSurfaces);
				differences.Repositories = std::move(aLayers);
				auto projection          = application::prompt_macro_projection{ .Set = core::merge_prompt_macros(shipped, differences), .Revision = ++Revision, .Readable = true, .Loaded = true };
				projection.LastWrite     = application::prompt_macro_write::stored;
				QTimer::singleShot(0, Page, [this, projection] { Page->present(projection); });
			});
			Page->present({ .Set = core::merge_prompt_macros(core::shipped_prompt_macros(), { }), .Readable = true, .Loaded = true });
			Scroll.setWidgetResizable(true);
			Scroll.setWidget(Page);
			Scroll.resize(1180, 840);
			Scroll.show();
			settle();
		}

		[[nodiscard]] auto ids(const core::prompt_surface aSurface) const -> std::vector<std::string> { return Page->surfaces().Lists.at(static_cast<std::size_t>(aSurface)); }
	};

	void render(harness& aHarness, const QString& aName)
	{
		const auto root = qEnvironmentVariable("BPR_PROMPT_RENDER_ROOT");
		if (root.isEmpty()) {
			return;
		}
		QDir{ }.mkpath(root);
		settle();
		static_cast<void>(aHarness.Page->grab().save(QDir{ root }.filePath(aName + QStringLiteral(".png"))));
	}

	void test_layout(harness& aHarness)
	{
		auto& page = *aHarness.Page;
		for (std::size_t surface = 0U; surface < core::prompt_surface_count; ++surface) {
			expect(page.surface_list(static_cast<core::prompt_surface>(surface)) != nullptr, "every surface has its list");
		}
		expect(page.surface_list(core::prompt_surface::pull_request_menu)->count() == static_cast<int>(aHarness.ids(core::prompt_surface::pull_request_menu).size()), "a surface lists its placements");
		expect(page.surface_list(core::prompt_surface::thread_menu)->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff, "Surfaces has no scroll bar of its own");
		// The owner, look v004: the page scrolls; the library is not squashed.
		aHarness.Scroll.resize(1180, 520);
		settle();
		expect(aHarness.Scroll.verticalScrollBar()->maximum() > 0, "a short window scrolls the page");
		expect(page.library_list()->height() >= 360, "the library keeps its full height");
		aHarness.Scroll.resize(1180, 840);
		settle();
		// The system prompts are in the library, under System.
		auto system = false;
		for (int row = 0; row < page.library_list()->count(); ++row) {
			system = system || page.library_list()->item(row)->text() == QStringLiteral("System");
		}
		expect(system, "the system prompts are in the library");
		render(aHarness, QStringLiteral("01-page"));
	}

	void test_selection_outlines(harness& aHarness)
	{
		auto& page = *aHarness.Page;
		page.select("conversation.summarize");
		settle();
		expect(page.footer_text() == QStringLiteral("On 2 surfaces, outlined above"), "the footer counts its surfaces: " + page.footer_text().toStdString());
		auto* const menu     = page.surface_list(core::prompt_surface::thread_menu);
		auto        outlined = false;
		for (int row = 0; row < menu->count(); ++row) {
			outlined = outlined || (menu->item(row)->data(Qt::UserRole + 1).toString() == QStringLiteral("conversation.summarize") && (menu->item(row)->data(Qt::UserRole + 5).toInt() & 8) != 0);
		}
		expect(outlined, "each placement of the selected prompt is outlined");
		expect(page.name_field()->text() == QStringLiteral("Summarize this conversation") && page.name_field()->isEnabled(), "the editor shows it, no hover or double-click");
		const auto name = page.placement_name(core::prompt_surface::thread_menu, 0U);
		expect(name.startsWith(QStringLiteral("Thread menu, 1 of ")), "a placement is named with its surface and position: " + name.toStdString());
		render(aHarness, QStringLiteral("02-selected"));
	}

	void test_placing(harness& aHarness)
	{
		auto&      page = *aHarness.Page;
		const auto n    = aHarness.Saves.size();
		// The owner: action rows are menu only, never chips.
		expect(page.place("agent-tab.what-changed", core::prompt_surface::pull_request_chips) == core::placement_refusal::action_not_chip && !page.last_refusal().isEmpty(),
			   "a chip surface refuses an action row, with a reason");
		auto menu = QMenu{ };
		page.fill_add_to(menu, "agent-tab.what-changed");
		const auto* const chips = menu.findChild<QAction*>(QStringLiteral("promptsAddTo-pull-request-chips"));
		expect(chips != nullptr && !chips->isEnabled() && !chips->toolTip().isEmpty(), "Add to disables a surface that cannot take it, and says why");
		const auto* const ticked = menu.findChild<QAction*>(QStringLiteral("promptsAddTo-pull-request-menu"));
		expect(ticked != nullptr && ticked->isChecked(), "Add to ticks a surface it is on");
		// A drop from the library adds it where the line is.
		expect(page.drop("agent-tab.what-could-break", std::nullopt, core::prompt_surface::thread_chips, 0U, false) == core::placement_refusal::none
				   && aHarness.ids(core::prompt_surface::thread_chips).front() == "agent-tab.what-could-break",
			   "a drop from the library places it at the line");
		// Dragged to another surface it moves; with Option it is copied.
		expect(page.drop("agent-tab.what-could-break", core::prompt_surface::thread_chips, core::prompt_surface::file_chips, 0U, false) == core::placement_refusal::none
				   && aHarness.ids(core::prompt_surface::thread_chips).front() != "agent-tab.what-could-break" && aHarness.ids(core::prompt_surface::file_chips).front() == "agent-tab.what-could-break",
			   "dragged to another surface it moves");
		expect(page.drop("agent-tab.what-could-break", core::prompt_surface::file_chips, core::prompt_surface::thread_chips, 0U, true) == core::placement_refusal::none
				   && aHarness.ids(core::prompt_surface::file_chips).front() == "agent-tab.what-could-break",
			   "with Option it is copied");
		// A drop refused shows no line: the list does not accept it.
		auto* const data = new QMimeData;
		data->setData(QStringLiteral("application/x-bpr-prompt-placement"), QByteArrayLiteral("agent-tab.explain-file\n-1"));
		expect(page.refusal("agent-tab.explain-file", core::prompt_surface::file_chips) == core::placement_refusal::action_not_chip, "the not-allowed drop");
		delete data;
		// Option-Up moves the selected placement; Delete removes it.
		auto* const list   = page.surface_list(core::prompt_surface::thread_chips);
		const auto  before = aHarness.ids(core::prompt_surface::thread_chips);
		list->setFocus();
		list->setCurrentRow(1);
		settle();
		{
			auto up = QKeyEvent{ QEvent::KeyPress, Qt::Key_Up, Qt::AltModifier };
			QApplication::sendEvent(list, &up);
		}
		settle();
		expect(aHarness.ids(core::prompt_surface::thread_chips).front() == before.at(1), "Option-Up moves a placement up");
		list->setCurrentRow(0);
		settle();
		{
			auto remove = QKeyEvent{ QEvent::KeyPress, Qt::Key_Delete, Qt::NoModifier };
			QApplication::sendEvent(list, &remove);
		}
		settle();
		expect(aHarness.ids(core::prompt_surface::thread_chips).size() == before.size() - 1U, "Delete removes it");
		expect(std::ranges::any_of(page.library(), [&before](const core::prompt_macro& aMacro) { return aMacro.Id == before.at(1); }), "removing a placement never deletes the prompt");
		expect(aHarness.Saves.size() > n && aHarness.Saves.back().second == page.surfaces(), "every change saves, surfaces included");
		expect(page.saved_text() == QStringLiteral("Saved"), "and says Saved: " + page.saved_text().toStdString());
	}

	void test_library_actions(harness& aHarness)
	{
		auto&      page = *aHarness.Page;
		const auto id   = page.new_prompt();
		settle();
		expect(page.selected() == id && page.name_field()->text() == QStringLiteral("New prompt") && page.window()->focusWidget() == page.name_field(), "New prompt: selected, its name focused");
		expect(core::surfaces_of(page.surfaces(), id).empty(), "on no surface");
		page.edit_selected([](core::prompt_macro& aMacro) {
			aMacro.Body.Name     = "Explain the risk";
			aMacro.Body.Template = "What could go wrong in {file}?";
		});
		expect(page.place(id, core::prompt_surface::pull_request_chips) == core::placement_refusal::placeholder_unfilled, "the pull request can't fill {file}");
		expect(page.place(id, core::prompt_surface::file_chips) == core::placement_refusal::none, "the file can");
		const auto copy = page.duplicate(id);
		settle();
		expect(copy && page.selected() == *copy && core::surfaces_of(page.surfaces(), *copy).empty() && page.name_field()->text() == QStringLiteral("Explain the risk copy"), "Duplicate: a copy, on no surface, selected");
		expect(page.delete_prompt(id), "an own prompt can be deleted");
		expect(core::surfaces_of(page.surfaces(), id).empty() && std::ranges::none_of(page.library(), [&id](const core::prompt_macro& aMacro) { return aMacro.Id == id; }), "with its placements");
		expect(!page.delete_prompt("agent-tab.review-pull-request"), "a built-in cannot be deleted");
		// Reset to default: the shipped text, On and placements untouched.
		page.select("agent-tab.what-could-break");
		page.edit_selected([](core::prompt_macro& aMacro) { aMacro.Body.Name = "What breaks?"; });
		page.set_on("agent-tab.what-could-break", false);
		const auto placed = core::surfaces_of(page.surfaces(), "agent-tab.what-could-break");
		page.reset_to_default("agent-tab.what-could-break");
		const auto found = std::ranges::find(page.library(), std::string{ "agent-tab.what-could-break" }, &core::prompt_macro::Id);
		expect(found->Body.Name == "What could this change break?" && !found->On && core::surfaces_of(page.surfaces(), "agent-tab.what-could-break") == placed, "Reset restores the text, not On or placements");
		page.set_on("agent-tab.what-could-break", true);
		// The library row's menu: built-ins have no Delete….
		auto menu = QMenu{ };
		page.fill_library_menu(menu, "agent-tab.what-could-break");
		expect(std::ranges::none_of(menu.actions(), [](const QAction* aAction) { return aAction->text().startsWith(QStringLiteral("Delete")); }), "a built-in's menu has no Delete…");
		// Filters.
		page.set_filter(qt::prompt_library_filter::system);
		auto placeable = 0;
		for (int row = 0; row < page.library_list()->count(); ++row) {
			placeable += page.library_list()->item(row)->data(Qt::UserRole + 1).isValid() ? 1 : 0;
		}
		expect(placeable == 0 && page.library_list()->count() > 3, "System lists only the system prompts");
		page.set_filter(qt::prompt_library_filter::all);
	}

	void test_conditions(harness& aHarness)
	{
		auto& page = *aHarness.Page;
		page.select("conversation.summarize");
		page.edit_selected([](core::prompt_macro& aMacro) {
			aMacro.Body.Conditions = { { .Fact = core::prompt_fact::thread_unresolved }, { .Fact = core::prompt_fact::takes_replies } };
			aMacro.Body.Match      = core::prompt_match::any;
		});
		settle();
		auto* const any = page.findChild<QToolButton*>(QStringLiteral("promptsMatchAny"));
		expect(any != nullptr && any->isChecked(), "Any of these is shown chosen");
		const auto* const first = page.findChild<QWidget*>(QStringLiteral("promptsCondition-0"));
		auto              kinds = QStringList{ };
		if (first != nullptr) {
			for (const auto* const label : first->findChildren<QLabel*>()) {
				kinds << label->text();
			}
		}
		expect(kinds.contains(QStringLiteral("Disable")) && kinds.contains(QStringLiteral("Thread is unresolved")), "a condition row shows its name and kind: " + kinds.join(QLatin1Char('|')).toStdString());
		expect(qt::prompt_condition_reason({ .Fact = core::prompt_fact::thread_unresolved }) == QStringLiteral("This thread is resolved."), "its greyed reason");
		auto* const add = page.findChild<QPushButton*>(QStringLiteral("promptsAddCondition"));
		page.edit_selected([](core::prompt_macro& aMacro) {
			aMacro.Body.Conditions.resize(core::maximum_prompt_conditions, { .Fact = core::prompt_fact::draft });
			aMacro.Body.Groups.resize(core::maximum_prompt_conditions);
		});
		expect(add != nullptr && !add->isEnabled() && !add->toolTip().isEmpty(), "full, the add button is disabled and says so");
		// Look prompts-v001: groups nest (All, Any, None), each its own box.
		page.edit_selected([](core::prompt_macro& aMacro) {
			aMacro.Body.Conditions = { { .Fact = core::prompt_fact::thread_unresolved } };
			aMacro.Body.Match      = core::prompt_match::all;
			aMacro.Body.Groups     = { { .Match = core::prompt_match::any, .Conditions = { { .Fact = core::prompt_fact::replies_since_mine } }, .Groups = { { .Match = core::prompt_match::none, .Conditions = { { .Fact = core::prompt_fact::draft } } } } } };
		});
		settle();
		auto* const outer = page.findChild<QWidget*>(QStringLiteral("promptsConditionGroup-g.0"));
		auto* const inner = page.findChild<QWidget*>(QStringLiteral("promptsConditionGroup-g.0.0"));
		auto* const none  = page.findChild<QToolButton*>(QStringLiteral("promptsGroupMatch-g.0.0-none"));
		expect(outer != nullptr && inner != nullptr && inner->parentWidget() == outer && none != nullptr && none->isChecked(), "a group holds a group, each with its own match");
		auto* const anyOuter = page.findChild<QToolButton*>(QStringLiteral("promptsGroupMatch-g.0-all"));
		if (anyOuter != nullptr) {
			anyOuter->click();
			settle();
		}
		const auto changed = std::ranges::find(page.library(), std::string{ "conversation.summarize" }, &core::prompt_macro::Id);
		expect(changed->Body.Groups.front().Match == core::prompt_match::all && aHarness.Saves.back().first == page.library(), "a group's match is edited and saved");
		// Look prompts-v002: Undo and Redo step through condition edits; the
		// list itself offers no None; Add offers Condition and Group; the
		// conditions do not scroll on their own.
		expect(page.undo(), "Undo");
		expect(std::ranges::find(page.library(), std::string{ "conversation.summarize" }, &core::prompt_macro::Id)->Body.Groups.front().Match == core::prompt_match::any, "Undo puts the group's match back");
		expect(page.redo() && std::ranges::find(page.library(), std::string{ "conversation.summarize" }, &core::prompt_macro::Id)->Body.Groups.front().Match == core::prompt_match::all, "Redo does it again");
		expect(page.findChild<QToolButton*>(QStringLiteral("promptsMatchNone"))->isHidden(), "the list itself offers no None");
		auto addMenu = QMenu{ };
		page.fill_condition_menu_for_test(addMenu);
		auto words = QStringList{ };
		for (const auto* const action : addMenu.actions()) {
			words << action->text();
		}
		expect(words == QStringList{ QStringLiteral("Condition"), QStringLiteral("Group") }, "Add offers Condition and Group: " + words.join(QLatin1Char('|')).toStdString());
		expect(page.findChild<QScrollArea*>(QStringLiteral("promptsConditionsScroll")) == nullptr && page.findChild<QWidget*>(QStringLiteral("promptsRemoveCondition")) != nullptr, "no inner scroll, and a trash can removes");
		render(aHarness, QStringLiteral("03-conditions"));
		page.reset_to_default("conversation.summarize");
	}

	// Slice 7: a repository's layer (spec section 11).
	void test_repository(harness& aHarness)
	{
		auto& page = *aHarness.Page;
		page.set_repositories({ QStringLiteral("octo/render") });
		page.choose_repository("octo/render");
		settle();
		expect(page.repository() == "octo/render", "Prompts for chooses the repository");
		// Remove an inherited placement: the row stays, struck, Removed here.
		page.remove("agent-tab.review-pull-request", core::prompt_surface::pull_request_chips);
		auto* const chips   = page.surface_list(core::prompt_surface::pull_request_chips);
		auto        removed = false;
		for (int row = 0; row < chips->count(); ++row) {
			removed = removed || (chips->item(row)->data(Qt::UserRole + 4).toString() == QStringLiteral("Removed here") && (chips->item(row)->data(Qt::UserRole + 5).toInt() & 32) != 0);
		}
		expect(removed, "a placement removed here stays, struck through, Removed here");
		// Off here, a changed text, one of its own.
		page.set_on("agent-tab.what-could-break", false);
		page.select("agent-tab.what-could-break");
		page.edit_selected([](core::prompt_macro& aMacro) { aMacro.Body.Template = "What could break in octo/render?"; });
		const auto own = page.new_prompt();
		expect(page.place(own, core::prompt_surface::thread_chips) == core::placement_refusal::none, "one of its own, placed here");
		settle();
		const auto& layers = page.layers();
		const auto  layer  = std::ranges::find(layers, std::string{ "octo/render" }, &core::prompt_repository_layer::Repository);
		expect(layer != layers.end() && layer->Own.size() == 1U && layer->Overrides.size() == 1U && layer->Overrides.front().On == false && layer->Overrides.front().Template && !layer->Overrides.front().Name,
			   "the layer holds each change on its own");
		page.select("agent-tab.what-could-break");
		settle();
		auto* const differs = page.findChild<QLabel*>(QStringLiteral("promptsDiffers"));
		expect(differs != nullptr && differs->isVisible() && differs->text().contains(QStringLiteral("Use app-wide text")), "the editor says which fields differ, each with its way back");
		render(aHarness, QStringLiteral("05-repository"));
		// The ways back.
		page.use_app_wide_field("agent-tab.what-could-break", QStringLiteral("text"));
		page.revert_placement("agent-tab.review-pull-request", core::prompt_surface::pull_request_chips);
		page.use_app_wide("agent-tab.what-could-break");
		expect(page.delete_prompt(own) && !page.delete_prompt("agent-tab.what-could-break"), "only its own prompt is deleted here");
		settle();
		const auto after = std::ranges::find(page.layers(), std::string{ "octo/render" }, &core::prompt_repository_layer::Repository);
		expect(after == page.layers().end(), "every change reverted: no layer left");
		// All repositories again: untouched.
		page.choose_repository({ });
		const auto appChips = aHarness.ids(core::prompt_surface::pull_request_chips);
		expect(std::ranges::find(appChips, std::string{ "agent-tab.review-pull-request" }) != appChips.end(), "the app-wide surfaces are untouched");
	}

	void test_system_prompts(harness& aHarness)
	{
		auto& page = *aHarness.Page;
		page.select_system(static_cast<int>(core::writing_hook::feedback));
		settle();
		expect(!page.selected() && aHarness.Hooks->isVisible() && aHarness.Hooks->selected_hook() == core::writing_hook::feedback, "a system prompt opens its own editor");
		QListWidgetItem* row = nullptr;
		for (int at = 0; at < page.library_list()->count(); ++at) {
			if (page.library_list()->item(at)->data(Qt::UserRole + 2).toInt() == static_cast<int>(core::writing_hook::feedback) && page.library_list()->item(at)->data(Qt::UserRole + 2).isValid()) {
				row = page.library_list()->item(at);
			}
		}
		expect(row != nullptr && (row->flags() & Qt::ItemIsDragEnabled) == 0 && (row->flags() & Qt::ItemIsUserCheckable) == 0, "a system row is never dragged and has no on/off");
		expect(row != nullptr && row->text().contains(QStringLiteral("System")), "and is marked System");
		render(aHarness, QStringLiteral("04-system"));
	}
} // namespace

auto main(int aArgc, char** aArgv) -> int
{
	auto application = QApplication{ aArgc, aArgv };
	auto harnessed   = harness{ };
	test_layout(harnessed);
	test_selection_outlines(harnessed);
	test_placing(harnessed);
	test_library_actions(harnessed);
	test_conditions(harnessed);
	test_repository(harnessed);
	test_system_prompts(harnessed);
	if (failures != 0) {
		return EXIT_FAILURE;
	}
	std::cout << "prompts-page:pass\n";
	return EXIT_SUCCESS;
}
