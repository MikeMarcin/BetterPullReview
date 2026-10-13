#include "bpr/qt/source_find_bar.hpp"
#include <QAccessible>
#include <QApplication>
#include <QHBoxLayout>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

namespace bpr::qt {
	source_find_bar::source_find_bar(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("sourceFindBar"));
		setAccessibleName(QStringLiteral("Find in source"));
		auto* column = new QVBoxLayout{ this };
		column->setContentsMargins(4, 2, 4, 2);
		column->setSpacing(2);
		auto* top = new QHBoxLayout;
		Scope     = new QLabel{ this };
		Scope->setTextFormat(Qt::PlainText);
		top->addWidget(Scope);
		Query = new QPlainTextEdit{ this };
		Query->setObjectName(QStringLiteral("sourceFindQuery"));
		Query->setAccessibleName(QStringLiteral("Find query"));
		Query->setAccessibleDescription(QStringLiteral("Find source text. Enter next match; Shift+Enter previous; Ctrl+Enter newline; Escape closes Find."));
		Query->setPlaceholderText(QStringLiteral("Find"));
		Query->setTabChangesFocus(true);
		Query->setMinimumWidth(80);
		Query->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		Query->installEventFilter(this);
		top->addWidget(Query, 1);
		auto*      optionsRow = new QHBoxLayout;
		const auto button     = [this](const QString& aName, const QString& aText, const QString& aDescription, const bool aToggle) {
			auto* control = new QToolButton{ this };
			control->setObjectName(aName);
			control->setText(aText);
			control->setAccessibleName(aDescription);
			control->setToolTip(aDescription);
			control->setCheckable(aToggle);
			control->installEventFilter(this);
			return control;
		};
		Previous    = button(QStringLiteral("sourceFindPrevious"), QStringLiteral("↑"), QStringLiteral("Previous match"), false);
		Next        = button(QStringLiteral("sourceFindNext"), QStringLiteral("↓"), QStringLiteral("Next match"), false);
		auto* close = button(QStringLiteral("sourceFindClose"), QStringLiteral("×"), QStringLiteral("Close Find"), false);
		for (auto* control : { Previous, Next, close }) {
			top->addWidget(control);
		}
		Case      = button(QStringLiteral("sourceFindCase"), QStringLiteral("Aa"), QStringLiteral("Match case"), true);
		Word      = button(QStringLiteral("sourceFindWord"), QStringLiteral("Word"), QStringLiteral("Whole word"), true);
		Regex     = button(QStringLiteral("sourceFindRegex"), QStringLiteral(".*"), QStringLiteral("Regular expression"), true);
		Selection = button(QStringLiteral("sourceFindSelection"), QStringLiteral("Selection"), QStringLiteral("Find in Selection"), true);
		for (auto* control : { Case, Word, Regex, Selection }) {
			optionsRow->addWidget(control);
		}
		Status = new QLabel{ this };
		Status->setTextFormat(Qt::PlainText);
		Status->setWordWrap(true);
		Status->setObjectName(QStringLiteral("sourceFindStatus"));
		Status->setAccessibleName(QStringLiteral("Find match status"));
		optionsRow->addWidget(Status, 1);
		column->addLayout(top);
		column->addLayout(optionsRow);
		Debounce = new QTimer{ this };
		Debounce->setSingleShot(true);
		Debounce->setInterval(150);
		connect(Debounce, &QTimer::timeout, this, [this] {if(Dispatch){ Dispatch(source_find_gesture::edit);
} });
		connect(Query, &QPlainTextEdit::textChanged, this, [this] { schedule(); });
		for (auto* control : { Case, Word, Regex }) {
			connect(control, &QToolButton::toggled, this, [this] { flush(); });
		}
		connect(Selection, &QToolButton::toggled, this, [this] {if(Dispatch){ Dispatch(source_find_gesture::selection);
} });
		connect(Previous, &QToolButton::clicked, this, [this] {flush(); if(Dispatch){ Dispatch(source_find_gesture::previous);
} });
		connect(Next, &QToolButton::clicked, this, [this] {flush(); if(Dispatch){ Dispatch(source_find_gesture::next);
} });
		connect(close, &QToolButton::clicked, this, [this] {Debounce->stop(); if(Dispatch){ Dispatch(source_find_gesture::close);
} });
		adapt_font();
		hide();
	}
	void source_find_bar::adapt_font()
	{
		setFont(QApplication::font());
		Query->setFixedHeight(fontMetrics().lineSpacing() * 2 + 10);
	}
	void source_find_bar::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::ApplicationFontChange) {
			adapt_font();
		}
	}
	void source_find_bar::schedule()
	{
		SeedError.clear();
		// Obsolete decorations disappear immediately, before the typing debounce.
		Projection.Matches.reset();
		Projection.CurrentMatch.reset();
		Status->setText(Query->toPlainText().isEmpty() ? QStringLiteral("Type to find") : QStringLiteral("Searching…"));
		Previous->setEnabled(false);
		Next->setEnabled(false);
		if (Dispatch) {
			Dispatch(source_find_gesture::query_dirty);
		}
		Status->setText(Query->toPlainText().isEmpty() ? QStringLiteral("Type to find") : QStringLiteral("Searching…"));
		if (!Composing) {
			Debounce->start();
		}
	}
	void source_find_bar::flush()
	{
		Debounce->stop();
		if (!Composing && Dispatch) {
			Dispatch(source_find_gesture::edit);
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
		if (aWatched != Query && (aEvent->type() == QEvent::KeyPress || aEvent->type() == QEvent::ShortcutOverride)
			&& static_cast<QKeyEvent*>(aEvent)->key() == Qt::Key_Escape) {
			aEvent->accept();
			if (aEvent->type() == QEvent::KeyPress) {
				Debounce->stop();
				if (Dispatch) {
					Dispatch(source_find_gesture::close);
				}
			}
			return true;
		}
		if (aWatched == Query && (aEvent->type() == QEvent::KeyPress || aEvent->type() == QEvent::ShortcutOverride)) {
			auto* key = static_cast<QKeyEvent*>(aEvent);
			if (key->key() == Qt::Key_Escape || key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
				aEvent->accept();
				if (aEvent->type() == QEvent::ShortcutOverride) {
					return true;
				}
				if (key->key() == Qt::Key_Escape) {
					Debounce->stop();
					if (Dispatch) {
						Dispatch(source_find_gesture::close);
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
					if (!pending && Dispatch) {
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
		}
		Query->setFocus(Qt::ShortcutFocusReason);
		Query->selectAll();
	}
	void source_find_bar::set_scope(const core::source_search_target aTarget, const bool aHasSelection, QString aReason)
	{
		Scope->setText(aTarget == core::source_search_target::left ? QStringLiteral("Earlier") : aTarget == core::source_search_target::right ? QStringLiteral("Later")
																																			  : QStringLiteral("Unified"));
		Selection->setEnabled(aHasSelection || Selection->isChecked());
		Selection->setToolTip(aHasSelection ? QStringLiteral("Search the captured original selection") : std::move(aReason));
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
	}
	void source_find_bar::reset_query()
	{
		SeedError.clear();
		Debounce->stop();
		const auto queryBlock = QSignalBlocker{ Query };
		Query->clear();
		for (auto* control : { Case, Word, Regex, Selection }) {
			const auto block = QSignalBlocker{ control };
			control->setChecked(false);
		}
		hide();
	}
	auto source_find_bar::query_editor() const -> QPlainTextEdit* { return Query; }
	auto source_find_bar::projection() const -> const application::source_search_projection& { return Projection; }
	void source_find_bar::present(const application::source_search_projection& aProjection)
	{
		Projection = aProjection;
		auto words = QStringLiteral("Type to find");
		switch (aProjection.Status) {
		case core::source_search_status::searching:           words = QStringLiteral("Searching…"); break;
		case core::source_search_status::invalid_pattern:     words = QStringLiteral("Invalid regular expression"); break;
		case core::source_search_status::unsupported_pattern: words = QStringLiteral("Unsupported regular expression"); break;
		case core::source_search_status::invalid_input:       words = QStringLiteral("Query exceeds 4 KiB or is invalid text"); break;
		case core::source_search_status::unavailable:         words = QStringLiteral("Source unavailable"); break;
		case core::source_search_status::failure:             words = QStringLiteral("Search failed; edit the query to retry"); break;
		case core::source_search_status::resource_limit:      words = QStringLiteral("Search resource limit; narrow the query"); break;
		case core::source_search_status::result_limit:        words = QStringLiteral("%1 shown; limit reached").arg(aProjection.Summary.Matches); break;
		case core::source_search_status::cancelled:           words = QStringLiteral("Search stopped"); break;
		case core::source_search_status::partial:             words = QStringLiteral("%1 matches; incomplete").arg(aProjection.Summary.Matches); break;
		default:
			if (aProjection.NoResults) {
				words = QStringLiteral("No results");
			} else if (aProjection.Summary.Matches) {
				words = QStringLiteral("%1 of %2").arg(aProjection.CurrentMatch.value_or(0U) + 1U).arg(aProjection.Summary.Matches);
			}
			break;
		}
		if (!SeedError.isEmpty()) {
			words = SeedError;
		} else if (aProjection.Wrapped) {
			words += QStringLiteral("; wrapped");
		}
		Previous->setEnabled(aProjection.CurrentMatch.has_value());
		Next->setEnabled(aProjection.CurrentMatch.has_value());
		if (Status->text() != words) {
			Status->setText(words);
			Status->setAccessibleDescription(Scope->text() + QStringLiteral(" source. ") + words);
			auto announcement = QAccessibleAnnouncementEvent{ Status, Status->accessibleDescription() };
			announcement.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&announcement);
		}
	}
}
