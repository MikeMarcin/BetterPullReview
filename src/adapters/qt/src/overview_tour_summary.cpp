#include "bpr/qt/overview_tour_summary.hpp"

#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/tour_publication_surfaces.hpp"
#include "github_presentation_text.hpp"

#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <ranges>

namespace bpr::qt {
	namespace {
		constexpr int ring_size = 22;

		[[nodiscard]] auto dark(const QPalette& aPalette) -> bool
		{
			return aPalette.color(QPalette::Window).lightness() < 128;
		}

		/// The route's amber and the ready green, as Review's route draws them.
		[[nodiscard]] auto route_amber(const QPalette& aPalette) -> QColor
		{
			return dark(aPalette) ? QColor{ 0xE3, 0xB3, 0x41 } : QColor{ 0xC3, 0x8A, 0x10 };
		}

		[[nodiscard]] auto route_green(const QPalette& aPalette) -> QColor
		{
			return dark(aPalette) ? QColor{ 0x3F, 0xB9, 0x50 } : QColor{ 0x1A, 0x7F, 0x37 };
		}

		/// [[BPR-2014]], RFC-BPR-0106. Review's words for a stop, as for a file.
		[[nodiscard]] auto readiness_words(const core::tour_acceptance_state aState) -> QString
		{
			switch (aState) {
			case core::tour_acceptance_state::accepted:     return QObject::tr("Accepted");
			case core::tour_acceptance_state::changed:      return QObject::tr("Changed since accepted");
			case core::tour_acceptance_state::not_accepted: return QObject::tr("Not accepted yet");
			}
			return { };
		}

		[[nodiscard]] auto text(const std::string& aValue) -> QString
		{
			return QString::fromUtf8(aValue.data(), static_cast<qsizetype>(aValue.size()));
		}

		/// \brief A stop's numbered ring on the route line, its readiness in
		/// the ring as Review's route draws it; the line runs through the
		/// row's height except above the first stop and below the last.
		class route_ring final : public QWidget
		{
		public:
			explicit route_ring(QWidget* aParent)
				: QWidget{ aParent }
			{
				setFixedWidth(ring_size + 4);
				setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
				setMinimumHeight(ring_size + 8);
			}

			int                         Number     = 1;
			bool                        First      = false;
			bool                        Last       = false;
			bool                        Writer     = true;
			core::tour_acceptance_state Acceptance = core::tour_acceptance_state::not_accepted;

		protected:
			void paintEvent(QPaintEvent*) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing);
				const auto amber  = route_amber(palette());
				const auto base   = palette().color(QPalette::Active, QPalette::Base);
				const auto ink    = palette().color(QPalette::Active, QPalette::WindowText);
				const auto centre = QPointF{ width() / 2.0, 4.0 + ring_size / 2.0 };
				const auto ring   = QRectF{ centre.x() - ring_size / 2.0, centre.y() - ring_size / 2.0, static_cast<qreal>(ring_size), static_cast<qreal>(ring_size) };
				auto       line   = amber;
				line.setAlphaF(0.55F);
				painter.setPen(QPen{ line, 2.0 });
				if (!First) {
					painter.drawLine(QPointF{ centre.x(), 0.0 }, QPointF{ centre.x(), ring.top() });
				}
				if (!Last) {
					painter.drawLine(QPointF{ centre.x(), ring.bottom() }, QPointF{ centre.x(), static_cast<qreal>(height()) });
				}
				auto small = font();
				small.setWeight(QFont::DemiBold);
				small.setPointSizeF(std::max(1.0, small.pointSizeF() * 0.86));
				painter.setFont(small);
				const auto number = QString::number(Number);
				// A reader's route has no acceptance: every ring is amber.
				const auto accepted = Writer && Acceptance == core::tour_acceptance_state::accepted;
				if (accepted) {
					painter.setPen(QPen{ route_green(palette()), 1.6 });
					painter.setBrush(base);
					painter.drawEllipse(ring.adjusted(1.0, 1.0, -1.0, -1.0));
					auto tick = QPainterPath{ };
					tick.moveTo(ring.left() + 6.5, ring.center().y() + 0.5);
					tick.lineTo(ring.left() + 9.5, ring.center().y() + 3.5);
					tick.lineTo(ring.right() - 6.0, ring.center().y() - 3.0);
					painter.setBrush(Qt::NoBrush);
					painter.drawPath(tick);
					return;
				}
				painter.setPen(QPen{ amber, 1.6 });
				painter.setBrush(base);
				painter.drawEllipse(ring.adjusted(1.0, 1.0, -1.0, -1.0));
				const auto keep = 0.65;
				painter.setPen(QColor::fromRgbF(static_cast<float>(ink.redF() * keep + amber.redF() * (1.0 - keep)), static_cast<float>(ink.greenF() * keep + amber.greenF() * (1.0 - keep)),
												static_cast<float>(ink.blueF() * keep + amber.blueF() * (1.0 - keep))));
				painter.drawText(ring, Qt::AlignCenter, number);
				if (Writer && Acceptance == core::tour_acceptance_state::changed) {
					auto       warn = QPainterPath{ };
					const auto x    = ring.right() - 3.0;
					const auto y    = ring.top() - 2.0;
					warn.moveTo(x, y);
					warn.lineTo(x + 5.0, y + 8.5);
					warn.lineTo(x - 5.0, y + 8.5);
					warn.closeSubpath();
					painter.setPen(QPen{ base, 1.5 });
					painter.setBrush(amber);
					painter.drawPath(warn);
				}
			}
		};

		/// \brief [[BPR-1993]]. A stop row that opens its stop: a click, or
		/// Return or Space with the keyboard on it.
		class stop_row_widget final : public QWidget
		{
		public:
			using QWidget::QWidget;
			std::function<void()> Open;
			bool                  Opens = false;

		protected:
			void mouseReleaseEvent(QMouseEvent* aEvent) override
			{
				if (Opens && Open && aEvent->button() == Qt::LeftButton && rect().contains(aEvent->position().toPoint())) {
					Open();
					return;
				}
				QWidget::mouseReleaseEvent(aEvent);
			}
			void keyPressEvent(QKeyEvent* aEvent) override
			{
				if (Opens && Open && (aEvent->key() == Qt::Key_Return || aEvent->key() == Qt::Key_Enter || aEvent->key() == Qt::Key_Space)) {
					Open();
					return;
				}
				QWidget::keyPressEvent(aEvent);
			}
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				if (!Opens || !(underMouse() || hasFocus())) {
					return;
				}
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing);
				auto wash = palette().color(QPalette::Active, QPalette::Highlight);
				wash.setAlphaF(hasFocus() ? 0.14F : 0.07F);
				painter.setPen(Qt::NoPen);
				painter.setBrush(wash);
				painter.drawRoundedRect(QRectF{ rect() }.adjusted(ring_size + 6.0, 1.0, 0.0, -6.0), 6.0, 6.0);
			}
			void enterEvent(QEnterEvent* aEvent) override
			{
				QWidget::enterEvent(aEvent);
				update();
			}
			void leaveEvent(QEvent* aEvent) override
			{
				QWidget::leaveEvent(aEvent);
				update();
			}
			void focusInEvent(QFocusEvent* aEvent) override
			{
				QWidget::focusInEvent(aEvent);
				update();
			}
			void focusOutEvent(QFocusEvent* aEvent) override
			{
				QWidget::focusOutEvent(aEvent);
				update();
			}
		};

		[[nodiscard]] auto heading(const QString& aText, QWidget* aParent, const QString& aName) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			label->setObjectName(aName);
			auto font = label->font();
			font.setWeight(QFont::DemiBold);
			label->setFont(font);
			detail::set_plain_text_label(label);
			return label;
		}

		[[nodiscard]] auto prose(QWidget* aParent, const QString& aName) -> QLabel*
		{
			auto* const label = new QLabel{ aParent };
			label->setObjectName(aName);
			label->setWordWrap(true);
			label->setTextInteractionFlags(Qt::TextSelectableByMouse);
			detail::set_plain_text_label(label);
			return label;
		}
	}

	overview_tour_summary::overview_tour_summary(QWidget* const aParent)
		: QScrollArea{ aParent }
	{
		setObjectName(QStringLiteral("overviewTourSummary"));
		setAccessibleName(tr("Review tour"));
		setFrameShape(QFrame::NoFrame);
		setWidgetResizable(true);
		setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		auto* const page = new QWidget{ this };
		page->setObjectName(QStringLiteral("overviewTourPage"));
		page->setAutoFillBackground(true);
		page->setBackgroundRole(QPalette::Base);
		auto* const column = new QVBoxLayout{ page };
		column->setContentsMargins(14, 12, 14, 14);
		column->setSpacing(4);
		State = prose(page, QStringLiteral("overviewTourState"));
		State->setForegroundRole(QPalette::PlaceholderText);
		column->addWidget(State);
		// [[BPR-1993]]. A writer's state line and actions are the Tour tab
		// head's, so publishing, updating or a stopped publication is seen
		// and acted on here too.
		Head = new tour_head_bar{ QStringLiteral("overviewTourHead"), page };
		Head->layout()->setContentsMargins(0, 0, 0, 4);
		Head->set_dispatch([this](const core::tour_head_action aAction) {
			if (HeadDispatch) {
				HeadDispatch(aAction);
			}
		});
		column->addWidget(Head);
		Invitation = prose(page, QStringLiteral("overviewTourInvitation"));
		Invitation->setText(tr("Walk your reviewers through this change: the problem, your solution, and a route through the code, stop by stop. It stays private until you publish it."));
		column->addWidget(Invitation);
		column->addSpacing(6);
		ProblemHeading = heading(tr("Problem"), page, QStringLiteral("overviewTourProblemHeading"));
		// [[BPR-1993]]. Tour text is authored as Markdown and read so, as the
		// Tour tab reads it.
		Problem = new markdown_body_view{ page };
		Problem->setObjectName(QStringLiteral("overviewTourProblem"));
		Problem->setAccessibleName(tr("Problem"));
		Problem->set_empty_body_text(tr("Not written yet"));
		SolutionHeading = heading(tr("Solution"), page, QStringLiteral("overviewTourSolutionHeading"));
		Solution        = new markdown_body_view{ page };
		Solution->setObjectName(QStringLiteral("overviewTourSolution"));
		Solution->setAccessibleName(tr("Solution"));
		Solution->set_empty_body_text(tr("Not written yet"));
		column->addWidget(ProblemHeading);
		column->addWidget(Problem);
		column->addSpacing(8);
		column->addWidget(SolutionHeading);
		column->addWidget(Solution);
		column->addSpacing(10);
		StopsHeading = heading(QString{ }, page, QStringLiteral("overviewTourStopsHeading"));
		column->addWidget(StopsHeading);
		Route = new QWidget{ page };
		Route->setObjectName(QStringLiteral("overviewTourRoute"));
		Route->setAccessibleName(tr("Stops"));
		RouteLayout = new QVBoxLayout{ Route };
		RouteLayout->setContentsMargins(0, 2, 0, 0);
		RouteLayout->setSpacing(0);
		column->addWidget(Route);
		column->addSpacing(12);
		Action = new QPushButton{ page };
		Action->setObjectName(QStringLiteral("overviewTourAction"));
		connect(Action, &QPushButton::clicked, this, [this] {
			if (!Dispatch) {
				return;
			}
			switch (Shown.Kind) {
			case core::overview_tour_kind::prepare: Dispatch(overview_tour_gesture::prepare); return;
			case core::overview_tour_kind::writer:  Dispatch(overview_tour_gesture::continue_tour); return;
			case core::overview_tour_kind::reader:  Dispatch(overview_tour_gesture::start_tour); return;
			case core::overview_tour_kind::hidden:  return;
			}
		});
		column->addWidget(Action, 0, Qt::AlignLeft);
		column->addStretch(1);
		setWidget(page);
		present({ });
	}

	void overview_tour_summary::set_dispatch(gesture_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void overview_tour_summary::set_stop_dispatch(std::function<void(core::tour_stop_id)> aDispatch)
	{
		StopDispatch = std::move(aDispatch);
	}

	void overview_tour_summary::set_head_dispatch(std::function<void(core::tour_head_action)> aDispatch)
	{
		HeadDispatch = std::move(aDispatch);
	}

	void overview_tour_summary::open_row(const std::size_t aIndex)
	{
		if (aIndex < Shown.Stops.size() && StopDispatch && (Shown.Kind == core::overview_tour_kind::writer || Shown.Imported)) {
			StopDispatch(Shown.Stops[aIndex].Id);
		}
	}

	void overview_tour_summary::present(const core::overview_tour& aTour, const core::tour_head& aHead)
	{
		const auto sameRoute = Rows.size() == aTour.Stops.size()
			&& std::ranges::equal(Shown.Stops, aTour.Stops, [](const auto& aLeft, const auto& aRight) { return aLeft.Id == aRight.Id; });
		Shown              = aTour;
		ShownHead          = aHead;
		const auto kind    = Shown.Kind;
		const auto writer  = kind == core::overview_tour_kind::writer;
		const auto reader  = kind == core::overview_tour_kind::reader;
		const auto touring = writer || reader;
		// A writer with a head reads it in the head's words; otherwise the line.
		const auto headed = writer && aHead.State != core::tour_head_state::none;
		Head->present(headed ? aHead : core::tour_head{ });
		State->setText(writer       ? (Shown.Published ? tr("Published on the pull request. Changes stay private until you update the tour.")
													   : tr("A private draft. Nothing is published until you publish the tour."))
						   : reader ? tr("A published tour of this pull request. Follow it stop by stop in Review.")
									: QString{ });
		State->setVisible(touring && !headed);
		Invitation->setVisible(kind == core::overview_tour_kind::prepare);
		// A writer sees an empty field as still to write; a reader sees only
		// what was written.
		const auto field = [writer](QLabel* aHeading, markdown_body_view* aBody, const std::string& aValue, const char* aKey) {
			const auto empty = aValue.empty();
			aBody->set_body(aValue, std::string{ "overview-tour-" } + aKey);
			const auto shown = writer || !empty;
			aHeading->setVisible(shown);
			aBody->setVisible(shown);
		};
		if (touring) {
			field(ProblemHeading, Problem, Shown.Problem, "problem");
			field(SolutionHeading, Solution, Shown.Solution, "solution");
		} else {
			for (auto* const part : std::initializer_list<QWidget*>{ ProblemHeading, Problem, SolutionHeading, Solution }) {
				part->hide();
			}
		}
		const auto count = static_cast<int>(Shown.Stops.size());
		StopsHeading->setText(count == 0 ? tr("No stops yet") : count == 1 ? tr("1 stop")
																		   : tr("%1 stops").arg(count));
		StopsHeading->setVisible(touring);
		Route->setVisible(touring && count > 0);
		if (sameRoute) {
			refresh_stops();
		} else {
			rebuild_stops();
		}
		const auto label = kind == core::overview_tour_kind::prepare ? tr("Prepare review tour") : writer ? tr("Continue in Review")
																										  : tr("Start tour");
		Action->setText(label);
		Action->setAccessibleName(label);
		Action->setToolTip(kind == core::overview_tour_kind::prepare ? tr("Open Review's Tour tab to write the problem, the solution and the stops")
							   : writer                              ? tr("Open Review's Tour tab at the stop you were on")
																	 : tr("Open Review at the tour's first stop"));
		Action->setVisible(kind != core::overview_tour_kind::hidden);
		Action->setEnabled(kind != core::overview_tour_kind::hidden && (!reader || count > 0));
	}

	void overview_tour_summary::rebuild_stops()
	{
		for (auto& row : Rows) {
			delete row.Row;
		}
		Rows.clear();
		for (std::size_t index = 0U; index < Shown.Stops.size(); ++index) {
			auto        row    = stop_row{ };
			auto* const opener = new stop_row_widget{ Route };
			opener->Open       = [this, index] { open_row(index); };
			row.Row            = opener;
			row.Row->setObjectName(QStringLiteral("overviewTourStop"));
			auto* const line = new QHBoxLayout{ row.Row };
			line->setContentsMargins(0, 0, 0, 0);
			line->setSpacing(8);
			row.Ring = new route_ring{ row.Row };
			line->addWidget(row.Ring);
			auto* const words = new QVBoxLayout{ };
			words->setContentsMargins(0, 5, 0, 14);
			words->setSpacing(1);
			row.Title = prose(row.Row, QStringLiteral("overviewTourStopTitle"));
			auto font = row.Title->font();
			font.setWeight(QFont::DemiBold);
			row.Title->setFont(font);
			words->addWidget(row.Title);
			// [[BPR-1993]]. Where the stop is and what it says first.
			row.Place = new QLabel{ row.Row };
			row.Place->setObjectName(QStringLiteral("overviewTourStopPlace"));
			detail::set_plain_text_label(row.Place);
			row.Place->setForegroundRole(QPalette::PlaceholderText);
			words->addWidget(row.Place);
			row.Lead = prose(row.Row, QStringLiteral("overviewTourStopLead"));
			words->addWidget(row.Lead);
			row.Readiness = new QLabel{ row.Row };
			row.Readiness->setObjectName(QStringLiteral("overviewTourStopReadiness"));
			detail::set_plain_text_label(row.Readiness);
			auto small = row.Readiness->font();
			small.setPointSizeF(std::max(1.0, small.pointSizeF() * 0.9));
			row.Readiness->setFont(small);
			words->addWidget(row.Readiness);
			line->addLayout(words, 1);
			RouteLayout->addWidget(row.Row);
			Rows.push_back(row);
		}
		refresh_stops();
	}

	void overview_tour_summary::refresh_stops()
	{
		const auto writer = Shown.Kind == core::overview_tour_kind::writer;
		for (std::size_t index = 0U; index < Rows.size() && index < Shown.Stops.size(); ++index) {
			const auto& stop = Shown.Stops[index];
			auto&       row  = Rows[index];
			auto* const ring = static_cast<route_ring*>(row.Ring);
			ring->Number     = static_cast<int>(index) + 1;
			ring->First      = index == 0U;
			ring->Last       = index + 1U == Rows.size();
			ring->Writer     = writer;
			ring->Acceptance = stop.Acceptance;
			ring->update();
			const auto title = stop.Title.empty() ? tr("Untitled stop") : text(stop.Title);
			row.Title->setText(title);
			const auto file  = text(stop.Path).section(QLatin1Char{ '/' }, -1);
			const auto place = stop.Path.empty() ? QString{ } : stop.FirstLine == stop.LastLine ? tr("%1, line %2").arg(file).arg(stop.FirstLine)
																								: tr("%1, lines %2\u2013%3").arg(file).arg(stop.FirstLine).arg(stop.LastLine);
			row.Place->setText(place);
			row.Place->setToolTip(text(stop.Path));
			row.Place->setVisible(!place.isEmpty());
			row.Lead->setText(text(stop.Lead));
			row.Lead->setVisible(!stop.Lead.empty());
			auto* const opener = static_cast<stop_row_widget*>(row.Row);
			opener->Opens      = writer || Shown.Imported;
			opener->setFocusPolicy(opener->Opens ? Qt::StrongFocus : Qt::NoFocus);
			opener->setCursor(opener->Opens ? Qt::PointingHandCursor : Qt::ArrowCursor);
			opener->setToolTip(opener->Opens ? tr("Open this stop in Review") : QString{ });
			opener->setAttribute(Qt::WA_Hover, opener->Opens);
			row.Title->setForegroundRole(stop.Title.empty() ? QPalette::PlaceholderText : QPalette::WindowText);
			const auto readiness = writer ? readiness_words(stop.Acceptance) : QString{ };
			row.Readiness->setText(readiness);
			row.Readiness->setVisible(writer);
			row.Row->setAccessibleName(writer ? tr("Stop %1, %2, %3").arg(index + 1U).arg(title, readiness) : tr("Stop %1, %2").arg(index + 1U).arg(title));
		}
		refresh_colours();
	}

	void overview_tour_summary::refresh_colours()
	{
		// The readiness words in the colours of their rings; the palette's
		// change redraws them.
		for (std::size_t index = 0U; index < Rows.size() && index < Shown.Stops.size(); ++index) {
			auto       colours = Rows[index].Readiness->palette();
			const auto state   = Shown.Stops[index].Acceptance;
			const auto colour  = state == core::tour_acceptance_state::accepted ? route_green(palette())
				: state == core::tour_acceptance_state::changed                 ? route_amber(palette())
																				: palette().color(QPalette::PlaceholderText);
			colours.setColor(QPalette::WindowText, colour);
			Rows[index].Readiness->setPalette(colours);
		}
	}

	auto overview_tour_summary::stop_titles() const -> QStringList
	{
		auto result = QStringList{ };
		for (const auto& row : Rows) {
			result.push_back(row.Title->text());
		}
		return result;
	}

	auto overview_tour_summary::stop_places() const -> QStringList
	{
		auto result = QStringList{ };
		for (const auto& row : Rows) {
			result.push_back(row.Place->text());
		}
		return result;
	}

	auto overview_tour_summary::stop_leads() const -> QStringList
	{
		auto result = QStringList{ };
		for (const auto& row : Rows) {
			result.push_back(row.Lead->text());
		}
		return result;
	}

	auto overview_tour_summary::stop_readiness() const -> QStringList
	{
		auto result = QStringList{ };
		for (const auto& row : Rows) {
			if (!row.Readiness->isHidden()) {
				result.push_back(row.Readiness->text());
			}
		}
		return result;
	}

	void overview_tour_summary::changeEvent(QEvent* const aEvent)
	{
		QScrollArea::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange) {
			refresh_colours();
		}
	}
} // namespace bpr::qt
