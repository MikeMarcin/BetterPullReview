#include "bpr/qt/agent_tab_view.hpp"

#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/quiet_tool_button.hpp"

#include "bpr/core/agent_finding.hpp"
#include "bpr/core/defect_workflow.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_setup_view.hpp"
#include "bpr/qt/disclosure_button.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/finding_queue_view.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/side_chat_panel.hpp"

#include <QAbstractItemModel>
#include <QAction>
#include <QActionGroup>
#include <QComboBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

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
				ready,
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
				} else if (Look == look::ready) {
					const auto green = dark(palette()) ? QColor{ 0x3F, 0xB9, 0x50 } : QColor{ 0x1A, 0x7F, 0x37 };
					fill             = mixed(window, green, 0.10);
					line             = mixed(window, green, 0.35);
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
		// Owner amendment 2026-10-23: the agent is switched from the chip in
		// the question field; the selector stays the one choice, unseen.
		profiles->hide();
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
		// Owner decision (b) 2026-10-23: one continuous session per pull
		// request, with an option to clear it.
		ClearSession = new quiet_tool_button{ context };
		ClearSession->setObjectName(QStringLiteral("agentClearSession"));
		ClearSession->setAutoRaise(true);
		ClearSession->setText(tr("Clear"));
		ClearSession->setAccessibleName(tr("Clear session"));
		ClearSession->setToolTip(tr("Clear session: empty the thread and start the next question afresh"));
		connect(ClearSession, &QToolButton::clicked, this, [this] { clear_session(); });
		row->addWidget(ClearSession);
		History = new menu_word{ context };
		History->setObjectName(QStringLiteral("agentHistory"));
		History->setText(tr("History"));
		History->setToolTip(tr("Sessions, sessions on earlier heads and private runs"));
		History->setAccessibleName(tr("History"));
		History->setAutoRaise(true);
		History->setPopupMode(QToolButton::InstantPopup);
		auto* const historyMenu = new QMenu{ History };
		HistoryMenu             = historyMenu;
		historyMenu->setObjectName(QStringLiteral("agentHistoryMenu"));
		auto* const clear = historyMenu->addAction(tr("Clear session"));
		clear->setObjectName(QStringLiteral("agentClearSessionAction"));
		connect(clear, &QAction::triggered, this, [this] { clear_session(); });
		connect(historyMenu, &QMenu::aboutToShow, this, [this, clear] { clear->setEnabled(!Turns.empty() && State != agent_tab_state::working); });
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
		// [[BPR-PENDING-AQ6]]. The ready line once setup finishes, and what
		// the agent gets with each question.
		ReadyCard = new rounded{ rounded::look::ready, content };
		ReadyCard->setObjectName(QStringLiteral("agentReadyCard"));
		auto* const readyCol = new QVBoxLayout{ ReadyCard };
		readyCol->setContentsMargins(12, 10, 12, 10);
		readyCol->setSpacing(4);
		auto* const readyTop = new QHBoxLayout{ };
		ReadyTitle           = new QLabel{ ReadyCard };
		ReadyTitle->setObjectName(QStringLiteral("agentReadyTitle"));
		ReadyTitle->setFont(bold);
		ReadyTitle->setWordWrap(true);
		readyTop->addWidget(ReadyTitle, 1);
		auto* const dismiss = new QToolButton{ ReadyCard };
		dismiss->setObjectName(QStringLiteral("agentReadyDismiss"));
		dismiss->setAutoRaise(true);
		dismiss->setText(QString{ QChar{ 0x2715 } });
		dismiss->setAccessibleName(tr("Dismiss"));
		connect(dismiss, &QToolButton::clicked, ReadyCard, &QWidget::hide);
		readyTop->addWidget(dismiss, 0, Qt::AlignTop);
		readyCol->addLayout(readyTop);
		// Owner look v001: the ready line alone; the check's details and what
		// the agent gets wait, folded, under one disclosure.
		auto* const gets = new disclosure_button{ ReadyCard };
		gets->setObjectName(QStringLiteral("agentReadyGetsToggle"));
		gets->setText(tr("Details and what the agent gets"));
		gets->setCheckable(true);
		gets->setChecked(false);
		readyCol->addWidget(gets, 0, Qt::AlignLeft);
		ReadyText = new QLabel{ ReadyCard };
		ReadyText->setObjectName(QStringLiteral("agentReadyText"));
		ReadyText->setWordWrap(true);
		ReadyText->setTextFormat(Qt::PlainText);
		ReadyText->hide();
		readyCol->addWidget(ReadyText);
		ReadyGets = new QLabel{ ReadyCard };
		ReadyGets->setObjectName(QStringLiteral("agentReadyGets"));
		ReadyGets->setWordWrap(true);
		ReadyGets->setTextFormat(Qt::PlainText);
		ReadyGets->setTextInteractionFlags(Qt::TextSelectableByMouse);
		ReadyGets->hide();
		readyCol->addWidget(ReadyGets);
		connect(gets, &QToolButton::toggled, this, [this](const bool aOpen) {
			ReadyText->setVisible(aOpen);
			ReadyGets->setVisible(aOpen);
		});
		ReadyCard->hide();
		contentCol->addWidget(ReadyCard);
		// The thread: each question and its answer, oldest first.
		Thread = new QWidget{ content };
		Thread->setObjectName(QStringLiteral("agentThread"));
		ThreadColumn = new QVBoxLayout{ Thread };
		ThreadColumn->setContentsMargins(0, 0, 0, 0);
		ThreadColumn->setSpacing(10);
		Thread->hide();
		contentCol->addWidget(Thread);
		auto* const replies = Chat.replies_list();
		Borrowed.emplace_back(replies, replies->parentWidget());
		replies->setParent(content);
		replies->setFrameShape(QFrame::NoFrame);
		replies->setWordWrap(true);
		replies->setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
		replies->viewport()->setAutoFillBackground(false);
		contentCol->addWidget(replies);
		replies->hide();
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
		askCol->setSpacing(6);
		// Owner decision (a) 2026-10-23: with an empty thread, suggested
		// prompts stand above the field; each is sent as written.
		Suggestions = new QWidget{ ask };
		Suggestions->setObjectName(QStringLiteral("agentSuggestions"));
		SuggestionColumn = new QVBoxLayout{ Suggestions };
		SuggestionColumn->setContentsMargins(0, 0, 0, 0);
		SuggestionColumn->setSpacing(4);
		auto* const suggestLine = new QLabel{ tr("Ask about this pull request, or try:"), Suggestions };
		suggestLine->setObjectName(QStringLiteral("agentSuggestionsLine"));
		suggestLine->setForegroundRole(QPalette::PlaceholderText);
		suggestLine->setWordWrap(true);
		SuggestionColumn->addWidget(suggestLine);
		askCol->addWidget(Suggestions);
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
		// Owner decision (c) 2026-10-23 ("A chip in the message box like VS
		// Code"): the model and effort the agent runs with, in the field, and
		// what will run in its tooltip.
		RunSettings = new menu_word{ field };
		RunSettings->setObjectName(QStringLiteral("agentRunSettings"));
		RunSettings->setAccessibleName(tr("Model and effort"));
		RunSettings->setMenu(new QMenu{ RunSettings });
		connect(RunSettings->menu(), &QMenu::aboutToShow, this, [this] { present_run_settings_button(); });
		askRow->addWidget(RunSettings);
		// Once the thread has turns the suggestions fold into this menu.
		SuggestionsMenu = new menu_word{ field };
		SuggestionsMenu->setObjectName(QStringLiteral("agentSuggestionsMenu"));
		SuggestionsMenu->setText(tr("Suggestions"));
		SuggestionsMenu->setAccessibleName(tr("Suggested prompts"));
		SuggestionsMenu->setMenu(new QMenu{ SuggestionsMenu });
		SuggestionsMenu->hide();
		askRow->addWidget(SuggestionsMenu);
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
		connect(Ask, &QPushButton::clicked, this, [this] { this->ask(); });
		// Return with Command or Control asks from the field, as Ask does, so a
		// slash command is read the same way from either.
		composer->installEventFilter(this);
		Notice = new QLabel{ field };
		Notice->setObjectName(QStringLiteral("agentComposerNotice"));
		Notice->setWordWrap(true);
		Notice->setForegroundRole(QPalette::PlaceholderText);
		Notice->hide();
		fieldCol->insertWidget(1, Notice);
		// What was asked is read when it is sent, by Ask or by the keyboard.
		connect(Chat.send_action(), &QAction::triggered, this, [this] {
			PendingQuestion = Chat.composer()->toPlainText().trimmed();
			PendingReview   = false;
		});
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
		connect(Findings.findings_list()->model(), &QAbstractItemModel::modelReset, this, [this] { ++FindingsPresented; });
		connect(Findings.findings_list()->model(), &QAbstractItemModel::rowsInserted, this, [this] { ++FindingsPresented; });
		connect(Findings.findings_list()->model(), &QAbstractItemModel::modelReset, this, again);
		connect(Findings.findings_list()->model(), &QAbstractItemModel::rowsInserted, this, again);
		connect(Findings.findings_list()->model(), &QAbstractItemModel::rowsRemoved, this, again);
		connect(Chat.replies_list()->model(), &QAbstractItemModel::rowsInserted, this, again);
		connect(Chat.replies_list()->model(), &QAbstractItemModel::modelReset, this, again);
		for (auto* const action : { Launch.start_action(), Launch.cancel_action(), Chat.continue_old_revision_action(), Chat.new_session_action(), Chat.send_action() }) {
			connect(action, &QAction::changed, this, again);
		}
		connect(profiles, &QComboBox::currentIndexChanged, this, again);
		// [[BPR-PENDING-AQ6]], owner look v001 ("no indication anything
		// happened"). The working line follows the request as it moves, with
		// the time it has taken, so a long run never looks like nothing.
		Launch.add_progress_observer([this] { progress(); });
		Ticker = new QTimer{ this };
		Ticker->setInterval(1000);
		connect(Ticker, &QTimer::timeout, this, [this] {
			present_banner();
			update_turn();
		});
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
		Pages->setCurrentWidget(State == agent_tab_state::setting_up       ? static_cast<QWidget*>(SetupView)
									: State == agent_tab_state::not_set_up ? static_cast<QWidget*>(Empty)
																		   : Conversation);
	}

	void agent_tab_view::adopt_setup(agent_setup_view& aSetup, std::function<void()> aCheckAgain)
	{
		SetupView  = &aSetup;
		CheckAgain = std::move(aCheckAgain);
		Pages->addWidget(SetupView);
		SetupView->set_change_observer([this] { refresh(); });
		HistoryMenu->addSeparator();
		auto* const again = HistoryMenu->addAction(tr("Set up agent…"));
		again->setObjectName(QStringLiteral("agentSetUpAgainAction"));
		connect(again, &QAction::triggered, this, [this] {
			if (SetupView != nullptr) {
				SetupView->begin(core::setup_step::agent);
			}
		});
		auto* const folder = HistoryMenu->addAction(tr("Change repository folder…"));
		folder->setObjectName(QStringLiteral("agentChangeRepositoryAction"));
		connect(folder, &QAction::triggered, this, [this] {
			if (SetupView != nullptr) {
				SetupView->begin(core::setup_step::repository);
			}
		});
		refresh();
	}

	void agent_tab_view::present_ready(const QString& aTitle, const QString& aText, const QString& aGets)
	{
		ReadyTitle->setText(aTitle);
		ReadyText->setText(aText);
		ReadyGets->setText(aGets);
		ReadyCard->setVisible(!aTitle.isEmpty());
	}

	void agent_tab_view::refresh()
	{
		if (!panels_alive()) {
			return;
		}
		const auto setUp      = Launch.profile_selector()->count() > 0;
		const auto running    = Launch.cancel_action()->isEnabled();
		const auto status     = Launch.status_text();
		const auto failed     = status.startsWith(QStringLiteral("Finished:")) && !status.contains(QStringLiteral("success"));
		const auto moved      = !Chat.offered_continuations().empty();
		const auto setting    = SetupView != nullptr && SetupView->needed();
		const auto wasWorking = State == agent_tab_state::working;
		State                 = setting ? agent_tab_state::setting_up
			: !setUp                    ? agent_tab_state::not_set_up
			: running                   ? agent_tab_state::working
			: moved                     ? agent_tab_state::head_moved
			: failed                    ? agent_tab_state::stopped
										: agent_tab_state::idle;
		if (State == agent_tab_state::working && !wasWorking) {
			Started.start();
			Ticker->start();
			ReadyCard->hide();
		} else if (State != agent_tab_state::working && wasWorking) {
			Ticker->stop();
			// A request that just ended says so, whatever it found.
			Finished = Launch.last_outcome() == application::agent_run_outcome::success;
		}
		if (State == agent_tab_state::working) {
			Finished = false;
		}
		if (Pages->currentWidget() != HistoryPage || setting) {
			Pages->setCurrentWidget(setting ? static_cast<QWidget*>(SetupView) : State == agent_tab_state::not_set_up ? static_cast<QWidget*>(Empty)
																													  : Conversation);
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
		Thread->setVisible(!Turns.empty());
		ClearSession->setVisible(!Turns.empty());
		ClearSession->setEnabled(State != agent_tab_state::working);
		refresh_suggestions();
		update_turn();
	}

	void agent_tab_view::progress()
	{
		if (!panels_alive()) {
			return;
		}
		const auto phase = static_cast<int>(Launch.current_phase());
		if (phase != LastPhase) {
			if (LastPhase == static_cast<int>(agent_launch_panel::phase::idle) && phase == static_cast<int>(agent_launch_panel::phase::preparing)) {
				begin_turn();
			}
			LastPhase = phase;
			refresh();
			return;
		}
		present_banner();
		update_turn();
	}

	void agent_tab_view::begin_turn()
	{
		auto next = turn{ };
		if (PendingReview || !PendingQuestion) {
			next.Review   = true;
			next.Question = tr("Review this pull request");
		} else {
			next.Question = *PendingQuestion;
			// Sent: the field is ready for the next question.
			Chat.composer()->clear();
		}
		PendingQuestion.reset();
		PendingReview  = false;
		FindingsAtTurn = FindingsPresented;
		add_turn_widgets(next);
		Turns.push_back(std::move(next));
		Thread->show();
	}

	void agent_tab_view::add_turn_widgets(turn& aTurn)
	{
		auto* const box = new QWidget{ Thread };
		box->setObjectName(QStringLiteral("agentTurn"));
		auto* const column = new QVBoxLayout{ box };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(4);
		auto* const asked = new rounded{ rounded::look::field, box };
		asked->setObjectName(QStringLiteral("agentTurnQuestion"));
		auto* const askedColumn = new QVBoxLayout{ asked };
		askedColumn->setContentsMargins(10, 6, 10, 6);
		auto* const question = new QLabel{ aTurn.Question, asked };
		question->setWordWrap(true);
		question->setTextFormat(Qt::PlainText);
		question->setTextInteractionFlags(Qt::TextSelectableByMouse);
		if (aTurn.Review) {
			auto italic = question->font();
			italic.setItalic(true);
			question->setFont(italic);
		}
		askedColumn->addWidget(question);
		column->addWidget(asked);
		// Owner decision (a): the answer reads as the agent wrote it, its
		// Markdown rendered in place; links and images do nothing.
		auto* const answer = new markdown_body_view{ box };
		answer->setObjectName(QStringLiteral("agentTurnAnswer"));
		answer->set_in_place(true);
		answer->set_height_observer([this, answer] { fit_answer(*answer); });
		answer->hide();
		column->addWidget(answer);
		// Owner decision 2026-10-23: the turn's findings, as cards, under its answer.
		auto* const cards = new QVBoxLayout{ };
		cards->setSpacing(8);
		column->addLayout(cards);
		auto* const footerRow = new QHBoxLayout{ };
		footerRow->setSpacing(6);
		auto* const footer = new QLabel{ box };
		footer->setObjectName(QStringLiteral("agentTurnFooter"));
		footer->setForegroundRole(QPalette::PlaceholderText);
		footer->setWordWrap(true);
		footerRow->addWidget(footer, 1);
		// Save as note: the whole reply, or the part selected in it.
		auto* const save = new quiet_push_button{ tr("Save as note"), box };
		save->setObjectName(QStringLiteral("agentTurnSaveNote"));
		save->setFlat(true);
		save->setToolTip(tr("Keep this reply, or the part of it you selected, as a private note. Nothing is published."));
		save->hide();
		footerRow->addWidget(save, 0, Qt::AlignTop);
		column->addLayout(footerRow);
		ThreadColumn->addWidget(box);
		aTurn.Widget     = box;
		aTurn.Answer     = answer;
		aTurn.Footer     = footer;
		aTurn.SaveNote   = save;
		aTurn.CardColumn = cards;
		connect(save, &QPushButton::clicked, this, [this, save] {
			for (auto& kept : Turns) {
				if (kept.SaveNote != save || !NoteDispatch) {
					continue;
				}
				auto       text     = kept.AnswerText;
				const auto selected = kept.Answer != nullptr ? kept.Answer->rendered_view()->textCursor().selectedText() : QString{ };
				if (!selected.trimmed().isEmpty()) {
					text = selected;
					text.replace(QChar::ParagraphSeparator, QLatin1Char{ '\n' });
				}
				save->setEnabled(false);
				NoteDispatch(text, [guarded = QPointer{ save }](const bool aSaved, const QString& aWhy) {
					if (guarded == nullptr) {
						return;
					}
					guarded->setText(aSaved ? QObject::tr("Saved as a note") : QObject::tr("Not saved: %1").arg(aWhy));
					guarded->setEnabled(!aSaved);
				});
			}
		});
	}

	void agent_tab_view::set_answer(turn& aTurn, const QString& aText)
	{
		if (aTurn.Answer == nullptr || (aText == aTurn.AnswerText && aTurn.Answer->isHidden() == aText.isEmpty())) {
			return;
		}
		aTurn.AnswerText = aText;
		// The findings block is for the cards; the reply shows without it.
		if (const auto fence = aTurn.AnswerText.lastIndexOf(QStringLiteral("```bpr-findings")); fence >= 0) {
			const auto closed = aTurn.AnswerText.indexOf(QStringLiteral("```"), fence + 3);
			aTurn.AnswerText.remove(fence, closed < 0 ? aTurn.AnswerText.size() - fence : closed + 3 - fence);
			aTurn.AnswerText = aTurn.AnswerText.trimmed();
		}
		aTurn.Answer->set_body(aTurn.AnswerText.toStdString(), "agent-turn");
		aTurn.Answer->setVisible(!aTurn.AnswerText.isEmpty());
		fit_answer(*aTurn.Answer);
	}

	void agent_tab_view::fit_answer(markdown_body_view& aAnswer) const
	{
		// The thread is the viewport less the conversation's margins.
		constexpr auto margins = 12 + 12;
		const auto     width   = Viewport == nullptr ? aAnswer.width() : Viewport->width() - margins;
		aAnswer.setFixedHeight(aAnswer.isHidden() ? 0 : aAnswer.heightForWidth(std::max(1, width)));
	}

	void agent_tab_view::present_thread(const std::vector<agent_thread_turn>& aTurns)
	{
		for (auto& kept : Turns) {
			if (kept.Widget != nullptr) {
				kept.Widget->hide();
				ThreadColumn->removeWidget(kept.Widget);
				kept.Widget->deleteLater();
			}
		}
		Turns.clear();
		// A turn's cards went with its widgets.
		std::erase_if(CardEntries, [](const card_entry& aEntry) { return aEntry.Turn.has_value(); });
		for (const auto& shown : aTurns) {
			auto next     = turn{ };
			next.Question = shown.Question;
			next.Done     = true;
			add_turn_widgets(next);
			set_answer(next, shown.Answer);
			next.Footer->setText(shown.Footer);
			Turns.push_back(std::move(next));
		}
		Finished = false;
		refresh();
	}

	void agent_tab_view::set_thread_observer(std::function<void(std::vector<agent_thread_turn>)> aObserver) { ThreadObserver = std::move(aObserver); }

	auto agent_tab_view::thread_turns() const -> std::vector<agent_thread_turn>
	{
		auto result = std::vector<agent_thread_turn>{ };
		for (const auto& kept : Turns) {
			if (kept.Done) {
				result.push_back({ .Question = kept.Question, .Answer = kept.AnswerText, .Footer = kept.Footer != nullptr ? kept.Footer->text() : QString{ } });
			}
		}
		return result;
	}

	void agent_tab_view::set_clear_session_dispatch(std::function<void()> aDispatch) { ClearDispatch = std::move(aDispatch); }

	void agent_tab_view::set_clear_confirmation(std::function<bool(std::size_t)> aConfirm) { ClearConfirm = std::move(aConfirm); }

	void agent_tab_view::clear_session()
	{
		if (!panels_alive() || State == agent_tab_state::working) {
			return;
		}
		// Owner decision (b): a confirmation only when turns would leave the
		// thread; an empty one is cleared at once.
		if (!Turns.empty()) {
			const auto count     = Turns.size();
			const auto confirmed = ClearConfirm ? ClearConfirm(count)
												: QMessageBox::question(this, tr("Clear session?"),
																		count == 1U ? tr("The thread's one turn leaves it, and the next question starts a new session that does not know it.")
																					: tr("The thread's %1 turns leave it, and the next question starts a new session that does not know them.").arg(count),
																		QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel)
					== QMessageBox::Yes;
			if (!confirmed) {
				return;
			}
		}
		present_thread({ });
		if (ClearDispatch) {
			ClearDispatch();
		}
	}

	void agent_tab_view::set_suggestion_context(std::function<agent_suggestion_context()> aContext)
	{
		SuggestionContext = std::move(aContext);
		refresh_suggestions();
	}

	auto agent_tab_view::suggestions() const -> QStringList { return Prompts; }

	auto agent_tab_view::suggestion_buttons() const -> std::vector<QPushButton*>
	{
		auto result = std::vector<QPushButton*>{ };
		for (const auto& button : PromptButtons) {
			if (button != nullptr) {
				result.push_back(button);
			}
		}
		return result;
	}

	void agent_tab_view::send_prompt(const QString& aPrompt)
	{
		if (!panels_alive() || !Chat.send_action()->isEnabled()) {
			return;
		}
		Chat.composer()->setPlainText(aPrompt);
		ask();
	}

	void agent_tab_view::refresh_suggestions()
	{
		if (!panels_alive() || Suggestions == nullptr) {
			return;
		}
		// Owner decision (a): the pull request, the open file when one is
		// open, what the change could break, and the selected lines when
		// lines are selected.
		const auto context = SuggestionContext ? SuggestionContext() : agent_suggestion_context{ };
		auto       prompts = QStringList{ tr("Review this pull request") };
		if (!context.OpenFile.isEmpty()) {
			prompts << tr("Review %1").arg(context.OpenFile);
		}
		prompts << tr("What could this change break?");
		if (context.Lines) {
			const auto [first, last] = *context.Lines;
			prompts << (first == last ? tr("Explain the selected line (%1)").arg(first) : tr("Explain the selected lines (%1–%2)").arg(first).arg(last));
		}
		const auto usable = Chat.send_action()->isEnabled() && State != agent_tab_state::working;
		if (prompts != Prompts) {
			Prompts = prompts;
			for (auto& button : PromptButtons) {
				if (button != nullptr) {
					button->hide();
					SuggestionColumn->removeWidget(button);
					button->deleteLater();
				}
			}
			PromptButtons.clear();
			auto* const menu = SuggestionsMenu->menu();
			menu->clear();
			for (const auto& prompt : Prompts) {
				auto* const button = new quiet_push_button{ Suggestions };
				button->setObjectName(QStringLiteral("agentSuggestion"));
				// A long path is shortened in its middle; the prompt sent is whole.
				button->setText(button->fontMetrics().elidedText(prompt, Qt::ElideMiddle, 320));
				button->setToolTip(prompt);
				button->setAccessibleName(prompt);
				button->setProperty("agentPrompt", prompt);
				connect(button, &QPushButton::clicked, this, [this, prompt] { send_prompt(prompt); });
				SuggestionColumn->addWidget(button, 0, Qt::AlignLeft);
				PromptButtons.emplace_back(button);
				auto* const item = menu->addAction(prompt);
				connect(item, &QAction::triggered, this, [this, prompt] { send_prompt(prompt); });
			}
		}
		for (const auto& button : PromptButtons) {
			if (button != nullptr) {
				button->setEnabled(usable);
			}
		}
		for (auto* const item : SuggestionsMenu->menu()->actions()) {
			item->setEnabled(usable);
		}
		const auto conversing = State != agent_tab_state::not_set_up && State != agent_tab_state::setting_up;
		Suggestions->setVisible(conversing && Turns.empty());
		SuggestionsMenu->setVisible(conversing && !Turns.empty());
	}

	void agent_tab_view::update_turn()
	{
		if (Turns.empty() || Turns.back().Done || Turns.back().Answer == nullptr || Turns.back().Footer == nullptr) {
			return;
		}
		auto&      current = Turns.back();
		const auto agent   = Launch.profile_selector()->currentText();
		const auto seconds = Started.isValid() ? Started.elapsed() / 1000 : 0;
		const auto elapsed = QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char{ '0' });
		const auto written = Launch.output_text().trimmed();
		switch (Launch.current_phase()) {
		case agent_launch_panel::phase::preparing:
			set_answer(current, written);
			current.Footer->setText(tr("%1 is making its workspace at the head, %2.").arg(agent, elapsed));
			return;
		case agent_launch_panel::phase::running:
			set_answer(current, written);
			current.Footer->setText(written.isEmpty() ? tr("%1 is thinking, %2.").arg(agent, elapsed) : tr("%1 is writing, %2.").arg(agent, elapsed));
			return;
		case agent_launch_panel::phase::idle:
			break;
		}
		// The request ended: say what came of it.
		current.Done          = true;
		const auto findings   = Findings.finding_count();
		const auto succeeded  = Launch.last_outcome() == application::agent_run_outcome::success;
		const auto stopReason = Launch.stop_reason_text();
		if (succeeded) {
			set_answer(current, written.isEmpty() ? tr("%1 finished without writing an answer.").arg(agent) : written);
		} else {
			set_answer(current, !stopReason.isEmpty() ? stopReason : tr("%1 stopped before answering: %2.").arg(agent, Launch.status_text()));
		}
		auto ran = agent;
		if (!Launch.last_reported_model().isEmpty()) {
			ran = tr("%1 (%2)").arg(agent, Launch.last_reported_model());
		}
		auto footer = succeeded ? tr("%1 answered in %2").arg(ran, elapsed) : tr("Stopped after %1").arg(elapsed);
		footer += Launch.last_reported_cost().isEmpty() ? QStringLiteral(".") : tr(" for $%1.").arg(Launch.last_reported_cost());
		if (succeeded && findings > 0U && FindingsPresented != FindingsAtTurn) {
			footer += QStringLiteral(" ") + (findings == 1U ? tr("1 finding, above.") : tr("%1 findings, above.").arg(findings));
		}
		current.Footer->setText(footer);
		if (succeeded && FindingsPresented != FindingsAtTurn) {
			claim_findings(Turns.size() - 1U);
		}
		if (current.SaveNote != nullptr) {
			current.SaveNote->setVisible(succeeded && !current.AnswerText.isEmpty());
		}
		ClearSession->setVisible(true);
		if (ThreadObserver) {
			ThreadObserver(thread_turns());
		}
	}

	auto agent_tab_view::turn_answer(const std::size_t aTurn) const -> QString
	{
		return aTurn < Turns.size() ? Turns[aTurn].AnswerText : QString{ };
	}

	auto agent_tab_view::turn_question(const std::size_t aTurn) const -> QString { return aTurn < Turns.size() ? Turns[aTurn].Question : QString{ }; }

	auto agent_tab_view::turn_footer(const std::size_t aTurn) const -> QString
	{
		return aTurn < Turns.size() && Turns[aTurn].Footer != nullptr ? Turns[aTurn].Footer->text() : QString{ };
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
		BannerFirst->show();
		BannerSecond->show();
		Log->setVisible(LogShown && State == agent_tab_state::stopped);
		switch (State) {
		case agent_tab_state::not_set_up:
		case agent_tab_state::setting_up:
			Banner->hide();
			return;
		case agent_tab_state::idle: {
			if (!Finished || !Turns.empty()) {
				Banner->hide();
				return;
			}
			callout->Look    = rounded::look::ready;
			const auto count = Findings.finding_count();
			BannerTitle->setText(tr("%1 finished").arg(Launch.profile_selector()->currentText()));
			BannerText->setText(count > 0U                             ? (count == 1U ? tr("1 finding below. Draft a comment from it, or ask a follow-up.") : tr("%1 findings below. Draft a comment from one, or ask a follow-up.").arg(count))
									: Chat.replies_list()->count() > 0 ? tr("Its answer is below. Ask a follow-up in the question field.")
																	   : tr("It found nothing to report. Ask a question below."));
			BannerFirst->hide();
			BannerSecond->hide();
			Banner->show();
			Banner->update();
			return;
		}
		case agent_tab_state::working: {
			BannerTitle->setText(tr("%1 is working").arg(Launch.profile_selector()->currentText()));
			const auto seconds = Started.isValid() ? Started.elapsed() / 1000 : 0;
			const auto elapsed = QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char{ '0' });
			auto       line    = Launch.current_phase() == agent_launch_panel::phase::running
				? tr("Running, %1. The answer and any findings appear here when it finishes.").arg(elapsed)
				: tr("Making the agent's workspace at the head, %1.").arg(elapsed);
			const auto latest  = Launch.latest_output_line();
			if (!latest.isEmpty()) {
				line += QStringLiteral("\n") + tr("Latest: %1").arg(latest.left(160));
			}
			BannerText->setText(line);
			BannerFirst->setText(tr("Stop"));
			BannerFirst->show();
			connect(BannerFirst, &QPushButton::clicked, Launch.cancel_action(), &QAction::trigger);
			BannerSecond->hide();
			break;
		}
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
			// [[BPR-PENDING-AQ6]], RFC-BPR-0092 Partly set up. An Ask that ended
			// for a setup reason offers that check again, then the setup's
			// words, rather than only asking again. The question is kept.
			if (SetupView != nullptr && CheckAgain
				&& (Launch.last_outcome() == application::agent_run_outcome::workspace_unavailable || Launch.last_outcome() == application::agent_run_outcome::failed_to_start)) {
				BannerFirst->disconnect();
				BannerFirst->setText(tr("Check again"));
				connect(BannerFirst, &QPushButton::clicked, this, [this] {
					SetupView->begin(core::setup_step::check);
					CheckAgain();
				});
			}
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
		for (const auto& entry : CardEntries) {
			if (entry.Widget != nullptr) {
				result.push_back(entry.Widget);
			}
		}
		return result;
	}

	auto agent_tab_view::turn_cards(const std::size_t aTurn) const -> std::vector<QWidget*>
	{
		auto result = std::vector<QWidget*>{ };
		for (const auto& entry : CardEntries) {
			if (entry.Turn == std::optional{ aTurn } && entry.Widget != nullptr) {
				result.push_back(entry.Widget);
			}
		}
		return result;
	}

	auto agent_tab_view::finding_card_state(const std::size_t aCard) const -> agent_finding_card_state
	{
		return aCard < CardEntries.size() ? CardEntries[aCard].State : agent_finding_card_state::open;
	}

	void agent_tab_view::set_finding_card_dispatch(agent_finding_card_dispatch aDispatch) { CardDispatch = std::move(aDispatch); }

	void agent_tab_view::set_answer_note_dispatch(agent_answer_note_dispatch aDispatch) { NoteDispatch = std::move(aDispatch); }

	auto agent_tab_view::turn_note_button(const std::size_t aTurn) const -> QPushButton* { return aTurn < Turns.size() ? Turns[aTurn].SaveNote.data() : nullptr; }

	void agent_tab_view::present_finding_card_state(const std::uint64_t aSession, const std::size_t aOrdinal, const agent_finding_card_state aState, const QString& aWhy)
	{
		for (auto& entry : CardEntries) {
			if (entry.Session == aSession && entry.Ordinal == aOrdinal) {
				entry.State = aState;
				entry.Why   = aWhy;
				show_card_state(entry);
			}
		}
	}

	void agent_tab_view::rebuild_findings()
	{
		if (!panels_alive()) {
			return;
		}
		// The cards no turn brought: a delivery shown before any turn, or one
		// a turn has not claimed. Cards under a turn stay with it.
		for (auto entry = CardEntries.begin(); entry != CardEntries.end();) {
			if (entry->Turn) {
				++entry;
				continue;
			}
			if (entry->Widget != nullptr) {
				// Gone from view now; deleted once any press on it returns.
				entry->Widget->hide();
				CardColumn->removeWidget(entry->Widget);
				entry->Widget->deleteLater();
			}
			entry = CardEntries.erase(entry);
		}
		const auto* const delivery = Findings.shown_delivery();
		if (delivery == nullptr || ClaimedSession == std::optional{ delivery->Session.value() }) {
			Cards->hide();
			return;
		}
		for (std::size_t ordinal = 0U; ordinal < delivery->Findings.size(); ++ordinal) {
			auto entry = card_entry{
				.Session  = delivery->Session.value(),
				.Ordinal  = ordinal,
				.Finding  = delivery->Findings[ordinal].Finding,
				.Anchored = delivery->Findings[ordinal].Verdict.State == core::agent_finding_anchor_state::anchored,
			};
			CardEntries.push_back(std::move(entry));
			auto* const card = make_card(CardEntries.back(), Cards);
			CardColumn->addWidget(card);
		}
		Cards->setVisible(!delivery->Findings.empty());
	}

	void agent_tab_view::claim_findings(const std::size_t aTurn)
	{
		const auto* const delivery = Findings.shown_delivery();
		if (delivery == nullptr || aTurn >= Turns.size() || Turns[aTurn].CardColumn == nullptr || delivery->Findings.empty()) {
			return;
		}
		ClaimedSession = delivery->Session.value();
		// The loose cards were this delivery's; they move under the turn.
		rebuild_findings();
		for (std::size_t ordinal = 0U; ordinal < delivery->Findings.size(); ++ordinal) {
			auto entry = card_entry{
				.Session  = delivery->Session.value(),
				.Ordinal  = ordinal,
				.Finding  = delivery->Findings[ordinal].Finding,
				.Anchored = delivery->Findings[ordinal].Verdict.State == core::agent_finding_anchor_state::anchored,
				.Turn     = aTurn,
			};
			CardEntries.push_back(std::move(entry));
			auto* const card = make_card(CardEntries.back(), Turns[aTurn].Widget);
			Turns[aTurn].CardColumn->addWidget(card);
		}
	}

	auto agent_tab_view::make_card(card_entry& aEntry, QWidget* aParent) -> QWidget*
	{
		const auto& finding = aEntry.Finding;
		auto* const card    = new rounded{ rounded::look::card, aParent };
		card->setObjectName(QStringLiteral("agentFindingCard"));
		card->Edge      = severity_colour(finding.Severity, palette());
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
		// Lines are shown one-based, first and last, as an editor numbers them.
		const auto  first = finding.Lines.Begin + 1U;
		const auto  last  = std::max(first, finding.Lines.End);
		auto* const where = new QLabel{ first == last ? QStringLiteral("%1:%2").arg(QString::fromStdString(finding.Path).section(QLatin1Char{ '/' }, -1)).arg(first)
													  : QStringLiteral("%1:%2–%3").arg(QString::fromStdString(finding.Path).section(QLatin1Char{ '/' }, -1)).arg(first).arg(last),
										card };
		where->setObjectName(QStringLiteral("agentFindingAnchor"));
		auto code = QFont{ QStringLiteral("Menlo") };
		code.setStyleHint(QFont::Monospace);
		code.setPointSizeF(std::max(1.0, font().pointSizeF() * 0.86));
		code.setWeight(QFont::DemiBold);
		where->setFont(code);
		where->setToolTip(aEntry.Anchored ? QString::fromStdString(finding.Path) : tr("%1 (not the file open when the agent answered)").arg(QString::fromStdString(finding.Path)));
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
		if (finding.SuggestedComment && !finding.SuggestedComment->empty()) {
			auto* const comment = new QLabel{ QString::fromStdString(*finding.SuggestedComment), card };
			comment->setObjectName(QStringLiteral("agentFindingComment"));
			comment->setWordWrap(true);
			comment->setTextFormat(Qt::PlainText);
			comment->setTextInteractionFlags(Qt::TextSelectableByMouse);
			auto italic = comment->font();
			italic.setItalic(true);
			comment->setFont(italic);
			col->addWidget(comment);
		}
		auto* const actions = new QHBoxLayout{ };
		actions->setSpacing(6);
		aEntry.Id         = NextCardId++;
		const auto  index = aEntry.Id;
		auto* const draft = new quiet_push_button{ tr("Add as draft comment"), card };
		draft->setObjectName(QStringLiteral("agentFindingDraftComment"));
		draft->setToolTip(tr("A private draft at its lines, from its suggested comment. Nothing is published."));
		connect(draft, &QPushButton::clicked, this, [this, index] { act_on_card(index, agent_finding_card_action::draft_comment); });
		actions->addWidget(draft);
		auto* const note = new quiet_push_button{ tr("Save as note"), card };
		note->setObjectName(QStringLiteral("agentFindingSaveNote"));
		note->setToolTip(tr("A private note on its lines, never published"));
		connect(note, &QPushButton::clicked, this, [this, index] { act_on_card(index, agent_finding_card_action::save_note); });
		actions->addWidget(note);
		auto* const dismiss = new quiet_push_button{ tr("Dismiss"), card };
		dismiss->setObjectName(QStringLiteral("agentFindingDismiss"));
		dismiss->setFlat(true);
		connect(dismiss, &QPushButton::clicked, this, [this, index] { act_on_card(index, agent_finding_card_action::dismiss); });
		actions->addWidget(dismiss);
		auto* const state = new QLabel{ card };
		state->setObjectName(QStringLiteral("agentFindingState"));
		state->setForegroundRole(QPalette::PlaceholderText);
		state->setWordWrap(true);
		state->hide();
		actions->addWidget(state, 1);
		auto* const undo = new quiet_push_button{ tr("Undo"), card };
		undo->setObjectName(QStringLiteral("agentFindingUndo"));
		undo->setFlat(true);
		undo->hide();
		connect(undo, &QPushButton::clicked, this, [this, index] { act_on_card(index, agent_finding_card_action::undo_dismiss); });
		actions->addWidget(undo);
		actions->addStretch(1);
		col->addLayout(actions);
		// Choosing a card selects its finding and shows its lines.
		card->installEventFilter(this);
		card->setProperty("agentFindingCard", QVariant::fromValue<qulonglong>(index));
		card->setCursor(Qt::PointingHandCursor);
		aEntry.Widget = card;
		show_card_state(aEntry);
		return card;
	}

	void agent_tab_view::show_card_state(card_entry& aEntry)
	{
		if (aEntry.Widget == nullptr) {
			return;
		}
		auto* const card  = aEntry.Widget.data();
		auto* const state = card->findChild<QLabel*>(QStringLiteral("agentFindingState"));
		auto* const undo  = card->findChild<QPushButton*>(QStringLiteral("agentFindingUndo"));
		const auto  open  = aEntry.State == agent_finding_card_state::open || aEntry.State == agent_finding_card_state::refused;
		for (const auto* const name : { "agentFindingDraftComment", "agentFindingSaveNote", "agentFindingDismiss" }) {
			if (auto* const button = card->findChild<QPushButton*>(QString::fromLatin1(name)); button != nullptr) {
				button->setVisible(open);
			}
		}
		undo->setVisible(aEntry.State == agent_finding_card_state::dismissed);
		switch (aEntry.State) {
		case agent_finding_card_state::open:      state->clear(); break;
		case agent_finding_card_state::working:   state->setText(tr("Working…")); break;
		case agent_finding_card_state::drafted:   state->setText(tr("Drafted. It is a private draft until you publish it.")); break;
		case agent_finding_card_state::noted:     state->setText(tr("Saved as a private note.")); break;
		case agent_finding_card_state::dismissed: state->setText(tr("Dismissed.")); break;
		case agent_finding_card_state::refused:   state->setText(aEntry.Why.isEmpty() ? tr("That could not be done.") : aEntry.Why); break;
		}
		state->setVisible(!state->text().isEmpty());
		card->setProperty("agentFindingStateName", static_cast<int>(aEntry.State));
		card->setEnabled(true);
		card->update();
	}

	void agent_tab_view::act_on_card(const std::uint64_t aCard, const agent_finding_card_action aAction)
	{
		const auto found = std::ranges::find(CardEntries, aCard, &card_entry::Id);
		if (found == CardEntries.end() || !panels_alive()) {
			return;
		}
		auto& entry = *found;
		if (aAction == agent_finding_card_action::undo_dismiss) {
			// A dismissal made nothing, so taking it back is the card's alone.
			entry.State = agent_finding_card_state::open;
			show_card_state(entry);
			if (CardDispatch) {
				CardDispatch({ .Session = entry.Session, .Ordinal = entry.Ordinal, .Finding = entry.Finding, .Action = aAction });
			}
			return;
		}
		if (CardDispatch) {
			entry.State = aAction == agent_finding_card_action::dismiss ? agent_finding_card_state::dismissed : agent_finding_card_state::working;
			show_card_state(entry);
			CardDispatch({ .Session = entry.Session, .Ordinal = entry.Ordinal, .Finding = entry.Finding, .Action = aAction });
			return;
		}
		// Without a host the queue's own actions are asked, as before.
		const auto* const delivery = Findings.shown_delivery();
		if (delivery == nullptr || delivery->Session.value() != entry.Session) {
			return;
		}
		Findings.select(entry.Ordinal);
		const auto queued = aAction == agent_finding_card_action::draft_comment ? finding_queue_action::convert_to_draft_comment
			: aAction == agent_finding_card_action::save_note                   ? finding_queue_action::add_to_private_notes
																				: finding_queue_action::dismiss;
		if (auto* const action = Findings.action(queued); action != nullptr) {
			action->trigger();
		}
		entry.State = aAction == agent_finding_card_action::draft_comment ? agent_finding_card_state::drafted
			: aAction == agent_finding_card_action::save_note             ? agent_finding_card_state::noted
																		  : agent_finding_card_state::dismissed;
		show_card_state(entry);
	}

	void agent_tab_view::ask()
	{
		if (!panels_alive()) {
			return;
		}
		if (run_slash_command(Chat.composer()->toPlainText().trimmed())) {
			return;
		}
		Notice->hide();
		Chat.send_action()->trigger();
	}

	void agent_tab_view::present_run_settings(std::vector<agent_run_settings> aSettings)
	{
		RunSettingsShown = std::move(aSettings);
		present_run_settings_button();
	}

	void agent_tab_view::set_run_settings_dispatch(agent_run_settings_dispatch aDispatch) { RunSettingsDispatch = std::move(aDispatch); }

	auto agent_tab_view::current_run_settings() const -> const agent_run_settings*
	{
		const auto profile = Launch.selected_profile();
		if (!profile) {
			return nullptr;
		}
		const auto found = std::ranges::find_if(RunSettingsShown, [&profile](const agent_run_settings& aSettings) { return aSettings.Profile == profile->value(); });
		return found == RunSettingsShown.end() ? nullptr : &*found;
	}

	auto agent_tab_view::run_settings_text() const -> QString { return RunSettings->text(); }

	void agent_tab_view::present_run_settings_button()
	{
		const auto* const settings = current_run_settings();
		RunSettings->setVisible(settings != nullptr);
		if (settings == nullptr) {
			return;
		}
		// Short enough for the field's row: the model, or "Default model",
		// and the effort when one is chosen.
		const auto model = settings->Model.empty() ? tr("default model") : QString::fromStdString(settings->Model);
		auto       shown = settings->Model.empty() ? tr("Default model") : QString::fromStdString(settings->Model);
		if (!settings->Effort.empty()) {
			shown += QStringLiteral(" · ") + tr("%1 effort").arg(QString::fromStdString(settings->Effort));
		}
		// Owner amendment 2026-10-23: agent · model · effort.
		RunSettings->setText(settings->Agent + QStringLiteral(" · ") + (!(settings->ModelChoosable || settings->EffortChoosable) ? tr("its own model") : shown));
		auto tip = tr("%1 runs %2").arg(settings->Agent, settings->Program.isEmpty() ? tr("the program setup found") : settings->Program);
		if (settings->ModelChoosable) {
			tip += QStringLiteral("\n") + (settings->Model.empty() ? tr("Model: the agent's own default") : tr("Model: %1").arg(model));
		}
		if (settings->EffortChoosable) {
			tip += QStringLiteral("\n") + (settings->Effort.empty() ? tr("Effort: the agent's own default") : tr("Effort: %1").arg(QString::fromStdString(settings->Effort)));
		}
		if (!settings->Note.isEmpty()) {
			tip += QStringLiteral("\n") + settings->Note;
		}
		if (!Launch.last_reported_model().isEmpty()) {
			tip += QStringLiteral("\n") + tr("Last answer ran %1").arg(Launch.last_reported_model());
		}
		RunSettings->setToolTip(tip);
		auto* const menu = RunSettings->menu();
		menu->clear();
		// Every agent added, one press away; each keeps its own conversation
		// about this pull request (owner decision 2026-10-23).
		auto* const agents = new QActionGroup{ menu };
		for (const auto& offered : RunSettingsShown) {
			auto* const item = menu->addAction(offered.Agent);
			item->setObjectName(QStringLiteral("agentRunSettingsAgent"));
			item->setCheckable(true);
			item->setChecked(offered.Profile == settings->Profile);
			agents->addAction(item);
			connect(item, &QAction::triggered, this, [this, profile = offered.Profile] { choose_agent(profile); });
		}
		menu->addSeparator();
		auto* const what = menu->addAction(tr("%1 runs %2").arg(settings->Agent, settings->Program.isEmpty() ? tr("the program setup found") : settings->Program));
		what->setEnabled(false);
		if (!settings->Note.isEmpty()) {
			menu->addAction(settings->Note)->setEnabled(false);
		}
		const auto profile = settings->Profile;
		const auto apply   = [this, profile](std::optional<std::string> aModel, std::optional<std::string> aEffort) {
			if (RunSettingsDispatch && !RunSettingsDispatch(profile, std::move(aModel), std::move(aEffort))) {
				Notice->setText(tr("The agent doesn't take that setting."));
				Notice->show();
			}
		};
		if (settings->ModelChoosable) {
			auto* const models = menu->addMenu(tr("Model"));
			models->setObjectName(QStringLiteral("agentRunSettingsModels"));
			auto* const group = new QActionGroup{ models };
			const auto  add   = [&](const std::string& aValue, const QString& aLabel) {
				auto* const action = models->addAction(aLabel);
				action->setCheckable(true);
				action->setChecked(settings->Model == aValue);
				group->addAction(action);
				connect(action, &QAction::triggered, this, [apply, aValue] { apply(aValue, std::nullopt); });
			};
			add({ }, tr("Default (the agent's own setting)"));
			for (const auto& [value, label] : settings->Models) {
				add(value, label);
			}
			const auto known = settings->Model.empty() || std::ranges::any_of(settings->Models, [&settings](const auto& aModel) { return aModel.first == settings->Model; });
			if (!known) {
				add(settings->Model, QString::fromStdString(settings->Model));
			}
			connect(models->addAction(tr("Other…")), &QAction::triggered, this, [this, apply, current = settings->Model] {
				auto       accepted = false;
				const auto name     = QInputDialog::getText(this, tr("Model"), tr("The model's name, as the agent's --model takes it:"), QLineEdit::Normal, QString::fromStdString(current), &accepted).trimmed();
				if (accepted) {
					apply(name.toStdString(), std::nullopt);
				}
			});
		}
		if (settings->EffortChoosable) {
			auto* const efforts = menu->addMenu(tr("Effort"));
			efforts->setObjectName(QStringLiteral("agentRunSettingsEfforts"));
			auto* const group = new QActionGroup{ efforts };
			const auto  add   = [&](const std::string& aValue, const QString& aLabel) {
				auto* const action = efforts->addAction(aLabel);
				action->setCheckable(true);
				action->setChecked(settings->Effort == aValue);
				group->addAction(action);
				connect(action, &QAction::triggered, this, [apply, aValue] { apply(std::nullopt, aValue); });
			};
			add({ }, tr("Default (the agent's own setting)"));
			for (const auto& level : settings->Efforts) {
				add(level, QString::fromStdString(level));
			}
		}
		menu->addSeparator();
		menu->addAction(tr("Type /model NAME or /effort LEVEL in the question field"))->setEnabled(false);
	}

	auto agent_tab_view::choose_agent(const std::uint64_t aProfile) -> bool
	{
		if (!panels_alive() || State == agent_tab_state::working) {
			return false;
		}
		auto* const selector = Launch.profile_selector();
		const auto  index    = selector->findData(QVariant::fromValue<qulonglong>(aProfile));
		if (index < 0) {
			return false;
		}
		selector->setCurrentIndex(index);
		present_run_settings_button();
		return true;
	}

	auto agent_tab_view::run_slash_command(const QString& aText) -> bool
	{
		if (!aText.startsWith(QLatin1Char{ '/' })) {
			return false;
		}
		const auto words   = aText.mid(1).split(QLatin1Char{ ' ' }, Qt::SkipEmptyParts);
		const auto command = words.isEmpty() ? QString{ } : words.front().toLower();
		if (command != QStringLiteral("model") && command != QStringLiteral("effort")) {
			return false;
		}
		const auto* const settings = current_run_settings();
		const auto        isModel  = command == QStringLiteral("model");
		Notice->show();
		if (settings == nullptr || (isModel ? !settings->ModelChoosable : !settings->EffortChoosable)) {
			Notice->setText(isModel ? tr("This agent chooses its own model.") : tr("This agent has no effort setting."));
			return true;
		}
		if (words.size() < 2) {
			RunSettings->showMenu();
			Notice->setText(isModel ? tr("Choose a model, or type /model NAME.") : tr("Choose an effort, or type /effort LEVEL."));
			return true;
		}
		auto value = words.at(1).toStdString();
		if (value == "default") {
			value.clear();
		}
		const auto taken = RunSettingsDispatch && (isModel ? RunSettingsDispatch(settings->Profile, value, std::nullopt) : RunSettingsDispatch(settings->Profile, std::nullopt, value));
		if (!taken) {
			Notice->setText(isModel ? tr("%1 isn't a model name the agent takes.").arg(words.at(1)) : tr("%1 isn't an effort this agent takes. It takes %2.").arg(words.at(1), QString::fromStdString([&settings] {
																																									  auto list = std::string{ };
																																									  for (const auto& effort : settings->Efforts) {
																																										  list += (list.empty() ? "" : ", ") + effort;
																																									  }
																																									  return list;
																																								  }())));
			return true;
		}
		Chat.composer()->clear();
		Notice->setText(isModel ? (value.empty() ? tr("The next question runs on the agent's own model.") : tr("The next question runs on %1.").arg(QString::fromStdString(value)))
								: (value.empty() ? tr("The next question runs at the agent's own effort.") : tr("The next question runs at %1 effort.").arg(QString::fromStdString(value))));
		return true;
	}

	auto agent_tab_view::eventFilter(QObject* aWatched, QEvent* aEvent) -> bool
	{
		if (aEvent->type() == QEvent::KeyPress && aWatched == Chat.composer()) {
			const auto* const key = static_cast<QKeyEvent*>(aEvent);
			if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && (key->modifiers() & (Qt::ControlModifier | Qt::MetaModifier)) != 0) {
				ask();
				return true;
			}
		}
		if (aEvent->type() == QEvent::Resize && aWatched == Viewport) {
			for (const auto& kept : Turns) {
				if (kept.Answer != nullptr) {
					fit_answer(*kept.Answer);
				}
			}
			return false;
		}
		if (aEvent->type() == QEvent::MouseButtonRelease && aWatched->objectName() == QStringLiteral("agentFindingCard")) {
			const auto id    = static_cast<std::uint64_t>(aWatched->property("agentFindingCard").toULongLong());
			const auto found = std::ranges::find(CardEntries, id, &card_entry::Id);
			// Its lines are shown while its delivery is the one the queue holds.
			if (const auto* const delivery = Findings.shown_delivery(); found != CardEntries.end() && delivery != nullptr && delivery->Session.value() == found->Session) {
				Findings.select(found->Ordinal);
				if (auto* const action = Findings.action(finding_queue_action::open_evidence); action != nullptr) {
					action->trigger();
				}
			}
			for (const auto& entry : CardEntries) {
				if (entry.Widget != nullptr) {
					static_cast<rounded*>(entry.Widget.data())->Selected = entry.Widget == aWatched;
					entry.Widget->update();
				}
			}
			return true;
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}

	void agent_tab_view::showEvent(QShowEvent* aEvent)
	{
		QWidget::showEvent(aEvent);
		if (FirstShown) {
			const auto observer = std::exchange(FirstShown, { });
			observer();
		}
		refresh();
	}

	void agent_tab_view::set_first_shown(std::function<void()> aObserver)
	{
		FirstShown = std::move(aObserver);
		if (FirstShown && isVisible()) {
			const auto observer = std::exchange(FirstShown, { });
			observer();
		}
	}

	void agent_tab_view::changeEvent(QEvent* aEvent)
	{
		QWidget::changeEvent(aEvent);
		if (aEvent->type() == QEvent::PaletteChange && Empty != nullptr) {
			rebuild_findings();
		}
	}
} // namespace bpr::qt
