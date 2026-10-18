#include "bpr/qt/theme_preview_strip.hpp"

#include "bpr/application/appearance_runtime_ports.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/code_sample_painter.hpp"

#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		/// The gap between two tiles, from the accepted design's strip.
		constexpr int TileGap = 18;
		/// The name beneath a tile.
		constexpr int TileNameHeight = 18;
		/// The caption beneath the name.
		constexpr int TileNoteHeight = 16;
		/// Breathing room between the frame and the name.
		constexpr int TileNameGap = 8;
		/// The well's own inset, which is what makes it read as a well.
		constexpr int WellHorizontalInset = 18;
		constexpr int WellTopInset        = 16;
		constexpr int WellBottomInset     = 14;
		/// How many tiles and part of a tile the strip prefers to show, which is
		/// what the accepted design shows at the page's default width.
		constexpr qreal PreferredTiles = 3.5;
		/// The largest size at which four rows and a gutter fit the accepted 232 by
		/// 132 point frame. The family, the weight and every colour still come from
		/// the supplied font and the supplied palette; only the size is bounded,
		/// because the frame is a fixed size and a 24 point sample in it would show
		/// one row rather than a diff.
		constexpr qreal TileCodePointSize = 11.0;
		/// The band of the frame the snippet's rows never reach. It exists so a
		/// tile always shows all four rows and always keeps a strip of its own code
		/// background beneath them, whatever line spacing the supplied font has.
		constexpr int TileSampleBottomMargin = 26;
		/// The caption's separator when a tile has both a resolution and a count to
		/// report.
		constexpr auto NoteSeparator = " \xc2\xb7 ";

		constexpr int TileCardHeight = theme_preview_tile_height + TileNameGap
			+ TileNameHeight + TileNoteHeight;

		[[nodiscard]] auto high_contrast_mode(const core::appearance_mode aMode) noexcept
			-> bool
		{
			return aMode == core::appearance_mode::high_contrast_system
				|| aMode == core::appearance_mode::high_contrast_light
				|| aMode == core::appearance_mode::high_contrast_dark;
		}

		[[nodiscard]] auto follows_desktop(const core::appearance_mode aMode) noexcept
			-> bool
		{
			return aMode == core::appearance_mode::system
				|| aMode == core::appearance_mode::high_contrast_system;
		}

		[[nodiscard]] auto dark_family(const core::appearance_variant aVariant) noexcept
			-> bool
		{
			return aVariant == core::appearance_variant::dark
				|| aVariant == core::appearance_variant::high_contrast_dark;
		}

		/// \brief The dark member of one mode's family, which a tile paints first.
		[[nodiscard]] auto base_variant(const core::appearance_mode aMode) noexcept
			-> core::appearance_variant
		{
			switch (aMode) {
			case core::appearance_mode::light:
				return core::appearance_variant::light;
			case core::appearance_mode::high_contrast_light:
				return core::appearance_variant::high_contrast_light;
			case core::appearance_mode::high_contrast_dark:
			case core::appearance_mode::high_contrast_system:
				return core::appearance_variant::high_contrast_dark;
			case core::appearance_mode::system:
			case core::appearance_mode::dark:
				break;
			}
			return core::appearance_variant::dark;
		}

		/// \brief The light member a System tile lays over its dark half.
		///
		/// \details Only the two desktop-following modes have one. The orientation
		/// is fixed by RFC-BPR-0034 — light in the top-left triangle over dark in
		/// the bottom-right — so the tile shows what the mode *means* rather than
		/// which half is in effect; the caption is what names the half in effect.
		[[nodiscard]] auto overlay_variant(const core::appearance_mode aMode) noexcept
			-> std::optional<core::appearance_variant>
		{
			switch (aMode) {
			case core::appearance_mode::system:
				return core::appearance_variant::light;
			case core::appearance_mode::high_contrast_system:
				return core::appearance_variant::high_contrast_light;
			case core::appearance_mode::light:
			case core::appearance_mode::dark:
			case core::appearance_mode::high_contrast_light:
			case core::appearance_mode::high_contrast_dark:
				break;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto mode_name(const core::appearance_mode aMode) -> QString
		{
			switch (aMode) {
			case core::appearance_mode::system:
				return QStringLiteral("System");
			case core::appearance_mode::light:
				return QStringLiteral("Light");
			case core::appearance_mode::dark:
				return QStringLiteral("Dark");
			case core::appearance_mode::high_contrast_system:
				return QStringLiteral("High contrast system");
			case core::appearance_mode::high_contrast_light:
				return QStringLiteral("High contrast light");
			case core::appearance_mode::high_contrast_dark:
				return QStringLiteral("High contrast dark");
			}
			return { };
		}

		[[nodiscard]] auto customised_note(const std::size_t aCount) -> QString
		{
			if (aCount == 0U) {
				return { };
			}
			return aCount == 1U
				? QStringLiteral("1 colour customised")
				: QStringLiteral("%1 colours customised").arg(aCount);
		}

		/// \brief The code font a fresh installation starts from.
		///
		/// \details `default_typography_settings`' own, so a strip nobody has
		/// handed the accepted code font still draws code in the code font a
		/// fresh profile would ([[BPR-1270]]) — and no family is named in this
		/// file.
		[[nodiscard]] auto default_code_font() -> QFont
		{
			auto font = to_presentation_font(default_typography_settings().Code);
			font.setStyleHint(QFont::Monospace);
			return font;
		}
	} // namespace

	/// \brief The strip itself: six tiles side by side, wider than the well.
	///
	/// \details A separate widget from the well so that scrolling moves which part
	/// of the strip is shown rather than re-laying the tiles out. It paints nothing
	/// of its own; the enclosing strip owns the tiles and paints them here.
	class theme_preview_strip::tile_canvas final : public QWidget
	{
	public:
		explicit tile_canvas(theme_preview_strip* const aStrip)
			: QWidget{ aStrip }
			, Strip{ aStrip }
		{
			setObjectName(QStringLiteral("appearanceThemePreviewStrip"));
			setFocusPolicy(Qt::NoFocus);
		}

	protected:
		void paintEvent(QPaintEvent* const aEvent) override
		{
			Strip->paint_tiles(aEvent);
		}

		void mousePressEvent(QMouseEvent* const aEvent) override
		{
			if (aEvent->button() != Qt::LeftButton) {
				QWidget::mousePressEvent(aEvent);
				return;
			}
			if (const auto hit = Strip->hit_test(aEvent->position().toPoint());
				hit.has_value()) {
				Strip->setFocus(Qt::MouseFocusReason);
				Strip->choose(*hit);
				aEvent->accept();
				return;
			}
			QWidget::mousePressEvent(aEvent);
		}

	private:
		theme_preview_strip* Strip = nullptr;
	};

	theme_preview_strip::theme_preview_strip(QWidget* const aParent)
		: QWidget{ aParent }
		, CodeFont{ default_code_font() }
	{
		setObjectName(QStringLiteral("appearanceThemePreviews"));
		setAccessibleName(QStringLiteral("Theme"));
		setFocusPolicy(Qt::StrongFocus);
		for (auto index = std::size_t{ 0U }; index < Cards.size(); ++index) {
			const auto mode   = static_cast<core::appearance_mode>(index);
			Cards[index].Mode = mode;
			Cards[index].Name = mode_name(mode);
		}
		// Every theme's own built-in profile until the composition supplies the
		// palettes with their overrides. That is the truth about an installation
		// which has customised nothing, rather than a placeholder.
		for (auto index = std::size_t{ 0U };
			 index < core::appearance_variant_count;
			 ++index) {
			ThemePalettes[index] = builtin_appearance_palette(
				static_cast<core::appearance_variant>(index));
		}

		Canvas = new tile_canvas{ this };
		Well   = new QScrollArea{ this };
		Well->setObjectName(QStringLiteral("appearanceThemePreviewWell"));
		// Native inset rather than a style sheet: the frame the platform draws for
		// a sunken panel is what makes the strip read as a well.
		Well->setFrameShape(QFrame::StyledPanel);
		Well->setFrameShadow(QFrame::Sunken);
		Well->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
		Well->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Well->setWidgetResizable(false);
		// The strip takes the focus and answers the arrow keys, so the well must
		// not become a second tab stop in front of it.
		Well->setFocusPolicy(Qt::NoFocus);
		Well->setWidget(Canvas);

		auto* const layout = new QVBoxLayout{ this };
		layout->setContentsMargins(0, 0, 0, 0);
		layout->setSpacing(0);
		layout->addWidget(Well);

		// No `set_system_variant` here on purpose. This widget cannot observe the
		// desktop, so a value chosen at construction would be a claim it has no
		// evidence for; the composition supplies the resolved variant.
		relayout();
		refresh_accessible_description();
	}

	void theme_preview_strip::set_current(const core::appearance_mode aMode)
	{
		if (Current == aMode) {
			return;
		}
		Current = aMode;
		// Deliberately does not scroll. This is the mirror of what the host
		// published, and a published adoption moving the page under the reviewer's
		// pointer is not the same gesture as choosing a tile.
		refresh_accessible_description();
		if (Canvas != nullptr) {
			Canvas->update();
		}
	}

	auto theme_preview_strip::current() const noexcept -> core::appearance_mode
	{
		return Current;
	}

	void theme_preview_strip::set_system_variant(
		const core::appearance_variant aVariant)
	{
		// The composition supplies this on every accepted appearance change, so a
		// repeated value must not ask for a repaint: the captions and both System
		// tiles are derived from it and none of them moved.
		if (System == aVariant) {
			return;
		}
		System = aVariant;
		refresh_notes();
		refresh_accessible_description();
	}

	auto theme_preview_strip::system_variant() const noexcept
		-> const std::optional<core::appearance_variant>&
	{
		return System;
	}

	void theme_preview_strip::set_customised_counts(
		std::array<std::size_t, core::appearance_variant_count> aCounts)
	{
		if (CustomisedCounts == aCounts) {
			return;
		}
		CustomisedCounts = aCounts;
		refresh_notes();
		refresh_accessible_description();
	}

	auto theme_preview_strip::customised_counts() const noexcept
		-> const std::array<std::size_t, core::appearance_variant_count>&
	{
		return CustomisedCounts;
	}

	void theme_preview_strip::set_theme_palettes(
		std::array<core::appearance_palette, core::appearance_variant_count>
			aPalettes)
	{
		if (ThemePalettes == aPalettes) {
			return;
		}
		ThemePalettes = std::move(aPalettes);
		if (Canvas != nullptr) {
			Canvas->update();
		}
	}

	void theme_preview_strip::set_white_space(const core::white_space_drawing aDrawing)
	{
		if (WhiteSpace == aDrawing) {
			return;
		}
		WhiteSpace = aDrawing;
		if (Canvas != nullptr) {
			Canvas->update();
		}
	}

	void theme_preview_strip::set_code_font(QFont aFont)
	{
		if (CodeFont == aFont) {
			return;
		}
		CodeFont = std::move(aFont);
		if (Canvas != nullptr) {
			Canvas->update();
		}
	}

	void theme_preview_strip::set_dispatch(theme_preview_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	auto theme_preview_strip::cards() const noexcept
		-> const std::array<theme_preview_card, theme_preview_count>&
	{
		return Cards;
	}

	auto theme_preview_strip::hit_test(const QPoint aPoint) const
		-> std::optional<std::size_t>
	{
		for (auto index = std::size_t{ 0U }; index < Cards.size(); ++index) {
			if (Cards[index].Rect.contains(aPoint)) {
				return index;
			}
		}
		return std::nullopt;
	}

	auto theme_preview_strip::tile_rect_in_strip(const std::size_t aCard) const
		-> QRect
	{
		if (aCard >= Cards.size() || Canvas == nullptr || Well == nullptr) {
			return { };
		}
		const auto tile  = QRect{ Canvas->mapTo(this, Cards[aCard].Rect.topLeft()),
								  Cards[aCard].Rect.size() };
		const auto shown = QRect{ Well->viewport()->mapTo(this, QPoint{ 0, 0 }),
								  Well->viewport()->size() };
		return tile.intersected(shown);
	}

	auto theme_preview_strip::well() const noexcept -> QScrollArea*
	{
		return Well;
	}

	auto theme_preview_strip::statistics() const noexcept
		-> theme_preview_statistics
	{
		return Statistics;
	}

	auto theme_preview_strip::sizeHint() const -> QSize
	{
		const auto frame = Well == nullptr ? 0 : 2 * Well->frameWidth();
		const auto bar   = Well == nullptr || Well->horizontalScrollBar() == nullptr
			? 0
			: Well->horizontalScrollBar()->sizeHint().height();
		const auto width = WellHorizontalInset
			+ static_cast<int>(
							   PreferredTiles
							   * static_cast<qreal>(theme_preview_tile_width + TileGap))
			+ WellHorizontalInset;
		return { width + frame,
				 WellTopInset + TileCardHeight + WellBottomInset + frame + bar };
	}

	auto theme_preview_strip::minimumSizeHint() const -> QSize
	{
		// One tile, so the page never has to widen for the strip: a strip that
		// demanded its own full width would push the whole Settings page into
		// sideways scrolling, which is exactly what the well exists to prevent.
		const auto frame = Well == nullptr ? 0 : 2 * Well->frameWidth();
		const auto bar   = Well == nullptr || Well->horizontalScrollBar() == nullptr
			? 0
			: Well->horizontalScrollBar()->sizeHint().height();
		return { WellHorizontalInset + theme_preview_tile_width
					 + WellHorizontalInset + frame,
				 WellTopInset + TileCardHeight + WellBottomInset + frame + bar };
	}

	void theme_preview_strip::relayout()
	{
		for (auto index = std::size_t{ 0U }; index < Cards.size(); ++index) {
			Cards[index].Rect = QRect{
				WellHorizontalInset
					+ static_cast<int>(index)
						* (theme_preview_tile_width + TileGap),
				WellTopInset,
				theme_preview_tile_width,
				TileCardHeight,
			};
		}
		refresh_notes();
		if (Canvas != nullptr) {
			Canvas->setFixedSize(
				WellHorizontalInset
					+ static_cast<int>(Cards.size())
						* (theme_preview_tile_width + TileGap)
					- TileGap + WellHorizontalInset,
				WellTopInset + TileCardHeight + WellBottomInset);
		}
		updateGeometry();
	}

	auto theme_preview_strip::card_variant(const std::size_t aCard) const
		-> std::optional<core::appearance_variant>
	{
		if (aCard >= Cards.size()) {
			return std::nullopt;
		}
		const auto mode = Cards[aCard].Mode;
		if (!follows_desktop(mode)) {
			return base_variant(mode);
		}
		if (!System.has_value()) {
			// Nobody has said what the desktop resolves, so this tile's theme is
			// not known here. Guessing one is what RFC-BPR-0034 prohibits.
			return std::nullopt;
		}
		if (!high_contrast_mode(mode)) {
			return System;
		}
		// The supplied variant carries the desktop's light-or-dark family, which is
		// the only part of it a high-contrast tile needs.
		return dark_family(*System)
			? core::appearance_variant::high_contrast_dark
			: core::appearance_variant::high_contrast_light;
	}

	void theme_preview_strip::refresh_notes()
	{
		auto changed = false;
		for (auto index = std::size_t{ 0U }; index < Cards.size(); ++index) {
			auto note = QString{ };
			if (follows_desktop(Cards[index].Mode) && System.has_value()) {
				note = dark_family(*System) ? QStringLiteral("dark now")
											: QStringLiteral("light now");
			}
			if (const auto variant = card_variant(index); variant.has_value()) {
				const auto customised = customised_note(
					CustomisedCounts[application::variant_index(*variant)]);
				if (!customised.isEmpty()) {
					note = note.isEmpty()
						? customised
						: note + QString::fromUtf8(NoteSeparator) + customised;
				}
			}
			if (Cards[index].Note != note) {
				Cards[index].Note = std::move(note);
				changed           = true;
			}
		}
		if (changed && Canvas != nullptr) {
			Canvas->update();
		}
	}

	void theme_preview_strip::refresh_accessible_description()
	{
		auto selected = QString{ };
		for (const auto& card : Cards) {
			if (card.Mode == Current) {
				selected = card.Note.isEmpty()
					? card.Name
					: card.Name + QStringLiteral(", ") + card.Note;
			}
		}
		setAccessibleDescription(
			QStringLiteral(
				"Six themes, each drawn from its own palette; Left and Right move "
				"between them and scroll the chosen one into view, Return chooses. "
				"Selected: %1")
				.arg(selected));
	}

	void theme_preview_strip::paint_tiles(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		if (Canvas == nullptr) {
			return;
		}
		auto        painter = QPainter{ Canvas };
		const auto& colours = palette();
		const auto  accent  = colours.color(QPalette::Active, QPalette::Highlight);
		const auto  rule    = colours.color(QPalette::Active, QPalette::Mid);
		const auto  ink     = colours.color(QPalette::Active, QPalette::WindowText);
		const auto  faint   = colours.color(
			QPalette::Active, QPalette::PlaceholderText);
		painter.fillRect(
			Canvas->rect(), colours.color(QPalette::Active, QPalette::Window));

		const auto sample = theme_tile_code_sample();
		auto       code   = CodeFont;
		code.setPointSizeF(std::min(code.pointSizeF(), TileCodePointSize));
		// The rows share a fixed part of the frame rather than following the font's
		// own line spacing, so which rows a tile shows does not depend on which
		// font the composition supplied.
		const auto options = code_sample_paint_options{
			.RowHeight = static_cast<qreal>(
							 theme_preview_tile_height - TileSampleBottomMargin)
				/ static_cast<qreal>(std::max<std::size_t>(sample.size(), 1U)),
			.WhiteSpace = WhiteSpace,
		};

		for (auto index = std::size_t{ 0U }; index < Cards.size(); ++index) {
			const auto& card     = Cards[index];
			const auto  selected = card.Mode == Current;
			const auto  frame    = QRectF{ card.Rect }
									   .adjusted(1.0, 1.0, -1.0, 0.0)
									   .adjusted(
										   0.0,
										   0.0,
										   0.0,
										   -static_cast<qreal>(
											   TileCardHeight
											   - theme_preview_tile_height + 1));
			auto        rounded  = QPainterPath{ };
			rounded.addRoundedRect(frame, 6.0, 6.0);

			painter.save();
			painter.setClipPath(rounded);
			Statistics.WhiteSpacePainted += paint_code_sample(
												painter,
												frame,
												sample,
												code_role_colors_of(
													ThemePalettes[application::variant_index(
														base_variant(card.Mode))]),
												code,
												options)
												.WhiteSpacePainted;
			if (const auto overlay = overlay_variant(card.Mode);
				overlay.has_value()) {
				// The split is a clip and nothing else. No edge is drawn, so what
				// separates the halves is the change of palette itself.
				auto triangle = QPainterPath{ };
				triangle.moveTo(frame.topLeft());
				triangle.lineTo(frame.topRight());
				triangle.lineTo(frame.bottomLeft());
				triangle.closeSubpath();
				painter.setClipPath(rounded.intersected(triangle));
				static_cast<void>(paint_code_sample(
					painter,
					frame,
					sample,
					code_role_colors_of(
						ThemePalettes[application::variant_index(*overlay)]),
					code,
					options));
				++Statistics.DiagonalTilesPainted;
			}
			painter.restore();

			painter.save();
			painter.setRenderHint(QPainter::Antialiasing, true);
			painter.setBrush(Qt::NoBrush);
			painter.setPen(QPen{ selected ? accent : rule, selected ? 2.0 : 1.0 });
			painter.drawRoundedRect(frame, 6.0, 6.0);
			if (selected && hasFocus()) {
				painter.setPen(QPen{ accent, 1.0, Qt::DotLine });
				painter.drawRoundedRect(
					frame.adjusted(-2.5, -2.5, 2.5, 2.5), 8.0, 8.0);
			}
			painter.restore();

			const auto name = QRect{ card.Rect.left(),
									 card.Rect.top() + theme_preview_tile_height
										 + TileNameGap,
									 card.Rect.width(),
									 TileNameHeight };
			painter.setFont(font());
			painter.setPen(selected ? accent : ink);
			painter.drawText(
				name,
				Qt::AlignLeft | Qt::AlignVCenter,
				QFontMetrics{ font() }.elidedText(
					card.Name, Qt::ElideRight, name.width()));
			if (!card.Note.isEmpty()) {
				auto small = font();
				small.setPointSizeF(small.pointSizeF() * 0.85);
				painter.setFont(small);
				painter.setPen(faint);
				painter.drawText(
					name.translated(0, TileNameHeight),
					Qt::AlignLeft | Qt::AlignVCenter,
					QFontMetrics{ small }.elidedText(
						card.Note, Qt::ElideRight, name.width()));
			}
			++Statistics.TilesPainted;
		}
	}

	void theme_preview_strip::ensure_visible(const std::size_t aCard)
	{
		if (aCard >= Cards.size() || Well == nullptr) {
			return;
		}
		auto* const bar    = Well->horizontalScrollBar();
		const auto  before = bar == nullptr ? 0 : bar->value();
		const auto  rect   = Cards[aCard].Rect;
		Well->ensureVisible(
			rect.center().x(), rect.center().y(), rect.width() / 2 + TileGap, 0);
		if (bar != nullptr && bar->value() != before) {
			++Statistics.ScrollsIntoView;
		}
	}

	void theme_preview_strip::choose(const std::size_t aCard)
	{
		if (aCard >= Cards.size()) {
			return;
		}
		Current = Cards[aCard].Mode;
		ensure_visible(aCard);
		refresh_accessible_description();
		if (Canvas != nullptr) {
			Canvas->update();
		}
		if (Dispatch) {
			Dispatch(Current);
		}
	}

	void theme_preview_strip::mousePressEvent(QMouseEvent* const aEvent)
	{
		if (aEvent->button() != Qt::LeftButton) {
			QWidget::mousePressEvent(aEvent);
			return;
		}
		// A press delivered to the strip rather than to the canvas is measured in
		// this widget's coordinates, and the tiles are laid out in the canvas's, so
		// the point is mapped rather than compared across two origins.
		const auto point = Canvas == nullptr
			? aEvent->position().toPoint()
			: Canvas->mapFrom(this, aEvent->position().toPoint());
		if (const auto hit = hit_test(point); hit.has_value()) {
			setFocus(Qt::MouseFocusReason);
			choose(*hit);
			aEvent->accept();
			return;
		}
		QWidget::mousePressEvent(aEvent);
	}

	void theme_preview_strip::keyPressEvent(QKeyEvent* const aEvent)
	{
		auto index = std::size_t{ 0U };
		for (auto candidate = std::size_t{ 0U }; candidate < Cards.size();
			 ++candidate) {
			if (Cards[candidate].Mode == Current) {
				index = candidate;
			}
		}
		switch (aEvent->key()) {
		case Qt::Key_Right:
			choose((index + 1U) % Cards.size());
			aEvent->accept();
			return;
		case Qt::Key_Left:
			choose((index + Cards.size() - 1U) % Cards.size());
			aEvent->accept();
			return;
		case Qt::Key_Return:
		case Qt::Key_Enter:
		case Qt::Key_Space:
			choose(index);
			aEvent->accept();
			return;
		default:
			break;
		}
		QWidget::keyPressEvent(aEvent);
	}
} // namespace bpr::qt
