#include "bpr/qt/appearance_role_display.hpp"

namespace bpr::qt {
	auto display_of(const core::appearance_role aRole) noexcept
		-> appearance_role_display
	{
		using group = appearance_role_group;
		using role  = core::appearance_role;

		// One switch, no default. Adding a role to the core's closed vocabulary
		// stops this translation unit from compiling, which is the only way a
		// presentation vocabulary over a core enumeration can be kept total: a
		// table keyed by index would accept a new role and show it as an empty
		// row, and a lookup with a fallback would show it as whatever the
		// fallback said.
		//
		// The wording is the accepted design's. `diff_intraline_*` reads "in-line"
		// rather than "intraline" because that is what the board says and what a
		// reviewer calls it.
		switch (aRole) {
		case role::application_surface:
			return { group::surfaces_and_text, "Application surface" };
		case role::application_text:
			return { group::surfaces_and_text, "Application text" };
		case role::code_background:
			return { group::surfaces_and_text, "Code background" };
		case role::code_gutter_background:
			return { group::surfaces_and_text, "Code gutter background" };
		case role::code_line_number_text:
			return { group::surfaces_and_text, "Code line number text" };
		case role::code_selection_background:
			return { group::surfaces_and_text, "Code selection background" };
		case role::code_current_line_background:
			return { group::surfaces_and_text, "Code current line background" };

		case role::diff_added_background:
			return { group::diff, "Diff added background" };
		case role::diff_removed_background:
			return { group::diff, "Diff removed background" };
		case role::diff_intraline_added_background:
			return { group::diff, "Diff in-line added background" };
		case role::diff_intraline_removed_background:
			return { group::diff, "Diff in-line removed background" };
		case role::diff_moved_background:
			return { group::diff, "Diff moved background" };
		// Not offered ([[BPR-0986]]): the review window has no hunk header. Its
		// panes show whole documents, and a group's start is drawn as the
		// boundary rule below.
		case role::diff_hunk_header_background:
			return { .Group   = group::diff,
					 .Text    = "Diff hunk header background",
					 .Offered = false };
		case role::diff_hunk_header_text:
			return { .Group = group::diff, .Text = "Diff hunk header text", .Offered = false };
		case role::diff_group_boundary_rule:
			return { group::diff, "Diff group boundary rule" };

		case role::state_unread:
			return { group::review_states, "State unread" };
		case role::state_accepted:
			return { group::review_states, "State accepted" };
		case role::state_defect:
			return { group::review_states, "State defect" };
		case role::state_fixed:
			return { group::review_states, "State fixed" };
		case role::state_verified:
			return { group::review_states, "State verified" };
		case role::state_stale:
			return { group::review_states, "State stale" };

		case role::syntax_normal:
			return { group::syntax, "Syntax normal" };
		case role::syntax_keyword:
			return { group::syntax, "Syntax keyword" };
		case role::syntax_control_flow:
			return { group::syntax, "Syntax control flow" };
		case role::syntax_operator:
			return { group::syntax, "Syntax operator" };
		case role::syntax_builtin:
			return { group::syntax, "Syntax builtin" };
		case role::syntax_function:
			return { group::syntax, "Syntax function" };
		case role::syntax_variable:
			return { group::syntax, "Syntax variable" };
		case role::syntax_type:
			return { group::syntax, "Syntax type" };
		case role::syntax_constant:
			return { group::syntax, "Syntax constant" };
		case role::syntax_number:
			return { group::syntax, "Syntax number" };
		case role::syntax_string:
			return { group::syntax, "Syntax string" };
		case role::syntax_character:
			return { group::syntax, "Syntax character" };
		case role::syntax_escape_sequence:
			return { group::syntax, "Syntax escape sequence" };
		case role::syntax_comment:
			return { group::syntax, "Syntax comment" };
		case role::syntax_documentation:
			return { group::syntax, "Syntax documentation" };
		case role::syntax_annotation:
			return { group::syntax, "Syntax annotation" };
		case role::syntax_macro:
			return { group::syntax, "Syntax macro" };
		case role::syntax_preprocessor:
			return { group::syntax, "Syntax preprocessor" };
		case role::syntax_import:
			return { group::syntax, "Syntax import" };
		case role::syntax_attribute:
			return { group::syntax, "Syntax attribute" };
		case role::syntax_region_marker:
			return { group::syntax, "Syntax region marker" };
		case role::syntax_information:
			return { group::syntax, "Syntax information" };
		case role::syntax_warning:
			return { group::syntax, "Syntax warning" };
		case role::syntax_alert:
			return { group::syntax, "Syntax alert" };
		case role::syntax_error:
			return { group::syntax, "Syntax error" };
		case role::syntax_namespace:
			return { group::syntax, "Namespaces" };
		case role::syntax_enum_member:
			return { group::syntax, "Enum members" };
		case role::syntax_other:
			return { group::syntax, "Syntax other" };

		case role::feedback_label_praise:
			return { group::comment_labels, "Label praise" };
		case role::feedback_label_nitpick:
			return { group::comment_labels, "Label nitpick" };
		case role::feedback_label_suggestion:
			return { group::comment_labels, "Label suggestion" };
		case role::feedback_label_issue:
			return { group::comment_labels, "Label issue" };
		case role::feedback_label_todo:
			return { group::comment_labels, "Label todo" };
		case role::feedback_label_question:
			return { group::comment_labels, "Label question" };
		case role::feedback_label_thought:
			return { group::comment_labels, "Label thought" };
		case role::feedback_label_chore:
			return { group::comment_labels, "Label chore" };
		case role::feedback_label_note:
			return { group::comment_labels, "Label note" };
		case role::feedback_label_typo:
			return { group::comment_labels, "Label typo" };
		case role::feedback_label_polish:
			return { group::comment_labels, "Label polish" };
		case role::feedback_label_quibble:
			return { group::comment_labels, "Label quibble" };

		// [[BPR-1085]]. Declared last for the durable order, listed with the
		// review states it sits beside.
		case role::state_attention:
			return { group::review_states, "State attention" };
		}

		return { };
	}

	auto appearance_role_group_of(const core::appearance_role aRole) noexcept
		-> appearance_role_group
	{
		return display_of(aRole).Group;
	}

	auto appearance_role_offered(const core::appearance_role aRole) noexcept -> bool
	{
		return display_of(aRole).Offered;
	}

	auto appearance_role_text(const core::appearance_role aRole) -> QString
	{
		const auto text = display_of(aRole).Text;
		return QString::fromUtf8(
			text.data(), static_cast<qsizetype>(text.size()));
	}

	auto appearance_role_group_text(const appearance_role_group aGroup) -> QString
	{
		switch (aGroup) {
		case appearance_role_group::surfaces_and_text:
			return QStringLiteral("Surfaces and text");
		case appearance_role_group::diff:
			return QStringLiteral("Diff");
		case appearance_role_group::review_states:
			return QStringLiteral("Review states");
		case appearance_role_group::syntax:
			return QStringLiteral("Syntax");
		case appearance_role_group::comment_labels:
			return QStringLiteral("Comment labels");
		}

		return { };
	}
} // namespace bpr::qt
