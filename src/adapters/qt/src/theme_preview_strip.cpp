#include "bpr/qt/theme_preview_strip.hpp"

#include "bpr/qt/appearance_profile.hpp"

#include <QFont>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QResizeEvent>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <utility>

namespace bpr::qt {
	namespace {
		constexpr int card_width   = 150;
		constexpr int card_height  = 92;
		constexpr int preview_rows = 3;
		constexpr int card_gap     = 12;

		[[nodiscard]] auto role_colour(
			const core::appearance_palette& aPalette,
			const core::appearance_role     aRole) -> QColor
		{
			return to_presentation_color(aPalette[static_cast<std::size_t>(aRole)]);
		}

		/// \brief Paints three lines of code in one palette over a rect.
		void paint_sample(
			QPainter&                       aPainter,
			const QRectF&                   aRect,
			const core::appearance_palette& aPalette,
			const QFont&                    aFont)
		{
			aPainter.fillRect(aRect, role_colour(aPalette, core::appearance_role::code_background));
			const auto rowHeight = aRect.height() / preview_rows;
			const auto keyword   = role_colour(aPalette, core::appearance_role::syntax_keyword);
			const auto normal    = role_colour(aPalette, core::appearance_role::syntax_normal);
			const auto control   = role_colour(aPalette, core::appearance_role::syntax_control_flow);
			aPainter.setFont(aFont);
			// Row one: `if (!bUse)`.
			auto row = QRectF{ aRect.left() + 6.0, aRect.top(), aRect.width() - 6.0, rowHeight };
			aPainter.setPen(keyword);
			aPainter.drawText(row, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("if"));
			const auto ifWidth = QFontMetrics{ aFont }.horizontalAdvance(QStringLiteral("if "));
			aPainter.setPen(normal);
			aPainter.drawText(row.adjusted(ifWidth, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("(!bUse)"));
			// Row two: an added line, `  return;`.
			row.translate(0, rowHeight);
			aPainter.fillRect(QRectF{ aRect.left(), row.top(), aRect.width(), rowHeight },
							  role_colour(aPalette, core::appearance_role::diff_added_background));
			aPainter.setPen(control);
			aPainter.drawText(row.adjusted(10, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("return;"));
			// Row three: a closing brace.
			row.translate(0, rowHeight);
			aPainter.setPen(normal);
			aPainter.drawText(row, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("}"));
		}
	} // namespace

	theme_preview_strip::theme_preview_strip(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("appearanceThemePreviews"));
		setAccessibleName(QStringLiteral("Theme"));
		setAccessibleDescription(QStringLiteral(
			"System, Light, Dark or High contrast, each drawn from its own "
			"palette; Left and Right move between them, Return chooses"));
		setFocusPolicy(Qt::StrongFocus);
		Cards[0] = theme_preview_card{ .Mode = core::appearance_mode::system, .Name = QStringLiteral("System") };
		Cards[1] = theme_preview_card{ .Mode = core::appearance_mode::light, .Name = QStringLiteral("Light") };
		Cards[2] = theme_preview_card{ .Mode = core::appearance_mode::dark, .Name = QStringLiteral("Dark") };
		Cards[3] = theme_preview_card{
			.Mode = core::appearance_mode::high_contrast_system,
			.Name = QStringLiteral("High contrast"),
			.Note = QStringLiteral("follows desktop"),
		};
		set_system_variant(core::appearance_variant::light);
		relayout();
	}

	void theme_preview_strip::set_current(const core::appearance_mode aMode)
	{
		Current = aMode;
		update();
	}

	auto theme_preview_strip::current() const noexcept -> core::appearance_mode
	{
		return Current;
	}

	void theme_preview_strip::set_system_variant(const core::appearance_variant aVariant)
	{
		System        = aVariant;
		Cards[0].Note = aVariant == core::appearance_variant::dark
				|| aVariant == core::appearance_variant::high_contrast_dark
			? QStringLiteral("dark now")
			: QStringLiteral("light now");
		update();
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

	auto theme_preview_strip::hit_test(const QPoint aPoint) const -> std::optional<std::size_t>
	{
		for (std::size_t index = 0U; index < Cards.size(); ++index) {
			if (Cards[index].Rect.contains(aPoint)) {
				return index;
			}
		}
		return std::nullopt;
	}

	auto theme_preview_strip::sizeHint() const -> QSize
	{
		return { static_cast<int>(Cards.size()) * (card_width + card_gap), card_height + 36 };
	}

	auto theme_preview_strip::minimumSizeHint() const -> QSize
	{
		return { card_width, card_height + 36 };
	}

	void theme_preview_strip::relayout()
	{
		// Four across when there is room, else wrapped two by two.
		const auto perRow = std::max(1, std::min(static_cast<int>(Cards.size()), (width() + card_gap) / (card_width + card_gap)));
		for (std::size_t index = 0U; index < Cards.size(); ++index) {
			const auto column = static_cast<int>(index) % perRow;
			const auto row    = static_cast<int>(index) / perRow;
			Cards[index].Rect = QRect{ column * (card_width + card_gap), row * (card_height + 36 + card_gap),
									   card_width, card_height + 36 };
		}
		updateGeometry();
		update();
	}

	void theme_preview_strip::resizeEvent(QResizeEvent* const aEvent)
	{
		QWidget::resizeEvent(aEvent);
		relayout();
	}

	void theme_preview_strip::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto& colours = palette();
		const auto  accent  = colours.color(QPalette::Active, QPalette::Highlight);
		const auto  rule    = colours.color(QPalette::Active, QPalette::Mid);
		const auto  ink     = colours.color(QPalette::Active, QPalette::WindowText);
		const auto  faint   = colours.color(QPalette::Active, QPalette::PlaceholderText);
		auto        code    = font();
		code.setFamily(QStringLiteral("JetBrainsMonoNL Nerd Font"));
		code.setStyleHint(QFont::Monospace);
		code.setPointSizeF(code.pointSizeF() * 0.85);
		for (const auto& card : Cards) {
			const auto current = card.Mode == Current;
			const auto preview = QRectF{ card.Rect }.adjusted(0.5, 0.5, -0.5, -(36.0 + 0.5));
			painter.save();
			painter.setClipRect(preview);
			switch (card.Mode) {
			case core::appearance_mode::system: {
				// Half light, half dark, whichever the desktop is now on the left.
				const auto now   = System == core::appearance_variant::dark
					? core::appearance_variant::dark
					: core::appearance_variant::light;
				const auto other = now == core::appearance_variant::dark
					? core::appearance_variant::light
					: core::appearance_variant::dark;
				const auto half  = QRectF{ preview.left(), preview.top(), preview.width() / 2.0, preview.height() };
				paint_sample(painter, half, builtin_appearance_palette(now), code);
				paint_sample(painter, half.translated(preview.width() / 2.0, 0.0), builtin_appearance_palette(other), code);
				break;
			}
			case core::appearance_mode::light:
				paint_sample(painter, preview, builtin_appearance_palette(core::appearance_variant::light), code);
				break;
			case core::appearance_mode::dark:
				paint_sample(painter, preview, builtin_appearance_palette(core::appearance_variant::dark), code);
				break;
			case core::appearance_mode::high_contrast_system:
			case core::appearance_mode::high_contrast_light:
			case core::appearance_mode::high_contrast_dark:
				paint_sample(painter, preview,
							 builtin_appearance_palette(System == core::appearance_variant::dark
															? core::appearance_variant::high_contrast_dark
															: core::appearance_variant::high_contrast_light),
							 code);
				break;
			}
			painter.restore();
			painter.setBrush(Qt::NoBrush);
			painter.setPen(QPen{ current ? accent : rule, current ? 2.0 : 1.0 });
			painter.drawRoundedRect(preview, 6.0, 6.0);
			painter.setFont(font());
			painter.setPen(current ? accent : ink);
			const auto label = QRect{ card.Rect.left(), card.Rect.bottom() - 34, card.Rect.width(), 18 };
			painter.drawText(label, Qt::AlignLeft | Qt::AlignVCenter, card.Name);
			if (!card.Note.isEmpty()) {
				auto small = font();
				small.setPointSizeF(small.pointSizeF() * 0.85);
				painter.setFont(small);
				painter.setPen(faint);
				painter.drawText(label.translated(0, 16), Qt::AlignLeft | Qt::AlignVCenter, card.Note);
			}
			if (current && hasFocus()) {
				painter.setPen(QPen{ accent, 1.0, Qt::DotLine });
				painter.drawRoundedRect(QRectF{ card.Rect }.adjusted(-2.5, -2.5, 2.5, 2.5), 8.0, 8.0);
			}
		}
	}

	void theme_preview_strip::choose(const std::size_t aCard)
	{
		if (aCard >= Cards.size()) {
			return;
		}
		Current = Cards[aCard].Mode;
		update();
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
		if (const auto hit = hit_test(aEvent->position().toPoint()); hit.has_value()) {
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
		for (std::size_t candidate = 0U; candidate < Cards.size(); ++candidate) {
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
