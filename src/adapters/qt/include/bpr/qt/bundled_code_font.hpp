#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_BUNDLED_CODE_FONT_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_BUNDLED_CODE_FONT_HPP

#include <QString>
#include <QStringList>

namespace bpr::qt {
	/// \brief [[BPR-1270]]. The family the bundled code font's faces name.
	///
	/// \details JetBrains Mono NL, the owner's choice for the default
	/// code font on every platform. Its four faces (regular, bold, italic and
	/// bold italic, the faces syntax emphasis asks for) are resources of this
	/// adapter, pinned by cmake/BprCodeFontProvider.cmake.
	[[nodiscard]] auto bundled_code_font_family() -> QString;

	/// \brief The resource paths of the four bundled faces.
	[[nodiscard]] auto bundled_code_font_files() -> QStringList;

	/// \brief Adds font files to this process's application font database.
	///
	/// \details Succeeds only when every file loads and names \p aFamily. A
	/// registration that fails part way removes what it added, so a failure
	/// leaves the database as it found it. Needs a `QGuiApplication`.
	[[nodiscard]] auto register_code_font_files(
		const QStringList& aFiles,
		const QString&     aFamily) -> bool;

	/// \brief Adds the bundled faces to this process's font database.
	///
	/// \details The composition calls this once at startup, after
	/// `QApplication` exists and before it composes any appearance. A failure
	/// is not fatal: the default code font is then the platform's fixed font.
	[[nodiscard]] auto register_bundled_code_font() -> bool;

	/// \brief Whether this process's font database resolves the bundled family.
	///
	/// \details True after a successful registration, and also where the family
	/// is installed on the system, which is the same family.
	[[nodiscard]] auto bundled_code_font_available() -> bool;
} // namespace bpr::qt

#endif
