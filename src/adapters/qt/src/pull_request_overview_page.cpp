#include "bpr/qt/pull_request_overview_page.hpp"
#include <QAction>

#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include <QStringDecoder>
#include <QTextEdit>
#include <type_traits>

#include <QAbstractItemView>
#include <QAccessible>
#include <QApplication>
#include <QDateTime>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRectF>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSizePolicy>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTextBlock>
#include <QTextOption>
#include <QTimeZone>
#include <QTimer>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bpr::qt {
	// One shared reader keeps both native documents through compact/expanded layout.
	class overview_description_view final : public QWidget
	{
	public:
		explicit overview_description_view(QWidget* aParent)
			: QWidget{ aParent }
		{
			Reader       = new markdown_body_view{ this };
			auto* layout = new QVBoxLayout{ this };
			layout->setContentsMargins(0, 0, 0, 0);
			layout->addWidget(Reader);
			setFocusProxy(Reader);
			watch(Reader->rendered_view());
			watch(Reader->source_view());
		}
		markdown_body_view*       Reader = nullptr;
		std::function<void(bool)> OverflowChanged;
		void                      present_body(const std::string& aText)
		{
			const auto changed = Reader->original_body() != aText;
			Adjusting          = true;
			Reader->set_body(aText, "description");
			if (changed) {
				RenderedPlace = { };
				SourcePlace   = { };
			}
			Adjusting = false;
			queue_layout();
		}
		void reset_reader()
		{
			Expanded      = false;
			RenderedPlace = { };
			SourcePlace   = { };
			Adjusting     = true;
			Reader->clear();
			Adjusting = false;
			queue_layout();
		}
		void set_expanded(const bool aExpanded)
		{
			if (Expanded && !aExpanded) {
				remember();
			}
			Expanded = aExpanded;
			setMinimumHeight(0);
			setMaximumHeight(QWIDGETSIZE_MAX);
			setSizePolicy(QSizePolicy::Expanding, aExpanded ? QSizePolicy::Expanding : QSizePolicy::Fixed);
			queue_layout();
		}

	protected:
		bool event(QEvent* aEvent) override
		{
			if (aEvent->type() == QEvent::Resize || aEvent->type() == QEvent::FontChange
				|| aEvent->type() == QEvent::ApplicationFontChange || aEvent->type() == QEvent::StyleChange) {
				queue_layout();
			}
			return QWidget::event(aEvent);
		}
		bool eventFilter(QObject* aObject, QEvent* aEvent) override
		{
			if (Expanded && (aEvent->type() == QEvent::KeyPress || aEvent->type() == QEvent::MouseButtonPress || aEvent->type() == QEvent::MouseButtonRelease || aEvent->type() == QEvent::Wheel)) {
				ReadingInput = true;
			}
			if (!Expanded && aEvent->type() == QEvent::Wheel) {
				aEvent->ignore();
				return true;
			}
			if ((!Expanded && aEvent->type() == QEvent::Show) || aEvent->type() == QEvent::DevicePixelRatioChange) {
				queue_layout();
			}
			return QWidget::eventFilter(aObject, aEvent);
		}

	private:
		struct reading_place
		{
			int Top        = 0;
			int Anchor     = 0;
			int Position   = 0;
			int Horizontal = 0;
		};
		template <typename TView>
		void watch(TView* aView)
		{
			aView->installEventFilter(this);
			aView->viewport()->installEventFilter(this);
			connect(aView->verticalScrollBar(), &QScrollBar::valueChanged, this, [this] { remember(); });
			connect(aView, &TView::selectionChanged, this, [this] { remember(); });
			connect(aView, &TView::cursorPositionChanged, this, [this] { remember(); });
		}
		template <typename TView>
		void save(TView* aView, reading_place& aPlace)
		{
			aPlace = { aView->cursorForPosition(QPoint{ 0, 0 }).position(), aView->textCursor().anchor(), aView->textCursor().position(), aView->horizontalScrollBar()->value() };
		}
		void remember()
		{
			if (!Expanded || Adjusting || Queued) {
				return;
			}
			if (Reader->source_visible()) {
				save(Reader->source_view(), SourcePlace);
			} else {
				save(Reader->rendered_view(), RenderedPlace);
			}
		}
		template <typename TView>
		void restore(TView* aView, const reading_place& aPlace)
		{
			const auto last   = aView->document()->characterCount() - 1;
			auto       cursor = QTextCursor{ aView->document() };
			cursor.setPosition(std::min(aPlace.Top, last));
			aView->setTextCursor(cursor);
			aView->ensureCursorVisible();
			auto* bar = aView->verticalScrollBar();
			if constexpr (std::is_same_v<TView, QPlainTextEdit>) {
				bar->setValue(cursor.blockNumber());
			} else {
				bar->setValue(bar->value() + aView->cursorRect(cursor).top());
			}
			const auto offset = bar->value();
			cursor.setPosition(std::min(aPlace.Anchor, last));
			cursor.setPosition(std::min(aPlace.Position, last), QTextCursor::KeepAnchor);
			aView->setTextCursor(cursor);
			bar->setValue(offset);
			aView->horizontalScrollBar()->setValue(aPlace.Horizontal);
		}
		void queue_layout()
		{
			if (Queued || Reader == nullptr) {
				return;
			}
			Queued = true;
			QTimer::singleShot(0, this, [this] {
				// A reading gesture after layout was queued takes precedence over
				// the saved pre-layout place, including a new source fallback.
				if (Expanded && ReadingInput) {
					if (Reader->source_visible()) {
						save(Reader->source_view(), SourcePlace);
					} else {
						save(Reader->rendered_view(), RenderedPlace);
					}
				}
				ReadingInput   = false;
				Adjusting      = true;
				auto* rendered = Reader->rendered_view();
				auto* source   = Reader->source_view();
				rendered->setVerticalScrollBarPolicy(Expanded ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
				source->setVerticalScrollBarPolicy(Expanded ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
				if (Expanded) {
					if (Reader->source_visible()) {
						restore(source, SourcePlace);
					} else {
						restore(rendered, RenderedPlace);
					}
				} else {
					const auto budget   = 4 * (Reader->source_visible() ? source->fontMetrics().lineSpacing() : rendered->fontMetrics().lineSpacing());
					const auto natural  = Reader->source_visible() ? source->document()->blockCount() * source->fontMetrics().lineSpacing()
																   : static_cast<int>(std::ceil(rendered->document()->size().height()));
					const auto viewport = Reader->source_visible() ? source->viewport()->height() : rendered->viewport()->height();
					const auto chrome   = std::max(0, Reader->height() - viewport);
					setFixedHeight(std::min(natural, budget) + chrome);
					rendered->verticalScrollBar()->setValue(0);
					source->verticalScrollBar()->setValue(0);
					if (OverflowChanged) {
						OverflowChanged(natural > budget || !Reader->rendered_available());
					}
				}
				Adjusting = false;
				Queued    = false;
			});
		}
		reading_place RenderedPlace, SourcePlace;
		bool          Expanded = false, Adjusting = false, Queued = false, ReadingInput = false;
	};

	namespace {
		[[nodiscard]] auto text(const std::string& aValue) -> QString
		{
			return QString::fromUtf8(aValue.data(), static_cast<qsizetype>(aValue.size()));
		}

		[[nodiscard]] auto operation_letter(const application::overview_file_operation aOperation)
			-> QString
		{
			switch (aOperation) {
			case application::overview_file_operation::added:
				return QStringLiteral("A");
			case application::overview_file_operation::removed:
				return QStringLiteral("D");
			case application::overview_file_operation::renamed:
				return QStringLiteral("R");
			case application::overview_file_operation::copied:
				return QStringLiteral("C");
			case application::overview_file_operation::type_changed:
				return QStringLiteral("T");
			case application::overview_file_operation::unchanged:
				return QStringLiteral("=");
			case application::overview_file_operation::modified:
				break;
			}
			return QStringLiteral("M");
		}

		[[nodiscard]] auto operation_name(const application::overview_file_operation aOperation)
			-> QString
		{
			switch (aOperation) {
			case application::overview_file_operation::added:
				return QStringLiteral("Added");
			case application::overview_file_operation::removed:
				return QStringLiteral("Removed");
			case application::overview_file_operation::renamed:
				return QStringLiteral("Renamed");
			case application::overview_file_operation::copied:
				return QStringLiteral("Copied");
			case application::overview_file_operation::type_changed:
				return QStringLiteral("Type changed");
			case application::overview_file_operation::unchanged:
				return QStringLiteral("Unchanged");
			case application::overview_file_operation::modified:
				break;
			}
			return QStringLiteral("Modified");
		}

		[[nodiscard]] auto changed_text(const application::overview_change_since_review aChanged)
			-> QString
		{
			switch (aChanged) {
			case application::overview_change_since_review::changed:
				return QStringLiteral("changed");
			case application::overview_change_since_review::added:
				return QStringLiteral("new");
			case application::overview_change_since_review::unknown:
				return QStringLiteral("?");
			case application::overview_change_since_review::unchanged:
				break;
			}
			return { };
		}

		[[nodiscard]] auto read_text(const std::optional<core::file_read_state>& aRead) -> QString
		{
			if (!aRead.has_value()) {
				return QStringLiteral("unread");
			}
			switch (*aRead) {
			case core::file_read_state::read:
				return QStringLiteral("read");
			case core::file_read_state::seen:
				return QStringLiteral("seen");
			case core::file_read_state::unread:
				break;
			}
			return QStringLiteral("unread");
		}

		[[nodiscard]] auto role_text(const application::overview_participant_role aRole) -> QString
		{
			switch (aRole) {
			case application::overview_participant_role::author:
				return QStringLiteral("author");
			case application::overview_participant_role::reviewer:
				return QStringLiteral("reviewer");
			case application::overview_participant_role::observer:
				break;
			}
			return QStringLiteral("observer");
		}

		[[nodiscard]] auto signoff_text(const application::overview_signoff aSignoff) -> QString
		{
			switch (aSignoff) {
			case application::overview_signoff::uploaded:
				return QStringLiteral("uploaded");
			case application::overview_signoff::commented:
				return QStringLiteral("commented");
			case application::overview_signoff::approved:
				return QStringLiteral("approved");
			case application::overview_signoff::changes_requested:
				return QStringLiteral("changes requested");
			case application::overview_signoff::dismissed:
				return QStringLiteral("review dismissed");
			case application::overview_signoff::pending:
				return QStringLiteral("review pending");
			case application::overview_signoff::none:
				break;
			}
			return { };
		}

		[[nodiscard]] auto count_text(const std::size_t aCount) -> QString
		{
			return QString::number(static_cast<qulonglong>(aCount));
		}

		[[nodiscard]] auto checks_text(const application::overview_check_summary& aChecks) -> QString
		{
			if (aChecks.total() == 0U) {
				return QStringLiteral("none");
			}
			auto parts = QStringList{ };
			if (aChecks.Passed != 0U) {
				parts.push_back(QStringLiteral("%1 passed").arg(count_text(aChecks.Passed)));
			}
			if (aChecks.Failed != 0U) {
				parts.push_back(QStringLiteral("%1 failed").arg(count_text(aChecks.Failed)));
			}
			if (aChecks.Pending != 0U) {
				parts.push_back(QStringLiteral("%1 pending").arg(count_text(aChecks.Pending)));
			}
			if (aChecks.Skipped != 0U) {
				parts.push_back(QStringLiteral("%1 skipped").arg(count_text(aChecks.Skipped)));
			}
			if (aChecks.Other != 0U) {
				parts.push_back(QStringLiteral("%1 other").arg(count_text(aChecks.Other)));
			}
			return parts.join(QStringLiteral(", "));
		}

		[[nodiscard]] auto heading_font(const QWidget& aWidget) -> QFont
		{
			auto font = aWidget.font();
			font.setPointSizeF(font.pointSizeF() * 0.9);
			font.setWeight(QFont::DemiBold);
			return font;
		}

		[[nodiscard]] auto make_heading(const QString& aText, const QString& aName, QWidget* const aParent)
			-> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			label->setObjectName(aName);
			label->setTextFormat(Qt::PlainText);
			label->setFont(heading_font(*aParent));
			label->setForegroundRole(QPalette::PlaceholderText);
			return label;
		}

		[[nodiscard]] auto value_name(const overview_value aValue) -> QString
		{
			switch (aValue) {
			case overview_value::uploads:
				return QStringLiteral("Uploads");
			case overview_value::files_changed:
				return QStringLiteral("Files changed");
			case overview_value::new_comments:
				return QStringLiteral("New comments");
			case overview_value::replies_to_me:
				return QStringLiteral("Replies to me");
			case overview_value::my_unresolved_threads:
				return QStringLiteral("My unresolved threads");
			case overview_value::checks:
				return QStringLiteral("Checks");
			case overview_value::files_read:
				return QStringLiteral("Files read");
			case overview_value::private_notes:
				return QStringLiteral("Private notes");
			case overview_value::drafted_comments:
				break;
			}
			return QStringLiteral("Drafted comments");
		}

		[[nodiscard]] auto value_object_name(const overview_value aValue) -> QString
		{
			switch (aValue) {
			case overview_value::uploads:
				return QStringLiteral("overviewUploadsValue");
			case overview_value::files_changed:
				return QStringLiteral("overviewFilesChangedValue");
			case overview_value::new_comments:
				return QStringLiteral("overviewNewCommentsValue");
			case overview_value::replies_to_me:
				return QStringLiteral("overviewRepliesToMeValue");
			case overview_value::my_unresolved_threads:
				return QStringLiteral("overviewMyUnresolvedThreadsValue");
			case overview_value::checks:
				return QStringLiteral("overviewChecksValue");
			case overview_value::files_read:
				return QStringLiteral("overviewFilesReadValue");
			case overview_value::private_notes:
				return QStringLiteral("overviewPrivateNotesValue");
			case overview_value::drafted_comments:
				break;
			}
			return QStringLiteral("overviewDraftedCommentsValue");
		}

		constexpr int side_width = 300;
		constexpr int chip_width = 58;

		auto cached_side_text(core::cached_thread_side aSide) -> QString
		{
			switch (aSide) {
			case core::cached_thread_side::left:    return QStringLiteral("left");
			case core::cached_thread_side::right:   return QStringLiteral("right");
			case core::cached_thread_side::unknown: return QStringLiteral("unknown side");
			}
			return QStringLiteral("unknown side");
		}
		auto cached_thread_text(const std::vector<core::cached_review_thread>& aThreads) -> QString
		{
			QString result;
			for (const auto& thread : aThreads) {
				result += QStringLiteral("%1 · %2%3\n")
							  .arg(text(thread.Path), thread.Resolved ? QStringLiteral("was resolved") : QStringLiteral("was unresolved"),
								   thread.Outdated ? QStringLiteral(" · was outdated") : QString{ });
				if (thread.FileLevel) {
					result += QStringLiteral("File discussion\n");
				} else {
					const auto side = cached_side_text(thread.Side);
					result += QStringLiteral("Observed anchor: %1 · line %2 · original line %3\n")
								  .arg(side, thread.Line ? QString::number(*thread.Line) : QStringLiteral("unavailable"),
									   thread.OriginalLine ? QString::number(*thread.OriginalLine) : QStringLiteral("unavailable"));
				}
				if (thread.Comments.empty()) {
					result += QStringLiteral("No comments observed in this thread.\n");
				}
				for (const auto& comment : thread.Comments) {
					result += QStringLiteral("%1%2%3\n%4\n\n")
								  .arg(comment.ReplyTo ? QStringLiteral("Reply · ") : QString{ },
									   comment.AuthorLogin.empty() ? QStringLiteral("Unknown author") : text(comment.AuthorLogin),
									   comment.Outdated ? QStringLiteral(" · was outdated") : QString{ },
									   comment.Deleted ? QStringLiteral("Comment was deleted") : text(comment.Body));
				}
			}
			return result;
		}
		auto displayable_source(const std::optional<std::string>& aSource) -> bool
		{
			if (!aSource) {
				return true;
			}
			QStringDecoder decoder{ QStringDecoder::Utf8 };
			const QString  decoded = decoder(QByteArrayView{ aSource->data(), static_cast<qsizetype>(aSource->size()) });
			static_cast<void>(decoded);
			return !decoder.hasError();
		}
		auto cached_discussion_text(const std::optional<std::vector<core::cached_discussion_message>>& aMessages, const QString& aHeading) -> QString
		{
			if (!aMessages) {
				return aHeading + QStringLiteral(" — unavailable offline\n\n");
			}
			auto result = aHeading + QStringLiteral(" — %1 observed · freshness unknown\n").arg(count_text(aMessages->size()));
			for (const auto& message : *aMessages) {
				QString state;
				switch (message.Kind) {
				case core::cached_discussion_kind::general_comment:          break;
				case core::cached_discussion_kind::pending_review:           state = QStringLiteral("was pending"); break;
				case core::cached_discussion_kind::commented_review:         state = QStringLiteral("was commented"); break;
				case core::cached_discussion_kind::approved_review:          state = QStringLiteral("was approved"); break;
				case core::cached_discussion_kind::changes_requested_review: state = QStringLiteral("changes were requested"); break;
				case core::cached_discussion_kind::dismissed_review:         state = QStringLiteral("was dismissed"); break;
				}
				result += text(message.AuthorLogin.empty() ? "Unknown author" : message.AuthorLogin) + QStringLiteral(" · ") + state;
				if (message.Deleted) {
					result += QStringLiteral(" · was deleted");
				}
				if (message.CommitOid) {
					result += QStringLiteral("\nObserved review commit: ") + text(*message.CommitOid);
				}
				result += QStringLiteral("\n") + text(message.Body) + QStringLiteral("\n\n");
			}
			return result + QStringLiteral("\n");
		}
		auto cached_file_rows(const std::optional<std::vector<core::cached_review_file>>& aCatalogue)
			-> std::vector<application::overview_file_row>
		{
			std::vector<application::overview_file_row> rows;
			if (aCatalogue) {
				rows.reserve(aCatalogue->size());
				for (const auto& file : *aCatalogue) {
					auto       row       = application::overview_file_row{ .Path = file.Path, .PreviousPath = file.PreviousPath };
					const auto separator = file.Path.rfind('/');
					row.Folder           = separator == std::string::npos ? std::string{ } : file.Path.substr(0, separator);
					row.Name             = separator == std::string::npos ? file.Path : file.Path.substr(separator + 1);
					switch (file.Change) {
					case core::cached_file_change::added:        row.Operation = application::overview_file_operation::added; break;
					case core::cached_file_change::modified:     row.Operation = application::overview_file_operation::modified; break;
					case core::cached_file_change::removed:      row.Operation = application::overview_file_operation::removed; break;
					case core::cached_file_change::renamed:      row.Operation = application::overview_file_operation::renamed; break;
					case core::cached_file_change::copied:       row.Operation = application::overview_file_operation::copied; break;
					case core::cached_file_change::type_changed: row.Operation = application::overview_file_operation::type_changed; break;
					case core::cached_file_change::unchanged:    row.Operation = application::overview_file_operation::unchanged; break;
					}
					row.Additions = file.Additions;
					row.Deletions = file.Deletions;
					rows.push_back(std::move(row));
				}
			}
			return rows;
		}

		void expand_new_folders(QTreeView& aView, const QModelIndex& aParent, const int aFirst, const int aLast)
		{
			auto pending = std::vector<QModelIndex>{ };
			for (auto row = aFirst; row <= aLast; ++row) {
				pending.push_back(aView.model()->index(row, 0, aParent));
			}
			while (!pending.empty()) {
				const auto index = pending.back();
				pending.pop_back();
				if (!index.data(overview_file_model::folder_role).toBool()) {
					continue;
				}
				aView.expand(index);
				for (auto row = 0; row < aView.model()->rowCount(index); ++row) {
					pending.push_back(aView.model()->index(row, 0, index));
				}
			}
		}

	} // namespace

	// ---- overview_file_model ------------------------------------------------

	overview_file_model::overview_file_model(QObject* const aParent)
		: QAbstractItemModel{ aParent }
	{
	}

	void overview_file_model::update_file(node& aNode, const application::overview_file_row& aRow)
	{
		if (aNode.File != aRow) {
			aNode.File = aRow;
			emit dataChanged(index_of(aNode), index_of(aNode, column_count - 1));
		}
	}

	void overview_file_model::build_tree(node& aRoot, const std::vector<application::overview_file_row>& aRows)
	{
		auto       folders = QHash<QString, node*>{ };
		const auto folder  = [&folders](node& aParent, const QString& aPath) {
			if (auto* found = folders.value(aPath, nullptr)) {
				return found;
			}
			auto child    = std::make_unique<node>();
			child->Path   = aPath;
			child->Parent = &aParent;
			child->Row    = static_cast<int>(aParent.Children.size());
			auto* result  = child.get();
			aParent.Children.push_back(std::move(child));
			folders.insert(aPath, result);
			return result;
		};
		// [[BPR-1213]]. Flat: one row per folder that holds files, named by
		// its whole path, so a folder with no file of its own takes no row and
		// every file sits one level down, at the same left column.
		for (const auto& row : aRows) {
			auto* const parent = folder(aRoot, text(row.Folder));
			auto        file   = std::make_unique<node>();
			file->Path         = text(row.Path);
			file->File         = row;
			file->Parent       = parent;
			file->Row          = static_cast<int>(parent->Children.size());
			parent->Children.push_back(std::move(file));
			for (auto* ancestor = parent; ancestor != nullptr; ancestor = ancestor->Parent) {
				++ancestor->FileCount;
			}
		}
	}

	void overview_file_model::update_rows(node& aParent, const int aFirst, const int aLast)
	{
		for (auto row = aFirst; row <= aLast; ++row) {
			aParent.Children[static_cast<std::size_t>(row)]->Row = row;
		}
	}

	void overview_file_model::reconcile(node& aParent, node& aDesired)
	{
		auto folders = QHash<QString, node*>{ };
		auto files   = QHash<QString, node*>{ };
		for (const auto& child : aParent.Children) {
			(child->File ? files : folders).insert(child->Path, child.get());
		}
		const auto parentIndex = index_of(aParent);
		for (std::size_t position = 0; position < aDesired.Children.size(); ++position) {
			auto&      desired  = aDesired.Children[position];
			auto*      existing = (desired->File ? files : folders).value(desired->Path, nullptr);
			const auto row      = static_cast<int>(position);
			if (existing == nullptr) {
				beginInsertRows(parentIndex, row, row);
				desired->Parent = &aParent;
				aParent.Children.insert(aParent.Children.begin() + row, std::move(desired));
				update_rows(aParent, row, static_cast<int>(aParent.Children.size()) - 1);
				endInsertRows();
				continue;
			}
			if (existing->Row != row) {
				const auto previous = existing->Row;
				beginMoveRows(parentIndex, previous, previous, parentIndex, row);
				std::rotate(aParent.Children.begin() + row, aParent.Children.begin() + previous,
							aParent.Children.begin() + previous + 1);
				update_rows(aParent, row, previous);
				endMoveRows();
			}
			if (desired->File) {
				update_file(*existing, *desired->File);
			} else {
				reconcile(*existing, *desired);
			}
		}
		const auto retained = static_cast<int>(aDesired.Children.size());
		if (aParent.Children.size() > aDesired.Children.size()) {
			beginRemoveRows(parentIndex, retained, static_cast<int>(aParent.Children.size()) - 1);
			aParent.Children.erase(aParent.Children.begin() + retained, aParent.Children.end());
			endRemoveRows();
		}
		if (aParent.FileCount != aDesired.FileCount) {
			aParent.FileCount = aDesired.FileCount;
			if (parentIndex.isValid()) {
				emit dataChanged(parentIndex, index_of(aParent, column_count - 1));
			}
		}
	}

	void overview_file_model::rebuild_lookup(const std::vector<application::overview_file_row>& aRows)
	{
		FileNodes.clear();
		FileOrder.clear();
		FolderCount  = 0U;
		auto pending = std::vector<node*>{ &Root };
		while (!pending.empty()) {
			auto* parent = pending.back();
			pending.pop_back();
			for (const auto& child : parent->Children) {
				if (child->File) {
					FileNodes.insert(child->Path, child.get());
				} else {
					++FolderCount;
					pending.push_back(child.get());
				}
			}
		}
		FileOrder.reserve(aRows.size());
		for (const auto& row : aRows) {
			FileOrder.push_back(FileNodes.value(text(row.Path)));
		}
	}

	void overview_file_model::apply(const std::vector<application::overview_file_row>& aRows)
	{
		// Metadata-only refreshes retain the tree and allocate no replacement nodes.
		const auto sameOrder = std::ranges::equal(FileOrder, aRows,
												  [](const node* aNode, const application::overview_file_row& aRow) {
													  return aNode->File->Path == aRow.Path && aNode->File->Folder == aRow.Folder;
												  });
		if (sameOrder) {
			for (std::size_t row = 0; row < aRows.size(); ++row) {
				update_file(*FileOrder[row], aRows[row]);
			}
			return;
		}
		auto desired = node{ };
		build_tree(desired, aRows);
		reconcile(Root, desired);
		rebuild_lookup(aRows);
	}

	void overview_file_model::update_rows(const std::span<const application::overview_file_row> aRows)
	{
		for (const auto& row : aRows) {
			if (auto* const file = FileNodes.value(text(row.Path), nullptr)) {
				update_file(*file, row);
			}
		}
	}

	auto overview_file_model::node_at(const QModelIndex& aIndex) const -> const node*
	{
		return aIndex.isValid() && aIndex.model() == this
			? static_cast<const node*>(aIndex.internalPointer())
			: nullptr;
	}

	auto overview_file_model::index_of(const node& aNode, const int aColumn) const -> QModelIndex
	{
		return &aNode == &Root ? QModelIndex{ }
							   : createIndex(aNode.Row, aColumn, const_cast<node*>(&aNode));
	}

	auto overview_file_model::index(const int aRow, const int aColumn, const QModelIndex& aParent) const
		-> QModelIndex
	{
		if (aRow < 0 || aColumn < 0 || aColumn >= column_count
			|| (aParent.isValid() && aParent.column() != 0)) {
			return { };
		}
		const auto* parent = aParent.isValid() ? node_at(aParent) : &Root;
		return parent != nullptr && static_cast<std::size_t>(aRow) < parent->Children.size()
			? index_of(*parent->Children[static_cast<std::size_t>(aRow)], aColumn)
			: QModelIndex{ };
	}

	auto overview_file_model::parent(const QModelIndex& aIndex) const -> QModelIndex
	{
		const auto* item = node_at(aIndex);
		return item != nullptr && item->Parent != nullptr ? index_of(*item->Parent) : QModelIndex{ };
	}

	auto overview_file_model::rowCount(const QModelIndex& aParent) const -> int
	{
		if (aParent.isValid() && aParent.column() != 0) {
			return 0;
		}
		const auto* parent = aParent.isValid() ? node_at(aParent) : &Root;
		return parent == nullptr ? 0 : static_cast<int>(parent->Children.size());
	}

	auto overview_file_model::columnCount(const QModelIndex& aParent) const -> int
	{
		static_cast<void>(aParent);
		return column_count;
	}

	auto overview_file_model::row_at(const QModelIndex& aIndex) const
		-> const application::overview_file_row*
	{
		const auto* item = node_at(aIndex);
		return item != nullptr && item->File ? &*item->File : nullptr;
	}

	auto overview_file_model::data(const QModelIndex& aIndex, const int aRole) const -> QVariant
	{
		const auto* item = node_at(aIndex);
		if (item == nullptr) {
			return { };
		}
		if (!item->File) {
			if (aRole == folder_role) {
				return true;
			}
			if (aIndex.column() != file_column && aIndex.column() != operation_column) {
				return { };
			}
			if (aRole == Qt::DisplayRole && aIndex.column() == file_column) {
				return item->Path.isEmpty() ? QStringLiteral("/") : item->Path;
			}
			if (aRole == Qt::DisplayRole && aIndex.column() == operation_column) {
				return QString::number(item->FileCount);
			}
			if (aRole == Qt::ToolTipRole || aRole == Qt::AccessibleTextRole) {
				return QStringLiteral("Folder %1, %2 files").arg(item->Path.isEmpty() ? QStringLiteral("/") : item->Path).arg(item->FileCount);
			}
			return { };
		}
		const auto* const row = row_at(aIndex);
		if (row == nullptr) {
			return { };
		}
		switch (aRole) {
		case folder_role:
			return false;
		case path_role:
			return text(row->Path);
		case operation_role:
			return static_cast<int>(row->Operation);
		case changed_role:
			return static_cast<int>(row->Changed);
		case read_role:
			return row->Read.has_value() ? static_cast<int>(*row->Read) : -1;
		case threads_role:
			return static_cast<qulonglong>(row->ThreadCount);
		case unresolved_role:
			return static_cast<qulonglong>(row->UnresolvedThreadCount);
		case Qt::TextAlignmentRole:
			if (aIndex.column() == added_column || aIndex.column() == removed_column) {
				return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
			}
			return { };
		case Qt::ToolTipRole:
			switch (aIndex.column()) {
			case operation_column:
				return operation_name(row->Operation);
			case file_column:
				return row->PreviousPath.has_value()
					? QStringLiteral("%1 (was %2)").arg(text(row->Path), text(*row->PreviousPath))
					: text(row->Path);
			case changed_column:
				return row->Changed == application::overview_change_since_review::unknown
					? QStringLiteral("Not yet resolved: compare since my last review to find out")
					: changed_text(row->Changed);
			case read_column:
				return read_text(row->Read);
			case comments_column:
				return QStringLiteral("%1 threads, %2 unresolved")
					.arg(count_text(row->ThreadCount), count_text(row->UnresolvedThreadCount));
			case note_column:
				return row->Note.has_value() ? text(*row->Note) : QString{ };
			default:
				return { };
			}
		case Qt::DisplayRole:
		case Qt::AccessibleTextRole:
			switch (aIndex.column()) {
			case operation_column:
				return operation_letter(row->Operation);
			case file_column:
				return text(row->Name);
			case added_column:
				return row->Additions == 0U ? QString{ } : QStringLiteral("+%1").arg(row->Additions);
			case removed_column:
				return row->Deletions == 0U ? QString{ } : QStringLiteral("−%1").arg(row->Deletions);
			case changed_column:
				return changed_text(row->Changed);
			case read_column:
				return aRole == Qt::AccessibleTextRole ? read_text(row->Read) : QString{ };
			case comments_column:
				if (row->ThreadCount == 0U) {
					return QString{ };
				}
				return row->UnresolvedThreadCount == 0U
					? count_text(row->ThreadCount)
					: QStringLiteral("%1 / %2").arg(count_text(row->UnresolvedThreadCount), count_text(row->ThreadCount));
			case note_column:
				return row->Note.has_value() ? text(*row->Note) : QString{ };
			default:
				return { };
			}
		default:
			return { };
		}
	}

	auto overview_file_model::headerData(const int aSection, const Qt::Orientation aOrientation, const int aRole) const
		-> QVariant
	{
		if (aOrientation != Qt::Horizontal || aRole != Qt::DisplayRole) {
			return { };
		}
		switch (aSection) {
		case operation_column:
			return QString{ };
		case file_column:
			return QStringLiteral("File");
		case added_column:
			return QStringLiteral("Added");
		case removed_column:
			return QStringLiteral("Removed");
		case changed_column:
			return QStringLiteral("Changed");
		case read_column:
			return QStringLiteral("Read");
		case comments_column:
			return QStringLiteral("Comments");
		case note_column:
			return QStringLiteral("Note");
		default:
			return { };
		}
	}

	auto overview_file_model::folder_count() const noexcept -> std::size_t
	{
		return FolderCount;
	}

	auto overview_file_model::file_count() const noexcept -> std::size_t
	{
		return FileOrder.size();
	}

	auto overview_file_model::index_for_path(const QString& aPath) const -> QModelIndex
	{
		const auto* file = FileNodes.value(aPath, nullptr);
		return file == nullptr ? QModelIndex{ } : index_of(*file, file_column);
	}

	// ---- overview_row_delegate ----------------------------------------------

	overview_row_delegate::overview_row_delegate(const overview_colours& aColours, QObject* const aParent)
		: QStyledItemDelegate{ aParent }
		, Colours{ aColours }
	{
	}

	auto overview_row_delegate::sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
		-> QSize
	{
		const auto folderPath = aIndex.column() == overview_file_model::file_column
			&& aIndex.data(overview_file_model::folder_role).toBool();
		auto option = aOption;
		if (folderPath) {
			// Measured in the weight it is painted in, so the path is never elided.
			option.font.setWeight(QFont::DemiBold);
		}
		auto hint = QStyledItemDelegate::sizeHint(option, aIndex);
		hint.setHeight(std::max(hint.height(), 22));
		if (folderPath) {
			hint.setWidth(hint.width() + disclosure_width(aOption));
		}
		return hint;
	}

	auto overview_row_delegate::disclosure_width(const QStyleOptionViewItem& aOption) -> int
	{
		const auto* const style = aOption.widget != nullptr ? aOption.widget->style() : QApplication::style();
		return style->pixelMetric(QStyle::PM_TreeViewIndentation, &aOption, aOption.widget);
	}

	auto overview_row_delegate::disclosure_rect(const QStyleOptionViewItem& aOption) -> QRect
	{
		return QRect{ aOption.rect.left(), aOption.rect.top(), disclosure_width(aOption), aOption.rect.height() };
	}

	void overview_row_delegate::paint_folder(QPainter* const aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
	{
		// [[BPR-1213]]. The rows stay flat, so the view has no indentation to
		// draw its branch in; the folder's own cell carries the native
		// disclosure mark before its path.
		auto option = aOption;
		initStyleOption(&option, aIndex);
		const auto* const style = option.widget != nullptr ? option.widget->style() : QApplication::style();
		const auto        text  = option.text;
		option.text.clear();
		style->drawControl(QStyle::CE_ItemViewItem, &option, aPainter, option.widget);

		const auto* const tree     = qobject_cast<const QTreeView*>(option.widget);
		const auto        expanded = tree != nullptr && tree->isExpanded(aIndex.siblingAtColumn(0));
		auto              branch   = QStyleOption{ };
		branch.QStyleOption::operator=(option);
		branch.rect  = disclosure_rect(option);
		branch.state = (option.state & (QStyle::State_Enabled | QStyle::State_Active | QStyle::State_Selected | QStyle::State_MouseOver))
			| QStyle::State_Children | QStyle::State_Item | (expanded ? QStyle::State_Open : QStyle::State_None);
		style->drawPrimitive(QStyle::PE_IndicatorBranch, &branch, aPainter, option.widget);

		auto label = option;
		label.text = text;
		label.rect.setLeft(branch.rect.right() + 1);
		label.backgroundBrush = Qt::NoBrush;
		label.state &= ~(QStyle::State_HasFocus | QStyle::State_Selected);
		label.font.setWeight(QFont::DemiBold);
		const auto group = option.state & QStyle::State_Active ? QPalette::Active : QPalette::Inactive;
		label.palette.setColor(QPalette::Text, option.state & QStyle::State_Selected ? option.palette.color(group, QPalette::HighlightedText) : option.palette.color(group, QPalette::PlaceholderText));
		style->drawControl(QStyle::CE_ItemViewItem, &label, aPainter, label.widget);
	}

	void overview_row_delegate::paint(
		QPainter* const             aPainter,
		const QStyleOptionViewItem& aOption,
		const QModelIndex&          aIndex) const
	{
		const auto folder = aIndex.data(overview_file_model::folder_role).toBool();
		const auto column = aIndex.column();
		const auto marked = !folder
			&& (column == overview_file_model::operation_column
				|| column == overview_file_model::changed_column
				|| column == overview_file_model::read_column
				|| column == overview_file_model::comments_column);
		if (folder && column == overview_file_model::file_column) {
			paint_folder(aPainter, aOption, aIndex);
			return;
		}
		if (!marked) {
			auto option = aOption;
			initStyleOption(&option, aIndex);
			if (folder) {
				option.font.setWeight(QFont::DemiBold);
				option.palette.setColor(QPalette::Text, option.palette.color(QPalette::PlaceholderText));
			}
			if (!folder && column == overview_file_model::added_column) {
				option.palette.setColor(QPalette::Text, Colours.Accepted);
			}
			if (!folder && column == overview_file_model::removed_column) {
				option.palette.setColor(QPalette::Text, Colours.Defect);
			}
			if (!folder && column == overview_file_model::note_column) {
				option.palette.setColor(QPalette::Text, option.palette.color(QPalette::PlaceholderText));
			}
			const auto* const style = option.widget != nullptr ? option.widget->style() : QApplication::style();
			style->drawControl(QStyle::CE_ItemViewItem, &option, aPainter, option.widget);
			return;
		}
		// The background and selection as the style draws them, then the mark.
		auto option = aOption;
		initStyleOption(&option, aIndex);
		option.text.clear();
		const auto* const style = option.widget != nullptr ? option.widget->style() : QApplication::style();
		style->drawControl(QStyle::CE_ItemViewItem, &option, aPainter, option.widget);

		aPainter->save();
		aPainter->setRenderHint(QPainter::Antialiasing, true);
		const auto rect = option.rect;
		const auto ink  = option.palette.color(QPalette::Active, QPalette::Text);
		auto       font = option.font;
		font.setPointSizeF(font.pointSizeF() * 0.85);
		font.setWeight(QFont::DemiBold);
		aPainter->setFont(font);
		const auto metrics = QFontMetrics{ font };
		const auto chip    = [&](const QString& aText, const QColor& aFill, const QColor& aText2, const int aWidth) {
			const auto box = QRectF{ rect.left() + 6.0, rect.center().y() - 8.0, static_cast<qreal>(aWidth), 16.0 };
			aPainter->setPen(Qt::NoPen);
			aPainter->setBrush(aFill);
			aPainter->drawRoundedRect(box, 8.0, 8.0);
			aPainter->setPen(aText2);
			aPainter->drawText(box, Qt::AlignCenter, aText);
		};
		switch (column) {
		case overview_file_model::operation_column: {
			const auto operation = static_cast<application::overview_file_operation>(
				aIndex.data(overview_file_model::operation_role).toInt());
			auto fill = option.palette.color(QPalette::Active, QPalette::Mid);
			switch (operation) {
			case application::overview_file_operation::added:
				fill = Colours.Accepted;
				break;
			case application::overview_file_operation::removed:
				fill = Colours.Defect;
				break;
			case application::overview_file_operation::renamed:
			case application::overview_file_operation::copied:
			case application::overview_file_operation::type_changed:
				fill = Colours.Marker;
				break;
			case application::overview_file_operation::modified:
			case application::overview_file_operation::unchanged:
				break;
			}
			const auto box = QRectF{ rect.left() + 6.0, rect.center().y() - 7.0, 14.0, 14.0 };
			aPainter->setPen(Qt::NoPen);
			aPainter->setBrush(fill);
			aPainter->drawRoundedRect(box, 3.0, 3.0);
			aPainter->setPen(option.palette.color(QPalette::Active, QPalette::Base));
			aPainter->drawText(box, Qt::AlignCenter, operation_letter(operation));
			break;
		}
		case overview_file_model::changed_column: {
			const auto changed = static_cast<application::overview_change_since_review>(
				aIndex.data(overview_file_model::changed_role).toInt());
			if (changed == application::overview_change_since_review::changed) {
				chip(QStringLiteral("changed"), Colours.Unread, option.palette.color(QPalette::Active, QPalette::Base), chip_width);
			} else if (changed == application::overview_change_since_review::added) {
				chip(QStringLiteral("new"), Colours.Accepted, option.palette.color(QPalette::Active, QPalette::Base), chip_width);
			} else if (changed == application::overview_change_since_review::unknown) {
				aPainter->setPen(option.palette.color(QPalette::Active, QPalette::PlaceholderText));
				aPainter->drawText(rect.adjusted(6, 0, 0, 0), Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("?"));
			}
			break;
		}
		case overview_file_model::read_column: {
			// A hollow dot unread, a half dot seen, a filled dot read: three
			// shapes, so the state survives without its colour.
			const auto read   = aIndex.data(overview_file_model::read_role).toInt();
			const auto centre = QPointF{ rect.left() + 13.0, static_cast<qreal>(rect.center().y()) };
			aPainter->setPen(QPen{ ink, 1.2 });
			aPainter->setBrush(Qt::NoBrush);
			aPainter->drawEllipse(centre, 4.5, 4.5);
			if (read == static_cast<int>(core::file_read_state::read)) {
				aPainter->setBrush(Colours.Accepted);
				aPainter->setPen(Qt::NoPen);
				aPainter->drawEllipse(centre, 4.5, 4.5);
			} else if (read == static_cast<int>(core::file_read_state::seen)) {
				aPainter->setBrush(ink);
				aPainter->setPen(Qt::NoPen);
				aPainter->drawPie(QRectF{ centre.x() - 4.5, centre.y() - 4.5, 9.0, 9.0 }, 90 * 16, 180 * 16);
			}
			break;
		}
		case overview_file_model::comments_column: {
			const auto threads    = aIndex.data(overview_file_model::threads_role).toULongLong();
			const auto unresolved = aIndex.data(overview_file_model::unresolved_role).toULongLong();
			auto       left       = rect.left() + 6.0;
			const auto bubble     = [&](const QString& aText, const QColor& aFill, const QColor& aText2) {
				const auto width = std::max(18, metrics.horizontalAdvance(aText) + 8);
				const auto box   = QRectF{ left, rect.center().y() - 7.0, static_cast<qreal>(width), 14.0 };
				aPainter->setPen(Qt::NoPen);
				aPainter->setBrush(aFill);
				aPainter->drawRoundedRect(box, 7.0, 7.0);
				aPainter->setPen(aText2);
				aPainter->drawText(box, Qt::AlignCenter, aText);
				left += width + 4.0;
			};
			if (unresolved != 0U) {
				bubble(QString::number(unresolved), Colours.Marker, option.palette.color(QPalette::Active, QPalette::Base));
			}
			if (threads != 0U) {
				bubble(QString::number(threads), option.palette.color(QPalette::Active, QPalette::Mid), ink);
			}
			break;
		}
		default:
			break;
		}
		aPainter->restore();
	}

	// ---- overview_participant_row -------------------------------------------

	overview_participant_row::overview_participant_row(
		const application::overview_participant& aParticipant,
		const overview_colours&                  aColours,
		QWidget* const                           aParent)
		: QWidget{ aParent }
		, Participant{ aParticipant }
		, Colours{ aColours }
	{
		setObjectName(QStringLiteral("overviewParticipant"));
		setAccessibleName(QStringLiteral("%1, %2, %3")
							  .arg(login(), role_text(aParticipant.Role), standing_text()));
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		setToolTip(accessibleName());
	}

	auto overview_participant_row::login() const -> QString
	{
		return text(Participant.Login);
	}

	auto overview_participant_row::standing_text() const -> QString
	{
		return signoff_text(Participant.Signoff);
	}

	auto overview_participant_row::sizeHint() const -> QSize
	{
		return { 200, heightForWidth(200) };
	}

	auto overview_participant_row::minimumSizeHint() const -> QSize
	{
		return { 0, heightForWidth(width()) };
	}

	auto overview_participant_row::heightForWidth(const int aWidth) const -> int
	{
		static_cast<void>(aWidth);
		return std::max(28, fontMetrics().height() + 10);
	}

	void overview_participant_row::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto& colours = palette();
		const auto  ink     = colours.color(QPalette::Active, QPalette::WindowText);
		const auto  faint   = colours.color(QPalette::Active, QPalette::PlaceholderText);
		// The avatar: an initial in a circle, accented for me.
		const auto centre = QPointF{ 11.0, height() / 2.0 };
		painter.setPen(Qt::NoPen);
		painter.setBrush(Participant.Viewer ? Colours.Unread : colours.color(QPalette::Active, QPalette::Mid));
		painter.drawEllipse(centre, 9.0, 9.0);
		auto small = font();
		small.setPointSizeF(small.pointSizeF() * 0.8);
		small.setWeight(QFont::DemiBold);
		painter.setFont(small);
		painter.setPen(Participant.Viewer ? colours.color(QPalette::Active, QPalette::Base) : ink);
		const auto initial = login().isEmpty() ? QString{ } : login().left(1).toUpper();
		painter.drawText(QRectF{ centre.x() - 9.0, centre.y() - 9.0, 18.0, 18.0 }, Qt::AlignCenter, initial);
		// One compact line per person; full login, role and standing remain
		// available in the tooltip and accessible name when space is tight.
		painter.setFont(font());
		// The standing is coloured by what it means.
		auto standing = faint;
		switch (Participant.Signoff) {
		case application::overview_signoff::approved:
			standing = Colours.Accepted;
			break;
		case application::overview_signoff::changes_requested:
			standing = Colours.Marker;
			break;
		case application::overview_signoff::dismissed:
		case application::overview_signoff::pending:
		case application::overview_signoff::uploaded:
		case application::overview_signoff::commented:
		case application::overview_signoff::none:
			break;
		}
		painter.setPen(standing);
		const auto details     = standing_text().isEmpty() ? role_text(Participant.Role) : standing_text();
		const auto detailWidth = std::min(fontMetrics().horizontalAdvance(details), std::max(0, width() - 60));
		painter.drawText(QRect{ width() - detailWidth - 4, 0, detailWidth, height() },
						 Qt::AlignVCenter | Qt::AlignRight, details);
		painter.setPen(ink);
		const auto loginWidth = std::max(0, width() - detailWidth - 44);
		painter.drawText(QRect{ 28, 0, loginWidth, height() }, Qt::AlignVCenter | Qt::AlignLeft,
						 fontMetrics().elidedText(login(), Qt::ElideRight, loginWidth));
		// The rule beneath.
		painter.setPen(QPen{ colours.color(QPalette::Active, QPalette::Mid), 1.0 });
		painter.drawLine(QPointF{ 0.0, height() - 0.5 }, QPointF{ static_cast<qreal>(width()), height() - 0.5 });
	}

	// ---- pull_request_overview_page -----------------------------------------

	pull_request_overview_page::pull_request_overview_page(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("workbenchOverviewPage"));
		setAccessibleName(QStringLiteral("Overview"));
		setAccessibleDescription(QStringLiteral(
			"What changed in the pull request and who has signed off, with my "
			"review progress and the way in"));

		auto* const main = new QWidget{ this };
		main->setObjectName(QStringLiteral("overviewMainColumn"));
		auto* const mainColumn = new QVBoxLayout{ main };
		mainColumn->setContentsMargins(18, 12, 18, 12);
		mainColumn->setSpacing(4);
		Header             = new QWidget{ main };
		auto* headerLayout = new QVBoxLayout{ Header };
		headerLayout->setContentsMargins(0, 0, 0, 0);
		headerLayout->setSpacing(4);
		mainColumn->addWidget(Header);

		Title = new QLabel{ main };
		Title->setObjectName(QStringLiteral("overviewTitle"));
		Title->setAccessibleName(QStringLiteral("Pull request title"));
		Title->setTextFormat(Qt::PlainText);
		Title->setWordWrap(true);
		auto titleFont = Title->font();
		titleFont.setPointSizeF(titleFont.pointSizeF() * 1.3);
		titleFont.setWeight(QFont::DemiBold);
		Title->setFont(titleFont);
		headerLayout->addWidget(Title);

		Summary = new QLabel{ main };
		Summary->setObjectName(QStringLiteral("overviewSummary"));
		Summary->setAccessibleName(QStringLiteral("Pull request author and branches"));
		Summary->setTextFormat(Qt::PlainText);
		Summary->setWordWrap(true);
		Summary->setForegroundRole(QPalette::PlaceholderText);
		headerLayout->addWidget(Summary);
		// [[BPR-0671]]. The commit oids behind the branches, disclosed. They
		// were in the files drawer's summary box, which the overview replaces;
		// they belong beside the branches they identify.
		TechnicalRow = new QHBoxLayout;
		TechnicalRow->setContentsMargins(0, 0, 0, 0);
		TechnicalRow->setSpacing(6);
		headerLayout->addLayout(TechnicalRow);

		ReturnToFiles = new QPushButton{ QStringLiteral("Return to Files"), main };
		ReturnToFiles->setObjectName(QStringLiteral("overviewReturnToFiles"));
		ReturnToFiles->setAccessibleName(ReturnToFiles->text());
		mainColumn->addWidget(ReturnToFiles, 0, Qt::AlignLeft);
		connect(ReturnToFiles, &QPushButton::clicked, this, [this] { set_description_expanded(false); });

		Body = new overview_description_view{ main };
		Body->setObjectName(QStringLiteral("overviewBody"));
		Body->setAccessibleName(QStringLiteral("Pull request description"));
		Body->Reader->setAccessibleName(QStringLiteral("Pull request description"));
		mainColumn->addSpacing(4);
		mainColumn->addWidget(Body);
		EmptyBody = new QLabel{ QStringLiteral("No description"), main };
		EmptyBody->setObjectName(QStringLiteral("overviewNoDescription"));
		mainColumn->addWidget(EmptyBody);
		ReadDescription = new QPushButton{ QStringLiteral("Read full description"), main };
		ReadDescription->setObjectName(QStringLiteral("overviewReadDescription"));
		ReadDescription->setAccessibleName(ReadDescription->text());
		mainColumn->addWidget(ReadDescription, 0, Qt::AlignLeft);
		connect(ReadDescription, &QPushButton::clicked, this, [this] { set_description_expanded(true); });
		Body->OverflowChanged = [this](const bool aOverflow) {
			ReadDescription->setVisible(aOverflow && Projection.has_value() && !Projection->Body.empty());
		};
		auto* escape = new QShortcut{ QKeySequence{ Qt::Key_Escape }, this };
		escape->setContext(Qt::WidgetWithChildrenShortcut);
		escape->setEnabled(false);
		connect(escape, &QShortcut::activated, this, [this] { set_description_expanded(false); });
		escape->setObjectName(QStringLiteral("overviewDescriptionEscape"));

		FilesHeading = make_heading(QStringLiteral("Files"), QStringLiteral("overviewFilesHeading"), main);
		FilesHeading->setAccessibleName(QStringLiteral("Files in the pull request"));
		mainColumn->addSpacing(8);
		mainColumn->addWidget(FilesHeading);

		Model = new overview_file_model{ this };
		Files = new QTreeView{ main };
		Files->setObjectName(QStringLiteral("overviewFiles"));
		Files->setAccessibleName(QStringLiteral("Files table"));
		Files->setAccessibleDescription(QStringLiteral(
			"Every file with its operation, line counts, whether it changed "
			"since my last review, my read state, comment counts and my note; "
			"activate a file to read it; click a folder, or use Left and Right, to hide and show its files"));
		Files->setModel(Model);
		Files->setItemDelegate(new overview_row_delegate{ Colours, Files });
		// [[BPR-1213]]. Flat rows: no indentation, so every file aligns at the
		// same left column; the folder cell draws its own disclosure.
		Files->setRootIsDecorated(false);
		Files->setIndentation(0);
		Files->setItemsExpandable(true);
		Files->setExpandsOnDoubleClick(false);
		Files->setTreePosition(overview_file_model::file_column);
		Files->setUniformRowHeights(true);
		Files->setAlternatingRowColors(false);
		Files->setSelectionBehavior(QAbstractItemView::SelectRows);
		Files->setSelectionMode(QAbstractItemView::SingleSelection);
		Files->setFrameShape(QFrame::NoFrame);
		Files->setEditTriggers(QAbstractItemView::NoEditTriggers);
		Files->header()->setStretchLastSection(true);
		// Size from the visible rows. Measuring every column of the complete
		// review here blocks the first Overview show on the GUI lane.
		Files->header()->setResizeContentsPrecision(0);
		Files->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
		Files->header()->setSectionResizeMode(overview_file_model::note_column, QHeaderView::Stretch);
		connect(Model, &QAbstractItemModel::rowsInserted, this,
				[this](const QModelIndex& aModelParent, const int aFirst, const int aLast) {
					expand_new_folders(*Files, aModelParent, aFirst, aLast);
				});
		connect(Files, &QTreeView::collapsed, this, [this](const QModelIndex& aFolder) {
			for (auto parent = Files->currentIndex().parent(); parent.isValid(); parent = parent.parent()) {
				if (parent == aFolder) {
					Files->setCurrentIndex(aFolder);
					break;
				}
			}
		});
		// A click on a folder row shows or hides its files; files open on
		// activation as before. Neither dispatches anything for a folder.
		connect(Files, &QAbstractItemView::clicked, this, [this](const QModelIndex& aIndex) {
			if (aIndex.data(overview_file_model::folder_role).toBool()) {
				const auto folder = aIndex.siblingAtColumn(0);
				Files->setExpanded(folder, !Files->isExpanded(folder));
			}
		});
		connect(Files, &QAbstractItemView::activated, this, [this](const QModelIndex& aIndex) {
			const auto path = aIndex.siblingAtColumn(overview_file_model::file_column)
								  .data(overview_file_model::path_role)
								  .toString();
			if (Projection && !path.isEmpty() && FileDispatch) {
				FileDispatch(path);
			} else if (Cached && !path.isEmpty() && CachedSourceDispatch) {
				CachedSourceDispatch(path);
			}
		});
		mainColumn->addWidget(Files, 1);
		CachedThreadsHeading = make_heading(QString{ }, QStringLiteral("overviewCachedThreadsHeading"), main);
		mainColumn->addWidget(CachedThreadsHeading);
		CachedThreads = new QPlainTextEdit{ main };
		CachedThreads->setObjectName(QStringLiteral("overviewCachedThreads"));
		CachedThreads->setAccessibleName(QStringLiteral("Historical review threads; freshness unknown"));
		CachedThreads->setAccessibleDescription(QStringLiteral("Read-only historical text. Anchors and links cannot be opened."));
		CachedThreads->setReadOnly(true);
		CachedThreads->setLineWrapMode(QPlainTextEdit::WidgetWidth);
		mainColumn->addWidget(CachedThreads, 1);
		CachedDiscussion = new QPlainTextEdit{ main };
		CachedDiscussion->setObjectName(QStringLiteral("overviewCachedDiscussion"));
		CachedDiscussion->setAccessibleName(QStringLiteral("Historical general comments and review summaries; freshness unknown"));
		CachedDiscussion->setReadOnly(true);
		CachedDiscussion->setLineWrapMode(QPlainTextEdit::WidgetWidth);
		mainColumn->addWidget(CachedDiscussion, 1);
		CachedSourceBack = new QPushButton{ QStringLiteral("Back to cached overview"), main };
		CachedSourceBack->setObjectName(QStringLiteral("overviewCachedSourceBack"));
		connect(CachedSourceBack, &QPushButton::clicked, this, [this] { show_cached_source(false); Files->setFocus(Qt::OtherFocusReason); });
		mainColumn->addWidget(CachedSourceBack);
		CachedSource = new QPlainTextEdit{ main };
		CachedSource->setObjectName(QStringLiteral("overviewCachedSource"));
		CachedSource->setAccessibleName(QStringLiteral("Historical exact base and head source; freshness unknown"));
		CachedSource->setReadOnly(true);
		CachedSource->setLineWrapMode(QPlainTextEdit::NoWrap);
		mainColumn->addWidget(CachedSource, 2);

		auto* const sideScroll = new QScrollArea{ this };
		sideScroll->setObjectName(QStringLiteral("overviewSideScroll"));
		sideScroll->setAccessibleName(QStringLiteral("Participants and review summary"));
		sideScroll->setWidgetResizable(true);
		sideScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		sideScroll->setFrameShape(QFrame::NoFrame);
		sideScroll->setFixedWidth(side_width);
		auto* const side = new QWidget{ sideScroll };
		Side             = sideScroll;
		side->setObjectName(QStringLiteral("overviewSideColumn"));
		side->setAutoFillBackground(true);
		side->setBackgroundRole(QPalette::AlternateBase);
		auto* const sideColumn = new QVBoxLayout{ side };
		sideColumn->setContentsMargins(14, 12, 14, 12);
		sideColumn->setSpacing(2);
		sideColumn->setSizeConstraint(QLayout::SetMinimumSize);

		auto* const participantsHeading
			= make_heading(QStringLiteral("Participants"), QStringLiteral("overviewParticipantsHeading"), side);
		sideColumn->addWidget(participantsHeading);
		Participants = new QVBoxLayout;
		Participants->setContentsMargins(0, 0, 0, 0);
		Participants->setSpacing(0);
		sideColumn->addLayout(Participants);

		SinceHeading = make_heading(QStringLiteral("Since my last review"), QStringLiteral("overviewSinceHeading"), side);
		sideColumn->addSpacing(12);
		sideColumn->addWidget(SinceHeading);
		const auto addValue = [this, side, sideColumn](const overview_value aValue) {
			auto* const row       = new QWidget{ side };
			auto* const rowLayout = new QHBoxLayout{ row };
			rowLayout->setContentsMargins(0, 3, 0, 3);
			auto* const name = new QLabel{ value_name(aValue), row };
			name->setTextFormat(Qt::PlainText);
			auto* const value = new QLabel{ row };
			value->setObjectName(value_object_name(aValue));
			value->setAccessibleName(value_name(aValue));
			value->setTextFormat(Qt::PlainText);
			value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
			rowLayout->addWidget(name, 1);
			rowLayout->addWidget(value);
			sideColumn->addWidget(row);
			Values[static_cast<std::size_t>(aValue)] = value;
		};
		addValue(overview_value::uploads);
		addValue(overview_value::files_changed);
		addValue(overview_value::new_comments);
		addValue(overview_value::replies_to_me);
		addValue(overview_value::my_unresolved_threads);
		addValue(overview_value::checks);

		auto* const myReviewHeading
			= make_heading(QStringLiteral("My review"), QStringLiteral("overviewMyReviewHeading"), side);
		sideColumn->addSpacing(12);
		sideColumn->addWidget(myReviewHeading);
		addValue(overview_value::files_read);
		addValue(overview_value::private_notes);
		addValue(overview_value::drafted_comments);

		Entry = new QPushButton{ QStringLiteral("Start review"), side };
		Entry->setObjectName(QStringLiteral("overviewEntryButton"));
		Entry->setAccessibleName(QStringLiteral("Start review"));
		Entry->setAccessibleDescription(QStringLiteral(
			"Enters Read at the first unseen change"));
		Entry->setDefault(true);
		Entry->setFocusPolicy(Qt::StrongFocus);
		connect(Entry, &QPushButton::clicked, this, [this] {
			if (Projection.has_value() && Projection->EntryPath.has_value() && EntryDispatch) {
				EntryDispatch(Projection->Entry, text(*Projection->EntryPath));
			}
		});
		sideColumn->addSpacing(12);
		auto* const entryRow = new QHBoxLayout;
		entryRow->setContentsMargins(0, 0, 0, 0);
		entryRow->addWidget(Entry);
		entryRow->addStretch(1);
		sideColumn->addLayout(entryRow);
		sideColumn->addStretch(1);
		sideScroll->setWidget(side);

		auto* const columns = new QHBoxLayout{ this };
		columns->setContentsMargins(0, 0, 0, 0);
		columns->setSpacing(0);
		columns->addWidget(main, 1);
		columns->addWidget(sideScroll);

		clear();
	}

	void pull_request_overview_page::present(const application::pull_request_overview_projection& aProjection)
	{
		Cached = false;
		CachedBase.clear();
		CachedHead.clear();
		CachedSource->clear();
		CachedSource->hide();
		CachedSourceBack->hide();
		CachedDiscussion->clear();
		CachedDiscussion->hide();
		CachedThreads->clear();
		CachedThreads->hide();
		CachedThreadsHeading->hide();
		const auto bodyChanged = Projection.has_value() && Projection->Body != aProjection.Body;
		const auto updateBody  = !Projection.has_value() || bodyChanged;
		Projection             = aProjection;
		for (int column = overview_file_model::changed_column; column < overview_file_model::column_count; ++column) {
			Files->setColumnHidden(column, false);
		}
		Files->setAccessibleDescription(QStringLiteral("Current file catalogue; activate a file to read it."));
		Summary->setToolTip({ });
		Summary->setAccessibleDescription({ });
		Title->setText(text(aProjection.Title));
		auto state = QString{ };
		switch (aProjection.State) {
		case application::github_pull_request_state::merged:
			state = QStringLiteral(" Merged.");
			break;
		case application::github_pull_request_state::closed:
			state = QStringLiteral(" Closed without merge.");
			break;
		case application::github_pull_request_state::open:
			state = aProjection.Draft ? QStringLiteral(" Draft.") : QString{ };
			break;
		}
		auto commits = aProjection.CommitCount == 1U
			? QStringLiteral("1 commit")
			: QStringLiteral("%1 commits").arg(count_text(aProjection.CommitCount));
		if (aProjection.ReturnedCommitCount < aProjection.CommitCount) {
			commits += QStringLiteral(" (%1 returned by GitHub)")
						   .arg(count_text(aProjection.ReturnedCommitCount));
		}
		Summary->setText(QStringLiteral("%1 wants to merge %2 into %3 from %4.%5")
							 .arg(text(aProjection.AuthorLogin),
								  commits,
								  text(aProjection.BaseRef),
								  text(aProjection.HeadRef),
								  state));
		if (updateBody) {
			Body->present_body(aProjection.Body);
		}
		Body->setVisible(!aProjection.Body.empty());
		EmptyBody->setVisible(aProjection.Body.empty());
		if (aProjection.Body.empty() && !ReturnToFiles->isHidden()) {
			set_description_expanded(false);
		}
		if (bodyChanged) {
			QAccessibleAnnouncementEvent event{ Body, QStringLiteral("Description updated") };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		}

		const auto& since = aProjection.SinceLastReview;
		present_files_heading(aProjection);
		Model->apply(aProjection.Files);

		rebuild_participants(aProjection.Participants);

		apply_history({ .Known = since.Known, .ReviewedAt = since.ReviewedAt, .Uploads = since.Uploads });
		const auto countOr = [](const std::optional<std::size_t>& aCount, const QString& aAbsent) {
			return aCount.has_value() ? count_text(*aCount) : aAbsent;
		};
		set_value(overview_value::files_changed,
				  since.Known ? countOr(since.FilesChanged, QStringLiteral("not yet resolved")) : QStringLiteral("—"),
				  since.FilesChanged.value_or(0U) != 0U);
		set_value(overview_value::new_comments,
				  countOr(since.NewComments, QStringLiteral("not available")),
				  since.NewComments.value_or(0U) != 0U);
		set_value(overview_value::replies_to_me, count_text(since.RepliesToMe), since.RepliesToMe != 0U);
		set_value(overview_value::my_unresolved_threads, count_text(since.MyUnresolvedThreads), false);
		set_value(overview_value::checks, checks_text(aProjection.Checks), aProjection.Checks.Failed != 0U);
		set_value(overview_value::files_read,
				  QStringLiteral("%1 of %2").arg(count_text(aProjection.Progress.FilesRead), count_text(aProjection.Progress.FilesTotal)),
				  false);
		set_value(overview_value::private_notes, count_text(aProjection.Progress.PrivateNotes), false);
		set_value(overview_value::drafted_comments,
				  aProjection.Progress.DraftedComments.has_value()
					  ? (*aProjection.Progress.DraftedComments == 0U
							 ? QStringLiteral("none")
							 : QStringLiteral("%1 unpublished").arg(count_text(*aProjection.Progress.DraftedComments)))
					  : QStringLiteral("not available"),
				  false);

		present_entry(aProjection);
	}

	void pull_request_overview_page::present_files_heading(const application::pull_request_overview_projection& aProjection)
	{
		const auto& since   = aProjection.SinceLastReview;
		const auto  notRead = aProjection.Progress.FilesTotal - std::min(aProjection.Progress.FilesRead, aProjection.Progress.FilesTotal);
		auto        heading = QStringLiteral("Files: %1 in this pull request").arg(count_text(aProjection.Progress.FilesTotal));
		if (since.Known && since.FilesChanged.has_value()) {
			heading += QStringLiteral(", %1 changed since my last review").arg(count_text(*since.FilesChanged));
		} else if (since.Known) {
			heading += QStringLiteral(", changes since my last review not yet resolved");
		}
		heading += QStringLiteral(", %1 not read").arg(count_text(notRead));
		FilesHeading->setText(heading);
		FilesHeading->setToolTip({ });
	}

	void pull_request_overview_page::present_entry(const application::pull_request_overview_projection& aProjection)
	{
		const auto start = aProjection.Entry == application::overview_entry_kind::start_review;
		Entry->setText(start ? QStringLiteral("Start review") : QStringLiteral("Continue review"));
		Entry->setAccessibleName(Entry->text());
		auto entryDescription = QStringLiteral("Nothing to read yet");
		if (aProjection.EntryPath.has_value()) {
			entryDescription = start
				? QStringLiteral("Starts Review at %1").arg(text(*aProjection.EntryPath))
				: QStringLiteral("Continues at %1").arg(text(*aProjection.EntryPath));
		}
		Entry->setToolTip(entryDescription);
		Entry->setAccessibleDescription(Entry->toolTip());
		Entry->setEnabled(aProjection.EntryPath.has_value());
	}

	void pull_request_overview_page::apply_history(const application::overview_history_update& aUpdate)
	{
		if (!Projection) {
			return;
		}
		auto& since      = Projection->SinceLastReview;
		since.Known      = aUpdate.Known;
		since.ReviewedAt = aUpdate.ReviewedAt;
		since.Uploads    = aUpdate.Uploads;
		if (!since.Known) {
			SinceHeading->setText(QStringLiteral("Since my last review: none yet"));
		} else if (since.ReviewedAt) {
			const auto when = QDateTime::fromMSecsSinceEpoch(since.ReviewedAt->value(), QTimeZone::UTC).toLocalTime();
			SinceHeading->setText(QStringLiteral("Since my last review on %1").arg(when.toString(QStringLiteral("MMM d"))));
		} else {
			SinceHeading->setText(QStringLiteral("Since my last review"));
		}
		set_value(overview_value::uploads,
				  since.Known ? (since.Uploads ? count_text(*since.Uploads) : QStringLiteral("not retained")) : QStringLiteral("—"),
				  since.Uploads.value_or(0U) != 0U);
	}

	void pull_request_overview_page::apply_receipts(const application::overview_receipt_update& aUpdate)
	{
		if (!Projection.has_value()) {
			return;
		}
		// The page keeps the progress and the action it dispatches; its rows
		// live in the model, which updates only the changed files.
		Projection->Progress  = aUpdate.Progress;
		Projection->Entry     = aUpdate.Entry;
		Projection->EntryPath = aUpdate.EntryPath;
		Model->update_rows(aUpdate.Rows);
		present_files_heading(*Projection);
		set_value(overview_value::files_read,
				  QStringLiteral("%1 of %2").arg(count_text(Projection->Progress.FilesRead), count_text(Projection->Progress.FilesTotal)),
				  false);
		present_entry(*Projection);
	}

	void pull_request_overview_page::clear()
	{
		set_description_expanded(false);
		Projection.reset();
		Cached = false;
		CachedBase.clear();
		CachedHead.clear();
		CachedSource->clear();
		CachedSource->hide();
		CachedSourceBack->hide();
		CachedDiscussion->clear();
		CachedDiscussion->hide();
		CachedThreads->clear();
		CachedThreads->hide();
		CachedThreadsHeading->hide();
		Title->setText(QStringLiteral("No pull request"));
		Summary->clear();
		Summary->setToolTip({ });
		Summary->setAccessibleDescription({ });
		Body->reset_reader();
		Body->setVisible(false);
		EmptyBody->setVisible(false);
		ReadDescription->setVisible(false);
		FilesHeading->setText(QStringLiteral("Files"));
		FilesHeading->setToolTip({ });
		Model->apply({ });
		rebuild_participants({ });
		SinceHeading->setText(QStringLiteral("Since my last review"));
		for (auto* const value : Values) {
			if (value != nullptr) {
				value->setText(QStringLiteral("—"));
				value->setForegroundRole(QPalette::PlaceholderText);
			}
		}
		Entry->setText(QStringLiteral("Start review"));
		Entry->setEnabled(false);
	}

	void pull_request_overview_page::present_loading()
	{
		Title->setText(QStringLiteral("Loading pull request…"));
		Summary->setText(QStringLiteral("Details, files and conversations are loading in the background."));
		FilesHeading->setText(QStringLiteral("Files — loading…"));
		if (auto* none = findChild<QLabel*>(QStringLiteral("overviewNoParticipants"))) {
			none->setText(QStringLiteral("Loading participants…"));
		}
	}

	void pull_request_overview_page::present_observation(const application::github_loading_overview_projection& aObservation)
	{
		clear();
		Title->setText(text(aObservation.Title));
		Summary->setToolTip({ });
		Summary->setAccessibleDescription({ });
		auto state = aObservation.Draft ? QStringLiteral("Draft") : QStringLiteral("Open");
		if (aObservation.State == application::github_pull_request_state_projection::merged) {
			state = QStringLiteral("Merged");
		} else if (aObservation.State == application::github_pull_request_state_projection::closed) {
			state = QStringLiteral("Closed without merge");
		}
		const auto commits = aObservation.ReportedCommits == 1U ? QStringLiteral("1 commit")
																: QStringLiteral("%1 commits").arg(count_text(aObservation.ReportedCommits));
		Summary->setText(QStringLiteral("%1 · %2 · %3 → %4 · %5. Provisional metadata; review data is still loading.")
							 .arg(aObservation.AuthorLogin.empty() ? QStringLiteral("Unknown author") : text(aObservation.AuthorLogin),
								  commits, text(aObservation.HeadRef), text(aObservation.BaseRef), state));
		Body->present_body(aObservation.Body);
		Body->setVisible(!aObservation.Body.empty());
		FilesHeading->setText(QStringLiteral("Files — %1 reported; catalogue loading…").arg(count_text(aObservation.ReportedFiles)));
		SinceHeading->setText(QStringLiteral("Review state — loading…"));
		if (auto* none = findChild<QLabel*>(QStringLiteral("overviewNoParticipants"))) {
			none->setText(QStringLiteral("Loading participants…"));
		}
		Entry->setText(QStringLiteral("Review data loading…"));
		Entry->setAccessibleName(Entry->text());
		Entry->setEnabled(false);
	}

	void pull_request_overview_page::present_cached(const application::github_cached_overview_projection& aOverview)
	{
		present_observation(aOverview.Overview);
		Cached     = true;
		CachedBase = aOverview.BaseOid;
		CachedHead = aOverview.HeadOid;
		Summary->setText(QStringLiteral("Cached overview · freshness unknown. Open this pull request again to retry."));
		Summary->setToolTip(QStringLiteral("Previously accepted base %1\nPreviously accepted head %2")
								.arg(text(aOverview.BaseOid), text(aOverview.HeadOid)));
		Summary->setAccessibleDescription(Summary->toolTip());
		FilesHeading->setText(aOverview.FileCatalogue
								  ? QStringLiteral("Cached file catalogue — %1 files observed; open a file to read retained source").arg(count_text(aOverview.FileCatalogue->size()))
								  : QStringLiteral("File catalogue unavailable offline; source unavailable offline"));
		FilesHeading->setToolTip(QStringLiteral("Historical GitHub change kinds and line counts, reported with the retained base/head. These are not a recomputed comparison of the base tip and head."));
		Model->apply(cached_file_rows(aOverview.FileCatalogue));
		for (int column = overview_file_model::changed_column; column < overview_file_model::column_count; ++column) {
			Files->setColumnHidden(column, true);
		}
		Files->setAccessibleDescription(QStringLiteral("Historical file catalogue; freshness unknown. Open a file to read its exact retained base and head when available. Current conversation counts and review progress unavailable."));
		CachedThreadsHeading->setText(aOverview.ReviewThreads
										  ? QStringLiteral("Cached review threads — %1 observed · freshness unknown").arg(count_text(aOverview.ReviewThreads->size()))
										  : QStringLiteral("Review threads unavailable offline"));
		CachedThreadsHeading->show();
		CachedDiscussion->setPlainText(cached_discussion_text(aOverview.GeneralComments, QStringLiteral("General comments")) + cached_discussion_text(aOverview.ReviewSummaries, QStringLiteral("Review summaries")));
		CachedDiscussion->show();
		CachedThreads->setPlainText(aOverview.ReviewThreads ? cached_thread_text(*aOverview.ReviewThreads) : QString{ });
		CachedThreads->setVisible(aOverview.ReviewThreads && !aOverview.ReviewThreads->empty());
		SinceHeading->setText(QStringLiteral("Review state unavailable offline"));
		if (auto* none = findChild<QLabel*>(QStringLiteral("overviewNoParticipants"))) {
			none->setWordWrap(true);
			none->setText(QStringLiteral("Participants unavailable offline"));
		}
		Entry->setText(QStringLiteral("Review data unavailable offline"));
		Entry->setAccessibleName(Entry->text());
	}

	void pull_request_overview_page::show_cached_source(bool aShow)
	{
		CachedSource->setVisible(aShow);
		CachedSourceBack->setVisible(aShow);
		Files->setVisible(!aShow);
		FilesHeading->setVisible(!aShow);
		CachedThreadsHeading->setVisible(!aShow);
		CachedThreads->setVisible(!aShow && !CachedThreads->toPlainText().isEmpty());
		CachedDiscussion->setVisible(!aShow);
	}

	void pull_request_overview_page::set_cached_source_dispatch(overview_path_dispatch aDispatch) { CachedSourceDispatch = std::move(aDispatch); }
	void pull_request_overview_page::present_cached_source(const application::cached_review_source_projection& aSource)
	{
		if (!Cached || aSource.BaseOid != CachedBase || aSource.HeadOid != CachedHead) {
			return;
		}
		auto content = QStringLiteral("Historical source: %1 · freshness unknown\nBase %2\nHead %3\n\n").arg(text(aSource.Path), text(aSource.BaseOid), text(aSource.HeadOid));
		switch (aSource.Status) {
		case application::cached_source_status::loading:     content += QStringLiteral("Reading retained source…"); break;
		case application::cached_source_status::unavailable: content += QStringLiteral("Source unavailable for this exact retained revision."); break;
		case application::cached_source_status::binary:      content += QStringLiteral("Binary source cannot be displayed as text."); break;
		case application::cached_source_status::too_large:   content += QStringLiteral("Source exceeds the offline reading limit."); break;
		case application::cached_source_status::available:
			if (!displayable_source(aSource.BaseText) || !displayable_source(aSource.HeadText)) {
				content += QStringLiteral("Source encoding cannot be displayed as UTF-8 text.");
				break;
			}
			content += QStringLiteral("BASE — ") + (aSource.BaseText ? QStringLiteral("observed source\n") + text(*aSource.BaseText) : QStringLiteral("path absent at this commit"));
			content += QStringLiteral("\n\nHEAD — ") + (aSource.HeadText ? QStringLiteral("observed source\n") + text(*aSource.HeadText) : QStringLiteral("path absent at this commit"));
			break;
		}
		CachedSource->setPlainText(content);
		show_cached_source(true);
		CachedSource->setFocus(Qt::OtherFocusReason);
	}

	void pull_request_overview_page::set_description_expanded(const bool aExpanded)
	{
		Body->set_expanded(aExpanded);
		Header->setVisible(!aExpanded);
		Side->setVisible(!aExpanded);
		Files->setVisible(!aExpanded);
		FilesHeading->setVisible(!aExpanded);
		ReturnToFiles->setVisible(aExpanded);
		ReadDescription->setVisible(false);
		EmptyBody->setVisible(!aExpanded && Projection.has_value() && Projection->Body.empty());
		findChild<QShortcut*>(QStringLiteral("overviewDescriptionEscape"))->setEnabled(aExpanded);
		if (aExpanded) {
			Body->setFocus(Qt::OtherFocusReason);
		} else if (isVisible()) {
			Files->setFocus(Qt::OtherFocusReason);
		}
		QWidget::setTabOrder(ReturnToFiles, Body);
	}

	void pull_request_overview_page::adopt_technical_details(
		QWidget* const aButton, QWidget* const aDetails)
	{
		if (TechnicalRow == nullptr) {
			return;
		}
		for (auto* const adopted : { aButton, aDetails }) {
			if (adopted == nullptr) {
				continue;
			}
			adopted->setParent(this);
			TechnicalRow->addWidget(adopted);
		}
		TechnicalRow->addStretch(1);
	}

	auto pull_request_overview_page::presented() const noexcept -> bool
	{
		return Projection.has_value();
	}

	auto pull_request_overview_page::projection() const noexcept
		-> const std::optional<application::pull_request_overview_projection>&
	{
		return Projection;
	}

	void pull_request_overview_page::adopt_appearance(const appearance_profile& aProfile)
	{
		Body->Reader->set_code_font(aProfile.code_font(), aProfile.code_tab_stop_distance());
		Colours.Unread   = aProfile.color(core::appearance_role::state_unread);
		Colours.Accepted = aProfile.color(core::appearance_role::state_accepted);
		Colours.Defect   = aProfile.color(core::appearance_role::state_defect);
		update();
		Files->viewport()->update();
	}

	auto pull_request_overview_page::colours() const noexcept -> const overview_colours&
	{
		return Colours;
	}

	void pull_request_overview_page::set_prepare_tour_action(QAction* aAction)
	{
		auto* button = new QPushButton{ tr("Prepare review tour"), Entry->parentWidget() };
		button->setObjectName(QStringLiteral("overviewPrepareTour"));
		button->setAccessibleDescription(tr("Write a private problem statement, solution and source tour."));
		Entry->parentWidget()->layout()->addWidget(button);
		button->setEnabled(aAction->isEnabled());
		connect(button, &QPushButton::clicked, aAction, &QAction::trigger);
		connect(aAction, &QAction::changed, button, [button, aAction] { button->setEnabled(aAction->isEnabled()); });
	}

	void pull_request_overview_page::set_entry_dispatch(overview_entry_dispatch aDispatch)
	{
		EntryDispatch = std::move(aDispatch);
	}

	void pull_request_overview_page::set_file_dispatch(overview_path_dispatch aDispatch)
	{
		FileDispatch = std::move(aDispatch);
	}

	auto pull_request_overview_page::title_label() const noexcept -> QLabel*
	{
		return Title;
	}

	auto pull_request_overview_page::summary_label() const noexcept -> QLabel*
	{
		return Summary;
	}

	auto pull_request_overview_page::body_view() const noexcept -> markdown_body_view*
	{
		return Body->Reader;
	}

	auto pull_request_overview_page::files_heading() const noexcept -> QLabel*
	{
		return FilesHeading;
	}

	auto pull_request_overview_page::files_view() const noexcept -> QTreeView*
	{
		return Files;
	}

	auto pull_request_overview_page::file_model() const noexcept -> overview_file_model*
	{
		return Model;
	}

	auto pull_request_overview_page::participant_rows() const -> std::vector<overview_participant_row*>
	{
		auto rows = std::vector<overview_participant_row*>{ };
		for (int index = 0; index < Participants->count(); ++index) {
			if (auto* const row = dynamic_cast<overview_participant_row*>(Participants->itemAt(index)->widget());
				row != nullptr) {
				rows.push_back(row);
			}
		}
		return rows;
	}

	auto pull_request_overview_page::since_heading() const noexcept -> QLabel*
	{
		return SinceHeading;
	}

	auto pull_request_overview_page::value_label(const overview_value aValue) const noexcept -> QLabel*
	{
		return Values[static_cast<std::size_t>(aValue)];
	}

	auto pull_request_overview_page::minimumSizeHint() const -> QSize
	{
		// A stacked widget's minimum is the largest of its pages', shown or
		// not. The comparison decides the centre's minimum; this page takes
		// what it is given and scrolls its table.
		return { 0, 0 };
	}

	auto pull_request_overview_page::entry_button() const noexcept -> QPushButton*
	{
		return Entry;
	}

	void pull_request_overview_page::set_value(const overview_value aValue, const QString& aText, const bool aHighlight)
	{
		auto* const label = Values[static_cast<std::size_t>(aValue)];
		if (label == nullptr) {
			return;
		}
		label->setText(aText);
		// A count that means new activity is in the unread colour; everything
		// else steps back, as the accepted design draws the side column.
		auto colours = palette();
		colours.setColor(QPalette::WindowText, aHighlight ? Colours.Unread : palette().color(QPalette::Active, QPalette::PlaceholderText));
		label->setPalette(colours);
		label->setForegroundRole(QPalette::WindowText);
	}

	void pull_request_overview_page::rebuild_participants(
		const std::vector<application::overview_participant>& aParticipants)
	{
		while (Participants->count() != 0) {
			auto* const item = Participants->takeAt(0);
			delete item->widget();
			delete item;
		}
		for (const auto& participant : aParticipants) {
			Participants->addWidget(new overview_participant_row{ participant, Colours });
		}
		if (aParticipants.empty()) {
			auto* const none = new QLabel{ QStringLiteral("No one yet"), this };
			none->setObjectName(QStringLiteral("overviewNoParticipants"));
			none->setTextFormat(Qt::PlainText);
			none->setForegroundRole(QPalette::PlaceholderText);
			Participants->addWidget(none);
		}
	}
} // namespace bpr::qt
