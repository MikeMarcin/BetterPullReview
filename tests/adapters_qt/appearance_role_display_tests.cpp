/// \file
/// \brief Proves the Colours list's role vocabulary is total, grouped and its own.
///
/// \details RFC-BPR-0034's C5 lists all 47 roles in four groups with counts. The
/// vocabulary is presentation's: `core::stable_name` is the durable exchange
/// identifier a theme document keys on and must never be shown, so this course
/// checks that no displayed string is one, that every declared role has words and
/// a group, and that the four groups hold the accepted 7, 8, 6 and 26 roles.

#include "bpr/core/appearance_model.hpp"
#include "bpr/qt/appearance_role_display.hpp"

#include <QString>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {
	using namespace bpr;

	void expect(const bool aCondition, const std::string_view aMessage)
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto role_at(const std::size_t aIndex) -> core::appearance_role
	{
		return static_cast<core::appearance_role>(
			static_cast<std::uint8_t>(aIndex));
	}

	/// \brief Every role the core declares must have words and a group.
	void test_every_role_has_words()
	{
		for (auto index = std::size_t{ 0U };
			 index < core::appearance_role_count;
			 ++index) {
			const auto role = role_at(index);
			const auto text = qt::appearance_role_text(role);
			expect(
				!text.isEmpty(),
				"every role the core declares has a name in words");
			expect(
				text.front().isUpper(),
				"a role's words read as a sentence rather than as an identifier");
			expect(
				!text.contains(QLatin1Char{ '_' }),
				"a role's words carry no underscore, which is what told the reader "
				"the old page was showing a stable identifier");
			expect(
				!qt::appearance_role_group_text(
					 qt::appearance_role_group_of(role))
					 .isEmpty(),
				"and lists in a group that has a heading");
		}
	}

	/// \brief The displayed wording must never be the durable exchange identifier.
	void test_no_displayed_name_is_a_stable_name()
	{
		for (auto shown = std::size_t{ 0U }; shown < core::appearance_role_count;
			 ++shown) {
			const auto text = qt::appearance_role_text(role_at(shown));
			for (auto durable = std::size_t{ 0U };
				 durable < core::appearance_role_count;
				 ++durable) {
				const auto name = core::stable_name(role_at(durable));
				expect(
					text
						!= QString::fromUtf8(
							name.data(), static_cast<qsizetype>(name.size())),
					"no displayed role name is any role's stable exchange "
					"identifier, so the wording can change without moving a "
					"theme document's keys");
			}
		}
	}

	/// \brief The four groups must hold exactly the accepted counts, and all 47.
	void test_the_four_groups_hold_every_role()
	{
		auto counted = std::array<std::size_t, qt::appearance_role_group_count>{ };
		for (auto index = std::size_t{ 0U };
			 index < core::appearance_role_count;
			 ++index) {
			++counted[static_cast<std::size_t>(
				qt::appearance_role_group_of(role_at(index)))];
		}

		// The counts the accepted design prints beside each sticky header.
		const auto accepted = std::array<std::size_t, qt::appearance_role_group_count>{
			7U, 8U, 6U, 26U
		};
		expect(counted == accepted, "the groups hold 7, 8, 6 and 26 roles");

		auto total = std::size_t{ 0U };
		for (const auto count : counted) {
			total += count;
		}
		expect(
			total == core::appearance_role_count && total == 47U,
			"and together they hold every one of the 47 roles, so no role can be "
			"grouped out of the list");
	}

	/// \brief The wording the owner accepted, where it differs from the identifier.
	void test_the_accepted_wording_is_used()
	{
		expect(
			qt::appearance_role_text(
				core::appearance_role::diff_intraline_added_background)
					== QStringLiteral("Diff in-line added background")
				&& qt::appearance_role_text(
					   core::appearance_role::diff_intraline_removed_background)
					== QStringLiteral("Diff in-line removed background"),
			"the intraline roles read as the board renders them, in-line rather "
			"than intraline");
		expect(
			qt::appearance_role_text(core::appearance_role::application_surface)
					== QStringLiteral("Application surface")
				&& qt::appearance_role_text(core::appearance_role::syntax_other)
					== QStringLiteral("Syntax other"),
			"the first and last roles of the vocabulary read as the board lists "
			"them");
		expect(
			qt::appearance_role_group_text(
				qt::appearance_role_group::surfaces_and_text)
					== QStringLiteral("Surfaces and text")
				&& qt::appearance_role_group_text(qt::appearance_role_group::diff)
					== QStringLiteral("Diff")
				&& qt::appearance_role_group_text(
					   qt::appearance_role_group::review_states)
					== QStringLiteral("Review states")
				&& qt::appearance_role_group_text(qt::appearance_role_group::syntax)
					== QStringLiteral("Syntax"),
			"the four headings are the accepted four");
		expect(
			qt::appearance_role_group_of(core::appearance_role::application_surface)
					== qt::appearance_role_group::surfaces_and_text
				&& qt::appearance_role_group_of(
					   core::appearance_role::code_current_line_background)
					== qt::appearance_role_group::surfaces_and_text,
			"the core's chrome and canvas comment groups are one group here, as "
			"the design merges them");
	}
} // namespace

auto main() -> int
{
	test_every_role_has_words();
	test_no_displayed_name_is_a_stable_name();
	test_the_four_groups_hold_every_role();
	test_the_accepted_wording_is_used();
	std::cout << "appearance-role-display-tests:pass\n";
	return EXIT_SUCCESS;
}
