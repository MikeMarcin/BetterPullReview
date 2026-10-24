#include "bpr/qt/agent_setup_view.hpp"

#include "bpr/qt/quiet_push_button.hpp"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFileDialog>
#include <QFont>
#include <QFontDatabase>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QStyleOptionButton>
#include <QStylePainter>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <tuple>
#include <utility>

namespace bpr::qt {
	namespace {
		[[nodiscard]] auto text_of(const std::string& aText) -> QString { return QString::fromUtf8(aText.data(), static_cast<qsizetype>(aText.size())); }

		[[nodiscard]] auto plain(const core::setup_agent aAgent) -> QString
		{
			const auto name = core::plain_name(aAgent);
			return QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size()));
		}

		/// A path as the reviewer reads it: their home folder as `~`.
		[[nodiscard]] auto shown_path(const std::string& aPath) -> QString
		{
			auto       path = QDir::toNativeSeparators(text_of(aPath));
			const auto home = QDir::toNativeSeparators(QDir::homePath());
			if (!home.isEmpty() && (path == home || path.startsWith(home + QDir::separator()))) {
				path.replace(0, home.size(), QStringLiteral("~"));
			}
			return path;
		}

		[[nodiscard]] auto mixed(const QColor& aFrom, const QColor& aTo, const qreal aShare) -> QColor
		{
			const auto keep = 1.0 - aShare;
			return QColor::fromRgbF(static_cast<float>(aFrom.redF() * keep + aTo.redF() * aShare), static_cast<float>(aFrom.greenF() * keep + aTo.greenF() * aShare),
									static_cast<float>(aFrom.blueF() * keep + aTo.blueF() * aShare));
		}

		[[nodiscard]] auto dark(const QPalette& aPalette) -> bool { return aPalette.color(QPalette::Window).lightness() < 128; }

		[[nodiscard]] auto passed_colour(const QPalette& aPalette) -> QColor { return dark(aPalette) ? QColor{ 0x3F, 0xB9, 0x50 } : QColor{ 0x1A, 0x7F, 0x37 }; }
		[[nodiscard]] auto failed_colour(const QPalette& aPalette) -> QColor { return dark(aPalette) ? QColor{ 0xF8, 0x51, 0x49 } : QColor{ 0xCF, 0x22, 0x2E }; }

		/// \brief A step's marker: its number while open or ahead, a tick once
		/// done. The steps are a real sequence, so they are numbered.
		class step_marker final : public QWidget
		{
		public:
			enum class state : std::uint8_t {
				ahead,
				open,
				done,
			};
			step_marker(const int aNumber, const state aState, QWidget* aParent)
				: QWidget{ aParent }
				, Number{ aNumber }
				, State{ aState }
			{
				setFixedSize(24, 24);
				setAccessibleName(aState == state::done ? QObject::tr("Done") : QObject::tr("Step %1").arg(aNumber));
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto box    = QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0);
				const auto accent = palette().color(QPalette::Active, QPalette::Highlight);
				const auto ink    = palette().color(QPalette::Active, QPalette::WindowText);
				if (State == state::done) {
					painter.setPen(Qt::NoPen);
					painter.setBrush(passed_colour(palette()));
					painter.drawEllipse(box);
					auto tick = QPen{ QColor{ Qt::white }, 2.0 };
					tick.setCapStyle(Qt::RoundCap);
					tick.setJoinStyle(Qt::RoundJoin);
					painter.setPen(tick);
					const auto c = box.center();
					painter.drawPolyline(std::array{ QPointF{ c.x() - 4.5, c.y() }, QPointF{ c.x() - 1.5, c.y() + 3.0 }, QPointF{ c.x() + 4.5, c.y() - 3.5 } }.data(), 3);
					return;
				}
				painter.setPen(State == state::open ? QPen{ Qt::NoPen } : QPen{ mixed(palette().color(QPalette::Window), ink, 0.35), 1.2 });
				painter.setBrush(State == state::open ? accent : QBrush{ Qt::NoBrush });
				painter.drawEllipse(box);
				auto number = font();
				number.setWeight(QFont::DemiBold);
				number.setPointSizeF(std::max(1.0, number.pointSizeF() * 0.85));
				painter.setFont(number);
				painter.setPen(State == state::open ? palette().color(QPalette::Active, QPalette::HighlightedText) : mixed(palette().color(QPalette::Window), ink, 0.55));
				painter.drawText(box, Qt::AlignCenter, QString::number(Number));
			}

		private:
			int   Number;
			state State;
		};

		/// \brief A choice card: an agent or a clone, the chosen one outlined.
		/// \brief A choice card: an agent or a clone, the chosen one outlined.
		/// \details Owner look v002 ("Why do I have to click a little circle
		/// instead of the whole card"): the whole card chooses, by a click or,
		/// with the keyboard focus on it, Space or Return. The radio inside only
		/// shows the choice.
		class choice_card final : public QWidget
		{
		public:
			explicit choice_card(QWidget* aParent)
				: QWidget{ aParent }
			{
				setFocusPolicy(Qt::StrongFocus);
				setCursor(Qt::PointingHandCursor);
			}
			bool                  Chosen   = false;
			bool                  Disabled = false;
			std::function<void()> Activate;

			void settle()
			{
				setFocusPolicy(Disabled ? Qt::NoFocus : Qt::StrongFocus);
				setCursor(Disabled ? Qt::ArrowCursor : Qt::PointingHandCursor);
				for (auto* const radio : findChildren<QRadioButton*>()) {
					radio->setFocusPolicy(Qt::NoFocus);
					radio->setAttribute(Qt::WA_TransparentForMouseEvents, true);
				}
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto base   = palette().color(QPalette::Active, QPalette::Base);
				const auto ink    = palette().color(QPalette::Active, QPalette::WindowText);
				const auto accent = palette().color(QPalette::Active, QPalette::Highlight);
				const auto box    = QRectF{ rect() }.adjusted(0.75, 0.75, -0.75, -0.75);
				const auto ring   = Chosen || hasFocus();
				painter.setPen(QPen{ ring ? accent : mixed(base, ink, 0.16), hasFocus() ? 2.0 : (Chosen ? 1.5 : 1.0) });
				painter.setBrush(Chosen ? mixed(base, accent, 0.06) : (Disabled ? mixed(base, ink, 0.03) : (Hovered ? mixed(base, accent, 0.03) : base)));
				painter.drawRoundedRect(box, 8.0, 8.0);
			}
			void mouseReleaseEvent(QMouseEvent* aEvent) override
			{
				if (aEvent->button() == Qt::LeftButton && rect().contains(aEvent->position().toPoint()) && !Disabled && Activate) {
					Activate();
					return;
				}
				QWidget::mouseReleaseEvent(aEvent);
			}
			void keyPressEvent(QKeyEvent* aEvent) override
			{
				if ((aEvent->key() == Qt::Key_Space || aEvent->key() == Qt::Key_Return || aEvent->key() == Qt::Key_Enter) && !Disabled && Activate) {
					Activate();
					return;
				}
				QWidget::keyPressEvent(aEvent);
			}
			void enterEvent(QEnterEvent* aEvent) override
			{
				Hovered = true;
				update();
				QWidget::enterEvent(aEvent);
			}
			void leaveEvent(QEvent* aEvent) override
			{
				Hovered = false;
				update();
				QWidget::leaveEvent(aEvent);
			}
			void focusInEvent(QFocusEvent* aEvent) override
			{
				update();
				QWidget::focusInEvent(aEvent);
			}
			void focusOutEvent(QFocusEvent* aEvent) override
			{
				update();
				QWidget::focusOutEvent(aEvent);
			}

		private:
			bool Hovered = false;
		};

		/// \brief A step's header row. A step already reached opens with a
		/// click or, focused, Space or Return (owner look v002: "click (2) to
		/// continue").
		class step_header final : public QWidget
		{
		public:
			explicit step_header(QWidget* aParent)
				: QWidget{ aParent }
			{ }
			std::function<void()> Activate;

			void settle()
			{
				setFocusPolicy(Activate ? Qt::StrongFocus : Qt::NoFocus);
				setCursor(Activate ? Qt::PointingHandCursor : Qt::ArrowCursor);
			}

		protected:
			void mouseReleaseEvent(QMouseEvent* aEvent) override
			{
				if (aEvent->button() == Qt::LeftButton && Activate && rect().contains(aEvent->position().toPoint())) {
					Activate();
					return;
				}
				QWidget::mouseReleaseEvent(aEvent);
			}
			void keyPressEvent(QKeyEvent* aEvent) override
			{
				if ((aEvent->key() == Qt::Key_Space || aEvent->key() == Qt::Key_Return || aEvent->key() == Qt::Key_Enter) && Activate) {
					Activate();
					return;
				}
				QWidget::keyPressEvent(aEvent);
			}
			void paintEvent(QPaintEvent* aEvent) override
			{
				QWidget::paintEvent(aEvent);
				if (hasFocus()) {
					auto painter = QPainter{ this };
					painter.setRenderHint(QPainter::Antialiasing, true);
					painter.setPen(QPen{ palette().color(QPalette::Active, QPalette::Highlight), 1.5 });
					painter.drawRoundedRect(QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0), 6.0, 6.0);
				}
			}
			void focusInEvent(QFocusEvent* aEvent) override
			{
				update();
				QWidget::focusInEvent(aEvent);
			}
			void focusOutEvent(QFocusEvent* aEvent) override
			{
				update();
				QWidget::focusOutEvent(aEvent);
			}
		};

		[[nodiscard]] auto muted(const QString& aText, QWidget* aParent) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			label->setWordWrap(true);
			label->setTextFormat(Qt::PlainText);
			label->setForegroundRole(QPalette::PlaceholderText);
			return label;
		}

		[[nodiscard]] auto words(const QString& aText, QWidget* aParent) -> QLabel*
		{
			auto* const label = new QLabel{ aText, aParent };
			label->setWordWrap(true);
			label->setTextFormat(Qt::PlainText);
			return label;
		}

		[[nodiscard]] auto code_font(const QFont& aFont) -> QFont
		{
			auto code = QFontDatabase::systemFont(QFontDatabase::FixedFont);
			code.setPointSizeF(std::max(1.0, aFont.pointSizeF() * 0.92));
			return code;
		}

		/// \brief A flat button drawn in the palette's link colour, read at
		/// each paint so it follows light and dark.
		class link_push final : public QPushButton
		{
		public:
			link_push(const QString& aText, QWidget* aParent)
				: QPushButton{ aText, aParent }
			{
				setFlat(true);
				setCursor(Qt::PointingHandCursor);
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QStylePainter{ this };
				auto option  = QStyleOptionButton{ };
				initStyleOption(&option);
				option.palette.setColor(QPalette::ButtonText, palette().color(QPalette::Active, QPalette::Link));
				painter.drawControl(QStyle::CE_PushButton, option);
			}
		};

		[[nodiscard]] auto link_button(const QString& aText, QWidget* aParent, const QString& aName) -> QPushButton*
		{
			auto* const button = new link_push{ aText, aParent };
			button->setObjectName(aName);
			return button;
		}

		[[nodiscard]] auto primary_button(const QString& aText, QWidget* aParent, const QString& aName) -> QPushButton*
		{
			auto* const button = new QPushButton{ aText, aParent };
			button->setObjectName(aName);
			button->setDefault(true);
			return button;
		}
	} // namespace

	auto workspace_unavailable_words(const application::agent_workspace_unavailable_reason aReason) -> QString
	{
		using reason = application::agent_workspace_unavailable_reason;
		switch (aReason) {
		case reason::copy_failed:
			return QObject::tr("The agent's copy of this pull request could not be made. Reopen the pull request, then check again.");
		case reason::download_failed:
			return QObject::tr("The agent needs every file at this head, and this review holds only the files it shows. Downloading the rest failed. Check again, or show the log for the cause.");
		case reason::connection_failed:
			return QObject::tr("The agent needs every file at this head, and GitHub could not be reached to download them. Check the connection, then check again.");
		case reason::sign_in_refused:
			return QObject::tr("The agent needs every file at this head, and GitHub refused to send them to the signed-in account. Check that gh is signed in with access to this repository, then check again.");
		case reason::too_large:
			return QObject::tr("The agent needs every file at this head, and this head is larger than the agent's private copy allows.");
		}
		return { };
	}

	auto agent_ready_gets(const agent_ready_facts& aFacts) -> QString
	{
		auto lines = QStringList{ };
		lines << QObject::tr("Pull request: %1 #%2, \u201c%3\u201d, and its description").arg(text_of(aFacts.Repository)).arg(aFacts.Number).arg(text_of(aFacts.Title));
		lines << QObject::tr("Base and head: %1 at %2, head %3").arg(text_of(aFacts.BaseBranch), text_of(aFacts.Base), text_of(aFacts.Head));
		lines << (aFacts.Sparse && !aFacts.Clone ? QObject::tr("Changes: %1 files; no base to compare with in its copy").arg(aFacts.Files)
												 : QObject::tr("Changes: %1 files; the diff is git diff %2...%3 in its workspace").arg(aFacts.Files).arg(text_of(aFacts.Base), text_of(aFacts.Head)));
		lines << QObject::tr("Threads: %1 review threads, %2 unresolved").arg(aFacts.Threads).arg(aFacts.Unresolved);
		lines << (aFacts.Clone ? QObject::tr("Workspace: its own, at the head, with the history of %1").arg(shown_path(*aFacts.Clone))
							   : QObject::tr("Workspace: a private copy at the head"));
		lines << QObject::tr("Review skill: bpr-pr-review, editable in Settings");
		lines << QObject::tr("It may not: push, comment or change your checkout");
		return lines.join(QLatin1Char{ '\n' });
	}

	auto sign_in_command(const core::setup_agent aAgent) -> QString
	{
		switch (aAgent) {
		case core::setup_agent::claude_code:        return QStringLiteral("claude auth login");
		case core::setup_agent::codex:              return QStringLiteral("codex login");
		case core::setup_agent::plain_text_harness: break;
		}
		return { };
	}

	auto status_command(const core::setup_agent aAgent) -> QString
	{
		switch (aAgent) {
		case core::setup_agent::claude_code:        return QStringLiteral("claude auth status");
		case core::setup_agent::codex:              return QStringLiteral("codex login status");
		case core::setup_agent::plain_text_harness: return QStringLiteral("--version");
		}
		return { };
	}

	auto setup_failure_words(const core::agent_setup_failure aFailure, const QString& aAgent, const QString& aDetail) -> QString
	{
		using failure = core::agent_setup_failure;
		switch (aFailure) {
		case failure::agent_not_found:       return QObject::tr("%1 isn't where it was found any more. Look again, or choose its program in Settings.").arg(aAgent);
		case failure::program_refused:       return QObject::tr("%1's program can't be used: %2").arg(aAgent, aDetail.isEmpty() ? QObject::tr("it is not an absolute path to a file this account can run.") : aDetail);
		case failure::no_program_chosen:     return QObject::tr("Choose the program for %1 in Settings first.").arg(aAgent);
		case failure::not_signed_in:         return QObject::tr("%1 isn't signed in. Sign in from a terminal, then check again.").arg(aAgent);
		case failure::failed_to_start:       return QObject::tr("%1 didn't start.").arg(aAgent) + (aDetail.isEmpty() ? QString{ } : QStringLiteral(" ") + aDetail);
		case failure::status_unreadable:     return QObject::tr("%1 answered, but not in a way BetterPullReview can read. Check again, or show the details.").arg(aAgent);
		case failure::timed_out:             return QObject::tr("%1 didn't answer within 15 seconds. Check again.").arg(aAgent);
		case failure::workspace_unavailable: return QObject::tr("The agent's workspace could not be made.");
		}
		return { };
	}

	agent_setup_view::agent_setup_view(QWidget* aParent)
		: QWidget{ aParent }
	{
		setObjectName(QStringLiteral("agentSetup"));
		setAccessibleName(tr("Set up an agent"));
		auto* const outer = new QVBoxLayout{ this };
		outer->setContentsMargins(0, 0, 0, 0);
		auto* const scroll = new QScrollArea{ this };
		scroll->setObjectName(QStringLiteral("agentSetupScroll"));
		scroll->setWidgetResizable(true);
		scroll->setFrameShape(QFrame::NoFrame);
		scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		auto* const content = new QWidget{ scroll };
		auto* const column  = new QVBoxLayout{ content };
		column->setContentsMargins(16, 14, 16, 12);
		column->setSpacing(10);
		auto* const title = new QLabel{ tr("Set up an agent"), content };
		title->setObjectName(QStringLiteral("agentSetupTitle"));
		auto heading = title->font();
		heading.setPointSizeF(heading.pointSizeF() * 1.25);
		heading.setWeight(QFont::DemiBold);
		title->setFont(heading);
		column->addWidget(title);
		Intro = muted(tr("Ask an agent you already use about this pull request. It runs on this computer with its own sign-in, in its own copy of the pull request at its head. Nothing it says is published unless you make it a comment."), content);
		Intro->setObjectName(QStringLiteral("agentSetupIntro"));
		column->addWidget(Intro);
		Steps = new QVBoxLayout{ };
		Steps->setSpacing(8);
		column->addLayout(Steps);
		column->addStretch(1);
		scroll->setWidget(content);
		outer->addWidget(scroll, 1);

		auto* const footer = new QWidget{ this };
		footer->setObjectName(QStringLiteral("agentSetupFooter"));
		auto* const row = new QHBoxLayout{ footer };
		row->setContentsMargins(12, 6, 12, 8);
		auto* const again = link_button(tr("Look again"), footer, QStringLiteral("agentSetupLookAgain"));
		again->setToolTip(tr("Look again for agents and for clones in your clone folders. Nothing runs."));
		connect(again, &QPushButton::clicked, this, [this] {
			if (Intents.LookAgain) {
				Intents.LookAgain();
			}
		});
		row->addWidget(again);
		row->addStretch(1);
		auto* const settings = link_button(tr("Agent settings"), footer, QStringLiteral("agentSetupSettings"));
		connect(settings, &QPushButton::clicked, this, [this] {
			if (Intents.OpenSettings) {
				Intents.OpenSettings();
			}
		});
		row->addWidget(settings);
		outer->addWidget(footer);
		rebuild();
	}

	void agent_setup_view::set_intents(agent_setup_intents aIntents) { Intents = std::move(aIntents); }

	void agent_setup_view::set_change_observer(std::function<void()> aObserver) { Changed = std::move(aObserver); }

	void agent_setup_view::present(const application::agent_setup_projection& aSetup, const application::clone_link_projection& aClones, const agent_setup_facts& aFacts)
	{
		Setup  = aSetup;
		Clones = aClones;
		Facts  = aFacts;
		// The setup shows once the host knows a step is missing: the agents
		// were looked for, and the repository's stored link has been read and
		// checked again.
		const auto known   = Setup.Detected && Clones.Loaded && !Clones.Verifying;
		const auto missing = Setup.Step == core::setup_step::agent || Setup.Step == core::setup_step::repository;
		if (known && missing) {
			Active = true;
		}
		if (Active && Setup.Step == core::setup_step::ready && !Editing) {
			Active = false;
			Ready  = true;
		}
		if (Editing && *Editing == core::setup_step::agent && Picked && Setup.State.Chosen == Picked) {
			Editing.reset();
		}
		if (Editing && *Editing == core::setup_step::repository && Setup.State.RepositoryChosen && Clones.Linked) {
			Editing.reset();
		}
		rebuild();
		if (Changed) {
			Changed();
		}
	}

	void agent_setup_view::begin(const core::setup_step aStep)
	{
		Active  = true;
		Ready   = false;
		Editing = aStep == core::setup_step::ready ? std::optional<core::setup_step>{ } : std::optional{ aStep };
		Picked.reset();
		rebuild();
		if (Changed) {
			Changed();
		}
	}

	auto agent_setup_view::take_ready() -> bool { return std::exchange(Ready, false); }

	auto agent_setup_view::agent_name() const -> QString { return Setup.State.Chosen ? name_for(*Setup.State.Chosen) : tr("The agent"); }

	auto agent_setup_view::name_for(const core::setup_agent aAgent) const -> QString
	{
		const auto& preferences = Setup.Preferences;
		const auto  found       = std::ranges::find(preferences.Agents, preferences.PreferredAgent, &core::agent_entry::Id);
		if (preferences.PreferredAgent != 0U && found != preferences.Agents.end() && core::setup_agent_of(found->Protocol) == aAgent) {
			return text_of(found->Name);
		}
		return plain(aAgent);
	}

	void agent_setup_view::set_entry_editor(agent_entry_editor aEditor) { Editor = std::move(aEditor); }

	void agent_setup_view::add_custom_agent()
	{
		if (!Editor || !Intents.AddCustomAgent) {
			return;
		}
		if (auto entry = Editor(core::agent_entry{ }, this)) {
			Intents.AddCustomAgent(std::move(*entry));
		}
	}

	auto agent_setup_view::workspace_words() const -> QString
	{
		if (Clones.Linked && !Clones.Linked->PrivateCopy) {
			return tr("a workspace with the history of %1 at %2").arg(shown_path(Clones.Linked->Path), text_of(Facts.Head));
		}
		return tr("a private copy at %1").arg(text_of(Facts.Head));
	}

	auto agent_setup_view::ready_title() const -> QString { return tr("%1 is ready").arg(agent_name()); }

	auto agent_setup_view::ready_text() const -> QString
	{
		const auto& version  = Setup.State.AgentCheck.Version;
		const auto  signedIn = version.empty() ? tr("Signed in.") : tr("Signed in, version %1.").arg(text_of(version));
		auto        where    = workspace_words();
		if (!where.isEmpty()) {
			where[0] = where[0].toUpper();
		}
		return signedIn + QStringLiteral(" ") + tr("Works in %1.").arg(workspace_words());
	}

	auto agent_setup_view::step_text() const -> QString
	{
		auto       text    = QString{ };
		const auto collect = [&text](const QObject* aObject, const auto& aSelf) -> void {
			for (const auto* const child : aObject->children()) {
				if (const auto* const label = qobject_cast<const QLabel*>(child); label != nullptr && !label->text().isEmpty()) {
					text += label->text() + QLatin1Char{ '\n' };
				}
				if (const auto* const button = qobject_cast<const QPushButton*>(child); button != nullptr && !button->text().isEmpty()) {
					text += QStringLiteral("[") + button->text() + QStringLiteral("]\n");
				}
				aSelf(child, aSelf);
			}
		};
		collect(this, collect);
		return text;
	}

	void agent_setup_view::rebuild()
	{
		while (auto* const item = Steps->takeAt(0)) {
			if (auto* const widget = item->widget()) {
				// Out of the view at once, so nothing finds a step that is
				// gone; deleted once any press inside it has returned.
				widget->hide();
				widget->setParent(nullptr);
				widget->deleteLater();
			}
			delete item;
		}
		const auto current = Setup.Step == core::setup_step::ready ? core::setup_step::check : Setup.Step;
		Open               = Editing.value_or(current);
		struct step
		{
			core::setup_step Step;
			QString          Title;
			QString          Summary;
		};
		const auto repository = Clones.Linked ? (Clones.Linked->PrivateCopy ? tr("A private copy") : shown_path(Clones.Linked->Path)) : QString{ };
		const auto steps      = std::array{
			step{ core::setup_step::agent, tr("Agent"), Setup.State.Chosen ? name_for(*Setup.State.Chosen) : QString{ } },
			step{ core::setup_step::repository, tr("Repository"), repository },
			step{ core::setup_step::check, tr("Check it works"), QString{ } },
			step{ core::setup_step::ready, tr("Ready"), QString{ } },
		};
		for (auto index = std::size_t{ 0U }; index < steps.size(); ++index) {
			const auto& item  = steps[index];
			const auto  order = static_cast<int>(item.Step);
			const auto  done  = order < static_cast<int>(current) || (item.Step == core::setup_step::check && Setup.Step == core::setup_step::ready);
			const auto  open  = item.Step == Open;
			auto* const block = new QWidget{ this };
			block->setObjectName(QStringLiteral("agentSetupStep%1").arg(index + 1U));
			auto* const blockColumn = new QVBoxLayout{ block };
			blockColumn->setContentsMargins(0, 0, 0, 0);
			blockColumn->setSpacing(6);
			auto* const headerRow = new step_header{ block };
			headerRow->setObjectName(QStringLiteral("agentSetupStepHeader"));
			auto* const header = new QHBoxLayout{ headerRow };
			header->setContentsMargins(0, 2, 0, 2);
			header->setSpacing(10);
			header->addWidget(new step_marker{ order + 1, done && !open ? step_marker::state::done : open ? step_marker::state::open
																										  : step_marker::state::ahead,
											   block });
			auto* const name = new QLabel{ item.Title, block };
			auto        bold = name->font();
			bold.setWeight(QFont::DemiBold);
			name->setFont(bold);
			if (!open && !done) {
				name->setForegroundRole(QPalette::PlaceholderText);
			}
			header->addWidget(name);
			if (done && !open && !item.Summary.isEmpty()) {
				auto* const summary = muted(item.Summary, block);
				summary->setWordWrap(false);
				summary->setObjectName(QStringLiteral("agentSetupSummary"));
				// A long path gives way to Change rather than widening the step.
				summary->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
				summary->setToolTip(item.Summary);
				header->addWidget(summary, 1);
			} else {
				header->addStretch(1);
			}
			if (done && !open && item.Step != core::setup_step::check && item.Step != core::setup_step::ready) {
				auto* const change = link_button(tr("Change"), block, QStringLiteral("agentSetupChange"));
				change->setAccessibleName(tr("Change the %1").arg(item.Title.toLower()));
				const auto target = item.Step;
				connect(change, &QPushButton::clicked, this, [this, target] {
					Editing = target;
					Picked.reset();
					PickedClone.reset();
					if (target == core::setup_step::repository && Intents.LookAgain) {
						Intents.LookAgain();
					}
					rebuild();
				});
				header->addWidget(change);
			}
			// A step reached and not open opens from its header: the current
			// step closes any Change, an earlier one is changed.
			const auto reached = order <= static_cast<int>(current) && item.Step != core::setup_step::ready;
			if (reached && !open) {
				const auto target   = item.Step;
				headerRow->Activate = [this, target, current] {
					if (target == current) {
						Editing.reset();
					} else {
						Editing = target;
						if (target == core::setup_step::repository && Intents.LookAgain) {
							Intents.LookAgain();
						}
					}
					Picked.reset();
					rebuild();
				};
				headerRow->setToolTip(target == current ? tr("Continue setting up") : tr("Change this step"));
			}
			headerRow->settle();
			blockColumn->addWidget(headerRow);
			if (open) {
				auto* const body       = new QWidget{ block };
				auto* const bodyColumn = new QVBoxLayout{ body };
				bodyColumn->setContentsMargins(34, 0, 0, 4);
				bodyColumn->setSpacing(8);
				switch (item.Step) {
				case core::setup_step::agent:      build_agent(*bodyColumn); break;
				case core::setup_step::repository: build_repository(*bodyColumn); break;
				case core::setup_step::check:      build_check(*bodyColumn); break;
				case core::setup_step::ready:      break;
				}
				blockColumn->addWidget(body);
			}
			Steps->addWidget(block);
		}
	}

	void agent_setup_view::build_agent(QVBoxLayout& aBody)
	{
		auto* const parent = aBody.parentWidget();
		if (!Setup.Detected || Setup.Detecting) {
			aBody.addWidget(muted(tr("Looking for agents on this computer…"), parent));
			return;
		}
		const auto& rows = Setup.State.Rows;
		if (rows.empty()) {
			auto* const none = words(tr("No agent found on this computer"), parent);
			none->setObjectName(QStringLiteral("agentSetupNoneFound"));
			auto bold = none->font();
			bold.setWeight(QFont::DemiBold);
			none->setFont(bold);
			aBody.addWidget(none);
			auto searched = QStringList{ };
			for (const auto agent : Setup.Searched) {
				if (agent != core::setup_agent::plain_text_harness) {
					searched << plain(agent);
				}
			}
			aBody.addWidget(muted(tr("Looked for %1 on PATH and in the usual install folders. Nothing was run.").arg(searched.join(tr(" and "))), parent));
			auto* const installs = new QHBoxLayout{ };
			for (const auto& [label, agent, name] : { std::tuple{ tr("Install Claude Code"), core::setup_agent::claude_code, QStringLiteral("agentSetupInstallClaude") },
													  std::tuple{ tr("Install Codex"), core::setup_agent::codex, QStringLiteral("agentSetupInstallCodex") } }) {
				auto* const button = new quiet_push_button{ label, parent };
				button->setObjectName(name);
				const auto target = agent;
				connect(button, &QPushButton::clicked, this, [this, target] {
					if (Intents.OpenInstallPage) {
						Intents.OpenInstallPage(target);
					}
				});
				installs->addWidget(button);
			}
			installs->addStretch(1);
			aBody.addLayout(installs);
			// Owner amendment 2026-10-23: a wrapper of the reviewer's own is a
			// custom agent, described here and added to the list.
			auto* const chosen = link_button(tr("Custom agent…"), parent, QStringLiteral("agentSetupCustomAgent"));
			chosen->setToolTip(tr("Use a program of your own, such as a wrapper, with fixed arguments"));
			connect(chosen, &QPushButton::clicked, this, [this] { add_custom_agent(); });
			aBody.addWidget(chosen, 0, Qt::AlignLeft);
			return;
		}
		const auto chosen = Picked ? Picked : Setup.State.Chosen;
		for (const auto& row : rows) {
			auto* const card = new choice_card{ parent };
			card->setObjectName(QStringLiteral("agentSetupAgentRow"));
			card->Chosen     = chosen == row.Agent;
			card->Disabled   = !row.Verdict.Permitted;
			auto* const grid = new QHBoxLayout{ card };
			grid->setContentsMargins(12, 10, 12, 10);
			grid->setSpacing(10);
			auto* const radio = new QRadioButton{ card };
			radio->setObjectName(QStringLiteral("agentSetupAgentChoice"));
			radio->setChecked(card->Chosen);
			radio->setEnabled(row.Verdict.Permitted);
			radio->setAccessibleName(name_for(row.Agent));
			grid->addWidget(radio, 0, Qt::AlignTop);
			auto* const lines = new QVBoxLayout{ };
			lines->setSpacing(2);
			auto* const name = words(name_for(row.Agent), card);
			auto        bold = name->font();
			bold.setWeight(QFont::DemiBold);
			name->setFont(bold);
			lines->addWidget(name);
			auto* const path = words(shown_path(row.Path), card);
			path->setFont(code_font(font()));
			path->setToolTip(text_of(row.Path));
			lines->addWidget(path);
			auto how = QString{ };
			switch (row.How) {
			case core::agent_found_how::on_path:       how = tr("Found on PATH."); break;
			case core::agent_found_how::usual_folder:  how = tr("Found in a usual install folder."); break;
			case core::agent_found_how::saved_program: how = row.Agent == core::setup_agent::claude_code ? tr("Your process wrapper.") : tr("The program you chose."); break;
			}
			if (!row.Verdict.Permitted) {
				how = tr("Not allowed for this pull request's host.");
			}
			lines->addWidget(muted(how, card));
			grid->addLayout(lines, 1);
			const auto agent = row.Agent;
			card->Activate   = [this, agent] {
				Picked = agent;
				if (Setup.State.Chosen == agent) {
					// The agent already chosen: keep it and go on.
					Editing.reset();
					rebuild();
					return;
				}
				if (Intents.Choose) {
					Intents.Choose(agent);
				}
			};
			card->setAccessibleName(name_for(row.Agent));
			card->settle();
			aBody.addWidget(card);
		}
		// Owner look v002 ("If I click change on agent it really makes me
		// change it"): changing is optional; the agent already chosen stays a
		// choice and Continue goes on with it.
		if (Editing == core::setup_step::agent && Setup.State.Chosen) {
			auto* const keep = primary_button(tr("Continue with %1").arg(name_for(*Setup.State.Chosen)), parent, QStringLiteral("agentSetupContinue"));
			connect(keep, &QPushButton::clicked, this, [this] {
				Editing.reset();
				Picked.reset();
				rebuild();
			});
			aBody.addWidget(keep, 0, Qt::AlignLeft);
		}
		auto* const other = link_button(tr("Custom agent…"), parent, QStringLiteral("agentSetupCustomAgent"));
		other->setToolTip(tr("Use a program of your own, such as a wrapper, with fixed arguments"));
		connect(other, &QPushButton::clicked, this, [this] { add_custom_agent(); });
		aBody.addWidget(other, 0, Qt::AlignLeft);
	}

	void agent_setup_view::build_repository(QVBoxLayout& aBody)
	{
		auto* const parent = aBody.parentWidget();
		const auto  name   = text_of(Facts.Repository);
		if (!Clones.Repository || name.isEmpty()) {
			aBody.addWidget(muted(tr("Open a pull request to choose the repository the agent reads."), parent));
			return;
		}
		if (Clones.Problem != application::clone_link_problem::none && !Clones.Links.empty()) {
			const auto* const link = [this]() -> const application::repository_link* {
				for (const auto& candidate : Clones.Links) {
					if (candidate.Owner + "/" + candidate.Name == Facts.Repository && !candidate.PrivateCopy) {
						return &candidate;
					}
				}
				return nullptr;
			}();
			const auto  where   = link != nullptr ? shown_path(link->Path) : tr("The linked folder");
			auto* const problem = words(Clones.Problem == application::clone_link_problem::not_a_clone ? tr("%1 is no longer a clone.").arg(where)
																									   : tr("%1 no longer has a remote for %2.").arg(where, name),
										parent);
			problem->setObjectName(QStringLiteral("agentSetupLinkProblem"));
			aBody.addWidget(problem);
		}
		const auto chooseFolder = [this] {
			const auto folder = QFileDialog::getExistingDirectory(this, tr("Choose the clone's folder"), QDir::homePath());
			if (!folder.isEmpty() && Intents.ChooseClone) {
				Intents.ChooseClone(QDir::toNativeSeparators(folder).toStdString());
			}
		};
		const auto privateCopy = [this, parent](const QString& aText) {
			auto* const button = link_button(aText, parent, QStringLiteral("agentSetupPrivateCopy"));
			button->setToolTip(Facts.Sparse ? tr("The head's files only, without history, branches, tags, LFS files or submodules")
											: tr("The head and its history, without your branches, tags, LFS files or submodules"));
			connect(button, &QPushButton::clicked, this, [this] {
				if (Intents.UsePrivateCopy) {
					Intents.UsePrivateCopy();
				}
			});
			return button;
		};
		if (Clones.Folders.empty()) {
			auto* const ask = words(tr("Where do your clones live?"), parent);
			ask->setObjectName(QStringLiteral("agentSetupNoFolders"));
			auto bold = ask->font();
			bold.setWeight(QFont::DemiBold);
			ask->setFont(bold);
			aBody.addWidget(ask);
			aBody.addWidget(muted(tr("The agent reads more than the diff: callers, tests, history. Add the folder your clones are in, and BetterPullReview looks there for a clone of %1. It reads only each clone's Git configuration and runs nothing.").arg(name), parent));
			// Owner look v003 ("it's fine to just add a single folder line
			// from there. The list editor was meant for settings page"): one
			// folder, typed or browsed, then Add.
			auto* const line = new QHBoxLayout{ };
			line->setSpacing(6);
			auto* const path = new QLineEdit{ parent };
			path->setObjectName(QStringLiteral("agentSetupFolderPath"));
			path->setPlaceholderText(QDir::toNativeSeparators(QStringLiteral("~/Documents/GitHub")));
			path->setAccessibleName(tr("The folder your clones are in"));
			line->addWidget(path, 1);
			auto* const browse = new quiet_push_button{ tr("Browse…"), parent };
			browse->setObjectName(QStringLiteral("agentSetupFolderBrowse"));
			line->addWidget(browse);
			auto* const add = primary_button(tr("Add"), parent, QStringLiteral("agentSetupFolderAdd"));
			add->setEnabled(false);
			line->addWidget(add);
			aBody.addLayout(line);
			auto* const status = muted({ }, parent);
			status->setObjectName(QStringLiteral("agentSetupFolderStatus"));
			status->hide();
			aBody.addWidget(status);
			connect(path, &QLineEdit::textChanged, add, [add, status](const QString& aText) {
				add->setEnabled(!aText.trimmed().isEmpty());
				status->hide();
			});
			connect(browse, &QPushButton::clicked, this, [this, path] {
				const auto folder = QFileDialog::getExistingDirectory(this, tr("Choose the folder your clones are in"), QDir::homePath());
				if (!folder.isEmpty()) {
					path->setText(QDir::toNativeSeparators(folder));
				}
			});
			const auto submit = [this, path, status] {
				auto typed = path->text().trimmed();
				if (typed == QStringLiteral("~") || typed.startsWith(QStringLiteral("~/")) || typed.startsWith(QStringLiteral("~\\"))) {
					typed = QDir::homePath() + typed.mid(1);
				}
				if (typed.isEmpty()) {
					return;
				}
				if (!QDir::isAbsolutePath(typed)) {
					status->setText(tr("Type the folder's full path, such as %1.").arg(QDir::toNativeSeparators(QDir::homePath() + QStringLiteral("/Documents/GitHub"))));
					status->show();
					return;
				}
				if (Intents.AddCloneFolder) {
					Intents.AddCloneFolder(QDir::toNativeSeparators(QDir::cleanPath(typed)).toStdString());
				}
			};
			connect(add, &QPushButton::clicked, this, submit);
			connect(path, &QLineEdit::returnPressed, this, submit);
			// Owner look v004 ("get rid of the or add it's just confusing.
			// Just let the add button be clickable with the the prefilled
			// text"): the folder found likeliest is the line's text, so Add
			// takes it as it is; Browse… or typing changes it.
			if (!Clones.Suggestions.empty()) {
				path->setText(shown_path(Clones.Suggestions.front()));
				path->setToolTip(tr("Found on this computer: %1").arg(text_of(Clones.Suggestions.front())));
			}
			aBody.addWidget(privateCopy(tr("Use a private copy instead")), 0, Qt::AlignLeft);
			return;
		}
		if (Clones.Discovering || !Clones.Discovered) {
			aBody.addWidget(muted(tr("Looking in your clone folders for %1…").arg(name), parent));
			return;
		}
		if (Clones.Matches.empty()) {
			auto* const none = words(tr("No clone of %1 in your clone folders").arg(name), parent);
			none->setObjectName(QStringLiteral("agentSetupNoClone"));
			auto bold = none->font();
			bold.setWeight(QFont::DemiBold);
			none->setFont(bold);
			aBody.addWidget(none);
			auto looked = QStringList{ };
			for (const auto& folder : Clones.Folders) {
				looked << shown_path(folder);
			}
			aBody.addWidget(muted(tr("Looked in %1, two levels down.").arg(looked.join(QStringLiteral(", "))), parent));
			auto* const row    = new QHBoxLayout{ };
			auto* const choose = primary_button(tr("Choose its folder…"), parent, QStringLiteral("agentSetupChooseFolder"));
			connect(choose, &QPushButton::clicked, this, chooseFolder);
			row->addWidget(choose);
			row->addStretch(1);
			aBody.addLayout(row);
			aBody.addWidget(privateCopy(tr("Use a private copy")), 0, Qt::AlignLeft);
			auto* const edit = link_button(tr("Edit clone folders…"), parent, QStringLiteral("agentSetupEditFolders"));
			connect(edit, &QPushButton::clicked, this, [this] {
				if (Intents.OpenRepositorySettings) {
					Intents.OpenRepositorySettings();
				}
			});
			aBody.addWidget(edit, 0, Qt::AlignLeft);
			return;
		}
		const auto count = Clones.Matches.size();
		aBody.addWidget(muted(count == 1U ? tr("The agent reads more than the diff: callers, tests, history. A clone of %1 is in your clone folders. It is remembered for this repository; the agent gets its own workspace at the head with that clone's history, and your checkout and branches are not touched.").arg(name)
										  : tr("The agent reads more than the diff: callers, tests, history. %1 clones of %2 are in your clone folders. The one you choose is remembered for this repository; the agent gets its own workspace at the head with that clone's history, and your checkout and branches are not touched.").arg(count).arg(name),
							  parent));
		if (!PickedClone && count == 1U) {
			PickedClone = Clones.Matches.front().Path;
		}
		for (const auto& match : Clones.Matches) {
			auto* const card = new choice_card{ parent };
			card->setObjectName(QStringLiteral("agentSetupCloneRow"));
			card->Chosen     = PickedClone == match.Path;
			auto* const grid = new QHBoxLayout{ card };
			grid->setContentsMargins(12, 10, 12, 10);
			grid->setSpacing(10);
			auto* const radio = new QRadioButton{ card };
			radio->setObjectName(QStringLiteral("agentSetupCloneChoice"));
			radio->setChecked(card->Chosen);
			radio->setAccessibleName(shown_path(match.Path));
			grid->addWidget(radio, 0, Qt::AlignTop);
			auto* const lines = new QVBoxLayout{ };
			lines->setSpacing(2);
			auto* const path = words(shown_path(match.Path), card);
			auto        bold = path->font();
			bold.setWeight(QFont::DemiBold);
			path->setFont(bold);
			lines->addWidget(path);
			auto* const remote = words(text_of(match.Remote) + QStringLiteral("  ") + text_of(match.Location), card);
			remote->setFont(code_font(font()));
			lines->addWidget(remote);
			lines->addWidget(muted(match.Branch.empty() ? tr("In %1.").arg(shown_path(match.Folder)) : tr("In %1. On branch %2.").arg(shown_path(match.Folder), text_of(match.Branch)), card));
			grid->addLayout(lines, 1);
			const auto chosen = match.Path;
			card->Activate    = [this, chosen] {
				PickedClone = chosen;
				rebuild();
			};
			card->setAccessibleName(shown_path(match.Path));
			card->settle();
			aBody.addWidget(card);
		}
		auto* const row = new QHBoxLayout{ };
		auto* const use = primary_button(tr("Use this clone"), parent, QStringLiteral("agentSetupUseClone"));
		use->setEnabled(PickedClone.has_value());
		connect(use, &QPushButton::clicked, this, [this] {
			if (PickedClone && Intents.ChooseClone) {
				Intents.ChooseClone(*PickedClone);
			}
		});
		row->addWidget(use);
		auto* const choose = new quiet_push_button{ tr("Choose a folder…"), parent };
		choose->setObjectName(QStringLiteral("agentSetupChooseFolder"));
		choose->setFlat(true);
		connect(choose, &QPushButton::clicked, this, chooseFolder);
		row->addWidget(choose);
		row->addStretch(1);
		aBody.addLayout(row);
		aBody.addWidget(privateCopy(tr("Use a private copy instead")), 0, Qt::AlignLeft);
	}

	void agent_setup_view::build_check(QVBoxLayout& aBody)
	{
		auto* const parent = aBody.parentWidget();
		const auto  agent  = Setup.State.Chosen.value_or(core::setup_agent::claude_code);
		const auto  name   = plain(agent);
		auto* const intro  = muted(tr("Before your first question, BetterPullReview runs %1, which sends no question and no code, and makes the agent's workspace once.")
									   .arg(agent == core::setup_agent::plain_text_harness ? tr("your harness with --version") : status_command(agent)),
								   parent);
		aBody.addWidget(intro);
		auto* const card = new choice_card{ parent };
		card->setObjectName(QStringLiteral("agentSetupCheckRows"));
		auto* const rows = new QVBoxLayout{ card };
		rows->setContentsMargins(12, 10, 12, 10);
		rows->setSpacing(6);
		const auto row = [&](const core::setup_check& aCheck, const QString& aLabel, const QString& aName) {
			auto* const line = new QHBoxLayout{ };
			line->setSpacing(8);
			auto* const mark = new QLabel{ card };
			mark->setObjectName(aName + QStringLiteral("Mark"));
			auto glyph  = QString{ QChar{ 0x25CB } };
			auto colour = palette().color(QPalette::PlaceholderText);
			switch (aCheck.State) {
			case core::setup_check_state::not_run: break;
			case core::setup_check_state::running:
				glyph  = QString{ QChar{ 0x25D4 } };
				colour = palette().color(QPalette::Highlight);
				break;
			case core::setup_check_state::passed:
				glyph  = QString{ QChar{ 0x2713 } };
				colour = passed_colour(palette());
				break;
			case core::setup_check_state::failed:
				glyph  = QString{ QChar{ 0x2715 } };
				colour = failed_colour(palette());
				break;
			}
			mark->setText(glyph);
			auto markPalette = mark->palette();
			markPalette.setColor(QPalette::WindowText, colour);
			mark->setPalette(markPalette);
			line->addWidget(mark, 0, Qt::AlignTop);
			auto* const label = words(aLabel, card);
			label->setObjectName(aName);
			line->addWidget(label, 1);
			rows->addLayout(line);
		};
		row(Setup.State.AgentCheck, tr("%1 is signed in").arg(name), QStringLiteral("agentSetupAgentCheck"));
		auto where = workspace_words();
		if (!where.isEmpty()) {
			where[0] = where[0].toUpper();
		}
		row(Setup.State.WorkspaceCheck, where, QStringLiteral("agentSetupWorkspaceCheck"));
		aBody.addWidget(card);

		// A failed row says what failed and what to do.
		const auto failed = [&](const core::setup_check& aCheck) { return aCheck.State == core::setup_check_state::failed && aCheck.Failure.has_value(); };
		if (failed(Setup.State.AgentCheck)) {
			auto* const why = words(setup_failure_words(*Setup.State.AgentCheck.Failure, name, text_of(Setup.CheckDetail)), parent);
			why->setObjectName(QStringLiteral("agentSetupCheckFailure"));
			aBody.addWidget(why);
			if (*Setup.State.AgentCheck.Failure == core::agent_setup_failure::not_signed_in && !sign_in_command(agent).isEmpty()) {
				auto* const line    = new QHBoxLayout{ };
				auto* const command = words(sign_in_command(agent), parent);
				command->setObjectName(QStringLiteral("agentSetupSignInCommand"));
				command->setFont(code_font(font()));
				command->setTextInteractionFlags(Qt::TextSelectableByMouse);
				line->addWidget(command);
				auto* const copy = new quiet_push_button{ tr("Copy"), parent };
				copy->setObjectName(QStringLiteral("agentSetupCopyCommand"));
				const auto text = sign_in_command(agent);
				connect(copy, &QPushButton::clicked, this, [text] { QApplication::clipboard()->setText(text); });
				line->addWidget(copy);
				line->addStretch(1);
				aBody.addLayout(line);
			}
		}
		if (failed(Setup.State.WorkspaceCheck)) {
			auto* const why = words(Facts.WorkspaceReason ? workspace_unavailable_words(*Facts.WorkspaceReason) : setup_failure_words(core::agent_setup_failure::workspace_unavailable, name, { }), parent);
			why->setObjectName(QStringLiteral("agentSetupWorkspaceFailure"));
			aBody.addWidget(why);
		}
		auto* const buttons = new QHBoxLayout{ };
		if (Setup.Checking) {
			auto* const stop = new quiet_push_button{ tr("Stop"), parent };
			stop->setObjectName(QStringLiteral("agentSetupStop"));
			connect(stop, &QPushButton::clicked, this, [this] {
				if (Intents.Stop) {
					Intents.Stop();
				}
			});
			buttons->addWidget(stop);
			buttons->addWidget(muted(tr("Checking…"), parent));
		} else {
			const auto  ran   = Setup.State.AgentCheck.State != core::setup_check_state::not_run || Setup.State.WorkspaceCheck.State != core::setup_check_state::not_run;
			auto* const check = primary_button(ran ? tr("Check again") : tr("Check"), parent, QStringLiteral("agentSetupCheck"));
			connect(check, &QPushButton::clicked, this, [this] {
				if (Intents.Check) {
					Intents.Check();
				}
			});
			buttons->addWidget(check);
			if (failed(Setup.State.WorkspaceCheck) && !(Clones.Linked && Clones.Linked->PrivateCopy)) {
				buttons->addWidget(link_button(tr("Use a private copy for now"), parent, QStringLiteral("agentSetupCheckPrivateCopy")));
				auto* const fallback = qobject_cast<QPushButton*>(buttons->itemAt(buttons->count() - 1)->widget());
				connect(fallback, &QPushButton::clicked, this, [this] {
					if (Intents.UsePrivateCopy) {
						Intents.UsePrivateCopy();
					}
				});
			}
		}
		buttons->addStretch(1);
		aBody.addLayout(buttons);
	}
} // namespace bpr::qt
