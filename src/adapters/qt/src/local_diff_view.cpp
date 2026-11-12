#include "bpr/qt/local_diff_view.hpp"

#include <QEvent>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollBar>

#include <algorithm>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto text_of(const std::string& aText) -> QString { return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size())); }

		[[nodiscard]] auto dark(const QPalette& aPalette) -> bool { return aPalette.color(QPalette::Window).lightness() < 128; }

		[[nodiscard]] auto removed_wash(const QPalette& aPalette) -> QColor { return dark(aPalette) ? QColor{ 94, 38, 40 } : QColor{ 255, 235, 233 }; }
		[[nodiscard]] auto added_wash(const QPalette& aPalette) -> QColor { return dark(aPalette) ? QColor{ 33, 74, 45 } : QColor{ 230, 255, 236 }; }

		[[nodiscard]] auto quiet_ink(const QPalette& aPalette) -> QColor
		{
			auto ink = aPalette.color(QPalette::WindowText);
			ink.setAlpha(140);
			return ink;
		}
	}

	local_diff_view::local_diff_view(QWidget* const aParent)
		: QAbstractScrollArea{ aParent }
	{
		setObjectName(QStringLiteral("localDiffView"));
		setAccessibleName(QStringLiteral("Comparison"));
		setFrameShape(QFrame::NoFrame);
		setFocusPolicy(Qt::StrongFocus);
		Code = QFontDatabase::systemFont(QFontDatabase::FixedFont);
		viewport()->setMouseTracking(false);
	}

	void local_diff_view::present(std::vector<core::cycle_diff_row> aRows, std::vector<local_diff_band> aBands, QString aLeft, QString aPath, QString aRight)
	{
		Rows   = std::move(aRows);
		Bands  = std::move(aBands);
		Left   = std::move(aLeft);
		Path   = std::move(aPath);
		Right  = std::move(aRight);
		Status.clear();
		Anchor.reset();
		Cursor.reset();
		layout_lines();
		viewport()->update();
	}

	void local_diff_view::clear(const QString& aStatus)
	{
		present({ }, { }, { }, { }, { });
		Status = aStatus;
		viewport()->update();
	}

	void local_diff_view::set_one_pane(const bool aOnePane)
	{
		if (OnePane == aOnePane) {
			return;
		}
		OnePane = aOnePane;
		layout_lines();
		viewport()->update();
	}

	auto local_diff_view::one_pane() const noexcept -> bool { return OnePane; }

	void local_diff_view::set_code_font(const QFont& aFont)
	{
		Code = aFont;
		layout_lines();
		viewport()->update();
	}

	void local_diff_view::set_action(std::function<void(std::size_t, bool)> aAction) { Action = std::move(aAction); }

	auto local_diff_view::band_buttons() const -> std::vector<QPushButton*> { return Buttons; }

	auto local_diff_view::rows() const noexcept -> const std::vector<core::cycle_diff_row>& { return Rows; }

	auto local_diff_view::header_text() const -> QString { return QStringList{ Left, Path, Right }.join(QLatin1Char{ '\t' }); }

	auto local_diff_view::text() const -> QString
	{
		auto lines = QStringList{ };
		for (const auto& row : Rows) {
			const auto mark = row.Kind == core::cycle_diff_row_kind::removed ? QLatin1Char{ '-' } : row.Kind == core::cycle_diff_row_kind::added ? QLatin1Char{ '+' }
																																				 : QLatin1Char{ ' ' };
			lines << QString{ mark } + text_of(row.Text);
		}
		return lines.join(QLatin1Char{ '\n' });
	}

	auto local_diff_view::line_height() const -> int { return QFontMetrics{ Code }.height() + 4; }
	auto local_diff_view::band_height() const -> int { return std::max(30, fontMetrics().height() + 14); }
	auto local_diff_view::header_height() const -> int { return std::max(32, fontMetrics().height() + 14); }

	void local_diff_view::layout_lines()
	{
		Lines.clear();
		auto bandAt = [this](const std::size_t aRow) {
			for (std::size_t index = 0U; index < Bands.size(); ++index) {
				if (Bands[index].Row == aRow) {
					Lines.push_back({ .Band = index });
				}
			}
		};
		for (std::size_t row = 0U; row < Rows.size();) {
			bandAt(row);
			if (OnePane || Rows[row].Kind == core::cycle_diff_row_kind::context) {
				const auto kind = Rows[row].Kind;
				Lines.push_back({ .Left = kind == core::cycle_diff_row_kind::added ? std::nullopt : std::optional{ row }, .Right = kind == core::cycle_diff_row_kind::removed ? std::nullopt : std::optional{ row } });
				++row;
				continue;
			}
			auto removed = std::vector<std::size_t>{ };
			auto added   = std::vector<std::size_t>{ };
			while (row < Rows.size() && Rows[row].Kind == core::cycle_diff_row_kind::removed) {
				removed.push_back(row++);
			}
			while (row < Rows.size() && Rows[row].Kind == core::cycle_diff_row_kind::added) {
				added.push_back(row++);
			}
			for (std::size_t index = 0U; index < std::max(removed.size(), added.size()); ++index) {
				Lines.push_back({ .Left = index < removed.size() ? std::optional{ removed[index] } : std::nullopt, .Right = index < added.size() ? std::optional{ added[index] } : std::nullopt });
			}
		}
		bandAt(Rows.size());
		Tops.assign(Lines.size() + 1U, 0);
		for (std::size_t index = 0U; index < Lines.size(); ++index) {
			Tops[index + 1U] = Tops[index] + (Lines[index].Band ? band_height() : line_height());
		}
		for (auto* const button : Buttons) {
			button->deleteLater();
		}
		Buttons.clear();
		for (const auto& band : Bands) {
			auto* const button = new QPushButton{ band.Action, viewport() };
			button->setObjectName(band.Stage ? QStringLiteral("localDiffStageChange") : QStringLiteral("localDiffUnstageChange"));
			button->setVisible(!band.Action.isEmpty());
			button->setToolTip(band.Stage ? QStringLiteral("Stage exactly this change; the rest of the file stays as it is in the index") : QStringLiteral("Return exactly this change to the last commit's version in the index"));
			// One width for both words, so a band's button never changes size.
			button->setMinimumWidth(std::max(button->fontMetrics().horizontalAdvance(QStringLiteral("Unstage Change")) + 28, button->sizeHint().width()));
			connect(button, &QPushButton::clicked, this, [this, change = band.Change, stage = band.Stage] {
				if (Action) {
					Action(change, stage);
				}
			});
			Buttons.push_back(button);
		}
		update_scrollbars();
		place_buttons();
	}

	void local_diff_view::update_scrollbars()
	{
		const auto total = Tops.empty() ? 0 : Tops.back();
		verticalScrollBar()->setRange(0, std::max(0, total - (viewport()->height() - header_height())));
		verticalScrollBar()->setPageStep(viewport()->height());
		verticalScrollBar()->setSingleStep(line_height());
	}

	auto local_diff_view::top_of(const std::size_t aLine) const -> int { return header_height() + Tops[aLine] - verticalScrollBar()->value(); }

	auto local_diff_view::line_at(const int aY) const -> std::optional<std::size_t>
	{
		const auto y = aY - header_height() + verticalScrollBar()->value();
		if (y < 0 || Lines.empty()) {
			return std::nullopt;
		}
		const auto found = std::upper_bound(Tops.begin(), Tops.end(), y);
		if (found == Tops.begin() || found == Tops.end()) {
			return std::nullopt;
		}
		return static_cast<std::size_t>(std::distance(Tops.begin(), found) - 1);
	}

	auto local_diff_view::right_line_of(const std::size_t aLine) const -> std::optional<std::uint32_t>
	{
		if (aLine >= Lines.size() || !Lines[aLine].Right) {
			return std::nullopt;
		}
		return Rows[*Lines[aLine].Right].RightLine;
	}

	void local_diff_view::place_buttons()
	{
		for (std::size_t index = 0U; index < Lines.size(); ++index) {
			if (!Lines[index].Band) {
				continue;
			}
			auto* const button = Buttons[*Lines[index].Band];
			const auto  top    = top_of(index);
			const auto  size   = button->sizeHint().expandedTo(QSize{ button->minimumWidth(), 0 });
			button->setGeometry(viewport()->width() - size.width() - 10, top + (band_height() - size.height()) / 2, size.width(), size.height());
			button->setVisible(!Bands[*Lines[index].Band].Action.isEmpty() && top + band_height() > header_height() && top < viewport()->height());
		}
	}

	void local_diff_view::paintEvent(QPaintEvent*)
	{
		auto        painter = QPainter{ viewport() };
		const auto& colours = palette();
		const auto  width   = viewport()->width();
		painter.fillRect(viewport()->rect(), colours.color(QPalette::Base));
		const auto metrics = QFontMetrics{ Code };
		const auto gutter  = metrics.horizontalAdvance(QStringLiteral("00000")) + 12;
		const auto half    = OnePane ? width : width / 2;
		const auto height  = line_height();
		painter.setFont(Code);
		for (std::size_t index = 0U; index < Lines.size(); ++index) {
			const auto top = top_of(index);
			if (top + (Lines[index].Band ? band_height() : height) < header_height() || top > viewport()->height()) {
				continue;
			}
			const auto& drawn = Lines[index];
			if (drawn.Band) {
				const auto& band = Bands[*drawn.Band];
				auto        rule = colours.color(QPalette::Mid);
				rule.setAlpha(90);
				painter.fillRect(QRect{ 0, top, width, band_height() }, colours.color(QPalette::Window));
				painter.fillRect(QRect{ 0, top, width, 1 }, rule);
				painter.fillRect(QRect{ 0, top + band_height() - 1, width, 1 }, rule);
				painter.setFont(font());
				painter.setPen(quiet_ink(colours));
				auto x = gutter + 12;
				painter.drawText(QRect{ x, top, width, band_height() }, Qt::AlignVCenter | Qt::AlignLeft, band.Title);
				x += painter.fontMetrics().horizontalAdvance(band.Title) + 14;
				if (!band.State.isEmpty()) {
					painter.setRenderHint(QPainter::Antialiasing);
					painter.setPen(Qt::NoPen);
					painter.setBrush(dark(colours) ? QColor{ 63, 185, 80 } : QColor{ 26, 127, 55 });
					painter.drawEllipse(QPointF{ static_cast<qreal>(x + 4), static_cast<qreal>(top) + band_height() / 2.0 }, 4.0, 4.0);
					painter.setPen(quiet_ink(colours));
					painter.drawText(QRect{ x + 14, top, width, band_height() }, Qt::AlignVCenter | Qt::AlignLeft, band.State);
					painter.setRenderHint(QPainter::Antialiasing, false);
				}
				painter.setFont(Code);
				continue;
			}
			const auto side = [&](const std::optional<std::size_t>& aRow, const int aLeft, const int aWidth, const bool aRightSide) {
				const auto area = QRect{ aLeft, top, aWidth, height };
				if (!aRow) {
					// Hatching where the other side has lines this one lacks.
					painter.fillRect(area, colours.color(QPalette::Base));
					auto hatch = colours.color(QPalette::Mid);
					hatch.setAlpha(45);
					painter.setPen(hatch);
					for (auto x = aLeft - height; x < aLeft + aWidth; x += 7) {
						painter.drawLine(std::max(x, aLeft), top + height - std::max(0, aLeft - x), std::min(x + height, aLeft + aWidth), top + std::max(0, x + height - (aLeft + aWidth)));
					}
					return;
				}
				const auto& row = Rows[*aRow];
				if (row.Kind == core::cycle_diff_row_kind::removed) {
					painter.fillRect(area, removed_wash(colours));
				} else if (row.Kind == core::cycle_diff_row_kind::added) {
					painter.fillRect(area, added_wash(colours));
				}
				const auto number = aRightSide ? row.RightLine : row.LeftLine;
				painter.setPen(quiet_ink(colours));
				if (number) {
					painter.drawText(QRect{ aLeft, top, gutter - 8, height }, Qt::AlignVCenter | Qt::AlignRight, QString::number(*number));
				}
				painter.setPen(colours.color(QPalette::Text));
				auto text = text_of(row.Text);
				text.replace(QLatin1Char{ '\t' }, QStringLiteral("    "));
				painter.drawText(QRect{ aLeft + gutter, top, aWidth - gutter - 4, height }, Qt::AlignVCenter | Qt::AlignLeft | Qt::TextSingleLine, text);
			};
			if (OnePane) {
				const auto row = drawn.Left ? drawn.Left : drawn.Right;
				side(row, 0, width, Rows[*row].Kind != core::cycle_diff_row_kind::removed);
			} else {
				side(drawn.Left, 0, half, false);
				side(drawn.Right, half, width - half, true);
				auto divider = colours.color(QPalette::Mid);
				divider.setAlpha(110);
				painter.fillRect(QRect{ half, top, 1, height }, divider);
			}
			if (Anchor && Cursor && index >= std::min(*Anchor, *Cursor) && index <= std::max(*Anchor, *Cursor) && drawn.Right) {
				auto selection = colours.color(QPalette::Highlight);
				selection.setAlpha(60);
				painter.fillRect(QRect{ OnePane ? 0 : half, top, OnePane ? width : width - half, height }, selection);
			}
		}
		if (Lines.empty() && !Status.isEmpty()) {
			painter.setFont(font());
			painter.setPen(quiet_ink(colours));
			painter.drawText(viewport()->rect(), Qt::AlignCenter, Status);
		}
		// The file header: left side's name, the path, and the right side's.
		painter.setFont(font());
		painter.fillRect(QRect{ 0, 0, width, header_height() }, colours.color(QPalette::Window));
		auto rule = colours.color(QPalette::Mid);
		rule.setAlpha(110);
		painter.fillRect(QRect{ 0, header_height() - 1, width, 1 }, rule);
		painter.setPen(quiet_ink(colours));
		painter.drawText(QRect{ 12, 0, width / 3, header_height() }, Qt::AlignVCenter | Qt::AlignLeft, Left);
		painter.drawText(QRect{ width * 2 / 3, 0, width / 3 - 12, header_height() }, Qt::AlignVCenter | Qt::AlignRight, Right);
		painter.setPen(colours.color(QPalette::WindowText));
		painter.drawText(QRect{ 0, 0, width, header_height() }, Qt::AlignCenter, Path);
	}

	void local_diff_view::resizeEvent(QResizeEvent* const aEvent)
	{
		QAbstractScrollArea::resizeEvent(aEvent);
		update_scrollbars();
		place_buttons();
	}

	void local_diff_view::scrollContentsBy(const int, const int)
	{
		place_buttons();
		viewport()->update();
	}

	void local_diff_view::mousePressEvent(QMouseEvent* const aEvent)
	{
		const auto at = line_at(aEvent->position().toPoint().y());
		Anchor        = at && right_line_of(*at) ? at : std::nullopt;
		Cursor        = Anchor;
		viewport()->update();
		QAbstractScrollArea::mousePressEvent(aEvent);
	}

	void local_diff_view::mouseMoveEvent(QMouseEvent* const aEvent)
	{
		if (Anchor && (aEvent->buttons() & Qt::LeftButton) != 0) {
			if (const auto at = line_at(aEvent->position().toPoint().y()); at) {
				Cursor = at;
				viewport()->update();
			}
		}
		QAbstractScrollArea::mouseMoveEvent(aEvent);
	}

	void local_diff_view::changeEvent(QEvent* const aEvent)
	{
		QAbstractScrollArea::changeEvent(aEvent);
		if (aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::StyleChange) {
			layout_lines();
		}
		if (aEvent->type() == QEvent::PaletteChange) {
			viewport()->update();
		}
	}

	auto local_diff_view::selected_lines() const -> std::optional<std::pair<std::uint32_t, std::uint32_t>>
	{
		if (!Anchor || !Cursor) {
			return std::nullopt;
		}
		auto first = std::optional<std::uint32_t>{ };
		auto last  = std::optional<std::uint32_t>{ };
		for (auto index = std::min(*Anchor, *Cursor); index <= std::max(*Anchor, *Cursor); ++index) {
			if (const auto line = right_line_of(index)) {
				first = first ? std::min(*first, *line) : *line;
				last  = last ? std::max(*last, *line) : *line;
			}
		}
		if (!first) {
			return std::nullopt;
		}
		return std::pair{ *first, *last };
	}

	void local_diff_view::reveal(const std::uint32_t aFirst, const std::uint32_t aLast)
	{
		Anchor.reset();
		Cursor.reset();
		for (std::size_t index = 0U; index < Lines.size(); ++index) {
			const auto line = right_line_of(index);
			if (line && *line >= aFirst && *line <= aLast) {
				Anchor = Anchor ? Anchor : std::optional{ index };
				Cursor = index;
			}
		}
		if (Anchor) {
			verticalScrollBar()->setValue(std::max(0, Tops[*Anchor] - line_height() * 3));
		}
		viewport()->update();
	}
}
