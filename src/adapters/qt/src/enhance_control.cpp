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
#include <QVBoxLayout>

#include <atomic>
#include <utility>

namespace bpr::qt {
	namespace {
		std::atomic<enhance_glyph_style> CurrentStyle{ enhance_glyph_style::pen };
		std::atomic<std::uint64_t>       Sequence{ 0U };

		/// The agent's four-pointed spark, as `drawer_glyph::agent` draws it,
		/// centred at (aX, aY) of a 24-unit square.
		void spark(QPainter& aPainter, const qreal aUnit, const qreal aX, const qreal aY, const qreal aReach)
		{
			auto path = QPainterPath{ QPointF{ aX * aUnit, (aY - aReach) * aUnit } };
			path.quadTo(QPointF{ aX * aUnit, aY * aUnit }, QPointF{ (aX + aReach) * aUnit, aY * aUnit });
			path.quadTo(QPointF{ aX * aUnit, aY * aUnit }, QPointF{ aX * aUnit, (aY + aReach) * aUnit });
			path.quadTo(QPointF{ aX * aUnit, aY * aUnit }, QPointF{ (aX - aReach) * aUnit, aY * aUnit });
			path.quadTo(QPointF{ aX * aUnit, aY * aUnit }, QPointF{ aX * aUnit, (aY - aReach) * aUnit });
			aPainter.drawPath(path);
		}

		/// Strokes one option `aSide` logical pixels square at the origin.
		void paint_glyph(QPainter& aPainter, const qreal aSide, const QColor& aInk, const enhance_glyph_style aStyle)
		{
			aPainter.setPen(QPen{ aInk, aSide / 14.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
			aPainter.setBrush(Qt::NoBrush);
			const auto unit = aSide / 24.0;
			const auto at   = [unit](const qreal aX, const qreal aY) { return QPointF{ aX * unit, aY * unit }; };
			switch (aStyle) {
			case enhance_glyph_style::pen: {
				// A pen laid from the lower left, its nib at the line it
				// writes, and the spark over its end.
				auto body = QPainterPath{ at(4.0, 20.0) };
				body.lineTo(at(5.0, 15.5));
				body.lineTo(at(14.0, 6.5));
				body.lineTo(at(17.5, 10.0));
				body.lineTo(at(8.5, 19.0));
				body.closeSubpath();
				aPainter.drawPath(body);
				aPainter.drawLine(at(12.0, 8.5), at(15.5, 12.0));
				aPainter.setPen(QPen{ aInk, aSide / 16.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
				spark(aPainter, unit, 19.0, 4.5, 3.0);
				break;
			}
			case enhance_glyph_style::lines:
				// Three lines of text, the first short where the spark sits.
				aPainter.drawLine(at(4.0, 7.0), at(12.0, 7.0));
				aPainter.drawLine(at(4.0, 12.5), at(20.0, 12.5));
				aPainter.drawLine(at(4.0, 18.0), at(15.0, 18.0));
				aPainter.setPen(QPen{ aInk, aSide / 16.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
				spark(aPainter, unit, 18.0, 6.0, 3.5);
				break;
			case enhance_glyph_style::wand:
				// A wand from the lower left, a band near its tip, a spark
				// at the tip and two small ones beside it.
				aPainter.drawLine(at(4.0, 20.0), at(13.5, 10.5));
				aPainter.drawLine(at(10.0, 12.0), at(12.0, 14.0));
				aPainter.setPen(QPen{ aInk, aSide / 16.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
				spark(aPainter, unit, 17.0, 7.0, 4.0);
				spark(aPainter, unit, 10.5, 4.0, 1.6);
				spark(aPainter, unit, 20.5, 14.0, 1.6);
				break;
			}
		}

		[[nodiscard]] auto noun(const enhance_surface aSurface) -> QString { return aSurface == enhance_surface::comment ? QObject::tr("comment") : QObject::tr("note"); }

		[[nodiscard]] auto utf8_offset(const QString& aText, const int aPosition) -> std::size_t { return static_cast<std::size_t>(aText.left(aPosition).toUtf8().size()); }
	} // namespace

	auto enhance_glyph_style_name(const enhance_glyph_style aStyle) -> QString
	{
		switch (aStyle) {
		case enhance_glyph_style::pen:   return QObject::tr("Pen and spark");
		case enhance_glyph_style::lines: return QObject::tr("Lines and spark");
		case enhance_glyph_style::wand:  return QObject::tr("Wand");
		}
		return { };
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
		setObjectName(aSurface == enhance_surface::comment ? QStringLiteral("commentEnhance") : QStringLiteral("noteEnhance"));
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
		setAccessibleName(aRunning ? tr("Stop enhancing") : tr("Enhance"));
		setToolTip(aRunning ? tr("Stop enhancing. Your %1 stays as it is.").arg(what)
							: tr("Enhance: asks the agent to improve this %1, or only the selected text. You choose whether to use what it suggests; nothing is published.").arg(what));
		setAccessibleDescription(toolTip());
		restyle();
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
		setObjectName(aSurface == enhance_surface::comment ? QStringLiteral("commentEnhanceSuggestion") : QStringLiteral("noteEnhanceSuggestion"));
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
		Heading->setText(aSelection ? tr("Suggested rewrite of your selection") : Surface == enhance_surface::comment ? tr("Suggested comment")
																													  : tr("Suggested note"));
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
