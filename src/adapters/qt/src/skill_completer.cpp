#include "bpr/qt/skill_completer.hpp"

#include <QAbstractItemView>
#include <QApplication>
#include <QKeyEvent>
#include <QListWidget>
#include <QPainter>
#include <QPlainTextEdit>
#include <QScreen>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QTextBlock>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		constexpr int source_role      = Qt::UserRole + 1;
		constexpr int description_role = Qt::UserRole + 2;
		constexpr int kind_role        = Qt::UserRole + 3;
		constexpr int origin_role      = Qt::UserRole + 4;

		[[nodiscard]] auto text_of(const std::string& aText) -> QString { return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size())); }

		/// Each row: the name to insert, where the skill came from beside it,
		/// and its description on a second, quieter line. Colors and fonts
		/// come from the row's own style option, so selection, focus and the
		/// appearance stay native.
		class skill_row_delegate final : public QStyledItemDelegate
		{
		public:
			using QStyledItemDelegate::QStyledItemDelegate;

			void paint(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const override
			{
				auto option = aOption;
				initStyleOption(&option, aIndex);
				const auto name = option.text;
				option.text.clear();
				const auto* const widget = option.widget;
				auto* const       style  = widget != nullptr ? widget->style() : QApplication::style();
				style->drawControl(QStyle::CE_ItemViewItem, &option, aPainter, widget);
				const bool selected = (option.state & QStyle::State_Selected) != 0;
				const auto text     = option.palette.color(selected ? QPalette::HighlightedText : QPalette::Text);
				auto       quiet    = text;
				quiet.setAlphaF(selected ? 0.85F : 0.62F);
				const auto area = option.rect.adjusted(10, 5, -10, -5);
				auto       bold = option.font;
				bold.setWeight(QFont::DemiBold);
				const auto boldMetrics = QFontMetrics{ bold };
				const auto metrics     = QFontMetrics{ option.font };
				aPainter->save();
				// The name, and on the right which kind of skill it is; then
				// where it came from and what it is for.
				const auto kind      = aIndex.data(kind_role).toString();
				const auto kindWidth = metrics.horizontalAdvance(kind) + 12;
				aPainter->setFont(bold);
				aPainter->setPen(text);
				aPainter->drawText(QRect{ area.left(), area.top(), area.width() - kindWidth, boldMetrics.height() }, Qt::AlignLeft | Qt::AlignVCenter,
								   boldMetrics.elidedText(name, Qt::ElideRight, area.width() - kindWidth));
				aPainter->setFont(option.font);
				aPainter->setPen(quiet);
				aPainter->drawText(QRect{ area.right() - kindWidth, area.top(), kindWidth, boldMetrics.height() }, Qt::AlignRight | Qt::AlignVCenter, kind);
				aPainter->drawText(QRect{ area.left(), area.top() + boldMetrics.height() + 2, area.width(), metrics.height() }, Qt::AlignLeft | Qt::AlignVCenter,
								   metrics.elidedText(aIndex.data(origin_role).toString(), Qt::ElideMiddle, area.width()));
				const auto description = aIndex.data(description_role).toString();
				if (!description.isEmpty()) {
					aPainter->drawText(QRect{ area.left(), area.top() + boldMetrics.height() + metrics.height() + 4, area.width(), metrics.height() }, Qt::AlignLeft | Qt::AlignVCenter,
									   metrics.elidedText(description, Qt::ElideRight, area.width()));
				}
				aPainter->restore();
			}

			[[nodiscard]] auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const -> QSize override
			{
				const auto metrics = QFontMetrics{ aOption.font };
				const auto lines   = aIndex.data(description_role).toString().isEmpty() ? 2 : 3;
				return { 360, 10 + lines * metrics.height() + (lines - 1) * 2 + 2 };
			}
		};
	}

	skill_completer::skill_completer(QPlainTextEdit* const aEditor)
		: QObject{ aEditor }
		, Editor{ aEditor }
	{
		Popup = new QListWidget{ };
		Popup->setObjectName(QStringLiteral("skillCompletion"));
		Popup->setAccessibleName(tr("Skills"));
		// A tool tip window shows above the field and never takes its focus,
		// so typing goes on in the field while the list is open.
		Popup->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint);
		Popup->setAttribute(Qt::WA_ShowWithoutActivating);
		Popup->setFocusPolicy(Qt::NoFocus);
		Popup->setSelectionMode(QAbstractItemView::SingleSelection);
		Popup->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Popup->setUniformItemSizes(true);
		Popup->setItemDelegate(new skill_row_delegate{ Popup });
		connect(Popup, &QListWidget::itemClicked, this, [this](QListWidgetItem* aItem) { accept_row(Popup->row(aItem)); });
		Editor->installEventFilter(this);
		connect(Editor, &QPlainTextEdit::textChanged, this, [this] { refresh(); });
		connect(Editor, &QPlainTextEdit::cursorPositionChanged, this, [this] { refresh(); });
	}

	skill_completer::~skill_completer() { delete Popup; }

	void skill_completer::present(std::vector<core::skill_entry> aCatalog)
	{
		Catalog = std::move(aCatalog);
		refresh();
	}

	auto skill_completer::rows() const -> QStringList
	{
		auto shown = QStringList{ };
		if (Popup->isHidden()) {
			return shown;
		}
		for (int row = 0; row < Popup->count(); ++row) {
			shown.push_back(Popup->item(row)->text() + QStringLiteral(" (") + Popup->item(row)->data(source_role).toString() + QStringLiteral(")"));
		}
		return shown;
	}

	void skill_completer::accept_current()
	{
		if (!Popup->isHidden() && Popup->currentRow() >= 0) {
			accept_row(Popup->currentRow());
		}
	}

	auto skill_completer::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aWatched == Editor && aEvent->type() == QEvent::FocusOut) {
			close_popup();
		}
		if (aWatched != Editor || aEvent->type() != QEvent::KeyPress || Popup->isHidden()) {
			return QObject::eventFilter(aWatched, aEvent);
		}
		const auto* const key = static_cast<QKeyEvent*>(aEvent);
		switch (key->key()) {
		case Qt::Key_Down:
			Popup->setCurrentRow((Popup->currentRow() + 1) % Popup->count());
			return true;
		case Qt::Key_Up:
			Popup->setCurrentRow((Popup->currentRow() + Popup->count() - 1) % Popup->count());
			return true;
		case Qt::Key_Return:
		case Qt::Key_Enter:
		case Qt::Key_Tab:
			accept_current();
			return true;
		case Qt::Key_Escape:
			close_popup();
			return true;
		default:
			return QObject::eventFilter(aWatched, aEvent);
		}
	}

	void skill_completer::refresh()
	{
		if (Inserting) {
			return;
		}
		const auto text   = Editor->toPlainText();
		const auto cursor = Editor->textCursor();
		if (Catalog.empty() || cursor.hasSelection() || !Editor->isVisible()) {
			close_popup();
			return;
		}
		const auto before = text.left(cursor.position()).toUtf8();
		const auto bytes  = text.toUtf8();
		Context           = core::skill_completion_at(std::string_view{ bytes.constData(), static_cast<std::size_t>(bytes.size()) }, static_cast<std::size_t>(before.size()));
		Rows              = Context ? core::complete_skill_references(Context->Prefix, Catalog) : std::vector<core::skill_completion>{ };
		if (Rows.empty()) {
			close_popup();
			return;
		}
		Popup->clear();
		for (const auto& row : Rows) {
			const auto& entry = Catalog[row.Entry];
			auto* const item  = new QListWidgetItem{ QStringLiteral("/") + text_of(row.Insert), Popup };
			item->setData(source_role, entry.Source == core::skill_source::personal ? tr("Personal, %1").arg(text_of(entry.Origin)) : tr("Repository, %1").arg(text_of(entry.Origin)));
			item->setData(origin_role, text_of(entry.Origin));
			item->setData(kind_role, entry.Source == core::skill_source::personal ? tr("Personal") : row.Shadowed ? tr("Repository, hidden by yours")
																												  : tr("Repository"));
			item->setData(description_role, text_of(entry.Description));
			item->setToolTip(text_of(entry.Description));
		}
		Popup->setCurrentRow(0);
		const auto rowHeight = Popup->sizeHintForRow(0);
		const auto visible   = std::min<int>(Popup->count(), 5);
		const auto width     = std::max(420, Editor->width() / 2);
		Popup->resize(width, visible * rowHeight + 2 * Popup->frameWidth() + 2);
		auto place = Editor->viewport()->mapToGlobal(Editor->cursorRect().bottomLeft() + QPoint{ 0, 4 });
		if (const auto* const screen = Editor->screen(); screen != nullptr) {
			const auto room = screen->availableGeometry();
			if (place.y() + Popup->height() > room.bottom()) {
				place.setY(Editor->viewport()->mapToGlobal(Editor->cursorRect().topLeft()).y() - Popup->height() - 4);
			}
			place.setX(std::clamp(place.x(), room.left(), std::max(room.left(), room.right() - Popup->width())));
		}
		Popup->move(place);
		Popup->show();
	}

	void skill_completer::close_popup()
	{
		Context.reset();
		Rows.clear();
		Popup->hide();
	}

	void skill_completer::accept_row(const int aRow)
	{
		if (!Context || aRow < 0 || static_cast<std::size_t>(aRow) >= Rows.size()) {
			return;
		}
		auto       cursor = Editor->textCursor();
		const auto prefix = text_of(Context->Prefix);
		// The typed prefix is ASCII (letters, digits, `-` and `:`), so its
		// length in UTF-16 is its length in bytes.
		cursor.setPosition(cursor.position() - static_cast<int>(prefix.size()), QTextCursor::KeepAnchor);
		const auto next     = Editor->document()->characterAt(cursor.selectionEnd());
		const auto trailing = next == QLatin1Char(' ') || next == QLatin1Char('\t') ? QString{ } : QStringLiteral(" ");
		Inserting           = true;
		cursor.insertText(text_of(Rows[static_cast<std::size_t>(aRow)].Insert) + trailing);
		Editor->setTextCursor(cursor);
		Inserting = false;
		close_popup();
	}
}
