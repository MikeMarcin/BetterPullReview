#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_CYCLE_SHARE_MARKS_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_REVIEW_CYCLE_SHARE_MARKS_HPP

#include "bpr/qt/code_sample_painter.hpp"

#include <QColor>
#include <QIcon>
#include <QLabel>
#include <QPixmap>
#include <QWidget>

#include <cstdint>

class QPalette;

/// \file
/// \brief [[BPR-1285]]. The marks Share notes uses to say less in words: a
/// small set of line glyphs, the tones that colour them from the theme's role
/// table, a chip that pairs a glyph with a few words, and a tinted panel.
/// \details Presentation only. Every colour is a role of the appearance the
/// sheet was given (`code_role_colors`), never a fixed one; before an
/// appearance arrives a tone falls back to the palette's placeholder colour,
/// so a mark still has its glyph and words. No mark carries meaning by colour
/// alone: a chip has words, a glyph has a tooltip and an accessible name on
/// the control it sits on.
namespace bpr::qt {
	/// \brief A line glyph on a 24-unit grid, stroked in one ink.
	enum class share_glyph : std::uint8_t {
		/// Copy: two sheets.
		copy,
		/// Save as File: an arrow into a tray.
		save,
		/// Save as Bundle: a box with a lid.
		bundle,
		/// The exact text: a page with lines.
		page,
		/// A terminal prompt: the external harness, a printed round, the invocation.
		terminal,
		/// Send to Agent: a paper plane.
		send,
		/// Copied: a clipboard with a tick.
		copied,
		/// Read through the local interface: an eye.
		read,
		/// Accepted by the agent adapter: an inbox.
		handoff,
		/// The recipient acknowledged: a speech bubble with a tick.
		acknowledged,
		/// Not confirmed: a question in a circle.
		uncertain,
		/// Failed: a cross in a circle.
		failed,
		/// Not allowed: a slashed circle.
		refused,
		/// Not shared yet: a dashed circle.
		pending,
		/// Changed after sharing: a pencil.
		changed,
		/// Fixed as shared: a padlock.
		fixed,
		/// Plain information: an i in a circle.
		information,
		/// Asks before replacing: a triangle with a bang.
		caution,
	};

	/// \brief What a mark says about a state, which chooses its role colour.
	enum class share_tone : std::uint8_t {
		/// Nothing has happened yet (`state_stale`).
		waiting,
		/// The bytes left here, on the clipboard or in a file (`state_accepted`).
		local,
		/// The bytes went on to something else: an interface, an adapter, a recipient (`state_verified`).
		onward,
		/// Changed since it was shared (`state_fixed`).
		changed,
		/// Not confirmed, or asks first (`syntax_warning`).
		warning,
		/// Failed or not allowed (`state_defect`).
		refused,
		/// Plain words about what happened (`state_unread`).
		information,
	};

	/// \brief The role a tone takes its colour from.
	[[nodiscard]] auto share_tone_role(share_tone aTone) noexcept -> core::appearance_role;
	/// \brief The tone's colour in `aColours`; the palette's placeholder colour
	/// while `aColours` holds no valid colour for its role.
	[[nodiscard]] auto share_tone_colour(share_tone aTone, const code_role_colors& aColours, const QPalette& aPalette) -> QColor;

	/// \brief One glyph, `aLogical` pixels square at `aScale`, in `aInk`.
	[[nodiscard]] auto share_glyph_pixmap(share_glyph aGlyph, const QColor& aInk, int aLogical, qreal aScale) -> QPixmap;
	/// \brief The glyph as an icon, painted at the size and scale a button or
	/// list asks for ([[BPR-1296]]); disabled in `aDisabled`.
	[[nodiscard]] auto share_glyph_icon(share_glyph aGlyph, const QColor& aInk, const QColor& aDisabled) -> QIcon;

	/// \brief A chip: a glyph in its tone's colour and a few words, on a faint
	/// wash of the same colour inside an outline of it.
	/// \details The words are the label's text, so they are its accessible
	/// name; the colour is never the only sign. It takes no focus.
	class share_chip final : public QLabel
	{
	public:
		explicit share_chip(QWidget* aParent = nullptr);

		/// \brief Shows `aWords` with `aGlyph` in `aColour`.
		void               set_mark(share_glyph aGlyph, const QColor& aColour, const QString& aWords);
		[[nodiscard]] auto glyph() const noexcept -> share_glyph;
		[[nodiscard]] auto colour() const noexcept -> const QColor&;
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		share_glyph Glyph = share_glyph::information;
		QColor      Colour;
	};

	/// \brief A panel washed faintly in one colour, with a glyph at its start:
	/// what holds the sheet's status and its replace question.
	class share_panel final : public QWidget
	{
	public:
		explicit share_panel(QWidget* aParent = nullptr);

		void               set_mark(share_glyph aGlyph, const QColor& aColour);
		[[nodiscard]] auto glyph() const noexcept -> share_glyph;
		[[nodiscard]] auto colour() const noexcept -> const QColor&;
		/// \brief The label that shows the glyph, first in the panel's layout.
		[[nodiscard]] auto mark() const noexcept -> QLabel*;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
		void changeEvent(QEvent* aEvent) override;

	private:
		void        repaint_mark();
		share_glyph Glyph = share_glyph::information;
		QColor      Colour;
		QLabel*     Mark = nullptr;
	};
}

#endif
