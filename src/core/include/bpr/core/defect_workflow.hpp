#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_DEFECT_WORKFLOW_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_DEFECT_WORKFLOW_HPP

#include "bpr/core/diff_model.hpp"
#include "bpr/core/model.hpp"
#include "bpr/core/review_ledger.hpp"

#include <compare>
#include <cstdint>
#include <optional>
#include <string_view>

namespace bpr::core {
	using defect_id = strong_id<struct defect_id_tag>;

	/// \brief How much the defect matters.
	///
	/// \details Severity is **local enrichment**. It never appears in the
	/// published comment, because a GitHub-only collaborator must be able to
	/// understand what corrective work is being asked for from the prose alone.
	/// A defect whose meaning lived in a severity field would be unreadable to
	/// everyone not using this tool.
	enum class defect_severity : std::uint8_t {
		trivial,
		minor,
		major,
		critical,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(defect_severity aSeverity) noexcept -> std::string_view;

	/// \brief What kind of problem the defect is. Local enrichment, as severity is.
	enum class defect_category : std::uint8_t {
		correctness,
		security,
		performance,
		maintainability,
		style,
		documentation,
		other,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(defect_category aCategory) noexcept -> std::string_view;

	/// \brief Where one defect sits in its lifecycle.
	///
	/// \details `fix_claimed` and `needs_verification` are both "somebody says
	/// this is done" and neither is `verified`. They are distinct because the
	/// claim's source differs: an author saying so, versus evidence suggesting
	/// it. Collapsing them would lose which one happened, and the reviewer
	/// needs to know whether a person asserted the fix or a heuristic noticed a
	/// change.
	enum class defect_state : std::uint8_t {
		open,
		fix_claimed,
		needs_verification,
		verified,
		reopened,
		withdrawn,
		accepted_as_is,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(defect_state aState) noexcept -> std::string_view;

	/// \brief Reports whether a state still asks something of the reviewer.
	[[nodiscard]] constexpr auto outstanding(const defect_state aState) noexcept
		-> bool
	{
		return aState == defect_state::open
			|| aState == defect_state::fix_claimed
			|| aState == defect_state::needs_verification
			|| aState == defect_state::reopened;
	}

	/// \brief One defect, as a local projection over an ordinary review thread.
	///
	/// \details The anchor is RFC-BPR-0004's semantic source-line identity. A
	/// defect that remembered a visual row would point at the wrong code as
	/// soon as anything above it moved.
	struct defect_record
	{
		defect_id       Id;
		source_line_ref Anchor;
		defect_severity Severity = defect_severity::minor;
		defect_category Category = defect_category::correctness;
		defect_state    State    = defect_state::open;

		auto operator<=>(const defect_record&) const = default;
	};

	/// \brief Names one command against a defect.
	///
	/// \details `verify` is the only command that reaches `verified`, and it
	/// exists only as an explicit reviewer intent. There is deliberately no
	/// command by which evidence, a reply, or a code change can verify
	/// anything.
	enum class defect_command_kind : std::uint8_t {
		claim_fix,
		verify,
		reopen,
		withdraw,
		accept_as_is,
		reclassify,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(defect_command_kind aKind) noexcept -> std::string_view;

	/// \brief Carries one defect command.
	struct defect_command
	{
		defect_id                      Id;
		defect_command_kind            Kind = defect_command_kind::claim_fix;
		std::optional<defect_severity> Severity;
		std::optional<defect_category> Category;

		auto operator<=>(const defect_command&) const = default;
	};

	/// \brief Names why a defect command was refused.
	enum class defect_rejection : std::uint8_t {
		none,
		unknown_defect,
		nothing_to_verify,
		not_verified,
		already_settled,
		nothing_to_reclassify,
	};

	/// \brief Returns the stable name for the supplied value.
	[[nodiscard]] auto name(defect_rejection aRejection) noexcept
		-> std::string_view;

	/// \brief Reports what one defect command did.
	struct defect_outcome
	{
		defect_record    Defect;
		defect_rejection Rejection = defect_rejection::none;
		bool             Changed   = false;

		auto operator<=>(const defect_outcome&) const = default;
	};

	/// \brief Applies one command to one defect.
	///
	/// \details The whole lifecycle policy. It reaches no thread, issues no
	/// request, and knows nothing about GitHub: classifying a thread as a
	/// defect is a local act, and this function is where that stays true.
	[[nodiscard]] auto apply(
		const defect_record&  aDefect,
		const defect_command& aCommand) noexcept -> defect_outcome;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_DEFECT_WORKFLOW_HPP
