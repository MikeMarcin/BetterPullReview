#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/pull_request_overview_page.hpp"
#include "bpr/qt/steady_columns.hpp"
#include "overview_summary_splitter.hpp"
#include "painted_glyph_icon.hpp"
#include "pull_request_overview_support.hpp"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractScrollArea>
#include <QAccessible>
#include <QAccessibleAnnouncementEvent>
#include <QAction>
#include <QActionGroup>
#include <QAnyStringView>
#include <QBoxLayout>
#include <QBrush>
#include <QByteArrayView>
#include <QChar>
#include <QColor>
#include <QEvent>
#include <QFlags>
#include <QFocusEvent>
#include <QFont>
#include <QHBoxLayout>
#include <QIcon>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLabel>
#include <QLatin1Char>
#include <QLatin1StringView>
#include <QList>
#include <QListSpecialMethods>
#include <QMenu>
#include <QMetaObject>
#include <QModelIndex>
#include <QMouseEvent>
#include <QObject>
#include <QPaintDevice>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPersistentModelIndex>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPoint>
#include <QPointF>
#include <QPushButton>
#include <QRect>
#include <QRectF>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSinglePointEvent>
#include <QString>
#include <QStringConverter>
#include <QStringDecoder>
#include <QToolButton>
#include <QTreeView>
#include <QVariant>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cstddef>
#include <functional>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace bpr::qt {
	namespace {
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

		/// A key: the Label key's icon.
		void paint_key(QPainter& aPainter, const qreal aSide, const QColor& aInk)
		{
			const auto unit = aSide / 16.0;
			aPainter.setPen(QPen{ aInk, std::max<qreal>(1.25, aSide / 11.0), Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
			aPainter.setBrush(Qt::NoBrush);
			aPainter.drawEllipse(QPointF{ 5.0 * unit, 8.0 * unit }, 3.0 * unit, 3.0 * unit);
			aPainter.drawLine(QPointF{ 8.0 * unit, 8.0 * unit }, QPointF{ 14.0 * unit, 8.0 * unit });
			aPainter.drawLine(QPointF{ 12.0 * unit, 8.0 * unit }, QPointF{ 12.0 * unit, 10.5 * unit });
			aPainter.drawLine(QPointF{ 14.0 * unit, 8.0 * unit }, QPointF{ 14.0 * unit, 10.5 * unit });
		}
	}

	void pull_request_overview_page::show_cached_source(bool aShow)
	{
		CachedSource->setVisible(aShow);
		CachedSourceBack->setVisible(aShow);
		Files->setVisible(!aShow);
		FilesHeading->setVisible(!aShow);
		update_folder_controls();
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
		SummarySplit->set_expanded(aExpanded);
		FilesPane->setVisible(!aExpanded);
		Header->setVisible(!aExpanded);
		StatusDrawer->setVisible(!aExpanded);
		Files->setVisible(!aExpanded);
		FilesHeading->setVisible(!aExpanded);
		update_folder_controls();
		FullView->setText(aExpanded ? QStringLiteral("Exit full view") : QStringLiteral("Enter full view"));
		FullView->setAccessibleName(FullView->text());
		FullView->setToolTip(aExpanded ? QStringLiteral("Exit full view (Return to Files)") : FullView->text());
		FullView->setIcon(full_view_icon(palette(), aExpanded));
		FullView->setVisible(!Body->Reader->original_body().empty());
		EmptyBody->setVisible(!aExpanded && Projection.has_value() && Projection->Body.empty());
		present_description_controls();
		findChild<QShortcut*>(QStringLiteral("overviewDescriptionEscape"))->setEnabled(aExpanded);
		if (aExpanded) {
			Body->setFocus(Qt::OtherFocusReason);
		} else if (isVisible()) {
			Files->setFocus(Qt::OtherFocusReason);
		}
		QWidget::setTabOrder(FullView, Body);
	}

	void pull_request_overview_page::set_summary_fraction(const std::optional<std::uint32_t> aFraction)
	{
		SummarySplit->set_preference(aFraction);
	}

	auto pull_request_overview_page::summary_fraction() const -> std::optional<std::uint32_t>
	{
		return SummarySplit->preference();
	}

	void pull_request_overview_page::set_summary_resize_dispatch(std::function<void(std::optional<std::uint32_t>)> aDispatch)
	{
		SummarySplit->Changed = std::move(aDispatch);
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
		// [[BPR-1562]]. Fenced code in the review panes' colours.
		Body->Reader->set_code_theme({ .Colors = code_role_colors_of(aProfile.palette()), .Variant = aProfile.variant() });
		Colours.Unread   = aProfile.color(core::appearance_role::state_unread);
		Colours.Accepted = aProfile.color(core::appearance_role::state_accepted);
		Colours.Defect   = aProfile.color(core::appearance_role::state_defect);
		Colours.Waiting  = aProfile.attention_color(attention_tone::waiting_on_others);
		Colours.Caution  = aProfile.color(core::appearance_role::state_attention);
		Colours.Progress = progress_mark_colours_for(aProfile);
		// [[BPR-1089]]. The state marks: open amber and resolved green.
		// [[BPR-1604]]. The table's amber draws digits, the R badge with its
		// letter cut from the ground, and the unresolved bubble, on the ground
		// and on the band: the theme's amber where it holds the text floor
		// there, otherwise darkened toward a burnt orange until it does.
		// `Marker` had kept its default, the Dark theme's amber, in every theme.
		const auto table          = aProfile.color(core::appearance_role::code_background);
		const auto grounds        = std::array{ core::contrast_ground{ .Colour = to_appearance_color(table), .MinimumHundredths = core::banded_floor_hundredths(core::text_contrast_minimum_hundredths) } };
		const auto amber          = to_presentation_color(core::readable_signal_ink(to_appearance_color(aProfile.color(core::appearance_role::state_attention)), grounds));
		Colours.Marker            = amber;
		Colours.States.Open       = amber;
		Colours.States.Resolved   = Colours.Accepted;
		Colours.States.OnResolved = feedback_mark_blocking_ink(Colours.Accepted);
		refresh_folder_icons();
		refresh_feedback_icons();
		if (Projection) {
			present_merge(*Projection);
		} else if (ObservedMerge) {
			present_merge_details(ObservedMerge->State, ObservedMerge->Tone, { }, ObservedMerge->Details);
		}
		update();
		Files->viewport()->update();
	}

	void pull_request_overview_page::collapse_all_folders()
	{
		// One folder at a time, so a hidden current file moves to its folder
		// exactly as a single collapse moves it.
		for (auto row = 0; row < Model->rowCount(); ++row) {
			const auto folder = Model->index(row, 0);
			if (folder.data(overview_file_model::folder_role).toBool()) {
				Files->collapse(folder);
			}
		}
	}

	void pull_request_overview_page::expand_all_folders()
	{
		for (auto row = 0; row < Model->rowCount(); ++row) {
			const auto folder = Model->index(row, 0);
			if (folder.data(overview_file_model::folder_role).toBool()) {
				Files->expand(folder);
			}
		}
	}

	auto pull_request_overview_page::collapse_folders_button() const noexcept -> QToolButton*
	{
		return CollapseFolders;
	}

	auto pull_request_overview_page::expand_folders_button() const noexcept -> QToolButton*
	{
		return ExpandFolders;
	}

	void pull_request_overview_page::update_folder_controls()
	{
		const auto shown = FilesHeading != nullptr && !FilesHeading->isHidden() && Model != nullptr && Model->folder_count() > 0U;
		CollapseFolders->setVisible(shown);
		ExpandFolders->setVisible(shown);
	}

	void pull_request_overview_page::refresh_folder_icons()
	{
		// Two chevrons that close upward, or open downward, in the heading's ink.
		const auto chevrons = [](const bool aDown) {
			return [aDown](QPainter& aPainter, const qreal aSide, const QColor& aInk) {
				auto pen = QPen{ aInk, std::max<qreal>(1.25, aSide / 11.0) };
				pen.setCapStyle(Qt::RoundCap);
				pen.setJoinStyle(Qt::RoundJoin);
				aPainter.setPen(pen);
				const auto lean = aSide * (aDown ? 0.11 : -0.11);
				for (const auto middle : { aSide * 0.34, aSide * 0.66 }) {
					aPainter.drawPolyline(std::array{ QPointF{ aSide * 0.25, middle - lean }, QPointF{ aSide * 0.5, middle + lean },
													  QPointF{ aSide * 0.75, middle - lean } }
											  .data(),
										  3);
				}
			};
		};
		const auto ink      = palette().color(QPalette::Text);
		const auto disabled = palette().color(QPalette::Disabled, QPalette::Text);
		CollapseFolders->setIcon(painted_glyph_icon(chevrons(false), ink, disabled));
		ExpandFolders->setIcon(painted_glyph_icon(chevrons(true), ink, disabled));
	}

	auto pull_request_overview_page::colours() const noexcept -> const overview_colours&
	{
		return Colours;
	}

	auto pull_request_overview_page::feedback_request() const -> application::overview_feedback_request
	{
		auto request    = application::overview_feedback_request{ };
		request.Columns = Columns;
		request.Mode    = Mode;
		request.Sort    = Sort;
		return request;
	}

	void pull_request_overview_page::dispatch_feedback_request()
	{
		if (!Projection || Cached || !FeedbackDispatch) {
			return;
		}
		const auto request = feedback_request();
		if (AskedFeedback == std::optional{ request }) {
			return;
		}
		AskedFeedback = request;
		FeedbackDispatch(request);
		apply_pending();
	}

	void pull_request_overview_page::present_feedback(const application::overview_feedback_projection& aFeedback)
	{
		// An answer to an earlier request is replaced by the one on its way.
		if (!Projection || Cached || aFeedback.Request != feedback_request()) {
			return;
		}
		const auto reorder = !Model->feedback() || Model->feedback()->Order != aFeedback.Order;
		Model->apply_feedback(aFeedback);
		if (reorder) {
			apply_rows();
		}
		present_feedback_controls();
		apply_pending();
	}

	auto pull_request_overview_page::ordered_rows() const -> std::vector<application::overview_file_row>
	{
		if (!Projection) {
			return { };
		}
		const auto& feedback = Model->feedback();
		if (!feedback || feedback->Order.empty()) {
			return Projection->Files;
		}
		auto byPath = std::unordered_map<std::string_view, const application::overview_file_row*>{ };
		byPath.reserve(Projection->Files.size());
		for (const auto& row : Projection->Files) {
			byPath.emplace(row.Path, &row);
		}
		auto rows = std::vector<application::overview_file_row>{ };
		rows.reserve(Projection->Files.size());
		for (const auto& path : feedback->Order) {
			if (const auto found = byPath.find(path); found != byPath.end()) {
				rows.push_back(*found->second);
				byPath.erase(found);
			}
		}
		for (const auto& row : Projection->Files) {
			if (byPath.contains(row.Path)) {
				rows.push_back(row);
			}
		}
		return rows;
	}

	void pull_request_overview_page::apply_rows()
	{
		if (!Projection) {
			return;
		}
		// File order needs no copy of the rows.
		if (const auto& feedback = Model->feedback(); !feedback || feedback->Order.empty()) {
			Model->apply(Projection->Files);
			return;
		}
		Model->apply(ordered_rows());
	}

	void pull_request_overview_page::present_feedback_controls()
	{
		const auto& feedback = Model->feedback();
		const auto  shown    = feedback.has_value() && Projection.has_value() && !Cached;
		{
			const QSignalBlocker openBlocker{ OpenCounts };
			const QSignalBlocker allBlocker{ AllCounts };
			OpenCounts->setChecked(Mode == core::feedback_resolution_mode::unresolved);
			AllCounts->setChecked(Mode == core::feedback_resolution_mode::all);
		}
		OpenCounts->setVisible(shown);
		AllCounts->setVisible(shown);
		LabelKey->setVisible(shown);
		const auto* whole     = shown ? &feedback->PullRequest.Counts : nullptr;
		const auto  qualified = whole != nullptr && (whole->Uncertain != 0U || !whole->Exact);
		Uncertain->setVisible(qualified);
		if (qualified) {
			Uncertain->setText(count_text(whole->Uncertain));
			auto chip  = overview_comment_chip{ };
			chip.Kind  = overview_comment_chip::kind::uncertain;
			chip.Count = whole->Uncertain;
			Uncertain->setToolTip(chip_tooltip(chip, *whole, any_columns(feedback)));
			Uncertain->setAccessibleName(QStringLiteral("Not counted: %1").arg(count_text(whole->Uncertain)));
			Uncertain->setAccessibleDescription(qualifier_clauses(*whole).join(QStringLiteral("; ")));
			Uncertain->setEnabled(whole->Uncertain != 0U);
		}
		FileColumns->set_fixed_width(overview_file_model::comments_column, shown ? std::optional{ overview_comments_width } : std::nullopt);
	}

	void pull_request_overview_page::refresh_feedback_icons()
	{
		const auto ratio = devicePixelRatioF();
		auto       state = Colours.States;
		state.Faint      = feedback_state_faint(palette());
		OpenCounts->setIcon(QIcon{ feedback_state_mark_pixmap(core::feedback_resolution::unresolved, state, ratio) });
		static_cast<counts_mode_button*>(OpenCounts)->set_mark(core::feedback_resolution::unresolved, state);
		Uncertain->setIcon(QIcon{ feedback_state_mark_pixmap(core::feedback_resolution::unknown, state, ratio) });
		LabelKey->setIcon(painted_glyph_icon(paint_key, palette().color(QPalette::Text), palette().color(QPalette::Disabled, QPalette::Text)));
	}

	void pull_request_overview_page::set_label_columns(const core::feedback_label_columns& aColumns)
	{
		if (Columns == aColumns) {
			return;
		}
		Columns = aColumns;
		// A sort by a chip that is gone returns to the file order.
		if (Sort && ((Sort->Kind == core::feedback_count_kind::dedicated && std::ranges::find(core::labels_of(Columns), Sort->Label) == core::labels_of(Columns).end()) || (Sort->Kind == core::feedback_count_kind::catchall && Columns.Count == 0U))) {
			Sort.reset();
		}
		dispatch_feedback_request();
	}

	void pull_request_overview_page::set_default_mode(const core::feedback_resolution_mode aMode)
	{
		DefaultMode = aMode;
		if (!ModeChosen && Mode != aMode) {
			Mode = aMode;
			present_feedback_controls();
			dispatch_feedback_request();
		}
	}

	void pull_request_overview_page::choose_mode(const core::feedback_resolution_mode aMode)
	{
		ModeChosen = true;
		if (Mode == aMode) {
			return;
		}
		Mode = aMode;
		present_feedback_controls();
		dispatch_feedback_request();
	}

	void pull_request_overview_page::choose_sort(std::optional<core::feedback_count_target> aSort)
	{
		if (Sort == aSort) {
			return;
		}
		Sort = std::move(aSort);
		dispatch_feedback_request();
	}

	void pull_request_overview_page::set_label_marks(const feedback_mark_theme& aTheme)
	{
		Colours.Marks = aTheme;
		Files->viewport()->update();
	}

	void pull_request_overview_page::set_attention_colour(const QColor& aColour)
	{
		Colours.States.Open = aColour;
		refresh_feedback_icons();
		Files->viewport()->update();
	}

	void pull_request_overview_page::set_feedback_dispatch(overview_feedback_dispatch aDispatch)
	{
		FeedbackDispatch = std::move(aDispatch);
		AskedFeedback.reset();
		dispatch_feedback_request();
	}

	void pull_request_overview_page::set_drill_down_dispatch(overview_drill_down_dispatch aDispatch)
	{
		DrillDownDispatch = std::move(aDispatch);
	}

	void pull_request_overview_page::drill(const application::overview_feedback_row_kind aRow, const std::string& aPath, const core::feedback_count_target& aTarget)
	{
		if (!DrillDownDispatch || !Model->feedback()) {
			return;
		}
		auto down   = application::overview_drill_down{ };
		down.Row    = aRow;
		down.Path   = aPath;
		down.Target = aTarget;
		DrillDownDispatch(down);
	}

	auto pull_request_overview_page::chip_at(const QPoint& aViewportPosition) const -> std::optional<std::pair<QModelIndex, overview_comment_chip>>
	{
		const auto index = Files->indexAt(aViewportPosition);
		if (!index.isValid() || index.column() != overview_file_model::comments_column) {
			return std::nullopt;
		}
		const auto* counts = Model->feedback_at(index);
		if (counts == nullptr) {
			return std::nullopt;
		}
		for (auto& chip : overview_comment_chips(*counts, Files->visualRect(index), Files->font())) {
			if (chip.Box.contains(QPointF{ aViewportPosition })) {
				return std::pair{ index, std::move(chip) };
			}
		}
		return std::nullopt;
	}

	auto pull_request_overview_page::make_comments_menu(const QModelIndex& aIndex) -> QMenu*
	{
		const auto  cell   = aIndex.siblingAtColumn(overview_file_model::comments_column);
		const auto* counts = Model->feedback_at(cell);
		const auto  row    = Model->feedback_row(cell);
		if (counts == nullptr || !row) {
			return nullptr;
		}
		const auto  anyColumns = any_columns(Model->feedback());
		const auto  name       = row->first == application::overview_feedback_row_kind::remainder ? QStringLiteral("Not on a file")
			: row->second.empty()                                                                 ? QStringLiteral("/")
																								  : text(row->second);
		auto* const menu       = new QMenu{ this };
		// A popup is its own window, which takes the application's palette
		// rather than the theme the window shows, unless it propagates.
		menu->setAttribute(Qt::WA_WindowPropagation);
		menu->setObjectName(QStringLiteral("overviewCommentsMenu"));
		menu->setTitle(QStringLiteral("Comments: %1").arg(name));
		menu->setAccessibleName(menu->title());
		const auto ratio  = devicePixelRatioF();
		const auto open   = counts->Mode == core::feedback_resolution_mode::unresolved;
		const auto amount = [&](const std::size_t aCount, const core::feedback_state_counts& aStates) {
			return open ? QStringLiteral("%1 open").arg(count_text(aCount)) : QStringLiteral("%1 (%2)").arg(count_text(aCount), breakdown_text(aStates));
		};
		const auto add = [&](const QIcon& aIcon, const QString& aText, const core::feedback_count_target& aTarget) {
			auto* const action = menu->addAction(aIcon, aText);
			connect(action, &QAction::triggered, this, [this, kind = row->first, path = row->second, aTarget] { drill(kind, path, aTarget); });
		};
		for (const auto& column : counts->Dedicated) {
			if (column.Count != 0U) {
				add(QIcon{ feedback_mark_pixmap(column.Label, { }, feedback_mark_colours_for(Colours.Marks, column.Label, palette()), ratio) },
					QStringLiteral("%1: %2").arg(feedback_label_text(column.Label), amount(column.Count, column.States)), { .Kind = core::feedback_count_kind::dedicated, .Label = column.Label });
			}
		}
		if (counts->Catchall != 0U) {
			const auto bubble = painted_glyph_icon([anyColumns](QPainter& aPainter, const qreal aSide, const QColor& aInk) { paint_bubble(aPainter, QRectF{ 0.0, 0.0, aSide, aSide }, aInk, anyColumns); },
												   palette().color(QPalette::Text), palette().color(QPalette::Disabled, QPalette::Text));
			add(bubble, QStringLiteral("%1: %2").arg(anyColumns ? QStringLiteral("Other") : QStringLiteral("Comments"), amount(counts->Catchall, counts->CatchallStates)),
				{ .Kind = core::feedback_count_kind::catchall });
		}
		if (counts->Uncertain != 0U) {
			auto state  = Colours.States;
			state.Faint = feedback_state_faint(palette());
			add(QIcon{ feedback_state_mark_pixmap(core::feedback_resolution::unknown, state, ratio) }, QStringLiteral("Not counted: %1").arg(count_text(counts->Uncertain)),
				{ .Kind = core::feedback_count_kind::uncertain });
		}
		if (menu->actions().isEmpty()) {
			menu->addAction(open ? QStringLiteral("No open threads") : QStringLiteral("No threads"))->setEnabled(false);
		}
		const auto clauses = qualifier_clauses(*counts);
		if (!clauses.isEmpty()) {
			menu->addSeparator();
			menu->addAction(clauses.join(QStringLiteral("; ")))->setEnabled(false);
		}
		menu->addSeparator();
		auto* const sort = make_sort_menu();
		sort->setParent(menu, sort->windowFlags());
		menu->addMenu(sort);
		return menu;
	}

	auto pull_request_overview_page::make_sort_menu() -> QMenu*
	{
		auto* const menu = new QMenu{ QStringLiteral("Sort by"), this };
		menu->setAttribute(Qt::WA_WindowPropagation);
		menu->setObjectName(QStringLiteral("overviewSortMenu"));
		auto* const group      = new QActionGroup{ menu };
		const auto  anyColumns = Columns.Count != 0U;
		const auto  add        = [&](const QString& aText, std::optional<core::feedback_count_target> aTarget) {
			auto* const action = menu->addAction(aText);
			action->setCheckable(true);
			action->setChecked(Sort == aTarget);
			group->addAction(action);
			connect(action, &QAction::triggered, this, [this, aTarget] { choose_sort(aTarget); });
		};
		add(QStringLiteral("File order"), std::nullopt);
		add(QStringLiteral("All comments"), core::feedback_count_target{ .Kind = core::feedback_count_kind::included });
		for (const auto label : core::labels_of(Columns)) {
			add(feedback_label_text(label), core::feedback_count_target{ .Kind = core::feedback_count_kind::dedicated, .Label = label });
		}
		if (anyColumns) {
			add(QStringLiteral("Other"), core::feedback_count_target{ .Kind = core::feedback_count_kind::catchall });
		}
		add(QStringLiteral("Not counted"), core::feedback_count_target{ .Kind = core::feedback_count_kind::uncertain });
		return menu;
	}

	void pull_request_overview_page::open_comments_menu(const QModelIndex& aIndex, const QPoint& aGlobal)
	{
		auto* const menu = make_comments_menu(aIndex);
		if (menu == nullptr) {
			return;
		}
		menu->setAttribute(Qt::WA_DeleteOnClose);
		menu->popup(aGlobal);
	}

	void pull_request_overview_page::open_file_at(const QModelIndex& aIndex)
	{
		const auto path = aIndex.siblingAtColumn(overview_file_model::file_column).data(overview_file_model::path_role).toString();
		if (Projection && !path.isEmpty() && FileDispatch) {
			FileDispatch(path);
		} else if (Cached && !path.isEmpty() && CachedSourceDispatch) {
			CachedSourceDispatch(path);
		} else if (ObservedFiles && Loading && !path.isEmpty()) {
			// [[BPR-1842]]. The file opens as soon as the review it belongs
			// to is accepted; the plate says so meanwhile.
			PendingOpen = path;
			apply_pending();
		}
	}

	bool pull_request_overview_page::eventFilter(QObject* const aObject, QEvent* const aEvent)
	{
		if (aObject == Files->viewport()) {
			const auto type = aEvent->type();
			if (type == QEvent::MouseButtonPress || type == QEvent::MouseButtonRelease || type == QEvent::MouseButtonDblClick) {
				const auto* mouse = static_cast<QMouseEvent*>(aEvent);
				const auto  chip  = mouse->button() == Qt::LeftButton ? chip_at(mouse->position().toPoint()) : std::nullopt;
				if (chip) {
					if (type == QEvent::MouseButtonPress) {
						// The chip's row becomes current; a chip opens no file.
						// The pointer leaves no keyboard ring behind.
						Files->setCurrentIndex(chip->first);
						focus_chip({ }, nullptr);
						Files->setFocus(Qt::MouseFocusReason);
					} else if (type == QEvent::MouseButtonRelease) {
						activate_chip(chip->first, chip->second);
					}
					return true;
				}
			}
		} else if (aObject == Files && aEvent->type() == QEvent::KeyPress) {
			if (handle_files_key(*static_cast<QKeyEvent*>(aEvent))) {
				return true;
			}
		} else if (aObject == Files && aEvent->type() == QEvent::FocusIn) {
			// Tab into the files lands on the row, as it always has.
			const auto reason = static_cast<QFocusEvent*>(aEvent)->reason();
			if (reason == Qt::TabFocusReason || reason == Qt::BacktabFocusReason) {
				focus_chip({ }, nullptr);
			}
		} else if (aObject == Files && aEvent->type() == QEvent::FocusOut && ChipFocus) {
			Files->viewport()->update();
		}
		return QWidget::eventFilter(aObject, aEvent);
	}

	auto pull_request_overview_page::handle_files_key(const QKeyEvent& aKey) -> bool
	{
		const auto current   = Files->currentIndex();
		const auto modifiers = aKey.modifiers() & ~Qt::KeypadModifier;
		const auto plain     = modifiers == Qt::NoModifier;
		const auto enter     = aKey.key() == Qt::Key_Return || aKey.key() == Qt::Key_Enter;
		const auto chips     = current.isValid() ? row_chips(current) : std::vector<overview_comment_chip>{ };
		const auto at        = focused_chip_position(chips);
		// [[BPR-1677]]. The document start and end, Command-Up and
		// Command-Down on macOS and Ctrl+Home and Ctrl+End elsewhere, go to
		// the top and bottom row of the list, as the owner asked. A chip
		// focused on the old row is left as any other row change leaves it.
		if (const auto edge = document_edge_for(aKey); edge.has_value() && move_files_to(*edge)) {
			return true;
		}
		// Tab steps from the row onto its chips and along them; past the last
		// it leaves the files as Tab always does. Shift+Tab steps back to the
		// row. The tree keeps Left and Right for folders until a chip is on.
		if (aKey.key() == Qt::Key_Tab && plain) {
			if (!at) {
				if (chips.empty()) {
					return false;
				}
				focus_chip(current, &chips.front());
				return true;
			}
			if (*at + 1U < chips.size()) {
				focus_chip(current, &chips[*at + 1U]);
				return true;
			}
			focus_chip({ }, nullptr);
			return false;
		}
		if (aKey.key() == Qt::Key_Backtab || (aKey.key() == Qt::Key_Tab && modifiers == Qt::ShiftModifier)) {
			if (!at) {
				return false;
			}
			focus_chip(current, *at == 0U ? nullptr : &chips[*at - 1U]);
			return true;
		}
		if (at) {
			if ((aKey.key() == Qt::Key_Left || aKey.key() == Qt::Key_Right) && plain) {
				const auto next = aKey.key() == Qt::Key_Right ? std::min(*at + 1U, chips.size() - 1U) : (*at == 0U ? 0U : *at - 1U);
				focus_chip(current, &chips[next]);
				return true;
			}
			if ((enter || aKey.key() == Qt::Key_Space) && plain) {
				activate_chip(current.siblingAtColumn(overview_file_model::comments_column), chips[*at]);
				return true;
			}
			if (aKey.key() == Qt::Key_Escape && plain) {
				focus_chip({ }, nullptr);
				return true;
			}
		}
		// Enter on a file's row reads it, whichever cell is current. Qt's item
		// view emits no activation for Return on macOS, so the page opens it
		// itself.
		if (enter && plain && current.isValid() && !current.data(overview_file_model::folder_role).toBool()) {
			open_file_at(current);
			return true;
		}
		return false;
	}

	auto pull_request_overview_page::move_files_to(const document_edge aEdge) -> bool
	{
		// The first or last row the tree shows among `aParent`'s children.
		const auto shownChild = [this, aEdge](const QModelIndex& aParent) {
			const auto count = Model->rowCount(aParent);
			for (auto step = 0; step < count; ++step) {
				const auto at = aEdge == document_edge::start ? step : count - 1 - step;
				if (!Files->isRowHidden(at, aParent)) {
					return Model->index(at, 0, aParent);
				}
			}
			return QModelIndex{ };
		};
		auto target = shownChild({ });
		// The bottom row is the last shown row of the deepest open folder.
		while (aEdge == document_edge::end && target.isValid() && Files->isExpanded(target)) {
			const auto last = shownChild(target);
			if (!last.isValid()) {
				break;
			}
			target = last;
		}
		if (!target.isValid()) {
			return false;
		}
		// The current column stays, as the tree's own Home and End keep it.
		const auto current = Files->currentIndex();
		Files->setCurrentIndex(current.isValid() ? target.siblingAtColumn(current.column()) : target);
		if (aEdge == document_edge::start) {
			Files->scrollToTop();
		} else {
			Files->scrollToBottom();
		}
		return true;
	}

	auto pull_request_overview_page::row_chips(const QModelIndex& aRow) const -> std::vector<overview_comment_chip>
	{
		const auto  cell   = aRow.siblingAtColumn(overview_file_model::comments_column);
		const auto* counts = cell.isValid() ? Model->feedback_at(cell) : nullptr;
		if (counts == nullptr || Files->isColumnHidden(overview_file_model::comments_column)) {
			return { };
		}
		return overview_comment_chips(*counts, Files->visualRect(cell), Files->font());
	}

	auto pull_request_overview_page::focused_chip_position(const std::vector<overview_comment_chip>& aChips) const -> std::optional<std::size_t>
	{
		if (!ChipFocus || !ChipFocus->Row.isValid() || ChipFocus->Row != Files->currentIndex().siblingAtColumn(0)) {
			return std::nullopt;
		}
		const auto found = std::ranges::find_if(aChips, [this](const overview_comment_chip& aChip) { return aChip.Kind == ChipFocus->Kind && aChip.Label == ChipFocus->Label; });
		return found == aChips.end() ? std::nullopt : std::optional{ static_cast<std::size_t>(found - aChips.begin()) };
	}

	auto pull_request_overview_page::focused_chip() const -> std::optional<overview_comment_chip>
	{
		const auto chips = row_chips(Files->currentIndex());
		const auto at    = focused_chip_position(chips);
		return at ? std::optional{ chips[*at] } : std::nullopt;
	}

	void pull_request_overview_page::focus_chip(const QModelIndex& aRow, const overview_comment_chip* const aChip)
	{
		if (aChip == nullptr) {
			if (!ChipFocus) {
				return;
			}
			ChipFocus.reset();
		} else {
			auto focus  = overview_chip_focus{ };
			focus.Row   = QPersistentModelIndex{ aRow.siblingAtColumn(0) };
			focus.Kind  = aChip->Kind;
			focus.Label = aChip->Label;
			ChipFocus   = std::move(focus);
		}
		RowDelegate->set_chip_focus(ChipFocus);
		Files->viewport()->update();
		if (aChip == nullptr) {
			return;
		}
		// The keyboard cannot hover: the chip's tooltip comes up beside it, and
		// a screen reader hears the same words.
		const auto  cell   = aRow.siblingAtColumn(overview_file_model::comments_column);
		const auto* counts = Model->feedback_at(cell);
		if (counts == nullptr) {
			return;
		}
		const auto tip = chip_tooltip(*aChip, *counts, any_columns(Model->feedback()));
		show_anchored_tooltip(*Files->viewport(), aChip->Box.toAlignedRect(), tip, anchored_tooltip_hold::focus);
		QAccessibleAnnouncementEvent announcement{ Files, QString{ tip }.replace(QLatin1Char{ '\n' }, QLatin1Char{ ' ' }) };
		announcement.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
		QAccessible::updateAccessibility(&announcement);
	}
}
