#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_THEME_PREVIEW_STRIP_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_THEME_PREVIEW_STRIP_HPP

#include "bpr/core/appearance_model.hpp"

#include <QFont>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QString>
#include <QWidget>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>

class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QScrollArea;

namespace bpr::qt {
	/// \brief One of the six theme choices the strip draws.
	struct theme_preview_card
	{
		core::appearance_mode Mode = core::appearance_mode::system;
		QString               Name;
		/// The small caption beneath the name: "dark now", "2 colours customised".
		QString Note;
		/// Where the tile is **inside the scrolling strip**, not inside the well.
		///
		/// The strip is wider than the widget that shows it, so a tile's position
		/// does not move when the well scrolls; what moves is which part of the
		/// strip the well shows. Use `tile_rect_in_strip` for a rect to compare
		/// against this widget's own geometry.
		QRect Rect;
	};

	using theme_preview_dispatch = std::function<void(core::appearance_mode)>;

	/// One tile per `core::appearance_mode`, which is what makes the tiles the
	/// control rather than a picture beside one.
	inline constexpr std::size_t theme_preview_count = core::appearance_mode_count;

	/// The accepted tile frame, in points.
	inline constexpr int theme_preview_tile_width  = 232;
	inline constexpr int theme_preview_tile_height = 132;

	/// \brief Records directly observable strip painting and scrolling.
	struct theme_preview_statistics
	{
		std::uint64_t TilesPainted = 0U;
		/// Tiles drawn as two palettes split by a clip rather than one palette.
		std::uint64_t DiagonalTilesPainted = 0U;
		/// Selections that had to move the well to show the tile they chose.
		std::uint64_t ScrollsIntoView = 0U;
		/// [[BPR-1107]]. White space marks the tiles drew, cumulative.
		std::uint64_t WhiteSpacePainted = 0U;

		auto operator<=>(const theme_preview_statistics&) const = default;
	};

	/// \brief RFC-BPR-0034's theme strip: six tiles, each painted in its own theme.
	///
	/// \details [[BPR-0652]], rebuilt by [[BPR-0790]]. An inset well holding a
	/// sideways-scrolling strip of one tile per mode, in the order System, Light,
	/// Dark, High contrast system, High contrast light, High contrast dark. A tile
	/// is a 232 by 132 point frame painted by the shared code sample painter from
	/// that mode's built-in palette plus that theme's overrides, so the preview is
	/// the theme rather than a picture of it. The two System tiles split
	/// diagonally, light in the top-left triangle over dark in the bottom-right,
	/// each half painted in its own palette by clipping and the edge being nothing
	/// but the change of palette.
	///
	/// Choosing a tile reports the mode; the panel turns it into the same
	/// appearance intent the page used to send from a combo box. Left and Right
	/// move the selection and scroll it into view.
	///
	/// The widget observes nothing. Which variant the desktop resolves, how many
	/// colours each theme customises, each theme's palette and the code font are
	/// all supplied, because the core decides the variant and the profile owns the
	/// palettes; a tile that worked any of them out for itself would be stating a
	/// fact it has no evidence for.
	class theme_preview_strip final : public QWidget
	{
	public:
		explicit theme_preview_strip(QWidget* aParent = nullptr);

		void               set_current(core::appearance_mode aMode);
		[[nodiscard]] auto current() const noexcept -> core::appearance_mode;
		/// \brief What the desktop resolves now, for the System tiles' captions.
		///
		/// \details Supplied by the composition, which is the only place that knows:
		/// the core decides the variant from the selected mode and a bounded
		/// observation, and this widget observes nothing. Until it is supplied the
		/// System captions say nothing, rather than saying something untrue.
		void               set_system_variant(core::appearance_variant aVariant);
		[[nodiscard]] auto system_variant() const noexcept
			-> const std::optional<core::appearance_variant>&;
		/// \brief How many colours each theme customises, by variant ordinal.
		///
		/// \details Published by the composition from the appearance profile, which
		/// owns the built-in palettes a customisation is measured against.
		void set_customised_counts(
			std::array<std::size_t, core::appearance_variant_count> aCounts);
		[[nodiscard]] auto customised_counts() const noexcept
			-> const std::array<std::size_t, core::appearance_variant_count>&;
		/// \brief Publishes each theme's palette: its built-in plus its overrides.
		///
		/// \details RFC-BPR-0034 paints a tile from that theme's own colours, and a
		/// theme's colours are its built-in profile with its own customisations
		/// applied — which is `appearance_profile::palette_for`. Until the
		/// composition supplies them every tile paints its built-in profile, which
		/// is the truth about an installation that has customised nothing.
		void set_theme_palettes(
			std::array<core::appearance_palette, core::appearance_variant_count>
				aPalettes);
		/// \brief Adopts the code font the accepted typography asks for.
		///
		/// \details Supplied rather than chosen. Until it is, the tiles use the
		/// code font `default_typography_settings` starts a fresh installation
		/// from — never a family named in this file.
		void set_code_font(QFont aFont);
		/// \brief [[BPR-1107]]. When the tiles draw white space: the typography's
		/// own choice, so a tile shows what its theme's review window shows.
		void set_white_space(core::white_space_drawing aDrawing);
		void set_dispatch(theme_preview_dispatch aDispatch);

		[[nodiscard]] auto cards() const noexcept
			-> const std::array<theme_preview_card, theme_preview_count>&;
		/// \brief Which tile covers a point of the scrolling strip.
		[[nodiscard]] auto hit_test(QPoint aPoint) const
			-> std::optional<std::size_t>;
		/// \brief One tile's frame in this widget's coordinates, empty when hidden.
		///
		/// \details The clipped intersection of the tile and the well's viewport,
		/// so a tile scrolled out of sight answers an empty rect rather than a
		/// position outside the widget.
		[[nodiscard]] auto tile_rect_in_strip(std::size_t aCard) const -> QRect;
		/// \brief The inset well the strip scrolls inside.
		[[nodiscard]] auto well() const noexcept -> QScrollArea*;
		[[nodiscard]] auto statistics() const noexcept -> theme_preview_statistics;
		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void mousePressEvent(QMouseEvent* aEvent) override;
		void keyPressEvent(QKeyEvent* aEvent) override;

	private:
		class tile_canvas;

		void relayout();
		void choose(std::size_t aCard);
		void ensure_visible(std::size_t aCard);
		void refresh_notes();
		void refresh_accessible_description();
		/// \brief The theme one tile paints, once the desktop has been named.
		[[nodiscard]] auto card_variant(std::size_t aCard) const
			-> std::optional<core::appearance_variant>;
		/// \brief Paints every tile onto the canvas.
		void paint_tiles(QPaintEvent* aEvent);

		QScrollArea* Well   = nullptr;
		tile_canvas* Canvas = nullptr;

		std::array<theme_preview_card, theme_preview_count> Cards{ };
		core::appearance_mode                               Current
			= core::appearance_mode::system;
		/// \brief What the desktop resolves, once the composition has said.
		///
		/// \details Deliberately not defaulted to a variant. The constructor used
		/// to call `set_system_variant(light)`, so every System tile captioned
		/// "light now" whatever the desktop was and no production caller ever
		/// corrected it — a widget guessing the resolved variant, which
		/// RFC-BPR-0034 prohibits. Absent means nothing has been supplied yet.
		std::optional<core::appearance_variant>                 System;
		std::array<std::size_t, core::appearance_variant_count> CustomisedCounts{ };
		std::array<core::appearance_palette, core::appearance_variant_count>
								  ThemePalettes{ };
		QFont                     CodeFont;
		core::white_space_drawing WhiteSpace = core::white_space_drawing::none;
		theme_preview_dispatch    Dispatch;
		theme_preview_statistics  Statistics;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_THEME_PREVIEW_STRIP_HPP
