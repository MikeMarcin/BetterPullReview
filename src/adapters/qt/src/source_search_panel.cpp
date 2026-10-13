#include "bpr/qt/source_search_panel.hpp"
#include <QAbstractItemModel>
#include <QAccessible>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <algorithm>
#include <map>
namespace bpr::qt {
	/// Immutable host records are shared; only numeric file groups are retained.
	/// Bounded preview text is converted to QString when a visible row asks for it.
	class source_search_result_model final : public QAbstractItemModel
	{
	public:
		explicit source_search_result_model(QObject* aParent)
			: QAbstractItemModel{ aParent }
		{ }
		auto index(int aRow, int aColumn, const QModelIndex& aParent = { }) const -> QModelIndex override
		{
			if (aRow < 0 || aColumn != 0 || aRow >= rowCount(aParent)) {
				return { };
			}
			return createIndex(aRow, aColumn, aParent.isValid() ? static_cast<quintptr>(aParent.row() + 1) : 0);
		}
		auto parent(const QModelIndex& aChild) const -> QModelIndex override
		{
			return aChild.isValid() && aChild.internalId() != 0 ? createIndex(static_cast<int>(aChild.internalId() - 1), 0, quintptr{ 0 }) : QModelIndex{ };
		}
		auto rowCount(const QModelIndex& aParent = { }) const -> int override
		{
			if (!aParent.isValid()) {
				return static_cast<int>(Groups.size());
			}
			return aParent.internalId() == 0 && aParent.row() < static_cast<int>(Groups.size()) ? static_cast<int>(Groups[static_cast<std::size_t>(aParent.row())].Count) : 0;
		}
		auto columnCount(const QModelIndex& = { }) const -> int override { return 1; }
		auto data(const QModelIndex& aIndex, int aRole = Qt::DisplayRole) const -> QVariant override
		{
			if (!aIndex.isValid()) {
				return { };
			}
			const auto leaf  = aIndex.internalId() != 0;
			const auto group = leaf ? aIndex.internalId() - 1 : static_cast<std::size_t>(aIndex.row());
			if (group >= Groups.size() || !Corpus) {
				return { };
			}
			const auto& g = Groups[group];
			if (aRole == Qt::UserRole) {
				return leaf ? QVariant{ static_cast<unsigned>(g.Begin + static_cast<std::size_t>(aIndex.row()) + 1) } : QVariant{ };
			}
			if (aRole == Qt::FontRole) {
				return leaf ? QVariant{ SnippetFont } : QVariant{ };
			}
			if (aRole != Qt::DisplayRole && aRole != Qt::ToolTipRole && aRole != Qt::AccessibleTextRole) {
				return { };
			}
			if (!leaf) {
				const auto& entry = Corpus->Entries[g.Entry];
				auto        path  = QString::fromStdString(std::string{ entry.path() });
				if (entry.Left && entry.Right && entry.Left->Path != entry.Right->Path) {
					path = QString::fromStdString(entry.Left->Path) + QStringLiteral(" → ") + QString::fromStdString(entry.Right->Path);
				}
				return path + QStringLiteral("  (%1)").arg(g.Count);
			}
			if (!Matches || g.Begin + static_cast<std::size_t>(aIndex.row()) >= Matches->size()) {
				return { };
			}
			const auto& hit      = (*Matches)[g.Begin + static_cast<std::size_t>(aIndex.row())];
			const auto& left     = hit.Match.Locations[0];
			const auto& right    = hit.Match.Locations[1];
			const auto& location = left ? left : right;
			if (!location) {
				return { };
			}
			const auto preview = QString::fromStdString(hit.Match.Preview).replace(QLatin1Char('\n'), QChar{ 0x21b5 });
			if (left && right) {
				return tr("Left %1:%2; Right %3:%4  %5").arg(left->Line + 1U).arg(left->Column + 1U).arg(right->Line + 1U).arg(right->Column + 1U).arg(preview);
			}
			return QStringLiteral("%1 %2:%3  %4").arg(left ? tr("Left") : tr("Right")).arg(location->Line + 1U).arg(location->Column + 1U).arg(preview);
		}
		void present(const application::source_search_projection& aProjection, bool aClear)
		{
			if (aClear || !aProjection.Matches || aProjection.Matches->size() < Count) {
				if (!Groups.empty()) {
					beginRemoveRows({ }, 0, static_cast<int>(Groups.size() - 1));
					Groups.clear();
					Count = 0;
					endRemoveRows();
				}
			}
			if (Corpus != aProjection.Corpus) {
				Corpus = aProjection.Corpus;
				Entries.clear();
				if (Corpus) {
					for (std::size_t i = 0; i < Corpus->Entries.size(); ++i) {
						Entries.emplace(Corpus->Entries[i].File, i);
					}
				}
			}
			Matches = aProjection.Matches;
			if (!Matches || !Corpus) {
				return;
			}
			while (Count < Matches->size()) {
				const auto file  = (*Matches)[Count].File;
				const auto found = Entries.find(file);
				if (found == Entries.end()) {
					return;
				}
				if (Groups.empty() || Corpus->Entries[Groups.back().Entry].File != file) {
					const auto row = static_cast<int>(Groups.size());
					beginInsertRows({ }, row, row);
					Groups.push_back({ found->second, Count, 0 });
					endInsertRows();
				}
				auto& g   = Groups.back();
				auto  end = Count + 1;
				while (end < Matches->size() && (*Matches)[end].File == file) {
					++end;
				}
				const auto parent = index(static_cast<int>(Groups.size() - 1), 0);
				beginInsertRows(parent, static_cast<int>(g.Count), static_cast<int>(g.Count + end - Count - 1));
				g.Count += end - Count;
				Count = end;
				endInsertRows();
				emit dataChanged(parent, parent);
			}
		}
		auto match_index(std::uint32_t aMatch) const -> QModelIndex
		{
			if (aMatch >= Count) {
				return { };
			}
			const auto next  = std::upper_bound(Groups.begin(), Groups.end(), aMatch, [](auto aValue, const group& aGroup) { return aValue < aGroup.Begin; });
			const auto found = next - 1;
			const auto row   = static_cast<int>(found - Groups.begin());
			return index(static_cast<int>(aMatch - found->Begin), 0, index(row, 0));
		}
		auto path(core::file_identity aFile) const -> QString
		{
			const auto found = Entries.find(aFile);
			return found == Entries.end() || !Corpus ? QString{ } : QString::fromStdString(std::string{ Corpus->Entries[found->second].path() });
		}
		void set_snippet_font(const QFont& aFont)
		{
			if (SnippetFont == aFont) {
				return;
			}
			SnippetFont = aFont;
			for (int i = 0; i < static_cast<int>(Groups.size()); ++i) {
				const auto p = index(i, 0);
				if (Groups[static_cast<std::size_t>(i)].Count) {
					emit dataChanged(index(0, 0, p), index(static_cast<int>(Groups[static_cast<std::size_t>(i)].Count - 1), 0, p), { Qt::FontRole });
				}
			}
		}

	private:
		struct group
		{
			std::size_t Entry;
			std::size_t Begin;
			std::size_t Count;
		};
		std::vector<group>                                                   Groups;
		std::map<core::file_identity, std::size_t>                           Entries;
		std::size_t                                                          Count = 0;
		std::shared_ptr<const application::source_corpus>                    Corpus;
		std::shared_ptr<const std::vector<application::source_search_match>> Matches;
		QFont                                                                SnippetFont;
	};

	source_search_panel::source_search_panel(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("sourceSearchPanel"));
		setAccessibleName(tr("Find in Files"));
		auto* column = new QVBoxLayout{ this };
		column->setContentsMargins(4, 2, 4, 2);
		column->setSpacing(3);
		Bar = new source_find_bar{ this, true };
		Bar->show();
		column->addWidget(Bar);
		Bar->Dispatch = [this](auto aGesture) {if(Dispatch){Dispatch(aGesture);} };
		Scope         = new QLabel{ this };
		Scope->setAccessibleName(tr("Pull request and exact comparison"));
		Scope->setTextFormat(Qt::PlainText);
		Scope->setWordWrap(true);
		column->addWidget(Scope);
		auto* details = new QToolButton{ this };
		details->setText(tr("Search Details"));
		details->setCheckable(true);
		details->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		details->setArrowType(Qt::RightArrow);
		column->addWidget(details, 0, Qt::AlignLeft);
		auto* body   = new QWidget{ this };
		auto* fields = new QVBoxLayout{ body };
		fields->setContentsMargins(0, 0, 0, 0);
		Identity = new QLabel{ body };
		Identity->setAccessibleName(tr("Exact source commit identities"));
		Identity->setTextFormat(Qt::PlainText);
		Identity->setWordWrap(true);
		Identity->setTextInteractionFlags(Qt::TextSelectableByMouse);
		fields->addWidget(Identity);
		const auto field = [&](const QString& aLabel, const QString& aName) {auto* label=new QLabel{aLabel,body};auto* editor=new QLineEdit{body}; editor->setObjectName(aName);editor->setAccessibleName(aLabel);editor->setAccessibleDescription(tr("Case-sensitive repository paths; comma alternatives; *, ?, **, braces and character classes; exclude wins."));label->setBuddy(editor);fields->addWidget(label);fields->addWidget(editor);return editor; };
		Include          = field(tr("Files to include"), QStringLiteral("sourceFilesInclude"));
		Exclude          = field(tr("Files to exclude"), QStringLiteral("sourceFilesExclude"));
		Skipped          = new QLabel{ body };
		Skipped->setTextFormat(Qt::PlainText);
		Skipped->setWordWrap(true);
		fields->addWidget(Skipped);
		body->hide();
		column->addWidget(body);
		connect(details, &QToolButton::toggled, this, [body, details](bool aShown) {body->setVisible(aShown);details->setArrowType(aShown?Qt::DownArrow:Qt::RightArrow); });
		Coverage = new QLabel{ this };
		Coverage->setObjectName(QStringLiteral("sourceSearchCoverage"));
		Coverage->setTextFormat(Qt::PlainText);
		Coverage->setWordWrap(true);
		Coverage->setAccessibleName(tr("Search coverage"));
		column->addWidget(Coverage);
		Retry = new QToolButton{ this };
		Retry->setText(tr("Retry unavailable reads"));
		Retry->setObjectName(QStringLiteral("sourceSearchRetry"));
		column->addWidget(Retry, 0, Qt::AlignLeft);
		Retry->hide();
		connect(Retry, &QToolButton::clicked, this, [this] {if(Dispatch){Dispatch(source_find_gesture::selection);} });
		Tree = new QTreeView{ this };
		Tree->setObjectName(QStringLiteral("sourceSearchResults"));
		Tree->setAccessibleName(tr("Search results by file, source side, line and column"));
		Tree->setHeaderHidden(true);
		Tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
		Tree->setUniformRowHeights(true);
		Tree->setTextElideMode(Qt::ElideNone);
		Tree->installEventFilter(this);
		Model = new source_search_result_model{ Tree };
		Tree->setModel(Model);
		connect(Tree->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& aIndex, const QModelIndex&) {const auto index=aIndex.data(Qt::UserRole).toUInt();if(index && Select){Select(index-1U);} });
		column->addWidget(Tree, 1);
		connect(Tree, &QTreeView::clicked, this, [this](const QModelIndex& aIndex) {const auto index=aIndex.data(Qt::UserRole).toUInt();if(index && Activate){Activate(index-1U);} });
		Debounce = new QTimer{ this };
		Debounce->setSingleShot(true);
		Debounce->setInterval(150);
		connect(Debounce, &QTimer::timeout, this, [this] { Bar->flush(); });
		for (auto* editor : { Include, Exclude }) {
			editor->installEventFilter(this);
			connect(editor, &QLineEdit::textChanged, this, [this] {if(Dispatch){Dispatch(source_find_gesture::query_dirty);}Debounce->start(); });
		}
	}
	auto source_search_panel::eventFilter(QObject* aWatched, QEvent* aEvent) -> bool
	{
		if (aEvent->type() == QEvent::KeyPress) {
			auto* key = static_cast<QKeyEvent*>(aEvent);
			if (key->key() == Qt::Key_Escape) {
				if (Dispatch) {
					Dispatch(source_find_gesture::escape);
				}
				return true;
			}
			if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
				if (aWatched == Tree) {
					const auto index = Tree->currentIndex().data(Qt::UserRole).toUInt();
					if (index && Activate) {
						Activate(index - 1U);
					}
				} else {
					Debounce->stop();
					Bar->flush();
				}
				return true;
			}
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}
	void source_search_panel::append_results(const application::source_search_projection& aProjection)
	{
		const auto clear  = Projection.Scope != aProjection.Scope || Projection.Query != aProjection.Query || Projection.Options != aProjection.Options || Projection.Include != aProjection.Include || Projection.Exclude != aProjection.Exclude;
		const auto before = clear ? 0 : Model->rowCount();
		Model->present(aProjection, clear);
		for (int row = before; row < Model->rowCount(); ++row) {
			Tree->setExpanded(Model->index(row, 0), true);
		}
	}
	void source_search_panel::present(const application::source_search_projection& aProjection)
	{
		const auto previous = Projection.CurrentMatch;
		append_results(aProjection);
		Projection = aProjection;
		Bar->present(aProjection);
		auto words = tr("%1 matches; %2 of %3 eligible files searched; %4 skipped sides").arg(aProjection.Summary.Matches).arg(aProjection.Summary.SearchedFiles).arg(aProjection.Summary.EligibleFiles).arg(aProjection.Summary.SkippedSides);
		if (aProjection.Status == core::source_search_status::idle) {
			words = tr("Type to search full changed files on both sides");
		} else if (aProjection.Status == core::source_search_status::searching) {
			words += tr("; searching / acquiring source");
		} else if (aProjection.Status != core::source_search_status::complete) {
			words += tr("; incomplete coverage");
		}
		if (aProjection.NoResults) {
			words = tr("No results; all %1 eligible files searched").arg(aProjection.Summary.EligibleFiles);
		}
		if (aProjection.Wrapped) {
			words += tr("; wrapped within shown results");
		}
		Coverage->setText(words);
		const auto description = Scope->text() + QStringLiteral(". ") + Bar->status_text() + QStringLiteral(". ") + words;
		Coverage->setAccessibleDescription(description);
		if (LastAnnouncement != description) {
			LastAnnouncement  = description;
			auto announcement = QAccessibleAnnouncementEvent{ Coverage, description };
			announcement.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&announcement);
		}
		Retry->setVisible(aProjection.RetryAvailable);
		if (Changed) {
			Changed();
		}
		auto skipped = QString{ };
		if (aProjection.Skipped) {
			for (const auto& side : *aProjection.Skipped) {
				if (skipped.size() > 4096) {
					skipped += tr("More skipped sides; %1 total").arg(aProjection.Summary.SkippedSides);
					break;
				}
				auto reason = tr("Read failed");
				switch (side.Reason) {
				case application::diff_blob_status::missing:                    reason = tr("Material unavailable"); break;
				case application::diff_blob_status::binary:                     reason = tr("Binary source"); break;
				case application::diff_blob_status::decode_failure:             reason = tr("Undecodable text"); break;
				case application::diff_blob_status::materialization_limit:      reason = tr("Source size or line limit"); break;
				case application::diff_blob_status::generated_file_unsupported: reason = tr("Unsupported source mode (symlink or submodule)"); break;
				case application::diff_blob_status::cancelled:                  reason = tr("Read cancelled"); break;
				default:                                                        break;
				}
				const auto path = Model->path(side.File);
				skipped += tr("%1 — %2: %3\n").arg(path, side.Side == core::diff_side::left ? tr("Left") : tr("Right"), reason);
			}
		}
		Skipped->setText(skipped);
		if (aProjection.CurrentMatch && Model->match_index(*aProjection.CurrentMatch).isValid() && (aProjection.CurrentMatch != previous || !Tree->currentIndex().isValid())) {
			const auto blocked = QSignalBlocker{ Tree->selectionModel() };
			Tree->setCurrentIndex(Model->match_index(*aProjection.CurrentMatch));
		}
	}
	void source_search_panel::set_snippet_font(const QFont& aFont) { Model->set_snippet_font(aFont); }
	void source_search_panel::set_scope_labels(QString aLabel, QString aIdentity)
	{
		Scope->setText(std::move(aLabel));
		Identity->setText(std::move(aIdentity));
	}
	void source_search_panel::cancel_pending()
	{
		Debounce->stop();
		Bar->cancel_pending();
	}
	void source_search_panel::reset()
	{
		Debounce->stop();
		Bar->reset_query();
		Bar->show();
		const auto includeBlock = QSignalBlocker{ Include };
		const auto excludeBlock = QSignalBlocker{ Exclude };
		Include->clear();
		Exclude->clear();
		present({ });
		set_scope_labels({ }, { });
	}
	auto source_search_panel::bar() const -> source_find_bar* { return Bar; }
	auto source_search_panel::include() const -> std::string { return Include->text().toUtf8().toStdString(); }
	auto source_search_panel::exclude() const -> std::string { return Exclude->text().toUtf8().toStdString(); }
	auto source_search_panel::projection() const -> const application::source_search_projection& { return Projection; }
	auto source_search_panel::tree() const -> QTreeView* { return Tree; }
}
