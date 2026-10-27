#include "bpr/qt/overview_tour_summary.hpp"

#include "github_presentation_text.hpp"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
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

		[[nodiscard]] auto readiness_words(const core::tour_preparation aPreparation) -> QString
		{
			switch (aPreparation) {
			case core::tour_preparation::ready:           return QObject::tr("Ready");
			case core::tour_preparation::needs_attention: return QObject::tr("Needs attention");
			case core::tour_preparation::suggested:       return QObject::tr("Suggested");
			case core::tour_preparation::editing:         return QObject::tr("Editing");
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

			int                    Number      = 1;
			bool                   First       = false;
			bool                   Last        = false;
			bool                   Writer      = true;
			core::tour_preparation Preparation = core::tour_preparation::editing;

		protected:
			void paintEvent(QPaintEvent*) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing);
				const auto amber  = route_amber(palette());
				const auto base   = palette().color(QPalette::Active, QPalette::Base);
				const auto faint  = palette().color(QPalette::Active, QPalette::PlaceholderText);
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
				// A reader's route has no readiness: every ring is amber.
				const auto preparation = Writer ? Preparation : core::tour_preparation::editing;
				if (preparation == core::tour_preparation::ready) {
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
				if (preparation == core::tour_preparation::suggested) {
					painter.setPen(QPen{ faint, 1.4, Qt::DashLine });
					painter.setBrush(base);
					painter.drawEllipse(ring.adjusted(1.0, 1.0, -1.0, -1.0));
					painter.setPen(faint);
					painter.drawText(ring, Qt::AlignCenter, number);
					return;
				}
				painter.setPen(QPen{ amber, 1.6 });
				painter.setBrush(base);
				painter.drawEllipse(ring.adjusted(1.0, 1.0, -1.0, -1.0));
				const auto keep = 0.65;
				painter.setPen(QColor::fromRgbF(static_cast<float>(ink.redF() * keep + amber.redF() * (1.0 - keep)), static_cast<float>(ink.greenF() * keep + amber.greenF() * (1.0 - keep)),
												static_cast<float>(ink.blueF() * keep + amber.blueF() * (1.0 - keep))));
				painter.drawText(ring, Qt::AlignCenter, number);
				if (preparation == core::tour_preparation::needs_attention) {
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
		Invitation = prose(page, QStringLiteral("overviewTourInvitation"));
		Invitation->setText(tr("Walk your reviewers through this change: the problem, your solution, and a route through the code, stop by stop. It stays private until you publish it."));
		column->addWidget(Invitation);
		column->addSpacing(6);
		ProblemHeading  = heading(tr("Problem"), page, QStringLiteral("overviewTourProblemHeading"));
		Problem         = prose(page, QStringLiteral("overviewTourProblem"));
		SolutionHeading = heading(tr("Solution"), page, QStringLiteral("overviewTourSolutionHeading"));
		Solution        = prose(page, QStringLiteral("overviewTourSolution"));
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

	void overview_tour_summary::present(const core::overview_tour& aTour)
	{
		const auto sameRoute = Rows.size() == aTour.Stops.size()
			&& std::ranges::equal(Shown.Stops, aTour.Stops, [](const auto& aLeft, const auto& aRight) { return aLeft.Id == aRight.Id; });
		Shown              = aTour;
		const auto kind    = Shown.Kind;
		const auto writer  = kind == core::overview_tour_kind::writer;
		const auto reader  = kind == core::overview_tour_kind::reader;
		const auto touring = writer || reader;
		State->setText(writer       ? (Shown.Published ? tr("Published on the pull request. Changes stay private until you update the tour.")
													   : tr("A private draft. Nothing is published until you publish the tour."))
						   : reader ? tr("A published tour of this pull request. Follow it stop by stop in Review.")
									: QString{ });
		State->setVisible(touring);
		Invitation->setVisible(kind == core::overview_tour_kind::prepare);
		// A writer sees an empty field as still to write; a reader sees only
		// what was written.
		const auto field = [writer](QLabel* aHeading, QLabel* aBody, const std::string& aValue) {
			const auto empty = aValue.empty();
			aBody->setText(empty ? tr("Not written yet") : text(aValue));
			aBody->setForegroundRole(empty ? QPalette::PlaceholderText : QPalette::WindowText);
			const auto shown = writer || !empty;
			aHeading->setVisible(shown);
			aBody->setVisible(shown);
		};
		if (touring) {
			field(ProblemHeading, Problem, Shown.Problem);
			field(SolutionHeading, Solution, Shown.Solution);
		} else {
			for (auto* const label : { ProblemHeading, Problem, SolutionHeading, Solution }) {
				label->hide();
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
			auto row = stop_row{ };
			row.Row  = new QWidget{ Route };
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
			const auto& stop  = Shown.Stops[index];
			auto&       row   = Rows[index];
			auto* const ring  = static_cast<route_ring*>(row.Ring);
			ring->Number      = static_cast<int>(index) + 1;
			ring->First       = index == 0U;
			ring->Last        = index + 1U == Rows.size();
			ring->Writer      = writer;
			ring->Preparation = stop.Preparation;
			ring->update();
			const auto title = stop.Title.empty() ? tr("Untitled stop") : text(stop.Title);
			row.Title->setText(title);
			row.Title->setForegroundRole(stop.Title.empty() ? QPalette::PlaceholderText : QPalette::WindowText);
			const auto readiness = writer ? readiness_words(stop.Preparation) : QString{ };
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
			const auto prep    = Shown.Stops[index].Preparation;
			const auto colour  = prep == core::tour_preparation::ready ? route_green(palette())
				: prep == core::tour_preparation::needs_attention      ? route_amber(palette())
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
