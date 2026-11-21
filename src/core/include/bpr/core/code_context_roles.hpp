#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_CODE_CONTEXT_ROLES_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_CODE_CONTEXT_ROLES_HPP

#include "bpr/core/appearance_model.hpp"
#include "bpr/core/syntax_classification.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief [[BPR-2351]]. Code anchored to a reviewed file, coloured with that
/// file's names (owner, 2026-11-21: "when we syntax highlight for a report
/// finding are we using the context of where the comment is located to find
/// names of symbols? Seems like we should.").
/// \details A policy over classifications that already exist, not a parser:
/// the file's names and roles are read off the spans the file view's own
/// refined classification gave it, and a block borrows them by name for its
/// own tokens. The block's grammar still decides every span; only an
/// identifier its grammar left plain or gave a heuristic role takes the
/// file's role.
namespace bpr::core {
	/// \brief One name of a file and the one role its classification gives it.
	struct code_context_name
	{
		std::string     Name;
		appearance_role Role = appearance_role::syntax_normal;

		auto operator<=>(const code_context_name&) const = default;
	};

	/// \brief The names a file's classification gives exactly one borrowable
	/// role, ordered by name, each once.
	/// \details A name the file gives two roles that cannot both hold
	/// (a namespace and a type, a macro and a function) is left out, so a
	/// block falls back to its own classification for it. A type that is
	/// also called (`frame(...)`, a constructor) stays a type. A name the file
	/// leaves plain everywhere is not here.
	struct code_context_roles
	{
		std::vector<code_context_name> Names;

		auto operator<=>(const code_context_roles&) const = default;
	};

	/// \brief The most distinct names one file contributes; later ones are not read.
	inline constexpr std::size_t maximum_code_context_names = 16'384U;
	/// \brief The longest name read; a longer identifier is not a context name.
	inline constexpr std::size_t maximum_code_context_name_bytes = 128U;

	/// \brief Reads a file's names and roles off its own classification.
	/// \param aText The file's bytes, as classified.
	/// \param aSpans Its spans: lines and byte columns of `aText`. A span
	/// outside the text, across a line, or not covering exactly one whole
	/// identifier is not read.
	/// \details Deterministic: equal text and spans give equal names. The
	/// borrowable roles are namespace, type, function, enum member and macro;
	/// a variable or constant role is evidence against a name, never borrowed.
	[[nodiscard]] auto code_context_roles_of(std::string_view aText, const token_span_list& aSpans) -> code_context_roles;

	/// \brief The role `aRoles` gives `aName`, if it gives one.
	[[nodiscard]] auto code_context_role(const code_context_roles& aRoles, std::string_view aName) -> std::optional<appearance_role>;

	/// \brief A block's spans with the identifiers `aContext` names in the
	/// file's roles.
	/// \param aText The block's text; `aSpans` its own classification.
	/// \param aMaximumSpans The span ceiling the block was classified under.
	/// \returns `aSpans` unchanged when nothing is borrowed or the result would
	/// pass `aMaximumSpans`. Otherwise every identifier `aContext` names whose
	/// own role is plain, a type, a function or a macro takes the file's role,
	/// except where the place cannot hold it: after `.` or `->` only a
	/// function is borrowed, and before `::` only a namespace or a type.
	/// Strings, comments, keywords, built-ins, preprocessor lines and the
	/// block's own namespaces and enum members are never repainted.
	[[nodiscard]] auto borrow_code_context_roles(std::string_view aText, const token_span_list& aSpans, const code_context_roles& aContext, std::size_t aMaximumSpans)
		-> token_span_list;
} // namespace bpr::core

#endif
