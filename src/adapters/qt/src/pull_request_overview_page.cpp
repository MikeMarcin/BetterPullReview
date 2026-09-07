#include "bpr/qt/pull_request_overview_page.hpp"

#include "bpr/qt/appearance_profile.hpp"

#include <QAbstractItemView>
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
#include <QSizePolicy>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTimeZone>
#include <QTreeView>
#include <QVBoxLayout>

#include <algorithm>
#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bpr::qt {
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
	} // namespace

	// ---- overview_file_model ------------------------------------------------

	overview_file_model::overview_file_model(QObject* const aParent)
		: QAbstractItemModel{ aParent }
	{
	}

	void overview_file_model::apply(const std::vector<application::overview_file_row>& aRows)
	{
		beginResetModel();
		Folders.clear();
		for (const auto& row : aRows) {
			const auto name = text(row.Folder);
			if (Folders.empty() || Folders.back().Name != name) {
				Folders.push_back(folder{ .Name = name, .Files = { } });
			}
			Folders.back().Files.push_back(row);
		}
		endResetModel();
	}

	auto overview_file_model::index(const int aRow, const int aColumn, const QModelIndex& aParent) const
		-> QModelIndex
	{
		if (aRow < 0 || aColumn < 0 || aColumn >= column_count) {
			return { };
		}
		if (!aParent.isValid()) {
			return static_cast<std::size_t>(aRow) < Folders.size()
				? createIndex(aRow, aColumn, static_cast<quintptr>(0U))
				: QModelIndex{ };
		}
		if (aParent.internalId() != 0U || aParent.column() != 0) {
			return { };
		}
		const auto folderIndex = static_cast<std::size_t>(aParent.row());
		if (folderIndex >= Folders.size()
			|| static_cast<std::size_t>(aRow) >= Folders[folderIndex].Files.size()) {
			return { };
		}
		// A file's internal id is its folder's row plus one; folders carry zero.
		return createIndex(aRow, aColumn, static_cast<quintptr>(folderIndex) + 1U);
	}

	auto overview_file_model::parent(const QModelIndex& aIndex) const -> QModelIndex
	{
		if (!aIndex.isValid() || aIndex.internalId() == 0U) {
			return { };
		}
		return createIndex(static_cast<int>(aIndex.internalId() - 1U), 0, static_cast<quintptr>(0U));
	}

	auto overview_file_model::rowCount(const QModelIndex& aParent) const -> int
	{
		if (!aParent.isValid()) {
			return static_cast<int>(Folders.size());
		}
		if (aParent.internalId() != 0U || aParent.column() != 0) {
			return 0;
		}
		const auto folderIndex = static_cast<std::size_t>(aParent.row());
		return folderIndex < Folders.size() ? static_cast<int>(Folders[folderIndex].Files.size()) : 0;
	}

	auto overview_file_model::columnCount(const QModelIndex& aParent) const -> int
	{
		static_cast<void>(aParent);
		return column_count;
	}

	auto overview_file_model::row_at(const QModelIndex& aIndex) const
		-> const application::overview_file_row*
	{
		if (!aIndex.isValid() || aIndex.internalId() == 0U) {
			return nullptr;
		}
		const auto folderIndex = static_cast<std::size_t>(aIndex.internalId() - 1U);
		if (folderIndex >= Folders.size()) {
			return nullptr;
		}
		const auto& files = Folders[folderIndex].Files;
		const auto  row   = static_cast<std::size_t>(aIndex.row());
		return row < files.size() ? &files[row] : nullptr;
	}

	auto overview_file_model::data(const QModelIndex& aIndex, const int aRole) const -> QVariant
	{
		if (!aIndex.isValid()) {
			return { };
		}
		if (aIndex.internalId() == 0U) {
			const auto& folder = Folders[static_cast<std::size_t>(aIndex.row())];
			if (aRole == folder_role) {
				return true;
			}
			if (aIndex.column() != file_column && aIndex.column() != operation_column) {
				return { };
			}
			if (aRole == Qt::DisplayRole && aIndex.column() == file_column) {
				return folder.Name.isEmpty() ? QStringLiteral("/") : folder.Name;
			}
			if (aRole == Qt::DisplayRole && aIndex.column() == operation_column) {
				return QString::number(folder.Files.size());
			}
			if (aRole == Qt::ToolTipRole) {
				return QStringLiteral("%1 files").arg(folder.Files.size());
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
		return Folders.size();
	}

	auto overview_file_model::file_count() const noexcept -> std::size_t
	{
		auto count = std::size_t{ 0U };
		for (const auto& folder : Folders) {
			count += folder.Files.size();
		}
		return count;
	}

	auto overview_file_model::index_for_path(const QString& aPath) const -> QModelIndex
	{
		const auto path = aPath.toStdString();
		for (std::size_t folderIndex = 0U; folderIndex < Folders.size(); ++folderIndex) {
			const auto& files = Folders[folderIndex].Files;
			for (std::size_t row = 0U; row < files.size(); ++row) {
				if (files[row].Path == path) {
					return createIndex(static_cast<int>(row), file_column, static_cast<quintptr>(folderIndex) + 1U);
				}
			}
		}
		return { };
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
		auto hint = QStyledItemDelegate::sizeHint(aOption, aIndex);
		hint.setHeight(std::max(hint.height(), 22));
		return hint;
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
		return { 200, std::max(24, fontMetrics().height() + 8) };
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
		// Login, then the role in the faint ink.
		painter.setFont(font());
		painter.setPen(ink);
		const auto metrics    = QFontMetrics{ font() };
		const auto loginWidth = metrics.horizontalAdvance(login());
		painter.drawText(QRectF{ 28.0, 0.0, static_cast<qreal>(loginWidth + 2), static_cast<qreal>(height()) }, Qt::AlignVCenter | Qt::AlignLeft, login());
		painter.setPen(faint);
		painter.setFont(small);
		painter.drawText(QRectF{ 28.0 + loginWidth + 6.0, 0.0, 90.0, static_cast<qreal>(height()) }, Qt::AlignVCenter | Qt::AlignLeft, role_text(Participant.Role));
		// The standing at the right, coloured by what it means.
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
		painter.drawText(QRectF{ 0.0, 0.0, width() - 2.0, static_cast<qreal>(height()) }, Qt::AlignVCenter | Qt::AlignRight, standing_text());
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

		Title = new QLabel{ main };
		Title->setObjectName(QStringLiteral("overviewTitle"));
		Title->setAccessibleName(QStringLiteral("Pull request title"));
		Title->setTextFormat(Qt::PlainText);
		Title->setWordWrap(true);
		auto titleFont = Title->font();
		titleFont.setPointSizeF(titleFont.pointSizeF() * 1.3);
		titleFont.setWeight(QFont::DemiBold);
		Title->setFont(titleFont);
		mainColumn->addWidget(Title);

		Summary = new QLabel{ main };
		Summary->setObjectName(QStringLiteral("overviewSummary"));
		Summary->setAccessibleName(QStringLiteral("Pull request author and branches"));
		Summary->setTextFormat(Qt::PlainText);
		Summary->setWordWrap(true);
		Summary->setForegroundRole(QPalette::PlaceholderText);
		mainColumn->addWidget(Summary);
		// [[BPR-0671]]. The commit oids behind the branches, disclosed. They
		// were in the files drawer's summary box, which the overview replaces;
		// they belong beside the branches they identify.
		TechnicalRow = new QHBoxLayout;
		TechnicalRow->setContentsMargins(0, 0, 0, 0);
		TechnicalRow->setSpacing(6);
		mainColumn->addLayout(TechnicalRow);

		Body = new QPlainTextEdit{ main };
		Body->setObjectName(QStringLiteral("overviewBody"));
		Body->setAccessibleName(QStringLiteral("Pull request description"));
		Body->setReadOnly(true);
		Body->setTabChangesFocus(true);
		Body->setFrameShape(QFrame::NoFrame);
		Body->setMaximumHeight(110);
		Body->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Maximum);
		mainColumn->addSpacing(4);
		mainColumn->addWidget(Body);

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
			"activate a file to read it"));
		Files->setModel(Model);
		Files->setItemDelegate(new overview_row_delegate{ Colours, Files });
		Files->setRootIsDecorated(false);
		Files->setItemsExpandable(false);
		Files->setUniformRowHeights(true);
		Files->setAlternatingRowColors(false);
		Files->setSelectionBehavior(QAbstractItemView::SelectRows);
		Files->setSelectionMode(QAbstractItemView::SingleSelection);
		Files->setFrameShape(QFrame::NoFrame);
		Files->setIndentation(0);
		Files->setEditTriggers(QAbstractItemView::NoEditTriggers);
		Files->header()->setStretchLastSection(true);
		Files->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
		Files->header()->setSectionResizeMode(overview_file_model::note_column, QHeaderView::Stretch);
		connect(Files, &QAbstractItemView::activated, this, [this](const QModelIndex& aIndex) {
			const auto path = aIndex.siblingAtColumn(overview_file_model::file_column)
								  .data(overview_file_model::path_role)
								  .toString();
			if (!path.isEmpty() && FileDispatch) {
				FileDispatch(path);
			}
		});
		mainColumn->addWidget(Files, 1);

		auto* const side = new QWidget{ this };
		side->setObjectName(QStringLiteral("overviewSideColumn"));
		side->setFixedWidth(side_width);
		side->setAutoFillBackground(true);
		side->setBackgroundRole(QPalette::AlternateBase);
		auto* const sideColumn = new QVBoxLayout{ side };
		sideColumn->setContentsMargins(14, 12, 14, 12);
		sideColumn->setSpacing(2);

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
				EntryDispatch(text(*Projection->EntryPath));
			}
		});
		sideColumn->addSpacing(12);
		auto* const entryRow = new QHBoxLayout;
		entryRow->setContentsMargins(0, 0, 0, 0);
		entryRow->addWidget(Entry);
		entryRow->addStretch(1);
		sideColumn->addLayout(entryRow);
		sideColumn->addStretch(1);

		auto* const columns = new QHBoxLayout{ this };
		columns->setContentsMargins(0, 0, 0, 0);
		columns->setSpacing(0);
		columns->addWidget(main, 1);
		columns->addWidget(side);

		clear();
	}

	void pull_request_overview_page::present(const application::pull_request_overview_projection& aProjection)
	{
		Projection = aProjection;
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
		Summary->setText(QStringLiteral("%1 wants to merge %2 into %3 from %4.%5")
							 .arg(text(aProjection.AuthorLogin),
								  aProjection.CommitCount == 1U
									  ? QStringLiteral("1 commit")
									  : QStringLiteral("%1 commits").arg(count_text(aProjection.CommitCount)),
								  text(aProjection.BaseRef),
								  text(aProjection.HeadRef),
								  state));
		Body->setPlainText(text(aProjection.Body));
		Body->setVisible(!aProjection.Body.empty());

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
		Model->apply(aProjection.Files);
		Files->expandAll();
		for (int row = 0; row < Model->rowCount(); ++row) {
			Files->setFirstColumnSpanned(row, { }, false);
		}

		rebuild_participants(aProjection.Participants);

		if (!since.Known) {
			SinceHeading->setText(QStringLiteral("Since my last review: none yet"));
		} else if (since.ReviewedAt.has_value()) {
			const auto when = QDateTime::fromMSecsSinceEpoch(since.ReviewedAt->value(), QTimeZone::UTC).toLocalTime();
			SinceHeading->setText(QStringLiteral("Since my last review on %1").arg(when.toString(QStringLiteral("MMM d"))));
		} else {
			SinceHeading->setText(QStringLiteral("Since my last review"));
		}
		const auto countOr = [](const std::optional<std::size_t>& aCount, const QString& aAbsent) {
			return aCount.has_value() ? count_text(*aCount) : aAbsent;
		};
		set_value(overview_value::uploads,
				  since.Known ? countOr(since.Uploads, QStringLiteral("not retained")) : QStringLiteral("—"),
				  since.Uploads.value_or(0U) != 0U);
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

		const auto start = aProjection.Entry == application::overview_entry_kind::start_review;
		Entry->setText(start ? QStringLiteral("Start review") : QStringLiteral("Continue review"));
		Entry->setAccessibleName(Entry->text());
		Entry->setToolTip(aProjection.EntryPath.has_value()
							  ? QStringLiteral("Opens %1 in Read").arg(text(*aProjection.EntryPath))
							  : QStringLiteral("Nothing to read yet"));
		Entry->setEnabled(aProjection.EntryPath.has_value());
	}

	void pull_request_overview_page::clear()
	{
		Projection.reset();
		Title->setText(QStringLiteral("No pull request"));
		Summary->clear();
		Body->clear();
		Body->setVisible(false);
		FilesHeading->setText(QStringLiteral("Files"));
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

	void pull_request_overview_page::set_entry_dispatch(overview_path_dispatch aDispatch)
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

	auto pull_request_overview_page::body_view() const noexcept -> QPlainTextEdit*
	{
		return Body;
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
