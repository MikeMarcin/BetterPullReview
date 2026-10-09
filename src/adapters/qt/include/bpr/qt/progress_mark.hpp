#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PROGRESS_MARK_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_PROGRESS_MARK_HPP

#include "bpr/core/appearance_model.hpp"
#include "bpr/core/file_progress_mark.hpp"

#include <QColor>
#include <QIcon>

#include <optional>

class QPainter;
class QRectF;

/// \file
/// \brief [[BPR-1331]] v003. The one picture of a file's private progress.
/// \details The owner rejected v002 on 2026-10-09: "It's very weird to me that
/// we have 2 icon languages for the same concept. Pick one. I think the circles
/// here is probably better overall but it should get some color. All green
/// aint the best either." So the review bar's button, its menu, Overview's read
/// column and the Files drawer all paint the mark here, and nowhere else.
///
/// One 16-unit circle in a 24-unit box. Each state has its own shape inside
/// the circle, so it survives without colour, and its own theme colour, so it
/// reads at a glance: colour accrues as the review moves on.
///
/// | Mark      | Shape                               | Colour role                  |
/// |-----------|-------------------------------------|------------------------------|
/// | unknown   | a dashed ring                       | `state_stale`                |
/// | unread    | a ring                              | `state_stale`                |
/// | seen      | a ring with its left half filled    | `state_attention`            |
/// | read      | a filled disc                       | `state_unread` (the blue)    |
/// | accepted  | a filled disc with a tick cut out   | `state_accepted`             |
/// | excluded  | a ring struck through               | `state_stale`, `state_defect`|
///
/// Unknown is the button's while no file's progress is known (no file
/// selected, the ledger unavailable); a receipt never shows it.
namespace bpr::qt {
	class appearance_profile;

	/// \brief The colours the marks take, one per state, from the theme.
	struct progress_mark_colours
	{
		/// The unread and unknown rings, and the excluded ring.
		QColor Neutral;
		QColor Seen;
		QColor Read;
		QColor Accepted;
		/// The excluded mark's strike.
		QColor Excluded;

		[[nodiscard]] auto operator==(const progress_mark_colours&) const -> bool = default;
	};

	/// \brief The marks' colours in the theme on screen.
	[[nodiscard]] auto progress_mark_colours_for(const appearance_profile& aProfile) -> progress_mark_colours;

	/// \brief The marks' colours in one built-in variant, which a surface uses
	/// until the composition hands it the resolved profile.
	[[nodiscard]] auto builtin_progress_mark_colours(core::appearance_variant aVariant) -> progress_mark_colours;

	/// \brief Every part of the mark in one colour: a disabled control's.
	[[nodiscard]] auto monochrome_progress_mark_colours(const QColor& aInk) -> progress_mark_colours;

	/// \brief Paints `aMark` centred in the square `aBox`; `std::nullopt` is
	/// the unknown mark. The painter's state is restored.
	void paint_progress_mark(QPainter& aPainter, const QRectF& aBox, std::optional<core::file_progress_mark> aMark, const progress_mark_colours& aColours);

	/// \brief The mark as an icon, painted at the size and scale a view asks
	/// for: in `aColours` normally and `aDisabled` alone when disabled.
	/// \details `aActiveHalo`, when given, rings the mark in that colour in the
	/// active mode, which is how a menu draws its highlighted row: given the
	/// menu's own background, the mark sits in a cut-out ring and keeps its
	/// colour on the selection, as a tag's dot does in a macOS menu, so the
	/// blue read disc stays visible on a blue selection.
	[[nodiscard]] auto progress_mark_icon(
		std::optional<core::file_progress_mark> aMark,
		const progress_mark_colours&            aColours,
		const QColor&                           aDisabled,
		std::optional<QColor>                   aActiveHalo = std::nullopt) -> QIcon;
} // namespace bpr::qt

#endif
