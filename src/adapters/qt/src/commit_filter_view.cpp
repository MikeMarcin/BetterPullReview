#include "bpr/qt/commit_filter_view.hpp"

#include "bpr/qt/bundled_code_font.hpp"

#include <QAbstractItemView>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QShowEvent>
#include <QStyle>
#include <QTextOption>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace bpr::qt {
	namespace {
		constexpr int row_margin        = 6;
		constexpr int side_indent       = 16;
		constexpr int tag_gap           = 8;
		constexpr int line_gap          = 2;
		constexpr int detail_column_gap = 10;

		[[nodiscard]] auto text(const std::string& aText) -> QString
		{
			return QString::fromStdString(aText);
		}

		[[nodiscard]] auto detail_font(const QFont& aFont) -> QFont
		{
			auto font = aFont;
			if (font.pointSizeF() > 0.0) {
				font.setPointSizeF(font.pointSizeF() * 0.92);
			}
			return font;
		}

		[[nodiscard]] auto code_font(const QFont& aFont) -> QFont
		{
			auto font = detail_font(aFont);
			if (bundled_code_font_available()) {
				font.setFamily(bundled_code_font_family());
			} else {
				font.setStyleHint(QFont::Monospace);
				font.setFamily(QStringLiteral("monospace"));
			}
			return font;
		}

		[[nodiscard]] auto files_text(const std::optional<std::size_t>& aFiles) -> QString
		{
			if (!aFiles) {
				return { };
			}
			return *aFiles == 1U ? QStringLiteral("1 file") : QStringLiteral("%1 files").arg(*aFiles);
		}

		[[nodiscard]] auto accessible_text(const application::commit_filter_row& aRow) -> QString
		{
			auto parts = QStringList{ text(aRow.Headline), QStringLiteral("commit %1").arg(text(aRow.ShortCommit)) };
			if (!aRow.Author.empty()) {
				parts << QStringLiteral("by %1").arg(text(aRow.Author));
			}
			if (const auto files = files_text(aRow.Files); !files.isEmpty()) {
				parts << files;
			}
			switch (aRow.Role) {
			case core::filter_commit_role::chain_merge: parts << QStringLiteral("merge"); break;
			case core::filter_commit_role::base_merge:  parts << QStringLiteral("merge from the base branch"); break;
			case core::filter_commit_role::side:        parts << QStringLiteral("in merge %1").arg(text(aRow.Merge)); break;
			case core::filter_commit_role::chain:       break;
			}
			if (aRow.Bulk) {
				parts << QStringLiteral("bulk");
			}
			switch (aRow.State) {
			case core::filter_commit_state::hidden:             parts << QStringLiteral("hidden"); break;
			case core::filter_commit_state::outside_comparison: parts << QStringLiteral("not in this comparison"); break;
			case core::filter_commit_state::shown:              break;
			}
			return parts.join(QStringLiteral(", "));
		}
	} // namespace

	auto commit_filter_unavailable_text(const core::commit_filter_unavailable aReason) -> QString
	{
		switch (aReason) {
		case core::commit_filter_unavailable::truncated:
			return QStringLiteral("GitHub lists only the first 250 commits of this pull request, so its commits can't be filtered.");
		case core::commit_filter_unavailable::parents_unknown:
			return QStringLiteral("GitHub didn't say how these commits connect, so they can't be filtered.");
		case core::commit_filter_unavailable::unrelated_history:
			return QStringLiteral("This pull request shares no history with its base branch, so its commits can't be filtered.");
		case core::commit_filter_unavailable::head_missing:
		case core::commit_filter_unavailable::duplicate_commit:
		case core::commit_filter_unavailable::too_many_parents:
		case core::commit_filter_unavailable::invalid_list:
			return QStringLiteral("GitHub's list of these commits doesn't add up to the pull request's head, so they can't be filtered.");
		}
		return QStringLiteral("These commits can't be filtered.");
	}

	auto commit_filter_suspension_text(const core::commit_filter_suspension aReason) -> QString
	{
		switch (aReason) {
		case core::commit_filter_suspension::reversed:
			return QStringLiteral("This comparison runs from a newer update to an older one, so the filter is paused.");
		case core::commit_filter_suspension::empty_comparison:
			return QStringLiteral("This comparison compares an update with itself, so the filter is paused.");
		case core::commit_filter_suspension::not_first_parent_range:
			return QStringLiteral("This comparison doesn't follow the pull request's commits (it crosses a force push), so the filter is paused.");
		case core::commit_filter_suspension::none:
			return { };
		}
		return { };
	}

	auto commit_filter_trees_text(const application::commit_filter_trees aTrees) -> QString
	{
		switch (aTrees) {
		case application::commit_filter_trees::loading:
			return QStringLiteral("Reading the files each commit changes…");
		case application::commit_filter_trees::objects_unavailable:
			return QStringLiteral("This review hasn't downloaded the commits between its updates, so the filter can't list their files yet.");
		case application::commit_filter_trees::limit_exceeded:
			return QStringLiteral("A commit changes more than 10,000 files, more than the filter reads, so the file list isn't filtered.");
		case application::commit_filter_trees::failed:
			return QStringLiteral("The files these commits change couldn't be read, so the file list isn't filtered.");
		case application::commit_filter_trees::idle:
		case application::commit_filter_trees::ready:
			return { };
		}
		return { };
	}

	auto commit_filter_refusal_text(const core::commit_filter_refusal aRefusal) -> QString
	{
		switch (aRefusal) {
		case core::commit_filter_refusal::base_merge_alone:
			return QStringLiteral("A merge from the base branch brings in the base branch's work, so it can't be shown on its own.");
		case core::commit_filter_refusal::empty_selection:
			return QStringLiteral("Select a commit this comparison includes first.");
		case core::commit_filter_refusal::nothing_hidden:
			return QStringLiteral("The selected commits are already shown.");
		case core::commit_filter_refusal::unknown_commit:
			return QStringLiteral("A selected commit is no longer in the pull request.");
		case core::commit_filter_refusal::suspended:
			return QStringLiteral("The filter is paused for this comparison.");
		case core::commit_filter_refusal::unavailable:
			return QStringLiteral("These commits can't be filtered.");
		case core::commit_filter_refusal::invalid_state:
			return QStringLiteral("The filter couldn't be changed.");
		case core::commit_filter_refusal::none:
			return { };
		}
		return { };
	}

	auto commit_filter_summary_text(const application::commit_filter_projection& aProjection) -> QString
	{
		if (aProjection.Status == core::commit_filter_status::active) {
			return QStringLiteral("%1 of %2 commits shown").arg(aProjection.Shown).arg(aProjection.Spanned);
		}
		if (aProjection.Status == core::commit_filter_status::suspended) {
			return QStringLiteral("Commit filter paused");
		}
		if (aProjection.FileFilterPaths != 0U) {
			return QStringLiteral("File filter on");
		}
		const auto listed = aProjection.Rows.size();
		return listed == 1U ? QStringLiteral("1 commit") : QStringLiteral("%1 commits").arg(listed);
	}

	auto commit_filter_files_text(const application::commit_filter_projection& aProjection) -> QString
	{
		if (!aProjection.FilteredFiles) {
			return { };
		}
		auto reasons = QStringList{ };
		if (aProjection.CommitFiltered) {
			reasons << QStringLiteral("filtered by commits");
		}
		if (aProjection.FileFilteredFiles != 0U) {
			reasons << QStringLiteral("%1 hidden by file filters").arg(aProjection.FileFilteredFiles);
		}
		if (!aProjection.Generated.empty()) {
			reasons << QStringLiteral("%1 generated set aside").arg(aProjection.Generated.size());
		}
		auto result = QStringLiteral("%1 of %2 files").arg(*aProjection.FilteredFiles).arg(aProjection.ComparisonFiles);
		return reasons.isEmpty() ? result : result + QStringLiteral(", ") + reasons.join(QStringLiteral(", "));
	}

	auto commit_filter_gone_text(const application::commit_filter_projection& aProjection) -> QString
	{
		auto sentences = QStringList{ };
		for (const auto& gone : aProjection.Gone) {
			sentences << QStringLiteral("Hidden commit %1 %2 is no longer in the pull request, so it no longer applies.")
							 .arg(text(gone.ShortCommit), gone.Headline.empty() ? QString{ } : QStringLiteral("(%1)").arg(text(gone.Headline)));
		}
		return sentences.join(QLatin1Char{ ' ' });
	}

	commit_filter_list_model::commit_filter_list_model(QObject* const aParent)
		: QAbstractListModel{ aParent }
	{
	}

	void commit_filter_list_model::apply(const std::vector<application::commit_filter_row>& aRows)
	{
		// Rows no longer listed leave first, from the bottom.
		for (auto row = static_cast<int>(Rows.size()) - 1; row >= 0; --row) {
			const auto& commit = Rows[static_cast<std::size_t>(row)].Commit;
			if (std::ranges::none_of(aRows, [&commit](const application::commit_filter_row& aRow) { return aRow.Commit == commit; })) {
				beginRemoveRows({ }, row, row);
				Rows.erase(Rows.begin() + row);
				endRemoveRows();
			}
		}
		// Then each position takes its commit: in place, moved up, or inserted.
		for (std::size_t position = 0U; position < aRows.size(); ++position) {
			const auto at = static_cast<int>(position);
			if (position < Rows.size() && Rows[position].Commit == aRows[position].Commit) {
				if (Rows[position] != aRows[position]) {
					Rows[position] = aRows[position];
					emit dataChanged(index(at), index(at));
				}
				continue;
			}
			const auto found = std::find_if(Rows.begin() + static_cast<std::ptrdiff_t>(std::min(position, Rows.size())), Rows.end(),
											[&](const application::commit_filter_row& aRow) { return aRow.Commit == aRows[position].Commit; });
			if (found != Rows.end()) {
				const auto from = static_cast<int>(found - Rows.begin());
				beginMoveRows({ }, from, from, { }, at);
				auto moved = std::move(*found);
				Rows.erase(found);
				Rows.insert(Rows.begin() + at, std::move(moved));
				endMoveRows();
				if (Rows[position] != aRows[position]) {
					Rows[position] = aRows[position];
					emit dataChanged(index(at), index(at));
				}
				continue;
			}
			beginInsertRows({ }, at, at);
			Rows.insert(Rows.begin() + at, aRows[position]);
			endInsertRows();
		}
	}

	auto commit_filter_list_model::row_at(const int aRow) const -> const application::commit_filter_row*
	{
		return aRow >= 0 && static_cast<std::size_t>(aRow) < Rows.size() ? &Rows[static_cast<std::size_t>(aRow)] : nullptr;
	}

	auto commit_filter_list_model::row_of(const std::string& aCommit) const -> int
	{
		const auto found = std::ranges::find(Rows, aCommit, &application::commit_filter_row::Commit);
		return found != Rows.end() ? static_cast<int>(found - Rows.begin()) : -1;
	}

	auto commit_filter_list_model::rowCount(const QModelIndex& aParent) const -> int
	{
		return aParent.isValid() ? 0 : static_cast<int>(Rows.size());
	}

	auto commit_filter_list_model::data(const QModelIndex& aIndex, const int aRole) const -> QVariant
	{
		const auto* row = row_at(aIndex.row());
		if (row == nullptr || aIndex.parent().isValid()) {
			return { };
		}
		switch (aRole) {
		case Qt::DisplayRole:        return text(row->Headline);
		case Qt::AccessibleTextRole: return accessible_text(*row);
		case Qt::ToolTipRole:        return QStringLiteral("%1\n%2").arg(text(row->Headline), text(row->Commit));
		case commit_role:            return text(row->Commit);
		case short_commit_role:      return text(row->ShortCommit);
		case author_role:            return text(row->Author);
		case files_role:             return row->Files ? static_cast<qlonglong>(*row->Files) : qlonglong{ -1 };
		case commit_kind_role:       return static_cast<int>(row->Role);
		case state_role:             return static_cast<int>(row->State);
		case merge_role:             return text(row->Merge);
		case bulk_role:              return row->Bulk;
		default:                     return { };
		}
	}

	auto commit_filter_list_model::resets() const noexcept -> int
	{
		return Resets;
	}

	commit_filter_delegate::commit_filter_delegate(QObject* const aParent)
		: QStyledItemDelegate{ aParent }
	{
	}

	auto commit_filter_delegate::tag(const QModelIndex& aIndex) -> QString
	{
		auto tags = QStringList{ };
		switch (static_cast<core::filter_commit_state>(aIndex.data(commit_filter_list_model::state_role).toInt())) {
		case core::filter_commit_state::hidden:             tags << QStringLiteral("Hidden"); break;
		case core::filter_commit_state::outside_comparison: tags << QStringLiteral("Not in this comparison"); break;
		case core::filter_commit_state::shown:              break;
		}
		if (tags.isEmpty()) {
			switch (static_cast<core::filter_commit_role>(aIndex.data(commit_filter_list_model::commit_kind_role).toInt())) {
			case core::filter_commit_role::chain_merge: tags << QStringLiteral("Merge"); break;
			case core::filter_commit_role::base_merge:  tags << QStringLiteral("Merge from base"); break;
			case core::filter_commit_role::side:
			case core::filter_commit_role::chain:       break;
			}
		}
		// RFC-BPR-0089 M2. A hint beside the state: it filters nothing.
		if (aIndex.data(commit_filter_list_model::bulk_role).toBool()) {
			tags << (tags.isEmpty() ? QStringLiteral("Bulk") : QStringLiteral("bulk"));
		}
		return tags.join(QStringLiteral(", "));
	}

	auto commit_filter_delegate::sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const -> QSize
	{
		const auto width     = aOption.widget != nullptr ? aOption.widget->width() : aOption.rect.width();
		const auto indent    = static_cast<core::filter_commit_role>(aIndex.data(commit_filter_list_model::commit_kind_role).toInt()) == core::filter_commit_role::side ? side_indent : 0;
		const auto tagText   = tag(aIndex);
		const auto tagWidth  = tagText.isEmpty() ? 0 : QFontMetrics{ detail_font(aOption.font) }.horizontalAdvance(tagText) + tag_gap;
		const auto textWidth = std::max(40, width - (2 * row_margin) - indent - tagWidth - 4);
		const auto headline  = QFontMetrics{ aOption.font }.boundingRect(QRect{ 0, 0, textWidth, 100000 }, Qt::TextWordWrap, aIndex.data(Qt::DisplayRole).toString());
		const auto detail    = std::max(QFontMetrics{ detail_font(aOption.font) }.height(), QFontMetrics{ code_font(aOption.font) }.height());
		return { width, headline.height() + line_gap + detail + (2 * row_margin) };
	}

	void commit_filter_delegate::paint(QPainter* const aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
	{
		auto option = aOption;
		initStyleOption(&option, aIndex);
		option.text.clear();
		const auto* widget = option.widget;
		auto*       style  = widget != nullptr ? widget->style() : QApplication::style();
		// Native selection and focus painting, then the row's own text.
		style->drawControl(QStyle::CE_ItemViewItem, &option, aPainter, widget);

		const auto state    = static_cast<core::filter_commit_state>(aIndex.data(commit_filter_list_model::state_role).toInt());
		const auto selected = (option.state & QStyle::State_Selected) != 0;
		const auto group    = state == core::filter_commit_state::shown ? QPalette::Normal : QPalette::Disabled;
		const auto ink      = option.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text);
		const auto quiet    = selected ? ink : option.palette.color(group, QPalette::PlaceholderText);
		const auto indent   = static_cast<core::filter_commit_role>(aIndex.data(commit_filter_list_model::commit_kind_role).toInt()) == core::filter_commit_role::side ? side_indent : 0;
		const auto box      = option.rect.adjusted(row_margin + indent, row_margin, -row_margin, -row_margin);
		const auto tagText  = tag(aIndex);
		const auto small    = detail_font(option.font);
		const auto tagWidth = tagText.isEmpty() ? 0 : QFontMetrics{ small }.horizontalAdvance(tagText);

		aPainter->save();
		auto headlineBox = box;
		headlineBox.setRight(box.right() - (tagWidth > 0 ? tagWidth + tag_gap : 0));
		aPainter->setFont(option.font);
		aPainter->setPen(ink);
		const auto headline = aIndex.data(Qt::DisplayRole).toString();
		const auto drawn    = aPainter->boundingRect(headlineBox, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, headline);
		aPainter->drawText(headlineBox, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, headline);
		if (tagWidth > 0) {
			aPainter->setFont(small);
			aPainter->setPen(quiet);
			aPainter->drawText(QRect{ box.right() - tagWidth, box.top(), tagWidth, QFontMetrics{ small }.height() }, Qt::AlignRight | Qt::AlignTop, tagText);
		}

		// The short ID, the author and the file count, left to right.
		auto       x      = box.left();
		const auto top    = drawn.bottom() + 1 + line_gap;
		const auto shortC = aIndex.data(commit_filter_list_model::short_commit_role).toString();
		const auto code   = code_font(option.font);
		aPainter->setFont(code);
		aPainter->setPen(quiet);
		aPainter->drawText(QRect{ x, top, box.right() - x, QFontMetrics{ code }.height() }, Qt::AlignLeft | Qt::AlignTop, shortC);
		x += QFontMetrics{ code }.horizontalAdvance(shortC) + detail_column_gap;
		aPainter->setFont(small);
		auto details = QStringList{ };
		if (const auto author = aIndex.data(commit_filter_list_model::author_role).toString(); !author.isEmpty()) {
			details << author;
		}
		if (const auto files = aIndex.data(commit_filter_list_model::files_role).toLongLong(); files >= 0) {
			details << files_text(static_cast<std::size_t>(files));
		}
		for (const auto& part : details) {
			if (x >= box.right()) {
				break;
			}
			aPainter->drawText(QRect{ x, top, box.right() - x, QFontMetrics{ small }.height() }, Qt::AlignLeft | Qt::AlignTop, part);
			x += QFontMetrics{ small }.horizontalAdvance(part) + detail_column_gap;
		}
		aPainter->restore();
	}

	commit_filter_tab::commit_filter_tab(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("commitFilterTab"));
		setAccessibleName(QStringLiteral("Commits"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(4, 6, 4, 4);
		column->setSpacing(4);

		auto* const top = new QHBoxLayout{ };
		top->setContentsMargins(4, 0, 0, 0);
		Status = new QLabel{ this };
		Status->setObjectName(QStringLiteral("commitFilterStatus"));
		Status->setTextFormat(Qt::PlainText);
		Status->setWordWrap(true);
		Status->setAccessibleName(QStringLiteral("Commit filter"));
		top->addWidget(Status, 1, Qt::AlignTop);
		Clear = new QToolButton{ this };
		Clear->setObjectName(QStringLiteral("commitFilterClear"));
		Clear->setText(QStringLiteral("Clear all filters"));
		Clear->setAccessibleName(QStringLiteral("Clear all filters"));
		Clear->setAutoRaise(true);
		Clear->setFocusPolicy(Qt::StrongFocus);
		Clear->hide();
		connect(Clear, &QToolButton::clicked, this, [this] { dispatch(core::commit_filter_operation_kind::clear, { }); });
		top->addWidget(Clear, 0, Qt::AlignTop);
		column->addLayout(top);

		Model = new commit_filter_list_model{ this };
		List  = new QListView{ this };
		List->setObjectName(QStringLiteral("commitFilterList"));
		List->setAccessibleName(QStringLiteral("Commits"));
		List->setAccessibleDescription(QStringLiteral(
			"The pull request's commits in GitHub's order. Select commits with Shift and Command or Control, then open the context menu to show or hide them."));
		List->setModel(Model);
		List->setItemDelegate(new commit_filter_delegate{ List });
		List->setSelectionMode(QAbstractItemView::ExtendedSelection);
		List->setSelectionBehavior(QAbstractItemView::SelectRows);
		List->setUniformItemSizes(false);
		List->setWordWrap(true);
		List->setResizeMode(QListView::Adjust);
		List->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		List->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(List, &QWidget::customContextMenuRequested, this, [this](const QPoint& aPosition) {
			if (List->selectionModel()->selectedRows().isEmpty()) {
				if (const auto at = List->indexAt(aPosition); at.isValid()) {
					List->setCurrentIndex(at);
				}
			}
			auto* const menu = make_menu(this);
			menu->setAttribute(Qt::WA_DeleteOnClose);
			menu->popup(List->viewport()->mapToGlobal(aPosition));
		});
		column->addWidget(List, 1);

		Hint = new QLabel{ QStringLiteral("Select commits, then open the context menu to show or hide them."), this };
		Hint->setObjectName(QStringLiteral("commitFilterHint"));
		Hint->setTextFormat(Qt::PlainText);
		Hint->setWordWrap(true);
		Hint->setForegroundRole(QPalette::PlaceholderText);
		column->addWidget(Hint);
	}

	void commit_filter_tab::present(const application::commit_filter_projection& aProjection)
	{
		Projection = aProjection;
		Model->apply(aProjection.Rows);
		auto sentences = QStringList{ };
		if (aProjection.Unavailable) {
			sentences << commit_filter_unavailable_text(*aProjection.Unavailable);
		} else {
			sentences << commit_filter_summary_text(aProjection) + QStringLiteral(".");
			if (const auto files = commit_filter_files_text(aProjection); !files.isEmpty()) {
				sentences << files + QStringLiteral(".");
			}
			if (aProjection.SplitFiles > 0U) {
				sentences << (aProjection.SplitFiles == 1U ? QStringLiteral("1 file has a hidden commit between two shown ones.")
														   : QStringLiteral("%1 files have a hidden commit between two shown ones.").arg(aProjection.SplitFiles));
			}
			if (const auto paused = commit_filter_suspension_text(aProjection.Suspension); aProjection.Status == core::commit_filter_status::suspended && !paused.isEmpty()) {
				sentences << paused;
			}
			if (const auto trees = commit_filter_trees_text(aProjection.Trees); !trees.isEmpty()) {
				sentences << trees;
			}
			if (const auto gone = commit_filter_gone_text(aProjection); !gone.isEmpty()) {
				sentences << gone;
			}
		}
		if (aProjection.Refusal) {
			sentences << commit_filter_refusal_text(*aProjection.Refusal);
		}
		const auto status = sentences.join(QLatin1Char{ '\n' });
		if (Status->text() != status) {
			Status->setText(status);
		}
		Clear->setVisible(application::commit_filter_chip_shown(aProjection));
		List->setVisible(!aProjection.Rows.empty());
		Hint->setVisible(!aProjection.Rows.empty() && !aProjection.Unavailable);
	}

	void commit_filter_tab::set_gesture_dispatch(gesture_dispatch aDispatch)
	{
		Gestures = std::move(aDispatch);
	}

	void commit_filter_tab::set_shown_dispatch(std::function<void()> aDispatch)
	{
		Shown = std::move(aDispatch);
		if (Shown && isVisible() && !Announced) {
			Announced = true;
			Shown();
		}
	}

	void commit_filter_tab::set_open_dispatch(commit_dispatch aDispatch)
	{
		Open = std::move(aDispatch);
	}

	void commit_filter_tab::set_file_filter_dispatch(commits_dispatch aDispatch)
	{
		FileFilter = std::move(aDispatch);
	}

	auto commit_filter_tab::selected_commits() const -> std::vector<std::string>
	{
		auto rows = List->selectionModel()->selectedRows();
		std::ranges::sort(rows, { }, &QModelIndex::row);
		auto commits = std::vector<std::string>{ };
		for (const auto& index : rows) {
			if (const auto* row = Model->row_at(index.row())) {
				commits.push_back(row->Commit);
			}
		}
		return commits;
	}

	auto commit_filter_tab::make_menu(QWidget* const aParent) -> QMenu*
	{
		auto* const menu = new QMenu{ aParent };
		menu->setObjectName(QStringLiteral("commitFilterMenu"));
		const auto commits  = selected_commits();
		const auto operable = Projection.has_value() && !Projection->Unavailable && Projection->Status != core::commit_filter_status::suspended;
		auto       inSpan   = false;
		auto       hidden   = false;
		auto       counted  = false;
		for (const auto& commit : commits) {
			if (const auto* row = Model->row_at(Model->row_of(commit))) {
				inSpan  = inSpan || row->State != core::filter_commit_state::outside_comparison;
				hidden  = hidden || row->State == core::filter_commit_state::hidden;
				counted = counted || row->Files.has_value() || row->Role == core::filter_commit_role::side;
			}
		}
		auto* const showOnlyAction = menu->addAction(QStringLiteral("Show only selected"));
		showOnlyAction->setObjectName(QStringLiteral("commitFilterShowOnly"));
		showOnlyAction->setEnabled(operable && inSpan);
		connect(showOnlyAction, &QAction::triggered, this, [this, commits] { dispatch(core::commit_filter_operation_kind::show_only, commits); });
		auto* const hideAction = menu->addAction(QStringLiteral("Hide selected"));
		hideAction->setObjectName(QStringLiteral("commitFilterHide"));
		hideAction->setEnabled(operable && inSpan);
		connect(hideAction, &QAction::triggered, this, [this, commits] { dispatch(core::commit_filter_operation_kind::hide, commits); });
		if (hidden) {
			auto* const showAction = menu->addAction(QStringLiteral("Show selected"));
			showAction->setObjectName(QStringLiteral("commitFilterShow"));
			showAction->setEnabled(operable);
			connect(showAction, &QAction::triggered, this, [this, commits] { dispatch(core::commit_filter_operation_kind::show, commits); });
		}
		// RFC-BPR-0089 M2. A file filter: how "skip the regeneration" works
		// when a later commit also edits the generated files.
		auto* const hideFilesAction = menu->addAction(QStringLiteral("Hide files these commits changed"));
		hideFilesAction->setObjectName(QStringLiteral("commitFilterHideFiles"));
		hideFilesAction->setEnabled(operable && inSpan && counted && Projection->Trees == application::commit_filter_trees::ready && static_cast<bool>(FileFilter));
		connect(hideFilesAction, &QAction::triggered, this, [this, commits] {
			if (FileFilter) {
				FileFilter(commits);
			}
		});
		menu->addSeparator();
		auto* const clearAction = menu->addAction(QStringLiteral("Clear all filters"));
		clearAction->setObjectName(QStringLiteral("commitFilterClearAll"));
		clearAction->setEnabled(Projection.has_value() && application::commit_filter_chip_shown(*Projection));
		connect(clearAction, &QAction::triggered, this, [this] { dispatch(core::commit_filter_operation_kind::clear, { }); });
		menu->addSeparator();
		auto* const copyAction = menu->addAction(commits.size() > 1U ? QStringLiteral("Copy commit IDs") : QStringLiteral("Copy commit ID"));
		copyAction->setObjectName(QStringLiteral("commitFilterCopy"));
		copyAction->setEnabled(!commits.empty());
		connect(copyAction, &QAction::triggered, this, [commits] {
			auto lines = QStringList{ };
			for (const auto& commit : commits) {
				lines << text(commit);
			}
			if (auto* const clipboard = QGuiApplication::clipboard(); clipboard != nullptr) {
				clipboard->setText(lines.join(QLatin1Char{ '\n' }));
			}
		});
		auto* const openAction = menu->addAction(QStringLiteral("Open on GitHub"));
		openAction->setObjectName(QStringLiteral("commitFilterOpen"));
		openAction->setEnabled(commits.size() == 1U && static_cast<bool>(Open));
		connect(openAction, &QAction::triggered, this, [this, commits] {
			if (Open && commits.size() == 1U) {
				Open(commits.front());
			}
		});
		return menu;
	}

	auto commit_filter_tab::list() const noexcept -> QListView*
	{
		return List;
	}

	auto commit_filter_tab::model() const noexcept -> commit_filter_list_model*
	{
		return Model;
	}

	auto commit_filter_tab::status() const noexcept -> QLabel*
	{
		return Status;
	}

	auto commit_filter_tab::clear_button() const noexcept -> QToolButton*
	{
		return Clear;
	}

	auto commit_filter_tab::projection() const noexcept -> const std::optional<application::commit_filter_projection>&
	{
		return Projection;
	}

	void commit_filter_tab::showEvent(QShowEvent* const aEvent)
	{
		QWidget::showEvent(aEvent);
		if (Shown && !Announced) {
			Announced = true;
			Shown();
		}
	}

	void commit_filter_tab::dispatch(const core::commit_filter_operation_kind aKind, std::vector<std::string> aCommits)
	{
		if (Gestures) {
			Gestures(application::commit_filter_gesture{ .Kind = aKind, .Commits = std::move(aCommits) });
		}
	}

	commit_filter_chip::commit_filter_chip(const bool aOffersCommits, QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("commitFilterChip"));
		setAccessibleName(QStringLiteral("Commit filter"));
		auto* const row = new QHBoxLayout{ this };
		row->setContentsMargins(6, 2, 2, 2);
		row->setSpacing(8);
		Summary = new QLabel{ this };
		Summary->setObjectName(QStringLiteral("commitFilterChipSummary"));
		Summary->setTextFormat(Qt::PlainText);
		auto strong = Summary->font();
		strong.setWeight(QFont::DemiBold);
		Summary->setFont(strong);
		row->addWidget(Summary, 0);
		Detail = new QLabel{ this };
		Detail->setObjectName(QStringLiteral("commitFilterChipDetail"));
		Detail->setTextFormat(Qt::PlainText);
		Detail->setForegroundRole(QPalette::PlaceholderText);
		Detail->setWordWrap(true);
		row->addWidget(Detail, 1);
		if (aOffersCommits) {
			Commits = new QToolButton{ this };
			Commits->setObjectName(QStringLiteral("commitFilterChipCommits"));
			Commits->setText(QStringLiteral("Commits"));
			Commits->setAccessibleName(QStringLiteral("Open the Commits tab"));
			Commits->setToolTip(QStringLiteral("Open the Commits tab, where the filter is set"));
			Commits->setAutoRaise(true);
			Commits->setFocusPolicy(Qt::StrongFocus);
			connect(Commits, &QToolButton::clicked, this, [this] {
				if (CommitsDispatch) {
					CommitsDispatch();
				}
			});
			row->addWidget(Commits, 0);
		}
		Clear = new QToolButton{ this };
		Clear->setObjectName(QStringLiteral("commitFilterChipClear"));
		Clear->setText(QStringLiteral("Clear all filters"));
		Clear->setAccessibleName(QStringLiteral("Clear all filters"));
		Clear->setAutoRaise(true);
		Clear->setFocusPolicy(Qt::StrongFocus);
		connect(Clear, &QToolButton::clicked, this, [this] {
			if (ClearDispatch) {
				ClearDispatch();
			}
		});
		row->addWidget(Clear, 0);
		hide();
	}

	void commit_filter_chip::present(const application::commit_filter_projection& aProjection)
	{
		const auto shown = application::commit_filter_chip_shown(aProjection);
		setVisible(shown);
		if (!shown) {
			return;
		}
		auto detail = QStringList{ };
		if (const auto files = commit_filter_files_text(aProjection); !files.isEmpty()) {
			detail << files;
		}
		if (aProjection.Status == core::commit_filter_status::suspended) {
			detail << commit_filter_suspension_text(aProjection.Suspension);
		} else if (const auto trees = commit_filter_trees_text(aProjection.Trees); !trees.isEmpty() && aProjection.Status == core::commit_filter_status::active) {
			detail << trees;
		}
		if (const auto gone = commit_filter_gone_text(aProjection); !gone.isEmpty()) {
			detail << gone;
		}
		const auto summary = commit_filter_summary_text(aProjection);
		Summary->setText(summary);
		Detail->setText(detail.join(QLatin1Char{ ' ' }));
		Detail->setVisible(!detail.isEmpty());
		setAccessibleDescription(detail.isEmpty() ? summary : summary + QStringLiteral(". ") + detail.join(QLatin1Char{ ' ' }));
	}

	void commit_filter_chip::set_clear_dispatch(std::function<void()> aDispatch)
	{
		ClearDispatch = std::move(aDispatch);
	}

	void commit_filter_chip::set_commits_dispatch(std::function<void()> aDispatch)
	{
		CommitsDispatch = std::move(aDispatch);
	}

	auto commit_filter_chip::summary() const noexcept -> QLabel*
	{
		return Summary;
	}

	auto commit_filter_chip::detail() const noexcept -> QLabel*
	{
		return Detail;
	}

	auto commit_filter_chip::clear_button() const noexcept -> QToolButton*
	{
		return Clear;
	}

	auto commit_filter_chip::commits_button() const noexcept -> QToolButton*
	{
		return Commits;
	}
} // namespace bpr::qt
