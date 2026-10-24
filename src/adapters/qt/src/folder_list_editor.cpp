#include "bpr/qt/folder_list_editor.hpp"

#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QSignalBlocker>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <utility>

namespace bpr::qt {
	namespace {
		/// The text a row held before an edit, to go back to on a refusal.
		constexpr auto previous_role = Qt::UserRole + 1;

		/// \brief An editing row: the path field and its **…** browser.
		class folder_row_editor final : public QWidget
		{
		public:
			explicit folder_row_editor(QWidget* aParent)
				: QWidget{ aParent }
			{
				setAutoFillBackground(true);
				auto* const row = new QHBoxLayout{ this };
				row->setContentsMargins(0, 0, 0, 0);
				row->setSpacing(2);
				Field = new QLineEdit{ this };
				Field->setObjectName(QStringLiteral("folderRowPath"));
				Field->setFrame(false);
				Field->setPlaceholderText(QDir::toNativeSeparators(QStringLiteral("~/Documents/GitHub")));
				Field->setAccessibleName(tr("Folder path"));
				row->addWidget(Field, 1);
				Browse = new QToolButton{ this };
				Browse->setObjectName(QStringLiteral("folderRowBrowse"));
				Browse->setText(QStringLiteral("…"));
				Browse->setToolTip(tr("Choose the folder in a file browser"));
				Browse->setAccessibleName(tr("Browse for the folder"));
				row->addWidget(Browse);
				connect(Browse, &QToolButton::clicked, this, [this] {
					const auto start  = Field->text().trimmed().isEmpty() ? QDir::homePath() : Field->text().trimmed();
					const auto folder = QFileDialog::getExistingDirectory(this, tr("Choose a clone folder"), start);
					if (!folder.isEmpty()) {
						Field->setText(QDir::toNativeSeparators(folder));
					}
					Field->setFocus(Qt::OtherFocusReason);
				});
				setFocusProxy(Field);
			}
			QLineEdit*   Field  = nullptr;
			QToolButton* Browse = nullptr;
		};

		class folder_row_delegate final : public QStyledItemDelegate
		{
		public:
			folder_row_delegate(QObject* aParent, std::function<void(bool)> aEditing)
				: QStyledItemDelegate{ aParent }
				, Editing{ std::move(aEditing) }
			{ }

			auto createEditor(QWidget* aParent, const QStyleOptionViewItem& /*aOption*/, const QModelIndex& /*aIndex*/) const -> QWidget* override
			{
				Editing(true);
				auto* const editor = new folder_row_editor{ aParent };
				// Return in the path field ends the edit, as in a plain row;
				// Escape, handled by the view, abandons it.
				auto* const self = const_cast<folder_row_delegate*>(this);
				connect(editor->Field, &QLineEdit::returnPressed, self, [self, editor] {
					emit self->commitData(editor);
					emit self->closeEditor(editor, QAbstractItemDelegate::NoHint);
				});
				return editor;
			}
			void setEditorData(QWidget* aEditor, const QModelIndex& aIndex) const override
			{
				static_cast<folder_row_editor*>(aEditor)->Field->setText(aIndex.data(Qt::EditRole).toString());
			}
			void setModelData(QWidget* aEditor, QAbstractItemModel* aModel, const QModelIndex& aIndex) const override
			{
				aModel->setData(aIndex, static_cast<folder_row_editor*>(aEditor)->Field->text().trimmed(), Qt::EditRole);
			}
			void updateEditorGeometry(QWidget* aEditor, const QStyleOptionViewItem& aOption, const QModelIndex& /*aIndex*/) const override
			{
				aEditor->setGeometry(aOption.rect);
			}
			void destroyEditor(QWidget* aEditor, const QModelIndex& aIndex) const override
			{
				QStyledItemDelegate::destroyEditor(aEditor, aIndex);
				Editing(false);
			}

		private:
			std::function<void(bool)> Editing;
		};

		[[nodiscard]] auto tool(QWidget* aParent, const QStyle::StandardPixmap aIcon, const QString& aName, const QString& aObjectName) -> QToolButton*
		{
			auto* const button = new QToolButton{ aParent };
			button->setObjectName(aObjectName);
			button->setIcon(aParent->style()->standardIcon(aIcon));
			button->setToolTip(aName);
			button->setAccessibleName(aName);
			button->setAutoRaise(true);
			return button;
		}
	} // namespace

	folder_list_editor::folder_list_editor(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("folderListEditor"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(2);
		auto* const tools = new QHBoxLayout{ };
		tools->setSpacing(2);
		tools->addStretch(1);
		New    = tool(this, QStyle::SP_FileDialogNewFolder, tr("New folder"), QStringLiteral("folderListNew"));
		Remove = tool(this, QStyle::SP_DialogDiscardButton, tr("Remove the selected folder"), QStringLiteral("folderListRemove"));
		Down   = tool(this, QStyle::SP_ArrowDown, tr("Move down"), QStringLiteral("folderListDown"));
		Up     = tool(this, QStyle::SP_ArrowUp, tr("Move up"), QStringLiteral("folderListUp"));
		for (auto* const button : { New, Remove, Down, Up }) {
			tools->addWidget(button);
		}
		column->addLayout(tools);
		List = new QListWidget{ this };
		List->setObjectName(QStringLiteral("folderList"));
		List->setAccessibleName(tr("Clone folders"));
		List->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed | QAbstractItemView::SelectedClicked);
		List->setItemDelegate(new folder_row_delegate{ List, [this](const bool aEditing) {
														  Editing = aEditing;
														  update_tools();
													  } });
		List->setMinimumHeight(List->sizeHintForRow(0) > 0 ? List->sizeHintForRow(0) * 4 : 96);
		List->setMaximumHeight(150);
		column->addWidget(List);
		Status = new QLabel{ this };
		Status->setObjectName(QStringLiteral("folderListStatus"));
		Status->setWordWrap(true);
		Status->setTextFormat(Qt::PlainText);
		Status->setForegroundRole(QPalette::PlaceholderText);
		column->addWidget(Status);

		connect(New, &QToolButton::clicked, this, [this] { add_row(); });
		connect(Remove, &QToolButton::clicked, this, [this] {
			const auto row = List->currentRow();
			if (row < 0) {
				return;
			}
			delete List->takeItem(row);
			changed();
		});
		const auto move = [this](const int aBy) {
			const auto row = List->currentRow();
			const auto to  = row + aBy;
			if (row < 0 || to < 0 || to >= List->count()) {
				return;
			}
			auto* const item = List->takeItem(row);
			List->insertItem(to, item);
			List->setCurrentRow(to);
			changed();
		};
		connect(Down, &QToolButton::clicked, this, [move] { move(1); });
		connect(Up, &QToolButton::clicked, this, [move] { move(-1); });
		connect(List, &QListWidget::currentRowChanged, this, [this] { update_tools(); });
		connect(List, &QListWidget::itemChanged, this, [this](QListWidgetItem* aItem) { committed(List->row(aItem)); });
		update_tools();
	}

	void folder_list_editor::set_change(std::function<void(std::vector<std::string>)> aChange) { Change = std::move(aChange); }

	void folder_list_editor::present(const std::vector<std::string>& aFolders)
	{
		if (Editing) {
			return;
		}
		Shown            = aFolders;
		const auto row   = List->currentRow();
		const auto block = QSignalBlocker{ List };
		List->clear();
		for (const auto& folder : aFolders) {
			const auto text = QDir::toNativeSeparators(QString::fromStdString(folder));
			auto*      item = new QListWidgetItem{ text, List };
			item->setFlags(item->flags() | Qt::ItemIsEditable);
			item->setData(previous_role, text);
			item->setToolTip(text);
		}
		if (row >= 0 && row < List->count()) {
			List->setCurrentRow(row);
		}
		update_tools();
	}

	void folder_list_editor::add_row()
	{
		Status->clear();
		auto* item = static_cast<QListWidgetItem*>(nullptr);
		{
			// Making the row is not an edit of it.
			const auto block = QSignalBlocker{ List };
			item             = new QListWidgetItem{ QString{ }, List };
			item->setFlags(item->flags() | Qt::ItemIsEditable);
			item->setData(previous_role, QString{ });
		}
		List->setCurrentItem(item);
		List->editItem(item);
	}

	auto folder_list_editor::folders() const -> std::vector<std::string>
	{
		auto result = std::vector<std::string>{ };
		for (auto row = 0; row < List->count(); ++row) {
			const auto text = List->item(row)->text().trimmed();
			if (!text.isEmpty()) {
				result.push_back(text.toStdString());
			}
		}
		return result;
	}

	void folder_list_editor::committed(const int aRow)
	{
		auto* const item = List->item(aRow);
		if (item == nullptr) {
			return;
		}
		// The view is still closing the row's editor while this runs, so a row
		// that goes is taken out once that is done.
		const auto drop = [this, item] {
			QTimer::singleShot(0, this, [this, item] {
				const auto at = List->row(item);
				if (at >= 0) {
					delete List->takeItem(at);
					update_tools();
				}
			});
		};
		auto       text     = item->text().trimmed();
		const auto previous = item->data(previous_role).toString();
		const auto block    = QSignalBlocker{ List };
		if (text.isEmpty()) {
			Status->clear();
			drop();
			if (!previous.isEmpty()) {
				changed();
			}
			return;
		}
		if (text == QStringLiteral("~") || text.startsWith(QStringLiteral("~/"))) {
			text = QDir::homePath() + text.mid(1);
		}
		if (!QDir::isAbsolutePath(text)) {
			Status->setText(tr("Write the full path of the folder, starting at %1, or use … to browse.").arg(QDir::toNativeSeparators(QDir::rootPath())));
			if (previous.isEmpty()) {
				item->setText(QString{ });
				drop();
			} else {
				item->setText(previous);
			}
			return;
		}
		text = QDir::toNativeSeparators(QDir::cleanPath(text));
		item->setText(text);
		item->setToolTip(text);
		item->setData(previous_role, text);
		if (text == previous) {
			return;
		}
		Status->setText(tr("Looking in %1 for clones…").arg(text));
		changed();
	}

	void folder_list_editor::changed()
	{
		update_tools();
		if (Change) {
			Change(folders());
		}
	}

	void folder_list_editor::update_tools()
	{
		const auto row = List->currentRow();
		Remove->setEnabled(row >= 0 && !Editing);
		Up->setEnabled(row > 0 && !Editing);
		Down->setEnabled(row >= 0 && row + 1 < List->count() && !Editing);
		New->setEnabled(!Editing);
	}
} // namespace bpr::qt
