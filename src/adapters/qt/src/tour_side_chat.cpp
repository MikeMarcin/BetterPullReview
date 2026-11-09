#include "bpr/qt/tour_side_chat.hpp"

#include "bpr/core/agent_turn.hpp"
#include "bpr/qt/agent_feature.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/quiet_tool_button.hpp"
#include "bpr/qt/skill_completer.hpp"
#include "bpr/qt/tour_agent_panel.hpp"

#include <QClipboard>
#include <QDialog>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextCursor>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <map>

namespace bpr::qt {
	namespace {
		const auto open_quote  = QChar{ 0x201C };
		const auto close_quote = QChar{ 0x201D };
		const auto ellipsis    = QChar{ 0x2026 };

		auto mixed(const QColor& aFrom, const QColor& aTo, const double aAmount) -> QColor
		{
			return QColor::fromRgbF(static_cast<float>(aFrom.redF() + (aTo.redF() - aFrom.redF()) * aAmount), static_cast<float>(aFrom.greenF() + (aTo.greenF() - aFrom.greenF()) * aAmount),
									static_cast<float>(aFrom.blueF() + (aTo.blueF() - aFrom.blueF()) * aAmount));
		}
		auto small(QLabel* aLabel) -> QLabel*
		{
			auto font = aLabel->font();
			font.setPointSizeF(font.pointSizeF() * 0.92);
			aLabel->setFont(font);
			return aLabel;
		}
		auto words(const QString& aText, QWidget* aParent) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			label->setWordWrap(true);
			label->setTextFormat(Qt::PlainText);
			label->setTextInteractionFlags(Qt::TextSelectableByMouse);
			return label;
		}
		auto quiet(QLabel* aLabel) -> QLabel*
		{
			aLabel->setForegroundRole(QPalette::PlaceholderText);
			return aLabel;
		}
		void clear(QVBoxLayout* aLayout)
		{
			while (auto* const item = aLayout->takeAt(0)) {
				if (auto* const widget = item->widget()) {
					widget->hide();
					widget->deleteLater();
				}
				if (auto* const inner = item->layout()) {
					clear(static_cast<QVBoxLayout*>(inner));
				}
				delete item;
			}
		}
		auto passage_heading(const core::tour_passage& aPassage) -> QString
		{
			switch (aPassage.Field) {
			case core::tour_passage_field::problem:  return QObject::tr("From the Problem");
			case core::tour_passage_field::solution: return QObject::tr("From the Solution");
			case core::tour_passage_field::stop:     return QObject::tr("From the stop's explanation");
			case core::tour_passage_field::stop_code:
				return aPassage.Path.empty() ? QObject::tr("The stop's code")
											 : QObject::tr("%1, lines %2 to %3").arg(QString::fromStdString(aPassage.Path)).arg(aPassage.FirstLine).arg(aPassage.LastLine);
			}
			return { };
		}
		/// Return sends; Shift+Return starts a new line.
		class send_keys final : public QObject
		{
		public:
			send_keys(QObject* aParent, std::function<void()> aSend)
				: QObject{ aParent }
				, Send{ std::move(aSend) }
			{
			}

		protected:
			bool eventFilter(QObject* aWatched, QEvent* aEvent) override
			{
				if (aEvent->type() == QEvent::KeyPress) {
					const auto* key = static_cast<QKeyEvent*>(aEvent);
					if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && !(key->modifiers() & Qt::ShiftModifier)) {
						Send();
						return true;
					}
				}
				return QObject::eventFilter(aWatched, aEvent);
			}

		private:
			std::function<void()> Send;
		};
		/// The author's words: a quiet rounded bubble, right-aligned.
		class bubble_frame final : public QWidget
		{
		public:
			using QWidget::QWidget;

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				painter.setPen(Qt::NoPen);
				painter.setBrush(mixed(palette().color(QPalette::Active, QPalette::Base), palette().color(QPalette::Active, QPalette::WindowText), 0.08));
				painter.drawRoundedRect(QRectF{ rect() }, 12.0, 12.0);
			}
		};
		/// A proposal waiting in the chat: the agent accent's rule down its
		/// left, as the proposal under Draft the tour is drawn.
		class proposal_frame final : public QWidget
		{
		public:
			using QWidget::QWidget;

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto       painter = QPainter{ this };
				const auto accent  = agent_accent(palette());
				painter.fillRect(rect(), mixed(palette().color(QPalette::Active, QPalette::Base), accent, 0.06));
				painter.fillRect(QRect{ 0, 0, 2, height() }, accent);
			}
		};
	}

	auto side_chat_icon(const QPalette& aPalette) -> QIcon
	{
		auto pixmap = QPixmap{ 32, 32 };
		pixmap.fill(Qt::transparent);
		auto painter = QPainter{ &pixmap };
		painter.setRenderHint(QPainter::Antialiasing, true);
		painter.setPen(QPen{ aPalette.color(QPalette::Active, QPalette::ButtonText), 2.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
		painter.setBrush(Qt::NoBrush);
		auto bubble = QPainterPath{ };
		bubble.addRoundedRect(QRectF{ 4.0, 5.0, 24.0, 17.0 }, 6.0, 6.0);
		painter.drawPath(bubble);
		auto tail = QPainterPath{ };
		tail.moveTo(10.0, 22.0);
		tail.lineTo(9.0, 28.0);
		tail.lineTo(16.0, 22.0);
		painter.drawPath(tail);
		painter.end();
		return QIcon{ pixmap };
	}

	namespace {
		/// The bubble inside a toggle's box: its rounded body and its tail.
		void draw_bubble(QPainter& aPainter, const QRectF& aGlyph, const QColor& aInk)
		{
			aPainter.setPen(QPen{ aInk, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
			aPainter.setBrush(Qt::NoBrush);
			const auto body = QRectF{ aGlyph.left() + 0.5, aGlyph.top() + 1.0, aGlyph.width() - 1.0, aGlyph.height() * 0.66 };
			aPainter.drawRoundedRect(body, 3.0, 3.0);
			auto tail = QPainterPath{ };
			tail.moveTo(body.left() + 3.0, body.bottom());
			tail.lineTo(body.left() + 2.5, aGlyph.bottom() - 0.5);
			tail.lineTo(body.left() + 6.5, body.bottom());
			aPainter.drawPath(tail);
		}
	}

	auto chat_toggle_size(const QFont& aFont, const std::size_t aTurns) -> QSize
	{
		const auto metrics = QFontMetrics{ aFont };
		const auto glyph   = 14;
		const auto figures = aTurns == 0U ? 0 : metrics.horizontalAdvance(QString::number(aTurns)) + 4;
		return { 8 + glyph + figures + 8, std::max(22, metrics.height() + 6) };
	}

	void paint_chat_toggle(QPainter& aPainter, const QRectF& aBox, const QPalette& aPalette, const std::size_t aTurns, const bool aOn, const bool aHovered)
	{
		aPainter.save();
		aPainter.setRenderHint(QPainter::Antialiasing, true);
		const auto ground = aPalette.color(QPalette::Active, QPalette::Window);
		const auto accent = agent_accent(aPalette);
		const auto pill   = aBox.adjusted(0.5, 0.5, -0.5, -0.5);
		const auto radius = pill.height() / 2.0;
		if (aOn) {
			aPainter.setPen(Qt::NoPen);
			aPainter.setBrush(accent);
			aPainter.drawRoundedRect(pill, radius, radius);
		} else {
			aPainter.setPen(QPen{ mixed(ground, accent, aHovered ? 0.85 : 0.55), 1.0 });
			aPainter.setBrush(mixed(ground, accent, aHovered ? 0.16 : 0.07));
			aPainter.drawRoundedRect(pill, radius, radius);
		}
		const auto ink   = aOn ? ground : accent;
		const auto glyph = QRectF{ pill.left() + 8.0, pill.center().y() - 7.0, 14.0, 14.0 };
		draw_bubble(aPainter, glyph, ink);
		if (aTurns != 0U) {
			auto font = aPainter.font();
			font.setWeight(QFont::DemiBold);
			aPainter.setFont(font);
			aPainter.setPen(ink);
			aPainter.drawText(QRectF{ glyph.right() + 4.0, pill.top(), pill.right() - glyph.right() - 4.0 - 6.0, pill.height() }, Qt::AlignLeft | Qt::AlignVCenter, QString::number(aTurns));
		}
		aPainter.restore();
	}

	part_chat_toggle::part_chat_toggle(const QString& aPart, QWidget* aParent)
		: QToolButton{ aParent }
		, Part{ aPart }
	{
		setCheckable(true);
		setAutoRaise(true);
		setFocusPolicy(Qt::StrongFocus);
		setCursor(Qt::PointingHandCursor);
		setAttribute(Qt::WA_Hover, true);
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		present(0U, false);
	}

	void part_chat_toggle::present(const std::size_t aTurns, const bool aOn)
	{
		const auto resized = Turns != aTurns;
		Turns              = aTurns;
		setChecked(aOn);
		setText(aTurns == 0U ? QString{ } : QString::number(aTurns));
		const auto turns = aTurns == 0U ? tr("no turns yet") : aTurns == 1U ? tr("one turn")
																			: tr("%1 turns").arg(aTurns);
		setAccessibleName(aOn ? tr("Close the chat about %1").arg(Part) : tr("Open the chat about %1").arg(Part));
		setAccessibleDescription(turns);
		setToolTip(aOn ? tr("Close the chat about %1 (%2). Every chat is kept.").arg(Part, turns) : tr("Open the chat about %1 beside the tour (%2)").arg(Part, turns));
		if (resized) {
			updateGeometry();
		}
		update();
	}

	auto part_chat_toggle::sizeHint() const -> QSize { return chat_toggle_size(font(), Turns); }

	void part_chat_toggle::paintEvent(QPaintEvent* /*aEvent*/)
	{
		auto painter = QPainter{ this };
		painter.setFont(font());
		const auto box = QRectF{ rect() }.adjusted(1.0, (height() - chat_toggle_size(font(), Turns).height()) / 2.0 + 1.0, -1.0, -(height() - chat_toggle_size(font(), Turns).height()) / 2.0 - 1.0);
		paint_chat_toggle(painter, box, palette(), Turns, isChecked(), underMouse() || isDown());
		if (hasFocus()) {
			painter.setRenderHint(QPainter::Antialiasing, true);
			painter.setPen(QPen{ palette().color(QPalette::Active, QPalette::Highlight), 2.0 });
			painter.setBrush(Qt::NoBrush);
			painter.drawRoundedRect(QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0), height() / 2.0 - 1.0, height() / 2.0 - 1.0);
		}
	}

	tour_side_chat::tour_side_chat(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("tourSideChat"));
		setAccessibleName(tr("Side chat with the tour agent"));
		setMinimumWidth(300);
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(10, 8, 10, 10);
		column->setSpacing(6);

		// [[BPR-1925]], owner 2026-11-07: "it's kind of weird that the
		// sidechat is so decoupled from the tour when it's really about the
		// tour". The chat is the Tour tab's second column, beside the part it
		// is about, which is marked in the tour; the part's own glyph chooses
		// the chat, so there are no tabs. The title row: the agent's glyph,
		// the part, Show, Copy chat and close.
		auto* const top = new QHBoxLayout{ };
		top->setSpacing(4);
		Glyph = new QLabel{ this };
		Glyph->setObjectName(QStringLiteral("tourSideChatGlyph"));
		top->addWidget(Glyph, 0, Qt::AlignTop);
		auto* const aboutText = new QVBoxLayout{ };
		aboutText->setSpacing(1);
		Subject = new QLabel{ this };
		Subject->setObjectName(QStringLiteral("tourSideChatSubject"));
		Subject->setTextFormat(Qt::PlainText);
		Subject->setWordWrap(true);
		auto heavy = Subject->font();
		heavy.setWeight(QFont::DemiBold);
		Subject->setFont(heavy);
		Quote = quiet(small(words({ }, this)));
		Quote->setObjectName(QStringLiteral("tourSideChatQuote"));
		aboutText->addWidget(Subject);
		aboutText->addWidget(Quote);
		top->addLayout(aboutText, 1);
		ShowPart = new quiet_push_button{ tr("Show"), this };
		ShowPart->setObjectName(QStringLiteral("tourSideChatShow"));
		ShowPart->setFlat(true);
		ShowPart->setFocusPolicy(Qt::TabFocus);
		top->addWidget(ShowPart, 0, Qt::AlignTop);
		CopyChat = new QToolButton{ this };
		CopyChat->setObjectName(QStringLiteral("tourSideChatCopy"));
		CopyChat->setAutoRaise(true);
		CopyChat->setToolTip(tr("Copy this chat as text"));
		CopyChat->setAccessibleName(tr("Copy chat"));
		ClosePane = new QToolButton{ this };
		ClosePane->setObjectName(QStringLiteral("tourSideChatClose"));
		ClosePane->setAutoRaise(true);
		ClosePane->setToolTip(tr("Close the chat. Every part's chat is kept; its glyph in the tour opens it again."));
		ClosePane->setAccessibleName(tr("Close the chat"));
		top->addWidget(CopyChat, 0, Qt::AlignTop);
		top->addWidget(ClosePane, 0, Qt::AlignTop);
		column->addLayout(top);

		// The chat.
		Scroll = new QScrollArea{ this };
		Scroll->setObjectName(QStringLiteral("tourSideChatScroll"));
		Scroll->setWidgetResizable(true);
		Scroll->setFrameShape(QFrame::NoFrame);
		Scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		Scroll->setFocusPolicy(Qt::NoFocus);
		auto* const body = new QWidget{ Scroll };
		Turns            = new QVBoxLayout{ body };
		Turns->setContentsMargins(0, 6, 6, 6);
		Turns->setSpacing(12);
		Scroll->setWidget(body);
		Scroll->viewport()->installEventFilter(this);
		// Scrolling back through the chat lets it stay where the author put it.
		connect(Scroll->verticalScrollBar(), &QScrollBar::sliderMoved, this, [this] { Stick = false; });
		column->addWidget(Scroll, 1);

		// The one action that changes the tour.
		Update = new quiet_push_button{ this };
		Update->setObjectName(QStringLiteral("tourSideChatUpdate"));
		Update->setFocusPolicy(Qt::TabFocus);
		column->addWidget(Update);
		Accented.emplace_back(Update);

		auto* const sessionRow = new QHBoxLayout{ };
		Sessions               = new quiet_tool_button{ this };
		Sessions->setText(tr("History"));
		Sessions->setObjectName(QStringLiteral("tourChatHistory"));
		SessionMenu = new QMenu{ Sessions };
		Sessions->setMenu(SessionMenu);
		Sessions->setPopupMode(QToolButton::InstantPopup);
		NewSession = new quiet_tool_button{ this };
		NewSession->setText(tr("New session"));
		NewSession->setObjectName(QStringLiteral("tourChatNewSession"));
		NewSession->setToolTip(tr("Keep this conversation in history and start fresh. Accepted tour work stays. Context sent earlier remains in the old conversation even after its selection is removed."));
		sessionRow->addWidget(Sessions);
		sessionRow->addStretch();
		sessionRow->addWidget(NewSession);
		column->addLayout(sessionRow);
		ContinuationMode = new QLabel{ tr("Native continuation"), this };
		ContinuationMode->setObjectName(QStringLiteral("tourContinuationMode"));
		ContinuationMode->setWordWrap(true);
		ContinuationMode->setTextFormat(Qt::PlainText);
		column->addWidget(ContinuationMode);
		RevisionStatus = new QLabel{ this };
		RevisionStatus->setObjectName(QStringLiteral("tourRevisionStatus"));
		RevisionStatus->setTextFormat(Qt::PlainText);
		RevisionStatus->setWordWrap(true);
		RevisionStatus->hide();
		column->addWidget(RevisionStatus);
		ContinueOld = new quiet_tool_button{ this };
		ContinueOld->setObjectName(QStringLiteral("tourContinueOldRevision"));
		ContinueOld->setText(tr("Continue old revision"));
		ContinueOld->setToolTip(tr("Keep this chat's saved revision and workspace. No question is sent until you press Send."));
		ContinueOld->hide();
		column->addWidget(ContinueOld, 0, Qt::AlignLeft);
		connect(ContinueOld, &QToolButton::clicked, this, [this] {
			if (!Running && !HistoryLoading && RevisionChoiceRequired && ContinuationDispatch) {
				if (const auto thread = current()) {
					ContinuationDispatch(*thread);
				}
			}
		});
		connect(SessionMenu, &QMenu::aboutToShow, this, [this] { if (auto thread=current(); thread && HistoryDispatch) { HistoryDispatch(*thread,std::nullopt); } });
		connect(NewSession, &QToolButton::clicked, this, [this] { if (!Running && !HistoryLoading) { if (auto thread=current(); thread && HistoryDispatch) { HistoryDispatch(*thread,std::string{}); } } });
		// The composer: the passage it quotes, the words, Send.
		PassageRow             = new QWidget{ this };
		auto* const passageRow = new QHBoxLayout{ PassageRow };
		passageRow->setContentsMargins(2, 0, 0, 0);
		passageRow->setSpacing(4);
		PassageLine = quiet(small(new QLabel{ PassageRow }));
		PassageLine->setObjectName(QStringLiteral("tourSideChatPassage"));
		PassageLine->setTextFormat(Qt::PlainText);
		PassageLine->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		ClearPassage = new QToolButton{ PassageRow };
		ClearPassage->setObjectName(QStringLiteral("tourSideChatPassageClear"));
		ClearPassage->setAutoRaise(true);
		ClearPassage->setToolTip(tr("Ask without this passage"));
		ClearPassage->setAccessibleName(tr("Ask without this passage"));
		passageRow->addWidget(PassageLine, 1);
		passageRow->addWidget(ClearPassage, 0);
		column->addWidget(PassageRow);
		auto* const compose = new QHBoxLayout{ };
		compose->setSpacing(6);
		Composer = new QPlainTextEdit{ this };
		Composer->setObjectName(QStringLiteral("tourSideChatComposer"));
		Composer->setAccessibleName(tr("Message to the tour agent"));
		Composer->setTabChangesFocus(true);
		Composer->document()->setDocumentMargin(6);
		Composer->setFixedHeight(3 * Composer->fontMetrics().lineSpacing() + 20);
		Composer->installEventFilter(new send_keys{ Composer, [this] { send(Composer->toPlainText().trimmed(), false); } });
		// [[BPR-1916]]. `/name` completion; installed after Return's filter,
		// so Return chooses a row while the list is open.
		SkillCompletion = new skill_completer{ Composer };
		Send            = new quiet_push_button{ this };
		Send->setObjectName(QStringLiteral("tourSideChatSend"));
		Send->setFocusPolicy(Qt::TabFocus);
		Send->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
		compose->addWidget(Composer, 1);
		compose->addWidget(Send, 0, Qt::AlignBottom);
		column->addLayout(compose);
		Accented.emplace_back(Send);

		connect(CopyChat, &QToolButton::clicked, this, [this] {
			if (auto* const clipboard = QGuiApplication::clipboard(); clipboard != nullptr) {
				clipboard->setText(chat_text());
			}
		});
		connect(ClosePane, &QToolButton::clicked, this, [this] { set_column_open(false); });
		connect(ShowPart, &QPushButton::clicked, this, [this] {
			if (const auto thread = current(); thread && RevealDispatch) {
				RevealDispatch(*thread);
			}
		});
		connect(Update, &QPushButton::clicked, this, [this] {
			if (const auto thread = current()) {
				send(tr("Update %1 from our chat.").arg(thread->Kind == core::tour_thread_kind::stop ? tr("this stop's explanation") : thread->Kind == core::tour_thread_kind::problem ? tr("the Problem")
															: thread->Kind == core::tour_thread_kind::solution                                                                         ? tr("the Solution")
																																													   : tr("the tour")),
					 true);
			}
		});
		connect(Send, &QPushButton::clicked, this, [this] {
			if (Mine) {
				if (StopDispatch) {
					StopDispatch();
				}
				return;
			}
			send(Composer->toPlainText().trimmed(), false);
		});
		connect(ClearPassage, &QToolButton::clicked, this, [this] {
			if (const auto thread = current()) {
				Passages.erase(static_cast<std::size_t>(index_of(*thread)));
			}
			show_passage();
			Composer->setFocus(Qt::OtherFocusReason);
		});
		connect(Composer, &QPlainTextEdit::textChanged, this, [this] { refresh_actions(); });
		rebuild();
	}

	void tour_side_chat::set_dispatch(ask_dispatch aAsk, std::function<void()> aStop, decide_dispatch aDecide)
	{
		AskDispatch    = std::move(aAsk);
		StopDispatch   = std::move(aStop);
		DecideDispatch = std::move(aDecide);
	}

	auto tour_side_chat::index_of(const core::tour_thread& aThread) const -> int
	{
		const auto found = std::ranges::find(Open, aThread);
		return found == Open.end() ? -1 : static_cast<int>(found - Open.begin());
	}

	auto tour_side_chat::current() const -> std::optional<core::tour_thread>
	{
		const auto index = Current;
		return index < 0 || static_cast<std::size_t>(index) >= Open.size() ? std::nullopt : std::optional{ Open[static_cast<std::size_t>(index)] };
	}

	auto tour_side_chat::passage() const -> std::optional<core::tour_passage>
	{
		const auto index = Current;
		const auto found = index < 0 ? Passages.end() : Passages.find(static_cast<std::size_t>(index));
		return found == Passages.end() ? std::nullopt : std::optional{ found->second };
	}

	void tour_side_chat::open(const core::tour_thread& aThread, std::optional<core::tour_passage> aPassage, const bool aFocus)
	{
		auto index = index_of(aThread);
		if (index < 0) {
			Open.push_back(aThread);
			index = static_cast<int>(Open.size()) - 1;
		}
		if (aPassage) {
			Passages[static_cast<std::size_t>(index)] = std::move(*aPassage);
		}
		select(index);
		set_column_open(true);
		if (aFocus) {
			Composer->setFocus(Qt::OtherFocusReason);
		}
	}

	void tour_side_chat::select(const int aIndex)
	{
		if (aIndex < 0 || static_cast<std::size_t>(aIndex) >= Open.size()) {
			return;
		}
		if (Current != aIndex) {
			Current = aIndex;
			Edited.clear();
			Editing.clear();
			Stick = true;
		}
		rebuild();
		changed();
		if (auto thread = current(); thread && HistoryDispatch) {
			HistoryDispatch(*thread, std::nullopt);
		}
	}

	void tour_side_chat::set_column_open(const bool aOpen)
	{
		const auto was = ColumnOpen;
		ColumnOpen     = aOpen && current().has_value();
		if (VisibilityDispatch && (was != ColumnOpen || ColumnOpen)) {
			VisibilityDispatch(ColumnOpen);
		}
		if (was != ColumnOpen) {
			changed();
		}
	}

	void tour_side_chat::changed()
	{
		if (ChangeObserver) {
			ChangeObserver();
		}
	}

	auto tour_side_chat::discussed() const -> std::optional<core::tour_thread>
	{
		return ColumnOpen ? current() : std::nullopt;
	}

	auto tour_side_chat::turns_about(const core::tour_thread& aThread) const -> std::size_t
	{
		return core::thread_runs(Session, aThread).size();
	}

	auto tour_side_chat::chat_parts() const -> std::size_t
	{
		return core::chat_threads(Session).size();
	}

	void tour_side_chat::close(const core::tour_thread& aThread)
	{
		const auto index = index_of(aThread);
		if (index < 0) {
			return;
		}
		// Passages are kept by tab place: those after this one move up.
		auto moved = std::map<std::size_t, core::tour_passage>{ };
		for (auto& [at, held] : Passages) {
			if (static_cast<int>(at) != index) {
				moved[static_cast<int>(at) > index ? at - 1U : at] = std::move(held);
			}
		}
		Passages = std::move(moved);
		Open.erase(Open.begin() + index);
		// The chat after it takes its place, or the one before at the end.
		// The chat in the column was this one: the column closes.
		const auto shown = Current == index;
		if (Current > index || Current >= static_cast<int>(Open.size())) {
			--Current;
		}
		Edited.clear();
		Editing.clear();
		if (shown || Open.empty()) {
			set_column_open(false);
		}
		rebuild();
		changed();
	}

	void tour_side_chat::present(const core::tour_agent_session& aSession, const std::optional<core::tour_proposal>& aProposal, std::shared_ptr<const core::review_tour> aDraft)
	{
		const auto same = Proposal && aProposal && Proposal->Run == aProposal->Run;
		if (!same) {
			Edited.clear();
			Editing.clear();
		}
		Session  = aSession;
		Proposal = aProposal;
		Draft    = std::move(aDraft);

		rebuild();
		changed();
	}

	void tour_side_chat::present_native_history(const application::native_agent_history_projection& aView)
	{
		if (aView.ReadOnly) {
			auto* dialog = new QDialog{ this };
			dialog->setAttribute(Qt::WA_DeleteOnClose);
			dialog->setWindowTitle(tr("Retained tour chat"));
			auto* column = new QVBoxLayout{ dialog };
			auto* text   = new QPlainTextEdit{ dialog };
			text->setReadOnly(true);
			auto body = tr("Retained history. This chat is detached from the current tour part. Viewing it sends no question.\n\n");
			for (const auto& turn : aView.Turns) {
				body += QString::fromStdString(turn.Asked) + QStringLiteral("\n\n") + QString::fromStdString(turn.Answered) + QStringLiteral("\n\n");
			}
			text->setPlainText(body);
			column->addWidget(text);
			dialog->resize(600, 480);
			dialog->show();
			return;
		}
		HistoryLoading = aView.Loading;
		Sessions->setProperty("loading", aView.Loading);
		NewSession->setEnabled(!Running && !HistoryLoading);
		if (!aView.Loading) {
			SessionMenu->clear();
			ContinuationMode->setText(aView.Replay ? tr("Transcript replay: saved questions and replies; prior tools and workspace changes are absent.") : aView.NativeSupported ? tr("Native continuation")
																																												 : tr("Native continuation unavailable. Choose Transcript replay or New session."));
			ContinuationMode->setToolTip(tr("Replay keeps at most the newest 12 answered turns within 48 KiB. Older or unanswered turns are omitted. The source history stays. Tour tool and network restrictions still apply."));
			ContinuationMode->setText(ContinuationMode->text() + tr(" Replay omits %1 retained turns and cuts %2 text bytes; earlier storage omissions are unknown.").arg(aView.ReplayTurnsOmitted).arg(aView.ReplayTextCutBytes));
			auto* replay = SessionMenu->addAction(tr("Use Transcript replay with selected agent"));
			replay->setObjectName(QStringLiteral("tourTranscriptReplay"));
			replay->setEnabled(!Running);
			replay->setToolTip(ContinuationMode->toolTip());
			if (!aView.ReplaySource.empty()) {
				ContinuationMode->setToolTip(ContinuationMode->toolTip() + tr(" Source history: %1").arg(QString::fromStdString(aView.ReplaySource)));
			}
			connect(replay, &QAction::triggered, this, [this, id = aView.Selected] {if (auto thread=current();thread && HistoryDispatch) {HistoryDispatch(*thread,"replay:"+id);} });
			SessionMenu->addSeparator();
			for (const auto& entry : aView.Entries) {
				auto* item = SessionMenu->addAction(tr("%1: %2 (%3 turns)").arg(entry.Replay ? tr("Transcript replay") : tr("Native"), QString::fromStdString(entry.Title).simplified().left(72)).arg(entry.Turns));
				item->setData(QString::fromStdString(entry.Id));
				item->setProperty("turns", QVariant::fromValue<qulonglong>(entry.Turns));
				item->setCheckable(true);
				item->setChecked(entry.Id == aView.Selected);
				item->setEnabled(!Running);
				connect(item, &QAction::triggered, this, [this, id = entry.Id] { if (auto thread=current(); thread && HistoryDispatch) { HistoryDispatch(*thread,id); } });
			}
			if (!aView.OtherChats.empty()) {
				auto* retained = SessionMenu->addMenu(tr("Other tour chats and detached history"));
				for (const auto& entry : aView.OtherChats) {
					auto* item = retained->addAction(([&] { const auto stop=entry.Scope.rfind(':'); const auto kind=stop==std::string::npos ? std::string::npos : entry.Scope.rfind(':',stop-1U); const auto part=kind==std::string::npos ? std::string{} : entry.Scope.substr(kind+1U,stop-kind-1U); return part=="1" ? tr("Problem") : part=="2" ? tr("Solution") : part=="3" ? tr("Stop ID %1").arg(QString::fromStdString(entry.Scope.substr(stop+1U))) : tr("Whole tour"); }()) + QStringLiteral(": ") + QString::fromStdString(entry.Title).simplified().left(60));
					connect(item, &QAction::triggered, this, [this, id = entry.Id] { if (auto thread=current(); thread && HistoryDispatch) { HistoryDispatch(*thread,"view:"+id); } });
				}
			}
		}
		refresh_actions();
	}

	void tour_side_chat::present_revision_continuation(std::optional<core::agent_continuation_offer> aOffer, const core::git_commit_oid aObservedHead, const bool aContinued)
	{
		const auto stale       = aOffer && aOffer->Required;
		RevisionChoiceRequired = stale && !aContinued;
		ContinueOld->setVisible(RevisionChoiceRequired);
		RevisionStatus->setVisible(stale);
		if (stale) {
			RevisionStatus->setText((aContinued ? tr("Continuing saved revision %1. Current head: %2.") : tr("Head changed. This chat reads %1; current head: %2. Choose New session or Continue old revision."))
										.arg(QString::fromStdString(std::string{ core::canonical_hex(aOffer->ContinueAgainst.HeadOid).view() }).left(12), QString::fromStdString(std::string{ core::canonical_hex(aObservedHead).view() }).left(12)));
		}
		refresh_actions();
	}

	void tour_side_chat::set_draft(std::shared_ptr<const core::review_tour> aDraft)
	{
		if (Draft == aDraft) {
			return;
		}
		present(Session, Proposal, std::move(aDraft));
	}

	void tour_side_chat::set_running(const bool aAny, const bool aDrafting)
	{
		const auto wasDrafting = Drafting;
		Running                = aAny;
		Drafting               = aAny && aDrafting;
		// [[BPR-1941]]. The run's time counts from when it was asked for, as
		// the Agent tab's does; it ends with the run.
		if (aAny && !LiveClock.isValid()) {
			LiveClock.start();
		} else if (!aAny) {
			LiveClock.invalidate();
			Live.reset();
		}
		if (wasDrafting != Drafting) {
			rebuild();
			return;
		}
		refresh_actions();
	}

	void tour_side_chat::present_run_status(std::optional<agent_run_status> aStatus)
	{
		if (aStatus && !LiveClock.isValid()) {
			LiveClock.start();
		}
		Live = std::move(aStatus);
		if (!Live) {
			return;
		}
		if (const auto running = core::running_tour_run(Session)) {
			RunAgents[running->value()] = Live->Agent;
		}
		for (auto* const line : { LiveLine.data(), DraftLine }) {
			if (line != nullptr && line->live()) {
				line->present(*Live, LiveClock);
			}
		}
	}

	auto tour_side_chat::live_status() const -> agent_run_status
	{
		if (Live) {
			return *Live;
		}
		auto status  = agent_run_status{ };
		status.Agent = tr("The tour agent");
		status.Stage = agent_run_stage::thinking;
		return status;
	}

	auto tour_side_chat::agent_of(const core::tour_agent_run_id aRun) const -> QString
	{
		const auto found = RunAgents.find(aRun.value());
		return found == RunAgents.end() || found->second.isEmpty() ? tr("The tour agent") : found->second;
	}

	/// [[BPR-1930]], owner: "particularly during the initial agent draft".
	/// While Draft the tour runs, the tour's own chat still takes feedback.
	auto tour_side_chat::takes_feedback() const -> bool
	{
		const auto thread = current();
		return Drafting && !Mine && thread && thread->Kind == core::tour_thread_kind::tour;
	}

	void tour_side_chat::set_available(const bool aReady, const QString& aWhy)
	{
		Ready = aReady;
		Why   = aWhy;
		refresh_actions();
	}

	void tour_side_chat::set_prompt_source(prompt_source aSource)
	{
		PromptSource = std::move(aSource);
		rebuild();
	}

	auto tour_side_chat::prompts_usable() const -> bool { return Ready && !HistoryLoading && current() && (!Draft || core::active_tour_thread(*Draft, *current())) && !Mine && (!Running || takes_feedback()); }

	void tour_side_chat::choose_prompt(const QString& aPrompt, const bool aEditFirst)
	{
		const auto thread = current();
		if (!thread || !PromptSource) {
			return;
		}
		// A prompt the reviewer finishes ("Focus on …") is always put in the
		// composer, with its words there to be completed.
		auto text   = aPrompt;
		auto finish = false;
		// [[BPR-1934]] v002. Explain the selected block: asked in the
		// selected part's chat, opened if it is not the one shown, with
		// exactly the selected text quoted.
		if (Selection && !SelectionLabel.isEmpty() && aPrompt == SelectionLabel) {
			const auto chosen = *Selection;
			for (const auto& prompt : PromptSource(chosen.Thread.Kind)) {
				if (prompt.Selection) {
					text = QString::fromUtf8(prompt.Text.data(), static_cast<qsizetype>(prompt.Text.size()));
				}
			}
			if (chosen.Thread != *thread) {
				open(chosen.Thread, chosen.Passage, false);
			} else {
				Passages[static_cast<std::size_t>(Current)] = chosen.Passage;
				show_passage();
			}
		}
		for (const auto& prompt : PromptSource(thread->Kind)) {
			const auto written = QString::fromUtf8(prompt.Text.data(), static_cast<qsizetype>(prompt.Text.size()));
			if (prompt.Finish && aPrompt == written + ellipsis && text == aPrompt) {
				text   = written;
				finish = true;
			}
		}
		const auto draft = Composer->toPlainText();
		const auto press = core::decide_agent_suggestion_press({
			.EditFirst = aEditFirst || finish,
			.DraftHeld = !draft.trimmed().isEmpty(),
			.Usable    = prompts_usable(),
		});
		const auto toEnd = [this] {
			auto cursor = Composer->textCursor();
			cursor.movePosition(QTextCursor::End);
			Composer->setTextCursor(cursor);
		};
		switch (press) {
		case core::agent_suggestion_press::ignored: return;
		case core::agent_suggestion_press::send:    send(text, false); return;
		case core::agent_suggestion_press::send_keeping_draft:
			// Sent alone, as if typed and sent; the reviewer's words stay.
			send(text, false);
			Composer->setPlainText(draft);
			toEnd();
			return;
		case core::agent_suggestion_press::edit: Composer->setPlainText(text); break;
		case core::agent_suggestion_press::edit_after_draft:
			toEnd();
			Composer->textCursor().insertText((draft.endsWith(QLatin1Char('\n')) ? QString{ } : QStringLiteral("\n")) + text);
			break;
		}
		Composer->setFocus(Qt::OtherFocusReason);
		toEnd();
	}

	void tour_side_chat::set_code_style(const QFont& aFont, const qreal aTabStopDistance, std::optional<markdown_code_theme> aTheme, std::function<void(std::vector<application::markdown_code_syntax_request>)> aDispatch)
	{
		CodeFont     = aFont;
		TabStop      = aTabStopDistance;
		CodeTheme    = std::move(aTheme);
		CodeDispatch = std::move(aDispatch);
		for (const auto& each : Readers) {
			if (each) {
				each->set_code_font(CodeFont, TabStop);
				if (CodeTheme) {
					each->set_code_theme(*CodeTheme);
				}
				if (CodeDispatch) {
					each->set_code_syntax_dispatch(CodeDispatch);
					each->request_code_syntax();
				}
			}
		}
		fit();
	}

	void tour_side_chat::apply_code_syntax(const application::markdown_code_syntax_publication& aSpans)
	{
		for (const auto& each : Readers) {
			if (each) {
				each->apply_code_syntax(aSpans);
			}
		}
	}

	void tour_side_chat::restyle(const QPalette& aGround)
	{
		for (const auto& widget : Accented) {
			if (widget) {
				apply_agent_accent(*widget, aGround);
			}
		}
		// [[BPR-1925]]. The agent's glyph leads the title, in its accent.
		auto accented = palette();
		accented.setColor(QPalette::Active, QPalette::ButtonText, agent_accent(aGround));
		const auto side = Glyph->fontMetrics().height();
		Glyph->setPixmap(side_chat_icon(accented).pixmap(QSize{ side, side }, devicePixelRatioF()));
		CopyChat->setIcon(drawer_icon(drawer_glyph::copy, palette()));
		ClosePane->setIcon(drawer_icon(drawer_glyph::close, palette()));
		ClearPassage->setIcon(drawer_icon(drawer_glyph::close, palette()));
		Update->setIcon(drawer_icon(drawer_glyph::agent, Update->palette()));
		Send->setIcon(drawer_icon(Mine ? drawer_glyph::close : drawer_glyph::send, Send->palette()));
		for (auto* const copy : Copies) {
			if (copy != nullptr) {
				copy->setIcon(drawer_icon(drawer_glyph::copy, palette()));
			}
		}
		update();
	}

	void tour_side_chat::present_skills(std::vector<core::skill_entry> aSkills)
	{
		if (SkillCompletion != nullptr) {
			SkillCompletion->present(std::move(aSkills));
		}
	}

	auto tour_side_chat::bubble(const std::size_t aTurn) const -> QWidget* { return aTurn < Bubbles.size() ? Bubbles[aTurn] : nullptr; }
	auto tour_side_chat::reply(const std::size_t aTurn) const -> markdown_body_view* { return aTurn < Replies.size() ? Replies[aTurn] : nullptr; }
	auto tour_side_chat::status_line(const std::size_t aTurn) const -> agent_run_status_line* { return aTurn < Endings.size() ? Endings[aTurn] : nullptr; }
	auto tour_side_chat::copy_reply_button(const std::size_t aTurn) const -> QToolButton* { return aTurn < Copies.size() ? Copies[aTurn] : nullptr; }
	auto tour_side_chat::resend_button(const std::size_t aTurn) const -> QPushButton* { return aTurn < Resends.size() ? Resends[aTurn] : nullptr; }
	auto tour_side_chat::delete_button(const std::size_t aTurn) const -> QPushButton* { return aTurn < Deletes.size() ? Deletes[aTurn] : nullptr; }

	auto tour_side_chat::accept_button(const std::size_t aIndex) const -> QPushButton*
	{
		const auto found = Accepts.find(aIndex);
		return found == Accepts.end() ? nullptr : found->second;
	}

	auto tour_side_chat::proposal_reader(const std::size_t aIndex) const -> markdown_body_view*
	{
		const auto found = ProposalReaders.find(aIndex);
		return found == ProposalReaders.end() ? nullptr : found->second;
	}

	auto tour_side_chat::reply_text(const core::tour_agent_run& aRun) -> QString { return QString::fromStdString(aRun.Reply); }

	auto tour_side_chat::chat_text() const -> QString
	{
		const auto thread = current();
		if (!thread) {
			return { };
		}
		auto lines = QStringList{ tr("## Side chat: %1").arg(name_of(*thread)), QString{ } };
		for (const auto* run : core::thread_runs(Session, *thread)) {
			if (run->Passage) {
				lines << QStringLiteral("> ") + passage_heading(*run->Passage) + QLatin1Char(':');
				for (const auto& line : QString::fromStdString(run->Passage->Text).split(QLatin1Char('\n'))) {
					lines << QStringLiteral("> ") + line;
				}
				lines << QString{ };
			}
			lines << tr("**You:** %1").arg(QString::fromStdString(run->Question)) << QString{ };
			if (run->State == core::tour_agent_run_state::answered) {
				lines << (run->Seconds ? tr("**%1** (answered in %2):").arg(agent_of(run->Id), agent_elapsed_words(*run->Seconds)) : tr("**%1:**").arg(agent_of(run->Id))) << QString{ } << reply_text(*run) << QString{ };
			} else {
				const auto ended = std::ranges::find(run->Entries, core::tour_transcript_kind::ended, &core::tour_transcript_entry::Kind);
				lines << tr("**%1** did not answer: %2").arg(agent_of(run->Id), ended == run->Entries.end() ? QString{ } : QString::fromStdString(ended->Text).section(QLatin1Char('\n'), 0, 0)) << QString{ };
			}
		}
		return lines.join(QLatin1Char('\n'));
	}

	void tour_side_chat::paintEvent(QPaintEvent* /*aEvent*/)
	{
		if (isWindow()) {
			return;
		}
		auto       painter = QPainter{ this };
		const auto ground  = palette().color(QPalette::Active, QPalette::Window);
		const auto accent  = agent_accent(palette());
		auto       wash    = QLinearGradient{ 0.0, 0.0, static_cast<qreal>(std::min(width(), 160)), 0.0 };
		wash.setColorAt(0.0, mixed(ground, accent, 0.06));
		wash.setColorAt(1.0, ground);
		painter.fillRect(QRect{ 0, 0, std::min(width(), 160), height() }, wash);
		painter.fillRect(QRect{ 0, 0, 1, height() }, mixed(ground, accent, 0.5));
	}

	bool tour_side_chat::eventFilter(QObject* aWatched, QEvent* aEvent)
	{
		if (aEvent->type() == QEvent::Resize && aWatched == Scroll->viewport()) {
			QTimer::singleShot(0, this, [this] { fit(); });
		}
		return QWidget::eventFilter(aWatched, aEvent);
	}

	auto tour_side_chat::name_of(const core::tour_thread& aThread) const -> QString
	{
		switch (aThread.Kind) {
		case core::tour_thread_kind::tour:     return tr("Tour");
		case core::tour_thread_kind::problem:  return tr("Problem");
		case core::tour_thread_kind::solution: return tr("Solution");
		case core::tour_thread_kind::stop:     {
			if (Draft) {
				const auto found = std::ranges::find(Draft->Stops, aThread.Stop, &core::tour_stop::Id);
				if (found != Draft->Stops.end()) {
					return tr("Stop %1").arg(static_cast<std::size_t>(found - Draft->Stops.begin()) + 1U);
				}
			}
			return tr("Removed stop %1").arg(aThread.Stop.value());
		}
		}
		return { };
	}

	void tour_side_chat::refresh_header()
	{
		const auto thread = current();
		Subject->clear();
		Quote->clear();
		ShowPart->setVisible(thread.has_value() && thread->Kind != core::tour_thread_kind::tour);
		if (!thread) {
			return;
		}
		auto quote = QString{ };
		switch (thread->Kind) {
		case core::tour_thread_kind::tour:
			Subject->setText(tr("The whole tour"));
			break;
		case core::tour_thread_kind::problem:
			Subject->setText(tr("Problem"));
			quote = Draft ? QString::fromStdString(Draft->Problem) : QString{ };
			ShowPart->setToolTip(tr("Show the Problem in the Tour tab"));
			break;
		case core::tour_thread_kind::solution:
			Subject->setText(tr("Solution"));
			quote = Draft ? QString::fromStdString(Draft->Solution) : QString{ };
			ShowPart->setToolTip(tr("Show the Solution in the Tour tab"));
			break;
		case core::tour_thread_kind::stop: {
			const core::tour_stop* stop = nullptr;
			if (Draft) {
				if (const auto found = std::ranges::find(Draft->Stops, thread->Stop, &core::tour_stop::Id); found != Draft->Stops.end()) {
					stop = &*found;
				}
			}
			if (stop != nullptr) {
				Subject->setText(tr("%1, %2").arg(name_of(*thread), QString::fromStdString(stop->Title)));
				if (const auto& source = stop->Mapped ? stop->Mapped : stop->Original; source) {
					Quote->setText(source->First.Line == source->Last.Line ? tr("%1, line %2").arg(QString::fromStdString(source->Path)).arg(source->First.Line + 1U)
																		   : tr("%1, lines %2 to %3").arg(QString::fromStdString(source->Path)).arg(source->First.Line + 1U).arg(source->Last.Line + 1U));
				} else if (stop->Destination) {
					// A stop the agent proposed, not yet confirmed in the code.
					Quote->setText(tr("%1, lines %2 to %3").arg(QString::fromStdString(stop->Destination->Path)).arg(stop->Destination->FirstLine + 1U).arg(stop->Destination->LastLine + 1U));
				}
			} else {
				Subject->setText(tr("A stop"));
			}
			ShowPart->setText(tr("Show in code"));
			ShowPart->setToolTip(tr("Open the stop in the Tour tab and show its lines in the code"));
			return;
		}
		}
		ShowPart->setText(tr("Show"));
		quote = quote.simplified();
		if (!quote.isEmpty()) {
			if (quote.size() > 140) {
				quote = quote.left(138).trimmed() + ellipsis;
			}
			Quote->setText(open_quote + quote + close_quote);
		}
	}

	void tour_side_chat::show_passage()
	{
		const auto held = passage();
		PassageRow->setVisible(held.has_value());
		if (!held) {
			PassageLine->clear();
			return;
		}
		const auto said = held->Field == core::tour_passage_field::stop_code ? tr("Asking about %1").arg(passage_heading(*held))
																			 : tr("Asking about %1%2%3").arg(open_quote).arg(QString::fromStdString(held->Text).simplified()).arg(close_quote);
		PassageLine->setText(PassageLine->fontMetrics().elidedText(said, Qt::ElideRight, std::max(160, Scroll->viewport()->width() - 40)));
		PassageLine->setToolTip(QString::fromStdString(held->Text));
	}

	void tour_side_chat::refresh_actions()
	{
		const auto thread   = current();
		const auto answered = thread && std::ranges::any_of(core::thread_runs(Session, *thread), [](const core::tour_agent_run* aRun) { return aRun->State == core::tour_agent_run_state::answered; });
		const auto blank    = Composer->toPlainText().trimmed().isEmpty();
		const auto active   = thread && (!Draft || core::active_tour_thread(*Draft, *thread));
		NewSession->setEnabled(active && !Running && !HistoryLoading);
		Send->setText(Mine ? tr("Stop") : QString{ });
		Send->setToolTip(!Ready ? Why : Mine ? tr("Stop the tour agent; nothing in the tour changes.")
											 : tr("Send (Return). Shift+Return starts a new line."));
		Send->setAccessibleName(Mine ? tr("Stop") : tr("Send"));
		ContinueOld->setEnabled(active && !Running && !HistoryLoading && Ready);
		Send->setEnabled(active && !HistoryLoading && Ready && thread.has_value() && (Mine || (!RevisionChoiceRequired && (!Running || takes_feedback()) && !blank)));
		Send->setIcon(drawer_icon(Mine ? drawer_glyph::close : drawer_glyph::send, Send->palette()));
		const auto part = !thread                              ? QString{ }
			: thread->Kind == core::tour_thread_kind::stop     ? tr("this stop")
			: thread->Kind == core::tour_thread_kind::problem  ? tr("the Problem")
			: thread->Kind == core::tour_thread_kind::solution ? tr("the Solution")
															   : tr("the tour");
		Update->setText(tr("Update %1 from this chat").arg(part));
		Update->setEnabled(active && !RevisionChoiceRequired && !HistoryLoading && Ready && thread.has_value() && answered && !Running);
		Update->setToolTip(!Ready          ? Why
							   : !answered ? tr("Ask something first. Then this asks the agent to rewrite %1 from the chat; you accept, edit or discard what it proposes.").arg(part)
							   : thread->Kind == core::tour_thread_kind::tour
							   // [[BPR-1937]]. The whole tour's update takes its shape too.
							   ? tr("Asks the agent for every change this chat agreed on, merges, removals and a new order included. It waits in the tour, marked grey; nothing changes until you accept it.")
							   : tr("Asks the agent to rewrite %1 from this chat. It comes back as a proposal here; nothing changes until you accept it.").arg(part));
		// [[BPR-1930]]. The tour's chat is for feedback on its shape too.
		Composer->setPlaceholderText(!thread                                            ? QString{ }
										 : takes_feedback()                             ? tr("Feedback for the draft. It goes to the agent when the draft is ready")
										 : thread->Kind == core::tour_thread_kind::tour ? tr("Ask about the tour, or say how to change it: fewer stops, merge two, focus on a part")
																						: tr("Ask about %1").arg(part));
		Composer->setEnabled(thread.has_value());
		refresh_prompts();
	}

	void tour_side_chat::offer_selection(const core::tour_thread& aThread, std::optional<core::tour_passage> aSelected)
	{
		if (aSelected && !aSelected->Text.empty()) {
			Selection.emplace();
			Selection->Thread  = aThread;
			Selection->Passage = std::move(*aSelected);
		} else if (Selection && Selection->Thread.Kind == aThread.Kind) {
			// A stop's text is one editor for every stop: going to another
			// stop ends what was selected in the last.
			Selection.reset();
		}
		refresh_prompts();
	}

	auto tour_side_chat::prompt_labels() -> QStringList
	{
		auto       labels = QStringList{ };
		const auto thread = current();
		SelectionLabel.clear();
		if (!thread || !PromptSource) {
			return labels;
		}
		const auto text = [](const application::tour_chat_prompt& aPrompt) { return QString::fromUtf8(aPrompt.Text.data(), static_cast<qsizetype>(aPrompt.Text.size())); };
		// [[BPR-1934]] v002. Explain the selected block leads while text is
		// selected in a part, whatever the chat holds already; for another
		// part than this chat's, it names the part.
		if (Selection) {
			for (const auto& prompt : PromptSource(Selection->Thread.Kind)) {
				if (prompt.Selection) {
					const auto part = Selection->Thread.Kind == core::tour_thread_kind::problem ? tr("the Problem")
						: Selection->Thread.Kind == core::tour_thread_kind::solution            ? tr("the Solution")
																								: name_of(Selection->Thread);
					SelectionLabel  = Selection->Thread == *thread ? text(prompt) : tr("%1 in %2").arg(text(prompt), part);
					labels << SelectionLabel;
					break;
				}
			}
		}
		// The rest are for an empty chat, and go with its first turn.
		const auto empty = core::thread_runs(Session, *thread).empty() && !(thread->Kind == core::tour_thread_kind::tour && Session.Waiting);
		if (empty) {
			for (const auto& prompt : PromptSource(thread->Kind)) {
				if (!prompt.Selection) {
					labels << (prompt.Finish ? text(prompt) + ellipsis : text(prompt));
				}
			}
		}
		return labels;
	}

	void tour_side_chat::refresh_prompts()
	{
		if (Suggestions == nullptr) {
			return;
		}
		// [[BPR-1934]]. The suggested prompts, only once the agent is set up
		// ([[BPR-1881]]); usable whenever a question could be sent.
		const auto labels = prompt_labels();
		const auto thread = current();
		Suggestions->present(labels);
		Suggestions->lead()->setVisible(thread && core::thread_runs(Session, *thread).empty());
		Suggestions->setVisible(Ready && !labels.isEmpty());
		Suggestions->set_usable(prompts_usable());
	}

	void tour_side_chat::send(const QString& aQuestion, const bool aUpdate)
	{
		const auto thread = current();
		if (!thread || aQuestion.isEmpty() || (Running && !(takes_feedback() && !aUpdate)) || HistoryLoading || (Draft && !core::active_tour_thread(*Draft, *thread)) || !Ready || !AskDispatch) {
			return;
		}
		auto ask = tour_ask{ .Thread = *thread, .Passage = aUpdate ? std::nullopt : passage(), .Question = aQuestion, .Update = aUpdate };
		AskDispatch(ask);
		if (!aUpdate) {
			Composer->clear();
			Passages.erase(static_cast<std::size_t>(Current));
		}
		show_passage();
	}

	auto tour_side_chat::inline_selector() const -> std::string
	{
		const auto thread = current();
		if (!thread || !Draft) {
			return { };
		}
		const auto pathOf = [](const core::tour_stop& aStop) -> std::string {
			if (const auto& source = aStop.Mapped ? aStop.Mapped : aStop.Original; source) {
				return source->Path;
			}
			return aStop.Destination ? aStop.Destination->Path : std::string{ };
		};
		if (thread->Kind == core::tour_thread_kind::stop) {
			const auto found = std::ranges::find(Draft->Stops, thread->Stop, &core::tour_stop::Id);
			if (found != Draft->Stops.end()) {
				return pathOf(*found);
			}
		}
		// The language most of the tour's stops are in, the first such stop
		// breaking a tie, named as a file of that kind.
		auto counts = std::map<std::string, std::size_t>{ };
		auto first  = std::vector<std::string>{ };
		for (const auto& stop : Draft->Stops) {
			const auto path = pathOf(stop);
			const auto dot  = path.rfind('.');
			if (dot == std::string::npos || path.find('/', dot) != std::string::npos) {
				continue;
			}
			const auto kind = path.substr(dot);
			if (counts[kind]++ == 0U) {
				first.push_back(kind);
			}
		}
		auto best = std::string{ };
		for (const auto& kind : first) {
			if (best.empty() || counts[kind] > counts[best]) {
				best = kind;
			}
		}
		return best.empty() ? std::string{ } : "code" + best;
	}

	auto tour_side_chat::reader(QWidget* aParent, const std::string& aBody, const std::string& aIdentity, const std::string& aInlineSelector, const bool aFramed) -> markdown_body_view*
	{
		auto* const view = new markdown_body_view{ aParent };
		view->set_in_place(true);
		// [[BPR-1944]]. A reply's code is a box as wide as the chat, its long
		// lines scrolling inside it, with Wrap and Copy. A proposal keeps the
		// look its stop will have once published, as the stop reader shows it.
		view->set_code_block_tools(aFramed);
		// [[BPR-1914]] v004, owner look v003: "it seems like inline code isnt
		// being highlighted only blocks". Inline code reads as the part's
		// code does; with no language it stays plain monospace on its wash.
		view->set_inline_code_selector(aInlineSelector);
		view->setFocusPolicy(Qt::NoFocus);
		if (CodeFont != QFont{ }) {
			view->set_code_font(CodeFont, TabStop);
		}
		if (CodeTheme) {
			view->set_code_theme(*CodeTheme);
		}
		if (CodeDispatch) {
			view->set_code_syntax_dispatch(CodeDispatch);
		}
		view->set_height_observer([this] { QTimer::singleShot(0, this, [this] { fit(); }); });
		// [[BPR-1925]] v002, owner on v001: the chat's code was plain again.
		// Setting the body asks for its colours, and a text the host has
		// classified before is answered inside that request; the reader is
		// already among those an answer reaches, or that answer misses it.
		Readers.emplace_back(view);
		view->set_body(aBody, aIdentity);
		return view;
	}

	void tour_side_chat::rebuild()
	{
		clear(Turns);
		Suggestions = nullptr;
		Bubbles.clear();
		Replies.clear();
		Endings.clear();
		Copies.clear();
		Resends.clear();
		Deletes.clear();
		LiveLine  = nullptr;
		DraftLine = nullptr;
		Accepts.clear();
		ProposalReaders.clear();
		Readers.erase(std::remove_if(Readers.begin(), Readers.end(), [](const QPointer<markdown_body_view>& aView) { return aView.isNull(); }), Readers.end());
		Accented.erase(std::remove_if(Accented.begin(), Accented.end(), [](const QPointer<QWidget>& aWidget) { return aWidget.isNull(); }), Accented.end());
		refresh_header();
		show_passage();
		Mine                 = false;
		auto* const body     = Scroll->widget();
		const auto  thread   = current();
		const auto  runs     = thread ? core::thread_runs(Session, *thread) : std::vector<const core::tour_agent_run*>{ };
		const auto  mono     = CodeFont != QFont{ } ? CodeFont : QFontDatabase::systemFont(QFontDatabase::FixedFont);
		const auto  selector = inline_selector();
		WaitingBubble        = nullptr;
		InTour               = nullptr;
		RefusedLine          = nullptr;
		UnchangedLine        = nullptr;
		FocusLine            = nullptr;
		const auto wholeTour = thread && thread->Kind == core::tour_thread_kind::tour;
		if (runs.empty() && !(wholeTour && Session.Waiting)) {
			// [[BPR-1930]]. The tour's chat takes feedback on the tour's shape.
			const auto  invite = !thread ? QString{ }
				: wholeTour              ? tr("Give feedback on the whole tour: too many or too few stops, two to merge, one to add or split, a better order, or a part of the change to focus on. "
											  "The changes wait in the tour, marked grey, until you accept them. This works while the draft runs, too.")
										 : tr("Ask anything about this part: what a passage means, whether the code supports it, what a reviewer should notice. When the chat has what you want, Update rewrites the part from it.");
			auto* const empty  = quiet(small(words(invite, body)));
			empty->setObjectName(QStringLiteral("tourSideChatEmpty"));
			Turns->addWidget(empty);
		}
		for (const auto* run : runs) {
			// The author's words, on the right.
			auto* const mineRow = new QHBoxLayout{ };
			mineRow->addStretch(1);
			auto* const bubble = new bubble_frame{ body };
			bubble->setObjectName(QStringLiteral("tourSideChatBubble"));
			auto* const inside = new QVBoxLayout{ bubble };
			inside->setContentsMargins(12, 8, 12, 8);
			inside->setSpacing(3);
			if (run->Passage) {
				const auto code   = run->Passage->Field == core::tour_passage_field::stop_code;
				auto       quoted = QString::fromStdString(run->Passage->Text);
				const auto named  = code && quoted.toStdString() == run->Passage->Path + ":" + std::to_string(run->Passage->FirstLine) + "-" + std::to_string(run->Passage->LastLine);
				inside->addWidget(quiet(small(words(passage_heading(*run->Passage), bubble))));
				if (!named) {
					if (code) {
						auto kept = quoted.split(QLatin1Char('\n'));
						if (kept.size() > 6) {
							kept = kept.mid(0, 6);
							kept << ellipsis;
						}
						auto* const lines = quiet(small(words(kept.join(QLatin1Char('\n')), bubble)));
						auto        face  = mono;
						face.setPointSizeF(lines->font().pointSizeF());
						lines->setFont(face);
						lines->setWordWrap(false);
						inside->addWidget(lines);
					} else {
						auto* const said = quiet(small(words(open_quote + quoted.simplified() + close_quote, bubble)));
						auto        face = said->font();
						face.setItalic(true);
						said->setFont(face);
						inside->addWidget(said);
					}
				}
			}
			auto* const asked = words(QString::fromStdString(run->Question), bubble);
			asked->setObjectName(QStringLiteral("tourSideChatQuestion"));
			inside->addWidget(asked);
			mineRow->addWidget(bubble);
			Turns->addLayout(mineRow);
			Bubbles.push_back(bubble);

			// The agent's answer, on the left: the reply, then the Agent tab's
			// ending line ([[BPR-1941]]): live while it runs, then who answered
			// and how long it took, and Copy; or, for a turn that ended
			// without an answer, why, with Resend and Delete.
			auto* const answer = new QWidget{ body };
			auto* const column = new QVBoxLayout{ answer };
			column->setContentsMargins(0, 0, 0, 0);
			column->setSpacing(4);
			const auto  id      = run->Id.value();
			const auto  running = run->State == core::tour_agent_run_state::running;
			auto* const ending  = new agent_run_status_line{ answer };
			ending->setObjectName(QStringLiteral("tourSideChatStatus"));
			small(ending);
			Endings.push_back(ending);
			auto* said = static_cast<markdown_body_view*>(nullptr);
			if (running) {
				Mine = true;
				if (Live) {
					RunAgents[id] = Live->Agent;
				}
				ending->present(live_status(), LiveClock);
				LiveLine = ending;
				column->addWidget(ending);
			} else {
				auto failure = QString{ };
				if (run->State == core::tour_agent_run_state::answered) {
					said = reader(answer, run->Reply, "tour-side-chat:" + std::to_string(id), selector, true);
					said->setObjectName(QStringLiteral("tourSideChatReply"));
					column->addWidget(said);
				} else {
					const auto ended = std::ranges::find(run->Entries, core::tour_transcript_kind::ended, &core::tour_transcript_entry::Kind);
					failure          = ended == run->Entries.end() ? QString{ } : QString::fromStdString(ended->Text).trimmed();
					const auto  why  = failure.section(QLatin1Char('\n'), 0, 0);
					auto* const line = words(run->State == core::tour_agent_run_state::cancelled ? tr("Stopped. Nothing in the tour changed.") : why.isEmpty() ? tr("It didn't finish.")
																																							   : tr("Didn't finish: %1").arg(why),
											 answer);
					line->setObjectName(QStringLiteral("tourSideChatFailure"));
					column->addWidget(line);
				}
				// The core decides an ended turn's actions, as it does for the
				// Agent tab's turns.
				const auto answered = run->State == core::tour_agent_run_state::answered;
				const auto outcome  = answered                            ? core::agent_turn_outcome::answered
					: run->State == core::tour_agent_run_state::cancelled ? core::agent_turn_outcome::stopped
																		  : core::agent_turn_outcome::exited_with_error;
				const auto actions  = core::decide_agent_turn_actions({
					.Outcome = outcome,
					.HasText = answered ? !run->Reply.empty() : !failure.isEmpty(),
					.Latest  = run == runs.back(),
					.Working = Running,
				});
				const auto agent    = agent_of(run->Id);
				ending->settle(answered ? (run->Seconds ? agent_run_answered_words(agent, *run->Seconds) + QStringLiteral(".") : tr("%1 answered.").arg(agent))
										: (run->Seconds ? agent_run_stopped_words(*run->Seconds) + QStringLiteral(".") : tr("Stopped.")),
							   answered);
				// One row in the narrow column: the ending, then its actions.
				auto* const row = new QHBoxLayout{ };
				row->setContentsMargins(0, 0, 0, 0);
				row->setSpacing(2);
				row->addWidget(ending, 1, Qt::AlignVCenter);
				auto* copy = static_cast<QToolButton*>(nullptr);
				if (actions.Copy) {
					copy = new QToolButton{ answer };
					copy->setObjectName(QStringLiteral("tourSideChatCopyReply"));
					copy->setAutoRaise(true);
					copy->setToolTip(answered ? tr("Copy this reply") : tr("Copy what went wrong"));
					copy->setAccessibleName(answered ? tr("Copy reply") : tr("Copy error"));
					copy->setIcon(drawer_icon(drawer_glyph::copy, palette()));
					const auto text = answered ? reply_text(*run) : failure;
					connect(copy, &QToolButton::clicked, this, [text] {
						if (auto* const clipboard = QGuiApplication::clipboard(); clipboard != nullptr) {
							clipboard->setText(text);
						}
					});
					row->addWidget(copy);
				}
				auto* resend = static_cast<QPushButton*>(nullptr);
				if (actions.Recovery == core::agent_turn_recovery::resend) {
					resend = new quiet_push_button{ tr("Resend"), answer };
					resend->setObjectName(QStringLiteral("tourSideChatResend"));
					resend->setFlat(true);
					resend->setFocusPolicy(Qt::TabFocus);
					resend->setToolTip(tr("Send this message to the agent again"));
					resend->setEnabled(Ready);
					auto again = tour_ask{ .Thread = run->Thread, .Passage = run->Passage, .Question = QString::fromStdString(run->Question), .Update = run->Update };
					connect(resend, &QPushButton::clicked, this, [this, again] {
						if (Ready && !Running && AskDispatch) {
							AskDispatch(again);
						}
					});
					row->addWidget(resend);
				}
				auto* remove = static_cast<QPushButton*>(nullptr);
				if (actions.Delete && DeleteDispatch) {
					remove = new quiet_push_button{ tr("Delete"), answer };
					remove->setObjectName(QStringLiteral("tourSideChatDelete"));
					remove->setFlat(true);
					remove->setFocusPolicy(Qt::TabFocus);
					remove->setToolTip(tr("Remove this message from the chat. Later questions are asked without it."));
					const auto turn = run->Id;
					connect(remove, &QPushButton::clicked, this, [this, turn] {
						if (!Running && DeleteDispatch) {
							DeleteDispatch(turn);
						}
					});
					row->addWidget(remove);
				}
				column->addLayout(row);
				Copies.push_back(copy);
				Resends.push_back(resend);
				Deletes.push_back(remove);
			}
			if (running) {
				Copies.push_back(nullptr);
				Resends.push_back(nullptr);
				Deletes.push_back(nullptr);
			}
			Replies.push_back(said);
			// [[BPR-1930]] follow-up, owner: "I dont really understand the focus
			// changes button. I clicked it and couldnt tell what it did then it
			// went away." A focus applies at once; one line says so, with Undo
			// while it is still the focus.
			if (wholeTour && !run->FocusSet.empty()) {
				const auto  focus = QString::fromStdString(run->FocusSet).toHtmlEscaped();
				const auto  undo  = core::tour_focus_undoable(Session, *run);
				auto* const line  = quiet(small(new QLabel{ answer }));
				line->setObjectName(QStringLiteral("tourSideChatFocus"));
				line->setWordWrap(true);
				line->setTextFormat(Qt::RichText);
				line->setText(run->FocusUndone ? tr("Focus set to %1%2%3, then undone.").arg(QString{ open_quote }, focus, QString{ close_quote })
								  : undo       ? tr("Focus set to %1%2%3 · <a href=\"#undo\">Undo</a>").arg(QString{ open_quote }, focus, QString{ close_quote })
											   : tr("Focus set to %1%2%3.").arg(QString{ open_quote }, focus, QString{ close_quote }));
				line->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
				line->setToolTip(tr("Later drafts and chat answers favour this part of the pull request. It shows as a chip under the tour's header."));
				const auto turn = run->Id;
				connect(line, &QLabel::linkActivated, this, [this, turn] {
					if (FocusUndoDispatch) {
						FocusUndoDispatch(turn);
					}
				});
				column->addWidget(line);
				FocusLine = line;
			}
			// [[BPR-1930]]. The tour chat's proposal waits in the tour itself,
			// on the stops it changes; here one line says so.
			if (wholeTour && Proposal) {
				const auto mine    = std::ranges::count_if(Proposal->Items, [run](const core::tour_proposal_item& aItem) { return aItem.Run == run->Id; });
				const auto waiting = std::ranges::count_if(Proposal->Items, [run](const core::tour_proposal_item& aItem) { return aItem.Run == run->Id && aItem.Decision == core::tour_proposal_decision::pending; });
				if (mine > 0) {
					auto* const line = quiet(small(words(waiting == 0       ? tr("Its changes were decided in the tour.")
															 : waiting == 1 ? tr("1 change waits in the tour, marked grey. Accept or discard it where it is.")
																			: tr("%1 changes wait in the tour, marked grey. Accept or discard each where it is.").arg(waiting),
														 answer)));
					line->setObjectName(QStringLiteral("tourSideChatInTour"));
					column->addWidget(line);
					InTour = line;
				}
			}
			// [[BPR-1937]], owner 2026-11-08: asked for 8 stops, the refused
			// merges went unsaid and the earlier plan was applied. Each part of
			// this turn's revision that couldn't be proposed, and why.
			if (wholeTour && run->State == core::tour_agent_run_state::answered && !run->Proposed.Refused.empty()) {
				auto refused = QStringList{ };
				for (const auto& part : run->Proposed.Refused) {
					refused << tour_agent_panel::refused_words(part);
				}
				auto* const line = quiet(small(words(refused.join(QLatin1Char('\n')), answer)));
				line->setObjectName(QStringLiteral("tourSideChatRefused"));
				column->addWidget(line);
				RefusedLine = line;
			}
			// [[BPR-1937]]. A plan described only in words changes nothing:
			// under the newest whole-tour answer that proposed nothing, the
			// way to ask for the change itself.
			if (wholeTour && run == runs.back() && run->State == core::tour_agent_run_state::answered && run->Proposed == core::tour_proposal_tally{ } && run->FocusSet.empty()) {
				auto* const line = quiet(small(new QLabel{ answer }));
				line->setObjectName(QStringLiteral("tourSideChatUnchanged"));
				line->setWordWrap(true);
				line->setTextFormat(Qt::RichText);
				line->setText(tr("Nothing in the tour changed. If this answer describes a change, <a href=\"#update\">Update the tour from this chat</a> asks the agent for it."));
				line->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
				connect(line, &QLabel::linkActivated, this, [this] {
					if (Update->isEnabled()) {
						Update->click();
					}
				});
				column->addWidget(line);
				UnchangedLine = line;
			}
			// What it proposed, decided here.
			if (Proposal && !wholeTour) {
				for (std::size_t index = 0U; index < Proposal->Items.size(); ++index) {
					const auto& item = Proposal->Items[index];
					if (item.Run != run->Id) {
						continue;
					}
					auto* const card  = new proposal_frame{ answer };
					auto* const lines = new QVBoxLayout{ card };
					lines->setContentsMargins(12, 8, 8, 8);
					lines->setSpacing(4);
					auto heading = QString{ };
					switch (item.Part) {
					case core::tour_proposal_part::problem:   heading = tr("Proposed Problem"); break;
					case core::tour_proposal_part::solution:  heading = tr("Proposed Solution"); break;
					case core::tour_proposal_part::stop_text: heading = tr("Proposed explanation for %1").arg(QString::fromStdString(item.Stop.Title)); break;
					case core::tour_proposal_part::stop:      heading = tr("Proposed stop: %1 (%2, lines %3 to %4)").arg(QString::fromStdString(item.Stop.Title), QString::fromStdString(item.Stop.Path)).arg(item.Stop.FirstLine).arg(item.Stop.LastLine); break;
					// [[BPR-1930]]. Only the tour's chat proposes these, and they
					// wait in the tour, not here.
					case core::tour_proposal_part::remove_stop:
					case core::tour_proposal_part::merge_stops:
					case core::tour_proposal_part::split_stop:
					case core::tour_proposal_part::reorder:     heading = tr("A change to the tour"); break;
					}
					auto* const title = small(words(heading, card));
					auto        heavy = title->font();
					heavy.setWeight(QFont::DemiBold);
					title->setFont(heavy);
					Accented.emplace_back(title);
					lines->addWidget(title);
					if (item.Decision != core::tour_proposal_decision::pending) {
						lines->addWidget(quiet(small(words(!item.Refusal.empty() ? tr("Refused: the target changed since this question was sent.") : item.Decision == core::tour_proposal_decision::accepted ? tr("Accepted into the tour.")
																																																			 : tr("Discarded."),
														   card))));
						column->addWidget(card);
						continue;
					}
					if (item.ReplacesAuthorText) {
						lines->addWidget(quiet(small(words(tr("Accepting replaces what is there now."), card))));
					}
					const auto  source  = Edited.contains(index) ? Edited[index] : QString::fromStdString(item.Text);
					const auto  editing = Editing.contains(index);
					auto* const shown   = reader(card, source.toStdString(), "tour-side-chat-proposal:" + std::to_string(id) + ":" + std::to_string(index) + ":" + std::to_string(qHash(source)), selector);
					shown->setObjectName(QStringLiteral("tourSideChatProposal"));
					shown->setVisible(!editing);
					ProposalReaders[index] = shown;
					lines->addWidget(shown);
					auto* const editor = new QPlainTextEdit{ card };
					editor->setObjectName(QStringLiteral("tourSideChatProposalText"));
					editor->setAccessibleName(heading);
					editor->setPlainText(source);
					editor->setTabChangesFocus(true);
					editor->setFixedHeight(std::clamp(static_cast<int>(source.size() / 40 + 2), 3, 12) * editor->fontMetrics().lineSpacing() + 14);
					editor->setVisible(editing);
					lines->addWidget(editor);
					connect(editor, &QPlainTextEdit::textChanged, this, [this, index, editor] { Edited[index] = editor->toPlainText(); });
					auto* const buttons = new QHBoxLayout{ };
					buttons->setSpacing(6);
					auto* const accept = new quiet_push_button{ tr("Accept"), card };
					accept->setObjectName(QStringLiteral("tourSideChatAccept"));
					accept->setFocusPolicy(Qt::TabFocus);
					accept->setToolTip(item.Part == core::tour_proposal_part::stop ? tr("Adds this stop at the end of the tour.") : tr("Puts this text in the tour."));
					auto* const edit = new quiet_push_button{ editing ? tr("Show formatted") : tr("Edit text"), card };
					edit->setObjectName(QStringLiteral("tourSideChatEdit"));
					edit->setFlat(true);
					edit->setFocusPolicy(Qt::TabFocus);
					auto* const discard = new quiet_push_button{ tr("Discard"), card };
					discard->setObjectName(QStringLiteral("tourSideChatDiscard"));
					discard->setFlat(true);
					discard->setFocusPolicy(Qt::TabFocus);
					buttons->addWidget(accept);
					buttons->addWidget(edit);
					buttons->addWidget(discard);
					buttons->addStretch(1);
					lines->addLayout(buttons);
					Accepts[index] = accept;
					Accented.emplace_back(accept);
					connect(accept, &QPushButton::clicked, this, [this, index, editor] {
						if (DecideDispatch) {
							DecideDispatch(index, core::tour_proposal_decision::accepted, editor->toPlainText());
						}
					});
					connect(discard, &QPushButton::clicked, this, [this, index] {
						if (DecideDispatch) {
							DecideDispatch(index, core::tour_proposal_decision::discarded, { });
						}
					});
					connect(edit, &QPushButton::clicked, this, [this, index] {
						if (!Editing.insert(index).second) {
							Editing.erase(index);
						}
						rebuild();
					});
					column->addWidget(card);
				}
			}
			Turns->addWidget(answer);
		}
		// [[BPR-1930]] AC3. Feedback given while the draft runs, waiting for
		// its proposal: the author's words, and when they go.
		if (wholeTour && Session.Waiting) {
			auto* const mineRow = new QHBoxLayout{ };
			mineRow->addStretch(1);
			auto* const bubble = new bubble_frame{ body };
			bubble->setObjectName(QStringLiteral("tourSideChatWaiting"));
			auto* const inside = new QVBoxLayout{ bubble };
			inside->setContentsMargins(12, 8, 12, 8);
			inside->setSpacing(3);
			inside->addWidget(words(QString::fromStdString(*Session.Waiting), bubble));
			inside->addWidget(quiet(small(words(tr("Sent when the draft is ready, to revise it."), bubble))));
			mineRow->addWidget(bubble);
			Turns->addLayout(mineRow);
			WaitingBubble = bubble;
		}
		// [[BPR-1941]]. A running Draft Tour, in the whole tour's chat, as
		// the Agent tab says a running turn.
		if (wholeTour && Drafting) {
			auto* const drafting = new QWidget{ body };
			drafting->setObjectName(QStringLiteral("tourSideChatDrafting"));
			auto* const lines = new QVBoxLayout{ drafting };
			lines->setContentsMargins(0, 0, 0, 0);
			lines->setSpacing(2);
			auto* const heading = small(words(tr("Draft Tour"), drafting));
			heading->setObjectName(QStringLiteral("tourSideChatDraftHeading"));
			auto heavy = heading->font();
			heavy.setWeight(QFont::DemiBold);
			heading->setFont(heavy);
			Accented.emplace_back(heading);
			lines->addWidget(heading);
			DraftLine = new agent_run_status_line{ drafting };
			DraftLine->setObjectName(QStringLiteral("tourSideChatDraftStatus"));
			small(DraftLine);
			DraftLine->present(live_status(), LiveClock);
			lines->addWidget(DraftLine);
			Turns->addWidget(drafting);
		}
		// [[BPR-1934]], owner 2026-11-07: "Tour side chat should get
		// suggestions like the agent tab has." The Agent tab's own component,
		// the chat's last item, in the agent's accent: the prompts for this
		// kind of chat while it is empty, and Explain the selected block
		// while text is selected (v002).
		if (thread && PromptSource) {
			Suggestions = new agent_suggestion_list{ tr("Or try:"), body };
			Suggestions->setObjectName(QStringLiteral("tourSideChatSuggestions"));
			Suggestions->set_choose_dispatch([this](const QString& aPrompt, const bool aEditFirst) { choose_prompt(aPrompt, aEditFirst); });
			Accented.emplace_back(Suggestions.data());
			Turns->addWidget(Suggestions);
		}
		Turns->addStretch(1);
		const auto grew = runs.size() != Shown || Mine;
		Shown           = runs.size();
		Stick           = Stick || grew;
		restyle(palette());
		refresh_actions();
		QTimer::singleShot(0, this, [this] { fit(); });
	}

	void tour_side_chat::fit()
	{
		// Each rendered text as tall as it reads at the chat's width; the
		// author's bubbles at most four fifths of it.
		const auto available = std::max(160, Scroll->viewport()->width() - Turns->contentsMargins().left() - Turns->contentsMargins().right());
		auto       bubbles   = Bubbles;
		if (WaitingBubble != nullptr) {
			bubbles.push_back(WaitingBubble);
		}
		for (auto* const bubble : bubbles) {
			const auto most = available * 4 / 5;
			bubble->setMaximumWidth(most);
			// A short question keeps to one line; a long one wraps at the
			// bubble's width.
			for (auto* const label : bubble->findChildren<QLabel*>()) {
				auto widest = 0;
				for (const auto& line : label->text().split(QLatin1Char('\n'))) {
					widest = std::max(widest, label->fontMetrics().horizontalAdvance(line));
				}
				label->setMinimumWidth(std::min(widest + 2, most - 24));
			}
		}
		for (const auto& each : Readers) {
			if (each && !each->isHidden()) {
				// Inside a proposal card, less the card's margins.
				const auto width  = dynamic_cast<proposal_frame*>(each->parentWidget()) != nullptr ? available - 20 : available;
				const auto height = each->heightForWidth(std::max(120, width));
				if (each->height() != height || each->minimumHeight() != height) {
					each->setFixedHeight(height);
				}
			}
		}
		show_passage();
		// A new turn keeps the chat at its end while its replies lay out.
		if (Stick) {
			to_bottom();
		}
	}

	void tour_side_chat::to_bottom()
	{
		QTimer::singleShot(0, this, [this] { Scroll->verticalScrollBar()->setValue(Scroll->verticalScrollBar()->maximum()); });
	}
}
