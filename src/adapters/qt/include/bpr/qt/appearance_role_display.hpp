#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_APPEARANCE_ROLE_DISPLAY_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_APPEARANCE_ROLE_DISPLAY_HPP

#include "bpr/core/appearance_model.hpp"

#include <QString>

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace bpr::qt {
	/// \brief The groups RFC-BPR-0034's Colours card lists the roles in.
	///
	/// \details Presentation's own grouping. The core's header carries comments
	/// that mark five runs of roles — application chrome, the code canvas, diff,
	/// review states, syntax — and the accepted design merges the first two into
	/// one group, because a reviewer looking for a background does not first decide
	/// whether it is chrome or canvas. The grouping therefore belongs here and not
	/// in the core: it is how the list reads, not what a theme means.
	enum class appearance_role_group : std::uint8_t {
		surfaces_and_text,
		diff,
		review_states,
		syntax,
		/// [[BPR-1083]]. The fifth group, Comment labels: the twelve label
		/// tints (owner, BPR-1079 `tints-v002`).
		comment_labels,
	};

	inline constexpr std::size_t appearance_role_group_count = 5U;

	static_assert(
		static_cast<std::size_t>(appearance_role_group::comment_labels) + 1U
			== appearance_role_group_count,
		"appearance_role_group_count must cover every declared group");

	/// \brief How one semantic role is shown: its words, and the group it lists in.
	///
	/// \details `Text` is UTF-8 presentation text and is deliberately never a
	/// `core::stable_name`. The stable name is the durable exchange identifier a
	/// theme document keys on, so it must not change with wording; this is the
	/// wording, so it must not reach a document. The page the RFC replaces showed
	/// stable names in a combo box, which is why the distinction is stated here.
	///
	/// `Offered` says whether the Colours card lists the role at all. Two roles
	/// are not offered: `diff_hunk_header_background` and `diff_hunk_header_text`
	/// name a surface the review window does not have, and a row that changed
	/// nothing on screen would offer a reviewer a control that does nothing. The
	/// owner amended RFC-BPR-0034 C5 from 47 offered roles to 45 on 2026-09-24
	/// ([[BPR-0986]]). The core's vocabulary and the theme documents keep both
	/// roles, so import and export still carry them.
	///
	/// The twelve label tints [[BPR-1083]] adds are offered, which makes 57
	/// (owner, BPR-1079 `tints-v002`; RFC-BPR-0034 C5 amended), the attention
	/// amber [[BPR-1085]] adds, in Review states, makes 58, and RFC-BPR-0072's
	/// namespace and enum-member colours, in Syntax, make 60.
	struct appearance_role_display
	{
		appearance_role_group Group = appearance_role_group::surfaces_and_text;
		std::string_view      Text;
		bool                  Offered = true;
	};

	/// \brief Returns how the supplied role is shown.
	///
	/// \details Total over the core's closed vocabulary, and derived from it by one
	/// `switch` with no `default`: a role added to the core is a compile error here
	/// rather than a row the list silently omits. A value that is not a declared
	/// role has no words, which is the honest answer and the one the empty `Text`
	/// reports; the list never reaches it, because it walks
	/// `core::appearance_role_count`.
	[[nodiscard]] auto display_of(core::appearance_role aRole) noexcept
		-> appearance_role_display;

	/// \brief Returns the group the supplied role lists in.
	[[nodiscard]] auto appearance_role_group_of(core::appearance_role aRole) noexcept
		-> appearance_role_group;

	/// \brief Returns whether the Colours card lists the supplied role.
	[[nodiscard]] auto appearance_role_offered(core::appearance_role aRole) noexcept
		-> bool;

	/// \brief Returns the role's name in words, as the Colours list shows it.
	[[nodiscard]] auto appearance_role_text(core::appearance_role aRole) -> QString;

	/// \brief Returns one group's heading, as the sticky header shows it.
	[[nodiscard]] auto appearance_role_group_text(appearance_role_group aGroup)
		-> QString;
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_APPEARANCE_ROLE_DISPLAY_HPP
