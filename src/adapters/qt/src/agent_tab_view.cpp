#include "bpr/qt/agent_tab_view.hpp"

#include "bpr/core/agent_report.hpp"
#include "bpr/qt/agent_pending_list.hpp"
#include "bpr/qt/agent_steered_words.hpp"
#include "bpr/qt/ask_menu.hpp"

#include "bpr/qt/quiet_push_button.hpp"
#include "bpr/qt/quiet_tool_button.hpp"
#include "bpr/qt/skill_completer.hpp"

#include "bpr/core/agent_answer_findings.hpp"
#include "bpr/core/agent_finding.hpp"
#include "bpr/core/agent_queue.hpp"
#include "bpr/core/defect_workflow.hpp"
#include "bpr/core/feedback_convention.hpp"
#include "bpr/core/local_comment.hpp"
#include "bpr/core/prompt_surfaces.hpp"
#include "bpr/core/review_preparation.hpp"
#include "bpr/qt/agent_launch_panel.hpp"
#include "bpr/qt/agent_setup_view.hpp"
#include "bpr/qt/appearance_profile.hpp"
#include "bpr/qt/code_sample_painter.hpp"
#include "bpr/qt/disclosure_button.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/finding_queue_view.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "conversation_glyphs.hpp"

#include <QAbstractItemModel>
#include <QAccessible>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDrag>
#include <QDragEnterEvent>
#include <QEnterEvent>
#include <QEvent>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLayoutItem>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyleOptionToolButton>
#include <QStylePainter>
#include <QTextCursor>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <limits>
#include <tuple>
#include <utility>

namespace bpr::qt {
	namespace {
		/// A reply card's side margins; its answer is this much narrower.
		constexpr auto reply_left_margin  = 12;
		constexpr auto reply_right_margin = 10;
		/// [[BPR-1924]]. The least room kept between the scope menu and the
		/// agent picker when they share the tool row.
		constexpr auto run_settings_gap = 12;
		/// RFC-BPR-0125 (AC07). A sent question longer than this many lines
		/// folds behind Show all.
		constexpr qsizetype folded_question_lines = 6;
		/// RFC-BPR-0116. The returning prompts' keys among the chips.
		[[nodiscard]] auto latest_upload_key() -> QString { return QStringLiteral("returning.latest-upload"); }
		[[nodiscard]] auto proposed_fix_key() -> QString { return QStringLiteral("returning.proposed-fix"); }

		[[nodiscard]] auto suggestion_of(const core::prompt_offer& aOffer) -> agent_suggestion
		{
			return { .Key = QString::fromStdString(aOffer.Id), .Label = QString::fromStdString(aOffer.Label), .Expansion = QString::fromStdString(aOffer.Expansion.Text), .Finish = aOffer.finish() };
		}

		/// The caret's place in a drafted expansion: at `{cursor}`, or at
		/// its end.
		[[nodiscard]] auto caret_of(const core::prompt_expansion& aExpansion) -> int
		{
			const auto at = aExpansion.Cursor.value_or(aExpansion.Text.size());
			return static_cast<int>(QString::fromUtf8(aExpansion.Text.data(), static_cast<qsizetype>(at)).size());
		}

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

		/// \brief A thin rule inside the question field, in its outline's colour.
		class rule_line final : public QWidget
		{
		public:
			explicit rule_line(QWidget* aParent)
				: QWidget{ aParent }
			{
				setFixedHeight(1);
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.fillRect(rect(), mixed(palette().color(QPalette::Active, QPalette::Base), palette().color(QPalette::Active, QPalette::WindowText), 0.14));
			}
		};

		/// \brief [[BPR-2059]]. A finding's file and lines, shortened in the
		/// middle when the card is narrow so the severity and kind beside it
		/// keep their words. Its text stays whole: the accessible name, and the
		/// tooltip the card sets.
		class eliding_anchor final : public QLabel
		{
		public:
			using QLabel::QLabel;
			[[nodiscard]] auto minimumSizeHint() const -> QSize override
			{
				return QSize{ fontMetrics().horizontalAdvance(QStringLiteral("…:9")), QLabel::minimumSizeHint().height() };
			}
			[[nodiscard]] auto shown_text() const -> QString { return fontMetrics().elidedText(text(), Qt::ElideMiddle, std::max(0, contentsRect().width())); }

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				style()->drawItemText(&painter, contentsRect(), static_cast<int>(alignment()), palette(), isEnabled(), shown_text(), foregroundRole());
			}
		};

		/// \brief [[BPR-2059]]. The suggested comment's inset: a faint wash
		/// and a rule down its left, so the words that would be posted read as
		/// a quotation apart from the agent's own claim and evidence.
		class suggestion_inset final : public QWidget
		{
		public:
			explicit suggestion_inset(QWidget* aParent)
				: QWidget{ aParent }
			{
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto base = palette().color(QPalette::Active, QPalette::Base);
				const auto ink  = palette().color(QPalette::Active, QPalette::WindowText);
				painter.setPen(Qt::NoPen);
				painter.setBrush(mixed(base, ink, 0.04));
				painter.drawRoundedRect(QRectF{ rect() }, 6.0, 6.0);
				painter.setBrush(mixed(base, ink, 0.26));
				painter.drawRoundedRect(QRectF{ 0.0, 0.0, 2.0, static_cast<qreal>(height()) }, 1.0, 1.0);
			}
		};

		/// \brief docs/design/report-card-v4 (round 1's C, Attachment, chosen
		/// 2026-11-14): the report as an attachment under its summary. A row
		/// on the window's surface colour, a one point border, 7 point corners,
		/// 7 by 8 padding: a 24 by 30 page carrying the agent glyph, the title
		/// in semibold (two lines at most) over a 12 point line, a chevron. The
		/// whole row is Open report; while the report's tab is active it is
		/// selected (a 1.5 point outline and the selection tint).
		class report_attachment_row final : public QPushButton
		{
		public:
			explicit report_attachment_row(QWidget* aParent)
				: QPushButton{ aParent }
			{
				setCursor(Qt::PointingHandCursor);
				setFocusPolicy(Qt::StrongFocus);
				// Look ask-v007 ("Why is your mockup report attachment cutting
				// of text"): the row was given one height, from whatever width it
				// had when first asked; a narrower drawer wrapped the title into
				// that and cut it. Its height now follows its width.
				auto policy = QSizePolicy{ QSizePolicy::Expanding, QSizePolicy::Preferred };
				policy.setHeightForWidth(true);
				setSizePolicy(policy);
				setAttribute(Qt::WA_Hover, true);
				auto* const row = new QHBoxLayout{ this };
				row->setContentsMargins(8 + 24 + 10, 7, 8 + 16, 7);
				auto* const words = new QVBoxLayout{ };
				words->setSpacing(1);
				Title = new QLabel{ this };
				Title->setObjectName(QStringLiteral("agentTurnReportTitle"));
				Title->setTextFormat(Qt::PlainText);
				Title->setWordWrap(true);
				Title->setAttribute(Qt::WA_TransparentForMouseEvents, true);
				Title->setMaximumHeight(2 * Title->fontMetrics().lineSpacing() + 2);
				auto semibold = Title->font();
				semibold.setWeight(QFont::DemiBold);
				Title->setFont(semibold);
				Meta = new QLabel{ this };
				Meta->setObjectName(QStringLiteral("agentTurnReportMeta"));
				Meta->setForegroundRole(QPalette::PlaceholderText);
				Meta->setAttribute(Qt::WA_TransparentForMouseEvents, true);
				// The secondary line, a point under the title's size.
				auto small = Meta->font();
				small.setPointSizeF(std::max(9.0, small.pointSizeF() - 1.0));
				Meta->setFont(small);
				words->addWidget(Title);
				words->addWidget(Meta);
				row->addLayout(words, 1);
				// [[BPR-2215]]. The page icon's height, so the layout that
				// places the row never gives it less than the icon needs.
				row->addStrut(30);
			}

			[[nodiscard]] auto title() const noexcept -> QLabel* { return Title; }
			[[nodiscard]] auto meta() const noexcept -> QLabel* { return Meta; }

			void set_selected(const bool aSelected)
			{
				if (Selected != aSelected) {
					Selected = aSelected;
					setProperty("bprReportSelected", aSelected);
					update();
				}
			}

			[[nodiscard]] auto hasHeightForWidth() const -> bool override { return true; }

			/// The title's lines at `aWidth` (two at most), its second line and
			/// the padding; never less than the page icon needs.
			/// \details [[BPR-2215]]. A parent layout asks the row's own layout,
			/// not this, for the height at a width, so this answers what that
			/// layout does: on Windows a separate sum here asked one pixel
			/// more than the row was given (44 against 43).
			[[nodiscard]] auto heightForWidth(const int aWidth) const -> int override { return layout()->totalHeightForWidth(aWidth); }

			[[nodiscard]] auto sizeHint() const -> QSize override { return { 200, heightForWidth(width() > 0 ? width() : 320) }; }

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				const auto box  = QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0);
				auto       fill = palette().color(QPalette::Window);
				auto       edge = palette().color(QPalette::WindowText);
				edge.setAlphaF(underMouse() || hasFocus() ? 0.35F : 0.18F);
				if (Selected) {
					auto tint = palette().color(QPalette::Highlight);
					tint.setAlphaF(0.14F);
					painter.setBrush(fill);
					painter.setPen(Qt::NoPen);
					painter.drawRoundedRect(box, 7.0, 7.0);
					painter.setBrush(tint);
					painter.setPen(QPen{ palette().color(QPalette::Highlight), 1.5 });
				} else {
					painter.setBrush(isDown() ? fill.darker(106) : fill);
					painter.setPen(QPen{ edge, 1.0 });
				}
				painter.drawRoundedRect(box, 7.0, 7.0);
				// The page, its corner folded, carrying the glyph.
				const auto page = QRectF{ 8.0, (height() - 30) / 2.0, 24.0, 30.0 };
				auto       ink  = palette().color(QPalette::WindowText);
				ink.setAlphaF(0.55F);
				auto sheet = QPainterPath{ };
				sheet.moveTo(page.topLeft());
				sheet.lineTo(page.right() - 7.0, page.top());
				sheet.lineTo(page.right(), page.top() + 7.0);
				sheet.lineTo(page.bottomRight());
				sheet.lineTo(page.bottomLeft());
				sheet.closeSubpath();
				painter.setBrush(palette().color(QPalette::Base));
				painter.setPen(QPen{ ink, 1.2 });
				painter.drawPath(sheet);
				painter.drawLine(QPointF{ page.right() - 7.0, page.top() }, QPointF{ page.right() - 7.0, page.top() + 7.0 });
				painter.drawLine(QPointF{ page.right() - 7.0, page.top() + 7.0 }, QPointF{ page.right(), page.top() + 7.0 });
				agent_glyph(palette()).paint(&painter, QRect{ static_cast<int>(page.left()) + 5, static_cast<int>(page.center().y()) - 6, 14, 14 });
				// The chevron.
				const auto middle  = QPointF{ width() - 13.0, height() / 2.0 };
				auto       chevron = QPainterPath{ };
				chevron.moveTo(middle + QPointF{ -2.0, -4.0 });
				chevron.lineTo(middle + QPointF{ 2.0, 0.0 });
				chevron.lineTo(middle + QPointF{ -2.0, 4.0 });
				painter.setBrush(Qt::NoBrush);
				painter.setPen(QPen{ ink, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin });
				painter.drawPath(chevron);
			}

			void keyPressEvent(QKeyEvent* aEvent) override
			{
				if (aEvent->key() == Qt::Key_Return || aEvent->key() == Qt::Key_Enter) {
					click();
					return;
				}
				QPushButton::keyPressEvent(aEvent);
			}

		private:
			QLabel* Title    = nullptr;
			QLabel* Meta     = nullptr;
			bool    Selected = false;
		};

		/// \brief Owner look v003 ("Timestamp would also be nice"): a time
		/// as near words, the whole of it on hover.
		[[nodiscard]] auto relative_time(const QDateTime& aWhen) -> QString
		{
			if (!aWhen.isValid()) {
				return { };
			}
			const auto now     = QDateTime::currentDateTime();
			const auto seconds = aWhen.secsTo(now);
			if (seconds < 60) {
				return QObject::tr("just now");
			}
			if (seconds < 3600) {
				return QObject::tr("%n min ago", nullptr, static_cast<int>(seconds / 60));
			}
			const auto local = aWhen.toLocalTime();
			if (local.date() == now.date()) {
				return QLocale{ }.toString(local.time(), QLocale::ShortFormat);
			}
			return QLocale{ }.toString(local, QLocale::ShortFormat);
		}

		void show_time(QLabel& aLabel, const QDateTime& aWhen)
		{
			aLabel.setText(relative_time(aWhen));
			aLabel.setToolTip(aWhen.isValid() ? QLocale{ }.toString(aWhen.toLocalTime(), QLocale::LongFormat) : QString{ });
			aLabel.setVisible(aWhen.isValid());
		}

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

	auto agent_model_display_name(const QString& aModel) -> QString
	{
		auto       name    = aModel.trimmed();
		auto       context = QString{ };
		const auto bracket = name.indexOf(QLatin1Char{ '[' });
		if (bracket > 0 && name.endsWith(QLatin1Char{ ']' })) {
			// `claude-opus-5-5[1m]`: the window the agent asked for.
			context = name.mid(bracket + 1, name.size() - bracket - 2).toUpper();
			name    = name.left(bracket);
		}
		const auto        family  = [](const QString& aWord) { return aWord.left(1).toUpper() + aWord.mid(1).toLower(); };
		const auto        with    = [&context](QString aShown) { return context.isEmpty() ? aShown : aShown + QStringLiteral(" (") + context + QLatin1Char{ ')' }; };
		static const auto aliases = QStringList{ QStringLiteral("opus"), QStringLiteral("sonnet"), QStringLiteral("haiku"), QStringLiteral("fable") };
		if (aliases.contains(name.toLower())) {
			return with(QObject::tr("%1 (newest)").arg(family(name)));
		}
		// `claude-<family>-<major>[-<minor>][-<date>]`.
		auto parts = name.split(QLatin1Char{ '-' });
		if (parts.size() >= 3 && parts.front().compare(QStringLiteral("claude"), Qt::CaseInsensitive) == 0) {
			if (parts.size() >= 4 && parts.back().size() == 8) {
				parts.removeLast();
			}
			auto       majorOk = false;
			const auto major   = parts[2].toUInt(&majorOk);
			if (majorOk && parts[1].size() > 1 && parts.size() <= 4) {
				auto version = QString::number(major);
				if (parts.size() == 4) {
					auto       minorOk = false;
					const auto minor   = parts[3].toUInt(&minorOk);
					if (!minorOk || parts[3].size() > 2) {
						return aModel.trimmed();
					}
					version += QLatin1Char{ '.' } + QString::number(minor);
				}
				return with(family(parts[1]) + QLatin1Char{ ' ' } + version);
			}
		}
		return aModel.trimmed();
	}

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
		// Owner look v003 and the VS Code agent drawers: the session's title,
		// then History and New session as icons.
		SessionTitle = new QLabel{ tr("Agent"), context };
		SessionTitle->setObjectName(QStringLiteral("agentSessionTitle"));
		SessionTitle->setTextFormat(Qt::PlainText);
		SessionTitle->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		auto titleFont = SessionTitle->font();
		titleFont.setWeight(QFont::DemiBold);
		SessionTitle->setFont(titleFont);
		row->addWidget(SessionTitle, 1);
		Head = chip_label(context, QStringLiteral("agentHeadChip"));
		Head->setToolTip(tr("The head the session reads"));
		Head->setForegroundRole(QPalette::PlaceholderText);
		row->addWidget(Head);
		History = make_drawer_tool(context, drawer_icon(drawer_glyph::history, palette()), tr("History: sessions, earlier heads and private runs"), QStringLiteral("agentHistory"));
		History->setAccessibleName(tr("History"));
		History->setPopupMode(QToolButton::InstantPopup);
		auto* const historyMenu = new QMenu{ History };
		HistoryMenu             = historyMenu;
		historyMenu->setObjectName(QStringLiteral("agentHistoryMenu"));
		NativeHistoryMenu = historyMenu->addMenu(tr("Sessions"));
		NativeHistoryMenu->setObjectName(QStringLiteral("nativeAgentSessions"));
		connect(historyMenu, &QMenu::aboutToShow, this, [this] { if (NativeHistoryDispatch) { NativeHistoryDispatch(std::nullopt); } });
		historyMenu->addAction(Chat.new_session_action());
		historyMenu->addAction(Chat.continue_old_revision_action());
		historyMenu->addSeparator();
		auto* const privateRuns = historyMenu->addAction(tr("Private history and runs"));
		privateRuns->setObjectName(QStringLiteral("agentPrivateHistoryAction"));
		connect(privateRuns, &QAction::triggered, this, [this] { show_history(true); });

		History->setMenu(historyMenu);
		row->addWidget(History);
		// Owner decision (b) 2026-10-23: a new session clears the current
		// agent's thread.
		ClearSession = make_drawer_tool(context, drawer_icon(drawer_glyph::new_session, palette()), tr("New session: retain this conversation in history and start fresh"),
										QStringLiteral("agentClearSession"));
		ClearSession->setAccessibleName(tr("New session"));
		connect(ClearSession, &QToolButton::clicked, this, [this] { clear_session(); });
		row->addWidget(ClearSession);
		column->addWidget(context);
		ContinuationMode = new QLabel{ tr("Native continuation"), this };
		ContinuationMode->setObjectName(QStringLiteral("agentContinuationMode"));
		ContinuationMode->setWordWrap(true);
		ContinuationMode->setTextFormat(Qt::PlainText);
		column->addWidget(ContinuationMode);

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
		// [[BPR-1753]]. The ready line once setup finishes, and what
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
		auto* const dismiss = new quiet_tool_button{ ReadyCard };
		dismiss->setObjectName(QStringLiteral("agentReadyDismiss"));
		dismiss->setAutoRaise(true);
		dismiss->setText(QString{ QChar{ 0x2715 } });
		dismiss->setAccessibleName(tr("Dismiss"));
		connect(dismiss, &QToolButton::clicked, ReadyCard, &QWidget::hide);
		// [[BPR-1877]]. The suggestions show beside the Ready line; closing it
		// gives its place back to the empty tab's line.
		connect(dismiss, &QToolButton::clicked, this, [this] { refresh(); });
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
		// Owner look v003: with nothing asked, an icon and a line, as the
		// agent drawers show it; the suggested prompts wait above the field.
		EmptyIntro              = new QWidget{ content };
		auto* const introColumn = new QVBoxLayout{ EmptyIntro };
		introColumn->setContentsMargins(0, 36, 0, 12);
		introColumn->setSpacing(6);
		auto* const introIcon = new QLabel{ EmptyIntro };
		introIcon->setObjectName(QStringLiteral("agentEmptyIcon"));
		introIcon->setPixmap(drawer_icon(drawer_glyph::agent, palette()).pixmap(32, 32));
		introColumn->addWidget(introIcon, 0, Qt::AlignHCenter);
		auto* const introTitle = new QLabel{ tr("Ask about this pull request"), EmptyIntro };
		introTitle->setObjectName(QStringLiteral("agentEmptyTitle"));
		introTitle->setFont(bold);
		introColumn->addWidget(introTitle, 0, Qt::AlignHCenter);
		auto* const introText = new QLabel{ tr("The agent reads the pull request at its head. Nothing it says is published unless you make it a comment."), EmptyIntro };
		introText->setWordWrap(true);
		introText->setAlignment(Qt::AlignHCenter);
		introText->setForegroundRole(QPalette::PlaceholderText);
		introColumn->addWidget(introText);
		EmptyIntro->hide();
		contentCol->addWidget(EmptyIntro);
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
		// Owner decision (a) 2026-10-23: suggested prompts, each sent as
		// written. Owner look v005 ("I want the suggestions to draw more like
		// they're in the chat window rather than attached to the bottom pane.
		// That way they show up after the last chat message not overtop of
		// it"): they are the thread's last item and scroll with it.
		// [[BPR-1934]]. One component, which the tour chats show too.
		Suggestions = new agent_suggestion_list{ tr("Ask about this pull request, or try:"), content };
		Suggestions->set_choose_dispatch([this](const QString& aKey, const bool aEditFirst) { choose_prompt(aKey, aEditFirst); });
		contentCol->addWidget(Suggestions);
		// RFC-BPR-0128 with RFC-BPR-0126's queue (owner, 2026-11-14: "a plain
		// click queues it"). What waits its turn, in words.
		QueueNote = new QLabel{ content };
		QueueNote->setObjectName(QStringLiteral("agentQueueNote"));
		QueueNote->setWordWrap(true);
		QueueNote->setTextFormat(Qt::PlainText);
		QueueNote->hide();
		contentCol->addWidget(QueueNote);
		// RFC-BPR-0126 ([[BPR-2151]]). The session's waiting messages, each
		// with Edit (a typed one, held while edited), Remove and Move up and
		// down, and Pause or Resume for the queue.
		QueueList = new agent_pending_list{ content };
		QueueList->setObjectName(QStringLiteral("agentQueueList"));
		contentCol->addWidget(QueueList);
		// [[BPR-1847]], owner 2026-11-03: "doesn't make sense to show
		// suggested prompts before the wizard completes." While the setup
		// is unfinished and not on screen, its way in stands where they would.
		SetupPrompt = new QWidget{ content };
		SetupPrompt->setObjectName(QStringLiteral("agentSuggestionsSetup"));
		auto* const setupRow = new QHBoxLayout{ SetupPrompt };
		setupRow->setContentsMargins(0, 2, 0, 0);
		setupRow->setSpacing(6);
		auto* const setupLine = new QLabel{ tr("Suggested prompts appear once the agent is set up."), SetupPrompt };
		setupLine->setObjectName(QStringLiteral("agentSuggestionsSetupLine"));
		setupLine->setForegroundRole(QPalette::PlaceholderText);
		setupLine->setWordWrap(true);
		setupRow->addWidget(setupLine, 1);
		auto* const finish = new quiet_push_button{ tr("Finish setup"), SetupPrompt };
		finish->setObjectName(QStringLiteral("agentSuggestionsSetupFinish"));
		finish->setFlat(true);
		connect(finish, &QPushButton::clicked, this, [this] { open_setup(); });
		setupRow->addWidget(finish, 0, Qt::AlignTop);
		SetupPrompt->hide();
		contentCol->addWidget(SetupPrompt);
		contentCol->addStretch(1);
		scroll->setWidget(content);
		talk->addWidget(scroll, 1);

		// The question field: the words, its context chips, Context and Ask.
		auto* const ask = new QWidget{ Conversation };
		ask->setObjectName(QStringLiteral("agentQuestion"));
		auto* const askCol = new QVBoxLayout{ ask };
		askCol->setContentsMargins(10, 8, 10, 10);
		askCol->setSpacing(6);
		// Owner look v003 ("This bottom menu area is a big mess") and the
		// Claude Code and Codex composers: one rounded field. Any added
		// context, the words, a thin rule, and one row: Context and
		// Suggestions as icons and what the agent may read on the left; the
		// agent · model · effort pill and Send on the right.
		auto* const field    = new rounded{ rounded::look::field, ask };
		Field                = field;
		auto* const fieldCol = new QVBoxLayout{ field };
		fieldCol->setContentsMargins(8, 6, 6, 6);
		fieldCol->setSpacing(4);
		field->installEventFilter(this);
		Chips = chip_label(field, QStringLiteral("agentContextChips"));
		Chips->setForegroundRole(QPalette::PlaceholderText);
		Chips->setWordWrap(true);
		Chips->hide();
		fieldCol->addWidget(Chips);
		// [[BPR-1773]]. The conversation the next questions are about: what
		// it is, and a close button. It stays until it is closed, so a
		// follow-up is about the same conversation.
		ConversationChip = new rounded{ rounded::look::card, field };
		ConversationChip->setObjectName(QStringLiteral("agentConversationChip"));
		auto* const chipRow = new QHBoxLayout{ ConversationChip };
		chipRow->setContentsMargins(8, 3, 2, 3);
		chipRow->setSpacing(4);
		ConversationText = new QLabel{ ConversationChip };
		ConversationText->setObjectName(QStringLiteral("agentConversationChipText"));
		ConversationText->setTextFormat(Qt::PlainText);
		ConversationText->setWordWrap(true);
		chipRow->addWidget(ConversationText, 1);
		ConversationClose = make_drawer_tool(ConversationChip, drawer_icon(drawer_glyph::close, palette()), tr("Stop asking about this conversation"), QStringLiteral("agentConversationChipClose"));
		ConversationClose->set_surface(QPalette::Base);
		connect(ConversationClose, &QToolButton::clicked, this, [this] {
			if (ConversationDismiss) {
				ConversationDismiss();
			}
		});
		chipRow->addWidget(ConversationClose, 0, Qt::AlignTop);
		ConversationChip->hide();
		fieldCol->addWidget(ConversationChip);
		// [[BPR-2002]], RFC-BPR-0102. The selected lines, attached: the
		// question is about them. Its close button detaches them from the
		// next questions and leaves the diff's selection alone.
		LinesChip = new rounded{ rounded::look::card, field };
		LinesChip->setObjectName(QStringLiteral("agentLinesChip"));
		auto* const linesRow = new QHBoxLayout{ LinesChip };
		linesRow->setContentsMargins(8, 3, 2, 3);
		linesRow->setSpacing(4);
		LinesText = new QLabel{ LinesChip };
		LinesText->setObjectName(QStringLiteral("agentLinesChipText"));
		LinesText->setTextFormat(Qt::PlainText);
		linesRow->addWidget(LinesText, 1);
		LinesClose = make_drawer_tool(LinesChip, drawer_icon(drawer_glyph::close, palette()), tr("Don't attach these lines"), QStringLiteral("agentLinesChipClose"));
		LinesClose->setAccessibleName(tr("Detach the selected lines"));
		connect(LinesClose, &QToolButton::clicked, this, [this] {
			LinesDetached = true;
			refresh_suggestions();
		});
		linesRow->addWidget(LinesClose, 0, Qt::AlignTop);
		LinesChip->setToolTip(tr("The question is about these lines. The agent can still read the whole pull request."));
		LinesChip->hide();
		// RFC-BPR-0125 (Q2). Beside the lines chip, the suggested prompt the
		// field holds as a draft: the question keeps its subject and kind
		// until this is closed or the field is emptied.
		MacroChip = new rounded{ rounded::look::card, field };
		MacroChip->setObjectName(QStringLiteral("agentMacroChip"));
		auto* const macroRow = new QHBoxLayout{ MacroChip };
		macroRow->setContentsMargins(8, 3, 2, 3);
		macroRow->setSpacing(4);
		MacroText = new QLabel{ MacroChip };
		MacroText->setObjectName(QStringLiteral("agentMacroChipText"));
		MacroText->setTextFormat(Qt::PlainText);
		macroRow->addWidget(MacroText, 1);
		MacroClose = make_drawer_tool(MacroChip, drawer_icon(drawer_glyph::close, palette()), tr("Send what you wrote as a typed question"), QStringLiteral("agentMacroChipClose"));
		MacroClose->setAccessibleName(tr("Remove the suggested prompt"));
		connect(MacroClose, &QToolButton::clicked, this, [this] {
			DraftedMacro.reset();
			present_macro_chip();
			Chat.composer()->setFocus(Qt::OtherFocusReason);
		});
		macroRow->addWidget(MacroClose, 0, Qt::AlignTop);
		MacroChip->hide();
		auto* const attachedRow = new QHBoxLayout{ };
		attachedRow->setContentsMargins(0, 0, 0, 0);
		attachedRow->setSpacing(4);
		attachedRow->addWidget(LinesChip);
		attachedRow->addWidget(MacroChip);
		attachedRow->addStretch(1);
		fieldCol->addLayout(attachedRow);
		auto* const composer = Chat.composer();
		Borrowed.emplace_back(composer, composer->parentWidget());
		composer->setParent(field);
		composer->setFrameShape(QFrame::NoFrame);
		composer->setPlaceholderText(tr("Ask about this pull request…"));
		composer->setToolTip(send_tip());
		// [[BPR-1916]]. `/name` completion of the person's own skills.
		SkillCompletion = new skill_completer{ composer };
		// RFC-BPR-0125 (Q2). An emptied field holds no suggested prompt.
		connect(composer, &QPlainTextEdit::textChanged, this, [this] {
			if (DraftedMacro && Chat.composer()->toPlainText().trimmed().isEmpty()) {
				DraftedMacro.reset();
				present_macro_chip();
			}
		});
		composer->setMaximumHeight(90);
		composer->viewport()->setAutoFillBackground(false);
		fieldCol->addWidget(composer);
		Notice = new QLabel{ field };
		Notice->setObjectName(QStringLiteral("agentComposerNotice"));
		Notice->setWordWrap(true);
		Notice->setForegroundRole(QPalette::PlaceholderText);
		Notice->hide();
		fieldCol->addWidget(Notice);
		auto* const rule = new rule_line{ field };
		rule->setObjectName(QStringLiteral("agentComposerRule"));
		fieldCol->addWidget(rule);
		auto* const askRow = new QHBoxLayout{ };
		AskRow             = askRow;
		askRow->setSpacing(2);
		// [[BPR-2002]]. Attaches the lines selected in the code again, after
		// the chip's close button detached them.
		Context = make_drawer_tool(field, drawer_icon(drawer_glyph::plus, palette()), tr("Attach the lines selected in the code"), QStringLiteral("agentAddContext"));
		Context->set_surface(QPalette::Base);
		connect(Context, &QToolButton::clicked, this, [this] { attach_lines(); });
		askRow->addWidget(Context);
		// Owner look v004 ("I would like the suggestions to come back where
		// they were in the chat window... maybe turn the suggestion prompts
		// into a toggle to show/hide them instead of a drop down"): the
		// prompts stay above the field, and this shows or hides them; the
		// host keeps the choice.
		SuggestionsMenu = make_drawer_tool(field, drawer_icon(drawer_glyph::agent, palette()), tr("Hide the suggested prompts"), QStringLiteral("agentSuggestionsToggle"));
		SuggestionsMenu->set_surface(QPalette::Base);
		SuggestionsMenu->setCheckable(true);
		SuggestionsMenu->setChecked(true);
		SuggestionsMenu->setAccessibleName(tr("Suggested prompts"));
		connect(SuggestionsMenu, &QToolButton::toggled, this, [this](const bool aShown) {
			SuggestionsMenu->setToolTip(aShown ? tr("Hide the suggested prompts") : tr("Show the suggested prompts"));
			refresh_suggestions();
			if (SuggestionsObserver) {
				SuggestionsObserver(aShown);
			}
		});
		askRow->addWidget(SuggestionsMenu);
		// [[BPR-2002]], RFC-BPR-0102: no scope list. It bounded nothing; the
		// lines chip is the one thing that differs between questions.
		askRow->addStretch(1);
		// Owner decision (c) and amendment 2026-10-23: the agent, model and
		// effort as a pill in the field. [[BPR-1924]]: a row of its own under
		// the tools when the field is too narrow for it whole.
		// [[BPR-1878]]: one source behind this pill and the Tour tab's.
		RunSettingsSource           = std::make_shared<agent_run_settings_source>(Launch);
		RunSettingsSource->Choose   = [this](const std::uint64_t aProfile) { return choose_agent(aProfile); };
		RunSettingsSource->Dispatch = [this](const std::uint64_t aProfile, std::optional<std::string> aModel, std::optional<std::string> aEffort) {
			return RunSettingsDispatch && RunSettingsDispatch(aProfile, std::move(aModel), std::move(aEffort));
		};
		RunSettingsSource->Catalog = [this](const std::uint64_t aProfile) {
			if (CatalogDispatch) {
				CatalogDispatch(aProfile);
			}
		};
		RunSettingsSource->Setup = [this] {
			if (Setup) {
				Setup();
			}
		};
		RunSettingsSource->Refused = [this](const QString& aWhy) {
			Notice->setText(aWhy);
			Notice->show();
		};
		// [[BPR-2051]], owner decision 2026-11-11 ("A + footer 1"): the
		// picker left the drawers for the pull request window's footer, one
		// choice for the Agent tab, the tour chat, Enhance and Draft the tour.
		// The tab borrows the footer's picker for /model and /effort.
		Ask = new QPushButton{ field };
		Ask->setObjectName(QStringLiteral("agentAsk"));
		Ask->setDefault(true);
		Ask->setIcon(drawer_icon(drawer_glyph::send, palette()));
		Ask->setAccessibleName(tr("Send"));
		Ask->setToolTip(send_tip());
		// Send, or Stop while the agent works: one place, always in reach.
		connect(Ask, &QPushButton::clicked, this, [this] {
			if (State == agent_tab_state::working) {
				Launch.cancel_action()->trigger();
				return;
			}
			this->ask();
		});
		// Return with Command or Control asks from the field, as Send does,
		// so a slash command is read the same way from either.
		composer->installEventFilter(this);
		// What was asked is read when it is sent, by Send or by the keyboard.
		connect(Chat.send_action(), &QAction::triggered, this, [this] {
			PendingQuestion   = Chat.composer()->toPlainText().trimmed();
			PendingName       = SendingMacro && SendingMacro->Name != *PendingQuestion ? SendingMacro->Name : QString{ };
			PendingEdited     = !PendingName.isEmpty() && !core::prompt_sent_unedited(PendingQuestion->toStdString(), SendingMacro->Expansion.toStdString());
			PendingReview     = false;
			PendingKeepsField = KeepingDraft;
			PendingAbout      = ConversationShown;
			PendingReturning  = SendingReturning;
			PendingExplain    = SendingExplain;
			PendingSuggest    = SendingSuggest;
			// [[BPR-1773]]. Where its answer's Draft comment would go is
			// decided now, while what it was asked on is on screen.
			PendingDraftTarget = DraftTargetSource ? std::optional{ DraftTargetSource() } : std::nullopt;
		});
		askRow->addWidget(Ask);
		fieldCol->addLayout(askRow);
		PickerRow = new QHBoxLayout{ };
		PickerRow->setSpacing(0);
		PickerRow->addStretch(1);
		fieldCol->addLayout(PickerRow);
		// [[BPR-1817]]. Why the last send produced no turn, said directly
		// above the field it came from, in the danger look the stopped
		// banner uses, until the reviewer sends again or closes it.
		auto* const refusal = new rounded{ rounded::look::danger, ask };
		Refusal             = refusal;
		refusal->setObjectName(QStringLiteral("agentSendRefusal"));
		auto* const refusalRow = new QHBoxLayout{ refusal };
		refusalRow->setContentsMargins(10, 6, 2, 6);
		refusalRow->setSpacing(4);
		RefusalText = new QLabel{ refusal };
		RefusalText->setObjectName(QStringLiteral("agentSendRefusalText"));
		RefusalText->setTextFormat(Qt::PlainText);
		RefusalText->setWordWrap(true);
		RefusalText->setTextInteractionFlags(Qt::TextSelectableByMouse);
		refusalRow->addWidget(RefusalText, 1);
		auto* const refusalClose = make_drawer_tool(refusal, drawer_icon(drawer_glyph::close, palette()), tr("Dismiss"), QStringLiteral("agentSendRefusalClose"));
		connect(refusalClose, &QToolButton::clicked, this, [this] { hide_send_refusal(); });
		refusalRow->addWidget(refusalClose, 0, Qt::AlignTop);
		refusal->hide();
		askCol->addWidget(refusal);
		askCol->addWidget(field);
		// [[BPR-2002]]. What is true about what the agent reads, said once:
		// [[BPR-2038]], owner look on RFC-BPR-0101 AC08 ("this agent can read
		// whole PR thing is way too long ... needs to be a tooltip"), in the
		// field's tooltip, which its chips and buttons override with their own.
		field->setToolTip(tr("The agent can read the whole pull request; attached lines only tell it where you are looking."));
		// [[BPR-1999]]. The session's cost, when the agent reported any, on
		// one line at the right under the composer.
		SessionCostLine = new QLabel{ ask };
		SessionCostLine->setObjectName(QStringLiteral("agentSessionCost"));
		SessionCostLine->setTextFormat(Qt::PlainText);
		SessionCostLine->setForegroundRole(QPalette::PlaceholderText);
		SessionCostLine->setToolTip(tr("What the agent reported this session has cost. It restarts at New session."));
		SessionCostLine->hide();
		auto* const underRow = new QHBoxLayout{ };
		underRow->setContentsMargins(0, 0, 0, 0);
		underRow->addStretch(1);
		underRow->addWidget(SessionCostLine, 0, Qt::AlignTop | Qt::AlignRight);
		askCol->addLayout(underRow);
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
		// Another agent is another question: what refused the last one may
		// not refuse this one.
		connect(profiles, &QComboBox::currentIndexChanged, this, [this] { hide_send_refusal(); });
		// [[BPR-1753]], owner look v001 ("no indication anything
		// happened"). The working line follows the request as it moves, with
		// the time it has taken, so a long run never looks like nothing.
		Launch.add_progress_observer([this] { progress(); });
		// The turns' times read as near words; a minute refreshes them.
		auto* const minutes = new QTimer{ this };
		minutes->setInterval(60'000);
		connect(minutes, &QTimer::timeout, this, [this] { refresh_times(); });
		minutes->start();
		Ticker = new QTimer{ this };
		Ticker->setInterval(1000);
		connect(Ticker, &QTimer::timeout, this, [this] {
			present_banner();
			update_turn();
		});
		AnswerPacer = new QTimer{ this };
		AnswerPacer->setSingleShot(true);
		AnswerPacer->setInterval(100);
		connect(AnswerPacer, &QTimer::timeout, this, [this] {
			if (AnswerPending && panels_alive()) {
				AnswerPending = false;
				update_turn();
				AnswerPacer->start();
			}
		});
		refresh();
	}

	agent_tab_view::~agent_tab_view()
	{
		Dying = true;
		// [[BPR-1878]]. Another picker on this source may outlive the tab.
		if (RunSettingsSource) {
			RunSettingsSource->Choose        = { };
			RunSettingsSource->Dispatch      = { };
			RunSettingsSource->Catalog       = { };
			RunSettingsSource->Setup         = { };
			RunSettingsSource->Refused       = { };
			RunSettingsSource->ToggleScratch = { };
			RunSettingsSource->OpenScratch   = { };
		}
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

	void agent_tab_view::open_setup()
	{
		// At the setup's first unfinished step, not pinned to it, so the
		// setup closes on its Ready line once that step is done.
		if (SetupView != nullptr) {
			SetupView->begin(core::setup_step::ready);
		} else if (Setup) {
			Setup();
		}
	}

	auto agent_tab_view::setup_readiness() const -> agent_setup_readiness
	{
		if (SetupView != nullptr && !SetupView->projection().Detected) {
			return agent_setup_readiness::checking;
		}
		const auto verdict = suggestions_verdict();
		return verdict == core::agent_suggestions_verdict::no_agent || verdict == core::agent_suggestions_verdict::setup_unfinished ? agent_setup_readiness::unfinished : agent_setup_readiness::ready;
	}

	void agent_tab_view::set_readiness_observer(std::function<void()> aObserver)
	{
		ReadinessObserver = std::move(aObserver);
		ToldReadiness.reset();
		refresh_suggestions();
	}

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

	void agent_tab_view::adopt_setup(agent_setup_view& aSetup)
	{
		SetupView = &aSetup;
		Pages->addWidget(SetupView);
		SetupView->set_change_observer([this] { refresh(); });
		HistoryMenu->addSeparator();
		// Owner look v006 ("I dont think we should have this option to go back
		// to the wizard, actually we can keep it if it's a "reset agents"
		// which clears all settings and goes back to the wizard fresh").
		auto* const reset = HistoryMenu->addAction(tr("Reset agents…"));
		reset->setObjectName(QStringLiteral("agentResetAgentsAction"));
		connect(reset, &QAction::triggered, this, [this] { reset_agents(); });
		refresh();
	}

	void agent_tab_view::present_ready(const QString& aTitle, const QString& aText, const QString& aGets)
	{
		ReadyTitle->setText(aTitle);
		ReadyText->setText(aText);
		ReadyGets->setText(aGets);
		ReadyCard->setVisible(!aTitle.isEmpty());
		// [[BPR-1877]]. The whole tab, so the Ready line takes the empty tab's
		// line's place at once and the suggestions follow straight under it.
		refresh();
	}

	void agent_tab_view::refresh()
	{
		if (!panels_alive()) {
			return;
		}
		for (const auto& entry : CardEntries) {
			show_card_keep(entry);
		}
		const auto setUp      = Launch.offered_count() > 0U;
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
			StoppedDismissed = false;
			Started.start();
			Ticker->start();
			ReadyCard->hide();
		} else if (State != agent_tab_state::working && wasWorking) {
			Ticker->stop();
			// [[BPR-1817]]. "Still answering" no longer is.
			if (ShownRefusal == core::agent_send_refusal::request_in_flight) {
				hide_send_refusal();
			}
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
		refresh_send_label();
		Head->setToolTip(tr("The head the session reads. %1").arg(Chat.session_state_text()));
		// The added context, above the words, only when there is some.
		Chips->setText(Chat.chips_text());
		Chips->setVisible(Chat.chip_count() > 0U);
		// Send, or Stop while the agent works.
		// [[BPR-2000]], RFC-BPR-0101. Send starts a run and wears the agent
		// accent; Stop ends spending and wears the ordinary roles.
		if (State == agent_tab_state::working) {
			wear_ordinary_roles(*Ask);
			Ask->setIcon({ });
			Ask->setText(tr("Stop"));
			Ask->setAccessibleName(tr("Stop"));
			Ask->setToolTip(tr("Stop the agent"));
			Ask->setEnabled(Launch.cancel_action()->isEnabled());
		} else {
			wear_agent_accent(*Ask, palette());
			Ask->setText({ });
			Ask->setIcon(drawer_icon(drawer_glyph::send, Ask->palette()));
			Ask->setAccessibleName(tr("Send"));
			// [[BPR-1817]]. A Send that cannot be pressed says why.
			const auto blocked = send_blocked();
			Ask->setToolTip(blocked == core::agent_send_refusal::none
								? send_tip()
								: send_refusal_words(blocked, agent_name()));
			Ask->setAccessibleDescription(blocked == core::agent_send_refusal::none ? QString{ } : Ask->toolTip());
			Ask->setEnabled(Chat.send_action()->isEnabled() && !Ask->property("conversationLoading").toBool());
		}
		present_banner();
		rebuild_findings();
		Thread->setVisible(!Turns.empty());
		EmptyIntro->setVisible(Turns.empty() && (State == agent_tab_state::idle || State == agent_tab_state::stopped) && !ReadyCard->isVisible());
		ClearSession->setVisible(true);
		ClearSession->setEnabled(!NativeHistoryView.Loading && State != agent_tab_state::working);
		// Owner look v006: the agent shown is asked what it offers, so its
		// models are there when its menu opens.
		if (isVisible() && CatalogDispatch && State != agent_tab_state::setting_up && State != agent_tab_state::not_set_up) {
			if (const auto profile = Launch.selected_profile()) {
				CatalogDispatch(profile->value());
			}
		}
		refresh_suggestions();
		update_turn();
		present_turn_actions();
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
			// A phase change renders now, through refresh, and owes nothing.
			AnswerPending = false;
			refresh();
			return;
		}
		present_banner();
		if (AnswerPacer->isActive()) {
			AnswerPending = true;
		} else {
			update_turn();
			AnswerPacer->start();
		}
		// [[BPR-1817]]. A refusal the host named after a send, before any
		// turn began, is said beside the composer rather than only on the
		// hidden launch panel. A refusal for some other surface's request is
		// not this composer's.
		if (Launch.refusal_count() != RefusalsSeen) {
			RefusalsSeen = Launch.refusal_count();
			if (PendingQuestion || PendingReview) {
				refuse_send(core::agent_send_refusal::launch_refused, Launch.last_refusal());
			}
		}
	}

	void agent_tab_view::begin_turn()
	{
		hide_send_refusal();
		auto next = turn{ };
		if (PendingReview || !PendingQuestion) {
			next.Review   = true;
			next.Question = tr("Review this pull request");
		} else {
			next.Question    = *PendingQuestion;
			next.Name        = PendingName;
			next.Edited      = PendingEdited;
			next.About       = PendingAbout;
			next.DraftTarget = PendingDraftTarget;
			next.Returning   = PendingReturning;
			next.Explain     = PendingExplain;
			if (!next.Explain.isEmpty()) {
				next.ContextFile = next.Explain;
			}
			next.SuggestChange = PendingSuggest;
			if (!next.SuggestChange.isEmpty()) {
				next.ContextFile = next.SuggestChange;
			}
			// Sent: the field is ready for the next question, unless it
			// holds what the reviewer typed before a suggestion was sent.
			if (!PendingKeepsField) {
				Chat.composer()->clear();
			}
		}
		PendingKeepsField = false;
		PendingQuestion.reset();
		PendingAbout.reset();
		PendingName.clear();
		PendingEdited = false;
		PendingDraftTarget.reset();
		PendingReturning.reset();
		PendingExplain.clear();
		PendingSuggest.clear();
		PendingReview  = false;
		FindingsAtTurn = FindingsPresented;
		next.AskedAt   = QDateTime::currentDateTime();
		add_turn_widgets(next);
		Turns.push_back(std::move(next));
		Thread->show();
		if (!Turns.back().Explain.isEmpty()) {
			auto status  = agent_run_status{ };
			status.Agent = Launch.profile_selector()->currentText();
			status.Stage = agent_run_stage::preparing;
			publish_explain(Turns.size() - 1U, status, { }, false, 0);
		}
	}

	void agent_tab_view::add_turn_widgets(turn& aTurn)
	{
		if (aTurn.Serial == 0U) {
			aTurn.Serial = NextSerial++;
		}
		auto* const box = new QWidget{ Thread };
		box->setObjectName(QStringLiteral("agentTurn"));
		auto* const column = new QVBoxLayout{ box };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(4);
		// Owner look v003 and the agent drawers: the question in an outlined
		// bubble, its time right-aligned above it.
		auto* const askedTime = new QLabel{ box };
		askedTime->setObjectName(QStringLiteral("agentTurnAskedTime"));
		askedTime->setForegroundRole(QPalette::PlaceholderText);
		askedTime->setAlignment(Qt::AlignRight);
		column->addWidget(askedTime);
		show_time(*askedTime, aTurn.AskedAt);
		auto* const asked = new rounded{ rounded::look::field, box };
		asked->setObjectName(QStringLiteral("agentTurnQuestion"));
		auto* const askedColumn = new QVBoxLayout{ asked };
		askedColumn->setContentsMargins(10, 6, 10, 6);
		// [[BPR-1773]], owner look 2026-10-27 ("I don't even understand what
		// the question I asked was"). A question about a conversation names
		// it first, in the question's own colour and a heavier weight, so the
		// bubble reads "About <conversation>: <question>".
		if (aTurn.About) {
			auto* const about = new QLabel{ tr("About %1").arg(aTurn.About->Text), asked };
			about->setObjectName(QStringLiteral("agentTurnAbout"));
			about->setTextFormat(Qt::PlainText);
			about->setWordWrap(true);
			auto strong = about->font();
			strong.setWeight(QFont::DemiBold);
			about->setFont(strong);
			about->setToolTip(aTurn.About->Detail);
			askedColumn->addWidget(about);
		}
		// RFC-BPR-0125 (AC07). A suggested prompt's turn shows what was sent
		// under the prompt's name, in the weight About uses.
		// Look review-report-v002 (owner: "I wish the option when it's
		// collapsed took a little less space"): the name and its Show prompt
		// share one row.
		auto* nameRow = static_cast<QHBoxLayout*>(nullptr);
		if (!aTurn.Name.isEmpty()) {
			auto* const name = new QLabel{ aTurn.Name, asked };
			name->setObjectName(QStringLiteral("agentTurnName"));
			name->setTextFormat(Qt::PlainText);
			name->setWordWrap(true);
			auto strong = name->font();
			strong.setWeight(QFont::DemiBold);
			name->setFont(strong);
			nameRow = new QHBoxLayout{ };
			nameRow->setContentsMargins(0, 0, 0, 0);
			nameRow->setSpacing(6);
			// Look review-report-v003 (owner, 2026-11-16: "I would add the
			// little indigo ai symbol next to anything coming from an app
			// prompt in the transcript"): the agent glyph in the Agent
			// accent, before the prompt's name.
			auto* const glyph = new QLabel{ asked };
			glyph->setObjectName(QStringLiteral("agentTurnPromptGlyph"));
			glyph->setPixmap(agent_glyph(palette()).pixmap(QSize{ 14, 14 }));
			glyph->setToolTip(tr("Sent from a prompt"));
			nameRow->addWidget(glyph, 0, Qt::AlignVCenter);
			nameRow->addWidget(name, 1);
			askedColumn->addLayout(nameRow);
		}
		const auto spoken = aTurn.Name.isEmpty() ? aTurn.Question : tr("%1: %2").arg(aTurn.Name, aTurn.Question);
		asked->setAccessibleName(aTurn.About ? tr("About %1: %2").arg(aTurn.About->Text, spoken) : spoken);
		auto* const question = new QLabel{ asked };
		question->setObjectName(QStringLiteral("agentTurnQuestionText"));
		question->setWordWrap(true);
		question->setOpenExternalLinks(false);
		// Owner look v004: a file and line the question names is a link to it.
		connect(question, &QLabel::linkActivated, this, [this](const QString& aLink) { follow_link(aLink); });
		aTurn.QuestionLabel = question;
		// [[BPR-2219]], look review-report-v001 (owner, 2026-11-16: "All of
		// this extra context from a prompt we select should be hidden.. it's
		// just noise. Maybe you can see it with a click or disclosure"): a
		// turn sent under a prompt's name shows the name; what was sent is
		// behind Show prompt.
		// [[BPR-2237]]. Unless the reviewer changed the prompt's text: then it
		// is theirs, shown as written under the name.
		const auto named = !aTurn.Name.isEmpty() && !aTurn.Edited;
		if (named || aTurn.Question.count(QLatin1Char{ '\n' }) >= folded_question_lines) {
			// RFC-BPR-0125 (AC07). Text past six lines folds behind Show all.
			auto* const showAll = new quiet_push_button{ named ? tr("Show prompt") : tr("Show all"), asked };
			showAll->setObjectName(QStringLiteral("agentTurnShowAll"));
			showAll->setFlat(true);
			connect(showAll, &QPushButton::clicked, this, [this, serial = aTurn.Serial] {
				const auto found = std::ranges::find(Turns, serial, &turn::Serial);
				if (found == Turns.end() || found->ShowAll == nullptr) {
					return;
				}
				found->Unfolded = !found->Unfolded;
				found->ShowAll->setText(!found->Name.isEmpty() && !found->Edited ? (found->Unfolded ? tr("Hide prompt") : tr("Show prompt")) : found->Unfolded ? tr("Show less")
																																							   : tr("Show all"));
				show_question(*found);
			});
			aTurn.ShowAll = showAll;
		}
		show_question(aTurn);
		if (aTurn.Review) {
			auto italic = question->font();
			italic.setItalic(true);
			question->setFont(italic);
		}
		askedColumn->addWidget(question);
		if (aTurn.ShowAll != nullptr && nameRow != nullptr) {
			nameRow->addWidget(aTurn.ShowAll, 0, Qt::AlignRight | Qt::AlignTop);
		} else if (aTurn.ShowAll != nullptr) {
			askedColumn->addWidget(aTurn.ShowAll, 0, Qt::AlignLeft);
		}
		column->addWidget(asked);
		// [[BPR-2153]], look steer-v001 (owner: "I would have expected to see
		// Two show up as something I had sent"). What the reviewer steered
		// into the turn shows as their own messages, after the question and
		// before the reply the agent wrote after taking it.
		auto* const steered = new QWidget{ box };
		steered->setObjectName(QStringLiteral("agentTurnSteeredMessages"));
		auto* const steeredColumn = new QVBoxLayout{ steered };
		steeredColumn->setContentsMargins(0, 0, 0, 0);
		steeredColumn->setSpacing(4);
		steered->hide();
		column->addWidget(steered);
		aTurn.SteeredBox = steered;
		// Owner look v003 ("the response contained in something we could
		// copy"): the reply in its own card, its footer as the agent apps
		// have it: Copy and its one keep action, then who answered and when.
		auto* const reply = new rounded{ rounded::look::card, box };
		reply->setObjectName(QStringLiteral("agentTurnReply"));
		auto* const replyColumn = new QVBoxLayout{ reply };
		replyColumn->setContentsMargins(reply_left_margin, 8, reply_right_margin, 6);
		replyColumn->setSpacing(6);
		// [[BPR-1876]]. Over an error whose cause has cleared since: that it
		// is fixed, ahead of the old cause, which then reads as history.
		auto* const fixedNote = new QLabel{ reply };
		fixedNote->setObjectName(QStringLiteral("agentTurnFixedSince"));
		fixedNote->setTextFormat(Qt::RichText);
		fixedNote->setWordWrap(true);
		fixedNote->hide();
		replyColumn->addWidget(fixedNote);
		aTurn.FixedNote = fixedNote;
		// Owner decision (a): the answer reads as the agent wrote it, its
		// Markdown rendered in place, its code coloured as comments' are;
		// links and images do nothing.
		auto* const answer = new markdown_body_view{ reply };
		answer->setObjectName(QStringLiteral("agentTurnAnswer"));
		answer->set_in_place(true);
		// [[BPR-1944]]. Its code blocks are boxes as wide as the chat, with
		// Wrap and Copy; a long line scrolls inside its box.
		answer->set_code_block_tools(true);
		answer->set_height_observer([this, answer] { fit_answer(*answer); });
		// A reference the answer makes a link opens its file at its line;
		// any other link does nothing.
		answer->set_link_dispatch([this](const std::string& aLink) { follow_link(QString::fromStdString(aLink)); });
		dress_answer(*answer);
		answer->hide();
		// RFC-BPR-0116 ([[BPR-2074]]). Was this addressed?'s lead word, said
		// as the agent's claim above its answer; nothing follows from it.
		auto* const claim = new QLabel{ reply };
		claim->setObjectName(QStringLiteral("agentTurnClaim"));
		claim->setForegroundRole(QPalette::PlaceholderText);
		claim->hide();
		replyColumn->addWidget(claim);
		aTurn.Claim = claim;
		// [[BPR-2236]]. A report's verdict, over its summary, as the page
		// shows it.
		auto* const verdict = new QLabel{ reply };
		verdict->setObjectName(QStringLiteral("agentTurnReportVerdict"));
		verdict->setTextFormat(Qt::RichText);
		verdict->hide();
		replyColumn->addWidget(verdict);
		aTurn.Verdict = verdict;
		replyColumn->addWidget(answer);
		// RFC-BPR-0128 ([[BPR-2166]]) as the owner chose it on 2026-11-14
		// (docs/design/report-card-v4, round 1's C, Attachment): the reply
		// card is the only surface. The report's summary shows as the answer
		// does; under it, one attachment row names the report and opens it.
		// Opening runs nothing, so the row wears no accent but the glyph.
		{
			auto* const card = new QWidget{ reply };
			card->setObjectName(QStringLiteral("agentTurnReportCard"));
			auto* const cardColumn = new QVBoxLayout{ card };
			cardColumn->setContentsMargins(0, 9, 0, 0);
			auto* const row = new report_attachment_row{ card };
			row->setObjectName(QStringLiteral("agentTurnReportOpen"));
			const auto serial = aTurn.Serial;
			connect(row, &QPushButton::clicked, this, [this, serial] {
				const auto found = std::ranges::find(Turns, serial, &turn::Serial);
				if (found != Turns.end()) {
					open_report(static_cast<std::size_t>(std::distance(Turns.begin(), found)));
				}
			});
			cardColumn->addWidget(row);
			card->hide();
			replyColumn->addWidget(card);
			aTurn.ReportCard  = card;
			aTurn.ReportTitle = row->title();
			aTurn.ReportMeta  = row->meta();
			aTurn.ReportOpen  = row;
		}

		// [[BPR-2061]], owner 2026-11-10. A reply with several suggested
		// comments drafts them all at once: one quiet action over its cards,
		// what the last press did, and the Undo of that batch. It starts no
		// run, so it wears no Agent accent.
		auto* const draftAllRow = new QWidget{ reply };
		draftAllRow->setObjectName(QStringLiteral("agentTurnDraftAllRow"));
		// The action on its own line; what the last press did under it, with
		// its Undo beside the sentence it takes back.
		auto* const draftAllColumn = new QVBoxLayout{ draftAllRow };
		draftAllColumn->setContentsMargins(0, 4, 0, 0);
		draftAllColumn->setSpacing(4);
		auto* const draftAllLine = new QHBoxLayout{ };
		draftAllLine->setSpacing(6);
		auto* const draftAll = new quiet_push_button{ draftAllRow };
		draftAll->setObjectName(QStringLiteral("agentTurnDraftAll"));
		draftAll->setToolTip(tr("Make each suggested comment below a private draft at its lines, as Add as draft comment does. Nothing is published."));
		draftAllLine->addWidget(draftAll);
		draftAllLine->addStretch(1);
		draftAllColumn->addLayout(draftAllLine);
		auto* const draftAllSaid = new QHBoxLayout{ };
		draftAllSaid->setSpacing(6);
		auto* const draftAllState = new QLabel{ draftAllRow };
		draftAllState->setObjectName(QStringLiteral("agentTurnDraftAllState"));
		draftAllState->setForegroundRole(QPalette::PlaceholderText);
		draftAllState->setWordWrap(true);
		draftAllState->hide();
		draftAllSaid->addWidget(draftAllState, 1);
		auto* const draftAllUndo = new quiet_push_button{ tr("Undo"), draftAllRow };
		draftAllUndo->setObjectName(QStringLiteral("agentTurnDraftAllUndo"));
		draftAllUndo->setFlat(true);
		draftAllUndo->setToolTip(tr("Remove the drafts this made. A draft you have edited since is kept."));
		draftAllUndo->hide();
		draftAllSaid->addWidget(draftAllUndo, 0, Qt::AlignTop);
		draftAllColumn->addLayout(draftAllSaid);
		draftAllRow->hide();
		replyColumn->addWidget(draftAllRow);
		aTurn.DraftAllRow   = draftAllRow;
		aTurn.DraftAll      = draftAll;
		aTurn.DraftAllState = draftAllState;
		aTurn.DraftAllUndo  = draftAllUndo;
		connect(draftAll, &QPushButton::clicked, this, [this, draftAll] {
			if (const auto turn = turn_of_widget(draftAll)) {
				draft_all(*turn);
			}
		});
		connect(draftAllUndo, &QPushButton::clicked, this, [this, draftAllUndo] {
			if (const auto turn = turn_of_widget(draftAllUndo)) {
				undo_draft_all(*turn);
			}
		});
		// Owner decision 2026-10-23: the turn's findings, as cards, under its answer.
		auto* const cards = new QVBoxLayout{ };
		cards->setSpacing(8);
		replyColumn->addLayout(cards);
		// While the agent works, and how it ended: one quiet line, the one
		// the tour chats' turns show too ([[BPR-1941]]).
		auto* const footer = new agent_run_status_line{ reply };
		footer->setObjectName(QStringLiteral("agentTurnFooter"));
		replyColumn->addWidget(footer);
		// [[BPR-2005]], RFC-BPR-0103. Under the line: its helpers, the quiet
		// notice, and after the turn the folded line.
		auto* const activity = new agent_activity_view{ reply };
		activity->setObjectName(QStringLiteral("agentTurnActivity"));
		activity->set_stop([this] {
			if (auto* const stop = Launch.cancel_action(); stop != nullptr && stop->isEnabled()) {
				stop->trigger();
			}
		});
		replyColumn->addWidget(activity);
		auto* const footerRow = new QHBoxLayout{ };
		footerRow->setSpacing(2);
		auto* const copy = make_drawer_tool(reply, drawer_icon(drawer_glyph::copy, palette()), tr("Copy the response as Markdown"), QStringLiteral("agentTurnCopy"));
		copy->set_surface(QPalette::Base);
		copy->hide();
		footerRow->addWidget(copy);
		// RFC-BPR-0127. One keep action: Keep local where no thread or line
		// takes a draft (`core::decide_keep`), else Draft comment below. Keep
		// local keeps the whole reply, or the part selected in it, as a Local
		// comment on the pull request.
		auto* const save = new quiet_push_button{ tr("Keep local"), reply };
		save->setObjectName(QStringLiteral("agentTurnKeepLocal"));
		save->setFlat(true);
		save->setToolTip(tr("Keep this reply, or the part of it you selected, as a local comment on the pull request. Only on this computer."));
		save->hide();
		footerRow->addWidget(save);
		// [[BPR-1773]], owner look 2026-10-27 ("let you turn anything into a
		// note or draft comment directly"). Draft comment, in Keep local's
		// place and style: the whole reply, or the part selected, as a
		// private draft where the host decided when it was asked (the
		// conversation's reply or the lines asked on). Nothing is published.
		// Once drafted it goes to the draft.
		// Owner, look v006 ("That's too subtle in the agent context" and
		// "Don't have keep local there, just draft is fine, they can move it
		// to local after"): the answer's one keep action is Draft, a raised
		// button with the pencil and its name, as large as the footer's
		// others; where no line or thread takes a draft it is disabled with
		// the pencil struck through and says why in its tooltip.
		auto* const draft = new quiet_push_button{ tr("Draft"), reply };
		draft->setObjectName(QStringLiteral("agentTurnDraftComment"));
		draft->setFlat(false);
		draft->setIcon(conversation_icon(conversation_glyph::edit, palette()));
		draft->hide();
		footerRow->addWidget(draft);
		// [[BPR-2138]], RFC-BPR-0116. Ask more on a returning question's
		// fresh answer: the next question here opens with it.
		auto* const askMore = new quiet_push_button{ tr("Ask more"), reply };
		askMore->setObjectName(QStringLiteral("agentTurnAskMore"));
		askMore->setFlat(true);
		askMore->hide();
		footerRow->addWidget(askMore);
		aTurn.AskMore = askMore;
		connect(askMore, &QPushButton::clicked, this, [this, askMore] {
			for (const auto& kept : Turns) {
				if (kept.AskMore == askMore) {
					if (AskMoreDispatch) {
						AskMoreDispatch(kept.Question, kept.AnswerText);
					}
					Chat.composer()->setPlaceholderText(tr("Ask more about this answer…"));
					Chat.composer()->setFocus(Qt::OtherFocusReason);
					return;
				}
			}
		});
		// [[BPR-1847]]. An error's one way back, in place of the keep
		// action, which an error never offers.
		auto* const recovery = new quiet_push_button{ reply };
		recovery->setObjectName(QStringLiteral("agentTurnRecovery"));
		recovery->setFlat(true);
		recovery->hide();
		footerRow->addWidget(recovery);
		// [[BPR-1876]], owner look v001: "We should also be able to delete a
		// message that was never sent to the agent instead of just resend."
		// [[BPR-1880]]: on every error; its tooltip is set with its actions.
		auto* const remove = new quiet_push_button{ tr("Delete"), reply };
		remove->setObjectName(QStringLiteral("agentTurnDelete"));
		remove->setFlat(true);
		remove->hide();
		footerRow->addWidget(remove);
		footerRow->addStretch(1);
		auto* const answeredTime = new QLabel{ reply };
		answeredTime->setObjectName(QStringLiteral("agentTurnAnsweredTime"));
		answeredTime->setForegroundRole(QPalette::PlaceholderText);
		answeredTime->hide();
		footerRow->addWidget(answeredTime);
		replyColumn->addLayout(footerRow);
		column->addWidget(reply);
		ThreadColumn->addWidget(box);
		aTurn.Widget       = box;
		aTurn.Answer       = answer;
		aTurn.Footer       = footer;
		aTurn.Activity     = activity;
		aTurn.SaveNote     = save;
		aTurn.CardColumn   = cards;
		aTurn.Copy         = copy;
		aTurn.AskedTime    = askedTime;
		aTurn.AnsweredTime = answeredTime;
		aTurn.Draft        = draft;
		aTurn.Asked        = asked;
		aTurn.Reply        = reply;
		aTurn.Recovery     = recovery;
		aTurn.Delete       = remove;
		connect(recovery, &QPushButton::clicked, this, [this, recovery] {
			for (const auto& kept : Turns) {
				if (kept.Recovery == recovery) {
					recover(kept);
					return;
				}
			}
		});
		connect(remove, &QPushButton::clicked, this, [this, remove] {
			for (auto index = std::size_t{ 0U }; index < Turns.size(); ++index) {
				if (Turns[index].Delete == remove) {
					delete_turn(index);
					return;
				}
			}
		});
		connect(draft, &QPushButton::clicked, this, [this, draft] {
			for (auto& kept : Turns) {
				if (kept.Draft != draft) {
					continue;
				}
				const auto key = kept.DraftTarget ? kept.DraftTarget->Key : std::string{ };
				// As Go to comment does: once drafted, the button opens the draft.
				if (kept.Drafted) {
					if (DraftNavigate) {
						DraftNavigate(key);
					}
					return;
				}
				if (!DraftDispatch) {
					return;
				}
				auto       text     = kept.AnswerText;
				const auto selected = kept.Answer != nullptr ? kept.Answer->rendered_view()->textCursor().selectedText() : QString{ };
				if (!selected.trimmed().isEmpty()) {
					text = selected;
					text.replace(QChar::ParagraphSeparator, QLatin1Char{ '\n' });
				}
				draft->setEnabled(false);
				DraftDispatch(key, text, [this, guarded = QPointer{ draft }](const bool aDrafted, const QString& aWhy) {
					if (guarded == nullptr) {
						return;
					}
					if (!aDrafted) {
						guarded->setText(tr("Not drafted: %1").arg(aWhy));
						guarded->setEnabled(true);
						for (auto& turn : Turns) {
							if (turn.Draft == guarded) {
								present_keep(turn);
							}
						}
						return;
					}
					for (auto& turn : Turns) {
						if (turn.Draft == guarded) {
							turn.Drafted = true;
						}
					}
					const auto navigable = static_cast<bool>(DraftNavigate);
					guarded->setText(navigable ? tr("Go to draft") : tr("Drafted"));
					guarded->setToolTip(navigable ? tr("Open the draft this reply was written into") : QString{ });
					guarded->setEnabled(navigable);
					for (auto& turn : Turns) {
						if (turn.Draft == guarded) {
							present_keep(turn);
						}
					}
				});
				return;
			}
		});
		connect(copy, &QToolButton::clicked, this, [this, copy] {
			for (const auto& kept : Turns) {
				if (kept.Copy == copy) {
					QGuiApplication::clipboard()->setText(kept.AnswerText);
					copy->setToolTip(tr("Copied"));
				}
			}
		});
		connect(save, &QPushButton::clicked, this, [this, save] {
			for (auto& kept : Turns) {
				if (kept.SaveNote != save) {
					continue;
				}
				// Owner look v004: once kept, the button goes to the comment.
				if (kept.Note != 0U) {
					if (NoteNavigate) {
						NoteNavigate(kept.Note);
					}
					return;
				}
				if (!NoteDispatch) {
					return;
				}
				auto       text     = kept.AnswerText;
				const auto selected = kept.Answer != nullptr ? kept.Answer->rendered_view()->textCursor().selectedText() : QString{ };
				if (!selected.trimmed().isEmpty()) {
					text = selected;
					text.replace(QChar::ParagraphSeparator, QLatin1Char{ '\n' });
				}
				save->setEnabled(false);
				NoteDispatch(text, [this, guarded = QPointer{ save }](const bool aSaved, const QString& aWhy, const std::uint64_t aNote) {
					if (guarded == nullptr) {
						return;
					}
					if (!aSaved) {
						guarded->setText(tr("Not kept: %1").arg(aWhy));
						guarded->setEnabled(true);
						for (auto& turn : Turns) {
							if (turn.SaveNote == guarded) {
								present_keep(turn);
							}
						}
						return;
					}
					for (auto& turn : Turns) {
						if (turn.SaveNote == guarded) {
							turn.Note = aNote;
						}
					}
					const auto navigable = aNote != 0U && NoteNavigate;
					guarded->setText(navigable ? tr("Go to comment") : tr("Kept local"));
					guarded->setToolTip(navigable ? tr("Open the local comment this reply was kept as") : QString{ });
					guarded->setEnabled(navigable);
					for (auto& turn : Turns) {
						if (turn.SaveNote == guarded) {
							present_keep(turn);
						}
					}
				});
				return;
			}
		});
	}

	void agent_tab_view::set_answer(turn& aTurn, const QString& aText)
	{
		if (aTurn.Answer == nullptr || (aText == aTurn.AnswerText && aTurn.Answer->isHidden() == aText.isEmpty())) {
			return;
		}
		aTurn.AnswerText = aText;
		aTurn.FindingsBlock.clear();
		// The findings block is for the cards; the reply shows without it.
		// [[BPR-1861]]. The core ends the block where its JSON ends, so a
		// suggested comment's code fence cannot cut it in half, and shows a
		// block it cannot read whole, as code, never as a torn Markdown tail.
		if (aTurn.AnswerText.contains(QLatin1StringView{ core::agent_findings_fence.data(), static_cast<qsizetype>(core::agent_findings_fence.size()) })) {
			auto split = core::split_agent_answer(aTurn.AnswerText.toStdString());
			if (split.Block != core::agent_findings_block::absent) {
				aTurn.AnswerText = QString::fromStdString(split.Prose);
			}
			if (split.Block == core::agent_findings_block::complete) {
				aTurn.FindingsBlock = std::move(split.Payload);
			}
		}
		show_answer(aTurn);
		if (aTurn.Claim != nullptr) {
			const auto claim = aTurn.Returning == core::returning_operation::proposed_fix ? core::read_addressed_claim(aTurn.AnswerText.toStdString()) : std::nullopt;
			aTurn.Claim->setText(claim ? tr("Agent's claim: %1").arg(QString::fromUtf8(core::claim_words(*claim).data(), static_cast<qsizetype>(core::claim_words(*claim).size()))) : QString{ });
			aTurn.Claim->setVisible(claim.has_value());
			// RFC-BPR-0128 ([[BPR-2165]]). The claim reaches its thread's chip.
			// (A claim's answer is never folded into a report card.)
			if (claim && aTurn.About && ClaimObserver) {
				ClaimObserver(aTurn.About->Key, *claim, Turns.empty() ? 0U : static_cast<std::size_t>(&aTurn - Turns.data()));
			}
		}
		// RFC-BPR-0128 as amended (look ask-v005): one ask about several
		// threads lists a claim a thread; each reaches its thread's chip, and
		// the answer shows its prose with what the claims came to.
		if (aTurn.Returning == core::returning_operation::addressed_threads) {
			const auto claims = core::read_addressed_claims(aTurn.AnswerText.toStdString());
			if (ClaimObserver) {
				for (const auto& [thread, claim] : claims) {
					ClaimObserver(thread, claim, Turns.empty() ? 0U : static_cast<std::size_t>(&aTurn - Turns.data()));
				}
			}
			const auto fence = aTurn.AnswerText.indexOf(QLatin1StringView{ core::addressed_claims_fence.data(), static_cast<qsizetype>(core::addressed_claims_fence.size()) });
			if (fence >= 0 && !claims.empty()) {
				auto       prose = aTurn.AnswerText.left(fence).trimmed();
				const auto close = aTurn.AnswerText.indexOf(QStringLiteral("\n```"), fence + 3);
				if (close >= 0) {
					const auto after = aTurn.AnswerText.mid(close + 4).trimmed();
					if (!after.isEmpty()) {
						prose += QStringLiteral("\n\n") + after;
					}
				}
				auto counts = std::array<int, core::addressed_claim_count>{ };
				for (const auto& [thread, claim] : claims) {
					++counts[static_cast<std::size_t>(claim)];
				}
				auto parts = QStringList{ };
				for (std::size_t index = 0U; index < counts.size(); ++index) {
					if (counts[index] > 0) {
						const auto words = core::claim_words(static_cast<core::addressed_claim>(index));
						parts << QStringLiteral("%1 %2").arg(counts[index]).arg(QString::fromUtf8(words.data(), static_cast<qsizetype>(words.size())).toLower());
					}
				}
				// What the claims came to belongs with the summary, before the
				// first thread's section.
				const auto said = tr("Claims on %n thread(s): %1. Each is on its thread in Comments.", nullptr, static_cast<int>(claims.size())).arg(parts.join(QStringLiteral(", ")));
				// The first heading after the answer's first line (a title on
				// the first line stays the title).
				const auto heading = prose.indexOf(QRegularExpression{ QStringLiteral("\n#{1,6} ") });
				aTurn.AnswerText   = heading >= 0 ? prose.left(heading).trimmed() + QStringLiteral("\n\n") + said + QStringLiteral("\n\n") + prose.mid(heading + 1) : prose + QStringLiteral("\n\n") + said;
				show_answer(aTurn);
			}
		}
	}

	void agent_tab_view::show_answer(turn& aTurn)
	{
		if (aTurn.Answer == nullptr) {
			return;
		}
		// RFC-BPR-0128 ([[BPR-2166]]), Q7. A report by its shape: the card
		// stands in the answer's place; the answer stays the turn's text.
		const auto answer = aTurn.AnswerText.toStdString();
		// Q4: Explain this file and What changed since my review answer as
		// reports whatever their length.
		// Look ask-v007 ("Why does asking was this addressed on the PR ...
		// not show an attachment report?"): it was left out on purpose, as a
		// claim's answer; one about several threads is a report too, a section
		// a thread, with its claims on the threads.
		const auto many   = aTurn.Returning == core::returning_operation::addressed_threads;
		const auto always = !aTurn.Explain.isEmpty() || aTurn.Returning == core::returning_operation::latest_upload || many;
		// [[BPR-2246]]. A turn that ended without an answer has no report:
		// its text is why it stopped, shown in the thread.
		const auto report = aTurn.ReportCard != nullptr && aTurn.Returning != core::returning_operation::proposed_fix
			&& core::shows_report_attachment({ .AlwaysReport = always, .Ended = aTurn.Done, .Answered = aTurn.Outcome == core::agent_turn_outcome::answered }, answer);
		auto summary = QString{ };
		if (aTurn.ReportCard != nullptr) {
			if (report) {
				const auto named = !always     ? QString{ }
					: many                     ? aTurn.Question
					: !aTurn.Explain.isEmpty() ? tr("Explain %1").arg(aTurn.Explain)
											   : tr("What changed since your review");
				// [[BPR-2219]]. The card reads the report, not what the agent
				// said while it worked.
				const auto card = core::report_card_of(core::read_report_answer(answer).Report, named.toStdString());
				aTurn.ReportTitle->setText(QString::fromStdString(card.Title));
				// Round 1's wording; defect 2: "1 section", counted without
				// the title.
				aTurn.ReportMeta->setText(card.Sections == 0U       ? tr("Report")
											  : card.Sections == 1U ? tr("Report, 1 section")
																	: tr("Report, %1 sections").arg(card.Sections));
				aTurn.ReportOpen->setAccessibleName(tr("Open report: %1").arg(aTurn.ReportTitle->text()));
				aTurn.ReportOpen->setToolTip(tr("Open report"));
				aTurn.ReportOpen->updateGeometry();
				summary           = QString::fromStdString(card.Summary);
				aTurn.VerdictWord = card.Verdict;
				aTurn.VerdictTone = card.VerdictTone;
			} else {
				aTurn.VerdictWord.clear();
				aTurn.VerdictTone = core::report_tone::neutral;
			}
			show_verdict(aTurn);
			aTurn.ReportCard->setVisible(report);
		}

		// Owner look v004: the files and lines the answer names are links;
		// Copy and Keep local keep the answer as the agent wrote it.
		// [[BPR-1998]]: the core's one recogniser, for every agent reply.
		// A report shows its summary here, rendered (defect 3); the whole
		// report is its tab.
		const auto body  = report ? summary : aTurn.AnswerText;
		const auto shown = LineNavigate && LineResolve ? agent_markdown_with_reference_links(body, LineResolve(), aTurn.ContextFile) : body;
		aTurn.Answer->set_body(shown.toStdString(), "agent-turn-" + std::to_string(aTurn.Serial) + (report ? "-summary" : ""));
		aTurn.Answer->setVisible(!body.isEmpty());
		fit_answer(*aTurn.Answer);
	}

	void agent_tab_view::show_steered(turn& aTurn)
	{
		if (aTurn.SteeredBox == nullptr) {
			return;
		}
		auto* const column = qobject_cast<QVBoxLayout*>(aTurn.SteeredBox->layout());
		while (column != nullptr && column->count() > 0) {
			auto* const item = column->takeAt(0);
			if (auto* const widget = item->widget(); widget != nullptr) {
				widget->hide();
				widget->deleteLater();
			}
			delete item;
		}
		for (const auto& message : aTurn.Steered) {
			auto* const bubble = new rounded{ rounded::look::field, aTurn.SteeredBox };
			bubble->setObjectName(QStringLiteral("agentTurnSteered"));
			auto* const bubbleColumn = new QVBoxLayout{ bubble };
			bubbleColumn->setContentsMargins(10, 6, 10, 6);
			bubbleColumn->setSpacing(2);
			const auto  state = agent_steered_state_words(message);
			auto* const label = new QLabel{ state, bubble };
			label->setObjectName(QStringLiteral("agentTurnSteeredState"));
			label->setForegroundRole(QPalette::PlaceholderText);
			label->setWordWrap(true);
			bubbleColumn->addWidget(label);
			auto* const text = new QLabel{ QString::fromStdString(message.Text), bubble };
			text->setObjectName(QStringLiteral("agentTurnSteeredText"));
			text->setTextFormat(Qt::PlainText);
			text->setWordWrap(true);
			text->setTextInteractionFlags(Qt::TextSelectableByMouse);
			bubbleColumn->addWidget(text);
			bubble->setAccessibleName(agent_steered_accessible_name(message));
			column->addWidget(bubble);
		}
		aTurn.SteeredBox->setVisible(!aTurn.Steered.empty());
	}

	void agent_tab_view::show_question(turn& aTurn)
	{
		if (aTurn.QuestionLabel == nullptr) {
			return;
		}
		// [[BPR-1998]]. The question's references, by the same recogniser,
		// each a link to what it opens; offsets are the text's UTF-8 bytes.
		struct shown_reference
		{
			qsizetype              Begin  = 0;
			qsizetype              Length = 0;
			agent_reference_target Target;
		};
		auto refs = std::vector<shown_reference>{ };
		if (LineNavigate && LineResolve) {
			const auto files = LineResolve();
			const auto bytes = aTurn.Question.toUtf8();
			for (const auto& found : core::find_agent_references(std::string_view{ bytes.constData(), static_cast<std::size_t>(bytes.size()) }, false)) {
				const auto paths = core::resolve_agent_reference(found.Path, files);
				if (paths.empty()) {
					continue;
				}
				auto target = agent_reference_target{ .First = found.First, .Last = found.Last, .Before = found.Before };
				for (const auto& path : paths) {
					target.Paths << QString::fromStdString(path);
				}
				const auto begin = QString::fromUtf8(bytes.left(static_cast<qsizetype>(found.Begin))).size();
				refs.push_back({ .Begin = begin, .Length = QString::fromUtf8(bytes.mid(static_cast<qsizetype>(found.Begin), static_cast<qsizetype>(found.Length))).size(), .Target = std::move(target) });
			}
		}
		// The one file the question names is where its answer's bare lines are.
		aTurn.ContextFile.clear();
		for (const auto& ref : refs) {
			if (ref.Target.Paths.size() != 1) {
				aTurn.ContextFile.clear();
				break;
			}
			if (aTurn.ContextFile.isEmpty()) {
				aTurn.ContextFile = ref.Target.Paths.front();
			} else if (aTurn.ContextFile != ref.Target.Paths.front()) {
				aTurn.ContextFile.clear();
				break;
			}
		}
		// RFC-BPR-0125 (AC07). Folded, the first six lines show, and only
		// the references within them are links.
		// [[BPR-2219]]. A named turn's text shows whole, or not at all.
		if (aTurn.QuestionLabel != nullptr) {
			aTurn.QuestionLabel->setVisible(aTurn.Name.isEmpty() || aTurn.Edited || aTurn.Unfolded);
		}
		auto shown = aTurn.Question;
		if (aTurn.ShowAll != nullptr && !aTurn.Unfolded && (aTurn.Name.isEmpty() || aTurn.Edited)) {
			shown = aTurn.Question.section(QLatin1Char{ '\n' }, 0, folded_question_lines - 1) + QStringLiteral("…");
			std::erase_if(refs, [cut = shown.size() - 1](const shown_reference& aRef) { return aRef.Begin + aRef.Length > cut; });
		}
		auto html = QString{ };
		auto at   = qsizetype{ 0 };
		for (const auto& ref : refs) {
			html += shown.mid(at, ref.Begin - at).toHtmlEscaped();
			html += QStringLiteral("<a href=\"%1\">%2</a>").arg(agent_reference_link_text(ref.Target).toHtmlEscaped(), shown.mid(ref.Begin, ref.Length).toHtmlEscaped());
			at = ref.Begin + ref.Length;
		}
		html += shown.mid(at).toHtmlEscaped();
		html.replace(QLatin1Char{ '\n' }, QStringLiteral("<br>"));
		aTurn.QuestionLabel->setTextFormat(refs.empty() ? Qt::PlainText : Qt::RichText);
		aTurn.QuestionLabel->setText(refs.empty() ? shown : html);
		aTurn.QuestionLabel->setTextInteractionFlags(refs.empty() ? Qt::TextSelectableByMouse : Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
		aTurn.QuestionLabel->setAccessibleName(aTurn.Question);
		aTurn.QuestionLabel->setToolTip(refs.empty() ? QString{ } : tr("Click a file and line to open it in Review"));
	}

	void agent_tab_view::follow_link(const QString& aLink)
	{
		if (LineNavigate && LineResolve) {
			static_cast<void>(follow_agent_reference(this, aLink, LineNavigate, ReferenceChooser));
		}
	}

	void agent_tab_view::set_note_navigation(agent_note_navigation aNavigate) { NoteNavigate = std::move(aNavigate); }

	void agent_tab_view::set_catalog_dispatch(std::function<void(std::uint64_t)> aDispatch) { CatalogDispatch = std::move(aDispatch); }

	void agent_tab_view::set_reset_dispatch(std::function<void()> aReset, std::function<bool()> aConfirm)
	{
		ResetDispatch = std::move(aReset);
		ResetConfirm  = std::move(aConfirm);
	}

	void agent_tab_view::reset_agents()
	{
		if (!panels_alive() || State == agent_tab_state::working || !ResetDispatch) {
			return;
		}
		const auto confirmed = ResetConfirm ? ResetConfirm()
											: QMessageBox::warning(this, tr("Reset agents?"),
																   tr("This removes every agent you added and its model, effort and check, the clone folders and the clones linked to repositories, "
																	  "and your edited review skill (a copy is saved to Downloads first). Notes, drafts and agent conversations stay. Setup then starts again."),
																   QMessageBox::Reset | QMessageBox::Cancel, QMessageBox::Cancel)
				== QMessageBox::Reset;
		if (!confirmed) {
			return;
		}
		ResetDispatch();
		if (SetupView != nullptr) {
			SetupView->begin(core::setup_step::agent);
		}
		refresh();
	}

	void agent_tab_view::set_line_links(agent_reference_files_source aFiles, agent_reference_navigation aNavigate)
	{
		LineResolve  = std::move(aFiles);
		LineNavigate = std::move(aNavigate);
		for (auto& kept : Turns) {
			show_question(kept);
			show_answer(kept);
		}
	}

	void agent_tab_view::set_inline_code_file(const QString& aFileName)
	{
		if (InlineCodeFile == aFileName) {
			return;
		}
		InlineCodeFile = aFileName;
		for (const auto& kept : Turns) {
			if (kept.Answer != nullptr) {
				kept.Answer->set_inline_code_selector(InlineCodeFile.toStdString());
			}
		}
	}

	auto agent_tab_view::turn_question_label(const std::size_t aTurn) const -> QLabel* { return aTurn < Turns.size() ? Turns[aTurn].QuestionLabel.data() : nullptr; }

	auto agent_tab_view::turn_answer_view(const std::size_t aTurn) const -> markdown_body_view* { return aTurn < Turns.size() ? Turns[aTurn].Answer.data() : nullptr; }

	void agent_tab_view::fit_answer(markdown_body_view& aAnswer) const
	{
		// The answer is the viewport less the conversation's margins and its
		// reply card's. Owner look v004: measured without the card's, an
		// answer was given the height of a wider one and lost its last line.
		constexpr auto margins = 12 + 12 + reply_left_margin + reply_right_margin;
		const auto     width   = Viewport == nullptr ? aAnswer.width() : Viewport->width() - margins;
		aAnswer.setFixedHeight(aAnswer.isHidden() ? 0 : aAnswer.heightForWidth(std::max(1, width)));
	}

	void agent_tab_view::present_thread(const std::vector<agent_thread_turn>& aTurns)
	{
		// Owner report 2026-11-10. A turn shown again gets its cards back.
		retain_turn_cards();
		for (auto& kept : Turns) {
			if (kept.Widget != nullptr) {
				kept.Widget->hide();
				ThreadColumn->removeWidget(kept.Widget);
				kept.Widget->deleteLater();
			}
		}
		// [[BPR-2184]] (review N5). A turn drawn again in its place keeps its
		// identity and ending, so a report keyed by it still finds it.
		auto before = std::vector<std::tuple<QString, std::uint64_t, QString>>{ };
		before.reserve(Turns.size());
		for (const auto& kept : Turns) {
			before.emplace_back(kept.Question, kept.Serial, kept.Ending);
		}
		Turns.clear();
		// A turn's cards went with its widgets.
		std::erase_if(CardEntries, [](const card_entry& aEntry) { return aEntry.Turn.has_value(); });
		for (const auto& shown : aTurns) {
			auto next = turn{ };
			// [[BPR-2241]]. A turn kept with how it ended as an outcome
			// rather than the line it settled with (a native session's, or
			// one no window saw end) is said as the live turn said it.
			const auto worded = shown.Footer.contains(QLatin1Char{ ' ' });
			const auto ending = worded ? shown.Footer.section(QStringLiteral(" · "), 0, 0)
									   : agent_turn_ending_words(Launch.profile_selector()->currentText(), shown.Outcome, shown.Seconds);
			const auto footer = worded ? shown.Footer : ending + QLatin1Char{ '.' };
			if (const auto at = Turns.size(); at < before.size() && std::get<0>(before[at]) == shown.Question) {
				next.Serial = std::get<1>(before[at]);
				next.Ending = std::get<2>(before[at]);
			} else {
				next.Ending = ending;
			}
			next.Question   = shown.Question;
			next.Name       = shown.Name;
			next.Edited     = shown.Edited;
			next.Done       = true;
			next.AskedAt    = shown.AskedAt;
			next.AnsweredAt = shown.AnsweredAt;
			next.Outcome    = shown.Outcome;
			next.Steered    = shown.Steered;
			add_turn_widgets(next);
			show_steered(next);
			// [[BPR-1860]]. Kept before its answer is shown: the code host
			// answers a text it has seen from its cache inside the ask, and
			// `apply_code_syntax` hands spans only to the turns kept here.
			Turns.push_back(std::move(next));
			auto& kept = Turns.back();
			set_answer(kept, shown.Answer);
			kept.Footer->setText(footer);
			// [[BPR-2005]]. A turn this window saw end keeps its line and its
			// folded block when the thread is drawn again.
			// The same question in the same order: the latest such turn not
			// already matched.
			const auto asked   = std::ranges::count_if(Turns, [&kept](const turn& aEarlier) { return aEarlier.Question == kept.Question; });
			auto       seen    = EndedTurns.end();
			auto       matched = decltype(asked){ 0 };
			for (auto at = EndedTurns.begin(); at != EndedTurns.end(); ++at) {
				if (at->Question == kept.Question && ++matched == asked) {
					seen = at;
				}
			}
			if (seen != EndedTurns.end()) {
				if (!worded) {
					kept.Footer->setText(seen->Footer);
				}
				if (!seen->Ending.isEmpty()) {
					set_answer(kept, seen->Ending);
				}
				if (kept.Activity != nullptr) {
					kept.Activity->fold(seen->Activity, seen->Seconds);
				}
			}
			settle_reply_footer(kept);
			// The cards this view still holds are the freshest; otherwise
			// those the host read from the stored answer, as they were left.
			if (!restore_turn_cards(Turns.size() - 1U)) {
				make_restored_cards(Turns.size() - 1U, shown.Cards);
			}
		}
		Finished = false;
		present_session_cost(std::nullopt);
		refresh();
	}

	void agent_tab_view::set_footer_observers(agent_tab_run_observer aRun, std::function<void(const QString&)> aEnded, std::function<void(std::optional<core::agent_cost>)> aCost)
	{
		RunObserver   = std::move(aRun);
		EndedObserver = std::move(aEnded);
		CostObserver  = std::move(aCost);
	}

	void agent_tab_view::present_session_cost(const std::optional<core::agent_cost> aTotal)
	{
		SessionTotal = aTotal;
		if (CostObserver) {
			CostObserver(aTotal);
		}
		if (SessionCostLine == nullptr) {
			return;
		}
		SessionCostLine->setText(aTotal ? tr("%1 this session").arg(QString::fromStdString(core::agent_cost_words(*aTotal))) : QString{ });
		SessionCostLine->setVisible(aTotal.has_value());
	}

	auto agent_tab_view::send_tip() const -> QString
	{
		return tr("Send the question to the agent (Return). Shift+Return starts a new line.");
	}

	void agent_tab_view::make_restored_cards(const std::size_t aTurn, const std::vector<agent_restored_finding_card>& aCards)
	{
		if (aTurn >= Turns.size() || Turns[aTurn].CardColumn == nullptr) {
			return;
		}
		for (const auto& restored : aCards) {
			CardEntries.push_back(card_entry{
				.Session  = restored.Session,
				.Ordinal  = restored.Ordinal,
				.Finding  = restored.Finding,
				.Anchored = false,
				// A card that was being worked on when the application closed
				// is open again.
				.State = restored.State == agent_finding_card_state::working ? agent_finding_card_state::open : restored.State,
				.Why   = restored.Why,
				.Note  = restored.State == agent_finding_card_state::noted ? restored.Note : 0U,
				.Turn  = aTurn,
			});
			auto* const widget = make_card(CardEntries.back(), Turns[aTurn].Widget);
			Turns[aTurn].CardColumn->addWidget(widget);
		}
	}

	void agent_tab_view::observe_card_state(const std::string& aFindingsBlock, const card_entry& aEntry) const
	{
		if (!CardObserver || aFindingsBlock.empty() || aEntry.State == agent_finding_card_state::working) {
			return;
		}
		CardObserver({ .FindingsBlock = aFindingsBlock, .Ordinal = aEntry.Ordinal, .State = aEntry.State, .Why = aEntry.Why, .Note = aEntry.Note });
	}

	void agent_tab_view::retain_turn_cards()
	{
		for (std::size_t index = 0U; index < Turns.size(); ++index) {
			auto kept = retained_cards{ .Answer = Turns[index].AnswerText.trimmed(), .FindingsBlock = Turns[index].FindingsBlock, .Cards = { } };
			for (const auto& entry : CardEntries) {
				if (entry.Turn == std::optional{ index }) {
					auto card   = entry;
					card.Turn   = std::nullopt;
					card.Widget = nullptr;
					kept.Cards.push_back(std::move(card));
				}
			}
			if (!kept.Cards.empty() && (!kept.Answer.isEmpty() || !kept.FindingsBlock.empty())) {
				RetainedCards.push_back(std::move(kept));
			}
		}
		if (RetainedCards.size() > maximum_retained_card_turns) {
			RetainedCards.erase(RetainedCards.begin(), RetainedCards.end() - static_cast<std::ptrdiff_t>(maximum_retained_card_turns));
		}
	}

	auto agent_tab_view::restore_turn_cards(const std::size_t aTurn) -> bool
	{
		if (aTurn >= Turns.size() || Turns[aTurn].CardColumn == nullptr) {
			return false;
		}
		const auto& shown  = Turns[aTurn];
		const auto  answer = shown.AnswerText.trimmed();
		// The findings block names the cards exactly. The answer's words do
		// when the block is not there: findings that came without one, or the
		// kept thread, which keeps the answer as shown.
		const auto found = std::ranges::find_if(RetainedCards, [&shown, &answer](const retained_cards& aKept) {
			return (!aKept.FindingsBlock.empty() && aKept.FindingsBlock == shown.FindingsBlock) || (!answer.isEmpty() && aKept.Answer == answer);
		});
		if (found == RetainedCards.end()) {
			return false;
		}
		auto cards = std::move(found->Cards);
		RetainedCards.erase(found);
		for (auto& card : cards) {
			card.Turn = aTurn;
			CardEntries.push_back(std::move(card));
			auto* const widget = make_card(CardEntries.back(), Turns[aTurn].Widget);
			Turns[aTurn].CardColumn->addWidget(widget);
		}
		return true;
	}

	void agent_tab_view::set_thread_observer(std::function<void(std::vector<agent_thread_turn>)> aObserver) { ThreadObserver = std::move(aObserver); }

	auto agent_tab_view::thread_turns() const -> std::vector<agent_thread_turn>
	{
		auto result = std::vector<agent_thread_turn>{ };
		for (const auto& kept : Turns) {
			if (kept.Done) {
				// Owner report 2026-11-10, after BPR-2052. The kept thread keeps
				// the findings block its cards come from, so they are made again
				// when it is read back after a restart. Shown again here, the
				// answer shows without it. A block the kept text could not hold
				// whole is left off rather than torn.
				auto answer = kept.AnswerText;
				if (!kept.FindingsBlock.empty()) {
					auto withBlock = answer + QStringLiteral("\n\n") + QLatin1StringView{ core::agent_findings_fence.data(), static_cast<qsizetype>(core::agent_findings_fence.size()) }
						+ QLatin1Char{ '\n' } + QString::fromStdString(kept.FindingsBlock) + QStringLiteral("\n```");
					if (static_cast<std::size_t>(withBlock.toUtf8().size()) <= application::maximum_conversation_text_bytes) {
						answer = std::move(withBlock);
					}
				}
				result.push_back({ .Question   = kept.Question,
								   .Name       = kept.Name,
								   .Edited     = kept.Edited,
								   .Answer     = answer,
								   .Footer     = kept.Footer != nullptr ? kept.Footer->text() : QString{ },
								   .AskedAt    = kept.AskedAt,
								   .AnsweredAt = kept.AnsweredAt,
								   .Outcome    = kept.Outcome,
								   .Steered    = kept.Steered });
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
		if (ClearDispatch) {
			ClearDispatch();
		}
	}

	void agent_tab_view::present_native_history(const application::native_agent_history_projection& aView)
	{
		if (aView.ReadOnly) {
			auto* dialog = new QDialog{ this };
			dialog->setAttribute(Qt::WA_DeleteOnClose);
			dialog->setWindowTitle(tr("Legacy history (agent identity unknown)"));
			auto* column = new QVBoxLayout{ dialog };
			auto* text   = new QPlainTextEdit{ dialog };
			text->setReadOnly(true);
			auto body = tr("Legacy saved questions and replies. Native tool observations and workspace state were not retained. Viewing sends no question. Choose an explicit replay destination in History to continue.\n\n");
			for (const auto& turn : aView.Turns) {
				body += QString::fromStdString(turn.Asked) + QStringLiteral("\n\n") + QString::fromStdString(turn.Answered) + QStringLiteral("\n\n");
			}
			text->setPlainText(body);
			column->addWidget(text);
			dialog->resize(600, 480);
			dialog->show();
			return;
		}
		NativeHistoryView = aView;
		ClearSession->setEnabled(!aView.Loading && State != agent_tab_state::working);
		if (aView.Loading) {
			return;
		}
		NativeHistoryMenu->clear();
		ContinuationMode->setText(aView.Replay ? tr("Transcript replay: saved questions and replies; prior tool memory is absent; PR files and Git are shared.") : aView.NativeSupported ? tr("Native continuation")
																																														 : tr("Native continuation unavailable. Choose Transcript replay or New session in History."));
		ContinuationMode->setToolTip(tr("Replay uses at most 50 retained turns, 8 KiB per field and 48 KiB total; older turns and cut text are omitted. The source history stays. New session starts empty and sends no question."));
		// [[BPR-2059]], owner 2026-11-10: the replay's counts ("Replay omits 0
		// retained turns and cuts 0 text bytes …") are a diagnostic, not words
		// for the reviewer. They stay on the label for diagnostics and tests,
		// and the tooltip says them only when something was left out.
		ContinuationMode->setProperty("replayTurnsOmitted", QVariant::fromValue<qulonglong>(aView.ReplayTurnsOmitted));
		ContinuationMode->setProperty("replayTextCutBytes", QVariant::fromValue<qulonglong>(aView.ReplayTextCutBytes));
		if (aView.Replay && (aView.ReplayTurnsOmitted > 0U || aView.ReplayTextCutBytes > 0U)) {
			ContinuationMode->setToolTip(ContinuationMode->toolTip() + tr(" This replay leaves out %1 older turns and %2 bytes of cut text.").arg(aView.ReplayTurnsOmitted).arg(aView.ReplayTextCutBytes));
		}
		auto* replay = NativeHistoryMenu->addAction(tr("Use Transcript replay with selected agent"));
		replay->setObjectName(QStringLiteral("agentTranscriptReplay"));
		replay->setToolTip(ContinuationMode->toolTip());
		if (!aView.ReplaySource.empty()) {
			ContinuationMode->setToolTip(ContinuationMode->toolTip() + tr(" Source history: %1").arg(QString::fromStdString(aView.ReplaySource)));
		}
		replay->setEnabled(State != agent_tab_state::working);
		connect(replay, &QAction::triggered, this, [this, id = aView.Selected] {
			if (NativeHistoryDispatch) {
				NativeHistoryDispatch("replay:" + id);
			} });
		if (!aView.Legacy.empty()) {
			auto* legacy = NativeHistoryMenu->addMenu(tr("Legacy histories (agent identity unknown)"));
			for (const auto& entry : aView.Legacy) {
				auto* inspect = legacy->addAction(tr("View %1").arg(QString::fromStdString(entry.Title)));
				connect(inspect, &QAction::triggered, this, [this, id = entry.Id] {if(NativeHistoryDispatch) {NativeHistoryDispatch("view:"+id);} });
				auto* action = legacy->addAction(tr("Replay %1 with selected agent (%2 turns)").arg(QString::fromStdString(entry.Title)).arg(entry.Turns));
				action->setData(QString::fromStdString(entry.Id));
				action->setEnabled(State != agent_tab_state::working);
				connect(action, &QAction::triggered, this, [this, id = entry.Id] {if(NativeHistoryDispatch) {NativeHistoryDispatch("replay:"+id);} });
			}
		}
		NativeHistoryMenu->addSeparator();
		for (const auto& entry : aView.Entries) {
			auto  title = QString::fromStdString(entry.Title).simplified().left(72);
			auto* item  = NativeHistoryMenu->addAction(tr("%1: %2 (%3 turns)").arg(entry.Replay ? tr("Transcript replay") : tr("Native"), title).arg(entry.Turns));
			item->setData(QString::fromStdString(entry.Id));
			item->setProperty("turns", QVariant::fromValue<qulonglong>(entry.Turns));
			item->setCheckable(true);
			item->setChecked(entry.Id == aView.Selected);
			item->setEnabled(State != agent_tab_state::working);
			connect(item, &QAction::triggered, this, [this, id = entry.Id] { if (NativeHistoryDispatch) { NativeHistoryDispatch(id); } });
		}
		NativeHistoryMenu->setEnabled(true);
	}

	void agent_tab_view::set_suggestion_context(std::function<agent_suggestion_context()> aContext)
	{
		SuggestionContext = std::move(aContext);
		refresh_suggestions();
	}

	auto agent_tab_view::suggestions() const -> QStringList { return Suggestions->prompts(); }

	auto agent_tab_view::suggestion_buttons() const -> std::vector<QPushButton*> { return Suggestions->buttons(); }

	void agent_tab_view::send_prompt(const QString& aPrompt) { choose_prompt(aPrompt, false); }

	void agent_tab_view::set_prompt_source(prompt_source aSource)
	{
		PromptSource = std::move(aSource);
		refresh_suggestions();
	}

	auto agent_tab_view::prompt_view() const -> core::prompt_chat_view
	{
		const auto context = current_suggestion_context();
		auto       view    = core::prompt_chat_view{ .Place = ConversationShown ? core::prompt_place::conversation : core::prompt_place::agent_tab, .ChatEmpty = Turns.empty() };
		if (ConversationShown) {
			view.Thread       = ConversationShown->Key;
			view.TakesReplies = ConversationShown->Replies;
		}
		if (!context.OpenFile.isEmpty()) {
			view.OpenFile = context.OpenFile.toStdString();
		}
		if (context.Lines) {
			view.Lines = core::prompt_line_range{ .First = context.Lines->first, .Last = context.Lines->second };
		}
		return view;
	}

	auto agent_tab_view::offered(const QString& aKeyOrLabel) const -> const core::prompt_offer*
	{
		const auto key   = aKeyOrLabel.toStdString();
		const auto found = std::ranges::find_if(Offered, [&key](const core::prompt_offer& aOffer) { return aOffer.Id == key; });
		if (found != Offered.end()) {
			return &*found;
		}
		const auto named = std::ranges::find(Offered, key, &core::prompt_offer::Label);
		return named != Offered.end() ? &*named : nullptr;
	}

	void agent_tab_view::choose_prompt(const QString& aPrompt, const bool aEditFirst)
	{
		if (!panels_alive()) {
			return;
		}
		// RFC-BPR-0116. A returning suggestion asks its own question.
		const auto returning = ReturningPrompts.find(aPrompt);
		const auto byLabel   = std::ranges::find_if(ReturningPrompts, [&aPrompt](const auto& aEntry) {
			return (aEntry.first == latest_upload_key() ? tr("What changed since my review") : tr("Was this addressed?")) == aPrompt;
		});
		if (const auto chosen = returning != ReturningPrompts.end() ? returning : byLabel; chosen != ReturningPrompts.end()) {
			if (!aEditFirst) {
				// [[BPR-2251]]. What changed since my review reads the change
				// at the press; the host asks once it has.
				if (chosen->second == core::returning_operation::latest_upload && LatestUploadDispatch) {
					LatestUploadDispatch();
				} else {
					ask_returning(chosen->second);
				}
			}
			return;
		}
		// RFC-BPR-0125. A press expands against the facts at the press: the
		// list is decided again, and a prompt that no longer applies does
		// nothing.
		refresh_suggestions();
		const auto* const found = offered(aPrompt);
		if (found == nullptr) {
			return;
		}
		press_offer(*found, aEditFirst);
	}

	auto agent_tab_view::send_decided_prompt(const core::prompt_offer& aOffer) -> release_outcome
	{
		if (!panels_alive()) {
			return release_outcome::refused;
		}
		Released = release_outcome::refused;
		press_offer(aOffer, false);
		return Released;
	}

	void agent_tab_view::press_offer(const core::prompt_offer& aChosen, const bool aEditFirst)
	{
		const auto offer    = aChosen;
		const auto text     = QString::fromStdString(offer.Expansion.Text);
		auto*      composer = Chat.composer();
		const auto draft    = composer->toPlainText();
		const auto press    = core::decide_agent_suggestion_press({
			.EditFirst = aEditFirst || offer.finish(),
			.DraftHeld = !draft.trimmed().isEmpty(),
			.Usable    = Chat.send_action()->isEnabled() && State != agent_tab_state::working,
		});
		auto       carried  = macro_send{
			.Key       = QString::fromStdString(offer.Id),
			.Name      = QString::fromStdString(offer.Label),
			.Subject   = core::suggested_subject_of(offer.Subject),
			.Review    = offer.Kind == core::prompt_ask::review,
			.Expansion = text,
		};
		const auto toEnd = [composer] {
			auto cursor = composer->textCursor();
			cursor.movePosition(QTextCursor::End);
			composer->setTextCursor(cursor);
		};
		switch (press) {
		case core::agent_suggestion_press::ignored:            return;
		case core::agent_suggestion_press::send:
		case core::agent_suggestion_press::send_keeping_draft: {
			// AC09. Sent as offered: skills only from the template's own
			// text, and never read as a command.
			const auto kept   = press == core::agent_suggestion_press::send_keeping_draft;
			carried.SkillText = core::prompt_skill_text(offer.Expansion);
			composer->setPlainText(text);
			// Sent alone, as if typed and sent; what the reviewer had typed
			// is put back, and its turn's start leaves it there.
			KeepingDraft = kept;
			SendingMacro = std::move(carried);
			ask();
			SendingMacro.reset();
			KeepingDraft = false;
			if (kept) {
				composer->setPlainText(draft);
				toEnd();
			}
			return;
		}
		case core::agent_suggestion_press::edit: {
			composer->setPlainText(text);
			auto cursor = composer->textCursor();
			cursor.setPosition(caret_of(offer.Expansion));
			composer->setTextCursor(cursor);
			break;
		}
		case core::agent_suggestion_press::edit_after_draft: {
			// Added, never replacing: on its own line after the words there,
			// and undone in one step.
			toEnd();
			const auto lead = draft.endsWith(QLatin1Char('\n')) ? QString{ } : QStringLiteral("\n");
			const auto from = composer->textCursor().position() + static_cast<int>(lead.size());
			composer->textCursor().insertText(lead + text);
			auto cursor = composer->textCursor();
			cursor.setPosition(from + caret_of(offer.Expansion));
			composer->setTextCursor(cursor);
			break;
		}
		}
		// Q2. A drafted prompt keeps its subject and kind, shown as a chip.
		DraftedMacro = std::move(carried);
		present_macro_chip();
		composer->setFocus(Qt::OtherFocusReason);
	}

	void agent_tab_view::present_macro_chip()
	{
		if (MacroChip == nullptr) {
			return;
		}
		if (DraftedMacro) {
			MacroText->setText(DraftedMacro->Name);
			const auto as = DraftedMacro->Review                                         ? tr("Sent as a review")
				: DraftedMacro->Subject == core::agent_suggested_subject::open_file      ? tr("Sent as a question about the open file")
				: DraftedMacro->Subject == core::agent_suggested_subject::selected_lines ? tr("Sent as a question about the selected lines")
				: ConversationShown                                                      ? tr("Sent as a question about the conversation")
																						 : tr("Sent as a question about the pull request");
			MacroChip->setToolTip(tr("%1, as the suggested prompt %2 is. Remove this to send what you wrote as a typed question.").arg(as, DraftedMacro->Name));
			MacroChip->setAccessibleName(tr("Suggested prompt %1").arg(DraftedMacro->Name));
		}
		MacroChip->setVisible(DraftedMacro.has_value());
	}

	auto agent_tab_view::macro_chip_text() const -> QString { return MacroChip == nullptr || MacroChip->isHidden() ? QString{ } : MacroText->text(); }

	void agent_tab_view::refresh_suggestions()
	{
		if (!panels_alive() || Suggestions == nullptr) {
			return;
		}
		const auto context = current_suggestion_context();
		present_lines_chip(context);
		// RFC-BPR-0125; [[BPR-2181]] (AC13). The offers are decided outside
		// the tab, from what it shows and what the window holds; this
		// presents them. Owner decision (a): the pull request, the open file
		// when one is open, what the change could break, and the selected
		// lines when lines are selected.
		const auto offers = PromptSource ? PromptSource(prompt_view()) : core::chat_prompts{ };
		Offered           = offers.Pressable;
		auto chips        = std::vector<agent_suggestion>{ };
		auto more         = std::vector<agent_suggestion>{ };
		ReturningPrompts.clear();
		// RFC-BPR-0116 ([[BPR-2073]], [[BPR-2074]]). First when commits
		// followed my review, or on a thread I commented in once they did.
		// They are the host's questions, not suggested prompts.
		if (ConversationShown ? ConversationShown->Addressed : LatestUpload.State == core::latest_upload_state::offered) {
			const auto key   = ConversationShown ? proposed_fix_key() : latest_upload_key();
			const auto label = ConversationShown ? tr("Was this addressed?") : tr("What changed since my review");
			chips.push_back({ .Key = key, .Label = label, .Expansion = label });
			ReturningPrompts[key] = ConversationShown ? core::returning_operation::proposed_fix : core::returning_operation::latest_upload;
		}
		for (const auto& offer : offers.Chips) {
			chips.push_back(suggestion_of(offer));
		}
		for (const auto& offer : offers.More) {
			more.push_back(suggestion_of(offer));
		}
		const auto usable = Chat.send_action()->isEnabled() && State != agent_tab_state::working;
		Suggestions->present(std::move(chips), std::move(more));
		Suggestions->set_usable(usable);
		const auto conversing = State != agent_tab_state::not_set_up && State != agent_tab_state::setting_up;
		Suggestions->lead()->setVisible(Turns.empty());
		// [[BPR-1755]], the owner's accept of look v007 ("Hide
		// suggestions until the the agent is done thinking"): none while a
		// turn runs; they return when it has finished. [[BPR-1847]]: none
		// before the setup has finished either; the core decides. A passed
		// check's Ready line does not hold them back ([[BPR-1877]]).
		const auto verdict = suggestions_verdict();
		Suggestions->setVisible(verdict == core::agent_suggestions_verdict::shown);
		SuggestionsMenu->setVisible(conversing && verdict != core::agent_suggestions_verdict::setup_unfinished);
		// The setup's way in, unless the setup is on screen. Only once the
		// setup knows what is missing: not while agents are still being
		// looked for. [[BPR-1877]]: an unfinished setup always says why the
		// prompts are not there, whatever else is on screen.
		const auto known = SetupView == nullptr || SetupView->projection().Detected;
		SetupPrompt->setVisible(conversing && known && verdict == core::agent_suggestions_verdict::setup_unfinished);
		// [[BPR-1881]]. The Tour tab's agent features follow the same verdict.
		if (const auto readiness = setup_readiness(); ReadinessObserver && ToldReadiness != readiness) {
			ToldReadiness = readiness;
			ReadinessObserver();
		}
		if (AvailabilityObserver) {
			AvailabilityObserver(false);
		}
	}

	void agent_tab_view::draft_prompt(const QString& aPrompt) { choose_prompt(aPrompt, true); }

	auto agent_tab_view::ready_to_send() const -> bool { return State != agent_tab_state::working && Chat.send_action() != nullptr && Chat.send_action()->isEnabled(); }

	void agent_tab_view::return_unqueued(const QString& aText, const QString& aWhy)
	{
		if (!panels_alive()) {
			return;
		}
		const auto typed = Chat.composer()->toPlainText();
		draft_text(typed.trimmed().isEmpty() ? aText : aText + QStringLiteral("\n\n") + typed);
		Notice->setText(aWhy);
		Notice->show();
	}

	void agent_tab_view::draft_text(const QString& aText)
	{
		if (!panels_alive()) {
			return;
		}
		auto* const composer = Chat.composer();
		composer->setPlainText(aText);
		auto cursor = composer->textCursor();
		cursor.movePosition(QTextCursor::End);
		composer->setTextCursor(cursor);
		composer->setFocus(Qt::OtherFocusReason);
	}

	void agent_tab_view::set_follow_up_steer(const bool aSteer)
	{
		FollowUpSteer = aSteer;
		refresh_send_label();
	}

	/// While a turn runs or messages wait, Send says what Return will do.
	void agent_tab_view::refresh_send_label()
	{
		auto* const send = Chat.send_action();
		if (send == nullptr) {
			return;
		}
		const auto following = FollowUp && core::decide_follow_up({ .TurnShown = State == agent_tab_state::working, .QueueHolds = QueueOccupied }) != core::follow_up::send;
		const auto other     = QStringLiteral("%1Return").arg(QKeySequence{ Qt::ControlModifier }.toString(QKeySequence::NativeText));
		const auto words     = !following ? tr("Send")
			: FollowUpSteer               ? tr("Steer (Return) · Queue (%1)").arg(other)
										  : tr("Queue (Return) · Steer (%1)").arg(other);
		send->setToolTip(words);
		send->setText(!following ? tr("Send") : FollowUpSteer ? tr("Steer")
															  : tr("Queue"));
	}

	auto agent_tab_view::send_released(const QString& aText) -> release_outcome
	{
		if (!panels_alive()) {
			return release_outcome::refused;
		}
		auto* const composer = Chat.composer();
		const auto  draft    = composer->toPlainText();
		composer->setPlainText(aText);
		KeepingDraft = !draft.trimmed().isEmpty();
		Releasing    = true;
		Released     = release_outcome::refused;
		ask();
		Releasing = false;
		if (KeepingDraft) {
			composer->setPlainText(draft);
		}
		KeepingDraft = false;
		return Released;
	}

	void agent_tab_view::present_queue_rows(std::vector<queue_row> aRows, const QString& aState, const bool aPaused)
	{
		// AC10: a released message whose turn does not show yet holds the
		// session too.
		QueueOccupied = !aRows.empty();
		refresh_send_label();
		if (QueueList == nullptr) {
			return;
		}
		const auto any = !aRows.empty();
		QueueList->present(std::move(aRows), aState, aPaused);
		if (any && QueueNote != nullptr) {
			// The panel says what waits; the line above it would repeat it.
			QueueNote->hide();
		}
	}

	void agent_tab_view::set_queue_dispatch(queue_dispatch aDispatch)
	{
		if (QueueList != nullptr) {
			QueueList->set_dispatch(std::move(aDispatch));
		}
	}

	void agent_tab_view::drop_queued(const std::uint64_t aId, const int aAt)
	{
		if (QueueList != nullptr) {
			QueueList->land(aId, aAt);
		}
	}

	void agent_tab_view::present_queue(const QStringList& aWaiting, const QString& aState)
	{
		if (QueueNote == nullptr) {
			return;
		}
		QueueNote->setText(aWaiting.isEmpty() ? QString{ } : tr("Queued (%1): %2").arg(aState, aWaiting.join(QStringLiteral(", "))));
		QueueNote->setAccessibleName(QueueNote->text());
		QueueNote->setVisible(!aWaiting.isEmpty() && (QueueList == nullptr || QueueList->isHidden()));
	}

	auto agent_tab_view::suggestions_verdict() const -> core::agent_suggestions_verdict
	{
		// No setup adopted: nothing to wait on, as before it was composed.
		const auto* const setup = SetupView != nullptr ? &SetupView->projection() : nullptr;
		return core::decide_agent_suggestions({
			.AgentOffered   = State != agent_tab_state::not_set_up,
			.SetupShowing   = State == agent_tab_state::setting_up,
			.SetupStep      = setup != nullptr ? setup->Step : core::setup_step::ready,
			.AgentCheck     = setup != nullptr ? setup->State.AgentCheck.State : core::setup_check_state::passed,
			.WorkspaceCheck = setup != nullptr ? setup->State.WorkspaceCheck.State : core::setup_check_state::passed,
			.Working        = State == agent_tab_state::working,
			.Toggled        = SuggestionsMenu->isChecked(),
		});
	}

	void agent_tab_view::present_latest_upload(const core::latest_upload_offer& aOffer)
	{
		if (aOffer == LatestUpload) {
			return;
		}
		LatestUpload = aOffer;
		refresh_suggestions();
	}

	void agent_tab_view::ask_returning(const core::returning_operation aOperation, const std::size_t aThreads)
	{
		if (!panels_alive() || State == agent_tab_state::working || !Chat.send_action()->isEnabled()) {
			return;
		}
		auto* const composer = Chat.composer();
		const auto  draft    = composer->toPlainText();
		composer->setPlainText(aOperation == core::returning_operation::latest_upload           ? tr("What changed since my review")
								   : aOperation == core::returning_operation::addressed_threads ? tr("Was this addressed? (%1 threads)").arg(aThreads)
																								: tr("Was this addressed?"));
		// Sent alone, as a suggestion is; what the reviewer had typed stays.
		KeepingDraft     = !draft.trimmed().isEmpty();
		SendingReturning = aOperation;
		ask();
		SendingReturning.reset();
		if (KeepingDraft) {
			composer->setPlainText(draft);
		}
		KeepingDraft = false;
	}

	auto agent_tab_view::ask_explain_file(const QString& aPath) -> QString
	{
		if (!panels_alive() || aPath.isEmpty()) {
			return tr("Not sent.");
		}
		if (State == agent_tab_state::working) {
			return tr("Not sent. The agent is answering another question.");
		}
		auto* const composer = Chat.composer();
		const auto  draft    = composer->toPlainText();
		composer->setPlainText(tr("Explain %1").arg(aPath));
		// Sent alone, as a suggestion is; what the reviewer had typed stays.
		KeepingDraft   = !draft.trimmed().isEmpty();
		SendingExplain = aPath;
		ask();
		SendingExplain.clear();
		if (KeepingDraft) {
			composer->setPlainText(draft);
		} else if (!send_refusal_text().isEmpty()) {
			composer->clear();
		}
		KeepingDraft = false;
		return send_refusal_text();
	}

	auto agent_tab_view::turn_explain(const std::size_t aTurn) const -> QString { return aTurn < Turns.size() ? Turns[aTurn].Explain : QString{ }; }

	auto agent_tab_view::ask_suggest_change(const QString& aPath, const std::uint32_t aFirst, const std::uint32_t aLast) -> QString
	{
		if (!panels_alive() || aPath.isEmpty()) {
			return tr("Not sent.");
		}
		if (State == agent_tab_state::working) {
			return tr("Not sent. The agent is answering another question.");
		}
		auto* const composer = Chat.composer();
		const auto  draft    = composer->toPlainText();
		composer->setPlainText(aFirst == aLast ? tr("Suggest the change on line %1 of %2").arg(aLast + 1U).arg(aPath)
											   : tr("Suggest the change on lines %1–%2 of %3").arg(aFirst + 1U).arg(aLast + 1U).arg(aPath));
		// Sent alone, as a suggestion is; what the reviewer had typed stays.
		KeepingDraft   = !draft.trimmed().isEmpty();
		SendingSuggest = aPath;
		ask();
		SendingSuggest.clear();
		if (KeepingDraft) {
			composer->setPlainText(draft);
		} else if (!send_refusal_text().isEmpty()) {
			composer->clear();
		}
		KeepingDraft = false;
		return send_refusal_text();
	}

	auto agent_tab_view::turn_suggest_change(const std::size_t aTurn) const -> QString { return aTurn < Turns.size() ? Turns[aTurn].SuggestChange : QString{ }; }

	void agent_tab_view::reveal_turn(const std::size_t aTurn)
	{
		if (aTurn >= Turns.size() || Turns[aTurn].Widget == nullptr) {
			return;
		}
		if (auto* const scroll = qobject_cast<QScrollArea*>(Viewport != nullptr ? Viewport->parentWidget() : nullptr); scroll != nullptr) {
			scroll->ensureWidgetVisible(Turns[aTurn].Widget, 0, 12);
		}
	}

	void agent_tab_view::publish_explain(const std::size_t aTurn, std::optional<agent_run_status> aRunning, const QString& aEnding, const bool aAnswered, const qint64 aSeconds)
	{
		if (!ExplainObserver || aTurn >= Turns.size()) {
			return;
		}
		const auto& kept     = Turns[aTurn];
		auto        progress = explain_file_progress{ .Path = kept.Explain, .Turn = aTurn, .Running = std::move(aRunning), .Started = Started, .Seconds = aSeconds, .AnswerText = kept.AnswerText, .Ending = aEnding, .Answered = aAnswered };
		if (Launch.activity_reported()) {
			progress.Activity = Launch.activity();
			if (progress.Running) {
				progress.Running->Activity          = Launch.activity();
				progress.Running->ActivityStartedMs = Launch.activity_started_ms();
			}
		}
		progress.Answer = LineNavigate && LineResolve ? agent_markdown_with_reference_links(kept.AnswerText, LineResolve(), kept.ContextFile) : kept.AnswerText;
		ExplainObserver(progress);
	}

	void agent_tab_view::open_report(const std::size_t aTurn)
	{
		if (aTurn >= Turns.size() || !ReportDispatch) {
			return;
		}
		const auto& turn = Turns[aTurn];
		// References are links in the report as they are in the answer.
		const auto shown = LineNavigate && LineResolve ? agent_markdown_with_reference_links(turn.AnswerText, LineResolve(), turn.ContextFile) : turn.AnswerText;
		// [[BPR-2219]]. The comments the agent suggested for this turn's
		// findings, by place: the report's findings draft from them.
		// [[BPR-2231]]: every card of the turn, so a report finding at a
		// card's place is that card and shares its one state.
		auto       suggested = std::vector<core::report_suggested_comment>{ };
		auto       cards     = std::vector<agent_report_card>{ };
		const auto block     = core::digest_agent_findings(turn.FindingsBlock);
		for (const auto& entry : CardEntries) {
			if (entry.Turn == std::optional{ aTurn } && entry.Ordinal <= std::numeric_limits<std::uint32_t>::max()) {
				suggested.push_back({ .Path    = entry.Finding.Path,
									  .First   = entry.Finding.Lines.Begin + 1U,
									  .Last    = std::max(entry.Finding.Lines.End, entry.Finding.Lines.Begin + 1U),
									  .Comment = entry.Finding.SuggestedComment ? *entry.Finding.SuggestedComment : std::string{ } });
				cards.push_back({ .Session = entry.Session, .Ordinal = entry.Ordinal, .Finding = entry.Finding, .Key = { .Findings = block, .Ordinal = static_cast<std::uint32_t>(entry.Ordinal) } });
			}
		}
		// [[BPR-2184]] (review N5): keyed by the turn's stable identity, its
		// byline handed over as data.
		ReportDispatch(agent_report_document{
			.Turn     = turn.Serial,
			.Title    = turn.ReportTitle != nullptr ? turn.ReportTitle->text() : QString{ },
			.Markdown = shown,
			// [[BPR-2219]]. A turn sent under a name (a review prompt, whose
			// question is its whole body) is said by that name.
			.Question  = turn.Name.isEmpty() ? turn.Question : turn.Name,
			.Byline    = { .Ending = turn.Ending, .When = relative_time(turn.AnsweredAt.isValid() ? turn.AnsweredAt : turn.AskedAt), .Head = Head != nullptr ? Head->text() : QString{ } },
			.Suggested = std::move(suggested),
			.Cards     = std::move(cards),
			.Answer    = turn.AnswerText.trimmed(),
		});
	}

	void agent_tab_view::mark_open_report(const std::optional<std::uint64_t> aTurn)
	{
		for (const auto& each : Turns) {
			if (auto* const row = static_cast<report_attachment_row*>(each.ReportOpen.data()); row != nullptr) {
				row->set_selected(aTurn == each.Serial);
			}
		}
	}

	auto agent_tab_view::turn_of(const std::uint64_t aTurn) const -> std::optional<std::size_t>
	{
		const auto found = std::ranges::find(Turns, aTurn, &turn::Serial);
		return found == Turns.end() ? std::nullopt : std::optional{ static_cast<std::size_t>(std::distance(Turns.begin(), found)) };
	}

	auto agent_tab_view::turn_report_card(const std::size_t aTurn) const -> QWidget* { return aTurn < Turns.size() ? Turns[aTurn].ReportCard.data() : nullptr; }

	auto agent_tab_view::turn_claim(const std::size_t aTurn) const -> QLabel* { return aTurn < Turns.size() ? Turns[aTurn].Claim.data() : nullptr; }

	auto agent_tab_view::turn_verdict(const std::size_t aTurn) const -> QLabel* { return aTurn < Turns.size() ? Turns[aTurn].Verdict.data() : nullptr; }

	auto agent_tab_view::turn_ask_more_button(const std::size_t aTurn) const -> QPushButton* { return aTurn < Turns.size() ? Turns[aTurn].AskMore.data() : nullptr; }

	auto agent_tab_view::turn_returning(const std::size_t aTurn) const -> std::optional<core::returning_operation> { return aTurn < Turns.size() ? Turns[aTurn].Returning : std::nullopt; }

	void agent_tab_view::set_suggestions_shown(const bool aShown)
	{
		// The host's choice is not a new one: it is not said back.
		const auto quiet = QSignalBlocker{ SuggestionsMenu };
		SuggestionsMenu->setChecked(aShown);
		SuggestionsMenu->setToolTip(aShown ? tr("Hide the suggested prompts") : tr("Show the suggested prompts"));
		refresh_suggestions();
	}

	void agent_tab_view::set_suggestions_observer(std::function<void(bool)> aObserver) { SuggestionsObserver = std::move(aObserver); }

	auto agent_tab_view::suggestions_shown() const -> bool { return SuggestionsMenu->isChecked(); }

	void agent_tab_view::update_turn()
	{
		if (Turns.empty() || Turns.back().Done || Turns.back().Answer == nullptr || Turns.back().Footer == nullptr) {
			return;
		}
		auto& current = Turns.back();
		// [[BPR-2153]]. The running turn's steered messages, as its run's
		// turn events said them, and how each ended.
		if (current.Steered != Launch.steered()) {
			current.Steered = Launch.steered();
			show_steered(current);
		}
		const auto agent   = Launch.profile_selector()->currentText();
		const auto seconds = Started.isValid() ? Started.elapsed() / 1000 : 0;
		const auto written = Launch.output_text().trimmed();
		switch (Launch.current_phase()) {
		case agent_launch_panel::phase::preparing: {
			set_answer(current, written);
			auto status  = agent_run_status{ };
			status.Agent = agent;
			status.Stage = Launch.workspace_checking() ? agent_run_stage::checking : agent_run_stage::preparing;
			current.Footer->present(status, Started);
			if (RunObserver) {
				RunObserver(status, Started);
			}
			if (!current.Explain.isEmpty()) {
				publish_explain(Turns.size() - 1U, status, { }, false, seconds);
			}
			return;
		}
		case agent_launch_panel::phase::running: {
			set_answer(current, written);
			// [[BPR-1941]]. The words the tour chats' running turns say too.
			auto status      = agent_run_status{ };
			status.Agent     = agent;
			status.Stage     = written.isEmpty() ? agent_run_stage::thinking : agent_run_stage::writing;
			status.Latest    = written.isEmpty() ? Launch.latest_output_line() : QString{ };
			status.FilesOnly = Launch.pull_request_files_only();
			// [[BPR-2252]]. What the turn has cost so far, once the agent said.
			status.Cost = Launch.running_turn_cost();
			// [[BPR-2005]]. What the runtime says it is doing, when it says.
			if (Launch.activity_reported()) {
				status.Activity          = Launch.activity();
				status.ActivityStartedMs = Launch.activity_started_ms();
				if (current.Activity != nullptr) {
					current.Activity->present(agent_short_name(agent), Launch.activity(), Launch.activity_started_ms());
				}
			}
			current.Footer->present(status, Started);
			if (RunObserver) {
				RunObserver(status, Started);
			}
			if (!current.Explain.isEmpty()) {
				publish_explain(Turns.size() - 1U, status, { }, false, seconds);
			}
			return;
		}
		case agent_launch_panel::phase::idle:
			break;
		}
		// The request ended: say what came of it.
		current.Done          = true;
		const auto findings   = Findings.finding_count();
		const auto succeeded  = Launch.last_outcome() == application::agent_run_outcome::success;
		current.Outcome       = application::turn_outcome(Launch.last_outcome().value_or(application::agent_run_outcome::failed_to_start));
		const auto stopReason = Launch.stop_reason_text();
		// [[BPR-2005]]. The block folds under the answer.
		if (current.Activity != nullptr) {
			current.Activity->fold(Launch.activity(), seconds);
		}
		const auto kept       = Launch.kept_text().trimmed();
		const auto unfinished = Launch.last_failure() == application::agent_protocol_failure::helpers_unfinished;
		if (succeeded) {
			set_answer(current, written.isEmpty() ? tr("%1 finished without writing an answer.").arg(agent) : written);
		} else if (unfinished || (Launch.last_outcome() == application::agent_run_outcome::cancelled && !kept.isEmpty())) {
			// [[BPR-2004]], RFC-BPR-0103. Stopped, or the runtime ended under
			// its helpers: what arrived is kept, and the ending is said
			// under it, never as an answer that promises more.
			auto names = QStringList{ };
			for (const auto& name : Launch.unfinished_helpers()) {
				names << (name.empty() ? tr("a helper") : QStringLiteral("“%1”").arg(QString::fromStdString(name)));
			}
			const auto ending = unfinished ? (names.size() == 1 ? tr("%1 stopped before its remaining helper finished").arg(agent_short_name(agent)) : tr("%1 stopped before its %2 remaining helpers finished").arg(agent_short_name(agent)).arg(names.size())) + (names.isEmpty() ? QString{ } : QStringLiteral(": ") + names.join(QStringLiteral(", "))) + QStringLiteral(". ") + tr("What arrived is kept.")
										   : tr("Stopped. What arrived is kept.");
			set_answer(current, (kept.isEmpty() ? QString{ } : kept + QStringLiteral("\n\n")) + QStringLiteral("*") + ending + QStringLiteral("*"));
		} else {
			// [[BPR-1817]]. A run that ended without an answer is an error
			// entry in the thread: what happened in words, then the tail of
			// what the agent wrote to its error stream.
			auto answer = stopReason.isEmpty() ? run_failure_words(Launch.last_outcome().value_or(application::agent_run_outcome::failed_to_start), agent) : stopReason;
			if (auto said = Launch.output_text().trimmed(); !said.isEmpty() && Launch.last_outcome() != application::agent_run_outcome::cancelled) {
				// The last lines, which say why; the log keeps all of it.
				auto lines = said.split(QLatin1Char{ '\n' });
				if (lines.size() > 12) {
					lines = lines.mid(lines.size() - 12);
				}
				said = lines.join(QLatin1Char{ '\n' }).right(1200);
				said.replace(QStringLiteral("~~~~"), QStringLiteral("~ ~ ~ ~"));
				answer += QStringLiteral("\n\n") + tr("It wrote:") + QStringLiteral("\n\n~~~~text\n") + said + QStringLiteral("\n~~~~");
			}
			set_answer(current, answer);
		}
		auto ran = agent;
		if (const auto reported = Launch.last_reported_model(); succeeded && !reported.isEmpty()) {
			ran = tr("%1 (%2)").arg(agent, agent_model_display_name(reported));
			if (const auto profile = Launch.selected_profile()) {
				RunSettingsSource->report_model(profile->value(), reported);
			}
		}
		// [[BPR-1999]], RFC-BPR-0101. The turn's cost after its duration, in
		// dollars and cents: "Claude answered in 1:12 · $0.41". None when the
		// agent reported none. [[BPR-2038]], owner look on AC08: "one line
		// for both turn and session status lines at this default width".
		// The line names the agent short and counts the findings; the model
		// and what the agent could see are in its tooltip.
		const auto cost      = Launch.last_turn_cost();
		const auto spent     = cost ? QString::fromStdString(core::agent_cost_words(*cost)) : QString{ };
		const auto delivered = succeeded && findings > 0U && FindingsPresented != FindingsAtTurn;
		auto       parts     = QStringList{ agent_turn_ending_words(agent, succeeded, seconds) };
		auto       tip       = QStringList{ succeeded ? agent_run_answered_words(ran, seconds) : parts.front() };
		if (cost) {
			parts << spent;
			tip << spent;
		}
		if (delivered) {
			parts << (findings == 1U ? tr("1 finding") : tr("%1 findings").arg(findings));
		}
		auto footer = parts.join(QStringLiteral(" · "));
		if (parts.size() == 1) {
			footer += QLatin1Char{ '.' };
		}
		auto detail = tip.join(QStringLiteral(" · ")) + QLatin1Char{ '.' };
		if (succeeded && Launch.pull_request_files_only()) {
			detail += QLatin1Char{ ' ' } + tr("It saw only this pull request's files.");
		}
		if (delivered) {
			detail += QLatin1Char{ ' ' } + (findings == 1U ? tr("1 finding, above.") : tr("%1 findings, above.").arg(findings));
		}
		// The session's total: the conversation's own, when kept with it,
		// else this thread's turns added up.
		if (const auto total = Launch.last_session_cost()) {
			present_session_cost(total);
		} else if (cost) {
			present_session_cost(SessionTotal.value_or(0U) + *cost);
		}
		current.Footer->settle(footer, succeeded);
		current.Footer->setToolTip(detail);
		current.Ending = parts.front();
		// [[BPR-2051]]. The window's footer says how it ended, for a moment,
		// after where it started: "answered in 1:12 · $0.41 · 2 findings".
		// RFC-BPR-0126: a settled turn may release the next queued input.
		if (AvailabilityObserver) {
			QMetaObject::invokeMethod(this, [this] { if (AvailabilityObserver) { AvailabilityObserver(true); } }, Qt::QueuedConnection);
		}
		if (EndedObserver) {
			auto ending    = parts;
			ending.front() = succeeded ? tr("answered in %1").arg(agent_elapsed_words(seconds)) : tr("stopped after %1").arg(agent_elapsed_words(seconds));
			EndedObserver(ending.join(QStringLiteral(" · ")));
		}
		EndedTurns.push_back({ .Question = current.Question, .Ending = succeeded ? QString{ } : current.AnswerText, .Footer = footer, .Activity = Launch.activity(), .Seconds = seconds });
		if (EndedTurns.size() > 32U) {
			EndedTurns.erase(EndedTurns.begin());
		}
		current.AnsweredAt = QDateTime::currentDateTime();
		settle_reply_footer(current);
		if (succeeded && FindingsPresented != FindingsAtTurn) {
			claim_findings(Turns.size() - 1U);
		}
		ClearSession->setVisible(true);
		if (!current.Explain.isEmpty()) {
			publish_explain(Turns.size() - 1U, std::nullopt, footer, succeeded, seconds);
		}
		// RFC-BPR-0118. The window offers the answer to the core.
		if (!current.SuggestChange.isEmpty() && SuggestObserver) {
			SuggestObserver(suggest_change_turn{ .Turn = Turns.size() - 1U, .Ended = true, .Answered = succeeded, .AnswerText = current.AnswerText, .Ending = footer });
		}
		if (ThreadObserver) {
			ThreadObserver(thread_turns());
		}
	}

	void agent_tab_view::settle_reply_footer(turn& aTurn)
	{
		// Stopped reads as the agent drawers' Interrupted: in italics.
		if (aTurn.Footer != nullptr) {
			aTurn.Footer->settle(aTurn.Footer->text(), aTurn.Outcome == core::agent_turn_outcome::answered);
		}
		if (aTurn.AnsweredTime != nullptr) {
			show_time(*aTurn.AnsweredTime, aTurn.AnsweredAt);
		}
		present_turn_actions();
	}

	void agent_tab_view::present_turn_actions()
	{
		const auto working = State == agent_tab_state::working;
		// [[BPR-1876]]. Where the setup's two checks stand now; none without
		// a setup, so no cause is ever taken as cleared.
		const auto* const setup      = SetupView != nullptr ? &SetupView->projection() : nullptr;
		const auto        checkState = [setup](const core::agent_turn_cause aCause) -> std::optional<core::setup_check_state> {
			if (setup == nullptr) {
				return std::nullopt;
			}
			switch (aCause) {
			case core::agent_turn_cause::none:            return std::nullopt;
			case core::agent_turn_cause::agent_check:     return setup->State.AgentCheck.State;
			case core::agent_turn_cause::workspace_check: return setup->State.WorkspaceCheck.State;
			}
			return std::nullopt;
		};
		for (auto index = std::size_t{ 0U }; index < Turns.size(); ++index) {
			auto& kept = Turns[index];
			if (!kept.Done) {
				continue;
			}
			// A pass counts as made since the turn ended only once its check
			// was seen not passed after it: a pass from before the failure
			// says nothing about it.
			const auto cause = core::agent_turn_cause_of(kept.Outcome);
			const auto check = checkState(cause);
			if (check && *check != core::setup_check_state::passed) {
				kept.CauseRechecked = true;
			}
			const auto latest = index + 1U == Turns.size();
			// [[BPR-1847]], owner 2026-11-03: "Agent wizard errors shouldn't
			// get the save as note and draft comment treatment." The core
			// decides from how the turn ended; this presents it.
			const auto actions = core::decide_agent_turn_actions({
				.Outcome          = kept.Outcome,
				.HasText          = !kept.AnswerText.isEmpty(),
				.Latest           = latest,
				.Working          = working,
				.CausePassedSince = kept.CauseRechecked && check == std::optional{ core::setup_check_state::passed },
			});
			if (auto* const card = static_cast<rounded*>(kept.Reply.data()); card != nullptr) {
				// A fixed error is no longer drawn as one that holds now.
				const auto look        = actions.Error && !actions.FixedSince ? rounded::look::danger : rounded::look::card;
				const auto description = !actions.Error ? QString{ } : actions.FixedSince ? tr("Error, fixed since")
																						  : tr("Error");
				if (card->Look != look || card->accessibleDescription() != description) {
					card->Look = look;
					card->setAccessibleDescription(description);
					card->update();
				}
			}
			if (kept.FixedNote != nullptr) {
				if (actions.FixedSince) {
					const auto [glyph, colour] = setup_check_mark(core::setup_check_state::passed, palette());
					const auto what            = cause == core::agent_turn_cause::workspace_check ? tr("Fixed since: the workspace check passed.") : tr("Fixed since: %1 passed its check.").arg(agent_name().toHtmlEscaped());
					const auto next            = actions.Recovery == core::agent_turn_recovery::resend ? QStringLiteral(" ") + tr("Resend to try it now.") : QString{ };
					kept.FixedNote->setText(QStringLiteral("<span style=\"color:%1\">%2</span>&nbsp; <b>%3</b>%4").arg(colour.name(), glyph, what, next.toHtmlEscaped()));
				}
				kept.FixedNote->setVisible(actions.FixedSince);
			}
			if (kept.Copy != nullptr) {
				kept.Copy->setVisible(actions.Copy);
				kept.Copy->setToolTip(actions.Error ? tr("Copy the error and what the agent wrote") : tr("Copy the response as Markdown"));
			}
			// RFC-BPR-0127. One keep action: Keep local where the host found
			// no thread or line to take a draft (or none was decided).
			kept.KeepOffered = actions.Keep;
			present_keep(kept);
			if (kept.AskMore != nullptr) {
				kept.AskMore->setVisible((kept.Returning.has_value() || !kept.Explain.isEmpty() || !kept.SuggestChange.isEmpty()) && actions.Keep);
			}
			kept.RecoveryKind = actions.Recovery;
			if (kept.Recovery != nullptr) {
				switch (actions.Recovery) {
				case core::agent_turn_recovery::none: break;
				case core::agent_turn_recovery::resend:
					kept.Recovery->setText(tr("Resend"));
					kept.Recovery->setToolTip(tr("Send this message to the agent again"));
					// [[BPR-2000]]. Resend starts a run.
					wear_agent_accent(*kept.Recovery, palette());
					break;
				case core::agent_turn_recovery::agent_settings:
					kept.Recovery->setText(tr("Agent settings…"));
					kept.Recovery->setToolTip(tr("Check the agent's program and the form it answers in"));
					wear_ordinary_roles(*kept.Recovery);
					break;
				}
				kept.Recovery->setVisible(actions.Recovery != core::agent_turn_recovery::none);
			}
			if (kept.Delete != nullptr) {
				// [[BPR-1880]]. A message the agent received is forgotten once
				// deleted: no agent session is resumed, and the next message
				// carries the kept thread without it.
				kept.Delete->setToolTip(core::agent_turn_never_sent(kept.Outcome) ? tr("Remove this message from the conversation. It never reached the agent.")
																				  : tr("Remove this message from the conversation. The agent won't be sent it again, so it won't remember it."));
				kept.Delete->setVisible(actions.Delete);
			}
			if (latest && LatestFixedSince != actions.FixedSince) {
				LatestFixedSince = actions.FixedSince;
				present_banner();
			}
		}
		if (Turns.empty() && LatestFixedSince) {
			LatestFixedSince = false;
			present_banner();
		}
	}

	void agent_tab_view::recover(const turn& aTurn)
	{
		if (!panels_alive() || State == agent_tab_state::working) {
			return;
		}
		switch (aTurn.RecoveryKind) {
		case core::agent_turn_recovery::none: return;
		case core::agent_turn_recovery::resend:
			if (aTurn.Review) {
				Launch.start_action()->trigger();
			} else if (Chat.send_action()->isEnabled()) {
				// The question as it was asked, not a suggestion looked up by
				// its words: a typed question is offered by no list. What the
				// reviewer is typing is put back after it.
				auto* const composer = Chat.composer();
				const auto  draft    = composer->toPlainText();
				const auto  kept     = !draft.trimmed().isEmpty();
				composer->setPlainText(aTurn.Question);
				KeepingDraft = kept;
				ask();
				KeepingDraft = false;
				if (kept) {
					composer->setPlainText(draft);
					auto cursor = composer->textCursor();
					cursor.movePosition(QTextCursor::End);
					composer->setTextCursor(cursor);
				}
			}
			return;
		case core::agent_turn_recovery::agent_settings:
			if (Setup) {
				Setup();
			}
			return;
		}
	}

	void agent_tab_view::delete_turn(const std::size_t aTurn)
	{
		if (!panels_alive() || aTurn >= Turns.size() || State == agent_tab_state::working || !Turns[aTurn].Done || Turns[aTurn].Outcome == core::agent_turn_outcome::answered) {
			return;
		}
		const auto latest = aTurn + 1U == Turns.size();
		if (auto* const widget = Turns[aTurn].Widget.data(); widget != nullptr) {
			widget->hide();
			ThreadColumn->removeWidget(widget);
			widget->deleteLater();
		}
		Turns.erase(Turns.begin() + static_cast<std::ptrdiff_t>(aTurn));
		// The cards under later turns follow them down one place.
		std::erase_if(CardEntries, [aTurn](const card_entry& aEntry) { return aEntry.Turn == std::optional{ aTurn }; });
		for (auto& entry : CardEntries) {
			if (entry.Turn && *entry.Turn > aTurn) {
				--*entry.Turn;
			}
		}
		// The stopped run was this message's: nothing is left to say it.
		if (latest && State == agent_tab_state::stopped) {
			StoppedDismissed = true;
		}
		refresh();
		if (ThreadObserver) {
			ThreadObserver(thread_turns());
		}
	}

	void agent_tab_view::refresh_times()
	{
		for (const auto& kept : Turns) {
			if (kept.AskedTime != nullptr) {
				show_time(*kept.AskedTime, kept.AskedAt);
			}
			if (kept.AnsweredTime != nullptr && kept.Done) {
				show_time(*kept.AnsweredTime, kept.AnsweredAt);
			}
		}
	}

	auto agent_tab_view::turn_copy_button(const std::size_t aTurn) const -> QToolButton* { return aTurn < Turns.size() ? Turns[aTurn].Copy.data() : nullptr; }

	auto agent_tab_view::turn_time(const std::size_t aTurn) const -> QLabel* { return aTurn < Turns.size() ? Turns[aTurn].AnsweredTime.data() : nullptr; }

	void agent_tab_view::set_session_title(const QString& aTitle)
	{
		SessionTitle->setText(aTitle.isEmpty() ? tr("Agent") : aTitle);
		SessionTitle->setToolTip(aTitle);
	}

	void agent_tab_view::dress_answer(markdown_body_view& aAnswer) const
	{
		if (CodeFont) {
			aAnswer.set_code_font(*CodeFont, 4.0 * QFontMetricsF{ *CodeFont }.horizontalAdvance(QLatin1Char{ ' ' }));
		}
		if (CodeTheme) {
			aAnswer.set_code_theme(*CodeTheme);
		}
		aAnswer.set_inline_code_selector(InlineCodeFile.toStdString());
		if (CodeDispatch) {
			aAnswer.set_code_syntax_dispatch(CodeDispatch);
		}
	}

	void agent_tab_view::set_code_syntax_dispatch(std::function<void(std::vector<application::markdown_code_syntax_request>)> aDispatch)
	{
		CodeDispatch = std::move(aDispatch);
		for (const auto& kept : Turns) {
			if (kept.Answer != nullptr) {
				kept.Answer->set_code_syntax_dispatch(CodeDispatch);
				kept.Answer->request_code_syntax();
			}
		}
	}

	void agent_tab_view::apply_code_syntax(const application::markdown_code_syntax_publication& aSpans)
	{
		for (const auto& kept : Turns) {
			if (kept.Answer != nullptr) {
				kept.Answer->apply_code_syntax(aSpans);
			}
		}
	}

	void agent_tab_view::adopt_appearance(const appearance_profile& aProfile)
	{
		CodeFont  = aProfile.code_font();
		CodeTheme = markdown_code_theme{ .Colors = code_role_colors_of(aProfile.palette()), .Variant = aProfile.variant() };
		Tones     = tone_colours_of(aProfile);
		for (const auto& kept : Turns) {
			if (kept.Answer != nullptr) {
				dress_answer(*kept.Answer);
			}
			show_verdict(kept);
		}
		for (const auto& entry : CardEntries) {
			colour_card(entry);
		}
	}

	auto agent_tab_view::tones() const -> tone_colours { return Tones.value_or(builtin_tone_colours(dark(palette()))); }

	void agent_tab_view::colour_card(const card_entry& aEntry) const
	{
		// A card's widget is always its `rounded` frame (`make_card`).
		auto* const card = static_cast<rounded*>(aEntry.Widget.data());
		if (card == nullptr) {
			return;
		}
		const auto colours = tones();
		const auto quiet   = palette().color(QPalette::Active, QPalette::PlaceholderText);
		const auto tone    = tone_colour(core::report_tone_of(aEntry.Finding.Severity), colours);
		card->Edge         = tone.value_or(quiet);
		card->update();
		const auto paint = [](QLabel* aLabel, const std::optional<QColor>& aColour) {
			if (aLabel == nullptr) {
				return;
			}
			if (aColour) {
				auto coloured = aLabel->palette();
				coloured.setColor(QPalette::WindowText, *aColour);
				aLabel->setForegroundRole(QPalette::WindowText);
				aLabel->setPalette(coloured);
			} else {
				aLabel->setForegroundRole(QPalette::PlaceholderText);
			}
		};
		paint(card->findChild<QLabel*>(QStringLiteral("agentFindingSeverity")), tone.value_or(quiet));
		if (auto* const label = card->findChild<QLabel*>(QStringLiteral("agentFindingSuggestionLabel")); label != nullptr) {
			paint(label, tone_colour(core::report_tone_of(label->property("bprToneLabel").toString().toStdString()), colours));
		}
	}

	void agent_tab_view::show_verdict(const turn& aTurn) const
	{
		if (aTurn.Verdict == nullptr) {
			return;
		}
		if (aTurn.VerdictWord.empty()) {
			aTurn.Verdict->clear();
			aTurn.Verdict->hide();
			return;
		}
		// The report page's mark: "Verdict", quiet, then the verdict filled
		// with its tone and lettered in the ground.
		const auto ground = palette().color(QPalette::Active, QPalette::Base);
		const auto quiet  = palette().color(QPalette::Active, QPalette::PlaceholderText);
		const auto words  = QString::fromStdString(aTurn.VerdictWord).toHtmlEscaped();
		const auto tone   = tone_colour(aTurn.VerdictTone, tones());
		aTurn.Verdict->setText(tone ? QStringLiteral("<span style=\"color:%1\">%2</span>&nbsp;&nbsp;<span style=\"background-color:%3;color:%4;font-weight:600\">&nbsp;%5&nbsp;</span>")
										  .arg(quiet.name(), tr("Verdict").toHtmlEscaped(), tone->name(), ground.name(), words)
									: QStringLiteral("<span style=\"color:%1\">%2</span>&nbsp;&nbsp;<b>%3</b>").arg(quiet.name(), tr("Verdict").toHtmlEscaped(), words));
		aTurn.Verdict->setAccessibleName(tr("Verdict: %1").arg(QString::fromStdString(aTurn.VerdictWord)));
		aTurn.Verdict->show();
	}

	auto agent_tab_view::turn_answer(const std::size_t aTurn) const -> QString
	{
		return aTurn < Turns.size() ? Turns[aTurn].AnswerText : QString{ };
	}

	auto agent_tab_view::turn_question(const std::size_t aTurn) const -> QString { return aTurn < Turns.size() ? Turns[aTurn].Question : QString{ }; }

	auto agent_tab_view::turn_activity(const std::size_t aTurn) const -> agent_activity_view*
	{
		return aTurn < Turns.size() ? Turns[aTurn].Activity.data() : nullptr;
	}

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
			// Owner look v003: the turn's own quiet line says it works, and
			// Stop is the field's Send; a turn shown makes the banner needless.
			if (!Turns.empty()) {
				Banner->hide();
				return;
			}
			BannerTitle->setText(tr("%1 is working").arg(Launch.profile_selector()->currentText()));
			const auto seconds = Started.isValid() ? Started.elapsed() / 1000 : 0;
			const auto elapsed = agent_elapsed_words(seconds);
			auto       line    = Launch.current_phase() == agent_launch_panel::phase::running
				? tr("Running, %1. The answer and any findings appear here when it finishes.").arg(elapsed)
				: tr("Making the agent's workspace at the head, %1.").arg(elapsed);
			const auto latest  = Launch.latest_output_line();
			if (!latest.isEmpty()) {
				line += QStringLiteral("\n") + tr("Latest: %1").arg(latest.left(160));
			}
			BannerText->setText(line);
			BannerFirst->setText(tr("Stop"));
			wear_ordinary_roles(*BannerFirst);
			BannerFirst->show();
			connect(BannerFirst, &QPushButton::clicked, Launch.cancel_action(), &QAction::trigger);
			BannerSecond->hide();
			break;
		}
		case agent_tab_state::head_moved:
			BannerTitle->setText(tr("This session read %1").arg(Head->text()));
			BannerText->setText(tr("The pull request has moved on. Its findings stay anchored to the head it read."));
			BannerFirst->setText(Chat.new_session_action()->text().isEmpty() ? tr("Start a new session") : Chat.new_session_action()->text());
			wear_ordinary_roles(*BannerFirst);
			connect(BannerFirst, &QPushButton::clicked, Chat.new_session_action(), &QAction::trigger);
			BannerSecond->setText(Chat.continue_old_revision_action()->text().isEmpty() ? tr("Continue on the old head") : Chat.continue_old_revision_action()->text());
			connect(BannerSecond, &QPushButton::clicked, Chat.continue_old_revision_action(), &QAction::trigger);
			break;
		case agent_tab_state::stopped:
			BannerTitle->setText(tr("%1 stopped before answering").arg(Launch.profile_selector()->currentText()));
			// [[BPR-1738]]. A stop with a reason says it and what to do;
			// the log keeps the raw text.
			// [[BPR-1817]]. Said in words, never as the outcome's stable name.
			BannerText->setText(tr("%1 Your message is kept.")
									.arg(Launch.stop_reason_text().isEmpty() ? run_failure_words(Launch.last_outcome().value_or(application::agent_run_outcome::failed_to_start), Launch.profile_selector()->currentText())
																			 : Launch.stop_reason_text()));
			// [[BPR-1876]]. The latest turn says its cause is fixed since:
			// a banner still saying it stopped would read as how things stand.
			// [[BPR-1876]]. Nor once the stopped run's turn is deleted.
			if (LatestFixedSince || StoppedDismissed) {
				Banner->hide();
				return;
			}
			// [[BPR-1876]], superseding [[BPR-1753]]'s Check again here: an
			// Ask that ended for a setup reason is resent too, and the new run
			// starts the agent and makes its workspace, which is the check. A
			// Check again that re-ran only the setup check left this banner
			// up, and the owner read it as the check failing.
			BannerFirst->setText(tr("Resend"));
			// [[BPR-2000]]. Resend starts a run.
			wear_agent_accent(*BannerFirst, palette());
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

	void agent_tab_view::set_conversation_chip(std::optional<agent_conversation_chip> aChip)
	{
		if (aChip == ConversationShown) {
			return;
		}
		ConversationShown = std::move(aChip);
		ConversationChip->setVisible(ConversationShown.has_value());
		ConversationText->setText(ConversationShown ? ConversationShown->Text : QString{ });
		ConversationChip->setToolTip(ConversationShown ? ConversationShown->Detail : QString{ });
		Chat.composer()->setPlaceholderText(ConversationShown ? tr("Ask about this conversation…") : tr("Ask about this pull request…"));
		refresh_suggestions();
	}

	void agent_tab_view::set_conversation_dismiss(std::function<void()> aDismiss)
	{
		ConversationDismiss = std::move(aDismiss);
	}

	auto agent_tab_view::conversation_chip_text() const -> QString
	{
		return ConversationShown ? ConversationShown->Text : QString{ };
	}

	void agent_tab_view::set_answer_draft_target(std::function<agent_answer_draft_target()> aTarget) { DraftTargetSource = std::move(aTarget); }

	void agent_tab_view::set_answer_draft_dispatch(agent_answer_draft_dispatch aDispatch) { DraftDispatch = std::move(aDispatch); }

	void agent_tab_view::set_draft_navigation(agent_answer_draft_navigation aNavigate) { DraftNavigate = std::move(aNavigate); }

	void agent_tab_view::release_answer_draft(const std::string& aKey)
	{
		for (auto& turn : Turns) {
			if (!turn.Drafted || !turn.DraftTarget || turn.DraftTarget->Key != aKey || turn.Draft == nullptr) {
				continue;
			}
			turn.Drafted = false;
			turn.Draft->setText(tr("Draft"));
			turn.Draft->setEnabled(true);
			present_keep(turn);
		}
	}

	void agent_tab_view::present_keep(turn& aTurn)
	{
		if (aTurn.SaveNote == nullptr || aTurn.Draft == nullptr) {
			return;
		}
		// Owner, look v006: no Keep local on the agent's answer; Draft alone.
		aTurn.SaveNote->setVisible(aTurn.KeepOffered && aTurn.Note != 0U);
		// RFC-BPR-0127 (`core::decide_keep`): Draft where the host found a
		// thread or line to take a draft; else the struck pencil says why.
		const auto canDraft = aTurn.DraftTarget.has_value() && !aTurn.DraftTarget->Local;
		const auto said     = aTurn.Drafted || aTurn.Draft->text().startsWith(tr("Not drafted"));
		if (!said) {
			aTurn.Draft->setText(tr("Draft"));
			aTurn.Draft->setEnabled(canDraft);
			aTurn.Draft->setToolTip(canDraft ? tr("Draft it where you asked; nothing is posted") : tr("No line or thread here takes a draft"));
		}
		const auto refused = !canDraft && !aTurn.Drafted;
		aTurn.Draft->setIcon(conversation_icon(refused ? conversation_glyph::edit_refused : conversation_glyph::edit, palette()));
		aTurn.Draft->setProperty("draftRefused", refused);
		aTurn.Draft->setAccessibleDescription(aTurn.Draft->toolTip());
		aTurn.Draft->setVisible(aTurn.KeepOffered);
	}

	auto agent_tab_view::turn_draft_button(const std::size_t aTurn) const -> QPushButton* { return aTurn < Turns.size() ? Turns[aTurn].Draft.data() : nullptr; }

	auto agent_tab_view::turn_asked(const std::size_t aTurn) const -> QString
	{
		if (aTurn >= Turns.size()) {
			return { };
		}
		return Turns[aTurn].Asked != nullptr ? Turns[aTurn].Asked->accessibleName() : Turns[aTurn].Question;
	}

	auto agent_tab_view::turn_name(const std::size_t aTurn) const -> QString { return aTurn < Turns.size() ? Turns[aTurn].Name : QString{ }; }

	auto agent_tab_view::turn_show_all(const std::size_t aTurn) const -> QPushButton* { return aTurn < Turns.size() ? Turns[aTurn].ShowAll.data() : nullptr; }

	auto agent_tab_view::turn_about(const std::size_t aTurn) const -> QString
	{
		return aTurn < Turns.size() && Turns[aTurn].About ? Turns[aTurn].About->Text : QString{ };
	}

	auto agent_tab_view::turn_note_button(const std::size_t aTurn) const -> QPushButton* { return aTurn < Turns.size() ? Turns[aTurn].SaveNote.data() : nullptr; }

	auto agent_tab_view::turn_recovery_button(const std::size_t aTurn) const -> QPushButton* { return aTurn < Turns.size() ? Turns[aTurn].Recovery.data() : nullptr; }

	auto agent_tab_view::turn_delete_button(const std::size_t aTurn) const -> QPushButton* { return aTurn < Turns.size() ? Turns[aTurn].Delete.data() : nullptr; }

	auto agent_tab_view::turn_reply(const std::size_t aTurn) const -> QWidget* { return aTurn < Turns.size() ? Turns[aTurn].Reply.data() : nullptr; }

	auto agent_tab_view::turn_outcome(const std::size_t aTurn) const -> std::optional<core::agent_turn_outcome>
	{
		return aTurn < Turns.size() && Turns[aTurn].Done ? std::optional{ Turns[aTurn].Outcome } : std::nullopt;
	}

	void agent_tab_view::present_finding_card_state(const std::uint64_t aSession, const std::size_t aOrdinal, const agent_finding_card_state aState, const QString& aWhy, const std::uint64_t aNote)
	{
		const auto settle = [aState, &aWhy, aNote](card_entry& aEntry) {
			aEntry.State = aState;
			aEntry.Why   = aWhy;
			aEntry.Note  = aState == agent_finding_card_state::noted ? aNote : 0U;
		};
		for (auto& entry : CardEntries) {
			if (entry.Session == aSession && entry.Ordinal == aOrdinal) {
				settle(entry);
				show_card_state(entry);
				// Owner report 2026-11-10, after BPR-2052: kept past a restart.
				if (entry.Turn && *entry.Turn < Turns.size()) {
					observe_card_state(Turns[*entry.Turn].FindingsBlock, entry);
				}
			}
		}
		// A card whose turn the thread stopped showing while the host worked
		// on it takes the state too, so it comes back with it.
		for (auto& kept : RetainedCards) {
			for (auto& entry : kept.Cards) {
				if (entry.Session == aSession && entry.Ordinal == aOrdinal) {
					settle(entry);
					observe_card_state(kept.FindingsBlock, entry);
				}
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
		refresh_draft_all(aTurn);
	}

	auto agent_tab_view::make_card(card_entry& aEntry, QWidget* aParent) -> QWidget*
	{
		const auto& finding = aEntry.Finding;
		auto* const card    = new rounded{ rounded::look::card, aParent };
		card->setObjectName(QStringLiteral("agentFindingCard"));
		auto* const col = new QVBoxLayout{ card };
		col->setContentsMargins(14, 10, 12, 10);
		col->setSpacing(5);
		auto* const meta = new QHBoxLayout{ };
		meta->setSpacing(6);
		auto* const severity = new QLabel{ capitalised(core::name(finding.Severity)), card };
		auto        sevFont  = severity->font();
		sevFont.setWeight(QFont::DemiBold);
		severity->setFont(sevFont);
		// [[BPR-2236]]. Its colour, and the card's edge, are its tone's
		// (`colour_card`), as the report page draws the same finding.
		severity->setObjectName(QStringLiteral("agentFindingSeverity"));
		meta->addWidget(severity);
		auto* const kind = new QLabel{ QStringLiteral("%1 · %2").arg(confidence_words(finding.Confidence), QString::fromUtf8(core::name(finding.Category).data(), static_cast<qsizetype>(core::name(finding.Category).size()))), card };
		kind->setObjectName(QStringLiteral("agentFindingKind"));
		kind->setForegroundRole(QPalette::PlaceholderText);
		// [[BPR-2059]], owner 2026-11-10: the chips were cut to "possible ·
		// correctn" while the path kept its width. The severity and kind keep
		// their words; the anchor gives way and shortens in the middle.
		kind->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
		meta->addWidget(kind);
		meta->addStretch(1);
		// Lines are shown one-based, first and last, as an editor numbers them.
		const auto  first = finding.Lines.Begin + 1U;
		const auto  last  = std::max(first, finding.Lines.End);
		auto* const where = new eliding_anchor{ first == last ? QStringLiteral("%1:%2").arg(QString::fromStdString(finding.Path).section(QLatin1Char{ '/' }, -1)).arg(first)
															  : QStringLiteral("%1:%2–%3").arg(QString::fromStdString(finding.Path).section(QLatin1Char{ '/' }, -1)).arg(first).arg(last),
												card };
		where->setObjectName(QStringLiteral("agentFindingAnchor"));
		where->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		where->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		where->setMinimumWidth(where->minimumSizeHint().width());
		auto code = QFont{ QStringLiteral("Menlo") };
		code.setStyleHint(QFont::Monospace);
		code.setPointSizeF(std::max(1.0, font().pointSizeF() * 0.86));
		code.setWeight(QFont::DemiBold);
		where->setFont(code);
		where->setToolTip(aEntry.Anchored ? QString::fromStdString(finding.Path) : tr("%1 (not the file open when the agent answered)").arg(QString::fromStdString(finding.Path)));
		where->setAccessibleName(where->text());
		meta->addWidget(where, 1);
		col->addLayout(meta);
		aEntry.Id = NextCardId++;
		// [[BPR-2059]], owner 2026-11-10 ("just a big wall of text"): the
		// claim, the evidence and the suggested comment are each Markdown, so
		// code reads as code, each in its own block with room between them.
		// The suggested comment is set upright in an inset: it is the text a
		// draft starts from, shown as it would read once posted, its leading
		// Conventional Comments label drawn as the label mark comments wear.
		const auto cardText = [this, &aEntry](QWidget* aHolder, const std::string& aBody, const QString& aName, const int aInset) {
			auto* const view = new markdown_body_view{ aHolder };
			view->setObjectName(aName);
			view->set_in_place(true);
			// Its width is the card's: a long line wraps rather than widening it.
			auto policy = view->sizePolicy();
			policy.setHorizontalPolicy(QSizePolicy::Ignored);
			view->setSizePolicy(policy);
			view->setMinimumWidth(0);
			view->setFocusPolicy(Qt::NoFocus);
			view->setProperty("agentCardText", true);
			view->setProperty("agentCardInset", aInset);
			dress_answer(*view);
			view->set_height_observer([this, view] { fit_card_text(*view); });
			view->installEventFilter(this);
			view->set_body(aBody, "agent-card-" + std::to_string(aEntry.Id) + "-" + aName.toStdString());
			aEntry.Texts.emplace_back(view);
			return view;
		};
		auto* const claim     = cardText(card, finding.Claim, QStringLiteral("agentFindingClaim"), 0);
		auto        claimFont = font();
		claimFont.setWeight(QFont::DemiBold);
		claim->setFont(claimFont);
		col->addWidget(claim);
		if (!finding.Evidence.empty()) {
			col->addSpacing(2);
			auto* const evidence = cardText(card, finding.Evidence, QStringLiteral("agentFindingEvidence"), 0);
			auto        quiet    = evidence->palette();
			quiet.setColor(QPalette::Text, palette().color(QPalette::PlaceholderText));
			quiet.setColor(QPalette::WindowText, palette().color(QPalette::PlaceholderText));
			evidence->setPalette(quiet);
			col->addWidget(evidence);
		}
		if (finding.SuggestedComment && !finding.SuggestedComment->empty()) {
			col->addSpacing(6);
			auto* const inset = new suggestion_inset{ card };
			inset->setObjectName(QStringLiteral("agentFindingSuggestion"));
			auto* const insetColumn = new QVBoxLayout{ inset };
			insetColumn->setContentsMargins(12, 7, 10, 8);
			insetColumn->setSpacing(3);
			// A Conventional Comments label the agent led with is said once,
			// quietly, beside the caption, and the comment reads from its
			// subject; the draft still opens with it in the label control.
			const auto& suggested = *finding.SuggestedComment;
			const auto  read      = core::recognize_feedback_prefix(suggested);
			auto        shown     = suggested;
			auto        label     = QString{ };
			if (read.Status == core::feedback_prefix_status::recognized && read.Header && read.Subject && read.Subject->Begin <= suggested.size() && read.Header->End <= suggested.size()) {
				label = QString::fromStdString(suggested.substr(read.Header->Begin, read.Header->End - read.Header->Begin)).trimmed();
				if (label.endsWith(QLatin1Char{ ':' })) {
					label.chop(1);
				}
				shown = suggested.substr(read.Subject->Begin);
			}
			auto* const captionRow = new QHBoxLayout{ };
			captionRow->setSpacing(8);
			auto* const caption = new QLabel{ tr("Suggested comment"), inset };
			caption->setObjectName(QStringLiteral("agentFindingSuggestionCaption"));
			caption->setForegroundRole(QPalette::PlaceholderText);
			auto small = caption->font();
			small.setPointSizeF(std::max(1.0, small.pointSizeF() * 0.86));
			caption->setFont(small);
			captionRow->addWidget(caption);
			captionRow->addStretch(1);
			if (!label.isEmpty()) {
				auto* const labelled = new QLabel{ label, inset };
				labelled->setObjectName(QStringLiteral("agentFindingSuggestionLabel"));
				// [[BPR-2236]]. In its tone, as the report page draws the label.
				labelled->setForegroundRole(QPalette::PlaceholderText);
				labelled->setProperty("bprToneLabel", label);
				labelled->setFont(small);
				labelled->setToolTip(tr("The label the agent gave it. A draft opens with it chosen in the label control."));
				captionRow->addWidget(labelled);
			}
			insetColumn->addLayout(captionRow);
			auto* const comment = cardText(inset, shown, QStringLiteral("agentFindingComment"), 22);
			insetColumn->addWidget(comment);
			col->addWidget(inset);
		}
		col->addSpacing(2);
		auto* const actions = new QHBoxLayout{ };
		actions->setSpacing(6);
		const auto index = aEntry.Id;
		// RFC-BPR-0127. One keep action, in one place: Draft comment where
		// its line takes a draft, Keep local where none does
		// (`core::decide_keep`; `show_card_state` shows the one).
		// Owner, look v006 ("Don't have keep local there, just draft is
		// fine"): the card's one keep action is Draft, raised, with the
		// pencil; struck through and disabled where its line takes no draft.
		auto* const draft = new quiet_push_button{ tr("Draft"), card };
		draft->setObjectName(QStringLiteral("agentFindingDraftComment"));
		draft->setIcon(conversation_icon(conversation_glyph::edit, palette()));
		connect(draft, &QPushButton::clicked, this, [this, index] { act_on_card(index, agent_finding_card_action::draft_comment); });
		actions->addWidget(draft);
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
		// Owner look v004: a finding kept local goes to its comment.
		auto* const goToNote = new quiet_push_button{ tr("Go to comment"), card };
		goToNote->setObjectName(QStringLiteral("agentFindingGoToComment"));
		goToNote->setToolTip(tr("Open the local comment this finding was kept as"));
		goToNote->hide();
		connect(goToNote, &QPushButton::clicked, this, [this, index] {
			const auto found = std::ranges::find(CardEntries, index, &card_entry::Id);
			if (found != CardEntries.end() && found->Note != 0U && NoteNavigate) {
				NoteNavigate(found->Note);
			}
		});
		actions->addWidget(goToNote);
		actions->addStretch(1);
		col->addLayout(actions);
		// Choosing a card selects its finding and shows its lines.
		card->installEventFilter(this);
		card->setProperty("agentFindingCard", QVariant::fromValue<qulonglong>(index));
		card->setCursor(Qt::PointingHandCursor);
		aEntry.Widget = card;
		colour_card(aEntry);
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
		show_card_keep(aEntry);
		if (auto* const button = card->findChild<QPushButton*>(QStringLiteral("agentFindingDismiss")); button != nullptr) {
			button->setVisible(open);
		}
		undo->setVisible(aEntry.State == agent_finding_card_state::dismissed);
		if (auto* const goToNote = card->findChild<QPushButton*>(QStringLiteral("agentFindingGoToComment")); goToNote != nullptr) {
			goToNote->setVisible(aEntry.State == agent_finding_card_state::noted && aEntry.Note != 0U && NoteNavigate);
		}
		switch (aEntry.State) {
		case agent_finding_card_state::open:      state->clear(); break;
		case agent_finding_card_state::working:   state->setText(tr("Working…")); break;
		case agent_finding_card_state::drafted:   state->setText(tr("Drafted. It is a private draft until you publish it.")); break;
		case agent_finding_card_state::noted:     state->setText(tr("Kept local.")); break;
		case agent_finding_card_state::dismissed: state->setText(tr("Dismissed.")); break;
		case agent_finding_card_state::refused:   state->setText(aEntry.Why.isEmpty() ? tr("That could not be done.") : aEntry.Why); break;
		}
		state->setVisible(!state->text().isEmpty());
		card->setProperty("agentFindingStateName", static_cast<int>(aEntry.State));
		card->setEnabled(true);
		card->update();
		if (aEntry.Turn) {
			refresh_draft_all(*aEntry.Turn);
		}
	}

	void agent_tab_view::show_card_keep(const card_entry& aEntry) const
	{
		if (aEntry.Widget == nullptr) {
			return;
		}
		// RFC-BPR-0127. The one keep action `core::decide_keep` chooses: a
		// card whose line takes a draft offers Draft comment, one whose line
		// is known not to (gone, or a file the pull request does not change,
		// or past the end of the file shown) Keep local. Asked again on each
		// refresh, so a card follows what the host knows now.
		const auto open      = aEntry.State == agent_finding_card_state::open || aEntry.State == agent_finding_card_state::refused;
		const auto placeable = !aEntry.Gone && (!FindingPlaceable || FindingPlaceable(aEntry.Finding));
		const auto keep      = core::decide_keep({ .ThreadReply = false, .LineTakesDraft = placeable, .FileInPullRequest = false }).Kind;
		if (auto* const draft = aEntry.Widget->findChild<QPushButton*>(QStringLiteral("agentFindingDraftComment")); draft != nullptr) {
			const auto allowed = keep == core::keep_kind::draft;
			draft->setVisible(open);
			draft->setEnabled(allowed);
			draft->setIcon(conversation_icon(allowed ? conversation_glyph::edit : conversation_glyph::edit_refused, palette()));
			draft->setProperty("draftRefused", !allowed);
			draft->setToolTip(allowed ? tr("Draft it at its line; nothing is posted") : tr("No line here takes a draft"));
			draft->setAccessibleDescription(draft->toolTip());
		}
	}

	void agent_tab_view::fit_card_text(markdown_body_view& aView) const
	{
		// Before the card is laid out its width is a guess: the viewport less
		// the thread's, the reply's and the card's margins, and the inset's.
		constexpr auto margins = 12 + 12 + reply_left_margin + reply_right_margin + 14 + 12;
		const auto     guess   = (Viewport == nullptr ? 360 : Viewport->width()) - margins - aView.property("agentCardInset").toInt();
		const auto     width   = aView.isVisible() && aView.width() > 16 ? aView.width() : guess;
		aView.setFixedHeight(aView.heightForWidth(std::max(1, width)));
	}

	auto agent_tab_view::turn_of_widget(const QObject* const aWidget) const -> std::optional<std::size_t>
	{
		for (std::size_t index = 0U; index < Turns.size(); ++index) {
			if (Turns[index].DraftAll == aWidget || Turns[index].DraftAllUndo == aWidget) {
				return index;
			}
		}
		return std::nullopt;
	}

	void agent_tab_view::set_draft_all_dispatch(agent_draft_all_dispatch aDispatch, agent_draft_all_undo aUndo)
	{
		DraftAllDispatch = std::move(aDispatch);
		DraftAllUndo     = std::move(aUndo);
		for (std::size_t index = 0U; index < Turns.size(); ++index) {
			refresh_draft_all(index);
		}
	}

	void agent_tab_view::set_finding_placeability(agent_finding_placeability aPlaceable)
	{
		FindingPlaceable = std::move(aPlaceable);
		for (auto& entry : CardEntries) {
			show_card_state(entry);
		}
		for (std::size_t index = 0U; index < Turns.size(); ++index) {
			refresh_draft_all(index);
		}
	}

	void agent_tab_view::mark_finding_gone(const std::uint64_t aSession, const std::size_t aOrdinal)
	{
		for (auto& entry : CardEntries) {
			if (entry.Session == aSession && entry.Ordinal == aOrdinal) {
				entry.Gone = true;
				// RFC-BPR-0127. Its line is gone: it offers Keep local now.
				show_card_state(entry);
			}
		}
	}

	auto agent_tab_view::turn_draft_all_button(const std::size_t aTurn) const -> QPushButton* { return aTurn < Turns.size() ? Turns[aTurn].DraftAll.data() : nullptr; }

	auto agent_tab_view::turn_draft_all_state(const std::size_t aTurn) const -> QLabel* { return aTurn < Turns.size() ? Turns[aTurn].DraftAllState.data() : nullptr; }

	auto agent_tab_view::turn_draft_all_undo(const std::size_t aTurn) const -> QPushButton* { return aTurn < Turns.size() ? Turns[aTurn].DraftAllUndo.data() : nullptr; }

	namespace {
		[[nodiscard]] auto draft_all_state_of(const agent_finding_card_state aState) noexcept -> core::draft_all_card_state
		{
			switch (aState) {
			case agent_finding_card_state::working:   return core::draft_all_card_state::working;
			case agent_finding_card_state::drafted:   return core::draft_all_card_state::drafted;
			case agent_finding_card_state::noted:     return core::draft_all_card_state::kept_local;
			case agent_finding_card_state::dismissed: return core::draft_all_card_state::dismissed;
			case agent_finding_card_state::open:
			case agent_finding_card_state::refused:   return core::draft_all_card_state::open;
			}
			return core::draft_all_card_state::open;
		}
	}

	namespace {
		/// "1 draft", "3 drafts": the count and the one or the other word.
		[[nodiscard]] auto counted(const std::size_t aCount, const QString& aOne, const QString& aMany) -> QString
		{
			return QStringLiteral("%1 %2").arg(aCount).arg(aCount == 1U ? aOne : aMany);
		}
	}

	auto agent_tab_view::draft_all_words(const core::draft_all_outcome& aOutcome) -> QString
	{
		auto reasons = QStringList{ };
		if (aOutcome.AlreadyDrafted > 0U) {
			reasons << tr("%1 already drafted").arg(aOutcome.AlreadyDrafted);
		}
		if (aOutcome.NotInDiff > 0U) {
			reasons << tr("%1 no longer in the diff").arg(aOutcome.NotInDiff);
		}
		if (aOutcome.Dismissed > 0U) {
			reasons << tr("%1 dismissed").arg(aOutcome.Dismissed);
		}
		if (aOutcome.Busy > 0U) {
			reasons << tr("%1 still being drafted or saved").arg(aOutcome.Busy);
		}
		if (aOutcome.KeptLocal > 0U) {
			reasons << tr("%1 kept local").arg(aOutcome.KeptLocal);
		}
		if (aOutcome.Refused > 0U) {
			reasons << (aOutcome.Refused == 1U ? tr("1 could not be drafted; its card says why") : tr("%1 could not be drafted; their cards say why").arg(aOutcome.Refused));
		}
		auto drafted = aOutcome.Drafted == 0U ? tr("Nothing drafted.") : tr("Drafted %1.").arg(counted(aOutcome.Drafted, tr("private draft"), tr("private drafts")));
		if (reasons.isEmpty()) {
			return drafted;
		}
		return drafted + QLatin1Char{ ' ' } + tr("Skipped %1: %2.").arg(aOutcome.skipped()).arg(reasons.join(QStringLiteral(", ")));
	}

	auto agent_tab_view::draft_all_undo_words(const std::size_t aRemoved, const std::size_t aKept) -> QString
	{
		const auto removed = aRemoved == 0U ? tr("Nothing removed.") : tr("Removed %1.").arg(counted(aRemoved, tr("draft"), tr("drafts")));
		return aKept == 0U ? removed : removed + QLatin1Char{ ' ' } + tr("Kept %1 you changed since.").arg(counted(aKept, tr("draft"), tr("drafts")));
	}

	void agent_tab_view::refresh_draft_all(const std::size_t aTurn)
	{
		if (aTurn >= Turns.size() || Turns[aTurn].DraftAll == nullptr) {
			return;
		}
		auto& turn  = Turns[aTurn];
		auto  cards = std::vector<core::draft_all_candidate>{ };
		for (const auto& entry : CardEntries) {
			if (entry.Turn == std::optional{ aTurn }) {
				cards.push_back({ .State     = draft_all_state_of(entry.State),
								  .Suggested = entry.Finding.SuggestedComment && !core::writing_blank(*entry.Finding.SuggestedComment),
								  .Placeable = !entry.Gone && (FindingPlaceable ? FindingPlaceable(entry.Finding) : entry.Anchored) });
			}
		}
		const auto offered = core::draft_all_offer(cards);
		const auto several = core::draft_all_suggestions(cards) >= core::minimum_draft_all_suggestions;
		turn.DraftAll->setText(tr("Draft all (%1)").arg(offered));
		turn.DraftAll->setVisible(offered > 0U || turn.DraftAllBusy);
		turn.DraftAll->setEnabled(offered > 0U && !turn.DraftAllBusy && DraftAllDispatch != nullptr);
		turn.DraftAllRow->setVisible(DraftAllDispatch != nullptr && (several || !turn.DraftAllState->isHidden()));
	}

	void agent_tab_view::draft_all(const std::size_t aTurn)
	{
		if (aTurn >= Turns.size() || !DraftAllDispatch || Turns[aTurn].DraftAllBusy || !panels_alive()) {
			return;
		}
		auto indices = std::vector<std::size_t>{ };
		auto cards   = std::vector<core::draft_all_candidate>{ };
		for (std::size_t index = 0U; index < CardEntries.size(); ++index) {
			const auto& entry = CardEntries[index];
			if (entry.Turn == std::optional{ aTurn }) {
				indices.push_back(index);
				cards.push_back({ .State     = draft_all_state_of(entry.State),
								  .Suggested = entry.Finding.SuggestedComment && !core::writing_blank(*entry.Finding.SuggestedComment),
								  .Placeable = !entry.Gone && (FindingPlaceable ? FindingPlaceable(entry.Finding) : entry.Anchored) });
			}
		}
		const auto plan    = core::plan_draft_all(cards);
		auto       request = agent_draft_all_request{ .Batch = NextDraftAllBatch++, .Planned = core::start_draft_all(plan) };
		// A suggestion skipped as not in the diff stays skipped: the file on
		// screen changes as the batch drafts, and with it what the host can
		// tell of another file's lines.
		for (std::size_t position = 0U; position < cards.size(); ++position) {
			if (cards[position].Suggested && !cards[position].Placeable) {
				CardEntries[indices[position]].Gone = true;
			}
		}
		auto& turn = Turns[aTurn];
		turn.DraftAllCards.clear();
		for (const auto position : plan.Draft) {
			auto& entry = CardEntries[indices[position]];
			request.Cards.push_back({ .Session = entry.Session, .Ordinal = entry.Ordinal, .Finding = entry.Finding, .Action = agent_finding_card_action::draft_comment });
			turn.DraftAllCards.push_back(entry.Id);
		}
		if (request.Cards.empty()) {
			return;
		}
		turn.DraftAllBusy  = true;
		turn.DraftAllBatch = request.Batch;
		turn.DraftAllUndo->hide();
		turn.DraftAllState->setText(tr("Drafting %1…").arg(counted(request.Cards.size(), tr("suggested comment"), tr("suggested comments"))));
		turn.DraftAllState->show();
		for (const auto id : turn.DraftAllCards) {
			if (const auto found = std::ranges::find(CardEntries, id, &card_entry::Id); found != CardEntries.end()) {
				found->State = agent_finding_card_state::working;
				show_card_state(*found);
			}
		}
		refresh_draft_all(aTurn);
		const auto batch = request.Batch;
		DraftAllDispatch(std::move(request), [this, alive = QPointer<agent_tab_view>{ this }, batch](const core::draft_all_outcome& aOutcome, const bool aUndoable) {
			if (alive == nullptr) {
				return;
			}
			for (std::size_t index = 0U; index < Turns.size(); ++index) {
				auto& kept = Turns[index];
				if (kept.DraftAllBatch != batch || kept.DraftAll == nullptr) {
					continue;
				}
				kept.DraftAllBusy = false;
				kept.DraftAllState->setText(draft_all_words(aOutcome));
				kept.DraftAllState->show();
				kept.DraftAllUndo->setVisible(aUndoable && DraftAllUndo != nullptr);
				refresh_draft_all(index);
			}
		});
	}

	void agent_tab_view::undo_draft_all(const std::size_t aTurn)
	{
		if (aTurn >= Turns.size() || !DraftAllUndo || Turns[aTurn].DraftAllBusy || Turns[aTurn].DraftAllBatch == 0U) {
			return;
		}
		auto& turn = Turns[aTurn];
		turn.DraftAllUndo->hide();
		turn.DraftAllBusy = true;
		refresh_draft_all(aTurn);
		const auto batch = turn.DraftAllBatch;
		DraftAllUndo(batch, [this, alive = QPointer<agent_tab_view>{ this }, batch](const std::size_t aRemoved, const std::size_t aKept) {
			if (alive == nullptr) {
				return;
			}
			for (std::size_t index = 0U; index < Turns.size(); ++index) {
				auto& kept = Turns[index];
				if (kept.DraftAllBatch != batch || kept.DraftAll == nullptr) {
					continue;
				}
				kept.DraftAllBusy  = false;
				kept.DraftAllBatch = 0U;
				// The host opened each card whose draft it removed, through
				// present_finding_card_state; a kept draft's card stays drafted.
				kept.DraftAllCards.clear();
				kept.DraftAllState->setText(draft_all_undo_words(aRemoved, aKept));
				kept.DraftAllState->show();
				refresh_draft_all(index);
			}
		});
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
			: aAction == agent_finding_card_action::keep_local                  ? finding_queue_action::keep_local
																				: finding_queue_action::dismiss;
		if (auto* const action = Findings.action(queued); action != nullptr) {
			action->trigger();
		}
		entry.State = aAction == agent_finding_card_action::draft_comment ? agent_finding_card_state::drafted
			: aAction == agent_finding_card_action::keep_local            ? agent_finding_card_state::noted
																		  : agent_finding_card_state::dismissed;
		show_card_state(entry);
	}

	void agent_tab_view::ask()
	{
		if (!panels_alive()) {
			return;
		}
		const auto text = Chat.composer()->toPlainText().trimmed();
		// RFC-BPR-0125 (AC09). A suggested prompt sent as offered is never
		// read as `/model` or `/effort`: it is sent as text.
		const auto direct = SendingMacro && SendingMacro->SkillText;
		if (!direct && run_slash_command(text)) {
			Released = release_outcome::answered;
			return;
		}
		// RFC-BPR-0126 Q4/Q5 as answered: while a turn runs or messages wait,
		// a typed message is a follow-up (Queue by default, Steer by the
		// setting; Cmd+Return the other for one message).
		const auto typed = !SendingMacro && !SendingReturning && SendingExplain.isEmpty() && SendingSuggest.isEmpty() && !DraftedMacro && !Releasing;
		// AC10: the core decides, over the run the tab shows and the queue.
		const auto followUp = core::decide_follow_up({ .TurnShown = State == agent_tab_state::working, .QueueHolds = QueueOccupied, .Empty = text.isEmpty(), .SteerByDefault = FollowUpSteer, .Opposite = Opposite });
		if (typed && FollowUp && followUp != core::follow_up::send) {
			if (FollowUp(text, followUp == core::follow_up::steer)) {
				Chat.composer()->clear();
				return;
			}
		}
		Notice->hide();
		hide_send_refusal();
		// [[BPR-1817]]. Asked of the core before anything is sent: a send
		// it refuses here is said here, never dropped by a disabled action.
		RefusalsSeen = Launch.refusal_count();
		if (const auto refused = core::decide_agent_send(send_facts(text.isEmpty())); refused != core::agent_send_refusal::none) {
			refuse_send(refused);
			return;
		}
		// [[BPR-2002]], RFC-BPR-0102. The core decides what the question is
		// about from what the reviewer did: a suggestion's own subject, the
		// attached lines, or else the pull request.
		// RFC-BPR-0125 (Q2, AC10). A drafted suggested prompt, its chip still
		// in the field, keeps its subject and kind; its edited words are the
		// reviewer's, so skills are read from all of them.
		const auto drafted = !SendingMacro && !SendingReturning && DraftedMacro.has_value();
		if (drafted) {
			SendingMacro            = DraftedMacro;
			SendingMacro->SkillText = std::nullopt;
		}
		const auto context = current_suggestion_context();
		const auto facts   = core::agent_question_facts{
			.LinesAttached        = lines_attached(),
			.LinesSelected        = context.Lines.has_value(),
			.ConversationAttached = ConversationShown.has_value(),
			.Suggested            = SendingMacro ? SendingMacro->Subject : std::nullopt,
			.Review               = SendingMacro && SendingMacro->Review,
			.OpenFile             = context.OpenFile.toStdString(),
			.Returning            = SendingReturning,
			.Explain              = SendingExplain.toStdString(),
			.SuggestChange        = SendingSuggest.toStdString(),
		};
		LastQuestion = core::decide_agent_question(facts);
		if (!LastQuestion) {
			// Not offered as it stands; sent as a question about the pull
			// request rather than refused after the press.
			LastQuestion = core::decide_agent_question(core::agent_question_facts{ .ConversationAttached = facts.ConversationAttached });
		}
		Chat.set_question(LastQuestion);
		Chat.set_skill_text(SendingMacro ? SendingMacro->SkillText : std::nullopt);
		Released = release_outcome::sent;
		Chat.send_action()->trigger();
		Chat.set_skill_text(std::nullopt);
		if (drafted) {
			SendingMacro.reset();
		}
	}

	auto agent_tab_view::current_suggestion_context() const -> agent_suggestion_context
	{
		return SuggestionContext ? SuggestionContext() : agent_suggestion_context{ };
	}

	void agent_tab_view::present_lines_chip(const agent_suggestion_context& aContext)
	{
		if (LinesChip == nullptr || Context == nullptr) {
			return;
		}
		// A new selection attaches again; the same one stays as it was left.
		const auto seen = aContext.Lines ? std::optional{ std::pair{ aContext.OpenFile, *aContext.Lines } } : std::nullopt;
		if (seen != LinesSeen) {
			LinesSeen     = seen;
			LinesDetached = false;
		}
		const auto shown = lines_attached();
		if (shown) {
			const auto [first, last] = LinesSeen->second;
			const auto name          = LinesSeen->first.section(QLatin1Char{ '/' }, -1);
			const auto range         = first == last ? QString::number(first) : QStringLiteral("%1–%2").arg(first).arg(last);
			LinesText->setText(name.isEmpty() ? tr("Lines %1").arg(range) : QStringLiteral("%1 %2").arg(name, range));
		}
		LinesChip->setVisible(shown);
		Context->setEnabled(LinesSeen.has_value() && LinesDetached && !ConversationShown);
		if (!ConversationShown) {
			Chat.composer()->setPlaceholderText(shown ? tr("Ask about these lines…") : tr("Ask about this pull request…"));
		}
	}

	auto agent_tab_view::lines_attached() const -> bool { return LinesSeen.has_value() && !LinesDetached && !ConversationShown; }

	auto agent_tab_view::lines_chip_text() const -> QString { return LinesChip->isHidden() ? QString{ } : LinesText->text(); }

	void agent_tab_view::attach_lines()
	{
		LinesDetached = false;
		refresh_suggestions();
	}

	auto agent_tab_view::send_facts(const bool aMessageEmpty) const -> core::agent_send_facts
	{
		const auto required = !Chat.offered_continuations().empty();
		return {
			.ReviewOpen           = Chat.send_action()->isEnabled() || required,
			.ContinuationRequired = required,
			.AgentChosen          = Launch.selected_profile().has_value(),
			.AgentUnavailable     = !Launch.unavailable_agent_name().isEmpty(),
			.MessageEmpty         = aMessageEmpty,
			.InFlight             = State == agent_tab_state::working,
		};
	}

	auto agent_tab_view::agent_name() const -> QString
	{
		const auto unavailable = Launch.unavailable_agent_name();
		return unavailable.isEmpty() ? Launch.profile_selector()->currentText() : unavailable;
	}

	auto agent_tab_view::send_blocked() const -> core::agent_send_refusal
	{
		// Only what stops the button itself; an empty field is said on a press.
		const auto refused = core::decide_agent_send(send_facts(false));
		return refused == core::agent_send_refusal::no_review || refused == core::agent_send_refusal::session_stale ? refused : core::agent_send_refusal::none;
	}

	void agent_tab_view::reveal_run_failure()
	{
		refresh();
		if (!panels_alive() || Banner == nullptr || Banner->isHidden()) {
			return;
		}
		if (auto* const scroll = qobject_cast<QScrollArea*>(Viewport != nullptr ? Viewport->parentWidget() : nullptr); scroll != nullptr) {
			scroll->ensureWidgetVisible(Banner);
		}
		if (BannerFirst != nullptr && !BannerFirst->isHidden()) {
			BannerFirst->setFocus(Qt::OtherFocusReason);
		}
	}

	auto agent_tab_view::run_failure_words(const application::agent_run_outcome aOutcome, const QString& aAgent) -> QString
	{
		const auto agent = aAgent.isEmpty() ? tr("The agent") : aAgent;
		switch (aOutcome) {
		case application::agent_run_outcome::failed_to_start:         return tr("%1 didn't start.").arg(agent);
		case application::agent_run_outcome::nonzero_exit:            return tr("%1 exited with an error before answering.").arg(agent);
		case application::agent_run_outcome::protocol_failure:        return tr("%1 answered in a form this app can't read.").arg(agent);
		case application::agent_run_outcome::workspace_unavailable:   return tr("%1's workspace couldn't be made, so it didn't start.").arg(agent);
		case application::agent_run_outcome::private_storage_failure: return tr("Private history couldn't record the run, so %1 didn't start.").arg(agent);
		case application::agent_run_outcome::cancelled:               return tr("Stopped before %1 answered.").arg(agent);
		case application::agent_run_outcome::timeout:                 return tr("%1 ran out of time before answering.").arg(agent);
		case application::agent_run_outcome::stale_result:            return tr("The pull request moved on while %1 worked, so its answer was set aside.").arg(agent);
		case application::agent_run_outcome::success:                 return { };
		}
		return { };
	}

	auto agent_tab_view::send_refusal_words(const core::agent_send_refusal aRefusal, const QString& aAgent) -> QString
	{
		const auto agent = aAgent.isEmpty() ? tr("The agent") : aAgent;
		switch (aRefusal) {
		case core::agent_send_refusal::none:                     return { };
		case core::agent_send_refusal::no_review:                return tr("Open a pull request's changes to ask an agent about them.");
		case core::agent_send_refusal::no_agent:                 return tr("Choose an agent in the agent menu, then send.");
		case core::agent_send_refusal::empty_message:            return tr("Type a question, then send.");
		case core::agent_send_refusal::request_in_flight:        return tr("%1 is still answering. Wait for its answer, or stop it and send again.").arg(agent);
		case core::agent_send_refusal::session_stale:            return tr("The pull request has moved on. Start a new session or continue on the old head, then send.");
		case core::agent_send_refusal::agent_unknown:            return tr("%1 is no longer set up. Choose another agent, or add it again in Settings › Agents.").arg(agent);
		case core::agent_send_refusal::host_not_mapped:          return host_not_mapped_words();
		case core::agent_send_refusal::agent_forbidden_on_host:  return tr("%1 isn't allowed for this pull request's server. Choose an agent the server allows.").arg(agent);
		case core::agent_send_refusal::agent_not_required_one:   return tr("This pull request's server requires a different agent. Choose it in the agent menu.");
		case core::agent_send_refusal::session_profile_mismatch: return tr("This conversation is with another agent. Start a new session to ask %1.").arg(agent);
		case core::agent_send_refusal::selection_required:       return tr("Select the lines to ask about in the diff, then send.");
		case core::agent_send_refusal::unknown_action:           return tr("This question can't be asked here.");
		case core::agent_send_refusal::scope_invalid:            return tr("What this question is about is no longer in the review. Choose its context again.");
		case core::agent_send_refusal::private_context_refused:  return tr("The local comments or draft chosen belong to another conversation. Choose them again.");
		case core::agent_send_refusal::launch_refused:           return tr("%1 didn't start.").arg(agent);
		case core::agent_send_refusal::agent_unavailable:        return tr("%1 isn't available. Choose an agent in the agent menu, then send.").arg(agent);
		}
		return { };
	}

	namespace {
		/// The launch path's own refusals, which arrive as its stable names,
		/// said as the composer says the core's.
		[[nodiscard]] auto launch_refusal_of(const QString& aReason) -> std::optional<core::agent_send_refusal>
		{
			const auto has = [&aReason](const char* aName) { return aReason.contains(QLatin1String{ aName }); };
			if (has("already_running")) {
				return core::agent_send_refusal::request_in_flight;
			}
			if (has("profile_forbidden_on_host")) {
				return core::agent_send_refusal::agent_forbidden_on_host;
			}
			if (has("host_not_mapped")) {
				return core::agent_send_refusal::host_not_mapped;
			}
			if (has("profile_not_required_one")) {
				return core::agent_send_refusal::agent_not_required_one;
			}
			if (has("profile_unknown") || has("profile_invalid")) {
				return core::agent_send_refusal::agent_unknown;
			}
			if (has("selection_required")) {
				return core::agent_send_refusal::selection_required;
			}
			if (has("session_profile_mismatch")) {
				return core::agent_send_refusal::session_profile_mismatch;
			}
			return std::nullopt;
		}
	} // namespace

	void agent_tab_view::present_skills(std::vector<core::skill_entry> aSkills)
	{
		if (SkillCompletion != nullptr) {
			SkillCompletion->present(std::move(aSkills));
		}
	}

	void agent_tab_view::refuse_send(core::agent_send_refusal aRefusal, const QString& aDetail)
	{
		if (!panels_alive() || aRefusal == core::agent_send_refusal::none) {
			return;
		}
		// RFC-BPR-0117. An Explain this file refused says so on its card.
		const auto explained = !SendingExplain.isEmpty() ? SendingExplain : PendingExplain;
		PendingExplain.clear();
		// RFC-BPR-0118. So does a Suggest the change, in its panel.
		const auto suggested = !SendingSuggest.isEmpty() || !PendingSuggest.isEmpty();
		PendingSuggest.clear();
		// Refused: the question stays in the field, and no turn waits for it.
		PendingQuestion.reset();
		PendingAbout.reset();
		PendingDraftTarget.reset();
		PendingReview = false;
		auto detail   = aDetail.trimmed();
		if (aRefusal == core::agent_send_refusal::launch_refused && !detail.isEmpty()) {
			if (const auto known = launch_refusal_of(detail)) {
				aRefusal = *known;
				detail.clear();
			}
		}
		auto text = tr("Not sent. %1").arg(send_refusal_words(aRefusal, agent_name()));
		if (!detail.isEmpty()) {
			// The host's words, with its stable names made readable.
			detail.replace(QLatin1Char{ '_' }, QLatin1Char{ ' ' });
			text += QStringLiteral(" ") + tr("(%1)").arg(detail);
		}
		if (!explained.isEmpty() && ExplainObserver) {
			ExplainObserver(explain_file_progress{ .Path = explained, .Turn = Turns.size(), .Ending = text, .Answered = false });
		}
		if (suggested && SuggestObserver) {
			SuggestObserver(suggest_change_turn{ .Turn = Turns.size(), .Ended = true, .Answered = false, .Ending = text });
		}
		ShownRefusal = aRefusal;
		RefusalText->setText(text);
		RefusalText->setAccessibleName(text);
		Refusal->show();
		Refusal->update();
		auto alert = QAccessibleEvent{ RefusalText, QAccessible::Alert };
		QAccessible::updateAccessibility(&alert);
	}

	void agent_tab_view::hide_send_refusal()
	{
		ShownRefusal = core::agent_send_refusal::none;
		if (Refusal != nullptr) {
			Refusal->hide();
		}
	}

	auto agent_tab_view::send_refusal_text() const -> QString { return Refusal != nullptr && !Refusal->isHidden() ? RefusalText->text() : QString{ }; }

	void agent_tab_view::present_run_settings(std::vector<agent_run_settings> aSettings) { RunSettingsSource->present(std::move(aSettings)); }

	void agent_tab_view::set_run_settings_dispatch(agent_run_settings_dispatch aDispatch) { RunSettingsDispatch = std::move(aDispatch); }

	auto agent_tab_view::current_run_settings() const -> const agent_run_settings* { return RunSettingsSource->current(); }

	auto agent_tab_view::run_settings_text() const -> QString { return RunSettings != nullptr ? RunSettings->full_text() : QString{ }; }

	void agent_tab_view::adopt_run_settings_button(agent_run_settings_picker* aPicker) { RunSettings = aPicker; }

	void agent_tab_view::fit_run_settings()
	{
		// [[BPR-1924]], owner look on BPR-1914 v004 ("maybe the picker needs
		// to wrap to a 2nd row on the agent tab if the drawer is narrow"). The
		// picker stays beside Send while the row holds every control at its
		// whole width with a gap to spare; otherwise it takes a row of its own
		// under the tools, left-aligned, with its whole words. The choice
		// depends only on the field's width and the controls' own sizes, never
		// on where the picker is now, so moving it cannot flip it back.
		if (Field == nullptr || RunSettings == nullptr || RunSettings->parentWidget() != Field || AskRow == nullptr || Field->width() <= 0) {
			return;
		}
		const auto margins   = Field->layout()->contentsMargins();
		const auto available = Field->width() - margins.left() - margins.right();
		auto       needed    = 0;
		auto       count     = 0;
		for (auto* const widget : { static_cast<QWidget*>(Context), static_cast<QWidget*>(SuggestionsMenu), static_cast<QWidget*>(Chat.scope_selector()), static_cast<QWidget*>(RunSettings), static_cast<QWidget*>(Ask) }) {
			if (widget != nullptr && widget->isVisibleTo(Field)) {
				// Match the public layout contract: a preferred width must
				// include native minimum hints and explicit min/max bounds.
				needed += QWidgetItem{ widget }.sizeHint().width();
				++count;
			}
		}
		// The scope-to-picker gap includes its ordinary layout spacing.
		const auto rowSpacing = AskRow->spacing();
		needed += rowSpacing * std::max(0, count - 1) + std::max(0, run_settings_gap - rowSpacing);
		const auto wrap = RunSettings->isVisibleTo(Field) && needed > available;
		if (wrap == RunSettingsWrapped) {
			return;
		}
		RunSettingsWrapped = wrap;
		if (wrap) {
			AskRow->removeWidget(RunSettings);
			PickerRow->insertWidget(0, RunSettings);
		} else {
			PickerRow->removeWidget(RunSettings);
			AskRow->insertWidget(AskRow->indexOf(Ask), RunSettings);
		}
	}

	void agent_tab_view::present_run_settings_button() { RunSettingsSource->refresh(); }

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
			if (RunSettings != nullptr) {
				RunSettings->showMenu();
			}
			Notice->setText(isModel ? tr("Choose a model, or type /model NAME.") : tr("Choose an effort, or type /effort LEVEL."));
			return true;
		}
		auto value = words.at(1).toStdString();
		if (value == "default") {
			value.clear();
		}
		// An effort is one the harness offers, as its menu lists them.
		const auto offered = isModel || value.empty() || settings->Efforts.empty() || std::ranges::contains(settings->Efforts, value);
		const auto taken   = offered && RunSettingsDispatch && (isModel ? RunSettingsDispatch(settings->Profile, value, std::nullopt) : RunSettingsDispatch(settings->Profile, std::nullopt, value));
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
		// [[BPR-1999]], RFC-BPR-0101. Return sends and Shift+Return starts a
		// new line, as the tour side chat does; Ctrl or Cmd with Return also
		// sends. Not while an input method composes, not in an empty field,
		// and not while the skill completion's list takes Return.
		if (aWatched == Chat.composer() && aEvent->type() == QEvent::InputMethod) {
			Composing = !static_cast<QInputMethodEvent*>(aEvent)->preeditString().isEmpty();
		}
		if (aEvent->type() == QEvent::KeyPress && aWatched == Chat.composer()) {
			const auto* const key       = static_cast<QKeyEvent*>(aEvent);
			const auto        modifiers = key->modifiers() & ~Qt::KeypadModifier;
			if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) && !modifiers.testFlag(Qt::ShiftModifier) && !modifiers.testFlag(Qt::AltModifier)) {
				if (Composing || (SkillCompletion != nullptr && SkillCompletion->popup() != nullptr && SkillCompletion->popup()->isVisible())) {
					return false;
				}
				// Plain Return in an empty field does nothing; Ctrl or Cmd
				// with Return asks, as before, and is told why nothing went.
				if (Chat.composer()->toPlainText().trimmed().isEmpty() && !modifiers.testAnyFlags(Qt::ControlModifier | Qt::MetaModifier)) {
					return true;
				}
				// Cmd+Return (Ctrl+Return off macOS): the other follow-up for
				// this one message.
				Opposite = modifiers.testAnyFlags(Qt::ControlModifier | Qt::MetaModifier);
				ask();
				Opposite = false;
				return true;
			}
		}
		// A resize, or a control in the field changing its size (the
		// picker's words, the scope's, the picker shown or hidden).
		if ((aEvent->type() == QEvent::Resize || aEvent->type() == QEvent::LayoutRequest) && aWatched == Field) {
			fit_run_settings();
			return false;
		}
		// [[BPR-2059]]. A card's Markdown text takes the height its new width needs.
		if (aEvent->type() == QEvent::Resize && aWatched->property("agentCardText").toBool()) {
			if (auto* const view = dynamic_cast<markdown_body_view*>(aWatched); view != nullptr && static_cast<QResizeEvent*>(aEvent)->size().width() != static_cast<QResizeEvent*>(aEvent)->oldSize().width()) {
				fit_card_text(*view);
			}
			return false;
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

	void agent_tab_view::look_for_agents()
	{
		if (FirstShown) {
			const auto observer = std::exchange(FirstShown, { });
			observer();
		}
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
			repaint_glyphs();
			rebuild_findings();
		}
	}

	void agent_tab_view::repaint_glyphs()
	{
		// [[BPR-1773]], owner look 2026-10-27 ("The copy to clipboard icon is
		// nearly unseeable on dark mode"). A glyph is painted in the ink of
		// the palette it was made with, so a theme change paints each again.
		History->setIcon(drawer_icon(drawer_glyph::history, palette()));
		ClearSession->setIcon(drawer_icon(drawer_glyph::new_session, palette()));
		ConversationClose->setIcon(drawer_icon(drawer_glyph::close, palette()));
		LinesClose->setIcon(drawer_icon(drawer_glyph::close, palette()));
		Context->setIcon(drawer_icon(drawer_glyph::plus, palette()));
		SuggestionsMenu->setIcon(drawer_icon(drawer_glyph::agent, palette()));
		if (auto* const introIcon = EmptyIntro->findChild<QLabel*>(QStringLiteral("agentEmptyIcon"))) {
			introIcon->setPixmap(drawer_icon(drawer_glyph::agent, palette()).pixmap(32, 32));
		}
		const auto copyIcon = drawer_icon(drawer_glyph::copy, palette());
		for (const auto& kept : Turns) {
			if (kept.Copy != nullptr) {
				kept.Copy->setIcon(copyIcon);
			}
		}
		// The send arrow, in the accent; while working the button says Stop
		// instead, in the ordinary roles.
		if (Ask->text().isEmpty()) {
			wear_agent_accent(*Ask, palette());
			// The prompt turns' glyphs follow the palette too.
			for (auto* const glyph : findChildren<QLabel*>(QStringLiteral("agentTurnPromptGlyph"))) {
				glyph->setPixmap(agent_glyph(palette()).pixmap(QSize{ 14, 14 }));
			}
			Ask->setIcon(drawer_icon(drawer_glyph::send, Ask->palette()));
		}
		for (const auto& kept : Turns) {
			if (kept.Recovery != nullptr && kept.RecoveryKind == core::agent_turn_recovery::resend) {
				wear_agent_accent(*kept.Recovery, palette());
			}
		}
		fit_run_settings();
	}
} // namespace bpr::qt
