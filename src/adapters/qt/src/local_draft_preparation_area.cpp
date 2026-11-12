#include "bpr/qt/local_draft_preparation_area.hpp"

#include "bpr/qt/agent_feature.hpp"

#include <QAction>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSplitter>
#include <QToolButton>
#include <QVBoxLayout>

#include <QAbstractButton>

#include <algorithm>
#include <ranges>
#include <utility>

namespace bpr::qt {
	namespace {
		constexpr int id_role = Qt::UserRole + 1;

		auto text_of(const std::string_view aBytes) -> QString { return QString::fromUtf8(aBytes.data(), static_cast<qsizetype>(aBytes.size())); }

		auto utf8(const QString& aText) -> std::string
		{
			const auto bytes = aText.toUtf8();
			return { bytes.constData(), static_cast<std::size_t>(bytes.size()) };
		}

		auto heading(const QString& aText, QWidget* aParent) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			auto        font  = label->font();
			font.setWeight(QFont::DemiBold);
			label->setFont(font);
			return label;
		}

		/// The error role: the palette's own, never a fixed colour.
		void wear_error_role(QLabel& aLabel)
		{
			auto palette = aLabel.palette();
			palette.setColor(QPalette::WindowText, QColor{ 0xcf, 0x22, 0x2e });
			aLabel.setPalette(palette);
		}

		/// The stop's ring: green for Accepted, yellow for the other two (RFC-BPR-0106).
		auto ring(const core::tour_acceptance_state aState) -> QIcon
		{
			auto pixmap = QPixmap{ 28, 28 };
			pixmap.setDevicePixelRatio(2.0);
			pixmap.fill(Qt::transparent);
			auto painter = QPainter{ &pixmap };
			painter.setRenderHint(QPainter::Antialiasing);
			painter.setPen(QPen{ aState == core::tour_acceptance_state::accepted ? QColor{ 0x2d, 0xa4, 0x4e } : QColor{ 0xbf, 0x87, 0x00 }, 2.0 });
			if (aState == core::tour_acceptance_state::accepted) {
				painter.setBrush(QColor{ 0x2d, 0xa4, 0x4e });
			}
			painter.drawEllipse(QRectF{ 2.0, 2.0, 10.0, 10.0 });
			painter.end();
			return QIcon{ pixmap };
		}

		auto reason_text(const core::local_stop_reason aReason) -> QString
		{
			switch (aReason) {
			case core::local_stop_reason::none:             return QStringLiteral("Accepted");
			case core::local_stop_reason::not_accepted:     return QStringLiteral("Not accepted yet");
			case core::local_stop_reason::edited:           return QStringLiteral("Changed since accepted: edited");
			case core::local_stop_reason::changed:          return QStringLiteral("Changed since accepted: changed");
			case core::local_stop_reason::ambiguous:        return QStringLiteral("Changed since accepted: ambiguous");
			case core::local_stop_reason::missing:          return QStringLiteral("Changed since accepted: missing");
			case core::local_stop_reason::unavailable:      return QStringLiteral("Changed since accepted: unavailable");
			case core::local_stop_reason::evidence_recheck: return QStringLiteral("Changed since accepted: cited evidence needs recheck");
			}
			return { };
		}

		auto target_text(const core::local_proposal& aProposal) -> QString
		{
			switch (aProposal.Target) {
			case core::local_proposal_target::title:       return QStringLiteral("Title");
			case core::local_proposal_target::description: return QStringLiteral("Description");
			case core::local_proposal_target::problem:     return QStringLiteral("Problem");
			case core::local_proposal_target::solution:    return QStringLiteral("Solution");
			case core::local_proposal_target::stop:        return QStringLiteral("Stop %1").arg(aProposal.Stop);
			case core::local_proposal_target::new_stop:    return QStringLiteral("New stop");
			case core::local_proposal_target::claim:       return QStringLiteral("Claim");
			case core::local_proposal_target::check:       return QStringLiteral("Planned check");
			}
			return { };
		}

		auto first_line(const std::string& aText) -> QString
		{
			const auto text = text_of(aText).trimmed();
			const auto end  = text.indexOf(QLatin1Char{ '\n' });
			return end < 0 ? text : text.left(end) + QStringLiteral("…");
		}

		auto lines_text(const std::string& aPath, const std::uint32_t aFirst, const std::uint32_t aLast) -> QString
		{
			return aFirst == aLast ? QStringLiteral("%1:%2").arg(text_of(aPath)).arg(aFirst) : QStringLiteral("%1:%2–%3").arg(text_of(aPath)).arg(aFirst).arg(aLast);
		}

		/// "1 stop", "2 stops": no translator is loaded, so the plural is chosen here.
		auto counted(const std::size_t aCount, const char* aOne, const char* aMany) -> QString
		{
			return QStringLiteral("%1 %2").arg(aCount).arg(QString::fromLatin1(aCount == 1U ? aOne : aMany));
		}

		/// Ctrl or Cmd with Return saves an editor (X10).
		void save_on_command_return(QWidget* aEditor, std::function<void()> aSave)
		{
			auto* const shortcut = new QShortcut{ QKeySequence{ Qt::CTRL | Qt::Key_Return }, aEditor };
			shortcut->setContext(Qt::WidgetShortcut);
			QObject::connect(shortcut, &QShortcut::activated, aEditor, std::move(aSave));
		}

		/// One width for a row of buttons: nothing changes size when its words do.
		void same_width(std::initializer_list<QAbstractButton*> aButtons)
		{
			auto width = 0;
			for (auto* const button : aButtons) {
				width = std::max(width, button->sizeHint().width());
			}
			for (auto* const button : aButtons) {
				button->setMinimumWidth(width);
			}
		}
	}

	auto preparation_refusal_text(const core::preparation_refusal aRefusal) -> QString
	{
		switch (aRefusal) {
		case core::preparation_refusal::stale:             return QStringLiteral("This draft changed elsewhere and was read again. Try again.");
		case core::preparation_refusal::too_long:          return QStringLiteral("That is longer than the pull request can carry. Nothing was cut; shorten it.");
		case core::preparation_refusal::invalid:           return QStringLiteral("That text cannot be saved as written.");
		case core::preparation_refusal::unknown:           return QStringLiteral("That stop or proposal is no longer in this draft.");
		case core::preparation_refusal::full:              return QStringLiteral("The tour holds as many as it keeps.");
		case core::preparation_refusal::outside_candidate: return QStringLiteral("Those lines are not in this candidate.");
		case core::preparation_refusal::proposal_stale:
			return QStringLiteral("That proposal was made on an earlier version of what it changes. It stays readable; ask again to get one on this version.");
		case core::preparation_refusal::decided:      return QStringLiteral("That proposal was already decided.");
		case core::preparation_refusal::lines_needed: return QStringLiteral("Its lines are not exactly in this candidate. Choose lines in Changes and Save.");
		case core::preparation_refusal::text_needed:  return QStringLiteral("A stop needs a title and an explanation.");
		}
		return { };
	}

	auto local_cost_text(const std::uint64_t aMicros) -> QString
	{
		if (aMicros < 10000U) {
			return QStringLiteral("< $0.01");
		}
		return QStringLiteral("$%1").arg(static_cast<double>(aMicros) / 1000000.0, 0, 'f', 2);
	}

	// --- The agent bar ---------------------------------------------------------------

	local_draft_chat::local_draft_chat(const bool aDescription, QWidget* const aParent)
		: QWidget{ aParent }
		, Description{ aDescription }
	{
		setObjectName(aDescription ? QStringLiteral("localDraftDescriptionChat") : QStringLiteral("localDraftTourChat"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 6, 0, 0);
		column->setSpacing(4);
		auto* const split = new QHBoxLayout{ };
		// The transcript: words in a scrolling pane; a reference to the
		// candidate is a link that opens it there.
		TranscriptScroll = new QScrollArea{ this };
		TranscriptScroll->setWidgetResizable(true);
		TranscriptScroll->setFixedHeight(150);
		TranscriptScroll->setBackgroundRole(QPalette::Base);
		Transcript = new QLabel{ TranscriptScroll };
		Transcript->setObjectName(QStringLiteral("localDraftTranscript"));
		Transcript->setAccessibleName(QStringLiteral("Conversation"));
		Transcript->setWordWrap(true);
		Transcript->setAlignment(Qt::AlignTop | Qt::AlignLeft);
		Transcript->setTextFormat(Qt::RichText);
		Transcript->setTextInteractionFlags(Qt::TextBrowserInteraction);
		Transcript->setContentsMargins(6, 4, 6, 4);
		TranscriptScroll->setWidget(Transcript);
		split->addWidget(TranscriptScroll, 3);
		auto* const side = new QVBoxLayout{ };
		Proposals        = new QListWidget{ this };
		Proposals->setObjectName(QStringLiteral("localDraftProposals"));
		Proposals->setAccessibleName(QStringLiteral("Proposals"));
		Proposals->setFixedHeight(118);
		Proposals->setToolTip(QStringLiteral("What the agent proposed, beside what it would change. Accept puts it in the draft; nothing is published"));
		side->addWidget(Proposals);
		auto* const decide = new QHBoxLayout{ };
		Accept             = new QPushButton{ QStringLiteral("Accept"), this };
		Accept->setObjectName(QStringLiteral("localDraftAcceptProposal"));
		Accept->setToolTip(QStringLiteral("Put the selected proposal in the draft"));
		Reject = new QPushButton{ QStringLiteral("Reject"), this };
		Reject->setObjectName(QStringLiteral("localDraftRejectProposal"));
		same_width({ Accept, Reject });
		decide->addStretch(1);
		decide->addWidget(Accept);
		decide->addWidget(Reject);
		side->addLayout(decide);
		split->addLayout(side, 2);
		column->addLayout(split);

		auto* const line = new QHBoxLayout{ };
		Session          = new QLabel{ this };
		Session->setObjectName(QStringLiteral("localDraftSessionLine"));
		Session->setTextInteractionFlags(Qt::TextSelectableByMouse);
		line->addWidget(Session, 1);
		Running = new QLabel{ this };
		Running->setObjectName(QStringLiteral("localDraftRunning"));
		line->addWidget(Running);
		SessionMenu = new QPushButton{ QStringLiteral("Session"), this };
		SessionMenu->setObjectName(QStringLiteral("localDraftSessionMenu"));
		SessionMenu->setMenu(new QMenu{ SessionMenu });
		line->addWidget(SessionMenu);
		column->addLayout(line);
		Failure = new QLabel{ this };
		Failure->setObjectName(QStringLiteral("localDraftAskFailure"));
		Failure->setWordWrap(true);
		wear_error_role(*Failure);
		Failure->hide();
		column->addWidget(Failure);

		auto* const compose = new QHBoxLayout{ };
		Composer            = new QLineEdit{ this };
		Composer->setObjectName(QStringLiteral("localDraftComposer"));
		Composer->setAccessibleName(QStringLiteral("Question"));
		Composer->setPlaceholderText(aDescription ? QStringLiteral("Ask about the description") : QStringLiteral("Ask about the change"));
		compose->addWidget(Composer, 1);
		Ask = new QPushButton{ QStringLiteral("Ask"), this };
		Ask->setObjectName(QStringLiteral("localDraftAsk"));
		Ask->setToolTip(QStringLiteral("Ask the agent. It reads this candidate as captured, never your worktree, and cannot run anything"));
		wear_agent_accent(*Ask, palette());
		Ask->setMinimumWidth(std::max(Ask->sizeHint().width(), QPushButton{ QStringLiteral("Stop") }.sizeHint().width()));
		compose->addWidget(Ask);
		column->addLayout(compose);

		connect(Composer, &QLineEdit::returnPressed, this, [this] { send(); });
		connect(Ask, &QPushButton::clicked, this, [this] {
			if (Projection.Asking) {
				if (Dispatch != nullptr && Dispatch->StopAsking) {
					Dispatch->StopAsking();
				}
				return;
			}
			send();
		});
		const auto decideSelected = [this](const bool aAccept) {
			const auto* const item = Proposals->currentItem();
			if (item != nullptr && Dispatch != nullptr && Dispatch->DecideProposal) {
				Dispatch->DecideProposal(item->data(id_role).toULongLong(), aAccept);
			}
		};
		connect(Accept, &QPushButton::clicked, this, [decideSelected] { decideSelected(true); });
		connect(Reject, &QPushButton::clicked, this, [decideSelected] { decideSelected(false); });
		connect(Proposals, &QListWidget::currentItemChanged, this, [this] { present_proposals(); });
		connect(Transcript, &QLabel::linkActivated, this, [this](const QString& aLink) {
			// A reference resolves against the candidate and opens it there (X09).
			constexpr auto scheme = QLatin1StringView{ "bpr-candidate:" };
			const auto     hash   = aLink.lastIndexOf(QLatin1Char{ '#' });
			if (!aLink.startsWith(scheme) || hash < 0 || Links == nullptr || !Links->Reveal) {
				return;
			}
			const auto range = aLink.mid(hash + 1).split(QLatin1Char{ '-' });
			const auto first = range.value(0).toUInt();
			const auto last  = range.size() > 1 ? range.value(1).toUInt() : first;
			Links->Reveal(utf8(aLink.mid(scheme.size(), hash - scheme.size())), first, last);
		});
	}

	void local_draft_chat::set_dispatch(local_draft_preparation_dispatch* const aDispatch, local_draft_page_links* const aLinks)
	{
		Dispatch = aDispatch;
		Links    = aLinks;
	}

	void local_draft_chat::set_subject(std::string aThread, const core::local_subject aSubject, const std::uint64_t aStop)
	{
		if (aThread != Thread) {
			Shown = 0U;
		}
		Thread  = std::move(aThread);
		Subject = aSubject;
		Stop    = aStop;
		Composer->setPlaceholderText(aSubject == core::local_subject::stop              ? QStringLiteral("Ask about stop %1").arg(aStop)
										 : aSubject == core::local_subject::description ? QStringLiteral("Ask about the description")
																						: QStringLiteral("Ask about the change"));
		present_transcript();
		present_session();
	}

	void local_draft_chat::show_conversation(const std::uint64_t aConversation)
	{
		Shown = aConversation;
		present_transcript();
		present_session();
	}

	void local_draft_chat::send()
	{
		const auto text = Composer->text().trimmed();
		if (text.isEmpty() || Projection.Asking || !Projection.AgentAvailable || Dispatch == nullptr || !Dispatch->Ask) {
			return;
		}
		auto question = application::local_draft_question{ .Thread = Thread, .Subject = Subject, .Stop = Stop, .Text = utf8(text) };
		if (Links != nullptr && Links->Selection && Subject == core::local_subject::whole_change) {
			// Lines chosen in Changes attach to a whole-change question (X12).
			if (const auto lines = Links->Selection(); lines && lines->First != 0U) {
				question.Subject   = core::local_subject::lines;
				question.LinesPath = lines->Path;
				question.FirstLine = lines->First;
				question.LastLine  = lines->Last;
			}
		}
		Shown = 0U;
		Dispatch->Ask(std::move(question));
		Composer->clear();
	}

	void local_draft_chat::present(const application::local_draft_workbench_projection& aProjection)
	{
		Projection = aProjection;
		present_transcript();
		present_proposals();
		present_session();
		const auto asking = Projection.Asking;
		Ask->setText(asking ? QStringLiteral("Stop") : QStringLiteral("Ask"));
		if (asking) {
			wear_ordinary_roles(*Ask);
		} else {
			wear_agent_accent(*Ask, palette());
		}
		Ask->setEnabled(asking || (Projection.AgentAvailable && Projection.Draft.has_value()));
		Composer->setEnabled(Projection.Draft.has_value());
		if (!Projection.AgentAvailable) {
			Ask->setToolTip(QStringLiteral("Add an agent that speaks Claude Code's stream JSON in Settings › Agents & context. Writing by hand needs none"));
		}
		Running->setText(asking ? QStringLiteral("Asking…") : QString{ });
		Running->setToolTip(asking ? text_of(Projection.AskingText) : QString{ });
		Failure->setText(text_of(Projection.AskFailure));
		Failure->setVisible(!Projection.AskFailure.empty());
	}

	void local_draft_chat::present_transcript()
	{
		const core::local_conversation* shown = nullptr;
		if (Projection.Draft) {
			const auto& all = Projection.Draft->Preparation.Conversations;
			if (Shown != 0U) {
				const auto found = std::ranges::find(all, Shown, &core::local_conversation::Id);
				shown            = found == all.end() ? nullptr : &*found;
			}
			if (shown == nullptr) {
				const auto* latest = core::latest_candidate(*Projection.Draft);
				for (const auto& conversation : std::views::reverse(all)) {
					if (!conversation.Ended && latest != nullptr && conversation.Candidate == latest->Ordinal && conversation.Thread == Thread) {
						shown = &conversation;
						break;
					}
				}
			}
		}
		auto files = QStringList{ };
		if (Projection.Draft) {
			if (const auto* latest = core::latest_candidate(*Projection.Draft); latest != nullptr) {
				for (const auto& change : latest->Changes) {
					files << text_of(change.Path);
				}
			}
		}
		auto html = QString{ };
		if (shown != nullptr) {
			static const auto reference = QRegularExpression{ QStringLiteral(R"(([A-Za-z0-9_./-]+):(\d+)(?:-(\d+))?)") };
			for (const auto& turn : shown->Turns) {
				auto body = text_of(turn.Text).toHtmlEscaped();
				// Only references to this candidate's files become links.
				auto out  = QString{ };
				auto last = qsizetype{ 0 };
				for (auto it = reference.globalMatch(body); it.hasNext();) {
					const auto match = it.next();
					out += body.mid(last, match.capturedStart() - last);
					if (files.contains(match.captured(1))) {
						const auto range = match.captured(3).isEmpty() ? match.captured(2) : match.captured(2) + QLatin1Char{ '-' } + match.captured(3);
						out += QStringLiteral("<a href=\"bpr-candidate:%1#%2\">%3</a>").arg(match.captured(1), range, match.captured(0));
					} else {
						out += match.captured(0);
					}
					last = match.capturedEnd();
				}
				out += body.mid(last);
				out.replace(QLatin1Char{ '\n' }, QStringLiteral("<br>"));
				const auto who  = turn.Author ? QStringLiteral("You") : text_of(shown->AgentName.empty() ? std::string{ "Agent" } : shown->AgentName);
				const auto cost = !turn.Author && turn.CostMicros ? QStringLiteral(" <span>(%1)</span>").arg(local_cost_text(*turn.CostMicros)) : QString{ };
				html += QStringLiteral("<p><b>%1</b>%2<br>%3</p>").arg(who.toHtmlEscaped(), cost, out);
			}
		}
		Transcript->setText(html);
		Transcript->adjustSize();
		TranscriptScroll->verticalScrollBar()->setValue(TranscriptScroll->verticalScrollBar()->maximum());
	}

	void local_draft_chat::present_proposals()
	{
		const auto keep = Proposals->currentItem() == nullptr ? 0ULL : Proposals->currentItem()->data(id_role).toULongLong();
		{
			const auto blocker = QSignalBlocker{ Proposals };
			Proposals->clear();
			if (Projection.Draft) {
				for (const auto& proposal : Projection.Draft->Preparation.Proposals) {
					const auto description = proposal.Target == core::local_proposal_target::title || proposal.Target == core::local_proposal_target::description;
					if (description != Description
						|| (proposal.State != core::local_proposal_state::pending && proposal.State != core::local_proposal_state::outside_candidate)) {
						continue;
					}
					const auto stale  = core::proposal_stale(*Projection.Draft, proposal);
					auto       label  = QStringLiteral("%1: %2").arg(target_text(proposal), first_line(proposal.Title.empty() ? proposal.Text : proposal.Title));
					auto*      item   = new QListWidgetItem{ label, Proposals };
					auto       detail = text_of(proposal.Text);
					if (proposal.Source) {
						detail = lines_text(proposal.Source->Path, proposal.Source->FirstLine, proposal.Source->LastLine) + QStringLiteral("\n\n") + detail;
					}
					if (proposal.State == core::local_proposal_state::outside_candidate) {
						detail = QStringLiteral("Its lines are not in the candidate it was made on; it cannot be accepted as written.\n\n") + detail;
					} else if (stale) {
						detail = QStringLiteral("Made on an earlier version of what it changes; it cannot overwrite what is there now.\n\n") + detail;
					}
					item->setToolTip(detail);
					item->setData(id_role, QVariant::fromValue<qulonglong>(proposal.Id));
					item->setData(Qt::UserRole + 2, proposal.State == core::local_proposal_state::pending && !stale);
					if (proposal.State != core::local_proposal_state::pending || stale) {
						auto font = item->font();
						font.setItalic(true);
						item->setFont(font);
					}
					if (proposal.Id == keep) {
						Proposals->setCurrentItem(item);
					}
				}
			}
		}
		const auto* const current = Proposals->currentItem();
		Accept->setEnabled(current != nullptr && current->data(Qt::UserRole + 2).toBool());
		Reject->setEnabled(current != nullptr);
	}

	void local_draft_chat::present_session()
	{
		auto* const menu = SessionMenu->menu();
		menu->clear();
		if (!Projection.Draft) {
			Session->clear();
			return;
		}
		const auto&                     all    = Projection.Draft->Preparation.Conversations;
		const auto*                     latest = core::latest_candidate(*Projection.Draft);
		const core::local_conversation* open   = nullptr;
		for (const auto& conversation : std::views::reverse(all)) {
			if (!conversation.Ended && latest != nullptr && conversation.Candidate == latest->Ordinal && conversation.Thread == Thread) {
				open = &conversation;
				break;
			}
		}
		const auto  found = Shown == 0U ? all.end() : std::ranges::find(all, Shown, &core::local_conversation::Id);
		const auto* shown = found == all.end() ? open : &*found;
		if (shown == nullptr) {
			Session->setText(latest == nullptr ? QString{ } : QStringLiteral("New session on candidate %1").arg(latest->Ordinal));
		} else {
			const auto cost = core::conversation_cost(*shown);
			Session->setText(QStringLiteral("%1 · candidate %2%3%4")
								 .arg(text_of(shown->AgentName.empty() ? std::string{ "Agent" } : shown->AgentName))
								 .arg(shown->Candidate)
								 .arg(cost ? QStringLiteral(" · %1 this session").arg(local_cost_text(*cost)) : QString{ })
								 .arg(shown != open ? QStringLiteral(" · earlier, read only") : QString{ }));
		}
		auto tip = QStringList{ QStringLiteral("Tool-free: the agent reads only what the request sends, from this candidate as captured") };
		if (Projection.Coverage) {
			tip << QStringLiteral("Last request: %1 of %2 changed files sent whole").arg(Projection.Coverage->FilesSent).arg(Projection.Coverage->FilesTotal);
			if (Projection.Coverage->Replayed != 0U) {
				tip << QStringLiteral("Earlier turns replayed: %1, left out: %2").arg(Projection.Coverage->Replayed).arg(Projection.Coverage->Omitted);
			}
		}
		Session->setToolTip(tip.join(QLatin1Char{ '\n' }));
		auto* const fresh = menu->addAction(QStringLiteral("New Session"));
		fresh->setEnabled(open != nullptr);
		connect(fresh, &QAction::triggered, this, [this, id = open == nullptr ? 0ULL : open->Id] {
			if (id != 0U && Dispatch != nullptr && Dispatch->NewSession) {
				Dispatch->NewSession(id);
			}
			show_conversation(0U);
		});
		if (open != nullptr && shown != open) {
			connect(menu->addAction(QStringLiteral("Back to This Session")), &QAction::triggered, this, [this] { show_conversation(0U); });
		}
		auto any = false;
		for (const auto& conversation : std::views::reverse(all)) {
			if (&conversation == open || conversation.Thread != Thread) {
				continue;
			}
			if (!any) {
				menu->addSeparator();
				any = true;
			}
			auto* const action = menu->addAction(QStringLiteral("Candidate %1 · %2 · %3")
													 .arg(conversation.Candidate)
													 .arg(text_of(conversation.AgentName.empty() ? std::string{ "Agent" } : conversation.AgentName))
													 .arg(counted(conversation.Turns.size(), "turn", "turns")));
			connect(action, &QAction::triggered, this, [this, id = conversation.Id] { show_conversation(id); });
		}
	}

	auto local_draft_chat::transcript() const noexcept -> QLabel* { return Transcript; }
	auto local_draft_chat::proposals() const noexcept -> QListWidget* { return Proposals; }
	auto local_draft_chat::session_line() const noexcept -> QLabel* { return Session; }
	auto local_draft_chat::session_menu() const noexcept -> QPushButton* { return SessionMenu; }
	auto local_draft_chat::composer() const noexcept -> QLineEdit* { return Composer; }
	auto local_draft_chat::ask_button() const noexcept -> QPushButton* { return Ask; }
	auto local_draft_chat::accept_button() const noexcept -> QPushButton* { return Accept; }
	auto local_draft_chat::reject_button() const noexcept -> QPushButton* { return Reject; }
	auto local_draft_chat::running() const noexcept -> QLabel* { return Running; }

	// --- Description ---------------------------------------------------------------------

	local_draft_description_area::local_draft_description_area(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("localDraftDescriptionArea"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 6, 0, 0);
		column->setSpacing(4);
		Body = new QPlainTextEdit{ this };
		Body->setObjectName(QStringLiteral("localDraftDescriptionBody"));
		Body->setAccessibleName(QStringLiteral("Description"));
		Body->setPlaceholderText(QStringLiteral("## Problem\n\n## Solution\n\n## Verification"));
		Body->setToolTip(QStringLiteral("The pull request's whole description, in Markdown. The tour never changes it. Ctrl or Cmd with Return saves"));
		column->addWidget(Body, 1);
		auto* const row = new QHBoxLayout{ };
		Notice          = new QLabel{ this };
		Notice->setObjectName(QStringLiteral("localDraftDescriptionNotice"));
		Notice->setWordWrap(true);
		row->addWidget(Notice, 1);
		Insert = new QPushButton{ QStringLiteral("Add from Tour"), this };
		Insert->setObjectName(QStringLiteral("localDraftInsertFromTour"));
		Insert->setToolTip(QStringLiteral("Copy the tour's Problem or Solution to the end of the description"));
		Insert->setMenu(new QMenu{ Insert });
		row->addWidget(Insert);
		Preview = new QPushButton{ QStringLiteral("Preview…"), this };
		Preview->setObjectName(QStringLiteral("localDraftPreview"));
		Preview->setToolTip(QStringLiteral("The exact text a pull request made now would carry. Only evidence you selected is in it"));
		Save = new QPushButton{ QStringLiteral("Save"), this };
		Save->setObjectName(QStringLiteral("localDraftSaveDescription"));
		same_width({ Insert, Preview, Save });
		row->addWidget(Preview);
		row->addWidget(Save);
		column->addLayout(row);
		Chat = new local_draft_chat{ true, this };
		Chat->set_subject(std::string{ core::local_change_thread }, core::local_subject::description, 0U);
		column->addWidget(Chat);

		save_on_command_return(Body, [this] { save(); });
		connect(Save, &QPushButton::clicked, this, [this] { save(); });
		connect(Body, &QPlainTextEdit::modificationChanged, Save, [this](const bool aModified) { Save->setEnabled(aModified && Projection.Draft.has_value()); });
		connect(Insert->menu(), &QMenu::aboutToShow, this, [this] {
			auto* const menu = Insert->menu();
			menu->clear();
			if (!Projection.Draft) {
				return;
			}
			const auto& tour = Projection.Draft->Preparation.Tour;
			for (const auto& [name, text] : { std::pair{ QStringLiteral("Problem"), tour.Problem }, std::pair{ QStringLiteral("Solution"), tour.Solution } }) {
				auto* const action = menu->addAction(name);
				action->setEnabled(!text.empty());
				connect(action, &QAction::triggered, this, [this, passage = "## " + utf8(name) + "\n\n" + text] {
					if (Dispatch != nullptr && Dispatch->Transfer) {
						Dispatch->Transfer(passage);
					}
				});
			}
		});
		connect(Preview, &QPushButton::clicked, this, [this] {
			auto dialog = QDialog{ this };
			dialog.setWindowTitle(QStringLiteral("Preview"));
			auto* const layout = new QVBoxLayout{ &dialog };
			auto* const text   = new QPlainTextEdit{ preview_text(), &dialog };
			text->setObjectName(QStringLiteral("localDraftPreviewText"));
			text->setReadOnly(true);
			layout->addWidget(text);
			auto* const buttons = new QDialogButtonBox{ QDialogButtonBox::Close, &dialog };
			connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
			layout->addWidget(buttons);
			dialog.resize(720, 640);
			dialog.exec();
		});
	}

	void local_draft_description_area::set_dispatch(local_draft_preparation_dispatch* const aDispatch, local_draft_page_links* const aLinks)
	{
		Dispatch = aDispatch;
		Chat->set_dispatch(aDispatch, aLinks);
	}

	void local_draft_description_area::save()
	{
		if (!Projection.Draft || Dispatch == nullptr || !Dispatch->SaveDescription) {
			return;
		}
		Dispatch->SaveDescription(Projection.Draft->Title, utf8(Body->toPlainText()));
		Body->document()->setModified(false);
	}

	auto local_draft_description_area::preview_text() const -> QString
	{
		if (!Projection.Draft) {
			return { };
		}
		const auto preview = core::compose_local_publication(*Projection.Draft);
		auto       text    = QStringLiteral("Title\n\n%1\n\nDescription\n\n%2\n\nTour\n\n%3").arg(text_of(preview.Title), text_of(preview.Body), text_of(preview.Tour));
		if (!preview.Unaccepted.empty()) {
			text += QStringLiteral("\n\nNot accepted yet: %1. They do not block creating the draft pull request; the tour is published only once every stop is accepted.")
						.arg(counted(preview.Unaccepted.size(), "stop", "stops"));
		}
		if (preview.PendingLinks != 0U) {
			text += QStringLiteral("\n\n%1 wait for the pull request's commits.").arg(counted(preview.PendingLinks, "code link", "code links"));
		}
		for (const auto& name : preview.LocalOnly) {
			text += QStringLiteral("\n%1 is held only on this computer and has no public address.").arg(text_of(name));
		}
		return text;
	}

	void local_draft_description_area::present(const application::local_draft_workbench_projection& aProjection)
	{
		const auto other = !Projection.Draft || !aProjection.Draft || Projection.Draft->Id != aProjection.Draft->Id;
		Projection       = aProjection;
		if (other || !Body->document()->isModified()) {
			const auto blocker = QSignalBlocker{ Body };
			const auto text    = aProjection.Draft ? text_of(aProjection.Draft->Preparation.Description.Body) : QString{ };
			if (Body->toPlainText() != text) {
				Body->setPlainText(text);
			}
			Body->document()->setModified(false);
		}
		Body->setReadOnly(!aProjection.Draft);
		Save->setEnabled(aProjection.Draft.has_value() && Body->document()->isModified());
		Preview->setEnabled(aProjection.Draft.has_value());
		Insert->setEnabled(aProjection.Draft.has_value());
		const auto bytes = Body->toPlainText().toUtf8().size();
		auto       words = QString{ };
		if (aProjection.PreparationRefusal) {
			words = preparation_refusal_text(*aProjection.PreparationRefusal);
		} else if (static_cast<std::size_t>(bytes) > core::maximum_local_description_bytes * 9U / 10U) {
			words = QStringLiteral("%1 of %2 bytes a pull request description can carry").arg(bytes).arg(core::maximum_local_description_bytes);
		}
		Notice->setText(words);
		Chat->present(aProjection);
	}

	auto local_draft_description_area::body() const noexcept -> QPlainTextEdit* { return Body; }
	auto local_draft_description_area::save_button() const noexcept -> QPushButton* { return Save; }
	auto local_draft_description_area::preview_button() const noexcept -> QPushButton* { return Preview; }
	auto local_draft_description_area::insert_button() const noexcept -> QPushButton* { return Insert; }
	auto local_draft_description_area::notice() const noexcept -> QLabel* { return Notice; }
	auto local_draft_description_area::chat() const noexcept -> local_draft_chat* { return Chat; }

	// --- Tour ------------------------------------------------------------------------------

	local_draft_tour_area::local_draft_tour_area(QWidget* const aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("localDraftTourArea"));
		auto* const column = new QVBoxLayout{ this };
		column->setContentsMargins(0, 6, 0, 0);
		column->setSpacing(4);
		// The head: the local tour's stops and how many to accept. No Publish:
		// the draft pull request is created from the page's header (X04).
		auto* const headRow = new QHBoxLayout{ };
		Head                = heading(QString{ }, this);
		Head->setObjectName(QStringLiteral("localDraftTourHead"));
		headRow->addWidget(Head, 1);
		AddStop = new QPushButton{ QStringLiteral("Add Stop"), this };
		AddStop->setObjectName(QStringLiteral("localDraftAddStop"));
		AddStop->setToolTip(QStringLiteral("A stop on the lines selected in Changes"));
		headRow->addWidget(AddStop);
		column->addLayout(headRow);

		// The route, scrolling.
		auto* const scroll = new QScrollArea{ this };
		scroll->setObjectName(QStringLiteral("localDraftTourRoute"));
		scroll->setWidgetResizable(true);
		scroll->setFrameShape(QFrame::NoFrame);
		scroll->setFocusPolicy(Qt::NoFocus);
		auto* const route  = new QWidget{ scroll };
		auto* const routes = new QVBoxLayout{ route };
		routes->setContentsMargins(0, 0, 8, 0);
		routes->addWidget(heading(QStringLiteral("Problem"), route));
		Problem = new QPlainTextEdit{ route };
		Problem->setObjectName(QStringLiteral("localDraftProblem"));
		Problem->setAccessibleName(QStringLiteral("Problem"));
		Problem->setFixedHeight(56);
		routes->addWidget(Problem);
		routes->addWidget(heading(QStringLiteral("Solution"), route));
		Solution = new QPlainTextEdit{ route };
		Solution->setObjectName(QStringLiteral("localDraftSolution"));
		Solution->setAccessibleName(QStringLiteral("Solution"));
		Solution->setFixedHeight(56);
		routes->addWidget(Solution);
		auto* const briefRow = new QHBoxLayout{ };
		briefRow->addStretch(1);
		SaveBrief = new QPushButton{ QStringLiteral("Save"), route };
		SaveBrief->setObjectName(QStringLiteral("localDraftSaveBrief"));
		SaveBrief->setToolTip(QStringLiteral("Save the Problem and Solution. The description is not changed"));
		briefRow->addWidget(SaveBrief);
		routes->addLayout(briefRow);
		routes->addWidget(heading(QStringLiteral("Stops"), route));
		Stops = new QListWidget{ route };
		Stops->setObjectName(QStringLiteral("localDraftStops"));
		Stops->setAccessibleName(QStringLiteral("Tour stops"));
		Stops->setMinimumHeight(120);
		routes->addWidget(Stops);
		StopTitle = new QLineEdit{ route };
		StopTitle->setObjectName(QStringLiteral("localDraftStopTitle"));
		StopTitle->setAccessibleName(QStringLiteral("Stop title"));
		StopTitle->setPlaceholderText(QStringLiteral("Title"));
		routes->addWidget(StopTitle);
		StopText = new QPlainTextEdit{ route };
		StopText->setObjectName(QStringLiteral("localDraftStopText"));
		StopText->setAccessibleName(QStringLiteral("Stop explanation"));
		StopText->setFixedHeight(96);
		StopText->setToolTip(QStringLiteral("What the reviewer should notice here. Ctrl or Cmd with Return saves"));
		routes->addWidget(StopText);
		auto* const linesRow = new QHBoxLayout{ };
		StopLines            = new QLabel{ route };
		StopLines->setObjectName(QStringLiteral("localDraftStopLines"));
		StopLines->setTextInteractionFlags(Qt::TextSelectableByMouse);
		linesRow->addWidget(StopLines, 1);
		Show = new QPushButton{ QStringLiteral("Show"), route };
		Show->setObjectName(QStringLiteral("localDraftShowStop"));
		Show->setToolTip(QStringLiteral("Show its lines in Changes"));
		UseLines = new QPushButton{ QStringLiteral("Use Selection"), route };
		UseLines->setObjectName(QStringLiteral("localDraftUseLines"));
		UseLines->setToolTip(QStringLiteral("Move the stop to the lines selected in Changes; Save keeps them"));
		Move = new QPushButton{ QStringLiteral("More"), route };
		Move->setObjectName(QStringLiteral("localDraftStopMore"));
		Move->setToolTip(QStringLiteral("Move the stop, or cite evidence you selected"));
		Move->setMenu(new QMenu{ Move });
		Remove = new QPushButton{ QStringLiteral("Remove"), route };
		Remove->setObjectName(QStringLiteral("localDraftRemoveStop"));
		AcceptStop = new QPushButton{ QStringLiteral("Accept"), route };
		AcceptStop->setObjectName(QStringLiteral("localDraftAcceptStop"));
		AcceptStop->setToolTip(QStringLiteral("Sign off this stop: its text, its lines and the evidence it cites"));
		SaveStop = new QPushButton{ QStringLiteral("Save"), route };
		SaveStop->setObjectName(QStringLiteral("localDraftSaveStop"));
		SaveStop->setToolTip(QStringLiteral("Save and accept this stop"));
		same_width({ Show, UseLines, Move, Remove, AcceptStop, SaveStop });
		for (auto* const button : std::initializer_list<QWidget*>{ Show, UseLines, Move, Remove, AcceptStop, SaveStop }) {
			linesRow->addWidget(button);
		}
		routes->addLayout(linesRow);
		routes->addStretch(1);
		scroll->setWidget(route);
		column->addWidget(scroll, 1);
		Notice = new QLabel{ this };
		Notice->setObjectName(QStringLiteral("localDraftTourNotice"));
		Notice->setWordWrap(true);
		column->addWidget(Notice);
		// The pinned agent bar.
		Chat = new local_draft_chat{ false, this };
		column->addWidget(Chat);

		connect(SaveBrief, &QPushButton::clicked, this, [this] {
			if (Dispatch != nullptr && Dispatch->SaveBrief) {
				Dispatch->SaveBrief(utf8(Problem->toPlainText()), utf8(Solution->toPlainText()));
				Problem->document()->setModified(false);
				Solution->document()->setModified(false);
			}
		});
		save_on_command_return(Problem, [this] { SaveBrief->click(); });
		save_on_command_return(Solution, [this] { SaveBrief->click(); });
		connect(AddStop, &QPushButton::clicked, this, [this] {
			const auto lines = Links != nullptr && Links->Selection ? Links->Selection() : std::nullopt;
			if (!lines || lines->First == 0U) {
				Notice->setText(QStringLiteral("Select lines in Changes first."));
				return;
			}
			if (Dispatch != nullptr && Dispatch->AddStop) {
				Dispatch->AddStop(lines->Path, lines->First, lines->Last, utf8(QStringLiteral("Stop at %1").arg(lines_text(lines->Path, lines->First, lines->Last))), std::string{ });
			}
		});
		connect(Stops, &QListWidget::currentItemChanged, this, [this] {
			PendingLines.reset();
			present_editor();
		});
		connect(Stops, &QListWidget::itemActivated, this, [this] { arrive(); });
		connect(Show, &QPushButton::clicked, this, [this] { arrive(); });
		connect(UseLines, &QPushButton::clicked, this, [this] {
			const auto lines = Links != nullptr && Links->Selection ? Links->Selection() : std::nullopt;
			if (!lines || lines->First == 0U) {
				Notice->setText(QStringLiteral("Select lines in Changes first."));
				return;
			}
			PendingLines = lines;
			StopLines->setText(lines_text(lines->Path, lines->First, lines->Last) + QStringLiteral(" (not saved)"));
		});
		const auto saveStop = [this] {
			const auto stop = selected_stop();
			if (stop == 0U || Dispatch == nullptr || !Dispatch->SaveStop) {
				return;
			}
			const auto lines = PendingLines.value_or(local_draft_lines{ });
			Dispatch->SaveStop(stop, utf8(StopTitle->text().trimmed()), utf8(StopText->toPlainText()), lines.Path, lines.First, lines.Last);
			PendingLines.reset();
			StopText->document()->setModified(false);
			StopTitle->setModified(false);
		};
		connect(SaveStop, &QPushButton::clicked, this, saveStop);
		save_on_command_return(StopText, saveStop);
		connect(AcceptStop, &QPushButton::clicked, this, [this] {
			if (const auto stop = selected_stop(); stop != 0U && Dispatch != nullptr && Dispatch->AcceptStop) {
				Dispatch->AcceptStop(stop);
			}
		});
		connect(Remove, &QPushButton::clicked, this, [this] {
			if (const auto stop = selected_stop(); stop != 0U && Dispatch != nullptr && Dispatch->RemoveStop) {
				Dispatch->RemoveStop(stop);
			}
		});
		connect(Move->menu(), &QMenu::aboutToShow, this, [this] {
			auto* const menu = Move->menu();
			menu->clear();
			const auto row   = Stops->currentRow();
			const auto count = Stops->count();
			for (const auto& [label, index] : { std::pair{ QStringLiteral("Earlier"), row - 1 }, std::pair{ QStringLiteral("Later"), row + 1 } }) {
				auto* const action = menu->addAction(label);
				action->setEnabled(row >= 0 && index >= 0 && index < count);
				connect(action, &QAction::triggered, this, [this, target = index] {
					if (const auto stop = selected_stop(); stop != 0U && Dispatch != nullptr && Dispatch->MoveStop) {
						Dispatch->MoveStop(stop, static_cast<std::size_t>(target));
					}
				});
			}
			// Cite selected evidence (X21): only what the author selected.
			if (Projection.Draft && selected_stop() != 0U) {
				const auto chosen = core::selected_evidence(*Projection.Draft);
				const auto stop   = std::ranges::find(Projection.Draft->Preparation.Tour.Stops, selected_stop(), &core::local_tour_stop::Id);
				auto*      cite   = menu->addMenu(QStringLiteral("Cite Evidence"));
				const auto add    = [&](const std::uint64_t aId, const QString& aLabel) {
					auto* const action = cite->addAction(aLabel);
					action->setCheckable(true);
					action->setChecked(std::ranges::contains(stop->Citations, aId));
					connect(action, &QAction::toggled, this, [this, aId, stopId = stop->Id](const bool aOn) {
						if (Dispatch != nullptr && Dispatch->Cite) {
							Dispatch->Cite(stopId, aId, aOn);
						}
					});
				};
				for (const auto& claim : chosen.Claims) {
					add(claim.Id, QStringLiteral("Claim: %1").arg(first_line(claim.Text)));
				}
				for (const auto& result : chosen.Results) {
					const auto check = std::ranges::find(chosen.Checks, result.Check, &core::evidence_check::Id);
					add(result.Id, QStringLiteral("Result: %1, %2").arg(check == chosen.Checks.end() ? QString{ } : text_of(check->Title), text_of(core::name(result.Outcome))));
				}
				for (const auto& question : chosen.Questions) {
					add(question.Id, QStringLiteral("Question: %1").arg(first_line(question.Text)));
				}
				cite->setEnabled(!cite->isEmpty());
			}
		});
	}

	void local_draft_tour_area::set_dispatch(local_draft_preparation_dispatch* const aDispatch, local_draft_page_links* const aLinks)
	{
		Dispatch = aDispatch;
		Links    = aLinks;
		Chat->set_dispatch(aDispatch, aLinks);
	}

	auto local_draft_tour_area::selected_stop() const -> std::uint64_t
	{
		const auto* const item = Stops->currentItem();
		return item == nullptr ? 0U : item->data(id_role).toULongLong();
	}

	void local_draft_tour_area::arrive()
	{
		if (!Projection.Draft || Links == nullptr || !Links->Reveal) {
			return;
		}
		const auto stop = std::ranges::find(Projection.Draft->Preparation.Tour.Stops, selected_stop(), &core::local_tour_stop::Id);
		if (stop == Projection.Draft->Preparation.Tour.Stops.end() || stop->Current.Correspondence != core::local_correspondence::exact) {
			Notice->setText(QStringLiteral("Its lines are not exactly in this candidate."));
			return;
		}
		Links->Reveal(stop->Source.Path, stop->Current.FirstLine, stop->Current.LastLine);
	}

	void local_draft_tour_area::present(const application::local_draft_workbench_projection& aProjection)
	{
		const auto other = !Projection.Draft || !aProjection.Draft || Projection.Draft->Id != aProjection.Draft->Id;
		Projection       = aProjection;
		if (other) {
			Editing = 0U;
			PendingLines.reset();
		}
		const auto& tour = aProjection.Draft ? aProjection.Draft->Preparation.Tour : core::local_tour{ };
		for (auto [editor, text] : { std::pair{ Problem, &tour.Problem }, std::pair{ Solution, &tour.Solution } }) {
			if (other || !editor->document()->isModified()) {
				const auto blocker = QSignalBlocker{ editor };
				if (editor->toPlainText() != text_of(*text)) {
					editor->setPlainText(text_of(*text));
				}
				editor->document()->setModified(false);
			}
			editor->setReadOnly(!aProjection.Draft);
		}
		SaveBrief->setEnabled(aProjection.Draft.has_value());
		AddStop->setEnabled(aProjection.Draft.has_value());
		if (aProjection.Draft) {
			const auto head = core::decide_local_tour_head(*aProjection.Draft);
			Head->setText(head.Stops == 0U          ? QStringLiteral("No stops yet")
							  : head.ToAccept == 0U ? QStringLiteral("%1, all accepted").arg(counted(head.Stops, "stop", "stops"))
													: QStringLiteral("%1, %2 to accept").arg(counted(head.Stops, "stop", "stops")).arg(head.ToAccept));
		} else {
			Head->clear();
		}
		present_stops();
		present_editor();
		Notice->setText(aProjection.PreparationRefusal ? preparation_refusal_text(*aProjection.PreparationRefusal) : QString{ });
		Chat->present(aProjection);
	}

	void local_draft_tour_area::present_stops()
	{
		const auto keep    = selected_stop() != 0U ? selected_stop() : Editing;
		const auto blocker = QSignalBlocker{ Stops };
		Stops->clear();
		if (!Projection.Draft) {
			return;
		}
		auto index = 0;
		for (const auto& stop : Projection.Draft->Preparation.Tour.Stops) {
			++index;
			const auto decision = core::decide_local_stop(*Projection.Draft, stop);
			const auto exact    = stop.Current.Correspondence == core::local_correspondence::exact;
			auto*      item     = new QListWidgetItem{ ring(decision.State),
													   QStringLiteral("%1. %2\n%3")
														   .arg(index)
														   .arg(text_of(stop.Title))
														   .arg((exact ? lines_text(stop.Source.Path, stop.Current.FirstLine, stop.Current.LastLine) : text_of(stop.Source.Path))
																+ (decision.State == core::tour_acceptance_state::accepted ? QString{ } : QStringLiteral("  ·  ") + reason_text(decision.Reason))),
													   Stops };
			item->setData(id_role, QVariant::fromValue<qulonglong>(stop.Id));
			item->setToolTip(reason_text(decision.Reason));
			item->setData(Qt::AccessibleDescriptionRole, reason_text(decision.Reason));
			if (stop.Id == keep) {
				Stops->setCurrentItem(item);
			}
		}
	}

	void local_draft_tour_area::present_editor()
	{
		const auto id   = selected_stop();
		const auto stop = Projection.Draft ? std::ranges::find(Projection.Draft->Preparation.Tour.Stops, id, &core::local_tour_stop::Id)
										   : std::vector<core::local_tour_stop>::const_iterator{ };
		const auto has  = Projection.Draft && id != 0U && stop != Projection.Draft->Preparation.Tour.Stops.end();
		if (id != Editing || !has || (!StopText->document()->isModified() && !StopTitle->isModified())) {
			const auto titleBlocker = QSignalBlocker{ StopTitle };
			const auto textBlocker  = QSignalBlocker{ StopText };
			StopTitle->setText(has ? text_of(stop->Title) : QString{ });
			StopText->setPlainText(has ? text_of(stop->Text) : QString{ });
			StopText->document()->setModified(false);
			StopTitle->setModified(false);
		}
		Editing = has ? id : 0U;
		for (auto* const widget : std::initializer_list<QWidget*>{ StopTitle, StopText, Show, UseLines, Move, Remove, AcceptStop, SaveStop }) {
			widget->setEnabled(has);
		}
		if (!has) {
			StopLines->clear();
			Chat->set_subject(std::string{ core::local_change_thread }, core::local_subject::whole_change, 0U);
			return;
		}
		const auto decision = core::decide_local_stop(*Projection.Draft, *stop);
		const auto exact    = stop->Current.Correspondence == core::local_correspondence::exact;
		StopLines->setText(PendingLines ? lines_text(PendingLines->Path, PendingLines->First, PendingLines->Last) + QStringLiteral(" (not saved)")
							   : exact  ? lines_text(stop->Source.Path, stop->Current.FirstLine, stop->Current.LastLine)
										: QStringLiteral("%1 (%2 in this candidate)").arg(lines_text(stop->Source.Path, stop->Source.FirstLine, stop->Source.LastLine), text_of(core::name(stop->Current.Correspondence))));
		StopLines->setToolTip(QStringLiteral("As captured on candidate %1:\n%2").arg(stop->Source.Candidate).arg(text_of(stop->Source.Excerpt)));
		AcceptStop->setEnabled(decision.State != core::tour_acceptance_state::accepted && exact);
		Show->setEnabled(exact);
		Chat->set_subject(core::local_stop_thread(stop->Id), core::local_subject::stop, stop->Id);
	}

	auto local_draft_tour_area::head() const noexcept -> QLabel* { return Head; }
	auto local_draft_tour_area::problem() const noexcept -> QPlainTextEdit* { return Problem; }
	auto local_draft_tour_area::solution() const noexcept -> QPlainTextEdit* { return Solution; }
	auto local_draft_tour_area::save_brief_button() const noexcept -> QPushButton* { return SaveBrief; }
	auto local_draft_tour_area::stops() const noexcept -> QListWidget* { return Stops; }
	auto local_draft_tour_area::add_stop_button() const noexcept -> QPushButton* { return AddStop; }
	auto local_draft_tour_area::stop_title() const noexcept -> QLineEdit* { return StopTitle; }
	auto local_draft_tour_area::stop_text() const noexcept -> QPlainTextEdit* { return StopText; }
	auto local_draft_tour_area::stop_lines() const noexcept -> QLabel* { return StopLines; }
	auto local_draft_tour_area::use_lines_button() const noexcept -> QPushButton* { return UseLines; }
	auto local_draft_tour_area::show_button() const noexcept -> QPushButton* { return Show; }
	auto local_draft_tour_area::accept_button() const noexcept -> QPushButton* { return AcceptStop; }
	auto local_draft_tour_area::save_button() const noexcept -> QPushButton* { return SaveStop; }
	auto local_draft_tour_area::remove_button() const noexcept -> QPushButton* { return Remove; }
	auto local_draft_tour_area::notice() const noexcept -> QLabel* { return Notice; }
	auto local_draft_tour_area::chat() const noexcept -> local_draft_chat* { return Chat; }
}
