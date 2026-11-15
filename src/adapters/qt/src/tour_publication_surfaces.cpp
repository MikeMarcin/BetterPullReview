#include "bpr/qt/tour_publication_surfaces.hpp"

#include "bpr/qt/quiet_push_button.hpp"

#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto night(const QPalette& aPalette) -> bool
		{
			return aPalette.color(QPalette::Active, QPalette::Window).lightness() < 128;
		}
		[[nodiscard]] auto mixed(const QColor& aFrom, const QColor& aTo, const qreal aShare) -> QColor
		{
			return QColor::fromRgbF(static_cast<float>(aFrom.redF() + (aTo.redF() - aFrom.redF()) * aShare), static_cast<float>(aFrom.greenF() + (aTo.greenF() - aFrom.greenF()) * aShare),
									static_cast<float>(aFrom.blueF() + (aTo.blueF() - aFrom.blueF()) * aShare));
		}
		/// The route's green, as the stop ring's accepted state draws it.
		[[nodiscard]] auto sent_green(const QPalette& aPalette) -> QColor
		{
			return night(aPalette) ? QColor{ 0x3F, 0xB9, 0x50 } : QColor{ 0x1A, 0x7F, 0x37 };
		}
		[[nodiscard]] auto sending_amber(const QPalette& aPalette) -> QColor
		{
			return night(aPalette) ? QColor{ 0xE3, 0xB3, 0x41 } : QColor{ 0xC3, 0x8A, 0x10 };
		}
		[[nodiscard]] auto text(const std::string& aValue) -> QString
		{
			return QString::fromUtf8(aValue.data(), static_cast<qsizetype>(aValue.size()));
		}
		constexpr std::array head_actions{ core::tour_head_action::publish, core::tour_head_action::details, core::tour_head_action::resume, core::tour_head_action::on_github, core::tour_head_action::update };
		[[nodiscard]] auto   action_name(const core::tour_head_action aAction) -> QString
		{
			switch (aAction) {
			case core::tour_head_action::publish:   return QStringLiteral("Publish");
			case core::tour_head_action::details:   return QStringLiteral("Details");
			case core::tour_head_action::resume:    return QStringLiteral("Resume");
			case core::tour_head_action::on_github: return QStringLiteral("OnGitHub");
			case core::tour_head_action::update:    return QStringLiteral("Update");
			}
			return { };
		}
		[[nodiscard]] auto head_title(const core::tour_head& aHead) -> QString
		{
			using state = core::tour_head_state;
			switch (aHead.State) {
			case state::none:              return { };
			case state::not_publishable:
			case state::publishable:       return QObject::tr("Private draft");
			case state::publishing:        return QObject::tr("Publishing");
			case state::stopped:           return QObject::tr("Publishing stopped");
			case state::published:
			case state::published_changed: return QObject::tr("Published");
			}
			return { };
		}
		[[nodiscard]] auto head_detail(const core::tour_head& aHead) -> QString
		{
			using state      = core::tour_head_state;
			const auto stops = aHead.Stops == 0U ? QObject::tr("no stops yet") : aHead.Stops == 1U ? QObject::tr("1 stop")
																								   : QObject::tr("%1 stops").arg(aHead.Stops);
			switch (aHead.State) {
			case state::none:            return { };
			case state::not_publishable: return aHead.ToAccept > 0U ? QObject::tr("%1, %2 to accept").arg(stops).arg(aHead.ToAccept) : stops;
			case state::publishable:     return aHead.Stops == 1U ? QObject::tr("1 stop, accepted") : QObject::tr("%1, all accepted").arg(stops);
			case state::publishing:      return QObject::tr("step %1 of %2").arg(aHead.Step).arg(aHead.Steps);
			case state::stopped:         return QObject::tr("at step %1 of %2").arg(aHead.Step).arg(aHead.Steps);
			case state::published:       return { };
			case state::published_changed:
				return aHead.Changes == 1U ? QObject::tr("1 change not on GitHub") : QObject::tr("%1 changes not on GitHub").arg(aHead.Changes);
			}
			return { };
		}
	}

	auto tour_error_colour(const QPalette& aPalette) -> QColor
	{
		return night(aPalette) ? QColor{ 0xF8, 0x51, 0x49 } : QColor{ 0xCF, 0x22, 0x2E };
	}

	auto tour_head_text(const core::tour_head& aHead) -> QString
	{
		const auto title  = head_title(aHead);
		const auto detail = head_detail(aHead);
		return detail.isEmpty() ? title : title + QStringLiteral(", ") + detail;
	}

	auto tour_head_missing_text(const core::tour_head& aHead) -> QString
	{
		using missing = core::tour_head_missing;
		switch (aHead.Missing) {
		case missing::nothing:    return { };
		case missing::conflict:   return QObject::tr("Settle the conflict with the saved copy or the website first.");
		case missing::problem:    return QObject::tr("Write the Problem first.");
		case missing::solution:   return QObject::tr("Write the Solution first.");
		case missing::stops:      return QObject::tr("Add a stop first.");
		case missing::acceptance: return aHead.ToAccept == 1U ? QObject::tr("Accept the remaining stop first: it needs confirmed lines and an explanation current at this head.")
															  : QObject::tr("Accept the remaining %1 stops first: each needs confirmed lines and an explanation current at this head.").arg(aHead.ToAccept);
		}
		return { };
	}

	auto tour_head_action_text(const core::tour_head_action aAction) -> QString
	{
		switch (aAction) {
		case core::tour_head_action::publish:   return QObject::tr("Publish…");
		case core::tour_head_action::details:   return QObject::tr("Details");
		case core::tour_head_action::resume:    return QObject::tr("Resume");
		case core::tour_head_action::on_github: return QObject::tr("On GitHub");
		case core::tour_head_action::update:    return QObject::tr("Update on GitHub…");
		}
		return { };
	}

	tour_head_bar::tour_head_bar(const QString& aName, QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(aName);
		setAccessibleName(tr("Tour publication"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(10, 7, 10, 7);
		column->setSpacing(5);
		auto* const row = new QHBoxLayout{ };
		row->setSpacing(6);
		State = new QLabel{ this };
		State->setObjectName(aName + QStringLiteral("State"));
		State->setTextFormat(Qt::RichText);
		State->setWordWrap(true);
		State->setTextInteractionFlags(Qt::NoTextInteraction);
		State->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		row->addWidget(State, 1, Qt::AlignVCenter);
		for (std::size_t index = 0U; index < head_actions.size(); ++index) {
			const auto action = head_actions[index];
			// Each action as the primary button and as a quiet one: the first
			// shown is primary, whichever it is.
			Primary[index] = new QPushButton{ tour_head_action_text(action), this };
			Quiet[index]   = new quiet_push_button{ tour_head_action_text(action), this };
			Primary[index]->setObjectName(aName + action_name(action));
			Quiet[index]->setObjectName(aName + action_name(action) + QStringLiteral("Quiet"));
			for (auto* const control : { Primary[index], Quiet[index] }) {
				control->hide();
				connect(control, &QPushButton::clicked, this, [this, action] {
					if (Dispatch) {
						Dispatch(action);
					}
				});
				row->addWidget(control, 0, Qt::AlignVCenter);
			}
		}
		column->addLayout(row);
		Progress = new QProgressBar{ this };
		Progress->setObjectName(aName + QStringLiteral("Progress"));
		Progress->setTextVisible(false);
		Progress->setFixedHeight(4);
		Progress->setAccessibleName(tr("Publication progress"));
		Progress->hide();
		column->addWidget(Progress);
		present({ });
	}

	void tour_head_bar::set_dispatch(action_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	auto tour_head_bar::button(const core::tour_head_action aAction) const -> QPushButton*
	{
		const auto at = static_cast<std::size_t>(std::ranges::find(head_actions, aAction) - head_actions.begin());
		if (at >= head_actions.size()) {
			return nullptr;
		}
		return !Primary[at]->isHidden() ? Primary[at] : !Quiet[at]->isHidden() ? static_cast<QPushButton*>(Quiet[at])
																			   : nullptr;
	}

	auto tour_head_bar::shown_actions() const -> std::vector<core::tour_head_action>
	{
		return Shown.Actions;
	}

	void tour_head_bar::present(const core::tour_head& aHead)
	{
		Shown = aHead;
		setVisible(aHead.State != core::tour_head_state::none);
		const auto title  = head_title(aHead);
		const auto detail = head_detail(aHead);
		State->setText(detail.isEmpty() ? QStringLiteral("<b>%1</b>").arg(title.toHtmlEscaped()) : QStringLiteral("<b>%1</b>&nbsp; %2").arg(title.toHtmlEscaped(), detail.toHtmlEscaped()));
		State->setAccessibleName(tour_head_text(aHead));
		for (std::size_t index = 0U; index < head_actions.size(); ++index) {
			const auto position = std::ranges::find(aHead.Actions, head_actions[index]);
			const auto shown    = position != aHead.Actions.end();
			const auto primary  = shown && position == aHead.Actions.begin();
			Primary[index]->setVisible(primary);
			Quiet[index]->setVisible(shown && !primary);
		}
		// The actions in the decided order: move each shown one to the end.
		auto* const row = static_cast<QHBoxLayout*>(static_cast<QVBoxLayout*>(layout())->itemAt(0)->layout());
		for (const auto action : aHead.Actions) {
			if (auto* const control = button(action)) {
				row->removeWidget(control);
				row->addWidget(control, 0, Qt::AlignVCenter);
			}
		}
		if (auto* const publish = button(core::tour_head_action::publish)) {
			publish->setEnabled(!aHead.PublishDisabled);
			publish->setToolTip(aHead.PublishDisabled ? tour_head_missing_text(aHead) : tr("Open the tour page with what GitHub will show. Nothing is sent until Publish tour."));
		}
		if (auto* const update = button(core::tour_head_action::update)) {
			update->setToolTip(tr("Open the tour page with the update GitHub will get. Nothing is sent until you publish it."));
		}
		if (auto* const details = button(core::tour_head_action::details)) {
			details->setToolTip(tr("Open the tour page at the publication's steps."));
		}
		const auto running = aHead.State == core::tour_head_state::publishing || aHead.State == core::tour_head_state::stopped;
		Progress->setVisible(running && aHead.Steps > 0U);
		Progress->setRange(0, static_cast<int>(std::max<std::size_t>(aHead.Steps, 1U)));
		// The bar fills with what GitHub has confirmed: the steps before the current one.
		Progress->setValue(static_cast<int>(aHead.Step > 0U ? aHead.Step - 1U : 0U));
		restyle();
	}

	void tour_head_bar::restyle()
	{
		auto colours = State->palette();
		colours.setColor(QPalette::WindowText, Shown.State == core::tour_head_state::stopped ? tour_error_colour(palette()) : palette().color(QPalette::WindowText));
		State->setPalette(colours);
		auto bar = Progress->palette();
		bar.setColor(QPalette::Highlight, Shown.State == core::tour_head_state::stopped ? tour_error_colour(palette()) : palette().color(QPalette::Highlight));
		Progress->setPalette(bar);
	}

	void tour_head_bar::changeEvent(QEvent* const aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange || aEvent->type() == QEvent::StyleChange) {
			restyle();
		}
	}

	namespace {
		/// One checklist row's mark: a tick, a filled dot, a ring, a cross, a dash.
		///
		/// \details The owner, accepting the publish steps on 2026-11-13: "I'm
		/// not sure if those circles are meant to be misaligned vertically with
		/// the text beside them." The mark is as tall as its label's first line
		/// and draws its 16-pixel glyph centred in it, so the glyph's centre is
		/// the first line's, at any font size and when the label wraps.
		class step_mark final : public QWidget
		{
		public:
			step_mark(const application::tour_step_state aState, const int aLineHeight, QWidget* const aParent)
				: QWidget{ aParent }
				, State{ aState }
			{
				setObjectName(QStringLiteral("tourPublicationStepMark"));
				setFixedSize(16, std::max(16, aLineHeight));
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				using state  = application::tour_step_state;
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing);
				const auto box   = QRectF{ 2.0, 2.0 + ((height() - 16) / 2.0), 12.0, 12.0 };
				const auto faint = palette().color(QPalette::Active, QPalette::PlaceholderText);
				switch (State) {
				case state::sent: {
					painter.setPen(QPen{ sent_green(palette()), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
					auto tick = QPainterPath{ };
					tick.moveTo(box.left() + 1.5, box.center().y() + 0.5);
					tick.lineTo(box.left() + 4.5, box.bottom() - 2.0);
					tick.lineTo(box.right() - 1.0, box.top() + 2.0);
					painter.drawPath(tick);
					break;
				}
				case state::sending:
					painter.setPen(Qt::NoPen);
					painter.setBrush(sending_amber(palette()));
					painter.drawEllipse(box.adjusted(2.0, 2.0, -2.0, -2.0));
					painter.setBrush(Qt::NoBrush);
					painter.setPen(QPen{ sending_amber(palette()), 1.2 });
					painter.drawEllipse(box);
					break;
				case state::waiting:
					painter.setPen(QPen{ faint, 1.2 });
					painter.drawEllipse(box.adjusted(1.5, 1.5, -1.5, -1.5));
					break;
				case state::failed: {
					const auto red = tour_error_colour(palette());
					painter.setPen(Qt::NoPen);
					painter.setBrush(red);
					painter.drawEllipse(box.adjusted(-1.0, -1.0, 1.0, 1.0));
					painter.setPen(QPen{ night(palette()) ? palette().color(QPalette::Window) : QColor{ Qt::white }, 1.6, Qt::SolidLine, Qt::RoundCap });
					painter.drawLine(QPointF{ box.left() + 3.5, box.top() + 3.5 }, QPointF{ box.right() - 3.5, box.bottom() - 3.5 });
					painter.drawLine(QPointF{ box.right() - 3.5, box.top() + 3.5 }, QPointF{ box.left() + 3.5, box.bottom() - 3.5 });
					break;
				}
				case state::not_sent:
					painter.setPen(QPen{ faint, 1.4, Qt::SolidLine, Qt::RoundCap });
					painter.drawLine(QPointF{ box.left() + 3.0, box.center().y() }, QPointF{ box.right() - 3.0, box.center().y() });
					break;
				}
			}

		private:
			application::tour_step_state State;
		};

		[[nodiscard]] auto state_words(const application::tour_step_state aState) -> QString
		{
			using state = application::tour_step_state;
			switch (aState) {
			case state::sent:     return QObject::tr("Sent");
			case state::sending:  return QObject::tr("Sending");
			case state::waiting:  return QObject::tr("Waiting");
			case state::failed:   return QObject::tr("Failed");
			case state::not_sent: return QObject::tr("Not sent");
			}
			return { };
		}
	}

	tour_step_list::tour_step_list(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("tourPublicationSteps"));
		setAccessibleName(tr("Publication steps"));
		Column = new QVBoxLayout{ this };
		Column->setContentsMargins(0, 0, 0, 0);
		Column->setSpacing(3);
		// Room the page gives beyond the rows' own height stays under them:
		// a row given it grew and its words sank below its mark.
		Column->addStretch(1);
	}

	void tour_step_list::present(const std::vector<application::tour_publication_step_view>& aSteps)
	{
		if (aSteps == Shown) {
			return;
		}
		Shown = aSteps;
		for (auto* const row : Rows) {
			row->hide();
			row->deleteLater();
		}
		Rows.clear();
		for (const auto& step : Shown) {
			auto* const row = new QWidget{ this };
			row->setObjectName(QStringLiteral("tourPublicationStep"));
			auto* const line = new QHBoxLayout{ row };
			line->setContentsMargins(0, 0, 0, 0);
			line->setSpacing(7);
			auto* const words = new QVBoxLayout{ };
			words->setContentsMargins(0, 0, 0, 0);
			words->setSpacing(1);
			auto* const label = new QLabel{ text(step.Label), row };
			label->setObjectName(QStringLiteral("tourPublicationStepLabel"));
			label->setTextFormat(Qt::PlainText);
			label->setWordWrap(true);
			// Its first line at its top, where the mark beside it is centred;
			// a wrapped label grows downward from there.
			label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
			// Its height follows its width: a policy set after word wrap drops
			// that, and a step that kept its plain font then took the label's
			// two-line size hint for one line of words.
			auto policy = QSizePolicy{ QSizePolicy::Ignored, QSizePolicy::Preferred };
			policy.setHeightForWidth(true);
			label->setSizePolicy(policy);
			const auto quiet = step.State == application::tour_step_state::waiting || step.State == application::tour_step_state::not_sent;
			label->setForegroundRole(quiet ? QPalette::PlaceholderText : QPalette::WindowText);
			if (step.State == application::tour_step_state::sending || step.State == application::tour_step_state::failed) {
				auto font = label->font();
				font.setWeight(QFont::DemiBold);
				label->setFont(font);
			}
			line->addWidget(new step_mark{ step.State, label->fontMetrics().height(), row }, 0, Qt::AlignTop);
			words->addWidget(label);
			if (!step.Reason.empty()) {
				auto* const reason = new QLabel{ text(step.Reason), row };
				reason->setObjectName(QStringLiteral("tourPublicationStepReason"));
				reason->setTextFormat(Qt::PlainText);
				reason->setWordWrap(true);
				auto colours = reason->palette();
				colours.setColor(QPalette::WindowText, tour_error_colour(palette()));
				reason->setPalette(colours);
				words->addWidget(reason);
			}
			line->addLayout(words, 1);
			row->setAccessibleName(state_words(step.State) + QStringLiteral(": ") + text(step.Label) + (step.Reason.empty() ? QString{ } : QStringLiteral(". ") + text(step.Reason)));
			Column->insertWidget(Column->count() - 1, row);
			Rows.push_back(row);
		}
	}

	auto tour_step_list::row_texts() const -> QStringList
	{
		auto result = QStringList{ };
		for (const auto* const row : Rows) {
			result.push_back(row->accessibleName());
		}
		return result;
	}

	namespace {
		[[nodiscard]] auto remedy_text(const application::tour_publication_remedy aRemedy, const bool aSingle) -> QString
		{
			using remedy = application::tour_publication_remedy;
			switch (aRemedy) {
			case remedy::resume:           return QObject::tr("Resume");
			case remedy::check_status:     return QObject::tr("Check status");
			case remedy::preview_again:    return QObject::tr("Preview again");
			case remedy::move_stops:       return aSingle ? QObject::tr("Move the stop") : QObject::tr("Move those stops");
			case remedy::other_connection: return QObject::tr("Use another connection");
			}
			return { };
		}
		[[nodiscard]] auto remedy_name(const application::tour_publication_remedy aRemedy) -> QString
		{
			using remedy = application::tour_publication_remedy;
			switch (aRemedy) {
			case remedy::resume:           return QStringLiteral("tourBannerResume");
			case remedy::check_status:     return QStringLiteral("tourBannerCheckStatus");
			case remedy::preview_again:    return QStringLiteral("tourBannerPreviewAgain");
			case remedy::move_stops:       return QStringLiteral("tourBannerMoveStops");
			case remedy::other_connection: return QStringLiteral("tourBannerOtherConnection");
			}
			return { };
		}
	}

	tour_publication_banner_view::tour_publication_banner_view(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("tourPublicationBanner"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(16, 10, 10, 11);
		column->setSpacing(5);
		const auto words = [this, column](const QString& aName) {
			auto* const label = new QLabel{ this };
			label->setObjectName(aName);
			label->setTextFormat(Qt::PlainText);
			label->setWordWrap(true);
			label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
			label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
			column->addWidget(label);
			return label;
		};
		Title     = words(QStringLiteral("tourPublicationBannerTitle"));
		auto font = Title->font();
		font.setWeight(QFont::DemiBold);
		Title->setFont(font);
		Why             = words(QStringLiteral("tourPublicationBannerWhy"));
		OnGitHub        = words(QStringLiteral("tourPublicationBannerOnGitHub"));
		Actions         = new QWidget{ this };
		auto* const row = new QHBoxLayout{ Actions };
		row->setContentsMargins(0, 3, 0, 0);
		row->setSpacing(6);
		row->addStretch(1);
		column->addWidget(Actions);
		hide();
	}

	void tour_publication_banner_view::set_dispatch(remedy_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	auto tour_publication_banner_view::buttons() const -> std::vector<QPushButton*>
	{
		auto result = std::vector<QPushButton*>{ };
		for (auto* const child : Actions->findChildren<QPushButton*>(Qt::FindDirectChildrenOnly)) {
			if (!child->isHidden()) {
				result.push_back(child);
			}
		}
		return result;
	}

	void tour_publication_banner_view::present(const std::optional<application::tour_publication_banner>& aBanner)
	{
		if (aBanner == Shown) {
			return;
		}
		Shown = aBanner;
		setVisible(Shown.has_value());
		if (!Shown) {
			return;
		}
		Title->setText(text(Shown->Title));
		Why->setText(text(Shown->Why));
		OnGitHub->setText(text(Shown->OnGitHub));
		Why->setVisible(!Shown->Why.empty());
		OnGitHub->setVisible(!Shown->OnGitHub.empty());
		auto* const row = static_cast<QHBoxLayout*>(Actions->layout());
		for (auto* const old : Actions->findChildren<QPushButton*>(Qt::FindDirectChildrenOnly)) {
			old->hide();
			old->deleteLater();
		}
		const auto single = Shown->Why.starts_with("Stop ");
		auto       first  = true;
		for (const auto remedy : Shown->Actions) {
			auto* const control = first ? new QPushButton{ remedy_text(remedy, single), Actions } : new quiet_push_button{ remedy_text(remedy, single), Actions };
			control->setObjectName(remedy_name(remedy));
			connect(control, &QPushButton::clicked, this, [this, remedy] {
				if (Dispatch && Shown) {
					Dispatch(remedy, Shown->MoveTo);
				}
			});
			row->insertWidget(row->count() - 1, control);
			first = false;
		}
		const auto said = Title->text() + QStringLiteral(" ") + Why->text() + QStringLiteral(" ") + OnGitHub->text();
		setAccessibleName(Title->text());
		setAccessibleDescription(said.trimmed());
		update();
	}

	void tour_publication_banner_view::paintEvent(QPaintEvent* /*aEvent*/)
	{
		// The tour's failure card: the window washed with the error red, its
		// edge solid, so the banner cannot be mistaken for a status line.
		auto painter = QPainter{ this };
		painter.setRenderHint(QPainter::Antialiasing, true);
		const auto ground = palette().color(QPalette::Active, QPalette::Window);
		const auto red    = tour_error_colour(palette());
		const auto box    = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
		painter.setPen(QPen{ mixed(ground, red, 0.55), 1.0 });
		painter.setBrush(mixed(ground, red, night(palette()) ? 0.16 : 0.10));
		painter.drawRoundedRect(box, 8.0, 8.0);
		painter.setPen(Qt::NoPen);
		painter.setBrush(red);
		painter.drawRoundedRect(QRectF{ box.left(), box.top(), 4.0, box.height() }, 2.0, 2.0);
	}
} // namespace bpr::qt
