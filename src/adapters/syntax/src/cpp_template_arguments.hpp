#ifndef BPR_SRC_ADAPTERS_SYNTAX_SRC_CPP_TEMPLATE_ARGUMENTS_HPP
#define BPR_SRC_ADAPTERS_SYNTAX_SRC_CPP_TEMPLATE_ARGUMENTS_HPP

#include "bpr/core/appearance_model.hpp"

#include <QStringView>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace bpr::adapters::syntax {
	/// \brief The template argument lists of one C++ line, found lexically.
	///
	/// \details The packaged C++ grammar styles every user identifier as normal
	/// text and has no rule for `<` after a name, so `getComponent<Type>()`
	/// carried no role at all (BPR-1559). A `<` opens a list only when it
	/// directly follows a name, is not `<<` or `<=`, and is closed on the same
	/// line by a `>` at the same parenthesis depth with nothing between that a
	/// template argument list cannot hold at that depth: `;`, a brace, a string,
	/// `=`, `?`, `|`, `->`, a logical `&&`, a further `<` comparison or a
	/// parenthesis that closes one opened before the list. Spaced comparisons
	/// (`a < b > c`), shifts and `a<b && c>d` therefore open nothing. The scan is
	/// one linear pass; storage is reused from line to line.
	class cpp_template_arguments final
	{
	public:
		/// \brief Finds the lists of one line, replacing the previous line's.
		void scan(QStringView aLine);

		/// \brief Returns the role template context gives the identifier at
		/// `[aBegin, aEnd)` of the scanned line, if any.
		///
		/// \details A name that opens a list is a function when `(` follows the
		/// list, and a type when a name, `::`, `&`, `*` or `{` follows it or when
		/// it is itself an argument. An argument is a type when it alone, after
		/// qualifiers and a `::` path, fills its argument: `<` or `,` before, and
		/// `>`, `,`, `(`, `[`, `...` or a trailing `*`, `&` or cv-qualifier after.
		[[nodiscard]] auto role_of(qsizetype aBegin, qsizetype aEnd) const noexcept
			-> std::optional<core::appearance_role>;

	private:
		struct open_list
		{
			qsizetype   At         = 0;
			int         Depth      = 0;
			std::size_t FirstComma = 0U;
		};

		[[nodiscard]] auto unit(qsizetype aAt) const noexcept -> char16_t;
		[[nodiscard]] auto opens(qsizetype aAt) const noexcept -> bool;
		[[nodiscard]] auto carries(qsizetype aAt, unsigned aFacts) const noexcept -> bool;
		[[nodiscard]] auto argument(qsizetype aBegin, qsizetype aAfter) const noexcept -> bool;
		void               mark(qsizetype aAt, unsigned aFacts);
		void               abandon(int aDepth);
		void               close(qsizetype aAt);

		QStringView               Line;
		bool                      Found = false;
		std::vector<std::int32_t> Close;
		std::vector<std::uint8_t> Facts;
		std::vector<open_list>    Open;
		std::vector<qsizetype>    Commas;
	};
}

#endif
