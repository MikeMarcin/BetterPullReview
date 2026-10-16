/// \file
/// \brief [[BPR-1088]]. Proves Reviews & queue › Comment labels as the owner
/// accepted it (BPR-1079 selected design, Settings; RFC-BPR-0049 R20, R28;
/// RFC-BPR-0048's remembered authoring preference): the label helper, off by
/// default and following only what is published (since [[BPR-1565]] it sends
/// the Review feedback guidance with a workshop request and no longer touches
/// the comment editor); the Overview's
/// starting count; the Overview chips, ticked and put in order; the Mark
/// colours link; a Reset scoped to this section; what a load left out of the
/// stored chips; and every row following the published record, so a refused
/// or failed save reverts.

#include "bpr/qt/appearance_settings_panel.hpp"
#include "bpr/qt/colour_role_list.hpp"
#include "bpr/qt/feedback_label_controls.hpp"
#include "bpr/qt/feedback_label_marks.hpp"
#include "bpr/qt/feedback_label_settings.hpp"
#include "bpr/qt/info_glyph.hpp"
#include "bpr/qt/review_draft_editor.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/workbench_window.hpp"

#include <QApplication>
#include <QCheckBox>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPoint>
#include <QRect>
#include <QToolButton>

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iostream>
#include <set>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void settle()
	{
		for (auto round = 0; round < 4; ++round) {
			QApplication::processEvents();
		}
	}

	void save_render(QWidget& aWidget, const char* const aName)
	{
		const auto folder = qEnvironmentVariable("BPR_FEEDBACK_RENDER_DIR");
		if (!folder.isEmpty()) {
			static_cast<void>(aWidget.grab().save(folder + QLatin1Char{ '/' } + QString::fromLatin1(aName) + QStringLiteral(".png")));
		}
	}

	auto columns_of(std::initializer_list<core::feedback_label> aLabels) -> core::feedback_label_columns
	{
		const auto labels = std::vector<core::feedback_label>(aLabels);
		const auto made   = core::make_feedback_label_columns(labels);
		expect(made.has_value(), "the test's own label columns are a selection");
		return *made;
	}

	/// \brief A window whose dispatched records are kept, and a way to publish one.
	struct fixture
	{
		qt::workbench_window                                Window;
		std::vector<application::review_behaviour_settings> Records;

		fixture()
		{
			Window.set_behaviour_dispatch([this](const application::review_behaviour_settings& aSettings) { Records.push_back(aSettings); });
		}

		void publish(const application::review_behaviour_settings& aSettings)
		{
			auto projection      = application::workbench_projection{ };
			projection.Behaviour = aSettings;
			Window.publish(projection);
			settle();
		}

		auto find(const char* const aName) -> QToolButton* { return Window.findChild<QToolButton*>(QString::fromLatin1(aName)); }
	};

	/// \brief The rows, in the owner's order, with their words and defaults.
	void test_the_section_offers_every_row_with_safe_defaults()
	{
		fixture     f;
		auto* const page    = f.Window.settings()->page_widget(qt::settings_page::reviews_and_queue);
		auto* const helper  = f.Window.label_helper_switch();
		auto* const marks   = f.find("feedbackMarksLetters");
		auto* const open    = f.find("overviewCountsOpen");
		auto* const all     = f.find("overviewCountsAll");
		auto* const link    = f.Window.mark_colours_link();
		auto* const chips   = f.Window.label_columns_editor();
		auto* const reset   = f.find("resetCommentLabels");
		auto* const info    = dynamic_cast<qt::info_glyph*>(f.Window.findChild<QToolButton*>(QStringLiteral("labelHelperInfo")));
		auto* const heading = f.Window.findChild<QLabel*>(QStringLiteral("resetCommentLabelsHeading"));
		expect(helper != nullptr && marks != nullptr && open != nullptr && all != nullptr && link != nullptr && chips != nullptr && reset != nullptr && info != nullptr && heading != nullptr,
			   "Comment labels holds the helper, Label marks, Overview counts, Mark colours and Overview chips");
		for (QWidget* const widget : std::array<QWidget*, 6>{ helper, marks, open, link, chips, reset }) {
			expect(page->isAncestorOf(widget), "every row is on Reviews & queue");
		}
		expect(heading->text() == QStringLiteral("Comment labels"), "the section is named Comment labels");
		// [[BPR-1089]]. Open carries the round open state mark the Overview
		// draws beside its own Open; All carries none.
		expect(!open->icon().isNull() && open->toolButtonStyle() == Qt::ToolButtonTextBesideIcon && open->text() == QStringLiteral("Open") && all->icon().isNull(),
			   "Overview counts: Open shows the round open mark beside its word");
		const auto top = [page](const QWidget* aWidget) { return aWidget->mapTo(page, QPoint{ 0, 0 }).y(); };
		f.Window.settings()->resize(1'100, 900);
		f.Window.settings()->present(qt::settings_page::reviews_and_queue);
		settle();
		expect(top(helper) < top(marks) && top(marks) < top(open) && top(open) < top(link) && top(link) < top(chips),
			   "in order: Label helper, Label marks, Overview counts, Mark colours, Overview chips");
		const auto label = [&f](const char* const aName) { return f.Window.findChild<QLabel*>(QString::fromLatin1(aName)); };
		expect(label("labelHelperLabel")->text() == QStringLiteral("Label helper") && label("overviewCountsLabel")->text() == QStringLiteral("Overview counts")
				   && label("markColoursLabel")->text() == QStringLiteral("Mark colours") && label("labelColumnsLabel")->text() == QStringLiteral("Overview chips"),
			   "the row names are the design's");
		expect(info->consequence() == QStringLiteral("Sends the Review feedback guidance with each comment workshop request. Off: requests go without it. Labels count either way."),
			   "the helper's info glyph says what it does since the codes were removed, with no :shortcodes:");
		expect(!helper->isChecked(), "the helper is off by default");
		expect(open->isChecked() && !all->isChecked(), "Overview counts start on Open");
		expect(link->text() == QStringLiteral("Appearance › Colours ›") && link->toolTip() == QStringLiteral("Opens Appearance with the filter set to Label"), "Mark colours is the design's link");
		expect(reset->toolTip() == QStringLiteral("Reset comment labels") && reset->accessibleName() == QStringLiteral("Reset comment labels"), "the section's Reset says what it resets");
		expect(chips->count() == 13 && chips->columns() == core::feedback_label_columns{ } && chips->separator_item()->isHidden(), "no chip is ticked and the rule is hidden");
		for (std::size_t index = 0U; index < core::feedback_label_count; ++index) {
			const auto  each = static_cast<core::feedback_label>(index);
			auto* const row  = chips->item_of(each);
			expect(row != nullptr && chips->row(row) == static_cast<int>(index + 1U) && row->text() == qt::feedback_label_text(each) && row->checkState() == Qt::Unchecked && !row->icon().isNull(),
				   "each of the twelve labels is a row in catalog order with its mark");
		}
		expect(chips->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff && chips->height() >= chips->sizeHint().height(), "the list is as tall as its rows, so it never scrolls inside the page");
		expect(f.Records.empty(), "showing the section dispatches nothing");
		save_render(*page, "comment-labels-default");
	}

	/// \brief The switch asks the host and shows only what is published.
	void test_the_helper_switch_follows_the_published_record()
	{
		fixture f;
		f.publish({ });
		auto* const helper = f.Window.label_helper_switch();
		helper->click();
		expect(!f.Records.empty() && f.Records.back().LabelHelper == application::review_label_helper::on, "turning the switch on dispatches the whole record with the helper on");
		auto expected        = application::review_behaviour_settings{ };
		expected.LabelHelper = application::review_label_helper::on;
		expect(f.Records.back() == expected, "and nothing else changed in it");
		f.publish(f.Records.back());
		expect(helper->isChecked() && f.Window.behaviour().LabelHelper == application::review_label_helper::on, "the published choice is the window's");
		f.publish({ });
		expect(!helper->isChecked() && f.Window.behaviour().LabelHelper == application::review_label_helper::off, "published off turns it off again, so every window of the profile follows");

		// Another window of the same profile is handed the same publication.
		fixture other;
		other.publish(expected);
		expect(other.Window.label_helper_switch()->isChecked(), "a second window follows the same published record");
	}

	/// \brief A refused or failed save is answered by the held record, which the
	/// rows then show again.
	void test_every_row_reverts_to_the_published_record()
	{
		fixture f;
		auto    held        = application::review_behaviour_settings{ };
		held.LabelColumns   = columns_of({ core::feedback_label::issue });
		held.CommentsFilter = application::review_comments_filter::unread;
		f.publish(held);
		f.Window.label_helper_switch()->click();
		f.find("overviewCountsAll")->click();
		f.Window.label_columns_editor()->item_of(core::feedback_label::question)->setCheckState(Qt::Checked);
		settle();
		expect(f.Records.size() == 3U && f.Records.back().LabelHelper == application::review_label_helper::on && f.Records.back().OverviewCounts == application::review_overview_counts::all
				   && f.Records.back().LabelColumns == columns_of({ core::feedback_label::issue, core::feedback_label::question }),
			   "three gestures dispatch three records, each building on the last");
		expect(f.Records.back().CommentsFilter == application::review_comments_filter::unread, "the Comments drawer's own filter is carried untouched");
		f.publish(held);
		expect(!f.Window.label_helper_switch()->isChecked() && f.find("overviewCountsOpen")->isChecked() && f.Window.label_columns_editor()->columns() == held.LabelColumns
				   && f.Window.label_columns_editor()->item_of(core::feedback_label::question)->checkState() == Qt::Unchecked,
			   "the held record, republished after a refusal or a failed write, puts every row back");
	}

	/// \brief Ticking, ordering and unticking the Overview chips.
	void test_the_overview_chips_are_ticked_and_ordered()
	{
		fixture     f;
		auto* const chips = f.Window.label_columns_editor();
		f.publish({ });
		chips->item_of(core::feedback_label::note)->setCheckState(Qt::Checked);
		settle();
		chips->item_of(core::feedback_label::note)->setCheckState(Qt::Unchecked);
		settle();
		expect(f.Records.size() == 2U && f.Records[0].LabelColumns == columns_of({ core::feedback_label::note }) && f.Records[1].LabelColumns == core::feedback_label_columns{ },
			   "unticking before the first tick is answered is still sent, though it equals the published record");
		f.publish({ });
		f.Records.clear();
		chips->item_of(core::feedback_label::issue)->setCheckState(Qt::Checked);
		chips->item_of(core::feedback_label::question)->setCheckState(Qt::Checked);
		settle();
		expect(f.Records.size() == 2U && f.Records[0].LabelColumns == columns_of({ core::feedback_label::issue })
				   && f.Records[1].LabelColumns == columns_of({ core::feedback_label::issue, core::feedback_label::question }),
			   "a ticked label joins the end of the chips");
		f.publish(f.Records.back());
		expect(chips->item(0) == chips->item_of(core::feedback_label::issue) && chips->item(1) == chips->item_of(core::feedback_label::question)
				   && chips->item(2) == chips->separator_item() && !chips->separator_item()->isHidden() && chips->item(3) == chips->item_of(core::feedback_label::praise),
			   "ticked rows come first in order, then the rule, then the rest in catalog order");
		f.Window.settings()->resize(1'100, 900);
		f.Window.settings()->present(qt::settings_page::reviews_and_queue);
		settle();
		save_render(*f.Window.settings()->page_widget(qt::settings_page::reviews_and_queue), "comment-labels-two-chips");
		chips->setFocus();
		chips->setCurrentItem(chips->item_of(core::feedback_label::issue));
		auto altDown = QKeyEvent{ QEvent::KeyPress, Qt::Key_Down, Qt::AltModifier };
		QApplication::sendEvent(chips, &altDown);
		settle();
		expect(f.Records.back().LabelColumns == columns_of({ core::feedback_label::question, core::feedback_label::issue }), "Alt+Down moves the current chip one place later");
		expect(chips->currentItem() == chips->item_of(core::feedback_label::issue) && chips->item(1) == chips->item_of(core::feedback_label::issue), "the moved row stays current, in its new place");
		auto altUp = QKeyEvent{ QEvent::KeyPress, Qt::Key_Up, Qt::AltModifier };
		QApplication::sendEvent(chips, &altUp);
		settle();
		expect(f.Records.back().LabelColumns == columns_of({ core::feedback_label::issue, core::feedback_label::question }), "Alt+Up moves it back");
		const auto before = f.Records.size();
		QApplication::sendEvent(chips, &altUp);
		settle();
		expect(f.Records.size() == before, "the first chip cannot move earlier, and nothing is dispatched");
		chips->setCurrentItem(chips->item_of(core::feedback_label::praise));
		QApplication::sendEvent(chips, &altUp);
		settle();
		expect(f.Records.size() == before, "an unticked row has no place to move to");
		chips->item_of(core::feedback_label::issue)->setCheckState(Qt::Unchecked);
		settle();
		expect(f.Records.back().LabelColumns == columns_of({ core::feedback_label::question }), "unticking removes a chip and keeps the others' order");

		// All twelve, in an order of the reviewer's own.
		auto all   = application::review_behaviour_settings{ };
		auto order = std::array<core::feedback_label, core::feedback_label_count>{ };
		for (std::size_t index = 0U; index < order.size(); ++index) {
			order[index] = static_cast<core::feedback_label>((index * 7U + 2U) % core::feedback_label_count);
		}
		all.LabelColumns = *core::make_feedback_label_columns(order);
		f.publish(all);
		expect(chips->columns() == all.LabelColumns && chips->separator_item()->isHidden(), "all twelve show in the published order, with no rule under them");
		for (std::size_t index = 0U; index < order.size(); ++index) {
			expect(chips->item(static_cast<int>(index)) == chips->item_of(order[index]), "each ticked row in its place");
		}
		expect(chips->row_tooltip(*chips->item_of(order[0])).startsWith(qt::feedback_label_description(order[0]))
				   && chips->row_tooltip(*chips->item_of(order[0])).endsWith(qt::feedback_label_columns_editor::reorder_hint()),
			   "a ticked row's tooltip is its description and how to reorder it");
		expect(qt::feedback_label_columns_editor::reorder_hint().startsWith(QStringLiteral("Drag to reorder (")), "the design's reorder words");
		auto repeated                   = all;
		repeated.LabelColumns.Labels[1] = repeated.LabelColumns.Labels[0];
		f.publish(repeated);
		expect(chips->columns() == all.LabelColumns, "a record naming a label twice is never shown");
	}

	/// \brief Reset returns this section only to its defaults.
	void test_reset_covers_this_section_only()
	{
		fixture f;
		auto    chosen        = application::review_behaviour_settings{ };
		chosen.OpenModality   = application::review_open_modality::review;
		chosen.CommentsFilter = application::review_comments_filter::unresolved;
		chosen.DrawerControls = application::review_drawer_controls::shown;
		chosen.FeedbackMarks  = application::review_feedback_marks::letters;
		chosen.LabelHelper    = application::review_label_helper::on;
		chosen.OverviewCounts = application::review_overview_counts::all;
		chosen.LabelColumns   = columns_of({ core::feedback_label::todo, core::feedback_label::issue });
		f.publish(chosen);
		f.find("resetCommentLabels")->click();
		auto expected           = chosen;
		expected.FeedbackMarks  = application::review_feedback_marks::icons;
		expected.LabelHelper    = application::review_label_helper::off;
		expected.OverviewCounts = application::review_overview_counts::open;
		expected.LabelColumns   = { };
		expect(!f.Records.empty() && f.Records.back() == expected, "Reset dispatches the label defaults and keeps every other preference, the drawer filter included");
		f.Records.clear();
		f.find("resetReadingBehaviour")->click();
		expect(!f.Records.empty() && f.Records.back().LabelColumns == chosen.LabelColumns && f.Records.back().LabelHelper == chosen.LabelHelper
				   && f.Records.back().OverviewCounts == chosen.OverviewCounts,
			   "another group's Reset leaves the label preferences alone");
	}

	/// \brief What a load left out of the stored chips is said, with how to
	/// save a clean list, until the reviewer's edit.
	void test_a_recovered_list_is_explained()
	{
		fixture f;
		auto    loaded              = application::review_behaviour_settings{ };
		loaded.LabelColumns         = columns_of({ core::feedback_label::issue, core::feedback_label::note });
		loaded.LabelColumnsRecovery = application::review_label_columns_recovery{ .Unknown = 1U, .Repeated = 2U };
		f.publish(loaded);
		auto* const notice = f.Window.label_columns_recovery();
		expect(notice != nullptr && !notice->isHidden(), "the Overview chips row says what was left out");
		expect(notice->text() == QStringLiteral("Some saved chips were left out: 1 unknown label and 2 repeats. The rest keep their order. Change a chip or Reset to save this list."),
			   "how many, and how to save a clean list");
		expect(f.Window.label_columns_editor()->columns() == loaded.LabelColumns, "the valid chips show in their saved order");
		f.Window.label_columns_editor()->item_of(core::feedback_label::praise)->setCheckState(Qt::Checked);
		settle();
		expect(!f.Records.empty() && !f.Records.back().LabelColumnsRecovery.has_value(), "an edit asks for preferences only, carrying no load report");
		f.publish(f.Records.back());
		expect(notice->isHidden(), "once saved, the notice is gone");
		expect(qt::feedback_label_columns_recovery_text(2U, 0U) == QStringLiteral("Some saved chips were left out: 2 unknown labels. The rest keep their order. Change a chip or Reset to save this list.")
				   && qt::feedback_label_columns_recovery_text(0U, 1U).contains(QStringLiteral(": 1 repeat.")),
			   "the counts read naturally alone");
	}

	/// \brief Mark colours opens Appearance filtered to the label tints.
	void test_mark_colours_opens_the_label_tints()
	{
		fixture f;
		f.Window.settings()->present(qt::settings_page::reviews_and_queue);
		settle();
		f.Window.mark_colours_link()->click();
		settle();
		auto* const colours = f.Window.appearance_settings()->colours();
		expect(f.Window.settings()->current_page() == qt::settings_page::appearance, "Appearance opens");
		expect(colours->filter_field()->text() == QStringLiteral("label"), "with its colour filter set to label");
		auto listed = colours->listed_roles();
		auto tints  = std::vector<core::appearance_role>{ };
		for (std::size_t index = 0U; index < core::feedback_label_count; ++index) {
			tints.push_back(qt::feedback_label_role(static_cast<core::feedback_label>(index)));
		}
		std::ranges::sort(listed);
		std::ranges::sort(tints);
		expect(listed == tints, "listing exactly the twelve label tints");
	}

	/// \brief At Settings' minimum size the list fits and drops the descriptions.
	void test_the_section_fits_at_the_minimum_size()
	{
		fixture f;
		f.Window.settings()->resize(760, 560);
		f.Window.settings()->present(qt::settings_page::reviews_and_queue);
		settle();
		auto* const page  = f.Window.settings()->page_widget(qt::settings_page::reviews_and_queue);
		auto* const chips = f.Window.label_columns_editor();
		const auto  box   = QRect{ chips->mapTo(page, QPoint{ 0, 0 }), chips->size() };
		expect(box.left() >= 0 && box.right() <= page->width() && chips->width() >= chips->minimumSizeHint().width(), "the chips list fits the page at 760 by 560");
		save_render(*page, "comment-labels-narrow");
		f.Window.settings()->resize(1'100, 900);
		settle();
		expect(chips->descriptions_shown() == (chips->width() >= qt::feedback_label_columns_editor::description_width), "descriptions show only where the list is wide enough");
		// Each row's phrase is the design's short one, whole beside its ID at
		// the narrowest width that shows it (grip, check and mark take 70 px).
		const auto metrics = QFontMetrics{ chips->font() };
		auto       phrases = std::set<QString>{ };
		for (std::size_t index = 0U; index < core::feedback_label_count; ++index) {
			const auto label  = static_cast<core::feedback_label>(index);
			const auto phrase = qt::feedback_label_phrase(label);
			phrases.insert(phrase);
			expect(!phrase.isEmpty() && phrase != qt::feedback_label_description(label)
					   && 70 + metrics.horizontalAdvance(qt::feedback_label_text(label)) + 12 + metrics.horizontalAdvance(phrase) + 6 <= qt::feedback_label_columns_editor::description_width,
				   "each label's row phrase is short and fits whole beside its ID");
		}
		expect(phrases.size() == core::feedback_label_count, "the twelve phrases differ");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	test_the_section_offers_every_row_with_safe_defaults();
	test_the_helper_switch_follows_the_published_record();
	test_every_row_reverts_to_the_published_record();
	test_the_overview_chips_are_ticked_and_ordered();
	test_reset_covers_this_section_only();
	test_a_recovered_list_is_explained();
	test_mark_colours_opens_the_label_tints();
	test_the_section_fits_at_the_minimum_size();
	std::cout << "feedback-label-settings-tests:pass\n";
	return EXIT_SUCCESS;
}
