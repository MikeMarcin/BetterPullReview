#include "bpr/core/file_progress_mark.hpp"
#include "bpr/qt/identity_avatar.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "pull_request_overview_support.hpp"

#include <QApplication>
#include <QBrush>
#include <QChar>
#include <QColor>
#include <QFlags>
#include <QFont>
#include <QFontMetrics>
#include <QItemSelectionModel>
#include <QLatin1Char>
#include <QLatin1StringView>
#include <QModelIndex>
#include <QObject>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPersistentModelIndex>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QSignalBlocker>
#include <QSize>
#include <QSizePolicy>
#include <QString>
#include <QStringDecoder>
#include <QStyle>
#include <QStyleOption>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QTreeView>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cstddef>
#include <numeric>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
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

		constexpr int chip_width = 58;

		/// [[BPR-1331]] v003. The read column's mark: a 24-unit design box this
		/// wide paints a mark ten pixels across, as the dot it replaced.
		constexpr qreal read_mark_side = 15.0;

		/// Whether the cell's qualifier is the unknown mark or, with only
		/// conversations that have no resolution, the not-resolvable one.
		[[nodiscard]] auto qualifier_state(const core::feedback_counter_projection& aCounts) -> core::feedback_resolution
		{
			const auto onlyUnresolvable = aCounts.Exact && state_of(aCounts.States, core::feedback_resolution::unknown) == 0U && aCounts.ScopeUnclassified == 0U;
			return onlyUnresolvable ? core::feedback_resolution::not_applicable : core::feedback_resolution::unknown;
		}

		/// [[BPR-1363]] v002. A folder's file count, in the row's regular
		/// weight with tabular figures, so equal digit counts are equally wide
		/// and right-aligned counts line up digit for digit.
		[[nodiscard]] auto folder_count_font(const QFont& aFont) -> QFont
		{
			auto font = aFont;
			font.setWeight(QFont::Normal);
			font.setFeature(QFont::Tag{ "tnum" }, 1U);
			return font;
		}

		[[nodiscard]] auto folder_count_text(const QModelIndex& aIndex) -> QString
		{
			return QString::number(aIndex.data(overview_file_model::file_count_role).toULongLong());
		}

		/// The width of the count slot: the largest count's digits, as zeros.
		[[nodiscard]] auto folder_count_width(const QFont& aFont, const QModelIndex& aIndex) -> int
		{
			const auto* const model   = dynamic_cast<const overview_file_model*>(aIndex.model());
			const auto        largest = model != nullptr ? model->largest_folder_file_count() : aIndex.data(overview_file_model::file_count_role).toULongLong();
			const auto        digits  = QString::number(largest).size();
			return QFontMetrics{ folder_count_font(aFont) }.horizontalAdvance(QString(digits, QLatin1Char('0')));
		}

		/// The space between the count and the name that follows it.
		[[nodiscard]] auto folder_count_gap(const QFont& aFont) -> int
		{
			return QFontMetrics{ aFont }.horizontalAdvance(QLatin1Char(' '));
		}

		/// \brief One roster line: an avatar, the name, and a short coloured
		/// word at the right, over a rule. A circle is a person; [[BPR-1405]] a
		/// rounded square is a team. [[BPR-1583]] The avatar is the shared one,
		/// in the person's own colour.
		struct roster_line
		{
			QString              Name;
			identity_avatar_face Face;
			QString              Detail;
			QColor               DetailInk;
			/// [[BPR-1405]]. Requests keep both ends: teams of one organization
			/// differ only in their slug.
			Qt::TextElideMode Elide = Qt::ElideRight;
		};

		void paint_roster_line(QWidget& aRow, const roster_line& aLine)
		{
			auto painter = QPainter{ &aRow };
			painter.setRenderHint(QPainter::Antialiasing, true);
			const auto& colours = aRow.palette();
			const auto  centre  = QPointF{ 11.0, aRow.height() / 2.0 };
			const auto  tile    = QRectF{ centre.x() - 9.0, centre.y() - 9.0, 18.0, 18.0 };
			paint_identity_avatar(painter, tile, aLine.Face, identity_avatar_grounds_of(colours), aRow.font());
			// One compact line; the whole name stays in the tooltip and the
			// accessible name when space is tight.
			painter.setFont(aRow.font());
			const auto metrics     = aRow.fontMetrics();
			const auto detailWidth = std::min(metrics.horizontalAdvance(aLine.Detail), std::max(0, aRow.width() - 60));
			painter.setPen(aLine.DetailInk);
			painter.drawText(QRect{ aRow.width() - detailWidth - 4, 0, detailWidth, aRow.height() },
							 Qt::AlignVCenter | Qt::AlignRight, aLine.Detail);
			painter.setPen(colours.color(QPalette::Active, QPalette::WindowText));
			const auto nameWidth = std::max(0, aRow.width() - detailWidth - 44);
			painter.drawText(QRect{ 28, 0, nameWidth, aRow.height() }, Qt::AlignVCenter | Qt::AlignLeft,
							 metrics.elidedText(aLine.Name, aLine.Elide, nameWidth));
			// The rule beneath.
			painter.setPen(QPen{ colours.color(QPalette::Active, QPalette::Mid), 1.0 });
			painter.drawLine(QPointF{ 0.0, aRow.height() - 0.5 }, QPointF{ static_cast<qreal>(aRow.width()), aRow.height() - 0.5 });
		}

		[[nodiscard]] auto roster_height(const QWidget& aRow) -> int
		{
			return std::max(28, aRow.fontMetrics().height() + 10);
		}
	}

	auto overview_row_delegate::banded(const QModelIndex& aIndex, const QWidget* const aView) -> bool
	{
		const auto* const tree = qobject_cast<const QTreeView*>(aView);
		const auto        row  = aIndex.siblingAtColumn(0);
		const auto        heading
			= [](const QModelIndex& aRow) { return aRow.data(overview_file_model::folder_role).toBool() || aRow.data(overview_file_model::remainder_role).toBool(); };
		if (tree == nullptr || !row.isValid() || heading(row)) {
			return false;
		}
		// The rows shown between this one and the header its group starts
		// at. A file sits under its folder's header, and with uniform rows
		// the view's own geometry counts them: a hidden or collapsed row has
		// none, and the count costs no walk over the folder's earlier files.
		const auto parent = row.parent();
		if (parent.isValid()) {
			const auto header = tree->visualRect(parent);
			const auto own    = tree->visualRect(row);
			if (!header.isValid() || own.height() <= 0) {
				return false;
			}
			return ((own.top() - header.bottom() - 1) / own.height()) % 2 == 0;
		}
		// At the top level a heading row ends the group.
		auto shown = 0;
		for (auto before = row.row() - 1; before >= 0; --before) {
			if (!parent.isValid() && heading(row.sibling(before, 0))) {
				break;
			}
			if (!tree->isRowHidden(before, parent)) {
				++shown;
			}
		}
		return shown % 2 == 0;
	}

	void overview_row_delegate::initStyleOption(QStyleOptionViewItem* const aOption, const QModelIndex& aIndex) const
	{
		QStyledItemDelegate::initStyleOption(aOption, aIndex);
		if ((aOption->state & QStyle::State_Selected) != 0 || !banded(aIndex, aOption->widget)) {
			return;
		}
		const auto active   = overview_row_band(aOption->palette, QPalette::Active);
		const auto inactive = overview_row_band(aOption->palette, QPalette::Inactive);
		aOption->palette.setColor(QPalette::Active, QPalette::Base, active);
		aOption->palette.setColor(QPalette::Inactive, QPalette::Base, inactive);
		aOption->backgroundBrush = (aOption->state & QStyle::State_Active) != 0 ? active : inactive;
	}

	auto overview_row_delegate::sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
		-> QSize
	{
		const auto folder = aIndex.data(overview_file_model::folder_role).toBool();
		auto       option = aOption;
		if (folder && aIndex.column() == overview_file_model::operation_column) {
			// [[BPR-1363]] v002. A folder row's first cell holds its disclosure
			// mark, then its count's slot and the space before the name.
			const auto width = disclosure_width(aOption) + folder_count_width(aOption.font, aIndex) + folder_count_gap(aOption.font);
			return QSize{ width, std::max(QStyledItemDelegate::sizeHint(option, aIndex).height(), 22) };
		}
		const auto folderPath = folder && aIndex.column() == overview_file_model::file_column;
		if (folderPath) {
			// Measured in the weight it is painted in, so the path is never elided.
			option.font.setWeight(QFont::DemiBold);
		}
		auto hint = QStyledItemDelegate::sizeHint(option, aIndex);
		hint.setHeight(std::max(hint.height(), 22));
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

	auto overview_row_delegate::count_rect(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) -> QRect
	{
		return QRect{ aOption.rect.left() + disclosure_width(aOption), aOption.rect.top(), folder_count_width(aOption.font, aIndex), aOption.rect.height() };
	}

	void overview_row_delegate::paint_disclosure(QPainter* const aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
	{
		// [[BPR-1213]]. The rows stay flat, so the view has no indentation to
		// draw its branch in; the folder row draws the native disclosure mark
		// itself. [[BPR-1363]]: in its first cell, so the mark is the leftmost
		// thing on the row (owner, 2026-10-09). v002: the file count follows
		// it, right-aligned in a slot every folder row shares, so the counts
		// read as one column (owner, session d827fdb7).
		auto option = aOption;
		initStyleOption(&option, aIndex);
		const auto* const style = option.widget != nullptr ? option.widget->style() : QApplication::style();
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

		const auto group = option.state & QStyle::State_Active ? QPalette::Active : QPalette::Inactive;
		aPainter->save();
		aPainter->setClipRect(option.rect);
		aPainter->setFont(folder_count_font(option.font));
		aPainter->setPen(option.state & QStyle::State_Selected ? option.palette.color(group, QPalette::HighlightedText) : option.palette.color(group, QPalette::PlaceholderText));
		aPainter->drawText(count_rect(option, aIndex), Qt::AlignRight | Qt::AlignVCenter, folder_count_text(aIndex));
		aPainter->restore();
	}

	void overview_row_delegate::paint_folder(QPainter* const aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
	{
		// The folder's whole path, where every file name starts; its file
		// count is in the row's first cell ([[BPR-1363]] v002).
		auto option = aOption;
		initStyleOption(&option, aIndex);
		const auto* const style = option.widget != nullptr ? option.widget->style() : QApplication::style();
		const auto        text  = option.text;
		option.text.clear();
		style->drawControl(QStyle::CE_ItemViewItem, &option, aPainter, option.widget);

		const auto group      = option.state & QStyle::State_Active ? QPalette::Active : QPalette::Inactive;
		auto       label      = option;
		label.text            = text;
		label.backgroundBrush = Qt::NoBrush;
		label.state &= ~(QStyle::State_HasFocus | QStyle::State_Selected);
		label.font.setWeight(QFont::DemiBold);
		label.palette.setColor(QPalette::Text, option.state & QStyle::State_Selected ? option.palette.color(group, QPalette::HighlightedText) : option.palette.color(group, QPalette::PlaceholderText));
		style->drawControl(QStyle::CE_ItemViewItem, &label, aPainter, label.widget);
	}

	void overview_row_delegate::paint(
		QPainter* const             aPainter,
		const QStyleOptionViewItem& aOption,
		const QModelIndex&          aIndex) const
	{
		// [[BPR-1089]]. While a chip of this row has the keyboard, its ring is
		// the only focus mark: the current cell draws no frame of its own.
		if (ChipFocus && ChipFocus->Row.isValid() && ChipFocus->Row == aIndex.siblingAtColumn(0) && (aOption.state & QStyle::State_HasFocus) != 0) {
			auto option = aOption;
			option.state &= ~QStyle::State_HasFocus;
			paint_cell(aPainter, option, aIndex);
			return;
		}
		paint_cell(aPainter, aOption, aIndex);
	}

	void overview_row_delegate::paint_cell(QPainter* const aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const
	{
		const auto folder = aIndex.data(overview_file_model::folder_role).toBool();
		const auto column = aIndex.column();
		// [[BPR-1089]]. The label chips, on file, folder and remainder rows alike.
		if (column == overview_file_model::comments_column) {
			const auto* model  = dynamic_cast<const overview_file_model*>(aIndex.model());
			const auto* counts = model != nullptr ? model->feedback_at(aIndex) : nullptr;
			if (counts != nullptr) {
				auto option = aOption;
				initStyleOption(&option, aIndex);
				option.text.clear();
				const auto* style = option.widget != nullptr ? option.widget->style() : QApplication::style();
				style->drawControl(QStyle::CE_ItemViewItem, &option, aPainter, option.widget);
				paint_chips(aPainter, option, *counts, model->feedback()->Request.Mode);
				return;
			}
		}
		// [[BPR-1585]]. The Not on a file row is no file: it has no operation,
		// change or read state to mark, only its name and its chips.
		const auto remainder = aIndex.data(overview_file_model::remainder_role).toBool();
		const auto marked    = !folder && !remainder
			&& (column == overview_file_model::operation_column
				|| column == overview_file_model::changed_column
				|| column == overview_file_model::read_column
				|| column == overview_file_model::comments_column);
		if (folder && column == overview_file_model::operation_column) {
			paint_disclosure(aPainter, aOption, aIndex);
			return;
		}
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
			if (remainder) {
				option.font.setWeight(QFont::DemiBold);
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
			const auto selected = (option.state & QStyle::State_Selected) != 0;
			const auto group    = (option.state & QStyle::State_Active) != 0 ? QPalette::Active : QPalette::Inactive;
			const auto mark     = overview_operation_mark_colours(operation, option.palette, Colours, selected, group);
			const auto box      = QRectF{ rect.left() + 6.0, rect.center().y() - 7.0, 14.0, 14.0 };
			aPainter->setPen(mark.Outline.isValid() ? QPen{ mark.Outline, 1.0 } : QPen{ Qt::NoPen });
			aPainter->setBrush(mark.Fill.isValid() ? QBrush{ mark.Fill } : QBrush{ Qt::NoBrush });
			aPainter->drawRoundedRect(mark.Outline.isValid() ? box.adjusted(0.5, 0.5, -0.5, -0.5) : box, 3.0, 3.0);
			aPainter->setPen(mark.Letter);
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
			// [[BPR-1331]] v003. The review bar's marks, one family wherever a
			// file's progress shows: a ring unread (and with no receipt), half
			// filled seen, a disc read, a disc with a tick cut out accepted, a
			// ring struck through excluded, each in its own theme colour and
			// its own shape, so the state survives without its colour.
			const auto mark   = aIndex.data(overview_file_model::progress_role).toInt();
			const auto centre = QPointF{ rect.left() + 13.0, static_cast<qreal>(rect.center().y()) };
			const auto shown  = mark < 0 ? core::file_progress_mark::unread : static_cast<core::file_progress_mark>(mark);
			paint_progress_mark(*aPainter, QRectF{ centre.x() - (read_mark_side / 2.0), centre.y() - (read_mark_side / 2.0), read_mark_side, read_mark_side }, shown, Colours.Progress);
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

	void overview_row_delegate::paint_chips(QPainter* const aPainter, const QStyleOptionViewItem& aOption, const core::feedback_counter_projection& aCounts,
											const core::feedback_resolution_mode aMode) const
	{
		const auto chips = overview_comment_chips(aCounts, aOption.rect, aOption.font);
		if (chips.empty()) {
			return; // a complete zero: blank, and nothing else is blank
		}
		aPainter->save();
		aPainter->setRenderHint(QPainter::Antialiasing, true);
		const auto  selected   = (aOption.state & QStyle::State_Selected) != 0;
		const auto  group      = (aOption.state & QStyle::State_Active) != 0 ? QPalette::Active : QPalette::Inactive;
		const auto  ink        = aOption.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text);
		const auto* model      = dynamic_cast<const overview_file_model*>(aOption.index.model());
		const auto  anyColumns = model != nullptr && any_columns(model->feedback());
		const auto  open       = aMode == core::feedback_resolution_mode::unresolved;
		aPainter->setFont(chip_font(aOption.font));
		for (const auto& chip : chips) {
			const auto& box = chip.Box;
			switch (chip.Kind) {
			case overview_comment_chip::kind::label:
			case overview_comment_chip::kind::other: {
				const auto mark = QRectF{ box.left(), box.top(), chip_mark, chip_mark };
				if (chip.Kind == overview_comment_chip::kind::label) {
					paint_feedback_mark(*aPainter, mark, *chip.Label, { }, feedback_mark_colours_for(Colours.Marks, *chip.Label, aOption.palette));
				} else {
					paint_bubble(*aPainter, mark, ink, anyColumns);
				}
				// The count is its digits beside the mark, as the selected design
				// draws it: in Open, the open amber on every row; in All, the
				// text ink with the state breakdown beneath it. A selected row
				// keeps its highlighted ink, so the digits stay legible.
				const auto pill = QRectF{ mark.right() + count_gap, box.top() + (open ? 0.5 : 0.0), box.width() - chip_mark - count_gap, open ? 15.0 : 13.5 };
				aPainter->setPen(open && !selected ? Colours.States.Open : ink);
				aPainter->drawText(pill, Qt::AlignLeft | Qt::AlignVCenter, QString::number(static_cast<qulonglong>(chip.Count)));
				if (!open) {
					// Open, resolved, unknown and not resolvable, in proportion.
					const auto total = std::accumulate(chip.States.begin(), chip.States.end(), std::size_t{ 0U });
					const auto bar   = QRectF{ pill.left(), pill.bottom() + 0.75, pill.width(), 2.5 };
					const auto fills = std::array{ Colours.States.Open, Colours.States.Resolved, aOption.palette.color(group, QPalette::PlaceholderText), aOption.palette.color(group, QPalette::Mid) };
					auto       x     = bar.left();
					aPainter->setPen(Qt::NoPen);
					for (std::size_t state = 0U; state < chip.States.size() && total != 0U; ++state) {
						const auto width = bar.width() * static_cast<qreal>(chip.States[state]) / static_cast<qreal>(total);
						if (width > 0.0) {
							aPainter->setBrush(fills[state]);
							aPainter->drawRect(QRectF{ x, bar.top(), width, bar.height() });
							x += width;
						}
					}
				}
				break;
			}
			case overview_comment_chip::kind::more:
				aPainter->setBrush(Qt::NoBrush);
				aPainter->setPen(QPen{ aOption.palette.color(group, QPalette::Mid), 1.0 });
				aPainter->drawRoundedRect(box.adjusted(0.5, 1.0, -0.5, -1.0), 7.0, 7.0);
				aPainter->setPen(ink);
				aPainter->drawText(box, Qt::AlignCenter, QStringLiteral("+%1").arg(chip.Count));
				break;
			case overview_comment_chip::kind::uncertain: {
				auto states  = Colours.States;
				states.Faint = selected ? ink : feedback_state_faint(aOption.palette, group);
				paint_feedback_state_mark(*aPainter, box, qualifier_state(aCounts), states);
				break;
			}
			}
		}
		// The chip the keyboard is on wears a ring, in the selection's own
		// text colour on the selected row, while the files have focus.
		const auto focused = ChipFocus && ChipFocus->Row.isValid() && ChipFocus->Row == aOption.index.siblingAtColumn(0) && aOption.widget != nullptr && aOption.widget->hasFocus();
		if (focused) {
			const auto found = std::ranges::find_if(chips, [this](const overview_comment_chip& aChip) { return aChip.Kind == ChipFocus->Kind && aChip.Label == ChipFocus->Label; });
			if (found != chips.end()) {
				aPainter->setBrush(Qt::NoBrush);
				aPainter->setPen(QPen{ aOption.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Highlight), 1.5 });
				aPainter->drawRoundedRect(found->Box.adjusted(-3.0, -2.0, 3.0, 2.0), 5.0, 5.0);
			}
		}
		aPainter->restore();
	}

	void overview_row_delegate::set_chip_focus(std::optional<overview_chip_focus> aFocus)
	{
		ChipFocus = std::move(aFocus);
	}

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
		return roster_height(*this);
	}

	void overview_participant_row::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		const auto& colours = palette();
		// The standing is coloured by what it means.
		auto standing = colours.color(QPalette::Active, QPalette::PlaceholderText);
		switch (Participant.Signoff) {
		case application::overview_signoff::approved:
			standing = Colours.Accepted;
			break;
		case application::overview_signoff::changes_requested:
			// [[BPR-1589]]. Red is blocking (RFC-BPR-0085 principle 4), as the face's mark.
			standing = Colours.Defect;
			break;
		case application::overview_signoff::dismissed:
		case application::overview_signoff::pending:
		case application::overview_signoff::uploaded:
		case application::overview_signoff::commented:
		case application::overview_signoff::none:
			break;
		}
		// [[BPR-1583]]. Mine is ringed, as Threads rings it.
		const auto face = identity_avatar_face{ .Identity = login(), .Ring = Participant.Viewer ? colours.color(QPalette::Active, QPalette::PlaceholderText) : QColor{ } };
		paint_roster_line(*this, { .Name = login(), .Face = face, .Detail = standing_text().isEmpty() ? role_text(Participant.Role) : standing_text(), .DetailInk = standing });
	}

	overview_request_row::overview_request_row(
		const application::overview_review_request& aRequest,
		const overview_colours&                     aColours,
		QWidget* const                              aParent)
		: QWidget{ aParent }
		, Request{ aRequest }
		, Colours{ aColours }
	{
		setObjectName(QStringLiteral("overviewReviewRequest"));
		const auto name  = Request.Name.empty() ? QStringLiteral("Reviewer unavailable") : text(Request.Name);
		auto       label = Request.Team ? QStringLiteral("%1, team").arg(name) : name;
		if (!Request.DisplayName.empty()) {
			label += QStringLiteral(" (%1)").arg(text(Request.DisplayName));
		}
		// [[BPR-1849]]. A request just asked for says so until GitHub answers.
		setAccessibleName(QStringLiteral("%1, %2").arg(label, Request.Pending ? QStringLiteral("asking GitHub for a review") : Request.CodeOwner ? QStringLiteral("code-owner review requested")
																																				 : QStringLiteral("review requested")));
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		setToolTip(accessibleName());
	}

	auto overview_request_row::request() const noexcept -> const application::overview_review_request&
	{
		return Request;
	}

	auto overview_request_row::sizeHint() const -> QSize
	{
		return { 200, heightForWidth(200) };
	}

	auto overview_request_row::minimumSizeHint() const -> QSize
	{
		return { 0, heightForWidth(width()) };
	}

	auto overview_request_row::heightForWidth(const int aWidth) const -> int
	{
		static_cast<void>(aWidth);
		return roster_height(*this);
	}

	void overview_request_row::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		const auto& colours = palette();
		const auto  faint   = colours.color(QPalette::Active, QPalette::PlaceholderText);
		const auto  name    = Request.Name.empty() ? QStringLiteral("Reviewer unavailable") : text(Request.Name);
		// [[BPR-1583]]. A team is keyed on organization/slug and initialled from
		// its slug; a hidden reviewer is the neutral face.
		const auto identity = text(Request.Name);
		const auto face     = identity_avatar_face{ .Identity     = identity,
													.InitialsFrom = Request.Team ? identity.mid(identity.indexOf(QLatin1Char{ '/' }) + 1) : QString{ },
													.Shape        = Request.Team ? identity_avatar_shape::group : identity_avatar_shape::person };
		const auto detail   = Request.Pending ? QStringLiteral("asking…") : Request.CodeOwner ? QStringLiteral("code owner")
																							  : QStringLiteral("requested");
		paint_roster_line(*this, { .Name = name, .Face = face, .Detail = detail, .DetailInk = Request.CodeOwner && !Request.Pending ? Colours.Waiting : faint, .Elide = Qt::ElideMiddle });
	}
}
