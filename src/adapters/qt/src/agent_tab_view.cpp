#include "bpr/qt/agent_tab_view.hpp"

#include "bpr/qt/quiet_push_button.hpp"

#include "bpr/core/agent_finding.hpp"
#include "bpr/core/defect_workflow.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/finding_queue_view.hpp"
#include "bpr/qt/side_chat_panel.hpp"

#include <QAbstractItemModel>
#include <QAction>
#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto mixed(const QColor& aFrom, const QColor& aTo, const qreal aShare) -> QColor
		{
			const auto keep = 1.0 - aShare;
			return QColor::fromRgbF(static_cast<float>(aFrom.redF() * keep + aTo.redF() * aShare), static_cast<float>(aFrom.greenF() * keep + aTo.greenF() * aShare),
									static_cast<float>(aFrom.blueF() * keep + aTo.blueF() * aShare));
		}

		[[nodiscard]] auto dark(const QPalette& aPalette) -> bool
		{
			return aPalette.color(QPalette::Window).lightness() < 128;
		}

		/// \brief A frame that draws a rounded card in the palette: the
		/// banner's callout, a finding's card, the question's field.
		class rounded final : public QWidget
		{
		public:
			enum class look : std::uint8_t {
				card,
				callout,
				danger,
				field,
			};
			rounded(const look aLook, QWidget* aParent)
				: QWidget{ aParent }
				, Look{ aLook }
			{
			}
			look Look;
			/// The severity's edge, on a finding's card.
			QColor Edge;
			bool   Selected = false;

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto ink    = palette().color(QPalette::Active, QPalette::WindowText);
				const auto base   = palette().color(QPalette::Active, QPalette::Base);
				const auto window = palette().color(QPalette::Active, QPalette::Window);
				const auto box    = QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5);
				auto       fill   = base;
				auto       line   = mixed(base, ink, 0.16);
				if (Look == look::callout) {
					fill = mixed(window, palette().color(QPalette::Active, QPalette::Highlight), 0.10);
					line = mixed(window, palette().color(QPalette::Active, QPalette::Highlight), 0.30);
				} else if (Look == look::danger) {
					const auto red = dark(palette()) ? QColor{ 0xF8, 0x51, 0x49 } : QColor{ 0xCF, 0x22, 0x2E };
					fill           = mixed(window, red, 0.10);
					line           = mixed(window, red, 0.35);
				}
				if (Selected) {
					line = dark(palette()) ? QColor{ 0xE3, 0xB3, 0x41 } : QColor{ 0xC3, 0x8A, 0x10 };
				}
				painter.setPen(QPen{ line, 1.0 });
				painter.setBrush(fill);
				painter.drawRoundedRect(box, 8.0, 8.0);
				if (Edge.isValid()) {
					painter.setPen(Qt::NoPen);
					painter.setBrush(Edge);
					painter.drawRoundedRect(QRectF{ box.left(), box.top() + 6.0, 3.0, box.height() - 12.0 }, 1.5, 1.5);
				}
			}

		private:
		};

		/// \brief A tool button whose menu opens on a press and draws no
		/// arrow: the word or the dots are the affordance in a narrow drawer.
		class menu_word final : public QToolButton
		{
		public:
			explicit menu_word(QWidget* aParent)
				: QToolButton{ aParent }
			{
				setAutoRaise(true);
				setPopupMode(QToolButton::InstantPopup);
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QStylePainter{ this };
				auto option  = QStyleOptionToolButton{ };
				initStyleOption(&option);
				option.features &= ~QStyleOptionToolButton::HasMenu;
				painter.drawComplexControl(QStyle::CC_ToolButton, option);
			}
		};

		/// \brief A quiet chip: the agent's head, the scope, a context.
		[[nodiscard]] auto chip_label(QWidget* aParent, const QString& aName) -> QLabel*
		{
			auto* const chip = new QLabel{ aParent };
			chip->setObjectName(aName);
			chip->setTextFormat(Qt::PlainText);
			chip->setMargin(3);
			chip->setForegroundRole(QPalette::WindowText);
			return chip;
		}

		[[nodiscard]] auto severity_colour(const core::defect_severity aSeverity, const QPalette& aPalette) -> QColor
		{
			const auto night = dark(aPalette);
			switch (aSeverity) {
			case core::defect_severity::critical:
			case core::defect_severity::major:    return night ? QColor{ 0xF8, 0x51, 0x49 } : QColor{ 0xCF, 0x22, 0x2E };
			case core::defect_severity::minor:    return night ? QColor{ 0xE3, 0xB3, 0x41 } : QColor{ 0xC3, 0x8A, 0x10 };
			case core::defect_severity::trivial:  return aPalette.color(QPalette::Active, QPalette::PlaceholderText);
			}
			return aPalette.color(QPalette::Active, QPalette::PlaceholderText);
		}

		[[nodiscard]] auto confidence_words(const core::agent_finding_confidence aConfidence) -> QString
		{
			switch (aConfidence) {
			case core::agent_finding_confidence::high:     return QObject::tr("likely");
			case core::agent_finding_confidence::medium:   return QObject::tr("possible");
			case core::agent_finding_confidence::low:      return QObject::tr("unsure");
			case core::agent_finding_confidence::unstated: break;
			}
			return QObject::tr("unrated");
		}

		[[nodiscard]] auto capitalised(std::string_view aName) -> QString
		{
			auto words = QString::fromUtf8(aName.data(), static_cast<qsizetype>(aName.size()));
			words.replace(QLatin1Char{ '_' }, QLatin1Char{ ' ' });
			if (!words.isEmpty()) {
				words[0] = words[0].toUpper();
			}
			return words;
		}
	} // namespace

	agent_tab_view::agent_tab_view(agent_launch_panel& aLaunch, side_chat_panel& aChat, finding_queue_view& aFindings, QWidget& aHistory, QWidget* aParent)
		: QWidget{ aParent }
		, Launch{ aLaunch }
		, Chat{ aChat }
		, Findings{ aFindings }
		, HistoryPanel{ aHistory }
		, LaunchAlive{ static_cast<QWidget*>(&aLaunch) }
		, ChatAlive{ static_cast<QWidget*>(&aChat) }
		, FindingsAlive{ static_cast<QWidget*>(&aFindings) }
	{
		setObjectName(QStringLiteral("agentTab"));
		setAccessibleName(tr("Agent"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(0);

		// The context row: the agent, the head it reads, its scope, History.
		auto* const context = new QWidget{ this };
		context->setObjectName(QStringLiteral("agentContextRow"));
		auto* const row = new QHBoxLayout{ context };
		row->setContentsMargins(10, 8, 8, 8);
		row->setSpacing(5);
		auto* const profiles = Launch.profile_selector();
		Borrowed.emplace_back(profiles, profiles->parentWidget());
		profiles->setParent(context);
		profiles->setToolTip(tr("The agent this tab asks"));
		profiles->setSizeAdjustPolicy(QComboBox::AdjustToContents);
		row->addWidget(profiles);
		Head = chip_label(context, QStringLiteral("agentHeadChip"));
		Head->setToolTip(tr("The head the session reads"));
		row->addWidget(Head);
		auto* const scope = Chat.scope_selector();
		Borrowed.emplace_back(scope, scope->parentWidget());
		scope->setParent(context);
		scope->setToolTip(tr("What the agent may read"));
		scope->setSizeAdjustPolicy(QComboBox::AdjustToContents);
		row->addWidget(scope);
		row->addStretch(1);
		History = new menu_word{ context };
		History->setObjectName(QStringLiteral("agentHistory"));
		History->setText(tr("History"));
		History->setToolTip(tr("Sessions, sessions on earlier heads and private runs"));
		History->setAccessibleName(tr("History"));
		History->setAutoRaise(true);
		History->setPopupMode(QToolButton::InstantPopup);
		auto* const historyMenu = new QMenu{ History };
		historyMenu->setObjectName(QStringLiteral("agentHistoryMenu"));
		historyMenu->addAction(Chat.new_session_action());
		historyMenu->addAction(Chat.continue_old_revision_action());
		historyMenu->addSeparator();
		auto* const privateRuns = historyMenu->addAction(tr("Private history and runs"));
		privateRuns->setObjectName(QStringLiteral("agentPrivateHistoryAction"));
		connect(privateRuns, &QAction::triggered, this, [this] { show_history(true); });
		auto* const sessions = historyMenu->addAction(tr("Sessions"));
		sessions->setObjectName(QStringLiteral("agentSessionsAction"));
		sessions->setEnabled(false);
		connect(historyMenu, &QMenu::aboutToShow, this, [this, sessions] { sessions->setText(Launch.session_catalog_text()); });
		History->setMenu(historyMenu);
		row->addWidget(History);
		column->addWidget(context);

		Pages = new QStackedWidget{ this };
		column->addWidget(Pages, 1);

		// Not set up: one sentence and one action.
		auto* const setup = new QAction{ tr("Set up an agent"), this };
		setup->setObjectName(QStringLiteral("agentSetUpAction"));
		connect(setup, &QAction::triggered, this, [this] {
			if (Setup) {
				Setup();
			}
		});
		Empty = new drawer_empty_state{ drawer_icon(drawer_glyph::agent, palette()), tr("Ask an agent about this pull request. Nothing it says is published unless you make it a comment."), setup, Pages };
		Empty->setObjectName(QStringLiteral("agentEmpty"));
		Pages->addWidget(Empty);

		// The conversation: the state's callout, the replies, the findings.
		Conversation     = new QWidget{ Pages };
		auto* const talk = new QVBoxLayout{ Conversation };
		talk->setContentsMargins(0, 0, 0, 0);
		talk->setSpacing(0);
		auto* const scroll = new QScrollArea{ Conversation };
		scroll->setObjectName(QStringLiteral("agentConversationScroll"));
		scroll->setWidgetResizable(true);
		scroll->setFrameShape(QFrame::NoFrame);
		scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		// The cards fit the drawer's width, not the other way round.
		Viewport = scroll->viewport();
		Viewport->installEventFilter(this);
		auto* const content    = new QWidget{ scroll };
		auto* const contentCol = new QVBoxLayout{ content };
		contentCol->setContentsMargins(12, 8, 12, 12);
		contentCol->setSpacing(10);
		Banner = new rounded{ rounded::look::callout, content };
		Banner->setObjectName(QStringLiteral("agentStateBanner"));
		auto* const bannerCol = new QVBoxLayout{ Banner };
		bannerCol->setContentsMargins(12, 10, 12, 10);
		bannerCol->setSpacing(4);
		BannerTitle = new QLabel{ Banner };
		BannerTitle->setObjectName(QStringLiteral("agentStateTitle"));
		auto bold = BannerTitle->font();
		bold.setWeight(QFont::DemiBold);
		BannerTitle->setFont(bold);
		BannerTitle->setWordWrap(true);
		bannerCol->addWidget(BannerTitle);
		BannerText = new QLabel{ Banner };
		BannerText->setObjectName(QStringLiteral("agentStateText"));
		BannerText->setWordWrap(true);
		BannerText->setForegroundRole(QPalette::PlaceholderText);
		bannerCol->addWidget(BannerText);
		auto* const bannerRow = new QHBoxLayout{ };
		bannerRow->setSpacing(6);
		BannerFirst = new QPushButton{ Banner };
		BannerFirst->setObjectName(QStringLiteral("agentStateFirst"));
		BannerSecond = new quiet_push_button{ Banner };
		BannerSecond->setObjectName(QStringLiteral("agentStateSecond"));
		BannerSecond->setFlat(true);
		bannerRow->addWidget(BannerFirst);
		bannerRow->addWidget(BannerSecond);
		bannerRow->addStretch(1);
		bannerCol->addLayout(bannerRow);
		Log = new QLabel{ Banner };
		Log->setObjectName(QStringLiteral("agentStateLog"));
		Log->setWordWrap(true);
		Log->setTextInteractionFlags(Qt::TextSelectableByMouse);
		Log->hide();
		bannerCol->addWidget(Log);
		contentCol->addWidget(Banner);
		// Start: run a review that finds findings, when none are shown.
		Start                = new QWidget{ content };
		auto* const startRow = new QHBoxLayout{ Start };
		startRow->setContentsMargins(0, 0, 0, 0);
		auto* const startWords = new QLabel{ tr("Ask a question below, or let the agent review the pull request for findings."), Start };
		startWords->setWordWrap(true);
		startWords->setForegroundRole(QPalette::PlaceholderText);
		startRow->addWidget(startWords, 1);
		auto* const review = new QToolButton{ Start };
		review->setObjectName(QStringLiteral("agentReviewButton"));
		review->setDefaultAction(Launch.start_action());
		review->setToolButtonStyle(Qt::ToolButtonTextOnly);
		startRow->addWidget(review, 0, Qt::AlignTop);
		contentCol->addWidget(Start);
		auto* const replies = Chat.replies_list();
		Borrowed.emplace_back(replies, replies->parentWidget());
		replies->setParent(content);
		replies->setFrameShape(QFrame::NoFrame);
		replies->setWordWrap(true);
		replies->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
		replies->viewport()->setAutoFillBackground(false);
		contentCol->addWidget(replies);
		Cards      = new QWidget{ content };
		CardColumn = new QVBoxLayout{ Cards };
		CardColumn->setContentsMargins(0, 0, 0, 0);
		CardColumn->setSpacing(8);
		contentCol->addWidget(Cards);
		contentCol->addStretch(1);
		scroll->setWidget(content);
		talk->addWidget(scroll, 1);

		// The question field: the words, its context chips, Context and Ask.
		auto* const ask = new QWidget{ Conversation };
		ask->setObjectName(QStringLiteral("agentQuestion"));
		auto* const askCol = new QVBoxLayout{ ask };
		askCol->setContentsMargins(10, 8, 10, 10);
		auto* const field    = new rounded{ rounded::look::field, ask };
		auto* const fieldCol = new QVBoxLayout{ field };
		fieldCol->setContentsMargins(8, 6, 8, 6);
		fieldCol->setSpacing(4);
		auto* const composer = Chat.composer();
		Borrowed.emplace_back(composer, composer->parentWidget());
		composer->setParent(field);
		composer->setFrameShape(QFrame::NoFrame);
		composer->setPlaceholderText(tr("Ask about this pull request…"));
		composer->setMaximumHeight(90);
		composer->viewport()->setAutoFillBackground(false);
		fieldCol->addWidget(composer);
		auto* const askRow = new QHBoxLayout{ };
		askRow->setSpacing(6);
		Chips = chip_label(field, QStringLiteral("agentContextChips"));
		Chips->setForegroundRole(QPalette::PlaceholderText);
		askRow->addWidget(Chips, 1);
		Context = new quiet_push_button{ tr("Context"), field };
		Context->setObjectName(QStringLiteral("agentAddContext"));
		Context->setFlat(true);
		Context->setToolTip(Chat.add_context_action()->toolTip().isEmpty() ? tr("Attach the lines selected in the code") : Chat.add_context_action()->toolTip());
		connect(Context, &QPushButton::clicked, Chat.add_context_action(), &QAction::trigger);
		askRow->addWidget(Context);
		Ask = new QPushButton{ tr("Ask"), field };
		Ask->setObjectName(QStringLiteral("agentAsk"));
		Ask->setDefault(true);
		Ask->setToolTip(tr("Send the question to the agent (%1)").arg(QKeySequence{ Qt::CTRL | Qt::Key_Return }.toString(QKeySequence::NativeText)));
		connect(Ask, &QPushButton::clicked, Chat.send_action(), &QAction::trigger);
		askRow->addWidget(Ask);
		fieldCol->addLayout(askRow);
		askCol->addWidget(field);
		talk->addWidget(ask);
		Pages->addWidget(Conversation);

		// History: the private history, with the way back.
		HistoryPage         = new QWidget{ Pages };
		auto* const histCol = new QVBoxLayout{ HistoryPage };
		histCol->setContentsMargins(0, 0, 0, 0);
		auto* const back = new quiet_push_button{ tr("Back to the conversation"), HistoryPage };
		back->setObjectName(QStringLiteral("agentHistoryBack"));
		back->setFlat(true);
		connect(back, &QPushButton::clicked, this, [this] { show_history(false); });
		histCol->addWidget(back, 0, Qt::AlignLeft);
		Borrowed.emplace_back(&HistoryPanel, HistoryPanel.parentWidget());
		HistoryPanel.setParent(HistoryPage);
		histCol->addWidget(&HistoryPanel, 1);
		HistoryPanel.show();
		Pages->addWidget(HistoryPage);

		// What the panels say moves this tab: their lists and actions.
		const auto again = [this] { refresh(); };
		connect(Findings.findings_list()->model(), &QAbstractItemModel::modelReset, this, again);
		connect(Findings.findings_list()->model(), &QAbstractItemModel::rowsInserted, this, again);
		connect(Findings.findings_list()->model(), &QAbstractItemModel::rowsRemoved, this, again);
		connect(Chat.replies_list()->model(), &QAbstractItemModel::rowsInserted, this, again);
		connect(Chat.replies_list()->model(), &QAbstractItemModel::modelReset, this, again);
		for (auto* const action : { Launch.start_action(), Launch.cancel_action(), Chat.continue_old_revision_action(), Chat.new_session_action(), Chat.send_action() }) {
			connect(action, &QAction::changed, this, again);
		}
		connect(profiles, &QComboBox::currentIndexChanged, this, again);
		refresh();
	}

	agent_tab_view::~agent_tab_view()
	{
		Dying = true;
		// The panels hold these controls by pointer and outlive this tab; the
		// tab never deletes what it only shows.
		for (auto& [widget, owner] : Borrowed) {
			if (widget != nullptr && owner != nullptr) {
				widget->hide();
				widget->setParent(owner);
			}
		}
	}

	void agent_tab_view::set_setup_dispatch(std::function<void()> aDispatch) { Setup = std::move(aDispatch); }

	void agent_tab_view::focus_question()
	{
		if (!panels_alive()) {
			return;
		}
		show_history(false);
		if (Chat.composer()->isVisible()) {
			Chat.composer()->setFocus(Qt::OtherFocusReason);
		}
	}

	void agent_tab_view::show_history(const bool aShown)
	{
		if (aShown) {
			Pages->setCurrentWidget(HistoryPage);
			return;
		}
		Pages->setCurrentWidget(State == agent_tab_state::not_set_up ? static_cast<QWidget*>(Empty) : Conversation);
	}

	void agent_tab_view::refresh()
	{
		if (!panels_alive()) {
			return;
		}
		const auto setUp   = Launch.profile_selector()->count() > 0;
		const auto running = Launch.cancel_action()->isEnabled();
		const auto status  = Launch.status_text();
		const auto failed  = status.startsWith(QStringLiteral("Finished:")) && !status.contains(QStringLiteral("success"));
		const auto moved   = !Chat.offered_continuations().empty();
		State              = !setUp ? agent_tab_state::not_set_up
			: running               ? agent_tab_state::working
			: moved                 ? agent_tab_state::head_moved
			: failed                ? agent_tab_state::stopped
									: agent_tab_state::idle;
		if (Pages->currentWidget() != HistoryPage) {
			Pages->setCurrentWidget(State == agent_tab_state::not_set_up ? static_cast<QWidget*>(Empty) : Conversation);
		}
		// The head the session reads, short; its state in the tooltip.
		auto head = Chat.head_oid_text().trimmed();
		if (head.contains(QLatin1Char{ ':' })) {
			head = head.section(QLatin1Char{ ':' }, -1).trimmed();
		}
		Head->setText(head.left(7));
		Head->setVisible(!head.isEmpty() && head != QStringLiteral("—"));
		Head->setToolTip(tr("The head the session reads. %1").arg(Chat.session_state_text()));
		Chips->setText(Chat.chips_text());
		Ask->setEnabled(Chat.send_action()->isEnabled());
		present_banner();
		rebuild_findings();
		Start->setVisible(State == agent_tab_state::idle && Findings.finding_count() == 0U && Chat.replies_list()->count() == 0);
		// The replies take the height of their rows, not a list's default.
		auto* const replies = Chat.replies_list();
		replies->setVisible(replies->count() > 0);
		auto rows = 0;
		for (auto row = 0; row < replies->count(); ++row) {
			rows += replies->sizeHintForRow(row);
		}
		replies->setFixedHeight(rows + (2 * replies->frameWidth()));
	}

	void agent_tab_view::present_banner()
	{
		if (!panels_alive()) {
			return;
		}
		auto* const callout = static_cast<rounded*>(Banner);
		callout->Look       = State == agent_tab_state::stopped ? rounded::look::danger : rounded::look::callout;
		BannerFirst->disconnect();
		BannerSecond->disconnect();
		BannerSecond->show();
		Log->setVisible(LogShown && State == agent_tab_state::stopped);
		switch (State) {
		case agent_tab_state::not_set_up:
		case agent_tab_state::idle:
			Banner->hide();
			return;
		case agent_tab_state::working:
			BannerTitle->setText(tr("%1 is working").arg(Launch.profile_selector()->currentText()));
			BannerText->setText(Launch.status_text());
			BannerFirst->setText(tr("Stop"));
			connect(BannerFirst, &QPushButton::clicked, Launch.cancel_action(), &QAction::trigger);
			BannerSecond->hide();
			break;
		case agent_tab_state::head_moved:
			BannerTitle->setText(tr("This session read %1").arg(Head->text()));
			BannerText->setText(tr("The pull request has moved on. Its findings stay anchored to the head it read."));
			BannerFirst->setText(Chat.new_session_action()->text().isEmpty() ? tr("Start a new session") : Chat.new_session_action()->text());
			connect(BannerFirst, &QPushButton::clicked, Chat.new_session_action(), &QAction::trigger);
			BannerSecond->setText(Chat.continue_old_revision_action()->text().isEmpty() ? tr("Continue on the old head") : Chat.continue_old_revision_action()->text());
			connect(BannerSecond, &QPushButton::clicked, Chat.continue_old_revision_action(), &QAction::trigger);
			break;
		case agent_tab_state::stopped:
			BannerTitle->setText(tr("%1 stopped before answering").arg(Launch.profile_selector()->currentText()));
			// [[BPR-1738]]. A stop with a reason says it and what to do;
			// the log keeps the raw text.
			BannerText->setText(Launch.stop_reason_text().isEmpty() ? tr("%1. Your question is kept.").arg(Launch.status_text())
																	: tr("%1 Your question is kept.").arg(Launch.stop_reason_text()));
			BannerFirst->setText(tr("Ask again"));
			connect(BannerFirst, &QPushButton::clicked, Launch.start_action(), &QAction::trigger);
			BannerSecond->setText(LogShown ? tr("Hide the log") : tr("Show the log"));
			connect(BannerSecond, &QPushButton::clicked, this, [this] {
				LogShown = !LogShown;
				Log->setText(Launch.output_text().isEmpty() ? tr("The agent wrote nothing.") : Launch.output_text());
				present_banner();
			});
			break;
		}
		Banner->show();
		Banner->update();
	}

	auto agent_tab_view::finding_cards() const -> std::vector<QWidget*>
	{
		auto result = std::vector<QWidget*>{ };
		for (const auto& card : CardWidgets) {
			if (card != nullptr) {
				result.push_back(card);
			}
		}
		return result;
	}

	void agent_tab_view::rebuild_findings()
	{
		if (!panels_alive()) {
			return;
		}
		for (auto& card : CardWidgets) {
			if (card != nullptr) {
				// Gone from view now; deleted once any press on it returns.
				card->hide();
				CardColumn->removeWidget(card);
				card->deleteLater();
			}
		}
		CardWidgets.clear();
		const auto* const delivery = Findings.shown_delivery();
		if (delivery == nullptr) {
			Cards->hide();
			return;
		}
		const auto selected = Findings.selected_ordinal();
		for (std::size_t ordinal = 0U; ordinal < delivery->Findings.size(); ++ordinal) {
			const auto& finding = delivery->Findings[ordinal].Finding;
			auto* const card    = new rounded{ rounded::look::card, Cards };
			card->setObjectName(QStringLiteral("agentFindingCard"));
			card->Edge      = severity_colour(finding.Severity, palette());
			card->Selected  = selected == std::optional{ ordinal };
			auto* const col = new QVBoxLayout{ card };
			col->setContentsMargins(14, 10, 12, 10);
			col->setSpacing(5);
			auto* const meta = new QHBoxLayout{ };
			meta->setSpacing(6);
			auto* const severity = new QLabel{ capitalised(core::name(finding.Severity)), card };
			auto        sevFont  = severity->font();
			sevFont.setWeight(QFont::DemiBold);
			severity->setFont(sevFont);
			auto sevPalette = severity->palette();
			sevPalette.setColor(QPalette::WindowText, severity_colour(finding.Severity, palette()));
			severity->setPalette(sevPalette);
			meta->addWidget(severity);
			auto* const kind = new QLabel{ QStringLiteral("%1 · %2").arg(confidence_words(finding.Confidence), QString::fromUtf8(core::name(finding.Category).data(), static_cast<qsizetype>(core::name(finding.Category).size()))), card };
			kind->setForegroundRole(QPalette::PlaceholderText);
			// The kind gives way to the anchor when the drawer is narrow.
			kind->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
			kind->setToolTip(kind->text());
			meta->addWidget(kind, 1);
			auto* const where = new QLabel{ QStringLiteral("%1:%2–%3").arg(QString::fromStdString(finding.Path).section(QLatin1Char{ '/' }, -1)).arg(finding.Lines.Begin).arg(finding.Lines.End), card };
			where->setObjectName(QStringLiteral("agentFindingAnchor"));
			auto code = QFont{ QStringLiteral("Menlo") };
			code.setStyleHint(QFont::Monospace);
			code.setPointSizeF(std::max(1.0, font().pointSizeF() * 0.86));
			code.setWeight(QFont::DemiBold);
			where->setFont(code);
			where->setToolTip(QString::fromStdString(finding.Path));
			meta->addWidget(where);
			col->addLayout(meta);
			auto* const claim = new QLabel{ QString::fromStdString(finding.Claim), card };
			claim->setWordWrap(true);
			claim->setFont(sevFont);
			claim->setTextFormat(Qt::PlainText);
			col->addWidget(claim);
			if (!finding.Evidence.empty()) {
				auto* const evidence = new QLabel{ QString::fromStdString(finding.Evidence), card };
				evidence->setWordWrap(true);
				evidence->setTextFormat(Qt::PlainText);
				evidence->setForegroundRole(QPalette::PlaceholderText);
				col->addWidget(evidence);
			}
			auto* const actions = new QHBoxLayout{ };
			actions->setSpacing(6);
			const auto act = [this, ordinal](const finding_queue_action aAction) {
				Findings.select(ordinal);
				if (auto* const action = Findings.action(aAction); action != nullptr) {
					action->trigger();
				}
			};
			auto* const draft = new quiet_push_button{ tr("Draft a comment"), card };
			draft->setObjectName(QStringLiteral("agentFindingDraftComment"));
			draft->setDefault(true);
			draft->setToolTip(tr("Copy this finding to a local draft at its line. Nothing is published."));
			connect(draft, &QPushButton::clicked, this, [act] { act(finding_queue_action::convert_to_draft_comment); });
			actions->addWidget(draft);
			auto* const follow = new quiet_push_button{ tr("Ask follow-up"), card };
			follow->setObjectName(QStringLiteral("agentFindingFollowUp"));
			connect(follow, &QPushButton::clicked, this, [act] { act(finding_queue_action::ask_follow_up); });
			actions->addWidget(follow);
			actions->addStretch(1);
			// Add to notes, then Ask follow-up, fold into More when the
			// drawer is too narrow for them; fit_card_actions decides.
			auto* const notes = new quiet_push_button{ tr("Add to notes"), card };
			notes->setObjectName(QStringLiteral("agentFindingAddToNotes"));
			notes->setFlat(true);
			connect(notes, &QPushButton::clicked, this, [act] { act(finding_queue_action::add_to_private_notes); });
			actions->addWidget(notes);
			auto* const more = new menu_word{ card };
			more->setObjectName(QStringLiteral("agentFindingMore"));
			more->setText(QStringLiteral("⋯"));
			more->setAutoRaise(true);
			more->setPopupMode(QToolButton::InstantPopup);
			more->setAccessibleName(tr("More finding actions"));
			auto* const menu       = new QMenu{ more };
			auto* const followItem = menu->addAction(tr("Ask follow-up"), this, [act] { act(finding_queue_action::ask_follow_up); });
			followItem->setObjectName(QStringLiteral("agentFindingFollowUpItem"));
			auto* const notesItem = menu->addAction(tr("Add to notes"), this, [act] { act(finding_queue_action::add_to_private_notes); });
			notesItem->setObjectName(QStringLiteral("agentFindingAddToNotesItem"));
			menu->addSeparator();
			menu->addAction(tr("Show the code"), this, [act] { act(finding_queue_action::open_evidence); });
			menu->addAction(tr("Draft a defect"), this, [act] { act(finding_queue_action::convert_to_draft_defect); });
			menu->addAction(tr("Dismiss"), this, [act] { act(finding_queue_action::dismiss); });
			more->setMenu(menu);
			actions->addWidget(more);
			col->addLayout(actions);
			// Choosing a card selects its finding and shows its lines.
			card->installEventFilter(this);
			card->setProperty("agentFindingOrdinal", QVariant::fromValue<qulonglong>(ordinal));
			card->setCursor(Qt::PointingHandCursor);
			fit_card_actions(*card, card_room());
			CardColumn->addWidget(card);
			CardWidgets.emplace_back(card);
		}
		Cards->setVisible(!delivery->Findings.empty());
	}

	auto agent_tab_view::card_room() const -> int
	{
		// The viewport, less the conversation's and the card's margins.
		constexpr auto margins = 12 + 12 + 14 + 12;
		return Viewport == nullptr ? 0 : Viewport->width() - margins;
	}

	void agent_tab_view::fit_card_actions(QWidget& aCard, const int aRoom)
	{
		auto* const draft  = aCard.findChild<QPushButton*>(QStringLiteral("agentFindingDraftComment"));
		auto* const follow = aCard.findChild<QPushButton*>(QStringLiteral("agentFindingFollowUp"));
		auto* const notes  = aCard.findChild<QPushButton*>(QStringLiteral("agentFindingAddToNotes"));
		auto* const more   = aCard.findChild<QToolButton*>(QStringLiteral("agentFindingMore"));
		if (draft == nullptr || follow == nullptr || notes == nullptr || more == nullptr || more->menu() == nullptr) {
			return;
		}
		const auto     room       = aRoom;
		constexpr auto gap        = 6;
		const auto     base       = draft->sizeHint().width() + gap + more->sizeHint().width();
		const auto     withFollow = base + gap + follow->sizeHint().width();
		const auto     withNotes  = withFollow + gap + notes->sizeHint().width();
		const auto     showFollow = withFollow <= room;
		const auto     showNotes  = withNotes <= room;
		follow->setVisible(showFollow);
		notes->setVisible(showNotes);
		for (auto* const item : more->menu()->actions()) {
			if (item->objectName() == QStringLiteral("agentFindingFollowUpItem")) {
				item->setVisible(!showFollow);
			} else if (item->objectName() == QStringLiteral("agentFindingAddToNotesItem")) {
				item->setVisible(!showNotes);
			}
		}
	}

	auto agent_tab_view::eventFilter(QObject* aWatched, QEvent* aEvent) -> bool
	{
		if (aEvent->type() == QEvent::Resize && aWatched == Viewport) {
			for (const auto& card : CardWidgets) {
				if (card != nullptr) {
					fit_card_actions(*card, card_room());
				}
			}
			return false;
		}
		if (aEvent->type() == QEvent::MouseButtonRelease && aWatched->objectName() == QStringLiteral("agentFindingCard")) {
			const auto ordinal = static_cast<std::size_t>(aWatched->property("agentFindingOrdinal").toULongLong());
			Findings.select(ordinal);
			if (auto* const action = Findings.action(finding_queue_action::open_evidence); action != nullptr) {
				action->trigger();
			}
			for (const auto& card : CardWidgets) {
				if (card != nullptr) {
					static_cast<rounded*>(card.data())->Selected = card == aWatched;
					card->update();
				}
			}
			return true;
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}

	void agent_tab_view::showEvent(QShowEvent* aEvent)
	{
		QWidget::showEvent(aEvent);
		refresh();
	}

	void agent_tab_view::changeEvent(QEvent* aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange && Empty != nullptr) {
			rebuild_findings();
		}
	}
} // namespace bpr::qt
