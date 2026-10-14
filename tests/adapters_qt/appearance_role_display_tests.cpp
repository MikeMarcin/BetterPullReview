/// \file
/// \brief Proves the Colours list's role vocabulary is total, grouped and its own.
///
/// \details RFC-BPR-0034's C5 lists the roles in five groups with counts. The
/// vocabulary is presentation's: `core::stable_name` is the durable exchange
/// identifier a theme document keys on and must never be shown, so this course
/// checks that no displayed string is one, that every declared role has words and
/// a group, and that the five groups hold the declared 7, 8, 7, 28 and 12 roles.

#include "bpr/core/appearance_model.hpp"
#include "bpr/qt/appearance_role_display.hpp"

#include <QString>

#include <array>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <vector>

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

	/// \brief The five groups must hold exactly the accepted counts, and all 62.
	///
	/// \details [[BPR-1085]] adds the attention amber to Review states.
	void test_the_five_groups_hold_every_role()
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
			7U, 8U, 7U, 28U, 12U
		};
		expect(counted == accepted, "the groups hold 7, 8, 7, 28 and 12 roles");

		auto total = std::size_t{ 0U };
		for (const auto count : counted) {
			total += count;
		}
		expect(
			total == core::appearance_role_count && total == 62U,
			"and together they hold every one of the 62 roles, so no role can be "
			"grouped out of the list");
	}

	/// \brief The card offers 60 roles: every one but the two hunk-header roles.
	///
	/// \details [[BPR-0986]]. RFC-BPR-0034 C5 was amended from 47 offered roles
	/// to 45 on 2026-09-24, because the review window has no hunk header for the
	/// two roles to change. They stay in the vocabulary and in their group.
	/// [[BPR-1083]] added the twelve label tints as a fifth group, Comment
	/// labels, all offered: 57 (owner, BPR-1079 `tints-v002`). [[BPR-1085]]
	/// added the attention amber to Review states: 58. RFC-BPR-0072 added the
	/// namespace and enum-member colours to Syntax: 60.
	void test_the_card_offers_every_role_but_the_hunk_header()
	{
		auto offered   = std::array<std::size_t, qt::appearance_role_group_count>{ };
		auto withdrawn = std::vector<core::appearance_role>{ };
		for (auto index = std::size_t{ 0U }; index < core::appearance_role_count;
			 ++index) {
			const auto role = role_at(index);
			if (qt::appearance_role_offered(role)) {
				++offered[static_cast<std::size_t>(qt::appearance_role_group_of(role))];
			} else {
				withdrawn.push_back(role);
			}
		}
		expect(
			withdrawn
				== std::vector{ core::appearance_role::diff_hunk_header_background,
								core::appearance_role::diff_hunk_header_text },
			"exactly the two hunk-header roles are not offered");
		expect(
			offered
				== std::array<std::size_t, qt::appearance_role_group_count>{ 7U, 6U, 7U, 28U, 12U },
			"so the card's groups offer 7, 6, 7, 28 and 12 roles, 60 in all");
		expect(
			qt::appearance_role_group_of(core::appearance_role::diff_hunk_header_text)
					== qt::appearance_role_group::diff
				&& !qt::appearance_role_text(
						core::appearance_role::diff_hunk_header_background)
						.isEmpty(),
			"and both keep their group and their words");
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
				&& qt::appearance_role_text(core::appearance_role::feedback_label_quibble)
					== QStringLiteral("Label quibble"),
			"the first role and the last label tint read as the boards list "
			"them");
		expect(
			qt::appearance_role_text(core::appearance_role::state_attention)
					== QStringLiteral("State attention")
				&& qt::appearance_role_group_of(core::appearance_role::state_attention)
					== qt::appearance_role_group::review_states
				&& qt::appearance_role_offered(core::appearance_role::state_attention),
			"the attention amber reads as a review state and is offered there");
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
					== QStringLiteral("Syntax")
				&& qt::appearance_role_group_text(qt::appearance_role_group::comment_labels)
					== QStringLiteral("Comment labels")
				&& qt::appearance_role_text(core::appearance_role::feedback_label_issue)
					== QStringLiteral("Label issue"),
			"the five headings are the accepted five, and a tint reads Label issue");
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
	test_the_five_groups_hold_every_role();
	test_the_card_offers_every_role_but_the_hunk_header();
	test_the_accepted_wording_is_used();
	std::cout << "appearance-role-display-tests:pass\n";
	return EXIT_SUCCESS;
}
