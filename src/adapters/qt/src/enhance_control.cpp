#include "bpr/qt/enhance_control.hpp"

#include "bpr/core/review_preparation.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "painted_glyph_icon.hpp"

#include <QAbstractButton>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QTextCursor>
#include <QTransform>
#include <QVBoxLayout>

#include <atomic>
#include <utility>

namespace bpr::qt {
	namespace {
		std::atomic<enhance_glyph_style> CurrentStyle{ enhance_glyph_style::nib_cut };
		std::atomic<std::uint64_t>       Sequence{ 0U };

		/// The nib upright, its tip at the bottom of a 24-unit square: the
		/// shoulders, the body to the tip, and the grip above. Each option
		/// turns it 45 degrees so the tip points to the lower left, as a pen
		/// held to write.
		[[nodiscard]] auto nib_outline_path(const qreal aUnit) -> QPainterPath
		{
			const auto at   = [aUnit](const qreal aX, const qreal aY) { return QPointF{ aX * aUnit, aY * aUnit }; };
			auto       path = QPainterPath{ at(12.0, 22.5) };
			path.lineTo(at(6.0, 12.6));
			path.lineTo(at(7.4, 8.6));
			path.lineTo(at(16.6, 8.6));
			path.lineTo(at(18.0, 12.6));
			path.closeSubpath();
			auto grip = QPainterPath{ };
			grip.addRoundedRect(QRectF{ at(8.4, 3.2), at(15.6, 7.6) }, 1.2 * aUnit, 1.2 * aUnit);
			return path.united(grip);
		}

		[[nodiscard]] auto spark_path(const qreal aUnit, const qreal aX, const qreal aY, const qreal aReach) -> QPainterPath
		{
			auto path = QPainterPath{ QPointF{ aX * aUnit, (aY - aReach) * aUnit } };
			path.quadTo(QPointF{ aX * aUnit, aY * aUnit }, QPointF{ (aX + aReach) * aUnit, aY * aUnit });
			path.quadTo(QPointF{ aX * aUnit, aY * aUnit }, QPointF{ aX * aUnit, (aY + aReach) * aUnit });
			path.quadTo(QPointF{ aX * aUnit, aY * aUnit }, QPointF{ (aX - aReach) * aUnit, aY * aUnit });
			path.quadTo(QPointF{ aX * aUnit, aY * aUnit }, QPointF{ aX * aUnit, (aY - aReach) * aUnit });
			path.closeSubpath();
			return path;
		}

		/// Draws one option `aSide` logical pixels square at the origin.
		void paint_glyph(QPainter& aPainter, const qreal aSide, const QColor& aInk, const enhance_glyph_style aStyle)
		{
			const auto unit = aSide / 24.0;
			const auto turn = [&](QPainterPath aPath, const qreal aScale, const QPointF aShift) {
				auto transform = QTransform{ };
				transform.translate((12.0 + aShift.x()) * unit, (12.0 + aShift.y()) * unit);
				transform.rotate(45.0);
				transform.scale(aScale, aScale);
				transform.translate(-12.0 * unit, -12.0 * unit);
				return transform.map(aPath);
			};
			// The slit from the tip to the breather hole.
			auto slit = QPainterPath{ };
			slit.addRoundedRect(QRectF{ QPointF{ 11.35 * unit, 15.0 * unit }, QPointF{ 12.65 * unit, 23.5 * unit } }, 0.6 * unit, 0.6 * unit);
			switch (aStyle) {
			case enhance_glyph_style::nib_cut: {
				// The owner's picture: one solid nib, the spark and the slit
				// cut out of it.
				auto body = nib_outline_path(unit).subtracted(slit).subtracted(spark_path(unit, 12.0, 12.0, 4.0));
				aPainter.setPen(Qt::NoPen);
				aPainter.setBrush(aInk);
				aPainter.drawPath(turn(body, 1.22, QPointF{ 0.6, -0.6 }));
				break;
			}
			case enhance_glyph_style::nib_outline: {
				// The nib in outline, slit and hole, smaller, and the spark
				// over its grip.
				aPainter.setPen(QPen{ aInk, aSide / 14.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
				aPainter.setBrush(Qt::NoBrush);
				auto lines = nib_outline_path(unit);
				auto cut   = QPainterPath{ QPointF{ 12.0 * unit, 22.0 * unit } };
				cut.lineTo(QPointF{ 12.0 * unit, 15.4 * unit });
				lines.addPath(cut);
				aPainter.drawPath(turn(lines, 0.98, QPointF{ -1.0, 1.0 }));
				aPainter.setPen(Qt::NoPen);
				aPainter.setBrush(aInk);
				aPainter.drawPath(spark_path(unit, 19.2, 4.8, 4.4));
				break;
			}
			case enhance_glyph_style::nib_spark: {
				// A solid nib with its slit, smaller, and the spark beside it.
				aPainter.setPen(Qt::NoPen);
				aPainter.setBrush(aInk);
				aPainter.drawPath(turn(nib_outline_path(unit).subtracted(slit), 0.98, QPointF{ -1.0, 1.0 }));
				aPainter.drawPath(spark_path(unit, 19.2, 4.8, 4.4));
				break;
			}
			}
		}

		[[nodiscard]] auto noun(const enhance_surface aSurface) -> QString
		{
			switch (aSurface) {
			case enhance_surface::comment: return QObject::tr("comment");
			case enhance_surface::reply:   return QObject::tr("reply");
			case enhance_surface::note:    return QObject::tr("note");
			}
			return QObject::tr("text");
		}

		[[nodiscard]] auto utf8_offset(const QString& aText, const int aPosition) -> std::size_t { return static_cast<std::size_t>(aText.left(aPosition).toUtf8().size()); }
	} // namespace

	auto enhance_glyph_style_name(const enhance_glyph_style aStyle) -> QString
	{
		switch (aStyle) {
		case enhance_glyph_style::nib_cut:     return QObject::tr("Nib with the spark cut out");
		case enhance_glyph_style::nib_outline: return QObject::tr("Nib outline and spark");
		case enhance_glyph_style::nib_spark:   return QObject::tr("Solid nib and spark");
		}
		return { };
	}

	auto enhance_tooltip(const enhance_surface aSurface, const bool aSelection) -> QString
	{
		return aSelection ? QObject::tr("Enhance writing for the selected text with agent.") : QObject::tr("Enhance writing for this %1 with agent.").arg(noun(aSurface));
	}

	void set_enhance_glyph_style(const enhance_glyph_style aStyle) { CurrentStyle.store(aStyle); }
	auto current_enhance_glyph_style() -> enhance_glyph_style { return CurrentStyle.load(); }

	auto enhance_icon(const QPalette& aPalette, const std::optional<enhance_glyph_style> aStyle) -> QIcon
	{
		const auto style = aStyle.value_or(current_enhance_glyph_style());
		return painted_glyph_icon([style](QPainter& aPainter, const qreal aSide, const QColor& aInk) { paint_glyph(aPainter, aSide, aInk, style); },
								  aPalette.color(QPalette::Active, QPalette::ButtonText), aPalette.color(QPalette::Disabled, QPalette::ButtonText));
	}

	enhance_button::enhance_button(const enhance_surface aSurface, QWidget* aParent)
		: QToolButton{ aParent }
		, Surface{ aSurface }
	{
		setObjectName(aSurface == enhance_surface::note ? QStringLiteral("noteEnhance") : QStringLiteral("commentEnhance"));
		setAutoRaise(true);
		setFocusPolicy(Qt::StrongFocus);
		setCursor(Qt::PointingHandCursor);
		setAttribute(Qt::WA_Hover, true);
		setToolButtonStyle(Qt::ToolButtonIconOnly);
		setIconSize(QSize{ 16, 16 });
		set_running(false);
	}

	void enhance_button::set_running(const bool aRunning)
	{
		Running         = aRunning;
		const auto what = noun(Surface);
		setAccessibleName(aRunning ? tr("Stop enhancing") : tr("Enhance writing"));
		setToolTip(aRunning ? tr("Stop enhancing. Your %1 stays as it is.").arg(what) : enhance_tooltip(Surface, Selection));
		setAccessibleDescription(aRunning ? toolTip() : toolTip() + QLatin1Char(' ') + tr("You choose whether to use what it suggests; nothing is published."));
		restyle();
	}

	void enhance_button::set_selection(const bool aSelection)
	{
		if (Selection != aSelection) {
			Selection = aSelection;
			set_running(Running);
		}
	}

	auto enhance_button::sizeHint() const -> QSize { return { 26, 24 }; }

	void enhance_button::restyle()
	{
		if (Restyling) {
			return;
		}
		Restyling         = true;
		const auto ground = parentWidget() != nullptr ? parentWidget()->palette() : palette();
		// [[BPR-2000]]. It starts a run, so it wears the accent; while it
		// stops one, the ordinary roles.
		if (Running) {
			wear_ordinary_roles(*this);
		} else {
			wear_agent_accent(*this, ground);
		}
		setIcon(enhance_icon(palette()));
		Restyling = false;
		update();
	}

	void enhance_button::changeEvent(QEvent* aEvent)
	{
		QToolButton::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange) {
			restyle();
		}
	}

	void enhance_button::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto box = QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0);
		const auto ink = palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::ButtonText);
		if ((underMouse() || isDown()) && isEnabled()) {
			auto wash = ink;
			wash.setAlphaF(isDown() ? 0.16F : 0.10F);
			painter.setPen(Qt::NoPen);
			painter.setBrush(wash);
			painter.drawRoundedRect(box, 5.0, 5.0);
		}
		const auto side  = 16.0;
		const auto glyph = QRectF{ (width() - side) / 2.0, (height() - side) / 2.0, side, side };
		if (Running) {
			// A stop square, in the ordinary ink.
			painter.setPen(Qt::NoPen);
			painter.setBrush(ink);
			painter.drawRoundedRect(glyph.adjusted(4.0, 4.0, -4.0, -4.0), 1.5, 1.5);
		} else {
			painter.save();
			painter.translate(glyph.topLeft());
			paint_glyph(painter, side, ink, current_enhance_glyph_style());
			painter.restore();
		}
		if (hasFocus()) {
			painter.setPen(QPen{ palette().color(QPalette::Active, QPalette::Highlight), 1.5 });
			painter.setBrush(Qt::NoBrush);
			painter.drawRoundedRect(box, 5.0, 5.0);
		}
	}

	enhance_suggestion::enhance_suggestion(const enhance_surface aSurface, QWidget* aParent)
		: QFrame{ aParent }
		, Surface{ aSurface }
	{
		setObjectName(aSurface == enhance_surface::note ? QStringLiteral("noteEnhanceSuggestion") : QStringLiteral("commentEnhanceSuggestion"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(12, 8, 10, 8);
		column->setSpacing(6);
		Heading = new QLabel{ this };
		Heading->setObjectName(QStringLiteral("enhanceHeading"));
		Heading->setTextFormat(Qt::PlainText);
		auto bold = font();
		bold.setWeight(QFont::DemiBold);
		Heading->setFont(bold);
		column->addWidget(Heading);
		Text = new QLabel{ this };
		Text->setObjectName(QStringLiteral("enhanceText"));
		Text->setTextFormat(Qt::PlainText);
		Text->setWordWrap(true);
		Text->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		Text->setAccessibleName(tr("Suggested text"));
		column->addWidget(Text);
		Notice = new QLabel{ this };
		Notice->setObjectName(QStringLiteral("enhanceNotice"));
		Notice->setTextFormat(Qt::PlainText);
		Notice->setWordWrap(true);
		Notice->setForegroundRole(QPalette::PlaceholderText);
		column->addWidget(Notice);
		hide();
	}

	void enhance_suggestion::set_buttons(QAbstractButton* aUse, QAbstractButton* aDiscard)
	{
		Use             = aUse;
		Discard         = aDiscard;
		auto* const row = new QHBoxLayout{ };
		row->setSpacing(6);
		row->addStretch(1);
		Discard->setParent(this);
		Use->setParent(this);
		row->addWidget(Discard);
		row->addWidget(Use);
		static_cast<QVBoxLayout*>(layout())->addLayout(row);
		Use->hide();
		Discard->hide();
	}

	void enhance_suggestion::show_running()
	{
		Heading->setText(tr("Enhancing…"));
		Text->clear();
		Text->hide();
		Notice->setText(tr("The agent is improving your %1. It stays as it is; you choose whether to use what comes back.").arg(noun(Surface)));
		Notice->show();
		if (Use != nullptr) {
			Use->hide();
			Discard->hide();
		}
		setAccessibleName(Heading->text());
		show();
	}

	void enhance_suggestion::show_suggestion(const QString& aText, const bool aSelection)
	{
		Heading->setText(aSelection ? tr("Suggested rewrite of your selection") : tr("Suggested %1").arg(noun(Surface)));
		Text->setText(aText);
		Text->show();
		Notice->clear();
		Notice->hide();
		if (Use != nullptr) {
			Use->show();
			Use->setEnabled(true);
			Discard->show();
		}
		setAccessibleName(Heading->text());
		show();
	}

	void enhance_suggestion::show_notice(const QString& aText)
	{
		Notice->setText(aText);
		Notice->setVisible(!aText.isEmpty());
		if (Text->text().isEmpty()) {
			Heading->setText(tr("Enhance"));
			Text->hide();
			if (Use != nullptr) {
				Use->hide();
				Discard->show();
			}
		}
		show();
	}

	void enhance_suggestion::clear()
	{
		Heading->clear();
		Text->clear();
		Notice->clear();
		hide();
	}

	auto enhance_suggestion::suggestion_text() const -> QString { return Text->text(); }
	auto enhance_suggestion::notice_text() const -> QString { return Notice->text(); }

	void enhance_suggestion::paintEvent(QPaintEvent* /*aEvent*/)
	{
		// A quiet panel under the field, its left edge in the Agent accent:
		// what is inside came from the agent and is not yet your text.
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto accent = agent_accent(palette());
		auto       wash   = palette().color(QPalette::Active, QPalette::Base);
		const auto mix    = [](const QColor& aBase, const QColor& aTint, const float aAmount) {
			return QColor::fromRgbF((aBase.redF() * (1.0F - aAmount)) + (aTint.redF() * aAmount), (aBase.greenF() * (1.0F - aAmount)) + (aTint.greenF() * aAmount),
									(aBase.blueF() * (1.0F - aAmount)) + (aTint.blueF() * aAmount));
		};
		wash        = mix(wash, accent, 0.06F);
		auto border = mix(palette().color(QPalette::Active, QPalette::Mid), accent, 0.25F);
		painter.setPen(QPen{ border, 1.0 });
		painter.setBrush(wash);
		const auto box = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
		painter.drawRoundedRect(box, 6.0, 6.0);
		painter.setPen(Qt::NoPen);
		painter.setBrush(accent);
		painter.drawRoundedRect(QRectF{ box.left() + 3.0, box.top() + 6.0, 2.5, box.height() - 12.0 }, 1.25, 1.25);
	}

	enhance_binding::enhance_binding(QPlainTextEdit& aEditor, const enhance_surface aSurface, enhance_button& aButton, enhance_suggestion& aPanel, QObject* aParent)
		: QObject{ aParent }
		, Editor{ &aEditor }
		, Surface{ aSurface }
		, Button{ &aButton }
		, Panel{ &aPanel }
	{
		connect(&aButton, &QAbstractButton::clicked, this, [this] { press(); });
		if (aPanel.use_button() != nullptr) {
			connect(aPanel.use_button(), &QAbstractButton::clicked, this, [this] { use(); });
			connect(aPanel.discard_button(), &QAbstractButton::clicked, this, [this] { discard(); });
		}
		connect(&aEditor, &QPlainTextEdit::textChanged, this, [this] { refresh(); });
		connect(&aEditor, &QPlainTextEdit::selectionChanged, this, [this] { refresh(); });
		refresh();
	}

	void enhance_binding::set_dispatch(enhance_dispatch aDispatch, enhance_cancel aCancel)
	{
		Dispatch = std::move(aDispatch);
		Cancel   = std::move(aCancel);
		refresh();
	}

	void enhance_binding::refresh()
	{
		if (Button == nullptr || Editor == nullptr) {
			return;
		}
		// Offered only where a composition answers it: a build without
		// agents never wires it, and shows none.
		Button->setVisible(Dispatch != nullptr);
		const auto blank = core::writing_blank(Editor->toPlainText().toStdString());
		Button->set_selection(Editor->textCursor().hasSelection());
		Button->setEnabled(Running || (Dispatch != nullptr && !Editor->isReadOnly() && !blank));
		if (!Running && Dispatch != nullptr && blank) {
			Button->setToolTip(tr("Enhance improves what you have written. Write your %1 first.").arg(noun(Surface)));
		} else if (!Running) {
			Button->set_running(false);
		}
		// A suggestion is for the text it was asked about; once that text has
		// changed, Use would overwrite words the agent never saw.
		if (Suggestion && Panel != nullptr && Panel->use_button() != nullptr) {
			const auto current = Editor->toPlainText() == AskedText;
			Panel->use_button()->setEnabled(current);
			Panel->use_button()->setToolTip(current ? tr("Puts the suggestion in place of your text. Undo takes it back.")
													: tr("Your text changed after Enhance. Enhance again, or discard this suggestion."));
		}
	}

	void enhance_binding::press()
	{
		if (Editor == nullptr || Button == nullptr || Panel == nullptr) {
			return;
		}
		if (Running) {
			Running = false;
			Button->set_running(false);
			Panel->show_notice(tr("Stopped. Your %1 is unchanged.").arg(noun(Surface)));
			if (Cancel) {
				Cancel(Asked.Id);
			}
			refresh();
			if (Changed) {
				Changed();
			}
			return;
		}
		if (!Dispatch) {
			return;
		}
		AskedText         = Editor->toPlainText();
		const auto cursor = Editor->textCursor();
		auto       ask    = enhance_request{ .Id = ++Sequence, .Surface = Surface, .Raw = AskedText.toStdString() };
		if (cursor.hasSelection()) {
			AskedBegin    = cursor.selectionStart();
			AskedEnd      = cursor.selectionEnd();
			ask.Begin     = utf8_offset(AskedText, AskedBegin);
			ask.End       = utf8_offset(AskedText, AskedEnd);
			ask.Selection = true;
		} else {
			AskedBegin = 0;
			AskedEnd   = static_cast<int>(AskedText.size());
			ask.End    = ask.Raw.size();
		}
		Asked   = ask;
		Running = true;
		Suggestion.reset();
		Button->set_running(true);
		Panel->show_running();
		refresh();
		if (Changed) {
			Changed();
		}
		Dispatch(std::move(ask));
	}

	auto enhance_binding::present(const enhance_outcome& aOutcome) -> bool
	{
		if (!Running || aOutcome.Id != Asked.Id || Panel == nullptr || Button == nullptr) {
			return false;
		}
		Running = false;
		Button->set_running(false);
		if (aOutcome.Replacement) {
			Suggestion = QString::fromStdString(*aOutcome.Replacement);
			Panel->show_suggestion(*Suggestion, Asked.Selection);
			if (!aOutcome.Notice.isEmpty()) {
				Panel->show_notice(aOutcome.Notice);
			}
		} else {
			Suggestion.reset();
			Panel->show_suggestion(QString{ }, false);
			Panel->clear();
			Panel->show_notice(aOutcome.Notice.isEmpty() ? tr("No suggestion came back. Your %1 is unchanged.").arg(noun(Surface)) : aOutcome.Notice);
		}
		refresh();
		if (Changed) {
			Changed();
		}
		return true;
	}

	void enhance_binding::use()
	{
		if (!Suggestion || Editor == nullptr || Editor->toPlainText() != AskedText) {
			refresh();
			return;
		}
		auto cursor = QTextCursor{ Editor->document() };
		cursor.beginEditBlock();
		cursor.setPosition(AskedBegin);
		cursor.setPosition(AskedEnd, QTextCursor::KeepAnchor);
		cursor.insertText(*Suggestion);
		cursor.endEditBlock();
		Editor->setTextCursor(cursor);
		Suggestion.reset();
		Panel->clear();
		Editor->setFocus(Qt::OtherFocusReason);
		refresh();
		if (Changed) {
			Changed();
		}
	}

	void enhance_binding::reset()
	{
		if (Running) {
			Running = false;
			if (Button != nullptr) {
				Button->set_running(false);
			}
			if (Cancel) {
				Cancel(Asked.Id);
			}
		}
		discard();
	}

	void enhance_binding::discard()
	{
		Suggestion.reset();
		if (Panel != nullptr) {
			Panel->clear();
		}
		refresh();
		if (Changed) {
			Changed();
		}
	}
} // namespace bpr::qt
