#include "bpr/qt/source_find_bar.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include <QAccessible>
#include <QApplication>
#include <QHBoxLayout>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>
#include <algorithm>
#include <cmath>

namespace bpr::qt {
	namespace {
		/// The panel's corner radius and the field's, in logical pixels.
		constexpr qreal panel_radius = 6.0;
		constexpr qreal field_radius = 4.0;
		/// The query field grows with a multi-line query up to this many lines.
		constexpr int maximum_query_lines = 4;

		enum class find_glyph : std::uint8_t { match_case,
											   whole_word,
											   regular_expression,
											   selection,
											   previous,
											   next,
											   close };

		/// \brief The side of every square control, from the UI font.
		[[nodiscard]] auto control_side(const QFontMetrics& aMetrics) -> int
		{
			return std::max(20, static_cast<int>(std::lround(aMetrics.height() * 1.3)));
		}

		/// \brief One square find control, drawn rather than labelled.
		///
		/// \details VS Code's find widget is icons, not words; text buttons
		/// ("Word", "Selection") were what made the old bar read as a form.
		/// Each glyph is painted from the palette so it follows the theme and
		/// stays sharp at every scale. A checked toggle is an accent tint with
		/// an accent outline, the one mark that says "this option is on".
		class find_button final : public QToolButton
		{
		public:
			find_button(QWidget* const aParent, const find_glyph aGlyph)
				: QToolButton{ aParent }
				, Glyph{ aGlyph }
			{
				setAutoRaise(true);
				setFocusPolicy(Qt::TabFocus);
				setCursor(Qt::ArrowCursor);
			}
			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				const auto side = control_side(fontMetrics());
				return { side, side };
			}
			[[nodiscard]] auto minimumSizeHint() const -> QSize override { return sizeHint(); }

		protected:
			void paintEvent(QPaintEvent* const aEvent) override
			{
				static_cast<void>(aEvent);
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto box    = QRectF{ rect() }.adjusted(1.5, 1.5, -1.5, -1.5);
				const auto accent = palette().color(QPalette::Active, QPalette::Highlight);
				if (isChecked()) {
					auto tint = accent;
					tint.setAlpha(isDown() ? 96 : 56);
					painter.setPen(QPen{ accent, 1.0 });
					painter.setBrush(tint);
					painter.drawRoundedRect(box, 3.0, 3.0);
				} else if (isEnabled() && (underMouse() || isDown())) {
					auto hover = palette().color(QPalette::Active, QPalette::Mid);
					hover.setAlpha(isDown() ? 110 : 64);
					painter.setPen(Qt::NoPen);
					painter.setBrush(hover);
					painter.drawRoundedRect(box, 3.0, 3.0);
				}
				if (hasFocus()) {
					painter.setPen(QPen{ accent, 1.5 });
					painter.setBrush(Qt::NoBrush);
					painter.drawRoundedRect(box, 3.0, 3.0);
				}
				const auto ink = palette().color(isEnabled() ? QPalette::Active : QPalette::Disabled, QPalette::WindowText);
				painter.setPen(QPen{ ink, std::max(1.2, width() / 14.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
				painter.setBrush(Qt::NoBrush);
				const auto c    = QRectF{ rect() }.center();
				const auto unit = std::min(width(), height()) / 20.0;
				switch (Glyph) {
				case find_glyph::match_case:
				case find_glyph::whole_word:
				case find_glyph::regular_expression: paint_text_glyph(painter, ink); break;
				case find_glyph::selection:
					// Lines of text inside a selection bracket.
					painter.drawLine(QPointF{ c.x() - 5.0 * unit, c.y() - 4.0 * unit }, QPointF{ c.x() + 5.0 * unit, c.y() - 4.0 * unit });
					painter.drawLine(QPointF{ c.x() - 5.0 * unit, c.y() }, QPointF{ c.x() + 2.0 * unit, c.y() });
					painter.drawLine(QPointF{ c.x() - 5.0 * unit, c.y() + 4.0 * unit }, QPointF{ c.x() + 4.0 * unit, c.y() + 4.0 * unit });
					break;
				case find_glyph::previous:
				case find_glyph::next:     {
					const auto sign = Glyph == find_glyph::previous ? -1.0 : 1.0;
					const auto tip  = QPointF{ c.x(), c.y() + sign * 5.5 * unit };
					painter.drawLine(QPointF{ c.x(), c.y() - sign * 5.5 * unit }, tip);
					painter.drawLine(tip, QPointF{ c.x() - 4.0 * unit, tip.y() - sign * 4.0 * unit });
					painter.drawLine(tip, QPointF{ c.x() + 4.0 * unit, tip.y() - sign * 4.0 * unit });
					break;
				}
				case find_glyph::close:
					painter.drawLine(QPointF{ c.x() - 4.5 * unit, c.y() - 4.5 * unit }, QPointF{ c.x() + 4.5 * unit, c.y() + 4.5 * unit });
					painter.drawLine(QPointF{ c.x() + 4.5 * unit, c.y() - 4.5 * unit }, QPointF{ c.x() - 4.5 * unit, c.y() + 4.5 * unit });
					break;
				}
			}

		private:
			find_glyph Glyph;

			void paint_text_glyph(QPainter& aPainter, const QColor& aInk) const
			{
				auto glyphFont = font();
				glyphFont.setPointSizeF(std::max(7.0, glyphFont.pointSizeF() * 0.92));
				glyphFont.setWeight(Glyph == find_glyph::regular_expression ? QFont::Bold : QFont::DemiBold);
				aPainter.setFont(glyphFont);
				aPainter.setPen(aInk);
				const auto text    = Glyph == find_glyph::match_case ? QStringLiteral("Aa") : Glyph == find_glyph::whole_word ? QStringLiteral("ab")
																															  : QStringLiteral(".*");
				const auto metrics = QFontMetricsF{ glyphFont };
				const auto advance = metrics.horizontalAdvance(text);
				const auto left    = (width() - advance) / 2.0;
				const auto base    = (height() + metrics.capHeight()) / 2.0 - (Glyph == find_glyph::whole_word ? 1.0 : 0.0);
				aPainter.drawText(QPointF{ left, base }, text);
				if (Glyph == find_glyph::whole_word) {
					// The underline is what says "word": a bounded stretch of text.
					aPainter.setPen(QPen{ aInk, 1.2, Qt::SolidLine, Qt::FlatCap });
					const auto y = base + 2.5;
					aPainter.drawLine(QPointF{ left - 1.0, y }, QPointF{ left + advance + 1.0, y });
				}
			}
		};

		/// \brief The query box: a rounded input whose border states focus and error.
		class find_field final : public QWidget
		{
		public:
			explicit find_field(QWidget* const aParent)
				: QWidget{ aParent }
			{
				setAttribute(Qt::WA_Hover, false);
			}
			bool   Error = false;
			QColor ErrorColor;

		protected:
			void paintEvent(QPaintEvent* const aEvent) override
			{
				static_cast<void>(aEvent);
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto* focused = QApplication::focusWidget();
				const bool  within  = focused != nullptr && focused->parentWidget() == this && qobject_cast<const QPlainTextEdit*>(focused) != nullptr;
				auto        edge    = palette().color(QPalette::Active, QPalette::Mid);
				auto        weight  = 1.0;
				if (Error && ErrorColor.isValid()) {
					edge   = ErrorColor;
					weight = 1.5;
				} else if (within) {
					edge = palette().color(QPalette::Active, QPalette::Highlight);
				}
				painter.setPen(QPen{ edge, weight });
				painter.setBrush(palette().color(QPalette::Active, QPalette::Base));
				const auto inset = weight / 2.0;
				painter.drawRoundedRect(QRectF{ rect() }.adjusted(inset, inset, -inset, -inset), field_radius, field_radius);
			}
		};

		[[nodiscard]] auto option_shortcut(const Qt::Key aKey) -> QString
		{
			return QKeySequence{ QKeyCombination{ Qt::AltModifier, aKey } }.toString(QKeySequence::NativeText);
		}
	} // namespace

	source_find_bar::source_find_bar(QWidget* const aParent, const bool aFiles)
		: QWidget{ aParent }
		, Files{ aFiles }
		, ErrorColor{ to_presentation_color(builtin_appearance_palette(core::appearance_variant::light)[core::index_of(core::appearance_role::state_defect)]) }
	{
		setObjectName(QStringLiteral("sourceFindBar"));
		setAccessibleName(QStringLiteral("Find in source"));
		auto* row = new QHBoxLayout{ this };
		row->setContentsMargins(Files ? QMargins{ 0, 0, 0, 0 } : QMargins{ 5, 5, 5, 5 });
		row->setSpacing(2);

		Field        = new find_field{ this };
		auto* inside = new QHBoxLayout{ Field };
		inside->setContentsMargins(6, 2, 2, 2);
		inside->setSpacing(1);
		Query = new QPlainTextEdit{ Field };
		Query->setObjectName(QStringLiteral("sourceFindQuery"));
		Query->setAccessibleName(QStringLiteral("Find query"));
		Query->setAccessibleDescription(QStringLiteral("Find source text. Enter next match; Shift+Enter previous; Ctrl+Enter newline; Option+W toggles Whole Word; Option+L toggles Find in Selection; Escape closes Find."));
		Query->setPlaceholderText(QStringLiteral("Find"));
		Query->setTabChangesFocus(true);
		Query->setFrameShape(QFrame::NoFrame);
		Query->setLineWrapMode(QPlainTextEdit::NoWrap);
		Query->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Query->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Query->document()->setDocumentMargin(2);
		Query->setMinimumWidth(48);
		Query->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		Query->installEventFilter(this);
		Query->viewport()->setAutoFillBackground(false);
		inside->addWidget(Query, 1, Qt::AlignTop);

		const auto button = [this](QWidget* const aHost, const find_glyph aGlyph, const QString& aName, const QString& aDescription, const bool aToggle) {
			auto* control = new find_button{ aHost, aGlyph };
			control->setObjectName(aName);
			control->setAccessibleName(aDescription);
			control->setToolTip(aDescription);
			control->setCheckable(aToggle);
			control->installEventFilter(this);
			return control;
		};
		Case  = button(Field, find_glyph::match_case, QStringLiteral("sourceFindCase"), QStringLiteral("Match case"), true);
		Word  = button(Field, find_glyph::whole_word, QStringLiteral("sourceFindWord"), QStringLiteral("Whole word"), true);
		Regex = button(Field, find_glyph::regular_expression, QStringLiteral("sourceFindRegex"), QStringLiteral("Regular expression"), true);
		Case->setToolTip(QStringLiteral("Match Case"));
		Word->setToolTip(QStringLiteral("Match Whole Word (%1)").arg(option_shortcut(Qt::Key_W)));
		Regex->setToolTip(QStringLiteral("Use Regular Expression"));
		for (auto* control : { Case, Word, Regex }) {
			inside->addWidget(control, 0, Qt::AlignTop);
		}
		row->addWidget(Field, 1);

		Count = new QLabel{ this };
		Count->setTextFormat(Qt::PlainText);
		Count->setObjectName(QStringLiteral("sourceFindStatus"));
		Count->setAccessibleName(QStringLiteral("Find match status"));
		Count->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
		Count->setContentsMargins(4, 0, 2, 0);
		row->addWidget(Count);
		Previous  = button(this, find_glyph::previous, QStringLiteral("sourceFindPrevious"), QStringLiteral("Previous match"), false);
		Next      = button(this, find_glyph::next, QStringLiteral("sourceFindNext"), QStringLiteral("Next match"), false);
		Selection = button(this, find_glyph::selection, QStringLiteral("sourceFindSelection"), QStringLiteral("Find in Selection"), true);
		Close     = button(this, find_glyph::close, QStringLiteral("sourceFindClose"), QStringLiteral("Close Find"), false);
		Previous->setToolTip(QStringLiteral("Previous Match (Shift+Enter)"));
		Next->setToolTip(QStringLiteral("Next Match (Enter)"));
		Close->setToolTip(QStringLiteral("Close (Escape)"));
		Selection->setToolTip(QStringLiteral("Find in Selection (%1)").arg(option_shortcut(Qt::Key_L)));
		for (auto* control : { Previous, Next, Selection, Close }) {
			row->addWidget(control, 0, Qt::AlignTop);
		}

		Debounce = new QTimer{ this };
		Debounce->setSingleShot(true);
		Debounce->setInterval(150);
		connect(Debounce, &QTimer::timeout, this, [this] {
			if (Dispatch) {
				Dispatch(source_find_gesture::edit);
			}
		});
		connect(Query, &QPlainTextEdit::textChanged, this, [this] {
			fit_query_height();
			schedule();
		});
		for (auto* control : { Case, Word, Regex }) {
			connect(control, &QToolButton::toggled, this, [this] { flush(); });
		}
		connect(Selection, &QToolButton::toggled, this, [this] {
			if (Dispatch) {
				Dispatch(source_find_gesture::selection);
			}
		});
		connect(Previous, &QToolButton::clicked, this, [this] {
			flush();
			if (Dispatch) {
				Dispatch(source_find_gesture::previous);
			}
		});
		connect(Next, &QToolButton::clicked, this, [this] {
			flush();
			if (Dispatch) {
				Dispatch(source_find_gesture::next);
			}
		});
		connect(Close, &QToolButton::clicked, this, [this] {
			Debounce->stop();
			if (Dispatch) {
				Dispatch(source_find_gesture::close);
			}
		});
		// The field's border follows keyboard focus into and out of the query.
		connect(qApp, &QApplication::focusChanged, Field, [this] { Field->update(); });
		if (Files) {
			setObjectName(QStringLiteral("sourceFilesBar"));
			Query->setObjectName(QStringLiteral("sourceFilesQuery"));
			Query->setAccessibleName(QStringLiteral("Find in Files query"));
			Query->setAccessibleDescription(QStringLiteral("Search full changed files. Enter searches now; Ctrl+Enter newline; Option+W toggles Whole Word; Escape returns to source."));
			Query->setPlaceholderText(QStringLiteral("Search"));
			// Close stays: it hands the drawer back to the tab it showed before.
			Close->setToolTip(QStringLiteral("Close Search"));
			Close->setAccessibleName(QStringLiteral("Close Search"));
			for (QWidget* control : { static_cast<QWidget*>(Count), static_cast<QWidget*>(Previous), static_cast<QWidget*>(Next), static_cast<QWidget*>(Selection) }) {
				control->hide();
			}
		}
		adapt_font();
		set_status(QStringLiteral("Type to find"), QString{ }, false);
		hide();
	}
	void source_find_bar::adapt_font()
	{
		setFont(QApplication::font());
		const auto metrics = fontMetrics();
		// A compact panel reserves only what the count says now; otherwise room
		// for "99 of 999", so typing does not make the controls jump.
		Count->setMinimumWidth(metrics.horizontalAdvance(Compact ? Count->text() : QStringLiteral("99 of 999")) + 7);
		fit_query_height();
		updateGeometry();
	}
	void source_find_bar::fit_query_height()
	{
		const auto lines   = std::clamp(Query->document()->blockCount(), 1, maximum_query_lines);
		const auto spacing = Query->fontMetrics().lineSpacing();
		const auto margin  = static_cast<int>(std::ceil(Query->document()->documentMargin()));
		const auto height  = std::max(control_side(fontMetrics()), lines * spacing + 2 * margin + 2);
		Query->setVerticalScrollBarPolicy(Query->document()->blockCount() > maximum_query_lines ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
		if (Query->height() != height || Query->minimumHeight() != height) {
			Query->setFixedHeight(height);
			if (!Files && layout() != nullptr) {
				// Its owner places it; a taller query only makes it taller.
				layout()->activate();
				resize(width(), sizeHint().height());
			}
		}
	}
	auto source_find_bar::preferred_width() const -> int
	{
		const auto metrics = fontMetrics();
		const auto side    = control_side(metrics);
		const auto field   = metrics.averageCharWidth() * 22 + 3 * side + 12;
		return field + Count->minimumWidth() + 4 + 4 * side + 4 * 2 + 10;
	}
	auto source_find_bar::fit_to(const int aAvailable) -> int
	{
		if (Files) {
			return minimumSizeHint().width();
		}
		const auto measure = [this] {
			layout()->invalidate();
			return minimumSizeHint().width();
		};
		Compact = false;
		layout()->setSpacing(2);
		adapt_font();
		Selection->setVisible(true);
		if (measure() <= aAvailable) {
			return minimumSizeHint().width();
		}
		Compact = true;
		layout()->setSpacing(0);
		adapt_font();
		if (measure() <= aAvailable) {
			return minimumSizeHint().width();
		}
		Selection->setVisible(false);
		return measure();
	}
	void source_find_bar::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::ApplicationFontChange) {
			adapt_font();
		}
		if (aEvent->type() == QEvent::PaletteChange) {
			update();
		}
	}
	void source_find_bar::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		if (Files) {
			return;
		}
		// A floating panel over the code: the window surface, one hairline in
		// the theme's rule colour, rounded so it reads as lifted off the text.
		// The rule colour is drawn a quarter of the way toward the text: on a
		// dark theme Mid is darker than the surface and would vanish into the
		// code behind the panel.
		auto       painter = QPainter{ this };
		const auto rule    = palette().color(QPalette::Active, QPalette::Mid);
		const auto text    = palette().color(QPalette::Active, QPalette::WindowText);
		const auto edge    = QColor::fromRgbF((rule.redF() * 3.0F + text.redF()) / 4.0F, (rule.greenF() * 3.0F + text.greenF()) / 4.0F, (rule.blueF() * 3.0F + text.blueF()) / 4.0F);
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setPen(QPen{ edge, 1.0 });
		painter.setBrush(palette().color(QPalette::Active, QPalette::Window));
		painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), panel_radius, panel_radius);
	}
	void source_find_bar::adopt_appearance(const appearance_profile& aProfile)
	{
		ErrorColor                                  = aProfile.color(core::appearance_role::state_defect);
		static_cast<find_field*>(Field)->ErrorColor = ErrorColor;
		Field->update();
	}
	void source_find_bar::schedule()
	{
		SeedError.clear();
		// Obsolete decorations disappear immediately, before the typing debounce.
		Projection.Matches.reset();
		Projection.CurrentMatch.reset();
		const auto searching = Query->toPlainText().isEmpty() ? QStringLiteral("Type to find") : QStringLiteral("Searching…");
		set_status(searching, QString{ }, false);
		Previous->setEnabled(false);
		Next->setEnabled(false);
		if (Dispatch) {
			Dispatch(source_find_gesture::query_dirty);
		}
		set_status(searching, QString{ }, false);
		if (!Composing) {
			Debounce->start();
		}
	}
	void source_find_bar::cancel_pending() { Debounce->stop(); }
	void source_find_bar::flush()
	{
		Debounce->stop();
		if (!Composing && Dispatch) {
			Dispatch(source_find_gesture::edit);
		}
	}
	auto source_find_bar::toggle_option(const source_find_option aOption) -> bool
	{
		auto* control = option_button(aOption);
		if (control == nullptr || !control->isEnabled()) {
			return false;
		}
		control->toggle();
		return true;
	}
	auto source_find_bar::handle_option_key(const QKeyEvent& aKey) -> bool
	{
		if ((aKey.modifiers() & ~Qt::KeypadModifier) != Qt::AltModifier) {
			return false;
		}
		switch (aKey.key()) {
		// Option+C and Option+R stay the window's Comment and Compose review
		// (owner, 2026-10-14), so only Whole Word and Find in Selection have keys.
		case Qt::Key_W: return toggle_option(source_find_option::whole_word);
		case Qt::Key_L:
			if (Files || !Selection->isEnabled()) {
				return false;
			}
			Selection->toggle();
			return true;
		default: return false;
		}
	}
	auto source_find_bar::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aWatched == Query && aEvent->type() == QEvent::InputMethod) {
			Composing = !static_cast<QInputMethodEvent*>(aEvent)->preeditString().isEmpty();
			if (Composing) {
				Debounce->stop();
			} else {
				Debounce->start();
			}
		}
		const bool keyed = aEvent->type() == QEvent::KeyPress || aEvent->type() == QEvent::ShortcutOverride;
		if (keyed) {
			// Option+W and Option+L would otherwise type "∑" or "¬" into the query.
			const auto& key = *static_cast<QKeyEvent*>(aEvent);
			if ((key.modifiers() & ~Qt::KeypadModifier) == Qt::AltModifier && (key.key() == Qt::Key_W || (!Files && key.key() == Qt::Key_L))) {
				aEvent->accept();
				if (aEvent->type() == QEvent::KeyPress) {
					static_cast<void>(handle_option_key(key));
				}
				return true;
			}
		}
		if (aWatched != Query && keyed && static_cast<QKeyEvent*>(aEvent)->key() == Qt::Key_Escape) {
			aEvent->accept();
			if (aEvent->type() == QEvent::KeyPress) {
				if (!Files) {
					Debounce->stop();
				}
				if (Dispatch) {
					Dispatch(Files ? source_find_gesture::escape : source_find_gesture::close);
				}
			}
			return true;
		}
		if (aWatched == Query && keyed) {
			auto* key = static_cast<QKeyEvent*>(aEvent);
			if (key->key() == Qt::Key_Escape || key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
				aEvent->accept();
				if (aEvent->type() == QEvent::ShortcutOverride) {
					return true;
				}
				if (key->key() == Qt::Key_Escape) {
					if (!Files) {
						Debounce->stop();
					}
					if (Dispatch) {
						Dispatch(Files ? source_find_gesture::escape : source_find_gesture::close);
					}
				} else if ((key->modifiers().testFlag(Qt::ControlModifier)
#if defined(Q_OS_MACOS)
							|| key->modifiers().testFlag(Qt::MetaModifier)
#endif
								)) {
					Query->insertPlainText(QStringLiteral("\n"));
				} else {
					const bool pending = Debounce->isActive();
					flush();
					if (!Files && !pending && Dispatch) {
						Dispatch(key->modifiers().testFlag(Qt::ShiftModifier) ? source_find_gesture::previous : source_find_gesture::next);
					}
				}
				return true;
			}
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}
	void source_find_bar::seed(const QString& aText, const bool aLiteral)
	{
		SeedError.clear();
		if (aText.size() > static_cast<qsizetype>(core::maximum_source_search_query_bytes)) {
			SeedError = QStringLiteral("Selection exceeds the 4 KiB query limit");
			present(Projection);
			Query->setFocus(Qt::ShortcutFocusReason);
			Query->selectAll();
			return;
		}
		auto text = aText;
		if (aLiteral && Regex->isChecked()) {
			text.clear();
			for (const auto scalar : aText) {
				if (QStringLiteral("\\.^$|?*+()[]{}").contains(scalar)) {
					text += QLatin1Char('\\');
				}
				text += scalar;
			}
		}
		if (text.toUtf8().size() > static_cast<qsizetype>(core::maximum_source_search_query_bytes)) {
			SeedError = QStringLiteral("Selection exceeds the 4 KiB query limit");
			present(Projection);
		} else if (!aText.isEmpty()) {
			const auto blocked = QSignalBlocker{ Query };
			Query->setPlainText(text);
			fit_query_height();
		}
		Query->setFocus(Qt::ShortcutFocusReason);
		Query->selectAll();
	}
	void source_find_bar::set_scope(const core::source_search_target aTarget, const bool aHasSelection, QString aReason)
	{
		ScopeText = aTarget == core::source_search_target::left ? QStringLiteral("Earlier") : aTarget == core::source_search_target::right ? QStringLiteral("Later")
																																		   : QStringLiteral("Unified");
		setAccessibleDescription(ScopeText + QStringLiteral(" source"));
		Selection->setEnabled(aHasSelection || Selection->isChecked());
		Selection->setToolTip(aHasSelection ? QStringLiteral("Find in Selection") : QStringLiteral("Find in Selection: %1").arg(std::move(aReason)));
	}
	auto source_find_bar::query() const -> std::string
	{
		if (Query->document()->characterCount() - 1 > static_cast<int>(core::maximum_source_search_query_bytes)) {
			return std::string(core::maximum_source_search_query_bytes + 1U, ' ');
		}
		return Query->toPlainText().toUtf8().toStdString();
	}
	auto source_find_bar::options() const -> core::source_search_options { return { .MatchCase = Case->isChecked(), .WholeWord = Word->isChecked(), .RegularExpression = Regex->isChecked() }; }
	auto source_find_bar::in_selection() const -> bool { return Selection->isChecked(); }
	void source_find_bar::clear_selection()
	{
		const auto blocked = QSignalBlocker{ Selection };
		Selection->setChecked(false);
		Selection->update();
	}
	void source_find_bar::reset_query()
	{
		SeedError.clear();
		Debounce->stop();
		const auto queryBlock = QSignalBlocker{ Query };
		Query->clear();
		fit_query_height();
		for (auto* control : { Case, Word, Regex, Selection }) {
			const auto block = QSignalBlocker{ control };
			control->setChecked(false);
			control->update();
		}
		set_status(QStringLiteral("Type to find"), QString{ }, false);
		hide();
	}
	auto source_find_bar::query_editor() const -> QPlainTextEdit* { return Query; }
	auto source_find_bar::status_text() const -> QString { return StatusText; }
	auto source_find_bar::count_text() const -> QString { return Count->text(); }
	auto source_find_bar::error_state() const -> bool { return Error; }
	auto source_find_bar::option_button(const source_find_option aOption) const -> QToolButton*
	{
		switch (aOption) {
		case source_find_option::match_case:         return Case;
		case source_find_option::whole_word:         return Word;
		case source_find_option::regular_expression: return Regex;
		}
		return nullptr;
	}
	auto source_find_bar::projection() const -> const application::source_search_projection& { return Projection; }
	void source_find_bar::set_status(QString aWords, QString aShort, const bool aError)
	{
		StatusText = std::move(aWords);
		// The count area is compact: an empty query shows nothing there, and a
		// long sentence shows its short form with the whole of it as a tooltip.
		const auto shown = aShort.isEmpty() ? (StatusText == QStringLiteral("Type to find") ? QString{ } : StatusText) : std::move(aShort);
		Count->setText(shown);
		if (Compact) {
			Count->setMinimumWidth(fontMetrics().horizontalAdvance(shown) + 7);
		}
		Count->setToolTip(shown == StatusText ? QString{ } : StatusText);
		if (Error != aError) {
			Error                                  = aError;
			static_cast<find_field*>(Field)->Error = aError;
			Field->update();
		}
		Field->setToolTip(aError ? StatusText : QString{ });
	}
	void source_find_bar::present(const application::source_search_projection& aProjection)
	{
		Projection   = aProjection;
		auto words   = QStringLiteral("Type to find");
		auto compact = QString{ };
		auto error   = false;
		switch (aProjection.Status) {
		case core::source_search_status::searching: words = QStringLiteral("Searching…"); break;
		case core::source_search_status::invalid_pattern:
			words   = QStringLiteral("Invalid regular expression");
			compact = QStringLiteral("Invalid");
			error   = true;
			break;
		case core::source_search_status::unsupported_pattern:
			words   = QStringLiteral("Unsupported regular expression");
			compact = QStringLiteral("Unsupported");
			error   = true;
			break;
		case core::source_search_status::invalid_input:
			words   = QStringLiteral("Query exceeds 4 KiB or is invalid text");
			compact = QStringLiteral("Too long");
			error   = true;
			break;
		case core::source_search_status::unavailable:
			words   = QStringLiteral("Source unavailable");
			compact = QStringLiteral("Unavailable");
			break;
		case core::source_search_status::failure:
			words   = QStringLiteral("Search failed; edit the query to retry");
			compact = QStringLiteral("Failed");
			break;
		case core::source_search_status::resource_limit:
			words   = QStringLiteral("Search resource limit; narrow the query");
			compact = QStringLiteral("Too complex");
			break;
		case core::source_search_status::result_limit:
			words   = QStringLiteral("%1 shown; limit reached; %2").arg(aProjection.Summary.Matches).arg(Files ? tr("narrow query or files") : tr("narrow query"));
			compact = aProjection.CurrentMatch ? QStringLiteral("%1 of %2+").arg(*aProjection.CurrentMatch + 1U).arg(aProjection.Summary.Matches) : QStringLiteral("%1+").arg(aProjection.Summary.Matches);
			break;
		case core::source_search_status::cancelled: words = QStringLiteral("Search stopped"); break;
		case core::source_search_status::partial:
			words   = QStringLiteral("%1 matches; incomplete").arg(aProjection.Summary.Matches);
			compact = aProjection.CurrentMatch ? QStringLiteral("%1 of %2?").arg(*aProjection.CurrentMatch + 1U).arg(aProjection.Summary.Matches) : QStringLiteral("%1?").arg(aProjection.Summary.Matches);
			break;
		default:
			if (aProjection.NoResults) {
				words = QStringLiteral("No results");
			} else if (aProjection.Summary.Matches) {
				words = QStringLiteral("%1 of %2").arg(aProjection.CurrentMatch.value_or(0U) + 1U).arg(aProjection.Summary.Matches);
			}
			break;
		}
		if (!SeedError.isEmpty()) {
			words   = SeedError;
			compact = QStringLiteral("Too long");
			error   = true;
		} else if (aProjection.Wrapped) {
			if (compact.isEmpty()) {
				compact = words;
			}
			words += QStringLiteral("; wrapped");
		}
		Previous->setEnabled(aProjection.CurrentMatch.has_value());
		Next->setEnabled(aProjection.CurrentMatch.has_value());
		if (StatusText != words || Error != error) {
			set_status(words, compact, error);
			Count->setAccessibleDescription(ScopeText + QStringLiteral(" source. ") + words);
			if (!Files) {
				auto announcement = QAccessibleAnnouncementEvent{ Count, Count->accessibleDescription() };
				announcement.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
				QAccessible::updateAccessibility(&announcement);
			}
		}
	}
}
