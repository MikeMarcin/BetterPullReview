#include "bpr/qt/view_picker.hpp"

#include "bpr/qt/workbench_strip.hpp"

#include <QEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyleOptionFocusRect>

namespace bpr::qt {
	namespace {
		constexpr int segment_width  = 20;
		constexpr int segment_height = 22;

		/// One segment: a glyph on the picker's ground, raised when picked.
		class view_segment final : public QAbstractButton
		{
		public:
			view_segment(const strip_glyph aGlyph, QWidget* const aParent)
				: QAbstractButton{ aParent }
				, Glyph{ aGlyph }
			{
				setCheckable(true);
				setAutoExclusive(true);
				setFocusPolicy(Qt::TabFocus);
				setFixedSize(segment_width, segment_height);
				setAttribute(Qt::WA_Hover);
			}

			[[nodiscard]] auto sizeHint() const -> QSize override { return { segment_width, segment_height }; }

		protected:
			void paintEvent(QPaintEvent*) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing);
				const auto& colours = palette();
				const auto  box     = QRectF{ rect() }.adjusted(1.5, 1.5, -1.5, -1.5);
				if (isChecked()) {
					// Raised and neutral: the control's own face, a hairline and a soft shadow.
					auto shadow = colours.color(QPalette::Shadow);
					shadow.setAlpha(40);
					painter.setPen(Qt::NoPen);
					painter.setBrush(shadow);
					painter.drawRoundedRect(box.translated(0.0, 0.5), 4.0, 4.0);
					painter.setBrush(colours.color(QPalette::Button).lighter(colours.color(QPalette::Window).lightness() < 128 ? 140 : 108));
					auto edge = colours.color(QPalette::Mid);
					edge.setAlpha(150);
					painter.setPen(QPen{ edge, 0.5 });
					painter.drawRoundedRect(box, 4.0, 4.0);
				} else if (underMouse() && isEnabled()) {
					auto hover = colours.color(QPalette::WindowText);
					hover.setAlpha(18);
					painter.setPen(Qt::NoPen);
					painter.setBrush(hover);
					painter.drawRoundedRect(box, 4.0, 4.0);
				}
				const auto icon = strip_icon(Glyph, colours);
				icon.paint(&painter, QRect{ (width() - 14) / 2, (height() - 14) / 2, 14, 14 }, Qt::AlignCenter, isEnabled() ? QIcon::Normal : QIcon::Disabled);
				if (hasFocus()) {
					auto focus = QStyleOptionFocusRect{ };
					focus.initFrom(this);
					focus.rect = rect().adjusted(1, 1, -1, -1);
					style()->drawPrimitive(QStyle::PE_FrameFocusRect, &focus, &painter, this);
				}
			}

		private:
			strip_glyph Glyph;
		};
	}

	view_picker::view_picker(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("reviewViewPicker"));
		setAccessibleName(QStringLiteral("View"));
		const auto glyphs = std::array{ strip_glyph::two_panes, strip_glyph::one_pane, strip_glyph::edit_file };
		const auto names  = std::array{ QStringLiteral("Two Panes"), QStringLiteral("One Pane"), QStringLiteral("Edit") };
		const auto tips   = std::array{ QStringLiteral("Two panes: the comparison side by side"), QStringLiteral("One pane: both sides in one document"),
										QStringLiteral("Edit: your file as it is on disk, editable, with changes marked in the gutter") };
		const auto ids    = std::array{ QStringLiteral("reviewViewTwoPanes"), QStringLiteral("reviewViewOnePane"), QStringLiteral("reviewViewEdit") };
		for (std::size_t index = 0U; index < Segments.size(); ++index) {
			auto* const segment = new view_segment{ glyphs[index], this };
			segment->setObjectName(ids[index]);
			segment->setAccessibleName(names[index]);
			segment->setToolTip(tips[index]);
			segment->setProperty("bprBaseTip", tips[index]);
			Segments[index] = segment;
			connect(segment, &QAbstractButton::clicked, this, [this, index] {
				const auto picked = static_cast<review_view>(index);
				if (picked == View) {
					Segments[index]->setChecked(true);
					return;
				}
				View = picked;
				if (Picked) {
					Picked(picked);
				}
			});
		}
		Segments[0]->setChecked(true);
		setFixedSize(sizeHint());
		place();
		set_author(false);
	}

	void view_picker::set_view(const review_view aView)
	{
		View = aView;
		Segments[static_cast<std::size_t>(aView)]->setChecked(true);
		update();
	}

	auto view_picker::view() const noexcept -> review_view { return View; }

	void view_picker::set_author(const bool aAuthor)
	{
		Author = aAuthor;
		// The third segment's room is kept; it is shown only in author state.
		Segments[2]->setVisible(aAuthor);
		if (!aAuthor && View == review_view::edit) {
			set_view(review_view::two_panes);
		}
		// A window is in author state from its start, so the width follows
		// the segments shown and the review bar keeps its room.
		setFixedSize(sizeHint());
		update();
	}

	auto view_picker::author() const noexcept -> bool { return Author; }

	void view_picker::set_edit_refusal(const QString& aReason)
	{
		auto* const edit = Segments[2];
		edit->setEnabled(aReason.isEmpty());
		edit->setToolTip(aReason.isEmpty() ? edit->property("bprBaseTip").toString() : QStringLiteral("Edit is unavailable: %1").arg(aReason));
		edit->setAccessibleDescription(edit->toolTip());
	}

	auto view_picker::segment(const review_view aView) const -> QAbstractButton* { return Segments[static_cast<std::size_t>(aView)]; }

	void view_picker::set_picked(std::function<void(review_view)> aPicked) { Picked = std::move(aPicked); }

	auto view_picker::sizeHint() const -> QSize { return { segment_width * (Author ? 3 : 2) + 2, segment_height + 2 }; }

	auto view_picker::minimumSizeHint() const -> QSize { return sizeHint(); }

	void view_picker::place()
	{
		for (std::size_t index = 0U; index < Segments.size(); ++index) {
			Segments[index]->move(1 + static_cast<int>(index) * segment_width, 1);
		}
	}

	void view_picker::paintEvent(QPaintEvent*)
	{
		// The well the segments sit in: as wide as the segments shown.
		auto       painter = QPainter{ this };
		const auto shown   = Author ? 3 : 2;
		painter.setRenderHint(QPainter::Antialiasing);
		auto ground = palette().color(QPalette::WindowText);
		ground.setAlpha(14);
		painter.setPen(Qt::NoPen);
		painter.setBrush(ground);
		painter.drawRoundedRect(QRectF{ 0.5, 0.5, static_cast<qreal>(shown * segment_width + 1), static_cast<qreal>(segment_height + 1) }, 5.0, 5.0);
	}

	void view_picker::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange) {
			restyle();
		}
	}

	void view_picker::restyle()
	{
		for (auto* const segment : Segments) {
			segment->update();
		}
		update();
	}
}
