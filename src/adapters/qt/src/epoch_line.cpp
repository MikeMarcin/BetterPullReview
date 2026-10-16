#include "bpr/qt/epoch_line.hpp"

#include "bpr/qt/anchored_tooltip.hpp"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDateTime>
#include <QEvent>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QRectF>
#include <QTimeZone>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace bpr::qt {
	namespace {
		constexpr int side_padding  = 12;
		constexpr int node_y        = 8;
		constexpr int node_radius   = 4;
		constexpr int handle_width  = 7;
		constexpr int handle_height = 13;
		constexpr int handle_top    = 1;
		constexpr int base_offset   = 7;
		constexpr int label_top     = 18;
		constexpr int hit_radius    = 7;
		/// The least room between two labels in the dates' row.
		constexpr int label_spacing = 4;
		constexpr int label_height  = 12;
		/// [[BPR-1557]]. The pull request's base: a post, not a round node,
		/// because it is where the line starts rather than an update.
		constexpr qreal base_post_width  = 3.0;
		constexpr qreal base_post_height = 9.0;
		/// [[BPR-1130]]. Room the mark takes before the track, and its gap.
		constexpr int mark_gap = 6;

		/// \brief The one colour the palette has no role for.
		///
		/// \details The design marks "my last review" in amber on every
		/// theme; it is a marker, not a state the appearance roles name,
		/// and it reads on both the light and the dark palette.
		[[nodiscard]] auto marker_colour() -> QColor
		{
			return QColor{ 0xD2, 0x99, 0x22 };
		}

		[[nodiscard]] auto short_oid(const core::git_commit_oid& aOid) -> QString
		{
			const auto canonical = core::canonical_hex(aOid);
			const auto view      = canonical.view();
			return QString::fromLatin1(
					   view.data(), static_cast<qsizetype>(view.size()))
				.left(12);
		}

		[[nodiscard]] auto cause_text(const core::epoch_primary_cause aCause)
			-> QString
		{
			switch (aCause) {
			case core::epoch_primary_cause::initial_observation:
				return QStringLiteral("first observed");
			case core::epoch_primary_cause::explicit_historical_import:
				return QStringLiteral("imported history");
			case core::epoch_primary_cause::base_branch_change:
				return QStringLiteral("base branch changed");
			case core::epoch_primary_cause::force_push_rebase:
				return QStringLiteral("force push");
			case core::epoch_primary_cause::author_upload:
				return QStringLiteral("upload");
			case core::epoch_primary_cause::head_change_lineage_unavailable:
				return QStringLiteral("head changed");
			case core::epoch_primary_cause::base_update:
				return QStringLiteral("base updated");
			case core::epoch_primary_cause::retention_rehydration:
				return QStringLiteral("restored from retention");
			}
			return QString{ };
		}

		[[nodiscard]] auto observed_at(const application::epoch_row_projection& aRow)
			-> QDateTime
		{
			return QDateTime::fromMSecsSinceEpoch(
					   aRow.ObservedAt.value(), QTimeZone::UTC)
				.toLocalTime();
		}
	} // namespace

	epoch_line::epoch_line(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("epochLine"));
		setAccessibleName(QStringLiteral("Updates line"));
		setAccessibleDescription(QStringLiteral(
			"One node per update to the pull request; the two handles are the "
			"compared updates. Click a node to compare from it, Cmd-click to "
			"compare to it, right-click for more. Left and Right move the left "
			"handle one update at a time; Shift+Left and Shift+Right move the "
			"right handle."));
		setFocusPolicy(Qt::StrongFocus);
		setMouseTracking(true);
		setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		setFixedHeight(height_pixels);
		setAttribute(Qt::WA_Hover, true);
		// [[BPR-1130]]. A node names its update, right below the node and
		// its label; anywhere else on the line the tooltip is the history
		// statement the caption used to carry, below the line
		// ([[BPR-1354]]: no longer at the pointer).
		anchor_tooltips(*this, [this](const QPoint aPosition) -> std::optional<anchored_tooltip_target> {
			if (base_at(aPosition)) {
				return anchored_tooltip_target{ .Anchor = QRect{ base_x() - hit_radius, 0, (2 * hit_radius) + 1, label_top + label_height }, .Text = base_tooltip() };
			}
			if (const auto hit = update_at(aPosition); hit) {
				return anchored_tooltip_target{ .Anchor = QRect{ node_x(*hit) - hit_radius, 0, (2 * hit_radius) + 1, label_top + label_height }, .Text = node_tooltip(*hit) };
			}
			return anchored_tooltip_target{ .Anchor = rect(), .Text = statement() };
		});
	}

	void epoch_line::paint_history_mark(QPainter& aPainter, const QRect& aBounds, const QPalette& aPalette)
	{
		const auto side = std::min({ aBounds.width(), aBounds.height(), history_mark_size });
		if (side < 5) {
			return;
		}
		const auto ink    = aPalette.color(QPalette::Active, QPalette::WindowText);
		const auto circle = QRectF{ static_cast<qreal>(aBounds.left()) + (static_cast<qreal>(aBounds.width() - side) / 2.0) + 0.5,
									static_cast<qreal>(aBounds.top()) + (static_cast<qreal>(aBounds.height() - side) / 2.0) + 0.5,
									static_cast<qreal>(side - 1),
									static_cast<qreal>(side - 1) };
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		aPainter.setPen(QPen{ ink, 1.2 });
		aPainter.setBrush(Qt::NoBrush);
		aPainter.drawEllipse(circle);
		// The exclamation point: a stroke and a dot, centred in the ring.
		const auto centre = circle.center();
		const auto unit   = circle.height() / 11.0;
		aPainter.setPen(QPen{ ink, std::max(1.2, 1.4 * unit), Qt::SolidLine, Qt::RoundCap });
		aPainter.drawLine(QPointF{ centre.x(), centre.y() - (3.0 * unit) }, QPointF{ centre.x(), centre.y() + (0.8 * unit) });
		aPainter.drawPoint(QPointF{ centre.x(), centre.y() + (2.9 * unit) });
		aPainter.restore();
	}

	auto epoch_line::history_marked() const noexcept -> bool
	{
		return Marked && !Updates.empty();
	}

	auto epoch_line::statement() const -> QString
	{
		return Updates.empty() ? EmptyStatus : HistoryText;
	}

	void epoch_line::set_empty_status(QString aStatus)
	{
		EmptyStatus = std::move(aStatus);
		setAccessibleDescription(statement());
		setToolTip(statement());
		update();
	}

	auto epoch_line::empty_status() const -> QString
	{
		return EmptyStatus;
	}

	void epoch_line::set_updates(
		std::vector<application::epoch_row_projection> aOldestFirst)
	{
		Updates.clear();
		for (const auto& row : aOldestFirst) {
			Updates.push_back({
				.Id      = row.Id,
				.Head    = row.Head,
				.Label   = QStringLiteral("First seen %1").arg(QLocale{ }.toString(observed_at(row).date(), QStringLiteral("MMM d"))),
				.Date    = QLocale{ }.toString(observed_at(row).date(), QStringLiteral("MMM d")),
				.Details = QStringLiteral("%1. Local first-seen %2. Head %3. Observed base %4.")
							   .arg(cause_text(row.Cause.Primary), QLocale{ }.toString(observed_at(row), QLocale::ShortFormat), short_oid(row.Head), short_oid(row.Base)),
				.HasBase = true,
			});
		}
		HistoryText = QStringLiteral("Incomplete history — local records only");
		Marked      = true;
		adopt_line_updates();
		setAccessibleDescription(statement());
		setToolTip(statement());
		Dragging.reset();
		DragStop.reset();
		refresh_name();
		update();
	}

	void epoch_line::adopt_line_updates()
	{
		LineUpdates.clear();
		LineUpdates.reserve(Updates.size());
		for (const auto& row : Updates) {
			LineUpdates.push_back({ .Epoch = row.Id, .Importable = row.ImportOccurrence.has_value(), .HasBase = row.HasBase });
		}
	}

	auto epoch_line::facts() const -> core::update_line_facts
	{
		return core::update_line_facts{
			.PullRequestBase = PullRequestBase ? std::optional{ PullRequestBase->Identity } : std::nullopt,
			.Updates         = LineUpdates,
		};
	}

	void epoch_line::set_pull_request_base(std::optional<core::comparison_anchor>         aBase,
										   std::optional<core::observed_utc_milliseconds> aObservedAt)
	{
		if (PullRequestBase == aBase && BaseObservedAt == aObservedAt) {
			return;
		}
		PullRequestBase = std::move(aBase);
		BaseObservedAt  = aObservedAt;
		Dragging.reset();
		DragStop.reset();
		refresh_name();
		update();
	}

	auto epoch_line::has_pull_request_base() const noexcept -> bool
	{
		return PullRequestBase.has_value();
	}

	auto epoch_line::base_label() const -> QString
	{
		return PullRequestBase ? QStringLiteral("Base") : QString{ };
	}

	auto epoch_line::base_tooltip() const -> QString
	{
		if (!PullRequestBase) {
			return QString{ };
		}
		auto text = QStringLiteral("Base of the pull request: commit %1 on the base branch, as GitHub recorded it")
						.arg(short_oid(PullRequestBase->Commit));
		if (BaseObservedAt) {
			text.append(QStringLiteral(" (seen here %1)")
							.arg(QLocale{ }.toString(QDateTime::fromMSecsSinceEpoch(BaseObservedAt->value(), QTimeZone::UTC).toLocalTime(), QLocale::ShortFormat)));
		}
		text.append(QStringLiteral(". Whole PR compares from here."));
		if (PullRequestBase->Availability != core::anchor_availability::available) {
			text.append(QStringLiteral(" Its commit is not available here."));
		}
		return text;
	}

	auto epoch_line::handle_text(const std::optional<epoch_line_stop>& aStop) const -> QString
	{
		if (!aStop) {
			return QStringLiteral("nothing");
		}
		switch (aStop->Kind) {
		case core::update_line_place_kind::pull_request_base:
			return PullRequestBase
				? QStringLiteral("the pull request base %1").arg(short_oid(PullRequestBase->Commit).left(7))
				: QStringLiteral("the pull request base");
		case core::update_line_place_kind::update_base:
			return QStringLiteral("the base of update %1 of %2").arg(aStop->Update + 1U).arg(Updates.size());
		case core::update_line_place_kind::update_head:
			return aStop->Update < Updates.size()
				? QStringLiteral("update %1 of %2, %3").arg(aStop->Update + 1U).arg(Updates.size()).arg(Updates[aStop->Update].Label)
				: QStringLiteral("an update");
		}
		return QString{ };
	}

	void epoch_line::refresh_name()
	{
		// [[BPR-1557]]. A screen reader hears where the two handles are, the
		// base by name and short commit, since the line draws them only.
		const auto left  = left_stop();
		const auto right = right_stop();
		const auto name  = Updates.empty() || (!left && !right)
			? QStringLiteral("Updates line")
			: QStringLiteral("Updates line: from %1 to %2").arg(handle_text(left), handle_text(right));
		if (accessibleName() != name) {
			setAccessibleName(name);
		}
	}

	void epoch_line::set_occurrences(const core::epoch_occurrence_history&                   aHistory,
									 const std::optional<application::epoch_row_projection>& aCurrentEpoch,
									 const std::vector<application::epoch_row_projection>& aLocalRows, const bool aAllowOccurrenceImport)
	{
		Updates.clear();
		for (const auto& occurrence : aHistory.Occurrences) {
			const auto date      = QDateTime::fromMSecsSinceEpoch(occurrence.DateUtcMilliseconds, QTimeZone::UTC).toLocalTime();
			const auto firstSeen = QDateTime::fromMSecsSinceEpoch(occurrence.FirstSeen.value(), QTimeZone::UTC).toLocalTime();
			// [[BPR-1340]]. With neither an event time nor a readable commit
			// date, the update still has the time this application first saw
			// it, and says so, rather than "Date unavailable".
			auto bareDate    = QLocale{ }.toString(firstSeen.date(), QStringLiteral("MMM d"));
			auto label       = QStringLiteral("First seen %1").arg(bareDate);
			auto dateDetails = QStringLiteral("Push and commit dates unavailable");
			if (occurrence.DateSource != core::epoch_occurrence_date_source::unavailable) {
				const auto source = occurrence.DateSource == core::epoch_occurrence_date_source::github_event
					? QStringLiteral("Event time")
					: QStringLiteral("Commit date");
				bareDate          = QLocale{ }.toString(date.date(), QStringLiteral("MMM d"));
				label             = QStringLiteral("%1 %2").arg(source, bareDate);
				dateDetails       = QStringLiteral("%1 %2").arg(source, QLocale{ }.toString(date, QLocale::ShortFormat));
			}
			auto details = QStringLiteral("%1. Head %2. Local first-seen %3. %4. %5.")
							   .arg(dateDetails, short_oid(occurrence.Evidence.Head),
									QLocale{ }.toString(QDateTime::fromMSecsSinceEpoch(occurrence.FirstSeen.value(), QTimeZone::UTC).toLocalTime(), QLocale::ShortFormat),
									occurrence.HistoricalBase ? QStringLiteral("Historical base %1").arg(short_oid(*occurrence.HistoricalBase)) : QStringLiteral("Historical base unknown"),
									occurrence.DerivedComparisonBase ? QStringLiteral("Derived comparison base %1").arg(short_oid(*occurrence.DerivedComparisonBase)) : QStringLiteral("Derived comparison base unavailable"));
			if (!occurrence.LocalEpoch) {
				details.append(QStringLiteral(" Comparison unavailable."));
			}
			const auto matches = [&occurrence](const application::epoch_row_projection& aRow) {
				return occurrence.LocalEpoch == aRow.Id && occurrence.Evidence.Head == aRow.Head && aRow.Base.valid();
			};
			const auto  local           = std::ranges::find_if(aLocalRows, matches);
			const auto* observed        = local != aLocalRows.end() ? &*local
				: aCurrentEpoch && matches(*aCurrentEpoch)          ? &*aCurrentEpoch
																	: nullptr;
			const auto  hasObservedBase = observed != nullptr;
			if (hasObservedBase && observed->Cause.Origin != core::epoch_origin::explicit_historical_import) {
				details.append(QStringLiteral(" Locally observed base %1.").arg(short_oid(observed->Base)));
			}
			Updates.push_back({ .Id = occurrence.LocalEpoch, .Head = occurrence.Evidence.Head, .Label = label, .Date = bareDate, .Details = details, .HasBase = hasObservedBase || occurrence.HistoricalBase.has_value() || occurrence.DerivedComparisonBase.has_value(), .ImportOccurrence = aAllowOccurrenceImport && !occurrence.LocalEpoch && occurrence.Evidence.Kind == core::epoch_occurrence_kind::force_push_event ? std::optional{ occurrence.Id } : std::nullopt });
		}
		const auto provisional = aCurrentEpoch && std::ranges::none_of(Updates, [&aCurrentEpoch](const line_row& aRow) {
									 return aRow.Id == aCurrentEpoch->Id;
								 });
		if (provisional) {
			Updates.push_back({ .Id = aCurrentEpoch->Id, .Head = aCurrentEpoch->Head, .Label = QStringLiteral("Provisional · first seen here"), .Date = QLocale{ }.toString(observed_at(*aCurrentEpoch).date(), QStringLiteral("MMM d")), .Details = QStringLiteral("Current snapshot; occurrence unresolved. Local first-seen %1. Head %2. Observed base %3.").arg(QLocale{ }.toString(observed_at(*aCurrentEpoch), QLocale::ShortFormat), short_oid(aCurrentEpoch->Head), short_oid(aCurrentEpoch->Base)), .HasBase = true });
		}
		HistoryText = aHistory.Incomplete || provisional ? QStringLiteral("Incomplete history") : QStringLiteral("Known updates");
		if (aHistory.Ambiguous) {
			HistoryText.append(QStringLiteral(" · ambiguous occurrences"));
		}
		if (aHistory.UnresolvedLegacy) {
			HistoryText.append(QStringLiteral(" · unresolved local records"));
		}
		HistoryText.append(QStringLiteral(" · display order is not push order"));
		// [[BPR-1130]]. RFC-BPR-0029: the line says when history is incomplete
		// or an occurrence cannot be uniquely matched. A provisional current
		// snapshot is incomplete history; an unresolved local record is one
		// that could not be matched.
		Marked = aHistory.Incomplete || provisional || aHistory.Ambiguous || aHistory.UnresolvedLegacy;
		adopt_line_updates();
		setAccessibleDescription(statement());
		setToolTip(statement());
		Dragging.reset();
		DragStop.reset();
		refresh_name();
		update();
	}

	auto epoch_line::history_text() const -> QString
	{
		return HistoryText;
	}

	void epoch_line::set_selection(
		std::optional<core::comparison_anchor_identity> aLeft,
		std::optional<core::comparison_anchor_identity> aRight)
	{
		Left  = std::move(aLeft);
		Right = std::move(aRight);
		refresh_name();
		update();
	}

	void epoch_line::set_last_review(std::optional<core::git_commit_oid> aCommit)
	{
		LastReview = std::move(aCommit);
		update();
	}

	void epoch_line::set_selection_offered(const bool aOffered)
	{
		Offered = aOffered;
		update();
	}

	void epoch_line::set_occurrence_dispatch(std::function<void(core::comparison_side, core::epoch_occurrence_id)> aDispatch)
	{
		OccurrenceDispatch = std::move(aDispatch);
	}

	void epoch_line::set_dispatch(epoch_line_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	auto epoch_line::update_count() const noexcept -> std::size_t
	{
		return Updates.size();
	}

	auto epoch_line::left_stop() const noexcept -> std::optional<epoch_line_stop>
	{
		return Left ? stop_for(*Left) : std::nullopt;
	}

	auto epoch_line::right_stop() const noexcept -> std::optional<epoch_line_stop>
	{
		return Right ? stop_for(*Right) : std::nullopt;
	}

	auto epoch_line::last_review_update() const noexcept -> std::optional<std::size_t>
	{
		if (!LastReview) {
			return std::nullopt;
		}
		for (std::size_t index = 0U; index < Updates.size(); ++index) {
			if (Updates[index].Head == *LastReview) {
				return index;
			}
		}
		return std::nullopt;
	}

	auto epoch_line::selection_offered() const noexcept -> bool
	{
		return Offered;
	}

	auto epoch_line::left_inset() const noexcept -> int
	{
		return history_marked() ? side_padding + history_mark_size + mark_gap : side_padding;
	}

	auto epoch_line::node_x(const std::size_t aUpdate) const -> int
	{
		const auto span = std::max(1, width() - left_inset() - side_padding);
		// [[BPR-1557]]. The base takes the track's start, and the updates
		// share the rest evenly: one update sits at the far end, apart from it.
		if (PullRequestBase && !Updates.empty()) {
			return left_inset()
				+ static_cast<int>(std::lround(
					static_cast<double>(span) * static_cast<double>(aUpdate + 1U)
					/ static_cast<double>(Updates.size())));
		}
		if (Updates.size() <= 1U) {
			return left_inset() + span / 2;
		}
		return left_inset()
			+ static_cast<int>(std::lround(
				static_cast<double>(span) * static_cast<double>(aUpdate)
				/ static_cast<double>(Updates.size() - 1U)));
	}

	auto epoch_line::base_x() const -> int
	{
		return left_inset();
	}

	auto epoch_line::slot_of(const std::size_t aUpdate) const noexcept -> std::size_t
	{
		return PullRequestBase ? aUpdate + 1U : aUpdate;
	}

	auto epoch_line::node_centre(const std::size_t aUpdate) const -> QPoint
	{
		return QPoint{ node_x(aUpdate), node_y };
	}

	auto epoch_line::stop_position(const epoch_line_stop& aStop) const -> QPoint
	{
		switch (aStop.Kind) {
		case core::update_line_place_kind::pull_request_base:
			return QPoint{ base_x(), node_y };
		case core::update_line_place_kind::update_base:
			return QPoint{ node_x(aStop.Update) - base_offset, node_y };
		case core::update_line_place_kind::update_head:
			break;
		}
		return QPoint{ node_x(aStop.Update), node_y };
	}

	auto epoch_line::label_text(const std::size_t aUpdate) const -> QString
	{
		if (aUpdate >= Updates.size()) {
			return QString{ };
		}
		return Updates[aUpdate].Label;
	}

	auto epoch_line::selection_dates() const -> std::optional<std::pair<QString, QString>>
	{
		return Left && Right ? dates_of(*Left, *Right) : std::nullopt;
	}

	auto epoch_line::dates_of(
		const core::comparison_anchor_identity& aLeft,
		const core::comparison_anchor_identity& aRight) const -> std::optional<std::pair<QString, QString>>
	{
		// An update's date; the base's is its epoch's update's date, as the
		// whole PR's left side always was, or the day it was seen here.
		const auto date = [this](const core::comparison_anchor_identity& aAnchor) -> QString {
			const auto place = stop_for(aAnchor);
			if (!place) {
				return QString{ };
			}
			if (place->Kind != core::update_line_place_kind::pull_request_base) {
				return place->Update < Updates.size() ? Updates[place->Update].Date : QString{ };
			}
			for (const auto& row : Updates) {
				if (row.Id && row.Id->Authority == aAnchor.Authority && row.Id->Incarnation == aAnchor.Incarnation && row.Id->Ordinal == aAnchor.Epoch) {
					return row.Date;
				}
			}
			return BaseObservedAt ? QLocale{ }.toString(QDateTime::fromMSecsSinceEpoch(BaseObservedAt->value(), QTimeZone::UTC).toLocalTime().date(), QStringLiteral("MMM d")) : QString{ };
		};
		const auto left  = date(aLeft);
		const auto right = date(aRight);
		if (left.isEmpty() || right.isEmpty()) {
			return std::nullopt;
		}
		return std::pair{ left, right };
	}

	auto epoch_line::label_rect(const QString& aText, const int aCentre) const -> QRect
	{
		const auto advance = fontMetrics().horizontalAdvance(aText);
		return QRect{ std::clamp(aCentre - advance / 2, 0, std::max(0, width() - advance - 2)), label_top, advance + 2, label_height + 2 };
	}

	auto epoch_line::shown_labels() const -> std::vector<bool>
	{
		// Dense histories keep the labels that carry meaning -- the base, the
		// newest update, the handles', my last review -- and show the rest,
		// oldest first, only where a label has room ([[BPR-1557]]: by the
		// labels' own extents, so the base's slot never crowds them).
		auto       shown   = std::vector<bool>(Updates.size(), false);
		auto       taken   = std::vector<QRect>{ };
		const auto room    = [](const QRect& aLabel) { return aLabel.adjusted(-label_spacing, 0, label_spacing, 0); };
		const auto left    = left_stop();
		const auto right   = right_stop();
		const auto reviews = last_review_update();
		const auto holding = [](const std::optional<epoch_line_stop>& aStop, const std::size_t aUpdate) {
			return aStop && aStop->Kind != core::update_line_place_kind::pull_request_base && aStop->Update == aUpdate;
		};
		if (PullRequestBase && !Updates.empty()) {
			taken.push_back(room(label_rect(base_label(), base_x())));
		}
		for (std::size_t index = 0U; index < Updates.size(); ++index) {
			if (index + 1U == Updates.size() || (index == 0U && !PullRequestBase) || holding(left, index) || holding(right, index) || reviews == index) {
				shown[index] = true;
				taken.push_back(room(label_rect(Updates[index].Label, node_x(index))));
			}
		}
		for (std::size_t index = 0U; index < Updates.size(); ++index) {
			if (shown[index]) {
				continue;
			}
			const auto label = room(label_rect(Updates[index].Label, node_x(index)));
			if (std::ranges::none_of(taken, [&label](const QRect& aTaken) { return aTaken.intersects(label); })) {
				shown[index] = true;
				taken.push_back(label);
			}
		}
		return shown;
	}

	auto epoch_line::label_shown(const std::size_t aUpdate) const -> bool
	{
		return aUpdate < Updates.size() && shown_labels()[aUpdate];
	}

	auto epoch_line::node_tooltip(const std::size_t aUpdate) const -> QString
	{
		if (aUpdate >= Updates.size()) {
			return QString{ };
		}
		auto text = QStringLiteral("Update %1 of %2. %3 %4")
						.arg(aUpdate + 1U)
						.arg(Updates.size())
						.arg(Updates[aUpdate].Details, HistoryText);
		if (last_review_update() == aUpdate) {
			text.append(QStringLiteral(" My last review."));
		}
		return text;
	}

	auto epoch_line::identity_of(const epoch_line_stop& aStop) const
		-> std::optional<core::comparison_anchor_identity>
	{
		return core::update_line_anchor(facts(), aStop);
	}

	auto epoch_line::active_side() const noexcept -> core::comparison_side
	{
		return ActiveSide;
	}

	auto epoch_line::sizeHint() const -> QSize
	{
		return QSize{ 240, height_pixels };
	}

	auto epoch_line::minimumSizeHint() const -> QSize
	{
		return QSize{ 80, height_pixels };
	}

	auto epoch_line::stop_for(const core::comparison_anchor_identity& aIdentity) const
		-> std::optional<epoch_line_stop>
	{
		return core::place_on_update_line(facts(), aIdentity);
	}

	auto epoch_line::nearest_stop(const core::comparison_side aSide, const int aX) const
		-> std::optional<epoch_line_stop>
	{
		auto best     = std::optional<epoch_line_stop>{ };
		auto distance = 0;
		for (const auto& stop : core::update_line_stops(facts(), aSide)) {
			const auto d = std::abs(stop_position(stop).x() - aX);
			if (!best || d < distance) {
				best     = stop;
				distance = d;
			}
		}
		return best;
	}

	auto epoch_line::update_at(const QPoint& aPoint) const -> std::optional<std::size_t>
	{
		if (aPoint.y() > label_top + label_height) {
			return std::nullopt;
		}
		for (std::size_t index = 0U; index < Updates.size(); ++index) {
			if (std::abs(node_x(index) - aPoint.x()) <= hit_radius) {
				return index;
			}
		}
		return std::nullopt;
	}

	auto epoch_line::base_at(const QPoint& aPoint) const -> bool
	{
		if (!PullRequestBase || Updates.empty() || aPoint.y() > label_top + label_height) {
			return false;
		}
		const auto distance = std::abs(base_x() - aPoint.x());
		if (distance > hit_radius) {
			return false;
		}
		// Where the base and the oldest update are close, the nearer one is hit.
		const auto update = update_at(aPoint);
		return !update || distance <= std::abs(node_x(*update) - aPoint.x());
	}

	auto epoch_line::handle_at(const QPoint& aPoint) const
		-> std::optional<core::comparison_side>
	{
		if (aPoint.y() > handle_top + handle_height + 2) {
			return std::nullopt;
		}
		const auto reach = [this, &aPoint](const std::optional<epoch_line_stop>& aStop) -> std::optional<int> {
			if (!aStop) {
				return std::nullopt;
			}
			const auto distance = std::abs(stop_position(*aStop).x() - aPoint.x());
			return distance <= hit_radius ? std::optional{ distance } : std::nullopt;
		};
		const auto left  = reach(left_stop());
		const auto right = reach(right_stop());
		// [[BPR-1340]]. The handle nearer the press is the one taken. The whole
		// PR puts the left handle on the newest update's base, one base offset
		// left of the right handle on its head, so both are in reach of a press
		// on either; taking the right one whenever it was in reach left the
		// left handle a sliver to grab, and a drag that started on it moved
		// the right one. Only a press exactly between them, or on one stop
		// both share, takes the right handle, so a pair on one node can still
		// be pulled apart.
		if (right && (!left || *right <= *left)) {
			return core::comparison_side::right;
		}
		if (left) {
			return core::comparison_side::left;
		}
		return std::nullopt;
	}

	void epoch_line::choose(const core::comparison_side aSide, const epoch_line_stop& aStop)
	{
		if (!Offered || (aStop.Kind != core::update_line_place_kind::pull_request_base && aStop.Update >= Updates.size())) {
			return;
		}
		if (const auto identity = identity_of(aStop); identity) {
			ActiveSide = aSide;
			if (Dispatch) {
				Dispatch(aSide, *identity);
			}
			return;
		}
		if (aStop.Kind == core::update_line_place_kind::update_head && Updates[aStop.Update].ImportOccurrence) {
			ActiveSide = aSide;
			if (OccurrenceDispatch) {
				OccurrenceDispatch(aSide, *Updates[aStop.Update].ImportOccurrence);
			}
		}
	}

	void epoch_line::step(const core::comparison_side aSide, const int aDirection)
	{
		const auto current = aSide == core::comparison_side::left ? left_stop() : right_stop();
		if (const auto next = core::step_on_update_line(facts(), aSide, current, aDirection); next) {
			choose(aSide, *next);
		}
	}

	void epoch_line::paintEvent(QPaintEvent* const aEvent)
	{
		static_cast<void>(aEvent);
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto& colours = palette();
		const auto  ink     = colours.color(QPalette::Active, QPalette::WindowText);
		const auto  faint   = colours.color(QPalette::Active, QPalette::PlaceholderText);
		const auto  rule    = colours.color(QPalette::Active, QPalette::Mid);
		const auto  accent  = colours.color(QPalette::Active, QPalette::Highlight);
		const auto  surface = colours.color(QPalette::Active, QPalette::Window);

		painter.fillRect(rect(), surface);
		if (Updates.empty()) {
			painter.setPen(faint);
			painter.drawText(
				rect().adjusted(side_padding, 0, -side_padding, 0),
				Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap,
				EmptyStatus);
			return;
		}

		// [[BPR-1130]]. No caption: the statement is the tooltip. When history
		// is incomplete or ambiguous the mark sits before the track, level
		// with the nodes, and the track starts after it.
		if (history_marked()) {
			paint_history_mark(painter,
							   QRect{ side_padding, node_y - (history_mark_size / 2), history_mark_size, history_mark_size },
							   palette());
		}

		// Track, then the span between the handles.
		painter.setPen(QPen{ rule, 1.0 });
		painter.drawLine(left_inset(), node_y, width() - side_padding, node_y);
		const auto left  = DragStop && Dragging == core::comparison_side::left ? DragStop : left_stop();
		const auto right = DragStop && Dragging == core::comparison_side::right ? DragStop : right_stop();
		if (left && right) {
			const auto from = stop_position(*left).x();
			const auto to   = stop_position(*right).x();
			painter.fillRect(
				QRect{ std::min(from, to), node_y - 1, std::abs(to - from), 3 }, accent);
		}

		// [[BPR-1557]]. The pull request's base: a post where the track
		// starts, filled in the faint ink, in the accent while the left
		// handle holds it, and named "Base" in the dates' row.
		const auto onBase   = left && left->Kind == core::update_line_place_kind::pull_request_base;
		const auto drawPost = [&](const QColor& aColour) {
			painter.setPen(Qt::NoPen);
			painter.setBrush(aColour);
			painter.drawRoundedRect(QRectF{ static_cast<qreal>(base_x()) - (base_post_width / 2.0),
											static_cast<qreal>(node_y) - (base_post_height / 2.0),
											base_post_width,
											base_post_height },
									1.0,
									1.0);
		};
		if (PullRequestBase) {
			drawPost(onBase ? accent : faint);
			painter.setPen(onBase ? ink : faint);
			painter.drawText(label_rect(base_label(), base_x()), Qt::AlignLeft | Qt::AlignTop, base_label());
		}

		// Nodes and their labels.
		const auto labelled = shown_labels();
		for (std::size_t index = 0U; index < Updates.size(); ++index) {
			const auto centre = node_centre(index);
			const auto holds  = [index](const std::optional<epoch_line_stop>& aStop) {
				return aStop && aStop->Kind == core::update_line_place_kind::update_head && aStop->Update == index;
			};
			const auto selected = holds(left) || holds(right);
			painter.setPen(QPen{ selected ? accent : faint, 1.5 });
			painter.setBrush(selected ? accent : surface);
			painter.drawEllipse(centre, node_radius, node_radius);
			if (labelled[index]) {
				painter.setPen(selected ? ink : faint);
				painter.drawText(label_rect(Updates[index].Label, centre.x()), Qt::AlignLeft | Qt::AlignTop, Updates[index].Label);
			}
		}

		// My last review, an amber tick with only a tooltip.
		if (const auto reviewed = last_review_update(); reviewed) {
			const auto x = node_x(*reviewed);
			painter.fillRect(QRect{ x - 1, 2, 2, 11 }, marker_colour());
		}

		// The handles, drawn last so they sit over the nodes.
		const auto drawHandle = [&](const std::optional<epoch_line_stop>& aStop) {
			if (!aStop) {
				return;
			}
			const auto at = stop_position(*aStop);
			painter.setPen(QPen{ accent, 1.5 });
			painter.setBrush(surface);
			painter.drawRoundedRect(
				QRectF{ static_cast<qreal>(at.x()) - static_cast<qreal>(handle_width) / 2.0,
						static_cast<qreal>(handle_top),
						static_cast<qreal>(handle_width),
						static_cast<qreal>(handle_height) },
				2.0,
				2.0);
		};
		drawHandle(left);
		drawHandle(right);
		// The handle's face would hide the post it holds; the post shows
		// through, in the accent, so the bracket reads as holding the base.
		if (PullRequestBase && onBase) {
			drawPost(accent);
		}

		if (hasFocus()) {
			painter.setPen(QPen{ accent, 1.0 });
			painter.setBrush(Qt::NoBrush);
			painter.drawRoundedRect(
				QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), 4.0, 4.0);
		}
		if (!Offered) {
			// Nothing to choose yet: the line shows what it knows in the
			// disabled ink rather than pretending the handles move.
			painter.fillRect(rect(), QColor{ surface.red(), surface.green(), surface.blue(), 96 });
		}
	}

	void epoch_line::mousePressEvent(QMouseEvent* const aEvent)
	{
		const auto point = aEvent->position().toPoint();
		if (aEvent->button() == Qt::LeftButton) {
			setFocus(Qt::MouseFocusReason);
			if (const auto handle = handle_at(point); handle && Offered) {
				Dragging   = handle;
				DragStop   = nearest_stop(*handle, point.x());
				ActiveSide = *handle;
				update();
				aEvent->accept();
				return;
			}
			if (base_at(point)) {
				// The base is compared from; the right handle never takes it.
				if ((aEvent->modifiers() & Qt::ControlModifier) == 0) {
					choose(core::comparison_side::left, { .Kind = core::update_line_place_kind::pull_request_base });
				}
				aEvent->accept();
				return;
			}
			if (const auto hit = update_at(point); hit) {
				const auto toHere = (aEvent->modifiers() & Qt::ControlModifier) != 0;
				choose(toHere ? core::comparison_side::right : core::comparison_side::left,
					   { .Kind = core::update_line_place_kind::update_head, .Update = *hit });
				aEvent->accept();
				return;
			}
		}
		QWidget::mousePressEvent(aEvent);
	}

	void epoch_line::mouseMoveEvent(QMouseEvent* const aEvent)
	{
		if (Dragging) {
			const auto stop = nearest_stop(*Dragging, aEvent->position().toPoint().x());
			if (stop != DragStop) {
				DragStop = stop;
				update();
			}
			aEvent->accept();
			return;
		}
		QWidget::mouseMoveEvent(aEvent);
	}

	void epoch_line::mouseReleaseEvent(QMouseEvent* const aEvent)
	{
		if (Dragging && aEvent->button() == Qt::LeftButton) {
			const auto side = *Dragging;
			const auto stop = DragStop;
			Dragging.reset();
			DragStop.reset();
			const auto current = side == core::comparison_side::left ? left_stop() : right_stop();
			if (stop && stop != current) {
				choose(side, *stop);
			}
			update();
			aEvent->accept();
			return;
		}
		QWidget::mouseReleaseEvent(aEvent);
	}

	void epoch_line::keyPressEvent(QKeyEvent* const aEvent)
	{
		const auto shifted = (aEvent->modifiers() & Qt::ShiftModifier) != 0;
		const auto side    = shifted ? core::comparison_side::right : core::comparison_side::left;
		switch (aEvent->key()) {
		case Qt::Key_Left:
			step(side, -1);
			aEvent->accept();
			return;
		case Qt::Key_Right:
			step(side, +1);
			aEvent->accept();
			return;
		default:
			break;
		}
		QWidget::keyPressEvent(aEvent);
	}

	void epoch_line::contextMenuEvent(QContextMenuEvent* const aEvent)
	{
		if (base_at(aEvent->pos())) {
			// [[BPR-1557]]. The base's menu: compare from it, or copy its commit.
			auto* const menu = new QMenu{ this };
			menu->setObjectName(QStringLiteral("epochLineBaseMenu"));
			menu->setAttribute(Qt::WA_DeleteOnClose);
			auto* const from = menu->addAction(QStringLiteral("Compare from here"));
			from->setObjectName(QStringLiteral("compareFromBaseAction"));
			from->setEnabled(Offered);
			menu->addSeparator();
			auto* const copy = menu->addAction(QStringLiteral("Copy base commit"));
			copy->setObjectName(QStringLiteral("copyBaseCommitAction"));
			connect(from, &QAction::triggered, this, [this] {
				choose(core::comparison_side::left, { .Kind = core::update_line_place_kind::pull_request_base });
			});
			connect(copy, &QAction::triggered, this, [this] {
				if (auto* const clipboard = QApplication::clipboard(); clipboard != nullptr && PullRequestBase) {
					const auto canonical = core::canonical_hex(PullRequestBase->Commit);
					const auto view      = canonical.view();
					clipboard->setText(QString::fromLatin1(view.data(), static_cast<qsizetype>(view.size())));
				}
			});
			menu->popup(aEvent->globalPos());
			aEvent->accept();
			return;
		}
		const auto hit = update_at(aEvent->pos());
		if (!hit) {
			aEvent->ignore();
			return;
		}
		const auto index = *hit;
		// [[BPR-1034]]. As [[BPR-1028]] did for the Windows strip's menu: a
		// heap child that deletes itself on close, opened with popup(), not a
		// menu on this handler's stack run by exec(). Anything that destroys the
		// window while the menu is open (an accessible Quit or close, a profile
		// shutdown, an update's quit) then deletes the menu with the line's other
		// children, and no frame of this handler is left waiting for it. The
		// actions' connections carry the update index by value and have this
		// line as their context, so they need nothing from this frame.
		auto* const menu = new QMenu{ this };
		menu->setObjectName(QStringLiteral("epochLineMenu"));
		menu->setAttribute(Qt::WA_DeleteOnClose);
		auto* const from = menu->addAction(QStringLiteral("Compare from here"));
		from->setObjectName(QStringLiteral("compareFromUpdateAction"));
		auto* const to = menu->addAction(QStringLiteral("Compare to here"));
		to->setObjectName(QStringLiteral("compareToUpdateAction"));
		auto* const alone = menu->addAction(QStringLiteral("Show this update alone"));
		alone->setObjectName(QStringLiteral("showUpdateAloneAction"));
		menu->addSeparator();
		auto* const copy = menu->addAction(QStringLiteral("Copy head commit"));
		copy->setObjectName(QStringLiteral("copyUpdateHeadAction"));
		from->setEnabled(Offered && (Updates[index].Id || Updates[index].ImportOccurrence));
		to->setEnabled(Offered && (Updates[index].Id || Updates[index].ImportOccurrence));
		alone->setEnabled(Offered && Updates[index].Id.has_value() && Updates[index].HasBase);
		connect(from, &QAction::triggered, this, [this, index] {
			choose(core::comparison_side::left, { .Kind = core::update_line_place_kind::update_head, .Update = index });
		});
		connect(to, &QAction::triggered, this, [this, index] {
			choose(core::comparison_side::right, { .Kind = core::update_line_place_kind::update_head, .Update = index });
		});
		connect(alone, &QAction::triggered, this, [this, index] {
			choose(core::comparison_side::left, { .Kind = core::update_line_place_kind::update_base, .Update = index });
			choose(core::comparison_side::right, { .Kind = core::update_line_place_kind::update_head, .Update = index });
		});
		connect(copy, &QAction::triggered, this, [this, index] {
			if (auto* const clipboard = QApplication::clipboard();
				clipboard != nullptr && index < Updates.size()) {
				const auto canonical = core::canonical_hex(Updates[index].Head);
				const auto view      = canonical.view();
				clipboard->setText(QString::fromLatin1(
					view.data(), static_cast<qsizetype>(view.size())));
			}
		});
		menu->popup(aEvent->globalPos());
		aEvent->accept();
	}

	void epoch_line::focusInEvent(QFocusEvent* const aEvent)
	{
		QWidget::focusInEvent(aEvent);
		update();
	}

	void epoch_line::focusOutEvent(QFocusEvent* const aEvent)
	{
		QWidget::focusOutEvent(aEvent);
		update();
	}
} // namespace bpr::qt
