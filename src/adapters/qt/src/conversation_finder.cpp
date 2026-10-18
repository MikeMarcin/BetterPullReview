#include "bpr/qt/conversation_finder.hpp"

#include "bpr/core/feedback_convention.hpp"
#include "bpr/core/feedback_conversation.hpp"

#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"

#include "conversation_glyphs.hpp"
#include "conversation_open_card.hpp"

#include <QAbstractButton>
#include <QAbstractListModel>
#include <QAccessible>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QContextMenuEvent>
#include <QEvent>
#include <QFocusEvent>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPushButton>
#include <QScreen>
#include <QScrollBar>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTextLayout>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace bpr::qt {
	namespace {
		/// The prototype's measures (`docs/design/BPR-1290-inline-drawer-v2`).
		constexpr int row_height     = 30;
		constexpr int group_gap      = 6;
		constexpr int list_left      = 6;
		constexpr int list_right     = 8;
		constexpr int line_width     = 46;
		constexpr int line_gap       = 10;
		constexpr int level_width    = 18;
		constexpr int mark_size      = 16;
		constexpr int mark_gap       = 9;
		constexpr int people_gap     = 14;
		constexpr int bead_size      = 16;
		constexpr int bead_gap       = 3;
		constexpr int state_width    = 30;
		constexpr int row_padding    = 12;
		constexpr int search_maximum = 520;
		constexpr int chip_height    = 30;
		constexpr int preview_width  = 340;
		/// [[BPR-1356]]. The row's Discuss button: its height, the room either
		/// side of its word, and the gap that keeps the words off it.
		constexpr int         discuss_height  = 20;
		constexpr int         discuss_padding = 9;
		constexpr int         discuss_gap     = 8;
		constexpr std::size_t maximum_beads   = 9U;
		constexpr std::size_t preview_lines   = 4U;
		constexpr auto        draft_identity  = std::string_view{ "draft:unpublished" };
		constexpr auto        file_key_prefix = std::string_view{ "file:" };
		constexpr auto        row_key_prefix  = std::string_view{ "row:" };

		[[nodiscard]] auto blend(const QColor& aFrom, const QColor& aTo, const qreal aAmount) -> QColor
		{
			const auto channel = [aAmount](const int aA, const int aB) {
				return static_cast<int>(std::lround(aA + (aB - aA) * aAmount));
			};
			return QColor{ channel(aFrom.red(), aTo.red()), channel(aFrom.green(), aTo.green()), channel(aFrom.blue(), aTo.blue()) };
		}

		[[nodiscard]] auto alpha(QColor aColour, const qreal aAlpha) -> QColor
		{
			aColour.setAlphaF(static_cast<float>(aAlpha));
			return aColour;
		}

		[[nodiscard]] auto weighted(QFont aFont, const QFont::Weight aWeight) -> QFont
		{
			aFont.setWeight(aWeight);
			return aFont;
		}

		[[nodiscard]] auto sized(QFont aFont, const qreal aFactor) -> QFont
		{
			if (aFont.pointSizeF() > 0.0) {
				aFont.setPointSizeF(aFont.pointSizeF() * aFactor);
			} else if (aFont.pixelSize() > 0) {
				aFont.setPixelSize(std::max(1, static_cast<int>(std::lround(aFont.pixelSize() * aFactor))));
			}
			return aFont;
		}

		/// The code font at a share of the widget's size, for line numbers and
		/// code in a row's words.
		[[nodiscard]] auto code_font_like(const QFont& aCode, const QFont& aBase, const qreal aFactor) -> QFont
		{
			auto font = aCode.family().isEmpty() ? QFontDatabase::systemFont(QFontDatabase::FixedFont) : aCode;
			if (aBase.pointSizeF() > 0.0) {
				font.setPointSizeF(aBase.pointSizeF() * aFactor);
			} else if (aBase.pixelSize() > 0) {
				font.setPixelSize(std::max(1, static_cast<int>(std::lround(aBase.pixelSize() * aFactor))));
			}
			return font;
		}

		/// The signal's soft fill: a checked chip.
		[[nodiscard]] auto signal_soft(const note_colours& aColours) -> QColor
		{
			return blend(aColours.Signal, aColours.Panel, aColours.Dark ? 0.72 : 0.86);
		}

		[[nodiscard]] auto base_name(const QString& aPath) -> QString
		{
			const auto slash = aPath.lastIndexOf(QLatin1Char{ '/' });
			return slash < 0 ? aPath : aPath.mid(slash + 1);
		}

		[[nodiscard]] auto folder_of(const QString& aPath) -> QString
		{
			const auto slash = aPath.lastIndexOf(QLatin1Char{ '/' });
			return slash < 0 ? QString{ } : aPath.left(slash + 1);
		}

		/// The mark a body carries and how much of it its header takes, read
		/// as the drawer reads it ([[BPR-1083]], [[BPR-1086]]).
		struct body_reading
		{
			std::optional<core::feedback_label> Label;
			feedback_mark_emphasis              Emphasis;
			qsizetype                           Header = 0;
		};

		[[nodiscard]] auto read_body(const std::string_view aBody) -> body_reading
		{
			const auto segment    = core::published_feedback_segment(aBody);
			const auto annotation = core::recognize_feedback_body(aBody, segment);
			const auto recognized = annotation.Status == core::feedback_prefix_status::recognized && annotation.Label.has_value()
				&& annotation.Subject.has_value();
			// [[BPR-1086]]. A published tour stop's words start at its
			// narrative, after its pinned source and generated heading.
			const auto narrative = segment.Managed && segment.UserBody.has_value() ? feedback_text_index(aBody, segment.UserBody->Begin) : 0;
			if (!recognized) {
				return { .Header = narrative };
			}
			return {
				.Label    = annotation.Label,
				.Emphasis = feedback_mark_emphasis_of(annotation),
				.Header   = annotation.Header.has_value() ? feedback_text_index(aBody, annotation.Header->End) : narrative,
			};
		}

		[[nodiscard]] auto make_post(std::string aId, const QString& aAuthor, const std::string_view aBody, const QString& aViewer) -> finder_post
		{
			const auto reading = read_body(aBody);
			auto       post    = finder_post{ };
			post.Id            = std::move(aId);
			post.Author        = aAuthor;
			post.Body          = QString::fromUtf8(aBody.data(), static_cast<qsizetype>(aBody.size()));
			post.Words         = post.Body.mid(std::clamp<qsizetype>(reading.Header, 0, post.Body.size())).simplified();
			if (post.Words.isEmpty()) {
				post.Words = post.Body.simplified();
			}
			post.Label    = reading.Label;
			post.Emphasis = reading.Emphasis;
			post.Mine     = !aViewer.isEmpty() && aAuthor == aViewer;
			return post;
		}

		[[nodiscard]] auto who(const finder_post& aPost) -> QString
		{
			return aPost.Mine ? QStringLiteral("you") : aPost.Author;
		}

		[[nodiscard]] auto first_words(const QString& aWords, const int aCharacters) -> QString
		{
			auto words = aWords;
			words.remove(QLatin1Char{ '`' });
			return words.size() <= aCharacters ? words : words.left(aCharacters - 1).trimmed() + QChar{ 0x2026 };
		}

		/// The reading order of a conversation: each branch whole, in the
		/// order it started, as the drawer reads it.
		[[nodiscard]] auto reading_order(const std::vector<finder_post>& aPosts) -> std::vector<std::size_t>
		{
			auto children = std::vector<std::vector<std::size_t>>(aPosts.size());
			for (std::size_t index = 1U; index < aPosts.size(); ++index) {
				const auto parent = aPosts[index].Parent.value_or(0U);
				children[parent < index ? parent : 0U].push_back(index);
			}
			auto order = std::vector<std::size_t>{ };
			auto stack = std::vector<std::size_t>{ };
			if (!aPosts.empty()) {
				stack.push_back(0U);
			}
			while (!stack.empty()) {
				const auto next = stack.back();
				stack.pop_back();
				order.push_back(next);
				for (auto child = children[next].rbegin(); child != children[next].rend(); ++child) {
					stack.push_back(*child);
				}
			}
			return order;
		}

		/// One run of a row's words: plain, code, or the search's match.
		struct word_run
		{
			QString Text;
			bool    Code  = false;
			bool    Match = false;
		};

		[[nodiscard]] auto runs_of(const QString& aWords, const QString& aSearch) -> std::vector<word_run>
		{
			// Backticks set code in the code font and are not shown, as the
			// prototype sets them.
			auto runs = std::vector<word_run>{ };
			auto code = false;
			auto text = QString{ };
			for (const auto character : aWords) {
				if (character == QLatin1Char{ '`' }) {
					if (!text.isEmpty()) {
						runs.push_back({ .Text = text, .Code = code });
						text.clear();
					}
					code = !code;
					continue;
				}
				text.append(character);
			}
			if (!text.isEmpty()) {
				runs.push_back({ .Text = text, .Code = code });
			}
			if (aSearch.isEmpty()) {
				return runs;
			}
			// The first match is marked, across runs.
			auto joined = QString{ };
			for (const auto& run : runs) {
				joined += run.Text;
			}
			const auto at = joined.indexOf(aSearch, 0, Qt::CaseInsensitive);
			if (at < 0) {
				return runs;
			}
			const auto end    = at + aSearch.size();
			auto       marked = std::vector<word_run>{ };
			auto       offset = qsizetype{ 0 };
			for (const auto& run : runs) {
				const auto from = offset;
				const auto to   = offset + run.Text.size();
				offset          = to;
				if (to <= at || from >= end) {
					marked.push_back(run);
					continue;
				}
				const auto left  = std::max(at, from) - from;
				const auto right = std::min(end, to) - from;
				if (left > 0) {
					marked.push_back({ .Text = run.Text.left(left), .Code = run.Code });
				}
				marked.push_back({ .Text = run.Text.mid(left, right - left), .Code = run.Code, .Match = true });
				if (right < run.Text.size()) {
					marked.push_back({ .Text = run.Text.mid(right), .Code = run.Code });
				}
			}
			return marked;
		}
	} // namespace

	auto finder_rows_of(const std::vector<finder_post>& aPosts) -> std::vector<finder_row>
	{
		auto rows = std::vector<finder_row>{ };
		if (aPosts.empty()) {
			return rows;
		}
		// Every post but the first answers an earlier post; one that names
		// none, or a later one, answers the first. So the shape is a tree.
		auto children = std::vector<std::vector<std::size_t>>(aPosts.size());
		for (std::size_t index = 1U; index < aPosts.size(); ++index) {
			const auto parent = aPosts[index].Parent.value_or(0U);
			children[parent < index ? parent : 0U].push_back(index);
		}
		const auto answered = [&children](const std::size_t aPost) { return !children[aPost].empty(); };
		struct pending
		{
			std::size_t                   Start = 0U;
			std::size_t                   Level = 0U;
			std::vector<finder_connector> Connectors;
		};
		// Depth first with an explicit stack, so a hostile depth costs memory,
		// never the call stack; each row is followed by the rows under it.
		auto stack = std::vector<pending>{ pending{ } };
		while (!stack.empty()) {
			auto next = std::move(stack.back());
			stack.pop_back();
			auto row       = finder_row{ };
			row.Level      = next.Level;
			row.Connectors = next.Connectors;
			row.Posts.push_back(next.Start);
			auto current = next.Start;
			auto forks   = std::vector<std::size_t>{ };
			for (;;) {
				forks.clear();
				for (const auto child : children[current]) {
					if (answered(child)) {
						forks.push_back(child);
					} else {
						row.Posts.push_back(child);
					}
				}
				if (forks.size() == 1U) {
					row.Posts.push_back(forks.front());
					current = forks.front();
					continue;
				}
				break;
			}
			row.Forks = forks.size() > 1U;
			std::ranges::sort(row.Posts);
			rows.push_back(std::move(row));
			if (forks.size() > 1U) {
				auto inherited = next.Connectors;
				for (auto& connector : inherited) {
					connector = connector == finder_connector::tee ? finder_connector::pipe
						: connector == finder_connector::end       ? finder_connector::blank
																   : connector;
				}
				for (auto fork = forks.size(); fork-- > 0U;) {
					auto connectors = inherited;
					connectors.push_back(fork + 1U < forks.size() ? finder_connector::tee : finder_connector::end);
					stack.push_back({ .Start = forks[fork], .Level = next.Level + 1U, .Connectors = std::move(connectors) });
				}
			}
		}
		return rows;
	}

	auto finder_conversations_of(const std::vector<application::github_conversation_projection>& aForest,
								 const std::vector<thread_read_state>& aReadState, const bool aReadStateKnown,
								 const QString& aViewer) -> std::vector<finder_conversation>
	{
		// A root is the topmost node present: a parent that is absent makes
		// its child a root, and the walk is bounded so no cycle can spin.
		auto byId = std::unordered_map<std::string_view, std::size_t>{ };
		byId.reserve(aForest.size());
		for (std::size_t index = 0U; index < aForest.size(); ++index) {
			byId.emplace(aForest[index].Id, index);
		}
		auto members = std::vector<std::vector<std::size_t>>(aForest.size());
		for (std::size_t index = 0U; index < aForest.size(); ++index) {
			auto current = index;
			for (std::size_t steps = 0U; aForest[current].ParentId.has_value() && steps < aForest.size(); ++steps) {
				const auto parent = byId.find(*aForest[current].ParentId);
				if (parent == byId.end() || parent->second == current) {
					break;
				}
				current = parent->second;
			}
			members[current].push_back(index);
		}
		auto unread = std::unordered_map<std::string_view, const thread_read_state*>{ };
		for (const auto& state : aReadState) {
			unread.emplace(state.Thread, &state);
		}
		auto result = std::vector<finder_conversation>{ };
		for (std::size_t root = 0U; root < aForest.size(); ++root) {
			const auto& node = aForest[root];
			if (members[root].empty() || node.Deleted) {
				continue;
			}
			auto conversation     = finder_conversation{ };
			conversation.Id       = node.Id;
			conversation.Path     = QString::fromStdString(node.Path);
			conversation.Line     = node.Line.value_or(node.OriginalLine.value_or(0U));
			conversation.Resolved = node.Resolved;
			const auto* state     = aReadStateKnown && unread.contains(node.Id) ? unread.at(node.Id) : nullptr;
			auto        indexOf   = std::unordered_map<std::string_view, std::size_t>{ };
			for (const auto member : members[root]) {
				const auto& post = aForest[member];
				// A native thread's root holds the thread, not a post.
				if (post.Kind == application::github_conversation_kind::review_thread || post.Deleted) {
					continue;
				}
				const auto& body  = post.ReviewerProse.has_value() ? *post.ReviewerProse : post.Body;
				auto        shown = make_post(post.Id, QString::fromStdString(post.AuthorLogin), body, aViewer);
				if (post.ParentId.has_value()) {
					if (const auto parent = indexOf.find(*post.ParentId); parent != indexOf.end()) {
						shown.Parent = parent->second;
					}
				}
				shown.Unread = state != nullptr && std::ranges::find(state->UnreadPosts, post.Id) != state->UnreadPosts.end();
				indexOf.emplace(post.Id, conversation.Posts.size());
				conversation.Posts.push_back(std::move(shown));
			}
			if (conversation.Posts.empty()) {
				continue;
			}
			// The first post has no parent; every other post answers one.
			conversation.Posts.front().Parent.reset();
			conversation.Unread = static_cast<std::size_t>(std::ranges::count_if(conversation.Posts, &finder_post::Unread));
			conversation.Rows   = finder_rows_of(conversation.Posts);
			result.push_back(std::move(conversation));
		}
		std::ranges::stable_sort(result, [](const finder_conversation& aLeft, const finder_conversation& aRight) {
			const auto left  = aLeft.Path.toStdString();
			const auto right = aRight.Path.toStdString();
			return left.empty() != right.empty() ? !left.empty() : (left != right ? left < right : aLeft.Line < aRight.Line);
		});
		return result;
	}

	auto finder_post_found(const finder_post& aPost, const QString& aSearch) -> bool
	{
		if (aSearch.isEmpty()) {
			return false;
		}
		return aPost.Body.contains(aSearch, Qt::CaseInsensitive) || aPost.Author.contains(aSearch, Qt::CaseInsensitive)
			|| (aPost.Mine && QStringLiteral("you").contains(aSearch, Qt::CaseInsensitive));
	}

	/// \brief One item the list can show: a file's header or a conversation's row.
	struct conversation_finder::item
	{
		bool                Header = false;
		QString             Path;
		core::file_identity File{ };
		std::size_t         Conversation = 0U;
		std::size_t         Row          = 0U;
		std::string         Key;
	};

	struct conversation_finder::bead
	{
		std::size_t Post = 0U;
	};

	struct conversation_finder::row_geometry
	{
		QRect              Box;
		QRect              Line;
		QRect              Connectors;
		QRect              Mark;
		QRect              Words;
		QRect              Fold;
		std::vector<QRect> Beads;
		QRect              State;
		/// [[BPR-1356]]. The Discuss button, while it shows.
		QRect Discuss;
		/// [[BPR-1582]]. The Review button, left of the Discuss button.
		QRect Review;
	};

	/// \brief The shown subsequence of the finder's items, applied by
	/// identity: a filter removes and inserts rows, never resets the list.
	class finder_model final : public QAbstractListModel
	{
	public:
		explicit finder_model(conversation_finder& aOwner)
			: QAbstractListModel{ &aOwner }
			, Owner{ aOwner }
		{
		}

		[[nodiscard]] auto rowCount(const QModelIndex& aParent = { }) const -> int override
		{
			return aParent.isValid() ? 0 : static_cast<int>(Shown.size());
		}

		[[nodiscard]] auto data(const QModelIndex& aIndex, const int aRole) const -> QVariant override
		{
			if (!aIndex.isValid() || static_cast<std::size_t>(aIndex.row()) >= Shown.size()) {
				return { };
			}
			const auto item = Shown[static_cast<std::size_t>(aIndex.row())];
			switch (aRole) {
			case Qt::DisplayRole:
			case Qt::AccessibleTextRole:
				return Owner.accessible_text(item);
			case Qt::ToolTipRole:
				return Owner.Items[item]->Header ? QVariant{ Owner.Items[item]->Path } : QVariant{ };
			default:
				return { };
			}
		}

		/// \brief Shows exactly these items, which are in order.
		void apply(const std::vector<std::size_t>& aShown, const bool aReset)
		{
			if (aReset) {
				beginResetModel();
				Shown = aShown;
				endResetModel();
				return;
			}
			auto at   = std::size_t{ 0U };
			auto next = std::size_t{ 0U };
			while (at < Shown.size() || next < aShown.size()) {
				if (next == aShown.size() || (at < Shown.size() && Shown[at] < aShown[next])) {
					auto last = at;
					while (last + 1U < Shown.size() && (next == aShown.size() || Shown[last + 1U] < aShown[next])) {
						++last;
					}
					beginRemoveRows({ }, static_cast<int>(at), static_cast<int>(last));
					Shown.erase(Shown.begin() + static_cast<std::ptrdiff_t>(at), Shown.begin() + static_cast<std::ptrdiff_t>(last) + 1);
					endRemoveRows();
				} else if (at == Shown.size() || Shown[at] > aShown[next]) {
					auto last = next;
					while (last + 1U < aShown.size() && (at == Shown.size() || aShown[last + 1U] < Shown[at])) {
						++last;
					}
					beginInsertRows({ }, static_cast<int>(at), static_cast<int>(at + (last - next)));
					Shown.insert(Shown.begin() + static_cast<std::ptrdiff_t>(at), aShown.begin() + static_cast<std::ptrdiff_t>(next), aShown.begin() + static_cast<std::ptrdiff_t>(last) + 1);
					endInsertRows();
					at += last - next + 1U;
					next = last + 1U;
				} else {
					++at;
					++next;
				}
			}
			if (!Shown.empty()) {
				Q_EMIT dataChanged(index(0), index(static_cast<int>(Shown.size()) - 1));
			}
		}

		std::vector<std::size_t> Shown;

	private:
		conversation_finder& Owner;
	};

	/// \brief Draws one item as the prototype does.
	class finder_delegate final : public QStyledItemDelegate
	{
	public:
		finder_delegate(conversation_finder& aOwner, finder_model& aModel, QObject* aParent)
			: QStyledItemDelegate{ aParent }
			, Owner{ aOwner }
			, Model{ aModel }
		{
		}

		void paint(QPainter* aPainter, const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const override
		{
			const auto row = static_cast<std::size_t>(aIndex.row());
			if (row >= Model.Shown.size()) {
				return;
			}
			const auto* const view    = qobject_cast<const QAbstractItemView*>(aOption.widget);
			const auto        focused = view != nullptr && view->hasFocus() && view->currentIndex() == aIndex;
			Owner.paint_item(*aPainter, Model.Shown[row], aOption.rect, Owner.HoveredItem == Model.Shown[row], focused);
		}

		[[nodiscard]] auto sizeHint(const QStyleOptionViewItem& aOption, const QModelIndex& aIndex) const -> QSize override
		{
			const auto row    = static_cast<std::size_t>(aIndex.row());
			const auto header = row < Model.Shown.size() && Owner.Items[Model.Shown[row]]->Header && row > 0U;
			return QSize{ aOption.rect.width(), header ? row_height + group_gap : row_height };
		}

	private:
		conversation_finder& Owner;
		finder_model&        Model;
	};

	/// \brief The list: rows, a header that stays at the top while its file's
	/// rows scroll under it, pointer picks and the finder's keys.
	class finder_list final : public QListView
	{
	public:
		finder_list(conversation_finder& aOwner, QWidget* aParent)
			: QListView{ aParent }
			, Owner{ aOwner }
		{
			setObjectName(QStringLiteral("threadFinderList"));
			setAccessibleName(QStringLiteral("Threads in the pull request, by file"));
			setFrameShape(QFrame::NoFrame);
			setSelectionMode(QAbstractItemView::SingleSelection);
			setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
			setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			setUniformItemSizes(false);
			setMouseTracking(true);
			viewport()->setMouseTracking(true);
			setFocusPolicy(Qt::StrongFocus);
			setEditTriggers(QAbstractItemView::NoEditTriggers);
		}

		/// The header drawn over the top of the list, while one is.
		QString StickyPath;
		QRect   StickyRect;

	protected:
		void paintEvent(QPaintEvent* aEvent) override
		{
			QListView::paintEvent(aEvent);
			StickyPath.clear();
			StickyRect     = { };
			const auto top = indexAt(QPoint{ 1, 1 });
			if (!top.isValid()) {
				return;
			}
			const auto& shown  = Owner.Model->Shown;
			auto        header = top.row();
			while (header >= 0 && !Owner.Items[shown[static_cast<std::size_t>(header)]]->Header) {
				--header;
			}
			if (header < 0) {
				return;
			}
			const auto own = visualRect(model()->index(header, 0));
			if (own.bottom() - row_height + 1 >= 0) {
				return;
			}
			auto y = 0;
			for (auto next = top.row() + 1; next < static_cast<int>(shown.size()); ++next) {
				if (Owner.Items[shown[static_cast<std::size_t>(next)]]->Header) {
					const auto box = visualRect(model()->index(next, 0)).bottom() - row_height + 1;
					if (box < row_height) {
						y = box - row_height;
					}
					break;
				}
			}
			const auto& path    = Owner.Items[shown[static_cast<std::size_t>(header)]]->Path;
			auto        painter = QPainter{ viewport() };
			StickyRect          = QRect{ 0, y, viewport()->width(), row_height };
			StickyPath          = path;
			painter.fillRect(StickyRect, palette().color(QPalette::Active, QPalette::Base));
			Owner.paint_header(painter, path, StickyRect, false, false);
		}

		void mousePressEvent(QMouseEvent* aEvent) override
		{
			if (aEvent->button() != Qt::LeftButton) {
				QListView::mousePressEvent(aEvent);
				return;
			}
			const auto point = aEvent->position().toPoint();
			if (!StickyRect.isEmpty() && StickyRect.contains(point)) {
				Owner.toggle_fold(StickyPath);
				return;
			}
			const auto index = indexAt(point);
			if (!index.isValid()) {
				QListView::mousePressEvent(aEvent);
				return;
			}
			const auto item = Owner.Model->Shown[static_cast<std::size_t>(index.row())];
			setCurrentIndex(index);
			if (Owner.Items[item]->Header) {
				Owner.toggle_fold(Owner.Items[item]->Path);
				return;
			}
			const auto geometry = Owner.geometry_of(item, visualRect(index));
			// [[BPR-1356]] [[BPR-1582]]. The two buttons open at once.
			if (geometry.Review.contains(point)) {
				Owner.pick(item, std::nullopt, core::workbench_thread_destination::review);
				return;
			}
			if (geometry.Discuss.contains(point)) {
				Owner.pick(item, std::nullopt, core::workbench_thread_destination::discuss);
				return;
			}
			if (geometry.Fold.contains(point)) {
				Owner.Unfolded.insert(Owner.item_key(item));
				viewport()->update();
				return;
			}
			// [[BPR-1582]]. A click selects, as everywhere else: the row, and
			// the post when it is on a bead. A double click opens.
			if (const auto bead = bead_at(item, geometry, point); bead.has_value()) {
				Owner.Bead = static_cast<int>(*bead);
			} else {
				Owner.Bead = -1;
			}
			viewport()->update();
		}

		/// [[BPR-1582]]. A double click opens the row, at the post under the
		/// pointer when it is on a bead: in Review, or with Shift in Discuss.
		void mouseDoubleClickEvent(QMouseEvent* aEvent) override
		{
			if (aEvent->button() != Qt::LeftButton) {
				QListView::mouseDoubleClickEvent(aEvent);
				return;
			}
			const auto point = aEvent->position().toPoint();
			const auto index = indexAt(point);
			if (!index.isValid() || (!StickyPath.isEmpty() && StickyRect.contains(point))) {
				return;
			}
			const auto item = Owner.Model->Shown[static_cast<std::size_t>(index.row())];
			if (Owner.Items[item]->Header) {
				return;
			}
			const auto geometry = Owner.geometry_of(item, visualRect(index));
			if (geometry.Review.contains(point) || geometry.Discuss.contains(point) || geometry.Fold.contains(point)) {
				return;
			}
			const auto destination = aEvent->modifiers().testFlag(Qt::ShiftModifier) ? core::workbench_thread_destination::discuss
																					 : core::workbench_thread_destination::review;
			Owner.pick(item, bead_at(item, geometry, point), destination);
		}

		[[nodiscard]] auto bead_at(const std::size_t aItem, const conversation_finder::row_geometry& aGeometry, const QPoint aPoint) const -> std::optional<std::size_t>
		{
			const auto beads = Owner.beads_of(aItem);
			for (std::size_t bead = 0U; bead < aGeometry.Beads.size() && bead < beads.size(); ++bead) {
				if (aGeometry.Beads[bead].adjusted(-1, -1, 1, 1).contains(aPoint)) {
					return bead;
				}
			}
			return std::nullopt;
		}

		void mouseMoveEvent(QMouseEvent* aEvent) override
		{
			QListView::mouseMoveEvent(aEvent);
			const auto point   = aEvent->position().toPoint();
			const auto index   = indexAt(point);
			auto       item    = std::optional<std::size_t>{ };
			auto       bead    = std::optional<std::size_t>{ };
			auto       discuss = false;
			auto       review  = false;
			if (index.isValid() && !(StickyRect.contains(point) && !StickyPath.isEmpty())) {
				item = Owner.Model->Shown[static_cast<std::size_t>(index.row())];
				if (!Owner.Items[*item]->Header) {
					// The button shows on the row under the pointer, so its
					// place is the hovered row's.
					const auto hovered  = std::exchange(Owner.HoveredItem, item);
					const auto rect     = visualRect(index);
					const auto geometry = Owner.geometry_of(*item, rect);
					Owner.HoveredItem   = hovered;
					discuss             = geometry.Discuss.contains(point);
					review              = geometry.Review.contains(point);
					for (std::size_t at = 0U; at < geometry.Beads.size(); ++at) {
						if (geometry.Beads[at].adjusted(-1, -1, 1, 1).contains(point)) {
							bead             = at;
							const auto beads = Owner.beads_of(*item);
							if (at < beads.size()) {
								Owner.show_preview(*item, beads[at].Post, geometry.Beads[at]);
							}
							break;
						}
					}
				}
			}
			if (!bead.has_value() && Owner.Bead < 0) {
				Owner.hide_preview();
			}
			if (item != Owner.HoveredItem || bead != Owner.HoveredBead || discuss != Owner.HoveredDiscuss || review != Owner.HoveredReview) {
				Owner.HoveredItem    = item;
				Owner.HoveredBead    = bead;
				Owner.HoveredDiscuss = discuss;
				Owner.HoveredReview  = review;
				viewport()->update();
			}
			viewport()->setCursor(item.has_value() ? Qt::PointingHandCursor : Qt::ArrowCursor);
		}

		void leaveEvent(QEvent* aEvent) override
		{
			QListView::leaveEvent(aEvent);
			Owner.HoveredItem.reset();
			Owner.HoveredBead.reset();
			Owner.HoveredDiscuss = false;
			Owner.HoveredReview  = false;
			if (Owner.Bead < 0) {
				Owner.hide_preview();
			}
			viewport()->update();
		}

		/// [[BPR-1356]]. The row menu: a right click opens it on the row
		/// under the pointer, the menu key on the cursor's row.
		void contextMenuEvent(QContextMenuEvent* aEvent) override
		{
			if (aEvent->reason() != QContextMenuEvent::Mouse) {
				if (const auto at = Owner.item_index(Owner.Cursor); at.has_value()) {
					const auto rect = visualRect(model()->index(static_cast<int>(*at), 0));
					Owner.popup_menu(Owner.Model->Shown[*at], std::nullopt, viewport()->mapToGlobal(QPoint{ rect.left() + 48, rect.bottom() }));
				}
				aEvent->accept();
				return;
			}
			// A scroll area hears its viewport's menu event in the viewport's
			// coordinates.
			const auto point = aEvent->pos();
			const auto index = indexAt(point);
			if (!index.isValid() || (!StickyPath.isEmpty() && StickyRect.contains(point))) {
				aEvent->ignore();
				return;
			}
			const auto item = Owner.Model->Shown[static_cast<std::size_t>(index.row())];
			if (Owner.Items[item]->Header) {
				aEvent->ignore();
				return;
			}
			setCurrentIndex(index);
			auto       bead     = std::optional<std::size_t>{ };
			const auto geometry = Owner.geometry_of(item, visualRect(index));
			for (std::size_t at = 0U; at < geometry.Beads.size(); ++at) {
				if (geometry.Beads[at].adjusted(-1, -1, 1, 1).contains(point)) {
					bead = at;
					break;
				}
			}
			Owner.popup_menu(item, bead, aEvent->globalPos());
			aEvent->accept();
		}

		void keyPressEvent(QKeyEvent* aEvent) override
		{
			if (Owner.handle_key(*aEvent, false)) {
				aEvent->accept();
				return;
			}
			QListView::keyPressEvent(aEvent);
		}

		void focusInEvent(QFocusEvent* aEvent) override
		{
			QListView::focusInEvent(aEvent);
			viewport()->update();
		}

		void focusOutEvent(QFocusEvent* aEvent) override
		{
			QListView::focusOutEvent(aEvent);
			viewport()->update();
		}

		void scrollContentsBy(const int aDx, const int aDy) override
		{
			QListView::scrollContentsBy(aDx, aDy);
			Owner.hide_preview();
		}

	private:
		conversation_finder& Owner;
	};

	/// \brief A filter chip: its glyph or mark, its count, and a chevron when
	/// it opens a menu.
	class finder_chip final : public QAbstractButton
	{
	public:
		finder_chip(const conversation_glyph aGlyph, const bool aMenu, QWidget* aParent)
			: QAbstractButton{ aParent }
			, Glyph{ aGlyph }
			, Menu{ aMenu }
		{
			setCheckable(!aMenu);
			setFocusPolicy(Qt::StrongFocus);
			setCursor(Qt::PointingHandCursor);
			setAttribute(Qt::WA_Hover, true);
		}

		void set_colours(const note_colours& aColours, const feedback_mark_theme& aTheme)
		{
			Colours = aColours;
			Theme   = aTheme;
			update();
		}

		void set_count(const QString& aCount)
		{
			if (Count != aCount) {
				Count = aCount;
				updateGeometry();
				update();
			}
		}

		void set_mark(const std::optional<core::feedback_label> aLabel)
		{
			if (Label != aLabel) {
				Label = aLabel;
				update();
			}
		}

		void set_on(const bool aOn)
		{
			On = aOn;
			update();
		}

		[[nodiscard]] auto on() const noexcept -> bool
		{
			return isCheckable() ? isChecked() : On;
		}

		[[nodiscard]] auto sizeHint() const -> QSize override
		{
			auto width = 9 + 16 + 9;
			if (!Count.isEmpty()) {
				width += 5 + QFontMetrics{ weighted(font(), QFont::DemiBold) }.horizontalAdvance(Count);
			}
			if (Menu) {
				width += 5 + 12;
			}
			return QSize{ width, chip_height };
		}

		[[nodiscard]] auto minimumSizeHint() const -> QSize override
		{
			return sizeHint();
		}

	protected:
		void paintEvent(QPaintEvent* /*aEvent*/) override
		{
			auto painter = QPainter{ this };
			painter.setRenderHint(QPainter::Antialiasing, true);
			const auto hovered = underMouse() && isEnabled();
			const auto box     = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
			auto       fill    = on() ? signal_soft(Colours) : hovered ? Colours.Hover
																	   : QColor{ Qt::transparent };
			auto       edge    = on() ? blend(Colours.Signal, Colours.Rule, 0.45) : hovered ? Colours.Rule2
																							: Colours.Rule;
			painter.setPen(QPen{ edge, 1.0 });
			painter.setBrush(fill);
			painter.drawRoundedRect(box, 7.0, 7.0);
			if (hasFocus()) {
				painter.setPen(QPen{ Colours.Signal, 2.0 });
				painter.setBrush(Qt::NoBrush);
				painter.drawRoundedRect(box.adjusted(-1.0, -1.0, 1.0, 1.0), 8.0, 8.0);
			}
			const auto ink = !isEnabled() ? Colours.Faint : on() || hovered ? Colours.Ink
																			: Colours.Ink2;
			auto       x   = 9;
			const auto top = (height() - 16) / 2;
			if (Label.has_value()) {
				paint_feedback_mark(painter, QRectF{ static_cast<qreal>(x), static_cast<qreal>(top), 16.0, 16.0 }, *Label, { },
									feedback_mark_colours_for(Theme, *Label, palette()));
			} else {
				paint_conversation_glyph(painter, QRectF{ static_cast<qreal>(x), static_cast<qreal>(top), 16.0, 16.0 }, Glyph, ink);
			}
			x += 16;
			if (!Count.isEmpty()) {
				x += 5;
				const auto bold = weighted(font(), QFont::DemiBold);
				painter.setFont(bold);
				painter.setPen(Glyph == conversation_glyph::only_new && isEnabled() ? Colours.New : ink);
				const auto width = QFontMetrics{ bold }.horizontalAdvance(Count);
				painter.drawText(QRect{ x, 0, width + 1, height() }, Qt::AlignLeft | Qt::AlignVCenter, Count);
				x += width;
			}
			if (Menu) {
				x += 5;
				painter.setOpacity(0.7);
				paint_conversation_glyph(painter, QRectF{ static_cast<qreal>(x), (height() - 12) / 2.0, 12.0, 12.0 }, conversation_glyph::down, ink);
			}
		}

		auto event(QEvent* aEvent) -> bool override
		{
			if (aEvent->type() == QEvent::HoverEnter || aEvent->type() == QEvent::HoverLeave) {
				update();
			}
			return QAbstractButton::event(aEvent);
		}

	private:
		conversation_glyph                  Glyph;
		bool                                Menu = false;
		bool                                On   = false;
		QString                             Count;
		std::optional<core::feedback_label> Label;
		note_colours                        Colours;
		feedback_mark_theme                 Theme;
	};

	/// \brief The search field's frame: its lens, the field, Clear and ⌘J.
	class finder_search_box final : public QWidget
	{
	public:
		explicit finder_search_box(QWidget* aParent)
			: QWidget{ aParent }
		{
			setObjectName(QStringLiteral("threadFinderSearchBox"));
			Field = new QLineEdit{ this };
			Field->setObjectName(QStringLiteral("threadFinderSearch"));
			Field->setFrame(false);
			Field->setPlaceholderText(QStringLiteral("Find a thread"));
			Field->setAccessibleName(QStringLiteral("Find a thread: words, a person, a file or a line"));
			Field->setAttribute(Qt::WA_MacShowFocusRect, false);
			Clear = new QToolButton{ this };
			Clear->setObjectName(QStringLiteral("threadFinderClear"));
			Clear->setAutoRaise(true);
			Clear->setIconSize(QSize{ 14, 14 });
			Clear->setFocusPolicy(Qt::NoFocus);
			Clear->setToolTip(QStringLiteral("Clear (Esc)"));
			Clear->setAccessibleName(QStringLiteral("Clear the search"));
			Clear->hide();
			auto* const row = new QHBoxLayout{ this };
			row->setContentsMargins(30, 0, 8, 0);
			row->setSpacing(4);
			row->addWidget(Field, 1);
			row->addWidget(Clear);
			Key = new QLabel{ this };
			Key->setObjectName(QStringLiteral("threadFinderKey"));
			Key->setToolTip(QStringLiteral("Goes to Threads, and back"));
			row->addWidget(Key);
			setFixedHeight(32);
			setMaximumWidth(search_maximum);
			setMinimumWidth(160);
			QObject::connect(Field, &QLineEdit::textChanged, Clear, [this](const QString& aText) { Clear->setVisible(!aText.isEmpty()); });
			QObject::connect(Clear, &QToolButton::clicked, Field, [this] {
				Field->clear();
				Field->setFocus(Qt::OtherFocusReason);
			});
		}

		void set_colours(const note_colours& aColours, const QString& aKey)
		{
			Colours = aColours;
			Clear->setIcon(conversation_icon(conversation_glyph::unresolve, aColours.Faint));
			auto palette = Field->palette();
			palette.setColor(QPalette::Base, Qt::transparent);
			palette.setColor(QPalette::Text, aColours.Ink);
			palette.setColor(QPalette::PlaceholderText, aColours.Faint);
			Field->setPalette(palette);
			Key->setText(aKey);
			auto keyPalette = Key->palette();
			keyPalette.setColor(QPalette::WindowText, aColours.Faint);
			Key->setPalette(keyPalette);
			Key->setFont(weighted(sized(font(), 0.85), QFont::DemiBold));
			Key->setContentsMargins(5, 1, 5, 1);
			update();
		}

		QLineEdit*   Field = nullptr;
		QToolButton* Clear = nullptr;
		QLabel*      Key   = nullptr;

	protected:
		void paintEvent(QPaintEvent* /*aEvent*/) override
		{
			auto painter = QPainter{ this };
			painter.setRenderHint(QPainter::Antialiasing, true);
			const auto focused = Field->hasFocus();
			const auto box     = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
			if (focused) {
				painter.setPen(QPen{ alpha(Colours.Signal, 0.2), 3.0 });
				painter.setBrush(Qt::NoBrush);
				painter.drawRoundedRect(box.adjusted(-1.0, -1.0, 1.0, 1.0), 9.0, 9.0);
			}
			painter.setPen(QPen{ focused ? Colours.Signal : Colours.Rule2, 1.0 });
			painter.setBrush(Colours.CodeBackground.isValid() ? Colours.CodeBackground : Colours.Panel);
			painter.drawRoundedRect(box, 8.0, 8.0);
			paint_conversation_glyph(painter, QRectF{ 10.0, (height() - 16) / 2.0, 16.0, 16.0 }, conversation_glyph::search, Colours.Faint);
			// The key, as a keycap.
			const auto key = Key->geometry();
			painter.setPen(QPen{ Colours.Rule2, 1.0 });
			painter.setBrush(Qt::NoBrush);
			painter.drawRoundedRect(QRectF{ key }.adjusted(0.5, 3.5, -0.5, -3.5), 4.0, 4.0);
		}

	private:
		note_colours Colours;
	};

	/// \brief One post's preview, beside its bead: the prototype's card.
	class finder_preview final : public QWidget
	{
	public:
		explicit finder_preview(QWidget* aParent)
			: QWidget{ aParent, Qt::ToolTip | Qt::FramelessWindowHint }
		{
			setObjectName(QStringLiteral("threadFinderPreview"));
			setAttribute(Qt::WA_ShowWithoutActivating, true);
			setAttribute(Qt::WA_TransparentForMouseEvents, true);
			setAttribute(Qt::WA_TranslucentBackground, true);
		}

		void present(const finder_post& aPost, const QString& aAnswers, const note_colours& aColours, const feedback_mark_theme& aTheme)
		{
			Post    = aPost;
			Answers = aAnswers;
			Colours = aColours;
			Theme   = aTheme;
			setAccessibleName(QStringLiteral("%1: %2").arg(who(Post), first_words(Post.Body.simplified(), 200)));
			Lines.clear();
			auto layout = QTextLayout{ Post.Body.simplified(), font() };
			layout.beginLayout();
			const auto width = preview_width - 22;
			for (;;) {
				auto line = layout.createLine();
				if (!line.isValid()) {
					break;
				}
				line.setLineWidth(width);
				Lines.push_back(Post.Body.simplified().mid(line.textStart(), line.textLength()).trimmed());
				if (Lines.size() == preview_lines) {
					const auto rest = Post.Body.simplified().mid(line.textStart());
					Lines.back()    = QFontMetrics{ font() }.elidedText(rest, Qt::ElideRight, width);
					break;
				}
			}
			layout.endLayout();
			const auto spacing = QFontMetrics{ font() }.lineSpacing();
			setFixedSize(preview_width, 9 + 20 + (Answers.isEmpty() ? 0 : 18) + 4 + static_cast<int>(Lines.size()) * spacing + 10);
			update();
		}

		[[nodiscard]] auto post() const noexcept -> const finder_post&
		{
			return Post;
		}

	protected:
		void paintEvent(QPaintEvent* /*aEvent*/) override
		{
			auto painter = QPainter{ this };
			painter.setRenderHint(QPainter::Antialiasing, true);
			const auto box = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
			painter.setPen(QPen{ Colours.Rule, 1.0 });
			painter.setBrush(Colours.Panel);
			painter.drawRoundedRect(box, 9.0, 9.0);
			auto y = 9;
			paint_avatar(painter, QRectF{ 11.0, static_cast<qreal>(y + 1), 18.0, 18.0 }, Post.Author, Post.Mine, Colours, font());
			const auto bold = weighted(font(), QFont::DemiBold);
			painter.setFont(bold);
			painter.setPen(Colours.Ink);
			const auto name = who(Post);
			painter.drawText(QRect{ 36, y, preview_width - 72, 20 }, Qt::AlignLeft | Qt::AlignVCenter, name);
			if (Post.Unread) {
				const auto x = 36 + QFontMetrics{ bold }.horizontalAdvance(name) + 7;
				painter.setPen(Qt::NoPen);
				painter.setBrush(Colours.New);
				painter.drawEllipse(QRectF{ static_cast<qreal>(x), y + 6.5, 7.0, 7.0 });
			}
			if (Post.Label.has_value()) {
				paint_feedback_mark(painter, QRectF{ preview_width - 11.0 - 16.0, y + 2.0, 16.0, 16.0 }, *Post.Label, Post.Emphasis,
									feedback_mark_colours_for(Theme, *Post.Label, palette()));
			}
			y += 20;
			painter.setFont(font());
			if (!Answers.isEmpty()) {
				y += 2;
				painter.setFont(sized(font(), 0.9));
				painter.setPen(Colours.Faint);
				painter.drawText(QRect{ 11, y, preview_width - 22, 16 }, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("↩ answers %1").arg(Answers));
				y += 16;
				painter.setFont(font());
			}
			y += 4;
			painter.setPen(Colours.Ink);
			const auto spacing = QFontMetrics{ font() }.lineSpacing();
			for (const auto& line : Lines) {
				painter.drawText(QRect{ 11, y, preview_width - 22, spacing }, Qt::AlignLeft | Qt::AlignVCenter, line);
				y += spacing;
			}
		}

	private:
		finder_post          Post;
		QString              Answers;
		note_colours         Colours;
		feedback_mark_theme  Theme;
		std::vector<QString> Lines;
	};

	conversation_finder::conversation_finder(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("threadFinder"));
		setAccessibleName(QStringLiteral("Threads"));
		setAccessibleDescription(QStringLiteral("Every thread in the pull request that is on a file, grouped by file. A click selects a thread; "
												"a double click or Return reads it in Review, and Shift with either reads it in Discuss."));
		setAutoFillBackground(true);
		setBackgroundRole(QPalette::Base);
		MarkTheme = default_feedback_mark_theme(palette());

		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(0);
		Bar = new QWidget{ this };
		Bar->setObjectName(QStringLiteral("threadFinderBar"));
		auto* const bar = new QHBoxLayout{ Bar };
		bar->setContentsMargins(14, 10, 16, 10);
		bar->setSpacing(6);
		auto* const searchBox = new finder_search_box{ Bar };
		Search                = searchBox->Field;
		bar->addWidget(searchBox, 1);
		OnlyNew = new finder_chip{ conversation_glyph::only_new, false, Bar };
		OnlyNew->setObjectName(QStringLiteral("threadFinderOnlyNew"));
		HideResolved = new finder_chip{ conversation_glyph::hide_resolved, false, Bar };
		HideResolved->setObjectName(QStringLiteral("threadFinderHideResolved"));
		HideResolved->setToolTip(QStringLiteral("Hide resolved threads"));
		HideResolved->setAccessibleName(HideResolved->toolTip());
		LabelChip = new finder_chip{ conversation_glyph::tag, true, Bar };
		LabelChip->setObjectName(QStringLiteral("threadFinderLabel"));
		bar->addWidget(OnlyNew);
		bar->addWidget(HideResolved);
		bar->addWidget(LabelChip);
		bar->addStretch(0);
		column->addWidget(Bar);

		Pages = new QStackedWidget{ this };
		Pages->setObjectName(QStringLiteral("threadFinderPages"));
		List  = new finder_list{ *this, Pages };
		Model = new finder_model{ *this };
		List->setModel(Model);
		List->setItemDelegate(new finder_delegate{ *this, *Model, List });
		List->viewport()->setAutoFillBackground(true);
		List->viewport()->setBackgroundRole(QPalette::Base);
		Pages->addWidget(List);
		List->setAccessibleDescription(QStringLiteral("Return or a double click opens a thread in Review; Shift and Return, or Shift and a double click, "
													  "open it in Discuss; the menu key lists both and Mark as read"));
		// The button's name, directly below it; a file's header names its path.
		anchor_tooltips(*List->viewport(), [this](const QPoint aPosition) -> std::optional<anchored_tooltip_target> {
			const auto index = List->indexAt(aPosition);
			if (!index.isValid()) {
				return std::nullopt;
			}
			const auto item = Model->Shown[static_cast<std::size_t>(index.row())];
			const auto rect = List->visualRect(index);
			if (Items[item]->Header) {
				return anchored_tooltip_target{ .Anchor = rect, .Text = Items[item]->Path };
			}
			const auto geometry = geometry_of(item, rect);
			if (geometry.Review.contains(aPosition)) {
				return anchored_tooltip_target{
					.Anchor = geometry.Review,
					.Text   = QStringLiteral("Open in Review (%1)").arg(QKeySequence{ Qt::Key_Return }.toString(QKeySequence::NativeText)),
				};
			}
			if (!geometry.Discuss.contains(aPosition)) {
				return std::nullopt;
			}
			return anchored_tooltip_target{
				.Anchor = geometry.Discuss,
				.Text   = QStringLiteral("Open in Discuss (%1)").arg(QKeySequence{ Qt::SHIFT | Qt::Key_Return }.toString(QKeySequence::NativeText)),
			};
		});
		const auto makeEmpty = [this](const QString& aName, const QString& aText, const QString& aButton) {
			auto* const page = new QWidget{ Pages };
			page->setObjectName(aName);
			auto* const layout = new QVBoxLayout{ page };
			layout->setContentsMargins(22, 40, 22, 22);
			layout->setSpacing(10);
			auto* const text = new QLabel{ aText, page };
			text->setObjectName(aName + QStringLiteral("Text"));
			text->setWordWrap(true);
			text->setForegroundRole(QPalette::PlaceholderText);
			layout->addWidget(text);
			if (!aButton.isEmpty()) {
				auto* const button = new QPushButton{ aButton, page };
				button->setObjectName(aName + QStringLiteral("Reset"));
				QObject::connect(button, &QPushButton::clicked, this, [this] {
					set_filters({ });
					Search->setFocus(Qt::OtherFocusReason);
				});
				layout->addWidget(button, 0, Qt::AlignLeft);
			}
			layout->addStretch(1);
			Pages->addWidget(page);
			return page;
		};
		Empty      = makeEmpty(QStringLiteral("threadFinderEmpty"), QStringLiteral("No thread matches."), QStringLiteral("Show all threads"));
		NoneOnFile = makeEmpty(QStringLiteral("threadFinderNoneOnFile"),
							   QStringLiteral("No thread in this pull request is on a file. Discuss reads its comments and review summaries."), { });
		column->addWidget(Pages, 1);
		Preview = new finder_preview{ this };
		Preview->hide();

		Search->installEventFilter(this);
		QObject::connect(Search, &QLineEdit::textChanged, this, [this](const QString& aText) {
			if (Filters.Search != aText) {
				Filters.Search = aText;
				apply_filters();
			}
		});
		QObject::connect(OnlyNew, &QAbstractButton::toggled, this, [this](const bool aOn) {
			if (Filters.OnlyNew != aOn) {
				Filters.OnlyNew = aOn;
				apply_filters();
			}
		});
		QObject::connect(HideResolved, &QAbstractButton::toggled, this, [this](const bool aOn) {
			if (Filters.HideResolved != aOn) {
				Filters.HideResolved = aOn;
				apply_filters();
			}
		});
		QObject::connect(LabelChip, &QAbstractButton::clicked, this, [this] { open_label_menu(); });
		QObject::connect(List->selectionModel(), &QItemSelectionModel::currentChanged, this, [this](const QModelIndex& aCurrent) {
			const auto key = aCurrent.isValid() ? item_key(Model->Shown[static_cast<std::size_t>(aCurrent.row())]) : std::string{ };
			if (key != Cursor) {
				Cursor = key;
				Bead   = -1;
				hide_preview();
			}
		});
		refresh_colours();
		refresh_bar();
	}

	conversation_finder::~conversation_finder() = default;

	void conversation_finder::adopt(const std::vector<application::github_conversation_projection>& aForest)
	{
		if (Forest == aForest) {
			return;
		}
		Forest = aForest;
		Dirty  = true;
		if (isVisible()) {
			refresh();
		}
	}

	void conversation_finder::clear()
	{
		Forest.clear();
		ReadState.clear();
		ReadStateKnown = false;
		Labels.reset();
		LabelMembers.clear();
		Draft.reset();
		Reading = { };
		Filters = { };
		Folded.clear();
		Unfolded.clear();
		Cursor.clear();
		Bead = -1;
		{
			const QSignalBlocker blocker{ Search };
			Search->clear();
		}
		Dirty = true;
		refresh();
	}

	void conversation_finder::set_read_state(std::vector<thread_read_state> aState)
	{
		if (ReadStateKnown && ReadState == aState) {
			return;
		}
		ReadState      = std::move(aState);
		ReadStateKnown = true;
		Dirty          = true;
		if (isVisible()) {
			refresh();
		}
	}

	void conversation_finder::set_thread_read_state(const std::string& aThread, std::vector<std::string> aUnreadPosts)
	{
		auto found = std::ranges::find(ReadState, aThread, &thread_read_state::Thread);
		if (aUnreadPosts.empty()) {
			if (found == ReadState.end()) {
				return;
			}
			ReadState.erase(found);
		} else if (found == ReadState.end()) {
			ReadState.push_back({ .Thread = aThread, .UnreadPosts = std::move(aUnreadPosts) });
		} else if (found->UnreadPosts == aUnreadPosts) {
			return;
		} else {
			found->UnreadPosts = std::move(aUnreadPosts);
		}
		Dirty = true;
		if (isVisible()) {
			refresh();
		}
	}

	void conversation_finder::set_viewer_login(const QString& aLogin)
	{
		if (Viewer == aLogin) {
			return;
		}
		Viewer = aLogin;
		Dirty  = true;
		if (isVisible()) {
			refresh();
		}
	}

	void conversation_finder::set_labels(std::optional<application::conversation_labels_projection> aLabels)
	{
		if (Labels == aLabels) {
			return;
		}
		Labels = std::move(aLabels);
		LabelMembers.clear();
		if (Labels.has_value()) {
			for (const auto& listing : Labels->Labels) {
				LabelMembers[listing.Label] = std::set<std::string>{ listing.Conversations.begin(), listing.Conversations.end() };
			}
		}
		apply_filters();
	}

	void conversation_finder::set_label_marks(const feedback_mark_theme& aTheme)
	{
		MarkTheme = aTheme;
		refresh_colours();
	}

	void conversation_finder::set_colours(const QColor& aNew, const QColor& aRoute)
	{
		New   = aNew;
		Route = aRoute;
		refresh_colours();
	}

	void conversation_finder::set_code_font(const QFont& aFont)
	{
		CodeFont = aFont;
		List->viewport()->update();
	}

	void conversation_finder::set_draft(std::optional<finder_draft> aDraft)
	{
		if (Draft == aDraft) {
			return;
		}
		Draft = std::move(aDraft);
		Dirty = true;
		if (isVisible()) {
			refresh();
		}
	}

	void conversation_finder::set_reading(finder_reading aReading)
	{
		if (Reading == aReading) {
			return;
		}
		Reading = std::move(aReading);
		List->viewport()->update();
	}

	void conversation_finder::enter()
	{
		refresh();
		// Start on the conversation being read, so Return goes back to it.
		if (const auto open = open_conversation(); open.has_value()) {
			const auto key = std::string{ row_key_prefix } + Conversations[*open].Id + "/" + Conversations[*open].Posts.front().Id;
			if (item_index(key).has_value()) {
				Cursor = key;
			}
		}
		if (!item_index(Cursor).has_value()) {
			Cursor.clear();
			for (const auto shown : Model->Shown) {
				if (!Items[shown]->Header) {
					Cursor = Items[shown]->Key;
					break;
				}
			}
		}
		Bead = -1;
		if (const auto at = item_index(Cursor); at.has_value()) {
			const auto index = Model->index(static_cast<int>(*at));
			List->setCurrentIndex(index);
			List->scrollTo(index, QAbstractItemView::PositionAtCenter);
		}
		Search->setFocus(Qt::OtherFocusReason);
		Search->selectAll();
	}

	auto conversation_finder::reveal(const std::string& aThread, const std::string& aPost, bool* const aCleared) -> bool
	{
		refresh();
		auto target = std::optional<std::size_t>{ };
		auto post   = std::optional<std::size_t>{ };
		for (std::size_t at = 0U; at < Items.size(); ++at) {
			const auto& entry = *Items[at];
			if (entry.Header || Conversations[entry.Conversation].Id != aThread) {
				continue;
			}
			const auto& conversation = Conversations[entry.Conversation];
			const auto& row          = conversation.Rows[entry.Row];
			if (!target.has_value()) {
				target = at;
			}
			const auto named = std::ranges::find(row.Posts, aPost, [&conversation](const std::size_t aIndex) { return conversation.Posts[aIndex].Id; });
			if (!aPost.empty() && named != row.Posts.end()) {
				target = at;
				post   = *named;
				break;
			}
		}
		if (!target.has_value()) {
			return false;
		}
		auto cleared = false;
		if (std::ranges::find(Model->Shown, *target) == Model->Shown.end()) {
			// The row is hidden: the search, a filter or its folded file.
			cleared = true;
			Folded.erase(Items[*target]->Path);
			Unfolded.insert(Items[*target]->Key);
			set_filters({ });
		}
		if (aCleared != nullptr) {
			*aCleared = cleared;
		}
		Cursor = Items[*target]->Key;
		Unfolded.insert(Cursor);
		const auto at = item_index(Cursor);
		if (!at.has_value()) {
			return false;
		}
		const auto index = Model->index(static_cast<int>(*at));
		List->setCurrentIndex(index);
		Bead = -1;
		if (post.has_value()) {
			const auto beads = beads_of(*target);
			if (const auto bead = std::ranges::find(beads, *post, &bead::Post); bead != beads.end()) {
				Bead = static_cast<int>(bead - beads.begin());
			}
		}
		List->scrollTo(index, QAbstractItemView::PositionAtCenter);
		List->setFocus(Qt::OtherFocusReason);
		List->viewport()->update();
		return true;
	}

	void conversation_finder::set_pick_dispatch(finder_pick_dispatch aDispatch)
	{
		PickDispatch = std::move(aDispatch);
	}

	void conversation_finder::set_leave_dispatch(std::function<void()> aDispatch)
	{
		LeaveDispatch = std::move(aDispatch);
	}

	auto conversation_finder::filters() const noexcept -> const finder_filters&
	{
		return Filters;
	}

	void conversation_finder::set_filters(finder_filters aFilters)
	{
		Filters = std::move(aFilters);
		{
			const QSignalBlocker search{ Search };
			const QSignalBlocker onlyNew{ OnlyNew };
			const QSignalBlocker hideResolved{ HideResolved };
			Search->setText(Filters.Search);
			OnlyNew->setChecked(Filters.OnlyNew);
			HideResolved->setChecked(Filters.HideResolved);
		}
		apply_filters();
	}

	auto conversation_finder::conversations() const -> const std::vector<finder_conversation>&
	{
		return Conversations;
	}

	auto conversation_finder::shown_keys() const -> std::vector<std::string>
	{
		auto keys = std::vector<std::string>{ };
		keys.reserve(Model->Shown.size());
		for (const auto shown : Model->Shown) {
			keys.push_back(Items[shown]->Key);
		}
		return keys;
	}

	auto conversation_finder::cursor_key() const -> std::string
	{
		return Cursor;
	}

	auto conversation_finder::cursor_post() const -> std::string
	{
		const auto at = item_index(Cursor);
		if (!at.has_value() || Bead < 0) {
			return { };
		}
		const auto item  = Model->Shown[*at];
		const auto beads = beads_of(item);
		if (static_cast<std::size_t>(Bead) >= beads.size()) {
			return { };
		}
		return Conversations[Items[item]->Conversation].Posts[beads[static_cast<std::size_t>(Bead)].Post].Id;
	}

	auto conversation_finder::shown_beads(const std::string& aRowKey) const -> std::vector<std::string>
	{
		auto       posts = std::vector<std::string>{ };
		const auto at    = item_index(aRowKey);
		if (!at.has_value() || Items[Model->Shown[*at]]->Header) {
			return posts;
		}
		const auto  item         = Model->Shown[*at];
		const auto& conversation = Conversations[Items[item]->Conversation];
		for (const auto& shown : beads_of(item)) {
			posts.push_back(conversation.Posts[shown.Post].Id);
		}
		return posts;
	}

	auto conversation_finder::folded_posts(const std::string& aRowKey) const -> std::size_t
	{
		const auto at = item_index(aRowKey);
		if (!at.has_value() || Items[Model->Shown[*at]]->Header) {
			return 0U;
		}
		const auto  item = Model->Shown[*at];
		const auto& row  = Conversations[Items[item]->Conversation].Rows[Items[item]->Row];
		return row.Posts.size() - beads_of(item).size();
	}

	auto conversation_finder::row_rect(const std::string& aRowKey) const -> QRect
	{
		const auto at = item_index(aRowKey);
		return at.has_value() ? List->visualRect(Model->index(static_cast<int>(*at))) : QRect{ };
	}

	auto conversation_finder::bead_rect(const std::string& aRowKey, const std::string& aPost) const -> QRect
	{
		const auto at = item_index(aRowKey);
		if (!at.has_value()) {
			return { };
		}
		const auto  item     = Model->Shown[*at];
		const auto  geometry = geometry_of(item, List->visualRect(Model->index(static_cast<int>(*at))));
		const auto  beads    = beads_of(item);
		const auto& posts    = Conversations[Items[item]->Conversation].Posts;
		for (std::size_t bead = 0U; bead < beads.size() && bead < geometry.Beads.size(); ++bead) {
			if (posts[beads[bead].Post].Id == aPost) {
				return geometry.Beads[bead];
			}
		}
		return { };
	}

	auto conversation_finder::label_counts() const -> std::map<core::feedback_label, std::size_t>
	{
		auto counts = std::map<core::feedback_label, std::size_t>{ };
		for (const auto& [label, members] : LabelMembers) {
			const auto count = static_cast<std::size_t>(std::ranges::count_if(Conversations, [&members](const finder_conversation& aConversation) {
				return members.contains(aConversation.Id);
			}));
			if (count > 0U) {
				counts[label] = count;
			}
		}
		return counts;
	}

	auto conversation_finder::row_text(const std::string& aRowKey) const -> QString
	{
		const auto at = item_index(aRowKey);
		return at.has_value() ? accessible_text(Model->Shown[*at]) : QString{ };
	}

	auto conversation_finder::search_field() const noexcept -> QLineEdit*
	{
		return Search;
	}

	auto conversation_finder::only_new_button() const noexcept -> QAbstractButton*
	{
		return OnlyNew;
	}

	auto conversation_finder::hide_resolved_button() const noexcept -> QAbstractButton*
	{
		return HideResolved;
	}

	auto conversation_finder::label_button() const noexcept -> QAbstractButton*
	{
		return LabelChip;
	}

	auto conversation_finder::list() const noexcept -> QListView*
	{
		return List;
	}

	auto conversation_finder::preview() const noexcept -> QWidget*
	{
		return Preview->isVisible() ? Preview : nullptr;
	}

	auto conversation_finder::locate(const std::string& aIdentity) -> std::optional<finder_location>
	{
		refresh();
		for (const auto& conversation : Conversations) {
			if (conversation.Draft) {
				continue;
			}
			if (conversation.Id == aIdentity) {
				return finder_location{ .Thread = conversation.Id, .Post = conversation.Posts.front().Id, .Path = conversation.Path };
			}
			for (const auto& post : conversation.Posts) {
				if (post.Id == aIdentity) {
					return finder_location{ .Thread = conversation.Id, .Post = post.Id, .Path = conversation.Path };
				}
			}
		}
		return std::nullopt;
	}

	void conversation_finder::refresh()
	{
		if (!Dirty) {
			return;
		}
		rebuild();
	}

	auto conversation_finder::rebuilds() const noexcept -> std::uint64_t
	{
		return Rebuilds;
	}

	void conversation_finder::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange || aEvent->type() == QEvent::FontChange) {
			refresh_colours();
		}
	}

	void conversation_finder::showEvent(QShowEvent* const aEvent)
	{
		QWidget::showEvent(aEvent);
		refresh();
	}

	void conversation_finder::hideEvent(QHideEvent* const aEvent)
	{
		QWidget::hideEvent(aEvent);
		hide_preview();
	}

	auto conversation_finder::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
		if (aWatched == Search && aEvent->type() == QEvent::KeyPress) {
			if (handle_key(*static_cast<QKeyEvent*>(aEvent), true)) {
				return true;
			}
		}
		if (aWatched == Search && (aEvent->type() == QEvent::FocusIn || aEvent->type() == QEvent::FocusOut)) {
			if (Search->parentWidget() != nullptr) {
				Search->parentWidget()->update();
			}
			List->viewport()->update();
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}

	void conversation_finder::rebuild()
	{
		Dirty = false;
		++Rebuilds;
		Conversations = finder_conversations_of(Forest, ReadState, ReadStateKnown, Viewer);
		if (Draft.has_value()) {
			// My draft sits in its file's group, which a conversation names.
			const auto file = std::ranges::find_if(Conversations, [this](const finder_conversation& aConversation) {
				return core::file_identity_of(aConversation.Path.toStdString()) == Draft->File;
			});
			if (file != Conversations.end()) {
				auto draft        = finder_conversation{ };
				draft.Id          = std::string{ draft_identity };
				draft.Path        = file->Path;
				draft.Line        = Draft->Line;
				draft.Draft       = true;
				const auto author = Draft->Author.isEmpty() ? Viewer : Draft->Author;
				auto       post   = make_post(std::string{ draft_identity }, author, Draft->Body.toStdString(), author);
				post.Mine         = true;
				draft.Posts       = { std::move(post) };
				draft.Rows        = finder_rows_of(draft.Posts);
				Conversations.push_back(std::move(draft));
				std::ranges::stable_sort(Conversations, [](const finder_conversation& aLeft, const finder_conversation& aRight) {
					const auto left  = aLeft.Path.toStdString();
					const auto right = aRight.Path.toStdString();
					return left.empty() != right.empty() ? !left.empty() : (left != right ? left < right : aLeft.Line < aRight.Line);
				});
			}
		}
		Items.clear();
		auto path = QString{ };
		for (std::size_t conversation = 0U; conversation < Conversations.size(); ++conversation) {
			const auto& shown = Conversations[conversation];
			if (Items.empty() || shown.Path != path) {
				path           = shown.Path;
				auto header    = std::make_unique<item>();
				header->Header = true;
				header->Path   = path;
				header->File   = core::file_identity_of(path.toStdString());
				header->Key    = std::string{ file_key_prefix } + path.toStdString();
				Items.push_back(std::move(header));
			}
			for (std::size_t row = 0U; row < shown.Rows.size(); ++row) {
				auto entry          = std::make_unique<item>();
				entry->Path         = path;
				entry->Conversation = conversation;
				entry->Row          = row;
				entry->Key          = std::string{ row_key_prefix } + shown.Id + "/" + shown.Posts[shown.Rows[row].Posts.front()].Id;
				Items.push_back(std::move(entry));
			}
		}
		{
			// A new forest keeps the cursor by its key.
			const QSignalBlocker blocker{ List->selectionModel() };
			Model->apply({ }, true);
		}
		apply_filters();
	}

	auto conversation_finder::label_members() const -> const std::set<std::string>*
	{
		if (!Filters.Label.has_value()) {
			return nullptr;
		}
		static const auto none  = std::set<std::string>{ };
		const auto        found = LabelMembers.find(*Filters.Label);
		return found == LabelMembers.end() ? &none : &found->second;
	}

	auto conversation_finder::matches(const finder_conversation& aConversation) const -> bool
	{
		if (Filters.OnlyNew && aConversation.Unread == 0U) {
			return false;
		}
		if (Filters.HideResolved && aConversation.Resolved) {
			return false;
		}
		if (const auto* const members = label_members(); members != nullptr && !members->contains(aConversation.Id)) {
			return false;
		}
		const auto search = Filters.Search.trimmed();
		if (search.isEmpty()) {
			return true;
		}
		return aConversation.Path.contains(search, Qt::CaseInsensitive)
			|| std::ranges::any_of(aConversation.Posts, [&search](const finder_post& aPost) { return finder_post_found(aPost, search); })
			|| (aConversation.Line != 0U && QString::number(aConversation.Line) == search);
	}

	void conversation_finder::apply_filters(const bool /*aKeepCursor*/)
	{
		auto       shown     = std::vector<std::size_t>{ };
		const auto searching = !Filters.Search.trimmed().isEmpty();
		auto       matched   = std::vector<char>(Conversations.size(), 0);
		for (std::size_t conversation = 0U; conversation < Conversations.size(); ++conversation) {
			matched[conversation] = matches(Conversations[conversation]) ? 1 : 0;
		}
		std::optional<std::size_t> header;
		auto                       groupShown = false;
		for (std::size_t at = 0U; at < Items.size(); ++at) {
			const auto& entry = *Items[at];
			if (entry.Header) {
				header     = at;
				groupShown = false;
				continue;
			}
			if (matched[entry.Conversation] == 0) {
				continue;
			}
			if (!groupShown && header.has_value()) {
				shown.push_back(*header);
				groupShown = true;
			}
			if (searching || !Folded.contains(entry.Path)) {
				shown.push_back(at);
			}
		}
		{
			// Removing rows moves the list's current row to a neighbour; the
			// cursor is only ever where the reviewer put it, so it stays on
			// its row or, when its row is filtered out, is on none (as the
			// prototype's is).
			const QSignalBlocker blocker{ List->selectionModel() };
			Model->apply(shown, false);
			if (const auto at = item_index(Cursor); at.has_value()) {
				List->selectionModel()->setCurrentIndex(Model->index(static_cast<int>(*at)), QItemSelectionModel::ClearAndSelect);
			} else {
				Cursor.clear();
				Bead = -1;
				List->selectionModel()->clear();
			}
		}
		Pages->setCurrentWidget(Conversations.empty() ? NoneOnFile : shown.empty() ? Empty
																				   : static_cast<QWidget*>(List));
		refresh_bar();
		List->viewport()->update();
	}

	void conversation_finder::refresh_bar()
	{
		const auto fresh = static_cast<std::size_t>(std::ranges::count_if(Conversations, [](const finder_conversation& aConversation) { return aConversation.Unread > 0U; }));
		OnlyNew->set_count(QString::number(fresh));
		const auto onlyNew = QStringLiteral("Only threads with new replies (%1)").arg(fresh);
		OnlyNew->setToolTip(onlyNew);
		OnlyNew->setAccessibleName(onlyNew);
		LabelChip->set_mark(Filters.Label);
		LabelChip->set_on(Filters.Label.has_value());
		const auto label = Filters.Label.has_value() ? QStringLiteral("Only threads with a %1").arg(feedback_label_text(*Filters.Label))
													 : QStringLiteral("Only threads with a label");
		LabelChip->setToolTip(label);
		LabelChip->setAccessibleName(label);
	}

	void conversation_finder::refresh_colours()
	{
		const auto colours = note_colours_of(palette(), New, Route);
		for (auto* const chip : { OnlyNew, HideResolved, LabelChip }) {
			if (chip != nullptr) {
				chip->set_colours(colours, MarkTheme);
			}
		}
		if (auto* const box = Search != nullptr ? dynamic_cast<finder_search_box*>(Search->parentWidget()) : nullptr; box != nullptr) {
			box->set_colours(colours, QKeySequence{ QStringLiteral("Ctrl+J") }.toString(QKeySequence::NativeText));
		}
		if (List != nullptr) {
			List->viewport()->update();
		}
	}

	void conversation_finder::open_label_menu()
	{
		auto* const menu = new QMenu{ this };
		menu->setObjectName(QStringLiteral("threadFinderLabelMenu"));
		menu->setAttribute(Qt::WA_DeleteOnClose, true);
		auto* const group = new QActionGroup{ menu };
		group->setExclusive(true);
		const auto  dpr = devicePixelRatioF();
		auto* const any = menu->addAction(conversation_icon(conversation_glyph::tag, palette()),
										  QStringLiteral("Any label\t%1").arg(Conversations.size()));
		any->setObjectName(QStringLiteral("threadFinderAnyLabel"));
		any->setCheckable(true);
		any->setChecked(!Filters.Label.has_value());
		group->addAction(any);
		QObject::connect(any, &QAction::triggered, this, [this] {
			auto next  = Filters;
			next.Label = std::nullopt;
			set_filters(std::move(next));
		});
		menu->addSeparator();
		const auto counts = label_counts();
		if (!Labels.has_value()) {
			auto* const waiting = menu->addAction(QStringLiteral("Labels are counted once the pull request is read"));
			waiting->setEnabled(false);
		}
		for (const auto& [label, count] : counts) {
			auto* const action = menu->addAction(
				QIcon{ feedback_mark_pixmap(label, { }, feedback_mark_colours_for(MarkTheme, label, palette()), dpr) },
				QStringLiteral("%1\t%2").arg(feedback_label_text(label)).arg(count));
			action->setObjectName(QStringLiteral("threadFinderLabel_%1").arg(QString::fromLatin1(core::definition_of(label).Id.data(), static_cast<qsizetype>(core::definition_of(label).Id.size()))));
			action->setCheckable(true);
			action->setChecked(Filters.Label == label);
			group->addAction(action);
			QObject::connect(action, &QAction::triggered, this, [this, label] {
				auto next  = Filters;
				next.Label = label;
				set_filters(std::move(next));
			});
		}
		menu->popup(LabelChip->mapToGlobal(QPoint{ 0, LabelChip->height() + 6 }));
	}

	void conversation_finder::pick(const std::size_t aItem, const std::optional<std::size_t> aBead, const core::workbench_thread_destination aDestination)
	{
		if (aItem >= Items.size() || Items[aItem]->Header) {
			return;
		}
		const auto& conversation = Conversations[Items[aItem]->Conversation];
		const auto& row          = conversation.Rows[Items[aItem]->Row];
		auto        post         = row.Posts.front();
		if (aBead.has_value()) {
			const auto beads = beads_of(aItem);
			if (*aBead < beads.size()) {
				post = beads[*aBead].Post;
			}
		}
		hide_preview();
		if (PickDispatch && (aDestination != core::workbench_thread_destination::review || !conversation.Path.isEmpty())) {
			// A copy: the dispatch leaves this page, which may rebuild it.
			const auto location = finder_location{ .Thread = conversation.Id, .Post = conversation.Posts[post].Id, .Path = conversation.Path };
			PickDispatch(location, aDestination);
		}
	}

	auto conversation_finder::discuss_shown(const std::size_t aItem) const -> bool
	{
		if (aItem >= Items.size() || Items[aItem]->Header || Conversations[Items[aItem]->Conversation].Draft) {
			return false;
		}
		return HoveredItem == aItem || (!Cursor.empty() && Items[aItem]->Key == Cursor);
	}

	auto conversation_finder::review_shown(const std::size_t aItem) const -> bool
	{
		if (aItem >= Items.size() || Items[aItem]->Header || Conversations[Items[aItem]->Conversation].Path.isEmpty()) {
			return false;
		}
		return HoveredItem == aItem || (!Cursor.empty() && Items[aItem]->Key == Cursor);
	}

	void conversation_finder::popup_menu(const std::size_t aItem, const std::optional<std::size_t> aBead, const QPoint aGlobal)
	{
		if (aItem >= Items.size() || Items[aItem]->Header) {
			return;
		}
		hide_preview();
		hide_anchored_tooltip();
		MenuRow  = Items[aItem]->Key;
		MenuBead = aBead;
		if (RowMenu != nullptr) {
			RowMenu->close();
		}
		// [[BPR-1356]]. Made for this opening and gone when it closes, as the
		// code's menu is: the finder reads Return and Shift+Return itself, so
		// the items only show them, and no window ever holds those keys.
		auto* const menu = new QMenu{ this };
		menu->setObjectName(QStringLiteral("threadFinderRowMenu"));
		menu->setAccessibleName(QStringLiteral("Thread row"));
		menu->setToolTipsVisible(true);
		QObject::connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
		const auto makeItem = [menu](const QString& aText, const QString& aName, const shortcut_command aKey, const QString& aTip) {
			auto* const action = menu->addAction(aText);
			action->setObjectName(aName);
			action->setShortcuts(shortcut_keys(aKey));
			action->setShortcutContext(Qt::WidgetShortcut);
			action->setShortcutVisibleInContextMenu(true);
			action->setStatusTip(aTip);
			return action;
		};
		OpenReview        = makeItem(QStringLiteral("Open in Review"), QStringLiteral("threadFinderOpenInReviewAction"), shortcut_command::thread_open_review,
									 QStringLiteral("Read this thread beside its code"));
		OpenDiscuss       = makeItem(QStringLiteral("Open in Discuss"), QStringLiteral("threadFinderOpenInDiscussAction"), shortcut_command::thread_open_discuss,
									 QStringLiteral("Read this thread with the rest of the pull request's conversation"));
		const auto onFile = !Conversations[Items[aItem]->Conversation].Path.isEmpty();
		OpenReview->setEnabled(onFile);
		if (!onFile) {
			OpenReview->setToolTip(QStringLiteral("This comment is not on a file; open it in Discuss"));
		}
		menu->setDefaultAction(onFile ? OpenReview : OpenDiscuss);
		// My draft is written beside its line; Discuss does not list it.
		if (Conversations[Items[aItem]->Conversation].Draft) {
			OpenDiscuss->setEnabled(false);
			OpenDiscuss->setToolTip(QStringLiteral("Your draft is written beside its line, in Review"));
		}
		const auto menuPick = [this](const core::workbench_thread_destination aDestination) {
			const auto row = std::ranges::find(Items, MenuRow, [](const std::unique_ptr<item>& aEntry) { return aEntry->Key; });
			if (!MenuRow.empty() && row != Items.end()) {
				pick(static_cast<std::size_t>(row - Items.begin()), MenuBead, aDestination);
			}
		};
		QObject::connect(OpenReview, &QAction::triggered, this, [menuPick] { menuPick(core::workbench_thread_destination::review); });
		QObject::connect(OpenDiscuss, &QAction::triggered, this, [menuPick] { menuPick(core::workbench_thread_destination::discuss); });
		// [[BPR-1582]]. Mark as read or Mark as unread, as the core decides
		// from the thread's read state; never on my draft.
		const auto& conversation = Conversations[Items[aItem]->Conversation];
		if (!conversation.Draft) {
			menu->addSeparator();
			const auto others = std::ranges::any_of(conversation.Posts, [](const finder_post& aPost) { return !aPost.Mine; });
			const auto action = core::conversation_read_action_for(conversation.Unread > 0U, others);
			ReadAction        = menu->addAction(action == core::conversation_read_action::mark_read ? QStringLiteral("Mark as Read") : QStringLiteral("Mark as Unread"));
			ReadAction->setObjectName(QStringLiteral("threadFinderReadAction"));
			ReadAction->setEnabled(action != core::conversation_read_action::none && static_cast<bool>(ReadDispatch));
			ReadAction->setStatusTip(action == core::conversation_read_action::mark_read ? QStringLiteral("Mark every post in this thread as read")
										 : action == core::conversation_read_action::mark_unread
										 ? QStringLiteral("Show the latest post by someone else as new again")
										 : QStringLiteral("Every post in this thread is yours"));
			ReadAction->setToolTip(ReadAction->statusTip());
			QObject::connect(ReadAction, &QAction::triggered, this, [this, thread = conversation.Id, read = action == core::conversation_read_action::mark_read] {
				if (ReadDispatch) {
					ReadDispatch(thread, read);
				}
			});
		}
		RowMenu = menu;
		menu->popup(aGlobal, OpenReview);
	}

	void conversation_finder::popup_row_menu(const std::string& aRowKey, const QPoint aPosition)
	{
		const auto at = item_index(aRowKey);
		if (!at.has_value()) {
			return;
		}
		auto       bead     = std::optional<std::size_t>{ };
		const auto item     = Model->Shown[*at];
		const auto geometry = geometry_of(item, List->visualRect(Model->index(static_cast<int>(*at))));
		for (std::size_t index = 0U; index < geometry.Beads.size(); ++index) {
			if (geometry.Beads[index].adjusted(-1, -1, 1, 1).contains(aPosition)) {
				bead = index;
				break;
			}
		}
		List->setCurrentIndex(Model->index(static_cast<int>(*at)));
		popup_menu(item, bead, List->viewport()->mapToGlobal(aPosition));
	}

	auto conversation_finder::row_menu() const noexcept -> QMenu*
	{
		return RowMenu;
	}

	auto conversation_finder::open_in_review_action() const noexcept -> QAction*
	{
		return OpenReview;
	}

	auto conversation_finder::open_in_discuss_action() const noexcept -> QAction*
	{
		return OpenDiscuss;
	}

	auto conversation_finder::read_action() const noexcept -> QAction*
	{
		return ReadAction;
	}

	void conversation_finder::set_read_dispatch(finder_read_dispatch aDispatch)
	{
		ReadDispatch = std::move(aDispatch);
	}

	void conversation_finder::set_reading_width(const core::reading_width aWidth)
	{
		if (ReadingWidth == aWidth) {
			return;
		}
		ReadingWidth = aWidth;
		List->doItemsLayout();
		List->viewport()->update();
	}

	auto conversation_finder::reading_width() const noexcept -> core::reading_width
	{
		return ReadingWidth;
	}

	auto conversation_finder::review_button_rect(const std::string& aRowKey) const -> QRect
	{
		const auto at = item_index(aRowKey);
		if (!at.has_value()) {
			return { };
		}
		return geometry_of(Model->Shown[*at], List->visualRect(Model->index(static_cast<int>(*at)))).Review;
	}

	auto conversation_finder::trailing_rect(const std::string& aRowKey) const -> QRect
	{
		const auto at = item_index(aRowKey);
		if (!at.has_value()) {
			return { };
		}
		const auto geometry = geometry_of(Model->Shown[*at], List->visualRect(Model->index(static_cast<int>(*at))));
		auto       left     = geometry.State.left();
		if (!geometry.Fold.isEmpty()) {
			left = std::min(left, geometry.Fold.left());
		}
		for (const auto& bead : geometry.Beads) {
			left = std::min(left, bead.left());
		}
		return QRect{ QPoint{ left, geometry.State.top() }, QPoint{ geometry.Box.right(), geometry.State.bottom() } };
	}

	auto conversation_finder::discuss_button_rect(const std::string& aRowKey) const -> QRect
	{
		const auto at = item_index(aRowKey);
		if (!at.has_value()) {
			return { };
		}
		return geometry_of(Model->Shown[*at], List->visualRect(Model->index(static_cast<int>(*at)))).Discuss;
	}

	void conversation_finder::toggle_fold(const QString& aPath)
	{
		if (!Folded.erase(aPath)) {
			Folded.insert(aPath);
		}
		Cursor = std::string{ file_key_prefix } + aPath.toStdString();
		apply_filters();
		if (const auto at = item_index(Cursor); at.has_value()) {
			List->setCurrentIndex(Model->index(static_cast<int>(*at)));
		}
	}

	void conversation_finder::move_bead(const int aStep)
	{
		const auto at = item_index(Cursor);
		if (!at.has_value()) {
			return;
		}
		const auto item  = Model->Shown[*at];
		const auto beads = beads_of(item);
		Bead             = std::clamp(Bead + aStep, -1, static_cast<int>(beads.size()) - 1);
		if (Bead < 0) {
			hide_preview();
		} else {
			const auto geometry = geometry_of(item, List->visualRect(Model->index(static_cast<int>(*at))));
			if (static_cast<std::size_t>(Bead) < geometry.Beads.size()) {
				show_preview(item, beads[static_cast<std::size_t>(Bead)].Post, geometry.Beads[static_cast<std::size_t>(Bead)]);
			}
		}
		List->viewport()->update();
	}

	void conversation_finder::show_preview(const std::size_t aItem, const std::size_t aPost, const QRect& aBead)
	{
		const auto& conversation = Conversations[Items[aItem]->Conversation];
		const auto& post         = conversation.Posts[aPost];
		auto        answers      = QString{ };
		if (conversation.Rows.size() > 1U && post.Parent.has_value()) {
			const auto order = reading_order(conversation.Posts);
			const auto at    = std::ranges::find(order, aPost);
			if (at != order.begin() && at != order.end() && *(at - 1) != *post.Parent) {
				answers = who(conversation.Posts[*post.Parent]);
			}
		}
		Preview->present(post, answers, note_colours_of(palette(), New, Route), MarkTheme);
		auto       global = List->viewport()->mapToGlobal(QPoint{ aBead.left() - 12, aBead.bottom() + 8 });
		const auto screen = List->screen() != nullptr ? List->screen()->availableGeometry() : QRect{ };
		if (screen.isValid()) {
			global.setX(std::clamp(global.x(), screen.left() + 8, std::max(screen.left() + 8, screen.right() - Preview->width() - 8)));
			if (global.y() + Preview->height() > screen.bottom() - 8) {
				global.setY(List->viewport()->mapToGlobal(QPoint{ 0, aBead.top() - 8 - Preview->height() }).y());
			}
		}
		Preview->move(global);
		Preview->show();
		Preview->raise();
	}

	void conversation_finder::hide_preview()
	{
		if (Preview != nullptr && Preview->isVisible()) {
			Preview->hide();
		}
	}

	auto conversation_finder::handle_key(QKeyEvent& aEvent, const bool aInSearch) -> bool
	{
		const auto plain = !(aEvent.modifiers() & (Qt::ControlModifier | Qt::MetaModifier | Qt::AltModifier));
		const auto at    = item_index(Cursor);
		switch (aEvent.key()) {
		case Qt::Key_Escape:
			if (!Search->text().isEmpty()) {
				Search->clear();
				Search->setFocus(Qt::OtherFocusReason);
			} else if (LeaveDispatch) {
				hide_preview();
				LeaveDispatch();
			}
			return true;
		case Qt::Key_Down:
		case Qt::Key_Up:   {
			if (!plain) {
				return false;
			}
			const auto down = aEvent.key() == Qt::Key_Down;
			auto       next = aInSearch ? (down ? 0 : -1) : (at.has_value() ? static_cast<int>(*at) : -1) + (down ? 1 : -1);
			if (next < 0) {
				Cursor.clear();
				Bead = -1;
				hide_preview();
				List->selectionModel()->clearCurrentIndex();
				Search->setFocus(Qt::OtherFocusReason);
				return true;
			}
			if (Model->Shown.empty()) {
				return true;
			}
			next             = std::min(next, static_cast<int>(Model->Shown.size()) - 1);
			const auto index = Model->index(next);
			List->setFocus(Qt::OtherFocusReason);
			List->setCurrentIndex(index);
			List->scrollTo(index, QAbstractItemView::EnsureVisible);
			return true;
		}
		case Qt::Key_Left:
		case Qt::Key_Right: {
			if (aInSearch || !at.has_value() || !plain) {
				return false;
			}
			const auto item = Model->Shown[*at];
			if (Items[item]->Header) {
				const auto fold = aEvent.key() == Qt::Key_Left;
				if (fold != Folded.contains(Items[item]->Path)) {
					toggle_fold(Items[item]->Path);
				}
				return true;
			}
			move_bead(aEvent.key() == Qt::Key_Right ? 1 : -1);
			return true;
		}
		case Qt::Key_Return:
		case Qt::Key_Enter:  {
			auto target = at;
			if (aInSearch && !target.has_value()) {
				for (std::size_t shown = 0U; shown < Model->Shown.size(); ++shown) {
					if (!Items[Model->Shown[shown]]->Header) {
						target = shown;
						break;
					}
				}
			}
			if (!target.has_value()) {
				return true;
			}
			const auto item = Model->Shown[*target];
			if (Items[item]->Header) {
				toggle_fold(Items[item]->Path);
				return true;
			}
			// [[BPR-1356]]. Shift+Return opens the thread in Discuss.
			const auto destination = aEvent.modifiers().testFlag(Qt::ShiftModifier) ? core::workbench_thread_destination::discuss
																					: core::workbench_thread_destination::review;
			pick(item, Bead >= 0 && !aInSearch ? std::optional<std::size_t>{ static_cast<std::size_t>(Bead) } : std::nullopt, destination);
			return true;
		}
		case Qt::Key_Menu:
		case Qt::Key_F10:  {
			// [[BPR-1356]]. The menu key and Shift+F10 open the cursor's row
			// menu from the list; the search keeps its own.
			if (aInSearch || (aEvent.key() == Qt::Key_F10 && aEvent.modifiers() != Qt::ShiftModifier)) {
				return false;
			}
			if (!at.has_value() || Items[Model->Shown[*at]]->Header) {
				return true;
			}
			const auto item = Model->Shown[*at];
			const auto rect = List->visualRect(Model->index(static_cast<int>(*at)));
			popup_menu(item, Bead >= 0 ? std::optional<std::size_t>{ static_cast<std::size_t>(Bead) } : std::nullopt,
					   List->viewport()->mapToGlobal(QPoint{ rect.left() + list_left + line_width, rect.bottom() }));
			return true;
		}
		default:
			break;
		}
		// Typing anywhere in the finder goes to the search.
		if (!aInSearch && plain && !aEvent.text().isEmpty() && aEvent.text().front().isPrint()) {
			Search->setFocus(Qt::OtherFocusReason);
			Search->end(false);
			Search->insert(aEvent.text());
			return true;
		}
		return false;
	}

	auto conversation_finder::item_index(const std::string& aKey) const -> std::optional<std::size_t>
	{
		if (aKey.empty()) {
			return std::nullopt;
		}
		for (std::size_t shown = 0U; shown < Model->Shown.size(); ++shown) {
			if (Items[Model->Shown[shown]]->Key == aKey) {
				return shown;
			}
		}
		return std::nullopt;
	}

	auto conversation_finder::item_key(const std::size_t aItem) const -> std::string
	{
		return aItem < Items.size() ? Items[aItem]->Key : std::string{ };
	}

	auto conversation_finder::open_conversation() const -> std::optional<std::size_t>
	{
		if (Reading.Thread.empty()) {
			return std::nullopt;
		}
		const auto found = std::ranges::find(Conversations, Reading.Thread, &finder_conversation::Id);
		return found == Conversations.end() ? std::nullopt : std::optional{ static_cast<std::size_t>(found - Conversations.begin()) };
	}

	auto conversation_finder::path_posts() const -> std::set<std::size_t>
	{
		// The posts from the first comment to the picked one, in the open
		// conversation.
		auto       path = std::set<std::size_t>{ };
		const auto open = open_conversation();
		if (!open.has_value() || Reading.Post.empty()) {
			return path;
		}
		const auto& posts  = Conversations[*open].Posts;
		const auto  picked = std::ranges::find(posts, Reading.Post, &finder_post::Id);
		if (picked == posts.end()) {
			return path;
		}
		auto at = static_cast<std::size_t>(picked - posts.begin());
		for (;;) {
			path.insert(at);
			if (at == 0U) {
				break;
			}
			const auto parent = posts[at].Parent.value_or(0U);
			at                = parent < at ? parent : 0U;
		}
		return path;
	}

	auto conversation_finder::beads_of(const std::size_t aItem) const -> std::vector<bead>
	{
		auto beads = std::vector<bead>{ };
		if (aItem >= Items.size() || Items[aItem]->Header) {
			return beads;
		}
		const auto& conversation = Conversations[Items[aItem]->Conversation];
		const auto& row          = conversation.Rows[Items[aItem]->Row];
		auto        keep         = std::vector<char>(row.Posts.size(), 1);
		if (row.Posts.size() > maximum_beads && !Unfolded.contains(Items[aItem]->Key)) {
			// Fold the oldest read posts; new ones, the path to the picked
			// one, the search's and the label's never fold.
			const auto open   = open_conversation();
			const auto path   = open.has_value() && *open == Items[aItem]->Conversation ? path_posts() : std::set<std::size_t>{ };
			const auto search = Filters.Search.trimmed();
			auto       kept   = std::size_t{ 0U };
			for (std::size_t at = 0U; at < row.Posts.size(); ++at) {
				const auto& post = conversation.Posts[row.Posts[at]];
				keep[at]         = post.Unread || path.contains(row.Posts[at]) || finder_post_found(post, search)
						|| (Filters.Label.has_value() && post.Label == Filters.Label)
					? 1
					: 0;
				kept += keep[at] != 0 ? 1U : 0U;
			}
			auto room = maximum_beads > kept ? maximum_beads - kept : 0U;
			for (auto at = row.Posts.size(); at-- > 0U && room > 0U;) {
				if (keep[at] == 0) {
					keep[at] = 1;
					--room;
				}
			}
		}
		for (std::size_t at = 0U; at < row.Posts.size(); ++at) {
			if (keep[at] != 0) {
				beads.push_back({ .Post = row.Posts[at] });
			}
		}
		return beads;
	}

	auto conversation_finder::geometry_of(const std::size_t aItem, const QRect& aRect) const -> row_geometry
	{
		auto geometry = row_geometry{ };
		// [[BPR-1584]]. The rows fill the page up to the reading width; the
		// people, counts and state stay at their right end.
		const auto page  = std::max(0, aRect.width() - list_left - list_right);
		const auto width = static_cast<int>(core::reading_column_width(ReadingWidth, static_cast<std::uint32_t>(page)));
		geometry.Box     = QRect{ aRect.left() + list_left, aRect.bottom() - row_height + 1, std::max(0, width), row_height };
		if (aItem >= Items.size() || Items[aItem]->Header) {
			return geometry;
		}
		const auto& box          = geometry.Box;
		const auto& conversation = Conversations[Items[aItem]->Conversation];
		const auto& row          = conversation.Rows[Items[aItem]->Row];
		geometry.Line            = QRect{ box.left(), box.top(), line_width, row_height };
		geometry.Connectors      = QRect{ geometry.Line.right() + 1, box.top(), static_cast<int>(row.Level) * level_width, row_height };
		geometry.Mark            = QRect{ geometry.Connectors.right() + 1, box.top() + (row_height - mark_size) / 2, mark_size, mark_size };
		const auto right         = box.right() - row_padding;
		geometry.State           = QRect{ right - state_width + 1, box.top(), state_width, row_height };
		const auto beads         = beads_of(aItem);
		auto       x             = geometry.State.left();
		const auto top           = box.top() + (row_height - bead_size) / 2;
		geometry.Beads.resize(beads.size());
		for (auto at = beads.size(); at-- > 0U;) {
			x -= bead_size;
			geometry.Beads[at] = QRect{ x, top, bead_size, bead_size };
			x -= bead_gap;
		}
		if (const auto folded = row.Posts.size() - beads.size(); folded > 0U) {
			const auto text = QStringLiteral("+%1").arg(folded);
			const auto bold = weighted(sized(font(), 0.75), QFont::Bold);
			const auto pill = std::max(bead_size, QFontMetrics{ bold }.horizontalAdvance(text) + 10);
			x -= pill;
			geometry.Fold = QRect{ x, top, pill, bead_size };
			x -= bead_gap;
		}
		const auto wordsLeft  = geometry.Mark.right() + 1 + mark_gap;
		auto       wordsRight = x + bead_gap - people_gap;
		const auto label      = weighted(sized(font(), 0.82), QFont::DemiBold);
		const auto pillTop    = box.top() + (row_height - discuss_height) / 2;
		if (discuss_shown(aItem)) {
			// [[BPR-1356]]. The button takes the end of the words, so the
			// people never move when it shows.
			const auto pillWidth = QFontMetrics{ label }.horizontalAdvance(QStringLiteral("Discuss")) + 2 * discuss_padding;
			geometry.Discuss     = QRect{ wordsRight - pillWidth, pillTop, pillWidth, discuss_height };
			wordsRight           = geometry.Discuss.left() - discuss_gap / 2;
		}
		if (review_shown(aItem)) {
			// [[BPR-1582]]. Review first, then Discuss: one pair, as the menu
			// orders them.
			const auto pillWidth = QFontMetrics{ label }.horizontalAdvance(QStringLiteral("Review")) + 2 * discuss_padding;
			geometry.Review      = QRect{ wordsRight - pillWidth, pillTop, pillWidth, discuss_height };
			wordsRight           = geometry.Review.left() - discuss_gap;
		} else if (!geometry.Discuss.isEmpty()) {
			wordsRight = geometry.Discuss.left() - discuss_gap;
		}
		geometry.Words = QRect{ wordsLeft, box.top(), std::max(0, wordsRight - wordsLeft), row_height };
		return geometry;
	}

	auto conversation_finder::accessible_text(const std::size_t aItem) const -> QString
	{
		if (aItem >= Items.size()) {
			return { };
		}
		const auto& entry = *Items[aItem];
		if (entry.Header) {
			auto count = 0;
			auto fresh = 0;
			for (const auto& conversation : Conversations) {
				if (conversation.Path == entry.Path) {
					++count;
					fresh += conversation.Unread > 0U ? 1 : 0;
				}
			}
			auto text = entry.Path.isEmpty()
				? QStringLiteral("Not on a file, %1, %2 new").arg(count == 1 ? QStringLiteral("one comment") : QStringLiteral("%1 comments").arg(count)).arg(fresh)
				: QStringLiteral("%1, %2, %3").arg(base_name(entry.Path), folder_of(entry.Path), count == 1 ? QStringLiteral("one thread") : QStringLiteral("%1 threads").arg(count));
			if (fresh > 0 && !entry.Path.isEmpty()) {
				text += fresh == 1 ? QStringLiteral(", one with new replies") : QStringLiteral(", %1 with new replies").arg(fresh);
			}
			if (!entry.Path.isEmpty() && Reading.File == entry.File) {
				text += QStringLiteral(", open in Review");
			}
			text += Folded.contains(entry.Path) && Filters.Search.trimmed().isEmpty() ? QStringLiteral(", collapsed") : QStringLiteral(", expanded");
			return text;
		}
		const auto& conversation = Conversations[entry.Conversation];
		const auto& row          = conversation.Rows[entry.Row];
		const auto& first        = conversation.Posts[row.Posts.front()];
		auto        text         = entry.Row == 0U ? (conversation.Line != 0U ? QStringLiteral("Line %1").arg(conversation.Line) : QStringLiteral("File"))
												   : QStringLiteral("Branch");
		text += QStringLiteral(": %1, ").arg(who(first));
		if (first.Label.has_value()) {
			text += feedback_label_text(*first.Label) + QStringLiteral(", ");
		}
		text += first_words(first.Words, 80);
		if (entry.Row == 0U) {
			text += conversation.Posts.size() == 1U ? QStringLiteral(", one post") : QStringLiteral(", %1 posts").arg(conversation.Posts.size());
			if (conversation.Draft) {
				text += QStringLiteral(", your draft");
			} else if (conversation.Unread > 0U) {
				text += QStringLiteral(", %1 new").arg(conversation.Unread);
			}
			if (conversation.Resolved) {
				text += QStringLiteral(", resolved");
			}
		} else {
			text += QStringLiteral(", %1 posts").arg(row.Posts.size());
		}
		return text;
	}

	void conversation_finder::paint_header(QPainter& aPainter, const QString& aPath, const QRect& aRect, const bool aHovered, const bool aFocused) const
	{
		const auto colours = note_colours_of(palette(), New, Route);
		const auto page    = std::max(0, aRect.width() - list_left - list_right);
		const auto width   = static_cast<int>(core::reading_column_width(ReadingWidth, static_cast<std::uint32_t>(page)));
		const auto box     = QRect{ aRect.left() + list_left, aRect.bottom() - row_height + 1, width, row_height };
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		if (aFocused) {
			aPainter.setPen(QPen{ colours.Signal, 2.0 });
			aPainter.setBrush(Qt::NoBrush);
			aPainter.drawRoundedRect(QRectF{ box }.adjusted(1.0, 1.0, -1.0, -1.0), 6.0, 6.0);
		}
		const auto folded = Folded.contains(aPath) && Filters.Search.trimmed().isEmpty();
		auto       x      = box.left() + 8;
		const auto mid    = box.top() + row_height / 2;
		aPainter.save();
		aPainter.translate(x + 7.0, mid);
		aPainter.rotate(folded ? -90.0 : 0.0);
		paint_conversation_glyph(aPainter, QRectF{ -7.0, -7.0, 14.0, 14.0 }, conversation_glyph::down, colours.Faint);
		aPainter.restore();
		x += 14 + 7;
		const auto bold = weighted(font(), QFont::DemiBold);
		const auto name = aPath.isEmpty() ? QStringLiteral("Not on a file") : base_name(aPath);
		aPainter.setFont(bold);
		aPainter.setPen(colours.Ink);
		const auto nameWidth = std::min(QFontMetrics{ bold }.horizontalAdvance(name), std::max(0, box.right() - row_padding - 30 - x));
		aPainter.drawText(QRect{ x, box.top(), nameWidth + 1, row_height }, Qt::AlignLeft | Qt::AlignVCenter,
						  QFontMetrics{ bold }.elidedText(name, Qt::ElideRight, nameWidth + 1));
		if (aHovered) {
			aPainter.setPen(QPen{ colours.Rule2, 1.0 });
			aPainter.drawLine(QPointF{ static_cast<qreal>(x), mid + QFontMetrics{ bold }.ascent() / 2.0 + 3.0 },
							  QPointF{ static_cast<qreal>(x + nameWidth), mid + QFontMetrics{ bold }.ascent() / 2.0 + 3.0 });
		}
		x += nameWidth + 7;
		const auto here  = !aPath.isEmpty() && Reading.File.has_value() && *Reading.File == core::file_identity_of(aPath.toStdString());
		auto       fresh = std::size_t{ 0U };
		for (const auto& conversation : Conversations) {
			if (conversation.Path == aPath && conversation.Unread > 0U) {
				fresh += conversation.Unread;
			}
		}
		const auto dotsRight = box.right() - row_padding;
		const auto dirRight  = dotsRight - (fresh ? 7 + 12 : 0) - (here ? 7 + 7 : 0);
		const auto small     = sized(font(), 0.88);
		aPainter.setFont(small);
		aPainter.setPen(colours.Faint);
		const auto folder = aPath.isEmpty() ? QStringLiteral("%1 new").arg(fresh) : folder_of(aPath);
		if (dirRight > x) {
			aPainter.drawText(QRect{ x, box.top(), dirRight - x, row_height }, Qt::AlignLeft | Qt::AlignVCenter,
							  QFontMetrics{ small }.elidedText(folder, Qt::ElideLeft, dirRight - x));
			x = std::min(dirRight, x + QFontMetrics{ small }.horizontalAdvance(folder)) + 7;
		}
		aPainter.setPen(Qt::NoPen);
		if (here) {
			aPainter.setBrush(colours.Route);
			aPainter.drawEllipse(QRectF{ static_cast<qreal>(x), mid - 3.5, 7.0, 7.0 });
		}
		if (fresh) {
			aPainter.setBrush(colours.New);
			aPainter.drawEllipse(QRectF{ static_cast<qreal>(dotsRight - 12 - 7), mid - 3.5, 7.0, 7.0 });
		}
		aPainter.restore();
	}

	void conversation_finder::paint_item(QPainter& aPainter, const std::size_t aItem, const QRect& aRect, const bool aHovered, const bool aFocused) const
	{
		if (aItem >= Items.size()) {
			return;
		}
		const auto& entry = *Items[aItem];
		if (entry.Header) {
			paint_header(aPainter, entry.Path, aRect, aHovered, aFocused);
			return;
		}
		const auto  colours      = note_colours_of(palette(), New, Route);
		const auto& conversation = Conversations[entry.Conversation];
		const auto& row          = conversation.Rows[entry.Row];
		const auto  geometry     = geometry_of(aItem, aRect);
		const auto  open         = open_conversation();
		const auto  isOpen       = open.has_value() && *open == entry.Conversation;
		const auto  current      = isOpen && entry.Row == 0U;
		const auto  path         = isOpen ? path_posts() : std::set<std::size_t>{ };
		const auto  onPath       = entry.Row != 0U && path.contains(row.Posts.front());
		const auto  search       = Filters.Search.trimmed();
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		const auto box = QRectF{ geometry.Box };
		// The row's ground: where you are, under the pointer, or plain.
		auto ground = colours.Panel;
		if (current) {
			ground = blend(colours.Panel, colours.Route, colours.Dark ? 0.22 : 0.13);
		} else if (aHovered) {
			ground = colours.Hover;
		}
		if (ground != colours.Panel) {
			aPainter.setPen(Qt::NoPen);
			aPainter.setBrush(ground);
			aPainter.drawRoundedRect(box, 7.0, 7.0);
		}
		if (current) {
			auto bar = QPainterPath{ };
			bar.addRoundedRect(QRectF{ box.left(), box.top() + 5.0, 3.0, box.height() - 10.0 }, 1.5, 1.5);
			aPainter.fillPath(bar, colours.Route);
		}
		if (aFocused) {
			aPainter.setPen(QPen{ colours.Signal, 2.0 });
			aPainter.setBrush(Qt::NoBrush);
			aPainter.drawRoundedRect(box.adjusted(1.0, 1.0, -1.0, -1.0), 6.0, 6.0);
		}
		// The line.
		if (entry.Row == 0U && conversation.Line != 0U) {
			auto lineFont = code_font_like(CodeFont, font(), 0.92);
			lineFont.setWeight(current ? QFont::Bold : QFont::Normal);
			aPainter.setFont(lineFont);
			aPainter.setPen(current ? colours.Ink : colours.Faint);
			aPainter.drawText(geometry.Line.adjusted(0, 0, -line_gap, 0), Qt::AlignRight | Qt::AlignVCenter, QString::number(conversation.Line));
		}
		// The outline to a branch.
		const auto mid = geometry.Box.top() + row_height / 2.0;
		for (std::size_t level = 0U; level < row.Connectors.size(); ++level) {
			const auto connector = row.Connectors[level];
			const auto x         = geometry.Connectors.left() + static_cast<qreal>(level * level_width) + 8.75;
			const auto last      = level + 1U == row.Connectors.size();
			const auto ink       = last && onPath ? colours.Route : colours.Rule2;
			aPainter.setPen(QPen{ ink, 1.5 });
			const auto top    = geometry.Box.top() - 1.0;
			const auto bottom = geometry.Box.bottom() + 2.0;
			switch (connector) {
			case finder_connector::blank:
				break;
			case finder_connector::pipe:
				aPainter.drawLine(QPointF{ x, top }, QPointF{ x, bottom });
				break;
			case finder_connector::tee:
				aPainter.setPen(QPen{ colours.Rule2, 1.5 });
				aPainter.drawLine(QPointF{ x, top }, QPointF{ x, bottom });
				aPainter.setPen(QPen{ ink, 1.5 });
				aPainter.drawLine(QPointF{ x, mid }, QPointF{ x + level_width - 10.0, mid });
				break;
			case finder_connector::end:
				aPainter.drawLine(QPointF{ x, top }, QPointF{ x, mid });
				aPainter.drawLine(QPointF{ x, mid }, QPointF{ x + level_width - 10.0, mid });
				break;
			}
		}
		if (row.Forks) {
			// The stem from this row's mark to the branches under it.
			const auto x = geometry.Mark.left() + 8.75;
			aPainter.setPen(QPen{ colours.Rule2, 1.5 });
			aPainter.drawLine(QPointF{ x, geometry.Mark.bottom() + 2.0 }, QPointF{ x, geometry.Box.bottom() + 2.0 });
		}
		// The words come from the row's first post, or the post the search found on it.
		auto shown = row.Posts.front();
		if (!search.isEmpty()) {
			for (const auto post : row.Posts) {
				if (finder_post_found(conversation.Posts[post], search)) {
					shown = post;
					break;
				}
			}
		}
		const auto& words = conversation.Posts[shown];
		const auto  faded = conversation.Resolved && !aHovered ? 0.5 : conversation.Resolved ? 0.85
																							 : 1.0;
		aPainter.setOpacity(faded);
		const auto mark = QRectF{ geometry.Mark };
		if (conversation.Draft) {
			auto dashed = QPen{ colours.Faint, 1.5 };
			dashed.setStyle(Qt::DashLine);
			aPainter.setPen(dashed);
			aPainter.setBrush(Qt::NoBrush);
			aPainter.drawRoundedRect(mark.adjusted(1.0, 1.0, -1.0, -1.0), 3.0, 3.0);
			paint_conversation_glyph(aPainter, mark.adjusted(3.5, 3.5, -3.5, -3.5), conversation_glyph::lock, colours.Faint);
		} else if (words.Label.has_value()) {
			paint_feedback_mark(aPainter, mark, *words.Label, words.Emphasis, feedback_mark_colours_for(MarkTheme, *words.Label, palette()));
		} else {
			aPainter.setPen(QPen{ colours.Faint, 1.5 });
			aPainter.setBrush(blend(colours.Panel, colours.Faint, 0.18));
			aPainter.drawRoundedRect(mark.adjusted(2.75, 2.75, -2.75, -2.75), 3.0, 3.0);
		}
		// The words, eliding at the right, with code in the code font and the
		// search's match washed.
		{
			auto       x        = geometry.Words.left();
			const auto right    = geometry.Words.right();
			const auto ink      = entry.Row == 0U ? colours.Ink : colours.Ink2;
			const auto codeFont = code_font_like(CodeFont, font(), 0.9);
			const auto runs     = runs_of(words.Words, search);
			for (std::size_t at = 0U; at < runs.size() && x < right; ++at) {
				const auto& run     = runs[at];
				const auto  runFont = run.Code ? codeFont : font();
				const auto  metrics = QFontMetrics{ runFont };
				auto        text    = run.Text;
				auto        advance = metrics.horizontalAdvance(text);
				if (x + advance > right) {
					text    = metrics.elidedText(text, Qt::ElideRight, right - x);
					advance = metrics.horizontalAdvance(text);
				}
				const auto rect = QRect{ x, geometry.Words.top(), advance + 1, row_height };
				if (run.Match) {
					aPainter.setPen(Qt::NoPen);
					aPainter.setBrush(alpha(colours.Signal, 0.22));
					aPainter.drawRoundedRect(QRectF{ rect }.adjusted(0.0, (row_height - metrics.height()) / 2.0, 0.0, -(row_height - metrics.height()) / 2.0), 2.0, 2.0);
				}
				aPainter.setFont(runFont);
				aPainter.setPen(ink);
				aPainter.drawText(rect, Qt::AlignLeft | Qt::AlignVCenter, text);
				x += advance;
				if (text != run.Text) {
					break;
				}
			}
		}
		// The people, one bead per post in posting order.
		const auto beads       = beads_of(aItem);
		const auto labelFilter = Filters.Label.has_value();
		const auto picked      = isOpen ? Reading.Post : std::string{ };
		const auto keyboard    = aFocused && Bead >= 0 ? std::optional<std::size_t>{ static_cast<std::size_t>(Bead) } : std::nullopt;
		if (!geometry.Fold.isEmpty()) {
			const auto pill = QRectF{ geometry.Fold }.adjusted(0.5, 0.5, -0.5, -0.5);
			aPainter.setPen(QPen{ colours.Rule, 1.0 });
			aPainter.setBrush(blend(colours.Panel, colours.Ink, 0.04));
			aPainter.drawRoundedRect(pill, 8.0, 8.0);
			aPainter.setFont(weighted(sized(font(), 0.75), QFont::Bold));
			aPainter.setPen(colours.Faint);
			aPainter.drawText(geometry.Fold, Qt::AlignCenter, QStringLiteral("+%1").arg(row.Posts.size() - beads.size()));
		}
		for (std::size_t at = 0U; at < beads.size() && at < geometry.Beads.size(); ++at) {
			const auto& post    = conversation.Posts[beads[at].Post];
			const auto  rect    = QRectF{ geometry.Beads[at] };
			const auto  found   = finder_post_found(post, search);
			const auto  labeled = labelFilter && post.Label == Filters.Label;
			auto        ring    = std::optional<std::pair<QColor, std::pair<qreal, qreal>>>{ };
			if ((keyboard == at) || (aHovered && HoveredBead == at)) {
				ring = std::pair{ colours.Ink2, std::pair{ 2.0, 1.5 } };
			}
			if (path.contains(beads[at].Post)) {
				ring = std::pair{ colours.RouteLine, std::pair{ 1.5, 1.5 } };
			}
			if (!picked.empty() && post.Id == picked) {
				ring = std::pair{ colours.Route, std::pair{ 2.0, 2.0 } };
			}
			if (found) {
				ring = std::pair{ colours.Ink, std::pair{ 2.0, 1.5 } };
			}
			if (labeled) {
				ring = std::pair{ MarkTheme.Tints[static_cast<std::size_t>(*post.Label)], std::pair{ 2.0, 1.5 } };
			}
			aPainter.setOpacity(faded * (labelFilter && !labeled && !found ? 0.35 : 1.0));
			if (ring.has_value()) {
				const auto [gap, thickness] = ring->second;
				aPainter.setPen(Qt::NoPen);
				aPainter.setBrush(ring->first);
				aPainter.drawEllipse(rect.adjusted(-gap - thickness, -gap - thickness, gap + thickness, gap + thickness));
				aPainter.setBrush(ground);
				aPainter.drawEllipse(rect.adjusted(-gap, -gap, gap, gap));
			} else if (post.Mine) {
				aPainter.setPen(QPen{ colours.Faint, 1.0 });
				aPainter.setBrush(Qt::NoBrush);
				aPainter.drawEllipse(rect.adjusted(-2.0, -2.0, 2.0, 2.0));
			}
			paint_avatar(aPainter, rect, post.Author, false, colours, font());
			if (post.Unread) {
				const auto dot = QRectF{ rect.right() - 3.0, rect.top() - 2.0, 6.0, 6.0 };
				aPainter.setPen(QPen{ ground, 1.5 });
				aPainter.setBrush(colours.New);
				aPainter.drawEllipse(dot);
			}
		}
		aPainter.setOpacity(1.0);
		// The state: my draft's lock, the new count, or resolved's tick.
		if (entry.Row == 0U) {
			const auto state = geometry.State;
			if (conversation.Draft) {
				paint_conversation_glyph(aPainter, QRectF{ state.right() - 14.0, mid - 7.0, 14.0, 14.0 }, conversation_glyph::lock, colours.Faint);
			} else if (conversation.Unread > 0U) {
				const auto text  = QString::number(conversation.Unread);
				const auto bold  = weighted(sized(font(), 0.8), QFont::Bold);
				const auto width = std::max(16, QFontMetrics{ bold }.horizontalAdvance(text) + 10);
				const auto pill  = QRectF{ static_cast<qreal>(state.right() - width + 1), mid - 8.0, static_cast<qreal>(width), 16.0 };
				aPainter.setPen(Qt::NoPen);
				aPainter.setBrush(colours.New);
				aPainter.drawRoundedRect(pill, 8.0, 8.0);
				aPainter.setFont(bold);
				aPainter.setPen(colours.OnNew);
				aPainter.drawText(pill, Qt::AlignCenter, text);
			} else if (conversation.Resolved) {
				paint_conversation_glyph(aPainter, QRectF{ state.right() - 14.0, mid - 7.0, 14.0, 14.0 }, conversation_glyph::resolved, colours.Ok);
			}
		}
		// [[BPR-1356]] [[BPR-1582]]. The Review and Discuss buttons: quiet
		// pills on the row under the pointer and on the cursor's row, inked
		// when the pointer is on one.
		const auto paintPill = [&](const QRect& aPill, const bool aOn, const QString& aWord) {
			if (aPill.isEmpty()) {
				return;
			}
			const auto pill  = QRectF{ aPill }.adjusted(0.5, 0.5, -0.5, -0.5);
			const auto small = weighted(sized(font(), 0.82), QFont::DemiBold);
			aPainter.setOpacity(1.0);
			aPainter.setPen(QPen{ aOn ? colours.Signal : colours.Rule2, 1.0 });
			aPainter.setBrush(aOn ? blend(ground, colours.Signal, colours.Dark ? 0.24 : 0.12) : blend(ground, colours.Panel, 0.65));
			aPainter.drawRoundedRect(pill, pill.height() / 2.0, pill.height() / 2.0);
			aPainter.setFont(small);
			aPainter.setPen(aOn ? colours.Ink : colours.Ink2);
			aPainter.drawText(aPill, Qt::AlignCenter, aWord);
		};
		paintPill(geometry.Review, HoveredItem == aItem && HoveredReview, QStringLiteral("Review"));
		paintPill(geometry.Discuss, HoveredItem == aItem && HoveredDiscuss, QStringLiteral("Discuss"));
		aPainter.restore();
	}
} // namespace bpr::qt
