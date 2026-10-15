#include "bpr/qt/source_search_panel.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "painted_glyph_icon.hpp"
#include <QAbstractButton>
#include <QAbstractItemModel>
#include <QAccessible>
#include <QApplication>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <algorithm>
#include <array>
#include <map>
namespace bpr::qt {
	namespace {
		/// The snippet as drawn and the match's place in it, in UTF-16 units.
		struct snippet
		{
			QString Text;
			int     Start  = 0;
			int     Length = 0;
		};
		auto snippet_of(const core::source_match_record& aRecord) -> snippet
		{
			const auto& preview = aRecord.Preview;
			const auto  begin   = static_cast<std::size_t>(std::min<std::uint64_t>(aRecord.PreviewMatch.Begin, preview.size()));
			const auto  end     = static_cast<std::size_t>(std::clamp<std::uint64_t>(aRecord.PreviewMatch.End, begin, preview.size()));
			auto        result  = snippet{ };
			const auto  before  = QString::fromUtf8(preview.data(), static_cast<qsizetype>(begin));
			const auto  match   = QString::fromUtf8(preview.data() + begin, static_cast<qsizetype>(end - begin));
			// The row shows the match's own line: context stops at the next newline.
			const auto stop  = std::min(preview.find('\n', end), preview.size());
			const auto after = QString::fromUtf8(preview.data() + end, static_cast<qsizetype>(stop - end));
			result.Text      = before + match + after;
			result.Start     = static_cast<int>(before.size());
			result.Length    = static_cast<int>(match.size());
			// A newline or tab is one UTF-16 unit drawn as one, so the range holds.
			result.Text.replace(QLatin1Char('\n'), QChar{ 0x21b5 }).replace(QLatin1Char('\t'), QLatin1Char(' '));
			auto blank = 0;
			while (blank < result.Start && result.Text[blank] == QLatin1Char(' ')) {
				++blank;
			}
			result.Text.remove(0, blank);
			result.Start -= blank;
			return result;
		}
		auto plural(std::size_t aCount, const QString& aOne, const QString& aMany) -> QString { return (aCount == 1U ? aOne : aMany).arg(aCount); }

		/// \brief File rows retain native disclosure; match rows use the full viewport.
		class source_search_result_tree final : public QTreeView
		{
		public:
			using QTreeView::QTreeView;
			auto visualRect(const QModelIndex& aIndex) const -> QRect override
			{
				auto area = QTreeView::visualRect(aIndex);
				if (!area.isEmpty() && !aIndex.data(source_search_file_role).toBool()) {
					area.setLeft(0);
				}
				return area;
			}

		protected:
			void drawBranches(QPainter* aPainter, const QRect& aArea, const QModelIndex& aIndex) const override
			{
				if (aIndex.data(source_search_file_role).toBool()) {
					QTreeView::drawBranches(aPainter, aArea, aIndex);
				}
			}
		};
	}
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
			return aParent.internalId() == 0 && aParent.row() < static_cast<int>(Groups.size()) ? static_cast<int>(Groups[static_cast<std::size_t>(aParent.row())].Rows.size()) : 0;
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
			if (aRole == source_search_file_role) {
				return !leaf;
			}
			return leaf ? match_data(g, static_cast<std::size_t>(aIndex.row()), aRole) : file_data(g, aRole);
		}
		void present(const application::source_search_projection& aProjection, bool aClear)
		{
			if (aClear || !aProjection.Matches || aProjection.Matches->size() < Count) {
				clear();
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
			append();
		}
		void set_filter(core::source_side_filter aFilter)
		{
			if (Filter == aFilter) {
				return;
			}
			Filter = aFilter;
			clear();
			append();
		}
		auto match_index(std::uint32_t aMatch) const -> QModelIndex
		{
			const auto next = std::upper_bound(Groups.begin(), Groups.end(), aMatch, [](auto aValue, const group& aGroup) { return aValue < aGroup.Rows.front(); });
			if (next == Groups.begin()) {
				return { };
			}
			const auto& rows  = (next - 1)->Rows;
			const auto  found = std::ranges::lower_bound(rows, aMatch);
			if (found == rows.end() || *found != aMatch) {
				return { };
			}
			return index(static_cast<int>(found - rows.begin()), 0, index(static_cast<int>(next - 1 - Groups.begin()), 0));
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
				if (!Groups[static_cast<std::size_t>(i)].Rows.empty()) {
					emit dataChanged(index(0, 0, p), index(static_cast<int>(Groups[static_cast<std::size_t>(i)].Rows.size() - 1), 0, p), { Qt::FontRole });
				}
			}
		}
		[[nodiscard]] auto filter() const -> core::source_side_filter { return Filter; }
		/// Digits of the widest shown line number, kept as rows arrive.
		[[nodiscard]] auto line_digits() const -> int { return LineDigits; }
		[[nodiscard]] auto shown_matches() const -> std::size_t { return Shown; }
		[[nodiscard]] auto shown_files() const -> std::size_t { return Groups.size(); }

	private:
		struct group
		{
			std::size_t                Entry;
			std::vector<std::uint32_t> Rows;
		};
		std::vector<group>                                                   Groups;
		std::map<core::file_identity, std::size_t>                           Entries;
		std::size_t                                                          Count      = 0;
		std::size_t                                                          Shown      = 0;
		int                                                                  LineDigits = 1;
		core::source_side_filter                                             Filter;
		std::shared_ptr<const application::source_corpus>                    Corpus;
		std::shared_ptr<const std::vector<application::source_search_match>> Matches;
		QFont                                                                SnippetFont;
		void                                                                 clear()
		{
			if (!Groups.empty()) {
				beginRemoveRows({ }, 0, static_cast<int>(Groups.size() - 1));
				Groups.clear();
				endRemoveRows();
			}
			Count      = 0;
			Shown      = 0;
			LineDigits = 1;
		}
		void append()
		{
			if (!Matches || !Corpus) {
				return;
			}
			auto rows = std::vector<std::uint32_t>{ };
			while (Count < Matches->size()) {
				const auto file  = (*Matches)[Count].File;
				const auto found = Entries.find(file);
				if (found == Entries.end()) {
					return;
				}
				auto end = Count;
				rows.clear();
				while (end < Matches->size() && (*Matches)[end].File == file) {
					if (const auto& match = (*Matches)[end].Match; core::shown(match, Filter)) {
						rows.push_back(static_cast<std::uint32_t>(end));
						const auto& location = match.Locations[1] ? match.Locations[1] : match.Locations[0];
						if (location) {
							LineDigits = std::max(LineDigits, static_cast<int>(QString::number(location->Line + 1U).size()));
						}
					}
					++end;
				}
				Count = end;
				if (rows.empty()) {
					continue;
				}
				if (Groups.empty() || Corpus->Entries[Groups.back().Entry].File != file) {
					const auto row = static_cast<int>(Groups.size());
					beginInsertRows({ }, row, row);
					Groups.push_back({ found->second, { } });
					endInsertRows();
				}
				auto&      g      = Groups.back();
				const auto parent = index(static_cast<int>(Groups.size() - 1), 0);
				const auto first  = static_cast<int>(g.Rows.size());
				beginInsertRows(parent, first, first + static_cast<int>(rows.size()) - 1);
				g.Rows.insert(g.Rows.end(), rows.begin(), rows.end());
				Shown += rows.size();
				endInsertRows();
				emit dataChanged(parent, parent);
			}
		}
		auto file_data(const group& aGroup, int aRole) const -> QVariant
		{
			const auto& entry   = Corpus->Entries[aGroup.Entry];
			const auto  path    = QString::fromStdString(std::string{ entry.path() });
			const auto  renamed = entry.Left && entry.Right && entry.Left->Path != entry.Right->Path;
			const auto  oldPath = renamed ? QString::fromStdString(entry.Left->Path) : path;
			const auto  newPath = renamed ? QString::fromStdString(entry.Right->Path) : path;
			const auto  change  = renamed ? tr("renamed") : entry.Left && !entry.Right ? tr("removed")
				: entry.Right && !entry.Left                                           ? tr("added")
																					   : QString{ };
			const auto  name    = [](const QString& aPath) { return aPath.mid(aPath.lastIndexOf(QLatin1Char('/')) + 1); };
			const auto  folder  = [](const QString& aPath) { const auto slash = aPath.lastIndexOf(QLatin1Char('/')); return slash < 0 ? QString{ } : aPath.left(slash); };
			switch (aRole) {
			case source_search_name_role:   return name(oldPath) == name(newPath) ? name(newPath) : name(oldPath) + QStringLiteral(" → ") + name(newPath);
			case source_search_folder_role: return folder(oldPath) == folder(newPath) ? folder(newPath) : folder(oldPath) + QStringLiteral(" → ") + folder(newPath);
			case source_search_count_role:  return static_cast<qulonglong>(aGroup.Rows.size());
			case source_search_change_role: return change;
			case Qt::ToolTipRole:
			case Qt::DisplayRole:
			case Qt::AccessibleTextRole:    {
				auto words = (renamed ? oldPath + QStringLiteral(" → ") + newPath : path) + QStringLiteral(", ") + plural(aGroup.Rows.size(), tr("%1 match"), tr("%1 matches"));
				return change.isEmpty() ? words : words + QStringLiteral(", ") + change;
			}
			default: return { };
			}
		}
		auto match_data(const group& aGroup, std::size_t aRow, int aRole) const -> QVariant
		{
			if (aRow >= aGroup.Rows.size() || !Matches || aGroup.Rows[aRow] >= Matches->size()) {
				return { };
			}
			const auto  global = aGroup.Rows[aRow];
			const auto& hit    = (*Matches)[global].Match;
			const auto& left   = hit.Locations[0];
			const auto& right  = hit.Locations[1];
			if (!left && !right) {
				return { };
			}
			// The later (Right) line when both sides carry the match.
			const auto line = [&] { return QString::number((right ? right->Line : left->Line) + 1U); };
			switch (aRole) {
			case source_search_match_role:            return static_cast<unsigned>(global + 1U);
			case source_search_sides_role:            return (left ? 1 : 0) | (right ? 2 : 0);
			case source_search_line_role:             return line();
			case source_search_preview_role:          return snippet_of(hit).Text;
			case source_search_highlight_start_role:  return snippet_of(hit).Start;
			case source_search_highlight_length_role: return snippet_of(hit).Length;
			case Qt::FontRole:                        return SnippetFont;
			case Qt::ToolTipRole:
			case Qt::DisplayRole:
			case Qt::AccessibleTextRole:              {
				auto where = QString{ };
				if (left && right) {
					where = left->Line == right->Line ? tr("Left and Right line %1").arg(right->Line + 1U) : tr("Left %1, Right %2").arg(left->Line + 1U).arg(right->Line + 1U);
				} else {
					where = (left ? tr("Left line %1") : tr("Right line %1")).arg((left ? left->Line : right->Line) + 1U);
				}
				return where + QStringLiteral(": ") + snippet_of(hit).Text;
			}
			default: return { };
			}
		}
	};

	/// File and match rows drawn natively: the style paints selection and
	/// focus, and every colour is a theme role or the view's palette.
	class source_search_result_delegate final : public QStyledItemDelegate
	{
	public:
		struct colours
		{
			QColor Removed;
			QColor Added;
			QColor RemovedSoft;
			QColor AddedSoft;
			QColor Find;
			QColor Text;
		};
		explicit source_search_result_delegate(QObject* aParent)
			: QStyledItemDelegate{ aParent }
		{ }
		void set_colours(colours aColours) { Colours = std::move(aColours); }
		void set_code_font(const QFont& aFont) { CodeFont = aFont; }
		void set_line_digits(int aDigits) { LineDigits = aDigits; }
		/// The fixed side-tag, line-number and snippet columns of a match row.
		/// Only the row's rectangle, the fonts and the widest shown line
		/// number decide them, so every match row shares the same three x ranges.
		[[nodiscard]] auto columns(const QRect& aRow, const QFont& aFont) const -> source_search_match_columns
		{
			const auto area    = aRow.adjusted(4, 0, -6, 0);
			const auto side    = QFontMetrics{ tag_font(aFont) }.height() + 2;
			const auto metrics = QFontMetrics{ CodeFont };
			const auto tags    = QRect{ area.left(), area.top(), side * 2 + 2, area.height() };
			const auto number  = QRect{ tags.right() + 1 + 4, area.top(), metrics.horizontalAdvance(QString(std::max(LineDigits, 3), QLatin1Char('0'))), area.height() };
			const auto snippet = QRect{ number.right() + 1 + metrics.averageCharWidth(), area.top(), 0, area.height() };
			return { tags, number, QRect{ snippet.topLeft(), QPoint{ area.right(), area.bottom() } } };
		}
		auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex&) const -> QSize override
		{
			const auto height = std::max(QFontMetrics{ aOption.font }.height(), QFontMetrics{ CodeFont }.height()) + 8;
			return { 120, height };
		}
		void paint(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const override
		{
			auto option = aOption;
			initStyleOption(&option, aIndex);
			if (!aIndex.data(source_search_file_role).toBool()) {
				option.rect.setLeft(0);
			}
			// The code font reaches the snippet through FontRole; tags, names
			// and column geometry measure the view's own font.
			option.font = aOption.font;
			option.text.clear();
			option.icon        = { };
			const auto* widget = option.widget;
			auto*       style  = widget ? widget->style() : QApplication::style();
			style->drawPrimitive(QStyle::PE_PanelItemViewItem, &option, aPainter, widget);
			const auto selected = (option.state & QStyle::State_Selected) != 0;
			const auto group    = (option.state & QStyle::State_Active) ? QPalette::Active : QPalette::Inactive;
			const auto ink      = option.palette.color(group, selected ? QPalette::HighlightedText : QPalette::Text);
			auto       dim      = selected ? ink : option.palette.color(group, QPalette::PlaceholderText);
			if (selected) {
				dim.setAlphaF(0.78F);
			}
			aPainter->save();
			aPainter->setRenderHint(QPainter::Antialiasing);
			aPainter->setClipRect(option.rect);
			const auto area = option.rect.adjusted(4, 0, -6, 0);
			if (aIndex.data(source_search_file_role).toBool()) {
				paint_file(*aPainter, option, aIndex, area, ink, dim);
			} else {
				paint_match(*aPainter, option, aIndex, area, ink, dim);
			}
			aPainter->restore();
		}

	private:
		colours     Colours;
		QFont       CodeFont;
		int         LineDigits = 1;
		static auto tag_font(const QFont& aFont) -> QFont
		{
			auto font = aFont;
			font.setPointSizeF(aFont.pointSizeF() > 0 ? aFont.pointSizeF() * 0.82 : 9.0);
			font.setWeight(QFont::DemiBold);
			return font;
		}
		/// A rounded tag; its height follows the tag font, so every tag and
		/// pill in the list shares one shape at every scale.
		static auto paint_tag(QPainter& aPainter, const QFont& aFont, const QString& aText, int aRight, int aCentre, const QColor& aFill, const QColor& aInk, int aMinimum, qreal aRadius) -> int
		{
			const auto metrics = QFontMetrics{ aFont };
			const auto height  = metrics.height();
			const auto width   = std::max(aMinimum, metrics.horizontalAdvance(aText) + height / 2 + 2);
			const auto box     = QRect{ aRight - width, aCentre - height / 2, width, height };
			aPainter.setPen(Qt::NoPen);
			aPainter.setBrush(aFill);
			aPainter.drawRoundedRect(QRectF{ box }, aRadius < 0 ? height / 2.0 : aRadius, aRadius < 0 ? height / 2.0 : aRadius);
			aPainter.setFont(aFont);
			aPainter.setPen(aInk);
			aPainter.drawText(box, Qt::AlignCenter, aText);
			return box.left();
		}
		void paint_file(QPainter& aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex, QRect aArea, const QColor& aInk, const QColor& aDim) const
		{
			const auto centre = aArea.center().y();
			const auto small  = tag_font(aOption.font);
			// The text colour, faint: a neutral chip that reads in every theme.
			auto pill = aOption.palette.color(QPalette::Text);
			pill.setAlphaF(0.13F);
			auto right = paint_tag(aPainter, small, aIndex.data(source_search_count_role).toString(), aArea.right(), centre, pill, aInk, 0, -1.0) - 6;
			if (const auto change = aIndex.data(source_search_change_role).toString(); !change.isEmpty()) {
				auto fill = change == tr("added") ? Colours.AddedSoft : change == tr("removed") ? Colours.RemovedSoft
																								: pill;
				right     = paint_tag(aPainter, small, change, right, centre, fill, Colours.Text.isValid() ? Colours.Text : aInk, 0, 3.0) - 6;
			}
			auto name = aOption.font;
			name.setWeight(QFont::Medium);
			const auto text      = aIndex.data(source_search_name_role).toString();
			const auto available = std::max(0, right - aArea.left());
			aPainter.setFont(name);
			// Measure the font the painter resolved, which a missing weight changes.
			const auto nameMetrics = aPainter.fontMetrics();
			const auto nameWidth   = std::min(nameMetrics.horizontalAdvance(text) + 1, available);
			aPainter.setPen(aInk);
			aPainter.drawText(QRect{ aArea.left(), aArea.top(), nameWidth, aArea.height() }, Qt::AlignVCenter | Qt::AlignLeft, nameMetrics.elidedText(text, Qt::ElideRight, nameWidth));
			const auto folder = aIndex.data(source_search_folder_role).toString();
			const auto gap    = nameMetrics.averageCharWidth();
			const auto room   = right - (aArea.left() + nameWidth + gap);
			if (!folder.isEmpty() && room > gap * 8) {
				const auto metrics = QFontMetrics{ aOption.font };
				aPainter.setFont(aOption.font);
				aPainter.setPen(aDim);
				aPainter.drawText(QRect{ aArea.left() + nameWidth + gap, aArea.top(), room, aArea.height() }, Qt::AlignVCenter | Qt::AlignLeft, metrics.elidedText(folder, Qt::ElideMiddle, room));
			}
		}
		void paint_match(QPainter& aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex, QRect aArea, const QColor& aInk, const QColor& aDim) const
		{
			const auto centre = aArea.center().y();
			const auto small  = tag_font(aOption.font);
			const auto column = columns(aOption.rect, aOption.font);
			const auto side   = (column.Tags.width() - 2) / 2;
			const auto sides  = aIndex.data(source_search_sides_role).toInt();
			const auto tagInk = Colours.Text.isValid() ? Colours.Text : aInk;
			// Two fixed slots, Left then Right, so a tag's place repeats its side.
			if (sides & 1) {
				paint_tag(aPainter, small, tr("L"), column.Tags.left() + side, centre, Colours.Removed, tagInk, side, 3.0);
			}
			if (sides & 2) {
				paint_tag(aPainter, small, tr("R"), column.Tags.right() + 1, centre, Colours.Added, tagInk, side, 3.0);
			}
			const auto code    = aIndex.data(Qt::FontRole).value<QFont>();
			const auto metrics = QFontMetrics{ code };
			aPainter.setFont(code);
			aPainter.setPen(aDim);
			aPainter.drawText(column.Number, Qt::AlignVCenter | Qt::AlignRight, aIndex.data(source_search_line_role).toString());
			const auto x      = column.Snippet.left();
			auto       text   = aIndex.data(source_search_preview_role).toString();
			auto       start  = aIndex.data(source_search_highlight_start_role).toInt();
			const auto length = aIndex.data(source_search_highlight_length_role).toInt();
			const auto width  = column.Snippet.width();
			// Leading context gives way so the match stays in view in a narrow drawer.
			if (width > 0 && metrics.horizontalAdvance(text.left(start)) > width / 3 && metrics.horizontalAdvance(text.left(start + length)) > width) {
				auto cut = 0;
				while (cut < start && metrics.horizontalAdvance(text.mid(cut, start - cut)) > width / 4) {
					++cut;
				}
				text  = QChar{ 0x2026 } + text.mid(cut);
				start = start - cut + 1;
			}
			const int  top      = centre - metrics.height() / 2;
			const auto baseline = top + metrics.ascent();
			if (length > 0) {
				// Already composited over the code background, so the match
				// reads in the same amber on a selected row as on the others.
				const auto wash  = Colours.Find;
				const auto left  = x + metrics.horizontalAdvance(text.left(start));
				const auto right = x + metrics.horizontalAdvance(text.left(start + length));
				aPainter.setPen(Qt::NoPen);
				aPainter.setBrush(wash);
				aPainter.drawRoundedRect(QRectF{ static_cast<qreal>(left) - 1.0, static_cast<qreal>(top) - 1.0, static_cast<qreal>(right - left) + 2.0, static_cast<qreal>(metrics.height()) + 2.0 }, 2.0, 2.0);
			}
			aPainter.setPen(aInk);
			aPainter.drawText(QPoint{ x, baseline }, text);
		}
	};

	/// A filter chip: filled in its side tag's colour when on, outlined when
	/// off. A native checkable button, so keyboard and accessibility come with it.
	class side_chip final : public QAbstractButton
	{
	public:
		explicit side_chip(QWidget* aParent)
			: QAbstractButton{ aParent }
		{
			setCheckable(true);
			setFocusPolicy(Qt::StrongFocus);
		}
		std::function<void()> Toggle;
		QColor                Fill;
		QColor                Ink;
		auto                  sizeHint() const -> QSize override
		{
			const auto metrics = fontMetrics();
			return { metrics.horizontalAdvance(text()) + metrics.height() + 6, metrics.height() + 8 };
		}

	protected:
		/// The panel decides through `core::toggled`, which keeps one side on.
		void nextCheckState() override
		{
			if (Toggle) {
				Toggle();
			}
		}
		void paintEvent(QPaintEvent*) override
		{
			auto painter = QPainter{ this };
			painter.setRenderHint(QPainter::Antialiasing);
			const auto box    = QRectF{ rect() }.adjusted(1.5, 1.5, -1.5, -1.5);
			const auto radius = box.height() / 2.0;
			const auto active = isEnabled() ? QPalette::Active : QPalette::Disabled;
			if (isChecked()) {
				painter.setPen(Qt::NoPen);
				painter.setBrush(Fill);
				painter.drawRoundedRect(box, radius, radius);
				painter.setPen(Ink);
			} else {
				painter.setPen(QPen{ palette().color(active, QPalette::Mid), 1.0 });
				painter.setBrush(Qt::NoBrush);
				painter.drawRoundedRect(box, radius, radius);
				painter.setPen(palette().color(active, QPalette::PlaceholderText));
			}
			if (hasFocus()) {
				painter.setBrush(Qt::NoBrush);
				painter.setPen(QPen{ palette().color(QPalette::Active, QPalette::Highlight), 2.0 });
				painter.drawRoundedRect(QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0), radius + 0.5, radius + 0.5);
				painter.setPen(isChecked() ? Ink : palette().color(active, QPalette::PlaceholderText));
			}
			auto label = font();
			label.setWeight(isChecked() ? QFont::DemiBold : QFont::Normal);
			painter.setFont(label);
			painter.drawText(rect(), Qt::AlignCenter, text());
		}
	};

	namespace {
		auto collapse_icon(const QPalette& aPalette) -> QIcon
		{
			return painted_glyph_icon([](QPainter& aPainter, const qreal aSide, const QColor& aInk) {
				aPainter.setPen(QPen{ aInk, aSide / 12.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
				aPainter.setBrush(Qt::NoBrush);
				aPainter.drawRoundedRect(QRectF{ aSide * 0.18, aSide * 0.18, aSide * 0.64, aSide * 0.64 }, aSide * 0.08, aSide * 0.08);
				aPainter.drawLine(QPointF{ aSide * 0.34, aSide * 0.5 }, QPointF{ aSide * 0.66, aSide * 0.5 });
			},
									  aPalette.color(QPalette::Active, QPalette::ButtonText), aPalette.color(QPalette::Disabled, QPalette::ButtonText));
		}
	}

	source_search_panel::source_search_panel(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("sourceSearchPanel"));
		setAccessibleName(tr("Find in Files"));
		auto* column = new QVBoxLayout{ this };
		column->setContentsMargins(6, 4, 4, 2);
		column->setSpacing(4);
		Bar = new source_find_bar{ this, true };
		Bar->show();
		column->addWidget(Bar);
		Bar->Dispatch = [this](auto aGesture) {if(Dispatch){Dispatch(aGesture);} };

		auto* controls = new QHBoxLayout{ };
		controls->setContentsMargins(0, 0, 0, 0);
		controls->setSpacing(0);
		controls->setSpacing(4);
		const auto chip = [&](core::diff_side aSide, const QString& aName, const QString& aText, const QString& aSpoken, const QString& aTip) {
			auto* button = new side_chip{ this };
			button->setObjectName(aName);
			button->setText(aText);
			button->setAccessibleName(aSpoken);
			button->setToolTip(aTip);
			button->setChecked(true);
			button->Toggle = [this, aSide] { set_side_filter(core::toggled(side_filter(), aSide)); };
			controls->addWidget(button);
			return button;
		};
		Sides[0] = chip(core::diff_side::left, QStringLiteral("sourceSearchSideLeft"), tr("Left"), tr("Left side"), tr("Show matches on the left side (at least one side stays on)"));
		Sides[1] = chip(core::diff_side::right, QStringLiteral("sourceSearchSideRight"), tr("Right"), tr("Right side"), tr("Show matches on the right side (at least one side stays on)"));
		controls->addStretch(1);
		CollapseAll = new QToolButton{ this };
		CollapseAll->setObjectName(QStringLiteral("sourceSearchCollapseAll"));
		CollapseAll->setAccessibleName(tr("Collapse all files"));
		CollapseAll->setToolTip(tr("Collapse all files"));
		CollapseAll->setAutoRaise(true);
		CollapseAll->setFocusPolicy(Qt::StrongFocus);
		controls->addWidget(CollapseAll);
		controls->addSpacing(2);
		Details = new QToolButton{ this };
		Details->setObjectName(QStringLiteral("sourceSearchDetailsToggle"));
		Details->setText(QStringLiteral("…"));
		Details->setAccessibleName(tr("Toggle search details"));
		Details->setToolTip(tr("Files to include and exclude, and the exact commits"));
		Details->setCheckable(true);
		Details->setAutoRaise(true);
		Details->setFocusPolicy(Qt::StrongFocus);
		controls->addWidget(Details);
		column->addLayout(controls);

		DetailsBody  = new QWidget{ this };
		auto* fields = new QVBoxLayout{ DetailsBody };
		fields->setContentsMargins(0, 0, 0, 2);
		fields->setSpacing(2);
		const auto field = [&](const QString& aLabel, const QString& aName) {auto* label=new QLabel{aLabel,DetailsBody};auto* editor=new QLineEdit{DetailsBody}; editor->setObjectName(aName);editor->setAccessibleName(aLabel);editor->setPlaceholderText(tr("e.g. src/**/*.cpp"));editor->setAccessibleDescription(tr("Case-sensitive repository paths; comma alternatives; *, ?, **, braces and character classes; exclude wins."));label->setBuddy(editor);fields->addWidget(label);fields->addWidget(editor);return editor; };
		Include          = field(tr("Files to include"), QStringLiteral("sourceFilesInclude"));
		Exclude          = field(tr("Files to exclude"), QStringLiteral("sourceFilesExclude"));
		Scope            = new QLabel{ DetailsBody };
		Scope->setAccessibleName(tr("Pull request and exact comparison"));
		Scope->setTextFormat(Qt::PlainText);
		Scope->setWordWrap(true);
		Scope->setForegroundRole(QPalette::PlaceholderText);
		fields->addWidget(Scope);
		Identity = new QLabel{ DetailsBody };
		Identity->setAccessibleName(tr("Exact source commit identities"));
		Identity->setTextFormat(Qt::PlainText);
		Identity->setWordWrap(true);
		Identity->setTextInteractionFlags(Qt::TextSelectableByMouse);
		Identity->setForegroundRole(QPalette::PlaceholderText);
		fields->addWidget(Identity);
		DetailsBody->hide();
		column->addWidget(DetailsBody);
		connect(Details, &QToolButton::toggled, DetailsBody, &QWidget::setVisible);

		Coverage = new QLabel{ this };
		Coverage->setObjectName(QStringLiteral("sourceSearchCoverage"));
		Coverage->setTextFormat(Qt::PlainText);
		Coverage->setWordWrap(true);
		Coverage->setAccessibleName(tr("Search coverage"));
		column->addWidget(Coverage);
		Hint = new QLabel{ this };
		Hint->setObjectName(QStringLiteral("sourceSearchHint"));
		Hint->setTextFormat(Qt::PlainText);
		Hint->setWordWrap(true);
		Hint->setForegroundRole(QPalette::PlaceholderText);
		Hint->hide();
		column->addWidget(Hint);

		auto* skippedRow = new QHBoxLayout{ };
		skippedRow->setContentsMargins(0, 0, 0, 0);
		skippedRow->setSpacing(4);
		SkippedToggle = new QToolButton{ this };
		SkippedToggle->setObjectName(QStringLiteral("sourceSearchSkippedToggle"));
		SkippedToggle->setCheckable(true);
		SkippedToggle->setAutoRaise(true);
		SkippedToggle->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
		SkippedToggle->setArrowType(Qt::RightArrow);
		SkippedToggle->setAccessibleDescription(tr("Shows why each side was not searched"));
		SkippedToggle->hide();
		skippedRow->addWidget(SkippedToggle);
		Retry = new QToolButton{ this };
		Retry->setText(tr("Retry"));
		Retry->setAccessibleName(tr("Retry unavailable reads"));
		Retry->setToolTip(tr("Retry unavailable reads"));
		Retry->setObjectName(QStringLiteral("sourceSearchRetry"));
		skippedRow->addWidget(Retry);
		skippedRow->addStretch(1);
		Retry->hide();
		column->addLayout(skippedRow);
		Skipped = new QLabel{ this };
		Skipped->setTextFormat(Qt::PlainText);
		Skipped->setWordWrap(true);
		Skipped->setForegroundRole(QPalette::PlaceholderText);
		Skipped->hide();
		column->addWidget(Skipped);
		connect(SkippedToggle, &QToolButton::toggled, this, [this](bool aShown) {Skipped->setVisible(aShown);SkippedToggle->setArrowType(aShown?Qt::DownArrow:Qt::RightArrow); });
		connect(Retry, &QToolButton::clicked, this, [this] {if(Dispatch){Dispatch(source_find_gesture::selection);} });

		Tree = new source_search_result_tree{ this };
		Tree->setObjectName(QStringLiteral("sourceSearchResults"));
		Tree->setAccessibleName(tr("Search results by file, source side, line and column"));
		Tree->setHeaderHidden(true);
		Tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
		Tree->setUniformRowHeights(true);
		Tree->setTextElideMode(Qt::ElideNone);
		Tree->setIndentation(14);
		Tree->setFrameShape(QFrame::NoFrame);
		Tree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Tree->installEventFilter(this);
		Model = new source_search_result_model{ Tree };
		Tree->setModel(Model);
		Delegate = new source_search_result_delegate{ Tree };
		Tree->setItemDelegate(Delegate);
		adopt_appearance(appearance_profile{ });
		connect(Tree->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& aIndex, const QModelIndex&) {const auto index=aIndex.data(source_search_match_role).toUInt();if(index && Select){Select(index-1U);} });
		column->addWidget(Tree, 1);
		connect(Tree, &QTreeView::clicked, this, [this](const QModelIndex& aIndex) {const auto index=aIndex.data(source_search_match_role).toUInt();if(index && Activate){Activate(index-1U);} });
		connect(CollapseAll, &QToolButton::clicked, Tree, &QTreeView::collapseAll);
		Debounce = new QTimer{ this };
		Debounce->setSingleShot(true);
		Debounce->setInterval(150);
		connect(Debounce, &QTimer::timeout, this, [this] { Bar->flush(); });
		for (auto* editor : { Include, Exclude }) {
			editor->installEventFilter(this);
			connect(editor, &QLineEdit::textChanged, this, [this] {if(Dispatch){Dispatch(source_find_gesture::query_dirty);}Debounce->start(); });
		}
		refresh_icons();
		present_summary();
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
					const auto index = Tree->currentIndex().data(source_search_match_role).toUInt();
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
	void source_search_panel::changeEvent(QEvent* aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange) {
			refresh_icons();
		}
	}
	void source_search_panel::refresh_icons() { CollapseAll->setIcon(collapse_icon(palette())); }
	void source_search_panel::adopt_appearance(const appearance_profile& aProfile)
	{
		using role = core::appearance_role;
		Bar->adopt_appearance(aProfile);
		Delegate->set_colours({ aProfile.color(role::diff_intraline_removed_background), aProfile.color(role::diff_intraline_added_background), aProfile.color(role::diff_removed_background), aProfile.color(role::diff_added_background), to_presentation_color(core::composited(aProfile.palette()[core::index_of(role::find_match_background)], aProfile.palette()[core::index_of(role::code_background)], core::selection_wash_alpha)), aProfile.color(role::application_text) });
		Tree->viewport()->update();
		for (std::size_t i = 0; i < 2U; ++i) {
			auto* chip = static_cast<side_chip*>(Sides[i]);
			chip->Fill = aProfile.color(i == 0U ? role::diff_intraline_removed_background : role::diff_intraline_added_background);
			chip->Ink  = aProfile.color(role::application_text);
			chip->update();
		}
	}
	void source_search_panel::append_results(const application::source_search_projection& aProjection)
	{
		const auto clear  = Projection.Scope != aProjection.Scope || Projection.Query != aProjection.Query || Projection.Options != aProjection.Options || Projection.Include != aProjection.Include || Projection.Exclude != aProjection.Exclude;
		const auto before = clear ? 0 : Model->rowCount();
		Model->present(aProjection, clear);
		for (int row = before; row < Model->rowCount(); ++row) {
			Tree->setExpanded(Model->index(row, 0), true);
		}
		Delegate->set_line_digits(Model->line_digits());
	}
	void source_search_panel::present_summary()
	{
		const auto& p       = Projection;
		const auto  matches = Model->shown_matches();
		const auto  files   = Model->shown_files();
		const auto  counted = plural(matches, tr("%1 match"), tr("%1 matches")) + QStringLiteral(" ") + plural(files, tr("in %1 file"), tr("in %1 files"));
		auto        words   = counted;
		auto        hint    = QString{ };
		const auto  filter  = side_filter();
		const auto  all     = filter == core::source_side_filter{ };
		const auto  side    = filter.Left ? tr("left") : tr("right");
		switch (p.Status) {
		case core::source_search_status::idle:                words = tr("Type to search every changed file on both sides."); break;
		case core::source_search_status::searching:           words = matches ? tr("Searching… %1 so far").arg(counted) : tr("Searching…"); break;
		case core::source_search_status::invalid_pattern:     words = tr("Invalid regular expression"); break;
		case core::source_search_status::unsupported_pattern: words = tr("Unsupported regular expression"); break;
		case core::source_search_status::invalid_input:       words = tr("Query exceeds 4 KiB or is invalid text"); break;
		case core::source_search_status::unavailable:         words = tr("Source unavailable"); break;
		case core::source_search_status::failure:             words = tr("Search failed; edit the query to retry"); break;
		case core::source_search_status::resource_limit:      words = tr("Search resource limit; narrow the query"); break;
		case core::source_search_status::identity_exhausted:  words = tr("Search unavailable; reopen the pull request"); break;
		case core::source_search_status::cancelled:           words = tr("Search stopped"); break;
		case core::source_search_status::result_limit:        words = tr("%1; limit reached, narrow the query or files").arg(counted); break;
		default:                                              break;
		}
		if (p.NoResults) {
			words = plural(p.Summary.EligibleFiles, tr("No matches in %1 file."), tr("No matches in %1 files."));
			hint  = all ? tr("Try fewer file filters, or turning off Match Case.") : tr("Try both sides, fewer file filters, or turning off Match Case.");
		} else if (!all && matches == 0U && p.Summary.Matches > 0U && p.Status != core::source_search_status::searching) {
			words = tr("No %1 side matches.").arg(side);
			hint  = plural(p.Summary.Matches, tr("Turn on both sides to see %1 match."), tr("Turn on both sides to see all %1 matches."));
		}
		if (p.Wrapped) {
			words += tr("; wrapped");
		}
		Coverage->setText(words);
		Hint->setText(hint);
		Hint->setVisible(!hint.isEmpty());
		auto qualifier = QString{ };
		if (p.Summary.SkippedSides) {
			qualifier = plural(p.Summary.SkippedSides, tr("%1 side skipped"), tr("%1 sides skipped"));
		}
		if (p.Status != core::source_search_status::searching && p.Status != core::source_search_status::idle && p.Summary.SearchedFiles < p.Summary.EligibleFiles) {
			const auto searched = tr("%1 of %2 files searched").arg(p.Summary.SearchedFiles).arg(p.Summary.EligibleFiles);
			qualifier           = qualifier.isEmpty() ? searched : qualifier + QStringLiteral("; ") + searched;
		}
		SkippedToggle->setText(qualifier);
		SkippedToggle->setAccessibleName(qualifier);
		SkippedToggle->setVisible(!qualifier.isEmpty() && !Skipped->text().isEmpty());
		if (!SkippedToggle->isVisible() && !qualifier.isEmpty()) {
			words += QStringLiteral("; ") + qualifier;
			Coverage->setText(words);
		}
		Coverage->setToolTip(Scope->text());
		const auto description = Scope->text() + QStringLiteral(". ") + words + (hint.isEmpty() ? QString{ } : QStringLiteral(" ") + hint) + (qualifier.isEmpty() ? QString{ } : QStringLiteral(". ") + qualifier);
		Coverage->setAccessibleDescription(description);
		if (LastAnnouncement != description) {
			LastAnnouncement  = description;
			auto announcement = QAccessibleAnnouncementEvent{ Coverage, description };
			announcement.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&announcement);
		}
	}
	void source_search_panel::present(const application::source_search_projection& aProjection)
	{
		const auto previous = Projection.CurrentMatch;
		append_results(aProjection);
		Projection = aProjection;
		Bar->present(aProjection);
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
		Skipped->setText(skipped.trimmed());
		if (skipped.isEmpty() && SkippedToggle->isChecked()) {
			SkippedToggle->setChecked(false);
		}
		Retry->setVisible(aProjection.RetryAvailable);
		present_summary();
		if (Changed) {
			Changed();
		}
		if (aProjection.CurrentMatch != previous || !Tree->currentIndex().isValid()) {
			select_current();
		}
	}
	void source_search_panel::select_current()
	{
		if (!Projection.CurrentMatch) {
			return;
		}
		const auto index = Model->match_index(*Projection.CurrentMatch);
		if (index.isValid()) {
			const auto blocked = QSignalBlocker{ Tree->selectionModel() };
			Tree->setCurrentIndex(index);
		}
	}
	void source_search_panel::set_side_filter(core::source_side_filter aFilter)
	{
		if (!aFilter.Left && !aFilter.Right) {
			return;
		}
		Sides[0]->setChecked(aFilter.Left);
		Sides[1]->setChecked(aFilter.Right);
		if (Model->filter() == aFilter) {
			return;
		}
		Model->set_filter(aFilter);
		Delegate->set_line_digits(Model->line_digits());
		for (int row = 0; row < Model->rowCount(); ++row) {
			Tree->setExpanded(Model->index(row, 0), true);
		}
		present_summary();
		if (SideFilterChanged) {
			SideFilterChanged(aFilter);
		}
		select_current();
	}
	auto source_search_panel::side_filter() const -> core::source_side_filter { return { .Left = Sides[0]->isChecked(), .Right = Sides[1]->isChecked() }; }
	void source_search_panel::set_snippet_font(const QFont& aFont)
	{
		Model->set_snippet_font(aFont);
		Delegate->set_code_font(aFont);
	}
	void source_search_panel::set_scope_labels(QString aLabel, QString aIdentity)
	{
		Scope->setText(std::move(aLabel));
		Identity->setText(std::move(aIdentity));
		Coverage->setToolTip(Scope->text());
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
		set_side_filter({ });
		present({ });
		set_scope_labels({ }, { });
	}
	auto source_search_panel::bar() const -> source_find_bar* { return Bar; }
	auto source_search_panel::include() const -> std::string { return Include->text().toUtf8().toStdString(); }
	auto source_search_panel::exclude() const -> std::string { return Exclude->text().toUtf8().toStdString(); }
	auto source_search_panel::summary_text() const -> QString { return Coverage->text(); }
	auto source_search_panel::projection() const -> const application::source_search_projection& { return Projection; }
	auto source_search_panel::tree() const -> QTreeView* { return Tree; }
	auto source_search_panel::match_columns(const QModelIndex& aIndex) const -> source_search_match_columns
	{
		if (!aIndex.isValid() || aIndex.data(source_search_file_role).toBool()) {
			return { };
		}
		return Delegate->columns(Tree->visualRect(aIndex), Tree->font());
	}
}
