/// \file
/// \brief Proves RFC-BPR-0034's Colours list: 62 roles, five groups, live picking.
///
/// \details Criterion C5's list half. The inventory is every role the core
/// declares except the two hunk-header roles the card no longer offers (C5
/// amended from 47 to 45 on 2026-09-24, [[BPR-0986]]; then to 57 when
/// [[BPR-1083]] added the twelve label tints as a fifth group, to 58 when
/// [[BPR-1085]] added the attention amber to Review states, and to 60 when
/// RFC-BPR-0072 added the namespace and enum-member colours to Syntax,
/// [[BPR-1483]], and to 62 when [[BPR-1503]] added Find's two washes), in five groups with sticky headers carrying counted totals;
/// the filter lists what it matches; a swatch opens the picker through the
/// settable seam and every colour the picker moves through is reported, so the
/// review window can preview as the reviewer picks; a cancelled pick reports the
/// colour the role started from; and an edited row shows its new value with the
/// old one beside it.

#include "bpr/core/appearance_model.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/appearance_role_display.hpp"
#include "bpr/qt/colour_role_list.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QFont>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QScrollBar>

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace {
	using namespace bpr;
	using role  = core::appearance_role;
	using group = qt::appearance_role_group;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	void settle()
	{
		QApplication::processEvents();
		QApplication::sendPostedEvents();
		QApplication::processEvents();
	}

	/// \brief Records every edit the list reports.
	class recorder final
	{
	public:
		[[nodiscard]] auto sink() -> qt::colour_edit_dispatch
		{
			return [this](const role aRole, const core::appearance_color aColour) {
				Edits.emplace_back(aRole, aColour);
			};
		}

		[[nodiscard]] auto count() const noexcept -> std::size_t
		{
			return Edits.size();
		}

		[[nodiscard]] auto last() const
			-> std::pair<role, core::appearance_color>
		{
			return Edits.back();
		}

		[[nodiscard]] auto at(const std::size_t aIndex) const
			-> std::pair<role, core::appearance_color>
		{
			return Edits[aIndex];
		}

		void clear()
		{
			Edits.clear();
		}

	private:
		std::vector<std::pair<role, core::appearance_color>> Edits;
	};

	/// \brief A picker a course can drive, standing in for the platform panel.
	class scripted_picker final
	{
	public:
		[[nodiscard]] auto seam() -> qt::colour_picker
		{
			return [this](const qt::colour_pick_request& aRequest)
					   -> std::optional<QColor> {
				++Openings;
				Title   = aRequest.Title;
				Initial = aRequest.Initial;
				for (const auto& moved : Moves) {
					aRequest.Changed(moved);
				}
				return Accepted;
			};
		}

		std::vector<QColor>   Moves;
		std::optional<QColor> Accepted;
		std::size_t           Openings = 0U;
		QString               Title;
		QColor                Initial;
	};

	[[nodiscard]] auto dark_palette() -> core::appearance_palette
	{
		return qt::builtin_appearance_palette(core::appearance_variant::dark);
	}

	void show(qt::colour_role_list& aList)
	{
		aList.resize(520, 360);
		aList.show();
		settle();
	}

	/// \brief Every offered role must be listed, grouped, and counted.
	void test_the_inventory_is_the_offered_vocabulary()
	{
		auto list = qt::colour_role_list{ };
		list.set_theme_colours(dark_palette());
		show(list);

		expect(
			list.listed_roles().size() == 62U && core::appearance_role_count == 64U,
			"62 of the 64 roles the core declares are listed");
		for (const auto unoffered :
			 { role::diff_hunk_header_background, role::diff_hunk_header_text }) {
			expect(
				!qt::appearance_role_offered(unoffered)
					&& list.row_of(unoffered) == nullptr
					&& list.swatch_of(unoffered) == nullptr
					&& list.value_text(unoffered).isEmpty(),
				"the two hunk-header roles, whose surface the review window does not "
				"have, get no row");
		}
		// By group, and within a group in the core's own order: a role declared
		// at the end for its durable ordinal lists in its own group
		// ([[BPR-1085]]: the attention amber under Review states).
		auto expectedOrder = std::vector<role>{ };
		for (auto group = std::size_t{ 0U }; group < qt::appearance_role_group_count; ++group) {
			for (auto index = std::size_t{ 0U }; index < core::appearance_role_count; ++index) {
				const auto candidate = static_cast<role>(static_cast<std::uint8_t>(index));
				if (qt::appearance_role_offered(candidate) && static_cast<std::size_t>(qt::appearance_role_group_of(candidate)) == group) {
					expectedOrder.push_back(candidate);
				}
			}
		}
		expect(list.listed_roles() == expectedOrder,
			   "the rows are grouped, each group in the core's own order, so the list cannot reorder "
			   "the vocabulary within a group");
		const auto stale     = list.row_of(role::state_stale)->geometry();
		const auto attention = list.row_of(role::state_attention)->geometry();
		const auto normal    = list.row_of(role::syntax_normal)->geometry();
		expect(stale.bottom() < attention.top() && attention.bottom() < list.group_header_of(group::syntax)->geometry().top()
				   && attention.bottom() < normal.top(),
			   "the attention amber's row sits directly under State stale, inside Review states");
		// RFC-BPR-0072's two syntax colours ([[BPR-1483]]) are offered as every
		// other syntax role is, worded as the rest of Syntax is, and, declared
		// last for their durable ordinals, list at the end of Syntax, above
		// Comment labels.
		const auto other      = list.row_of(role::syntax_other)->geometry();
		const auto namespaces = list.row_of(role::syntax_namespace)->geometry();
		const auto members    = list.row_of(role::syntax_enum_member)->geometry();
		expect(qt::appearance_role_group_of(role::syntax_namespace) == group::syntax
				   && qt::appearance_role_group_of(role::syntax_enum_member) == group::syntax
				   && list.name_label_of(role::syntax_namespace)->text() == QStringLiteral("Syntax namespace")
				   && list.name_label_of(role::syntax_enum_member)->text() == QStringLiteral("Syntax enum member"),
			   "the namespace and enum-member colours are listed in Syntax, worded like its other rows");
		expect(other.bottom() < namespaces.top() && namespaces.bottom() < members.top()
				   && members.bottom() < list.group_header_of(group::comment_labels)->geometry().top(),
			   "their rows follow Syntax other and end the Syntax group");
		for (const auto listed : expectedOrder) {
			expect(
				list.name_label_of(listed)->text()
					== qt::appearance_role_text(listed),
				"each row carries the role's name in words");
			const auto durable = core::stable_name(listed);
			expect(
				list.name_label_of(listed)->text()
					!= QString::fromUtf8(
						durable.data(), static_cast<qsizetype>(durable.size())),
				"and never its stable exchange identifier");
			expect(
				list.swatch_of(listed) != nullptr
					&& !list.swatch_of(listed)->icon().isNull()
					&& list.swatch_of(listed)->focusPolicy() == Qt::StrongFocus,
				"each row carries a swatch that is reachable by keyboard");
			expect(
				list.value_text(listed)
					== qt::to_presentation_color(
						   dark_palette()[core::index_of(listed)])
						   .name(QColor::HexRgb),
				"and its value, as the theme's own colour");
		}

		expect(
			list.group_heading_text(group::surfaces_and_text)
					== QStringLiteral("Surfaces and text")
				&& list.group_count_text(group::surfaces_and_text)
					== QStringLiteral("9")
				&& list.group_count_text(group::diff) == QStringLiteral("6")
				&& list.group_count_text(group::review_states)
					== QStringLiteral("7")
				&& list.group_count_text(group::syntax) == QStringLiteral("28")
				&& list.group_heading_text(group::comment_labels) == QStringLiteral("Comment labels")
				&& list.group_count_text(group::comment_labels) == QStringLiteral("12"),
			"the five group headers carry the accepted counts");

		expect(
			list.well() != nullptr
				&& list.well()->verticalScrollBar()->maximum() > 0,
			"the rows scroll inside the list rather than lengthening the page");
	}

	/// \brief The value column must be the code font the composition supplied.
	void test_values_are_set_in_the_code_font()
	{
		auto list = qt::colour_role_list{ };
		list.set_theme_colours(dark_palette());
		auto code = QFont{ QStringLiteral("Fixture Mono") };
		code.setPointSize(11);
		list.set_value_font(code);
		expect(
			list.value_label_of(role::code_background)->font().family()
					== QStringLiteral("Fixture Mono")
				&& list.previous_value_label_of(role::code_background)
						->font()
						.family()
					== QStringLiteral("Fixture Mono"),
			"a role's value, and the old value beside it, are set in the code font "
			"the design asks for");
	}

	/// \brief The filter must list what it matches, and nothing else.
	void test_the_filter_lists_what_it_matches()
	{
		auto list = qt::colour_role_list{ };
		list.set_theme_colours(dark_palette());
		show(list);

		list.filter_field()->setText(QStringLiteral("in-line"));
		settle();
		const auto listed = list.listed_roles();
		expect(
			listed.size() == 2U && listed[0] == role::diff_intraline_added_background
				&& listed[1] == role::diff_intraline_removed_background,
			"the filter lists exactly the roles whose words it matches");
		expect(
			list.group_count_text(group::diff) == QStringLiteral("2"),
			"a group's count states what it lists now rather than a fixed total");
		expect(
			list.group_header_of(group::syntax)->isHidden()
				&& !list.group_header_of(group::diff)->isHidden(),
			"a group the filter empties shows no heading, so the list never reads "
			"as a group with nothing in it");
		expect(
			list.row_of(role::syntax_keyword)->isHidden(),
			"and its rows are gone from the list rather than merely quiet");

		list.filter_field()->setText(QStringLiteral("In-line"));
		settle();
		expect(
			list.listed_roles().size() == 2U,
			"the filter is case-insensitive and matches the accepted wording, so "
			"searching for in-line finds the intraline roles");

		list.filter_field()->setText(QStringLiteral("hunk"));
		settle();
		expect(
			list.listed_roles().empty() && list.group_header_of(group::diff)->isHidden(),
			"a role the card does not offer is not found by the filter either");

		list.filter_field()->clear();
		settle();
		expect(
			list.listed_roles().size() == 62U
				&& list.group_count_text(group::syntax) == QStringLiteral("28")
				&& list.group_count_text(group::review_states) == QStringLiteral("7"),
			"clearing the filter lists every offered role again");

		// [[BPR-1083]]. Settings' "Mark colours" row opens Appearance filtered
		// to "label" (BPR-1088); the filter must find exactly the twelve tints.
		list.filter_field()->setText(QStringLiteral("label"));
		settle();
		expect(
			list.listed_roles().size() == 12U && list.listed_roles().front() == role::feedback_label_praise
				&& list.listed_roles().back() == role::feedback_label_quibble
				&& list.group_count_text(group::comment_labels) == QStringLiteral("12")
				&& list.group_header_of(group::syntax)->isHidden(),
			"filtering for label lists the twelve label tints in catalog order and nothing else");

		// [[BPR-1483]]. Every Syntax row is worded "Syntax …", the two
		// RFC-BPR-0072 added included, so the filter finds the whole group.
		list.filter_field()->setText(QStringLiteral("syntax"));
		settle();
		const auto syntax = list.listed_roles();
		expect(syntax.size() == 28U && list.group_count_text(group::syntax) == QStringLiteral("28")
				   && syntax[26] == role::syntax_namespace && syntax[27] == role::syntax_enum_member
				   && list.group_header_of(group::comment_labels)->isHidden(),
			   "filtering for syntax lists all 28 Syntax rows, namespace and enum member last, and nothing else");
		list.filter_field()->clear();
		settle();
	}

	/// \brief A group header must pin itself once its own header scrolls away.
	void test_group_headers_stick()
	{
		auto list = qt::colour_role_list{ };
		list.set_theme_colours(dark_palette());
		show(list);

		auto* const bar = list.well()->verticalScrollBar();
		expect(
			list.sticky_header()->isHidden(),
			"at the top of the list the first group's own header is the one in "
			"view, so nothing is pinned over it");

		// Far enough that the first group's header has left the viewport.
		bar->setValue(bar->maximum());
		settle();
		expect(
			!list.sticky_header()->isHidden()
				&& list.sticky_header()->y() == 0
				&& list.sticky_heading_text() == QStringLiteral("Comment labels"),
			"scrolled to the last group, its heading is pinned at the top of the "
			"list");
		expect(
			list.sticky_header()->width() == list.well()->viewport()->width(),
			"the pinned heading spans the list, so no row shows through beside it");

		bar->setValue(0);
		settle();
		expect(
			list.sticky_header()->isHidden(),
			"scrolling back to the top unpins it again");
	}

	/// \brief A swatch must open the picker and report every colour it moves through.
	void test_a_swatch_previews_through_the_picker_as_it_moves()
	{
		auto recorded = recorder{ };
		auto picker   = scripted_picker{ };
		auto list     = qt::colour_role_list{ };
		list.set_theme_colours(dark_palette());
		list.set_dispatch(recorded.sink());
		list.set_picker(picker.seam());
		show(list);

		picker.Moves    = { QColor{ 0x11, 0x22, 0x33 }, QColor{ 0x44, 0x55, 0x66 } };
		picker.Accepted = QColor{ 0x1a, 0x4a, 0x2e };
		list.swatch_of(role::diff_added_background)->click();

		expect(
			picker.Openings == 1U
				&& picker.Title == QStringLiteral("Diff added background"),
			"the swatch opens the picker once, and the picker is told which colour "
			"in words");
		expect(
			picker.Initial
				== qt::to_presentation_color(
					dark_palette()[core::index_of(role::diff_added_background)]),
			"the picker starts from the colour the role shows now");
		expect(
			recorded.count() == 3U,
			"every colour the picker moved through is reported, which is what lets "
			"the review window preview as the reviewer picks");
		expect(
			recorded.at(0).first == role::diff_added_background
				&& recorded.at(0).second
					== core::appearance_color{ 0x11U, 0x22U, 0x33U, 255U }
				&& recorded.at(1).second
					== core::appearance_color{ 0x44U, 0x55U, 0x66U, 255U }
				&& recorded.last().second
					== core::appearance_color{ 0x1aU, 0x4aU, 0x2eU, 255U },
			"each report names the edited role and the exact colour, in order");
		expect(
			list.statistics().PickersOpened == 1U
				&& list.statistics().LiveChanges == 2U
				&& list.statistics().AcceptedPicks == 1U
				&& list.statistics().CancelledPicks == 0U,
			"the live changes and the accepted pick are counted apart");

		// The list holds no palette authority: nothing is edited until whoever owns
		// the palette says so.
		expect(
			!list.edited(role::diff_added_background)
				&& list.edited_roles() == 0U,
			"the list applies no colour of its own; a row reads as edited only once "
			"the previewed palette it was given says so");
	}

	/// \brief A cancelled pick must put the role back where it started.
	void test_a_cancelled_pick_reports_the_colour_it_started_from()
	{
		auto recorded = recorder{ };
		auto picker   = scripted_picker{ };
		auto list     = qt::colour_role_list{ };
		list.set_theme_colours(dark_palette());
		list.set_dispatch(recorded.sink());
		list.set_picker(picker.seam());
		show(list);

		const auto before
			= dark_palette()[core::index_of(role::syntax_keyword)];
		picker.Moves    = { QColor{ 0xd4, 0x8a, 0xe9 } };
		picker.Accepted = std::nullopt;
		list.swatch_of(role::syntax_keyword)->click();

		expect(
			recorded.count() == 2U && recorded.last().first == role::syntax_keyword
				&& recorded.last().second == before,
			"a cancelled picker reports the colour the role had before it opened, "
			"so a colour nobody accepted is not left previewed");
		expect(
			list.statistics().CancelledPicks == 1U
				&& list.statistics().AcceptedPicks == 0U,
			"and the cancellation is counted as one");
	}

	/// \brief An edited row must show its new value and the old one beside it.
	void test_an_edited_row_shows_new_and_old()
	{
		auto list = qt::colour_role_list{ };
		list.set_theme_colours(dark_palette());
		show(list);

		auto previewed = dark_palette();
		previewed[core::index_of(role::diff_added_background)]
			= core::appearance_color{ 0x1aU, 0x4aU, 0x2eU, 255U };
		list.set_previewed_colours(previewed);

		const auto edited = role::diff_added_background;
		expect(
			list.edited(edited) && list.edited_roles() == 1U,
			"exactly the role whose previewed colour differs reads as edited");
		expect(
			list.value_text(edited) == QStringLiteral("#1a4a2e"),
			"the edited row shows its new value");
		expect(
			list.previous_value_label_of(edited)->isVisible()
				&& list.previous_value_label_of(edited)->text()
					== QStringLiteral("was #153622"),
			"with the old one beside it, in the form the board shows");
		expect(
			list.value_label_of(edited)->foregroundRole() == QPalette::WindowText
				&& list.name_label_of(edited)->font().weight() == QFont::DemiBold,
			"the new value is in ink rather than quiet, which is the non-colour "
			"mark that the row was edited");
		expect(
			list.swatch_of(edited)->accessibleDescription().contains(
				QStringLiteral("was #153622")),
			"and a reviewer who cannot see the row is told both values too");

		const auto untouched = role::diff_removed_background;
		expect(
			!list.edited(untouched)
				&& list.previous_value_label_of(untouched)->isHidden()
				&& list.value_label_of(untouched)->foregroundRole()
					== QPalette::PlaceholderText,
			"an unedited row says nothing about a previous value");

		list.set_previewed_colours(std::nullopt);
		expect(
			list.edited_roles() == 0U
				&& list.previous_value_label_of(edited)->isHidden()
				&& list.value_text(edited) == QStringLiteral("#153622"),
			"dropping the preview restores the adopted value and the row's plain "
			"reading");
	}

	/// \brief A colour carrying an alpha must say so.
	void test_a_transparent_colour_states_its_alpha()
	{
		auto list  = qt::colour_role_list{ };
		auto theme = dark_palette();
		theme[core::index_of(role::code_selection_background)]
			= core::appearance_color{ 0x28U, 0x51U, 0x8cU, 0x48U };
		list.set_theme_colours(theme);
		expect(
			list.value_text(role::code_selection_background)
				== QStringLiteral("#48 28518c").remove(QLatin1Char{ ' ' }),
			"a colour with an alpha shows all eight digits, so a value cannot hide "
			"its own transparency");
		expect(
			list.value_text(role::code_background) == QStringLiteral("#0d1117"),
			"and an opaque colour still shows six");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setApplicationName(
		QStringLiteral("BetterPullReviewColourRoleListTests"));
	test_the_inventory_is_the_offered_vocabulary();
	test_values_are_set_in_the_code_font();
	test_the_filter_lists_what_it_matches();
	test_group_headers_stick();
	test_a_swatch_previews_through_the_picker_as_it_moves();
	test_a_cancelled_pick_reports_the_colour_it_started_from();
	test_an_edited_row_shows_new_and_old();
	test_a_transparent_colour_states_its_alpha();
	std::cout << "colour-role-list-tests:pass\n";
	return EXIT_SUCCESS;
}
