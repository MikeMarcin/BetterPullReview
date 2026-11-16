#ifndef BPR_ADAPTERS_QT_INCLUDE_BPR_QT_FOLD_FACES_HPP
#define BPR_ADAPTERS_QT_INCLUDE_BPR_QT_FOLD_FACES_HPP

#include <QString>

#include <cstdint>
#include <initializer_list>
#include <iterator>

/// \file
/// \brief [[BPR-2197]]. The faces a row of named segments folds through as
/// its room shrinks, and the one rule that picks among them. Review's tray
/// tabs, Overview's drawer tabs and the mode switcher all take their faces
/// here; none keeps a copy of its own.
namespace bpr::qt {
	/// \brief A row's faces, widest first.
	///
	/// \details [[BPR-1684]], [[BPR-1755]]: every name; the open segment's
	/// glyph and name with the others' glyphs; glyphs alone. No segment shows
	/// a glyph while every name is showing. The mode switcher has one wider
	/// face before them, each name with its key ([[BPR-2197]]).
	enum class fold_face : std::uint8_t {
		names_keyed,
		names,
		open_named,
		glyphs,
	};

	/// \brief Whether every segment shows its glyph in `aFace`.
	[[nodiscard]] constexpr auto fold_shows_glyph(const fold_face aFace) noexcept -> bool
	{
		return aFace == fold_face::open_named || aFace == fold_face::glyphs;
	}

	/// \brief The text a segment shows in `aFace`: its name with its key,
	/// its name, or nothing.
	///
	/// \param aOpen whether the segment is the open one, which keeps its
	/// name in `fold_face::open_named`.
	/// \param aKey the key as the platform writes it; empty for none.
	[[nodiscard]] inline auto fold_text(const fold_face aFace, const bool aOpen, const QString& aName, const QString& aKey = { }) -> QString
	{
		switch (aFace) {
		case fold_face::names_keyed:
			return aKey.isEmpty() ? aName : QStringLiteral("%1  %2").arg(aName, aKey);
		case fold_face::names:
			return aName;
		case fold_face::open_named:
			return aOpen ? aName : QString{ };
		case fold_face::glyphs:
			return QString{ };
		}
		return aName;
	}

	/// \brief A segment's tooltip: one showing only its glyph says its name
	/// first, as a folded tab does.
	[[nodiscard]] inline auto fold_tooltip(const QString& aShown, const QString& aName, const QString& aAbout) -> QString
	{
		return aShown.isEmpty() ? QStringLiteral("%1\n%2").arg(aName, aAbout) : aAbout;
	}

	/// \brief The first of `aFaces`, widest first, for which `aFits` holds;
	/// the last when none does.
	///
	/// \details `aFits` is asked in order and not past the first face that
	/// fits, so it may lay the row out in each face it is asked about. The
	/// last face is never asked: it is what the row shows when nothing wider
	/// fits.
	/// \tparam TFits callable as `bool(fold_face)`.
	template <typename TFits>
	[[nodiscard]] auto choose_fold_face(const std::initializer_list<fold_face> aFaces, const TFits& aFits) -> fold_face
	{
		auto chosen = fold_face::glyphs;
		for (auto face = aFaces.begin(); face != aFaces.end(); ++face) {
			chosen = *face;
			if (std::next(face) == aFaces.end() || aFits(*face)) {
				break;
			}
		}
		return chosen;
	}
} // namespace bpr::qt

#endif
