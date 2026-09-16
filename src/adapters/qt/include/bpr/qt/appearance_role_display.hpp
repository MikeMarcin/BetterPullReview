#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_APPEARANCE_ROLE_DISPLAY_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_APPEARANCE_ROLE_DISPLAY_HPP

#include "bpr/core/appearance_model.hpp"

#include <QString>

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace bpr::qt {
	/// \brief The four groups RFC-BPR-0034's Colours card lists the roles in.
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
	};

	inline constexpr std::size_t appearance_role_group_count = 4U;

	static_assert(
		static_cast<std::size_t>(appearance_role_group::syntax) + 1U
			== appearance_role_group_count,
		"appearance_role_group_count must cover every declared group");

	/// \brief How one semantic role is shown: its words, and the group it lists in.
	///
	/// \details `Text` is UTF-8 presentation text and is deliberately never a
	/// `core::stable_name`. The stable name is the durable exchange identifier a
	/// theme document keys on, so it must not change with wording; this is the
	/// wording, so it must not reach a document. The page the RFC replaces showed
	/// stable names in a combo box, which is why the distinction is stated here.
	struct appearance_role_display
	{
		appearance_role_group Group = appearance_role_group::surfaces_and_text;
		std::string_view      Text;
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

	/// \brief Returns the role's name in words, as the Colours list shows it.
	[[nodiscard]] auto appearance_role_text(core::appearance_role aRole) -> QString;

	/// \brief Returns one group's heading, as the sticky header shows it.
	[[nodiscard]] auto appearance_role_group_text(appearance_role_group aGroup)
		-> QString;
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_APPEARANCE_ROLE_DISPLAY_HPP
