/// \file
/// \brief [[BPR-2179]] (docs/design/prompts-settings-v4/SPEC.md): Settings ›
/// Prompts. Surfaces above the Prompt library; selection outlines every
/// placement; drag, Add to, Option-Up and Delete edit the surfaces and the
/// core refuses what a surface cannot take; New prompt, Duplicate, Reset to
/// default and Delete; conditions with All or Any and their kinds; the system
/// prompts in the library, never placed or duplicated; every change saves;
/// the page scrolls in a short window and the library keeps its height.
///
/// [[BPR-2220]] (owner, 2026-11-16): a prompt is always selected while the
/// library shows one, and its editor is hidden only while it shows none;
/// New prompt and Restore built-ins sit on the filter's row.
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
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <optional>
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

	/// A host in miniature: a store with the revision rule, each save's
	/// outcome heard first and the store read back on the next turn, as the
	/// host does; identities minted here.
	struct harness
	{
		QScrollArea                                                                    Scroll;
		qt::prompts_page*                                                              Page  = nullptr;
		qt::writing_hooks_panel*                                                       Hooks = nullptr;
		std::vector<std::pair<std::vector<core::prompt_macro>, core::prompt_surfaces>> Saves;
		std::vector<std::uint64_t>                                                     From;
		core::prompt_macro_differences                                                 Stored;
		std::uint64_t                                                                  Revision = 0U;
		bool                                                                           Refuse   = false;
		int                                                                            Minted   = 0;

		[[nodiscard]] auto projection(const std::optional<core::prompt_save_outcome> aWrite = std::nullopt) const -> application::prompt_macro_projection
		{
			auto read      = application::prompt_macro_projection{ .Set = core::merge_prompt_macros(core::shipped_prompt_macros(), Stored), .Revision = Revision, .Readable = true, .Loaded = true };
			read.LastWrite = aWrite;
			return read;
		}

		harness()
		{
			Page  = new qt::prompts_page;
			Hooks = new qt::writing_hooks_panel{ Page };
			Hooks->adopt_review_skill(new qt::review_skill_panel{ Page });
			Page->adopt_system_prompts(Hooks);
			Page->set_identity_source([this] { return "own.minted-" + std::to_string(++Minted); });
			Page->set_save_dispatch([this](core::prompt_save_request aRequest) {
				From.push_back(aRequest.From);
				auto outcome = core::prompt_save_outcome::stored;
				if (Refuse) {
					outcome = core::prompt_save_outcome::failed;
				} else if (aRequest.From != Revision) {
					outcome = core::prompt_save_outcome::stale;
				} else {
					Stored         = aRequest.Differences;
					Revision       = aRequest.From + 1U;
					const auto set = core::merge_prompt_macros(core::shipped_prompt_macros(), Stored);
					Saves.emplace_back(set.macros(), set.Surfaces);
				}
				QTimer::singleShot(0, Page, [this, outcome] {
					Page->settle_save(outcome);
					Page->present(projection(outcome));
				});
			});
			Page->present(projection());
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

	/// The library's rows that are prompts, in order, by identity.
	[[nodiscard]] auto shown_prompts(const qt::prompts_page& aPage) -> std::vector<std::string>
	{
		auto ids = std::vector<std::string>{ };
		for (int row = 0; row < aPage.library_list()->count(); ++row) {
			if (const auto id = aPage.library_list()->item(row)->data(Qt::UserRole + 1); id.isValid()) {
				ids.push_back(id.toString().toStdString());
			}
		}
		return ids;
	}

	[[nodiscard]] auto current_prompt(const qt::prompts_page& aPage) -> std::optional<std::string>
	{
		const auto* const item = aPage.library_list()->currentItem();
		if (item == nullptr || !item->data(Qt::UserRole + 1).isValid()) {
			return std::nullopt;
		}
		return item->data(Qt::UserRole + 1).toString().toStdString();
	}

	/// [[BPR-2220]]: the owner, 2026-11-16, "the bottom of prompt library
	/// looks like a broken menu when no prompt is selected... either always
	/// have one selected or hide that bottom area until one is."
	void test_always_selected(harness& aHarness)
	{
		auto&       page   = *aHarness.Page;
		auto* const editor = page.findChild<QWidget*>(QStringLiteral("promptsEditor"));
		auto* const filter = page.findChild<QLineEdit*>(QStringLiteral("promptsFilter"));
		expect(editor != nullptr && filter != nullptr, "the page has its editor and filter");
		if (editor == nullptr || filter == nullptr) {
			return;
		}
		// Narrow, the editor is the area under the library the owner saw.
		aHarness.Scroll.resize(620, 900);
		settle();
		render(aHarness, QStringLiteral("10-opened-narrow"));
		aHarness.Scroll.resize(1180, 840);
		settle();
		render(aHarness, QStringLiteral("11-opened-wide"));
		const auto first = shown_prompts(page);
		expect(!first.empty() && page.selected() == first.front() && current_prompt(page) == first.front(), "opened, the library's first prompt is selected");
		const auto        found = first.empty() ? page.library().end() : std::ranges::find(page.library(), first.front(), &core::prompt_macro::Id);
		const auto* const macro = found != page.library().end() ? &*found : nullptr;
		expect(macro != nullptr && page.name_field()->isEnabled() && page.name_field()->text() == QString::fromStdString(macro->Body.Name) && !page.text_field()->toPlainText().isEmpty(),
			   "and the editor shows it: " + page.name_field()->text().toStdString());
		expect(editor->isVisible(), "the editor is shown");

		// Deleting the selected prompt selects its neighbour.
		const auto a = page.new_prompt();
		const auto b = page.new_prompt();
		settle();
		page.select(a);
		settle();
		auto       shown     = shown_prompts(page);
		const auto at        = std::ranges::find(shown, a);
		const auto neighbour = at != shown.end() && std::next(at) != shown.end() ? *std::next(at) : (at != shown.begin() && at != shown.end() ? *std::prev(at) : std::string{ });
		expect(!neighbour.empty() && page.delete_prompt(a), "the selected prompt is deleted");
		settle();
		expect(page.selected() == neighbour && current_prompt(page) == neighbour && page.name_field()->isEnabled(), "deleting the selected prompt selects its neighbour");
		// Again from the other new prompt: the next row, else the one before.
		page.select(b);
		settle();
		shown            = shown_prompts(page);
		const auto atB   = std::ranges::find(shown, b);
		const auto other = atB == shown.end() ? std::string{ } : std::next(atB) != shown.end() ? *std::next(atB)
			: atB != shown.begin()                                                             ? *std::prev(atB)
																							   : std::string{ };
		expect(page.delete_prompt(b) && !other.empty() && page.selected() == other, "deleting it selects the next row, else the one before");
		settle();

		// [[BPR-2219]] (owner, 2026-11-16). The review prompt's body is its
		// real instructions, in the editor; under it, read-only, what is
		// always added: the report format the application reads. A prompt
		// that does not ask for findings has no such section.
		{
			page.set_review_result(QStringLiteral("# Reports\n\nThe first line is the title."));
			page.select("agent-tab.review-pull-request");
			settle();
			auto* const body   = page.findChild<QPlainTextEdit*>(QStringLiteral("promptsText"));
			auto* const box    = page.review_result_box();
			auto* const title  = page.findChild<QLabel*>(QStringLiteral("promptsResultTitle"));
			auto* const note   = page.findChild<QLabel*>(QStringLiteral("promptsResultNote"));
			auto* const format = page.findChild<QPlainTextEdit*>(QStringLiteral("promptsResultText"));
			expect(body != nullptr && body->toPlainText().startsWith(QStringLiteral("Review this pull request as a careful colleague would")) && body->toPlainText().contains(QStringLiteral("Sections, in this order"))
					   && body->toPlainText().contains(QStringLiteral("Severity:")) && body->toPlainText().contains(QStringLiteral("Tone:")) && !body->isReadOnly(),
				   "the review prompt's editor holds its instructions, editable");
			expect(box != nullptr && box->isVisibleTo(&page) && title != nullptr && title->text() == QStringLiteral("Result: Findings report") && note != nullptr
					   && note->text().startsWith(QStringLiteral("Always added: the report format BetterPullReview reads.")),
				   "under it, the Result: Findings report section says it is always added");
			expect(format != nullptr && format->isReadOnly() && format->toPlainText() == QStringLiteral("# Reports\n\nThe first line is the title.") && format->accessibleName().contains(QStringLiteral("read-only")),
				   "it shows the format, read-only");
			page.select("agent-tab.what-could-break");
			settle();
			expect(!box->isVisibleTo(&page), "a prompt that asks no findings has no Result section");
		}

		// A filter that still shows the selection keeps it.
		page.select("agent-tab.what-could-break");
		settle();
		filter->setText(QStringLiteral("break"));
		settle();
		expect(page.selected() == "agent-tab.what-could-break" && current_prompt(page) == "agent-tab.what-could-break", "a filter that still shows the selection keeps it");
		// One that hides it selects the first shown.
		filter->setText(QStringLiteral("summarize"));
		settle();
		shown = shown_prompts(page);
		expect(!shown.empty() && page.selected() == shown.front() && current_prompt(page) == shown.front(), "a filter that hides the selection selects the first prompt shown");
		const auto kept = page.selected();
		// One that shows nothing hides the editor; the list says so.
		filter->setText(QStringLiteral("no prompt is named this"));
		settle();
		expect(!editor->isVisible() && shown_prompts(page).empty() && page.library_list()->count() == 1 && page.library_list()->item(0)->text() == QStringLiteral("No prompts match."),
			   "a filter that shows nothing hides the editor, and the list says so");
		aHarness.Scroll.resize(620, 900);
		settle();
		render(aHarness, QStringLiteral("12-filtered-to-nothing-narrow"));
		aHarness.Scroll.resize(1180, 840);
		settle();
		// Cleared, the editor is back with the selection it had.
		filter->clear();
		settle();
		expect(editor->isVisible() && page.selected() == kept && current_prompt(page) == kept, "cleared, the editor is back with the selection it had");
		// Yours with none of your own says what to do.
		page.set_filter(qt::prompt_library_filter::yours);
		settle();
		expect(!editor->isVisible() && page.library_list()->count() == 1 && page.library_list()->item(0)->text().contains(QStringLiteral("New prompt")), "Yours with none says how to write one");
		page.set_filter(qt::prompt_library_filter::all);
		settle();
		expect(editor->isVisible() && page.selected() == kept, "All again, the same selection");
	}

	/// [[BPR-2220]]: the owner, 2026-11-16, "we can probably move the restore
	/// built-ins and new prompt to the same row as the filter for the prompt
	/// library." The filter takes the room left; the row fits the page's
	/// narrowest width.
	void test_tools_row(harness& aHarness)
	{
		auto&       page     = *aHarness.Page;
		auto* const filter   = page.findChild<QLineEdit*>(QStringLiteral("promptsFilter"));
		auto* const restore  = page.findChild<QPushButton*>(QStringLiteral("promptsRestore"));
		auto* const create   = page.findChild<QPushButton*>(QStringLiteral("promptsNew"));
		auto* const segment  = page.findChild<QWidget*>(QStringLiteral("promptsFilter-3"));
		auto* const segment0 = page.findChild<QWidget*>(QStringLiteral("promptsFilter-0"));
		expect(filter != nullptr && restore != nullptr && create != nullptr && segment != nullptr && segment0 != nullptr, "the library's tools are there");
		if (filter == nullptr || restore == nullptr || create == nullptr || segment == nullptr || segment0 == nullptr) {
			return;
		}
		const auto box      = [&page](const QWidget* aWidget) { return QRect{ aWidget->mapTo(&page, QPoint{ 0, 0 }), aWidget->size() }; };
		const auto same_row = [&box](const QWidget* aOne, const QWidget* aOther) {
			return box(aOne).center().y() >= box(aOther).top() && box(aOne).center().y() <= box(aOther).bottom();
		};
		for (const auto width : { 1180, 900, 700, 500 }) {
			aHarness.Scroll.resize(width, 840);
			settle();
			const auto here  = "at " + std::to_string(width) + ": ";
			const auto right = page.width();
			expect(aHarness.Scroll.horizontalScrollBar()->maximum() == 0, here + "the page does not scroll sideways");
			for (const auto* const widget : { static_cast<const QWidget*>(filter), static_cast<const QWidget*>(restore), static_cast<const QWidget*>(create), static_cast<const QWidget*>(segment), static_cast<const QWidget*>(segment0) }) {
				expect(box(widget).left() >= 0 && box(widget).right() < right && widget->width() >= widget->minimumSizeHint().width(), here + widget->objectName().toStdString() + " fits unclipped");
			}
			expect(same_row(restore, filter) && same_row(create, filter), here + "New prompt and Restore built-ins are on the filter's row");
			expect(box(restore).left() > box(filter).right() && box(create).left() > box(restore).right(), here + "at its right, Restore then New prompt");
			expect(box(create).right() >= right - 24, here + "New prompt ends the row");
			expect(filter->width() >= 120, here + "the filter keeps a usable width: " + std::to_string(filter->width()));
		}
		aHarness.Scroll.resize(500, 840);
		settle();
		render(aHarness, QStringLiteral("13-tools-narrowest"));
		aHarness.Scroll.resize(1180, 840);
		settle();
		expect(same_row(segment0, filter), "as drawn the filter choices share its row");
		render(aHarness, QStringLiteral("14-tools-wide"));
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

	/// The owner, 2026-11-15: "there's no information on where we're adding
	/// it to". The library row's Add to heads each group with the surface the
	/// Surfaces section shows it under, in that order.
	void test_add_to_headings(harness& aHarness)
	{
		auto& page = *aHarness.Page;
		auto  menu = QMenu{ };
		page.fill_library_menu(menu, "agent-tab.what-could-break");
		const auto   actions = menu.actions();
		const auto   addTo   = std::ranges::find(actions, QStringLiteral("Add to"), &QAction::text);
		QMenu* const sub     = addTo != actions.end() ? (*addTo)->menu() : nullptr;
		expect(sub != nullptr, "the library row's menu has Add to");
		if (sub == nullptr) {
			return;
		}
		// Each group, heading first: its title, then its items' words.
		auto groups = std::vector<QStringList>{ };
		for (const auto* const action : sub->actions()) {
			if (action->isSeparator()) {
				continue;
			}
			if (action->objectName() == QStringLiteral("promptsAddToHeading")) {
				expect(!action->isEnabled() && action->font().bold() && !action->isCheckable(), "a heading is a bold title, not a choice");
				groups.push_back({ action->text() });
			} else if (action->objectName().startsWith(QStringLiteral("promptsAddTo-")) && !groups.empty()) {
				groups.back() << action->text();
			}
		}
		const auto expected = std::vector<QStringList>{
			{ QStringLiteral("Pull request"), QStringLiteral("Ask menu"), QStringLiteral("Suggested chips") },
			{ QStringLiteral("File"), QStringLiteral("Ask menu"), QStringLiteral("Suggested chips") },
			{ QStringLiteral("Thread"), QStringLiteral("Ask menu"), QStringLiteral("Suggested chips") },
			{ QStringLiteral("Tour"), QStringLiteral("Tour stop"), QStringLiteral("Tour part"), QStringLiteral("Whole tour") },
		};
		auto seen = QStringList{ };
		for (const auto& group : groups) {
			seen << group.join(QStringLiteral(" / "));
		}
		expect(groups == expected, "Add to heads Pull request, File, Thread and Tour, in that order: " + seen.join(QStringLiteral(" | ")).toStdString());
		// Every item and every group is the Surfaces section's own words.
		for (std::size_t surface = 0U; surface < core::prompt_surface_count; ++surface) {
			const auto* const action = sub->findChild<QAction*>(QStringLiteral("promptsAddTo-%1").arg(QString::fromLatin1(core::stable_name(static_cast<core::prompt_surface>(surface)))));
			expect(action != nullptr && action->isCheckable(), "every surface is a checkable choice under its heading");
		}
		// The tour surfaces are greyed for another chat's prompt, and say why
		// under them as well as in their tooltips.
		const auto tours = std::array{ core::prompt_surface::tour_stop, core::prompt_surface::tour_part, core::prompt_surface::whole_tour };
		auto       why   = QString{ };
		for (const auto tour : tours) {
			const auto* const action = sub->findChild<QAction*>(QStringLiteral("promptsAddTo-%1").arg(QString::fromLatin1(core::stable_name(tour))));
			expect(action != nullptr && !action->isEnabled() && !action->toolTip().isEmpty(), "a tour surface is greyed for this prompt, with its reason as a tooltip");
			why = action != nullptr ? action->toolTip() : why;
		}
		const auto* const note = sub->actions().back();
		expect(note->objectName() == QStringLiteral("promptsAddToNote") && !note->isEnabled() && note->text() == why && !why.isEmpty(), "the greyed Tour group says why under it: " + note->text().toStdString());
		const auto notes = std::ranges::count(sub->actions(), QStringLiteral("promptsAddToNote"), &QAction::objectName);
		expect(notes == 1, "only a group greyed throughout gets a note");
		const auto root = qEnvironmentVariable("BPR_PROMPT_RENDER_ROOT");
		if (!root.isEmpty()) {
			QDir{ }.mkpath(root);
			sub->ensurePolished();
			sub->adjustSize();
			static_cast<void>(sub->grab().save(QDir{ root }.filePath(QStringLiteral("06-add-to-menu.png"))));
		}
		static_cast<void>(aHarness);
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
		// Look prompts-v003: no kind on a condition; the prompt chooses.
		expect(!kinds.contains(QStringLiteral("Disable")) && kinds.contains(QStringLiteral("Thread is unresolved")), "a condition row shows its name, no kind: " + kinds.join(QLatin1Char('|')).toStdString());
		auto* const unmetHide = page.findChild<QToolButton*>(QStringLiteral("promptsUnmetHide"));
		expect(unmetHide != nullptr && unmetHide->isVisible(), "the prompt offers Hide it and Show it disabled");
		unmetHide->click();
		settle();
		expect(std::ranges::find(page.library(), std::string{ "conversation.summarize" }, &core::prompt_macro::Id)->Body.WhenUnmet == core::prompt_when_unmet::hide, "Hide it is the prompt's choice");
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

	// [[BPR-2185]] (review N1, N2): the save protocol and undo over every
	// edit, through the page.
	void test_saving(harness& aHarness)
	{
		auto&      page = *aHarness.Page;
		const auto id   = std::string{ "agent-tab.what-could-break" };
		const auto on   = [&page, &id] { return std::ranges::find(page.library(), id, &core::prompt_macro::Id)->On; };
		settle();
		// A failed write keeps the edit.
		aHarness.Refuse = true;
		page.set_on(id, false);
		settle();
		auto* const status = page.findChild<QLabel*>(QStringLiteral("promptsStatus"));
		expect(!on() && status->text().contains(QStringLiteral("couldn't be saved")), "review N1(a): a failed write keeps the edit and says so");
		aHarness.Refuse = false;
		page.set_on("agent-tab.review-pull-request", false);
		settle();
		const auto stored = core::merge_prompt_macros(core::shipped_prompt_macros(), aHarness.Stored).macros();
		expect(std::ranges::find(stored, id, &core::prompt_macro::Id)->On == false && page.saved_text() == QStringLiteral("Saved"), "it saves with the next edit");
		// Undo covers On, and clears Redo.
		expect(page.undo() && std::ranges::find(page.library(), std::string{ "agent-tab.review-pull-request" }, &core::prompt_macro::Id)->On, "review N2: Undo turns a prompt back on");
		page.set_on(id, true);
		expect(!page.redo(), "an edit clears Redo");
		settle();
		// Another window writes; this window's edit was made before it read it.
		const auto from = aHarness.Revision;
		aHarness.Stored.Own.push_back(core::prompt_macro{ .Id = "own.theirs", .Body = { .Name = "Theirs", .Template = "Theirs" } });
		++aHarness.Revision;
		page.set_on(id, false);
		settle();
		expect(aHarness.From.back() == from, "review N1(b): the save names the revision its edit came from");
		expect(std::ranges::any_of(page.library(), [](const core::prompt_macro& aMacro) { return aMacro.Id == "own.theirs"; }) && status->text().contains(QStringLiteral("Another window")),
			   "the store refuses it as stale and the page shows theirs");
		// The host mints a new prompt's identity.
		const auto minted = page.new_prompt();
		expect(minted.starts_with("own.minted-"), "review N2: a new prompt's identity comes from the host");
		static_cast<void>(page.delete_prompt(minted));
		static_cast<void>(page.delete_prompt("own.theirs"));
		page.set_on(id, true);
		page.set_on("agent-tab.review-pull-request", true);
		settle();
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
	test_always_selected(harnessed);
	test_tools_row(harnessed);
	test_layout(harnessed);
	test_selection_outlines(harnessed);
	test_placing(harnessed);
	test_library_actions(harnessed);
	test_add_to_headings(harnessed);
	test_conditions(harnessed);
	test_repository(harnessed);
	test_saving(harnessed);
	test_system_prompts(harnessed);
	if (failures != 0) {
		return EXIT_FAILURE;
	}
	std::cout << "prompts-page:pass\n";
	return EXIT_SUCCESS;
}
