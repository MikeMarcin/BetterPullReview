#include "bpr/qt/review_tour_editor.hpp"

#include "bpr/qt/compose_review_sheet.hpp"
#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/resize_handle_cursor.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/type_preview.hpp"
#include "bpr/qt/workbench_window.hpp"

#include "bpr/core/review_comment_range.hpp"

#include "conversation_glyphs.hpp"
#if defined(Q_OS_MACOS)
	#include "insights_menu_mac.hpp"
#endif

#include "bpr/core/review_publication_policy.hpp"
#include "bpr/core/suggestion_draft.hpp"
#include "github_presentation_text.hpp"

#include "bpr/qt/github_conversation_model.hpp"

#include "bpr/qt/menu_tool_button.hpp"

#include "bpr/core/file_progress_mark.hpp"

#include "bpr/qt/agent_tab_view.hpp"
#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/commit_filter_view.hpp"
#include "bpr/qt/destructive_question.hpp"
#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/dif_runtime.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/drawer_parts.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/feedback_label_controls.hpp"
#include "bpr/qt/feedback_label_settings.hpp"
#include "bpr/qt/finding_queue_view.hpp"
#include "bpr/qt/generated_files_view.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/home_row_delegate.hpp"
#include "bpr/qt/info_glyph.hpp"
#include "bpr/qt/keyboard_shortcuts.hpp"
#include "bpr/qt/keyboard_shortcuts_window.hpp"
#include "bpr/qt/native_window_chrome.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/pending_veil.hpp"
#include "bpr/qt/private_notes_panel.hpp"
#include "bpr/qt/private_review_panel.hpp"
#include "bpr/qt/profile_management_window.hpp"
#include "bpr/qt/progress_mark.hpp"
#include "bpr/qt/refresh_note.hpp"
#include "bpr/qt/settings_window.hpp"
#include "bpr/qt/side_chat_panel.hpp"
#include "bpr/qt/usage_ring.hpp"
#include <QTabWidget>

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAccessible>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFont>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QIcon>
#include <QInputMethodQueryEvent>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QLocale>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMoveEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPen>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRectF>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QShortcut>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QStringList>
#include <QStyle>
#include <QStyleOption>
#include <QTabBar>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextEdit>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>
#include <QWidget>
#include <QWindow>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <string_view>
#include <system_error>
#include <utility>

namespace bpr::qt {
	namespace {
		/// [[BPR-1575]]. What a refresh compares of one accepted read; defined below.
		[[nodiscard]] auto refresh_observation_of(const application::github_pull_request_projection& aPullRequest) -> core::pull_request_refresh_observation;
	} // namespace

	namespace {
		/// \brief [[BPR-1592]] [[BPR-1684]]. The tray's tabs, each as wide
		/// as what it shows.
		///
		/// \details A tab is the style's own text tab for its name, plus its
		/// glyph and a gap when it shows one, plus its count when the count is
		/// showing. The style draws each tab's shape and its name; the bar
		/// places them, the glyph and the name as one group centred in the
		/// tab, or ending a gap before the count. Left to the style, the macOS
		/// style centres a tab's name and keeps its icon's width free on both
		/// sides, which made the Agent tab about 30 pixels wider than its spark
		/// and its name; the styles also kept room for the Comments count while
		/// it was hidden, and set a glyph-only tab's glyph off centre.
		class tray_tab_bar final : public QTabBar
		{
		public:
			using QTabBar::QTabBar;

			/// \brief The width tab `aIndex` takes showing `aText` and, when
			/// `aGlyph`, its glyph; its count is counted while it is showing.
			[[nodiscard]] auto face_width(const int aIndex, const QString& aText, const bool aGlyph) const -> int
			{
				auto option = QStyleOptionTab{ };
				initStyleOption(&option, aIndex);
				option.text            = aText;
				option.icon            = QIcon{ };
				option.leftButtonSize  = QSize{ };
				option.rightButtonSize = QSize{ };
				const auto metrics     = fontMetrics();
				const auto text        = metrics.size(Qt::TextShowMnemonic, aText);
				const auto contents    = QSize{ text.width() + style()->pixelMetric(QStyle::PM_TabBarTabHSpace, &option, this),
												std::max(metrics.height(), iconSize().height()) + style()->pixelMetric(QStyle::PM_TabBarTabVSpace, &option, this) };
				auto       width       = style()->sizeFromContents(QStyle::CT_TabBarTab, &option, contents, this).width();
				if (aGlyph) {
					width += iconSize().width() + (aText.isEmpty() ? 0 : glyph_gap);
				}
				if (const auto* const count = tabButton(aIndex, QTabBar::RightSide); count != nullptr && !count->isHidden()) {
					width += glyph_gap + count->sizeHint().width();
				}
				return width;
			}

		protected:
			[[nodiscard]] auto tabSizeHint(const int aIndex) const -> QSize override
			{
				auto size = QTabBar::tabSizeHint(aIndex);
				size.setWidth(face_width(aIndex, tabText(aIndex), !tabIcon(aIndex).isNull()));
				return size;
			}

			/// A tab never elides: the window chooses a face that fits.
			[[nodiscard]] auto minimumTabSizeHint(const int aIndex) const -> QSize override
			{
				return tabSizeHint(aIndex);
			}

			/// \brief QTabBar's own painting (its base, then each tab with the
			/// open one last) with each label placed by the bar.
			void paintEvent(QPaintEvent* const aEvent) override
			{
				static_cast<void>(aEvent);
				auto painter = QPainter{ this };
				auto base    = QStyleOptionTabBarBase{ };
				base.initFrom(this);
				base.shape        = shape();
				base.documentMode = documentMode();
				auto overlap      = QStyleOptionTab{ };
				overlap.shape     = shape();
				if (const auto lap = style()->pixelMetric(QStyle::PM_TabBarBaseOverlap, &overlap, this); lap > 0) {
					base.rect = QRect{ 0, height() - lap, width(), lap };
				}
				for (auto index = 0; index < count(); ++index) {
					base.tabBarRect |= tabRect(index);
				}
				base.selectedTabRect = tabRect(currentIndex());
				if (drawBase()) {
					style()->drawPrimitive(QStyle::PE_FrameTabBarBase, &base, &painter, this);
				}
				for (auto index = 0; index < count(); ++index) {
					if (index != currentIndex()) {
						paint_tab(painter, index);
					}
				}
				if (currentIndex() >= 0) {
					paint_tab(painter, currentIndex());
				}
			}

		private:
			void paint_tab(QPainter& aPainter, const int aIndex) const
			{
				auto option = QStyleOptionTab{ };
				initStyleOption(&option, aIndex);
				if (!(option.state & QStyle::State_Enabled)) {
					option.palette.setCurrentColorGroup(QPalette::Disabled);
				}
				style()->drawControl(QStyle::CE_TabBarTabShape, &option, &aPainter, this);
				// The name, whole, drawn by the style as a plain text tab whose
				// centre is the name's place in the group.
				const auto text       = tabText(aIndex);
				const auto glyph      = !option.icon.isNull();
				auto       label      = option;
				label.text            = text;
				label.icon            = QIcon{ };
				label.leftButtonSize  = QSize{ };
				label.rightButtonSize = QSize{ };
				const auto textWidth  = text.isEmpty() ? 0 : fontMetrics().size(Qt::TextShowMnemonic, text).width();
				const auto group      = (glyph ? iconSize().width() + (text.isEmpty() ? 0 : glyph_gap) : 0) + textWidth;
				auto       left       = option.rect.left() + (option.rect.width() - group) / 2;
				if (const auto* const count = tabButton(aIndex, QTabBar::RightSide); count != nullptr && !count->isHidden()) {
					left = count->geometry().left() - glyph_gap - group;
				}
				const auto textLeft = left + group - textWidth;
				label.rect.translate(textLeft + textWidth / 2 - option.rect.center().x(), 0);
				style()->drawControl(QStyle::CE_TabBarTabLabel, &label, &aPainter, this);
				if (glyph) {
					const auto middle = style()->subElementRect(QStyle::SE_TabBarTabText, &label, this).center().y();
					const auto place  = QRect{ QPoint{ left, middle - iconSize().height() / 2 }, iconSize() };
					option.icon.paint(&aPainter, place, Qt::AlignCenter, (option.state & QStyle::State_Enabled) ? QIcon::Normal : QIcon::Disabled,
									  (option.state & QStyle::State_Selected) ? QIcon::On : QIcon::Off);
				}
			}

			/// The gap QTabBar and the styles leave between a glyph, a name
			/// and a tab button.
			static constexpr int glyph_gap = 4;
		};

		[[nodiscard]] auto review_position_text_width(const QFontMetrics& aMetrics, const QString& aName, const bool aShown, const bool aCompact) -> int
		{
			// Native glyph advances can be fractional. Rounding down makes
			// elidedText shorten even the group name that the slot must retain.
			const auto precise = QFontMetricsF{ aMetrics };
			const auto width   = [&precise](const QString& aText) {
				return static_cast<int>(std::ceil(precise.horizontalAdvance(aText)));
			};
			auto result = width(aName);
			if (!aShown) {
				return result;
			}
			for (auto digit = '0'; digit <= '9'; ++digit) {
				const auto number = QString(aCompact ? 3 : 4, QLatin1Char{ digit });
				const auto sample = aCompact ? QStringLiteral("%1 %2/%2").arg(aName, number)
											 : QStringLiteral("%1 %2 of %2").arg(aName, number);
				result            = std::max(result, width(sample));
			}
			return result;
		}

		/// \brief [[BPR-1331]]. The mark the private progress button shows: the
		/// core's decision for the selected file's receipt, or none while no
		/// file's progress is known.
		[[nodiscard]] auto shown_progress_mark(const std::optional<application::file_receipt_action_projection>& aReceipt,
											   const bool                                                        aLedgerUnavailable) -> std::optional<core::file_progress_mark>
		{
			if (aLedgerUnavailable || !aReceipt.has_value()) {
				return std::nullopt;
			}
			return core::progress_mark(aReceipt->Receipt);
		}

		/// \brief [[BPR-1212]]. The code a maximized draft is on, above
		/// Discuss's composer: the draft's lines under a blue bracket and three
		/// lines either side, exactly as the head holds them.
		class draft_excerpt_view final : public QWidget
		{
		public:
			explicit draft_excerpt_view(QWidget* aParent)
				: QWidget{ aParent }
			{
				setObjectName(QStringLiteral("discussDraftExcerpt"));
				setAccessibleName(QStringLiteral("The code the draft is on"));
				setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
				hide();
			}

			void present(const QString& aPath, const std::uint32_t aFirstNumber, std::vector<QString> aLines,
						 const std::uint32_t aBracketFirst, const std::uint32_t aBracketLast, const QFont& aCode)
			{
				Path         = aPath;
				FirstNumber  = aFirstNumber;
				Lines        = std::move(aLines);
				BracketFirst = aBracketFirst;
				BracketLast  = aBracketLast;
				Code         = aCode;
				setAccessibleDescription(QStringLiteral("%1, %2").arg(Path, words()));
				setVisible(!Lines.empty());
				updateGeometry();
				update();
			}

			void clear()
			{
				Lines.clear();
				hide();
			}

			[[nodiscard]] auto words() const -> QString
			{
				return BracketFirst == BracketLast ? QStringLiteral("Line %1").arg(BracketFirst) : QStringLiteral("Lines %1–%2").arg(BracketFirst).arg(BracketLast);
			}

			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				return { 320, header() + static_cast<int>(Lines.size()) * row() + 2 };
			}

			[[nodiscard]] auto minimumSizeHint() const -> QSize override
			{
				return { 120, sizeHint().height() };
			}

		protected:
			void paintEvent(QPaintEvent* /*aEvent*/) override
			{
				auto       painter = QPainter{ this };
				const auto base    = palette().color(QPalette::Active, QPalette::Base);
				const auto ink     = palette().color(QPalette::Active, QPalette::Text);
				const auto faint   = palette().color(QPalette::Active, QPalette::PlaceholderText);
				const auto signal  = palette().color(QPalette::Active, QPalette::Highlight);
				auto       rule    = ink;
				rule.setAlphaF(0.16F);
				painter.setRenderHint(QPainter::Antialiasing, true);
				auto clip = QPainterPath{ };
				clip.addRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), 8.0, 8.0);
				painter.setPen(QPen{ rule, 1.0 });
				painter.setBrush(base);
				painter.drawPath(clip);
				painter.setClipPath(clip);
				auto bold = font();
				bold.setWeight(QFont::DemiBold);
				painter.setFont(bold);
				painter.setPen(ink);
				const auto title = QStringLiteral("%1   %2").arg(Path.section(QLatin1Char{ '/' }, -1), words());
				painter.drawText(QRect{ 10, 0, width() - 20, header() }, Qt::AlignLeft | Qt::AlignVCenter,
								 QFontMetrics{ bold }.elidedText(title, Qt::ElideMiddle, std::max(0, width() - 20)));
				painter.setPen(QPen{ rule, 1.0 });
				painter.drawLine(QPointF{ 0.0, header() - 0.5 }, QPointF{ static_cast<qreal>(width()), header() - 0.5 });
				painter.setFont(Code);
				const auto metrics = QFontMetrics{ Code };
				const auto gutter  = metrics.horizontalAdvance(QStringLiteral("00000")) + 12;
				auto       wash    = signal;
				wash.setAlphaF(0.14F);
				for (auto index = std::size_t{ 0U }; index < Lines.size(); ++index) {
					const auto number = FirstNumber + static_cast<std::uint32_t>(index);
					const auto top    = header() + static_cast<int>(index) * row();
					const auto inside = number >= BracketFirst && number <= BracketLast;
					if (inside) {
						painter.fillRect(QRect{ 0, top, width(), row() }, wash);
						painter.fillRect(QRect{ gutter - 3, top, 3, row() }, signal);
					}
					painter.setPen(inside ? signal : faint);
					painter.drawText(QRect{ 0, top, gutter - 8, row() }, Qt::AlignRight | Qt::AlignVCenter, QString::number(number));
					auto text = Lines[index];
					text.replace(QLatin1Char{ '\t' }, QStringLiteral("    "));
					painter.setPen(ink);
					painter.drawText(QRect{ gutter + 8, top, width() - gutter - 16, row() }, Qt::AlignLeft | Qt::AlignVCenter,
									 metrics.elidedText(text, Qt::ElideRight, std::max(0, width() - gutter - 16)));
				}
			}

		private:
			[[nodiscard]] auto header() const -> int
			{
				return fontMetrics().height() + 12;
			}

			[[nodiscard]] auto row() const -> int
			{
				return QFontMetrics{ Code }.lineSpacing() + 4;
			}

			QString              Path;
			std::uint32_t        FirstNumber = 1U;
			std::vector<QString> Lines;
			std::uint32_t        BracketFirst = 1U;
			std::uint32_t        BracketLast  = 1U;
			QFont                Code;
		};

#if defined(_WIN32)
		// [[BPR-0653]]. On Windows the strip also carries the menu control and
		// the three 46-pixel caption buttons. With the modality segments at their
		// three-character prefixes every control is top level from 902 pixels,
		// and the owner chose that floor on 2026-09-17.
		inline constexpr int platform_floor_width = 902;
#else
		inline constexpr int platform_floor_width = 900;
#endif
		/// \brief [[BPR-0775]]. Home's bar: its label, the field and the shortcut
		/// inside one bordered field with one 8-point radius.
		///
		/// \details Painted from the palette rather than a stylesheet. The rule
		/// takes Highlight while the field has focus, so the focus ring is the
		/// frame's rather than a second ring inside it.
		class home_bar_frame final : public QWidget
		{
		public:
			explicit home_bar_frame(QWidget* const aParent)
				: QWidget{ aParent }
			{
			}

			void watch(QWidget* const aField)
			{
				Field = aField;
				aField->installEventFilter(this);
			}

		protected:
			void paintEvent(QPaintEvent* const aEvent) override
			{
				static_cast<void>(aEvent);
				auto       painter = QPainter{ this };
				const auto focused = Field != nullptr && Field->hasFocus();
				painter.setRenderHint(QPainter::Antialiasing, true);
				painter.setPen(QPen{ palette().color(QPalette::Active, focused ? QPalette::Highlight : QPalette::Mid), focused ? 2.0 : 1.0 });
				painter.setBrush(palette().color(QPalette::Active, QPalette::Base));
				painter.drawRoundedRect(QRectF{ rect() }.adjusted(1.0, 1.0, -1.0, -1.0), 8.0, 8.0);
			}

			auto eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool override
			{
				if (aWatched == Field && aEvent != nullptr
					&& (aEvent->type() == QEvent::FocusIn || aEvent->type() == QEvent::FocusOut)) {
					update();
				}
				return QWidget::eventFilter(aWatched, aEvent);
			}

		private:
			QPointer<QWidget> Field;
		};

		/// \brief [[BPR-1381]]. The bar's own word that what it was given is
		/// opening: the rows' turning mark and "Opening…" where the shortcut
		/// stood, in the field's quiet ink.
		///
		/// \details What was asked is answered where it was asked, as a row
		/// answers on its row; the status band at Home's foot says the rest.
		class home_bar_activity final : public QWidget
		{
		public:
			explicit home_bar_activity(QWidget* const aParent)
				: QWidget{ aParent }
			{
				setObjectName(QStringLiteral("homeOpenActivity"));
				setAccessibleName(QStringLiteral("Opening"));
				setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
				Turn = new QTimer{ this };
				Turn->setInterval(80);
				connect(Turn, &QTimer::timeout, this, [this] {
					Frame = (Frame + 1) % home_open_frames;
					update();
				});
				hide();
			}

			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				const auto metrics = QFontMetrics{ font() };
				return { metrics.height() + gap + metrics.horizontalAdvance(text()), metrics.height() };
			}

		protected:
			void paintEvent(QPaintEvent* const aEvent) override
			{
				static_cast<void>(aEvent);
				auto       painter = QPainter{ this };
				const auto extent  = QFontMetrics{ font() }.height();
				const auto ink     = palette().color(QPalette::Active, QPalette::PlaceholderText);
				auto       faint   = ink;
				faint.setAlphaF(0.25F);
				painter.setRenderHint(QPainter::Antialiasing, true);
				draw_home_open_mark(painter, QRect{ 0, (height() - extent) / 2, extent, extent }, Frame, ink, faint);
				painter.setPen(ink);
				painter.drawText(QRect{ extent + gap, 0, width() - extent - gap, height() }, Qt::AlignVCenter | Qt::AlignLeft, text());
			}

			void showEvent(QShowEvent* const aEvent) override
			{
				QWidget::showEvent(aEvent);
				Turn->start();
			}

			void hideEvent(QHideEvent* const aEvent) override
			{
				QWidget::hideEvent(aEvent);
				Turn->stop();
			}

		private:
			static constexpr int gap = 6;

			[[nodiscard]] static auto text() -> QString
			{
				return QStringLiteral("Opening…");
			}

			QTimer* Turn  = nullptr;
			int     Frame = 0;
		};

		/// \brief [[BPR-1200]]. A read's first two answered pages are its
		/// repository and its pull request: GitHub has shown both to this
		/// connection, so the read may leave Home for Overview.
		constexpr std::uint32_t minimum_answered_pages = 2U;

		/// \brief [[BPR-1134]]. The right tray's tabs, in order.
		constexpr int tray_comments_tab = 0;
		/// [[BPR-1212]]. The conversations narrow to a list beside Discuss's composer.
		constexpr int discuss_composer_list_width = 340;
		/// [[BPR-1592]]. Notes holds only private notes; the agent's material
		/// is the provisional Agent tab's, after Tour, until [[BPR-1594]].
		constexpr int tray_notes_tab    = 1;
		constexpr int tray_tour_tab     = 2;
		constexpr int tray_findings_tab = 3;

		/// \brief [[BPR-0775]]. Dif's resting size in the bottom-left corner, in logical pixels.
		///
		/// \details [[BPR-1209]]. Hand-back 49 asks for a smaller corner presence.
		/// One status-row height leaves the first-launch Home and workshop prime
		/// slots unchanged while reducing the corner illustration by thirty percent.
		constexpr auto dif_corner_size = 28;
		/// [[BPR-1212]]. How long Add comment waits for the draft of a line
		/// selected just now before the status line says why none came.
		constexpr auto line_comment_wait_ms = 1500;

		/// \brief [[BPR-1347]]. Dif's size at the start of the comment
		/// workshop's row, in logical pixels.
		///
		/// \details The owner found the 64-pixel slot left "way too much
		/// deadspace around him" over Discuss (BPR-1091 v002). He now opens the
		/// workshop's row at the corner's size, and the row is that tall with
		/// or without him.
		constexpr auto dif_workshop_size = dif_corner_size;

		// The toolbar's first entries are the file actions and the rest are the
		// change actions, in the core's own command order.
		constexpr std::size_t file_navigation_entries = 2U;

		// The navigation entries are exactly the core's navigation commands in the
		// core's order, so entry `i` reads `workbench_command_id{ i }` and the
		// window needs no second mapping between the two.
		static_assert(
			static_cast<std::size_t>(core::workbench_command_id::previous_file) == 0U);
		static_assert(
			static_cast<std::size_t>(core::workbench_command_id::next_file) == 1U);
		static_assert(
			static_cast<std::size_t>(core::workbench_command_id::previous_change)
			== file_navigation_entries);
		static_assert(
			static_cast<std::size_t>(core::workbench_command_id::next_change) == 3U);
		static_assert(
			static_cast<std::size_t>(application::file_receipt_action_id::seen)
			== 0U);
		static_assert(
			static_cast<std::size_t>(application::file_receipt_action_id::excluded)
			== 3U);

		/// \brief The width the conversations drawer opens at, in logical pixels.
		///
		/// \details Stated here because the alternative is what the drawer had
		/// before: whichever row was widest set the column's size hint, the
		/// splitter handed the drawer exactly that, and then the vertical scroll
		/// bar took 18 px back out of it. So the widest row was always the row
		/// that lost its last control, at whatever width the drawer opened -
		/// 540 px while the defect rows were horizontal, 366 px once BPR-0520
		/// stacked them and the agent launcher's run controls became the widest
		/// row instead. A number nobody chose, that moved whenever a label did,
		/// and that was wrong by the scroll bar's width every time.
		///
		/// Measured at 100 % with every row of the drawer stacked (BPR-0506,
		/// BPR-0520, BPR-0527): the column asks for 357 px to draw every control
		/// at its natural width, needs 251 px for its widest single control -
		/// *Classify selected conversation as defect* - below which it starts
		/// losing controls again, and the vertical scroll bar takes 18 px that
		/// the column never gets. 357 plus 18 is this. A style or system font
		/// whose metrics run wider than this machine's - the headless Linux gate
		/// uses a different style from macOS - squeezes the column toward the
		/// 251 it needs rather than clipping it, which is why the number that is
		/// stated is the one the column asks for and not the one it needs.
		///
		/// It is the width the product opens at, not a floor on the drawer.
		/// [[BPR-0728]]: the drawer's content stops at
		/// `minimum_presented_drawer_width` like the other drawers, so a
		/// reviewer can drag it narrower and the host persists what they chose.
		/// What holds at those narrower widths is that every row is a column, so
		/// a row cannot put its last control out of reach.
		constexpr int conversation_drawer_default_width = 375;
		/// \brief [[BPR-1290]]. The narrowest and widest the conversations
		/// drawer drags to: the inline drawer prototype's range, which the
		/// owner kept ("I like it", 2026-10-06). This supersedes [[BPR-0728]]'s
		/// 160 pixel floor for this drawer.
		constexpr int conversation_drawer_minimum_width = 280;
		constexpr int conversation_drawer_maximum_width = 760;
		/// \brief Narrowest width a drawer is ever presented at when expanded.
		///
		/// \details [[BPR-0662]]. The host's floor is 64 pixels, the least it
		/// will retain; a persisted width beneath this one was not chosen by a
		/// reviewer but written by a squeeze (a hidden splitter laid out at
		/// its default size wrote 64 for every drawer), so the window grants
		/// the drawer's design width instead and the next drag persists a
		/// real one.
		constexpr std::uint32_t minimum_presented_drawer_width = 160U;
		/// \brief [[BPR-1129]]. The least of the title worth showing after the
		/// number; with less room the name drops the title rather than show a
		/// sliver of it.
		constexpr int identity_title_minimum = 80;
		/// \brief Least width the repository keeps beside the number when elided.
		constexpr int identity_fragment_width = 24;
		/// \brief [[BPR-1328]]. How long a press at a navigation bound says why
		/// it did nothing.
		constexpr int bounded_navigation_message_ms = 3000;

		[[nodiscard]] auto reason_text(const core::workbench_reason_code aReason)
			-> QString
		{
			switch (aReason) {
			case core::workbench_reason_code::none:
				return { };
			case core::workbench_reason_code::no_active_workbench:
				return QStringLiteral("Open a pull request to review its files.");
			case core::workbench_reason_code::no_actionable_files:
				return QStringLiteral("This pull request has no actionable files.");
			// [[BPR-1328]]. The navigation arrows stay pressable at their
			// bounds; these say why a press there did nothing.
			case core::workbench_reason_code::at_first_file:
				return QStringLiteral("First file in this pull request.");
			case core::workbench_reason_code::at_last_file:
				return QStringLiteral("Last file in this pull request.");
			case core::workbench_reason_code::invalid_file_identity:
				return QStringLiteral("The selected file identity is invalid.");
			case core::workbench_reason_code::invalid_navigation_cursor:
				return QStringLiteral("The file navigation position is invalid.");
			case core::workbench_reason_code::no_open_comparison:
				return QStringLiteral("No file comparison is open.");
			case core::workbench_reason_code::no_changes:
				return QStringLiteral(
					"This comparison has no changes to navigate.");
			case core::workbench_reason_code::at_first_change:
				return QStringLiteral("First change in this file.");
			case core::workbench_reason_code::at_last_change:
				return QStringLiteral("Last change in this file.");
			case core::workbench_reason_code::invalid_change_selection:
				return QStringLiteral("The selected change is no longer available.");
			case core::workbench_reason_code::invalid_change_cursor:
				return QStringLiteral("The change navigation position is invalid.");
			case core::workbench_reason_code::overview_keeps_drawers_collapsed:
				return QStringLiteral(
					"Overview keeps every drawer collapsed. Choose Review or "
					"Discuss to open one.");
			case core::workbench_reason_code::discuss_shows_the_conversation:
				return QStringLiteral(
					"In Discuss the thread is the page. Choose Review to "
					"hide or show the drawers.");
			case core::workbench_reason_code::threads_finds_a_thread:
				return QStringLiteral(
					"In Threads the finder is the page. Choose Review to "
					"hide or show the drawers.");
			case core::workbench_reason_code::header_source_requires_review:
				return QStringLiteral("Choose Review to go to a matching header or source file.");
			case core::workbench_reason_code::header_source_no_selection:
				return QStringLiteral("Select a header or source file first.");
			case core::workbench_reason_code::header_source_unsupported_file:
				return QStringLiteral("This file has no supported header or source extension.");
			case core::workbench_reason_code::header_source_unavailable_catalogue:
				return QStringLiteral("The changed-file catalogue is unavailable. Refresh the pull request.");
			}
			// Deliberately outside the switch and with no default label, so a
			// reason the core adds later is a compile error here rather than a
			// silently generic sentence in the toolbar.
			return QStringLiteral("Review navigation is unavailable.");
		}

		// The drawer commands share the workbench reason vocabulary, so only the
		// reason a drawer can actually be refused for needs drawer wording.
		[[nodiscard]] auto drawer_reason_text(
			const core::workbench_reason_code aReason) -> QString
		{
			if (aReason == core::workbench_reason_code::no_active_workbench) {
				return QStringLiteral(
					"Open a pull request to use its review drawers.");
			}
			return reason_text(aReason);
		}

		// The change commands share the workbench reason vocabulary, so only the
		// reason whose shared wording is file-flavoured needs a change sentence.
		[[nodiscard]] auto change_reason_text(
			const core::workbench_reason_code aReason) -> QString
		{
			if (aReason == core::workbench_reason_code::no_active_workbench) {
				return QStringLiteral(
					"Open a pull request to review its changes.");
			}
			return reason_text(aReason);
		}

		// [[BPR-0733]]. One recently viewed row. The pull request is named by
		// its locator and, where a snapshot was seen, its title; a kept
		// reading is said in words, so nothing here depends on colour. The
		// host and the instant it was last viewed are the row's tooltip and
		// its accessible description, which leaves the row itself one
		// readable line at every scale. The instant is the host's own
		// reading, not a clock this window consulted.
		[[nodiscard]] auto recent_review_item(
			const application::recent_review_entry& aEntry) -> QListWidgetItem*
		{
			const auto identity = QStringLiteral("%1/%2#%3")
									  .arg(QString::fromStdString(aEntry.Destination.Repository.Owner),
										   QString::fromStdString(aEntry.Destination.Repository.Name))
									  .arg(aEntry.Destination.Number.value());
			auto       text     = identity;
			if (!aEntry.Title.empty()) {
				text += QStringLiteral(" — ") + QString::fromStdString(aEntry.Title);
			}
			if (aEntry.Resumable) {
				text += QStringLiteral(" · saved reading");
			}
			auto       detail = QString::fromStdString(aEntry.Destination.Hostname);
			const auto viewed = aEntry.ViewedAt != 0
				? QLocale{ }.toString(QDateTime::fromSecsSinceEpoch(aEntry.ViewedAt).toLocalTime(), QLocale::ShortFormat)
				: QString{ };
			if (!viewed.isEmpty()) {
				detail += QStringLiteral(" · viewed ") + viewed;
			}
			auto* const item = new QListWidgetItem{ text };
			item->setData(home_row_role::draft, aEntry.Draft.value_or(false));
			item->setData(Qt::AccessibleTextRole, aEntry.Draft.value_or(false) ? QStringLiteral("Draft pull request; %1").arg(text) : text);
			item->setToolTip(detail);
			// [[BPR-1365]]. The review this window has open is said in words, as
			// its tag says it on the row.
			item->setData(Qt::AccessibleDescriptionRole,
						  aEntry.Open ? QStringLiteral("%1; %2").arg(home_reviewing_description(), detail) : detail);
			item->setData(home_row_role::reviewing, aEntry.Open);
			// [[BPR-0776]]. The queue row's anatomy. A recent row has no state, so
			// no reason word and no colour; its time is the host's reading of
			// when it was viewed, and it offers Resume only where a reading was
			// kept.
			item->setData(home_row_role::identity, identity);
			item->setData(home_row_role::title, QString::fromStdString(aEntry.Title));
			item->setData(home_row_role::why, aEntry.Resumable ? QStringLiteral("My reading is saved.") : QStringLiteral("Viewed."));
			item->setData(home_row_role::state, -1);
			item->setData(home_row_role::time, viewed);
			item->setData(home_row_role::row_action, aEntry.Resumable ? QStringLiteral("Resume") : QString{ });
			return item;
		}

		[[nodiscard]] auto failure_text(
			const application::workbench_host_failure aFailure) -> QString
		{
			switch (aFailure) {
			case application::workbench_host_failure::observation_unavailable:
				return QStringLiteral("The actionable file list is unavailable.");
			case application::workbench_host_failure::observation_limit:
				return QStringLiteral(
					"This pull request has more actionable files than the workbench "
					"retains.");
			case application::workbench_host_failure::settings_unavailable:
				return QStringLiteral("The saved workbench layout is unavailable.");
			case application::workbench_host_failure::selected_file_binary:
				return QStringLiteral("The selected file is binary.");
			case application::workbench_host_failure::selected_file_missing_left:
				return QStringLiteral("Comparison unavailable: the earlier selected revision has no file at this path. Pull-request status may describe a different comparison.");
			case application::workbench_host_failure::selected_file_missing_right:
				return QStringLiteral("Comparison unavailable: the later selected revision has no file at this path. Pull-request status may describe a different comparison.");
			case application::workbench_host_failure::selected_file_missing_both:
				return QStringLiteral("The selected file is absent from both revisions.");
			case application::workbench_host_failure::selected_file_ambiguous:
				return QStringLiteral(
					"The selected rename matches both paths in one revision.");
			case application::workbench_host_failure::selected_file_cancelled:
				return QStringLiteral("Loading the selected file was cancelled.");
			case application::workbench_host_failure::selected_file_comparison_unavailable:
				return QStringLiteral(
					"No exact comparison is ready for this file. Check Review history, "
					"then retry.");
			case application::workbench_host_failure::selected_file_stale:
				return QStringLiteral("The selected file changed before it could be loaded.");
			case application::workbench_host_failure::host_failure:
				return QStringLiteral("The workbench could not be updated.");
			case application::workbench_host_failure::selected_file_split_by_commit_filter:
				// RFC-BPR-0089: one exact diff cannot show it, and none is invented.
				return QStringLiteral(
					"A hidden commit changed this file between two shown commits, so no single diff shows only the shown changes. "
					"Show the hidden commit, or hide one of the others, to read it.");
			}
			return QStringLiteral("The workbench could not be updated.");
		}

		[[nodiscard]] auto receipt_failure_text(
			const application::receipt_failure_publication& aFailure) -> QString
		{
			if (aFailure.Store.has_value()) {
				switch (*aFailure.Store) {
				case application::ledger_store_failure::unavailable:
					return QStringLiteral("Private review progress is unavailable.");
				case application::ledger_store_failure::conflict:
					return QStringLiteral("Private review progress changed elsewhere.");
				case application::ledger_store_failure::integrity_failure:
					return QStringLiteral("Private review progress failed its integrity check.");
				case application::ledger_store_failure::quota_exceeded:
					return QStringLiteral("Private review progress reached its storage limit.");
				case application::ledger_store_failure::cancelled:
					return QStringLiteral("The private progress change was cancelled.");
				case application::ledger_store_failure::invalid_record:
					return QStringLiteral("The private progress change was invalid.");
				}
			}
			switch (aFailure.Rejection) {
			case core::file_receipt_rejection::none:
				break;
			case core::file_receipt_rejection::unknown_file:
				return QStringLiteral("The selected file is no longer current.");
			case core::file_receipt_rejection::material_mismatch:
				return QStringLiteral("The selected file material changed.");
			case core::file_receipt_rejection::excluded_file_cannot_be_accepted:
				return QStringLiteral("Include this file before accepting it.");
			case core::file_receipt_rejection::accepted_file_cannot_be_excluded:
				return QStringLiteral("Revoke acceptance before excluding this file.");
			}
			return QStringLiteral("Private review progress could not be updated.");
		}

		/// \brief Shows a literal ampersand where Qt would read a mnemonic.
		///
		/// \details [[BPR-0783]]. A menu or a button reads "&" in an action's text
		/// as the mnemonic marker and draws the next character underlined instead
		/// of the ampersand, so "Reviews & queue" arrives as "Reviews _queue".
		/// Doubling it is the escape. It belongs only in text that is drawn:
		/// a tooltip, a status tip and an accessible name are read rather than
		/// parsed, and Qt strips the pair back to one ampersand for assistive
		/// technology, so what is read aloud stays the plain sentence.
		[[nodiscard]] auto menu_literal(const QString& aText) -> QString
		{
			auto escaped = aText;
			escaped.replace(QStringLiteral("&"), QStringLiteral("&&"));
			return escaped;
		}

		/// \brief [[BPR-1328]]. The owner's final keys, 2026-10-08: "Okay final
		/// choice: option cmd down next file. Option down next change".
		/// Option-Up and Option-Down (Alt+Up and Alt+Down elsewhere) are
		/// Previous and Next change; Option-Command-Up and Option-Command-Down
		/// (Ctrl+Alt+Up and Ctrl+Alt+Down) are Previous and Next file.
		[[nodiscard]] auto previous_change_key() -> QKeySequence
		{
			return shortcut_key(shortcut_command::previous_change);
		}

		[[nodiscard]] auto next_change_key() -> QKeySequence
		{
			return shortcut_key(shortcut_command::next_change);
		}

		/// \brief [[BPR-1532]]. The owner, 2026-10-15: Option-Shift-Up and
		/// Option-Shift-Down (Alt+Shift+Up and Alt+Shift+Down elsewhere) go to
		/// the first and the last change in the current file.
		[[nodiscard]] auto change_end_keys() -> std::array<QKeySequence, 2>
		{
			return { shortcut_key(shortcut_command::first_change), shortcut_key(shortcut_command::last_change) };
		}

		/// \brief [[BPR-1532]]. Whether the focused widget, or a widget between
		/// it and the window, binds `aKey` with a widget-scoped shortcut of its
		/// own, as the tour editor's stop outline binds Option-Shift-Up to
		/// reorder. A window shortcut on the same key there would make Qt's
		/// match ambiguous, and neither would run.
		[[nodiscard]] auto binds_key_nearer(const QWidget* const aFocus, const QWidget* const aWindow, const QKeySequence& aKey) -> bool
		{
			for (const auto* widget = aFocus; widget != nullptr && widget != aWindow; widget = widget->parentWidget()) {
				const auto actions = widget->actions();
				if (std::ranges::any_of(actions, [&](const QAction* const aAction) {
						const auto context = aAction->shortcutContext();
						return (context == Qt::WidgetWithChildrenShortcut || (context == Qt::WidgetShortcut && widget == aFocus))
							&& aAction->shortcuts().contains(aKey);
					})) {
					return true;
				}
			}
			return false;
		}

		/// \brief [[BPR-1328]]. The keys an editor being written keeps for its
		/// own caret: Option-Up and Option-Down, its paragraph moves on macOS,
		/// and Command-Up and Command-Down, its document start and end there.
		/// Command-Up and Command-Down are no window command, but the code
		/// panes scroll on them, and an editor inside a pane must not hand an
		/// ignored press on to the pane.
		[[nodiscard]] auto editor_native_keys() -> QList<QKeySequence>
		{
			return { previous_change_key(),
					 next_change_key(),
					 QKeySequence{ QStringLiteral("Ctrl+Up") },
					 QKeySequence{ QStringLiteral("Ctrl+Down") } };
		}

		/// \brief [[BPR-1295]]. Whether a widget is an editable multi-line
		/// editor: a composer, a reply, a note or a suggestion.
		[[nodiscard]] auto editable_multi_line(const QObject* const aObject) -> bool
		{
			if (const auto* const plain = qobject_cast<const QPlainTextEdit*>(aObject); plain != nullptr) {
				return !plain->isReadOnly();
			}
			if (const auto* const rich = qobject_cast<const QTextEdit*>(aObject); rich != nullptr) {
				return !rich->isReadOnly();
			}
			return false;
		}

		/// \brief [[BPR-1531]]. Accept file's key: Option-A on macOS, Alt+A
		/// elsewhere (owner, 2026-10-15, replacing Command-Shift-A).
		[[nodiscard]] auto accept_file_key() -> QKeySequence
		{
			return shortcut_key(shortcut_command::accept_file);
		}

		/// \brief [[BPR-1531]] [[BPR-1532]] [[BPR-1536]]. Whether a focused
		/// widget takes typed text: an editable line or multi-line editor, or any
		/// other widget Qt routes text input to, such as an editable combo box, a
		/// spin box or the Review Cycle suggestion editor. There Option-A and,
		/// on macOS, every Option+character key type text, and Option-Shift-Up
		/// and Option-Shift-Down extend the selection (macOS) or make box carets
		/// (the suggestion editor), so the window's commands on those keys step
		/// aside.
		///
		/// \details A line or multi-line editor takes text when it is not
		/// read-only. Any other widget does when Qt marks it with
		/// `WA_InputMethodEnabled`, which Qt clears when it is read-only. An item
		/// view is not one: Qt marks it whenever its current item is editable,
		/// even with no edit trigger, and an edit in progress has its own focused
		/// editor. The read-only comparison panes are not one either.
		[[nodiscard]] auto takes_typed_text(const QWidget* const aWidget) -> bool
		{
			if (aWidget == nullptr || qobject_cast<const QAbstractItemView*>(aWidget) != nullptr) {
				return false;
			}
			if (qobject_cast<const QPlainTextEdit*>(aWidget) != nullptr || qobject_cast<const QTextEdit*>(aWidget) != nullptr) {
				return editable_multi_line(aWidget);
			}
			if (const auto* const line = qobject_cast<const QLineEdit*>(aWidget); line != nullptr) {
				return !line->isReadOnly();
			}
			return aWidget->testAttribute(Qt::WA_InputMethodEnabled);
		}

		/// \brief [[BPR-1536]]. Whether Option with a character key types text
		/// here. On macOS it types a character ("®", "ç", "†") or starts a dead
		/// key ("˜" before "n"); on Windows and Linux Alt is a command key, and
		/// text input there is AltGr, which [[BPR-1336]] yields.
		constexpr auto option_types_text =
#if defined(Q_OS_MACOS)
			true;
#else
			false;
#endif

		/// \brief [[BPR-1536]]. Option (Alt), alone or with Shift, and a
		/// character key: the presses that type text on macOS. Arrows, Page
		/// Up and Down, Return and Command-Option keys are not among them.
		[[nodiscard]] auto option_character_key(const QKeyCombination aKey) -> bool
		{
			const auto modifiers = aKey.keyboardModifiers() & ~Qt::KeypadModifier;
			const auto key       = aKey.key();
			return (modifiers == Qt::AltModifier || modifiers == (Qt::AltModifier | Qt::ShiftModifier))
				&& key >= Qt::Key_Space && key < Qt::Key_Escape;
		}

		/// \brief [[BPR-1531]] [[BPR-1536]]. Gives a widget in the window
		/// that takes typed text the keys that type there.
		///
		/// \details In `ShortcutOverride` it claims Accept file's Option-A
		/// (Alt+A) and, on macOS, every Option+character key, so no window,
		/// button or widget shortcut fires and the key goes on to the widget
		/// and the text input system as "å" or "®". The claim is made when the
		/// key arrives, so it also holds for a widget that begins taking text
		/// while it has focus: the Review Cycle reader becomes the suggestion
		/// editor in place. That change reaches Qt as an input method query
		/// for `ImEnabled` on the focused widget, on which `aRefresh` re-reads
		/// the window's typing state; it returns at once when nothing changed.
		class typed_text_key_claim final : public QObject
		{
		public:
			typed_text_key_claim(QWidget* const aWindow, std::function<void()> aRefresh)
				: QObject{ aWindow }
				, Window{ aWindow }
				, Refresh{ std::move(aRefresh) }
			{
			}

		protected:
			[[nodiscard]] auto eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool override
			{
				if (aEvent->type() == QEvent::ShortcutOverride) {
					const auto combination = static_cast<QKeyEvent*>(aEvent)->keyCombination();
					if (QKeySequence{ combination } == accept_file_key() || (option_types_text && option_character_key(combination))) {
						const auto* const widget = qobject_cast<const QWidget*>(aWatched);
						if (widget != nullptr && Window->isAncestorOf(widget) && takes_typed_text(widget)) {
							aEvent->accept();
							return true;
						}
					}
				} else if (aEvent->type() == QEvent::InputMethodQuery
						   && static_cast<QInputMethodQueryEvent*>(aEvent)->queries().testFlag(Qt::ImEnabled)
						   && aWatched == QApplication::focusWidget()) {
					Refresh();
				}
				return QObject::eventFilter(aWatched, aEvent);
			}

		private:
			QWidget*              Window;
			std::function<void()> Refresh;
		};

#if defined(Q_OS_WIN)
		/// \brief [[BPR-1336]]. Lets an editable field keep an AltGr character.
		class typed_text_yield final : public QObject
		{
		public:
			using QObject::QObject;

		protected:
			[[nodiscard]] auto eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool override
			{
				if (aEvent->type() == QEvent::ShortcutOverride && editor_keeps_typed_text(aWatched, *static_cast<QKeyEvent*>(aEvent))) {
					aEvent->accept();
					return true;
				}
				return QObject::eventFilter(aWatched, aEvent);
			}
		};
#endif

		/// \brief [[BPR-1295]]. Keeps Hide Drawers' key in an editable
		/// multi-line editor.
		///
		/// \details A window shortcut fires inside an editor unless the editor
		/// claims the key in `ShortcutOverride`, and some editors live in the
		/// Conversations drawer, so a key that won there would hide the text
		/// being written. The editor claims Command-B (Ctrl+B) and does
		/// nothing with it for now: Markdown bold would be its own card.
		/// Claiming it in `ShortcutOverride` also keeps the native macOS menu
		/// item from taking it, because Qt asks the focus object first.
		///
		/// [[BPR-1328]]. Option-Down and Option-Up (Alt+Down and Alt+Up) are
		/// Next and Previous change, and in an editor they are its own
		/// paragraph moves (macOS). The editor claims them, and Command-Down
		/// and Command-Up, in `ShortcutOverride` too, and hands the key press
		/// to the editor alone, so the caret moves as it always did.
		class editor_key_yield final : public QObject
		{
		public:
			editor_key_yield(QObject* const aParent, const QAction* const aAction, QList<QKeySequence> aNative)
				: QObject{ aParent }
				, Action{ aAction }
				, Native{ std::move(aNative) }
			{
			}

		protected:
			[[nodiscard]] auto eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool override
			{
				if ((aEvent->type() == QEvent::ShortcutOverride || aEvent->type() == QEvent::KeyPress)
					&& editable_multi_line(aWatched)) {
					// macOS marks the arrow keys as keypad keys; the shortcut
					// map looks past that, and so does the yield.
					const auto* const key      = static_cast<QKeyEvent*>(aEvent);
					const auto        sequence = QKeySequence{ QKeyCombination{ key->modifiers() & ~Qt::KeypadModifier, static_cast<Qt::Key>(key->key()) } };
					if (Action != nullptr && sequence == Action->shortcut()) {
						aEvent->accept();
						return true;
					}
					if (Native.contains(sequence)) {
						// The editor claims the key against the window's command,
						// acts on the press as it always did, and the press
						// stops there: one it ignores, such as Option-Up on
						// the first line, would otherwise travel on to the
						// window or the code pane around it. A press handed on is not spontaneous, so Qt
						// asks the shortcut map again, and the claim answers.
						if (aEvent->type() == QEvent::KeyPress) {
							if (Forwarding) {
								return false;
							}
							Forwarding = true;
							QCoreApplication::sendEvent(aWatched, aEvent);
							Forwarding = false;
						}
						aEvent->accept();
						return true;
					}
				}
				return QObject::eventFilter(aWatched, aEvent);
			}

		private:
			QPointer<const QAction> Action;
			QList<QKeySequence>     Native;
			bool                    Forwarding = false;
		};

		/// \brief [[BPR-1328]]. Describes one file or change arrow.
		///
		/// \details The owner kept the keys and asked that they be
		/// discoverable, so the tooltip's first line is the command and its
		/// key in the platform's own notation ("Next change (⌥↓)"). A second
		/// line names the bound a press would meet ("Last change in this
		/// file.") or why the arrow is disabled. The status tip carries the
		/// sentence alone.
		void describe_navigation_action(
			QAction* const aAction,
			const QString& aName,
			const bool     aEnabled,
			const QString& aEnabledText,
			const QString& aDetail)
		{
			aAction->setEnabled(aEnabled);
			const auto key   = aAction->shortcut().toString(QKeySequence::NativeText);
			const auto title = key.isEmpty() ? aName : QStringLiteral("%1 (%2)").arg(aName, key);
			aAction->setToolTip(aDetail.isEmpty() ? title : title + QLatin1Char{ '\n' } + aDetail);
			aAction->setStatusTip(aDetail.isEmpty() ? aEnabledText : aDetail);
		}

		void describe_action(
			QAction* const aAction,
			const bool     aEnabled,
			const QString& aEnabledText,
			const QString& aDisabledText)
		{
			aAction->setEnabled(aEnabled);
			const auto description = aEnabled ? aEnabledText : aDisabledText;
			aAction->setToolTip(description);
			aAction->setStatusTip(description);
		}
	}

	auto editor_keeps_typed_text(const QObject* const aReceiver, const QKeyEvent& aEvent) -> bool
	{
		const auto altGr = Qt::ControlModifier | Qt::AltModifier;
		if ((aEvent.modifiers() & altGr) != altGr || aEvent.text().isEmpty() || !aEvent.text().front().isPrint()) {
			return false;
		}
		if (const auto* const line = qobject_cast<const QLineEdit*>(aReceiver); line != nullptr) {
			return !line->isReadOnly();
		}
		return editable_multi_line(aReceiver);
	}

	workbench_window::workbench_window(
		workbench_intent_dispatch                       aDispatch,
		workbench_change_dispatch                       aChangeDispatch,
		workbench_drawer_width_dispatch                 aWidthDispatch,
		std::vector<application::github_profile_option> aProfiles,
		application::github_intent_dispatch             aGithubDispatch,
		QWidget* const                                  aParent,
		std::shared_ptr<dif_catalog_provider>           aDifCatalog)
		: QMainWindow{ aParent }
		, Dispatch{ std::move(aDispatch) }
		, ChangeDispatch{ std::move(aChangeDispatch) }
		, WidthDispatch{ std::move(aWidthDispatch) }
		, DifCatalog{ std::move(aDifCatalog) }
		, GithubProfiles{ std::move(aProfiles) }
		, GithubDispatch{ std::move(aGithubDispatch) }
	{
		DifCues     = std::make_unique<dif_cue_router>();
		DifCueClock = std::make_unique<QElapsedTimer>();
		DifCueClock->start();
		setObjectName(QStringLiteral("workbenchWindow"));
		setWindowTitle(QStringLiteral("BetterPullReview"));
#if defined(__APPLE__)
		// [[BPR-0646]]. The strip is the title row. The window's content
		// extends under a transparent title bar whose text is hidden, so the
		// traffic lights sit in the strip's left inset and the identity the
		// strip shows is the title, as Fork and Visual Studio Code do. The
		// safe-area margin Qt would otherwise add above the content is
		// declined: the strip takes that row itself.
		setWindowFlag(Qt::ExpandedClientAreaHint, true);
		setWindowFlag(Qt::NoTitleBarBackgroundHint, true);
		setAttribute(Qt::WA_ContentsMarginsRespectsSafeArea, false);
#endif
		// [[BPR-0646]]. Every strip control stays top-level at this supported
		// floor: the eleven bordered buttons and the four modality segments
		// fit with nothing folded into an overflow menu. The identity and the
		// two position labels are the only strip items that yield width, and
		// they do so before the floor (see apply_strip_width).
		// [[BPR-1255]]. A larger font can raise this to the review bar's fully
		// yielded width (see apply_review_bar_width).
		setMinimumSize(platform_floor_width, 360);
		resize(1'280, 820);

		build_menu_bar();
		build_primary_toolbar();
		build_central_surface();
		build_github_header();

		StatusLabel = new QLabel{ this };
		StatusLabel->setObjectName(QStringLiteral("workbenchStatus"));
		StatusLabel->setAccessibleName(QStringLiteral("Workbench status"));
		StatusLabel->setAccessibleDescription(QStringLiteral(
			"Selected file position, host failures, and review-history state in "
			"text"));
		// QStatusBar temporarily hides this label for an action status tip. Keep
		// both presentations on one compressible line so that swap cannot change
		// the status bar's height and move the workbench beneath it.
		StatusLabel->setWordWrap(false);
		StatusLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		StatusLabel->setTextInteractionFlags(Qt::TextSelectableByKeyboard
											 | Qt::TextSelectableByMouse);
		statusBar()->addWidget(StatusLabel, 1);
		// [[BPR-1357]]. What was just done is said in Home's band too, for as
		// long as the status bar says it.
		connect(statusBar(), &QStatusBar::messageChanged, this, [this](const QString&) { update_home_status(); });
		// [[BPR-1381]]. The band is docked just above the status bar, so while
		// it says the status bar's message the status bar does not draw it a
		// second time beneath (see eventFilter).
		statusBar()->installEventFilter(this);
		// [[BPR-0775]] mandate r2. Dif rests in the window's bottom-left corner
		// while Home is not the first-launch card. This slot reserves the start
		// of the status line for him; the sprite is laid over it (see
		// place_dif_corner), so the status bar keeps its height.
		DifCornerSlot = new QWidget{ this };
		DifCornerSlot->setObjectName(QStringLiteral("workbenchDifCornerSlot"));
		DifCornerSlot->setAccessibleName(QStringLiteral("Decorative Dif illustration"));
		DifCornerSlot->setFixedWidth(dif_corner_size + 8);
		DifCornerSlot->hide();
		DifCornerSlot->installEventFilter(this);
		statusBar()->insertWidget(0, DifCornerSlot, 0);
		// A message shown before Home arrives must not be painted over either
		// (review of BPR-0775, finding 1).
		connect(statusBar(), &QStatusBar::messageChanged, this, [this](const QString&) { update_dif_presentation(); });

		configure_tool_buttons();
		refresh_toolbar_icons();
		apply_focus_order();
		update_action_state();
		update_status_text();
	}

	workbench_window::~workbench_window()
	{
		// Child settings windows can change focus while QWidget tears down the
		// already-deleted menu actions. Stop application focus ingress first.
		disconnect(qApp, &QApplication::focusChanged, this, nullptr);
		unmount_settings();
	}

#if defined(_WIN32)
	auto workbench_window::title_row_part_at(const QPoint aWindowPoint) const -> title_row_part
	{
		if (Strip == nullptr || !Strip->isVisible()) {
			return title_row_part::client;
		}
		const auto local = Strip->mapFrom(this, aWindowPoint);
		if (!Strip->rect().contains(local)) {
			return title_row_part::client;
		}
		if (CaptionButtons != nullptr && CaptionButtons->isVisible()) {
			const auto inButtons = CaptionButtons->mapFrom(Strip, local);
			if (CaptionButtons->rect().contains(inButtons)) {
				return CaptionButtons->part_at(inButtons);
			}
		}
		return Strip->empty_surface_at(local) ? title_row_part::caption : title_row_part::client;
	}

	auto workbench_window::caption_buttons() const noexcept -> windows_caption_buttons*
	{
		return CaptionButtons;
	}

	auto workbench_window::strip_menu_button() const noexcept -> QToolButton*
	{
		return StripMenuButton;
	}

	auto workbench_window::open_strip_menu() -> QMenu*
	{
		// [[BPR-1028]]. This used to be a QMenu on this handler's stack, run
		// with exec(). An accessible invoke runs a menu action without closing
		// the menu first, so File > Quit destroyed the window inside that nested
		// loop. The window deleted the stack menu as one of its children, and
		// the Debug CRT asserted _CrtIsValidHeapPointer. A heap child that
		// deletes itself on close, opened without a nested loop, has neither
		// problem. The window deletes an open menu along with its other
		// children.
		auto* const menu = new QMenu{ this };
		menu->setObjectName(QStringLiteral("workbenchStripMenu"));
		menu->setAttribute(Qt::WA_DeleteOnClose);
		for (auto* const action : PrimaryMenuBar->actions()) {
			if (action->isVisible()) {
				// [[BPR-1136]]. Qt caches a menu item's accessible interface by its
				// action, and these actions outlive each strip menu. A menu shown
				// again would find the item its deleted predecessor made, invalid
				// and parented to nothing, and UI Automation saw an empty menu.
				// Dropping it here lets this menu make its own.
				if (auto* const stale = QAccessible::queryAccessibleInterface(action); stale != nullptr) {
					QAccessible::deleteAccessibleInterface(QAccessible::uniqueId(stale));
				}
				menu->addAction(action);
			}
		}
		menu->popup(StripMenuButton->mapToGlobal(QPoint{ 0, StripMenuButton->height() }));
		return menu;
	}

	void workbench_window::open_strip_menu_from_keyboard()
	{
		// The first menu is highlighted, not opened, as a menu bar entered
		// from the keyboard highlights File. Down is the menu's own gesture
		// for that: it selects the first entry and opens no submenu, where
		// setActiveAction would open File at once.
		auto* const menu = open_strip_menu();
		auto        down = QKeyEvent{ QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier };
		QCoreApplication::sendEvent(menu, &down);
	}

	auto workbench_window::read_strip_menu_key(const QEvent& aEvent) -> bool
	{
		// [[BPR-1022]]. A chord's own modifiers other than Alt: Alt held with
		// any of these is a chord (Ctrl with Alt is AltGr), never Alt alone.
		constexpr auto other = Qt::ControlModifier | Qt::ShiftModifier | Qt::MetaModifier;
		const auto     ready = [this] {
			return QApplication::activePopupWidget() == nullptr && StripMenuButton != nullptr
				&& StripMenuButton->isVisible();
		};
		const auto type = aEvent.type();
		if (type == QEvent::KeyPress || type == QEvent::ShortcutOverride || type == QEvent::KeyRelease) {
			const auto& key = static_cast<const QKeyEvent&>(aEvent);
			if (key.key() == Qt::Key_Alt) {
				// A held Alt repeats its press; only the first press arms, and
				// the release opens the menu only if nothing came between.
				if (type == QEvent::KeyPress && !key.isAutoRepeat()) {
					StripMenuAltAlone = (key.modifiers() & other) == 0 && ready();
				} else if (type == QEvent::KeyRelease && !key.isAutoRepeat() && std::exchange(StripMenuAltAlone, false)
						   && ready()) {
					open_strip_menu_from_keyboard();
				}
				return false;
			}
			if (type == QEvent::KeyRelease) {
				return false;
			}
			// Every other key, chord or shortcut makes a held Alt a modifier.
			// A shortcut is offered as ShortcutOverride and, when it matches,
			// never arrives as a press, so both are read.
			StripMenuAltAlone = false;
			if (type == QEvent::KeyPress && key.key() == Qt::Key_F10 && !key.isAutoRepeat()
				&& (key.modifiers() & (other | Qt::AltModifier)) == 0 && ready()) {
				open_strip_menu_from_keyboard();
				return true;
			}
			return false;
		}
		switch (type) {
		case QEvent::MouseButtonPress:
		case QEvent::MouseButtonDblClick:
		case QEvent::NonClientAreaMouseButtonPress:
		case QEvent::NonClientAreaMouseButtonDblClick:
		case QEvent::Wheel:
		case QEvent::TouchBegin:
		case QEvent::TabletPress:
		case QEvent::FocusIn:
		case QEvent::FocusOut:
		case QEvent::Hide:
			// An Alt-click, an Alt-wheel, or the window losing the keyboard
			// between Alt's press and release (Alt+Tab) is not Alt alone.
			StripMenuAltAlone = false;
			break;
		default:
			break;
		}
		return false;
	}

#endif
	void workbench_window::build_primary_toolbar()
	{
		// [[BPR-0646]]. RFC-BPR-0028's strip: one 30-pixel row in full ink
		// with bordered buttons. It stays the window's one toolbar so every
		// action keeps its presenter, shortcut and place in the declared focus
		// order. [[BPR-1129]]: RFC-BPR-0052 makes it the calm title row, and
		// what acts on the review moves to the review bar beneath it.
		Strip = new workbench_strip{ QStringLiteral("Title row"), this };
		addToolBar(Strip);
		PrimaryToolbar = Strip;
		PrimaryToolbar->setObjectName(QStringLiteral("workbenchPrimaryToolbar"));
		PrimaryToolbar->setAccessibleName(QStringLiteral("Title row"));
		PrimaryToolbar->setAccessibleDescription(QStringLiteral(
			"Home, the drawer toggles, the pull request, the review modality, "
			"refresh, comments, account and settings"));

		const auto makeDrawer = [this](const QString&             aText,
									   const QString&             aName,
									   const QString&             aAccessible,
									   const QString&             aObjectName,
									   const QString&             aCollapse,
									   const QString&             aExpand,
									   const QList<QKeySequence>& aShortcuts) -> drawer_entry {
			auto* const action = new QAction{ aText, this };
			action->setObjectName(aObjectName);
			action->setCheckable(true);
			// [[BPR-1678]]. Every catalogue key: Files and Repositories
			// have a second, command-modifier key that acts while typing.
			action->setShortcuts(aShortcuts);
			action->setShortcutContext(Qt::WindowShortcut);
			action->setIconText(aName);
			addAction(action);
			return {
				.Toggle     = action,
				.Name       = aName,
				.Accessible = aAccessible,
				.Collapse   = aCollapse,
				.Expand     = aExpand,
			};
		};

		// Alt+1 through Alt+3 belong to the composed review-history control's
		// named comparisons, so drawer gestures retain their unambiguous Alt+Shift
		// bindings from the existing semantic action path.
		Drawers[0] = makeDrawer(
			QStringLiteral("&Files drawer"),
			QStringLiteral("Files drawer"),
			QStringLiteral("Files drawer toggle"),
			QStringLiteral("toggleWorkbenchFilesDrawerAction"),
			QStringLiteral("Hide the file drawer and give its width to the comparison."),
			QStringLiteral("Show the file drawer at its saved width."),
			shortcut_keys(shortcut_command::files_drawer));
		Drawers[1] = makeDrawer(
			// [[BPR-1293]]. The drawer is Comments, as its tab and title-row
			// toggle say; only the modality is called Threads ([[BPR-1341]]).
			QStringLiteral("&Comments drawer"),
			QStringLiteral("Comments drawer"),
			QStringLiteral("Comments drawer toggle"),
			QStringLiteral("toggleWorkbenchConversationsDrawerAction"),
			QStringLiteral("Hide the comments drawer and give its width to the comparison."),
			QStringLiteral("Show the comments drawer at its saved width."),
			shortcut_keys(shortcut_command::comments_drawer));
		// [[BPR-0651]]. The repositories drawer, left of the files. Alt+Shift+3
		// to 5 are taken by the line numbers, review controls and review
		// details, so this one is mnemonic.
		Drawers[2] = makeDrawer(
			QStringLiteral("&Repositories drawer"),
			QStringLiteral("Repositories drawer"),
			QStringLiteral("Repositories drawer toggle"),
			QStringLiteral("toggleWorkbenchRepositoriesDrawerAction"),
			QStringLiteral("Hide the repositories drawer and give its width to the comparison."),
			QStringLiteral("Show the repositories drawer at its saved width."),
			shortcut_keys(shortcut_command::repositories_drawer));

		const auto make = [this](const QString&             aText,
								 const QString&             aName,
								 const QString&             aAccessible,
								 const QString&             aObjectName,
								 const QString&             aPurpose,
								 const QList<QKeySequence>& aShortcuts) -> navigation_entry {
			auto* const action = new QAction{ aText, this };
			action->setObjectName(aObjectName);
			action->setShortcuts(aShortcuts);
			// Window context keeps the shortcut live from anywhere in the window,
			// including while focus is inside either drawer or comparison pane.
			action->setShortcutContext(Qt::WindowShortcut);
			action->setIconText(aName);
			addAction(action);
			return {
				.Action     = action,
				.Name       = aName,
				.Accessible = aAccessible,
				.Purpose    = aPurpose,
			};
		};

		Navigation[0] = make(QStringLiteral("&Previous file"),
							 QStringLiteral("Previous file"),
							 QStringLiteral("Previous file"),
							 QStringLiteral("previousWorkbenchFileAction"),
							 QStringLiteral("Select the previous actionable file."),
							 shortcut_keys(shortcut_command::previous_file));
		Navigation[1] = make(QStringLiteral("&Next file"),
							 QStringLiteral("Next file"),
							 QStringLiteral("Next file"),
							 QStringLiteral("nextWorkbenchFileAction"),
							 QStringLiteral("Select the next actionable file."),
							 shortcut_keys(shortcut_command::next_file));
		Navigation[2] = make(QStringLiteral("Pre&vious change"),
							 QStringLiteral("Previous change"),
							 QStringLiteral("Previous change"),
							 QStringLiteral("previousWorkbenchChangeAction"),
							 QStringLiteral("Select the previous semantic change."),
							 // [[BPR-1328]]. Option-Up (Alt+Up) first, which the
							 // tooltip names; Option-Page Up stays as before.
							 shortcut_keys(shortcut_command::previous_change));
		Navigation[3] = make(QStringLiteral("Ne&xt change"),
							 QStringLiteral("Next change"),
							 QStringLiteral("Next change"),
							 QStringLiteral("nextWorkbenchChangeAction"),
							 QStringLiteral("Select the next semantic change."),
							 shortcut_keys(shortcut_command::next_change));

		// The native menu and toolbar share each retained navigation action.
		for (std::size_t index = 0U; index < Navigation.size(); ++index) {
			ViewMenu->insertAction(HeaderSourceAction, Navigation[index].Action);
			connect(Navigation[index].Action,
					&QAction::triggered,
					this,
					[this, index] { dispatch_navigation(index); });
		}
		// [[BPR-1532]]. First and Last change, after Next change. Menu and
		// keys only: the review bar keeps its four arrows. R and H are the
		// View menu's unused mnemonics.
		ChangeEnds[0] = make(QStringLiteral("Fi&rst change"),
							 QStringLiteral("First change"),
							 QStringLiteral("First change"),
							 QStringLiteral("firstWorkbenchChangeAction"),
							 QStringLiteral("Select the first change in this file."),
							 { change_end_keys()[0] });
		ChangeEnds[1] = make(QStringLiteral("Last c&hange"),
							 QStringLiteral("Last change"),
							 QStringLiteral("Last change"),
							 QStringLiteral("lastWorkbenchChangeAction"),
							 QStringLiteral("Select the last change in this file."),
							 { change_end_keys()[1] });
		for (const auto& [entry, id] : { std::pair{ &ChangeEnds[0], core::workbench_command_id::first_change }, std::pair{ &ChangeEnds[1], core::workbench_command_id::last_change } }) {
			ViewMenu->insertAction(HeaderSourceAction, entry->Action);
			connect(entry->Action, &QAction::triggered, this, [this, id] { dispatch_navigation(static_cast<std::size_t>(id)); });
		}
		connect(qApp, &QApplication::focusChanged, this, [this] { update_change_end_keys(); });
		ViewMenu->insertSeparator(HeaderSourceAction);
		// [[BPR-1287]]. Open first file is Next file's gesture, so it is
		// offered exactly when Next file is.
		connect(Navigation[1].Action, &QAction::changed, this, [this] {
			if (ComparisonSurface != nullptr) {
				ComparisonSurface->idle_open_first_button()->setEnabled(Navigation[1].Action->isEnabled());
			}
		});

		for (std::size_t index = 0U; index < Drawers.size(); ++index) {
			connect(Drawers[index].Toggle,
					&QAction::triggered,
					this,
					[this, index] { dispatch_drawer_toggle(index); });
		}

		ReviewDetailsAction = new QAction{ QStringLiteral("Review &details"), this };
		ReviewDetailsAction->setObjectName(
			QStringLiteral("toggleWorkbenchReviewDetailsAction"));
		ReviewDetailsAction->setCheckable(true);
		// [[BPR-0646]]. Off by default: the chrome above the comparison is
		// the strip and the epoch line; the exact anchors and the selected
		// change's details are shown on request.
		ReviewDetailsAction->setChecked(false);
		ReviewDetailsAction->setShortcuts(shortcut_keys(shortcut_command::review_details));
		ReviewDetailsAction->setShortcutContext(Qt::WindowShortcut);
		ReviewDetailsAction->setToolTip(QStringLiteral(
			"Show the exact review anchors."));
		ReviewDetailsAction->setStatusTip(ReviewDetailsAction->toolTip());
		addAction(ReviewDetailsAction);
		connect(ReviewDetailsAction, &QAction::toggled, this, [this](const bool) {
			apply_review_details_visibility();
		});

		ReviewControlsMenu = new QMenu{ this };
		ReviewControlsMenu->setObjectName(QStringLiteral("workbenchReviewControlsMenu"));
		ReviewControlsMenu->setAccessibleName(QStringLiteral("Private progress actions"));
		ReviewControlsMenu->setAccessibleDescription(QStringLiteral(
			"Seen, read, acceptance, and exclusion actions for the selected exact file material"));
		const auto receiptAction = [this](
									   const std::size_t aIndex,
									   const QString&    aText,
									   const QString&    aObjectName,
									   const QString&    aAccessible) {
			auto* const action = new QAction{ aText, this };
			action->setObjectName(aObjectName);
			action->setCheckable(true);
			action->setIconText(aText);
			action->setEnabled(false);
			addAction(action);
			ReviewControlsMenu->addAction(action);
			ReceiptActions[aIndex] = action;
			connect(action,
					&QAction::triggered,
					this,
					[this, aIndex] { dispatch_receipt(aIndex); });
			action->setToolTip(aAccessible);
			action->setStatusTip(aAccessible);
		};
		receiptAction(0U,
					  QStringLiteral("&Seen"),
					  QStringLiteral("markSelectedFileSeenAction"),
					  QStringLiteral("Mark the selected exact file material seen."));
		receiptAction(1U,
					  QStringLiteral("&Read"),
					  QStringLiteral("toggleSelectedFileReadAction"),
					  QStringLiteral("Change the selected exact file material's read state."));
		receiptAction(2U,
					  QStringLiteral("&Accept"),
					  QStringLiteral("toggleSelectedFileAcceptanceAction"),
					  QStringLiteral("Change private acceptance for the selected exact file material."));
		ReceiptActions[2]->setShortcutContext(Qt::WindowShortcut);
		ReceiptActions[2]->setAutoRepeat(false);
		ReceiptActions[2]->setShortcutVisibleInContextMenu(true);
		receiptAction(3U,
					  QStringLiteral("E&xclude"),
					  QStringLiteral("toggleSelectedFileExclusionAction"),
					  QStringLiteral("Change private exclusion for the selected file."));

		ReviewControlsButton = new menu_tool_button{ this };
		ReviewControlsButton->setObjectName(QStringLiteral("workbenchReviewControlsButton"));
		ReviewControlsButton->setText(QStringLiteral("Private &progress"));
		ReviewControlsButton->setShortcut(shortcut_key(shortcut_command::private_progress));
		ReviewControlsButton->setAccessibleName(QStringLiteral("Private progress"));
		ReviewControlsButton->setAccessibleDescription(QStringLiteral(
			"Open private seen, read, acceptance, and exclusion actions"));
		ReviewControlsButton->setToolTip(QStringLiteral("Private progress actions"));
		ReviewControlsButton->setStatusTip(ReviewControlsButton->toolTip());
		ReviewControlsButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
		ReviewControlsButton->setPopupMode(QToolButton::InstantPopup);
		ReviewControlsButton->setFocusPolicy(Qt::StrongFocus);
		ReviewControlsButton->setMenu(ReviewControlsMenu);

		AccountAction = new QAction{ QStringLiteral("&Account"), this };
		AccountAction->setObjectName(QStringLiteral("workbenchAccountAction"));
		AccountAction->setIconText(QStringLiteral("Account"));
		addAction(AccountAction);
		connect(AccountAction, &QAction::triggered, this, [this] {
			open_settings(settings_page::accounts_and_hosts);
			// [[BPR-0773]] D5. Signed out, the account button is the chosen
			// server's own Sign in: the card that offers the device flow and
			// shows Sign in is pressed, exactly as the reviewer would press it.
			// Anything else only presents the page.
			if (auto* const card = current_server_card();
				card != nullptr && card->offers_device_sign_in()
				&& card->sign_in_button()->isVisibleTo(card)) {
				card->sign_in_button()->click();
			}
		});

		const auto rowLabel = [](QWidget* const aRow,
								 const QString& aObjectName,
								 const QString& aAccessible,
								 const QString& aDescription) {
			auto* const label = new QLabel{ aRow };
			label->setObjectName(aObjectName);
			label->setAccessibleName(aAccessible);
			label->setAccessibleDescription(aDescription);
			label->setTextFormat(Qt::PlainText);
			label->setFocusPolicy(Qt::NoFocus);
			label->setContentsMargins(2, 0, 4, 0);
			return label;
		};
		const auto rowWidget = [](QToolBar* const aRow, QWidget* const aWidget, const QString& aActionName) {
			auto* const action = aRow->addWidget(aWidget);
			action->setObjectName(aActionName);
			return action;
		};
		const auto rowSpacer = [&rowWidget](QToolBar* const aRow, const QString& aObjectName, const QString& aActionName) {
			auto* const spacer = new QWidget{ aRow };
			spacer->setObjectName(aObjectName);
			spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
			spacer->setFocusPolicy(Qt::NoFocus);
			rowWidget(aRow, spacer, aActionName);
			return spacer;
		};

		// [[BPR-1129]]. RFC-BPR-0052's title row carries the window, the pull
		// request and the modality, in this exact persistent order: the
		// platform inset, Home, the repositories and files toggles, the pull
		// request's name, then at the right the modality, refresh (inserted
		// once the session chrome has made it), the comments toggle with the
		// new-comment count, a rule, account and settings. No navigation,
		// progress or composition control sits in it; they are the review
		// bar's, so the name has the room they took.
		// The platform inset comes first: on macOS the width the three window
		// buttons occupy plus the gap Fork and Visual Studio Code leave before
		// their first control; nothing elsewhere ([[BPR-1596]]: one named value
		// per platform, title_row_leading_inset). A widget item rather than a
		// margin, because the toolbar layout lays its items from its own edge.
		StripInset = new QWidget{ PrimaryToolbar };
		StripInset->setObjectName(QStringLiteral("workbenchStripInset"));
		StripInset->setFocusPolicy(Qt::NoFocus);
		StripInset->setFixedWidth(title_row_leading_inset(current_title_row_platform()));
		rowWidget(PrimaryToolbar, StripInset, QStringLiteral("workbenchStripInsetAction"));
#if defined(_WIN32)
		// [[BPR-0653]]. The Windows form of the title row, after Visual Studio
		// Code: no menu bar row, but one compact menu control first in the
		// strip that opens the same menus the window builds (File, Profiles,
		// Pull Request and View), read from the menu bar each time so a menu
		// added later is there too. Its description names them from the bar
		// for the same reason.
		StripMenuButton = new QToolButton{ PrimaryToolbar };
		StripMenuButton->setObjectName(QStringLiteral("workbenchStripMenuButton"));
		StripMenuButton->setAccessibleName(QStringLiteral("Application menu"));
		{
			auto menus = QStringList{ };
			for (auto* const action : PrimaryMenuBar->actions()) {
				if (action->menu() != nullptr) {
					menus.push_back(action->menu()->accessibleName());
				}
			}
			const auto last = menus.isEmpty() ? QString{ } : menus.takeLast();
			StripMenuButton->setAccessibleDescription(
				menus.isEmpty() ? QStringLiteral("The %1 menu").arg(last)
								: QStringLiteral("The %1 and %2 menus").arg(menus.join(QStringLiteral(", ")), last));
		}
		StripMenuButton->setToolTip(QStringLiteral("Menu"));
		StripMenuButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
		StripMenuButton->setFocusPolicy(Qt::StrongFocus);
		StripMenuButton->setIcon(windows_menu_icon(palette()));
		connect(StripMenuButton, &QToolButton::clicked, this, &workbench_window::open_strip_menu);
		rowWidget(PrimaryToolbar, StripMenuButton, QStringLiteral("workbenchStripMenuAction"));
#endif
		// [[BPR-0650]]. Home first: the way back to the landing and the queue
		// from any review, checked while Home shows. [[BPR-1535]]: Command-Shift-H
		// (Ctrl+Shift+H), as Safari's and Finder's Home; Command-0 is Actual Size.
		HomeAction = new QAction{ QStringLiteral("&Home"), this };
		HomeAction->setObjectName(QStringLiteral("workbenchHomeAction"));
		HomeAction->setCheckable(true);
		HomeAction->setShortcuts(shortcut_keys(shortcut_command::home));
		HomeAction->setShortcutContext(Qt::WindowShortcut);
		HomeAction->setIconText(QStringLiteral("Home"));
		HomeAction->setToolTip(QStringLiteral(
								   "Show Home (%1): sign-in or the review queue, and the way back to the review.")
								   .arg(HomeAction->shortcut().toString(QKeySequence::NativeText)));
		HomeAction->setStatusTip(HomeAction->toolTip());
		addAction(HomeAction);
		connect(HomeAction, &QAction::triggered, this, [this](const bool aChecked) {
			choose_home(aChecked);
		});
		PrimaryToolbar->addAction(HomeAction);
		PrimaryToolbar->addAction(Drawers[2].Toggle);
		PrimaryToolbar->addAction(Drawers[0].Toggle);
		IdentityLabel = rowLabel(
			PrimaryToolbar,
			QStringLiteral("workbenchStripIdentity"),
			QStringLiteral("Open pull request"),
			QStringLiteral("Repository, number and title of the open pull request"));
		IdentityLabel->setContentsMargins(4, 0, 4, 0);
		IdentityAction = rowWidget(
			PrimaryToolbar, IdentityLabel, QStringLiteral("workbenchStripIdentityAction"));
		// [[BPR-1338]]. Open on GitHub, the owner's "open in browser button next
		// to the pr title". The window's action names a pull request and the
		// host admits it and builds the address; Qt never holds one. Its button
		// sits right after the name and goes with it: counted in the name's
		// room, so the title elides around it, and shown exactly while the name
		// of a GitHub pull request is. It is borderless, as the modality's
		// segments are, so it reads as part of the name rather than as another
		// control in the row.
		OpenOnGithubAction = new QAction{ tr("Open on &GitHub"), this };
		OpenOnGithubAction->setObjectName(QStringLiteral("openOnGithubAction"));
		OpenOnGithubAction->setShortcuts(shortcut_keys(shortcut_command::open_on_github));
		OpenOnGithubAction->setShortcutContext(Qt::WindowShortcut);
		OpenOnGithubAction->setIconText(QStringLiteral("Open on GitHub"));
		OpenOnGithubAction->setToolTip(QStringLiteral("Open on GitHub (%1)").arg(OpenOnGithubAction->shortcut().toString(QKeySequence::NativeText)));
		OpenOnGithubAction->setStatusTip(QStringLiteral("Open the pull request's page on GitHub in your browser"));
		OpenOnGithubAction->setEnabled(false);
		addAction(OpenOnGithubAction);
		connect(OpenOnGithubAction, &QAction::triggered, this, [this] {
			if (const auto target = pull_request_page_target(); target.has_value()) {
				open_pull_request_page(*target);
			}
		});
		PullRequestMenu->insertAction(FollowPullRequest, OpenOnGithubAction);
		PullRequestMenu->insertSeparator(FollowPullRequest);
		OpenOnGithubButton = new QToolButton{ PrimaryToolbar };
		OpenOnGithubButton->setObjectName(QStringLiteral("workbenchOpenOnGithubButton"));
		OpenOnGithubButton->setDefaultAction(OpenOnGithubAction);
		OpenOnGithubButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
		OpenOnGithubButton->setAccessibleName(QStringLiteral("Open on GitHub"));
		OpenOnGithubButton->setAccessibleDescription(QStringLiteral("Opens the open pull request's page on GitHub in your browser"));
		OpenOnGithubButton->setProperty(strip_style::filled_property, true);
		Strip->adopt_button(OpenOnGithubButton);
		OpenOnGithubButtonAction = rowWidget(
			PrimaryToolbar, OpenOnGithubButton, QStringLiteral("workbenchOpenOnGithubAction"));
		// [[BPR-0656]]. Next new comment keeps its key and its window
		// ownership; [[BPR-1129]] its count rides on the comments toggle and its
		// presenter leaves the title row.
		NextNewCommentAction = new QAction{ QStringLiteral("Next new &comment"), this };
		NextNewCommentAction->setObjectName(QStringLiteral("nextNewCommentAction"));
		NextNewCommentAction->setIconText(QStringLiteral("Next new comment"));
		NextNewCommentAction->setToolTip(QStringLiteral("Next new comment"));
		NextNewCommentAction->setStatusTip(QStringLiteral(
			"Reveal the next thread with a post newer than what you have read."));
		NextNewCommentAction->setShortcuts(shortcut_keys(shortcut_command::next_new_comment));
		NextNewCommentAction->setShortcutContext(Qt::WindowShortcut);
		NextNewCommentAction->setEnabled(false);
		addAction(NextNewCommentAction);
		connect(NextNewCommentAction, &QAction::triggered, this, [this] { step_to_next_new_comment(); });

		ToolbarSpacer = rowSpacer(
			PrimaryToolbar,
			QStringLiteral("workbenchNavigationSpacer"),
			QStringLiteral("workbenchNavigationSpacerAction"));
		Modality = new modality_control{ *Strip, PrimaryToolbar };
		// [[BPR-1295]]. The strip's button and key choose; choosing Review
		// while it shows is the way back to its preset.
		Modality->set_dispatch([this](const review_modality aModality) {
			choose_modality(aModality, core::workbench_modality_entry::choose);
		});
		rowWidget(PrimaryToolbar, Modality, QStringLiteral("workbenchModalityAction"));
		PrimaryToolbar->addAction(Drawers[1].Toggle);
		// [[BPR-1295]]. Hide Drawers ends the drawer controls, right of the
		// Comments toggle and before the rule that sets off Account and
		// Settings: away from the modality segment, beside the drawer it
		// hides most often.
		PrimaryToolbar->addAction(ToggleDrawersAction);
		if (auto* const button = qobject_cast<QToolButton*>(PrimaryToolbar->widgetForAction(ToggleDrawersAction));
			button != nullptr) {
			button->setObjectName(QStringLiteral("workbenchToggleDrawersButton"));
			button->setToolButtonStyle(Qt::ToolButtonIconOnly);
			button->setFocusPolicy(Qt::StrongFocus);
		}
		rowWidget(
			PrimaryToolbar,
			new strip_divider{ PrimaryToolbar },
			QStringLiteral("workbenchStripRightDividerAction"));
		PrimaryToolbar->addAction(AccountAction);
		PrimaryToolbar->addAction(SettingsAction);
#if defined(_WIN32)
		// [[BPR-0653]]. The caption buttons end the row, after settings. The
		// host dispatches their presses and reports the window's state.
		CaptionButtons = new windows_caption_buttons{ PrimaryToolbar };
		rowWidget(PrimaryToolbar, CaptionButtons->reserve(), QStringLiteral("workbenchCaptionButtonsAction"));
		// The menu bar's row is the strip's menu control on Windows. The bar
		// stays the menus' owner, hidden, so the Quit shortcut it carried is
		// the window's to keep live.
		PrimaryMenuBar->setVisible(false);
		addAction(QuitProfilesAction);
#endif

		// [[BPR-1129]]. RFC-BPR-0052's review bar: full width under the title
		// row and always shown with a review, in this exact order. From its
		// left, Updates and the comparison it names with its dates and menu (the
		// epoch control hands its "Comparing" button over once it exists), the
		// file label and pair, the change label and pair and
		// private progress; at its right Compose review and Comment. It is an
		// ordinary row: its bare surface drags nothing.
		ReviewBar = new workbench_strip{ QStringLiteral("Review bar"), this, strip_role::review_bar };
		// [[BPR-1255]]. Its own font or style can move the window's floor.
		ReviewBar->installEventFilter(this);
		ReviewBar->setObjectName(QStringLiteral("workbenchReviewBar"));
		ReviewBar->setAccessibleName(QStringLiteral("Review bar"));
		ReviewBar->setAccessibleDescription(QStringLiteral(
			"Updates and the comparison, file and change navigation, private "
			"progress, Compose review and Comment"));
		auto* const barInset = new QWidget{ ReviewBar };
		barInset->setObjectName(QStringLiteral("reviewBarInset"));
		barInset->setFocusPolicy(Qt::NoFocus);
		barInset->setFixedWidth(5);
		rowWidget(ReviewBar, barInset, QStringLiteral("reviewBarInsetAction"));
		// [[BPR-1130]]. The Updates disclosure folds the timeline below the bar
		// away and opens it again, and the bar never moves. The core decides the
		// fold; the button shows it and sends the gesture.
		UpdatesDisclosure = new updates_disclosure{ ReviewBar };
		UpdatesDisclosure->setText(QStringLiteral("Updates"));
		UpdatesDisclosure->setAccessibleDescription(QStringLiteral(
			"Folds or opens the Updates line, the pull request's updates, whose timeline runs below this bar"));
		connect(UpdatesDisclosure, &QToolButton::clicked, this, [this] { dispatch_updates_line_toggle(); });
		rowWidget(ReviewBar, UpdatesDisclosure, QStringLiteral("reviewBarUpdatesAction"));
		FilePositionLabel = rowLabel(
			ReviewBar,
			QStringLiteral("workbenchStripFilePosition"),
			QStringLiteral("File position"),
			QStringLiteral("The selected file's position among the actionable files"));
		// [[BPR-1519]]. Keep each label beside its arrows inside the stable counter slot.
		FilePositionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		FilePositionAction = rowWidget(
			ReviewBar, FilePositionLabel, QStringLiteral("workbenchStripFilePositionAction"));
		ReviewBar->addAction(Navigation[0].Action);
		ReviewBar->addAction(Navigation[1].Action);
		ChangePositionLabel = rowLabel(
			ReviewBar,
			QStringLiteral("workbenchStripChangePosition"),
			QStringLiteral("Change position"),
			QStringLiteral("The selected change's position among the comparison's changes"));
		ChangePositionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		ChangePositionAction = rowWidget(
			ReviewBar, ChangePositionLabel, QStringLiteral("workbenchStripChangePositionAction"));
		ReviewBar->addAction(Navigation[2].Action);
		ReviewBar->addAction(Navigation[3].Action);
		// [[BPR-1507]]. The normal file control exposes the existing exact
		// acceptance action directly; its state menu keeps every alternative.
		// [[BPR-1578]]. The owner, 2026-10-17: "Accept is a button next to
		// other similar text labels in Changes and Files which are not
		// buttons, it's kind of confusing. It also doesn't have the under bar
		// that connects it to the dropdown control". After two rounds of
		// directions the owner chose the progress track (v002): Seen, Read and
		// Accept as steps of one control, the menu's chevron its last cell,
		// on one rail as File and Changes are with their arrows. The rails of
		// neighbouring groups must not run together: File and Changes are
		// parted by the Changes label's slot, Changes and this group by a bare
		// gap.
		auto* const progressGap = new QWidget{ ReviewBar };
		progressGap->setObjectName(QStringLiteral("reviewBarProgressGap"));
		progressGap->setFocusPolicy(Qt::NoFocus);
		progressGap->setFixedWidth(8);
		rowWidget(ReviewBar, progressGap, QStringLiteral("reviewBarProgressGapAction"));
		const auto makeStep = [this](const QString& aName, const QString& aAccessible, const std::size_t aIndex) {
			auto* const step = new QToolButton{ ReviewBar };
			step->setObjectName(aName);
			step->setText(aAccessible);
			step->setAccessibleName(aAccessible);
			// [[BPR-1701]]. Seen and Read are checked buttons, as Accept's
			// retained action is.
			step->setCheckable(true);
			step->setEnabled(false);
			connect(step, &QToolButton::clicked, this, [this, aIndex] { take_progress_step(aIndex); });
			return step;
		};
		SeenStepButton   = makeStep(QStringLiteral("workbenchProgressSeenStep"), QStringLiteral("Seen"), progress_track::seen_step);
		ReadStepButton   = makeStep(QStringLiteral("workbenchProgressReadStep"), QStringLiteral("Read"), progress_track::read_step);
		AcceptFileButton = new QToolButton{ ReviewBar };
		AcceptFileButton->setObjectName(QStringLiteral("workbenchAcceptFileButton"));
		AcceptFileButton->setDefaultAction(ReceiptActions[2]);
		ProgressTrack = new progress_track{ *SeenStepButton, *ReadStepButton, *AcceptFileButton, *ReviewControlsButton, ReviewBar };
		ProgressTrack->setAccessibleName(QStringLiteral("Private progress"));
		auto* const trackAction = rowWidget(ReviewBar, ProgressTrack, QStringLiteral("workbenchProgressTrackAction"));
		ReviewBar->set_group_rails({
			{ .First = FilePositionAction, .Last = Navigation[1].Action },
			{ .First = ChangePositionAction, .Last = Navigation[3].Action },
			{ .First = trackAction, .Last = trackAction },
		});
		rowSpacer(ReviewBar, QStringLiteral("reviewBarSpacer"), QStringLiteral("reviewBarSpacerAction"));
		// [[BPR-1129]]. The two composition commands are the window's, so their
		// keys work from anywhere in it without a visible presenter; the bar and
		// the Pull Request menu present them. They keep the object names the
		// composition pages and their tests already know.
		const auto makeComposition = [this](const QString& aText, const QString& aName, const shortcut_command aKey, const bool aInline) {
			auto* const action = new QAction{ aText, this };
			action->setObjectName(aName);
			action->setIconText(action->text().remove(QLatin1Char{ '&' }));
			action->setShortcuts(shortcut_keys(aKey));
			action->setShortcutContext(Qt::WindowShortcut);
			addAction(action);
			connect(action, &QAction::triggered, this, [this, aInline] {
				// [[BPR-1133]]. Comment answers the selected thread when there
				// is one, and comments on the current line otherwise.
				if (aInline) {
					comment_or_reply();
				} else {
					open_review_composition(false);
				}
			});
			return action;
		};
		ComposeReviewAction = makeComposition(tr("Compose &review"), QStringLiteral("composeReviewAction"), shortcut_command::compose_review, false);
		CommentAction       = makeComposition(tr("Co&mment"), QStringLiteral("composeInlineDraftAction"), shortcut_command::comment, true);
		// [[BPR-1345]]. While a review is in progress the bar says so, with
		// how many comments it holds, and Finish review opens its summary in
		// Discuss (the start-a-review frames, owner's Accept 2026-10-08).
		auto* const chip = new review_progress_chip{ ReviewBar };
		ReviewChip       = chip;
		connect(chip, &QToolButton::clicked, this, [this] { open_review_composition(false); });
		ReviewChipAction = rowWidget(ReviewBar, chip, QStringLiteral("reviewBarReviewChipAction"));
		ReviewChipAction->setVisible(false);
		// [[BPR-1345]] v002. Finish review has a key of its own (owner,
		// finish-review v001: "find some hotkey for finish review, maybe
		// cmd+option+enter"), bound once, for the whole window.
		FinishReviewAction = new QAction{ tr("Finish review"), this };
		FinishReviewAction->setObjectName(QStringLiteral("finishReviewAction"));
		FinishReviewAction->setShortcuts(shortcut_keys(shortcut_command::finish_review));
		FinishReviewAction->setShortcutContext(Qt::WindowShortcut);
		FinishReviewAction->setEnabled(false);
		addAction(FinishReviewAction);
		connect(FinishReviewAction, &QAction::triggered, this, [this] { open_review_composition(false); });
		ReviewBar->addAction(ComposeReviewAction);
		ReviewBar->addAction(CommentAction);
		// [[BPR-1133]]. Comment's arrow opens its menu, which names what
		// Comment would start: "Comment on line 431…" or "Reply to tbutler's
		// thread…". It offers that plain comment alone. Labels are chosen in
		// the editor's chip, never here (owner, BPR-1083 comment-menu-v001).
		CommentMenu = new QMenu{ this };
		CommentMenu->setObjectName(QStringLiteral("commentMenu"));
		CommentMenu->addAction(CommentAction);
		connect(CommentMenu, &QMenu::aboutToShow, this, [this] { update_comment_target(); });
		if (auto* const commentButton = qobject_cast<QToolButton*>(ReviewBar->widgetForAction(CommentAction)); commentButton != nullptr) {
			commentButton->setMenu(CommentMenu);
			commentButton->setPopupMode(QToolButton::MenuButtonPopup);
		}
		PullRequestMenu->addSeparator();
		PullRequestMenu->addAction(ComposeReviewAction);
		PullRequestMenu->addAction(CommentAction);
		PrepareTourAction = new QAction{ tr("Prepare review &tour"), this };
		PrepareTourAction->setObjectName(QStringLiteral("prepareReviewTourAction"));
		PrepareTourAction->setShortcuts(shortcut_keys(shortcut_command::prepare_tour));
		PrepareTourAction->setEnabled(false);
		addAction(PrepareTourAction);
		PullRequestMenu->addAction(PrepareTourAction);
		connect(PrepareTourAction, &QAction::triggered, this, [this] { open_review_tour(); });
		AddTourStopAction = new QAction{ tr("Add tour stop"), this };
		AddTourStopAction->setObjectName(QStringLiteral("addTourStopAction"));
		AddTourStopAction->setShortcuts(shortcut_keys(shortcut_command::add_tour_stop));
		AddTourStopAction->setEnabled(false);
		addAction(AddTourStopAction);
		PullRequestMenu->addAction(AddTourStopAction);
		connect(PrepareTourAction, &QAction::changed, this, [this] { AddTourStopAction->setEnabled(PrepareTourAction->isEnabled()); });
		connect(AddTourStopAction, &QAction::triggered, this, [this] { TourEditor->trigger(tour_editor_action::add_stop); });
		// [[BPR-1212]]. Add comment and Suggest change, on the code's own
		// menu. Their keys are the code's, Return and Shift+Return, read by
		// the pane while it has focus (owner's Q1 answer). No window action
		// claims those keys; the menu's items only show them.
		AddCommentAction = new QAction{ tr("Add comment"), this };
		AddCommentAction->setObjectName(QStringLiteral("addCommentAction"));
		AddCommentAction->setIcon(conversation_icon(conversation_glyph::comment, palette()));
		connect(AddCommentAction, &QAction::triggered, this, [this] { static_cast<void>(comment_on_selected_lines(false)); });
		SuggestChangeAction = new QAction{ tr("Suggest change"), this };
		SuggestChangeAction->setObjectName(QStringLiteral("suggestChangeAction"));
		SuggestChangeAction->setIcon(conversation_icon(conversation_glyph::suggest, palette()));
		connect(SuggestChangeAction, &QAction::triggered, this, [this] { static_cast<void>(comment_on_selected_lines(true)); });
		// [[BPR-1594]]. The Agent tab is one key away (⌥⇧A); ⌘I asks it about
		// the selected lines, as the code's menu does.
		OpenAgentAction = new QAction{ tr("Agent"), this };
		OpenAgentAction->setObjectName(QStringLiteral("openAgentTabAction"));
		OpenAgentAction->setShortcuts(shortcut_keys(shortcut_command::open_agent));
		OpenAgentAction->setShortcutContext(Qt::WindowShortcut);
		OpenAgentAction->setToolTip(tr("Open the Agent tab"));
		addAction(OpenAgentAction);
		connect(OpenAgentAction, &QAction::triggered, this, [this] { show_agent(false); });
		AskAgentAction = new QAction{ tr("Ask about these lines"), this };
		AskAgentAction->setObjectName(QStringLiteral("askAgentAboutLinesAction"));
		AskAgentAction->setShortcuts(shortcut_keys(shortcut_command::ask_agent));
		AskAgentAction->setShortcutContext(Qt::WindowShortcut);
		AskAgentAction->setToolTip(tr("Ask the agent about the lines selected in the code"));
		addAction(AskAgentAction);
		connect(AskAgentAction, &QAction::triggered, this, [this] { show_agent(true); });
		update_compose_mark();
		update_strip_positions();
		update_strip_identity();
		apply_strip_width();
	}

	void workbench_window::build_central_surface()
	{
		CentralSplitter = new workbench_central_splitter{ this };
		CentralSplitter->setObjectName(QStringLiteral("workbenchCentralSplitter"));
		CentralSplitter->setAccessibleName(QStringLiteral("Review workbench"));
		CentralSplitter->setAccessibleDescription(QStringLiteral(
			"Resizable file drawer, comparison centre, and comments drawer"));
		// A drawer leaves through the core's drawer state, never through a drag
		// to zero width that presentation decided by itself.
		CentralSplitter->setChildrenCollapsible(false);

		CentreSurface = new QWidget;
		CentreSurface->setObjectName(QStringLiteral("workbenchCentralSurface"));
		CentreSurface->setAccessibleName(QStringLiteral("Review comparison"));
		CentreSurface->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

		// RFC-BPR-0003's review-history control is composed unchanged above the
		// panes. It already owns the independent left and right selectors over
		// arbitrary retained epochs, the complete copyable commit OIDs, and the
		// review-history loading, failure, cancellation, and empty wording, so
		// the shell adds no second selector, OID presentation, or vocabulary.
		// [[BPR-0737]] (owner, 2026-09-11): the epoch line and the "Comparing"
		// menu are one full-width row between the strip and the drawers, not
		// the centre pane's first widget, so its labels no longer move with
		// the drawer widths and the modality. The review surface is that row
		// over the central splitter; Home replaces the whole surface.
		ReviewSurface = new QWidget;
		ReviewSurface->setObjectName(QStringLiteral("workbenchReviewSurface"));
		EpochSelectors = new epoch_comparison_control{ { }, ReviewSurface };
		// An ignored horizontal hint, exactly as the drawer heading uses. The
		// row must never impose a minimum width that the drawer widths the
		// host persists cannot coexist with, and the window must stay usable at
		// the supported compact icon-row width.
		EpochSelectors->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		auto* const review = new QVBoxLayout{ ReviewSurface };
		review->setContentsMargins(0, 0, 0, 0);
		review->setSpacing(0);
		// Readiness uses the Updates track or the folded comparison summary,
		// so loading never inserts a row above the comparison.
		review->addWidget(ReviewBar);
		review->addWidget(EpochSelectors);
		review->addWidget(CentralSplitter, 1);
		if (auto* const comparing = EpochSelectors->release_comparing_button(); comparing != nullptr) {
			comparing->setAutoRaise(true);
			comparing->setParent(ReviewBar);
			ReviewBar->insertWidget(FilePositionAction, comparing)->setObjectName(QStringLiteral("reviewBarComparingAction"));
			ReviewBar->insertWidget(FilePositionAction, new strip_divider{ ReviewBar })
				->setObjectName(QStringLiteral("reviewBarComparisonDividerAction"));
			if (auto* const menu = comparing->menu(); menu != nullptr) {
				menu->addSeparator();
				menu->addAction(ReviewDetailsAction);
			}
		}
		// RFC-BPR-0004's workbench is composed unchanged. It keeps the single
		// shared vertical scrollbar, the single link map, and the natural-layout
		// documents, so the centre gains no second mapping, no regrouping, no
		// follower-scroll coupling, and no synthetic alignment rows.
		ComparisonSurface = new diff_workbench{ CentreSurface };
		ComparisonSurface->set_source_find_changed([this] { update_source_find_actions(); });
		for (auto* pane : { ComparisonSurface->left_pane(), ComparisonSurface->right_pane() }) {
			pane->setContextMenuPolicy(Qt::CustomContextMenu);
			connect(pane, &QWidget::customContextMenuRequested, this, [this, pane](const QPoint& aPosition) {
				auto* const menu = code_context_menu(pane, aPosition);
				menu->setAttribute(Qt::WA_DeleteOnClose);
				menu->popup(pane->viewport()->mapToGlobal(aPosition));
			});
		}
		auto* const centre = new QVBoxLayout{ CentreSurface };
		centre->setContentsMargins(0, 0, 0, 0);
		// [[BPR-0646]]. Home's landing, above the epoch line while there is
		// nothing to compare and no pull request: it funnels a reviewer to
		// the account or to a pull request rather than showing empty panes.
		// [[BPR-0646]] [[BPR-0650]]. Home is the whole centre while nothing
		// is open: the landing card and the review queue, and none of the
		// review chrome that means nothing before a pull request is chosen.
		HomePage = new QWidget;
		HomePage->setObjectName(QStringLiteral("workbenchHome"));
		HomePage->setAccessibleName(QStringLiteral("Home"));
		HomePage->setAccessibleDescription(QStringLiteral(
			"The bar, then the first-launch card or the way back to an open "
			"review, and the local review queue"));
		auto* const home = new QVBoxLayout{ HomePage };
		// [[BPR-0775]]. The bottom inset keeps Home's rows clear of Dif resting
		// in the corner beneath them.
		home->setContentsMargins(0, 0, 0, 16);
		home->setSpacing(0);
		HomeColumn  = home;
		HomeLanding = new workbench_landing{ HomePage };
		// [[BPR-0775]], [[BPR-0909]]. The card's three explicit ways in and the
		// row's way back are all it offers.
		HomeLanding->set_primary_dispatch([this] {
			if (HomeLanding->state() == landing_state::back_to_review) {
				choose_home(false);
				return;
			}
			if (HomeLanding->state() == landing_state::sign_in) {
				begin_selected_sign_in();
			}
		});
		HomeLanding->set_secondary_dispatch([this] {
			if (HomeLanding->state() == landing_state::sign_in) {
				begin_github_cli_from_home();
			}
		});
		HomeLanding->set_tertiary_dispatch([this] {
			if (HomeLanding->state() == landing_state::sign_in) {
				begin_enterprise_server_from_home();
			}
		});
		HomeLanding->set_close_dispatch([this] { close_review(); });
		home->addWidget(HomeLanding);
		// [[BPR-0647]]. The centre is a stack: the
		// overview for the Overview modality, the comparison for the rest.
		// The comparison keeps a page of its own with no margins, so every
		// geometry the probes pin on it is what it was.
		CentreStack = new QStackedWidget{ CentreSurface };
		CentreStack->setObjectName(QStringLiteral("workbenchCentreStack"));
		ComparisonPage = new QWidget{ CentreStack };
		ComparisonPage->setObjectName(QStringLiteral("workbenchComparisonPage"));
		auto* const comparisonColumn = new QVBoxLayout{ ComparisonPage };
		comparisonColumn->setContentsMargins(0, 0, 0, 0);
		comparisonColumn->setSpacing(0);
		// [[BPR-1132]]. RFC-BPR-0052: the comparison's header is the document
		// tab strip. It takes the diff status row's place, and says the status
		// at its right while the status has more to say than "ready".
		DocumentTabs = new document_tab_strip{ ComparisonPage };
		DocumentTabs->set_dispatch([this](const core::document_tab_intent& aIntent) {
			if (DocumentTabDispatch) {
				DocumentTabDispatch(aIntent);
			}
		});
		comparisonColumn->addWidget(DocumentTabs);
		// RFC-BPR-0089 M3. Over a split file's diff, its per-commit diffs; it
		// takes no room while hidden, so every other diff keeps its geometry.
		SplitStrip = new split_diff_strip{ ComparisonPage };
		comparisonColumn->addWidget(SplitStrip);
		comparisonColumn->addWidget(ComparisonSurface, 1);
		update_idle_direction();
		ComparisonSurface->set_status_observer([this](const QString& aText, const bool aNotable) {
			if (DocumentTabs != nullptr) {
				// [[BPR-1287]]. With nothing chosen the tab says so in the
				// empty state's words, not "Select a file comparison".
				DocumentTabs->set_status(ComparisonSurface->idle() ? ComparisonSurface->idle_page()->accessibleName() : aText, aNotable);
			}
			// [[BPR-1197]]. With no source text to read, the empty comparison
			// keeps the complete, wrapping and copyable explanation. The compact
			// tab status can elide; text comparisons keep their full reading area.
			const auto& result = ComparisonSurface->current_result();
			ComparisonSurface->set_status_shown(aNotable
												&& (!result || result->Alignment.Content != core::diff_content_state::text));
		});
		OverviewPage = new pull_request_overview_page{ CentreStack };
		OverviewPage->set_prepare_tour_action(PrepareTourAction);
		// [[BPR-1588]]. Status's tab row carries Open on GitHub too.
		if (OpenOnGithubAction != nullptr) {
			OverviewPage->set_open_on_github_action(OpenOnGithubAction);
		}
		// [[BPR-1588]]. Alt+Shift+6 collapses or expands Status while the
		// Overview shows, as the drawers' keys do elsewhere.
		ToggleStatusDrawerAction = new QAction{ tr("Status drawer"), this };
		ToggleStatusDrawerAction->setObjectName(QStringLiteral("toggleOverviewStatusAction"));
		ToggleStatusDrawerAction->setShortcuts(shortcut_keys(shortcut_command::status_drawer));
		ToggleStatusDrawerAction->setShortcutContext(Qt::WindowShortcut);
		ToggleStatusDrawerAction->setToolTip(tr("Hide or show the Status drawer (%1)").arg(shortcut_key(shortcut_command::status_drawer).toString(QKeySequence::NativeText)));
		ToggleStatusDrawerAction->setEnabled(false);
		addAction(ToggleStatusDrawerAction);
		connect(ToggleStatusDrawerAction, &QAction::triggered, this, [this] {
			if (OverviewPage != nullptr && CentreStack != nullptr && CentreStack->currentWidget() == OverviewPage) {
				OverviewPage->status_drawer()->toggle();
			}
		});
		OverviewPage->set_file_dispatch([this](const QString& aPath) { enter_review_at(aPath); });
		// [[BPR-1589]]. My review's Go to them: Threads, with Only new on.
		OverviewPage->set_new_threads_dispatch([this] { show_new_threads(); });
		// Its conversation line opens Discuss on the pull request.
		OverviewPage->set_new_conversation_dispatch([this] {
			if (Modality != nullptr && HasGithubPullRequest) {
				choose_home(false);
				choose_modality(review_modality::discuss);
			}
		});
		CentreStack->addWidget(ComparisonPage);
		CentreStack->addWidget(OverviewPage);
		CentreStack->setCurrentWidget(ComparisonPage);
		centre->addWidget(CentreStack, 1);

		Drawers[0].Panel = new workbench_drawer_panel{ core::workbench_drawer::files };
		Drawers[1].Panel = new workbench_drawer_panel{
			core::workbench_drawer::conversations
		};
		// [[BPR-1290]]. The inline drawer drags from 280 to 760 pixels, the
		// owner's accepted prototype's range; it opens at 375.
		Drawers[1].Panel->set_expanded_width_bounds(conversation_drawer_minimum_width, conversation_drawer_maximum_width);
		// Its header is its tabs: the drawer's name is its accessible name only.
		Drawers[1].Panel->set_heading_shown(false);
		// [[BPR-0651]]. The repositories tree: each repository the queue names
		// with its pull requests of interest; choosing one opens it on its
		// overview through the same intent the locator sends.
		Drawers[2].Panel = new workbench_drawer_panel{
			core::workbench_drawer::repositories
		};
		// [[BPR-0981]]. Its filter above it, resident over the rows the queue
		// projection already gave the tree; nothing here reaches a provider.
		auto* const repositoriesSurface = new QWidget;
		repositoriesSurface->setObjectName(QStringLiteral("workbenchRepositoriesSurface"));
		auto* const repositoriesColumn = new QVBoxLayout{ repositoriesSurface };
		repositoriesColumn->setContentsMargins(0, 0, 0, 0);
		repositoriesColumn->setSpacing(2);
		RepositoriesFilter = new QLineEdit{ repositoriesSurface };
		RepositoriesFilter->setObjectName(QStringLiteral("repositoriesFilter"));
		RepositoriesFilter->setAccessibleName(QStringLiteral("Filter pull requests"));
		RepositoriesFilter->setAccessibleDescription(QStringLiteral(
			"Show only repositories and pull requests whose name, title or number "
			"contains these letters in order; a matching pull request keeps its repository"));
		RepositoriesFilter->setPlaceholderText(QStringLiteral("Filter"));
		RepositoriesFilter->setClearButtonEnabled(true);
		repositoriesColumn->addWidget(RepositoriesFilter);
		RepositoriesFilterNotice = new filter_empty_notice{
			QStringLiteral("pull requests"), QStringLiteral("repositories"), repositoriesSurface
		};
		RepositoriesFilterNotice->hide();
		repositoriesColumn->addWidget(RepositoriesFilterNotice);
		RepositoriesTree = new repositories_tree_view{ repositoriesSurface };
		repositoriesColumn->addWidget(RepositoriesTree, 1);
		RepositoriesTree->set_saved_repository_dispatch(
			[this](application::saved_repository_intent aIntent) {
				if (SavedRepositoryDispatch && QueueReady) {
					SavedRepositoryDispatch(std::move(aIntent));
				}
			});
		RepositoriesTree->set_open_dispatch([this](const core::queue_item& aItem) {
			open_pull_request_item(aItem);
		});
		connect(RepositoriesFilter, &QLineEdit::textChanged, this, [this](const QString& aText) {
			RepositoriesTree->set_filter(aText);
			refresh_filter_notices();
		});
		connect(RepositoriesFilterNotice->clear_button(), &QAbstractButton::clicked, this, [this] {
			RepositoriesFilter->clear();
		});
		Drawers[2].Panel->set_content_widget(repositoriesSurface, RepositoriesTree);
		// [[BPR-0646]]. A rail is the collapsed drawer's one control, and
		// pressing it is the same gesture as the strip's toggle: the action
		// is triggered, and the core decides.
		for (std::size_t index = 0U; index < Drawers.size(); ++index) {
			connect(Drawers[index].Panel->rail(),
					&QAbstractButton::clicked,
					this,
					[this, index] { Drawers[index].Toggle->trigger(); });
			// [[BPR-0675]]. And the same gesture from the drawer's own
			// control, which is where a reviewer looks for it.
			connect(Drawers[index].Panel->collapse_button(),
					&QAbstractButton::clicked,
					this,
					[this, index] { Drawers[index].Toggle->trigger(); });
		}

		// Route 2 of BPR-0191. The conversations drawer shows RFC-BPR-0002's
		// threaded review conversation, which is a forest and cannot be a list,
		// so the drawer keeps its collapse, width and toggle behaviour and hosts
		// the tree instead of its own list model. The files drawer is unchanged:
		// a file list is a list.
		ConversationPanel = new github_conversation_panel{ Drawers[1].Panel };
		ConversationPanel->set_current_endpoint_placement(false, false);
		// [[BPR-0648]]. The cards read one geometry from the comparison: where
		// a line is. The window relays it, and relays every applied scroll
		// frame, so the cards sit at their lines and follow the shared scroll
		// without the drawer ever reading the panes itself.
		ConversationPanel->cards()->set_line_locator(
			[this](const core::diff_side aSide, const std::uint32_t aOneBasedLine)
				-> std::optional<int> {
				if (ComparisonSurface == nullptr || aOneBasedLine == 0U
					|| ConversationPanel == nullptr) {
					return std::nullopt;
				}
				const auto rect = ComparisonSurface->line_rect(aSide, aOneBasedLine - 1U);
				if (rect.isEmpty()) {
					return std::nullopt;
				}
				const auto inWindow = ComparisonSurface->mapTo(this, rect.topLeft());
				return ConversationPanel->cards()->viewport()->mapFrom(this, inWindow).y();
			});
		ComparisonSurface->set_frame_observer([this] { refresh_cards(); });
		// [[BPR-1290]]. The gutter's markers and the scroll bar's ticks follow
		// the open thread and the note under the pointer; a marker opens its
		// thread, and Shift-click on a tick goes to its thread. A wheel over
		// the notes scrolls the code.
		ComparisonSurface->set_thread_markers(true);
		ComparisonSurface->set_line_mark_dispatch([this](const core::diff_side aSide, const std::uint32_t aLine) { toggle_thread_at(aSide, aLine); });
		// [[BPR-1212]], [[BPR-1335]]. The "+" button selects its line, or the
		// lines a drag on it covered, as a click or a drag in the code would,
		// and starts a comment there. The core decides whether the lines can
		// take one; the status bar says why not.
		ComparisonSurface->set_line_add_dispatch([this](const std::uint32_t aFirst, const std::uint32_t aLast) {
			const auto& result = ComparisonSurface->current_result();
			const auto  head   = [&result](const std::uint32_t aLine) {
				return core::source_line_ref{ .Comparison = result->Alignment.Digest, .Side = core::diff_side::right, .Line = aLine };
			};
			const auto selected = aFirst == aLast
				? ComparisonSurface->select_conversation_line(core::diff_side::right, aLast)
				: result != nullptr && ComparisonSurface->restore_source_range(head(aFirst), head(aLast));
			if (selected) {
				static_cast<void>(comment_on_selected_lines(false));
			}
		});
		// [[BPR-1212]]. Return and Shift+Return in the code (owner, Q1): the
		// pane takes them only while it has focus, and they start the same
		// comment the right-click items do.
		ComparisonSurface->set_line_draft_dispatch([this](const bool aSuggest) { static_cast<void>(comment_on_selected_lines(aSuggest)); });
		ComparisonSurface->set_thread_tick_dispatch([this](const core::diff_side aSide, const std::uint32_t aLine) { go_to_thread_at(aSide, aLine); });
		// [[BPR-1336]]. ⌘-scroll and pinch take the same path as the menu.
		ComparisonSurface->set_code_zoom_dispatch([this](const core::code_zoom_step aStep) { dispatch_code_zoom(aStep); });
		ConversationPanel->cards()->set_marks_observer([this] {
			apply_thread_marks();
			present_reply_placement();
		});
		ConversationPanel->cards()->set_scroll_forward([this](QWheelEvent& aEvent) {
			return ComparisonSurface != nullptr && ComparisonSurface->scroll_by_wheel(aEvent);
		});
		ConversationPanel->cards()->set_reply_text_dispatch([this](const std::string& aThread, const QString& aText) {
			if (!dispatch_conversation_reply(core::conversation_reply_action::edit, aThread, aText)) {
				write_inline_reply(aThread, aText);
			}
		});
		ConversationPanel->cards()->set_draft_text_dispatch([this](const QString& aText) { write_inline_draft(aText); });
		// [[BPR-0661]]. A file marks itself read once fully seen only when the
		// preference says so; the shared scrollbar is what says "fully".
		if (auto* const scrollbar = ComparisonSurface->shared_scrollbar(); scrollbar != nullptr) {
			connect(scrollbar, &QScrollBar::valueChanged, this, [this](int) { consider_mark_read_when_fully_seen(); });
			connect(scrollbar, &QScrollBar::rangeChanged, this, [this](int, int) { consider_mark_read_when_fully_seen(); });
		}
		// [[BPR-1699]]. A thread's or a post's Mark all from its author
		// as read goes where Mark All Comments as Read does, with the login.
		ConversationPanel->cards()->set_author_read_dispatch([this](const std::string& aLogin) {
			const auto dispatch = ConversationsReadDispatch;
			if (dispatch) {
				dispatch(aLogin);
			}
		});
		ConversationPanel->cards()->set_action_dispatch(
			[this](const card_action aAction, const std::string& aIdentity) {
				// [[BPR-1046]]. A reply still waiting for its comparison is
				// given up by any later card gesture.
				DeferredReply.reset();
				// [[BPR-1326]]. Publish, Edit and Delete act on the editor's
				// draft. On another line's saved draft they first choose that
				// line, which brings its draft into the editor; nothing else
				// is published, opened or deleted.
				if ((aAction == card_action::publish_draft || aAction == card_action::edit_draft || aAction == card_action::delete_draft)
					&& aIdentity != "draft:unpublished") {
					const auto& cards = ConversationPanel->cards()->cards();
					const auto  card  = std::ranges::find(cards, aIdentity, &thread_card::Id);
					if (card != cards.end() && card->Draft && card->Side.has_value() && card->Line.value_or(0U) > 0U && ComparisonSurface != nullptr) {
						const auto side = *card->Side;
						const auto line = *card->Line - 1U;
						static_cast<void>(ComparisonSurface->select_conversation_line(side, line));
						// [[BPR-1212]]. Opening it writes it in the drawer.
						if (aAction == card_action::edit_draft && ReviewEditor != nullptr && ReviewEditor->shown_draft().has_value()
							&& ReviewEditor->shown_draft()->Anchor.Side == side && ReviewEditor->shown_draft()->Anchor.Line == line) {
							open_review_composition(true);
						}
					}
					return;
				}
				switch (aAction) {
				case card_action::keep_reply_as_comment:
				case card_action::discard_reply:
					act_on_kept_reply(aAction, aIdentity);
					break;
				case card_action::copy_reply:
					// [[BPR-1374]]. The notes copied the words themselves.
					break;
				case card_action::defect:
					ConversationPanel->cards()->show_thread_menu(aIdentity);
					break;
				case card_action::publish_draft:
					// [[BPR-1212]]. Publish posts this one comment now; the
					// drawer closes the composer once the draft is published.
					if (ReviewEditor != nullptr && ReviewEditor->publish_action()->isEnabled()) {
						ReviewEditor->publish_action()->trigger();
					}
					break;
				case card_action::edit_draft:
					// [[BPR-1212]]. Opening a kept draft makes it editable again,
					// in the drawer.
					if (ReviewEditor != nullptr) {
						open_review_composition(true);
					}
					break;
				case card_action::delete_draft:
					// [[BPR-1345]] v002. Delete asks first when there are
					// words to lose.
					confirm_delete_drawer_draft();
					break;
				case card_action::finish_review:
					if (FinishReviewAction != nullptr && FinishReviewAction->isEnabled()) {
						FinishReviewAction->trigger();
					}
					break;
				case card_action::maximize_draft:
					maximize_drawer_draft();
					break;
				case card_action::close_draft:
					leave_drawer_draft(false);
					break;
				case card_action::review_draft:
					// [[BPR-1345]]. Start a review, or Add review comment: the
					// draft stays as written, pending in the review, and the
					// composer closes on it. Nothing is published.
					if (StartReviewDispatch && StartReviewDispatch()) {
						leave_drawer_draft(false);
					}
					break;
				case card_action::mark_read:
					// [[BPR-0656]]. The ledger host advances the cursor and
					// the read state comes back through present_read_state.
					if (ConversationReadDispatch) {
						ConversationReadDispatch(aIdentity);
					}
					break;
				case card_action::mark_unread:
					if (ConversationUnreadDispatch) {
						ConversationUnreadDispatch(aIdentity);
					}
					break;
				case card_action::resolve:
				case card_action::unresolve: {
					// [[BPR-0657]]. What the window knows travels with the
					// gesture: the thread's state as projected and the server's
					// capability for the direction asked; the composition adds
					// what only it knows and decides.
					if (!ThreadResolutionDispatch) {
						break;
					}
					const auto resolve  = aAction == card_action::resolve;
					auto       resolved = false;
					if (const auto index = ConversationPanel->model()->index_for_id(aIdentity); index.isValid()) {
						resolved = index.data(github_conversation_model::resolved_role).toBool();
					}
					ThreadResolutionDispatch(thread_resolution_gesture{
						.Identity   = aIdentity,
						.Resolve    = resolve,
						.Resolved   = resolved,
						.Capability = resolve ? Capabilities.ResolveThread : Capabilities.UnresolveThread,
					});
					break;
				}
				case card_action::reply:
					if (dispatch_conversation_reply(core::conversation_reply_action::open, aIdentity)) {
						break;
					}
					// [[BPR-1133]]. Beside the code as in Discuss: every card
					// answers its own thread, through the one Reply path;
					// [[BPR-1290]] written in place, in the open thread.
					reply_to_thread(aIdentity, true);
					break;
				case card_action::show_line:
					reveal_thread_line(aIdentity);
					break;
				case card_action::publish_reply:
					if (dispatch_conversation_reply(core::conversation_reply_action::send, aIdentity)) {
						break;
					}
					// [[BPR-1290]]. The reply in place is the draft editor's
					// draft; nothing publishes but its own Publish.
					if (ReviewEditor != nullptr && ReviewEditor->publish_action()->isEnabled()) {
						ReviewEditor->publish_action()->trigger();
					}
					break;
				case card_action::cancel_reply:
					if (dispatch_conversation_reply(core::conversation_reply_action::close, aIdentity)) {
						ConversationPanel->cards()->close_thread();
						break;
					}
					// Cancel gives up what was typed, through the editor's own Discard.
					if (ReviewEditor != nullptr && ReviewEditor->shown_draft().has_value()
						&& ReviewEditor->shown_draft()->ReplyToThread.has_value()
						&& !ReviewEditor->body_editor()->toPlainText().isEmpty()
						&& ReviewEditor->discard_action()->isEnabled()) {
						ReviewEditor->discard_action()->trigger();
					}
					break;
				case card_action::open_in_code:
					// [[BPR-1383]]. The same exact pick as Threads: open
					// Comments and retain the thread until its file loads.
					// [[BPR-1581]]: through the one route, so an outdated
					// thread opens among its file's unplaced ones.
					static_cast<void>(move_thread(aIdentity, core::workbench_thread_origin::discuss, core::workbench_thread_destination::review));
					break;
				case card_action::show_in_discuss:
					// [[BPR-1581]]. The thread, or the post its menu was on.
					static_cast<void>(move_thread(aIdentity, core::workbench_thread_origin::review, core::workbench_thread_destination::discuss));
					break;
				case card_action::show_in_threads:
					// [[BPR-1586]]. Its row in Threads, at that post.
					static_cast<void>(move_thread(aIdentity, core::workbench_thread_origin::review, core::workbench_thread_destination::threads));
					break;
				}
			});
		ConversationPanel->cards()->set_excerpt_locator(
			[this](const QString& aPath, const core::diff_side aSide, const std::uint32_t aLine)
				-> std::optional<QString> {
				// Only the compared file's text is on hand: one line each
				// side of the thread's, from the pane on the thread's side.
				if (ComparisonSurface == nullptr || ComparisonSurface->current_result() == nullptr
					|| !ComparisonSurface->current_context().has_value()
					|| ComparisonSurface->current_context()->Key.File != core::file_identity_of(aPath.toStdString())) {
					return std::nullopt;
				}
				auto* const pane = aSide == core::diff_side::left ? ComparisonSurface->left_pane()
																  : ComparisonSurface->right_pane();
				if (pane == nullptr || pane->document() == nullptr) {
					return std::nullopt;
				}
				auto lines = QStringList{ };
				for (auto number = static_cast<int>(aLine) - 2; number <= static_cast<int>(aLine); ++number) {
					if (number < 0) {
						continue;
					}
					const auto block = pane->document()->findBlockByNumber(number);
					if (block.isValid()) {
						lines.push_back(block.text());
					}
				}
				return lines.isEmpty() ? std::nullopt : std::optional{ lines.join(QLatin1Char{ '\n' }) };
			});
		// [[BPR-1564]]. A suggestion's commented lines, exactly as the compared
		// document holds them, when the thread's file is the one compared.
		ConversationPanel->cards()->set_suggestion_locator(
			[this](const QString& aPath, const core::diff_side aSide, const std::uint32_t aFirst, const std::uint32_t aLast)
				-> std::optional<std::vector<std::string>> {
				if (ComparisonSurface == nullptr || ComparisonSurface->current_result() == nullptr || !ComparisonSurface->current_context().has_value()
					|| ComparisonSurface->current_context()->Key.File != core::file_identity_of(aPath.toStdString()) || aFirst == 0U || aLast < aFirst) {
					return std::nullopt;
				}
				const auto& result   = *ComparisonSurface->current_result();
				const auto& document = aSide == core::diff_side::right ? result.RightDocument : result.LeftDocument;
				if (!document.has_value()) {
					return std::nullopt;
				}
				return core::suggestion_source_lines(*document, aFirst - 1U, aLast - 1U);
			});
		ComparisonSurface->set_selection_observer(
			[this](const std::optional<core::semantic_selection>& aSelection,
				   const bool                                     aInteractive) {
				if (ConversationPanel == nullptr || ComparisonSurface == nullptr) {
					return;
				}
				// [[BPR-0929]]. Discuss is a PR-wide reading surface. The change
				// controls may continue to move the retained comparison underneath
				// it, but that code navigation must not replace or clear the
				// conversation identity the reviewer is reading.
				// [[BPR-1293]]. So is Threads: the arrows move the
				// comparison underneath, not the finder's cursor.
				if (Modality != nullptr
					&& (Modality->current() == review_modality::discuss || Modality->current() == review_modality::threads)) {
					update_strip_positions();
					update_home_landing();
					return;
				}
				const auto endpointIsCurrent = aSelection.has_value()
					&& (aSelection->PreferredLine.Side == core::diff_side::left
							? LeftConversationEndpointCurrent
							: RightConversationEndpointCurrent);
				const auto line            = aInteractive && aSelection.has_value()
						&& aSelection->Kind == core::semantic_selection_kind::line
						&& (endpointIsCurrent || ConversationPanel->has_semantic_anchor(aSelection->PreferredLine))
					? std::optional<core::source_line_ref>{ aSelection->PreferredLine }
					: std::nullopt;
				ApplyingConversationAnchor = true;
				ConversationPanel->set_anchor_file(conversation_anchor_file());
				ConversationPanel->set_anchor_line(line);
				refresh_cards();
				if (aInteractive && !line.has_value()) {
					ConversationPanel->view()->setCurrentIndex({ });
					ConversationPanel->view()->selectionModel()->clearSelection();
				}
				ApplyingConversationAnchor = false;
				if (aInteractive && !line.has_value()) {
					update_defect_surface();
				}
				update_strip_positions();
				update_home_landing();
				update_comment_target();
			});
		ReviewSubmissionEditor = new review_submission_editor{
			[this](review_submission_editor_intent aIntent) {
				if (ReviewSubmissionDispatch) {
					ReviewSubmissionDispatch(std::move(aIntent));
				}
			},
			Drawers[1].Panel
		};
		ReviewEditor = new review_draft_editor{
			[this](core::review_draft_command aCommand) {
				if (ReviewDraftDispatch) {
					ReviewDraftDispatch(std::move(aCommand));
				}
			},
			[this](core::file_receipt_command aCommand) {
				if (ReceiptDispatch) {
					ReceiptDispatch(aCommand);
				}
			},
			Drawers[1].Panel,
		};
		ReviewEditor->setObjectName(QStringLiteral("workbenchReviewDraftEditor"));
		ReviewEditor->set_presentation_observer([this] {
			update_comment_target();
			present_inline_reply();
			present_inline_draft();
			// [[BPR-1212]]. A comment on the selected lines waiting for its
			// line's draft opens once it is here.
			static_cast<void>(resume_line_comment());
			// [[BPR-1590]] v003. So does a pending comment opened from
			// Finish your review.
			resume_pending_comment_open();
		});
		connect(ReviewEditor->body_editor(), &QPlainTextEdit::textChanged, this, [this] {
			present_inline_reply();
			present_inline_draft();
		});
		connect(ReviewEditor->publish_action(), &QAction::changed, this, [this] {
			present_inline_reply();
			present_inline_draft();
		});
		// [[BPR-1083]]. Marks are drawn from the start, in the built-in tints
		// until the composition supplies the theme's.
		apply_label_marks();
		DefectDialog = new QDialog{ this };
		DefectDialog->setObjectName(QStringLiteral("threadDefectDialog"));
		DefectDialog->setWindowTitle(tr("Thread defect"));
		DefectDialog->setWindowModality(Qt::WindowModal);
		auto* defectLayout = new QVBoxLayout{ DefectDialog };
		DefectCard         = new defect_card_view{
			[this](core::defect_command aCommand) {
				++Statistics.DefectIntents;
				if (DefectDispatch) {
					DefectDispatch(std::move(aCommand));
				}
			},
			DefectDialog,
		};
		DefectCard->setObjectName(QStringLiteral("workbenchDefectCard"));
		defectLayout->addWidget(DefectCard);
		auto* applyClassification = new QToolButton{ DefectDialog };
		applyClassification->setDefaultAction(DefectCard->reclassify_action());
		defectLayout->addWidget(applyClassification);
		auto* defectClose = new QDialogButtonBox{ QDialogButtonBox::Close, DefectDialog };
		connect(defectClose, &QDialogButtonBox::rejected, DefectDialog, &QDialog::hide);
		defectLayout->addWidget(defectClose);
		ConversationPanel->cards()->set_context_menu_builder([this](QMenu& aMenu, const std::string& aThread) { append_thread_defect_menu(aMenu, aThread); });
		ClassifyDefect = new QAction{
			tr("Classify selected thread as defect"), this
		};
		ClassifyDefect->setObjectName(QStringLiteral("classifySelectedDefect"));
		PreviousDefect = new QAction{ tr("Previous defect"), this };
		PreviousDefect->setObjectName(QStringLiteral("previousDefect"));
		NextDefect = new QAction{ tr("Next defect"), this };
		NextDefect->setObjectName(QStringLiteral("nextDefect"));
		connect(ClassifyDefect, &QAction::triggered, this, [this] {
			classify_selected_conversation();
		});
		connect(PreviousDefect, &QAction::triggered, this, [this] {
			navigate_defect(-1);
		});
		connect(NextDefect, &QAction::triggered, this, [this] {
			navigate_defect(1);
		});
		// [[BPR-1134]]. RFC-BPR-0052's right tray: tabs in the drawer's header,
		// each tab's tools beneath them. The Comments tab holds the conversation
		// with its scope and filters, and Next new comment; its host keeps the
		// panel's place while Discuss borrows it for the centre (the drawers
		// are hidden then, so the tab needs no notice of its own).
		auto* const commentsContents = new QWidget{ Drawers[1].Panel };
		commentsContents->setObjectName(QStringLiteral("workbenchCommentsTab"));
		auto* const commentsLayout = new QVBoxLayout{ commentsContents };
		commentsLayout->setContentsMargins(0, 0, 0, 0);
		// [[BPR-1290]]. Next new reply sits on the tab row with the filters;
		// the defect queue's row shows only while there are defects.
		NextNewCommentButton = new QToolButton{ commentsContents };
		NextNewCommentButton->setObjectName(QStringLiteral("workbenchNextNewCommentButton"));
		NextNewCommentButton->setDefaultAction(NextNewCommentAction);
		NextNewCommentButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
		NextNewCommentButton->setAutoRaise(true);
		NextNewCommentButton->setIconSize(QSize{ 16, 16 });
		NextNewCommentButton->setFocusPolicy(Qt::StrongFocus);
		NextNewCommentButton->setAccessibleName(QStringLiteral("Next new comment"));
		NextNewCommentButton->setAccessibleDescription(QStringLiteral(
			"Reveal the next thread with a post newer than what you have read"));
		DefectRow = new QWidget{ commentsContents };
		DefectRow->setObjectName(QStringLiteral("workbenchDefectRow"));
		auto* const commentsTools = new QHBoxLayout{ DefectRow };
		commentsTools->setContentsMargins(4, 2, 4, 2);
		commentsTools->addStretch(1);
		DefectCountLabel = new QLabel{ QStringLiteral("Defects 0/0"), commentsContents };
		DefectCountLabel->setObjectName(QStringLiteral("defectCount"));
		DefectCountLabel->setAccessibleName(QStringLiteral("Defect queue position"));
		commentsTools->addWidget(DefectCountLabel);
		PreviousDefectButton = new QToolButton{ commentsContents };
		NextDefectButton     = new QToolButton{ commentsContents };
		PreviousDefect->setIcon(style()->standardIcon(QStyle::SP_ArrowBack));
		NextDefect->setIcon(style()->standardIcon(QStyle::SP_ArrowForward));
		for (const auto& entry : { std::pair{ PreviousDefectButton, PreviousDefect }, std::pair{ NextDefectButton, NextDefect } }) {
			entry.first->setObjectName(entry.second->objectName() + QStringLiteral("Button"));
			entry.first->setDefaultAction(entry.second);
			entry.first->setToolButtonStyle(Qt::ToolButtonIconOnly);
			entry.first->setAccessibleName(entry.second->text());
			entry.first->setAccessibleDescription(tr("Select the %1 thread in Comments").arg(entry.second == PreviousDefect ? tr("previous defect") : tr("next defect")));
			entry.first->setToolTip(entry.second->text());
			entry.first->setFocusPolicy(Qt::StrongFocus);
			commentsTools->addWidget(entry.first);
		}
		commentsLayout->addWidget(DefectRow);
		DefectRow->hide();
		// [[BPR-1132]]. A tab over an older pair places only its own
		// revisions' threads; this says how many of the file's are on later
		// updates and routes to them.
		OlderPairNotice = new QWidget{ commentsContents };
		OlderPairNotice->setObjectName(QStringLiteral("olderPairNotice"));
		auto* const olderPair = new QVBoxLayout{ OlderPairNotice };
		olderPair->setContentsMargins(8, 6, 8, 6);
		olderPair->setSpacing(4);
		OlderPairText = new QLabel{ OlderPairNotice };
		OlderPairText->setObjectName(QStringLiteral("olderPairNoticeText"));
		OlderPairText->setAccessibleName(QStringLiteral("Threads on later updates"));
		OlderPairText->setTextFormat(Qt::PlainText);
		OlderPairText->setWordWrap(true);
		ShowLaterThreads = new QPushButton{ tr("Show them"), OlderPairNotice };
		ShowLaterThreads->setObjectName(QStringLiteral("showLaterThreads"));
		ShowLaterThreads->setAccessibleDescription(QStringLiteral(
			"Shows this file's threads on later updates"));
		olderPair->addWidget(OlderPairText);
		olderPair->addWidget(ShowLaterThreads, 0, Qt::AlignLeft);
		OlderPairNotice->hide();
		commentsLayout->addWidget(OlderPairNotice);
		connect(ShowLaterThreads, &QPushButton::clicked, this, [this] { show_later_threads(); });
		auto* const conversationHost   = new QWidget{ commentsContents };
		auto* const conversationLayout = new QVBoxLayout{ conversationHost };
		ConversationLayout             = conversationLayout;
		conversationLayout->setContentsMargins(0, 0, 0, 0);
		conversationLayout->addWidget(ConversationPanel, 1);
		commentsLayout->addWidget(conversationHost, 1);
		// The private tab holds AI tools and findings; thread controls belong
		// to the thread menus and Comments navigation.
		auto* const conversationContents = new QWidget{ Drawers[1].Panel };
		conversationContents->setObjectName(QStringLiteral("workbenchNotesTab"));
		auto* const findingsLayout = new QVBoxLayout{ conversationContents };
		findingsLayout->setContentsMargins(0, 0, 0, 0);
		DiscussPage = new QWidget{ CentreStack };
		DiscussPage->setObjectName(QStringLiteral("workbenchDiscussPage"));
		auto* discussLayout = new QHBoxLayout{ DiscussPage };
		discussLayout->setContentsMargins(0, 0, 0, 0);
		discussLayout->setSpacing(0);
		// [[BPR-1212]]. Discuss is the home of the full composer: the
		// conversations at the left, the draft or the review summary with
		// room at the right (owner, 2026-10-06: "Fold add comment window
		// into discuss").
		DiscussComposer = new QWidget{ DiscussPage };
		DiscussComposer->setObjectName(QStringLiteral("discussComposer"));
		DiscussComposer->setAccessibleName(tr("Composer"));
		auto* const composerColumn = new QVBoxLayout{ DiscussComposer };
		composerColumn->setContentsMargins(0, 0, 0, 0);
		composerColumn->setSpacing(0);
		DiscussComposer->hide();
		discussLayout->addWidget(DiscussComposer, 1);
		// [[BPR-1584]]. Discuss's notes keep the page's left edge: past the
		// reading width the rest of the page is this stretch's.
		discussLayout->addStretch(0);
		CentreStack->addWidget(DiscussPage);
		// [[BPR-1293]]. Threads: the finder fills the centre, as
		// Discuss's page does, and the drawers are hidden while it shows.
		ThreadsPage = new QWidget{ CentreStack };
		ThreadsPage->setObjectName(QStringLiteral("workbenchThreadsPage"));
		auto* const conversationsLayout = new QVBoxLayout{ ThreadsPage };
		conversationsLayout->setContentsMargins(0, 0, 0, 0);
		Finder = new conversation_finder{ ThreadsPage };
		conversationsLayout->addWidget(Finder);
		// The drawers are hidden while it shows, so the page always has the
		// window's whole width; as a page of the stack it must not widen the
		// centre's floor while Review shows its drawers.
		ThreadsPage->setMinimumSize(1, 1);
		CentreStack->addWidget(ThreadsPage);
		Finder->set_pick_dispatch([this](const finder_location& aLocation, const core::workbench_thread_destination aDestination) {
			static_cast<void>(open_thread_from_threads(aLocation.Thread, aLocation.Post, aDestination));
		});
		Finder->set_leave_dispatch([this] { leave_threads(); });
		// [[BPR-1582]]. The row menu's Mark as read is the card's Mark read.
		Finder->set_read_dispatch([this](const std::string& aThread, const bool aRead) {
			if (aRead && ConversationReadDispatch) {
				ConversationReadDispatch(aThread);
			} else if (!aRead && ConversationUnreadDispatch) {
				ConversationUnreadDispatch(aThread);
			}
		});
		// [[BPR-1699]]. Mark All from an author as Read, from a row.
		Finder->set_author_read_dispatch(
			[this](const std::string& aLogin) {
				const auto dispatch = ConversationsReadDispatch;
				if (dispatch) {
					dispatch(aLogin);
				}
			},
			[this](const QString& aLogin) { return ConversationPanel->cards()->unread_post_count_from(aLogin); });
		// [[BPR-1356]]. The notice that says which filters a pick cleared
		// goes once the reviewer changes one.
		ConversationPanel->set_discuss_filters_changed([this] {
			if (ThreadsRevealNotice) {
				ThreadsRevealNotice = false;
				ConversationPanel->present_notice(QString{ });
			}
		});
		const auto makeCompositionPage = [this](QWidget* aEditor, const QString& aName, const QString& aBack, QWidget* aExcerpt) {
			// [[BPR-1212]]. A composition page is a column of Discuss, beside
			// the conversations, not a page of its own.
			auto* scroll = new QScrollArea{ DiscussComposer };
			scroll->setObjectName(aName);
			scroll->setWidgetResizable(true);
			scroll->setFrameShape(QFrame::NoFrame);
			scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			auto* page   = new QWidget{ scroll };
			auto* column = new QVBoxLayout{ page };
			column->setContentsMargins(24, 12, 24, 24);
			column->setSpacing(10);
			auto* const bar  = new QHBoxLayout{ };
			auto*       back = new QPushButton{ aBack, page };
			back->setObjectName(aName + QStringLiteral("Back"));
			back->setIcon(conversation_icon(conversation_glyph::minimize, palette()));
			// [[BPR-1345]] v002. ⌘⇧M, as Maximize took the draft here; ⌘⇧↩
			// is Add single comment now.
			const auto key = shortcut_key(shortcut_command::draft_to_discuss).toString(QKeySequence::NativeText);
			back->setToolTip(tr("%1, without publishing  %2 or Esc").arg(aBack, key));
			back->setAccessibleDescription(tr("Return to the previous surface without submitting"));
			connect(back, &QPushButton::clicked, this, [this] { close_review_composition(); });
			// [[BPR-1129]]. Esc is Back, from anywhere on the page; so is
			// ⌘⇧M, the key that brought the draft here ([[BPR-1212]];
			// ⌘⇧↩ until [[BPR-1345]] v002).
			auto* const escape = new QShortcut{ shortcut_key(shortcut_command::leave_composer), scroll };
			escape->setObjectName(aName + QStringLiteral("Escape"));
			escape->setContext(Qt::WidgetWithChildrenShortcut);
			connect(escape, &QShortcut::activated, this, [this] { close_review_composition(); });
			auto* const restore = new QShortcut{ shortcut_key(shortcut_command::draft_to_discuss), scroll };
			restore->setObjectName(aName + QStringLiteral("Restore"));
			restore->setContext(Qt::WidgetWithChildrenShortcut);
			connect(restore, &QShortcut::activated, this, [this] { close_review_composition(); });
			auto* const title = new QLabel{ page };
			title->setObjectName(aName + QStringLiteral("Title"));
			title->setTextFormat(Qt::PlainText);
			// [[BPR-1212]] v002. The restore sits at the top right, where the
			// drawer draft's Maximize was (owner, maximize-to-discuss-v001:
			// "the restore (back to code) should still be in the top right").
			back->setFlat(true);
			back->setLayoutDirection(Qt::RightToLeft);
			bar->addWidget(title);
			bar->addStretch(1);
			bar->addWidget(back);
			column->addLayout(bar);
			if (aExcerpt != nullptr) {
				aExcerpt->setParent(page);
				column->addWidget(aExcerpt);
			}
			column->addWidget(aEditor, 1);
			scroll->setWidget(page);
			scroll->hide();
			DiscussComposer->layout()->addWidget(scroll);
			return scroll;
		};
		DraftExcerpt      = new draft_excerpt_view{ DiscussComposer };
		ComposeReviewPage = makeCompositionPage(ReviewSubmissionEditor, QStringLiteral("composeReviewPage"), tr("Back"), nullptr);
		ComposeInlinePage = makeCompositionPage(ReviewEditor, QStringLiteral("composeInlineDraftPage"), tr("Back to the code"), DraftExcerpt);
		// [[BPR-1590]]. RFC-BPR-0085 D6, the owner's "C with an expand to A":
		// Finish your review is a sheet at the review area's right edge, over
		// whatever the modality shows; Expand makes it the focused page in
		// Discuss, whose Restore brings the sheet back. The composer is one
		// widget, lent to whichever is open.
		ReviewSheet = new compose_review_sheet{ ReviewSurface };
		ReviewSheet->scroll()->viewport()->setBackgroundRole(QPalette::Base);
		{
			const auto expandKey = shortcut_key(shortcut_command::draft_to_discuss).toString(QKeySequence::NativeText);
			const auto closeKey  = shortcut_key(shortcut_command::leave_composer).toString(QKeySequence::NativeText);
			ReviewSheet->set_tools(conversation_icon(conversation_glyph::maximize, palette()), tr("Expand to a page  %1").arg(expandKey), [this] { expand_review_sheet(); }, drawer_icon(drawer_glyph::close, palette()), tr("Close  %1").arg(closeKey), [this] { close_review_sheet(); });
			auto* const sheetEscape = new QShortcut{ shortcut_key(shortcut_command::leave_composer), ReviewSheet };
			sheetEscape->setObjectName(QStringLiteral("composeReviewSheetEscape"));
			sheetEscape->setContext(Qt::WidgetWithChildrenShortcut);
			connect(sheetEscape, &QShortcut::activated, this, [this] { close_review_sheet(); });
			auto* const sheetExpand = new QShortcut{ shortcut_key(shortcut_command::draft_to_discuss), ReviewSheet };
			sheetExpand->setObjectName(QStringLiteral("composeReviewSheetExpandKey"));
			sheetExpand->setContext(Qt::WidgetWithChildrenShortcut);
			connect(sheetExpand, &QShortcut::activated, this, [this] { expand_review_sheet(); });
		}
		if (auto* const restore = ComposeReviewPage->findChild<QPushButton*>(QStringLiteral("composeReviewPageBack")); restore != nullptr) {
			const auto key = shortcut_key(shortcut_command::draft_to_discuss).toString(QKeySequence::NativeText);
			restore->setText(tr("Restore"));
			restore->setToolTip(tr("Back to the sheet beside the code  %1 or Esc").arg(key));
			restore->setAccessibleDescription(tr("Return to the review sheet without submitting"));
		}
		CentralSplitter->installEventFilter(this);
		ComposeReviewPage->viewport()->installEventFilter(this);
		// Keep the decision and its illustration visible while the before/after
		// scrolls. Mascot preferences must never hide these semantic controls.
		WorkshopHeader = new QWidget{ CentreSurface };
		WorkshopHeader->setObjectName(QStringLiteral("workbenchWorkshopHeader"));
		auto* workshopEscape = new QShortcut{ shortcut_key(shortcut_command::leave_composer), WorkshopHeader };
		workshopEscape->setObjectName(QStringLiteral("workbenchWorkshopHeaderEscape"));
		workshopEscape->setContext(Qt::WidgetWithChildrenShortcut);
		connect(workshopEscape, &QShortcut::activated, this, [this] { close_review_composition(); });
		auto* workshopHeader = new QHBoxLayout{ WorkshopHeader };
		// [[BPR-1347]]. One compact row: Dif first, beside what he
		// illustrates, then the workshop's name and its decision. The strut
		// keeps the row's height when Dif is hidden, and nothing is left where
		// he was.
		auto margins = workshopHeader->contentsMargins();
		margins.setTop(4);
		margins.setBottom(4);
		workshopHeader->setContentsMargins(margins);
		workshopHeader->addStrut(dif_workshop_size);
		DifWorkshopSlot = new QWidget{ WorkshopHeader };
		DifWorkshopSlot->setObjectName(QStringLiteral("workbenchDifWorkshopSlot"));
		DifWorkshopSlot->setAccessibleName(tr("Decorative Dif workshop illustration"));
		DifWorkshopSlot->setFixedSize(dif_workshop_size, dif_workshop_size);
		auto* workshopIllustration = new QHBoxLayout{ DifWorkshopSlot };
		workshopIllustration->setContentsMargins(0, 0, 0, 0);
		workshopHeader->addWidget(DifWorkshopSlot);
		auto* const workshopTitle = new QLabel{ tr("Comment workshop"), WorkshopHeader };
		workshopTitle->setObjectName(QStringLiteral("workbenchWorkshopTitle"));
		workshopHeader->addWidget(workshopTitle);
		for (auto* action : { ReviewEditor->accept_revision_action(), ReviewEditor->reject_revision_action() }) {
			auto* button = new QToolButton{ WorkshopHeader };
			button->setObjectName(action->objectName() + QStringLiteral("Button"));
			button->setDefaultAction(action);
			button->setToolButtonStyle(Qt::ToolButtonTextOnly);
			button->setFocusPolicy(Qt::StrongFocus);
			workshopHeader->addWidget(button);
			connect(action, &QAction::changed, this, [this] { update_dif_presentation(); });
		}
		workshopHeader->addStretch();
		WorkshopHeader->hide();
		centre->insertWidget(centre->indexOf(CentreStack), WorkshopHeader);
		// [[BPR-1129]]. The kept-draft mark follows what the workflow presents.
		ReviewSubmissionEditor->set_presentation_observer([this] { update_compose_mark(); });
		// [[BPR-1085]]. A preflight row links back to its exact item: my unsent
		// draft opens its page; a conversation is revealed in Discuss.
		// [[BPR-1345]] v002. A pending row goes to its comment; its Delete,
		// once confirmed, goes to the composition; a confirmed Discard review
		// closes the page.
		ReviewSubmissionEditor->set_pending_reveal([this](const core::review_submission_comment& aComment) {
			open_pending_comment(aComment, core::workbench_thread_destination::review);
		});
		// [[BPR-1590]] v003. Threads' gestures on a pending row.
		ReviewSubmissionEditor->set_pending_open([this](const core::review_submission_comment& aComment, const core::workbench_thread_destination aDestination) {
			open_pending_comment(aComment, aDestination);
		});
		ReviewSubmissionEditor->set_pending_delete([this](const core::review_submission_comment& aComment) {
			if (PendingCommentDeleteDispatch) {
				PendingCommentDeleteDispatch(aComment.Draft, aComment.DraftRevision);
			}
		});
		ReviewSubmissionEditor->set_discard_observer([this] {
			if (CompositionPage != nullptr && CompositionPage == ComposeReviewPage) {
				close_review_composition(false);
			}
			close_review_sheet();
		});
		// [[BPR-1345]] v003. A review GitHub confirmed closes the page and
		// returns to the view the reviewer came from, Review or Discuss,
		// with a brief line saying so (owner, finish-page v002). A refused or
		// unknown answer keeps the page and its recovery.
		ReviewSubmissionEditor->set_submitted_observer([this](const std::size_t aComments) {
			if (CompositionPage != nullptr && CompositionPage == ComposeReviewPage) {
				close_review_composition(false);
			}
			close_review_sheet();
			const auto said = aComments == 0U ? tr("Review submitted")
				: aComments == 1U             ? tr("Review submitted with 1 comment")
											  : tr("Review submitted with %1 comments").arg(aComments);
			statusBar()->showMessage(said, 8000);
			QAccessibleAnnouncementEvent event{ this, said };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		});
		ReviewSubmissionEditor->set_item_reveal([this](const std::string& aIdentity) {
			if (aIdentity.starts_with("draft:")) {
				open_review_composition(true);
				return;
			}
			close_review_composition();
			static_cast<void>(reveal_conversation(aIdentity));
		});
		// [[BPR-1592]]. RFC-BPR-0085 D3: Notes is the private notes at their
		// lines, and nothing else. The agent's panels move, unchanged, to the
		// provisional Agent tab with the private history and its purpose.
		auto* const notesPage       = new QWidget{ Drawers[1].Panel };
		auto* const notesPageLayout = new QVBoxLayout{ notesPage };
		notesPageLayout->setContentsMargins(0, 0, 0, 0);
		PrivateNotes = new private_notes_panel{ notesPage };
		notesPageLayout->addWidget(PrivateNotes, 1);
		wire_private_notes();
		auto* const agentPage       = new QWidget{ Drawers[1].Panel };
		auto* const agentPageLayout = new QVBoxLayout{ agentPage };
		agentPageLayout->setContentsMargins(0, 0, 0, 0);
		auto* const notesPurpose = new QLabel{ tr("Private to you and never published. Ask an agent about this pull request, then promote what you want to say to a draft in Comments."), agentPage };
		notesPurpose->setObjectName(QStringLiteral("workbenchNotesPurpose"));
		notesPurpose->setTextFormat(Qt::PlainText);
		notesPurpose->setWordWrap(true);
		notesPurpose->setAccessibleName(tr("About Agent"));
		notesPurpose->setAccessibleDescription(notesPurpose->text());
		notesPurpose->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		agentPageLayout->addWidget(notesPurpose);
		AgentPanel = new agent_launch_panel{ { }, conversationContents };
		AgentPanel->setMaximumHeight(300);
		findingsLayout->addWidget(AgentPanel);
		// The agent surfaces remain beside their launcher. The same retained
		// draft editor and its workshop now have their own composition page.
		SideChat = new side_chat_panel{ { }, conversationContents };
		findingsLayout->addWidget(SideChat);
		// Stacked, as the finding queue's actions are: the drawer never scrolls
		// sideways, so a row wider than the drawer loses its last actions rather
		// than pushing them offscreen (BPR-0506).
		auto* const chatActions = new QVBoxLayout{ };
		for (auto* const action : { SideChat->send_action(),
									SideChat->add_context_action(),
									SideChat->new_session_action(),
									SideChat->continue_old_revision_action() }) {
			auto* const button = new QToolButton{ conversationContents };
			button->setDefaultAction(action);
			chatActions->addWidget(button);
		}
		findingsLayout->addLayout(chatActions);
		FindingQueue = new finding_queue_view{ { }, conversationContents };
		findingsLayout->addWidget(FindingQueue);
		// One button per declared action, indexed by the enumerator, so a
		// seventh finding action cannot be declared and then never offered.
		//
		// BPR-0506. Stacked rather than in a row. Six buttons whose longest
		// label is *Convert to local draft comment* need more width than any
		// drawer a reviewer is likely to give this column, and the drawer
		// deliberately does not scroll sideways, so a row put the last two
		// actions somewhere no gesture reaches. A column cannot clip: each
		// button is as wide as its own label and the drawer already scrolls
		// down. The six actions, their order, their labels, and the single
		// intent each emits are untouched - this is where they are drawn, not
		// what they are.
		auto* const findingActions = new QVBoxLayout{ };
		for (auto index = std::size_t{ 0U }; index < finding_queue_action_count;
			 ++index) {
			auto* const action = FindingQueue->action(
				static_cast<finding_queue_action>(index));
			if (action == nullptr) {
				continue;
			}
			auto* const button = new QToolButton{ conversationContents };
			button->setDefaultAction(action);
			findingActions->addWidget(button);
		}
		findingsLayout->addLayout(findingActions);
		// Each tab's column scrolls on its own, so switching tabs keeps each
		// one's scroll position. A column holds more than a short drawer can
		// show at once, and a column that simply squeezed would show each panel
		// with its own fields overlapping — which is showing a reviewer
		// nothing. Collapse, width reporting, and the toggle are untouched:
		// this is inside the drawer's content, not around it.
		// BPR-0527. The drawer opens at the width stated above rather than at
		// whatever the widest row asked for. [[BPR-0728]]: that width is the
		// default, not a floor, so neither column claims a minimum of its own
		// and the handle drags the drawer down to the panel floor the other
		// drawers share. Beneath a column's own minimum its content scrolls
		// sideways rather than clipping, so every control stays reachable.
		const auto makeTabColumn = [this](QWidget* const aContents, const QString& aName) {
			auto* const scroll = new QScrollArea{ Drawers[1].Panel };
			scroll->setObjectName(aName);
			scroll->setWidgetResizable(true);
			scroll->setFrameShape(QFrame::NoFrame);
			scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
			scroll->setWidget(aContents);
			return scroll;
		};
		CommentsScroll     = makeTabColumn(commentsContents, QStringLiteral("workbenchCommentsScroll"));
		ConversationScroll = makeTabColumn(conversationContents, QStringLiteral("workbenchNotesScroll"));
		// Keep the purpose within the visible page even when tools scroll sideways.
		auto* privateTabs = new QTabWidget{ agentPage };
		privateTabs->setObjectName(QStringLiteral("privateConversationTabs"));
		privateTabs->setAccessibleName(tr("Private history and agent tools"));
		PrivateReview = new private_review_panel{ agentPage };
		ReviewSubmissionEditor->set_saved_comments_reveal([this] { PrivateReview->show_saved_comments(); });
		privateTabs->addTab(makeTabColumn(PrivateReview, QStringLiteral("privateHistoryScroll")), tr("Private history"));
		privateTabs->addTab(ConversationScroll, tr("Agent tools"));
		agentPageLayout->addWidget(privateTabs, 1);
		// [[BPR-1594]]. RFC-BPR-0085 D5: the Agent tab presents the agent's
		// panels, moved into one layout; the private history is its History.
		// The panels' own column stays, unseen, for every control not moved.
		AgentTab = new agent_tab_view{ *AgentPanel, *SideChat, *FindingQueue, *privateTabs->widget(0), agentPage };
		AgentTab->set_setup_dispatch([this] { open_settings(settings_page::agents_and_context); });
		agentPageLayout->addWidget(AgentTab, 1);
		notesPurpose->hide();
		privateTabs->hide();

		// [[BPR-1134]]. The tray's header is its tabs: Comments, then Notes
		// and Tour. The drawer still
		// opens on Comments. Choosing Notes is RFC-BPR-0031's explicit move
		// into the AI tools, the action that cues compare_glance in the corner; it
		// dispatches no agent or review action and never opens the drawer.
		auto* const conversationShell       = new QWidget{ Drawers[1].Panel };
		auto* const conversationShellLayout = new QVBoxLayout{ conversationShell };
		conversationShellLayout->setContentsMargins(0, 0, 0, 0);
		conversationShellLayout->setSpacing(0);
		TrayTabs = new tray_tab_bar{ conversationShell };
		TrayTabs->setObjectName(QStringLiteral("workbenchTrayTabs"));
		TrayTabs->setAccessibleName(QStringLiteral("Right tray"));
		TrayTabs->setAccessibleDescription(QStringLiteral(
			"Comments, Notes and Tour. Notes and tour preparation are private until explicitly published."));
		configure_drawer_tabs(*TrayTabs);
		// [[BPR-1594]]. The Agent keeps its spark beside its name.
		// [[BPR-1684]]: fit_tray_tabs gives each tab its face, glyph
		// and tooltip for the width the row has.
		TrayTabs->addTab(tr("Comments"));
		TrayTabs->addTab(tr("Notes"));
		TrayTabs->addTab(tr("Tour"));
		TrayTabs->addTab(drawer_icon(drawer_glyph::agent, palette()), tr("Agent"));
		TrayTabs->setAccessibleTabName(tray_comments_tab, tr("Comments"));
		TrayTabs->setAccessibleTabName(tray_notes_tab, tr("Notes"));
		TrayTabs->setAccessibleTabName(tray_tour_tab, tr("Tour"));
		TrayTabs->setAccessibleTabName(tray_findings_tab, tr("Agent"));
		// [[BPR-1684]]. The count is the tab's button only while it
		// shows, so a tab without new comments keeps no room for it.
		TrayCommentsCount = new drawer_count_pill{ TrayTabs };
		TrayCommentsCount->setObjectName(QStringLiteral("workbenchTrayCommentsCount"));
		TrayCommentsCount->hide();
		TrayPages = new QStackedWidget{ conversationShell };
		TrayPages->setObjectName(QStringLiteral("workbenchTrayPages"));
		TrayPages->addWidget(CommentsScroll);
		TrayPages->addWidget(notesPage);
		// [[BPR-1593]]. The Tour tab is the route; its editor moves to the tour
		// page on Expand and back on Back to the code, so it sits in a holder.
		TourHolder = new QWidget{ conversationShell };
		TourHolder->setObjectName(QStringLiteral("workbenchTourHolder"));
		auto* const tourHolding = new QVBoxLayout{ TourHolder };
		tourHolding->setContentsMargins(0, 0, 0, 0);
		TourEditor = new review_tour_editor{ TourHolder };
		tourHolding->addWidget(TourEditor);
		TrayPages->addWidget(makeTabColumn(TourHolder, QStringLiteral("workbenchTourScroll")));
		wire_tour_page();
		TrayPages->addWidget(agentPage);
		// [[BPR-1290]]. The tab row carries the Comments tab's tools at its
		// right: Only new, Hide resolved and Next new reply.
		auto* const trayHeader = new drawer_header{ conversationShell };
		trayHeader->setObjectName(QStringLiteral("workbenchTrayHeader"));
		CommentsTools = new QWidget{ trayHeader };
		CommentsTools->setObjectName(QStringLiteral("workbenchCommentsTools"));
		auto* const commentsToolsRow = new QHBoxLayout{ CommentsTools };
		commentsToolsRow->setContentsMargins(0, 0, 0, 0);
		commentsToolsRow->setSpacing(1);
		commentsToolsRow->addWidget(ConversationPanel->read_tools());
		NextNewCommentButton->setParent(CommentsTools);
		commentsToolsRow->addWidget(NextNewCommentButton);
		// [[BPR-1592]]. The Notes tab's tools: a new note on the selected line
		// (N), and the notes on the pull request.
		NotesTools = new QWidget{ CommentsTools->parentWidget() };
		NotesTools->setObjectName(QStringLiteral("workbenchNotesTools"));
		auto* const notesToolsRow = new QHBoxLayout{ NotesTools };
		notesToolsRow->setContentsMargins(0, 0, 0, 0);
		notesToolsRow->setSpacing(1);
		PrivateNotes->new_line_action()->setIcon(conversation_icon(conversation_glyph::comment, palette()));
		PrivateNotes->elsewhere_action()->setIcon(drawer_icon(drawer_glyph::pull_request, palette()));
		auto* const newNote = make_drawer_tool(NotesTools, PrivateNotes->new_line_action()->icon(), PrivateNotes->new_line_action()->toolTip(), QStringLiteral("workbenchNewNoteTool"));
		newNote->setDefaultAction(PrivateNotes->new_line_action());
		newNote->setToolButtonStyle(Qt::ToolButtonIconOnly);
		notesToolsRow->addWidget(newNote);
		auto* const elsewhere = make_drawer_tool(NotesTools, drawer_icon(drawer_glyph::pull_request, palette()), PrivateNotes->elsewhere_action()->text(), QStringLiteral("workbenchNotesElsewhereTool"));
		elsewhere->setDefaultAction(PrivateNotes->elsewhere_action());
		elsewhere->setToolButtonStyle(Qt::ToolButtonIconOnly);
		notesToolsRow->addWidget(elsewhere);
		NotesTools->hide();
		// [[BPR-1593]]. The Tour tab's tools: Add stop, and Expand to the
		// tour page.
		TourTools = new QWidget{ CommentsTools->parentWidget() };
		TourTools->setObjectName(QStringLiteral("workbenchTourTools"));
		auto* const tourToolsRow = new QHBoxLayout{ TourTools };
		tourToolsRow->setContentsMargins(0, 0, 0, 0);
		tourToolsRow->setSpacing(1);
		auto* const addStop = make_drawer_tool(TourTools, conversation_icon(conversation_glyph::comment, palette()), tr("Add a stop on the selected lines"), QStringLiteral("workbenchAddTourStopTool"));
		connect(addStop, &QToolButton::clicked, this, [this] { TourEditor->trigger(tour_editor_action::add_stop); });
		tourToolsRow->addWidget(addStop);
		auto* const expandTour = make_drawer_tool(TourTools, conversation_icon(conversation_glyph::maximize, palette()), tr("Expand to the tour page"), QStringLiteral("workbenchExpandTourTool"));
		connect(expandTour, &QToolButton::clicked, this, [this] { open_tour_page(false); });
		tourToolsRow->addWidget(expandTour);
		TourTools->hide();
		auto* const trayTools    = new QWidget{ trayHeader };
		auto* const trayToolsRow = new QHBoxLayout{ trayTools };
		trayToolsRow->setContentsMargins(0, 0, 0, 0);
		trayToolsRow->setSpacing(0);
		CommentsTools->setParent(trayTools);
		NotesTools->setParent(trayTools);
		TourTools->setParent(trayTools);
		trayToolsRow->addWidget(CommentsTools);
		trayToolsRow->addWidget(NotesTools);
		trayToolsRow->addWidget(TourTools);
		trayHeader->set_parts(TrayTabs, trayTools);
		// [[BPR-1592]] [[BPR-1684]]. RFC-BPR-0085 D5: where the names do
		// not fit the tabs go compact, so four tabs never scroll.
		trayHeader->installEventFilter(this);
		TrayHeader = trayHeader;
		fit_tray_tabs(true);
		conversationShellLayout->addWidget(trayHeader);
		conversationShellLayout->addWidget(TrayPages, 1);
		AiToolsAction = new QAction{ tr("Agent"), this };
		AiToolsAction->setObjectName(QStringLiteral("showWorkbenchAiToolsAction"));
		AiToolsAction->setCheckable(true);
		AiToolsAction->setToolTip(
			tr("Agent conversations, findings and private history. Nothing here is published; promote an item to a draft in Comments to publish it."));
		connect(TrayTabs, &QTabBar::currentChanged, this, [this](const int aIndex) {
			TrayPages->setCurrentIndex(aIndex);
			// [[BPR-1684]]. A compact row names the open tab.
			fit_tray_tabs();
			CommentsTools->setVisible(aIndex == tray_comments_tab);
			NotesTools->setVisible(aIndex == tray_notes_tab);
			TourTools->setVisible(aIndex == tray_tour_tab);
			if (aIndex == tray_notes_tab && PrivateNotes != nullptr) {
				PrivateNotes->refresh_geometry();
			}
			apply_thread_marks();
			const auto blocker = QSignalBlocker{ AiToolsAction };
			AiToolsSelected    = aIndex == tray_findings_tab;
			AiToolsAction->setChecked(AiToolsSelected);
			update_dif_presentation();
			if (AiToolsSelected && showing_ai_tools()) {
				play_dif_cue(DifCues->reveal_findings(DifCueClock->elapsed()));
			}
		});
		connect(AiToolsAction, &QAction::toggled, this, [this](bool aSelected) { TrayTabs->setCurrentIndex(aSelected ? tray_findings_tab : tray_comments_tab); });
		Drawers[1].Panel->set_content_widget(
			conversationShell, ConversationPanel->cards());
		CentralSplitter->addWidget(Drawers[2].Panel);
		CentralSplitter->addWidget(Drawers[0].Panel);
		CentralSplitter->addWidget(CentreSurface);
		CentralSplitter->addWidget(Drawers[1].Panel);
		// [[BPR-0646]]. The queue landing leaves the review header, which is no
		// longer a scroll area, and sits under the file list until the
		// repositories drawer of RFC-BPR-0028 takes it over ([[BPR-0651]]).
		// The review queue is how a pull request is chosen, so it is Home's
		// list, under the landing card, with the room the drawer could not
		// give it. The files drawer holds files.
		if (auto* const homeColumn = qobject_cast<QVBoxLayout*>(HomePage->layout());
			homeColumn != nullptr) {
			// [[BPR-0650]]. Open by link or owner/name#number, in the bar
			// above the list: the same intent the locator sends, and the
			// pull request arrives on Overview.
			// [[BPR-0669]]. The bar is the way a pull request is named: it
			// takes what a reviewer has on the clipboard, whole. It had no
			// label and no button, so the only affordance was a placeholder
			// and the Return key, and the owner went looking in Settings.
			// [[BPR-0775]]. RFC-BPR-0032's address field, alone in Home's first
			// row: the label "Open", the placeholder and Command-L inside one
			// bordered field, and Return to open. The heading over it and the
			// Open button beside it are gone; the action stays for Return and
			// the menu.
			auto* const openFrame = new home_bar_frame{ HomePage };
			openFrame->setObjectName(QStringLiteral("homeOpenFrame"));
			HomeOpenFrame = openFrame;
			HomeOpenLabel = new QLabel{ QStringLiteral("Open"), openFrame };
			HomeOpenLabel->setObjectName(QStringLiteral("homeOpenLabel"));
			auto openLabelFont = HomeOpenLabel->font();
			openLabelFont.setWeight(QFont::DemiBold);
			HomeOpenLabel->setFont(openLabelFont);
			HomeOpenBar = new QLineEdit{ openFrame };
			HomeOpenBar->setObjectName(QStringLiteral("homeOpenBar"));
			HomeOpenBar->setAccessibleName(QStringLiteral("Open a pull request"));
			HomeOpenBar->setAccessibleDescription(QStringLiteral(
				"Paste a pull request link, or owner/name#number, and press Return"));
			HomeOpenBar->setPlaceholderText(
				QStringLiteral("Paste a link, or write owner/name#123"));
			HomeOpenBar->setClearButtonEnabled(true);
			HomeOpenBar->setFrame(false);
			HomeOpenLabel->setBuddy(HomeOpenBar);
			openFrame->watch(HomeOpenBar);
			HomeOpenAction = new QAction{ tr("Open"), this };
			HomeOpenAction->setObjectName(QStringLiteral("homeOpenAction"));
			HomeFocusBarAction = new QAction{ tr("Go to the Open bar"), this };
			HomeFocusBarAction->setObjectName(QStringLiteral("homeFocusBarAction"));
			HomeFocusBarAction->setShortcuts(shortcut_keys(shortcut_command::home_open_bar));
			HomeFocusBarAction->setShortcutContext(Qt::WindowShortcut);
			addAction(HomeFocusBarAction);
			connect(HomeFocusBarAction, &QAction::triggered, this, [this] { focus_home_bar(); });
			auto* const openShortcut = new QLabel{
				HomeFocusBarAction->shortcut().toString(QKeySequence::NativeText), openFrame
			};
			openShortcut->setObjectName(QStringLiteral("homeOpenShortcut"));
			openShortcut->setForegroundRole(QPalette::PlaceholderText);
			openShortcut->setAccessibleName(QStringLiteral("Shortcut to the Open bar"));
			HomeOpenShortcut     = openShortcut;
			HomeOpenActivity     = new home_bar_activity{ openFrame };
			auto* const frameRow = new QHBoxLayout{ openFrame };
			frameRow->setContentsMargins(12, 8, 12, 8);
			frameRow->setSpacing(8);
			frameRow->addWidget(HomeOpenLabel);
			frameRow->addWidget(HomeOpenBar, 1);
			frameRow->addWidget(HomeOpenActivity);
			frameRow->addWidget(openShortcut);
			// [[BPR-0779]]. RFC-BPR-0032 puts the bar "alone in the first row",
			// and the accepted board H1 of `design/home-v2.html` puts first
			// launch's slack in a fill *below* the sign-in card. Signed in, the
			// list takes the column's spare height. Signed out the list is
			// hidden, and this frame's default Preferred policy let it absorb
			// that height instead, drawing its rounded border around some 530
			// points of nothing — the empty box A6 forbids. The frame is
			// bordered, so its height is the box: it keeps the height it asks
			// for in every session state.
			openFrame->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
			// [[BPR-1381]]. Over an open review the way back stands at the
			// bar's right (see place_home_landing). The owner, 2026-10-10: the
			// row it had above the bar "causes all the layout of the page to
			// shift and that section goes away when you open a new pr which
			// shifts the screen under your cursor". Beside the bar it changes
			// the bar's width only, and nothing below the bar moves.
			auto* const openRow = new QHBoxLayout{ };
			openRow->setContentsMargins(24, 16, 24, 0);
			openRow->setSpacing(12);
			openRow->addWidget(openFrame, 1);
			homeColumn->addLayout(openRow);
			HomeOpenRow = openRow;
			// A refusal belongs beside what was refused. Silence on Return was
			// indistinguishable from a product that does nothing.
			HomeOpenNotice = new QLabel{ HomePage };
			HomeOpenNotice->setObjectName(QStringLiteral("homeOpenNotice"));
			HomeOpenNotice->setWordWrap(true);
			HomeOpenNotice->hide();
			HomeOpenNotice->setTextFormat(Qt::PlainText);
			// [[BPR-0771]]. A pull request that could not be loaded is said
			// here, once, in one sentence, with its details a press away. The
			// banner on Accounts & hosts that used to say it is gone.
			HomeOpenDetails = new QPushButton{ tr("Copy details"), HomePage };
			HomeOpenDetails->setObjectName(QStringLiteral("homeOpenCopyDetails"));
			HomeOpenDetails->setAccessibleName(QStringLiteral("Copy the details of the refusal"));
			HomeOpenDetails->setFlat(true);
			HomeOpenDetails->setAutoDefault(false);
			HomeOpenDetails->setFocusPolicy(Qt::StrongFocus);
			HomeOpenDetails->hide();
			// [[BPR-1357]]. Both now speak in Home's status band, at body size
			// behind a mark that says their tone. The band also carries Home's
			// general status, and Cancel or Retry for what the bar opened.
			// [[BPR-1381]]. It is docked at Home's foot, under the list, rather
			// than under the bar: shown and hidden there it moves nothing
			// above it, where a band under the bar pushed the list down under
			// the pointer. What the bar is opening is also said in the bar.
			HomeStatus     = new qt::home_status_line{ HomePage };
			HomeStatusText = new QLabel{ HomeStatus };
			HomeStatusText->setObjectName(QStringLiteral("homeStatusText"));
			HomeStatusText->setAccessibleName(QStringLiteral("Home status"));
			HomeStatusText->setWordWrap(true);
			HomeStatusText->setTextFormat(Qt::PlainText);
			HomeStatusText->setTextInteractionFlags(Qt::TextSelectableByMouse);
			HomeStatusText->hide();
			HomeOpenCancel = new QPushButton{ tr("Cancel"), HomeStatus };
			HomeOpenCancel->setObjectName(QStringLiteral("homeOpenCancel"));
			HomeOpenCancel->setAutoDefault(false);
			HomeOpenCancel->setFocusPolicy(Qt::StrongFocus);
			HomeOpenCancel->hide();
			HomeOpenRetry = new QPushButton{ tr("Retry"), HomeStatus };
			HomeOpenRetry->setObjectName(QStringLiteral("homeOpenRetry"));
			HomeOpenRetry->setAutoDefault(false);
			HomeOpenRetry->setFocusPolicy(Qt::StrongFocus);
			HomeOpenRetry->hide();
			connect(HomeOpenCancel, &QPushButton::clicked, this, [this] { dispatch_home_open_action(); });
			connect(HomeOpenRetry, &QPushButton::clicked, this, [this] { dispatch_home_open_action(); });
			HomeStatus->adopt_text(*HomeOpenNotice);
			HomeStatus->adopt_text(*HomeStatusText);
			HomeStatus->adopt_button(*HomeOpenCancel);
			HomeStatus->adopt_button(*HomeOpenRetry);
			HomeStatus->adopt_button(*HomeOpenDetails);
			auto* const noticeRow = new QHBoxLayout{ };
			noticeRow->setContentsMargins(24, 0, 24, 0);
			noticeRow->addWidget(HomeStatus, 1);
			connect(HomeOpenDetails, &QPushButton::clicked, this, [this] {
				if (auto* const clipboard = QApplication::clipboard(); clipboard != nullptr) {
					clipboard->setText(HomeRefusalDetails);
				}
			});
			// [[BPR-1200]]. A pull request this connection cannot read is refused
			// in a card of its own under the bar, not in the line above: the
			// owner missed that line. It names the pull request, says why in
			// terms of the connection and offers what could open it.
			HomeRefusal = new home_refusal{ HomePage };
			homeColumn->addWidget(HomeRefusal);
			HomeRefusal->set_action_dispatch([this](const core::pull_request_refusal_action aAction) {
				take_refusal_action(aAction);
			});
			HomeRefusal->set_copy_dispatch([this] {
				if (auto* const clipboard = QApplication::clipboard(); clipboard != nullptr) {
					clipboard->setText(HomeRefusalDetails);
				}
			});
			auto* setupRow = new QHBoxLayout{ };
			setupRow->setContentsMargins(24, 2, 24, 4);
			SetupContinueButton = new QPushButton{ HomePage };
			SetupContinueButton->setObjectName(QStringLiteral("homeSetupContinue"));
			SetupCancelButton = new QPushButton{ tr("Cancel setup"), HomePage };
			SetupCancelButton->setObjectName(QStringLiteral("homeSetupCancel"));
			setupRow->addWidget(SetupContinueButton);
			setupRow->addWidget(SetupCancelButton);
			setupRow->addStretch();
			homeColumn->addLayout(setupRow);
			// The first-launch card sits under this row (see place_home_landing).
			HomeSetupRow = setupRow;
			SetupContinueButton->hide();
			SetupCancelButton->hide();
			connect(SetupContinueButton, &QPushButton::clicked, this, [this] {
				if (SetupDestinationDispatch && SetupDestinationRequest) {
					SetupDestinationDispatch(application::continue_setup_destination{ *SetupDestinationRequest });
				}
			});
			connect(SetupCancelButton, &QPushButton::clicked, this, [this] {
				if (SetupDestinationDispatch) {
					SetupDestinationDispatch(application::cancel_setup_destination{ });
				}
			});
			connect(HomeOpenBar, &QLineEdit::returnPressed, this, [this] { open_from_home_bar(); });
			connect(HomeOpenAction, &QAction::triggered, this, [this] { open_from_home_bar(); });
			connect(HomeOpenBar, &QLineEdit::textChanged, this, [this] {
				if (!ApplyingSetupProjection && SetupDestinationRequest && SetupDestinationDispatch) {
					SetupDestinationDispatch(application::cancel_setup_destination{ });
				}
				if (HomeOpenNotice != nullptr && HomeOpenNotice->isVisible()) {
					// A refusal the reviewer is already rewriting stays dismissed
					// until the next load rather than returning on the next projection.
					if (!ApplyingSetupProjection && HomeSetupNotice.isEmpty()) {
						DismissedHomeRefusal = HomeOpenNotice->text();
					}
					HomeOpenNotice->hide();
				}
				if (HomeOpenDetails != nullptr) {
					HomeOpenDetails->hide();
				}
				if (HomeRefusal != nullptr && HomeRefusal->isVisible()) {
					if (!ApplyingSetupProjection && HomeSetupNotice.isEmpty()) {
						DismissedHomeRefusal = HomeRefusal->text();
					}
					HomeRefusal->hide();
				}
				if (HomeOpenAction != nullptr) {
					HomeOpenAction->setEnabled(
						!HomeOpenBar->text().trimmed().isEmpty()
						&& (SessionPanel == nullptr || SessionPanel->status() != core::github_status::establishing_session));
				}
			});
			HomeOpenAction->setEnabled(false);
			// [[BPR-0733]] [[BPR-0776]]. What this window read last, most recent
			// first. The app no longer opens the last pull request by itself at
			// startup, so this list is how a reviewer goes back to one, by a
			// gesture. It is governed by the entries it was given rather than by
			// the account: the composition has already filtered them for this
			// window's session, so a signed-out Home lists its public reading.
			// It is the last panel of Home's list, and Resume is its rows'
			// action: on hover, on focus and in the row's menu.
			HomeRecentResumeAction = new QAction{ tr("Resume"), this };
			HomeRecentResumeAction->setObjectName(QStringLiteral("homeRecentResumeAction"));
			HomeRecentResumeAction->setToolTip(QStringLiteral(
				"Opens the selected recently viewed pull request where its reading was left"));
			HomeRecentResumeAction->setEnabled(false);
			// Recently viewed keeps the last ten pull requests read, so the
			// reviewer can take one off it, from the row's menu or by Delete.
			// Delete is the tour editor's Remove in this window, so the list
			// reads the key itself, as Settings › Repositories' list does, and
			// the action claims no sequence.
			HomeRecentRemoveAction = new QAction{ tr("Remove from Recently Viewed"), this };
			HomeRecentRemoveAction->setObjectName(QStringLiteral("homeRecentRemoveAction"));
			HomeRecentRemoveAction->setStatusTip(QStringLiteral(
				"Takes the selected pull request off Recently viewed; nothing changes on GitHub"));
			HomeRecentRemoveAction->setEnabled(false);
			auto* const recentList = new home_recent_view{ HomePage };
			recentList->hide();
			HomeRecentList = recentList;
			HomeRecentList->setObjectName(QStringLiteral("homeRecentList"));
			HomeRecentList->setAccessibleName(QStringLiteral("Recently viewed pull requests"));
			// [[BPR-1364]]. A single click only selects the row.
			HomeRecentList->setAccessibleDescription(
				home_open_description() + QStringLiteral("; Shift and Return resume its saved reading"));
			// The action is the row's menu as well as its hover and focus
			// button, so the gesture is where the row is. [[BPR-1342]]: the
			// row's menu is Home's one row menu, opened on the row the right
			// click made current, with Open Review and Open Review in New
			// Window before Resume and Open on GitHub ([[BPR-1338]]). Its
			// key, ⇧Return, is shown there; it is read by the list, below.
			HomeRecentResumeAction->setShortcuts(shortcut_keys(shortcut_command::home_resume));
			HomeRecentResumeAction->setShortcutContext(Qt::WidgetShortcut);
			HomeRecentResumeAction->setShortcutVisibleInContextMenu(true);
			HomeRecentList->setContextMenuPolicy(Qt::CustomContextMenu);
			connect(HomeRecentList, &QWidget::customContextMenuRequested, this, [this](const QPoint& aPosition) {
				auto* const item = HomeRecentList->itemAt(aPosition);
				if (item == nullptr) {
					return;
				}
				claim_home_selection(home_selection_list::recent);
				HomeRecentList->setCurrentItem(item);
				popup_home_row_menu(HomeRecentList->viewport()->mapToGlobal(aPosition));
			});
			HomeRecentList->installEventFilter(this);
			// [[BPR-1364]]. The pointer opens a row by a double-click, never a
			// single click: `itemActivated` is a single click wherever the style
			// asks for single-click activation (KDE). Return is read below.
			connect(HomeRecentList, &QListWidget::itemDoubleClicked, this,
					[this](QListWidgetItem* const aItem) {
						dispatch_recent_review(
							aItem == nullptr ? -1 : HomeRecentList->row(aItem), false);
					});
			// [[BPR-1365]]. A row made current while another list has Home's
			// keyboard is not shown selected.
			connect(HomeRecentList->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
				if (HomeSelection != home_selection_list::recent) {
					home_hide_selection(*HomeRecentList);
				}
			});
			connect(HomeRecentList, &QListWidget::currentRowChanged, this,
					[this](int) {
						update_recent_resume_action();
						update_open_on_github_action();
						update_home_review_actions();
					});
			connect(HomeRecentResumeAction, &QAction::triggered, this, [this] {
				dispatch_recent_review(HomeRecentList->currentRow(), true);
			});
			connect(HomeRecentRemoveAction, &QAction::triggered, this, [this] {
				remove_recent_review(HomeRecentList->currentRow());
			});
			// [[BPR-1157]]. Review Cycles beside the pull request ways in: Open
			// Project and the cycles this profile keeps, most recent first. It
			// needs no account, so it stays whether or not one is signed in.
			HomeCycles = new QWidget{ HomePage };
			HomeCycles->setObjectName(QStringLiteral("homeReviewCycles"));
			HomeCycles->setAccessibleName(QStringLiteral("Review Cycles"));
			HomeCycles->setContentsMargins(24, 4, 24, 4);
			auto* const cyclesColumn = new QVBoxLayout{ HomeCycles };
			cyclesColumn->setContentsMargins(0, 0, 0, 0);
			auto* const cyclesHeader = new QHBoxLayout{ };
			auto* const cyclesTitle  = new QLabel{ QStringLiteral("Review Cycles"), HomeCycles };
			cyclesTitle->setObjectName(QStringLiteral("homeReviewCyclesTitle"));
			auto cyclesFont = cyclesTitle->font();
			cyclesFont.setWeight(QFont::DemiBold);
			cyclesTitle->setFont(cyclesFont);
			cyclesHeader->addWidget(cyclesTitle);
			cyclesHeader->addStretch(1);
			auto* const openProject = new QToolButton{ HomeCycles };
			openProject->setObjectName(QStringLiteral("homeOpenProjectButton"));
			openProject->setText(QStringLiteral("Open Project…"));
			openProject->setToolTip(QStringLiteral("Review a local folder or file: no account, remote or agent needed"));
			cyclesHeader->addWidget(openProject);
			cyclesColumn->addLayout(cyclesHeader);
			HomeCyclesList = new QListWidget{ HomeCycles };
			HomeCyclesList->setObjectName(QStringLiteral("homeReviewCyclesList"));
			HomeCyclesList->setAccessibleName(QStringLiteral("Recent review cycles"));
			// [[BPR-1364]]. As every Home list: a single click selects, a
			// double-click or Return opens. Return is read in `eventFilter`,
			// because on macOS it is an item view's editing key.
			HomeCyclesList->setAccessibleDescription(home_open_description());
			HomeCyclesList->installEventFilter(this);
			HomeCyclesList->setMaximumHeight(HomeCyclesList->fontMetrics().lineSpacing() * 5 + 8);
			HomeCyclesList->hide();
			cyclesColumn->addWidget(HomeCyclesList);
			homeColumn->addWidget(HomeCycles);
			connect(openProject, &QToolButton::clicked, this, [this] {
				if (OpenProjectAction != nullptr) {
					OpenProjectAction->trigger();
				}
			});
			connect(HomeCyclesList->selectionModel(), &QItemSelectionModel::selectionChanged, this, [this] {
				if (HomeSelection != home_selection_list::cycles) {
					home_hide_selection(*HomeCyclesList);
				}
			});
			connect(HomeCyclesList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* const aItem) {
				resume_home_cycle(aItem == nullptr ? -1 : HomeCyclesList->row(aItem));
			});
			build_queue_landing(HomePage, *homeColumn);
			if (auto* const landing = HomePage->findChild<QWidget*>(
					QStringLiteral("reviewQueueLanding"), Qt::FindDirectChildrenOnly);
				landing != nullptr) {
				QueueSurface = landing;
				landing->setMaximumHeight(QWIDGETSIZE_MAX);
				landing->setContentsMargins(24, 0, 24, 12);
				homeColumn->setStretchFactor(landing, 1);
			}
			// [[BPR-1200]]. With no list to fill it, Home's slack is a fill below
			// the last thing shown, as the accepted board puts it under the
			// first-launch card, so a refusal or a load in progress is said
			// directly under the bar rather than wherever the spare height
			// spread the rows. The list's stretch still takes it when shown.
			homeColumn->addStretch(0);
			homeColumn->addLayout(noticeRow);
			if (HomeList != nullptr) {
				HomeList->adopt_recent(*recentList);
				HomeList->set_recent_action_dispatch([this](const int aRow) { dispatch_recent_review(aRow, true); });
				// [[BPR-1342]]. ⌘-click or middle-click on a Recently viewed row.
				HomeList->set_recent_new_window_dispatch([this](const int aRow) {
					if (aRow >= 0 && static_cast<std::size_t>(aRow) < RecentReviews.Entries.size()) {
						request_review_window(application::recent_review_row{ static_cast<std::size_t>(aRow) });
					}
				});
			}
			// Nothing was published yet, so there is no panel to show.
			apply_recent_reviews();
		}
		// Only the centre absorbs a window resize, so a drawer keeps the width the
		// host persisted until the user moves the handle.
		CentralSplitter->setStretchFactor(0, 0);
		CentralSplitter->setStretchFactor(1, 0);
		CentralSplitter->setStretchFactor(2, 1);
		CentralSplitter->setStretchFactor(3, 0);

		// [[BPR-0662]]. A width leaves for the host only from a handle drag.
		// Whatever the splitter is given by a resize -- the window, the
		// stack showing it, a hidden page laid out at its default size -- is
		// a squeeze the splitter itself undoes once there is room, and it is
		// never written back.
		connect(CentralSplitter, &QSplitter::splitterMoved, this,
				[this](int, int) { observe_handle_drag(); });
		connect(
			Drawers[0].Panel->contents_view()->selectionModel(),
			&QItemSelectionModel::currentChanged,
			this,
			[this](const QModelIndex& aCurrent) {
				dispatch_file_selection(aCurrent);
			});
		connect(
			ConversationPanel->view()->selectionModel(),
			&QItemSelectionModel::currentChanged,
			this,
			[this](const QModelIndex& aCurrent) {
				if (!ApplyingConversationAnchor) {
					synchronize_selected_conversation(aCurrent);
				}
				update_defect_surface();
				// [[BPR-1133]]. A thread chosen is what Comment now answers.
				update_comment_target();
			});
		update_defect_surface();
	}

	void workbench_window::build_github_header()
	{
		// RFC-BPR-0002's session chrome and pull-request metadata sit above the
		// workbench rather than in a window of their own. BPR-0188 decided this
		// window is the review surface; a second window owning the same
		// comparison is the defect that decision closes.
		// Object names, mnemonics, shortcuts and contexts carried over from
		// `github_window` unchanged, so the assertions that moved with them
		// still measure the same thing.
		GithubConnectAction = new QAction{ QStringLiteral("&Connect"), this };
		GithubConnectAction->setObjectName(QStringLiteral("connectGithubAction"));
		GithubConnectAction->setShortcuts(shortcut_keys(shortcut_command::connect));
		GithubConnectAction->setShortcutContext(Qt::ApplicationShortcut);
		addAction(GithubConnectAction);
		GithubOpenAction = new QAction{
			QStringLiteral("&Open pull request"), this
		};
		GithubOpenAction->setObjectName(QStringLiteral("openPullRequestAction"));
		GithubOpenAction->setShortcuts(shortcut_keys(shortcut_command::open_pull_request));
		GithubOpenAction->setShortcutContext(Qt::ApplicationShortcut);
		addAction(GithubOpenAction);
		GithubRefreshAction = new QAction{ QStringLiteral("&Refresh"), this };
		GithubRefreshAction->setObjectName(
			QStringLiteral("refreshPullRequestAction"));
		GithubRefreshAction->setShortcuts(shortcut_keys(shortcut_command::refresh));
		GithubRefreshAction->setShortcutContext(Qt::ApplicationShortcut);
		addAction(GithubRefreshAction);
		// [[BPR-0990]]. Home's refresh glyph is the profile's discovery, not
		// the pull request's refresh: the same place in the strip, its own
		// action, shown only on Home with an account. [[BPR-1600]]: the
		// window's Refresh key moves to it while Home shows
		// (`apply_refresh_key`).
		QueueRefreshAction = new QAction{ QStringLiteral("Refresh my pull requests"), this };
		QueueRefreshAction->setObjectName(QStringLiteral("refreshQueueAction"));
		QueueRefreshAction->setIconText(QStringLiteral("Refresh"));
		QueueRefreshAction->setVisible(false);
		QueueRefreshAction->setEnabled(false);
		addAction(QueueRefreshAction);
		connect(QueueRefreshAction, &QAction::triggered, this, [this] {
			// [[BPR-1575]]. The refresh this starts is the reviewer's own.
			HomeRefreshAsked = true;
			if (QueueDiscoveryDispatch) {
				QueueDiscoveryDispatch(application::queue_discovery_intent::refresh);
			}
		});

		SessionPanel = new github_session_panel{
			GithubProfiles,
			GithubDispatch,
			GithubConnectAction,
			GithubOpenAction,
			GithubRefreshAction,
			github_session_environment{
				.HasPullRequest = [this] { return HasGithubPullRequest; },
				// The panel reports that something it owns changed the
				// presentation -- a status, an identity, a diagnostic. Stubbing
				// this out left the empty state frozen on whatever it said when
				// the window was built.
				.PresentationChanged = [this] { update_github_presentation_state(); },
			},
			this,
		};
		// [[BPR-0771]]. Open names what the Home bar names; the locator it
		// once read is gone. With nothing written there it takes the reviewer
		// to the bar.
		connect(GithubOpenAction, &QAction::triggered, this, [this] {
			if (HomeOpenBar == nullptr) {
				return;
			}
			if (HomeOpenBar->text().trimmed().isEmpty()) {
				choose_home(true);
				HomeOpenBar->setFocus(Qt::ShortcutFocusReason);
				return;
			}
			open_from_home_bar();
		});
		PullRequestPanel = new github_pull_request_panel{ };

		// [[BPR-0526]], [[BPR-0773]]. A profile has two ways in -- the token
		// prompt the Connect action opens, and a code read aloud into a browser
		// -- and a reviewer looking for either looks in one place: the server's
		// card. So the device panel is laid into that card's account row
		// (`place_device_sign_in`) rather than behind a drawer or a dialog of
		// its own.
		//
		// The window supplies no dispatch. Every gesture leaves through
		// `DeviceDispatch`, which the composition sets, so a panel nobody wired
		// stays visible and inert instead of quietly performing a host effect
		// from the presentation layer.
		DevicePanel = new device_authorization_panel{
			SessionPanel->selected_profile().value_or(core::host_profile_id{ }),
			[this](application::device_authorization_gesture aGesture) {
				if (DeviceDispatch) {
					DeviceDispatch(std::move(aGesture));
				}
			},
			nullptr,
		};
		// [[BPR-0773]]. The window hears what the panel now shows so the card
		// holding it can make its account row the sign-in and its header say
		// Signing in. Where this host has no device flow, the token prompt the
		// Connect action already opens is the way in beside the refusal.
		DevicePanel->set_presentation_observer([this] {
			place_device_sign_in();
			// A flow belongs to the host it was asked for: while one runs, the
			// card holding the row is that host, and its terminal sentence is
			// said only there.
			if (DevicePanel->signing_in()) {
				const auto holder = std::ranges::find_if(ServerCards, [this](const server_card* const aCard) {
					return aCard->device_sign_in() == DevicePanel;
				});
				if (holder != ServerCards.end()) {
					DeviceFlowHost = (*holder)->hostname();
				}
			}
			update_server_cards();
		});
		DevicePanel->set_manual_token_dispatch([this] {
			if (GithubConnectAction != nullptr) {
				GithubConnectAction->trigger();
			}
		});

		// [[BPR-0646]]. RFC-BPR-0028 AC-01: the chrome above the comparison
		// is the strip and the epoch line only, and accounts live in Settings.
		// The server cards are the Accounts and hosts page of the Settings
		// window, one per configured server ([[BPR-0771]], [[BPR-0772]]), and
		// the device sign-in is the account row of its server's card
		// ([[BPR-0773]]). The strip's account button opens that page, and a
		// flow the core begins presents it, so the code is seen. The pull
		// request list (the queue landing) and the
		// pull request summary stay in the files drawer's bounded top region
		// until the repositories drawer (BPR-0651) and the overview
		// (BPR-0647) take them.
		SettingsWindow       = new settings_window{ this };
		auto* const accounts = new QWidget;
		AccountsPanel        = accounts;
		accounts->setObjectName(QStringLiteral("workbenchAccountsPage"));
		accounts->setAccessibleName(QStringLiteral("Accounts and hosts"));
		auto* const accountsColumn = new QVBoxLayout{ accounts };
		accountsColumn->setContentsMargins(0, 0, 0, 0);
		// [[BPR-0771]], [[BPR-0772]]. RFC-BPR-0032: the sentence, then one card
		// per configured server. The host picker, the locator, the status banner and
		// the public-preview strip are not here: a host is chosen by its card's
		// Connect, a pull request is named in the Home bar, a load refusal is
		// said under that bar, and a session failure is said in its card.
		auto* const lede = new QLabel{ QStringLiteral("One account per server. Credentials are never shown here."), accounts };
		lede->setObjectName(QStringLiteral("accountsLede"));
		lede->setWordWrap(true);
		lede->setTextFormat(Qt::PlainText);
		lede->setForegroundRole(QPalette::PlaceholderText);
		accountsColumn->addWidget(lede);
		// [[BPR-0773]]. Device sign-in is no longer a panel of its own on the
		// page: it is the account row of the card whose host it is bound to,
		// laid in once that card exists.
		DevicePanel->setParent(accounts);
		DevicePanel->hide();
		ServerCardColumn = new QVBoxLayout{ };
		ServerCardColumn->setContentsMargins(0, 0, 0, 0);
		ServerCardColumn->setSpacing(16);
		accountsColumn->addLayout(ServerCardColumn);
		for (const auto& profile : GithubProfiles) {
			add_server_card(profile);
		}
		place_device_sign_in();
		// [[BPR-0660]], [[BPR-0774]]. The enterprise servers the reviewer keeps,
		// so a GitHub Enterprise host is offered at the next launch without
		// being typed again. There is no list of them: a kept server is its
		// host's card, with Remember and Forget, and one bordered button under
		// the cards adds another as a card in its edit state. No token is kept.
		build_enterprise_server_draft(accounts, *accountsColumn);
		SettingsWindow->adopt_panel(settings_page::accounts_and_hosts, accounts);
		build_repositories_page();
		// [[BPR-0652]]. Home & queue (Reviews & queue until [[BPR-1597]]):
		// the queue defaults, the observer rules and the way to Repositories.
		// [[BPR-1597]]. The review and comment rows have pages of their own,
		// Review and Comments (owner's pick A, 2026-10-17: "Reviews & queue is
		// too much of a big grab bag of settings").
		auto* const reviews = new QWidget;
		reviews->setObjectName(QStringLiteral("workbenchReviewsPage"));
		reviews->setAccessibleName(QStringLiteral("Home and queue"));
		auto* const reviewsColumn = new QVBoxLayout{ reviews };
		reviewsColumn->setContentsMargins(0, 0, 0, 0);
		auto* const reviewsSubtitle = new QLabel{
			QStringLiteral("Local behaviour only. Nothing here changes assignments or subscriptions on GitHub."),
			reviews
		};
		reviewsSubtitle->setObjectName(QStringLiteral("reviewsSubtitle"));
		reviewsSubtitle->setWordWrap(true);
		reviewsColumn->addWidget(reviewsSubtitle);
		const auto settingsPage = [](const QString& aName, const QString& aAccessible, const QString& aSubtitle) {
			auto* const page = new QWidget;
			page->setObjectName(aName);
			page->setAccessibleName(aAccessible);
			auto* const pageColumn = new QVBoxLayout{ page };
			pageColumn->setContentsMargins(0, 0, 0, 0);
			auto* const subtitle = new QLabel{ aSubtitle, page };
			subtitle->setObjectName(aName + QStringLiteral("Subtitle"));
			subtitle->setWordWrap(true);
			pageColumn->addWidget(subtitle);
			return std::pair{ page, pageColumn };
		};
		const auto [reviewPage, reviewColumn]     = settingsPage(QStringLiteral("workbenchReviewPage"), QStringLiteral("Review"), QStringLiteral("How a pull request opens and reads. Local behaviour only."));
		const auto [commentsPage, commentsColumn] = settingsPage(QStringLiteral("workbenchCommentsPage"), QStringLiteral("Comments"), QStringLiteral("Labels, the comment workshop, and how comments read. Local behaviour only."));
		build_behaviour_editor(reviewPage, reviewColumn, commentsPage, commentsColumn);
		reviewColumn->addStretch(1);
		commentsColumn->addStretch(1);
		build_queue_settings(reviews, reviewsColumn);
		auto* const rulesHeading = new QLabel{ QStringLiteral("Observer rules"), reviews };
		rulesHeading->setObjectName(QStringLiteral("reviewsRulesHeading"));
		auto headingFont = rulesHeading->font();
		headingFont.setWeight(QFont::DemiBold);
		rulesHeading->setFont(headingFont);
		reviewsColumn->addWidget(rulesHeading);
		if (RulesEditor != nullptr) {
			RulesEditor->setParent(reviews);
			reviewsColumn->addWidget(RulesEditor);
		}
		// [[BPR-0659]]. Saved repositories moved to a page of their own
		// (RFC-BPR-0062, owner direction 2026-10-07). Observer rules stay: they
		// are queue behaviour over any repository, not the saved list. Home's
		// empty sentence still links here, so the page says where they went.
		auto* const savedHeading = new QLabel{ QStringLiteral("Saved repositories"), reviews };
		savedHeading->setObjectName(QStringLiteral("reviewsSavedHeading"));
		savedHeading->setFont(headingFont);
		reviewsColumn->addWidget(savedHeading);
		auto* const savedPointer = new QWidget{ reviews };
		savedPointer->setObjectName(QStringLiteral("reviewsRepositoriesPointer"));
		auto* const savedPointerRow = new QHBoxLayout{ savedPointer };
		savedPointerRow->setContentsMargins(0, 0, 0, 0);
		auto* const savedPointerText = new QLabel{ tr("Saved repositories are managed on their own page."), savedPointer };
		savedPointerText->setWordWrap(true);
		savedPointerText->setTextFormat(Qt::PlainText);
		savedPointerRow->addWidget(savedPointerText, 1);
		auto* const openRepositories = new QPushButton{ tr("Open Repositories"), savedPointer };
		openRepositories->setObjectName(QStringLiteral("reviewsOpenRepositories"));
		openRepositories->setAccessibleDescription(tr("Shows Settings › Repositories, where saved repositories are added and removed"));
		openRepositories->setAutoDefault(false);
		savedPointerRow->addWidget(openRepositories, 0, Qt::AlignRight);
		connect(openRepositories, &QPushButton::clicked, this, [this] { open_settings(settings_page::repositories); });
		reviewsColumn->addWidget(savedPointer);
		SettingsWindow->adopt_panel(settings_page::home_and_queue, reviews);
		SettingsWindow->adopt_panel(settings_page::review, reviewPage);
		SettingsWindow->adopt_panel(settings_page::comments, commentsPage);
		build_capability_settings();

		static_cast<void>(Drawers[0].Panel->layout());
		// [[BPR-0658]]. The files tree over the flat list: the list stays
		// the drawer's authority (the composition's courses select in it and
		// its selection dispatches the intent); the tree is what is seen, and
		// a row chosen in it becomes the list's selection.
		{
			auto* const filesSurface = new QWidget;
			filesSurface->setObjectName(QStringLiteral("workbenchFilesSurface"));
			auto* const surfaceColumn = new QVBoxLayout{ filesSurface };
			surfaceColumn->setContentsMargins(0, 0, 0, 0);
			surfaceColumn->setSpacing(2);
			auto* const controls = new QHBoxLayout{ };
			controls->setContentsMargins(4, 0, 4, 0);
			FilesOrderSelector = new QComboBox{ filesSurface };
			FilesOrderSelector->setObjectName(QStringLiteral("filesOrderSelector"));
			FilesOrderSelector->setAccessibleName(QStringLiteral("Files order"));
			FilesOrderSelector->setAccessibleDescription(QStringLiteral("Tree, flat, or the files changed since my last review first"));
			FilesOrderSelector->addItem(QStringLiteral("Tree"), static_cast<int>(files_order::tree));
			FilesOrderSelector->addItem(QStringLiteral("Flat"), static_cast<int>(files_order::flat));
			FilesOrderSelector->addItem(QStringLiteral("Changed first"), static_cast<int>(files_order::changed_first));
			controls->addWidget(FilesOrderSelector);
			FilesFilter = new QLineEdit{ filesSurface };
			FilesFilter->setObjectName(QStringLiteral("filesFilter"));
			FilesFilter->setAccessibleName(QStringLiteral("Filter files"));
			// [[BPR-0981]]. The resident fuzzy match of RFC-BPR-0025.
			FilesFilter->setAccessibleDescription(QStringLiteral(
				"Show only files whose name or path contains these letters in order; their folders stay visible"));
			FilesFilter->setPlaceholderText(QStringLiteral("Filter"));
			FilesFilter->setClearButtonEnabled(true);
			controls->addWidget(FilesFilter, 1);
			surfaceColumn->addLayout(controls);
			// RFC-BPR-0089. A commit filter is never active out of sight: the
			// chip names it here and opens the Commits tab.
			FilesCommitChip = new commit_filter_chip{ true, filesSurface };
			FilesCommitChip->set_clear_dispatch([this] {
				if (CommitFilterDispatch) {
					CommitFilterDispatch(application::commit_filter_gesture{ .Kind = core::commit_filter_operation_kind::clear, .Commits = { } });
				}
			});
			FilesCommitChip->set_commits_dispatch([this] { show_commits(); });
			surfaceColumn->addWidget(FilesCommitChip);
			FilesFilterNotice = new filter_empty_notice{
				QStringLiteral("files"), QStringLiteral("files"), filesSurface
			};
			FilesFilterNotice->hide();
			surfaceColumn->addWidget(FilesFilterNotice);
			FilesTree = new files_tree_view{ filesSurface };
			surfaceColumn->addWidget(FilesTree, 1);
			// RFC-BPR-0089 M2. The tree ends with the generated files set aside,
			// and every file's menu offers its generated gestures.
			FilesGeneratedRow = new generated_files_row{ filesSurface };
			FilesGeneratedRow->set_gesture_dispatch([this](const application::generated_file_gesture& aGesture) {
				if (GeneratedDispatch) {
					GeneratedDispatch(aGesture);
				}
			});
			// [[BPR-1693]]. A click opens a listed file, as one in the tree does.
			FilesGeneratedRow->set_open_on_click(true);
			surfaceColumn->addWidget(FilesGeneratedRow, 0);
			FilesTree->set_menu_extension([this](QMenu& aMenu, const QString& aPath) {
				if (!FilterProjection) {
					return;
				}
				const auto path = aPath.toStdString();
				add_generated_actions(aMenu, path, generated_gestures_for(*FilterProjection, path), GeneratedDispatch);
			});
			// [[BPR-1132]]. Cmd-click (Ctrl-click) and Open in New Tab.
			FilesTree->set_new_tab_dispatch([this](const core::workbench_file_id& aFile) {
				if (DocumentTabDispatch) {
					DocumentTabDispatch(core::open_in_new_tab_intent{ .File = aFile });
				}
			});
			FilesTree->set_select_dispatch([this](const core::workbench_file_id& aFile) {
				// The list's selection normally dispatches. A repeated tree
				// click is still an explicit open when its cursor is unchanged.
				if (ApplyingFileSelection || Drawers[0].Panel->contents() == nullptr
					|| Drawers[0].Panel->contents_view() == nullptr) {
					return;
				}
				const auto index = Drawers[0].Panel->contents()->index_for_file(aFile);
				if (index.isValid()) {
					if (Drawers[0].Panel->contents_view()->currentIndex() == index) {
						dispatch_file_selection(index);
					} else {
						Drawers[0].Panel->contents_view()->setCurrentIndex(index);
					}
				}
			});
			connect(FilesOrderSelector, qOverload<int>(&QComboBox::currentIndexChanged), this, [this] {
				FilesTree->set_order(static_cast<files_order>(FilesOrderSelector->currentData().toInt()));
			});
			connect(FilesFilter, &QLineEdit::textChanged, this, [this](const QString& aText) {
				FilesTree->set_filter(aText);
				refresh_filter_notices();
			});
			connect(FilesFilterNotice->clear_button(), &QAbstractButton::clicked, this, [this] {
				FilesFilter->clear();
			});
			FileSearchTabs = new QTabWidget;
			FileSearchTabs->setObjectName(QStringLiteral("fileSearchTabs"));
			FileSearchTabs->setAccessibleName(tr("Files and Search"));
			// [[BPR-1509]]. The tabs are the drawer's header, as in Comments/Notes.
			FileSearchTabs->setDocumentMode(true);
			FileSearchTabs->tabBar()->setExpanding(false);
			FileSearchTabs->tabBar()->setDrawBase(true);
			FileSearchTabs->tabBar()->setFocusPolicy(Qt::StrongFocus);
			Drawers[0].Panel->set_heading_shown(false);
			FileSearchTabs->addTab(filesSurface, tr("Files"));
			SearchPanel          = new source_search_panel{ FileSearchTabs };
			SearchPanel->Changed = [this] { update_source_find_actions(); };
			FileSearchTabs->addTab(SearchPanel, tr("Search"));
			Drawers[0].Panel->host_beside_list(FileSearchTabs, FilesTree);
		}
		// [[BPR-0671]]. The pull request's identity, author, branches, state
		// and description are the overview's, and the strip already names the
		// repository, the number and the title. [[BPR-0646]] parked them in a
		// bounded box at the top of the files drawer until the overview
		// ([[BPR-0647]]) took them, and the placeholder shipped: the owner met
		// "the entire set of information for this PR crammed into a tiny box
		// in files browser", above the files it was covering. The box is gone;
		// the widget survives as Home's, where its empty page says why a load
		// failed. The files drawer holds files.
		// The description lives in this column, which no longer has a place
		// in the window: the overview presents it. The widget is kept, parented
		// and hidden, so the panel it belongs to keeps its shape and nothing
		// that writes to it has to learn a new one.
		auto* const top = new QWidget{ this };
		top->setObjectName(QStringLiteral("workbenchDrawerTop"));
		top->setAccessibleName(QStringLiteral("Pull request"));
		top->hide();
		auto* const topColumn = new QVBoxLayout{ top };
		topColumn->setContentsMargins(0, 0, 0, 0);
		DrawerTopColumn = topColumn;
		// Switching to sign-in is offered from the ring and from every
		// refusal, and both go to the one entry point already mounted here:
		// the device panel's "Get a code", for the host last chosen. The
		// window adds no second way in, so sign-in cannot begin against a
		// host the reviewer is not looking at.
		set_public_preview_dispatch(
			[this](application::public_preview_gesture aGesture) {
				static_cast<void>(aGesture);
				begin_selected_sign_in();
			});

		// The empty state is not decoration. Without it a reviewer with no pull
		// request open sees a comparison of nothing and no indication of what to
		// do, which is what the shell shipped for exactly one commit before this
		// restored it. The session chrome stays visible either way, because
		// connecting is the action the empty state is asking for.
		PullRequestStack = new QStackedWidget{ top };
		PullRequestStack->setObjectName(QStringLiteral("githubViewState"));
		auto* const emptyState = new QWidget{ PullRequestStack };
		emptyState->setObjectName(QStringLiteral("githubEmptyState"));
		auto* const emptyLayout = new QVBoxLayout{ emptyState };
		EmptyStateTitleLabel    = new QLabel{ emptyState };
		EmptyStateTitleLabel->setObjectName(QStringLiteral("githubEmptyStateTitle"));
		EmptyStateTitleLabel->setAccessibleDescription(QStringLiteral("GitHub view state"));
		EmptyStateTitleLabel->setAlignment(Qt::AlignCenter);
		EmptyStateTitleLabel->setWordWrap(true);
		detail::set_plain_text_label(EmptyStateTitleLabel);
		EmptyStateTitleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		EmptyStateBodyLabel = new QLabel{ emptyState };
		EmptyStateBodyLabel->setObjectName(QStringLiteral("githubEmptyStateBody"));
		EmptyStateBodyLabel->setAccessibleDescription(QStringLiteral("GitHub view guidance"));
		EmptyStateBodyLabel->setAlignment(Qt::AlignCenter);
		EmptyStateBodyLabel->setWordWrap(true);
		detail::set_plain_text_label(EmptyStateBodyLabel);
		EmptyStateBodyLabel->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		emptyLayout->addStretch(1);
		emptyLayout->addWidget(EmptyStateTitleLabel);
		emptyLayout->addWidget(EmptyStateBodyLabel);
		auto* const copyMessage = new QToolButton{ emptyState };
		copyMessage->setObjectName(QStringLiteral("copyGithubMessageButton"));
		copyMessage->setText(QStringLiteral("Copy message"));
		copyMessage->setAccessibleName(QStringLiteral("Copy GitHub message"));
		connect(copyMessage, &QToolButton::clicked, this, [this] {
			if (auto* const clipboard = QApplication::clipboard(); clipboard != nullptr) {
				clipboard->setText(EmptyStateTitleLabel->text() + QStringLiteral("\n")
								   + EmptyStateBodyLabel->text());
			}
		});
		emptyLayout->addWidget(copyMessage, 0, Qt::AlignHCenter);
		emptyLayout->addStretch(2);
		PullRequestStack->addWidget(emptyState);

		auto* const content = new QWidget{ PullRequestStack };
		content->setObjectName(QStringLiteral("githubPullRequestContent"));
		auto* const contentLayout = new QVBoxLayout{ content };
		contentLayout->setContentsMargins(0, 0, 0, 0);
		contentLayout->addWidget(PullRequestPanel);
		PullRequestStack->addWidget(content);
		// Bounded: the drawer's files list is what the drawer is for, and a
		// long summary must not be able to take the height it needs.
		PullRequestStack->setMaximumHeight(200);
		// [[BPR-0775]]. The stack's empty page is not Home's, and it has no other
		// place: it stays hidden here so what writes to it keeps its shape.
		PullRequestStack->hide();
		// Placed by update_home_landing: on Home while nothing is open, at
		// the top of the drawer's region once a pull request is.
		// The panel builds the description but deliberately does not lay it out:
		// where a description belongs is a decision for whoever assembles the
		// window, and this one puts it under the metadata it describes. Bounded
		// rather than stretching, for the same reason.
		PullRequestPanel->body_editor()->setMaximumHeight(120);
		topColumn->addWidget(PullRequestPanel->body_editor());
		// [[BPR-0671]]. The one thing the summary box carried that the
		// overview did not: the commit oids behind the branches.
		if (OverviewPage != nullptr) {
			OverviewPage->adopt_technical_details(
				PullRequestPanel->technical_details_button(),
				PullRequestPanel->technical_details());
		}

		// [[BPR-0646]]. The appearance surface moves out of the review header
		// into the Settings window, a modeless child of this window reached by
		// the preferences shortcut. The composition still finds the same panel
		// through appearance_settings(), so its wiring is unchanged.
		AppearanceSettings = new appearance_settings_panel{ { } };
		AppearanceSettings->setObjectName(
			QStringLiteral("appearanceSettingsPanel"));
		SettingsWindow->adopt_panel(settings_page::appearance, AppearanceSettings);
		SettingsWindow->set_close_handler([this] { AppearanceSettings->cancel_preview(); });

		CentralStack = new QStackedWidget{ this };
		CentralStack->setObjectName(QStringLiteral("workbenchCentralStack"));
		CentralStack->addWidget(HomePage);
		CentralStack->addWidget(ReviewSurface);
		setCentralWidget(CentralStack);
		connect(CentralStack, &QStackedWidget::currentChanged, statusBar(), [this] { statusBar()->update(); });

		// [[BPR-0646]]. Refresh sits in the strip's right region, before the
		// comments toggle, as the design places it. Same action as the
		// session chrome's button, so there is one refresh with one state.
		PrimaryToolbar->insertAction(Drawers[1].Toggle, GithubRefreshAction);
		PrimaryToolbar->insertAction(Drawers[1].Toggle, QueueRefreshAction);
		if (auto* const glyph = qobject_cast<QToolButton*>(PrimaryToolbar->widgetForAction(QueueRefreshAction)); glyph != nullptr) {
			glyph->setFocusPolicy(Qt::StrongFocus);
			glyph->setAccessibleName(QStringLiteral("Refresh my pull requests"));
		}
		// [[BPR-0783]], [[BPR-1580]]. Home's View control sits before the refresh
		// glyph, so the strip reads View, refresh, the divider, account and
		// settings.
		build_home_strip_controls();
		// The action's default state is applied once everything it governs
		// exists; a default that is never applied is a default in name only.
		apply_review_details_visibility();

		SessionPanel->apply_capabilities({ });
		SessionPanel->apply_rate_limits({ });
		clear_github_pull_request();
		SessionPanel->update_connect_enabled();
		// Seeded, exactly as the window it replaces seeded them. A command's
		// enablement and its disabled reason arrive from projections, and until
		// one does every one of these stops would carry an empty accessible
		// description -- which the shell's own "every interactive element is
		// named and described" assertion catches.
		SessionPanel->apply_command({
			.Id             = core::github_command_id::open_pull_request,
			.Enabled        = false,
			.DisabledReason = core::github_reason_code::session_required,
		});
		SessionPanel->apply_command({
			.Id             = core::github_command_id::refresh_pull_request,
			.Enabled        = false,
			.DisabledReason = core::github_reason_code::no_active_pull_request,
		});
		PullRequestPanel->refresh_native_appearance();
		build_pending_veils();
		// [[BPR-1575]]. A refresh's note under its button, its freshness in
		// the status bar, and the clocks that turn and age them.
		RefreshNote = new refresh_note{ *this };
		RefreshNote->set_retry([this] {
			auto* const action = showing_home() ? QueueRefreshAction : GithubRefreshAction;
			if (action != nullptr && action->isEnabled()) {
				action->trigger();
			}
		});
		RefreshFreshness = new QLabel{ this };
		RefreshFreshness->setObjectName(QStringLiteral("refreshFreshness"));
		RefreshFreshness->setAccessibleName(QStringLiteral("Last refresh"));
		RefreshFreshness->setTextFormat(Qt::RichText);
		RefreshFreshness->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
		RefreshFreshness->setForegroundRole(QPalette::PlaceholderText);
		// Its words whole: the status line beside it is the one that yields.
		RefreshFreshness->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
		RefreshFreshness->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
		RefreshFreshness->hide();
		connect(RefreshFreshness, &QLabel::linkActivated, this, [this](const QString&) {
			auto* const action = showing_home() ? QueueRefreshAction : GithubRefreshAction;
			if (action != nullptr && action->isEnabled()) {
				action->trigger();
			}
		});
		statusBar()->addPermanentWidget(RefreshFreshness);
		RefreshFrames = new QTimer{ this };
		RefreshFrames->setInterval(80);
		connect(RefreshFrames, &QTimer::timeout, this, [this] {
			RefreshFrame = (RefreshFrame + 1) % 12;
			present_refresh();
		});
		const auto cue = [this](bool& aCue) {
			auto* const timer = new QTimer{ this };
			timer->setSingleShot(true);
			timer->setInterval(1'500);
			connect(timer, &QTimer::timeout, this, [this, &aCue] {
				aCue = false;
				present_refresh();
			});
			return timer;
		};
		PullCueTimer    = cue(PullSettledCue);
		HomeCueTimer    = cue(HomeSettledCue);
		RefreshAgeTimer = new QTimer{ this };
		RefreshAgeTimer->setSingleShot(true);
		connect(RefreshAgeTimer, &QTimer::timeout, this, [this] { present_refresh(); });
		for (auto** const slow : { &RefreshSlowTimer, &HomeSlowTimer }) {
			*slow = new QTimer{ this };
			(*slow)->setSingleShot(true);
			(*slow)->setInterval(static_cast<int>(core::pending_slow_milliseconds));
			connect(*slow, &QTimer::timeout, this, [this] { present_refresh(); });
		}
		update_github_presentation_state();
		apply_focus_order();
	}

	void workbench_window::build_queue_landing(
		QWidget* const aParent, QVBoxLayout& aLayout)
	{
		auto* const surface = new QWidget{ aParent };
		surface->setObjectName(QStringLiteral("reviewQueueLanding"));
		// Bounded so the file list above it keeps the drawer\'s height.
		surface->setMaximumHeight(240);
		surface->setAccessibleName(QStringLiteral("Local review queue"));
		surface->setAccessibleDescription(QStringLiteral(
			"Local observer rules and the pull requests they enrolled; rules never change GitHub assignments."));
		auto* const layout = new QVBoxLayout{ surface };
		layout->setContentsMargins(0, 0, 0, 0);

		QueueStatus = new QLabel{ QStringLiteral("Restoring local observer rules…"), surface };
		QueueStatus->setObjectName(QStringLiteral("reviewQueueStatus"));
		QueueStatus->setAccessibleName(QStringLiteral("Review queue status"));
		QueueStatus->setWordWrap(true);
		layout->addWidget(QueueStatus);
		// [[BPR-0990]]. What discovery last did, in one sentence, where Home
		// already reports what the queue refused.
		HomeRepositoryRow = new QWidget{ HomePage };
		HomeRepositoryRow->setObjectName(QStringLiteral("homeRepositoryRow"));
		auto* repositoryRow = new QBoxLayout{ QBoxLayout::LeftToRight, HomeRepositoryRow };
		repositoryRow->setContentsMargins(24, 0, 24, 4);
		HomeDiscoveryStatus = new QLabel{ HomeRepositoryRow };
		HomeDiscoveryStatus->setObjectName(QStringLiteral("homeDiscoveryStatus"));
		HomeDiscoveryStatus->setAccessibleName(QStringLiteral("My pull requests status"));
		HomeDiscoveryStatus->setWordWrap(true);
		HomeDiscoveryStatus->setVisible(false);
		repositoryRow->addWidget(HomeDiscoveryStatus, 1);
		AddSavedRepository = new QAction{ tr("Add repository"), this };
		AddSavedRepository->setObjectName(QStringLiteral("addSavedRepository"));
		connect(AddSavedRepository, &QAction::triggered, this, [this] { open_repository_addition(); });
		HomeAddRepositoryButton = new QToolButton{ HomeRepositoryRow };
		HomeAddRepositoryButton->setObjectName(QStringLiteral("homeAddRepositoryButton"));
		HomeAddRepositoryButton->setDefaultAction(AddSavedRepository);
		HomeAddRepositoryButton->setFocusPolicy(Qt::StrongFocus);
		HomeAddRepositoryButton->setAccessibleName(tr("Add repository"));
		HomeAddRepositoryButton->setAccessibleDescription(tr("Add a repository to find my pull requests and review requests"));
		repositoryRow->addWidget(HomeAddRepositoryButton, 0, Qt::AlignLeft);
		// RFC-BPR-0062, owner verdicts of 2026-10-07 and 2026-10-08: with no
		// saved repository for the displayed account and host, the row gives way
		// to one default-styled button in its place under the bar, "Just the
		// button that opens settings". It opens Settings › Repositories with the
		// repository field ready, as every Add repository does.
		HomeOnboarding = new QWidget{ HomePage };
		HomeOnboarding->setObjectName(QStringLiteral("homeRepositoryOnboarding"));
		HomeOnboarding->setAccessibleName(tr("Add a repository"));
		auto* const onboarding = new QVBoxLayout{ HomeOnboarding };
		onboarding->setContentsMargins(24, 16, 24, 8);
		onboarding->setSpacing(0);
		HomeOnboardingButton = new QPushButton{ tr("Add a repository"), HomeOnboarding };
		HomeOnboardingButton->setObjectName(QStringLiteral("homeRepositoryOnboardingButton"));
		HomeOnboardingButton->setAccessibleName(tr("Add a repository"));
		HomeOnboardingButton->setAccessibleDescription(tr("Opens Settings, Repositories, to add a repository where my pull requests and review requests are found; nothing changes on GitHub"));
		HomeOnboardingButton->setToolTip(tr("Find my pull requests and review requests in a repository"));
		HomeOnboardingButton->setFocusPolicy(Qt::StrongFocus);
		HomeOnboardingButton->setAutoDefault(false);
		// The default look only: the main window has no default-button key.
		HomeOnboardingButton->setDefault(true);
		connect(HomeOnboardingButton, &QPushButton::clicked, AddSavedRepository, &QAction::trigger);
		connect(AddSavedRepository, &QAction::changed, HomeOnboardingButton, [this] {
			HomeOnboardingButton->setEnabled(AddSavedRepository->isEnabled());
		});
		onboarding->addWidget(HomeOnboardingButton, 0, Qt::AlignLeft);
		if (auto* homeColumn = qobject_cast<QVBoxLayout*>(HomePage->layout())) {
			homeColumn->insertWidget(homeColumn->indexOf(HomeCycles), HomeOnboarding);
			homeColumn->insertWidget(homeColumn->indexOf(HomeCycles), HomeRepositoryRow);
		}
		HomeOnboarding->hide();
		HomeRepositoryRow->hide();

		QueueLanding     = new review_landing_model{ surface };
		QueueFilterModel = new review_landing_filter_model{ *QueueLanding, surface };
		// [[BPR-0650]] [[BPR-0776]]. Home's list: a panel per state, in state
		// order, over the same decided queue, and Recently viewed last. A
		// click or Return on a row opens its pull request on Overview, as a
		// row of the repositories tree does; an empty queue's sentence links
		// to Home & queue.
		HomeList = new qt::home_list{ *QueueFilterModel, surface };
		// The built-in state colours until the composition hands over the
		// profile it resolved.
		HomeList->set_state_colours(home_state_colours_for(appearance_profile{ }));
		HomeList->set_open_dispatch([this](const core::queue_item& aItem) { open_pull_request_item(aItem); });
		// [[BPR-1357]]. Cancel or Retry on the row being opened.
		HomeList->set_open_action_dispatch([this] { dispatch_home_open_action(); });
		// [[BPR-1338]]. Open on GitHub from a queue row: its icon names the row
		// directly; its menu holds the window's action, which names the row the
		// right click made current, as the action's key does.
		HomeList->set_page_dispatch([this](const core::queue_item& aItem) { open_pull_request_page(aItem.Ref); });
		HomeList->set_current_observer([this] {
			update_open_on_github_action();
			update_home_review_actions();
		});
		// [[BPR-1342]]. ⌘Return, ⌘-click or middle-click on a queue row.
		HomeList->set_new_window_dispatch([this](const core::queue_item& aItem) { request_review_window(aItem.Ref); });
		// Which of Home's lists the reviewer is in: the one that last took the
		// keyboard. A menu taking it for a moment changes nothing. [[BPR-1365]]:
		// it is also the one list that shows a selection.
		connect(qApp, &QApplication::focusChanged, this, [this](QWidget*, QWidget* const aNow) {
			if (aNow == nullptr || HomeList == nullptr) {
				return;
			}
			if (aNow == HomeRecentList) {
				claim_home_selection(home_selection_list::recent);
			} else if (aNow == HomeCyclesList) {
				claim_home_selection(home_selection_list::cycles);
			} else if (qobject_cast<QListView*>(aNow) != nullptr && HomeList->isAncestorOf(aNow)) {
				claim_home_selection(home_selection_list::queue);
			}
		});
		// [[BPR-1342]]. Home's one row menu, for a queue row and a Recently
		// viewed row alike: Open Review (the default, in this window), Open
		// Review in New Window, Resume for a Recently viewed row, then Open on
		// GitHub. Each shows its key. The keys are read by the lists
		// themselves, so these shortcuts are context-bound to the menu and
		// never act from the window.
		const auto menuKey = [](QAction& aAction, const shortcut_command aKey) {
			aAction.setShortcuts(shortcut_keys(aKey));
			aAction.setShortcutContext(Qt::WidgetShortcut);
			aAction.setShortcutVisibleInContextMenu(true);
		};
		HomeOpenReviewAction = new QAction{ tr("Open Review"), this };
		HomeOpenReviewAction->setObjectName(QStringLiteral("homeOpenReviewAction"));
		HomeOpenReviewAction->setStatusTip(QStringLiteral("Open this pull request in this window"));
		menuKey(*HomeOpenReviewAction, shortcut_command::home_open_review);
		HomeOpenReviewAction->setEnabled(false);
		connect(HomeOpenReviewAction, &QAction::triggered, this, [this] { open_home_review(false); });
		HomeOpenReviewWindowAction = new QAction{ tr("Open Review in New Window"), this };
		HomeOpenReviewWindowAction->setObjectName(QStringLiteral("homeOpenReviewInNewWindowAction"));
		HomeOpenReviewWindowAction->setStatusTip(QStringLiteral("Open this pull request in a new window of this profile"));
		menuKey(*HomeOpenReviewWindowAction, shortcut_command::home_open_window);
		HomeOpenReviewWindowAction->setEnabled(false);
		connect(HomeOpenReviewWindowAction, &QAction::triggered, this, [this] { open_home_review(true); });
		// [[BPR-1683]]. Ignore, a queue row's only; Unignore on an ignored
		// row. Delete (or Backspace) is read by the lists. Delete is the tour
		// editor's Remove in this window, so, as Remove from Recently Viewed,
		// the action claims no sequence; the catalogue names the key.
		HomeIgnoreAction = new QAction{ home_ignore_text(), this };
		HomeIgnoreAction->setObjectName(QStringLiteral("homeIgnoreAction"));
		HomeIgnoreAction->setStatusTip(QStringLiteral("Moves this pull request to Ignored at the bottom of Home; nothing changes on GitHub"));
		HomeIgnoreAction->setEnabled(false);
		connect(HomeIgnoreAction, &QAction::triggered, this, [this] {
			if (const auto target = home_review_target(); target.has_value()) {
				if (const auto* const item = std::get_if<core::queue_item>(&*target)) {
					dispatch_home_ignore(*item);
				}
			}
		});
		HomeList->set_ignore_dispatch([this](const core::queue_item& aItem) { dispatch_home_ignore(aItem); });
		OpenOnGithubAction->setShortcutVisibleInContextMenu(true);
		HomeRowMenu = new QMenu{ this };
		HomeRowMenu->setObjectName(QStringLiteral("homeRowMenu"));
		HomeRowMenu->setAccessibleName(QStringLiteral("Pull request row"));
		HomeRowMenu->addAction(HomeOpenReviewAction);
		HomeRowMenu->addAction(HomeOpenReviewWindowAction);
		if (HomeRecentResumeAction != nullptr) {
			HomeRowMenu->addAction(HomeRecentResumeAction);
		}
		HomeRowMenu->addSeparator();
		HomeRowMenu->addAction(OpenOnGithubAction);
		HomeRowMenu->addSeparator();
		HomeRowMenu->addAction(HomeIgnoreAction);
		if (HomeRecentRemoveAction != nullptr) {
			HomeRowMenu->addSeparator();
			HomeRowMenu->addAction(HomeRecentRemoveAction);
		}
		HomeRowMenu->setDefaultAction(HomeOpenReviewAction);
		connect(HomeRowMenu, &QMenu::aboutToHide, this, [this] {
			for (auto* const action : { HomeRecentResumeAction, HomeRecentRemoveAction }) {
				if (action != nullptr) {
					action->setVisible(true);
				}
			}
		});
		HomeList->set_row_menu_dispatch([this](const QPoint& aGlobal) {
			claim_home_selection(home_selection_list::queue);
			popup_home_row_menu(aGlobal);
		});
		// The empty lane's link names the saved repositories, so it opens their
		// page; observer rules stay reachable from Filter's Open Home & queue.
		HomeList->set_settings_dispatch([this] { open_settings(settings_page::repositories); });
		// [[BPR-1580]]. The filtered sentence's Clear filters.
		HomeList->set_clear_filters_dispatch([this] { restore_home_view_defaults(false); });
		// [[BPR-0783]]. Collapsing one of the panels that carry no name is part of
		// this window's view, so the whole view leaves for the host.
		HomeList->set_view_observer([this] {
			HomeView.Collapsed = HomeList->collapsed_panels();
			dispatch_home_view();
		});
		layout->addWidget(HomeList, 1);

		// [[BPR-0783]]. The six typed queue controls are built here and laid out
		// on Settings' Queue defaults (build_queue_settings): Home's Filter menu
		// holds the four that survive as filters, and what a new window starts
		// with is a setting. They are the same controls, not a mirror of them.
		QueueRoleFilter = new QComboBox{ surface };
		QueueRoleFilter->setObjectName(QStringLiteral("queueRoleFilter"));
		QueueRoleFilter->setAccessibleName(QStringLiteral("Queue role filter"));
		QueueRoleFilter->setAccessibleDescription(QStringLiteral(
			"Show any GitHub role or one exact RFC review role"));
		QueueRoleFilter->addItem(tr("Role: any"), -1);
		for (const auto role : { core::github_role::none,
								 core::github_role::participant,
								 core::github_role::reviewer,
								 core::github_role::requested_reviewer,
								 core::github_role::author }) {
			QueueRoleFilter->addItem(
				tr("Role: %1").arg(QString::fromUtf8(core::name(role).data())),
				static_cast<int>(role));
		}
		QueueObserverFilter = new QComboBox{ surface };
		QueueObserverFilter->setObjectName(QStringLiteral("queueObserverFilter"));
		QueueObserverFilter->setAccessibleName(
			QStringLiteral("Queue observer reason filter"));
		QueueObserverFilter->setAccessibleDescription(QStringLiteral(
			"Show any observer reason or the exact rule kind that enrolled it"));
		QueueObserverFilter->addItem(tr("Observer: any reason"), -1);
		for (const auto kind : { core::observer_rule_kind::repository,
								 core::observer_rule_kind::author,
								 core::observer_rule_kind::path }) {
			QueueObserverFilter->addItem(
				tr("Observer: %1 rule")
					.arg(QString::fromUtf8(core::name(kind).data())),
				static_cast<int>(kind));
		}
		QueueBucketFilter = new QComboBox{ surface };
		QueueBucketFilter->setObjectName(QStringLiteral("queueBucketFilter"));
		QueueBucketFilter->setAccessibleName(
			QStringLiteral("Queue attention bucket filter"));
		QueueBucketFilter->setAccessibleDescription(QStringLiteral(
			"Show any attention bucket or one exact core-decided reason"));
		QueueBucketFilter->addItem(tr("Bucket: any"), -1);
		for (const auto bucket : { core::attention_bucket::initial_review_requested,
								   core::attention_bucket::changed_since_my_review,
								   core::attention_bucket::new_comments_or_author_replies,
								   core::attention_bucket::fixes_awaiting_verification,
								   core::attention_bucket::authored_awaiting_reviewer_action,
								   core::attention_bucket::observed_with_new_activity,
								   core::attention_bucket::quiet,
								   core::attention_bucket::detail_unknown }) {
			QueueBucketFilter->addItem(
				tr("Bucket: %1").arg(QString::fromUtf8(core::name(bucket).data())),
				static_cast<int>(bucket));
		}
		QueueRepositoryFilter = new QComboBox{ surface };
		QueueRepositoryFilter->setObjectName(
			QStringLiteral("queueRepositoryFilter"));
		QueueRepositoryFilter->setAccessibleName(
			QStringLiteral("Queue repository filter"));
		QueueRepositoryFilter->setAccessibleDescription(QStringLiteral(
			"Show every repository or one exact repository in this queue"));
		QueueRepositoryFilter->addItem(tr("Repository: any"), QString{ });

		QueueQuietFilter = new QComboBox{ surface };
		QueueQuietFilter->setObjectName(QStringLiteral("queueQuietFilter"));
		QueueQuietFilter->setAccessibleName(
			QStringLiteral("Queue quiet or current filter"));
		QueueQuietFilter->setAccessibleDescription(QStringLiteral(
			"Show every review, only attention, or only quiet and current reviews"));
		QueueQuietFilter->addItem(
			tr("State: any"),
			static_cast<int>(application::queue_quiet_filter::all));
		QueueQuietFilter->addItem(
			tr("State: needs attention"),
			static_cast<int>(application::queue_quiet_filter::attention));
		QueueQuietFilter->addItem(
			tr("State: quiet/current"),
			static_cast<int>(application::queue_quiet_filter::quiet));

		QueueSort = new QComboBox{ surface };
		QueueSort->setObjectName(QStringLiteral("queueSortSelector"));
		QueueSort->setAccessibleName(QStringLiteral("Queue sort order"));
		QueueSort->setAccessibleDescription(QStringLiteral(
			"Core priority is the default; alternate orders apply only when selected"));
		QueueSort->addItem(
			tr("Sort: core priority"),
			static_cast<int>(application::queue_sort_order::core_priority));
		QueueSort->addItem(
			tr("Sort: repository, then PR"),
			static_cast<int>(application::queue_sort_order::repository));
		QueueSort->addItem(
			tr("Sort: PR number, then identity"),
			static_cast<int>(application::queue_sort_order::pull_request_number));

		for (auto* const selector : { QueueRoleFilter,
									  QueueObserverFilter,
									  QueueBucketFilter,
									  QueueRepositoryFilter,
									  QueueQuietFilter,
									  QueueSort }) {
			connect(
				selector,
				qOverload<int>(&QComboBox::currentIndexChanged),
				this,
				[this, selector] { dispatch_queue_preferences(selector); });
		}

		auto* const controls = new QHBoxLayout{ };
		QueueRuleSelector    = new QComboBox{ surface };
		QueueRuleSelector->setObjectName(QStringLiteral("observerRuleSelector"));
		QueueRuleSelector->setAccessibleName(QStringLiteral("Saved observer rules"));
		QueueRuleSelector->setAccessibleDescription(QStringLiteral(
			"Select an exact local rule to remove"));
		controls->addWidget(QueueRuleSelector);
		QueueRuleKind = new QComboBox{ surface };
		QueueRuleKind->setObjectName(QStringLiteral("observerRuleKind"));
		QueueRuleKind->setAccessibleName(QStringLiteral("Observer rule kind"));
		QueueRuleKind->setAccessibleDescription(QStringLiteral(
			"Choose repository, author, or changed path"));
		for (const auto kind : { core::observer_rule_kind::repository,
								 core::observer_rule_kind::author,
								 core::observer_rule_kind::path }) {
			QueueRuleKind->addItem(
				QString::fromUtf8(core::name(kind).data()),
				static_cast<int>(kind));
		}
		controls->addWidget(QueueRuleKind);
		QueueRulePattern = new QLineEdit{ surface };
		QueueRulePattern->setObjectName(QStringLiteral("observerRulePattern"));
		QueueRulePattern->setAccessibleName(QStringLiteral("Observer rule pattern"));
		QueueRulePattern->setAccessibleDescription(QStringLiteral(
			"Repository owner/name, author login, or path prefix"));
		QueueRulePattern->setPlaceholderText(QStringLiteral("owner/name, login, or path"));
		QueueRulePattern->setMaxLength(
			static_cast<int>(core::maximum_observer_rule_pattern_bytes));
		controls->addWidget(QueueRulePattern, 1);
		QueueRuleExcluding = new QCheckBox{ tr("Exclude"), surface };
		QueueRuleExcluding->setObjectName(QStringLiteral("observerRuleExcluding"));
		QueueRuleExcluding->setAccessibleName(QStringLiteral("Exclude matching rule"));
		QueueRuleExcluding->setAccessibleDescription(QStringLiteral(
			"Exclude a matching local observation; GitHub roles remain visible"));
		controls->addWidget(QueueRuleExcluding);

		AddQueueRule = new QAction{ tr("Add local rule"), this };
		AddQueueRule->setObjectName(QStringLiteral("addObserverRule"));
		RemoveQueueRule = new QAction{ tr("Remove selected rule"), this };
		RemoveQueueRule->setObjectName(QStringLiteral("removeObserverRule"));
		addAction(AddQueueRule);
		AddQueueRuleButton = new QToolButton{ surface };
		AddQueueRuleButton->setDefaultAction(AddQueueRule);
		AddQueueRuleButton->setObjectName(
			QStringLiteral("addObserverRuleButton"));
		AddQueueRuleButton->setAccessibleName(
			QStringLiteral("Add local observer rule"));
		AddQueueRuleButton->setAccessibleDescription(QStringLiteral(
			"Persist the configured observer rule locally without changing GitHub"));
		controls->addWidget(AddQueueRuleButton);

		addAction(RemoveQueueRule);
		RemoveQueueRuleButton = new QToolButton{ surface };
		RemoveQueueRuleButton->setDefaultAction(RemoveQueueRule);
		RemoveQueueRuleButton->setObjectName(
			QStringLiteral("removeObserverRuleButton"));
		RemoveQueueRuleButton->setAccessibleName(
			QStringLiteral("Remove selected observer rule"));
		RemoveQueueRuleButton->setAccessibleDescription(QStringLiteral(
			"Delete the selected persisted local observer rule"));
		controls->addWidget(RemoveQueueRuleButton);
		connect(AddQueueRule, &QAction::triggered, this, [this] {
			dispatch_queue_rule(application::queue_rule_action::add);
		});
		connect(RemoveQueueRule, &QAction::triggered, this, [this] {
			dispatch_queue_rule(application::queue_rule_action::remove);
		});
		connect(QueueRulePattern, &QLineEdit::textChanged, this, [this] {
			update_queue_rule_actions();
		});
		connect(QueueRuleSelector,
				qOverload<int>(&QComboBox::currentIndexChanged),
				this,
				[this] { update_queue_rule_actions(); });
		// [[BPR-0652]]. The rule editor is a settings surface: it lives on
		// Home & queue, adopted there once the Settings window exists.
		// Home keeps the queue and its filters.
		RulesEditor = new QWidget;
		RulesEditor->setObjectName(QStringLiteral("observerRulesEditor"));
		RulesEditor->setAccessibleName(QStringLiteral("Observer rules"));
		controls->setContentsMargins(0, 0, 0, 0);
		RulesEditor->setLayout(controls);
		aLayout.addWidget(surface);
		update_queue_rule_actions();
	}

	void workbench_window::set_queue_rule_dispatch(
		workbench_queue_rule_dispatch aDispatch)
	{
		QueueRuleDispatch = std::move(aDispatch);
		update_queue_rule_actions();
	}

	void workbench_window::build_capability_settings()
	{
		auto* const agents      = new QWidget;
		auto* const column      = new QVBoxLayout{ agents };
		auto* const explanation = new QLabel{ QStringLiteral("The selected host determines which agent profiles are allowed. The request choices here apply to the next explicit request in the current review; they are not saved as global defaults. Private notes, the current draft and uncommitted work are included only when selected. Start the request from the review's agent panel."), agents };
		explanation->setWordWrap(true);
		column->addWidget(explanation);
		auto* const form    = new QFormLayout{ };
		auto* const profile = new QComboBox{ agents };
		profile->setObjectName(QStringLiteral("settingsAgentProfile"));
		profile->setAccessibleName(QStringLiteral("Allowed agent profile for this request"));
		profile->setModel(AgentPanel->profile_selector()->model());
		form->addRow(QStringLiteral("Allowed profile"), profile);
		auto* const notes = new QListView{ agents };
		notes->setObjectName(QStringLiteral("settingsAgentNotes"));
		notes->setAccessibleName(QStringLiteral("Private notes included with this request"));
		notes->setModel(AgentPanel->note_selector()->model());
		notes->setMaximumHeight(160);
		form->addRow(QStringLiteral("Private notes"), notes);
		auto* const draft = new QCheckBox{ agents };
		draft->setObjectName(QStringLiteral("settingsAgentDraft"));
		draft->setAccessibleName(QStringLiteral("Include current private draft"));
		form->addRow(QStringLiteral("Private draft"), draft);
		column->addLayout(form);
		auto* const checkout = new QToolButton{ agents };
		checkout->setDefaultAction(AgentPanel->existing_checkout_action());
		checkout->setAccessibleName(QStringLiteral("Use existing working copy for this request"));
		column->addWidget(checkout);
		auto* const scope = new QLabel{ agents };
		scope->setTextFormat(Qt::PlainText);
		scope->setWordWrap(true);
		scope->setAccessibleName(QStringLiteral("Agent request scope"));
		column->addWidget(scope);
		const auto refresh = [page = QPointer<QWidget>{ agents }, panel = QPointer<agent_launch_panel>{ AgentPanel }, profile, notes, draft, scope] {
			if (!page || !panel) {
				return;
			}
			const auto profileBlock = QSignalBlocker{ profile };
			const auto draftBlock   = QSignalBlocker{ draft };
			profile->setCurrentIndex(panel->profile_selector()->currentIndex());
			profile->setEnabled(panel->profile_selector()->isEnabled() && profile->count() != 0);
			notes->setEnabled(panel->note_selector()->isEnabled());
			draft->setText(panel->draft_selector()->text());
			draft->setChecked(panel->draft_selector()->isChecked());
			draft->setEnabled(panel->draft_selector()->isEnabled());
			scope->setText(panel->disclosure_text().isEmpty() ? QStringLiteral("Open a review to see its allowed profiles and request scope.") : panel->disclosure_text());
		};
		connect(profile, &QComboBox::currentIndexChanged, AgentPanel->profile_selector(), &QComboBox::setCurrentIndex);
		connect(draft, &QCheckBox::toggled, AgentPanel->draft_selector(), &QCheckBox::setChecked);
		connect(AgentPanel->draft_selector(), &QCheckBox::toggled, agents, refresh);
		AgentPanel->set_configuration_observer(refresh);
		// [[BPR-1301]]. Saved for the profile, unlike the choices above, so
		// it sits apart under its own heading, beside the writing hooks.
		ClaudeWrapper = new claude_wrapper_panel{ agents };
		column->addSpacing(12);
		column->addWidget(ClaudeWrapper);
		WritingHooks = new writing_hooks_panel{ agents };
		column->addWidget(WritingHooks);
		WritingHooks->present_label_helper(Behaviour.LabelHelper);
		SettingsWindow->adopt_panel(settings_page::agents_and_context, agents);

		auto* const storage       = new QWidget;
		auto* const storageColumn = new QVBoxLayout{ storage };
		auto* const heading       = new QLabel{ QStringLiteral("Local data location"), storage };
		storageColumn->addWidget(heading);
		StorageLocation = new QLineEdit{ storage };
		StorageLocation->setObjectName(QStringLiteral("settingsStorageLocation"));
		StorageLocation->setAccessibleName(QStringLiteral("Local application data location"));
		StorageLocation->setReadOnly(true);
		StorageLocation->setPlaceholderText(QStringLiteral("Location unavailable"));
		storageColumn->addWidget(StorageLocation);
		auto* const facts = new QLabel{ QStringLiteral("Local data includes repository caches, retained review history, drafts, notes, receipts and preferences. Retained epochs remain until explicitly pruned; maintenance can remove revisions needed for an older comparison. Moving this storage location and global purge are not available in this build."), storage };
		facts->setWordWrap(true);
		storageColumn->addWidget(facts);
		for (auto* action : { EpochSelectors->prune_action(), EpochSelectors->delete_cache_action(), EpochSelectors->cancel_maintenance_action() }) {
			auto* const button = new QToolButton{ storage };
			button->setDefaultAction(action);
			button->setAccessibleName(action->text());
			storageColumn->addWidget(button);
		}
		auto* const privacy = new QLabel{ QStringLiteral("History maintenance above is limited to the selected review or repository cache and asks for confirmation. It does not change GitHub or your working copy. Diagnostic preview and local export are available in the profile controls. Removing the application does not remove its local data folder or system credential entries. Use Accounts & Hosts to sign out of remembered device sessions."), storage };
		privacy->setWordWrap(true);
		storageColumn->addWidget(privacy);
		// [[BPR-1602]]. The last group on the page, below diagnostics: four
		// resets of this profile's local data, each confirmed first.
		storageColumn->addSpacing(12);
		ProfileReset = new profile_reset_panel{ storage };
		ProfileReset->set_profile_label(ProfileLabel);
		storageColumn->addWidget(ProfileReset);
		SettingsWindow->adopt_panel(settings_page::storage_and_privacy, storage);
	}

	void workbench_window::set_profile_data_reset_dispatch(profile_reset_panel::dispatch aDispatch)
	{
		ProfileReset->set_dispatch(std::move(aDispatch));
	}

	void workbench_window::present_profile_data_reset(const core::profile_data_reset_outcome& aOutcome, const QString& aCopyFolder)
	{
		const auto notice = profile_data_reset_notice(aOutcome, aCopyFolder);
		statusBar()->showMessage(notice, 30000);
		ProfileReset->present_status(aOutcome.Reason == core::release_maintenance_reason::none ? QString{ } : notice);
	}

	void workbench_window::refuse_profile_data_reset(const QString& aReason)
	{
		ProfileReset->present_status(aReason);
	}

	auto workbench_window::profile_reset_panel_widget() const noexcept -> profile_reset_panel*
	{
		return ProfileReset;
	}

	void workbench_window::set_storage_location(const QString& aPath)
	{
		StorageLocation->setText(aPath);
		StorageLocation->setCursorPosition(0);
	}

	void workbench_window::build_queue_settings(QWidget* const aReviews, QVBoxLayout* const aColumn)
	{
		auto* const group   = new QGroupBox{ QStringLiteral("Queue defaults"), aReviews };
		auto* const form    = new QFormLayout{ group };
		const auto  sources = std::array{ QueueRoleFilter, QueueObserverFilter, QueueBucketFilter, QueueRepositoryFilter, QueueQuietFilter, QueueSort };
		const auto  labels  = std::array{ QStringLiteral("Role"), QStringLiteral("Observer reason"), QStringLiteral("Attention"), QStringLiteral("Repository"), QStringLiteral("Quiet reviews"), QStringLiteral("Sort") };
		// [[BPR-0783]]. The six controls themselves, in place of the selectors that
		// mirrored them. A mirror had to be kept in step by hand, which is a second
		// carrier for one choice; these are the same widgets Home used, with their
		// object names, their item data and their typed intents unchanged.
		for (auto i = std::size_t{ 0U }; i < sources.size(); ++i) {
			sources[i]->setParent(group);
			form->addRow(labels[i], sources[i]);
		}
		QueueDefaultsReset = new QPushButton{ QStringLiteral("Reset queue defaults"), group };
		form->addRow(QueueDefaultsReset);
		connect(QueueDefaultsReset, &QPushButton::clicked, this, [this] {
			if (!QueueReady || !QueuePreferencesDispatch) {
				return;
			}
			auto defaults          = application::queue_presentation_preferences{ };
			defaults.Notifications = QueueProjection.Preferences.Notifications;
			QueuePreferencesDispatch(application::queue_preferences_intent{ std::move(defaults), application::queue_preference_field::queue_filters });
		});
		aColumn->addWidget(group);
		SettingsQueueStatus = new QLabel{ aReviews };
		SettingsQueueStatus->setObjectName(QStringLiteral("settingsQueueStatus"));
		SettingsQueueStatus->setAccessibleName(QStringLiteral("Queue settings status"));
		SettingsQueueStatus->setWordWrap(true);
		aColumn->addWidget(SettingsQueueStatus);

		auto* const notifications = new QWidget;
		auto* const column        = new QVBoxLayout{ notifications };
		NotificationsEnabled      = new QCheckBox{ QStringLiteral("Enable review notifications"), notifications };
		NotificationsEnabled->setObjectName(QStringLiteral("notificationsEnabled"));
		NotificationsPaused = new QCheckBox{ QStringLiteral("Pause notifications"), notifications };
		NotificationsPaused->setObjectName(QStringLiteral("notificationsPaused"));
		column->addWidget(NotificationsEnabled);
		column->addWidget(NotificationsPaused);
		auto* const explanation = new QLabel{ QStringLiteral("Notifications contain the repository, review number, attention category and unread count. They never include titles, source code or comment text. Choosing a notification selects that review in Home.\n\nYour operating system controls permission and presentation. BetterPullReview cannot confirm every system permission setting."), notifications };
		explanation->setWordWrap(true);
		column->addWidget(explanation);
		NotificationPreview = new QPushButton{ QStringLiteral("Preview for selected Home review"), notifications };
		NotificationPreview->setObjectName(QStringLiteral("notificationPreview"));
		column->addWidget(NotificationPreview);
		NotificationReset = new QPushButton{ QStringLiteral("Reset notification preferences"), notifications };
		column->addWidget(NotificationReset);
		NotificationStatus = new QLabel{ notifications };
		NotificationStatus->setWordWrap(true);
		NotificationStatus->setAccessibleName(QStringLiteral("Notification status"));
		column->addWidget(NotificationStatus);
		const auto change = [this](const application::queue_preference_field aField) {
			if (ApplyingQueuePreferences || !QueueReady || !QueuePreferencesDispatch) {
				return;
			}
			auto next          = QueueProjection.Preferences;
			next.Notifications = { NotificationsEnabled->isChecked(), NotificationsPaused->isChecked() };
			apply_queue_preferences(QueueProjection.Preferences);
			QueuePreferencesDispatch(application::queue_preferences_intent{ std::move(next), aField });
		};
		connect(NotificationsEnabled, &QCheckBox::toggled, this, [change] { change(application::queue_preference_field::notifications_enabled); });
		connect(NotificationsPaused, &QCheckBox::toggled, this, [change] { change(application::queue_preference_field::notifications_paused); });
		connect(NotificationReset, &QPushButton::clicked, this, [this] {
			if (!QueueReady || !QueuePreferencesDispatch) {
				return;
			}
			auto next          = QueueProjection.Preferences;
			next.Notifications = { };
			QueuePreferencesDispatch(application::queue_preferences_intent{ std::move(next), application::queue_preference_field::notifications });
		});
		connect(NotificationPreview, &QPushButton::clicked, this, [this] {
			const auto item = selected_queue_item();
			if (!item) {
				NotificationStatus->setText(QStringLiteral("Select a review in Home first."));
				return;
			}
			const auto submitted = NotificationPreviewDispatch && NotificationPreviewDispatch(item->Ref);
			NotificationStatus->setText(submitted ? QStringLiteral("Preview submitted to the operating system.") : QStringLiteral("Preview was not submitted. Notifications may be paused, permission unavailable, or this review may have no new activity."));
		});
		SettingsWindow->adopt_panel(settings_page::notifications, notifications);
		update_notification_controls();
	}

	void workbench_window::set_notification_preview_dispatch(std::function<bool(const core::pull_request_ref&)> aDispatch)
	{
		NotificationPreviewDispatch = std::move(aDispatch);
		update_notification_controls();
	}

	void workbench_window::update_notification_controls()
	{
		if (NotificationsEnabled == nullptr) {
			return;
		}
		const auto writable = QueueReady && static_cast<bool>(QueuePreferencesDispatch);
		NotificationsEnabled->setEnabled(writable);
		NotificationsPaused->setEnabled(writable);
		NotificationReset->setEnabled(writable);
		QueueDefaultsReset->setEnabled(writable);
		NotificationPreview->setEnabled(writable && static_cast<bool>(NotificationPreviewDispatch)
										&& QueueProjection.Preferences.Notifications.Enabled && !QueueProjection.Preferences.Notifications.Paused);
		NotificationStatus->setText(!writable                         ? QStringLiteral("Preferences are unavailable or could not be saved; the last accepted choices remain in effect.")
										: NotificationPreviewDispatch ? QStringLiteral("Notification delivery is available. System permission may still prevent presentation.")
																	  : QStringLiteral("Notification delivery is unavailable in this environment."));
	}

	void workbench_window::set_queue_preferences_dispatch(
		workbench_queue_preferences_dispatch aDispatch)
	{
		QueuePreferencesDispatch = std::move(aDispatch);
		const auto enabled       = QueueReady
			&& static_cast<bool>(QueuePreferencesDispatch);
		for (auto* const selector : { QueueRoleFilter,
									  QueueObserverFilter,
									  QueueBucketFilter,
									  QueueRepositoryFilter,
									  QueueQuietFilter,
									  QueueSort }) {
			selector->setEnabled(enabled);
		}
		apply_queue_preferences(QueueProjection.Preferences);
	}

	void workbench_window::dispatch_queue_preferences(QComboBox* const aSource)
	{
		if (ApplyingQueuePreferences || !QueueReady
			|| !QueuePreferencesDispatch) {
			return;
		}
		const auto optionalRole = [this]() -> std::optional<core::github_role> {
			const auto value = QueueRoleFilter->currentData().toInt();
			return value < 0
				? std::nullopt
				: std::optional{ static_cast<core::github_role>(value) };
		};
		const auto optionalObserver = [this]()
			-> std::optional<core::observer_rule_kind> {
			const auto value = QueueObserverFilter->currentData().toInt();
			return value < 0
				? std::nullopt
				: std::optional{ static_cast<core::observer_rule_kind>(value) };
		};
		const auto optionalBucket = [this]()
			-> std::optional<core::attention_bucket> {
			const auto value = QueueBucketFilter->currentData().toInt();
			return value < 0
				? std::nullopt
				: std::optional{ static_cast<core::attention_bucket>(value) };
		};
		auto preferences = application::queue_presentation_preferences{
			.Role           = optionalRole(),
			.ObserverReason = optionalObserver(),
			.Bucket         = optionalBucket(),
			.Repository     = QueueRepositoryFilter->currentData().toString().toStdString(),
			.Quiet          = static_cast<application::queue_quiet_filter>(
				QueueQuietFilter->currentData().toInt()),
			.Sort = static_cast<application::queue_sort_order>(
				QueueSort->currentData().toInt()),
		};
		if (!application::valid(preferences)) {
			return;
		}
		const auto field          = aSource == QueueRoleFilter ? application::queue_preference_field::role
			: aSource == QueueObserverFilter                   ? application::queue_preference_field::observer
			: aSource == QueueBucketFilter                     ? application::queue_preference_field::bucket
			: aSource == QueueRepositoryFilter                 ? application::queue_preference_field::repository
			: aSource == QueueQuietFilter                      ? application::queue_preference_field::quiet
															   : application::queue_preference_field::sort;
		preferences.Notifications = QueueProjection.Preferences.Notifications;
		apply_queue_preferences(QueueProjection.Preferences);
		QueuePreferencesDispatch(application::queue_preferences_intent{
			.Preferences = std::move(preferences),
			.Field       = field,
		});
	}

	void workbench_window::apply_queue_preferences(
		const application::queue_presentation_preferences& aPreferences)
	{
		ApplyingQueuePreferences = true;
		const auto select        = [](QComboBox* const aSelector, const int aValue) {
			const auto index = aSelector->findData(aValue);
			aSelector->setCurrentIndex(index >= 0 ? index : 0);
		};
		select(
			QueueRoleFilter,
			aPreferences.Role.has_value()
				? static_cast<int>(*aPreferences.Role)
				: -1);
		select(
			QueueObserverFilter,
			aPreferences.ObserverReason.has_value()
				? static_cast<int>(*aPreferences.ObserverReason)
				: -1);
		select(
			QueueBucketFilter,
			aPreferences.Bucket.has_value()
				? static_cast<int>(*aPreferences.Bucket)
				: -1);
		const auto repository      = QString::fromStdString(aPreferences.Repository);
		const auto repositoryIndex = QueueRepositoryFilter->findData(repository);
		QueueRepositoryFilter->setCurrentIndex(
			repositoryIndex >= 0 ? repositoryIndex : 0);
		select(QueueQuietFilter, static_cast<int>(aPreferences.Quiet));
		select(QueueSort, static_cast<int>(aPreferences.Sort));
		QueueFilterModel->apply(aPreferences);
		// [[BPR-0783]]. Home filters by what its own view says: the window's four
		// values once it owns them, otherwise the profile's, and never Repository
		// or Bucket. The controls above keep showing the profile's answer.
		QueueFilterModel->set_home_view(HomeView);
		if (!HomeView.FilterOwned) {
			// [[BPR-0783]] review finding. Until its first Filter gesture the
			// window follows the profile, so an accepted preference changes what
			// Home is filtered by without any view of its own changing. The Filter
			// control is re-read from it, exactly as the six combos above are.
			apply_home_filter_controls();
		}
		if (NotificationsEnabled != nullptr) {
			NotificationsEnabled->setChecked(aPreferences.Notifications.Enabled);
			NotificationsPaused->setChecked(aPreferences.Notifications.Paused);
		}
		ApplyingQueuePreferences = false;
		update_notification_controls();
	}

	void workbench_window::build_home_strip_controls()
	{
		// [[BPR-0783]]. RFC-BPR-0032's amendment to RFC-BPR-0028's strip: while
		// Home is shown and an account can fill its list, Home's own control and
		// the refresh glyph stand before the right divider, the account and
		// settings. Signed out they act on nothing, so they are absent rather
		// than disabled, and the review strip is unchanged.
		//
		// [[BPR-1580]]. One View menu in place of Group, State headers and
		// Filter (the owner, 2026-10-17: "I wonder if we should collapse these 3
		// into one"). Filter's four groups were section titles, which the native
		// macOS menu draws as bare separators, so its choices arrived
		// unlabelled. Here every group is a submenu whose own item carries the
		// group's name and current value: an ordinary item's text, which every
		// platform's menu draws. No section title is used anywhere in it.
		HomeViewMenu = new QMenu{ this };
		HomeViewMenu->setObjectName(QStringLiteral("homeViewMenu"));
		HomeViewMenu->setAccessibleName(QStringLiteral("View options for Home's pull requests"));
		HomeViewMenu->setAccessibleDescription(QStringLiteral(
			"Group by, Sort by, State headers, My role, Added by, Quiet items and Reset to defaults; nothing here changes GitHub"));
		HomeViewMenu->setToolTipsVisible(true);

		auto* const group = add_home_view_submenu(
			home_view_choice::group, QStringLiteral("homeGroupMenu"),
			QStringLiteral("How Home gathers its rows into panels. The state order stays inside every grouping."));
		auto* const groupings = new QActionGroup{ group };
		groupings->setExclusive(true);
		for (const auto& [value, title] : std::array{
				 std::pair{ application::home_grouping::none, QStringLiteral("None") },
				 std::pair{ application::home_grouping::organization, QStringLiteral("Organization") },
				 std::pair{ application::home_grouping::repository, QStringLiteral("Repository") },
				 std::pair{ application::home_grouping::account, QStringLiteral("Account") } }) {
			auto* const action = group->addAction(title);
			action->setObjectName(QStringLiteral("homeGroupBy%1Action").arg(title));
			action->setCheckable(true);
			action->setData(static_cast<int>(value));
			groupings->addAction(action);
			const auto purpose = value == application::home_grouping::none
				? QStringLiteral("One list in state order, with no grouping.")
				: QStringLiteral("One panel per %1, with the state order kept inside it.").arg(title.toLower());
			action->setToolTip(purpose);
			action->setStatusTip(purpose);
			HomeGroupActions[static_cast<std::size_t>(value)] = action;
			connect(action, &QAction::triggered, this, [this, value] {
				if (ApplyingHomeView) {
					return;
				}
				HomeView.Grouping = value;
				apply_home_view();
				dispatch_home_view();
			});
		}

		// Sort sits with Group by: both arrange the rows, neither hides any.
		static_cast<void>(add_home_view_submenu(
			home_view_choice::sort, QStringLiteral("homeSortMenu"),
			QStringLiteral("The order of the rows inside each state. The states themselves keep their order.")));

		HomeStateHeaders = HomeViewMenu->addAction(QStringLiteral("State headers"));
		HomeStateHeaders->setObjectName(QStringLiteral("homeStateHeadersAction"));
		HomeStateHeaders->setCheckable(true);
		HomeStateHeaders->setChecked(true);
		HomeStateHeaders->setToolTip(QStringLiteral(
			"Show each state as its own panel. With it off the rows sit in state order with a state mark."));
		HomeStateHeaders->setStatusTip(HomeStateHeaders->toolTip());
		connect(HomeStateHeaders, &QAction::toggled, this, [this](const bool aChecked) {
			if (ApplyingHomeView) {
				return;
			}
			HomeView.StateHeaders = aChecked;
			apply_home_view();
			dispatch_home_view();
		});

		HomeViewMenu->addSeparator();
		static_cast<void>(add_home_view_submenu(
			home_view_choice::role, QStringLiteral("homeRoleMenu"),
			QStringLiteral("Show pull requests by my part in them on GitHub.")));
		static_cast<void>(add_home_view_submenu(
			home_view_choice::added_by, QStringLiteral("homeAddedByMenu"),
			QStringLiteral("Show pull requests by the observer rule that added them. Observer rules are mine, kept in Settings › Home & queue, and never change GitHub.")));
		static_cast<void>(add_home_view_submenu(
			home_view_choice::quiet, QStringLiteral("homeQuietMenu"),
			QStringLiteral("A quiet pull request has nothing new for me and nothing waiting on anyone.")));
		build_home_filter_menus();

		HomeViewMenu->addSeparator();
		HomeViewReset = HomeViewMenu->addAction(QStringLiteral("Reset to defaults"));
		HomeViewReset->setObjectName(QStringLiteral("homeViewResetAction"));
		HomeViewReset->setToolTip(QStringLiteral(
			"Group by None, Sort by Priority, State headers on, any role, added any way and quiet items shown."));
		HomeViewReset->setStatusTip(HomeViewReset->toolTip());
		connect(HomeViewReset, &QAction::triggered, this, [this] { restore_home_view_defaults(true); });

		HomeViewMenu->addSeparator();
		// [[BPR-1580]]. Where observer rules are kept is Added by's own first
		// line now, so the footer says only what holds for the whole menu.
		auto* const footer = HomeViewMenu->addAction(QStringLiteral("Nothing here changes GitHub."));
		footer->setObjectName(QStringLiteral("homeFilterFooter"));
		footer->setEnabled(false);
		// The footer says where they are; this is the way there, by keyboard as
		// well as by pointer.
		// The name carries an ampersand, which a menu would otherwise eat as a
		// mnemonic (`menu_literal`).
		HomeFilterSettings = HomeViewMenu->addAction(menu_literal(QStringLiteral("Open Home & queue")));
		HomeFilterSettings->setObjectName(QStringLiteral("homeFilterSettingsAction"));
		HomeFilterSettings->setToolTip(QStringLiteral(
			"Opens Settings, Home & queue, where observer rules and queue defaults are kept."));
		HomeFilterSettings->setStatusTip(HomeFilterSettings->toolTip());
		connect(HomeFilterSettings, &QAction::triggered, this, [this] {
			open_settings(settings_page::home_and_queue);
		});

		HomeViewButton = new menu_tool_button{ this };
		HomeViewButton->setObjectName(QStringLiteral("homeViewButton"));
		HomeViewButton->setText(QStringLiteral("View"));
		HomeViewButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
		HomeViewButton->setPopupMode(QToolButton::InstantPopup);
		HomeViewButton->setMenu(HomeViewMenu);
		HomeViewButton->setAccessibleName(QStringLiteral("View"));
		Strip->adopt_button(HomeViewButton);
		HomeViewItem = PrimaryToolbar->insertWidget(GithubRefreshAction, HomeViewButton);
		HomeViewItem->setObjectName(QStringLiteral("homeViewAction"));
		apply_home_view();
	}

	auto workbench_window::add_home_view_submenu(const home_view_choice aChoice, const QString& aName, const QString& aPurpose) -> QMenu*
	{
		auto* const menu = new QMenu{ HomeViewMenu };
		menu->setObjectName(aName);
		menu->setToolTipsVisible(true);
		menu->setAccessibleDescription(aPurpose);
		menu->menuAction()->setObjectName(aName + QStringLiteral("Action"));
		menu->menuAction()->setToolTip(aPurpose);
		menu->menuAction()->setStatusTip(aPurpose);
		HomeViewMenu->addMenu(menu);
		HomeViewSubmenus[static_cast<std::size_t>(aChoice)] = menu;
		return menu;
	}

	void workbench_window::build_home_filter_menus()
	{
		// [[BPR-0783]] Decisions (c). Exactly the four typed preferences that
		// survive as Home filters, each listing every value of its vocabulary.
		// Repository and Bucket are not here: Group by and the state order do
		// their job.
		//
		// [[BPR-0783]] review finding. Each is the one exclusive choice the
		// board draws, not independent check boxes: an item cannot be unticked
		// into nothing, and choosing one unticks its siblings.
		//
		// [[BPR-1580]]. A native macOS menu draws no tooltip, so a choice whose
		// meaning is not obvious says it in its own text; the submenu's own item
		// shows the short name, and the tooltip and status tip say it again
		// where the platform draws them.
		struct choice
		{
			QString Text;
			QString Short;
			QString Purpose;
		};
		auto sections = std::array<QActionGroup*, 4U>{ };
		for (auto& section : sections) {
			section = new QActionGroup{ this };
			section->setExclusive(true);
		}
		const auto item = [this, &sections](const std::size_t aSection,
											QMenu&            aMenu,
											const choice&     aChoice,
											const QString&    aName,
											const int         aValue,
											auto              aGesture) {
			auto* const action = aMenu.addAction(aChoice.Text);
			action->setObjectName(QStringLiteral("homeFilter%1Action").arg(aName));
			action->setCheckable(true);
			action->setData(aValue);
			action->setToolTip(aChoice.Purpose);
			action->setStatusTip(aChoice.Purpose);
			sections[aSection]->addAction(action);
			HomeFilterSections[aSection].push_back(action);
			HomeFilterShortNames[action] = aChoice.Short;
			connect(action, &QAction::triggered, this, [this, aGesture] {
				if (ApplyingHomeView) {
					return;
				}
				aGesture();
			});
		};
		const auto named = [](const QString& aShort) { return QString{ aShort }.remove(QLatin1Char{ ' ' }); };

		auto& role = *HomeViewSubmenus[static_cast<std::size_t>(home_view_choice::role)];
		for (const auto& [value, words] : std::array{
				 std::pair{ std::optional<core::github_role>{ },
							choice{ QStringLiteral("Any role"), QStringLiteral("Any"), QStringLiteral("Every pull request, whatever my part in it.") } },
				 std::pair{ std::optional{ core::github_role::author },
							choice{ QStringLiteral("I opened it"), QStringLiteral("Author"), QStringLiteral("Only pull requests I opened.") } },
				 std::pair{ std::optional{ core::github_role::reviewer },
							choice{ QStringLiteral("I reviewed it"), QStringLiteral("Reviewer"), QStringLiteral("Only pull requests I have already reviewed.") } },
				 std::pair{ std::optional{ core::github_role::requested_reviewer },
							choice{ QStringLiteral("My review is requested"), QStringLiteral("Requested"),
									QStringLiteral("Only pull requests that ask for my review and do not have it yet.") } },
				 std::pair{ std::optional{ core::github_role::participant },
							choice{ QStringLiteral("I took part in it"), QStringLiteral("Participant"),
									QStringLiteral("Only pull requests I took part in on GitHub without being their author or a reviewer.") } },
				 std::pair{ std::optional{ core::github_role::none },
							choice{ QStringLiteral("None of these (a rule or a follow added it)"), QStringLiteral("No role"),
									QStringLiteral("Only pull requests I have no part in on GitHub, which an observer rule or a follow brought to Home.") } } }) {
			item(0U, role, words, named(words.Short), value.has_value() ? static_cast<int>(*value) : -1, [this, value] {
				auto filter = home_filter();
				filter.Role = value;
				choose_home_filter(application::queue_preference_field::role, filter);
			});
		}

		auto& addedBy = *HomeViewSubmenus[static_cast<std::size_t>(home_view_choice::added_by)];
		// What an observer rule is, and where it is kept, as the first line.
		auto* const rules = addedBy.addAction(menu_literal(QStringLiteral("Observer rules are mine, kept in Settings › Home & queue")));
		rules->setObjectName(QStringLiteral("homeAddedByMeaning"));
		rules->setEnabled(false);
		for (const auto& [value, words] : std::array{
				 std::pair{ std::optional<core::observer_rule_kind>{ },
							choice{ QStringLiteral("Any way"), QStringLiteral("Any"), QStringLiteral("Every pull request, whether or not an observer rule added it.") } },
				 std::pair{ std::optional{ core::observer_rule_kind::repository },
							choice{ QStringLiteral("A rule on a repository"), QStringLiteral("Repository rule"),
									QStringLiteral("Only pull requests an observer rule watching a whole repository added.") } },
				 std::pair{ std::optional{ core::observer_rule_kind::author },
							choice{ QStringLiteral("A rule on an author"), QStringLiteral("Author rule"),
									QStringLiteral("Only pull requests an observer rule watching one author's pull requests added.") } },
				 std::pair{ std::optional{ core::observer_rule_kind::path },
							choice{ QStringLiteral("A rule on a path"), QStringLiteral("Path rule"),
									QStringLiteral("Only pull requests an observer rule watching files under a path added.") } } }) {
			// Any is a word My role uses too, so this one is named for its group.
			const auto name = value.has_value() ? named(words.Short) : QStringLiteral("AddedByAny");
			item(1U, addedBy, words, name, value.has_value() ? static_cast<int>(*value) : -1, [this, value] {
				auto filter    = home_filter();
				filter.AddedBy = value;
				choose_home_filter(application::queue_preference_field::observer, filter);
			});
		}

		auto& quiet = *HomeViewSubmenus[static_cast<std::size_t>(home_view_choice::quiet)];
		// What quiet means, as the submenu's first line: the one word here a
		// reviewer cannot be expected to know.
		auto* const meaning = quiet.addAction(QStringLiteral("Quiet: nothing new for me and nothing waiting on anyone"));
		meaning->setObjectName(QStringLiteral("homeQuietMeaning"));
		meaning->setEnabled(false);
		for (const auto& [value, words] : std::array{
				 std::pair{ application::queue_quiet_filter::all,
							choice{ QStringLiteral("Show"), QStringLiteral("Show"), QStringLiteral("Quiet pull requests are listed with the rest.") } },
				 std::pair{ application::queue_quiet_filter::attention,
							choice{ QStringLiteral("Hide"), QStringLiteral("Hide"),
									QStringLiteral("Quiet pull requests are hidden. One a failed check or merge block holds up still shows.") } },
				 std::pair{ application::queue_quiet_filter::quiet,
							choice{ QStringLiteral("Only quiet"), QStringLiteral("Only quiet"), QStringLiteral("Only quiet pull requests are listed.") } } }) {
			item(2U, quiet, words, named(words.Short), static_cast<int>(value), [this, value] {
				auto filter  = home_filter();
				filter.Quiet = value;
				choose_home_filter(application::queue_preference_field::quiet, filter);
			});
		}

		auto& sort = *HomeViewSubmenus[static_cast<std::size_t>(home_view_choice::sort)];
		for (const auto& [value, words] : std::array{
				 std::pair{ application::queue_sort_order::core_priority,
							choice{ QStringLiteral("Priority"), QStringLiteral("Priority"), QStringLiteral("Home's own order: what most needs me comes first.") } },
				 std::pair{ application::queue_sort_order::repository,
							choice{ QStringLiteral("Repository"), QStringLiteral("Repository"), QStringLiteral("By repository name, then pull request number.") } },
				 std::pair{ application::queue_sort_order::pull_request_number,
							choice{ QStringLiteral("Pull request number"), QStringLiteral("Pull request number"),
									QStringLiteral("By pull request number, then repository.") } } }) {
			item(3U, sort, words, named(words.Short), static_cast<int>(value), [this, value] {
				auto filter = home_filter();
				filter.Sort = value;
				choose_home_filter(application::queue_preference_field::sort, filter);
			});
		}
	}

	auto workbench_window::home_filter() const -> application::home_filter
	{
		return HomeView.FilterOwned
			? HomeView.Filter
			: application::home_filter_of(QueueProjection.Preferences);
	}

	void workbench_window::choose_home_filter(
		const application::queue_preference_field aField, const application::home_filter& aFilter)
	{
		if (!QueueReady || !QueuePreferencesDispatch) {
			// Nothing to filter, and nowhere for the choice to be kept. The menu
			// goes back to showing what is in force.
			apply_home_view();
			return;
		}
		// [[BPR-0783]] Decisions (e). One gesture, two destinations. The profile's
		// store hears the same typed intent the six combos sent, so the last
		// choice is what a new window starts with; and from this gesture on the
		// window keeps its own four values, so two windows of one profile filter
		// independently.
		auto next           = QueueProjection.Preferences;
		next.Role           = aFilter.Role;
		next.ObserverReason = aFilter.AddedBy;
		next.Quiet          = aFilter.Quiet;
		next.Sort           = aFilter.Sort;
		QueuePreferencesDispatch(application::queue_preferences_intent{
			.Preferences = std::move(next),
			.Field       = aField,
		});
		HomeView.FilterOwned = true;
		HomeView.Filter      = aFilter;
		apply_home_view();
		dispatch_home_view();
	}

	void workbench_window::restore_home_view_defaults(const bool aWholeView)
	{
		if (ApplyingHomeView) {
			return;
		}
		const auto before  = HomeView;
		const auto current = home_filter();
		auto       target  = current;
		target.Role.reset();
		target.AddedBy.reset();
		target.Quiet = application::queue_quiet_filter::all;
		if (aWholeView) {
			target.Sort           = application::queue_sort_order::core_priority;
			HomeView.Grouping     = application::home_grouping::none;
			HomeView.StateHeaders = true;
		}
		if (target != current && QueueReady && QueuePreferencesDispatch) {
			// The profile hears this as it hears a filter choice: one typed
			// intent per field that changes, each naming only its own field, so
			// the store merges them exactly as it merges single choices.
			auto next           = QueueProjection.Preferences;
			next.Role           = target.Role;
			next.ObserverReason = target.AddedBy;
			next.Quiet          = target.Quiet;
			next.Sort           = target.Sort;
			const auto send     = [this, &next](const application::queue_preference_field aField) {
				QueuePreferencesDispatch(application::queue_preferences_intent{ .Preferences = next, .Field = aField });
			};
			if (current.Role != target.Role) {
				send(application::queue_preference_field::role);
			}
			if (current.AddedBy != target.AddedBy) {
				send(application::queue_preference_field::observer);
			}
			if (current.Quiet != target.Quiet) {
				send(application::queue_preference_field::quiet);
			}
			if (current.Sort != target.Sort) {
				send(application::queue_preference_field::sort);
			}
			HomeView.FilterOwned = true;
			HomeView.Filter      = target;
		}
		if (HomeView == before) {
			return;
		}
		apply_home_view();
		dispatch_home_view();
	}

	void workbench_window::apply_home_view()
	{
		ApplyingHomeView = true;
		for (auto index = std::size_t{ 0U }; index < HomeGroupActions.size(); ++index) {
			if (HomeGroupActions[index] != nullptr) {
				HomeGroupActions[index]->setChecked(index == static_cast<std::size_t>(HomeView.Grouping));
			}
		}
		if (HomeStateHeaders != nullptr) {
			HomeStateHeaders->setChecked(HomeView.StateHeaders);
		}
		apply_home_filter_controls();
		if (HomeList != nullptr) {
			HomeList->set_grouping(HomeView.Grouping);
			HomeList->set_state_headers(HomeView.StateHeaders);
			HomeList->set_collapsed_panels(HomeView.Collapsed);
		}
		if (QueueFilterModel != nullptr) {
			QueueFilterModel->set_home_view(HomeView);
			if (HomeList != nullptr) {
				HomeList->refresh();
			}
		}
		ApplyingHomeView = false;
		if (QueueReady) {
			// The queue's own sentence explains the rows Home is hiding, which
			// the view may just have changed.
			update_queue_status(QueueProjection);
		}
		apply_focus_order();
	}

	void workbench_window::apply_home_filter_controls()
	{
		// Adopting what is in force is not a gesture, so nothing here reports back
		// out; a menu's own check change must not read as the reviewer's choice.
		const auto applying = ApplyingHomeView;
		ApplyingHomeView    = true;
		const auto filter   = home_filter();
		const auto shown    = [this](const std::size_t aSection, const int aValue) {
			auto chosen = QString{ };
			for (auto* const action : HomeFilterSections[aSection]) {
				action->setChecked(action->data().toInt() == aValue);
				if (action->isChecked()) {
					chosen = HomeFilterShortNames[action];
				}
			}
			return chosen;
		};
		const auto        role    = shown(0U, filter.Role.has_value() ? static_cast<int>(*filter.Role) : -1);
		const auto        addedBy = shown(1U, filter.AddedBy.has_value() ? static_cast<int>(*filter.AddedBy) : -1);
		const auto        quiet   = shown(2U, static_cast<int>(filter.Quiet));
		const auto        sort    = shown(3U, static_cast<int>(filter.Sort));
		const auto* const grouped = HomeGroupActions[static_cast<std::size_t>(HomeView.Grouping)];
		const auto        group   = grouped != nullptr ? grouped->text() : QStringLiteral("None");

		// [[BPR-1580]]. Each submenu's own item names its group and what is in
		// force, so the menu reads as a summary of the view before it is opened.
		const auto title = [this](const home_view_choice aChoice, const QString& aGroup, const QString& aValue) {
			if (auto* const menu = HomeViewSubmenus[static_cast<std::size_t>(aChoice)]; menu != nullptr) {
				menu->setTitle(QStringLiteral("%1: %2").arg(aGroup, aValue));
				menu->setAccessibleName(aGroup);
			}
		};
		title(home_view_choice::group, QStringLiteral("Group by"), group);
		title(home_view_choice::sort, QStringLiteral("Sort by"), sort);
		title(home_view_choice::role, QStringLiteral("My role"), role);
		title(home_view_choice::added_by, QStringLiteral("Added by"), addedBy);
		title(home_view_choice::quiet, QStringLiteral("Quiet items"), quiet);
		if (auto* const menu = HomeViewSubmenus[static_cast<std::size_t>(home_view_choice::added_by)]; menu != nullptr) {
			// With no observer rule, nothing was added by one, so the choice is
			// offered only once there is a rule, or while this window is
			// already filtered by one, which must stay within reach to undo.
			menu->menuAction()->setVisible(!QueueRules.empty() || filter.AddedBy.has_value());
		}

		// What differs from the default, said in the button's tooltip.
		auto changed = QStringList{ };
		if (HomeView.Grouping != application::home_grouping::none) {
			changed.push_back(QStringLiteral("Group by %1").arg(group));
		}
		if (filter.Sort != application::queue_sort_order::core_priority) {
			changed.push_back(QStringLiteral("Sort by %1").arg(sort));
		}
		if (!HomeView.StateHeaders) {
			changed.push_back(QStringLiteral("State headers off"));
		}
		if (filter.Role.has_value()) {
			changed.push_back(QStringLiteral("My role %1").arg(role));
		}
		if (filter.AddedBy.has_value()) {
			changed.push_back(QStringLiteral("Added by %1").arg(addedBy));
		}
		if (filter.Quiet != application::queue_quiet_filter::all) {
			changed.push_back(QStringLiteral("Quiet items %1").arg(quiet));
		}
		if (HomeViewReset != nullptr) {
			HomeViewReset->setEnabled(!changed.isEmpty());
		}
		if (HomeViewButton != nullptr) {
			// [[BPR-1580]] owner's note on v001: "I'm not sure why View carries a
			// count". The button stays "View" whatever is chosen; what differs is
			// said in its tooltip and description, the menu's own items name
			// every value, and a view that hides every row says so in the list.
			const auto purpose = changed.isEmpty()
				? QStringLiteral("Arrange and filter Home's pull requests. The default view: nothing is grouped or hidden.")
				: QStringLiteral("Arrange and filter Home's pull requests. %1 changed: %2.")
					  .arg(QString::number(changed.size()), changed.join(QStringLiteral("; ")));
			HomeViewButton->setAccessibleDescription(purpose);
			HomeViewButton->setToolTip(purpose);
		}
		ApplyingHomeView = applying;
	}

	void workbench_window::dispatch_home_view()
	{
		if (ApplyingHomeView || !HomeViewDispatch || !application::valid(HomeView)) {
			return;
		}
		HomeViewDispatch(HomeView);
	}

	void workbench_window::adopt_home_view(const application::home_view_state& aView)
	{
		if (!application::valid(aView) || aView == HomeView) {
			// Equal to what Home already shows, so nothing is re-laid out and
			// focus, selection and scroll stay where the reviewer left them. A
			// view the host refused arrives the same way and puts Home back.
			return;
		}
		HomeView = aView;
		apply_home_view();
	}

	void workbench_window::set_home_view_dispatch(std::function<void(application::home_view_state)> aDispatch)
	{
		HomeViewDispatch = std::move(aDispatch);
	}

	auto workbench_window::home_view() const noexcept -> const application::home_view_state&
	{
		return HomeView;
	}

	auto workbench_window::home_view_button() const noexcept -> QToolButton*
	{
		return HomeViewButton;
	}

	auto workbench_window::home_view_menu() const noexcept -> QMenu*
	{
		return HomeViewMenu;
	}

	auto workbench_window::home_view_submenu(const home_view_choice aChoice) const noexcept -> QMenu*
	{
		return HomeViewSubmenus[static_cast<std::size_t>(aChoice)];
	}

	auto workbench_window::home_view_reset_action() const noexcept -> QAction*
	{
		return HomeViewReset;
	}

	auto workbench_window::home_group_action(const application::home_grouping aGrouping) const noexcept -> QAction*
	{
		return HomeGroupActions[static_cast<std::size_t>(aGrouping)];
	}

	auto workbench_window::home_state_headers_action() const noexcept -> QAction*
	{
		return HomeStateHeaders;
	}

	auto workbench_window::home_filter_settings_action() const noexcept -> QAction*
	{
		return HomeFilterSettings;
	}

	void workbench_window::rebuild_queue_repositories(
		const std::span<const core::queue_item> aItems,
		const std::string&                      aSelectedRepository)
	{
		auto repositories = std::vector<std::string>{ };
		repositories.reserve(aItems.size());
		for (const auto& item : aItems) {
			repositories.push_back(item.Ref.Repository);
		}
		std::ranges::sort(repositories);
		const auto unique = std::ranges::unique(repositories);
		repositories.erase(unique.begin(), unique.end());
		if (!aSelectedRepository.empty()
			&& !std::ranges::contains(repositories, aSelectedRepository)) {
			repositories.push_back(aSelectedRepository);
			std::ranges::sort(repositories);
		}
		ApplyingQueuePreferences = true;
		QueueRepositoryFilter->clear();
		QueueRepositoryFilter->addItem(tr("Repository: any"), QString{ });
		for (const auto& repository : repositories) {
			const auto text = QString::fromStdString(repository);
			QueueRepositoryFilter->addItem(
				tr("Repository: %1").arg(text), text);
		}
		ApplyingQueuePreferences = false;
	}

	void workbench_window::update_queue_status(
		const application::queue_landing_projection& aProjection)
	{
		// [[BPR-0776]]. On Home the status sentence speaks only when it says
		// something the panels do not: restoring, unavailable, a limit. A filter
		// that hides every row is the list's own sentence ([[BPR-1580]]). The count is the panels' headers and an
		// empty queue is Needs me's sentence. Settings keeps the whole text.
		auto notable = true;
		switch (aProjection.Status) {
		case application::queue_landing_status::restoring:
			QueueStatus->setText(tr("Restoring local observer rules and queue preferences…"));
			break;
		case application::queue_landing_status::unavailable:
			QueueStatus->setText(tr("Local observer rules and queue preferences are unavailable."));
			break;
		case application::queue_landing_status::ready:
			if (QueueLanding->rowCount() == 0) {
				QueueStatus->setText(tr(
					"Review queue is empty. Local rules never change GitHub assignments."));
				notable = false;
			} else if (QueueFilterModel->rowCount() == 0
					   && QueueFilterModel->has_active_filters()) {
				QueueStatus->setText(tr(
										 "No reviews match the active queue filters. %1 review(s) remain in the queue.")
										 .arg(QueueLanding->rowCount()));
				// [[BPR-1580]]. Home's list says this itself, with the way out.
				notable = false;
			} else {
				QueueStatus->setText(
					tr("%1 of %2 review(s) shown. %3")
						.arg(QueueFilterModel->rowCount())
						.arg(QueueLanding->rowCount())
						.arg(aProjection.Online
								 ? tr("Refresh is online.")
								 : tr("Refresh is offline.")));
				notable = false;
			}
			break;
		}
		if (aProjection.Failure == application::queue_store_failure::collection_limit) {
			QueueStatus->setText(aProjection.Status == application::queue_landing_status::ready
									 ? tr("The local queue configuration limit was reached. Existing settings are unchanged; remove an entry before adding another.")
									 : tr("Stored queue configuration exceeds this build's supported limits. It was not loaded or changed."));
			notable = true;
		}
		if (SettingsQueueStatus != nullptr) {
			SettingsQueueStatus->setText(QueueStatus->text());
		}
		QueueStatusNotable = notable;
		update_home_surface();
		// [[BPR-1357]]. A queue row that appeared or went may carry the opening.
		if (HomeOpen.has_value()) {
			refresh_home_open();
		}
	}

	void workbench_window::dispatch_queue_rule(
		const application::queue_rule_action aAction)
	{
		if (!QueueRuleDispatch || !QueueReady) {
			return;
		}
		auto rule = core::observer_rule{ };
		if (aAction == application::queue_rule_action::add) {
			rule = {
				.Kind = static_cast<core::observer_rule_kind>(
					QueueRuleKind->currentData().toInt()),
				.Pattern   = QueueRulePattern->text().trimmed().toStdString(),
				.Excluding = QueueRuleExcluding->isChecked(),
			};
		} else {
			const auto index = QueueRuleSelector->currentIndex();
			if (index < 0 || static_cast<std::size_t>(index) >= QueueRules.size()) {
				return;
			}
			rule = QueueRules[static_cast<std::size_t>(index)];
		}
		if (!core::valid(rule)) {
			return;
		}
		++Statistics.QueueRuleIntents;
		QueueRuleDispatch(application::queue_rule_intent{
			.Action = aAction,
			.Rule   = std::move(rule),
		});
	}

	void workbench_window::update_queue_rule_actions()
	{
		if (AddQueueRule == nullptr || RemoveQueueRule == nullptr) {
			return;
		}
		const auto pattern = QueueRulePattern != nullptr
			? QueueRulePattern->text().trimmed().toStdString()
			: std::string{ };
		AddQueueRule->setEnabled(
			QueueReady && static_cast<bool>(QueueRuleDispatch)
			&& core::valid(core::observer_rule{ .Pattern = pattern }));
		RemoveQueueRule->setEnabled(
			QueueReady && static_cast<bool>(QueueRuleDispatch)
			&& QueueRuleSelector != nullptr
			&& QueueRuleSelector->currentIndex() >= 0);
		AddQueueRule->setToolTip(QStringLiteral(
			"Persist a local observer rule; this never changes GitHub."));
		RemoveQueueRule->setToolTip(QStringLiteral(
			"Delete the selected local observer rule."));
	}

	void workbench_window::publish(
		const application::queue_landing_projection& aProjection)
	{
		auto selectedReview = std::optional<core::pull_request_ref>{ };
		if (const auto item = selected_queue_item(); item.has_value()) {
			selectedReview = item->Ref;
		}
		auto       selectedRule  = std::optional<core::observer_rule>{ };
		const auto selectedIndex = QueueRuleSelector->currentIndex();
		if (selectedIndex >= 0
			&& static_cast<std::size_t>(selectedIndex) < QueueRules.size()) {
			selectedRule = QueueRules[static_cast<std::size_t>(selectedIndex)];
		}
		const auto rulesChanged   = QueueRules != aProjection.Rules;
		const auto serversChanged = QueueProjection.SavedServers != aProjection.SavedServers;
		const auto discovered     = QueueProjection.Discovery;

		QueueReady                = aProjection.Status == application::queue_landing_status::ready;
		const auto filtersEnabled = QueueReady
			&& static_cast<bool>(QueuePreferencesDispatch);
		for (auto* const selector : { QueueRoleFilter,
									  QueueObserverFilter,
									  QueueBucketFilter,
									  QueueRepositoryFilter,
									  QueueQuietFilter,
									  QueueSort }) {
			selector->setEnabled(filtersEnabled);
		}
		// The models retain item identity. Keep only the queue's metadata here.
		QueueProjection = application::queue_landing_projection{
			.Status = aProjection.Status, .Rules = aProjection.Rules, .Preferences = aProjection.Preferences, .Session = aProjection.Session, .Online = aProjection.Online, .BackoffSeconds = aProjection.BackoffSeconds, .SavedRepositories = aProjection.SavedRepositories, .SavedServers = aProjection.SavedServers, .Failure = aProjection.Failure, .Discovery = aProjection.Discovery, .Addition = aProjection.Addition
		};
		QueueRules = aProjection.Rules;
		// [[BPR-1580]]. Whether View offers Added by follows the rules.
		apply_home_filter_controls();
		// [[BPR-1407]]. Ages are measured to the host's clock as the queue arrives.
		if (HomeClock) {
			QueueLanding->set_now(HomeClock());
		}
		if (aProjection.UpdatedItem) {
			QueueLanding->apply_item(*aProjection.UpdatedItem);
		} else {
			QueueLanding->apply(aProjection.Items);
		}
		if (RepositoriesTree != nullptr) {
			if (aProjection.UpdatedItem) {
				RepositoriesTree->apply_item(*aProjection.UpdatedItem);
			} else {
				RepositoriesTree->apply(aProjection.Items, aProjection.SavedRepositories);
			}
			update_rail_summaries();
			refresh_filter_notices();
		}
		// [[BPR-0660]], [[BPR-0774]]. A remembered server is its host's card,
		// remembering exactly the record the queue keeps.
		if (serversChanged) {
			update_server_cards();
		}
		update_saved_repository_list();
		if (!aProjection.UpdatedItem) {
			rebuild_queue_repositories(aProjection.Items, aProjection.Preferences.Repository);
		}
		apply_queue_preferences(aProjection.Preferences);
		if (HomeList != nullptr) {
			// The panels are recounted now rather than after the model's own
			// signals settle, so the row the reviewer had is found again in
			// whichever state holds it. Restoring it neither expands a panel
			// the reviewer collapsed nor scrolls the page.
			HomeList->set_queue_status(QueueReady, HomeAccount);
			HomeList->refresh();
			if (selectedReview.has_value()) {
				static_cast<void>(HomeList->select(*selectedReview, false));
			}
		}
		if (rulesChanged) {
			QueueRuleSelector->clear();
			auto restoredIndex = -1;
			for (std::size_t index = 0U; index < QueueRules.size(); ++index) {
				const auto& rule = QueueRules[index];
				QueueRuleSelector->addItem(
					QStringLiteral("%1 %2%3")
						.arg(QString::fromUtf8(core::name(rule.Kind).data()))
						.arg(rule.Excluding ? QStringLiteral("not ") : QString{ })
						.arg(QString::fromStdString(rule.Pattern)));
				if (selectedRule == std::optional{ rule }) {
					restoredIndex = static_cast<int>(index);
				}
			}
			if (restoredIndex >= 0) {
				QueueRuleSelector->setCurrentIndex(restoredIndex);
			}
		}
		update_queue_status(aProjection);
		++Statistics.QueueUpdates;
		update_queue_rule_actions();
		update_saved_repository_actions();
		if (aProjection.Addition) {
			present_repository_addition(*aProjection.Addition);
		}
		update_enterprise_server_actions();
		update_discovery_controls();
		update_pull_request_queue_actions();
		update_pending_panes();
		observe_home_refresh(discovered, aProjection);
		apply_focus_order();
	}

	namespace {
		// [[BPR-0774]]. The card in its edit state is drawn the way a server
		// card is: the base colour, one Mid hairline and the card radius.
		class card_surface final : public QWidget
		{
		public:
			using QWidget::QWidget;

		protected:
			void paintEvent(QPaintEvent* const aEvent) override
			{
				static_cast<void>(aEvent);
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				painter.setPen(QPen{ palette().color(QPalette::Active, QPalette::Mid), 1.0 });
				painter.setBrush(palette().color(QPalette::Active, QPalette::Base));
				painter.drawRoundedRect(QRectF{ rect() }.adjusted(0.5, 0.5, -0.5, -0.5), 8.0, 8.0);
			}
		};
	} // namespace

	void workbench_window::build_repositories_page()
	{
		// RFC-BPR-0062, owner verdicts of 2026-10-07 and 2026-10-08: the saved
		// repositories have a page of their own, laid out the way Accounts &
		// hosts is. The account's repositories are one card; a new repository is
		// a card in its edit state above the bordered Add repository button that
		// opens it; there is no dialog. The page keeps no record of its own: the
		// rows are the queue host's accepted records, Remove sends the typed
		// intent for the exact record a row stands for, and Add sends the same
		// repository-addition gesture the dialog did.
		SavedRepositoriesEditor = new QWidget;
		SavedRepositoriesEditor->setObjectName(QStringLiteral("savedRepositoriesEditor"));
		SavedRepositoriesEditor->setAccessibleName(QStringLiteral("Repositories"));
		SavedRepositoriesEditor->setAccessibleDescription(QStringLiteral(
			"The repositories saved for the displayed account, where my pull requests and review requests are found"));
		auto* const column = new QVBoxLayout{ SavedRepositoriesEditor };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(16);
		auto* const lede = new QLabel{
			tr("My pull requests and review requests are found in these repositories. Nothing here changes GitHub."),
			SavedRepositoriesEditor
		};
		lede->setObjectName(QStringLiteral("repositoriesLede"));
		lede->setWordWrap(true);
		lede->setTextFormat(Qt::PlainText);
		lede->setForegroundRole(QPalette::PlaceholderText);
		column->addWidget(lede);

		// The account's card: whose repositories they are, then the list.
		auto* const card = new card_surface{ SavedRepositoriesEditor };
		card->setObjectName(QStringLiteral("savedRepositoriesCard"));
		card->setAccessibleName(QStringLiteral("Saved repositories"));
		auto* const cardColumn = new QVBoxLayout{ card };
		cardColumn->setContentsMargins(16, 12, 16, 12);
		cardColumn->setSpacing(8);
		SavedRepositoriesDestination = new QLabel{ card };
		SavedRepositoriesDestination->setObjectName(QStringLiteral("repositoriesDestination"));
		SavedRepositoriesDestination->setAccessibleName(tr("Account and host"));
		SavedRepositoriesDestination->setTextFormat(Qt::PlainText);
		SavedRepositoriesDestination->setWordWrap(true);
		SavedRepositoriesDestination->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		auto destinationFont = SavedRepositoriesDestination->font();
		destinationFont.setWeight(QFont::DemiBold);
		SavedRepositoriesDestination->setFont(destinationFont);
		cardColumn->addWidget(SavedRepositoriesDestination);
		SavedRepositoryList = new QListWidget{ card };
		SavedRepositoryList->setObjectName(QStringLiteral("savedRepositoryList"));
		SavedRepositoryList->setAccessibleName(QStringLiteral("Saved repositories"));
		SavedRepositoryList->setAccessibleDescription(QStringLiteral(
			"Select a repository; Delete or Remove stops finding my pull requests in it"));
		SavedRepositoryList->setSelectionMode(QAbstractItemView::SingleSelection);
		SavedRepositoryList->setFrameShape(QFrame::NoFrame);
		SavedRepositoryList->setWordWrap(true);
		SavedRepositoryList->setTextElideMode(Qt::ElideNone);
		SavedRepositoryList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
		SavedRepositoryList->setUniformItemSizes(false);
		SavedRepositoryList->setSpacing(2);
		// The card is as tall as its rows, up to ten, and scrolls beyond: a
		// card never draws an empty box under a short list.
		SavedRepositoryList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		SavedRepositoryList->installEventFilter(this);
		cardColumn->addWidget(SavedRepositoryList);
		// Empty is a sentence in the card, never an empty box.
		SavedRepositoriesEmpty = new QLabel{ tr("No repositories yet."), card };
		SavedRepositoriesEmpty->setObjectName(QStringLiteral("savedRepositoriesEmpty"));
		static_cast<QLabel*>(SavedRepositoriesEmpty)->setTextFormat(Qt::PlainText);
		SavedRepositoriesEmpty->setForegroundRole(QPalette::PlaceholderText);
		SavedRepositoriesEmpty->hide();
		cardColumn->addWidget(SavedRepositoriesEmpty);
		RemoveSavedRepository = new QAction{ tr("Remove"), this };
		RemoveSavedRepository->setObjectName(QStringLiteral("removeSavedRepository"));
		RemoveSavedRepositoryButton = new QPushButton{ tr("Remove"), card };
		RemoveSavedRepositoryButton->setObjectName(QStringLiteral("removeSavedRepositoryButton"));
		RemoveSavedRepositoryButton->setAccessibleName(QStringLiteral("Remove saved repository"));
		RemoveSavedRepositoryButton->setAccessibleDescription(QStringLiteral(
			"Stop finding my pull requests in the selected repository; nothing changes on GitHub"));
		RemoveSavedRepositoryButton->setAutoDefault(false);
		connect(RemoveSavedRepositoryButton, &QPushButton::clicked, RemoveSavedRepository, &QAction::trigger);
		auto* const cardActions = new QHBoxLayout{ };
		cardActions->setContentsMargins(0, 0, 0, 0);
		cardActions->addStretch(1);
		cardActions->addWidget(RemoveSavedRepositoryButton);
		cardColumn->addLayout(cardActions);
		column->addWidget(card);

		// The new repository, as Accounts & hosts draws a new server: a card in
		// its edit state with a labelled field, what was refused beside it, and
		// Add and Cancel at its trailing edge.
		auto* const draft = new card_surface{ SavedRepositoriesEditor };
		RepositoryDraft   = draft;
		draft->setObjectName(QStringLiteral("repositoryAdditionDraft"));
		draft->setAccessibleName(tr("New repository"));
		draft->setAccessibleDescription(tr("Find my pull requests and review requests in a repository saved for the displayed account"));
		auto* const grid = new QGridLayout{ draft };
		grid->setContentsMargins(16, 12, 16, 12);
		grid->setHorizontalSpacing(16);
		grid->setVerticalSpacing(8);
		grid->setColumnStretch(1, 1);
		auto* const draftTitle = new QLabel{ tr("New repository"), draft };
		draftTitle->setObjectName(QStringLiteral("repositoryAdditionTitle"));
		auto draftTitleFont = draftTitle->font();
		draftTitleFont.setWeight(QFont::DemiBold);
		draftTitle->setFont(draftTitleFont);
		RepositoryDestination = new QLabel{ draft };
		RepositoryDestination->setObjectName(QStringLiteral("repositoryAdditionDestination"));
		RepositoryDestination->setTextFormat(Qt::PlainText);
		RepositoryDestination->setWordWrap(true);
		RepositoryDestination->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		RepositoryDestination->setForegroundRole(QPalette::PlaceholderText);
		RepositoryDestination->setAccessibleName(tr("Destination account and host"));
		auto* const nameLabel = new QLabel{ tr("&Repository"), draft };
		SavedRepositoryName   = new QLineEdit{ draft };
		SavedRepositoryName->setObjectName(QStringLiteral("savedRepositoryName"));
		SavedRepositoryName->setAccessibleName(tr("Repository link or owner/name"));
		SavedRepositoryName->setAccessibleDescription(tr("Enter owner/name for the displayed host, or a complete HTTPS repository link"));
		SavedRepositoryName->setPlaceholderText(tr("owner/name or repository link"));
		SavedRepositoryName->setMaxLength(4096);
		SavedRepositoryName->installEventFilter(this);
		nameLabel->setBuddy(SavedRepositoryName);
		RepositoryAdditionStatus = new QLabel{ draft };
		RepositoryAdditionStatus->setObjectName(QStringLiteral("repositoryAdditionStatus"));
		RepositoryAdditionStatus->setAccessibleName(tr("Repository addition status"));
		RepositoryAdditionStatus->setTextFormat(Qt::PlainText);
		RepositoryAdditionStatus->setWordWrap(true);
		RepositoryAdditionStatus->hide();
		RepositoryPrepareButton = new QPushButton{ tr("Choose account or set up server…"), draft };
		RepositoryPrepareButton->setObjectName(QStringLiteral("repositoryAdditionPrepare"));
		RepositoryPrepareButton->setAccessibleDescription(tr("Keep this draft while signing in or explicitly adding its server; return here before saving"));
		RepositoryPrepareButton->setAutoDefault(false);
		RepositoryPrepareButton->hide();
		RepositoryAddButton = new QPushButton{ tr("Add"), draft };
		RepositoryAddButton->setObjectName(QStringLiteral("repositoryAdditionAdd"));
		RepositoryAddButton->setAccessibleName(tr("Add repository"));
		RepositoryAddButton->setAccessibleDescription(tr("Save this repository locally for the displayed account; nothing changes on GitHub"));
		RepositoryAddButton->setAutoDefault(false);
		RepositoryAddButton->setDefault(true);
		RepositoryCancelButton = new QPushButton{ tr("Cancel"), draft };
		RepositoryCancelButton->setObjectName(QStringLiteral("repositoryAdditionCancel"));
		RepositoryCancelButton->setAccessibleName(tr("Cancel new repository"));
		RepositoryCancelButton->setAccessibleDescription(tr("Discard this repository draft without saving"));
		RepositoryCancelButton->setAutoDefault(false);
		RepositoryCancelButton->setFlat(true);
		auto* const draftActions = new QHBoxLayout{ };
		draftActions->setContentsMargins(0, 0, 0, 0);
		draftActions->addWidget(RepositoryPrepareButton);
		draftActions->addStretch(1);
		draftActions->addWidget(RepositoryAddButton);
		draftActions->addWidget(RepositoryCancelButton);
		grid->addWidget(draftTitle, 0, 0, 1, 2);
		grid->addWidget(RepositoryDestination, 1, 0, 1, 2);
		grid->addWidget(nameLabel, 2, 0);
		grid->addWidget(SavedRepositoryName, 2, 1);
		grid->addWidget(RepositoryAdditionStatus, 3, 0, 1, 2);
		grid->addLayout(draftActions, 4, 0, 1, 2);
		draft->hide();
		column->addWidget(draft);

		AddSavedRepositoryButton = new QPushButton{ tr("Add repository"), SavedRepositoriesEditor };
		AddSavedRepositoryButton->setObjectName(QStringLiteral("addSavedRepositoryButton"));
		AddSavedRepositoryButton->setAccessibleName(QStringLiteral("Add repository"));
		AddSavedRepositoryButton->setAccessibleDescription(QStringLiteral(
			"Adds a repository for the displayed account as a new card on this page; nothing changes on GitHub"));
		AddSavedRepositoryButton->setAutoDefault(false);
		connect(AddSavedRepositoryButton, &QPushButton::clicked, AddSavedRepository, &QAction::trigger);
		auto* const addRow = new QHBoxLayout{ };
		addRow->setContentsMargins(0, 0, 0, 0);
		addRow->addWidget(AddSavedRepositoryButton);
		addRow->addStretch(1);
		column->addLayout(addRow);
		// A save is confirmed on the page as well as in the window's status bar.
		RepositoriesConfirmation = new QLabel{ SavedRepositoriesEditor };
		RepositoriesConfirmation->setObjectName(QStringLiteral("repositoriesConfirmation"));
		RepositoriesConfirmation->setAccessibleName(tr("Repository addition result"));
		RepositoriesConfirmation->setTextFormat(Qt::PlainText);
		RepositoriesConfirmation->setWordWrap(true);
		RepositoriesConfirmation->setForegroundRole(QPalette::PlaceholderText);
		RepositoriesConfirmation->hide();
		column->addWidget(RepositoriesConfirmation);
		column->addStretch(1);

		connect(RemoveSavedRepository, &QAction::triggered, this, [this] {
			dispatch_saved_repository(application::saved_repository_action::remove);
		});
		connect(SavedRepositoryList, &QListWidget::currentRowChanged, this,
				[this](int) { update_saved_repository_actions(); });
		connect(RepositoryAddButton, &QPushButton::clicked, this, [this] { submit_repository_addition(); });
		connect(SavedRepositoryName, &QLineEdit::returnPressed, this, [this] { submit_repository_addition(); });
		connect(RepositoryCancelButton, &QPushButton::clicked, this, [this] { close_repository_draft(); });
		connect(RepositoryPrepareButton, &QPushButton::clicked, this, [this] {
			if (!SetupDestinationDispatch || RepositorySaving) {
				return;
			}
			RepositoryPreparing      = true;
			RepositoryPendingGesture = 0U;
			update_repository_destination();
			SetupDestinationDispatch(application::submit_repository_setup{ .Text = SavedRepositoryName->text().toStdString() });
		});
		connect(SavedRepositoryName, &QLineEdit::textChanged, this, [this] {
			if (!RepositorySaving) {
				repository_addition_notice({ });
				RepositoryPrepareButton->hide();
			}
		});
		SettingsWindow->adopt_panel(settings_page::repositories, SavedRepositoriesEditor);
		update_saved_repository_list();
		update_saved_repository_actions();
	}

	void workbench_window::update_saved_repository_list()
	{
		if (SavedRepositoryList == nullptr) {
			return;
		}
		// Signed in, the displayed account's records. Signed out there is no
		// displayed account, so every kept record is listed with the exact
		// account and host it is saved for, rather than an empty page.
		const auto& session  = QueueProjection.Session;
		const auto  signedIn = !session.Host.empty() && !session.Account.empty();
		auto        rows     = QStringList{ };
		auto        records  = std::vector<const application::saved_repository*>{ };
		for (const auto& saved : QueueProjection.SavedRepositories) {
			if (!signedIn) {
				rows.push_back(tr("%1 (@%2 on %3)").arg(QString::fromStdString(saved.Repository), QString::fromStdString(saved.Account), QString::fromStdString(saved.Host)));
				records.push_back(&saved);
			} else if (saved.Host == session.Host && saved.Account == session.Account) {
				rows.push_back(QString::fromStdString(saved.Repository));
				records.push_back(&saved);
			}
		}
		auto current = QStringList{ };
		for (auto row = 0; row < SavedRepositoryList->count(); ++row) {
			current.push_back(SavedRepositoryList->item(row)->text());
		}
		SavedRepositoriesDestination->setText(signedIn ? tr("Saved for @%1 on %2").arg(QString::fromStdString(session.Account), QString::fromStdString(session.Host))
													   : tr("Signed out. Sign in to add or remove repositories for an account."));
		if (rows != current) {
			// The removed row's neighbour keeps the keyboard's place; a new list
			// starts on its first row so the keyboard lands on a repository.
			const auto selected = SavedRepositoryList->currentItem() != nullptr ? SavedRepositoryList->currentItem()->text() : QString{ };
			const auto previous = SavedRepositoryList->currentRow();
			const auto focused  = SavedRepositoryList->hasFocus();
			SavedRepositoryList->clear();
			for (std::size_t index = 0U; index < records.size(); ++index) {
				const auto& saved = *records[index];
				auto* const item  = new QListWidgetItem{ rows[static_cast<qsizetype>(index)], SavedRepositoryList };
				const auto  name  = QString::fromStdString(saved.Repository);
				const auto  host  = QString::fromStdString(saved.Host);
				const auto  login = QString::fromStdString(saved.Account);
				item->setData(Qt::UserRole, name);
				item->setData(Qt::UserRole + 1, host);
				item->setData(Qt::UserRole + 2, login);
				item->setToolTip(tr("%1 on %2, saved for @%3").arg(name, host, login));
				item->setData(Qt::AccessibleDescriptionRole, tr("On %1, saved for @%2").arg(host, login));
			}
			if (const auto restored = rows.indexOf(selected); restored >= 0) {
				SavedRepositoryList->setCurrentRow(static_cast<int>(restored));
			} else if (!rows.isEmpty()) {
				SavedRepositoryList->setCurrentRow(std::clamp(previous, 0, static_cast<int>(rows.size()) - 1));
			}
			if (focused && rows.isEmpty() && AddSavedRepositoryButton != nullptr) {
				AddSavedRepositoryButton->setFocus(Qt::OtherFocusReason);
			}
		}
		SavedRepositoryList->setVisible(!rows.isEmpty());
		if (!rows.isEmpty()) {
			const auto shown  = std::min(SavedRepositoryList->count(), 10);
			auto       height = 2 * SavedRepositoryList->frameWidth();
			for (auto row = 0; row < shown; ++row) {
				height += SavedRepositoryList->sizeHintForRow(row) + 2 * SavedRepositoryList->spacing();
			}
			SavedRepositoryList->setFixedHeight(height);
		}
		SavedRepositoriesEmpty->setVisible(rows.isEmpty());
		if (RemoveSavedRepositoryButton != nullptr) {
			RemoveSavedRepositoryButton->setVisible(!rows.isEmpty());
		}
	}

	void workbench_window::build_enterprise_server_draft(QWidget* const aParent, QVBoxLayout& aLayout)
	{
		auto* const draft     = new card_surface{ aParent };
		EnterpriseServerDraft = draft;
		draft->setObjectName(QStringLiteral("enterpriseServerDraft"));
		draft->setAccessibleName(QStringLiteral("New enterprise server"));
		auto* const grid = new QGridLayout{ draft };
		grid->setContentsMargins(16, 12, 16, 12);
		grid->setHorizontalSpacing(16);
		grid->setVerticalSpacing(8);
		grid->setColumnMinimumWidth(0, 150);
		grid->setColumnStretch(1, 1);
		auto* const title = new QLabel{ QStringLiteral("Enterprise server"), draft };
		title->setObjectName(QStringLiteral("enterpriseServerDraftTitle"));
		auto titleFont = title->font();
		titleFont.setWeight(QFont::DemiBold);
		title->setFont(titleFont);
		auto* const hostnameName = new QLabel{ QStringLiteral("Hostname"), draft };
		EnterpriseServerHostname = new QLineEdit{ draft };
		EnterpriseServerHostname->setObjectName(QStringLiteral("enterpriseServerHostname"));
		EnterpriseServerHostname->setAccessibleName(QStringLiteral("Hostname"));
		EnterpriseServerHostname->setAccessibleDescription(QStringLiteral("The hostname of a GitHub Enterprise server, without a scheme"));
		EnterpriseServerHostname->setPlaceholderText(QStringLiteral("github.example.com"));
		EnterpriseServerHostname->setMaxLength(253);
		hostnameName->setBuddy(EnterpriseServerHostname);
		auto* const accountName = new QLabel{ QStringLiteral("Account"), draft };
		EnterpriseServerAccount = new QLineEdit{ draft };
		EnterpriseServerAccount->setObjectName(QStringLiteral("enterpriseServerAccount"));
		EnterpriseServerAccount->setAccessibleName(QStringLiteral("Account"));
		EnterpriseServerAccount->setAccessibleDescription(QStringLiteral("The account you sign in with there, if you know it"));
		EnterpriseServerAccount->setPlaceholderText(QStringLiteral("optional"));
		EnterpriseServerAccount->setMaxLength(128);
		accountName->setBuddy(EnterpriseServerAccount);
		EnterpriseServerRefusal = new QLabel{ draft };
		EnterpriseServerRefusal->setObjectName(QStringLiteral("enterpriseServerRefusal"));
		EnterpriseServerRefusal->setTextFormat(Qt::PlainText);
		EnterpriseServerRefusal->setWordWrap(true);
		EnterpriseServerRefusal->hide();
		EnterpriseServerAdd = new QPushButton{ QStringLiteral("Add"), draft };
		EnterpriseServerAdd->setObjectName(QStringLiteral("enterpriseServerAdd"));
		EnterpriseServerAdd->setAutoDefault(false);
		EnterpriseServerAdd->setAccessibleName(QStringLiteral("Add enterprise server"));
		EnterpriseServerAdd->setAccessibleDescription(QStringLiteral("Keeps this server so it is offered as a host. No sign-in is stored."));
		EnterpriseServerCancel = new QPushButton{ QStringLiteral("Cancel"), draft };
		EnterpriseServerCancel->setObjectName(QStringLiteral("enterpriseServerCancel"));
		EnterpriseServerCancel->setAutoDefault(false);
		EnterpriseServerCancel->setFlat(true);
		auto* const actions = new QHBoxLayout{ };
		actions->setContentsMargins(0, 0, 0, 0);
		actions->addStretch(1);
		actions->addWidget(EnterpriseServerAdd);
		actions->addWidget(EnterpriseServerCancel);
		grid->addWidget(title, 0, 0, 1, 2);
		grid->addWidget(hostnameName, 1, 0);
		grid->addWidget(EnterpriseServerHostname, 1, 1);
		grid->addWidget(accountName, 2, 0);
		grid->addWidget(EnterpriseServerAccount, 2, 1);
		grid->addWidget(EnterpriseServerRefusal, 3, 0, 1, 2);
		grid->addLayout(actions, 4, 0, 1, 2);
		draft->hide();
		aLayout.addWidget(draft);

		UseGithubCliButton = new QPushButton{ QStringLiteral("Use GitHub CLI"), aParent };
		UseGithubCliButton->setObjectName(QStringLiteral("accountsUseGithubCli"));
		UseGithubCliButton->setAutoDefault(false);
		UseGithubCliButton->setAccessibleDescription(QStringLiteral(
			"Checks active authenticated hosts with the installed gh. Tokens are never requested or shown."));
		UseGithubCliButton->hide();

		AddEnterpriseServerButton = new QPushButton{ QStringLiteral("Add enterprise server"), aParent };
		AddEnterpriseServerButton->setObjectName(QStringLiteral("accountsAddEnterpriseServer"));
		AddEnterpriseServerButton->setAutoDefault(false);
		AddEnterpriseServerButton->setAccessibleDescription(QStringLiteral("Adds a GitHub Enterprise server as a card, offered again at the next start. No sign-in is stored."));
		auto* const addRow = new QHBoxLayout{ };
		addRow->setContentsMargins(0, 0, 0, 0);
		addRow->addWidget(UseGithubCliButton);
		addRow->addWidget(AddEnterpriseServerButton);
		addRow->addStretch(1);
		aLayout.addLayout(addRow);
		GithubCliDiscoveryStatus = new QLabel{ aParent };
		GithubCliDiscoveryStatus->setObjectName(QStringLiteral("githubCliDiscoveryStatus"));
		GithubCliDiscoveryStatus->setTextFormat(Qt::PlainText);
		GithubCliDiscoveryStatus->setWordWrap(true);
		GithubCliDiscoveryStatus->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
		GithubCliDiscoveryStatus->setForegroundRole(QPalette::PlaceholderText);
		GithubCliDiscoveryStatus->hide();
		aLayout.addWidget(GithubCliDiscoveryStatus);

		connect(UseGithubCliButton, &QPushButton::clicked, this, [this] {
			if (GithubCliDiscoveryDispatch
				&& GithubCliDiscoveryState != github_cli_discovery_state::discovering) {
				open_sign_in(SignInHostname, true);
				GithubCliDiscoveryDispatch();
			}
		});
		connect(AddEnterpriseServerButton, &QPushButton::clicked, this, [this] { open_enterprise_server_draft(); });
		connect(EnterpriseServerAdd, &QPushButton::clicked, this, [this] { add_enterprise_server(); });
		connect(EnterpriseServerHostname, &QLineEdit::returnPressed, this, [this] { add_enterprise_server(); });
		connect(EnterpriseServerAccount, &QLineEdit::returnPressed, this, [this] { add_enterprise_server(); });
		connect(EnterpriseServerCancel, &QPushButton::clicked, this, [this] { close_enterprise_server_draft(); });
		connect(EnterpriseServerHostname, &QLineEdit::textEdited, this, [this] { EnterpriseServerRefusal->hide(); });
		update_enterprise_server_actions();
	}

	void workbench_window::set_saved_server_dispatch(workbench_saved_server_dispatch aDispatch)
	{
		SavedServerDispatch = std::move(aDispatch);
		update_enterprise_server_actions();
	}

	void workbench_window::set_github_cli_discovery_dispatch(github_cli_discovery_dispatch aDispatch)
	{
		GithubCliDiscoveryDispatch = std::move(aDispatch);
		if (HomeLanding != nullptr) {
			HomeLanding->set_github_cli_available(
				static_cast<bool>(GithubCliDiscoveryDispatch));
		}
		if (UseGithubCliButton != nullptr) {
			UseGithubCliButton->setVisible(static_cast<bool>(GithubCliDiscoveryDispatch));
			UseGithubCliButton->setEnabled(
				GithubCliDiscoveryState != github_cli_discovery_state::discovering);
		}
	}

	void workbench_window::set_github_cli_check_dispatch(github_cli_discovery_dispatch aDispatch)
	{
		GithubCliCheckDispatch = std::move(aDispatch);
		update_server_cards();
	}

	void workbench_window::present_cli_accounts(std::map<std::string, std::vector<server_card_cli_account>, std::less<>> aAccounts)
	{
		CliAccountsByHost = std::move(aAccounts);
		update_server_cards();
	}

	void workbench_window::present_github_cli_discovery(
		const github_cli_discovery_state aState, const std::size_t aAccounts)
	{
		GithubCliDiscoveryState = aState;
		if (UseGithubCliButton == nullptr || GithubCliDiscoveryStatus == nullptr) {
			return;
		}
		UseGithubCliButton->setEnabled(
			static_cast<bool>(GithubCliDiscoveryDispatch)
			&& aState != github_cli_discovery_state::discovering);
		switch (aState) {
		case github_cli_discovery_state::idle:
			GithubCliDiscoveryStatus->clear();
			GithubCliDiscoveryStatus->hide();
			break;
		case github_cli_discovery_state::discovering:
			GithubCliDiscoveryStatus->setText(QStringLiteral("Checking GitHub CLI accounts…"));
			GithubCliDiscoveryStatus->show();
			break;
		case github_cli_discovery_state::ready:
			GithubCliDiscoveryStatus->setText(
				QStringLiteral("Found %1 GitHub CLI account%2.")
					.arg(aAccounts)
					.arg(aAccounts == 1U ? QString{ } : QStringLiteral("s")));
			GithubCliDiscoveryStatus->show();
			break;
		case github_cli_discovery_state::empty:
			GithubCliDiscoveryStatus->setText(QStringLiteral(
				"GitHub CLI reported no signed-in accounts. Run gh auth login in Terminal, then retry."));
			GithubCliDiscoveryStatus->show();
			break;
		case github_cli_discovery_state::unsupported_version:
			GithubCliDiscoveryStatus->setText(QStringLiteral(
				"This GitHub CLI version is too old. Upgrade to gh 2.96 or newer, then try again."));
			GithubCliDiscoveryStatus->show();
			break;
		case github_cli_discovery_state::process_failed:
			GithubCliDiscoveryStatus->setText(QStringLiteral(
				"GitHub CLI could not be run successfully. Verify gh auth status in Terminal, then try again."));
			GithubCliDiscoveryStatus->show();
			break;
		case github_cli_discovery_state::ineligible_accounts:
			GithubCliDiscoveryStatus->setText(QStringLiteral("GitHub CLI accounts were found, but none is both signed in and available from secure storage. Check gh auth status in Terminal. If it works there, check for operating-system updates and retry, or use Sign in."));
			GithubCliDiscoveryStatus->show();
			break;
		case github_cli_discovery_state::missing_executable:
			GithubCliDiscoveryStatus->setText(QStringLiteral("GitHub CLI was not found. Install gh 2.96 or newer in PATH or /usr/local/bin, then retry."));
			GithubCliDiscoveryStatus->show();
			break;
		case github_cli_discovery_state::launch_failed:
			GithubCliDiscoveryStatus->setText(QStringLiteral("GitHub CLI could not start. Check that gh can run on this system, then retry. On macOS, /usr/bin/sandbox-exec must also be available."));
			GithubCliDiscoveryStatus->show();
			break;
		case github_cli_discovery_state::timed_out:
			GithubCliDiscoveryStatus->setText(QStringLiteral("GitHub CLI account discovery timed out. Check gh auth status in Terminal and network access, then retry."));
			GithubCliDiscoveryStatus->show();
			break;
		case github_cli_discovery_state::exited_unsuccessfully:
			GithubCliDiscoveryStatus->setText(QStringLiteral("GitHub CLI exited unsuccessfully during discovery. Check gh --version and gh auth status in Terminal, then retry."));
			GithubCliDiscoveryStatus->show();
			break;
		case github_cli_discovery_state::output_rejected:
			GithubCliDiscoveryStatus->setText(QStringLiteral("GitHub CLI output exceeded a limit or wrote unexpected diagnostic output. Check gh --version and gh auth status in Terminal; share the safe BPR diagnostic category if this persists."));
			GithubCliDiscoveryStatus->show();
			break;
		case github_cli_discovery_state::malformed_response:
			GithubCliDiscoveryStatus->setText(QStringLiteral(
				"GitHub CLI returned an account status this version of BetterPullReview could not safely read."));
			GithubCliDiscoveryStatus->show();
			break;
		}
		update_server_cards();
		update_sign_in_surface();
	}

	auto workbench_window::use_github_cli_button() const noexcept -> QPushButton*
	{
		return UseGithubCliButton;
	}

	auto workbench_window::github_cli_discovery_status() const noexcept -> QLabel*
	{
		return GithubCliDiscoveryStatus;
	}

	void workbench_window::open_enterprise_server_draft()
	{
		if (EnterpriseServerDraft == nullptr || !QueueReady || !SavedServerDispatch) {
			return;
		}
		EnterpriseDraftOpen = true;
		EnterpriseServerRefusal->hide();
		update_enterprise_server_actions();
		EnterpriseServerHostname->setFocus(Qt::OtherFocusReason);
	}

	void workbench_window::close_enterprise_server_draft()
	{
		if (EnterpriseServerDraft == nullptr) {
			return;
		}
		const auto hadFocus = EnterpriseServerDraft->isAncestorOf(QApplication::focusWidget());
		EnterpriseDraftOpen = false;
		EnterpriseServerHostname->clear();
		EnterpriseServerAccount->clear();
		EnterpriseServerRefusal->hide();
		update_enterprise_server_actions();
		if (hadFocus && AddEnterpriseServerButton->isVisible()) {
			AddEnterpriseServerButton->setFocus(Qt::OtherFocusReason);
		}
	}

	void workbench_window::add_enterprise_server()
	{
		if (!EnterpriseDraftOpen) {
			return;
		}
		// A refusal sits beside what was refused, and the fields stay as typed.
		if (!QueueReady || !SavedServerDispatch) {
			EnterpriseServerRefusal->setText(QStringLiteral("Servers can be kept once the review queue has loaded."));
			EnterpriseServerRefusal->show();
			return;
		}
		auto server = application::saved_server{
			.Hostname = EnterpriseServerHostname->text().trimmed().toLower().toStdString(),
			.Account  = EnterpriseServerAccount->text().trimmed().toStdString(),
		};
		if (!application::valid(server)) {
			EnterpriseServerRefusal->setText(QStringLiteral("Enter a hostname such as github.example.com, without a scheme or a path."));
			EnterpriseServerRefusal->show();
			return;
		}
		SavedServerDispatch(application::saved_server_intent{
			.Action = application::saved_server_action::add,
			.Server = std::move(server),
		});
		close_enterprise_server_draft();
	}

	void workbench_window::update_enterprise_server_actions()
	{
		if (EnterpriseServerDraft == nullptr || AddEnterpriseServerButton == nullptr) {
			return;
		}
		// A server is kept without an account and without signing in: only the
		// queue's own durable state is needed, so adding waits for it.
		const auto admitted = QueueReady && static_cast<bool>(SavedServerDispatch);
		if (!admitted && EnterpriseDraftOpen) {
			EnterpriseDraftOpen = false;
			EnterpriseServerHostname->clear();
			EnterpriseServerAccount->clear();
			EnterpriseServerRefusal->hide();
		}
		EnterpriseServerDraft->setVisible(admitted && EnterpriseDraftOpen);
		AddEnterpriseServerButton->setVisible(admitted && !EnterpriseDraftOpen);
		update_server_cards();
	}

	void workbench_window::remember_enterprise_server(const std::string& aHostname)
	{
		if (!QueueReady || !SavedServerDispatch || SessionPanel == nullptr) {
			return;
		}
		// The account name kept with the hostname is the one signed in on that
		// server's card, when one is. The token is still asked for.
		auto        server  = application::saved_server{ .Hostname = aHostname };
		const auto& session = SessionPanel->session();
		const auto  card    = std::ranges::find_if(ServerCards, [&aHostname](const server_card* const aCard) {
			return aCard->hostname() == aHostname;
		});
		if (card != ServerCards.end() && session.has_value() && (*card)->holds(session->Profile)
			&& !SessionPanel->public_preview_signed_out()) {
			server.Account = session->Login;
		}
		if (!application::valid(server)) {
			return;
		}
		SavedServerDispatch(application::saved_server_intent{
			.Action = application::saved_server_action::add,
			.Server = std::move(server),
		});
	}

	void workbench_window::confirm_forget_server(const application::saved_server& aServer)
	{
		if (!QueueReady || !SavedServerDispatch) {
			return;
		}
		if (ForgetServerConfirmation != nullptr) {
			ForgetServerConfirmation->close();
		}
		// Forgetting is confirmed first, and the confirmation names exactly
		// the record it removes and what it does not touch.
		// Owned by the Settings window the reviewer is looking at: the profile's
		// shell when one is mounted, so the confirmation is modal to that page.
		auto* const owner = settings() != nullptr ? static_cast<QWidget*>(settings()) : static_cast<QWidget*>(this);
		auto* const box   = new QMessageBox{ owner };
		box->setObjectName(QStringLiteral("forgetServerConfirmation"));
		box->setAttribute(Qt::WA_DeleteOnClose);
		box->setWindowModality(Qt::WindowModal);
		box->setIcon(QMessageBox::Question);
		const auto host    = QString::fromStdString(aServer.Hostname);
		const auto account = QString::fromStdString(aServer.Account);
		box->setWindowTitle(QStringLiteral("Forget %1?").arg(host));
		box->setText(QStringLiteral("Forget %1?").arg(host));
		box->setInformativeText(
			account.isEmpty()
				? QStringLiteral("BetterPullReview stops offering %1 at the next start. Nothing on the server changes, and no sign-in is removed.").arg(host)
				: QStringLiteral("BetterPullReview forgets %1 with the account %2 and stops offering it at the next start. Nothing on the server changes, and no sign-in is removed.").arg(host, account));
		auto* const forget = box->addButton(QStringLiteral("Forget server"), QMessageBox::DestructiveRole);
		forget->setObjectName(QStringLiteral("forgetServerConfirm"));
		auto* const cancel = box->addButton(QMessageBox::Cancel);
		cancel->setObjectName(QStringLiteral("forgetServerCancel"));
		box->setDefaultButton(cancel);
		box->setEscapeButton(cancel);
		ForgetServerConfirmation = box;
		connect(box, &QMessageBox::buttonClicked, this, [this, server = aServer, forget](QAbstractButton* const aButton) {
			if (aButton != forget || !QueueReady || !SavedServerDispatch) {
				return;
			}
			SavedServerDispatch(application::saved_server_intent{
				.Action = application::saved_server_action::remove,
				.Server = server,
			});
		});
		box->open();
	}

	auto workbench_window::add_enterprise_server_button() const noexcept -> QPushButton*
	{
		return AddEnterpriseServerButton;
	}

	auto workbench_window::enterprise_server_draft() const noexcept -> QWidget*
	{
		return EnterpriseServerDraft;
	}

	auto workbench_window::enterprise_server_hostname() const noexcept -> QLineEdit*
	{
		return EnterpriseServerHostname;
	}

	auto workbench_window::enterprise_server_account() const noexcept -> QLineEdit*
	{
		return EnterpriseServerAccount;
	}

	auto workbench_window::enterprise_server_add() const noexcept -> QPushButton*
	{
		return EnterpriseServerAdd;
	}

	auto workbench_window::enterprise_server_cancel() const noexcept -> QPushButton*
	{
		return EnterpriseServerCancel;
	}

	auto workbench_window::enterprise_server_refusal() const noexcept -> QLabel*
	{
		return EnterpriseServerRefusal;
	}

	auto workbench_window::forget_server_confirmation() const noexcept -> QMessageBox*
	{
		return ForgetServerConfirmation.data();
	}

	auto workbench_window::kept_servers() const noexcept -> const std::vector<application::saved_server>&
	{
		return QueueProjection.SavedServers;
	}

	void workbench_window::set_saved_repository_dispatch(workbench_saved_repository_dispatch aDispatch)
	{
		SavedRepositoryDispatch = std::move(aDispatch);
		update_saved_repository_actions();
		update_pull_request_queue_actions();
	}

	void workbench_window::set_repository_addition_dispatch(std::function<void(application::repository_addition_intent)> aDispatch)
	{
		RepositoryAdditionDispatch = std::move(aDispatch);
		update_saved_repository_actions();
	}

	auto workbench_window::repository_addition_draft() const noexcept -> QWidget* { return RepositoryDraft; }

	void workbench_window::repository_addition_notice(const QString& aText)
	{
		if (!RepositoryAdditionStatus || RepositoryAdditionStatus->text() == aText) {
			return;
		}
		RepositoryAdditionStatus->setText(aText);
		RepositoryAdditionStatus->setVisible(!aText.isEmpty());
		if (!aText.isEmpty()) {
			QAccessibleAnnouncementEvent event{ RepositoryAdditionStatus, aText };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		}
	}

	void workbench_window::open_repository_addition()
	{
		if (!RepositoryAdditionDispatch || RepositoryDraft == nullptr) {
			return;
		}
		// Every Add repository is this page with the field ready. A draft kept
		// through explicit setup returns as it was; a new one starts clean.
		if (!RepositoryPreparing && !RepositoryDraftOpen) {
			repository_addition_notice({ });
			RepositoryPrepareButton->hide();
		}
		RepositoryPreparing = false;
		RepositoryDraftOpen = true;
		if (RepositoriesConfirmation != nullptr) {
			RepositoriesConfirmation->hide();
		}
		update_repository_destination();
		open_settings(settings_page::repositories);
		if (auto* const shell = settings(); shell != nullptr) {
			shell->activateWindow();
		}
		SavedRepositoryName->setFocus(Qt::OtherFocusReason);
	}

	void workbench_window::close_repository_draft()
	{
		if (RepositoryDraft == nullptr) {
			return;
		}
		const auto hadFocus      = RepositoryDraft->isAncestorOf(QApplication::focusWidget());
		const auto preparing     = RepositoryPreparing;
		RepositoryDraftOpen      = false;
		RepositoryPreparing      = false;
		RepositorySaving         = false;
		RepositoryPendingGesture = 0U;
		SavedRepositoryName->clear();
		repository_addition_notice({ });
		RepositoryPrepareButton->hide();
		if (preparing && SetupDestinationDispatch) {
			SetupDestinationDispatch(application::cancel_setup_destination{ });
		}
		update_repository_destination();
		if (hadFocus && AddSavedRepositoryButton->isVisibleTo(SavedRepositoriesEditor)) {
			AddSavedRepositoryButton->setFocus(Qt::OtherFocusReason);
		}
	}

	void workbench_window::submit_repository_addition()
	{
		if (!RepositoryDraftOpen || RepositorySaving || RepositoryPreparing || !RepositoryAdditionDispatch
			|| RepositoryGesture == std::numeric_limits<std::uint64_t>::max()) {
			return;
		}
		RepositoryPendingGesture = ++RepositoryGesture;
		RepositoryAdditionDispatch({ .Gesture = RepositoryPendingGesture, .Destination = RepositoryDisplayedDestination, .Text = SavedRepositoryName->text().toStdString() });
	}

	void workbench_window::update_repository_destination()
	{
		if (RepositoryDraft == nullptr) {
			return;
		}
		if (RepositoryDisplayedDestination != QueueProjection.Session) {
			RepositoryDisplayedDestination = QueueProjection.Session;
			RepositoryPendingGesture       = 0U;
			RepositorySaving               = false;
			if (!SavedRepositoryName->text().isEmpty()) {
				repository_addition_notice(tr("The destination changed. Check the account and host, then choose Add."));
			}
		}
		const auto& destination = RepositoryDisplayedDestination;
		RepositoryDestination->setText(destination.Account.empty()
										   ? tr("Sign in to choose the account for this repository.")
										   : tr("Save for @%1 on %2").arg(QString::fromStdString(destination.Account), QString::fromStdString(destination.Host)));
		RepositoryAddButton->setEnabled(static_cast<bool>(RepositoryAdditionDispatch) && !RepositorySaving && !RepositoryPreparing);
		SavedRepositoryName->setEnabled(!RepositorySaving);
		RepositoryPrepareButton->setEnabled(!RepositorySaving && static_cast<bool>(SetupDestinationDispatch));
		RepositoryDraft->setVisible(RepositoryDraftOpen);
		AddSavedRepositoryButton->setVisible(!RepositoryDraftOpen);
	}

	void workbench_window::present_repository_addition(const application::repository_addition_result& aResult)
	{
		if (RepositoryPendingGesture == 0U || aResult.Gesture != RepositoryPendingGesture || aResult.Destination != RepositoryDisplayedDestination) {
			return;
		}
		using state      = core::repository_addition_status;
		RepositorySaving = aResult.Status == state::saving;
		update_repository_destination();
		QString notice;
		switch (aResult.Status) {
		case state::saving: notice = tr("Adding repository…"); break;
		case state::saved:  {
			notice = aResult.Target ? tr("Added %1/%2.").arg(QString::fromStdString(aResult.Target->Repository.Owner), QString::fromStdString(aResult.Target->Repository.Name)) : tr("Repository added.");
			statusBar()->showMessage(notice, 6000);
			// The draft card closes into the list, as a kept server becomes its
			// card; the page says what was added and the keyboard returns to
			// Add repository.
			const auto onPage        = RepositoryDraft->isAncestorOf(QApplication::focusWidget());
			RepositoryPendingGesture = 0U;
			RepositorySaving         = false;
			RepositoryDraftOpen      = false;
			SavedRepositoryName->clear();
			repository_addition_notice({ });
			RepositoryPrepareButton->hide();
			update_repository_destination();
			RepositoriesConfirmation->setText(notice);
			RepositoriesConfirmation->show();
			QAccessibleAnnouncementEvent event{ RepositoriesConfirmation, notice };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
			if (onPage) {
				AddSavedRepositoryButton->setFocus(Qt::OtherFocusReason);
			}
			return;
		}
		case state::already_added:       notice = tr("This repository is already added."); break;
		case state::invalid_input:       notice = tr("Enter owner/name or a complete HTTPS repository link without a pull-request path, query or fragment."); break;
		case state::sign_in_required:    notice = tr("Sign in to the repository's host, then return here and choose Add."); break;
		case state::destination_changed: notice = tr("This repository needs a different destination. Choose its account or explicitly set up its server, then return here and choose Add."); break;
		case state::unavailable:         notice = tr("The repository could not be saved locally. Your input is kept; choose Add to retry."); break;
		case state::busy:                notice = tr("Another repository change is being saved. Try Add again when it finishes."); break;
		case state::collection_limit:    notice = tr("The saved repository limit has been reached. Remove a repository before adding another."); break;
		}
		if (RepositoryDraftOpen) {
			repository_addition_notice(notice);
			RepositoryPrepareButton->setVisible(aResult.Target.has_value() && (aResult.Status == state::sign_in_required || aResult.Status == state::destination_changed));
		} else if (!RepositorySaving) {
			statusBar()->showMessage(notice, 6000);
		}
	}

	auto workbench_window::saved_repositories_editor() const noexcept -> QWidget*
	{
		return SavedRepositoriesEditor;
	}

	auto workbench_window::saved_repository_list() const noexcept -> QListWidget*
	{
		return SavedRepositoryList;
	}

	auto workbench_window::saved_repositories_empty() const noexcept -> QWidget*
	{
		return SavedRepositoriesEmpty;
	}

	auto workbench_window::home_repository_onboarding() const noexcept -> QWidget*
	{
		return HomeOnboarding;
	}

	auto workbench_window::home_repository_onboarding_button() const noexcept -> QPushButton*
	{
		return HomeOnboardingButton;
	}

	auto workbench_window::saved_repository_name() const noexcept -> QLineEdit*
	{
		return SavedRepositoryName;
	}

	auto workbench_window::add_saved_repository_action() const noexcept -> QAction*
	{
		return AddSavedRepository;
	}

	auto workbench_window::remove_saved_repository_action() const noexcept -> QAction*
	{
		return RemoveSavedRepository;
	}

	void workbench_window::dispatch_saved_repository(const application::saved_repository_action aAction)
	{
		if (!SavedRepositoryDispatch || !QueueReady) {
			return;
		}
		auto repository = application::saved_repository{
			.Host    = QueueProjection.Session.Host,
			.Account = QueueProjection.Session.Account,
		};
		if (aAction == application::saved_repository_action::add) {
			repository.Repository = SavedRepositoryName->text().trimmed().toStdString();
		} else {
			// The row names the exact accepted record of the displayed account.
			const auto* const item = SavedRepositoryList != nullptr ? SavedRepositoryList->currentItem() : nullptr;
			if (item == nullptr) {
				return;
			}
			repository = application::saved_repository{
				.Host       = item->data(Qt::UserRole + 1).toString().toStdString(),
				.Account    = item->data(Qt::UserRole + 2).toString().toStdString(),
				.Repository = item->data(Qt::UserRole).toString().toStdString(),
			};
			if (std::ranges::find(QueueProjection.SavedRepositories, repository) == QueueProjection.SavedRepositories.end()) {
				return;
			}
		}
		if (!application::valid(repository)) {
			return;
		}
		SavedRepositoryDispatch(application::saved_repository_intent{
			.Action     = aAction,
			.Repository = std::move(repository),
		});
	}

	void workbench_window::update_saved_repository_actions()
	{
		if (AddSavedRepository == nullptr || RemoveSavedRepository == nullptr) {
			return;
		}
		const auto account = QueueReady && static_cast<bool>(SavedRepositoryDispatch)
			&& !QueueProjection.Session.Host.empty() && !QueueProjection.Session.Account.empty();
		AddSavedRepository->setEnabled(static_cast<bool>(RepositoryAdditionDispatch));
		RemoveSavedRepository->setEnabled(
			account && SavedRepositoryList != nullptr && SavedRepositoryList->currentItem() != nullptr);
		AddSavedRepository->setToolTip(tr("Find my pull requests and review requests in a saved repository; nothing changes on GitHub."));
		update_repository_destination();
		RemoveSavedRepository->setToolTip(account ? QStringLiteral("Stop finding my pull requests in the selected repository; nothing changes on GitHub.")
												  : QStringLiteral("Sign in to remove a saved repository."));
		if (AddSavedRepositoryButton != nullptr) {
			AddSavedRepositoryButton->setEnabled(AddSavedRepository->isEnabled());
			AddSavedRepositoryButton->setToolTip(AddSavedRepository->toolTip());
		}
		if (RemoveSavedRepositoryButton != nullptr) {
			RemoveSavedRepositoryButton->setEnabled(RemoveSavedRepository->isEnabled());
			RemoveSavedRepositoryButton->setToolTip(RemoveSavedRepository->toolTip());
		}
	}

	void workbench_window::publish(
		const application::workbench_projection& aProjection)
	{
		const auto previousAnchor = conversation_anchor_file();
		const auto heldRefresh    = RefreshConversationAnchor.has_value();
		const auto refreshGap     = HasGithubPullRequest && previousAnchor.has_value()
			&& LastWorkbenchContext.has_value() && aProjection.Context.Session.value() != 0U
			&& aProjection.Context.Session == LastWorkbenchContext->Session
			&& aProjection.Context.Generation.value() > LastWorkbenchContext->Generation.value()
			&& aProjection.Context.Request.value() == 0U
			&& aProjection.Files.has_value() && aProjection.Files->empty()
			&& aProjection.Selection.has_value() && !aProjection.Selection->Cursor.has_value();
		// [[BPR-0818]]. The hold ends with the gap. The observation's terminal
		// publishes the new file set and cursor, and nothing restores the file
		// selection after it, so holding past it scoped the drawer to a file
		// nobody had selected and hid every review summary and issue comment.
		// The widening that ends the hold runs on its own event-loop turn:
		// re-projecting a thousand-thread pull request takes tens of
		// milliseconds, and sharing a turn with the rest of the refresh held
		// input for longer than one frame budget allows.
		if (refreshGap) {
			RefreshConversationAnchor  = previousAnchor;
			RefreshConversationContext = aProjection.Context;
		} else if (aProjection.Selection.has_value() || aProjection.Files.has_value() || aProjection.FilesDelta.has_value() || (RefreshConversationContext.has_value() && aProjection.Context.Session != RefreshConversationContext->Session)) {
			RefreshConversationAnchor.reset();
			RefreshConversationContext.reset();
		}
		LastWorkbenchContext = aProjection.Context;

		if (aProjection.Commands) {
			Commands    = *aProjection.Commands;
			HasCommands = true;
			++Statistics.CommandUpdates;
		}
		if (aProjection.Files) {
			apply_file_rows(*aProjection.Files);
		}
		if (aProjection.FilesDelta) {
			apply_file_delta(*aProjection.FilesDelta);
		}
		if (aProjection.Selection) {
			Cursor = aProjection.Selection->Cursor;
			apply_file_selection();
		}
		if (ConversationPanel != nullptr && previousAnchor != conversation_anchor_file()) {
			if (heldRefresh && !RefreshConversationAnchor.has_value() && !conversation_anchor_file().has_value()) {
				schedule_conversation_anchor();
			} else {
				apply_conversation_anchor();
			}
		}
		if (aProjection.Drawers) {
			DrawerStates = *aProjection.Drawers;
			++Statistics.DrawerUpdates;
		}
		if (aProjection.DrawerRestore) {
			DrawerRestore = *aProjection.DrawerRestore;
		}
		if (aProjection.LineNumbers) {
			LineNumberState = *aProjection.LineNumbers;
		}
		if (aProjection.OnePane) {
			OnePaneState = *aProjection.OnePane;
		}
		if (aProjection.UpdatesLine) {
			UpdatesLineState = *aProjection.UpdatesLine;
		}
		if (aProjection.Layout) {
			LayoutWidths = *aProjection.Layout;
			// [[BPR-1588]]. The window's Status drawer as the host published it.
			if (aProjection.Layout->StatusDrawer) {
				OverviewPage->status_drawer()->adopt(*aProjection.Layout->StatusDrawer);
			}
			if (aProjection.Layout->SummaryFraction) {
				OverviewPage->set_summary_fraction(*aProjection.Layout->SummaryFraction);
			}
			// [[BPR-1334]]. This window's Line details height, once loaded.
			if (aProjection.Layout->LineDetailsHeight && ComparisonSurface != nullptr) {
				ComparisonSurface->set_line_details_height(*aProjection.Layout->LineDetailsHeight);
			}
			++Statistics.LayoutUpdates;
		}
		if (aProjection.Behaviour) {
			Behaviour          = *aProjection.Behaviour;
			BehaviourPublished = true;
			AskedBehaviour.reset();
			apply_behaviour_controls();
			apply_label_marks();
			// [[BPR-1089]]. The Overview's chips and where its count starts.
			if (OverviewPage != nullptr) {
				OverviewPage->set_label_columns(Behaviour.LabelColumns);
				OverviewPage->set_default_mode(Behaviour.OverviewCounts == application::review_overview_counts::all ? core::feedback_resolution_mode::all
																													: core::feedback_resolution_mode::unresolved);
			}
			apply_line_details();
			apply_markdown_images();
			apply_reading_width();
			ComparisonSurface->set_center_ribbon_width(Behaviour.CenterRibbonWidth);
			consider_mark_read_when_fully_seen();
			// A pull request that opened before the preferences landed takes
			// them now, once each.
			if (HasGithubPullRequest) {
				apply_first_comments_filter();
				if (!ReturnComparisonAsked) {
					ReturnComparisonPending = Behaviour.ReturnComparison
						!= application::review_return_comparison::whole_pr;
					honour_return_comparison();
				}
			}
		}
		if (aProjection.HomeView) {
			// [[BPR-0783]]. The window's own Home view, as the host holds it:
			// restored at load, and published again when a view was refused so
			// Home shows what was kept rather than what was asked for.
			adopt_home_view(*aProjection.HomeView);
		}
		if (aProjection.Geometry) {
			restore_geometry(*aProjection.Geometry);
		}
		// [[BPR-1295]]. The drawer that holds focus before it may be hidden.
		auto* const focusedDrawer = aProjection.Drawers ? drawer_holding_focus() : nullptr;
		if (aProjection.Drawers || aProjection.Layout || aProjection.Behaviour) {
			apply_drawer_layout();
		}
		keep_focus_out_of_hidden_drawers(focusedDrawer);
		if (aProjection.Drawers) {
			infer_modality_from_drawers();
		}
		Failure.reset();
		++Statistics.Publications;
		update_action_state();
		update_status_text();
		// [[BPR-0727]]. A refused gesture is explained in the status bar, where
		// the toggle's own status tip already speaks; the toggle stays enabled
		// and shows the state the core kept.
		if (aProjection.Refusal) {
			LastRefusal = *aProjection.Refusal;
			statusBar()->showMessage(drawer_reason_text(*aProjection.Refusal));
		}
		announce_update();
	}

	auto workbench_window::last_refusal() const noexcept -> core::workbench_reason_code
	{
		return LastRefusal;
	}

	namespace {
		/// \brief The drawer's row for one projected file.
		[[nodiscard]] auto drawer_row_for(const application::workbench_file_projection& aFile) -> workbench_drawer_row
		{
			auto identity = QString{ };
			identity.reserve(
				static_cast<qsizetype>(aFile.File.bytes().size()) * qsizetype{ 2 });
			for (const auto byte : aFile.File.bytes()) {
				identity.append(QString::number(byte, 16).rightJustified(2, QLatin1Char{ '0' }));
			}
			const auto path = QString::fromUtf8(aFile.Path.data(), static_cast<qsizetype>(aFile.Path.size()));
			return workbench_drawer_row{
				.Identity = std::move(identity),
				.Title    = path,
				.Detail   = path,
				.File     = aFile.File,
			};
		}
	} // namespace

	void workbench_window::apply_file_delta(const application::workbench_files_delta& aDelta)
	{
		// [[BPR-0972]]. The drawer list and the tree each take the few moved
		// files; neither receives the order again.
		auto inserted = std::vector<std::pair<std::size_t, workbench_drawer_row>>{ };
		auto rows     = std::vector<workbench_drawer_row>{ };
		inserted.reserve(aDelta.Inserted.size());
		rows.reserve(aDelta.Inserted.size());
		for (const auto& [position, file] : aDelta.Inserted) {
			inserted.emplace_back(position, drawer_row_for(file));
			rows.push_back(inserted.back().second);
		}
		ApplyingFileSelection = true;
		Drawers[0].Panel->contents()->apply_delta(aDelta.Removed, inserted);
		if (FilesTree != nullptr) {
			FilesTree->apply_delta(aDelta.Removed, rows);
			refresh_filter_notices();
		}
		FileRowsPresent       = Drawers[0].Panel->contents()->rowCount() > 0;
		ApplyingFileSelection = false;
		++Statistics.FileRowUpdates;
		apply_file_selection();
	}

	void workbench_window::apply_file_rows(
		const std::vector<application::workbench_file_projection>& aFiles)
	{
		OverviewFileFactsDirty = true;
		auto rows              = std::vector<workbench_drawer_row>{ };
		rows.reserve(aFiles.size());
		for (const auto& file : aFiles) {
			rows.push_back(drawer_row_for(file));
		}
		ApplyingFileSelection = true;
		Drawers[0].Panel->contents()->apply(rows);
		if (FilesTree != nullptr) {
			FilesTree->apply(rows);
			refresh_filter_notices();
		}
		FileRowsPresent       = !rows.empty();
		ApplyingFileSelection = false;
		++Statistics.FileRowUpdates;
		apply_file_selection();
	}

	auto workbench_window::conversation_anchor_file() const
		-> std::optional<core::file_identity>
	{
		// File selection survives the asynchronous gap between clearing the
		// previous diff and receiving the next one. Widening to the whole PR
		// in that gap rebuilds unrelated threads on every file switch.
		if (RefreshConversationAnchor.has_value()) {
			return RefreshConversationAnchor;
		}
		if (Cursor.has_value()) {
			return Cursor->File;
		}
		// Standalone diff presentation has no workbench cursor projection.
		if (ComparisonSurface != nullptr && ComparisonSurface->current_context()) {
			return ComparisonSurface->current_context()->Key.File;
		}
		return std::nullopt;
	}

	void workbench_window::apply_file_selection()
	{
		if (Drawers[0].Panel->contents() == nullptr
			|| Drawers[0].Panel->contents_view() == nullptr) {
			return;
		}
		const auto index      = Cursor && Cursor->File
			? Drawers[0].Panel->contents()->index_for_file(*Cursor->File)
			: QModelIndex{ };
		ApplyingFileSelection = true;
		Drawers[0].Panel->contents_view()->setCurrentIndex(index);
		if (FilesTree != nullptr) {
			FilesTree->select_file(Cursor ? Cursor->File : std::nullopt);
		}
		ApplyingFileSelection = false;
	}

	void workbench_window::dispatch_file_selection(const QModelIndex& aIndex)
	{
		if (ApplyingFileSelection || !Dispatch || Drawers[0].Panel->contents() == nullptr) {
			return;
		}
		const auto file = Drawers[0].Panel->contents()->file_at(aIndex);
		if (!file.has_value()) {
			return;
		}
		++Statistics.FileSelections;
		Dispatch(core::select_workbench_file_intent{ .File = *file });
	}

	void workbench_window::publish(
		const application::epoch_presentation_update& aUpdate)
	{
		// The composed control owns every review-history decision, so the update
		// reaches it unchanged and the shell only quotes the wording it produced.
		EpochSelectors->publish(aUpdate);
		honour_return_comparison();
		HasEpochState = true;
		++Statistics.EpochUpdates;
		// [[BPR-1130]]. The disclosure repeats the line's mark and statement.
		apply_updates_line();
		// [[BPR-1132]]. The tabs name their pairs by the line's dates.
		present_document_tabs();
		update_older_pair_notice();
		update_status_text();
		// Deliberately silent. The composed control already announced this exact
		// change, and a second announcement of the same fact is the duplicated
		// chatter a combined status update exists to prevent.
	}

	void workbench_window::fail(
		const application::workbench_failure_publication& aFailure)
	{
		// A failed effect changes no command legality. The host publishes a command
		// delta when legality actually changed, so the window reports the failure in
		// text and leaves enabled state exactly where the last projection put it.

		if (RefreshConversationAnchor.has_value() && RefreshConversationContext.has_value()
			&& aFailure.Context.Session == RefreshConversationContext->Session
			&& aFailure.Context.Generation == RefreshConversationContext->Generation) {
			RefreshConversationAnchor.reset();
			RefreshConversationContext.reset();
			apply_conversation_anchor();
		}
		Failure = aFailure.Failure;
		if (aFailure.Failure == application::workbench_host_failure::selected_file_missing_left
			|| aFailure.Failure == application::workbench_host_failure::selected_file_missing_right
			|| aFailure.Failure == application::workbench_host_failure::selected_file_missing_both
			// RFC-BPR-0089: a split file reads nothing, so the previous file's
			// diff must not stay on screen under its row.
			|| aFailure.Failure == application::workbench_host_failure::selected_file_split_by_commit_filter) {
			ComparisonSurface->clear(failure_text(aFailure.Failure));
		}
		++Statistics.Failures;
		update_status_text();
		announce_update();
	}

	void workbench_window::publish(
		const application::file_receipt_action_projection& aProjection)
	{
		Receipt = aProjection;
		if (ReviewEditor != nullptr) {
			ReviewEditor->present(aProjection.Receipt);
		}
		ReceiptFailure.reset();
		LedgerUnavailable = false;
		++Statistics.ReceiptUpdates;
		update_receipt_actions();
		consider_mark_read_when_fully_seen();
		update_status_text();
		announce_update();
	}

	void workbench_window::fail(
		const application::receipt_failure_publication& aFailure)
	{
		ReceiptFailure = aFailure;
		if (aFailure.Store == application::ledger_store_failure::unavailable) {
			LedgerUnavailable = true;
		}
		++Statistics.ReceiptFailures;
		update_receipt_actions();
		update_status_text();
		announce_update();
	}

	void workbench_window::clear_review_ledger(const bool aUnavailable)
	{
		Receipt.reset();
		ReceiptFailure.reset();
		LedgerUnavailable = aUnavailable;
		update_receipt_actions();
		update_status_text();
		announce_update();
	}

	void workbench_window::set_saved_layout_unavailable(const bool aUnavailable)
	{
		if (SavedLayoutUnavailable == aUnavailable) {
			return;
		}
		SavedLayoutUnavailable = aUnavailable;
		update_status_text();
		announce_update();
	}

	void workbench_window::set_agent_workspaces_unavailable(const bool aUnavailable)
	{
		if (AgentWorkspacesUnavailable == aUnavailable) {
			return;
		}
		AgentWorkspacesUnavailable = aUnavailable;
		update_status_text();
		announce_update();
	}

	void workbench_window::set_dispatch(workbench_intent_dispatch aDispatch)
	{
		Dispatch = std::move(aDispatch);
	}

	void workbench_window::set_change_dispatch(
		workbench_change_dispatch aChangeDispatch)
	{
		ChangeDispatch = std::move(aChangeDispatch);
	}

	void workbench_window::set_width_dispatch(
		workbench_drawer_width_dispatch aWidthDispatch)
	{
		WidthDispatch = std::move(aWidthDispatch);
	}

	void workbench_window::set_receipt_dispatch(
		workbench_receipt_dispatch aReceiptDispatch)
	{
		ReceiptDispatch = std::move(aReceiptDispatch);
	}

	void workbench_window::set_review_submission_dispatch(review_submission_editor_dispatch aDispatch) { ReviewSubmissionDispatch = std::move(aDispatch); }
	auto workbench_window::review_submission_editor_view() const noexcept -> review_submission_editor* { return ReviewSubmissionEditor; }

	void workbench_window::set_review_draft_dispatch(
		workbench_review_draft_dispatch aDraftDispatch)
	{
		ReviewDraftDispatch = std::move(aDraftDispatch);
	}

	void workbench_window::set_defect_dispatch(
		workbench_defect_dispatch aDefectDispatch)
	{
		DefectDispatch = std::move(aDefectDispatch);
	}

	void workbench_window::set_agent_dispatch(agent_intent_dispatch aAgentDispatch)
	{
		AgentPanel->set_dispatch(std::move(aAgentDispatch));
	}

	void workbench_window::set_device_authorization_dispatch(
		application::device_authorization_gesture_dispatch aDispatch)
	{
		DeviceDispatch = std::move(aDispatch);
	}

	void workbench_window::set_defect_classification_dispatch(
		workbench_defect_classification_dispatch aClassificationDispatch)
	{
		DefectClassificationDispatch = std::move(aClassificationDispatch);
		update_defect_surface();
	}

	void workbench_window::present_defects(
		std::vector<core::defect_history> aDefects, std::map<core::defect_id, std::string> aThreads)
	{
		DefectThreads = std::move(aThreads);
		std::ranges::sort(aDefects, { }, [](const core::defect_history& aDefect) {
			return aDefect.Defect.Id.value();
		});
		auto retained = std::optional<core::defect_id>{ };
		if (ShownDefectIndex.has_value() && *ShownDefectIndex < Defects.size()) {
			retained = Defects[*ShownDefectIndex].Defect.Id;
		}
		Defects = std::move(aDefects);
		ShownDefectIndex.reset();
		if (retained.has_value()) {
			const auto found = std::ranges::find(
				Defects, *retained, [](const core::defect_history& aHistory) {
					return aHistory.Defect.Id;
				});
			if (found != Defects.end()) {
				ShownDefectIndex = static_cast<std::size_t>(
					std::distance(Defects.begin(), found));
			}
		}
		if (!ShownDefectIndex.has_value() && !Defects.empty()) {
			ShownDefectIndex = 0U;
		}
		++Statistics.DefectUpdates;
		update_defect_badges();
		update_defect_surface();
	}

	void workbench_window::clear_defects()
	{
		Defects.clear();
		DefectThreads.clear();
		PendingDefectDialog.reset();
		DefectDialog->hide();
		ShownDefectIndex.reset();
		++Statistics.DefectUpdates;
		update_defect_badges();
		update_defect_surface();
	}

	void workbench_window::classify_selected_conversation()
	{
		if (!DefectClassificationDispatch) {
			return;
		}
		auto classification = selected_conversation_classification();
		if (!classification.has_value()) {
			return;
		}
		++Statistics.DefectClassifications;
		DefectClassificationDispatch(std::move(*classification));
	}

	void workbench_window::navigate_defect(const int aDirection)
	{
		if (!ShownDefectIndex.has_value() || Defects.empty()) {
			return;
		}
		const auto current = static_cast<std::ptrdiff_t>(*ShownDefectIndex);
		const auto next    = current + aDirection;
		if (next < 0 || next >= static_cast<std::ptrdiff_t>(Defects.size())) {
			return;
		}
		ShownDefectIndex = static_cast<std::size_t>(next);
		++Statistics.DefectNavigation;
		update_defect_surface();
		if (const auto thread = DefectThreads.find(Defects[*ShownDefectIndex].Defect.Id); thread != DefectThreads.end()) {
			const auto identity = thread->second;
			ConversationPanel->set_anchor_line(std::nullopt);
			ConversationPanel->set_anchor_file(std::nullopt);
			auto index = ConversationPanel->model()->index_for_id(identity);
			if (!index.isValid()) {
				// [[BPR-1293]]. A defect's thread on a file opens in Review,
				// in place; one that is not on a file still goes to Discuss.
				if (reveal_conversation_in_review(identity)) {
					return;
				}
				choose_modality(review_modality::discuss);
				index = ConversationPanel->model()->index_for_id(identity);
			}
			if (index.isValid()) {
				ConversationPanel->choose(index);
			}
			ConversationPanel->cards()->set_current_thread(identity);
		}
	}

	auto workbench_window::chosen_conversation() const -> QModelIndex
	{
		// [[BPR-0985]]. The panel's default row shows a detail without moving
		// code. Revealing it again after every published comparison moved the
		// reader to the first thread of each file, and as a reading gesture it
		// superseded the saved reading a restart was restoring.
		return ConversationPanel->current_is_default() ? QModelIndex{ } : ConversationPanel->view()->currentIndex();
	}

	void workbench_window::synchronize_selected_conversation(
		const QModelIndex& aIndex)
	{
		// A PR-wide selection reads discussion. Only Open in code may move
		// the retained comparison; an off-file row must not reveal a same-number line.
		if (ComparisonSurface == nullptr
			|| (Modality != nullptr && (Modality->current() == review_modality::discuss || Modality->current() == review_modality::threads))) {
			return;
		}
		auto root = aIndex;
		while (root.parent().isValid()) {
			root = root.parent();
		}
		if (!root.isValid()
			|| root.data(github_conversation_model::anchor_availability_role).toInt()
				!= static_cast<int>(
					application::github_conversation_anchor_availability::current)) {
			ComparisonSurface->clear_conversation_anchor();
			return;
		}
		const auto context = ComparisonSurface->current_context();
		const auto path    = root.data(
									 github_conversation_model::path_role)
								 .toString();
		if (!context.has_value() || path.isEmpty()
			|| context->Key.File != core::file_identity_of(path.toStdString())) {
			ComparisonSurface->clear_conversation_anchor();
			return;
		}
		bool       validLine = false;
		const auto oneBased  = root.data(github_conversation_model::line_role)
								   .toULongLong(&validLine);
		const auto sideValue = root.data(github_conversation_model::diff_side_role);
		if (!validLine || oneBased == 0U
			|| oneBased > std::numeric_limits<std::uint32_t>::max()
			|| !sideValue.isValid()) {
			ComparisonSurface->clear_conversation_anchor();
			return;
		}
		const auto side = github_conversation_source_side(sideValue.toInt());
		if (!side.has_value()) {
			ComparisonSurface->clear_conversation_anchor();
			ConversationPanel->model()->set_anchor_availability(
				root,
				application::github_conversation_anchor_availability::missing_side);
			return;
		}
		const auto semantic = ConversationPanel->model()->semantic_anchor(root);
		if (semantic && semantic->Comparison != core::comparison_digest(context->Key)) {
			ComparisonSurface->clear_conversation_anchor();
			return;
		}
		const auto endpointIsCurrent = *side == core::diff_side::left
			? LeftConversationEndpointCurrent
			: RightConversationEndpointCurrent;
		if (!semantic && !endpointIsCurrent) {
			ComparisonSurface->clear_conversation_anchor();
			return;
		}
		if (!ComparisonSurface->reveal_conversation_line(
				*side, static_cast<std::uint32_t>(oneBased - 1U))) {
			ComparisonSurface->clear_conversation_anchor();
			ConversationPanel->model()->set_anchor_availability(
				root,
				application::github_conversation_anchor_availability::selected_line_out_of_bounds);
		}
	}

	void workbench_window::update_defect_badges()
	{
		auto badges = conversation_cards_view::defect_badges{ };
		for (const auto& history : Defects) {
			const auto thread = DefectThreads.find(history.Defect.Id);
			if (thread == DefectThreads.end()) {
				continue;
			}
			const auto label   = tr("%1 %2 · %3").arg(badge_for(history.Defect.State), QString::fromUtf8(core::name(history.Defect.Severity)), QString::fromUtf8(core::name(history.Defect.Category)));
			const auto context = core::context_available(history) ? tr("Shared context available at the retained semantic anchor.") : tr("Shared context unavailable; the original semantic anchor is retained.");
			badges.emplace(thread->second, std::pair{ label, explanation_for(history.Defect.State) + QLatin1Char{ ' ' } + context });
		}
		ConversationPanel->cards()->set_defect_badges(std::move(badges));
	}

	void workbench_window::show_defect_dialog(core::defect_id aDefect)
	{
		const auto at = std::ranges::find(Defects, aDefect, [](const auto& aHistory) { return aHistory.Defect.Id; });
		if (at == Defects.end()) {
			return;
		}
		ShownDefectIndex = static_cast<std::size_t>(std::distance(Defects.begin(), at));
		update_defect_surface();
		DefectDialog->show();
		DefectDialog->raise();
		DefectCard->severity_selector()->setFocus(Qt::OtherFocusReason);
	}

	void workbench_window::append_thread_defect_menu(QMenu& aMenu, const std::string& aThread)
	{
		auto selected = ConversationPanel->model()->index_for_id(aThread);
		if (!selected.isValid()) {
			ConversationPanel->set_anchor_line(std::nullopt);
			ConversationPanel->set_anchor_file(std::nullopt);
			selected = ConversationPanel->model()->index_for_id(aThread);
		}
		if (selected.isValid()) {
			ConversationPanel->choose(selected);
		}
		const auto binding = std::ranges::find_if(DefectThreads, [&aThread](const auto& aEntry) { return aEntry.second == aThread; });
		const auto history = binding == DefectThreads.end() ? Defects.end() : std::ranges::find(Defects, binding->first, [](const auto& aEntry) { return aEntry.Defect.Id; });
		if (history == Defects.end()) {
			auto* action = aMenu.addAction(tr("Classify as defect…"));
			action->setObjectName(QStringLiteral("threadClassifyDefect"));
			const auto classification = selected_conversation_classification();
			const auto offered        = classification && classification->Conversation == aThread && static_cast<bool>(DefectClassificationDispatch);
			action->setEnabled(offered);
			action->setToolTip(offered ? tr("Classify this thread locally, then choose its severity and category.") : tr("Open this thread's current code comparison before classifying it."));
			connect(action, &QAction::triggered, &aMenu, [this, classification, aThread] {
				if (classification && DefectClassificationDispatch) {
					PendingDefectDialog = aThread;
					++Statistics.DefectClassifications;
					DefectClassificationDispatch(*classification);
				}
			});
			return;
		}
		// The menu owns an exact history snapshot. A later selection cannot
		// retarget an action already offered for this thread.
		auto* snapshot = new defect_card_view{ [this](core::defect_command aCommand) {
												  ++Statistics.DefectIntents;
												  if (DefectDispatch) {
													  DefectDispatch(std::move(aCommand));
												  }
											  },
											   &aMenu };
		snapshot->present(*history);
		snapshot->hide();
		for (auto* action : { snapshot->verify_action(), snapshot->reopen_action(), snapshot->withdraw_action(), snapshot->accept_as_is_action() }) {
			aMenu.addAction(action);
		}
		auto* reclassify = aMenu.addAction(tr("Reclassify…"));
		reclassify->setObjectName(QStringLiteral("threadReclassifyDefect"));
		reclassify->setEnabled(snapshot->reclassify_action()->isEnabled());
		reclassify->setToolTip(snapshot->reclassify_action()->toolTip());
		connect(reclassify, &QAction::triggered, &aMenu, [this, id = history->Defect.Id] { show_defect_dialog(id); });
	}

	void workbench_window::update_defect_surface()
	{
		ClassifyDefect->setEnabled(
			static_cast<bool>(DefectClassificationDispatch)
			&& selected_conversation_classification().has_value());
		const auto validIndex = ShownDefectIndex.has_value()
			&& *ShownDefectIndex < Defects.size();
		PreviousDefect->setEnabled(validIndex && *ShownDefectIndex > 0U);
		NextDefect->setEnabled(
			validIndex && *ShownDefectIndex + 1U < Defects.size());
		if (DefectRow != nullptr) {
			DefectRow->setVisible(!Defects.empty());
		}
		DefectCountLabel->setText(
			validIndex
				? tr("Defects %1/%2")
					  .arg(*ShownDefectIndex + 1U)
					  .arg(Defects.size())
				: tr("Defects 0/0"));
		if (validIndex) {
			DefectCard->present(Defects[*ShownDefectIndex]);
		} else {
			DefectCard->setVisible(false);
			return;
		}
		DefectCard->setVisible(true);
		if (PendingDefectDialog) {
			const auto thread = std::ranges::find_if(DefectThreads, [this](const auto& aEntry) { return aEntry.second == *PendingDefectDialog; });
			if (thread != DefectThreads.end()) {
				const auto id = thread->first;
				PendingDefectDialog.reset();
				show_defect_dialog(id);
			}
		}
	}

	auto workbench_window::selected_conversation_classification() const
		-> std::optional<workbench_defect_classification>
	{
		if (ConversationPanel == nullptr || ConversationPanel->view() == nullptr
			|| ConversationPanel->model() == nullptr || ComparisonSurface == nullptr
			|| ComparisonSurface->current_result() == nullptr) {
			return std::nullopt;
		}
		const auto current = ConversationPanel->view()->currentIndex();
		if (!current.isValid() || current.parent().isValid()
			|| current.data(github_conversation_model::kind_role).toInt()
				!= static_cast<int>(
					application::github_conversation_kind::review_thread)
			|| current
					.data(github_conversation_model::anchor_availability_role)
					.toInt()
				!= static_cast<int>(
					application::github_conversation_anchor_availability::current)) {
			return std::nullopt;
		}
		const auto context = ComparisonSurface->current_context();
		if (!context || context->Key.File != core::file_identity_of(current.data(github_conversation_model::path_role).toString().toStdString())) {
			return std::nullopt;
		}
		bool       validLine = false;
		const auto oneBased  = current.data(github_conversation_model::line_role)
								   .toULongLong(&validLine);
		const auto sideValue = current.data(github_conversation_model::diff_side_role);
		if (!validLine || oneBased == 0U
			|| oneBased > std::numeric_limits<std::uint32_t>::max()
			|| !sideValue.isValid()) {
			return std::nullopt;
		}
		const auto projectedSide = sideValue.toInt();
		if (projectedSide
				!= static_cast<int>(
					application::github_conversation_diff_side::left)
			&& projectedSide
				!= static_cast<int>(
					application::github_conversation_diff_side::right)) {
			return std::nullopt;
		}
		const auto  side              = projectedSide
				== static_cast<int>(
							  application::github_conversation_diff_side::left)
			? core::diff_side::left
			: core::diff_side::right;
		const auto  endpointIsCurrent = side == core::diff_side::left
			? LeftConversationEndpointCurrent
			: RightConversationEndpointCurrent;
		const auto& alignment         = ComparisonSurface->current_result()->Alignment;
		const auto  line              = static_cast<std::uint32_t>(oneBased - 1U);
		const auto  lineCount         = side == core::diff_side::left
			? alignment.LeftLineCount
			: alignment.RightLineCount;
		const auto  identity          = ConversationPanel->model()->id_for_index(current);
		if (!endpointIsCurrent || line >= lineCount || identity.empty()) {
			return std::nullopt;
		}
		return workbench_defect_classification{
			.Conversation = identity,
			.Anchor       = {
				.Comparison = alignment.Digest,
				.Side       = side,
				.Line       = line,
			},
		};
	}

	auto workbench_window::primary_toolbar() const noexcept -> QToolBar*
	{
		return PrimaryToolbar;
	}

	auto workbench_window::modality() const noexcept -> modality_control*
	{
		return Modality;
	}

	auto workbench_window::current_modality() const noexcept -> review_modality
	{
		return Modality != nullptr ? Modality->current() : review_modality::review;
	}

	auto workbench_window::toggle_drawers_action() const noexcept -> QAction*
	{
		return ToggleDrawersAction;
	}

	void workbench_window::publish(const application::commit_filter_projection& aProjection)
	{
		if (OverviewPage != nullptr) {
			OverviewPage->present_commit_filter(aProjection);
		}
		if (FilesCommitChip != nullptr) {
			FilesCommitChip->present(aProjection);
		}
		if (FilesGeneratedRow != nullptr) {
			FilesGeneratedRow->present(aProjection);
		}
		FilterProjection = aProjection;
	}

	void workbench_window::set_file_filter_dispatch(std::function<void(const std::vector<std::string>&)> aDispatch)
	{
		if (OverviewPage != nullptr) {
			OverviewPage->set_file_filter_dispatch(std::move(aDispatch));
		}
	}

	void workbench_window::set_generated_dispatch(std::function<void(const application::generated_file_gesture&)> aDispatch)
	{
		GeneratedDispatch = aDispatch;
		if (OverviewPage != nullptr) {
			OverviewPage->set_generated_dispatch(std::move(aDispatch));
		}
	}

	void workbench_window::set_generated_open_dispatch(std::function<void(const std::string&)> aDispatch)
	{
		if (FilesGeneratedRow != nullptr) {
			FilesGeneratedRow->set_open_dispatch(aDispatch);
		}
		if (OverviewPage != nullptr && OverviewPage->generated_row() != nullptr) {
			// Review first, then the file, as a pick in the Overview's tree does.
			OverviewPage->generated_row()->set_open_dispatch([this, dispatch = std::move(aDispatch)](const std::string& aPath) {
				if (!dispatch) {
					return;
				}
				ReviewEnteredFor = overview_review_key();
				choose_modality(review_modality::review);
				dispatch(aPath);
			});
		}
	}

	auto workbench_window::files_generated_row() const noexcept -> generated_files_row*
	{
		return FilesGeneratedRow;
	}

	void workbench_window::set_commit_filter_dispatch(std::function<void(const application::commit_filter_gesture&)> aDispatch)
	{
		CommitFilterDispatch = aDispatch;
		if (OverviewPage != nullptr) {
			OverviewPage->set_commit_filter_dispatch(std::move(aDispatch));
		}
	}

	void workbench_window::set_commit_filter_shown_dispatch(std::function<void()> aDispatch)
	{
		if (OverviewPage != nullptr) {
			OverviewPage->set_commit_filter_shown_dispatch(std::move(aDispatch));
		}
	}

	void workbench_window::set_commit_open_dispatch(std::function<void(const std::string&)> aDispatch)
	{
		if (OverviewPage != nullptr) {
			OverviewPage->set_commit_open_dispatch(std::move(aDispatch));
		}
	}

	void workbench_window::set_commit_rematch_dispatch(std::function<void(const application::commit_filter_rematch_gesture&)> aDispatch)
	{
		if (FilesCommitChip != nullptr) {
			FilesCommitChip->set_rematch_dispatch(aDispatch);
		}
		if (OverviewPage != nullptr) {
			if (OverviewPage->commit_chip() != nullptr) {
				OverviewPage->commit_chip()->set_rematch_dispatch(aDispatch);
			}
			if (OverviewPage->status_drawer() != nullptr && OverviewPage->status_drawer()->commits() != nullptr) {
				OverviewPage->status_drawer()->commits()->set_rematch_dispatch(std::move(aDispatch));
			}
		}
	}

	void workbench_window::present_split_diff(std::optional<application::split_diff_projection> aProjection)
	{
		if (SplitStrip != nullptr) {
			SplitStrip->present(aProjection);
		}
	}

	void workbench_window::set_split_segment_dispatch(std::function<void(std::size_t)> aDispatch)
	{
		if (SplitStrip != nullptr) {
			SplitStrip->set_dispatch(std::move(aDispatch));
		}
	}

	auto workbench_window::split_strip() const noexcept -> split_diff_strip*
	{
		return SplitStrip;
	}

	void workbench_window::show_commits()
	{
		if (OverviewPage == nullptr) {
			return;
		}
		if (Modality == nullptr || Modality->current() != review_modality::overview) {
			choose_modality(review_modality::overview, core::workbench_modality_entry::enter);
		}
		OverviewPage->status_drawer()->show_commits();
	}

	auto workbench_window::files_commit_chip() const noexcept -> commit_filter_chip*
	{
		return FilesCommitChip;
	}

	void workbench_window::choose_modality(const review_modality aModality, const core::workbench_modality_entry aEntry)
	{
		if (ReviewNavigationDispatch) {
			ReviewNavigationDispatch(std::nullopt);
		}
		CompositionPage = nullptr;
		// [[BPR-1588]]. The owner's decision D1 (RFC-BPR-0085): with Start
		// review gone, the first entry into Review from the Overview of a pull
		// request this window has not reviewed yet lands where the button did,
		// at the first unseen change's file. Later entries keep the file.
		const auto reviewKey = overview_review_key();
		// Only the reviewer's own choice (Command-2 or the Review segment); a
		// link, Resume or a tour enters with its own target.
		const auto firstReview = aModality == review_modality::review && aEntry == core::workbench_modality_entry::choose && Modality != nullptr
			&& Modality->current() == review_modality::overview
			&& !reviewKey.isEmpty() && ReviewEnteredFor != reviewKey;
		// Only on a pull request the reviewer has not started (nothing read, no
		// earlier review), where Start review stood; once started, a restored
		// reading place is never replaced.
		const auto notStarted = OverviewPage != nullptr && OverviewPage->projection().has_value()
			&& OverviewPage->projection()->Entry == application::overview_entry_kind::start_review;
		const auto entryPath = firstReview && notStarted ? OverviewPage->entry_path() : std::nullopt;
		if (aModality == review_modality::review && !reviewKey.isEmpty()) {
			ReviewEnteredFor = reviewKey;
		}
		// [[BPR-1293]]. Threads goes back where it was entered from.
		if (Modality != nullptr && aModality == review_modality::threads && Modality->current() != review_modality::threads) {
			ThreadsReturn      = Modality->current();
			ThreadsReturnFocus = Finder != nullptr && Finder->isAncestorOf(QApplication::focusWidget()) ? nullptr : QApplication::focusWidget();
		}
		if (Modality != nullptr) {
			Modality->adopt(aModality);
		}
		// [[BPR-0661]]. "Last used" is the modality chosen while a pull
		// request was open.
		if (HasGithubPullRequest) {
			LastUsedModality = aModality;
		}
		// [[BPR-1599]]. A choice made while the pull request loads is the
		// reviewer's, and its arrival keeps it.
		if (!EnteringOpeningModality && !HasGithubPullRequest && SessionPanel != nullptr
			&& SessionPanel->status() == core::github_status::loading_pull_request) {
			LoadingChoice = aModality;
		}
		ModalityChosen = true;
		apply_modality_surface();
		if (HomeRequested) {
			HomeRequested = false;
			update_home_landing();
		}
		// The core decides every drawer, as it decides a toggle. [[BPR-0675]]
		// made those decisions reachable without a comparison, which is when
		// a preset matters most.
		if (!Dispatch) {
			return;
		}
		// [[BPR-0727]]. The core retains the modality first, because Overview
		// carries a rule: while it is chosen, a drawer opened by hand is
		// refused, and choosing it collapses every drawer in the same
		// decision ([[BPR-0674]]). [[BPR-1295]]: Review's arrangement is the
		// core's too. It keeps the drawers Review last had in this window,
		// applies Review's preset on the window's first entry, and applies it
		// again when Review is chosen while it shows.
		Dispatch(core::set_workbench_modality_intent{
			.Modality = [aModality] {
				switch (aModality) {
				case review_modality::overview:
					return core::workbench_modality::overview;
				case review_modality::review:
					return core::workbench_modality::review;
				case review_modality::discuss:
					return core::workbench_modality::discuss;
				case review_modality::threads:
					return core::workbench_modality::threads;
				}
				return core::workbench_modality::review;
			}(),
			.Entry = aEntry,
		});
		if (entryPath) {
			if (const auto file = file_for_path(*entryPath); file.has_value()) {
				Dispatch(core::select_workbench_file_intent{ .File = *file });
			}
		}
		if (aModality != review_modality::discuss) {
			return;
		}
		// Discuss's preset stays two exact intents: the files drawer gives
		// the conversation the room, and the conversations panel, which
		// Discuss borrows as its page, is open. Repositories stay as they
		// were.
		Dispatch(core::set_workbench_drawer_intent{
			.Drawer = core::workbench_drawer::files,
			.State  = core::workbench_drawer_state::collapsed,
		});
		Dispatch(core::set_workbench_drawer_intent{
			.Drawer = core::workbench_drawer::conversations,
			.State  = core::workbench_drawer_state::expanded,
		});
	}

	void workbench_window::enter_opening_modality(const review_modality aModality)
	{
		// [[BPR-1599]]. Pressing Review while a pull request loads used to be
		// undone when it arrived: the arrival entered Overview over it, and
		// the reviewer had to choose Review again.
		const auto chosen       = LoadingChoice;
		EnteringOpeningModality = true;
		choose_modality(chosen.value_or(aModality));
		EnteringOpeningModality = false;
	}

	auto workbench_window::home_landing() const noexcept -> workbench_landing*
	{
		return HomeLanding;
	}

	auto workbench_window::dif() const noexcept -> dif_widget*
	{
		return Dif;
	}

	auto workbench_window::ai_tools_action() const noexcept -> QAction*
	{
		return AiToolsAction;
	}

	auto workbench_window::dif_workshop_slot() const noexcept -> QWidget*
	{
		return DifWorkshopSlot;
	}

	void workbench_window::adopt_dif(const core::dif_resolution aResolution)
	{
		DifResolution      = aResolution;
		DifResolutionReady = true;
		update_dif_presentation();
		apply_pending_appearance();
	}

	void workbench_window::observe_dif_context(
		const core::application_session_id aSession,
		core::pull_request_authority       aAuthority,
		core::agent_revision_binding       aBinding)
	{
		if (DifCues->observe(
				{ aSession, std::move(aAuthority), std::move(aBinding) })
			&& Dif != nullptr) {
			Dif->reset_contextual();
		}
	}

	void workbench_window::clear_dif_context()
	{
		DifCues->clear();
		if (Dif != nullptr) {
			Dif->reset_contextual();
		}
	}

	void workbench_window::play_dif_cue(std::optional<dif_clip_id> aCue)
	{
		if (aCue.has_value() && Dif != nullptr) {
			static_cast<void>(Dif->play(*aCue));
		}
	}

	void workbench_window::admit_dif_invocation(
		const application::agent_invocation_projection_identity& aIdentity)
	{
		static_cast<void>(DifCues->admit(aIdentity));
	}

	void workbench_window::start_dif_invocation(
		const application::agent_invocation_projection_identity& aIdentity)
	{
		play_dif_cue(DifCues->start(aIdentity, DifCueClock->elapsed()));
	}

	void workbench_window::present_dif_agent_delivery(
		const application::agent_invocation_projection_identity& aIdentity,
		const application::agent_finding_delivery&               aDelivery,
		const bool                                               aReplyPresented,
		const bool                                               aFindingsPresented)
	{
		DifCues->deliver(
			aIdentity, aDelivery, aReplyPresented, aFindingsPresented);
	}

	void workbench_window::finish_dif_invocation(
		const application::agent_invocation_projection_identity& aIdentity,
		const application::agent_run_outcome                     aOutcome)
	{
		play_dif_cue(DifCues->finish(aIdentity, aOutcome, DifCueClock->elapsed()));
	}

	void workbench_window::present_dif_workshop(
		const core::agent_session&  aSession,
		const std::uint64_t         aActionGeneration,
		const core::review_draft_id aDraft,
		const std::uint64_t         aRevision,
		const std::string_view      aProposed)
	{
		play_dif_cue(DifCues->present_workshop(
			aSession,
			aActionGeneration,
			aDraft,
			aRevision,
			aProposed,
			DifCueClock->elapsed()));
	}

	void workbench_window::begin_dif_draft(
		const core::revisioned_review_draft_command& aCommand)
	{
		DifCues->begin_draft(aCommand);
	}

	void workbench_window::accept_dif_draft(
		const core::revisioned_review_draft_command& aCommand,
		const core::revisioned_review_draft&         aAccepted)
	{
		play_dif_cue(DifCues->accept_draft(
			aCommand, aAccepted, DifCueClock->elapsed()));
	}

	void workbench_window::cancel_dif_draft(
		const core::revisioned_review_draft_command& aCommand)
	{
		DifCues->cancel_draft(aCommand);
	}

	void workbench_window::present_dif_continuation(
		const core::agent_session&            aSession,
		const core::agent_continuation_offer& aOffer)
	{
		play_dif_cue(DifCues->present_continuation(
			aSession, aOffer, DifCueClock->elapsed()));
	}

	void workbench_window::publish(const application::overview_history_update& aUpdate)
	{
		if (OverviewPage == nullptr || !PresentedOverview) {
			return;
		}
		auto& since      = PresentedOverview->SinceLastReview;
		since.Known      = aUpdate.Known;
		since.ReviewedAt = aUpdate.ReviewedAt;
		since.Uploads    = aUpdate.Uploads;
		OverviewPage->apply_history(aUpdate);
	}

	void workbench_window::publish(const application::overview_receipt_update& aUpdate)
	{
		if (OverviewPage == nullptr || !PresentedOverview.has_value()) {
			return;
		}
		// The retained overview takes the same rows, so an identical complete
		// publication afterwards is still recognised as unchanged.
		for (const auto& row : aUpdate.Rows) {
			if (const auto found = PresentedOverviewRows.find(row.Path); found != PresentedOverviewRows.end()) {
				PresentedOverview->Files[found->second] = row;
			}
		}
		PresentedOverview->Progress  = aUpdate.Progress;
		PresentedOverview->Entry     = aUpdate.Entry;
		PresentedOverview->EntryPath = aUpdate.EntryPath;
		OverviewPage->apply_receipts(aUpdate);
		if (FilesTree != nullptr && !aUpdate.Rows.empty()) {
			ApplyingFileSelection = true;
			FilesTree->apply_overview_rows(aUpdate.Rows);
			ApplyingFileSelection = false;
		}
	}

	void workbench_window::publish(
		const application::pull_request_overview_projection& aProjection)
	{
		if (OverviewPage == nullptr) {
			return;
		}
		const auto unchanged = PresentedOverview.has_value() && *PresentedOverview == aProjection;
		if (!unchanged) {
			OverviewPage->present(aProjection);
			present_new_threads();
			PresentedOverview = aProjection;
			PresentedOverviewRows.clear();
			PresentedOverviewRows.reserve(aProjection.Files.size());
			for (std::size_t row = 0U; row < aProjection.Files.size(); ++row) {
				PresentedOverviewRows.emplace(aProjection.Files[row].Path, row);
			}
		}
		if (FilesTree != nullptr && (!unchanged || OverviewFileFactsDirty)) {
			// [[BPR-0658]]. The tree draws the overview's per-file facts.
			ApplyingFileSelection = true;
			// [[BPR-0668]]. A comparison that could not be prepared leaves the
			// workbench with no file rows, and the drawer was empty beside a
			// pull request that plainly has files. The pull request's own file
			// set is enough to list them; opening one still needs the
			// comparison, and the status line says so.
			if (!FileRowsPresent) {
				auto rows = std::vector<workbench_drawer_row>{ };
				rows.reserve(aProjection.Files.size());
				for (const auto& file : aProjection.Files) {
					const auto path = QString::fromStdString(file.Path);
					rows.push_back(workbench_drawer_row{
						.Identity = path,
						.Title    = path,
						.Detail   = path,
						.File     = core::file_identity_of(file.Path),
					});
				}
				FilesTree->apply(rows);
			}
			FilesTree->apply_overview(aProjection.Files);
			refresh_filter_notices();
			OverviewFileFactsDirty = false;
			ApplyingFileSelection  = false;
		}
		apply_modality_surface();
		update_pending_panes();
	}

	void workbench_window::publish(const application::overview_feedback_projection& aFeedback)
	{
		if (OverviewPage != nullptr) {
			OverviewPage->present_feedback(aFeedback);
		}
	}

	void workbench_window::set_overview_feedback_dispatch(std::function<void(const application::overview_feedback_request&)> aDispatch)
	{
		if (OverviewPage != nullptr) {
			OverviewPage->set_feedback_dispatch(std::move(aDispatch));
		}
	}

	void workbench_window::set_overview_drill_down_dispatch(std::function<void(const application::overview_drill_down&)> aDispatch)
	{
		if (OverviewPage != nullptr) {
			OverviewPage->set_drill_down_dispatch(std::move(aDispatch));
		}
	}

	void workbench_window::list_conversations(const application::overview_drill_down_listing& aListing, const bool aReveal)
	{
		if (ConversationPanel == nullptr || Modality == nullptr) {
			return;
		}
		// The caption says which count it was: the label, the mode, where.
		const auto  anyColumns = Behaviour.LabelColumns.Count != 0U;
		const auto& down       = aListing.DrillDown;
		auto        where      = QString{ };
		switch (down.Row) {
		case application::overview_feedback_row_kind::file:
		case application::overview_feedback_row_kind::folder:
			where = down.Path.empty() ? QStringLiteral("/") : QString::fromStdString(down.Path);
			break;
		case application::overview_feedback_row_kind::remainder:
			where = QStringLiteral("Not on a file");
			break;
		case application::overview_feedback_row_kind::pull_request:
			where = QStringLiteral("Pull request");
			break;
		}
		auto parts = QStringList{ overview_count_name(down.Target, anyColumns) };
		if (down.Target.Kind != core::feedback_count_kind::uncertain) {
			parts.push_back(aListing.Mode == core::feedback_resolution_mode::unresolved ? QStringLiteral("open") : QStringLiteral("all"));
		}
		parts.push_back(where);
		const auto count       = static_cast<qulonglong>(aListing.Count);
		const auto caption     = QStringLiteral("%1 — %2 %3").arg(parts.join(QStringLiteral(" · ")), QString::number(count), count == 1U ? QStringLiteral("thread") : QStringLiteral("threads"));
		const auto description = QStringLiteral("Listed from the Overview: exactly the threads this count counted. Show every thread clears it.");
		if (aReveal) {
			if (OverviewPage != nullptr && OverviewPage->isAncestorOf(QApplication::focusWidget())) {
				OverviewReturnFocus = QApplication::focusWidget();
			}
			choose_home(false);
			choose_modality(review_modality::discuss);
		}
		ConversationPanel->list_conversations(aListing.Conversations, caption, description,
											  OverviewPage != nullptr ? OverviewPage->count_mark(down.Target, anyColumns) : QPixmap{ });
		if (aReveal) {
			ConversationPanel->cards()->setFocus(Qt::OtherFocusReason);
			QAccessibleAnnouncementEvent event{ ConversationPanel, QStringLiteral("Discuss lists %1").arg(caption) };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		}
	}

	auto workbench_window::conversation_listing_active() const -> bool
	{
		return ConversationPanel != nullptr && ConversationPanel->listing().has_value();
	}

	void workbench_window::clear_overview()
	{
		PresentedOverview.reset();
		PresentedOverviewRows.clear();
		OverviewFileFactsDirty = true;
		if (OverviewPage == nullptr) {
			return;
		}
		OverviewPage->clear();
		if (FilesTree != nullptr) {
			ApplyingFileSelection = true;
			FilesTree->apply_overview({ });
			ApplyingFileSelection = false;
		}
		apply_modality_surface();
	}

	auto workbench_window::overview_page() const noexcept -> pull_request_overview_page*
	{
		return OverviewPage;
	}

	auto workbench_window::showing_overview() const -> bool
	{
		return CentreStack != nullptr && CentreStack->currentWidget() == OverviewPage;
	}

	void workbench_window::apply_conversation_anchor()
	{
		if (ConversationAnchorTimer != nullptr) {
			ConversationAnchorTimer->stop();
		}
		if (ConversationPanel == nullptr) {
			return;
		}
		ApplyingConversationAnchor = true;
		ConversationPanel->set_anchor_file(conversation_anchor_file());
		ConversationPanel->set_anchor_line(std::nullopt);
		ApplyingConversationAnchor = false;
		refresh_cards();
	}

	void workbench_window::schedule_conversation_anchor()
	{
		// A zero-interval timer rather than a posted call: an overdue timer
		// fires before one registered after it, so input already waiting is
		// served before the drawer re-projects. A later publication that moves
		// the anchor applies it at once, which leaves this turn nothing to do.
		if (ConversationAnchorTimer == nullptr) {
			ConversationAnchorTimer = new QTimer{ this };
			ConversationAnchorTimer->setSingleShot(true);
			ConversationAnchorTimer->setInterval(0);
			connect(ConversationAnchorTimer, &QTimer::timeout, this, [this] {
				if (ConversationPanel != nullptr && ConversationPanel->anchor_file() != conversation_anchor_file()) {
					apply_conversation_anchor();
				}
			});
		}
		ConversationAnchorTimer->start();
	}

	void workbench_window::refresh_cards()
	{
		update_older_pair_notice();
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return;
		}
		auto* const cards = ConversationPanel->cards();
		cards->set_draft(
			ReviewEditor != nullptr ? ReviewEditor->shown_draft() : std::nullopt,
			ComparisonSurface != nullptr && ComparisonSurface->current_result() != nullptr
				? std::optional{ ComparisonSurface->current_result()->Alignment.Digest }
				: std::nullopt);
		cards->refresh_geometry();
		apply_thread_marks();
		// [[BPR-1293]]. My draft is a row of Threads, with its lock.
		if (Finder != nullptr) {
			auto draft = std::optional<finder_draft>{ };
			if (const auto& shown = ReviewEditor != nullptr ? ReviewEditor->shown_draft() : std::optional<core::review_comment_draft>{ };
				shown.has_value() && !shown->ReplyToThread.has_value() && !shown->shared() && !shown->Body.empty() && ComparisonSurface != nullptr
				&& ComparisonSurface->current_result() != nullptr && ComparisonSurface->current_context().has_value()
				&& ComparisonSurface->current_result()->Alignment.Digest == shown->Anchor.Comparison) {
				draft = finder_draft{
					.File = ComparisonSurface->current_context()->Key.File,
					.Line = shown->Anchor.Line + 1U,
					.Body = QString::fromStdString(shown->Body),
				};
			}
			Finder->set_draft(std::move(draft));
		}
		resume_conversation_pick();
		resume_pending_comment_pick();
		// [[BPR-1592]]. The notes follow the code as the threads do.
		if (PrivateNotes != nullptr) {
			if (PrivateNotes->isVisible()) {
				PrivateNotes->refresh_geometry();
			}
			resume_private_note_open();
		}
	}

	void workbench_window::present_saved_drafts(std::vector<core::review_comment_draft> aDrafts)
	{
		present_saved_drafts(std::move(aDrafts), { });
	}

	void workbench_window::present_saved_drafts(std::vector<core::review_comment_draft> aDrafts, std::vector<core::reply_draft_decision> aReplies)
	{
		// [[BPR-1345]]. The review in progress is what the saved drafts say:
		// each pending in it, or held while it is submitted.
		const auto pending    = static_cast<std::size_t>(std::ranges::count_if(aDrafts, [](const core::review_comment_draft& aDraft) { return core::pending_in_review(aDraft); }));
		const auto held       = static_cast<std::size_t>(std::ranges::count_if(aDrafts, [](const core::review_comment_draft& aDraft) { return core::held_by_review(aDraft); }));
		const auto changed    = pending != PendingReviewComments || held != HeldReviewComments;
		PendingReviewComments = pending;
		HeldReviewComments    = held;
		update_review_chip();
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return;
		}
		ConversationPanel->cards()->set_saved_drafts(std::move(aDrafts), std::move(aReplies));
		apply_thread_marks();
		if (changed && ConversationPanel->cards()->writing_draft()) {
			present_inline_draft();
		}
	}

	void workbench_window::set_thread_marks_suspended(const bool aSuspended)
	{
		if (ThreadMarksSuspended == aSuspended) {
			return;
		}
		ThreadMarksSuspended = aSuspended;
		apply_thread_marks();
	}

	void workbench_window::apply_thread_marks()
	{
		if (ComparisonSurface == nullptr || ConversationPanel == nullptr) {
			return;
		}
		// Amber for a thread on the line; blue for a thread with a post newer
		// than what the reader has read ([[BPR-0656]]). [[BPR-1335]]: the
		// thread's line number is tinted, and the open thread's line is washed
		// and its number barred.
		auto* const cards = ConversationPanel->cards();
		ComparisonSurface->set_mark_colours(cards->marker(), cards->accent());
		for (const auto side : { core::diff_side::left, core::diff_side::right }) {
			auto       marks      = std::vector<diff_line_mark>{ };
			const auto onNotesTab = TrayTabs != nullptr && TrayTabs->currentIndex() == tray_notes_tab;
			for (const auto& mark : ThreadMarksSuspended ? std::vector<thread_line_mark>{ } : cards->line_marks(side)) {
				marks.push_back(diff_line_mark{
					.Line        = mark.Line,
					.Kind        = mark.Unread ? diff_line_mark_kind::new_thread : diff_line_mark_kind::thread,
					.Resolved    = mark.Resolved,
					.Draft       = mark.Draft,
					.Open        = mark.Open && !onNotesTab,
					.Hovered     = mark.Hovered && !onNotesTab,
					.Writing     = mark.Writing && !onNotesTab,
					.FirstLine   = mark.FirstLine,
					.Description = mark.Description,
				});
			}
			// [[BPR-1592]]. A noted line's number is a dashed cell, as my
			// draft's is; a line that has a thread keeps the thread's mark.
			// The open note washes its line in the writing blue.
			if (PrivateNotes != nullptr) {
				for (const auto& note : PrivateNotes->line_marks(side)) {
					const auto taken = std::ranges::find(marks, note.Line, &diff_line_mark::Line);
					if (taken != marks.end()) {
						taken->Writing = taken->Writing || (note.Open && onNotesTab);
						continue;
					}
					auto mark        = diff_line_mark{ };
					mark.Line        = note.Line;
					mark.Draft       = true;
					mark.Writing     = note.Open && onNotesTab;
					mark.Description = note.Description;
					marks.push_back(std::move(mark));
				}
			}
			ComparisonSurface->set_line_marks(side, std::move(marks));
		}
		update_new_comment_group();
	}

	auto workbench_window::thread_on_line(const core::diff_side aSide, const std::uint32_t aLine) -> std::optional<std::string>
	{
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return std::nullopt;
		}
		const auto& cards = ConversationPanel->cards()->cards();
		const auto  card  = std::ranges::find_if(cards, [aSide, aLine](const thread_card& aCard) {
			return aCard.Side == aSide && aCard.Line == aLine + 1U && !aCard.Outdated;
		});
		if (card == cards.end()) {
			return std::nullopt;
		}
		// Copied before the drawer is shown, which may lay the notes out anew.
		auto identity = card->Id;
		// The marker is a gesture on the conversation: the drawer it opens in
		// is shown, on its Comments tab.
		if (Drawers[1].Panel->state() != core::workbench_drawer_state::expanded) {
			Drawers[1].Toggle->trigger();
		}
		if (TrayTabs != nullptr && TrayTabs->currentIndex() != tray_comments_tab) {
			TrayTabs->setCurrentIndex(tray_comments_tab);
		}
		return identity;
	}

	void workbench_window::toggle_thread_at(const core::diff_side aSide, const std::uint32_t aLine)
	{
		// [[BPR-1592]]. On the Notes tab a noted line's mark opens its note;
		// elsewhere, a line with no thread but a note opens the note too.
		const auto note       = PrivateNotes != nullptr ? PrivateNotes->note_on_line(aSide, aLine) : std::nullopt;
		const auto onNotesTab = TrayTabs != nullptr && TrayTabs->currentIndex() == tray_notes_tab;
		if (const auto identity = thread_on_line(aSide, aLine); identity.has_value() && !(note && onNotesTab)) {
			ConversationPanel->cards()->toggle_thread(*identity);
			return;
		}
		if (note) {
			show_private_note(*note);
		}
	}

	void workbench_window::show_private_note(const std::uint64_t aId)
	{
		if (Dispatch && Drawers[1].Panel->state() != core::workbench_drawer_state::expanded) {
			Dispatch(core::set_workbench_drawer_intent{ .Drawer = core::workbench_drawer::conversations, .State = core::workbench_drawer_state::expanded });
		}
		if (TrayTabs != nullptr && TrayTabs->currentIndex() != tray_notes_tab) {
			TrayTabs->setCurrentIndex(tray_notes_tab);
		}
		if (PrivateNotes->open_note_id() == aId) {
			PrivateNotes->close_note();
			return;
		}
		PrivateNotes->open_note(aId);
	}

	void workbench_window::show_agent(const bool aAsk)
	{
		if (Dispatch && Drawers[1].Panel->state() != core::workbench_drawer_state::expanded) {
			Dispatch(core::set_workbench_drawer_intent{ .Drawer = core::workbench_drawer::conversations, .State = core::workbench_drawer_state::expanded });
		} else if (!Dispatch && Drawers[1].Panel->state() != core::workbench_drawer_state::expanded) {
			conversations_drawer_toggle_action()->trigger();
		}
		if (TrayTabs != nullptr) {
			TrayTabs->setCurrentIndex(tray_findings_tab);
		}
		if (AgentTab == nullptr) {
			return;
		}
		// The selected lines go with the question, through the chat's own
		// Add context, which the core decides; nothing is sent.
		if (aAsk && SideChat != nullptr && SideChat->add_context_action()->isEnabled()) {
			SideChat->add_context_action()->trigger();
		}
		AgentTab->refresh();
		AgentTab->focus_question();
	}

	void workbench_window::fit_tray_tabs(const bool aRepaint)
	{
		constexpr auto tabCount = 4;
		if (TrayTabs == nullptr || TrayHeader == nullptr || TrayTabs->count() != tabCount) {
			return;
		}
		auto* const tabs = static_cast<tray_tab_bar*>(TrayTabs);
		// [[BPR-1594]]. Four tabs; the Agent keeps its spark beside its name.
		const QString names[] = { tr("Comments"), tr("Notes"), tr("Tour"), tr("Agent") };
		const QString about[] = {
			tr("The pull request's threads, with their scope and filters"),
			tr("Your private notes at their lines. Nothing here is published; Draft comment copies a note to a draft."),
			tr("Your review tour, stop by stop. Nothing in it is published until you publish the tour."),
			tr("Agent conversations, findings and private history. Nothing here is published; promote an item to a draft in Comments to publish it."),
		};
		// [[BPR-1684]]. The faces, widest first: every name; the open
		// tab's glyph and name with the others' glyphs; glyphs only. The row
		// takes the first that fits the header, whose tools move to their own
		// row before any name goes.
		enum class face : std::uint8_t { names,
										 open_named,
										 glyphs };
		const auto current = tabs->currentIndex();
		const auto textOf  = [&names, current](const face aFace, const int aIndex) {
			return aFace == face::names || (aFace == face::open_named && aIndex == current) ? names[aIndex] : QString{ };
		};
		const auto glyphOf = [](const face aFace, const int aIndex) { return aFace != face::names || aIndex == tray_findings_tab; };
		// The room is what the header's grid gives the tabs alone on their
		// row: its width less its left margin and the spacing of its columns.
		auto room = TrayHeader->width();
		if (const auto* const grid = qobject_cast<const QGridLayout*>(TrayHeader->layout()); grid != nullptr && room > 0) {
			room -= grid->contentsMargins().left() + 2 * std::max(0, grid->horizontalSpacing());
		}
		auto chosen = face::names;
		if (room > 0) {
			for (const auto candidate : { face::names, face::open_named, face::glyphs }) {
				chosen     = candidate;
				auto width = 0;
				for (auto index = 0; index < tabCount; ++index) {
					width += tabs->face_width(index, textOf(candidate, index), glyphOf(candidate, index));
				}
				if (width <= room) {
					break;
				}
			}
		}
		const auto glyph = [this](const int aIndex) {
			switch (aIndex) {
			case tray_comments_tab:
				return conversation_icon(conversation_glyph::comment, palette());
			case tray_notes_tab:
				return drawer_icon(drawer_glyph::page, palette());
			case tray_tour_tab:
				return drawer_icon(drawer_glyph::tour, palette());
			default:
				return drawer_icon(drawer_glyph::agent, palette());
			}
		};
		for (auto index = 0; index < tabCount; ++index) {
			const auto text  = textOf(chosen, index);
			const auto shown = glyphOf(chosen, index);
			if (aRepaint || tabs->tabIcon(index).isNull() == shown) {
				tabs->setTabIcon(index, shown ? glyph(index) : QIcon{ });
			}
			if (tabs->tabText(index) != text) {
				tabs->setTabText(index, text);
			}
			// A tab showing only its glyph says its name first.
			const auto tip = text.isEmpty() ? QStringLiteral("%1\n%2").arg(tabs->accessibleTabName(index), about[index]) : about[index];
			if (tabs->tabToolTip(index) != tip) {
				tabs->setTabToolTip(index, tip);
			}
		}
	}

	void workbench_window::wire_private_notes()
	{
		// [[BPR-1592]]. The notes hang at the code's lines: where a line is,
		// which comparison is shown and which line is selected are the
		// surface's; a note elsewhere is reached through the files drawer.
		PrivateNotes->set_line_locator([this](const core::diff_side aSide, const std::uint32_t aLine) -> std::optional<int> {
			if (ComparisonSurface == nullptr || PrivateNotes == nullptr) {
				return std::nullopt;
			}
			const auto rect = ComparisonSurface->line_rect(aSide, aLine);
			if (rect.isEmpty()) {
				return std::nullopt;
			}
			const auto inWindow = ComparisonSurface->mapTo(this, rect.topLeft());
			return PrivateNotes->mapFrom(this, inWindow).y();
		});
		PrivateNotes->set_comparison_source([this]() -> const core::file_alignment* {
			if (ComparisonSurface == nullptr || ComparisonSurface->current_result() == nullptr) {
				return nullptr;
			}
			return &ComparisonSurface->current_result()->Alignment;
		});
		PrivateNotes->set_selected_line_source([this]() -> std::optional<private_note_line> {
			if (ComparisonSurface == nullptr || ComparisonSurface->current_result() == nullptr || !ComparisonSurface->selection()) {
				return std::nullopt;
			}
			const auto& chosen = *ComparisonSurface->selection();
			if (chosen.PreferredLine.Comparison != ComparisonSurface->current_result()->Alignment.Digest) {
				return std::nullopt;
			}
			return private_note_line{ .Side = chosen.PreferredLine.Side, .Line = chosen.PreferredLine.Line };
		});
		PrivateNotes->set_line_dispatch([this](const core::diff_side aSide, const std::uint32_t aLine) {
			if (ComparisonSurface != nullptr) {
				static_cast<void>(ComparisonSurface->select_conversation_line(aSide, aLine));
			}
		});
		PrivateNotes->set_elsewhere_dispatch([this](const private_note_item& aNote) { open_private_note_elsewhere(aNote); });
		PrivateNotes->set_marks_observer([this] { apply_thread_marks(); });
		PrivateNotes->set_scroll_forward([this](QWheelEvent& aEvent) { return ComparisonSurface != nullptr && ComparisonSurface->scroll_by_wheel(aEvent); });
		if (ConversationPanel != nullptr && ConversationPanel->cards() != nullptr) {
			PrivateNotes->set_colours(ConversationPanel->cards()->accent(), ConversationPanel->cards()->marker());
		}
	}

	void workbench_window::open_private_note_elsewhere(const private_note_item& aNote)
	{
		const auto file = file_for_path(QString::fromStdString(aNote.Label));
		if (!file) {
			statusBar()->showMessage(tr("That note's file is not in this pull request now."));
			return;
		}
		PendingNoteOpen = aNote.Id;
		choose_home(false);
		choose_modality(review_modality::review);
		if (Dispatch) {
			Dispatch(core::select_workbench_file_intent{ .File = *file });
		}
	}

	void workbench_window::resume_private_note_open()
	{
		if (!PendingNoteOpen || PrivateNotes == nullptr) {
			return;
		}
		const auto place = PrivateNotes->placement(*PendingNoteOpen);
		if (place.Kind == core::note_placement_kind::other_file) {
			return;
		}
		const auto id = *PendingNoteOpen;
		PendingNoteOpen.reset();
		show_private_note(id);
	}

	void workbench_window::go_to_thread_at(const core::diff_side aSide, const std::uint32_t aLine)
	{
		const auto identity = thread_on_line(aSide, aLine);
		if (!identity.has_value()) {
			return;
		}
		if (ConversationPanel->cards()->open_thread_id() != *identity) {
			ConversationPanel->cards()->open_thread(*identity);
		}
		// Opening reveals the line only when it changes the current thread;
		// the tick asks for the line whatever was current before.
		reveal_thread_line(*identity);
	}

	void workbench_window::reveal_thread_line(const std::string& aIdentity)
	{
		// [[BPR-1290]]. Reveal the thread's line where the reader is, without
		// changing the modality.
		const auto index = ConversationPanel->model()->index_for_id(aIdentity);
		if (index.isValid()) {
			ConversationPanel->choose(index);
			synchronize_selected_conversation(index);
		} else {
			static_cast<void>(select_thread_line(aIdentity));
		}
	}

	void workbench_window::write_inline_reply(const std::string& aThread, const QString& aText)
	{
		if (ReviewEditor == nullptr) {
			return;
		}
		const auto& draft = ReviewEditor->shown_draft();
		if (!draft.has_value() || !draft->ReplyToThread.has_value() || "review-thread:" + *draft->ReplyToThread != aThread
			|| ReviewEditor->body_editor()->isReadOnly()) {
			return;
		}
		if (ReviewEditor->body_editor()->toPlainText() != aText) {
			ReviewEditor->body_editor()->setPlainText(aText);
		}
	}

	void workbench_window::present_reply_placement()
	{
		// [[BPR-1084]]. The draft editor's suggested-change mark reads what the
		// thread a reply answers is attached to, from the card the accepted
		// projection built; with no such card it claims nothing.
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr || ReviewEditor == nullptr) {
			return;
		}
		auto        placement = std::optional<core::suggestion_placement_observation>{ };
		const auto& draft     = ReviewEditor->shown_draft();
		if (draft.has_value() && draft->ReplyToThread.has_value()) {
			const auto  identity = "review-thread:" + *draft->ReplyToThread;
			const auto& cards    = ConversationPanel->cards()->cards();
			const auto  found    = std::ranges::find(cards, identity, &thread_card::Id);
			if (found != cards.end()) {
				placement = found->Suggestion;
			}
		}
		ReviewEditor->describe_reply_placement(placement);
	}

	void workbench_window::present_inline_reply()
	{
		if (ConversationReply) {
			ConversationPanel->cards()->present_reply(ConversationReply);
			return;
		}
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr || ReviewEditor == nullptr) {
			return;
		}
		present_reply_placement();
		const auto& draft = ReviewEditor->shown_draft();
		if (!draft.has_value() || !draft->ReplyToThread.has_value() || draft->shared()) {
			ConversationPanel->cards()->present_reply(std::nullopt);
			return;
		}
		ConversationPanel->cards()->present_reply(thread_reply_presentation{
			.Thread      = "review-thread:" + *draft->ReplyToThread,
			.Body        = ReviewEditor->body_editor()->toPlainText(),
			.Publishable = ReviewEditor->publish_action()->isEnabled() && !ReviewEditor->body_editor()->isReadOnly(),
		});
	}

	void workbench_window::set_conversation_reply_dispatch(std::function<void(core::conversation_reply_action, core::conversation_reply_draft)> aDispatch) { ConversationReplyDispatch = std::move(aDispatch); }
	void workbench_window::present_conversation_reply(std::optional<thread_reply_presentation> aReply)
	{
		ConversationReply = std::move(aReply);
		if (ConversationPanel) {
			ConversationPanel->cards()->present_reply(ConversationReply);
		}
	}
	auto workbench_window::dispatch_conversation_reply(const core::conversation_reply_action aAction, const std::string& aIdentity, const QString& aText) -> bool
	{
		if (!ConversationPanel) {
			return false;
		}
		const auto& nodes = ConversationPanel->cards()->retained();
		const auto  found = std::ranges::find(nodes, aIdentity, &application::github_conversation_projection::Id);
		if (found == nodes.end() || !found->Path.empty() || (found->Kind != application::github_conversation_kind::issue_comment && found->Kind != application::github_conversation_kind::review)) {
			return false;
		}
		if (ConversationReplyDispatch) {
			ConversationReplyDispatch(aAction, { found->Id, found->AuthorLogin, found->Body, aText.toStdString() });
			if (aAction == core::conversation_reply_action::open) {
				ConversationPanel->cards()->begin_reply(aIdentity);
			}
		} else {
			ConversationPanel->present_notice(tr("Replies are unavailable for this connection."));
		}
		return true;
	}
	void workbench_window::set_conversation_read_dispatch(conversation_read_dispatch aDispatch)
	{
		ConversationReadDispatch = std::move(aDispatch);
	}

	void workbench_window::set_conversation_unread_dispatch(conversation_read_dispatch aDispatch)
	{
		ConversationUnreadDispatch = std::move(aDispatch);
	}

	void workbench_window::set_conversations_read_dispatch(conversations_read_dispatch aDispatch)
	{
		ConversationsReadDispatch = std::move(aDispatch);
		update_new_comment_group();
	}

	auto workbench_window::mark_all_comments_read_action() const noexcept -> QAction*
	{
		return MarkAllCommentsRead;
	}

	void workbench_window::present_conversations_read(const std::optional<conversations_read_result> aResult, const std::optional<std::string>& aAuthor)
	{
		// [[BPR-1699]]. The counts and badges have already moved with
		// the read state; this says what the gesture did, once it is durable.
		const auto who = aAuthor.has_value() ? QString::fromStdString(*aAuthor) : QString{ };
		if (!aResult.has_value()) {
			statusBar()->showMessage(tr("Your read state could not be saved, so no comment was marked read. Try again."), 10000);
			return;
		}
		const auto comments = [](const std::uint32_t aCount) {
			return aCount == 1U ? QStringLiteral("1 comment") : QStringLiteral("%1 comments").arg(aCount);
		};
		auto said = QString{ };
		if (aResult->Marked == 0U) {
			said = who.isEmpty() ? tr("Nothing on this pull request is new.") : tr("Nothing %1 wrote is new.").arg(who);
		} else {
			said = who.isEmpty() ? tr("Marked %1 as read.").arg(comments(aResult->Marked)) : tr("Marked %1 from %2 as read.").arg(comments(aResult->Marked), who);
		}
		if (aResult->Kept != 0U) {
			said += QLatin1Char(' ')
				+ (aResult->Kept == 1U ? tr("1 more stays new: it follows a reply from someone else that you have not read.")
									   : tr("%1 more stay new: they follow a reply from someone else that you have not read.").arg(aResult->Kept));
		}
		statusBar()->showMessage(said, 10000);
	}

	void workbench_window::update_read_state(const std::string& aThread, std::vector<std::string> aUnreadPosts)
	{
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return;
		}
		if (Finder != nullptr) {
			Finder->set_thread_read_state(aThread, aUnreadPosts);
		}
		static_cast<void>(ConversationPanel->cards()->set_thread_read_state(aThread, std::move(aUnreadPosts)));
		apply_thread_marks();
		present_new_threads();
	}

	void workbench_window::present_read_state(std::vector<thread_read_state> aState)
	{
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return;
		}
		if (Finder != nullptr) {
			Finder->set_read_state(aState);
		}
		ConversationPanel->cards()->set_read_state(std::move(aState));
		apply_thread_marks();
		present_new_threads();
	}

	void workbench_window::present_new_conversation_comments(const std::uint32_t aCount)
	{
		NewConversationComments = aCount;
		apply_thread_marks();
		if (OverviewPage != nullptr) {
			OverviewPage->set_new_conversation_comments(aCount);
		}
	}

	void workbench_window::present_new_threads()
	{
		// [[BPR-1589]]. The Status drawer counts what Threads' Only new lists:
		// the threads with a post I have not read.
		if (OverviewPage != nullptr && ConversationPanel != nullptr && ConversationPanel->cards() != nullptr) {
			OverviewPage->set_new_threads(ConversationPanel->cards()->unread_threads_in_file_order().size());
		}
	}

	void workbench_window::show_new_threads()
	{
		if (Modality == nullptr || !HasGithubPullRequest) {
			return;
		}
		choose_home(false);
		choose_modality(review_modality::threads);
		if (Finder != nullptr) {
			auto filters    = Finder->filters();
			filters.OnlyNew = true;
			Finder->set_filters(std::move(filters));
		}
	}

	auto workbench_window::next_new_comment_action() const noexcept -> QAction*
	{
		return NextNewCommentAction;
	}

	auto workbench_window::review_bar() const noexcept -> QToolBar*
	{
		return ReviewBar;
	}

	auto workbench_window::tray_tabs() const noexcept -> QTabBar*
	{
		return TrayTabs;
	}

	auto workbench_window::comments_tab_scroll() const noexcept -> QScrollArea*
	{
		return CommentsScroll;
	}

	auto workbench_window::findings_tab_scroll() const noexcept -> QScrollArea*
	{
		return ConversationScroll;
	}

	auto workbench_window::next_new_comment_button() const noexcept -> QToolButton*
	{
		return NextNewCommentButton;
	}

	auto workbench_window::comments_count_text() const -> QString
	{
		return TrayCommentsCount == nullptr ? QString{ } : static_cast<const drawer_count_pill*>(TrayCommentsCount)->text();
	}

	auto workbench_window::updates_disclosure_button() const noexcept -> updates_disclosure*
	{
		return UpdatesDisclosure;
	}

	auto workbench_window::updates_line_action() const noexcept -> QAction*
	{
		return UpdatesLineAction;
	}

	auto workbench_window::updates_line_open() const noexcept -> bool
	{
		return UpdatesLineState.State == core::workbench_updates_line::open;
	}

	auto workbench_window::prepare_tour_action() const noexcept -> QAction* { return PrepareTourAction; }
	auto workbench_window::add_tour_stop_action() const noexcept -> QAction* { return AddTourStopAction; }
	auto workbench_window::tour_editor() const noexcept -> review_tour_editor* { return TourEditor; }
	void workbench_window::open_review_tour()
	{
		close_review_composition(false);
		close_review_sheet();
		choose_home(false);
		choose_modality(review_modality::review);
		if (Drawers[1].Panel->state() != core::workbench_drawer_state::expanded) {
			conversations_drawer_toggle_action()->trigger();
		}
		TrayTabs->setCurrentIndex(tray_tour_tab);
		TourEditor->show_problem();
	}

	void workbench_window::wire_tour_page()
	{
		// [[BPR-1593]]. RFC-BPR-0085 D4: Expand opens the tour page in the
		// main pane (layout C), and Back to the code returns to the route.
		TourPage = new QWidget{ CentreStack };
		TourPage->setObjectName(QStringLiteral("workbenchTourPage"));
		auto* const pageColumn = new QVBoxLayout{ TourPage };
		pageColumn->setContentsMargins(0, 0, 0, 0);
		pageColumn->setSpacing(0);
		auto* const bar = new QHBoxLayout{ };
		bar->setContentsMargins(12, 6, 12, 0);
		bar->addStretch(1);
		auto* const back = new QPushButton{ tr("Back to the code"), TourPage };
		back->setObjectName(QStringLiteral("workbenchTourPageBack"));
		back->setFlat(true);
		back->setIcon(conversation_icon(conversation_glyph::minimize, palette()));
		back->setToolTip(tr("Back to the code  Esc"));
		connect(back, &QPushButton::clicked, this, [this] { close_tour_page(); });
		bar->addWidget(back);
		pageColumn->addLayout(bar);
		TourPageBody           = new QWidget{ TourPage };
		auto* const bodyColumn = new QVBoxLayout{ TourPageBody };
		bodyColumn->setContentsMargins(0, 0, 0, 0);
		pageColumn->addWidget(TourPageBody, 1);
		auto* const escape = new QShortcut{ shortcut_key(shortcut_command::leave_composer), TourPage };
		escape->setObjectName(QStringLiteral("workbenchTourPageEscape"));
		escape->setContext(Qt::WidgetWithChildrenShortcut);
		connect(escape, &QShortcut::activated, this, [this] { close_tour_page(); });
		CentreStack->addWidget(TourPage);
		TourEditor->set_expand_dispatch([this](const bool aPublication) { open_tour_page(aPublication); });
		TourEditor->set_collapse_dispatch([this] { close_tour_page(); });
		TourEditor->set_excerpt_source([this](const core::tour_source_range& aRange) -> std::optional<tour_excerpt> {
			if (ComparisonSurface == nullptr || ComparisonSurface->current_result() == nullptr || !ComparisonSurface->current_context().has_value()) {
				return std::nullopt;
			}
			// The compared file's path, from the files drawer's rows.
			auto        path  = QString{ };
			const auto* model = Drawers[0].Panel != nullptr ? Drawers[0].Panel->contents() : nullptr;
			for (int row = 0; model != nullptr && row < model->rowCount() && path.isEmpty(); ++row) {
				const auto candidate = model->data(model->index(row, 0), Qt::DisplayRole).toString();
				if (core::file_identity_of(candidate.toStdString()) == ComparisonSurface->current_context()->Key.File) {
					path = candidate;
				}
			}
			if (path.toStdString() != aRange.Path) {
				return std::nullopt;
			}
			const auto& result   = *ComparisonSurface->current_result();
			const auto& document = aRange.First.Side == core::diff_side::left ? result.LeftDocument : result.RightDocument;
			if (!document.has_value() || document->Lines.empty()) {
				return std::nullopt;
			}
			const auto last  = static_cast<std::uint32_t>(document->Lines.size() - 1U);
			const auto first = aRange.First.Line > 3U ? aRange.First.Line - 3U : 0U;
			const auto end   = std::min(last, aRange.Last.Line + 3U);
			const auto lines = core::suggestion_source_lines(*document, first, end);
			if (!lines.has_value()) {
				return std::nullopt;
			}
			auto excerpt      = tour_excerpt{ };
			excerpt.First     = first + 1U;
			excerpt.StopFirst = aRange.First.Line + 1U;
			excerpt.StopLast  = aRange.Last.Line + 1U;
			for (const auto& line : *lines) {
				excerpt.Lines.push_back(QString::fromStdString(line));
			}
			return excerpt;
		});
	}

	void workbench_window::open_tour_page(const bool aPublication)
	{
		if (!TourPageOpen) {
			TourPageOpen = true;
			TourPageBody->layout()->addWidget(TourEditor);
			TourEditor->set_mode(tour_editor_mode::page);
			if (Modality != nullptr && Modality->current() != review_modality::review) {
				choose_modality(review_modality::review);
			}
			apply_modality_surface();
			// The page is the whole review area: no drawers beside it, which
			// keep their state for Back to the code.
			apply_drawer_layout();
		}
		TourEditor->show();
		if (aPublication) {
			if (auto* const prepare = TourEditor->findChild<QPushButton*>(QStringLiteral("tourPreparePublication")); prepare != nullptr) {
				prepare->setFocus(Qt::OtherFocusReason);
			}
		} else if (TourEditor->stop_body()->isVisible()) {
			TourEditor->stop_body()->setFocus(Qt::OtherFocusReason);
		}
	}

	void workbench_window::close_tour_page()
	{
		if (!TourPageOpen) {
			return;
		}
		TourPageOpen = false;
		TourHolder->layout()->addWidget(TourEditor);
		TourEditor->set_mode(tour_editor_mode::route);
		TourEditor->show();
		apply_modality_surface();
		apply_drawer_layout();
		if (TrayTabs != nullptr) {
			TrayTabs->setCurrentIndex(tray_tour_tab);
		}
	}

	auto workbench_window::tour_page_open() const noexcept -> bool
	{
		return TourPageOpen;
	}

	auto workbench_window::compose_review_action() const noexcept -> QAction*
	{
		return ComposeReviewAction;
	}

	auto workbench_window::comment_action() const noexcept -> QAction*
	{
		return CommentAction;
	}

	auto workbench_window::comment_menu() const noexcept -> QMenu*
	{
		return CommentMenu;
	}

	void workbench_window::publish(const application::document_tabs_projection& aProjection)
	{
		TabsProjection = aProjection;
		present_document_tabs();
		update_older_pair_notice();
		update_comment_target();
	}

	void workbench_window::set_document_tab_dispatch(document_tab_dispatch aDispatch)
	{
		DocumentTabDispatch = std::move(aDispatch);
		update_tab_actions();
	}

	auto workbench_window::document_tabs() const noexcept -> document_tab_strip*
	{
		return DocumentTabs;
	}

	auto workbench_window::next_tab_action() const noexcept -> QAction*
	{
		return NextTabAction;
	}

	auto workbench_window::previous_tab_action() const noexcept -> QAction*
	{
		return PreviousTabAction;
	}

	auto workbench_window::close_tab_action() const noexcept -> QAction*
	{
		return CloseTabAction;
	}

	auto workbench_window::older_pair_notice() const noexcept -> QWidget*
	{
		return OlderPairNotice;
	}

	auto workbench_window::older_pair_notice_text() const -> QString
	{
		return OlderPairText != nullptr ? OlderPairText->text() : QString{ };
	}

	auto workbench_window::show_later_threads_button() const noexcept -> QPushButton*
	{
		return ShowLaterThreads;
	}

	auto workbench_window::path_of(const core::file_identity& aFile) const -> QString
	{
		const auto* const model = Drawers[0].Panel != nullptr ? Drawers[0].Panel->contents() : nullptr;
		if (model == nullptr) {
			return { };
		}
		for (int row = 0; row < model->rowCount(); ++row) {
			const auto index = model->index(row, 0);
			if (model->file_at(index) == aFile) {
				return model->data(index, Qt::DisplayRole).toString();
			}
		}
		return { };
	}

	auto workbench_window::pair_text(const core::comparison_pair& aPair, const bool aShort) const -> QString
	{
		const auto* const line  = EpochSelectors != nullptr ? EpochSelectors->line() : nullptr;
		const auto        dates = line != nullptr ? line->dates_of(aPair.Left.Identity, aPair.Right.Identity) : std::nullopt;
		if (!dates.has_value()) {
			return QStringLiteral("update %1 → %2").arg(aPair.Left.Identity.Epoch.value()).arg(aPair.Right.Identity.Epoch.value());
		}
		// "Feb 4 → 9" when both are in one month, as the design's pill reads.
		const auto& [from, to] = *dates;
		const auto month       = from.section(QLatin1Char{ ' ' }, 0, 0);
		if (aShort && !month.isEmpty() && to.section(QLatin1Char{ ' ' }, 0, 0) == month) {
			return QStringLiteral("%1 → %2").arg(from, to.section(QLatin1Char{ ' ' }, 1));
		}
		return QStringLiteral("%1 → %2").arg(from, to);
	}

	auto workbench_window::comparing_off_current_head() const -> bool
	{
		return ComparisonSurface != nullptr && ComparisonSurface->current_context().has_value()
			&& !LeftConversationEndpointCurrent && !RightConversationEndpointCurrent;
	}

	void workbench_window::present_document_tabs()
	{
		if (DocumentTabs == nullptr) {
			return;
		}
		auto tabs = std::vector<document_tab_presentation>{ };
		if (TabsProjection.has_value()) {
			tabs.reserve(TabsProjection->Tabs.size());
			for (const auto& entry : TabsProjection->Tabs) {
				tabs.push_back(document_tab_presentation{
					.Id            = entry.Id,
					.Path          = entry.View.File ? path_of(*entry.View.File) : QString{ },
					.OwnPair       = entry.OwnComparison ? pair_text(entry.View.Comparison, true) : QString{ },
					.Pair          = pair_text(entry.View.Comparison, false),
					.State         = entry.State,
					.Active        = entry.Active,
					.OwnComparison = entry.OwnComparison,
				});
			}
			// A tab's name is its file's, with as much of the path before it
			// as tells it apart from another tab's file of the same name.
			const auto suffix = [](const QString& aPath, const qsizetype aParts) {
				const auto parts = aPath.split(QLatin1Char{ '/' }, Qt::SkipEmptyParts);
				return parts.mid(std::max<qsizetype>(0, parts.size() - aParts)).join(QLatin1Char{ '/' });
			};
			for (auto& tab : tabs) {
				if (tab.Path.isEmpty()) {
					tab.Name = tr("Unlisted file");
					continue;
				}
				for (qsizetype parts = 1;; ++parts) {
					tab.Name         = suffix(tab.Path, parts);
					const auto clash = std::ranges::any_of(tabs, [&tab, &suffix, parts](const document_tab_presentation& aOther) {
						return !aOther.Path.isEmpty() && aOther.Path != tab.Path && suffix(aOther.Path, parts) == tab.Name;
					});
					if (!clash || tab.Name == tab.Path) {
						break;
					}
				}
			}
		}
		DocumentTabs->present(std::move(tabs));
		update_tab_actions();
	}

	void workbench_window::update_tab_actions()
	{
		const auto count  = TabsProjection.has_value() ? TabsProjection->Tabs.size() : 0U;
		const auto active = TabsProjection.has_value()
			&& std::ranges::any_of(TabsProjection->Tabs, &application::document_tab_entry::Active);
		const auto wired = static_cast<bool>(DocumentTabDispatch);
		if (NextTabAction != nullptr) {
			NextTabAction->setEnabled(wired && count > 1U);
			PreviousTabAction->setEnabled(wired && count > 1U);
			CloseTabAction->setEnabled(wired && active);
		}
	}

	void workbench_window::update_older_pair_notice()
	{
		if (OlderPairNotice == nullptr || ConversationPanel == nullptr) {
			return;
		}
		const auto active    = TabsProjection.has_value()
			? std::ranges::find(TabsProjection->Tabs, true, &application::document_tab_entry::Active)
			: std::vector<application::document_tab_entry>::const_iterator{ };
		const auto hasActive = TabsProjection.has_value() && active != TabsProjection->Tabs.end() && active->View.File.has_value();
		auto       later     = std::size_t{ 0U };
		if (hasActive && comparing_off_current_head()) {
			later = ConversationPanel->current_head_thread_count(*active->View.File);
		}
		if (later == 0U) {
			OlderPairNotice->hide();
			return;
		}
		static constexpr auto words   = std::array{ "One thread is", "Two threads are", "Three threads are", "Four threads are", "Five threads are",
													"Six threads are", "Seven threads are", "Eight threads are", "Nine threads are" };
		const auto            counted = later <= words.size() ? QString::fromLatin1(words[later - 1U]) : tr("%1 threads are").arg(later);
		const auto            text    = tr("Comparing %1. %2 on later updates of this file.").arg(pair_text(active->View.Comparison, false), counted);
		if (OlderPairText->text() != text) {
			OlderPairText->setText(text);
			OlderPairText->setAccessibleDescription(text);
		}
		OlderPairNotice->show();
	}

	void workbench_window::show_later_threads()
	{
		if (!DocumentTabDispatch || !TabsProjection.has_value()) {
			return;
		}
		const auto& tabs   = TabsProjection->Tabs;
		const auto  active = std::ranges::find(tabs, true, &application::document_tab_entry::Active);
		if (active == tabs.end() || !active->View.File.has_value()) {
			return;
		}
		// A tab of this file on the window's comparison shows its threads
		// where they sit; then this tab back on the window's comparison, which
		// the reviewer asked for; and the pull request's conversation in
		// Discuss when the window's comparison is no later than this tab's.
		const auto sibling = std::ranges::find_if(tabs, [&active](const application::document_tab_entry& aTab) {
			return !aTab.Active && !aTab.OwnComparison && aTab.View.File == active->View.File;
		});
		if (sibling != tabs.end()) {
			DocumentTabDispatch(core::activate_tab_intent{ .Tab = sibling->Id });
			return;
		}
		if (active->OwnComparison) {
			DocumentTabDispatch(core::return_tab_to_window_comparison_intent{ .Tab = active->Id });
			return;
		}
		choose_modality(review_modality::discuss);
	}

	void workbench_window::update_new_comment_group()
	{
		if (NextNewCommentAction == nullptr || ConversationPanel == nullptr
			|| ConversationPanel->cards() == nullptr) {
			return;
		}
		const auto& unread = ConversationPanel->cards()->unread_conversations_in_navigation_order();
		NextNewCommentAction->setEnabled(!unread.empty());
		// [[BPR-1699]]. Offered while anything on the pull request is new.
		if (MarkAllCommentsRead != nullptr) {
			MarkAllCommentsRead->setEnabled(!unread.empty() && static_cast<bool>(ConversationsReadDispatch));
		}
		// A refresh may retain the drawer's visual anchor without a selected
		// file. Only standalone diff presentation falls back to that anchor.
		const auto file       = LastWorkbenchContext.has_value()
			? (Cursor.has_value() ? Cursor->File : std::nullopt)
			: (HasGithubPullRequest ? std::nullopt : conversation_anchor_file());
		const auto fileUnread = file.has_value() ? ConversationPanel->cards()->unread_thread_count(*file) : 0U;
		// [[BPR-1511]]. The file-context badges count this file; navigation
		// continues to walk unread threads across the pull request.
		if (TrayTabs != nullptr && TrayCommentsCount != nullptr) {
			const auto newCount = fileUnread + NewConversationComments;
			static_cast<drawer_count_pill*>(TrayCommentsCount)->set_count(newCount);
			// [[BPR-1684]]. Only a showing count takes room in its tab.
			if (auto* const shown = newCount != 0U ? TrayCommentsCount : nullptr; TrayTabs->tabButton(tray_comments_tab, QTabBar::RightSide) != shown) {
				TrayTabs->setTabButton(tray_comments_tab, QTabBar::RightSide, shown);
				TrayCommentsCount->setVisible(shown != nullptr);
			}
			TrayTabs->setAccessibleTabName(
				tray_comments_tab,
				NewConversationComments > 0U ? tr("Comments, %1 new file threads and %2 new comments on the pull request").arg(fileUnread).arg(NewConversationComments) : fileUnread == 0U ? tr("Comments")
																																														   : tr("Comments, %1 with new comments in this file").arg(fileUnread == 1U ? tr("one thread") : tr("%1 threads").arg(fileUnread)));
			// The count's width and the Comments tooltip follow it.
			fit_tray_tabs();
		}
		const auto threadCount  = ConversationPanel->cards()->unread_threads_in_file_order().size();
		const auto commentCount = unread.size() - threadCount;
		auto       counts       = QStringList{ };
		if (threadCount != 0U) {
			counts.push_back(threadCount == 1U ? QStringLiteral("1 thread") : QStringLiteral("%1 threads").arg(threadCount));
		}
		if (commentCount != 0U) {
			counts.push_back(commentCount == 1U ? QStringLiteral("1 comment not on a file") : QStringLiteral("%1 comments not on a file").arg(commentCount));
		}
		NextNewCommentAction->setToolTip(
			unread.empty() ? QStringLiteral("Next new comment: nothing unread")
						   : QStringLiteral("Next new comment (%1)").arg(counts.join(QStringLiteral(", "))));
		// [[BPR-1129]]. The comments toggle carries the count of threads with a
		// post newer than what I have read, and says it to accessibility.
		if (auto* const toggle = qobject_cast<QToolButton*>(PrimaryToolbar->widgetForAction(Drawers[1].Toggle));
			toggle != nullptr) {
			set_strip_badge(*toggle, fileUnread == 0U ? QString{ } : QString::number(fileUnread));
			const auto counted = fileUnread == 0U
				? Drawers[1].Toggle->toolTip()
				: QStringLiteral("%1 %2 with new comments in this file.").arg(Drawers[1].Toggle->toolTip()).arg(fileUnread == 1U ? QStringLiteral("One thread") : QStringLiteral("%1 threads").arg(fileUnread));
			toggle->setAccessibleDescription(counted);
		}
	}

	void workbench_window::update_compose_mark()
	{
		// [[BPR-1590]]. The sheet's header names the review as its page does.
		if (ReviewSheet != nullptr && ReviewSubmissionEditor != nullptr) {
			ReviewSheet->title()->setText(ReviewSubmissionEditor->title_label()->text());
		}
		if (ComposeReviewAction == nullptr || CommentAction == nullptr) {
			return;
		}
		// [[BPR-1129]]. The mark is the review workflow's draft state, as its
		// editor was last given it; the rule for "kept" is the core's.
		const auto& shown     = ReviewSubmissionEditor != nullptr ? ReviewSubmissionEditor->shown_draft() : std::optional<core::review_submission_draft>{ };
		const auto  keptDraft = shown.has_value() && core::kept(*shown);
		const auto  key       = [this](const QAction* const aAction) {
			return presented_key(aAction);
		};
		// [[BPR-1700]]. The tooltip names the three verdicts the review
		// page offers, so the command says what it is for in every state.
		const auto compose = keptDraft
			? QStringLiteral("Compose review (%1)\nApprove, request changes or comment on the whole pull request.\nA review draft is kept for this pull request.").arg(key(ComposeReviewAction))
			: QStringLiteral("Compose review (%1)\nApprove, request changes or comment on the whole pull request.").arg(key(ComposeReviewAction));
		ComposeReviewAction->setToolTip(compose);
		ComposeReviewAction->setStatusTip(compose);
		update_comment_target();
		present_compose_entry();
	}

	void workbench_window::present_compose_entry()
	{
		if (ComposeReviewAction == nullptr || ReviewBar == nullptr) {
			return;
		}
		auto* const button = qobject_cast<QToolButton*>(ReviewBar->widgetForAction(ComposeReviewAction));
		if (button == nullptr) {
			return;
		}
		// [[BPR-1700]]. The owner, 2026-10-21: "I think the compose review
		// button is a little too subtle when there's not a active review. It
		// might be difficult for the uninitiated to find out how to approve a
		// review." With no review in progress it says what it is for, beside
		// its pencil, on the accent's edge. While a review is in progress the
		// chip before it leads, and it is the plain icon again. It only ever
		// opens the review page; the verdict is chosen there.
		const auto& shown      = ReviewSubmissionEditor != nullptr ? ReviewSubmissionEditor->shown_draft() : std::optional<core::review_submission_draft>{ };
		const auto  keptDraft  = shown.has_value() && core::kept(*shown);
		const auto  inProgress = PendingReviewComments > 0U || HeldReviewComments > 0U;
		const auto  worded     = !inProgress && ComposeReviewWords != compose_words::none;
		// The bar's presenter takes its words from the action's icon text; the
		// menus keep the command's own name. Where the bar is short of room
		// the words keep Approve, the verdict a newcomer looks for.
		const auto words = !worded                       ? tr("Compose review")
			: ComposeReviewWords == compose_words::brief ? tr("Approve…")
														 : tr("Approve or comment…");
		if (ComposeReviewAction->iconText() != words) {
			ComposeReviewAction->setIconText(words);
		}
		const auto style = worded ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly;
		if (button->toolButtonStyle() != style) {
			button->setToolButtonStyle(style);
		}
		// Its name keeps the command's and, with no review in progress, holds
		// the words it shows, also when the bar is too narrow for them.
		button->setAccessibleName(inProgress ? tr("Compose review") : tr("Compose review: approve or comment"));
		button->setAccessibleDescription(keptDraft
											 ? tr("Open the review page, where you choose Approve, Request changes or Comment for the whole pull request; a review draft is kept for this pull request")
											 : tr("Open the review page, where you choose Approve, Request changes or Comment for the whole pull request"));
		if (button->property(strip_style::mark_property).toBool() != keptDraft) {
			button->setProperty(strip_style::mark_property, keptDraft);
			button->update();
		}
		if (button->property(strip_style::lead_property).toBool() == inProgress) {
			button->setProperty(strip_style::lead_property, !inProgress);
			button->update();
		}
	}

	auto workbench_window::comment_reply_thread() const -> std::optional<std::string>
	{
		if (ConversationPanel == nullptr) {
			return std::nullopt;
		}
		auto root = ConversationPanel->view()->currentIndex();
		while (root.isValid() && root.parent().isValid()) {
			root = root.parent();
		}
		if (!root.isValid()
			|| root.data(github_conversation_model::anchor_availability_role).toInt()
				!= static_cast<int>(application::github_conversation_anchor_availability::current)) {
			return std::nullopt;
		}
		auto identity = ConversationPanel->model()->id_for_index(root);
		if (!identity.starts_with("review-thread:")) {
			return std::nullopt;
		}
		// The reviewer's choice of thread, or the thread on the line they
		// selected: outside Discuss the panel lists only that line's threads
		// while a line is selected, so its current row is one of them. A row
		// that is current only by default otherwise answers nothing.
		// [[BPR-1293]]. In Threads, as in Discuss, Comment answers the
		// thread the reviewer picked and never a selected line.
		const auto discuss = Modality != nullptr && (Modality->current() == review_modality::discuss || Modality->current() == review_modality::threads);
		if (ConversationPanel->current_is_default() && (discuss || !ConversationPanel->anchor_line().has_value())) {
			return std::nullopt;
		}
		return identity;
	}

	auto workbench_window::comment_target_text() const -> QString
	{
		if (const auto thread = comment_reply_thread(); thread.has_value()) {
			const auto author = ConversationPanel->model()->index_for_id(*thread).data(github_conversation_model::author_login_role).toString();
			return author.isEmpty() ? tr("&Reply to the selected thread…") : tr("&Reply to %1's thread…").arg(author);
		}
		const auto discuss = Modality != nullptr && (Modality->current() == review_modality::discuss || Modality->current() == review_modality::threads);
		if (!discuss && ComparisonSurface != nullptr) {
			if (const auto selection = ComparisonSurface->selection(); selection.has_value()) {
				return tr("Co&mment on line %1…").arg(static_cast<qulonglong>(selection->PreferredLine.Line) + 1U);
			}
		}
		return { };
	}

	auto workbench_window::comment_target_reason(const bool aDraftRequired, QString* const aShort) const -> QString
	{
		const auto say = [aShort](QString aReason, QString aWords) {
			if (aShort != nullptr) {
				*aShort = std::move(aWords);
			}
			return aReason;
		};
		const auto selection = ComparisonSurface != nullptr ? ComparisonSurface->selection() : std::nullopt;
		if (comparing_off_current_head()) {
			return say(tr("Select the pull request's current head; this tab compares an earlier update."), tr("Comments need the current head"));
		}
		if (comment_target_text().isEmpty()) {
			return say(tr("Select a current-head diff line or thread to comment."), tr("Select a line to comment on"));
		}
		// [[BPR-1212]]. A new comment on a base line is GitHub's `side: LEFT`,
		// counted in the merge base's version of the file. A reply goes to
		// its thread, wherever it was drafted from.
		if (selection && selection->PreferredLine.Side == core::diff_side::left && !LeftConversationEndpointCurrent && !comment_reply_thread().has_value()) {
			return say(tr("GitHub counts base lines in the merge base's version of this file, and this comparison's base is not known to match it. Comment on a right-side line, or reply to an existing thread."),
					   tr("This file's base is not GitHub's"));
		}
		const auto draftReady = ReviewEditor != nullptr
			&& (!ReviewEditor->body_editor()->isReadOnly() || ReviewEditor->reload_draft_action()->isEnabled() || ReviewEditor->reapply_draft_action()->isEnabled())
			&& ReviewEditor->shown_draft() && selection
			&& ReviewEditor->shown_draft()->Anchor == selection->PreferredLine;
		if (aDraftRequired && !draftReady && !CommentDraftUnavailable.isEmpty()) {
			return say(CommentDraftUnavailable, tr("Open Saved comments to recover"));
		}
		if (aDraftRequired && !draftReady) {
			return say(tr("No editable draft is available. Wait for the review to load, or open an active pull request with an account that can comment."), tr("No draft available"));
		}
		return { };
	}

	void workbench_window::update_comment_target()
	{
		if (CommentAction == nullptr) {
			return;
		}
		// [[BPR-1133]]. The command names what it would start, in the bar's
		// tooltip, in its arrow's menu and in the Pull Request menu.
		const auto target = comment_target_text();
		auto       words  = target;
		words.remove(QLatin1Char{ '&' });
		words.remove(QChar{ 0x2026 });
		const auto key                = presented_key(CommentAction);
		const auto reason             = comment_target_reason(true);
		const auto promotionAvailable = reason.isEmpty() && !ReviewEditor->body_editor()->isReadOnly();
		const auto promotionReason    = !reason.isEmpty() ? reason : tr("Resolve the retained draft conflict before promoting an item.");
		if (PrivateNotes != nullptr) {
			PrivateNotes->set_promotion_target(promotionAvailable, promotionReason);
		}
		if (SideChat != nullptr) {
			SideChat->set_promotion_target(promotionAvailable, promotionReason);
		}
		CommentAction->setEnabled(reason.isEmpty());
		CommentAction->setText(reason.isEmpty() ? target : tr("Co&mment unavailable — %1").arg(reason));
		const auto tip = reason.isEmpty() ? QStringLiteral("%1 (%2)").arg(words, key) : tr("Comment (%1): %2").arg(key, reason);
		CommentAction->setToolTip(tip);
		CommentAction->setStatusTip(tip);
		if (ReviewBar == nullptr) {
			return;
		}
		if (auto* const button = qobject_cast<QToolButton*>(ReviewBar->widgetForAction(CommentAction)); button != nullptr) {
			button->setAccessibleName(QStringLiteral("Comment"));
			button->setAccessibleDescription(reason.isEmpty() ? QStringLiteral("%1, in the drawer beside the code, without publishing").arg(words) : reason);
		}
	}

	void workbench_window::comment_or_reply()
	{
		update_comment_target();
		if (!CommentAction->isEnabled()) {
			return;
		}
		if (const auto thread = comment_reply_thread(); thread.has_value()) {
			reply_to_thread(*thread, true);
			return;
		}
		static_cast<void>(begin_new_comment(false));
	}

	void workbench_window::reply_to_thread(const std::string& aIdentity, const bool aInPlace)
	{
		// [[BPR-0649]], [[BPR-0657]], [[BPR-1046]]. The thread becomes current
		// and its line is selected, which composes the draft on that line; only
		// then is the thread named, so that draft answers it and no draft on
		// another line can. While the thread's own file is still loading, the
		// reply waits for its comparison instead of refusing.
		DeferredReply.reset();
		if (ConversationPanel == nullptr) {
			return;
		}
		const auto index = ConversationPanel->model()->index_for_id(aIdentity);
		if (index.isValid()) {
			ConversationPanel->choose(index);
		}
		if (thread_comparison_loading(aIdentity)) {
			DeferredReply        = aIdentity;
			DeferredReplyInPlace = aInPlace;
			return;
		}
		begin_thread_reply(aIdentity, aInPlace);
	}

	void workbench_window::step_to_next_new_comment()
	{
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return;
		}
		const auto& unread = ConversationPanel->cards()->unread_conversations_in_navigation_order();
		if (unread.empty()) {
			return;
		}
		// The first unread thread after the current one in file order, and
		// the first of all once the last is passed.
		const auto& current = ConversationPanel->cards()->current_thread();
		auto        next    = unread.front();
		if (const auto at = std::ranges::find(unread, current); at != unread.end()) {
			const auto following = std::next(at);
			next                 = following != unread.end() ? *following : unread.front();
		}
		if (const auto location = Finder->locate(next); location.has_value() && location->Path.isEmpty()) {
			if (Modality && Modality->current() == review_modality::review) {
				if (TrayTabs) {
					TrayTabs->setCurrentIndex(tray_comments_tab);
				}
				if (Dispatch) {
					Dispatch(core::set_workbench_drawer_intent{ .Drawer = core::workbench_drawer::conversations, .State = core::workbench_drawer_state::expanded });
				}
				ConversationPanel->cards()->open_thread(next);
			} else {
				static_cast<void>(move_thread(next, core::workbench_thread_origin::review, core::workbench_thread_destination::discuss));
			}
			return;
		}
		// The conversation view's current index is what reveals the line, as a
		// press on a card does; the cards follow it.
		ConversationPanel->cards()->show_pull_request_comments(false);
		const auto index = ConversationPanel->model()->index_for_id(next);
		if (index.isValid()) {
			ConversationPanel->view()->setCurrentIndex(index);
		}
		ConversationPanel->cards()->set_current_thread(next);
		// [[BPR-1290]]. A thread of this file opens in place.
		if (std::ranges::any_of(ConversationPanel->cards()->cards(), [&next](const thread_card& aCard) { return aCard.Id == next; })) {
			ConversationPanel->cards()->open_thread(next);
		}
	}

	void workbench_window::apply_modality_surface()
	{
		if (CentreStack == nullptr || OverviewPage == nullptr) {
			return;
		}
		// Composition covers the invoking page without replacing its mode or
		// selection. Discuss borrows the same panel used beside the code.
		const auto overview = Modality != nullptr
			&& Modality->current() == review_modality::overview
			&& (OverviewPage->presented() || HasCachedOverview || (SessionPanel != nullptr && SessionPanel->status() == core::github_status::loading_pull_request));
		// [[BPR-1212]]. A composer is Discuss's: it shows there, beside the
		// conversations, whatever surface opened it.
		const auto discuss         = (Modality != nullptr && Modality->current() == review_modality::discuss) || CompositionPage != nullptr;
		const auto threads         = Modality != nullptr && Modality->current() == review_modality::threads;
		const auto enteringDiscuss = discuss && ConversationPanel != nullptr && ConversationPanel->parentWidget() != DiscussPage;
		if (ConversationPanel != nullptr) {
			if (discuss && ConversationPanel->parentWidget() != DiscussPage) {
				static_cast<QBoxLayout*>(DiscussPage->layout())->insertWidget(0, ConversationPanel, 1);
				ConversationPanel->show();
			} else if (!discuss && ConversationPanel->parentWidget() == DiscussPage) {
				if (ConversationPanel->isAncestorOf(QApplication::focusWidget())) {
					DiscussReturnFocus = QApplication::focusWidget();
				}
				ConversationLayout->insertWidget(0, ConversationPanel, 1);
				ConversationPanel->show();
			}
			ConversationPanel->set_discuss(discuss);
		}
		auto* page = ComparisonPage;
		if (overview) {
			page = OverviewPage;
		}
		if (discuss) {
			page = DiscussPage;
		}
		if (threads && ThreadsPage != nullptr) {
			page = ThreadsPage;
		}
		if (CompositionPage != nullptr) {
			page = DiscussPage;
		}
		// [[BPR-1593]]. The tour page covers Review while it is open, with no
		// drawers beside it.
		if (TourPageOpen && TourPage != nullptr) {
			if (Modality != nullptr && Modality->current() == review_modality::review && CompositionPage == nullptr) {
				page = TourPage;
			}
		}
		// The composer column shows the one page being written, and the
		// conversations narrow to a list beside it.
		if (DiscussComposer != nullptr) {
			for (auto* const composition : { static_cast<QWidget*>(ComposeInlinePage), static_cast<QWidget*>(ComposeReviewPage) }) {
				if (composition != nullptr) {
					composition->setVisible(composition == CompositionPage);
				}
			}
			DiscussComposer->setVisible(CompositionPage != nullptr);
			// [[BPR-1590]]. The expanded review is one centred column, as the
			// chosen layout A draws it (RFC-BPR-0085 D6), with no thread list; a
			// maximized draft keeps its list beside it ([[BPR-1212]]).
			const auto focused = CompositionPage != nullptr && CompositionPage == ComposeReviewPage;
			const auto column  = core::reading_width_limit(Behaviour.ReadingWidth);
			static_cast<void>(column);
			if (focused) {
				centre_review_page();
			}
			if (ConversationPanel != nullptr && ConversationPanel->parentWidget() == DiscussPage) {
				ConversationPanel->setVisible(!focused);
			}
			if (ConversationPanel != nullptr) {
				const auto listed = CompositionPage != nullptr && ConversationPanel->parentWidget() == DiscussPage;
				// [[BPR-1584]]. Discuss's notes stop at the reading width,
				// at the page's left, as Threads' rows do; beside the code
				// the drawer keeps its own bounds.
				const auto limit = core::reading_width_limit(Behaviour.ReadingWidth);
				const auto read  = ConversationPanel->parentWidget() == DiscussPage && limit.has_value()
					? static_cast<int>(std::min<std::uint32_t>(*limit, QWIDGETSIZE_MAX))
					: QWIDGETSIZE_MAX;
				ConversationPanel->setMaximumWidth(listed ? discuss_composer_list_width : read);
				if (ConversationPanel->parentWidget() == DiscussPage) {
					static_cast<QBoxLayout*>(DiscussPage->layout())->setStretchFactor(ConversationPanel, listed ? 0 : 1);
				}
			}
		}
		if (ToggleStatusDrawerAction != nullptr) {
			ToggleStatusDrawerAction->setEnabled(page == OverviewPage);
		}
		const auto enteringOverview = page == OverviewPage && CentreStack->currentWidget() != OverviewPage;
		const auto enteringThreads  = page == ThreadsPage && CentreStack->currentWidget() != ThreadsPage;
		CentreStack->setCurrentWidget(page);
		apply_drawer_layout();
		update_review_readiness();
		// [[BPR-1089]]. Back on the Overview after a count opened Discuss, the
		// same cell has focus.
		if (enteringOverview && OverviewReturnFocus) {
			OverviewReturnFocus->setFocus(Qt::OtherFocusReason);
			OverviewReturnFocus.clear();
		}
		if (enteringDiscuss && CompositionPage == nullptr && DiscussReturnFocus) {
			DiscussReturnFocus->setFocus(Qt::OtherFocusReason);
		}
		// [[BPR-1293]]. Entering Threads: the search has focus and the
		// cursor starts on the conversation open in Review.
		if (enteringThreads && Finder != nullptr) {
			present_finder_reading();
			Finder->enter();
		}
		if (ShowThreadsAction != nullptr) {
			ShowThreadsAction->setEnabled(HasGithubPullRequest);
		}
		update_show_thread_actions();
	}

	void workbench_window::open_review_composition(const bool aInline)
	{
		// [[BPR-1590]]. Compose review and Finish review open the sheet; only
		// its Expand opens the review as a page.
		if (!aInline && !ExpandingReviewSheet) {
			open_review_sheet();
			return;
		}
		if (aInline && ReviewSheetOpen) {
			close_review_sheet();
		}
		if (ReviewNavigationDispatch) {
			ReviewNavigationDispatch(std::nullopt);
		}
		// [[BPR-1212]]. The draft on the selected line is written in the
		// drawer at its line while the code shows; the full composer, and
		// the whole-review summary (owner's Q3: "A: move to Discuss"), are
		// Discuss's.
		if (aInline && CompositionPage == nullptr && Modality != nullptr && Modality->current() == review_modality::review && !showing_home()
			&& ReviewEditor != nullptr && ReviewEditor->shown_draft().has_value() && !ReviewEditor->shown_draft()->ReplyToThread.has_value()
			&& !ReviewEditor->body_editor()->isReadOnly()) {
			if (begin_new_comment(false)) {
				return;
			}
		}
		if (CompositionPage == nullptr) {
			CompositionReturnFocus     = QApplication::focusWidget();
			CompositionReturnModality  = Modality != nullptr ? std::optional{ Modality->current() } : std::nullopt;
			CompositionReturnsToDrawer = false;
		}
		if (ConversationPanel != nullptr && ConversationPanel->cards() != nullptr && ConversationPanel->cards()->writing_draft()) {
			ConversationPanel->cards()->end_draft();
		}
		if (Modality != nullptr && Modality->current() != review_modality::discuss) {
			const auto returnFocus     = CompositionReturnFocus;
			const auto returnModality  = CompositionReturnModality;
			const auto returnsToDrawer = CompositionReturnsToDrawer;
			choose_modality(review_modality::discuss);
			CompositionReturnFocus     = returnFocus;
			CompositionReturnModality  = returnModality;
			CompositionReturnsToDrawer = returnsToDrawer;
		}
		CompositionPage = aInline ? ComposeInlinePage : ComposeReviewPage;
		apply_modality_surface();
		present_inline_draft();
		auto* body = aInline ? ReviewEditor->body_editor() : ReviewSubmissionEditor->body_editor();
		if (body->isVisible() && body->isEnabled()) {
			body->setFocus(Qt::OtherFocusReason);
			if (aInline && CompositionCaret >= 0) {
				auto cursor = ReviewEditor->body_editor()->textCursor();
				cursor.setPosition(std::min(CompositionCaret, static_cast<int>(ReviewEditor->body_editor()->document()->characterCount() - 1)));
				ReviewEditor->body_editor()->setTextCursor(cursor);
			}
		} else if (auto* back = CompositionPage->findChild<QPushButton*>(aInline ? QStringLiteral("composeInlineDraftPageBack") : QStringLiteral("composeReviewPageBack"))) {
			back->setFocus(Qt::OtherFocusReason);
		}
		CompositionCaret = -1;
	}

	void workbench_window::open_draft_in_discuss()
	{
		if (CompositionPage == nullptr) {
			CompositionReturnFocus     = QApplication::focusWidget();
			CompositionReturnModality  = Modality != nullptr ? std::optional{ Modality->current() } : std::nullopt;
			CompositionReturnsToDrawer = false;
		}
		CompositionPage = ComposeInlinePage;
		open_review_composition(true);
	}

	void workbench_window::maximize_drawer_draft()
	{
		// [[BPR-1212]]. Maximize: the same draft, with room, in Discuss; the
		// same key brings it back to the drawer with its text and caret.
		auto* const cards = ConversationPanel != nullptr ? ConversationPanel->cards() : nullptr;
		if (cards != nullptr && cards->draft_editor() != nullptr) {
			CompositionCaret = cards->draft_editor()->textCursor().position();
		}
		CompositionReturnFocus     = ComparisonSurface != nullptr ? ComparisonSurface->right_pane() : nullptr;
		CompositionReturnModality  = Modality != nullptr ? std::optional{ Modality->current() } : std::nullopt;
		CompositionReturnsToDrawer = true;
		CompositionPage            = ComposeInlinePage;
		open_review_composition(true);
	}

	void workbench_window::close_review_composition(const bool aRestoreSheet)
	{
		const auto reviewPage      = CompositionPage != nullptr && CompositionPage == ComposeReviewPage;
		const auto inlineDraft     = CompositionPage == ComposeInlinePage;
		const auto returnsToDrawer = inlineDraft && CompositionReturnsToDrawer;
		const auto returnModality  = std::exchange(CompositionReturnModality, std::nullopt);
		CompositionReturnsToDrawer = false;
		if (returnsToDrawer && ReviewEditor != nullptr) {
			CompositionCaret = ReviewEditor->body_editor()->textCursor().position();
		}
		CompositionPage = nullptr;
		if (returnModality.has_value() && Modality != nullptr && Modality->current() != *returnModality) {
			const auto focus = CompositionReturnFocus;
			choose_modality(*returnModality);
			CompositionReturnFocus = focus;
		} else {
			apply_modality_surface();
		}
		if (returnsToDrawer && Modality != nullptr && Modality->current() == review_modality::review && begin_new_comment(false)) {
			auto* const editor = ConversationPanel->cards()->draft_editor();
			if (editor != nullptr && CompositionCaret >= 0) {
				auto cursor = editor->textCursor();
				cursor.setPosition(std::min(CompositionCaret, static_cast<int>(editor->document()->characterCount() - 1)));
				editor->setTextCursor(cursor);
			}
			CompositionCaret = -1;
			return;
		}
		CompositionCaret = -1;
		// [[BPR-1590]]. The expanded review restores to its sheet, beside the
		// surface it came from.
		if (reviewPage && aRestoreSheet) {
			const auto back = CompositionReturnFocus;
			open_review_sheet();
			SheetReturnFocus = back;
			return;
		}
		if (CompositionReturnFocus && CompositionReturnFocus->isVisible()) {
			CompositionReturnFocus->setFocus(Qt::OtherFocusReason);
		}
	}

	auto workbench_window::review_sheet() const noexcept -> compose_review_sheet*
	{
		return ReviewSheet;
	}

	auto workbench_window::review_sheet_open() const noexcept -> bool
	{
		return ReviewSheetOpen;
	}

	void workbench_window::centre_review_page()
	{
		if (ComposeReviewPage == nullptr || ComposeReviewPage->widget() == nullptr || ComposeReviewPage->widget()->layout() == nullptr) {
			return;
		}
		// The chosen layout A: a 680-pixel column, centred, at least 24 pixels in.
		constexpr auto column = 680;
		const auto     side   = std::max(24, (ComposeReviewPage->viewport()->width() - column) / 2);
		ComposeReviewPage->widget()->layout()->setContentsMargins(side, 12, side, 24);
	}

	void workbench_window::present_review_context(const bool aInSheet)
	{
		if (ReviewSubmissionEditor == nullptr) {
			return;
		}
		// The pull request in one quiet line under the page's title; the
		// sheet's header says enough, as layout C draws it.
		auto context = QString{ };
		if (!aInSheet && OverviewPage != nullptr && OverviewPage->projection().has_value()) {
			const auto& shown = *OverviewPage->projection();
			context           = tr("%1/%2 #%3 · %4").arg(QString::fromStdString(shown.Owner), QString::fromStdString(shown.RepositoryName), QString::number(shown.Number.value()), QString::fromStdString(shown.Title));
		}
		ReviewSubmissionEditor->set_context(context);
		// [[BPR-1590]] v003. How a row opens, on the sheet and the page alike.
		ReviewSubmissionEditor->set_reveal_hint(true);
	}

	void workbench_window::place_review_sheet()
	{
		if (ReviewSheet == nullptr || CentralSplitter == nullptr) {
			return;
		}
		ReviewSheet->place(CentralSplitter->geometry());
	}

	void workbench_window::move_review_composer(const bool aToSheet)
	{
		if (ReviewSheet == nullptr || ComposeReviewPage == nullptr || ReviewSubmissionEditor == nullptr) {
			return;
		}
		auto* const pageColumn = qobject_cast<QVBoxLayout*>(ComposeReviewPage->widget()->layout());
		auto* const from       = aToSheet ? static_cast<QScrollArea*>(ComposeReviewPage) : ReviewSheet->scroll();
		auto* const to         = aToSheet ? ReviewSheet->scroll() : static_cast<QScrollArea*>(ComposeReviewPage);
		const auto  inSheet    = ReviewSubmissionEditor->parentWidget() == ReviewSheet->scroll()->widget();
		if (inSheet == aToSheet) {
			return;
		}
		const auto scroll = from->verticalScrollBar()->value();
		if (aToSheet) {
			pageColumn->removeWidget(ReviewSubmissionEditor);
			ReviewSheet->content_layout()->addWidget(ReviewSubmissionEditor, 1);
		} else {
			ReviewSheet->content_layout()->removeWidget(ReviewSubmissionEditor);
			pageColumn->addWidget(ReviewSubmissionEditor, 1);
		}
		ReviewSubmissionEditor->show();
		// The sheet's header names the review; the page shows the editor's own title.
		ReviewSubmissionEditor->title_label()->setVisible(!aToSheet);
		present_review_context(aToSheet);
		ReviewSheet->title()->setText(ReviewSubmissionEditor->title_label()->text());
		to->widget()->adjustSize();
		to->verticalScrollBar()->setValue(scroll);
		// The geometry settles on the next pass; keep the place the reader had.
		QTimer::singleShot(0, to, [to, scroll] { to->verticalScrollBar()->setValue(scroll); });
	}

	void workbench_window::open_review_sheet(const bool aFocus)
	{
		if (ReviewSheet == nullptr) {
			return;
		}
		if (CompositionPage != nullptr) {
			// An inline draft's page gives way; the expanded review becomes the sheet.
			close_review_composition(false);
		}
		if (!ReviewSheetOpen) {
			SheetReturnFocus = QApplication::focusWidget();
		}
		move_review_composer(true);
		ReviewSheetOpen = true;
		ReviewSubmissionEditor->ensurePolished();
		ReviewSheet->content_layout()->activate();
		place_review_sheet();
		ReviewSheet->show();
		ReviewSheet->raise();
		// Its content's minimum settles once shown; the sheet follows it.
		QTimer::singleShot(0, ReviewSheet, [this] {
			if (ReviewSheetOpen) {
				place_review_sheet();
			}
		});
		if (!aFocus) {
			return;
		}
		auto* const body = ReviewSubmissionEditor->body_editor();
		if (body->isVisible() && body->isEnabled()) {
			body->setFocus(Qt::OtherFocusReason);
		} else {
			ReviewSheet->close_button()->setFocus(Qt::OtherFocusReason);
		}
	}

	void workbench_window::close_review_sheet()
	{
		if (ReviewSheet == nullptr || !ReviewSheetOpen) {
			return;
		}
		ReviewSheetOpen = false;
		ReviewSheet->hide();
		if (SheetReturnFocus && SheetReturnFocus->isVisible()) {
			SheetReturnFocus->setFocus(Qt::OtherFocusReason);
		}
	}

	void workbench_window::expand_review_sheet()
	{
		if (ReviewSheet == nullptr) {
			return;
		}
		const auto caret = ReviewSubmissionEditor->body_editor()->textCursor();
		const auto focus = SheetReturnFocus;
		ReviewSheetOpen  = false;
		ReviewSheet->hide();
		move_review_composer(false);
		ExpandingReviewSheet = true;
		CompositionReturnFocus.clear();
		open_review_composition(false);
		ExpandingReviewSheet   = false;
		CompositionReturnFocus = focus;
		ReviewSubmissionEditor->body_editor()->setTextCursor(caret);
	}

	auto workbench_window::begin_new_comment(const bool aSuggestion) -> bool
	{
		update_comment_target();
		if (CommentAction == nullptr || ReviewEditor == nullptr || ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return false;
		}
		const auto& draft = ReviewEditor->shown_draft();
		if (!CommentAction->isEnabled() || !draft.has_value()) {
			ConversationPanel->present_notice(CommentAction->toolTip());
			return false;
		}
		// A new comment is never a reply: R answers a thread (BPR-1212's
		// flagged point 3). A draft that answers one is finished first.
		if (draft->ReplyToThread.has_value()) {
			ConversationPanel->present_notice(tr("Finish or cancel the reply on this line first; a new comment starts once it is gone."));
			return false;
		}
		// Suggest change starts as a block holding the selected lines
		// exactly; a draft with words keeps them and gains the block.
		if (aSuggestion && !ReviewEditor->body_editor()->isReadOnly()
			&& core::classify_review_body(ReviewEditor->body_editor()->toPlainText().toStdString()) != core::review_body_compatibility::requires_line_subject) {
			if (const auto lines = draft_source_lines(); lines.has_value()) {
				const auto fence = QString::fromStdString(core::suggestion_fence_body(*lines));
				const auto words = ReviewEditor->body_editor()->toPlainText();
				ReviewEditor->body_editor()->setPlainText(words.trimmed().isEmpty() ? fence : words.trimmed() + QStringLiteral("\n\n") + fence);
				CompositionCaret = static_cast<int>(ReviewEditor->body_editor()->toPlainText().lastIndexOf(QLatin1Char{ '\n' }));
			}
		}
		const auto inReview = Modality != nullptr && Modality->current() == review_modality::review && !showing_home() && CompositionPage == nullptr;
		if (!inReview || ReviewEditor->body_editor()->isReadOnly()) {
			// Discuss, or a draft whose kept text is in conflict: the full
			// composer, where Reload and Reapply are.
			CompositionPage = nullptr;
			open_draft_in_discuss();
			return true;
		}
		if (Drawers[1].Panel->state() != core::workbench_drawer_state::expanded || (TrayTabs != nullptr && TrayTabs->currentIndex() != tray_comments_tab)) {
			if (TrayTabs != nullptr && TrayTabs->currentIndex() != tray_comments_tab) {
				TrayTabs->setCurrentIndex(tray_comments_tab);
			}
			if (Drawers[1].Panel->state() != core::workbench_drawer_state::expanded) {
				if (!Dispatch) {
					open_draft_in_discuss();
					return true;
				}
				PendingDraft = aSuggestion;
				Dispatch(core::set_workbench_drawer_intent{
					.Drawer = core::workbench_drawer::conversations,
					.State  = core::workbench_drawer_state::expanded,
				});
				if (Drawers[1].Panel->state() != core::workbench_drawer_state::expanded) {
					return true;
				}
				PendingDraft.reset();
			}
		}
		WritingAnchor = draft->Anchor;
		present_inline_draft();
		ConversationPanel->cards()->begin_draft();
		if (auto* const editor = ConversationPanel->cards()->draft_editor(); editor != nullptr && CompositionCaret >= 0) {
			auto cursor = editor->textCursor();
			cursor.setPosition(std::min(CompositionCaret, static_cast<int>(editor->document()->characterCount() - 1)));
			editor->setTextCursor(cursor);
		}
		CompositionCaret = -1;
		apply_thread_marks();
		return true;
	}

	auto workbench_window::decide_selected_lines(QString& aReason, QString* const aShort) const -> std::optional<core::comment_range_decision>
	{
		const auto refuse = [&aReason, aShort](QString aFull, QString aWords) {
			aReason = std::move(aFull);
			if (aShort != nullptr) {
				*aShort = std::move(aWords);
			}
			return std::optional<core::comment_range_decision>{ };
		};
		// [[BPR-1212]] slice 1. The selection's two ends, as the mouse and the
		// keyboard line cursor share them, decided by the core.
		const auto range = ComparisonSurface != nullptr ? ComparisonSurface->line_draft_range() : std::nullopt;
		if (!range) {
			return refuse(tr("Select a line first: click it, or move to it with Up and Down while the code has focus."), tr("Select a line to comment on"));
		}
		if (comparing_off_current_head()) {
			return refuse(tr("Select the pull request's current head; this tab compares an earlier update."), tr("Comments need the current head"));
		}
		const auto& result = ComparisonSurface->current_result();
		if (!result) {
			return refuse(tr("Wait for the comparison to load, then select a line."), tr("The comparison is loading"));
		}
		// A base line takes a comment once its numbering is known to be
		// GitHub's (`LEFT` lines count the merge base's version of the file).
		const auto decision = core::decide_comment_range(result->Alignment, range->first, range->second, LeftConversationEndpointCurrent);
		switch (decision.Refusal) {
		case core::comment_range_refusal::none:
			return decision;
		case core::comment_range_refusal::base_side:
			return refuse(tr("GitHub counts base lines in the merge base's version of this file, and this comparison's base is not known to match it. Comment on a right-side line, or reply to an existing thread."),
						  tr("This file's base is not GitHub's"));
		case core::comment_range_refusal::mixed_side:
			return refuse(tr("Select lines on one side to comment on them together."), tr("Select lines on one side"));
		case core::comment_range_refusal::outside_hunk:
			if (range->first == range->second) {
				// One base line: GitHub takes a `LEFT` comment only in the diff.
				return refuse(tr("GitHub takes a comment on a base line only inside a changed region. Select a base line within a hunk, or a right-side line."),
							  tr("Base lines need a changed region"));
			}
			[[fallthrough]];
		case core::comment_range_refusal::crosses_hunk:
			return refuse(tr("GitHub takes a comment on several lines only inside one changed region. Select lines within one hunk, or a single line."),
						  tr("Lines must be in one changed region"));
		case core::comment_range_refusal::mixed_comparison:
		case core::comment_range_refusal::outside_document:
			break;
		}
		return refuse(tr("Select a line of this comparison."), tr("Select a line of this comparison"));
	}

	auto workbench_window::comment_on_selected_lines(const bool aSuggestion) -> bool
	{
		// [[BPR-1212]]. One path for Return, Shift+Return, the right-click
		// items and the gutter's "+": the core's decision, then the drawer
		// draft `begin_new_comment` opens.
		PendingLineComment.reset();
		auto       reason   = QString{ };
		const auto decision = decide_selected_lines(reason);
		if (!decision) {
			statusBar()->showMessage(reason, 10000);
			return false;
		}
		// A suggestion replaces head lines; a base line takes a comment only.
		if (aSuggestion && decision->Anchor.Side != core::diff_side::right) {
			statusBar()->showMessage(tr("Suggest change replaces lines on the right, head side. A base line takes a comment: Add comment, or Return."), 10000);
			return false;
		}
		// The draft composes on the range's last line, which GitHub anchors
		// it to.
		const auto selected = ComparisonSurface->selection().has_value() && ComparisonSurface->selection()->PreferredLine == decision->Anchor;
		if (!selected && !ComparisonSurface->select_conversation_line(decision->Anchor.Side, decision->Anchor.Line)) {
			statusBar()->showMessage(tr("Select a line of this comparison."), 10000);
			return false;
		}
		statusBar()->clearMessage();
		PendingLineComment = pending_line_comment{ .Anchor = decision->Anchor, .StartLine = decision->StartLine, .Suggestion = aSuggestion };
		if (resume_line_comment()) {
			return true;
		}
		if (!PendingLineComment.has_value()) {
			return false;
		}
		// No draft is on the line yet. The host answers a line selected just
		// now, which a right-click on a new line is ([[BPR-1212]] v002: the
		// owner had to left-click first). It opens when it arrives while the
		// line stays selected; if none comes within the wait, the status line
		// says why.
		const auto base  = decision->Anchor.Side == core::diff_side::left ? tr("base ") : QString{ };
		const auto lines = decision->StartLine.has_value() ? tr("%1lines %2–%3").arg(base).arg(*decision->StartLine + 1U).arg(decision->Anchor.Line + 1U)
														   : tr("%1line %2").arg(base).arg(decision->Anchor.Line + 1U);
		statusBar()->showMessage(tr("Opening a draft on %1…").arg(lines), line_comment_wait_ms);
		QTimer::singleShot(line_comment_wait_ms, this, [this, anchor = decision->Anchor] { report_line_comment_wait(anchor); });
		return true;
	}

	void workbench_window::report_line_comment_wait(const core::source_line_ref aAnchor)
	{
		if (!PendingLineComment.has_value() || PendingLineComment->Anchor != aAnchor) {
			return;
		}
		update_comment_target();
		statusBar()->showMessage(CommentAction != nullptr && !CommentAction->isEnabled()
									 ? CommentAction->toolTip()
									 : tr("No editable draft is available. Wait for the review to load, or open an active pull request with an account that can comment."),
								 10000);
	}

	auto workbench_window::resume_line_comment() -> bool
	{
		if (!PendingLineComment.has_value()) {
			return false;
		}
		// Another line selected since: the waiting comment is not wanted.
		const auto& selection = ComparisonSurface != nullptr ? ComparisonSurface->selection() : std::nullopt;
		if (!selection || selection->PreferredLine != PendingLineComment->Anchor) {
			PendingLineComment.reset();
			return false;
		}
		const auto& draft = ReviewEditor != nullptr ? ReviewEditor->shown_draft() : std::nullopt;
		if (!draft || draft->Anchor != PendingLineComment->Anchor) {
			return false;
		}
		const auto pending = *std::exchange(PendingLineComment, std::nullopt);
		// The range goes on the draft in one edit through the editor's edit
		// path, before any suggestion block is written from it. A reply is
		// left to begin_new_comment, which says to finish it first.
		if (!draft->ReplyToThread.has_value() && draft->StartLine != pending.StartLine && !ReviewEditor->change_range(pending.StartLine) && pending.StartLine.has_value()) {
			statusBar()->showMessage(tr("This draft cannot take several lines now. Select one line, or settle the draft's retained text first."), 10000);
			return false;
		}
		const auto opened = begin_new_comment(pending.Suggestion);
		if (opened) {
			statusBar()->clearMessage();
		}
		return opened;
	}

	auto workbench_window::add_comment_action() const noexcept -> QAction*
	{
		return AddCommentAction;
	}

	auto workbench_window::suggest_change_action() const noexcept -> QAction*
	{
		return SuggestChangeAction;
	}

	auto workbench_window::code_context_menu(QPlainTextEdit* const aPane, const QPoint aPosition) -> QMenu*
	{
		// [[BPR-1212]]. A right-click outside the selection selects its line
		// first, as a click would; the menu then names it.
		if (ComparisonSurface != nullptr && aPane != nullptr) {
			const auto  side      = aPane == ComparisonSurface->left_pane() ? core::diff_side::left : core::diff_side::right;
			const auto  block     = aPane->cursorForPosition(aPosition).blockNumber();
			const auto& selection = ComparisonSurface->selection();
			// [[BPR-1212]] slice 1. Inside the selected lines keeps them all.
			const auto range  = ComparisonSurface->line_draft_range();
			const auto line   = static_cast<std::uint32_t>(std::max(block, 0));
			const auto inside = selection.has_value() && selection->PreferredLine.Side == side
				&& (selection->PreferredLine.Line == line
					|| (range.has_value() && range->first.Side == side && range->second.Side == side
						&& line >= std::min(range->first.Line, range->second.Line) && line <= std::max(range->first.Line, range->second.Line)));
			if (block >= 0 && !inside) {
				static_cast<void>(ComparisonSurface->select_conversation_line(side, static_cast<std::uint32_t>(block)));
			}
		}
		update_comment_target();
		auto* const menu = new QMenu{ aPane != nullptr ? static_cast<QWidget*>(aPane) : this };
		menu->setObjectName(QStringLiteral("codeContextMenu"));
		menu->setToolTipsVisible(true);
		const auto& selection = ComparisonSurface != nullptr ? ComparisonSurface->selection() : std::nullopt;
		if (selection.has_value()) {
			// [[BPR-1212]] slice 1. "Lines a–b" when the selection covers several.
			const auto  range   = ComparisonSurface->line_draft_range();
			const auto  first   = range.has_value() ? std::min(range->first.Line, range->second.Line) : selection->PreferredLine.Line;
			const auto  last    = range.has_value() ? std::max(range->first.Line, range->second.Line) : selection->PreferredLine.Line;
			const auto  base    = selection->PreferredLine.Side == core::diff_side::left;
			auto* const caption = menu->addAction(first == last ? (base ? tr("Base line %1") : tr("Line %1")).arg(first + 1U)
																: (base ? tr("Base lines %1–%2") : tr("Lines %1–%2")).arg(first + 1U).arg(last + 1U));
			caption->setObjectName(QStringLiteral("codeContextMenuCaption"));
			caption->setEnabled(false);
			// [[BPR-1335]]. The line's thread, reachable without its marker.
			const auto side = selection->PreferredLine.Side;
			const auto line = selection->PreferredLine.Line;
			if (thread_on_line(side, line).has_value()) {
				auto* const open = menu->addAction(tr("Open thread"));
				open->setObjectName(QStringLiteral("codeContextMenuOpenThread"));
				open->setToolTip(tr("Open this line's thread in the drawer"));
				connect(open, &QAction::triggered, this, [this, side, line] { go_to_thread_at(side, line); });
				// [[BPR-1581]]. The same thread with the rest of the conversation.
				auto* const discuss = menu->addAction(tr("Show in Discuss"));
				discuss->setObjectName(QStringLiteral("codeContextMenuShowInDiscuss"));
				discuss->setToolTip(tr("Show this line's thread in Discuss"));
				if (ShowInDiscussAction != nullptr) {
					discuss->setShortcut(ShowInDiscussAction->shortcut());
					discuss->setShortcutContext(Qt::WidgetShortcut);
					discuss->setShortcutVisibleInContextMenu(true);
				}
				connect(discuss, &QAction::triggered, this, [this, side, line] {
					if (const auto identity = thread_card_on_line(side, line); identity.has_value()) {
						static_cast<void>(move_thread(*identity, core::workbench_thread_origin::review, core::workbench_thread_destination::discuss));
					}
				});
				// [[BPR-1586]]. And its row in Threads.
				auto* const threads = menu->addAction(tr("Show in Threads"));
				threads->setObjectName(QStringLiteral("codeContextMenuShowInThreads"));
				threads->setToolTip(tr("Show this line's thread in Threads"));
				if (ShowInThreadsAction != nullptr) {
					threads->setShortcut(ShowInThreadsAction->shortcut());
					threads->setShortcutContext(Qt::WidgetShortcut);
					threads->setShortcutVisibleInContextMenu(true);
				}
				connect(threads, &QAction::triggered, this, [this, side, line] {
					if (const auto identity = thread_card_on_line(side, line); identity.has_value()) {
						static_cast<void>(move_thread(*identity, core::workbench_thread_origin::review, core::workbench_thread_destination::threads));
					}
				});
			}
		}
		// Add comment and Suggest change start a new comment, never a reply.
		// [[BPR-1212]] v002. A line selected by this right-click has its draft
		// on the way, so its absence greys nothing: the item waits for it. A
		// reason the line cannot take a comment shows under the items.
		const auto reply = ReviewEditor != nullptr && ReviewEditor->shown_draft().has_value() && ReviewEditor->shown_draft()->ReplyToThread.has_value()
			&& selection.has_value() && ReviewEditor->shown_draft()->Anchor == selection->PreferredLine;
		auto words  = QString{ };
		auto reason = comment_target_reason(false, &words);
		if (reason.isEmpty() && reply) {
			reason = tr("Finish or cancel the reply on this line first.");
			words  = tr("Finish the reply on this line first");
		}
		// [[BPR-1212]] slice 1. Lines the core refuses together say why.
		auto decision = std::optional<core::comment_range_decision>{ };
		if (reason.isEmpty()) {
			decision = decide_selected_lines(reason, &words);
		}
		// A suggestion replaces head lines; a base line takes a comment only.
		auto suggestReason = reason;
		auto suggestWords  = words;
		if (suggestReason.isEmpty() && decision.has_value() && decision->Anchor.Side != core::diff_side::right) {
			suggestReason = tr("Suggest change replaces lines on the right, head side. A base line takes a comment.");
			suggestWords  = tr("Suggest change: head side only");
		}
		const auto several = decision.has_value() && decision->StartLine.has_value();
		for (auto* const action : { AddCommentAction, SuggestChangeAction }) {
			const auto& why = action == AddCommentAction ? reason : suggestReason;
			action->setEnabled(why.isEmpty());
			action->setToolTip(!why.isEmpty()                   ? why
								   : action == AddCommentAction ? (several ? tr("Write a comment on these lines, in the drawer beside the code") : tr("Write a comment on this line, in the drawer beside the code"))
								   : several                    ? tr("Suggest new text for these lines: a suggestion block GitHub can apply")
																: tr("Suggest new text for this line: a suggestion block GitHub can apply"));
			action->setStatusTip(action->toolTip());
			// The menu's item shows the code's key; it is the menu's own, so
			// no window shortcut ever claims Return.
			auto* const item = new QAction{ action->icon(), action->text(), menu };
			item->setObjectName(action->objectName());
			item->setShortcuts(shortcut_keys(action == AddCommentAction ? shortcut_command::add_comment : shortcut_command::suggest_change));
			item->setShortcutContext(Qt::WidgetShortcut);
			item->setEnabled(action->isEnabled());
			item->setToolTip(action->toolTip());
			item->setStatusTip(action->statusTip());
			connect(item, &QAction::triggered, action, &QAction::trigger);
			menu->addAction(item);
		}
		// [[BPR-1594]]. Ask the agent about these lines, in the Agent tab.
		if (AskAgentAction != nullptr) {
			auto* const ask = new QAction{ AskAgentAction->text(), menu };
			ask->setObjectName(AskAgentAction->objectName());
			ask->setShortcuts(AskAgentAction->shortcuts());
			ask->setShortcutContext(Qt::WidgetShortcut);
			ask->setToolTip(AskAgentAction->toolTip());
			connect(ask, &QAction::triggered, AskAgentAction, &QAction::trigger);
			menu->addAction(ask);
		}
		// The reason in words under the items, not only in a tooltip.
		if (const auto& shown = !words.isEmpty() ? words : suggestWords; !shown.isEmpty()) {
			auto* const why = menu->addAction(shown);
			why->setObjectName(QStringLiteral("codeContextMenuReason"));
			why->setEnabled(false);
			why->setToolTip(!reason.isEmpty() ? reason : suggestReason);
		}
		if (aPane != nullptr) {
			auto* const standard = aPane->createStandardContextMenu();
			standard->setParent(menu, standard->windowFlags());
			standard->hide();
			menu->addSeparator();
			for (auto* const action : standard->actions()) {
				menu->addAction(action);
			}
		}
		menu->addSeparator();
		menu->addAction(AddTourStopAction);
		return menu;
	}

	void workbench_window::resume_pending_draft()
	{
		if (!PendingDraft.has_value() || Drawers[1].Panel->state() != core::workbench_drawer_state::expanded) {
			return;
		}
		const auto suggestion = *std::exchange(PendingDraft, std::nullopt);
		// The block was already written when the drawer was asked for.
		static_cast<void>(suggestion);
		static_cast<void>(begin_new_comment(false));
	}

	auto workbench_window::reopen_draft_in_drawer() -> bool
	{
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr || ReviewEditor == nullptr || Modality == nullptr) {
			return false;
		}
		const auto& draft = ReviewEditor->shown_draft();
		if (!draft.has_value() || draft->ReplyToThread.has_value() || draft->shared() || Modality->current() != review_modality::review || showing_home()
			|| CompositionPage != nullptr || Drawers[1].Panel->state() != core::workbench_drawer_state::expanded
			|| (TrayTabs != nullptr && TrayTabs->currentIndex() != tray_comments_tab)) {
			return false;
		}
		WritingAnchor = draft->Anchor;
		present_inline_draft();
		ConversationPanel->cards()->begin_draft();
		apply_thread_marks();
		return ConversationPanel->cards()->writing_draft();
	}

	void workbench_window::leave_drawer_draft(const bool aDiscard)
	{
		// [[BPR-1212]]. Esc keeps a written draft at its line and drops an
		// empty one; Discard drops it through the editor's own Discard.
		if (ReviewEditor != nullptr && ReviewEditor->discard_action()->isEnabled()
			&& (aDiscard || ReviewEditor->body_editor()->toPlainText().trimmed().isEmpty())
			&& !ReviewEditor->body_editor()->toPlainText().isEmpty()) {
			ReviewEditor->discard_action()->trigger();
		}
		const auto side = WritingAnchor.has_value() ? WritingAnchor->Side : core::diff_side::right;
		WritingAnchor.reset();
		if (ConversationPanel != nullptr && ConversationPanel->cards() != nullptr) {
			ConversationPanel->cards()->end_draft();
		}
		refresh_cards();
		// Focus goes back to the code, on the same line, in its own pane.
		if (auto* const code = ComparisonSurface == nullptr ? nullptr
				: side == core::diff_side::left             ? ComparisonSurface->left_pane()
															: ComparisonSurface->right_pane();
			code != nullptr) {
			code->setFocus(Qt::OtherFocusReason);
		}
	}

	auto workbench_window::draft_source_lines() const -> std::optional<std::vector<std::string>>
	{
		if (ReviewEditor == nullptr || !ReviewEditor->shown_draft().has_value() || ComparisonSurface == nullptr
			|| ComparisonSurface->current_result() == nullptr) {
			return std::nullopt;
		}
		const auto& anchor = ReviewEditor->shown_draft()->Anchor;
		const auto& result = *ComparisonSurface->current_result();
		if (anchor.Comparison != result.Alignment.Digest) {
			return std::nullopt;
		}
		const auto& document = anchor.Side == core::diff_side::right ? result.RightDocument : result.LeftDocument;
		if (!document.has_value()) {
			return std::nullopt;
		}
		// [[BPR-1212]] slice 1. A draft on several lines suggests a change to
		// all of them, from its first line to its anchor.
		return core::suggestion_source_lines(*document, ReviewEditor->shown_draft()->StartLine.value_or(anchor.Line), anchor.Line);
	}

	void workbench_window::write_inline_draft(const QString& aText)
	{
		if (ReviewEditor == nullptr) {
			return;
		}
		const auto& draft = ReviewEditor->shown_draft();
		if (!draft.has_value() || draft->ReplyToThread.has_value() || draft->shared() || ReviewEditor->body_editor()->isReadOnly()
			|| (WritingAnchor.has_value() && draft->Anchor != *WritingAnchor)) {
			return;
		}
		if (ReviewEditor->body_editor()->toPlainText() != aText) {
			ReviewEditor->body_editor()->setPlainText(aText);
		}
	}

	void workbench_window::present_inline_draft()
	{
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr || ReviewEditor == nullptr) {
			return;
		}
		auto* const cards = ConversationPanel->cards();
		const auto& draft = ReviewEditor->shown_draft();
		const auto  body  = ReviewEditor->body_editor()->toPlainText();
		// The notes hold the accepted draft, so a draft left in the drawer
		// is its note at once; while it is written this lays nothing out.
		cards->set_draft(draft, ComparisonSurface != nullptr && ComparisonSurface->current_result() != nullptr ? std::optional{ ComparisonSurface->current_result()->Alignment.Digest } : std::nullopt);
		// What a suggestion block changes, against the exact head lines.
		auto change = std::optional<core::suggested_change>{ };
		if (draft.has_value() && core::classify_review_body(body.toStdString()) == core::review_body_compatibility::requires_line_subject) {
			if (const auto lines = draft_source_lines(); lines.has_value()) {
				if (auto decided = core::suggested_change_of(body.toStdString(), *lines); decided.Fenced) {
					change = std::move(decided);
				}
			}
		}
		// [[BPR-1212]] slice 1. A draft on several lines names them; v002, a
		// draft on the base side says so.
		const auto base  = draft.has_value() && draft->Anchor.Side == core::diff_side::left;
		const auto words = !draft.has_value()      ? QString{ }
			: draft->StartLine.has_value() && base ? tr("Base lines %1–%2").arg(*draft->StartLine + 1U).arg(draft->Anchor.Line + 1U)
			: draft->StartLine.has_value()         ? tr("Lines %1–%2").arg(*draft->StartLine + 1U).arg(draft->Anchor.Line + 1U)
			: base                                 ? tr("Base line %1").arg(draft->Anchor.Line + 1U)
												   : tr("Line %1").arg(draft->Anchor.Line + 1U);
		// The full composer's excerpt follows the same draft.
		if (DraftExcerpt != nullptr) {
			auto* const excerpt = static_cast<draft_excerpt_view*>(DraftExcerpt);
			auto        shown   = false;
			if (draft.has_value() && ComparisonSurface != nullptr && ComparisonSurface->current_result() != nullptr
				&& ComparisonSurface->current_context().has_value()
				&& draft->Anchor.Comparison == ComparisonSurface->current_result()->Alignment.Digest) {
				const auto& result   = *ComparisonSurface->current_result();
				const auto& document = draft->Anchor.Side == core::diff_side::right ? result.RightDocument : result.LeftDocument;
				if (document.has_value() && !document->Lines.empty()) {
					const auto last  = static_cast<std::uint32_t>(document->Lines.size() - 1U);
					const auto start = draft->StartLine.value_or(draft->Anchor.Line);
					const auto first = start > 3U ? start - 3U : 0U;
					const auto end   = std::min(last, draft->Anchor.Line + 3U);
					if (const auto lines = core::suggestion_source_lines(*document, first, end); lines.has_value()) {
						auto texts = std::vector<QString>{ };
						texts.reserve(lines->size());
						for (const auto& line : *lines) {
							texts.push_back(QString::fromStdString(line));
						}
						// The compared file's path, from the files drawer's rows.
						auto        path  = QString{ };
						const auto* model = Drawers[0].Panel != nullptr ? Drawers[0].Panel->contents() : nullptr;
						for (int row = 0; model != nullptr && row < model->rowCount() && path.isEmpty(); ++row) {
							const auto candidate = model->data(model->index(row, 0), Qt::DisplayRole).toString();
							if (core::file_identity_of(candidate.toStdString()) == ComparisonSurface->current_context()->Key.File) {
								path = candidate;
							}
						}
						excerpt->present(path, first + 1U, std::move(texts), start + 1U, draft->Anchor.Line + 1U,
										 cards->code_font());
						shown = true;
					}
				}
			}
			if (!shown) {
				excerpt->clear();
			}
			if (auto* const title = ComposeInlinePage != nullptr ? ComposeInlinePage->findChild<QLabel*>(QStringLiteral("composeInlineDraftPageTitle")) : nullptr; title != nullptr) {
				title->setText(shown ? excerpt->words() : QString{ });
			}
		}
		// The drawer's composer: the draft stops being written there once it
		// is published, answers a thread, or another line's draft shows.
		if (cards->writing_draft()
			&& (!draft.has_value() || draft->shared() || draft->ReplyToThread.has_value() || (WritingAnchor.has_value() && draft->Anchor != *WritingAnchor))) {
			WritingAnchor.reset();
			cards->end_draft();
			apply_thread_marks();
		}
		if (!draft.has_value() || draft->shared() || draft->ReplyToThread.has_value()) {
			cards->present_draft(std::nullopt);
			return;
		}
		// [[BPR-1329]]. What the last Publish came to comes first: publishing,
		// or why it did not happen. Otherwise, why Publish is unavailable.
		auto notice = ReviewEditor->publication_status();
		if (notice.isEmpty() && !ReviewEditor->publish_action()->isEnabled() && !body.trimmed().isEmpty()) {
			notice = ReviewEditor->publish_action()->toolTip();
		}
		cards->present_draft(draft_composer_presentation{
			.Body        = body,
			.Publishable = ReviewEditor->publish_action()->isEnabled(),
			.Editable    = !ReviewEditor->body_editor()->isReadOnly(),
			.Lines       = words,
			.Change      = std::move(change),
			.Notice      = notice,
			.Retry       = ReviewEditor->publication_retry_offered(),
			.Review      = draft_review(),
		});
	}

	auto workbench_window::draft_review() const -> draft_review_state
	{
		const auto& shown = ReviewEditor != nullptr ? ReviewEditor->shown_draft() : std::optional<core::review_comment_draft>{ };
		if (shown.has_value() && core::pending_in_review(*shown)) {
			return draft_review_state::pending;
		}
		return PendingReviewComments > 0U ? draft_review_state::in_progress : draft_review_state::none;
	}

	void workbench_window::update_review_chip()
	{
		if (ReviewChip == nullptr || ReviewChipAction == nullptr) {
			return;
		}
		auto* const chip = static_cast<review_progress_chip*>(ReviewChip);
		const auto  held = HeldReviewComments > 0U;
		if (chip->pending() != PendingReviewComments || chip->submitting() != held) {
			chip->set_count(PendingReviewComments, held);
		}
		const auto shown = PendingReviewComments > 0U || held;
		if (ReviewChipAction->isVisible() != shown) {
			ReviewChipAction->setVisible(shown);
			// [[BPR-1700]]. Compose review is the plain icon beside the chip.
			present_compose_entry();
			apply_strip_width();
		}
		if (FinishReviewAction != nullptr) {
			FinishReviewAction->setEnabled(PendingReviewComments > 0U && !held);
		}
	}

	auto workbench_window::finish_review_action() const noexcept -> QAction*
	{
		return FinishReviewAction;
	}

	auto workbench_window::delete_draft_confirmation() const noexcept -> destructive_question*
	{
		return DeleteDraftConfirmation.data();
	}

	void workbench_window::set_comment_draft_unavailable(QString aReason)
	{
		CommentDraftUnavailable = std::move(aReason);
		update_comment_target();
	}
	void workbench_window::set_pending_comment_delete_dispatch(std::function<void(core::review_draft_id, std::uint64_t)> aDispatch)
	{
		PendingCommentDeleteDispatch = std::move(aDispatch);
	}

	void workbench_window::confirm_delete_drawer_draft()
	{
		const auto* const body = ReviewEditor != nullptr ? ReviewEditor->body_editor() : nullptr;
		if (body == nullptr || body->toPlainText().trimmed().isEmpty()) {
			leave_drawer_draft(true);
			return;
		}
		if (DeleteDraftConfirmation != nullptr) {
			DeleteDraftConfirmation->close();
		}
		const auto& shown   = ReviewEditor->shown_draft();
		const auto  pending = shown.has_value() && core::pending_in_review(*shown);
		// [[BPR-1345]] v003. The shared question: the comment's place, then
		// the comment as this drawer reads it, in its box (owner,
		// delete-comment v002).
		auto* const box = new destructive_question{ *this, QStringLiteral("deleteDraftConfirmation"), pending ? tr("Delete this comment from your review?") : tr("Delete this comment?"),
													tr("This cannot be undone."), tr("Delete comment") };
		if (shown.has_value()) {
			const auto context = ComparisonSurface != nullptr ? ComparisonSurface->current_context() : std::nullopt;
			const auto path    = context.has_value() ? path_of(context->Key.File) : QString{ };
			auto*      cards   = ConversationPanel != nullptr ? ConversationPanel->cards() : nullptr;
			box->set_subject(path.section(QLatin1Char{ '/' }, -1), comment_lines(shown->Anchor.Side, shown->Anchor.Line, shown->StartLine), body->toPlainText().toStdString(),
							 "draft:" + std::to_string(shown->Id.value()), label_marks(), cards != nullptr ? cards->code_font() : font());
		}
		DeleteDraftConfirmation = box;
		const auto id           = shown.has_value() ? std::optional{ shown->Id } : std::nullopt;
		connect(box, &QDialog::accepted, this, [this, id] {
			// The draft asked about, and no other.
			const auto& now = ReviewEditor != nullptr ? ReviewEditor->shown_draft() : std::optional<core::review_comment_draft>{ };
			if (now.has_value() && std::optional{ now->Id } == id) {
				leave_drawer_draft(true);
			}
		});
		box->open();
	}

	void workbench_window::reveal_pending_comment(const QString& aPath, const core::diff_side aSide, const std::uint32_t aLine, const std::optional<core::review_draft_id> aDraft,
												  const core::workbench_thread_destination aDestination)
	{
		if (CompositionPage != nullptr) {
			close_review_composition();
		}
		PendingCommentOpen.reset();
		PendingCommentPick = pending_comment_pick{
			.File        = core::file_identity_of(aPath.toStdString()),
			.Side        = aSide,
			.Line        = aLine,
			.Before      = ComparisonSurface != nullptr && ComparisonSurface->current_result() != nullptr
				? std::optional{ ComparisonSurface->current_result()->Alignment.Digest }
				: std::nullopt,
			.Draft       = aDraft,
			.Destination = aDestination,
		};
		choose_home(false);
		choose_modality(review_modality::review);
		const auto context = ComparisonSurface != nullptr ? ComparisonSurface->current_context() : std::nullopt;
		if (!context.has_value() || context->Key.File != PendingCommentPick->File) {
			if (const auto file = file_for_path(aPath); file.has_value() && Dispatch) {
				Dispatch(core::select_workbench_file_intent{ .File = *file });
			}
		}
		resume_pending_comment_pick();
	}

	void workbench_window::open_pending_comment(const core::review_submission_comment& aComment, const core::workbench_thread_destination aDestination)
	{
		// [[BPR-1581]]. The one route: my draft on a file opens beside its
		// line, in its composer for Discuss, in its row for Threads.
		const auto route = core::route_workbench_thread(core::open_workbench_thread_intent{
			.Destination = aDestination,
			.Origin      = core::workbench_thread_origin::review,
			.Draft       = true,
			.OnFile      = !aComment.Path.empty(),
			.Placed      = true,
		});
		if (!route.Available) {
			statusBar()->showMessage(tr("This comment is not on a file, so it has no line to open at."));
			return;
		}
		// Finish your review gives way: the sheet closes, the expanded page is left.
		if (CompositionPage != nullptr && CompositionPage == ComposeReviewPage) {
			close_review_composition(false);
		}
		close_review_sheet();
		reveal_pending_comment(QString::fromStdString(aComment.Path), aComment.Side, aComment.Line, aComment.Draft, aDestination);
	}

	void workbench_window::finish_pending_comment_pick(const pending_comment_pick& aPick)
	{
		if (!aPick.Draft.has_value() || ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return;
		}
		auto* const cards = ConversationPanel->cards();
		// The draft opens once the drawer holds it: choosing its line brings
		// a saved draft there, so until then it waits.
		const auto card = std::string{ "draft:unpublished" };
		if (ReviewEditor == nullptr || !ReviewEditor->shown_draft().has_value() || ReviewEditor->shown_draft()->Id != *aPick.Draft) {
			const auto saved = card + ":" + std::to_string(aPick.Draft->value());
			if (std::ranges::any_of(cards->cards(), [&saved](const thread_card& aCard) { return aCard.Id == saved; })) {
				PendingCommentOpen = aPick;
			} else {
				statusBar()->showMessage(tr("The comment's line is shown; its note opens once its draft is read."));
			}
			return;
		}
		PendingCommentOpen.reset();
		switch (aPick.Destination) {
		case core::workbench_thread_destination::discuss:
			static_cast<void>(show_draft_in_discuss(card));
			return;
		case core::workbench_thread_destination::threads:
			refresh_cards();
			if (!show_thread_in_threads(card, std::string{ })) {
				statusBar()->showMessage(tr("Threads lists your draft once it is open beside its line."));
			}
			return;
		case core::workbench_thread_destination::review:
			break;
		}
		if (Dispatch && Drawers[1].Panel->state() != core::workbench_drawer_state::expanded) {
			Dispatch(core::set_workbench_drawer_intent{ .Drawer = core::workbench_drawer::conversations, .State = core::workbench_drawer_state::expanded });
		}
		if (TrayTabs != nullptr && TrayTabs->currentIndex() != tray_comments_tab) {
			TrayTabs->setCurrentIndex(tray_comments_tab);
		}
		cards->open_thread(card);
		if (auto* const open = cards->open_card(); open != nullptr) {
			open->setFocus(Qt::OtherFocusReason);
		} else {
			cards->setFocus(Qt::OtherFocusReason);
		}
	}

	void workbench_window::resume_pending_comment_open()
	{
		if (!PendingCommentOpen.has_value() || ReviewEditor == nullptr || ComparisonSurface == nullptr) {
			return;
		}
		// The reviewer went on to another file: the opening is dropped.
		if (const auto context = ComparisonSurface->current_context(); !context.has_value() || context->Key.File != PendingCommentOpen->File) {
			PendingCommentOpen.reset();
			return;
		}
		if (const auto& shown = ReviewEditor->shown_draft(); shown.has_value() && shown->Id == PendingCommentOpen->Draft) {
			const auto pick = *PendingCommentOpen;
			PendingCommentOpen.reset();
			finish_pending_comment_pick(pick);
		}
	}

	void workbench_window::resume_pending_comment_pick()
	{
		if (!PendingCommentPick.has_value() || ComparisonSurface == nullptr || (Modality != nullptr && Modality->current() != review_modality::review)) {
			return;
		}
		const auto context = ComparisonSurface->current_context();
		const auto digest  = ComparisonSurface->current_result() != nullptr ? std::optional{ ComparisonSurface->current_result()->Alignment.Digest } : std::nullopt;
		if (context.has_value() && context->Key.File == PendingCommentPick->File && digest.has_value()) {
			const auto pick = *PendingCommentPick;
			PendingCommentPick.reset();
			static_cast<void>(ComparisonSurface->select_conversation_line(pick.Side, pick.Line));
			finish_pending_comment_pick(pick);
			return;
		}
		// Another file's comparison arrived first: the reviewer went on.
		if (context.has_value() && context->Key.File != PendingCommentPick->File && digest.has_value() && digest != PendingCommentPick->Before) {
			PendingCommentPick.reset();
		}
	}

	auto workbench_window::review_chip() const noexcept -> QToolButton*
	{
		return ReviewChip;
	}

	auto workbench_window::review_chip_action() const noexcept -> QAction*
	{
		return ReviewChipAction;
	}

	void workbench_window::set_start_review_dispatch(std::function<bool()> aDispatch)
	{
		StartReviewDispatch = std::move(aDispatch);
	}

	void workbench_window::set_unsent_reply_dispatch(std::function<void(core::review_draft_id, unsent_reply_action)> aDispatch)
	{
		UnsentReplyDispatch = std::move(aDispatch);
	}

	void workbench_window::act_on_kept_reply(const card_action aAction, const std::string& aIdentity)
	{
		// [[BPR-1374]]. The kept reply the note names, and no other.
		const auto& cards = ConversationPanel->cards()->cards();
		const auto  card  = std::ranges::find(cards, aIdentity, &thread_card::Id);
		if (card == cards.end() || !card->Draft || !card->UnsentReply.has_value() || !UnsentReplyDispatch) {
			return;
		}
		const auto prefix = conversation_cards_view::kept_reply_identity(core::review_draft_id{ 0U });
		const auto stem   = std::string_view{ prefix }.substr(0U, prefix.size() - 1U);
		auto       value  = std::uint64_t{ 0U };
		const auto digits = std::string_view{ aIdentity }.substr(std::min(stem.size(), aIdentity.size()));
		if (!aIdentity.starts_with(stem) || std::from_chars(digits.data(), digits.data() + digits.size(), value).ec != std::errc{ } || value == 0U) {
			return;
		}
		const auto draft = core::review_draft_id{ value };
		if (aAction == card_action::keep_reply_as_comment) {
			UnsentReplyDispatch(draft, unsent_reply_action::keep_as_comment);
			return;
		}
		if (DiscardReplyConfirmation != nullptr) {
			DiscardReplyConfirmation->close();
		}
		auto* const box = new destructive_question{ *this, QStringLiteral("discardReplyConfirmation"), tr("Discard this reply?"),
													tr("It was never sent, so GitHub does not have these words. This cannot be undone."), tr("Discard reply") };
		if (!card->Posts.empty() && card->Line.has_value()) {
			const auto context = ComparisonSurface != nullptr ? ComparisonSurface->current_context() : std::nullopt;
			const auto path    = context.has_value() ? path_of(context->Key.File) : QString{ };
			box->set_subject(path.section(QLatin1Char{ '/' }, -1), comment_lines(card->Side.value_or(core::diff_side::right), *card->Line - 1U, std::nullopt),
							 card->Posts.front().Body.toStdString(), aIdentity, label_marks(), ConversationPanel->cards()->code_font());
		}
		DiscardReplyConfirmation = box;
		connect(box, &QDialog::accepted, this, [this, draft] {
			if (UnsentReplyDispatch) {
				UnsentReplyDispatch(draft, unsent_reply_action::discard);
			}
		});
		box->open();
	}

	auto workbench_window::overview_review_key() const -> QString
	{
		const auto* const page = OverviewPage;
		if (page == nullptr || !page->projection()) {
			return { };
		}
		const auto& shown = *page->projection();
		return QStringLiteral("%1/%2#%3").arg(QString::fromStdString(shown.Owner), QString::fromStdString(shown.RepositoryName), QString::number(shown.Number.value()));
	}

	void workbench_window::enter_review_at(const QString& aPath)
	{
		// [[BPR-1588]]. A file the reviewer picked is never replaced by the
		// first unseen change.
		ReviewEnteredFor = overview_review_key();
		// [[BPR-1348]]. Review first, then the file, as a conversation pick
		// does. Choosing Review schedules its active tab's presentation
		// ([[BPR-1132]]), so a file chosen before it lost to the tab's file:
		// a double-click on main.cpp opened renderer.cpp.
		choose_modality(review_modality::review);
		if (const auto file = file_for_path(aPath); file.has_value() && Dispatch) {
			Dispatch(core::select_workbench_file_intent{ .File = *file });
		}
	}

	auto workbench_window::select_thread_line(const std::string& aIdentity) -> bool
	{
		if (ConversationPanel == nullptr || ComparisonSurface == nullptr) {
			return false;
		}
		// The thread's anchor, from the panel's list when it holds the thread.
		// [[BPR-1133]]. Otherwise from its card: while a line is selected the
		// list holds only that line's threads, and a card beside the code can
		// answer a thread on another line.
		auto path     = QString{ };
		auto side     = std::optional<core::diff_side>{ };
		auto oneBased = std::uint64_t{ 0U };
		auto current  = false;
		auto root     = ConversationPanel->model()->index_for_id(aIdentity);
		while (root.isValid() && root.parent().isValid()) {
			root = root.parent();
		}
		if (root.isValid()) {
			current = root.data(github_conversation_model::anchor_availability_role).toInt()
				== static_cast<int>(application::github_conversation_anchor_availability::current);
			path                 = root.data(github_conversation_model::path_role).toString();
			auto       validLine = false;
			const auto line      = root.data(github_conversation_model::line_role).toULongLong(&validLine);
			oneBased             = validLine ? line : 0U;
			if (const auto sideValue = root.data(github_conversation_model::diff_side_role); sideValue.isValid()) {
				side = github_conversation_source_side(sideValue.toInt());
			}
		} else if (ConversationPanel->cards() != nullptr) {
			const auto& cards = ConversationPanel->cards()->cards();
			const auto  card  = std::ranges::find(cards, aIdentity, &thread_card::Id);
			if (card != cards.end() && !card->Draft) {
				current  = !card->AnchorUnavailable;
				path     = card->Path;
				side     = card->Side;
				oneBased = card->Line.value_or(0U);
			}
		}
		const auto context = ComparisonSurface->current_context();
		if (!current || !context.has_value() || path.isEmpty()
			|| context->Key.File != core::file_identity_of(path.toStdString())) {
			return false;
		}
		if (oneBased == 0U || oneBased > std::numeric_limits<std::uint32_t>::max()) {
			return false;
		}
		if (!side.has_value()
			|| (*side == core::diff_side::left
					? !LeftConversationEndpointCurrent
					: !RightConversationEndpointCurrent)) {
			return false;
		}
		return ComparisonSurface->select_conversation_line(*side, static_cast<std::uint32_t>(oneBased - 1U));
	}

	auto workbench_window::thread_comparison_loading(const std::string& aIdentity) const -> bool
	{
		if (ConversationPanel == nullptr || ComparisonSurface == nullptr
			|| ComparisonSurface->current_result() != nullptr) {
			return false;
		}
		auto root = ConversationPanel->model()->index_for_id(aIdentity);
		while (root.isValid() && root.parent().isValid()) {
			root = root.parent();
		}
		const auto context = ComparisonSurface->current_context();
		const auto path    = root.isValid()
			? root.data(github_conversation_model::path_role).toString()
			: QString{ };
		return root.isValid()
			&& root.data(github_conversation_model::anchor_availability_role).toInt()
			== static_cast<int>(application::github_conversation_anchor_availability::current)
			&& context.has_value() && !path.isEmpty()
			&& context->Key.File == core::file_identity_of(path.toStdString());
	}

	void workbench_window::begin_thread_reply(const std::string& aIdentity, const bool aInPlace)
	{
		if (select_thread_line(aIdentity) && ReviewReplyDispatch && ReviewEditor != nullptr) {
			ReviewReplyDispatch(aIdentity);
			update_comment_target();
			// [[BPR-1290]]. Beside the code the reply is written in place, at
			// the foot of the open thread; it is the same draft, and nothing
			// publishes but its Publish.
			const auto discuss = Modality != nullptr && (Modality->current() == review_modality::discuss || Modality->current() == review_modality::threads);
			const auto inPlace = aInPlace && !discuss && Drawers[1].Panel->state() == core::workbench_drawer_state::expanded
				&& TrayTabs != nullptr && TrayTabs->currentIndex() == tray_comments_tab
				&& std::ranges::any_of(ConversationPanel->cards()->cards(), [&aIdentity](const thread_card& aCard) { return aCard.Id == aIdentity; });
			if (inPlace) {
				ConversationPanel->cards()->begin_reply(aIdentity);
				present_inline_reply();
				return;
			}
			if (CommentAction->isEnabled()) {
				open_review_composition(true);
			} else {
				ConversationPanel->present_notice(CommentAction->toolTip());
			}
			return;
		}
		ConversationPanel->present_notice(tr("Open this thread in code before replying."));
	}

	void workbench_window::resume_deferred_reply()
	{
		// The first comparison to arrive decides: the thread's own completes
		// the reply, any other refuses it, and nothing waits past it.
		if (!DeferredReply.has_value() || ComparisonSurface == nullptr
			|| ComparisonSurface->current_result() == nullptr) {
			return;
		}
		const auto identity = std::exchange(DeferredReply, std::nullopt);
		begin_thread_reply(*identity, DeferredReplyInPlace);
	}

	void workbench_window::set_review_reply_dispatch(workbench_review_reply_dispatch aDispatch)
	{
		ReviewReplyDispatch = std::move(aDispatch);
	}

	void workbench_window::set_thread_resolution_dispatch(workbench_thread_resolution_dispatch aDispatch)
	{
		ThreadResolutionDispatch = std::move(aDispatch);
	}

	void workbench_window::set_thread_resolution_provider(const bool aResolves)
	{
		ProviderResolvesThreads = aResolves;
		if (ConversationPanel != nullptr && ConversationPanel->cards() != nullptr) {
			ConversationPanel->cards()->set_resolution_offered(
				ProviderResolvesThreads
				&& (Capabilities.ResolveThread == core::github_capability_state::supported
					|| Capabilities.UnresolveThread == core::github_capability_state::supported));
		}
	}

	void workbench_window::set_behaviour_dispatch(review_behaviour_dispatch aDispatch)
	{
		BehaviourDispatch = std::move(aDispatch);
	}

	auto workbench_window::behaviour() const noexcept -> const application::review_behaviour_settings&
	{
		return Behaviour;
	}

	auto workbench_window::behaviour_editor() const noexcept -> QWidget*
	{
		return BehaviourEditor;
	}

	auto workbench_window::feedback_marks_preview() const noexcept -> feedback_mark_preview*
	{
		return FeedbackMarksPreview;
	}

	auto workbench_window::label_helper_switch() const noexcept -> QCheckBox* { return LabelHelperSwitch; }
	auto workbench_window::label_columns_editor() const noexcept -> feedback_label_columns_editor* { return LabelColumnsEditor; }
	auto workbench_window::label_columns_recovery() const noexcept -> QLabel* { return LabelColumnsRecovery; }
	auto workbench_window::mark_colours_link() const noexcept -> QToolButton* { return MarkColoursLink; }
	auto workbench_window::workshop_guidance_link() const noexcept -> QToolButton* { return WorkshopGuidanceLink; }
	auto workbench_window::conventional_comments_caption() const noexcept -> QLabel* { return ConventionalCommentsCaption; }
	auto workbench_window::conventional_comments_link() const noexcept -> QToolButton* { return ConventionalCommentsLink; }
	auto workbench_window::conventional_comments_status() const noexcept -> QLabel* { return ConventionalCommentsStatus; }

	// [[BPR-0661]]. Two groups of segmented rows: the modality rules (which
	// modality a pull request opens in; which one Continue review enters) and
	// the reading behaviour (when a file is marked read; the comparison on
	// return; the comments drawer's first filter). Each group resets on its
	// own. A row dispatches the whole record; the host validates, persists and
	// publishes it, and the page follows what is published, so a refused
	// record reverts.
	void workbench_window::build_behaviour_editor(QWidget* const aReviewPage, QVBoxLayout* const aReviewColumn, QWidget* const aCommentsPage, QVBoxLayout* const aCommentsColumn)
	{
		BehaviourEditor = new QWidget{ aReviewPage };
		BehaviourEditor->setObjectName(QStringLiteral("reviewBehaviourEditor"));
		BehaviourEditor->setAccessibleName(QStringLiteral("Review behaviour"));
		auto* const column = new QVBoxLayout{ BehaviourEditor };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(6);
		// [[BPR-1597]]. Settings › Comments: the comment rows, on their own page.
		CommentsEditor = new QWidget{ aCommentsPage };
		CommentsEditor->setObjectName(QStringLiteral("commentsBehaviourEditor"));
		CommentsEditor->setAccessibleName(QStringLiteral("Comments"));
		auto* const commentsColumn = new QVBoxLayout{ CommentsEditor };
		commentsColumn->setContentsMargins(0, 0, 0, 0);
		commentsColumn->setSpacing(6);

		struct option
		{
			QString Label;
			QString Name;
		};
		struct row
		{
			std::size_t         Index;
			QString             Label;
			QString             Name;
			std::vector<option> Options;
			/// Drawn after the choices, as the Label marks preview is.
			QWidget* Trailing = nullptr;
			/// [[BPR-1088]]. A row that is one control rather than a choice
			/// of buttons: a switch, a link or a list.
			QWidget* Control = nullptr;
			/// The space before `Trailing`: wide for a preview, narrow for an
			/// info glyph.
			int TrailingSpacing = 12;
			/// The row's name at the top of a tall control.
			bool Top = false;
			/// [[BPR-1566]]. A line of its own under the heading, with no
			/// name column: `Control` takes the whole width.
			bool Wide = false;
		};
		const auto addGroup = [this](QVBoxLayout* const      aColumn,
									 const QString&          aHeading,
									 const QString&          aResetName,
									 const std::vector<row>& aRows,
									 auto                    aReset) {
			auto* const headingRow = new QHBoxLayout{ };
			auto* const heading    = new QLabel{ aHeading, BehaviourEditor };
			heading->setObjectName(aResetName + QStringLiteral("Heading"));
			headingRow->addWidget(heading, 1);
			auto* const reset = new QToolButton{ BehaviourEditor };
			reset->setObjectName(aResetName);
			reset->setText(QStringLiteral("Reset"));
			reset->setAccessibleName(QStringLiteral("Reset %1").arg(aHeading.toLower()));
			reset->setAccessibleDescription(
				QStringLiteral("Returns this group to its defaults; the other group is left alone"));
			reset->setFocusPolicy(Qt::StrongFocus);
			connect(reset, &QToolButton::clicked, this, [this, aReset] {
				if (!BehaviourDispatch) {
					return;
				}
				auto next = behaviour_from_controls();
				aReset(next);
				AskedBehaviour = next;
				BehaviourDispatch(next);
			});
			headingRow->addWidget(reset);
			aColumn->addLayout(headingRow);
			for (const auto& one : aRows) {
				if (one.Wide && one.Control != nullptr) {
					aColumn->addWidget(one.Control);
					continue;
				}
				auto* const line  = new QHBoxLayout{ };
				auto* const label = new QLabel{ one.Label, BehaviourEditor };
				label->setObjectName(one.Name + QStringLiteral("Label"));
				label->setMinimumWidth(180);
				line->addWidget(label, 0, one.Top ? Qt::AlignTop : Qt::Alignment{ });
				if (one.Control != nullptr) {
					// [[BPR-1597]]. A control made before its group's heading
					// follows it in the page's child order, which is the
					// order Settings search reads headings and rows in.
					one.Control->raise();
					if (one.Trailing != nullptr) {
						one.Trailing->raise();
					}
					label->setBuddy(one.Control);
					line->addWidget(one.Control, 0, one.Top ? Qt::AlignTop : Qt::Alignment{ });
					if (one.Trailing != nullptr) {
						line->addSpacing(one.TrailingSpacing);
						line->addWidget(one.Trailing, 0, Qt::AlignVCenter);
					}
					line->addStretch(1);
					aColumn->addLayout(line);
					continue;
				}
				auto& buttons = BehaviourOptions[one.Index];
				// One exclusive group per row: autoExclusive would make every
				// button under the editor one group.
				auto* const group = new QButtonGroup{ BehaviourEditor };
				group->setExclusive(true);
				for (const auto& choice : one.Options) {
					auto* const button = new QToolButton{ BehaviourEditor };
					button->setObjectName(choice.Name);
					button->setText(choice.Label);
					button->setAccessibleName(one.Label + QStringLiteral(": ") + choice.Label);
					button->setCheckable(true);
					group->addButton(button);
					button->setFocusPolicy(Qt::StrongFocus);
					connect(button, &QToolButton::toggled, this, [this](const bool aChecked) {
						if (aChecked && !ApplyingBehaviour) {
							dispatch_behaviour_from_controls();
						}
					});
					line->addWidget(button);
					buttons.push_back(button);
				}
				if (one.Trailing != nullptr) {
					line->addSpacing(one.TrailingSpacing);
					line->addWidget(one.Trailing, 0, Qt::AlignVCenter);
				}
				line->addStretch(1);
				aColumn->addLayout(line);
			}
			return reset;
		};
		// [[BPR-1597]]. Settings › Review, by task (owner's pick A,
		// 2026-10-17): how a pull request opens, how it reads, and its layout.
		// Each group's Reset covers its own rows.
		addGroup(
			column,
			QStringLiteral("Opening"),
			QStringLiteral("resetOpening"),
			{
				row{ 0U,
					 QStringLiteral("A pull request opens in"),
					 QStringLiteral("openModality"),
					 { { QStringLiteral("Overview"), QStringLiteral("openModalityOverview") },
					   { QStringLiteral("Review"), QStringLiteral("openModalityReview") },
					   { QStringLiteral("Last used"), QStringLiteral("openModalityLastUsed") } } },
				// [[BPR-1295]]. Row 1, "Continue review enters", went with
				// Read: Start and Continue review both enter Review.
				row{ 3U,
					 QStringLiteral("On return, compare"),
					 QStringLiteral("returnComparison"),
					 { { QStringLiteral("Whole PR"), QStringLiteral("returnComparisonWholePr") },
					   { QStringLiteral("Since my last review"), QStringLiteral("returnComparisonSinceLastReview") },
					   { QStringLiteral("Since last upload"), QStringLiteral("returnComparisonSinceLastUpload") } } },
			},
			[](application::review_behaviour_settings& aNext) {
				aNext.OpenModality     = application::review_behaviour_settings{ }.OpenModality;
				aNext.ReturnComparison = application::review_behaviour_settings{ }.ReturnComparison;
			});
		// [[BPR-1584]]. The owner, 2026-10-17: "We can make that capped width
		// a setting." Threads and Discuss only; never the code comparison.
		// [[BPR-1597]]: a row of Review › Reading.
		auto* const readingInfo = new info_glyph{ QStringLiteral("Threads and Discuss stop at this width on a wide screen. The code comparison always uses the whole window."), BehaviourEditor };
		readingInfo->setObjectName(QStringLiteral("readingWidthInfo"));
		addGroup(
			column,
			QStringLiteral("Reading"),
			QStringLiteral("resetReadingBehaviour"),
			{
				row{ 2U,
					 QStringLiteral("Mark a file read"),
					 QStringLiteral("markRead"),
					 { { QStringLiteral("By hand"), QStringLiteral("markReadByHand") },
					   { QStringLiteral("When fully seen"), QStringLiteral("markReadWhenFullySeen") } } },
				row{ 4U,
					 QStringLiteral("Comments drawer opens on"),
					 QStringLiteral("commentsFilter"),
					 { { QStringLiteral("All"), QStringLiteral("commentsFilterAll") },
					   { QStringLiteral("Unread"), QStringLiteral("commentsFilterUnread") },
					   { QStringLiteral("Unresolved"), QStringLiteral("commentsFilterUnresolved") } } },
				row{ 10U,
					 QStringLiteral("Reading width"),
					 QStringLiteral("readingWidth"),
					 { { QStringLiteral("Narrow"), QStringLiteral("readingWidthNarrow") },
					   { QStringLiteral("Medium"), QStringLiteral("readingWidthMedium") },
					   { QStringLiteral("Wide"), QStringLiteral("readingWidthWide") },
					   { QStringLiteral("Full width"), QStringLiteral("readingWidthFull") } },
					 readingInfo,
					 nullptr,
					 4 },
			},
			[](application::review_behaviour_settings& aNext) {
				aNext.MarkRead       = application::review_behaviour_settings{ }.MarkRead;
				aNext.CommentsFilter = application::review_behaviour_settings{ }.CommentsFilter;
				aNext.ReadingWidth   = core::default_reading_width;
			});
		// [[BPR-0721]]. The owner found the collapsed rails and the Hide
		// buttons took room the strip's toggles already cover, and asked for
		// a setting rather than their removal. Off is the default.
		CenterRibbonWidth = new QSpinBox{ BehaviourEditor };
		CenterRibbonWidth->setObjectName(QStringLiteral("centerRibbonWidth"));
		CenterRibbonWidth->setAccessibleName(QStringLiteral("Center ribbon width"));
		CenterRibbonWidth->setAccessibleDescription(QStringLiteral("Width between the two source panes, in logical pixels"));
		CenterRibbonWidth->setRange(static_cast<int>(application::minimum_center_ribbon_width), static_cast<int>(application::maximum_center_ribbon_width));
		CenterRibbonWidth->setSuffix(QStringLiteral(" px"));
		CenterRibbonWidth->setKeyboardTracking(false);
		connect(CenterRibbonWidth, &QSpinBox::valueChanged, this, [this] {
			if (!ApplyingBehaviour) {
				dispatch_behaviour_from_controls();
			}
		});
		// [[BPR-1334]]. The owner, 2026-10-08: "an optional on by default view
		// page below the side by side". The same choice as View > Line Details.
		// [[BPR-0721]] drawer controls and the comparison's line details and
		// ribbon are one Layout group since [[BPR-1597]].
		addGroup(
			column,
			QStringLiteral("Layout"),
			QStringLiteral("resetLayout"),
			{
				row{ 5U,
					 QStringLiteral("Drawer controls"),
					 QStringLiteral("drawerControls"),
					 { { QStringLiteral("Hidden"), QStringLiteral("drawerControlsHidden") },
					   { QStringLiteral("Shown"), QStringLiteral("drawerControlsShown") } } },
				row{ 8U,
					 QStringLiteral("Line details"),
					 QStringLiteral("lineDetails"),
					 { { QStringLiteral("Shown"), QStringLiteral("lineDetailsShown") },
					   { QStringLiteral("Hidden"), QStringLiteral("lineDetailsHidden") } } },
				row{ 0U, QStringLiteral("Center ribbon width"), QStringLiteral("centerRibbonWidth"), { }, nullptr, CenterRibbonWidth },
			},
			[](application::review_behaviour_settings& aNext) {
				aNext.DrawerControls    = application::review_behaviour_settings{ }.DrawerControls;
				aNext.LineDetails       = application::review_behaviour_settings{ }.LineDetails;
				aNext.CenterRibbonWidth = application::default_center_ribbon_width;
			});
		// RFC-BPR-0084 Q3. The owner, 2026-10-16, accepted a "Load images in
		// descriptions and comments" preference, on by default.
		auto* const imagesInfo = new info_glyph{ QStringLiteral("Images in pull request descriptions and comments load as they do on GitHub. Don't load: each stays a link with its description, and nothing is fetched."), BehaviourEditor };
		imagesInfo->setObjectName(QStringLiteral("markdownImagesInfo"));
		// [[BPR-1083]]. The owner, 2026-10-05: "i think settings should also
		// have a toggle to switch between the icons and the shortcode icons,
		// because i liked both of them." Icons are the default. The Comment
		// labels section's other rows are BPR-1088's. The owner, accepting it
		// (mark-style-v001): "it'd be nice to see a preview", so a few marks
		// in the published style sit beside the choices.
		FeedbackMarksPreview = new feedback_mark_preview{ BehaviourEditor };
		// [[BPR-1088]]. The rest of the section the owner accepted (BPR-1079
		// Settings v1, adjusted to Overview chips in v2): where the Overview's
		// counts start, Open by default; a link to the label tints; and the
		// Overview chips, ticked and put in order. Each row dispatches the
		// whole record and shows what the host publishes, so a refused or
		// failed save reverts here.
		//
		// [[BPR-1568]]. The label helper, which since [[BPR-1565]] only
		// decides whether a comment workshop request carries the Review
		// feedback guidance ([[BPR-1091]]), is the Comment workshop group's
		// own row below (owner, 2026-10-16: "Just give it it's own section
		// for comments or something."), with its own Reset and a link to
		// where the guidance text is edited. Same stored preference.
		LabelHelperSwitch = new QCheckBox{ QStringLiteral("Send with comment workshop requests"), BehaviourEditor };
		LabelHelperSwitch->setObjectName(QStringLiteral("labelHelperSwitch"));
		LabelHelperSwitch->setAccessibleName(QStringLiteral("Feedback guidance: send with comment workshop requests"));
		LabelHelperSwitch->setFocusPolicy(Qt::StrongFocus);
		connect(LabelHelperSwitch, &QCheckBox::toggled, this, [this] {
			if (!ApplyingBehaviour) {
				dispatch_behaviour_from_controls();
			}
		});
		auto* const helperRow    = new QWidget{ BehaviourEditor };
		auto* const helperLayout = new QHBoxLayout{ helperRow };
		helperLayout->setContentsMargins(0, 0, 0, 0);
		helperLayout->setSpacing(4);
		helperLayout->addWidget(LabelHelperSwitch);
		auto* const helperInfo = new info_glyph{ QStringLiteral("The Review feedback guidance tells the agent how comment labels and decorations work. On: each comment workshop request carries it. Off, the default: requests go without it. Labels count either way."), helperRow };
		helperInfo->setObjectName(QStringLiteral("labelHelperInfo"));
		helperLayout->addWidget(helperInfo);
		LabelHelperSwitch->setAccessibleDescription(helperInfo->consequence());
		// Where the guidance text lives, as Mark colours points at the tints.
		WorkshopGuidanceLink = new QToolButton{ BehaviourEditor };
		WorkshopGuidanceLink->setObjectName(QStringLiteral("workshopGuidanceLink"));
		// "&&" draws one ampersand: a single "&" would be a mnemonic.
		WorkshopGuidanceLink->setText(QStringLiteral("Agents && context › Review feedback ›"));
		WorkshopGuidanceLink->setAutoRaise(true);
		WorkshopGuidanceLink->setCursor(Qt::PointingHandCursor);
		WorkshopGuidanceLink->setFocusPolicy(Qt::StrongFocus);
		WorkshopGuidanceLink->setToolTip(QStringLiteral("Opens the Review feedback writing instructions, where the guidance text is edited"));
		WorkshopGuidanceLink->setAccessibleName(QStringLiteral("Guidance text: Agents & context, Review feedback"));
		WorkshopGuidanceLink->setAccessibleDescription(WorkshopGuidanceLink->toolTip());
		anchor_tooltips(*WorkshopGuidanceLink);
		{
			auto linkPalette = WorkshopGuidanceLink->palette();
			linkPalette.setColor(QPalette::ButtonText, readable_link_colour(linkPalette));
			WorkshopGuidanceLink->setPalette(linkPalette);
		}
		connect(WorkshopGuidanceLink, &QToolButton::clicked, this, [this] {
			if (SettingsWindow == nullptr) {
				return;
			}
			SettingsWindow->present(settings_page::agents_and_context);
			if (WritingHooks != nullptr) {
				WritingHooks->select_hook(core::writing_hook::feedback);
			}
		});
		auto* const countsInfo = new info_glyph{ QStringLiteral("Where the Overview's comment counts start. Open counts unresolved threads; All counts every one. Switching on an Overview changes only that view."), BehaviourEditor };
		countsInfo->setObjectName(QStringLiteral("overviewCountsInfo"));
		MarkColoursLink = new QToolButton{ BehaviourEditor };
		MarkColoursLink->setObjectName(QStringLiteral("markColoursLink"));
		MarkColoursLink->setText(QStringLiteral("Appearance › Colours ›"));
		MarkColoursLink->setAutoRaise(true);
		MarkColoursLink->setCursor(Qt::PointingHandCursor);
		MarkColoursLink->setFocusPolicy(Qt::StrongFocus);
		MarkColoursLink->setToolTip(QStringLiteral("Opens Appearance with the filter set to Label"));
		MarkColoursLink->setAccessibleName(QStringLiteral("Mark colours: Appearance, Colours"));
		MarkColoursLink->setAccessibleDescription(MarkColoursLink->toolTip());
		anchor_tooltips(*MarkColoursLink);
		{
			auto linkPalette = MarkColoursLink->palette();
			linkPalette.setColor(QPalette::ButtonText, readable_link_colour(linkPalette));
			MarkColoursLink->setPalette(linkPalette);
		}
		connect(MarkColoursLink, &QToolButton::clicked, this, [this] {
			if (SettingsWindow == nullptr) {
				return;
			}
			SettingsWindow->present(settings_page::appearance);
			if (AppearanceSettings != nullptr && AppearanceSettings->colours() != nullptr) {
				auto* const colours = AppearanceSettings->colours();
				if (colours->filter_field() != nullptr) {
					colours->filter_field()->setText(QStringLiteral("label"));
				}
				colours->apply_filter(QStringLiteral("label"));
			}
		});
		auto* const chipsColumn = new QWidget{ BehaviourEditor };
		auto* const chipsLayout = new QVBoxLayout{ chipsColumn };
		chipsLayout->setContentsMargins(0, 0, 0, 0);
		chipsLayout->setSpacing(6);
		auto* const chipsLine = new QHBoxLayout{ };
		chipsLine->setSpacing(4);
		LabelColumnsEditor = new feedback_label_columns_editor{ chipsColumn };
		LabelColumnsEditor->set_dispatch([this](const core::feedback_label_columns&) {
			if (!ApplyingBehaviour) {
				dispatch_behaviour_from_controls();
			}
		});
		chipsLine->addWidget(LabelColumnsEditor, 1);
		auto* const chipsInfo = new info_glyph{ QStringLiteral("Ticked labels get their own count in the Overview's Comments column, in this order. The speech bubble counts the rest. Replies don't count."), chipsColumn };
		chipsInfo->setObjectName(QStringLiteral("labelColumnsInfo"));
		chipsLine->addWidget(chipsInfo, 0, Qt::AlignTop);
		chipsLayout->addLayout(chipsLine);
		LabelColumnsRecovery = new QLabel{ chipsColumn };
		LabelColumnsRecovery->setObjectName(QStringLiteral("labelColumnsRecovery"));
		LabelColumnsRecovery->setWordWrap(true);
		LabelColumnsRecovery->setMaximumWidth(420);
		LabelColumnsRecovery->hide();
		chipsLayout->addWidget(LabelColumnsRecovery);
		// [[BPR-1566]]. The owner, 2026-10-16: "add a small thing to the
		// settings page [...] that links to [Conventional Comments] for
		// people who don't know where those come from or how to use them."
		// One quiet sentence under the heading and the same link the label
		// menu ends with. It names the page; the application opens it. Not a
		// preference: the section's Reset leaves it alone, and it is hidden
		// until the window is bound to the application.
		ConventionalCommentsRow     = new QWidget{ BehaviourEditor };
		auto* const conventionalRow = new QVBoxLayout{ ConventionalCommentsRow };
		conventionalRow->setContentsMargins(0, 0, 0, 2);
		conventionalRow->setSpacing(2);
		auto* const conventionalLine = new QHBoxLayout{ };
		conventionalLine->setSpacing(2);
		ConventionalCommentsCaption = new QLabel{ QStringLiteral("These labels follow a widely used format for review comments."), ConventionalCommentsRow };
		ConventionalCommentsCaption->setObjectName(QStringLiteral("conventionalCommentsCaption"));
		ConventionalCommentsCaption->setForegroundRole(QPalette::PlaceholderText);
		ConventionalCommentsCaption->setTextFormat(Qt::PlainText);
		conventionalLine->addWidget(ConventionalCommentsCaption);
		ConventionalCommentsLink = new QToolButton{ ConventionalCommentsRow };
		ConventionalCommentsLink->setObjectName(QStringLiteral("conventionalCommentsLink"));
		ConventionalCommentsLink->setText(QStringLiteral("About Conventional Comments"));
		ConventionalCommentsLink->setAutoRaise(true);
		ConventionalCommentsLink->setCursor(Qt::PointingHandCursor);
		ConventionalCommentsLink->setFocusPolicy(Qt::StrongFocus);
		ConventionalCommentsLink->setToolTip(QStringLiteral("Where the labels come from and how to use them.\nOpens conventionalcomments.org in your browser."));
		ConventionalCommentsLink->setAccessibleName(QStringLiteral("About Conventional Comments"));
		ConventionalCommentsLink->setAccessibleDescription(QStringLiteral("Where the comment labels come from and how to use them. Opens conventionalcomments.org in your browser."));
		anchor_tooltips(*ConventionalCommentsLink);
		{
			auto linkPalette = ConventionalCommentsLink->palette();
			linkPalette.setColor(QPalette::ButtonText, readable_link_colour(linkPalette));
			ConventionalCommentsLink->setPalette(linkPalette);
		}
		connect(ConventionalCommentsLink, &QToolButton::clicked, this, [this] {
			open_reference_page(core::reference_page::conventional_comments, true);
		});
		conventionalLine->addWidget(ConventionalCommentsLink);
		conventionalLine->addStretch(1);
		conventionalRow->addLayout(conventionalLine);
		ConventionalCommentsStatus = new QLabel{ ConventionalCommentsRow };
		ConventionalCommentsStatus->setObjectName(QStringLiteral("conventionalCommentsStatus"));
		ConventionalCommentsStatus->setTextFormat(Qt::PlainText);
		ConventionalCommentsStatus->setWordWrap(true);
		ConventionalCommentsStatus->hide();
		conventionalRow->addWidget(ConventionalCommentsStatus);
		ConventionalCommentsRow->setVisible(static_cast<bool>(ReferenceDispatch));
		auto* const commentLabelsReset = addGroup(
			commentsColumn,
			QStringLiteral("Labels"),
			QStringLiteral("resetCommentLabels"),
			{
				row{ .Index = 0U, .Label = { }, .Name = QStringLiteral("conventionalComments"), .Options = { }, .Control = ConventionalCommentsRow, .Wide = true },
				row{ .Index    = 6U,
					 .Label    = QStringLiteral("Label marks"),
					 .Name     = QStringLiteral("feedbackMarks"),
					 .Options  = { { QStringLiteral("Icons"), QStringLiteral("feedbackMarksIcons") },
								   { QStringLiteral("Letters"), QStringLiteral("feedbackMarksLetters") } },
					 .Trailing = FeedbackMarksPreview },
				row{ .Index           = 7U,
					 .Label           = QStringLiteral("Overview counts"),
					 .Name            = QStringLiteral("overviewCounts"),
					 .Options         = { { QStringLiteral("Open"), QStringLiteral("overviewCountsOpen") },
										  { QStringLiteral("All"), QStringLiteral("overviewCountsAll") } },
					 .Trailing        = countsInfo,
					 .TrailingSpacing = 4 },
				row{ .Index = 0U, .Label = QStringLiteral("Mark colours"), .Name = QStringLiteral("markColours"), .Options = { }, .Control = MarkColoursLink },
				row{ .Index = 0U, .Label = QStringLiteral("Overview chips"), .Name = QStringLiteral("labelColumns"), .Options = { }, .Control = chipsColumn, .Top = true },
			},
			[](application::review_behaviour_settings& aNext) {
				const auto defaults  = application::review_behaviour_settings{ };
				aNext.FeedbackMarks  = defaults.FeedbackMarks;
				aNext.OverviewCounts = defaults.OverviewCounts;
				aNext.LabelColumns   = defaults.LabelColumns;
			});
		commentLabelsReset->setToolTip(QStringLiteral("Reset comment labels"));
		commentLabelsReset->setAccessibleName(QStringLiteral("Reset comment labels"));
		commentLabelsReset->setAccessibleDescription(QStringLiteral("Returns label marks, Overview counts and Overview chips to their defaults; nothing else changes"));
		auto* const commentWorkshopReset = addGroup(
			commentsColumn,
			QStringLiteral("Comment workshop"),
			QStringLiteral("resetCommentWorkshop"),
			{
				row{ .Index = 0U, .Label = QStringLiteral("Feedback guidance"), .Name = QStringLiteral("labelHelper"), .Options = { }, .Control = helperRow },
				row{ .Index = 0U, .Label = QStringLiteral("Guidance text"), .Name = QStringLiteral("workshopGuidance"), .Options = { }, .Control = WorkshopGuidanceLink },
			},
			[](application::review_behaviour_settings& aNext) {
				aNext.LabelHelper = application::review_behaviour_settings{ }.LabelHelper;
			});
		commentWorkshopReset->setToolTip(QStringLiteral("Reset comment workshop"));
		commentWorkshopReset->setAccessibleDescription(QStringLiteral("Turns the feedback guidance off, its default; nothing else changes"));
		addGroup(
			commentsColumn,
			QStringLiteral("Descriptions and comments"),
			QStringLiteral("resetDescriptionsAndComments"),
			{
				row{ 9U,
					 QStringLiteral("Images"),
					 QStringLiteral("markdownImages"),
					 { { QStringLiteral("Load"), QStringLiteral("markdownImagesLoad") },
					   { QStringLiteral("Don't load"), QStringLiteral("markdownImagesOff") } },
					 imagesInfo,
					 nullptr,
					 4 },
			},
			[](application::review_behaviour_settings& aNext) {
				aNext.MarkdownImages = application::review_behaviour_settings{ }.MarkdownImages;
			});
		aReviewColumn->addWidget(BehaviourEditor);
		aCommentsColumn->addWidget(CommentsEditor);
		apply_behaviour_controls();
		refresh_open_count_mark();
	}

	void workbench_window::set_label_tints(const feedback_label_tints& aTints)
	{
		LabelTints = aTints;
		apply_label_marks();
	}

	void workbench_window::set_attention_colour(const QColor& aColour)
	{
		AttentionColour = aColour;
		apply_label_marks();
	}

	auto workbench_window::label_marks() const -> feedback_mark_theme
	{
		auto theme  = LabelTints.has_value() ? feedback_mark_theme{ .Tints = *LabelTints } : default_feedback_mark_theme(palette());
		theme.Style = Behaviour.FeedbackMarks == application::review_feedback_marks::letters ? feedback_mark_style::letters : feedback_mark_style::icons;
		return theme;
	}

	void workbench_window::apply_label_marks()
	{
		const auto theme = label_marks();
		if (ReviewEditor != nullptr) {
			ReviewEditor->set_label_marks(theme);
		}
		if (ReviewSubmissionEditor != nullptr) {
			ReviewSubmissionEditor->set_label_marks(theme);
			if (AttentionColour.has_value()) {
				ReviewSubmissionEditor->set_attention_colour(*AttentionColour);
			}
		}
		if (ConversationPanel != nullptr) {
			ConversationPanel->set_label_marks(theme);
		}
		if (Finder != nullptr) {
			Finder->set_label_marks(theme);
		}
		if (FeedbackMarksPreview != nullptr) {
			FeedbackMarksPreview->set_theme(theme);
		}
		if (LabelColumnsEditor != nullptr) {
			LabelColumnsEditor->set_theme(theme);
		}
		// [[BPR-1089]]. The Overview's chips, and the round open mark beside
		// Settings' Open, in the theme's attention amber.
		auto states = default_feedback_state_colours(palette());
		if (AttentionColour.has_value()) {
			states.Open = *AttentionColour;
		}
		if (OverviewPage != nullptr) {
			OverviewPage->set_label_marks(theme);
			OverviewPage->set_attention_colour(states.Open);
		}
		refresh_open_count_mark();
	}

	void workbench_window::refresh_open_count_mark()
	{
		// [[BPR-1089]]. Settings' Open carries the round open state mark the
		// Overview draws beside its own Open, in the theme's attention amber.
		if (BehaviourOptions[7].empty()) {
			return;
		}
		auto states = default_feedback_state_colours(palette());
		if (AttentionColour.has_value()) {
			states.Open = *AttentionColour;
		}
		auto* const open = BehaviourOptions[7].front();
		open->setIcon(QIcon{ feedback_state_mark_pixmap(core::feedback_resolution::unresolved, states, devicePixelRatioF()) });
		open->setIconSize(QSize{ 16, 16 });
		open->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
	}

	void workbench_window::dispatch_line_details_toggle()
	{
		// The action's check already moved; the published behaviour decides,
		// so it goes back until the host answers.
		const auto current = AskedBehaviour.value_or(Behaviour);
		apply_line_details();
		if (!BehaviourDispatch) {
			return;
		}
		auto next        = current;
		next.LineDetails = current.LineDetails == application::review_line_details::shown
			? application::review_line_details::hidden
			: application::review_line_details::shown;
		AskedBehaviour   = next;
		BehaviourDispatch(next);
	}

	void workbench_window::apply_line_details()
	{
		const auto shown = Behaviour.LineDetails == application::review_line_details::shown;
		if (LineDetailsAction != nullptr) {
			const auto blocker = QSignalBlocker{ LineDetailsAction };
			LineDetailsAction->setChecked(shown);
		}
		if (ComparisonSurface != nullptr) {
			ComparisonSurface->set_line_details_visible(shown);
		}
	}

	void workbench_window::apply_reading_width()
	{
		// [[BPR-1584]]. Threads' rows and Discuss's notes stop at the reading
		// width, left-aligned; the code comparison is never touched.
		if (Finder != nullptr) {
			Finder->set_reading_width(Behaviour.ReadingWidth);
		}
		apply_modality_surface();
	}

	void workbench_window::apply_markdown_images()
	{
		const auto shown = Behaviour.MarkdownImages == application::review_markdown_images::load;
		if (OverviewPage != nullptr) {
			OverviewPage->body_view()->set_images_shown(shown);
		}
		if (ConversationPanel != nullptr) {
			ConversationPanel->set_images_shown(shown);
		}
	}

	void workbench_window::apply_behaviour_controls()
	{
		if (BehaviourEditor == nullptr) {
			return;
		}
		ApplyingBehaviour = true;
		const auto check  = [this](const std::size_t aRow, const std::size_t aChoice) {
			auto& buttons = BehaviourOptions[aRow];
			if (aChoice < buttons.size()) {
				buttons[aChoice]->setChecked(true);
			}
		};
		check(0U, static_cast<std::size_t>(Behaviour.OpenModality));
		check(2U, static_cast<std::size_t>(Behaviour.MarkRead));
		check(3U, static_cast<std::size_t>(Behaviour.ReturnComparison));
		check(4U, static_cast<std::size_t>(Behaviour.CommentsFilter));
		check(5U, static_cast<std::size_t>(Behaviour.DrawerControls));
		check(6U, static_cast<std::size_t>(Behaviour.FeedbackMarks));
		check(7U, static_cast<std::size_t>(Behaviour.OverviewCounts));
		check(8U, static_cast<std::size_t>(Behaviour.LineDetails));
		check(9U, static_cast<std::size_t>(Behaviour.MarkdownImages));
		check(10U, static_cast<std::size_t>(Behaviour.ReadingWidth));
		if (CenterRibbonWidth != nullptr) {
			CenterRibbonWidth->setValue(static_cast<int>(Behaviour.CenterRibbonWidth));
		}
		if (LabelHelperSwitch != nullptr) {
			LabelHelperSwitch->setChecked(Behaviour.LabelHelper == application::review_label_helper::on);
		}
		// [[BPR-1568]]. The Review feedback hook's line says what this
		// window's published record says.
		if (WritingHooks != nullptr) {
			WritingHooks->present_label_helper(Behaviour.LabelHelper);
		}
		if (LabelColumnsEditor != nullptr) {
			LabelColumnsEditor->set_columns(Behaviour.LabelColumns);
		}
		if (LabelColumnsRecovery != nullptr) {
			const auto& recovery = Behaviour.LabelColumnsRecovery;
			LabelColumnsRecovery->setVisible(recovery.has_value());
			LabelColumnsRecovery->setText(recovery.has_value() ? feedback_label_columns_recovery_text(recovery->Unknown, recovery->Repeated) : QString{ });
		}
		ApplyingBehaviour = false;
	}

	auto workbench_window::behaviour_from_controls() const -> application::review_behaviour_settings
	{
		const auto chosen = [this](const std::size_t aRow) -> std::size_t {
			const auto& buttons = BehaviourOptions[aRow];
			for (std::size_t index = 0U; index < buttons.size(); ++index) {
				if (buttons[index]->isChecked()) {
					return index;
				}
			}
			return 0U;
		};
		return application::review_behaviour_settings{
			.OpenModality      = static_cast<application::review_open_modality>(chosen(0U)),
			.MarkRead          = static_cast<application::review_mark_read_behaviour>(chosen(2U)),
			.ReturnComparison  = static_cast<application::review_return_comparison>(chosen(3U)),
			.CommentsFilter    = static_cast<application::review_comments_filter>(chosen(4U)),
			.DrawerControls    = static_cast<application::review_drawer_controls>(chosen(5U)),
			.FeedbackMarks     = static_cast<application::review_feedback_marks>(chosen(6U)),
			.LabelHelper       = LabelHelperSwitch != nullptr && LabelHelperSwitch->isChecked() ? application::review_label_helper::on : application::review_label_helper::off,
			.OverviewCounts    = static_cast<application::review_overview_counts>(chosen(7U)),
			.LabelColumns      = LabelColumnsEditor != nullptr ? LabelColumnsEditor->columns() : Behaviour.LabelColumns,
			.LineDetails       = BehaviourOptions[8].empty() ? Behaviour.LineDetails : static_cast<application::review_line_details>(chosen(8U)),
			.CenterRibbonWidth = CenterRibbonWidth != nullptr ? static_cast<std::uint32_t>(CenterRibbonWidth->value()) : Behaviour.CenterRibbonWidth,
			.MarkdownImages    = BehaviourOptions[9].empty() ? Behaviour.MarkdownImages : static_cast<application::review_markdown_images>(chosen(9U)),
			.ReadingWidth      = BehaviourOptions[10].empty() ? Behaviour.ReadingWidth : static_cast<core::reading_width>(chosen(10U)),
		};
	}

	void workbench_window::dispatch_behaviour_from_controls()
	{
		if (!BehaviourDispatch) {
			return;
		}
		const auto next = behaviour_from_controls();
		if (next != AskedBehaviour.value_or(Behaviour)) {
			AskedBehaviour = next;
			BehaviourDispatch(next);
		}
	}

	auto workbench_window::opening_modality() const -> review_modality
	{
		switch (Behaviour.OpenModality) {
		case application::review_open_modality::review:
			return review_modality::review;
		case application::review_open_modality::last_used:
			return LastUsedModality.value_or(review_modality::overview);
		case application::review_open_modality::overview:
			break;
		}
		return review_modality::overview;
	}

	void workbench_window::apply_first_comments_filter()
	{
		if (ConversationPanel == nullptr || FirstFilterApplied || !BehaviourPublished) {
			return;
		}
		FirstFilterApplied = true;
		ConversationPanel->apply_first_filter(Behaviour.CommentsFilter);
	}

	// The named comparison is dispatched through the control's own action,
	// which is what carries the catalog revision the core expects; until the
	// history offers it the preference waits, and a later epoch publish asks
	// again.
	void workbench_window::honour_return_comparison()
	{
		if (!ReturnComparisonPending || EpochSelectors == nullptr) {
			return;
		}
		auto* const action = Behaviour.ReturnComparison
				== application::review_return_comparison::since_last_review
			? EpochSelectors->last_reviewed_action()
			: Behaviour.ReturnComparison == application::review_return_comparison::since_last_upload
			? EpochSelectors->previous_upload_action()
			: nullptr;
		if (action == nullptr) {
			ReturnComparisonPending = false;
			return;
		}
		if (!action->isEnabled()) {
			return;
		}
		ReturnComparisonPending = false;
		ReturnComparisonAsked   = true;
		if (!action->isChecked()) {
			action->trigger();
		}
	}

	// "Fully seen" is the shared scrollbar at its end (or nothing to scroll)
	// with a comparison shown; the file's own mark-read command is dispatched
	// once per file and material, only while the ledger offers it.
	void workbench_window::consider_mark_read_when_fully_seen()
	{
		if (Behaviour.MarkRead != application::review_mark_read_behaviour::when_fully_seen
			|| (TourEditor != nullptr && TourEditor->isVisible())
			|| !ReceiptDispatch || !Receipt.has_value() || ComparisonSurface == nullptr
			|| ComparisonSurface->current_result() == nullptr) {
			return;
		}
		const auto* const scrollbar = ComparisonSurface->shared_scrollbar();
		if (scrollbar == nullptr || scrollbar->value() < scrollbar->maximum()) {
			return;
		}
		for (const auto& action : Receipt->Actions) {
			if (action.Command.Kind != core::file_receipt_command_kind::mark_read || !action.Enabled) {
				continue;
			}
			if (MarkedReadWhenSeen == std::optional{ action.Command }) {
				return;
			}
			MarkedReadWhenSeen = action.Command;
			ReceiptDispatch(action.Command);
			return;
		}
	}

	auto workbench_window::file_for_path(const QString& aPath) const
		-> std::optional<core::workbench_file_id>
	{
		const auto* const model = Drawers[0].Panel != nullptr ? Drawers[0].Panel->contents() : nullptr;
		if (model == nullptr) {
			return std::nullopt;
		}
		for (int row = 0; row < model->rowCount(); ++row) {
			const auto index = model->index(row, 0);
			if (model->data(index, Qt::DisplayRole).toString() == aPath) {
				return model->file_at(index);
			}
		}
		return std::nullopt;
	}

	void workbench_window::update_home_landing()
	{
		if (HomeLanding == nullptr || SessionPanel == nullptr) {
			return;
		}
		// Home hosts the empty state. Which way in it
		// offers follows the session: an account first, then a pull request.
		const auto status   = SessionPanel->status();
		const auto signedIn = session_signed_in();
		// [[BPR-0732]]. A public-preview session is signed in for the
		// window's chrome but has no account: no queue, no picker, and the
		// same card and bar as before the session, since the bar is what
		// opened it.
		const auto account   = signedIn && !PublicPreviewSignedOut;
		const auto comparing = ComparisonSurface != nullptr
			&& ComparisonSurface->current_result() != nullptr;
		const auto loading = status == core::github_status::loading_pull_request;
		HomeLanding->set_review_identity(IdentityText);
		HomeLanding->set_review_position(
			Cursor && Cursor->Ordinal > 0U
				? QStringLiteral("file %1 of %2").arg(Cursor->Ordinal).arg(Cursor->ActionableCount)
				: QString{ });
		HomeLanding->set_state(
			status == core::github_status::establishing_session ? landing_state::connecting
				: (HasGithubPullRequest || HasCachedOverview)   ? landing_state::back_to_review
				: account                                       ? landing_state::choose_pull_request
																: landing_state::sign_in);
		place_home_landing();
		if (HomeOpenAction != nullptr && HomeOpenBar != nullptr) {
			HomeOpenAction->setEnabled(status != core::github_status::establishing_session && !HomeOpenBar->text().trimmed().isEmpty());
		}
		// [[BPR-0775]]. Signed out, the bar reads a public pull request without
		// an account and says so; with an account it names any pull request.
		if (HomeOpenBar != nullptr) {
			HomeOpenBar->setPlaceholderText(
				account ? QStringLiteral("Paste a link, or write owner/name#123")
						: QStringLiteral("Paste a public pull request link to read it without an account"));
		}
		// The queue is a signed-in surface (RFC-BPR-0026 writes it for a
		// viewer); without an account there is nothing it could list.
		// [[BPR-0776]]. Recently viewed stays, for a signed-out window's public
		// reading, as [[BPR-0733]] and [[BPR-0775]] keep it.
		// The decision applies once the queue's surface exists; a window whose
		// session never spoke after it was built shows its queue, as it did.
		if (QueueSurface != nullptr) {
			HomeAccount = account;
			update_home_surface();
		}
		// Host-projected commands also keep standalone workbench compositions
		// visible before a GitHub snapshot or comparison has been presented.
		auto active = false;
		for (std::size_t index = 0U; index < core::workbench_command_count; ++index) {
			const auto projection = command(static_cast<core::workbench_command_id>(index));
			active                = active || (projection && projection->Enabled);
		}
		// Drawer commands remain enabled without accepted review content. An
		// initial read failure or cancellation belongs on Home, where the Open
		// bar and diagnostic are available; a retained review stays in place.
		const auto readUnavailable = status == core::github_status::pull_request_failed
			|| status == core::github_status::pull_request_cancelled;
		// [[BPR-1200]]. A first read stays on Home, said under the bar, until
		// something shows that this connection can read it: GitHub has answered
		// for both the repository and the pull request, the read has shown its
		// overview, or the review or a cached overview is here. A read the
		// connection is refused therefore never passes through an empty
		// Overview on its way back; a long read still shows its progress on
		// Overview from its first answers ([[BPR-0795]]).
		const auto answered    = ReadProgress.has_value() && ReadProgress->PagesReceived >= minimum_answered_pages;
		const auto unconfirmed = loading && !LoadingOverviewShown && !answered;
		const auto nothing     = !HasGithubPullRequest && !HasCachedOverview && !comparing
			&& (!loading || unconfirmed) && (!active || readUnavailable || unconfirmed);
		const auto home = (nothing && !LocalCycleShown) || HomeRequested || SetupDestinationNeedsHome;
		// [[BPR-1157]]. A local cycle takes the review surface's place, and the
		// title row keeps only what Home keeps plus the cycle's name.
		const auto cycle = !home && LocalCycleShown && LocalCyclePage != nullptr;
		if (HomeAction != nullptr && HomeAction->isChecked() != home) {
			HomeAction->setChecked(home);
		}
		auto leavingHome = false;
		auto arrivedHome = false;
		if (CentralStack != nullptr) {
			const auto arriving = home && CentralStack->currentWidget() != HomePage;
			arrivedHome         = arriving;
			leavingHome         = !home && CentralStack->currentWidget() == HomePage;
			if (arriving) {
				// [[BPR-0775]]. What had focus in the review has it again when
				// the review returns: Home covering it moves nothing there.
				// The window's own focus child, which it keeps whether or not it
				// is the active window.
				auto* const focused = focusWidget();
				FocusBeforeHome     = focused != nullptr && isAncestorOf(focused) && !HomePage->isAncestorOf(focused)
					? focused
					: nullptr;
			}
			CentralStack->setCurrentWidget(home ? HomePage : cycle ? static_cast<QWidget*>(LocalCyclePage)
																   : ReviewSurface);
			// [[BPR-0732]], [[BPR-0775]]. The bar has focus when Home appears,
			// signed in or out. Only on arrival, so a projection cannot steal
			// focus from wherever the reviewer has moved it since.
			if ((arriving || !HomeFocused) && home && HomeOpenBar != nullptr && isVisible()) {
				HomeOpenBar->setFocus(Qt::OtherFocusReason);
				HomeFocused = true;
			}
		}
		update_dif_presentation();
		// On Home the strip keeps only what Home can use: the account and
		// settings at its right, the inset and the spacers that place them.
		// Drawers, navigation, the modalities, review details and refresh
		// are a pull request's, and return with one.
		const auto stripHome = home || cycle;
		if (PrimaryToolbar != nullptr && (StripHome != stripHome || stripHome)) {
			StripHome = stripHome;
			for (auto* const action : PrimaryToolbar->actions()) {
				const auto name = action->objectName();
				const auto kept = action == AccountAction || action == SettingsAction
					|| action == HomeAction
					|| name == QLatin1String{ "workbenchStripInsetAction" }
					|| name == QLatin1String{ "workbenchNavigationSpacerAction" }
					// [[BPR-0653]]. On Windows the strip is the title row on
					// Home too: its menu control and caption buttons stay.
					|| name == QLatin1String{ "workbenchStripMenuAction" }
					|| name == QLatin1String{ "workbenchCaptionButtonsAction" }
					// With a review open behind Home, the modalities are the
					// way back to it.
					|| (HasGithubPullRequest && !cycle
						&& name == QLatin1String{ "workbenchModalityAction" })
					|| (cycle && action == IdentityAction);
				action->setVisible(!stripHome || kept);
			}
			// [[BPR-0783]], [[BPR-1580]]. View is Home's own, and
			// only while an account can fill the list: absent from the review
			// strip, and absent rather than disabled while signed out, which is
			// how RFC-BPR-0028's rule still holds there. [[BPR-0990]]: on Home
			// the refresh glyph standing with them is the profile's discovery;
			// the pull request's refresh is the review strip's.
			if (HomeViewItem != nullptr) {
				HomeViewItem->setVisible(home && HomeAccount);
			}
			if (GithubRefreshAction != nullptr) {
				GithubRefreshAction->setVisible(!stripHome);
			}
			// [[BPR-1575]]. The freshness follows the surface shown.
			present_refresh();
			if (QueueRefreshAction != nullptr) {
				QueueRefreshAction->setVisible(home && HomeAccount);
			}
			apply_refresh_key(home);
			apply_strip_width();
			// [[BPR-1295]]. Home has no review, so Hide Drawers is disabled.
			apply_drawers_toggle();
			// The toolbar shows and hides a widget action's widget on its
			// next layout; take it now so what the strip shows is settled
			// when this returns.
			if (auto* const layout = PrimaryToolbar->layout(); layout != nullptr) {
				layout->invalidate();
				layout->activate();
			}
		}
		// [[BPR-0775]]. Focus goes back once the strip is settled, so a strip
		// control that had it before Home is shown again to take it back.
		if (leavingHome) {
			if (FocusBeforeHome != nullptr && FocusBeforeHome->isVisible()) {
				FocusBeforeHome->setFocus(Qt::OtherFocusReason);
			}
			FocusBeforeHome = nullptr;
		}
		// [[BPR-0671]], [[BPR-0775]]. The pull request stack's empty page is not
		// Home's. It stays parented in the hidden pull request column, so what
		// writes to it keeps its shape. A first load in progress and a load
		// refusal are said under the bar, and a session's problem on its host's
		// card.
		if (StatusLabel != nullptr) {
			// update_status_text calls here, so the line is recomputed rather
			// than refreshed through it.
			if (const auto line = status_line(); StatusLabel->text() != line) {
				StatusLabel->setText(line);
			}
		}
		update_discovery_controls();
		// [[BPR-0990]]. Arriving at Home asks discovery to catch up if its list
		// is older than the freshness window; the host decides whether it is.
		if (arrivedHome && HomeAccount && QueueDiscoveryDispatch) {
			QueueDiscoveryDispatch(application::queue_discovery_intent::activate);
		}
		update_open_on_github_action();
	}

	auto workbench_window::showing_ai_tools() const -> bool
	{
		return AiToolsSelected && !showing_home()
			&& DrawerStates.Conversations == core::workbench_drawer_state::expanded
			&& Drawers[1].Panel != nullptr && Drawers[1].Panel->isVisible()
			&& Drawers[1].Panel->width() >= 240;
	}

	auto workbench_window::showing_dif_workshop() const -> bool
	{
		// Reuse the editor's exact draft/proposal and conflict gate rather than
		// retaining a second workshop identity just for decoration.
		return !showing_home() && CompositionPage == ComposeInlinePage
			&& ReviewEditor != nullptr && ReviewEditor->isVisible()
			&& ReviewEditor->accept_revision_action()->isEnabled();
	}

	void workbench_window::update_dif_presentation()
	{
		if (WorkshopHeader != nullptr) {
			WorkshopHeader->setVisible(!showing_home() && CompositionPage == ComposeInlinePage
									   && ReviewEditor->shown_proposal().has_value());
		}
		if (HomeLanding == nullptr || !DifResolutionReady) {
			return;
		}
		const auto shown = DifResolution.Preferences.Visibility == core::dif_visibility::shown;
		const auto home  = showing_home();
		// [[BPR-1209]]. Home and the exact offered workshop are prime spots;
		// every other surface uses the 28-pixel status corner.
		const auto card         = home && HomeLanding->state() == landing_state::sign_in;
		const auto cardSlot     = card && HomeLanding->illustration_space_available();
		const auto workshopSlot = showing_dif_workshop();
		const auto cornerSlot   = !card && !workshopSlot;
		const auto cornerShown  = cornerSlot && statusBar()->currentMessage().isEmpty();
		if (Dif == nullptr && shown && isVisible() && (cardSlot || cornerSlot || workshopSlot) && DifCatalog) {
			if (auto cache = DifCatalog->acquire()) {
				auto* const parent = cardSlot ? static_cast<QWidget*>(HomeLanding)
					: cornerSlot              ? static_cast<QWidget*>(this)
											  : DifWorkshopSlot;
				Dif                = new dif_widget{ std::move(cache), parent };
				Dif->setObjectName(QStringLiteral("workbenchDif"));
				Dif->setAttribute(Qt::WA_TransparentForMouseEvents, true);
			}
		}
		HomeLanding->set_illustration_enabled(shown && card);
		DifWorkshopSlot->setVisible(shown && workshopSlot && Dif != nullptr);
		DifCornerSlot->setVisible(shown && cornerShown && Dif != nullptr);
		DifCues->set_visible(
			shown && (workshopSlot || cornerShown) && DifResolution.EffectiveMotion == core::motion_mode::full
			&& isActiveWindow() && QApplication::activeModalWidget() == nullptr);
		if (Dif == nullptr) {
			return;
		}
		if (card) {
			if (HomeLanding->illustration() != Dif) {
				HomeLanding->set_illustration(Dif);
			}
		} else if (cornerSlot) {
			if (HomeLanding->illustration() == Dif) {
				static_cast<void>(HomeLanding->take_illustration());
			}
			if (Dif->parentWidget() != this) {
				Dif->setParent(this);
			}
			Dif->setFixedSize(dif_corner_size, dif_corner_size);
			place_dif_corner();
		} else if (workshopSlot) {
			if (HomeLanding->illustration() == Dif) {
				static_cast<void>(HomeLanding->take_illustration());
			}
			if (Dif->parentWidget() != DifWorkshopSlot) {
				Dif->setParent(DifWorkshopSlot);
				DifWorkshopSlot->layout()->addWidget(Dif);
				DifWorkshopSlot->layout()->setAlignment(Dif, Qt::AlignCenter);
			}
			Dif->setFixedSize(dif_workshop_size, dif_workshop_size);
			Dif->show();
		} else {
			Dif->hide();
		}
		Dif->set_eligibility({
			.Shown      = shown && (cardSlot || cornerShown || workshopSlot),
			.FullMotion = DifResolution.EffectiveMotion == core::motion_mode::full,
			.Active     = isActiveWindow(),
			.Modal      = QApplication::activeModalWidget() != nullptr,
			.Suspended  = false,
		});
		Dif->set_home(home);
	}

	auto workbench_window::home_action() const noexcept -> QAction*
	{
		return HomeAction;
	}

	void workbench_window::choose_home(const bool aHome)
	{
		if (ReviewNavigationDispatch) {
			ReviewNavigationDispatch(std::nullopt);
		}
		// Asked for, Home shows over an open review until a modality or a
		// pull request brings the review back; with nothing open Home is
		// simply where the window is, and cannot be left.
		HomeRequested = aHome;
		update_home_landing();
	}

	auto workbench_window::home_page() const noexcept -> QWidget*
	{
		return HomePage;
	}

	auto workbench_window::open_project_action() const noexcept -> QAction*
	{
		return OpenProjectAction;
	}

	void workbench_window::adopt_local_cycle(review_cycle_view* const aView)
	{
		if (LocalCyclePage != nullptr || aView == nullptr || CentralStack == nullptr) {
			return;
		}
		LocalCyclePage = aView;
		CentralStack->addWidget(static_cast<QWidget*>(aView));
	}

	auto workbench_window::local_cycle() const noexcept -> review_cycle_view*
	{
		return LocalCyclePage;
	}

	void workbench_window::show_local_cycle(const QString& aTitle)
	{
		if (ReviewNavigationDispatch) {
			ReviewNavigationDispatch(std::nullopt);
		}
		LocalCycleTitle = aTitle;
		LocalCycleShown = true;
		HomeRequested   = false;
		update_home_landing();
		update_strip_identity();
		apply_strip_width();
	}

	void workbench_window::retitle_local_cycle(const QString& aTitle)
	{
		if (!LocalCycleShown || LocalCycleTitle == aTitle) {
			return;
		}
		LocalCycleTitle = aTitle;
		update_strip_identity();
		apply_strip_width();
	}

	void workbench_window::leave_local_cycle()
	{
		if (!LocalCycleShown) {
			return;
		}
		LocalCycleShown = false;
		LocalCycleTitle.clear();
		update_home_landing();
		update_strip_identity();
		apply_strip_width();
	}

	auto workbench_window::showing_local_cycle() const noexcept -> bool
	{
		return LocalCycleShown && CentralStack != nullptr && LocalCyclePage != nullptr && CentralStack->currentWidget() == static_cast<QWidget*>(LocalCyclePage);
	}

	void workbench_window::present_local_cycle_recents(const std::vector<application::review_cycle_recent>& aRecents)
	{
		CycleRecents = aRecents;
		if (HomeCyclesList == nullptr) {
			return;
		}
		HomeCyclesList->clear();
		for (const auto& recent : CycleRecents) {
			const auto  title    = QString::fromUtf8(recent.Cycle.Title.data(), static_cast<qsizetype>(recent.Cycle.Title.size()));
			const auto  location = QString::fromUtf8(recent.Location.data(), static_cast<qsizetype>(recent.Location.size()));
			auto* const item     = new QListWidgetItem{ location.isEmpty() ? title : QStringLiteral("%1  —  %2").arg(title, location), HomeCyclesList };
			item->setToolTip(location);
		}
		HomeCyclesList->setVisible(!CycleRecents.empty());
	}

	void workbench_window::set_local_cycle_entry_dispatch(std::function<void()> aOpenProject, std::function<void(const core::local_cycle_subject&)> aResume)
	{
		OpenProjectDispatch = std::move(aOpenProject);
		ResumeCycleDispatch = std::move(aResume);
		if (OpenProjectAction != nullptr) {
			OpenProjectAction->setEnabled(static_cast<bool>(OpenProjectDispatch));
		}
	}

	auto workbench_window::home_cycles_list() const noexcept -> QListWidget*
	{
		return HomeCyclesList;
	}

	void workbench_window::apply_refresh_key(const bool aHome)
	{
		// [[BPR-1600]]. One window shows Home or a pull request, and the Refresh
		// key does the refresh that is showing. The key is on one action at a
		// time, taken off before it is put on, so no press is ever ambiguous.
		if (GithubRefreshAction == nullptr || QueueRefreshAction == nullptr) {
			return;
		}
		auto* const from = aHome ? GithubRefreshAction : QueueRefreshAction;
		auto* const to   = aHome ? QueueRefreshAction : GithubRefreshAction;
		from->setShortcuts({ });
		to->setShortcuts(shortcut_keys(aHome ? shortcut_command::home_refresh : shortcut_command::refresh));
	}

	auto workbench_window::showing_home() const -> bool
	{
		return CentralStack != nullptr && CentralStack->currentWidget() == HomePage;
	}

	void workbench_window::place_home_landing()
	{
		if (HomeLanding == nullptr || HomeColumn == nullptr || HomeSetupRow == nullptr) {
			return;
		}
		const auto state = HomeLanding->state();
		HomeLanding->setVisible(state != landing_state::choose_pull_request);
		// [[BPR-1381]]. Over a review the way back stands at the bar's right,
		// in the bar's own row; signed out the card follows the bar and what
		// is said under it.
		if (state == landing_state::back_to_review && HomeOpenRow != nullptr) {
			if (HomeOpenRow->indexOf(HomeLanding) >= 0) {
				return;
			}
			HomeColumn->removeWidget(HomeLanding);
			HomeOpenRow->addWidget(HomeLanding, 0, Qt::AlignVCenter);
			return;
		}
		const auto wanted = HomeColumn->indexOf(HomeSetupRow) + 1;
		if (HomeColumn->indexOf(HomeLanding) == wanted) {
			return;
		}
		if (HomeOpenRow != nullptr) {
			HomeOpenRow->removeWidget(HomeLanding);
		}
		HomeColumn->removeWidget(HomeLanding);
		HomeColumn->insertWidget(HomeColumn->indexOf(HomeSetupRow) + 1, HomeLanding);
	}

	void workbench_window::focus_home_bar()
	{
		if (HomeOpenBar == nullptr) {
			return;
		}
		if (!showing_home()) {
			choose_home(true);
		}
		HomeOpenBar->setFocus(Qt::ShortcutFocusReason);
		HomeOpenBar->selectAll();
	}

	void workbench_window::place_dif_corner()
	{
		if (Dif == nullptr || DifCornerSlot == nullptr || Dif->parentWidget() != this) {
			return;
		}
		// The slot is hidden while a prime spot holds Dif, and while the
		// status bar shows a transient message over its ordinary widgets; the
		// sprite follows it rather than covering that message.
		if (!DifCornerSlot->isVisible()) {
			Dif->hide();
			return;
		}
		const auto bottomLeft = DifCornerSlot->mapTo(this, QPoint{ 0, DifCornerSlot->height() });
		Dif->move(bottomLeft.x(), bottomLeft.y() - dif_corner_size);
		// Home's rows end above the part of the sprite that rises over the
		// status bar, whatever height the platform gives that bar (review of
		// BPR-0775, finding 6).
		if (HomeColumn != nullptr) {
			const auto inset = std::max(0, dif_corner_size - statusBar()->height()) + 4;
			if (HomeColumn->contentsMargins().bottom() != inset) {
				HomeColumn->setContentsMargins(0, 0, 0, inset);
			}
		}
		Dif->raise();
		Dif->show();
	}

	void workbench_window::begin_enterprise_server_from_home()
	{
		open_sign_in();
		// The same draft the page's own button opens, under the same readiness.
		open_enterprise_server_draft();
	}

	void workbench_window::begin_github_cli_from_home()
	{
		if (SessionPanel != nullptr && !SessionPanel->connect_enabled()) {
			return;
		}
		open_sign_in({ }, true);
		if (GithubCliDiscoveryDispatch && GithubCliDiscoveryState != github_cli_discovery_state::discovering) {
			GithubCliDiscoveryDispatch();
		}
	}

	auto workbench_window::session_signed_in() const -> bool
	{
		if (SessionPanel == nullptr) {
			return false;
		}
		const auto status = SessionPanel->status();
		return status == core::github_status::session_ready
			|| status == core::github_status::loading_pull_request
			|| status == core::github_status::pull_request_ready
			|| status == core::github_status::pull_request_failed
			|| status == core::github_status::pull_request_cancelled;
	}

	auto workbench_window::home_session_line() const -> QString
	{
		// [[BPR-0775]]. RFC-BPR-0032's session line. The session projection
		// carries one account on one host and no refresh time, so the line says
		// that much and no more.
		const auto host = SessionPanel != nullptr
			? QString::fromStdString(SessionPanel->active_host_text())
			: QString{ };
		if (session_signed_in() && !PublicPreviewSignedOut) {
			// The accepted session's login. The strip's account text is a label
			// ("Not authenticated" until the account row learns its login), not
			// the login itself.
			const auto& session = SessionPanel->session();
			const auto  login   = session.has_value() ? QString::fromStdString(session->Login) : QString{ };
			if (!login.isEmpty()) {
				return host.isEmpty() ? QStringLiteral("Signed in as %1.").arg(login)
									  : QStringLiteral("Signed in as %1 on %2.").arg(login, host);
			}
			return host.isEmpty() ? QStringLiteral("Signed in.")
								  : QStringLiteral("Signed in on %1.").arg(host);
		}
		if (SessionPanel != nullptr && SessionPanel->status() == core::github_status::establishing_session) {
			return host.isEmpty() ? QStringLiteral("Connecting.")
								  : QStringLiteral("Connecting to %1.").arg(host);
		}
		return QStringLiteral("Not signed in. Public pull requests open read-only.");
	}

	auto workbench_window::dif_corner_slot() const noexcept -> QWidget*
	{
		return DifCornerSlot;
	}

	auto workbench_window::home_open_label() const noexcept -> QLabel*
	{
		return HomeOpenLabel;
	}

	auto workbench_window::home_open_frame() const noexcept -> QWidget*
	{
		return HomeOpenFrame;
	}

	auto workbench_window::home_focus_bar_action() const noexcept -> QAction*
	{
		return HomeFocusBarAction;
	}

	auto workbench_window::status_label() const noexcept -> QLabel*
	{
		return StatusLabel;
	}

	auto workbench_window::account_action() const noexcept -> QAction*
	{
		return AccountAction;
	}

	auto workbench_window::identity_label() const noexcept -> QLabel*
	{
		return IdentityLabel;
	}

	auto workbench_window::change_position_label() const noexcept -> QLabel*
	{
		return ChangePositionLabel;
	}

	auto workbench_window::file_position_label() const noexcept -> QLabel*
	{
		return FilePositionLabel;
	}

	void workbench_window::update_strip_positions()
	{
		if (FilePositionLabel == nullptr || ChangePositionLabel == nullptr) {
			return;
		}
		// [[BPR-1385]]. Native glyph widths and digit boundaries must not move
		// the arrow hit targets. Both positions reserve four digits per number
		// (three in compact mode), even without a selection. Larger positions elide with
		// their complete words retained in the tooltip and accessible description.
		const auto present = [this](QLabel& aLabel, const QString& aName, const QString& aText, const QString& aWords) {
			const auto metrics   = aLabel.fontMetrics();
			const auto textWidth = review_position_text_width(metrics, aName, ReviewBarPositionsShown, ReviewBarCompact);
			const auto margins   = aLabel.contentsMargins();
			aLabel.setFixedWidth(textWidth + margins.left() + margins.right());
			aLabel.setText(metrics.elidedText(ReviewBarPositionsShown ? aText : aName, Qt::ElideRight, textWidth));
			aLabel.setToolTip(aWords);
			aLabel.setAccessibleDescription(aWords);
		};
		// [[BPR-1202]]. The group names remain visible even with no selection.
		// Narrow bars shorten the position, keeping its label before the arrows.
		auto fileText  = QStringLiteral("File");
		auto fileWords = QStringLiteral("No actionable file selected");
		if (Cursor && Cursor->ActionableCount > 0U) {
			fileWords = Cursor->Ordinal > 0U
				? QStringLiteral("File %1 of %2")
					  .arg(Cursor->Ordinal)
					  .arg(Cursor->ActionableCount)
				: QStringLiteral("Files %1").arg(Cursor->ActionableCount);
			fileText  = ReviewBarCompact && Cursor->Ordinal > 0U
				? QStringLiteral("File %1/%2").arg(Cursor->Ordinal).arg(Cursor->ActionableCount)
				: fileWords;
		}
		present(*FilePositionLabel, QStringLiteral("File"), fileText, fileWords);

		// Derived from the comparison the surface holds, exactly as the host
		// derives the change cursor it observes for the core: the group the
		// selection names, counted among the alignment's groups. No second
		// authority, and nothing when the selection is for another comparison.
		auto changeText  = QStringLiteral("Changes");
		auto changeWords = QStringLiteral("No comparison selected");
		if (ComparisonSurface != nullptr) {
			const auto& result    = ComparisonSurface->current_result();
			const auto& selection = ComparisonSurface->selection();
			if (result != nullptr) {
				const auto& groups  = result->Alignment.Groups;
				auto        ordinal = std::size_t{ 0U };
				if (selection.has_value() && selection->Group.has_value()
					&& selection->Comparison == result->Alignment.Digest) {
					const auto found = std::ranges::find(
						groups, *selection->Group, &core::change_group::Id);
					if (found != groups.end()) {
						ordinal = static_cast<std::size_t>(
							std::ranges::distance(groups.begin(), found) + 1);
					}
				}
				changeWords = ordinal > 0U
					? QStringLiteral("Changes %1 of %2").arg(ordinal).arg(groups.size())
					: QStringLiteral("Changes %1").arg(groups.size());
				changeText  = ReviewBarCompact && ordinal > 0U
					? QStringLiteral("Changes %1/%2").arg(ordinal).arg(groups.size())
					: changeWords;
			}
		}
		present(*ChangePositionLabel, QStringLiteral("Changes"), changeText, changeWords);
		update_idle_direction();
	}

	// [[BPR-1287]]. The owner, 2026-10-06: "this is what happens when you go
	// to Read without a file open. We need to not show a blank diff here and
	// instead have some clear direction to open a file." The comparison area
	// says so and offers the window's own gestures: Next file, which from no
	// file opens the first, and the Files drawer.
	void workbench_window::update_idle_direction()
	{
		if (ComparisonSurface == nullptr || Navigation[1].Action == nullptr) {
			return;
		}
		const auto actionable = Cursor.has_value() ? Cursor->ActionableCount : std::size_t{ 0U };
		const auto next       = Navigation[1].Action->shortcut().toString(QKeySequence::NativeText);
		auto       direction  = diff_workbench::idle_direction{ };
		if (Cursor.has_value() && actionable == 0U) {
			direction.Title  = QStringLiteral("No files to read");
			direction.Detail = QStringLiteral("This pull request has no file this comparison can show.");
		} else {
			direction.Title     = QStringLiteral("No file open");
			direction.Detail    = actionable > 0U
				? QStringLiteral("Choose one of the %1 files in Files, or open the first one. %2 opens the next file.").arg(actionable).arg(next)
				: QStringLiteral("Choose a file in Files, or open the first one. %1 opens the next file.").arg(next);
			direction.OpenFirst = [this] {
				if (Navigation[1].Action->isEnabled()) {
					Navigation[1].Action->trigger();
				}
			};
		}
		direction.ShowFiles = [this] {
			if (Drawers[0].Panel != nullptr && Drawers[0].Panel->state() != core::workbench_drawer_state::expanded && Drawers[0].Toggle != nullptr) {
				Drawers[0].Toggle->trigger();
			}
			if (FilesTree != nullptr) {
				FilesTree->setFocus(Qt::OtherFocusReason);
			}
		};
		ComparisonSurface->set_idle_direction(std::move(direction));
		ComparisonSurface->idle_open_first_button()->setEnabled(Navigation[1].Action->isEnabled());
	}

	void workbench_window::update_strip_identity()
	{
		if (IdentityLabel == nullptr) {
			return;
		}
		update_open_on_github_action();
		// [[BPR-0722]]. The identity is the only in-window statement of which
		// pull request is open (the macOS title bar is hidden), so it yields
		// last. By width it sheds the title, then the owner; the number stays
		// as far down as it has room (apply_strip_width decides that). Nothing
		// here folds into an overflow menu.
		const auto& metrics = IdentityLabel->fontMetrics();
		// What the other title row items leave: the toolbar folds anything
		// past its width into the extension button, so the name never asks
		// for more than that remainder.
		const auto margins = IdentityLabel->contentsMargins();
		const auto room    = std::max(strip_identity_budget() - margins.left() - margins.right(), 0);
		auto       text    = QString{ };
		if (LocalCycleShown && !showing_home()) {
			// [[BPR-1157]]. A local cycle is named by its title and badged as
			// local: no repository, number or account.
			const auto badge = QStringLiteral("  ·  Local cycle");
			text             = metrics.elidedText(LocalCycleTitle, Qt::ElideRight, std::max(room - metrics.horizontalAdvance(badge), identity_fragment_width)) + badge;
			IdentityLabel->setText(text);
			IdentityLabel->setToolTip(LocalCycleTitle + badge);
			return;
		}
		if (!IdentityText.isEmpty()) {
			// [[BPR-1129]]. The name has the room the navigation took, so the
			// forms follow the room rather than fixed widths: the whole name
			// with its title elided at the right, then owner/repository with
			// the number, then the repository, then a fragment of it; the
			// number stays in every form.
			const auto number = QStringLiteral("  #%1").arg(IdentityNumber);
			const auto owned  = QStringLiteral("%1/%2").arg(IdentityOwner, IdentityRepository) + number;
			const auto titled = metrics.horizontalAdvance(owned) + identity_title_minimum;
			const auto bare   = IdentityRepository + number;
			// [[BPR-1251]]. The title also needs the room it would have beside
			// the whole modality (apply_strip_width), or it could yield and
			// then return when the shortcuts go. [[BPR-1324]]. So does the
			// owner: with four modalities it yielded on Windows above the width
			// where the shortcuts go, and came back below it.
			const auto wholeRoom  = IdentityTitleBudget - margins.left() - margins.right();
			const auto ownedWidth = metrics.horizontalAdvance(owned);
			if (room >= titled && wholeRoom >= titled) {
				text = metrics.elidedText(IdentityText, Qt::ElideRight, room);
			} else if (room >= ownedWidth && wholeRoom >= ownedWidth) {
				text = owned;
			} else if (room >= metrics.horizontalAdvance(bare)) {
				text = bare;
			} else {
				const auto budget = room - metrics.horizontalAdvance(number);
				text              = metrics.elidedText(IdentityRepository, Qt::ElideMiddle, std::max(budget, identity_fragment_width)) + number;
			}
		}
		IdentityLabel->setText(text);
		IdentityLabel->setToolTip(IdentityText);
	}

	auto workbench_window::strip_identity_budget() const -> int
	{
		if (PrimaryToolbar == nullptr || IdentityLabel == nullptr || IdentityAction == nullptr) {
			return 0;
		}
		// [[BPR-0722]]. The toolbar folds into its extension button exactly
		// when its size hint exceeds its width, so the identity's room is
		// the width less what every other visible item asks for: the hint
		// with the identity's own current hint taken back out. A small
		// slack covers the toolbar's rounding; the floor leaves no other.
		constexpr int slack    = 12;
		const auto    identity = IdentityAction->isVisible() ? IdentityLabel->sizeHint().width() : 0;
		return width() - (PrimaryToolbar->sizeHint().width() - identity) - slack;
	}

	auto workbench_window::strip_identity_minimum() const -> int
	{
		if (IdentityLabel == nullptr) {
			return 0;
		}
		// The number, a short repository fragment and the label's margins.
		const auto margins = IdentityLabel->contentsMargins();
		return IdentityLabel->fontMetrics().horizontalAdvance(QStringLiteral("  #%1").arg(IdentityNumber))
			+ identity_fragment_width + margins.left() + margins.right();
	}

	void workbench_window::update_account_action()
	{
		if (AccountAction == nullptr || SessionPanel == nullptr) {
			return;
		}
		const auto status   = SessionPanel->status();
		const auto signedIn = status == core::github_status::session_ready
			|| status == core::github_status::loading_pull_request
			|| status == core::github_status::pull_request_ready
			|| status == core::github_status::pull_request_failed
			|| status == core::github_status::pull_request_cancelled;
		const auto account     = QString::fromStdString(SessionPanel->active_account_text());
		const auto description = signedIn
			? QStringLiteral("Signed in as %1. Open the accounts and hosts settings.")
				  .arg(account)
			: QStringLiteral("Sign in with a device code.");
		AccountAction->setText(signedIn ? account : QStringLiteral("Sign in"));
		AccountAction->setToolTip(description);
		AccountAction->setStatusTip(description);
		if (auto* const button = qobject_cast<QToolButton*>(
				PrimaryToolbar->widgetForAction(AccountAction));
			button != nullptr) {
			button->setAccessibleName(QStringLiteral("Account"));
			button->setAccessibleDescription(description);
			button->setFocusPolicy(Qt::StrongFocus);
		}
	}

	auto workbench_window::previous_file_action() const noexcept -> QAction*
	{
		return Navigation[0].Action;
	}

	auto workbench_window::next_file_action() const noexcept -> QAction*
	{
		return Navigation[1].Action;
	}

	auto workbench_window::header_source_action() const noexcept -> QAction*
	{
		return HeaderSourceAction;
	}

	void workbench_window::set_header_source_dispatch(std::function<void()> aDispatch)
	{
		HeaderSourceDispatch = std::move(aDispatch);
		update_header_source_action();
		update_source_find_actions();
	}

	void workbench_window::present_header_source_result(const core::header_source_resolution aResult)
	{
		auto message = QString{ };
		switch (aResult.Status) {
		case core::header_source_status::matched:
			ComparisonSurface->focus_source();
			return;
		case core::header_source_status::no_match:
			message = aResult.SelectedFamily == core::header_source_family::source ? tr("No matching header in this review") : tr("No matching source file in this review");
			break;
		case core::header_source_status::ambiguous:
			message = tr("More than one equally close header or source file matches in this review");
			break;
		case core::header_source_status::unavailable_catalogue:
			message = reason_text(core::workbench_reason_code::header_source_unavailable_catalogue);
			break;
		case core::header_source_status::unsupported_file:
			message = reason_text(core::workbench_reason_code::header_source_unsupported_file);
			break;
		case core::header_source_status::no_selection:
			message = reason_text(core::workbench_reason_code::header_source_no_selection);
			break;
		}
		statusBar()->showMessage(message, bounded_navigation_message_ms);
		QAccessibleAnnouncementEvent announcement{ this, message };
		announcement.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
		QAccessible::updateAccessibility(&announcement);
	}

	void workbench_window::set_pr_search_dispatch(std::function<void(source_find_gesture)> aDispatch)
	{
		PrSearchDispatch = std::move(aDispatch);
		connect(FileSearchTabs, &QTabWidget::currentChanged, this, [this](int aIndex) {if(aIndex==1 && PrSearchDispatch){PrSearchDispatch(source_find_gesture::document_changed);} });
		SearchPanel->Dispatch = [this](auto aGesture) {if(PrSearchDispatch){PrSearchDispatch(aGesture);}if(aGesture==source_find_gesture::close){FileSearchTabs->setCurrentIndex(SearchPreviousTab);if(SearchWasCollapsed && Dispatch){Dispatch(core::set_workbench_drawer_intent{core::workbench_drawer::files,core::workbench_drawer_state::collapsed});}ComparisonSurface->focus_source();}update_source_find_actions(); };
		update_source_find_actions();
	}
	auto workbench_window::pr_search() const -> source_search_panel* { return SearchPanel; }
	void workbench_window::update_source_find_actions()
	{
		if (!SourceFindActions[0] || !ComparisonSurface) {
			return;
		}
		auto*      focused     = QApplication::focusWidget();
		const bool sourceFocus = focused == ComparisonSurface->left_pane() || focused == ComparisonSurface->right_pane()
			|| (focused && focused->objectName() == QStringLiteral("diffUnifiedSource"));
		const bool queryFocus = focused && ComparisonSurface->source_find()->isAncestorOf(focused);
		const bool textFocus  = qobject_cast<QLineEdit*>(focused) || qobject_cast<QPlainTextEdit*>(focused) || qobject_cast<QTextEdit*>(focused);

		const auto filesCommands = core::project_source_files_commands({ .PullRequest = HasGithubPullRequest && !showing_home() && !LocalCycleShown && static_cast<bool>(PrSearchDispatch), .Modal = QApplication::activeModalWidget() != nullptr, .Editor = textFocus && !sourceFocus && !queryFocus && !(focused && SearchPanel && SearchPanel->isAncestorOf(focused)), .Query = SearchPanel && !SearchPanel->projection().Query.empty(), .Hits = SearchPanel && SearchPanel->projection().CurrentMatch.has_value() });
		for (std::size_t index = 0; index < SourceFilesActions.size(); ++index) {
			if (SourceFilesActions[index]) {
				describe_action(SourceFilesActions[index], filesCommands[index], index == 0 ? tr("Search full changed files in the active comparison") : tr("Open the next exact search occurrence"), tr("Open a pull request and leave modal dialogs"));
			}
		}
		const auto commands = core::project_source_find_commands({ .Reader         = !showing_home() && !LocalCycleShown && !CompositionPage && current_modality() == review_modality::review,
																   .Material       = ComparisonSurface->source_find_available(),
																   .EditorOwnsFind = textFocus && !sourceFocus && !queryFocus,
																   .Modal          = QApplication::activeModalWidget() != nullptr,
																   .Query          = !ComparisonSurface->source_find()->query().empty(),
																   .Hits           = ComparisonSurface->source_find_navigable() });
		for (std::size_t index = 0; index < SourceFindActions.size(); ++index) {
			describe_action(SourceFindActions[index], commands[index], index == 0U ? tr("Find in the focused source pane") : tr("Traverse source matches"), tr("Focus an available source reader in Review"));
		}
	}

	void workbench_window::update_header_source_action()
	{
		if (HeaderSourceAction == nullptr) {
			return;
		}
		const auto  projection = command(core::workbench_command_id::go_to_header_source);
		const auto* focused    = QApplication::focusWidget();
		const auto* line       = qobject_cast<const QLineEdit*>(focused);
		const auto  editing    = focused != nullptr && isAncestorOf(focused) && (editable_multi_line(focused) || (line != nullptr && !line->isReadOnly()));
		const auto  inReview   = !showing_home() && !LocalCycleShown && CompositionPage == nullptr && current_modality() == review_modality::review;
		const auto  reason     = !inReview ? reason_text(core::workbench_reason_code::header_source_requires_review)
			: editing                      ? tr("Keep Option/Alt text input in the editor. Focus the source comparison to navigate.")
										   : reason_text(projection ? projection->DisabledReason : core::workbench_reason_code::header_source_no_selection);
		describe_action(HeaderSourceAction, inReview && !editing && projection && projection->Enabled && static_cast<bool>(HeaderSourceDispatch),
						tr("Go to the closest matching header or source file in this review"), reason);
	}

	auto workbench_window::previous_change_action() const noexcept -> QAction*
	{
		return Navigation[2].Action;
	}

	auto workbench_window::next_change_action() const noexcept -> QAction*
	{
		return Navigation[3].Action;
	}

	auto workbench_window::first_change_action() const noexcept -> QAction*
	{
		return ChangeEnds[0].Action;
	}

	auto workbench_window::last_change_action() const noexcept -> QAction*
	{
		return ChangeEnds[1].Action;
	}

	void workbench_window::update_change_end_keys()
	{
		const auto* const focused = QApplication::focusWidget();
		const auto        inside  = focused != nullptr && isAncestorOf(focused);
		const auto        keys    = change_end_keys();
		for (std::size_t index = 0U; index < ChangeEnds.size(); ++index) {
			if (ChangeEnds[index].Action == nullptr) {
				continue;
			}
			const auto yields = inside && (takes_typed_text(focused) || binds_key_nearer(focused, this, keys[index]));
			ChangeEnds[index].Action->setShortcut(yields ? QKeySequence{ } : keys[index]);
		}
		describe_change_ends();
	}

	void workbench_window::describe_change_ends()
	{
		for (const auto& [entry, id] : { std::pair{ &ChangeEnds[0], core::workbench_command_id::first_change }, std::pair{ &ChangeEnds[1], core::workbench_command_id::last_change } }) {
			if (entry->Action == nullptr) {
				continue;
			}
			const auto projection = command(id);
			const auto enabled    = projection && projection->Enabled;
			const auto shown      = !projection ? core::workbench_reason_code::no_active_workbench
				: enabled                       ? projection->Bound
												: projection->DisabledReason;
			describe_navigation_action(entry->Action, entry->Name, enabled, entry->Purpose, change_reason_text(shown));
		}
	}

	auto workbench_window::seen_action() const noexcept -> QAction*
	{
		return ReceiptActions[0];
	}

	auto workbench_window::read_action() const noexcept -> QAction*
	{
		return ReceiptActions[1];
	}

	auto workbench_window::accept_action() const noexcept -> QAction*
	{
		return ReceiptActions[2];
	}

	auto workbench_window::exclude_action() const noexcept -> QAction*
	{
		return ReceiptActions[3];
	}

	auto workbench_window::review_controls_button() const noexcept
		-> QToolButton*
	{
		return ReviewControlsButton;
	}

	auto workbench_window::review_controls_menu() const noexcept -> QMenu*
	{
		return ReviewControlsMenu;
	}

	auto workbench_window::menu_bar() const noexcept -> QMenuBar*
	{
		return PrimaryMenuBar;
	}

	auto workbench_window::toggle_status_drawer_action() const noexcept -> QAction*
	{
		return ToggleStatusDrawerAction;
	}

	auto workbench_window::view_menu() const noexcept -> QMenu*
	{
		return ViewMenu;
	}

	auto workbench_window::line_numbers_action() const noexcept -> QAction*
	{
		return LineNumbersAction;
	}

	auto workbench_window::line_details_action() const noexcept -> QAction*
	{
		return LineDetailsAction;
	}

	auto workbench_window::one_pane_action() const noexcept -> QAction*
	{
		return OnePaneAction;
	}

	auto workbench_window::zoom_in_action() const noexcept -> QAction*
	{
		return ZoomInAction;
	}

	auto workbench_window::zoom_out_action() const noexcept -> QAction*
	{
		return ZoomOutAction;
	}

	auto workbench_window::actual_size_action() const noexcept -> QAction*
	{
		return ActualSizeAction;
	}

	void workbench_window::set_code_zoom_dispatch(std::function<void(core::code_zoom_step)> aDispatch)
	{
		CodeZoomDispatch = std::move(aDispatch);
		apply_code_zoom();
	}

	void workbench_window::dispatch_code_zoom(const core::code_zoom_step aStep)
	{
		if (CodeZoomDispatch && CodeZoom && core::code_zoom_steps_from(*CodeZoom, aStep)) {
			CodeZoomDispatch(aStep);
		}
	}

	void workbench_window::adopt_code_zoom(const core::code_font_settings& aCode)
	{
		const auto moved = CodeZoom && CodeZoom->ZoomPercent != aCode.ZoomPercent;
		CodeZoom         = aCode;
		apply_code_zoom();
		if (moved && ComparisonSurface != nullptr) {
			ComparisonSurface->show_code_zoom(aCode.ZoomPercent, core::zoomed_point_size(aCode));
		}
	}

	/// \brief [[BPR-1336]]. Each zoom command's state and the reason for it.
	///
	/// \details The core says whether a step changes what is shown and what it
	/// leads to; the words here only report it. Before the profile's code
	/// record arrives there is nothing to step from.
	void workbench_window::apply_code_zoom()
	{
		if (ZoomInAction == nullptr) {
			return;
		}
		const auto size = [](const std::uint32_t aCentipoints) {
			return point_size_text(aCentipoints) + QStringLiteral(" pt");
		};
		if (!CodeZoom || !CodeZoomDispatch) {
			const auto reason = QStringLiteral("The code size is not loaded yet.");
			for (auto* const action : { ZoomInAction, ZoomOutAction, ActualSizeAction }) {
				describe_action(action, false, { }, reason);
			}
			return;
		}
		const auto& code    = *CodeZoom;
		const auto  shown   = size(core::zoomed_point_size(code));
		const auto  towards = [&](const core::code_zoom_step aStep) {
			auto next        = code;
			next.ZoomPercent = core::stepped_code_zoom(code, aStep);
			return QStringLiteral("%1% (%2)").arg(next.ZoomPercent).arg(size(core::zoomed_point_size(next)));
		};
		describe_action(ZoomInAction, core::code_zoom_steps_from(code, core::code_zoom_step::in),
						QStringLiteral("Show the code in the comparison larger: %1.").arg(towards(core::code_zoom_step::in)),
						QStringLiteral("The code in the comparison is as large as it goes: %1% (%2).").arg(code.ZoomPercent).arg(shown));
		describe_action(ZoomOutAction, core::code_zoom_steps_from(code, core::code_zoom_step::out),
						QStringLiteral("Show the code in the comparison smaller: %1.").arg(towards(core::code_zoom_step::out)),
						QStringLiteral("The code in the comparison is as small as it goes: %1% (%2).").arg(code.ZoomPercent).arg(shown));
		const auto own = size(code.Font.PointSizeCentipoints);
		describe_action(ActualSizeAction, core::code_zoom_steps_from(code, core::code_zoom_step::actual_size),
						QStringLiteral("Show the code in the comparison at your code size, %1.").arg(own),
						QStringLiteral("The code in the comparison is at your code size, %1.").arg(own));
	}

	auto workbench_window::files_drawer() const noexcept -> workbench_drawer_panel*
	{
		return Drawers[0].Panel;
	}

	auto workbench_window::files_tree() const noexcept -> files_tree_view*
	{
		return FilesTree;
	}

	auto workbench_window::files_order_selector() const noexcept -> QComboBox*
	{
		return FilesOrderSelector;
	}

	auto workbench_window::files_filter() const noexcept -> QLineEdit*
	{
		return FilesFilter;
	}

	auto workbench_window::files_filter_notice() const noexcept -> filter_empty_notice*
	{
		return FilesFilterNotice;
	}

	auto workbench_window::repositories_filter() const noexcept -> QLineEdit*
	{
		return RepositoriesFilter;
	}

	auto workbench_window::repositories_filter_notice() const noexcept -> filter_empty_notice*
	{
		return RepositoriesFilterNotice;
	}

	void workbench_window::refresh_filter_notices()
	{
		// An empty state is a filter that found nothing among rows that exist;
		// an empty drawer with no filter says nothing new.
		if (FilesFilterNotice != nullptr && FilesTree != nullptr && FilesFilter != nullptr) {
			const auto filter = FilesFilter->text().trimmed();
			FilesFilterNotice->set_filter(filter);
			FilesFilterNotice->setVisible(!filter.isEmpty() && FilesTree->tree_model()->file_count() == 0U);
		}
		if (RepositoriesFilterNotice != nullptr && RepositoriesTree != nullptr && RepositoriesFilter != nullptr) {
			const auto filter = RepositoriesFilter->text().trimmed();
			RepositoriesFilterNotice->set_filter(filter);
			RepositoriesFilterNotice->setVisible(!filter.isEmpty() && RepositoriesTree->tree_model()->rowCount() == 0);
		}
	}

	auto workbench_window::central_splitter() const noexcept -> workbench_central_splitter*
	{
		return CentralSplitter;
	}

	auto workbench_window::repositories_drawer() const noexcept -> workbench_drawer_panel*
	{
		return Drawers[2].Panel;
	}

	auto workbench_window::repositories_drawer_toggle_action() const noexcept -> QAction*
	{
		return Drawers[2].Toggle;
	}

	auto workbench_window::repositories_tree() const noexcept -> repositories_tree_view*
	{
		return RepositoriesTree;
	}

	auto workbench_window::rules_editor() const noexcept -> QWidget*
	{
		return RulesEditor;
	}

	auto workbench_window::presented_repositories_state() const
		-> core::workbench_drawer_state
	{
		// As the files drawer: shown while the core offers no drawer command,
		// so a reviewer with nothing open can still reach a pull request.
		const auto toggle = command(core::workbench_command_id::toggle_repositories_drawer);
		if (!toggle || !toggle->Enabled) {
			return core::workbench_drawer_state::expanded;
		}
		return DrawerStates.Repositories;
	}

	auto workbench_window::conversations_drawer() const noexcept
		-> workbench_drawer_panel*
	{
		return Drawers[1].Panel;
	}

	void workbench_window::publish(
		const application::github_presentation_update& aUpdate)
	{
		if (aUpdate.ReadProgress) {
			const auto wasAnswered = ReadProgress.has_value() && ReadProgress->PagesReceived >= minimum_answered_pages;
			ReadProgress           = aUpdate.ReadProgress;
			// [[BPR-1200]]. The first answers that show the connection can read
			// what was named decide the page again.
			if (!wasAnswered && ReadProgress->PagesReceived >= minimum_answered_pages) {
				update_home_landing();
			}
			// [[BPR-1357]]. The step a waiting opening names.
			if (HomeOpen.has_value() && HomeOpen->State.has_value() && HomeOpen->State->Phase == core::home_open_phase::waiting) {
				refresh_home_open();
			}
		}
		if (aUpdate.Transition.has_value()) {
			apply_github_transition(*aUpdate.Transition);
		}
		if (aUpdate.LoadingOverview && !HasGithubPullRequest
			&& SessionPanel->status() == core::github_status::loading_pull_request) {
			OverviewPage->present_observation(*aUpdate.LoadingOverview);
			// [[BPR-1200]]. The read has shown what it names, so the connection
			// can read it: Overview now has something to be about.
			LoadingOverviewShown = true;
			update_home_landing();
		}
		if (aUpdate.CachedOverview && !HasGithubPullRequest) {
			HasCachedOverview = true;
			HomeRequested     = false;
			OverviewPage->present_cached(*aUpdate.CachedOverview);
			update_status_text();
			enter_opening_modality(review_modality::overview);
			apply_modality_surface();
		}
		if (aUpdate.CachedSource && HasCachedOverview && !HasGithubPullRequest) {
			OverviewPage->present_cached_source(*aUpdate.CachedSource);
		}
		// RFC-BPR-0084. Each answer reaches the body it names; a reader ignores
		// one that is not for its current image.
		for (const auto& image : aUpdate.Images) {
			if (image.Body.empty()) {
				OverviewPage->body_view()->apply_image(image);
			} else {
				ConversationPanel->apply_markdown_image(image);
			}
		}
		if (aUpdate.Session.has_value()) {
			// [[BPR-0803]]. A device flow still asking for its code when some
			// other way in -- GitHub CLI above all -- has signed in is ended
			// here, through its own Cancel, so it never keeps running where the
			// reviewer cannot see it. The flow's own session arrives after the
			// flow has authorized, so it is never cancelled by this.
			if (aUpdate.Session->Account.value() != 0U && DevicePanel != nullptr && DevicePanel->signing_in()
				&& DevicePanel->cancel_button()->isEnabled()) {
				DevicePanel->cancel_button()->click();
			}
			SessionPanel->apply_session(*aUpdate.Session);
			update_server_cards();
			if (SignInDialog && SignInDialog->isVisible() && aUpdate.Session->Account.value() != 0U
				&& (SignInHostname.empty() || (server_card_for(aUpdate.Session->Profile) && server_card_for(aUpdate.Session->Profile)->hostname() == SignInHostname))) {
				SignInDialog->accept();
			}
			// [[BPR-0775]]. Home's line names the accepted session's login, which
			// arrives after the transition that already decided the line.
			if (StatusLabel != nullptr) {
				StatusLabel->setText(status_line());
			}
			// [[BPR-1017]]. So does the strip's account button. A window told the
			// session in one presentation has no later transition to correct it.
			update_account_action();
			if (ConversationPanel != nullptr) {
				ConversationPanel->cards()->set_viewer_login(
					QString::fromStdString(aUpdate.Session->Login));
				if (Finder != nullptr) {
					Finder->set_viewer_login(QString::fromStdString(aUpdate.Session->Login));
				}
			}
		}
		if (aUpdate.PullRequest.has_value()) {
			apply_github_pull_request(*aUpdate.PullRequest, aUpdate.PreserveWindowNavigation);
		}
		for (const auto& diagnostic : aUpdate.HostDiagnostics) {
			apply_github_host_diagnostic(diagnostic);
		}
		update_review_readiness();
		update_pending_panes();
		settle_pull_request_refresh(aUpdate);
		announce_github_presentation_update();
	}

	void workbench_window::announce_github_presentation_update()
	{
		// One polite announcement per material change, not one per projection
		// field. A screen reader that is told the same thing twice has been told
		// nothing the second time.
		if (SessionPanel == nullptr) {
			return;
		}
		// [[BPR-0771]]. Attributed to where the change is said: a pull
		// request's status to Home, whose bar carries its refusal, and a
		// session's to its host's card.
		QObject* source = current_server_card();
		switch (SessionPanel->status()) {
		case core::github_status::loading_pull_request:
		case core::github_status::pull_request_ready:
		case core::github_status::pull_request_failed:
		case core::github_status::pull_request_cancelled:
			source = HomePage;
			break;
		case core::github_status::no_profile:
		case core::github_status::establishing_session:
		case core::github_status::session_ready:
		case core::github_status::session_failed:
		case core::github_status::session_cancelled:
			break;
		}
		// [[BPR-1357]]. An opening the application follows is announced in the
		// opening's own words, once, by `refresh_home_open`.
		if (source == HomePage && HomeOpen.has_value()) {
			return;
		}
		// [[BPR-1575]]. A refresh is announced by its own lifecycle, which
		// knows who asked and what it brought; the status says nothing more.
		if (RefreshReadSeen) {
			RefreshReadSeen = false;
			return;
		}
		// [[BPR-1200]]. A refusal with its own card is announced in the card's
		// words, so a reader hears what a sighted reviewer reads.
		const auto wording      = source == HomePage ? home_refusal_wording() : std::nullopt;
		const auto announcement = wording.has_value()
			? QString::fromStdString(wording->Heading) + QLatin1Char{ ' ' } + QString::fromStdString(wording->Explanation)
			: SessionPanel->summary_text();
		if (source == nullptr || announcement.isEmpty() || announcement == LastGithubAnnouncement) {
			return;
		}
		LastGithubAnnouncement = announcement;
		QAccessibleAnnouncementEvent event{ source, announcement };
		event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
		QAccessible::updateAccessibility(&event);
	}

	void workbench_window::apply_github_transition(
		const application::github_transition_projection& aTransition)
	{
		for (const auto& command : aTransition.Commands) {
			if (!SessionPanel->apply_command(command)) {
				apply_github_content_command(command);
			}
		}
		// [[BPR-0771]]. A command can change without a status: a card must not
		// go on offering a Connect the core has just refused.
		if (!aTransition.Commands.empty()) {
			update_server_cards();
		}
		if (aTransition.Identity.has_value()) {
			SessionPanel->apply_identity(*aTransition.Identity);
			HasCachedOverview = false;
			PullRequestRefusal.reset();
			// An accepted identity moves the picker with its signals blocked,
			// so the rebind the picker's own change would have performed has to
			// be performed here as well. Both paths end at the same profile.
			rebind_device_authorization_profile();
			++Statistics.GithubIdentityUpdates;
		}
		if (aTransition.Capabilities.has_value()) {
			SessionPanel->apply_capabilities(
				aTransition.Capabilities->Capabilities);
		}
		if (aTransition.PullRequest.has_value()
			&& !aTransition.PullRequest->Summary.has_value()) {
			clear_github_pull_request();
		}
		// [[BPR-1200]]. Kept until the next read, which replaces or clears it.
		if (aTransition.Refusal.has_value()) {
			PullRequestRefusal = aTransition.Refusal;
		}
		// [[BPR-1575]]. A refresh's own lifecycle, as the core named its read.
		if (aTransition.Read.has_value()) {
			observe_refresh_read(*aTransition.Read);
		}
		if (aTransition.Status.has_value()) {
			ReadProgress.reset();
			LoadingOverviewShown = false;
			if (aTransition.Status->Status == core::github_status::loading_pull_request
				|| aTransition.Status->Status == core::github_status::pull_request_ready) {
				PullRequestRefusal.reset();
			}
			SessionPanel->apply_status(aTransition.Status->Status);
			if (aTransition.Status->Status == core::github_status::loading_pull_request && !HasGithubPullRequest) {
				HasCachedOverview = false;
				HomeRequested     = false;
				// [[BPR-1599]]. A load starts with no choice of its own.
				LoadingChoice.reset();
				OverviewPage->present_loading();
				enter_opening_modality(review_modality::overview);
			}
			if (aTransition.Status->Status != core::github_status::loading_pull_request && !HasGithubPullRequest) {
				OverviewPage->clear();
			}
			// [[BPR-1599]]. A load that ends without its pull request leaves
			// no choice for the next one.
			if (aTransition.Status->Status == core::github_status::pull_request_failed
				|| aTransition.Status->Status == core::github_status::pull_request_cancelled) {
				LoadingChoice.reset();
			}
			apply_modality_surface();
		}
		for (const auto& diagnostic : aTransition.Diagnostics) {
			SessionPanel->apply_diagnostic(diagnostic.Code);
			++Statistics.GithubDiagnosticUpdates;
		}
	}

	void workbench_window::apply_github_pull_request(
		const application::github_pull_request_projection& aPullRequest, const bool aPreserveWindowNavigation)
	{
		const auto authority = std::to_string(aPullRequest.Profile.value()) + ":" + std::to_string(aPullRequest.Host.value()) + ":"
			+ std::to_string(aPullRequest.Account.value()) + ":" + std::to_string(aPullRequest.Repository.value()) + ":" + std::to_string(aPullRequest.PullRequest.value());
		if (!MarkdownAuthority.empty() && MarkdownAuthority != authority) {
			clear_github_pull_request();
		}
		MarkdownAuthority   = authority;
		PageContext         = aPullRequest.MarkdownContext;
		const auto dispatch = [this, context = aPullRequest.MarkdownContext](const std::string& aBody, const std::string& aDestination) {
			const auto result = context && MarkdownDispatch ? MarkdownDispatch({ *context, aBody, aDestination }) : core::markdown_link_result::unavailable;
			QString    message;
			switch (result) {
			case core::markdown_link_result::opened:      message = QStringLiteral("Opened in your browser."); break;
			case core::markdown_link_result::stale:       message = QStringLiteral("This body is no longer current. Reopen the link from the current content."); break;
			case core::markdown_link_result::unavailable: message = QStringLiteral("Opening links is unavailable until the pull request is accepted."); break;
			case core::markdown_link_result::refused:     message = QStringLiteral("This link cannot be opened safely. Its destination remains available in the source."); break;
			case core::markdown_link_result::failed:      message = QStringLiteral("The browser could not open this link."); break;
			}
			statusBar()->showMessage(message, 8000);
		};
		OverviewPage->body_view()->set_link_dispatch([dispatch](const std::string& aDestination) { dispatch({ }, aDestination); });
		// RFC-BPR-0084. Images ask under this publication's context; a new
		// context asks again for whatever is still loading.
		const auto images = [this, context = aPullRequest.MarkdownContext](const std::string& aBody, std::vector<core::markdown_image_reference> aReferences) {
			if (context && MarkdownImageDispatch) {
				MarkdownImageDispatch({ .Context = *context, .Body = aBody, .References = std::move(aReferences) });
			}
		};
		OverviewPage->body_view()->set_image_dispatch([images](std::vector<core::markdown_image_reference> aReferences) { images({ }, std::move(aReferences)); });
		ConversationPanel->set_image_dispatch(images);
		OverviewPage->set_check_link_dispatch(dispatch);
		ConversationPanel->set_link_dispatch(dispatch);
		PullRequestPanel->adopt(aPullRequest);
		ReviewThreadTotal = aPullRequest.ReviewThreadCount;
		if (!aPreserveWindowNavigation) {
			HomeRequested = false;
		}
		if (ConversationPanel != nullptr && ConversationPanel->cards() != nullptr) {
			ConversationPanel->cards()->set_pull_request_author(
				QString::fromStdString(aPullRequest.AuthorLogin));
		}
		// [[BPR-1157]]. Another pull request arriving takes the review surface
		// back from a local cycle; the same one refreshed behind it does not.
		const auto arriving = IdentityOwner != QString::fromStdString(aPullRequest.Owner)
			|| IdentityRepository != QString::fromStdString(aPullRequest.RepositoryName)
			|| IdentityNumber != QString::number(aPullRequest.Number.value());
		if (arriving && LocalCycleShown) {
			LocalCycleShown = false;
			LocalCycleTitle.clear();
		}
		// [[BPR-1575]]. What a refresh compares. Another pull request starts
		// its own freshness from this read; the same one handed over by a
		// sibling window is a refresh made elsewhere.
		{
			const auto observation = refresh_observation_of(aPullRequest);
			if (arriving) {
				PullRefresh = core::refresh_state{ .UpdatedAt = refresh_clock() };
				RefreshEnded.reset();
			} else if (aPreserveWindowNavigation && PullRefresh.Phase != core::refresh_phase::refreshing && RefreshSnapshot != std::optional{ aPullRequest.Snapshot }) {
				ElsewhereChanges = core::compare_refresh(RefreshLatest, observation);
			}
			RefreshLatest   = observation;
			RefreshSnapshot = aPullRequest.Snapshot;
		}
		IdentityOwner      = QString::fromStdString(aPullRequest.Owner);
		IdentityRepository = QString::fromStdString(aPullRequest.RepositoryName);
		IdentityNumber     = QString::number(aPullRequest.Number.value());
		// [[BPR-0990]]. The open pull request as the queue names it.
		OpenQueueRepository    = aPullRequest.Owner + "/" + aPullRequest.RepositoryName;
		OpenQueueNumber        = static_cast<std::int64_t>(aPullRequest.Number.value());
		OpenQueueTitle         = aPullRequest.Title;
		OpenQueuePublicPreview = aPullRequest.PublicPreview;
		IdentityText           = QStringLiteral("%1/%2  #%3 %4")
									 .arg(IdentityOwner,
										  IdentityRepository,
										  IdentityNumber,
										  QString::fromStdString(aPullRequest.Title));
		// [[BPR-0981]]. The repositories tree keeps the open pull request's
		// row current, however it was opened, so a stale row cannot hold the
		// current index and swallow the gesture that would open it.
		if (RepositoriesTree != nullptr) {
			RepositoriesTree->follow_open_pull_request(
				aPullRequest.Owner + "/" + aPullRequest.RepositoryName,
				static_cast<std::int64_t>(aPullRequest.Number.value()));
		}
		// [[BPR-1418]]. Content changes invalidate the row even when both
		// widgets were hidden during loading. Measure from the new identity
		// and context rather than using the prior visibility as admission.
		apply_strip_width();
		// The gap BPR-0191 recorded closes here. In the two-window arrangement
		// the anchor could only follow a conversation projection, because nothing
		// observed a comparison context change. The shell composes both surfaces,
		// so it sets the anchor from the file the reviewer is actually reading.
		ConversationPanel->set_anchor_file(conversation_anchor_file());
		// [[BPR-1337]]. Discuss's counts say an ordinary thread count; a line
		// that says threads are unavailable, or public preview's, stays shown.
		ConversationPanel->set_availability_ordinary(!aPullRequest.PublicPreview && aPullRequest.ReviewThreadsAvailable);
		ApplyingConversationAnchor = true;
		ConversationPanel->adopt(
			aPullRequest.Conversations,
			aPullRequest.PublicPreview
				? (aPullRequest.ReviewCommentsAvailable
					   ? QStringLiteral("Review comments are read-only in public preview. Review threads and their resolution are unavailable.")
					   : QStringLiteral("Review comments were not observable."))
				: detail::review_thread_availability_text(
					  aPullRequest.ReviewThreadsAvailable,
					  aPullRequest.ReviewThreadCount,
					  aPullRequest.Capabilities.ReviewThreads));
		ApplyingConversationAnchor = false;
		if (Finder != nullptr) {
			Finder->adopt(aPullRequest.Conversations);
		}
		synchronize_selected_conversation(chosen_conversation());
		refresh_cards();
		update_defect_surface();
		HasGithubPullRequest = true;
		HasCachedOverview    = false;
		update_pull_request_queue_actions();
		// [[BPR-0651]]. A pull request chosen from the repositories tree
		// opens on its overview, as the design says it does.
		if (OpenOnOverview && !aPreserveWindowNavigation) {
			OpenOnOverview = false;
			// [[BPR-0661]]. The modality is the reviewer's preference: the
			// overview the design opens on, Review, or the one last used here.
			// [[BPR-1599]]: or the one chosen while it loaded.
			enter_opening_modality(opening_modality());
		}
		LoadingChoice.reset();
		// [[BPR-0661]]. The drawer's first filter and the comparison on return
		// are the reviewer's preferences too; the comparison waits until the
		// history offers it.
		if (!aPreserveWindowNavigation) {
			FirstFilterApplied    = false;
			ReturnComparisonAsked = false;
			apply_first_comments_filter();
			ReturnComparisonPending = Behaviour.ReturnComparison
				!= application::review_return_comparison::whole_pr;
			honour_return_comparison();
		}
		SessionPanel->apply_capabilities(aPullRequest.Capabilities);
		SessionPanel->apply_rate_limits(aPullRequest.RateLimits);
		// [[BPR-0657]]. Resolve is offered once the provider and the server
		// both resolve threads; a notice from an earlier gesture is stale.
		Capabilities = aPullRequest.Capabilities;
		if (ConversationPanel != nullptr && ConversationPanel->cards() != nullptr) {
			ConversationPanel->cards()->set_resolution_offered(
				ProviderResolvesThreads
				&& (Capabilities.ResolveThread == core::github_capability_state::supported
					|| Capabilities.UnresolveThread == core::github_capability_state::supported));
			ConversationPanel->present_notice(QString{ });
		}
		update_github_presentation_state();
		// RFC-BPR-0084. Unchanged bodies keep their images; ones still loading
		// ask again under the new context, which a cached image answers at once.
		OverviewPage->body_view()->request_images();
		ConversationPanel->request_images();
		// [[BPR-0668]]. The line states what is open, so it is written when
		// what is open changes, not only when a workbench projection lands.
		// Guarded: this runs while the window is still being assembled.
		if (StatusLabel != nullptr && SessionPanel != nullptr && EpochSelectors != nullptr) {
			update_status_text();
		}
	}

	void workbench_window::apply_github_content_command(
		const core::github_command_projection& aCommand)
	{
		// Conversation loading is automatic; no focus-only reveal command remains.
		static_cast<void>(aCommand);
	}

	void workbench_window::apply_github_host_diagnostic(
		const application::github_host_diagnostic_projection& aDiagnostic)
	{
		SessionPanel->apply_rate_limits(aDiagnostic.RateLimits);
		// [[BPR-0644]]. Before the diagnostic, whose presentation change is
		// what redraws the cards.
		if (aDiagnostic.GrantKept) {
			SessionPanel->apply_kept_grant(aDiagnostic.Reason, aDiagnostic.AccountMismatch);
		}
		SessionPanel->apply_diagnostic(
			aDiagnostic.Reason,
			aDiagnostic.RetryAfterSeconds,
			aDiagnostic.RateLimitCause,
			true,
			aDiagnostic.BootstrapStage, aDiagnostic.CapabilityDetail, aDiagnostic.AccountMismatch);
	}

	void workbench_window::set_markdown_link_dispatch(std::function<core::markdown_link_result(const core::markdown_link_intent&)> aDispatch)
	{
		MarkdownDispatch = std::move(aDispatch);
	}

	void workbench_window::set_markdown_image_dispatch(std::function<void(const core::markdown_image_intent&)> aDispatch)
	{
		MarkdownImageDispatch = std::move(aDispatch);
	}

	void workbench_window::set_markdown_code_dispatch(std::function<void(std::vector<application::markdown_code_syntax_request>)> aDispatch)
	{
		MarkdownCodeDispatch = std::move(aDispatch);
		const auto relay     = [this](std::vector<application::markdown_code_syntax_request> aRequests) {
			if (MarkdownCodeDispatch) {
				MarkdownCodeDispatch(std::move(aRequests));
			}
		};
		if (OverviewPage != nullptr) {
			OverviewPage->body_view()->set_code_syntax_dispatch(relay);
		}
		if (ConversationPanel != nullptr) {
			ConversationPanel->set_code_syntax_dispatch(relay);
		}
	}

	void workbench_window::apply_markdown_code_syntax(const application::markdown_code_syntax_publication& aSpans)
	{
		// A reader takes an answer only for code it shows.
		if (OverviewPage != nullptr) {
			OverviewPage->body_view()->apply_code_syntax(aSpans);
		}
		if (ConversationPanel != nullptr) {
			ConversationPanel->apply_markdown_code_syntax(aSpans);
		}
	}

	void workbench_window::set_pull_request_page_dispatch(std::function<core::markdown_link_result(const application::pull_request_page_intent&)> aDispatch)
	{
		PageDispatch = std::move(aDispatch);
		update_open_on_github_action();
	}

	void workbench_window::set_reference_page_dispatch(std::function<core::markdown_link_result(core::reference_page)> aDispatch)
	{
		ReferenceDispatch = std::move(aDispatch);
		const auto bound  = static_cast<bool>(ReferenceDispatch);
		// The composer's menu and every reply's ask the window, which asks
		// the application; neither holds the dispatch itself.
		auto fromMenu = std::function<void(core::reference_page)>{ };
		if (bound) {
			fromMenu = [this](const core::reference_page aPage) { open_reference_page(aPage, false); };
		}
		if (ReviewEditor != nullptr && ReviewEditor->label_controls() != nullptr) {
			ReviewEditor->label_controls()->set_reference_dispatch(fromMenu);
		}
		if (ConversationPanel != nullptr) {
			ConversationPanel->set_reference_dispatch(fromMenu);
		}
		if (ConventionalCommentsRow != nullptr) {
			ConventionalCommentsRow->setVisible(bound);
		}
	}

	void workbench_window::open_reference_page(const core::reference_page aPage, const bool aFromSettings)
	{
		const auto result = ReferenceDispatch ? ReferenceDispatch(aPage) : core::markdown_link_result::unavailable;
		// One page today. Its name is the site's, which is what the browser
		// will show; the address itself never reaches this window.
		auto page = QString{ };
		switch (aPage) {
		case core::reference_page::conventional_comments: page = QStringLiteral("conventionalcomments.org"); break;
		}
		QString message;
		switch (result) {
		case core::markdown_link_result::opened:      message = QStringLiteral("Opened %1 in your browser.").arg(page); break;
		case core::markdown_link_result::stale:
		case core::markdown_link_result::unavailable: message = QStringLiteral("This window has no browser to open %1 with.").arg(page); break;
		case core::markdown_link_result::refused:     message = QStringLiteral("%1 cannot be opened safely from here.").arg(page); break;
		case core::markdown_link_result::failed:      message = QStringLiteral("The browser could not open %1.").arg(page); break;
		}
		statusBar()->showMessage(message, 8000);
		// Settings is a window of its own, where this window's status bar is
		// out of sight: a page that did not open says so under its link.
		if (aFromSettings && ConventionalCommentsStatus != nullptr) {
			const auto shown = result != core::markdown_link_result::opened;
			ConventionalCommentsStatus->setText(shown ? message : QString{ });
			ConventionalCommentsStatus->setAccessibleName(shown ? message : QString{ });
			ConventionalCommentsStatus->setVisible(shown);
		}
	}

	auto workbench_window::open_on_github_action() const noexcept -> QAction*
	{
		return OpenOnGithubAction;
	}

	auto workbench_window::open_on_github_button() const noexcept -> QToolButton*
	{
		return OpenOnGithubButton;
	}

	auto workbench_window::home_row_menu() const noexcept -> QMenu*
	{
		return HomeRowMenu;
	}

	auto workbench_window::pull_request_page_target() const -> std::optional<application::pull_request_page_intent>
	{
		// [[BPR-1338]]. On Home the row the reviewer last made current, in
		// whichever list; over a review the open pull request, by the
		// publication on screen.
		if (showing_home()) {
			// [[BPR-1365]]. A review cycle has no page; with its row selected
			// there is no pull request on Home to name.
			if (HomeSelection == home_selection_list::cycles) {
				return std::nullopt;
			}
			if (HomeSelection == home_selection_list::recent && HomeRecentList != nullptr && HomeRecentList->currentRow() >= 0
				&& static_cast<std::size_t>(HomeRecentList->currentRow()) < RecentReviews.Entries.size()) {
				return application::recent_review_row{ static_cast<std::size_t>(HomeRecentList->currentRow()) };
			}
			if (const auto item = HomeList != nullptr ? HomeList->current_queue_item() : std::nullopt; item.has_value()) {
				return item->Ref;
			}
			return std::nullopt;
		}
		if (PageContext.has_value() && HasGithubPullRequest && !LocalCycleShown) {
			return *PageContext;
		}
		return std::nullopt;
	}

	void workbench_window::update_open_on_github_action()
	{
		if (OpenOnGithubAction != nullptr) {
			OpenOnGithubAction->setEnabled(PageDispatch && pull_request_page_target().has_value());
		}
	}

	auto workbench_window::home_open_review_action() const noexcept -> QAction*
	{
		return HomeOpenReviewAction;
	}

	auto workbench_window::home_open_review_window_action() const noexcept -> QAction*
	{
		return HomeOpenReviewWindowAction;
	}

	void workbench_window::set_review_window_dispatch(std::function<bool(const application::review_window_intent&)> aDispatch)
	{
		ReviewWindowDispatch = std::move(aDispatch);
		update_home_review_actions();
	}

	auto workbench_window::home_review_target() const -> std::optional<std::variant<core::queue_item, application::recent_review_row>>
	{
		if (!showing_home() || HomeSelection == home_selection_list::cycles) {
			return std::nullopt;
		}
		if (HomeSelection == home_selection_list::recent && HomeRecentList != nullptr && HomeRecentList->currentRow() >= 0
			&& static_cast<std::size_t>(HomeRecentList->currentRow()) < RecentReviews.Entries.size()) {
			return application::recent_review_row{ static_cast<std::size_t>(HomeRecentList->currentRow()) };
		}
		if (auto item = HomeList != nullptr ? HomeList->current_queue_item() : std::nullopt; item.has_value()) {
			return std::move(*item);
		}
		return std::nullopt;
	}

	void workbench_window::update_home_review_actions()
	{
		if (HomeOpenReviewAction == nullptr || HomeOpenReviewWindowAction == nullptr) {
			return;
		}
		const auto target = home_review_target();
		const auto recent = target.has_value() && std::holds_alternative<application::recent_review_row>(*target);
		HomeOpenReviewAction->setEnabled(target.has_value() && (recent ? static_cast<bool>(RecentReviewDispatch) : static_cast<bool>(GithubDispatch)));
		// A new window is the profile's New Window, so it is offered exactly
		// when that is.
		HomeOpenReviewWindowAction->setEnabled(target.has_value() && ReviewWindowDispatch && NewBprWindowAction != nullptr
											   && NewBprWindowAction->isEnabled());
		if (HomeIgnoreAction != nullptr) {
			// [[BPR-1683]]. The action names what it will do to this row.
			const auto* const item = target.has_value() ? std::get_if<core::queue_item>(&*target) : nullptr;
			HomeIgnoreAction->setText(item != nullptr && item->Ignored ? home_unignore_text() : home_ignore_text());
			HomeIgnoreAction->setStatusTip(item != nullptr && item->Ignored
											   ? QStringLiteral("Puts this pull request back in its section; nothing changes on GitHub")
											   : QStringLiteral("Moves this pull request to Ignored at the bottom of Home; nothing changes on GitHub"));
			HomeIgnoreAction->setEnabled(item != nullptr && static_cast<bool>(HomeIgnoreDispatch));
		}
	}

	void workbench_window::dispatch_home_ignore(const core::queue_item& aItem)
	{
		if (!HomeIgnoreDispatch) {
			return;
		}
		HomeIgnoreDispatch(core::home_ignore_intent{
			.Ref    = aItem.Ref,
			.Action = aItem.Ignored ? core::home_ignore_action::unignore : core::home_ignore_action::ignore,
		});
	}

	void workbench_window::set_home_ignore_dispatch(workbench_home_ignore_dispatch aDispatch)
	{
		HomeIgnoreDispatch = std::move(aDispatch);
		update_home_review_actions();
	}

	auto workbench_window::home_ignore_action() const noexcept -> QAction*
	{
		return HomeIgnoreAction;
	}

	void workbench_window::popup_home_row_menu(const QPoint& aGlobal)
	{
		update_open_on_github_action();
		update_home_review_actions();
		// Resume is a Recently viewed row's only. It is hidden just while the
		// menu is open on a queue row, because a hidden action is a disabled
		// one, and the row's hover button follows this action.
		const auto target = home_review_target();
		const auto recent = target.has_value() && std::holds_alternative<application::recent_review_row>(*target);
		for (auto* const action : { HomeRecentResumeAction, HomeRecentRemoveAction }) {
			if (action != nullptr) {
				action->setVisible(recent);
			}
		}
		// [[BPR-1683]]. Ignore is a queue row's; a Recently viewed row is
		// not ignored from there.
		HomeIgnoreAction->setVisible(!recent);
		HomeRowMenu->popup(aGlobal);
	}

	void workbench_window::open_home_review(const bool aNewWindow)
	{
		const auto target = home_review_target();
		if (!target.has_value()) {
			return;
		}
		if (const auto* const recent = std::get_if<application::recent_review_row>(&*target)) {
			if (aNewWindow) {
				request_review_window(*recent);
			} else {
				dispatch_recent_review(static_cast<int>(recent->Index), false);
			}
			return;
		}
		const auto& item = std::get<core::queue_item>(*target);
		if (aNewWindow) {
			request_review_window(item.Ref);
		} else {
			open_pull_request_item(item);
		}
	}

	void workbench_window::request_review_window(const application::review_window_intent& aIntent)
	{
		if (!ReviewWindowDispatch || NewBprWindowAction == nullptr || !NewBprWindowAction->isEnabled()) {
			statusBar()->showMessage(QStringLiteral("A new window is unavailable until this profile is open."), 8000);
			return;
		}
		if (!ReviewWindowDispatch(aIntent)) {
			statusBar()->showMessage(QStringLiteral("This pull request cannot open in a new window from here. Choose it again."), 8000);
		}
	}

	void workbench_window::open_pull_request_page(const application::pull_request_page_intent& aIntent)
	{
		const auto result = PageDispatch ? PageDispatch(aIntent) : core::markdown_link_result::unavailable;
		QString    message;
		switch (result) {
		case core::markdown_link_result::opened:      message = QStringLiteral("Opened the pull request on GitHub in your browser."); break;
		case core::markdown_link_result::stale:       message = QStringLiteral("This pull request is no longer the one shown. Choose it again to open it on GitHub."); break;
		case core::markdown_link_result::unavailable: message = QStringLiteral("Opening on GitHub is unavailable until this window has a session for the pull request's server."); break;
		case core::markdown_link_result::refused:     message = QStringLiteral("This pull request's page cannot be opened safely from here."); break;
		case core::markdown_link_result::failed:      message = QStringLiteral("The browser could not open the pull request's page."); break;
		}
		statusBar()->showMessage(message, 8000);
	}

	void workbench_window::clear_github_pull_request()
	{
		MarkdownAuthority.clear();
		PageContext.reset();
		OverviewPage->body_view()->set_link_dispatch({ });
		ConversationPanel->set_link_dispatch({ });
		RefreshConversationAnchor.reset();
		RefreshConversationContext.reset();
		LastWorkbenchContext.reset();
		HasGithubPullRequest = false;
		HasCachedOverview    = false;
		LoadingOverviewShown = false;
		ReviewThreadTotal    = 0U;
		IdentityText.clear();
		IdentityOwner.clear();
		IdentityRepository.clear();
		IdentityNumber.clear();
		OpenQueueRepository.clear();
		OpenQueueNumber = 0;
		OpenQueueTitle.clear();
		OpenQueuePublicPreview = false;
		update_pull_request_queue_actions();
		if (RepositoriesTree != nullptr) {
			RepositoriesTree->forget_open_pull_request();
		}
		apply_strip_width();
		update_rail_summaries();
		PullRequestPanel->clear();
		clear_overview();
		ConversationPanel->clear(QStringLiteral(
			"Review threads are unavailable until a pull request is open."));
		if (Finder != nullptr) {
			Finder->clear();
		}
		PendingPick.reset();
		refresh_cards();
		update_github_presentation_state();
		if (StatusLabel != nullptr && SessionPanel != nullptr && EpochSelectors != nullptr) {
			update_status_text();
		}
	}

	void workbench_window::publish(
		const application::public_preview_presentation& aPresentation)
	{
		PublicPreview          = aPresentation;
		PublicPreviewSignedOut = aPresentation.Mode
			== application::public_preview_mode::signed_out;
		if (SessionPanel != nullptr) {
			SessionPanel->apply_public_preview(aPresentation, PublicPreviewNow);
		}
		update_public_preview_title();
		apply_public_preview_conversation();
		// Re-decides the commands from their projections first, then lets the
		// mode have the last word over them.
		update_action_state();
		apply_focus_order();
		update_github_presentation_state();
	}

	void workbench_window::set_public_preview_now(
		const std::uint64_t aNowUnixSeconds)
	{
		PublicPreviewNow = aNowUnixSeconds;
		if (SessionPanel != nullptr) {
			SessionPanel->apply_public_preview(PublicPreview, PublicPreviewNow);
			update_server_cards();
			update_home_open_notice();
		}
	}

	void workbench_window::set_profile_label(const QString& aLabel)
	{
		ProfileLabel = aLabel;
		if (ProfileReset) {
			ProfileReset->set_profile_label(aLabel);
		}
		SettingsWindow->set_profile_label(aLabel);
		if (MountedSettings) {
			MountedSettings->set_profile_label(aLabel);
		}
		update_public_preview_title();
	}

	void workbench_window::update_public_preview_title()
	{
		const auto title = PublicPreviewSignedOut ? QStringLiteral("BetterPullReview — %1 — %2 profile").arg(public_preview_mode_text(), ProfileLabel)
												  : QStringLiteral("BetterPullReview — %1 profile").arg(ProfileLabel);
		setWindowTitle(title);
		setAccessibleName(title);
	}

	void workbench_window::apply_public_preview_availability()
	{
		const auto restore = [](QWidget* const aWidget) {
			if (aWidget != nullptr) {
				aWidget->setEnabled(true);
				aWidget->setToolTip(QString{ });
			}
		};
		if (!PublicPreviewSignedOut) {
			restore(QueueRoleFilter);
			restore(ReviewEditor);
			restore(
				ConversationPanel != nullptr ? ConversationPanel->reveal_button()
											 : nullptr);
			return;
		}
		// Disabled and explained, never hidden and never defaulted. A control
		// that vanished would leave the reviewer guessing whether the mode
		// cannot do this or the pull request simply has none of it.
		const auto refuseAction
			= [](QAction* const aAction, const QString& aName) {
				  if (aAction == nullptr) {
					  return;
				  }
				  const auto wording = public_preview_unavailable_text(aName);
				  aAction->setEnabled(false);
				  aAction->setToolTip(wording);
				  aAction->setStatusTip(wording);
			  };
		const auto refuseWidget
			= [](QWidget* const aWidget, const QString& aName) {
				  if (aWidget == nullptr) {
					  return;
				  }
				  aWidget->setEnabled(false);
				  aWidget->setToolTip(public_preview_unavailable_text(aName));
			  };
		for (auto* const action : ReceiptActions) {
			refuseAction(action, QStringLiteral("Private review progress"));
		}
		refuseWidget(QueueRoleFilter, QStringLiteral("Queue role"));
		refuseWidget(ReviewEditor, QStringLiteral("Publishing a review"));
	}

	void workbench_window::apply_public_preview_conversation()
	{
		// Body content belongs to this window's accepted read projection. The
		// profile-wide preview status cannot replace it with a sibling's posts.
		if (PublicPreviewSignedOut && ConversationPanel != nullptr && !HasGithubPullRequest) {
			ConversationPanel->clear(QStringLiteral("Review comments were not observable."));
		}
	}

	auto workbench_window::usage_indicator() const noexcept -> usage_ring*
	{
		auto* const card = current_server_card();
		return card != nullptr ? card->usage_indicator() : nullptr;
	}

	auto workbench_window::public_preview_signed_out() const noexcept -> bool
	{
		return PublicPreviewSignedOut;
	}

	auto workbench_window::public_preview_refusal_text() const -> QString
	{
		return SessionPanel != nullptr ? SessionPanel->public_preview_refusal_text()
									   : QString{ };
	}

	void workbench_window::update_github_presentation_state()
	{
		// The session panel reports a presentation change from inside its own
		// constructor, before the member holding it has been assigned, so this
		// can run with nothing to read. Guarding here rather than deferring the
		// callback keeps the panel free to report whenever it likes.
		if (SessionPanel == nullptr || PullRequestStack == nullptr) {
			return;
		}
		PullRequestStack->setCurrentIndex(HasGithubPullRequest ? 1 : 0);
		PullRequestPanel->body_editor()->setVisible(HasGithubPullRequest);
		// [[BPR-0773]]. Sign-in is a state of its server's card: the card
		// holding the device sign-in shows it as its account row while a flow
		// runs (`update_server_cards`), so nothing here shows or hides it. A
		// flow begins from the accounts page itself -- the strip's account
		// button opens it -- so nothing here presents a window either.
		update_account_action();
		update_server_cards();
		update_home_landing();

		QString state;
		if (HasGithubPullRequest) {
			switch (SessionPanel->status()) {
			case core::github_status::loading_pull_request:
				state = QStringLiteral("refreshing");
				break;
			case core::github_status::pull_request_failed:
				state = QStringLiteral("loaded-error");
				break;
			case core::github_status::pull_request_cancelled:
				state = QStringLiteral("loaded-cancelled");
				break;
			default:
				state = QStringLiteral("loaded");
				break;
			}
		} else {
			switch (SessionPanel->status()) {
			case core::github_status::no_profile:
				state = QStringLiteral("disconnected");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Connect to GitHub"));
				EmptyStateBodyLabel->setText(
					GithubProfiles.empty()
						? QStringLiteral(
							  "No GitHub host profiles are configured.")
						: QStringLiteral(
							  "Choose a host under the account button, then connect."));
				break;
			case core::github_status::establishing_session:
				state = QStringLiteral("connecting");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Connecting to GitHub…"));
				EmptyStateBodyLabel->setText(
					QStringLiteral(
						"The selected host is establishing an authenticated "
						"session."));
				break;
			case core::github_status::session_ready:
			case core::github_status::pull_request_ready:
				state = QStringLiteral("empty");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Open a pull request"));
				EmptyStateBodyLabel->setText(
					QStringLiteral(
						"Paste a pull request link, or owner/name#123, in the bar "
						"on Home."));
				break;
			case core::github_status::loading_pull_request:
				state = QStringLiteral("loading");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Loading pull request…"));
				EmptyStateBodyLabel->setText(
					QStringLiteral(
						"BetterPullReview is reading the selected GitHub "
						"snapshot."));
				break;
			case core::github_status::session_failed:
				state = QStringLiteral("error");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Connection unavailable"));
				EmptyStateBodyLabel->setText(
					!SessionPanel->showing_diagnostic()
						? QStringLiteral(
							  "Review the connection details and try again.")
						: QString::fromStdString(SessionPanel->diagnostic_text()));
				break;
			case core::github_status::session_cancelled:
				state = QStringLiteral("cancelled");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Connection canceled"));
				EmptyStateBodyLabel->setText(
					QStringLiteral(
						"Choose Connect when you are ready to try again."));
				break;
			case core::github_status::pull_request_failed:
				state = QStringLiteral("error");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Pull request unavailable"));
				EmptyStateBodyLabel->setText(
					!SessionPanel->showing_diagnostic()
						? QStringLiteral(
							  "Check the link in the bar and open it again.")
						: QString::fromStdString(SessionPanel->diagnostic_text()));
				break;
			case core::github_status::pull_request_cancelled:
				state = QStringLiteral("cancelled");
				EmptyStateTitleLabel->setText(
					QStringLiteral("Pull request loading canceled"));
				EmptyStateBodyLabel->setText(
					QStringLiteral(
						"The link in the bar is unchanged and can be opened again."));
				break;
			}
		}
		PullRequestStack->setProperty("githubPresentationState", state);

		update_home_open_notice();
	}

	auto workbench_window::appearance_settings() const noexcept
		-> appearance_settings_panel*
	{
		return AppearanceSettings;
	}

	auto workbench_window::github_status_text() const -> std::string
	{
		return SessionPanel->status_text();
	}

	auto workbench_window::github_diagnostic_text() const -> std::string
	{
		return SessionPanel->diagnostic_text();
	}

	auto workbench_window::active_host_text() const -> std::string
	{
		return SessionPanel->active_host_text();
	}

	auto workbench_window::active_account_text() const -> std::string
	{
		return SessionPanel->active_account_text();
	}

	auto workbench_window::conversation_view() const noexcept -> QTreeView*
	{
		return ConversationPanel->view();
	}

	auto workbench_window::conversation_model() const noexcept
		-> github_conversation_model*
	{
		return ConversationPanel->model();
	}

	auto workbench_window::server_cards() const -> std::vector<server_card*>
	{
		return ServerCards;
	}

	auto workbench_window::server_card_for(const core::host_profile_id aProfile) const
		-> server_card*
	{
		const auto found = std::ranges::find_if(ServerCards, [aProfile](const server_card* const aCard) {
			return aCard->holds(aProfile);
		});
		return found == ServerCards.end() ? nullptr : *found;
	}

	auto workbench_window::current_server_card() const -> server_card*
	{
		if (SessionPanel == nullptr) {
			return nullptr;
		}
		const auto active  = SessionPanel->active_profile();
		const auto current = active.has_value() ? active : SessionPanel->selected_profile();
		return current.has_value() ? server_card_for(*current) : nullptr;
	}

	void workbench_window::set_public_preview_dispatch(
		application::public_preview_gesture_dispatch aDispatch)
	{
		PublicPreviewDispatch = std::move(aDispatch);
		for (auto* const card : ServerCards) {
			card->set_public_preview_dispatch(PublicPreviewDispatch);
		}
	}

	void workbench_window::select_github_profile(const core::host_profile_id aProfile)
	{
		if (SessionPanel == nullptr) {
			return;
		}
		SessionPanel->select_profile(aProfile);
		rebind_device_authorization_profile();
		update_server_cards();
	}

	void workbench_window::add_server_card(const application::github_profile_option& aOption)
	{
		if (ServerCardColumn == nullptr || server_card_for(aOption.Profile) != nullptr) {
			return;
		}
		// [[BPR-0772]]. One card per configured server: another way in to a
		// hostname already on the page joins that server's card.
		if (std::ranges::any_of(ServerCards, [&aOption](server_card* const aCard) {
				return aCard->adopt_option(aOption);
			})) {
			place_device_sign_in();
			return;
		}
		// The card asks; the window decides. Connect is the host chosen, then
		// the existing connect intent. Sign in is the device flow where the
		// host has one, and otherwise the way in Connect already opens.
		auto* const card = new server_card{
			aOption,
			server_card_gestures{
				.Connect =
					[this](const core::host_profile_id aProfile) {
						// [[BPR-1023]]. Every Connect on a card, and its GitHub CLI
						// offer, answers to the one admission the card shows it by:
						// the core's host-activation command. A device-capable
						// card's Connect begins device sign-in rather than
						// dispatching the activation, so it asks here first
						// instead of leaving the device command to decide. A
						// Connect that is not shown does nothing, as the
						// enterprise card's and the GitHub CLI offer's already did.
						if (SessionPanel == nullptr || !SessionPanel->connect_enabled()) {
							return;
						}
						const auto* chosen = server_card_for(aProfile);
						const auto  cli    = chosen->cli_option() && chosen->cli_option()->Profile == aProfile;
						if (cli) {
							present_cli_account_choices({ aProfile });
						}
						open_sign_in(chosen->hostname(), cli);
						select_github_profile(aProfile);
						if (chosen->offers_device_sign_in() && chosen->native_option() && chosen->native_option()->Profile == aProfile) {
							begin_selected_sign_in();
						} else {
							GithubConnectAction->trigger();
						}
					},
				.SignIn =
					[this](const core::host_profile_id aProfile) {
						open_sign_in(server_card_for(aProfile)->hostname());
						select_github_profile(aProfile);
						if (const auto* const chosen = server_card_for(aProfile);
							chosen != nullptr && chosen->offers_device_sign_in()) {
							begin_selected_sign_in();
						} else {
							GithubConnectAction->trigger();
						}
					},
				.SignOut =
					[this] {
						if (GithubDispatch) {
							GithubDispatch(core::deactivate_github_identity_intent{ });
						}
					},
				.Remember =
					[this, hostname = aOption.Hostname] {
						remember_enterprise_server(hostname);
					},
				.Forget =
					[this](const application::saved_server& aServer) {
						confirm_forget_server(aServer);
					},
				.CheckCliAccounts =
					[this] {
						if (GithubCliCheckDispatch && GithubCliDiscoveryState != github_cli_discovery_state::discovering) {
							GithubCliCheckDispatch();
						}
					},
			},
			ServerCardColumn->parentWidget(),
		};
		if (PublicPreviewDispatch) {
			card->set_public_preview_dispatch(PublicPreviewDispatch);
		}
		ServerCards.push_back(card);
		ServerCardColumn->addWidget(card);
		place_device_sign_in();
	}

	void workbench_window::update_server_cards()
	{
		if (SessionPanel == nullptr || ServerCards.empty()) {
			return;
		}
		const auto  status   = SessionPanel->status();
		const auto  active   = SessionPanel->active_profile();
		const auto  current  = active.has_value() ? active : SessionPanel->selected_profile();
		const auto& session  = SessionPanel->session();
		const auto  reading  = SessionPanel->public_preview_signed_out();
		const auto  admitted = SessionPanel->connect_enabled();
		// [[BPR-0773]]. What the device sign-in shows decides its card's
		// account row and, while a flow runs, its header's word.
		const auto deviceFlow    = DevicePanel != nullptr && DevicePanel->shows_flow();
		const auto deviceSigning = DevicePanel != nullptr && DevicePanel->signing_in();
		for (auto* const card : ServerCards) {
			auto shown               = server_card_presentation{ };
			shown.Current            = current.has_value() && card->holds(*current);
			shown.Chosen             = shown.Current ? current : std::nullopt;
			shown.ConnectAdmitted    = admitted;
			shown.ConnectDescription = admitted
				? QStringLiteral("Chooses %1 and connects it.").arg(QString::fromStdString(card->hostname()))
				: SessionPanel->connect_description();
			shown.PublicPreview      = SessionPanel->public_preview();
			shown.PublicPreviewNow   = SessionPanel->public_preview_now();
			if (shown.Current) {
				switch (status) {
				case core::github_status::no_profile:
				case core::github_status::session_cancelled:
					shown.State = server_card_state::signed_out;
					break;
				case core::github_status::establishing_session:
					shown.State = server_card_state::connecting;
					break;
				case core::github_status::session_failed:
					shown.State       = server_card_state::refused;
					shown.LastProblem = SessionPanel->showing_diagnostic()
						? QString::fromStdString(SessionPanel->diagnostic_text())
						: QString::fromStdString(SessionPanel->status_text());
					// [[BPR-0644]]. The host kept the remembered grant through
					// this failure: one that answered as another account says
					// so as its error, a refused one needs signing in again,
					// and anything else is tried again at the next launch.
					if (const auto kept = SessionPanel->kept_grant_reason(); kept.has_value()) {
						shown.KeptGrant = SessionPanel->kept_grant_names_another_account() ? kept_grant::another_account
							: *kept == core::github_reason_code::authentication_failed     ? kept_grant::needs_sign_in
																						   : kept_grant::retried_next_launch;
					}
					break;
				case core::github_status::session_ready:
				case core::github_status::loading_pull_request:
				case core::github_status::pull_request_ready:
				case core::github_status::pull_request_failed:
				case core::github_status::pull_request_cancelled:
					// Signed out and reading public pull requests is not an account.
					shown.State         = reading ? server_card_state::signed_out : server_card_state::connected;
					shown.ReadingPublic = reading;
					// The refusal that ended the last signed-out read is said beside the
					// reading, in the card's Details, rather than under the Home bar.
					if (reading) {
						shown.LastProblem = SessionPanel->public_preview_refusal_text();
					} else if (status == core::github_status::pull_request_failed && SessionPanel->showing_diagnostic()) {
						shown.LastProblem = QString::fromStdString(SessionPanel->diagnostic_text());
					}
					break;
				}
				if (!reading && session.has_value() && card->holds(session->Profile)) {
					shown.Session = session;
				}
				shown.Capabilities = SessionPanel->capabilities();
				shown.RateLimits   = SessionPanel->rate_limits();
			}
			const auto holdsSignIn = DevicePanel != nullptr && card->device_sign_in() == DevicePanel;
			// A running flow's row stays with its card; a finished one is said
			// only on the host it was asked for, so choosing another host does not
			// carry a denial or an expiry to a card that never had the code.
			const auto renewal = DevicePanel != nullptr
				&& DevicePanel->refresh_observation().State == application::device_authorization_refresh_state::reenrollment_required;
			const auto flowHere = deviceSigning || renewal || !DeviceFlowHost.has_value() || card->hostname() == *DeviceFlowHost;
			shown.DeviceSignIn  = holdsSignIn && deviceFlow && flowHere;
			// Sign in is shown only while its way in is admitted: the device
			// flow's begin for a host with one, otherwise Connect.
			shown.SignInAdmitted = admitted && (!card->offers_device_sign_in() || (DevicePanel != nullptr && DevicePanel->begin_button()->isEnabled()));
			const auto signedIn  = shown.State == server_card_state::connected && shown.Session.has_value();
			if (holdsSignIn && deviceSigning && !signedIn) {
				shown.State = server_card_state::signing_in;
			}
			// [[BPR-1373]]. Every gh account on this host from the last check.
			// A refused session on an account gh has signed out is said as
			// that, not as a failure to connect: gh fell back to its active
			// account and the viewer check refused it.
			if (const auto listed = CliAccountsByHost.find(card->hostname()); listed != CliAccountsByHost.end()) {
				shown.CliAccounts = listed->second;
				if (shown.State == server_card_state::refused && shown.Chosen.has_value()) {
					const auto chosen = std::ranges::find_if(shown.CliAccounts, [&](const server_card_cli_account& aAccount) {
						return aAccount.Profile == shown.Chosen && aAccount.State != server_card_cli_account_state::signed_in;
					});
					if (chosen != shown.CliAccounts.end()) {
						shown.State       = server_card_state::signed_out;
						shown.KeptGrant   = kept_grant::none;
						shown.LastProblem = QStringLiteral("%1 is signed out in gh, so this profile is signed out too. Sign it in with gh auth login, then check gh again.")
												.arg(QString::fromStdString(chosen->Login));
					}
				}
			}
			shown.CliCheckAdmitted = static_cast<bool>(GithubCliCheckDispatch) && GithubCliDiscoveryState != github_cli_discovery_state::discovering;
			// [[BPR-0774]]. An enterprise server's card remembers exactly the
			// record the queue keeps for its hostname.
			shown.RememberAdmitted = card->enterprise() && QueueReady && static_cast<bool>(SavedServerDispatch);
			if (card->enterprise()) {
				const auto kept = std::ranges::find(QueueProjection.SavedServers, card->hostname(), &application::saved_server::Hostname);
				if (kept != QueueProjection.SavedServers.end()) {
					shown.Remembered = *kept;
				}
			}
			card->apply(shown);
		}
		update_sign_in_surface();
	}

	auto workbench_window::session_panel() const noexcept -> github_session_panel*
	{
		return SessionPanel;
	}

	auto workbench_window::device_authorization() const noexcept
		-> device_authorization_panel*
	{
		return DevicePanel;
	}

	void workbench_window::begin_selected_sign_in()
	{
		const auto* card = current_server_card();
		open_sign_in(card ? card->hostname() : std::string{ });
		if (DevicePanel == nullptr) {
			return;
		}
		rebind_device_authorization_profile();
		// The same command-projected button owns admission and profile binding
		// for Home, the strip and Settings. A running flow cannot begin twice.
		DevicePanel->begin_button()->setFocus(Qt::OtherFocusReason);
		DevicePanel->begin_button()->click();
	}

	void workbench_window::rebind_device_authorization_profile()
	{
		if (DevicePanel == nullptr || SessionPanel == nullptr) {
			return;
		}
		if (const auto profile = SessionPanel->selected_profile();
			profile.has_value()) {
			DevicePanel->bind_profile(*profile);
		}
		place_device_sign_in();
	}

	void workbench_window::place_device_sign_in()
	{
		if (DevicePanel == nullptr || ServerCards.empty()) {
			return;
		}
		auto* target = server_card_for(DevicePanel->bound_profile());
		if (target == nullptr) {
			target = current_server_card();
		}
		if (target == nullptr) {
			target = ServerCards.front();
		}
		if (target->device_sign_in() == DevicePanel) {
			return;
		}
		// A live code stays on the card of the host it was asked for. The core
		// forbids a new flow while one runs, so the row follows a newly chosen
		// host once the running one has settled, and the observer places it
		// then.
		if (DevicePanel->signing_in()
			&& std::ranges::any_of(ServerCards, [this](const server_card* const aCard) {
				   return aCard->device_sign_in() == DevicePanel;
			   })) {
			return;
		}
		for (auto* const card : ServerCards) {
			if (card != target && card->device_sign_in() == DevicePanel) {
				card->release_device_sign_in();
			}
		}
		target->adopt_device_sign_in(DevicePanel);
		update_server_cards();
	}

	auto workbench_window::pull_request_panel() const noexcept
		-> github_pull_request_panel*
	{
		return PullRequestPanel;
	}

	auto workbench_window::github_connect_action() const noexcept -> QAction*
	{
		return GithubConnectAction;
	}

	auto workbench_window::github_open_action() const noexcept -> QAction*
	{
		return GithubOpenAction;
	}

	auto workbench_window::github_refresh_action() const noexcept -> QAction*
	{
		return GithubRefreshAction;
	}

	auto workbench_window::queue_refresh_action() const noexcept -> QAction*
	{
		return QueueRefreshAction;
	}

	auto workbench_window::home_discovery_status() const noexcept -> QLabel*
	{
		return HomeDiscoveryStatus;
	}

	auto workbench_window::pull_request_menu() const noexcept -> QMenu*
	{
		return PullRequestMenu;
	}

	auto workbench_window::follow_pull_request_action() const noexcept -> QAction*
	{
		return FollowPullRequest;
	}

	auto workbench_window::save_pull_request_repository_action() const noexcept -> QAction*
	{
		return SavePullRequestRepository;
	}

	void workbench_window::set_queue_follow_dispatch(workbench_queue_follow_dispatch aDispatch)
	{
		QueueFollowDispatch = std::move(aDispatch);
		update_pull_request_queue_actions();
	}

	void workbench_window::set_queue_discovery_dispatch(workbench_queue_discovery_dispatch aDispatch)
	{
		QueueDiscoveryDispatch = std::move(aDispatch);
		update_discovery_controls();
	}

	void workbench_window::set_home_clock(std::function<std::optional<std::int64_t>()> aClock)
	{
		HomeClock = std::move(aClock);
		QueueLanding->set_now(HomeClock ? HomeClock() : std::nullopt);
	}

	auto workbench_window::open_queue_ref() const -> std::optional<core::pull_request_ref>
	{
		// Only a pull request read under this window's account session can be
		// kept for that account: the queue session names the server and login.
		if (!HasGithubPullRequest || OpenQueuePublicPreview || OpenQueueRepository.empty() || OpenQueueNumber <= 0 || !QueueReady
			|| QueueProjection.Session.Host.empty() || QueueProjection.Session.Account.empty()) {
			return std::nullopt;
		}
		return core::pull_request_ref{
			.Host       = QueueProjection.Session.Host,
			.Account    = QueueProjection.Session.Account,
			.Repository = OpenQueueRepository,
			.Number     = OpenQueueNumber,
		};
	}

	void workbench_window::update_discovery_controls()
	{
		const auto& discovery = QueueProjection.Discovery;
		const auto  session   = !discovery.Account.empty();
		if (QueueRefreshAction != nullptr) {
			const auto refreshing = discovery.State == application::queue_discovery_state::refreshing;
			QueueRefreshAction->setEnabled(static_cast<bool>(QueueDiscoveryDispatch) && HomeAccount && session && !refreshing);
			const auto tip = !QueueDiscoveryDispatch || !HomeAccount || !session
				? QStringLiteral("Sign in to find my pull requests in saved repositories.")
				: refreshing ? QStringLiteral("Finding my pull requests in saved repositories…")
							 : QStringLiteral("Find my pull requests in saved repositories again; this never changes GitHub.");
			QueueRefreshAction->setToolTip(tip);
			QueueRefreshAction->setStatusTip(tip);
			if (auto* const glyph = PrimaryToolbar != nullptr ? PrimaryToolbar->widgetForAction(QueueRefreshAction) : nullptr; glyph != nullptr) {
				glyph->setAccessibleDescription(tip);
			}
		}
		update_home_repository_onboarding();
		if (HomeRepositoryRow != nullptr) {
			if (auto* row = qobject_cast<QBoxLayout*>(HomeRepositoryRow->layout())) {
				row->setDirection(width() < 640 ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
			}
		}
		if (HomeDiscoveryStatus != nullptr) {
			const auto hasSaved = std::ranges::any_of(QueueProjection.SavedRepositories, [this](const auto& aSaved) {
				return aSaved.Host == QueueProjection.Session.Host && aSaved.Account == QueueProjection.Session.Account;
			});
			const auto sentence = !HomeAccount ? QString{ } : !hasSaved ? tr("Add a repository to find my pull requests and review requests.")
				: session                                               ? home_discovery_sentence(discovery)
																		: tr("Personal discovery is unavailable. Saved repositories are kept.");
			if (HomeDiscoveryStatus->text() != sentence) {
				HomeDiscoveryStatus->setText(sentence);
			}
			HomeDiscoveryStatus->setVisible(!sentence.isEmpty());
		}
	}

	void workbench_window::update_home_repository_onboarding()
	{
		if (HomeOnboarding == nullptr || HomeRepositoryRow == nullptr) {
			return;
		}
		// Only a ready queue knows that nothing is saved: while it restores, the
		// ordinary row stands, so a returning reviewer never sees onboarding flash.
		const auto session    = QueueReady && !QueueProjection.Session.Host.empty() && !QueueProjection.Session.Account.empty();
		const auto hasSaved   = std::ranges::any_of(QueueProjection.SavedRepositories, [this](const auto& aSaved) {
			return aSaved.Host == QueueProjection.Session.Host && aSaved.Account == QueueProjection.Session.Account;
		});
		const auto onboarding = HomeAccount && session && !hasSaved;
		const auto handFocus  = !onboarding && HomeOnboardingButton->hasFocus();
		HomeOnboarding->setVisible(onboarding);
		HomeRepositoryRow->setVisible(HomeAccount && !onboarding);
		// With nothing saved, an empty queue's Needs me lane could only ever say
		// that nothing needs me; rows enrolled by an observer rule or a follow
		// still show, as does Recently viewed and Review Cycles.
		if (HomeList != nullptr && HomeList->onboarding() != onboarding) {
			HomeList->set_onboarding(onboarding);
			HomeList->refresh();
		}
		HomeOnboardingButton->setEnabled(AddSavedRepository != nullptr && AddSavedRepository->isEnabled());
		// The first save retires the call to action under the keyboard; its
		// ordinary Add repository takes the focus rather than nothing.
		if (handFocus && HomeAddRepositoryButton->isVisibleTo(this)) {
			HomeAddRepositoryButton->setFocus(Qt::OtherFocusReason);
		}
	}

	void workbench_window::update_pull_request_queue_actions()
	{
		// [[BPR-1365]]. The queue row of the open pull request says so, by the
		// same exact identity Follow names.
		if (QueueLanding != nullptr) {
			QueueLanding->set_reviewing(open_queue_ref());
		}
		if (FollowPullRequest == nullptr || SavePullRequestRepository == nullptr) {
			return;
		}
		const auto ref      = open_queue_ref();
		const auto item     = ref ? QueueLanding->item_for(*ref) : std::nullopt;
		const auto followed = item && item->Membership.Followed;
		FollowPullRequest->setEnabled(ref.has_value() && static_cast<bool>(QueueFollowDispatch));
		FollowPullRequest->setChecked(followed);
		const auto followTip = ref.has_value()
			? (followed ? QStringLiteral("Stop following this pull request in my queue; nothing changes on GitHub.")
						: QStringLiteral("Keep this pull request in my queue; nothing changes on GitHub."))
			: QStringLiteral("Open a pull request signed in to follow it.");
		FollowPullRequest->setToolTip(followTip);
		FollowPullRequest->setStatusTip(followTip);
		const auto saved = ref.has_value() && std::ranges::any_of(QueueProjection.SavedRepositories, [&ref](const application::saved_repository& aSaved) {
							   return aSaved.Host == ref->Host && aSaved.Account == ref->Account && aSaved.Repository == ref->Repository;
						   });
		SavePullRequestRepository->setEnabled(ref.has_value() && !saved && !RepositorySaving && static_cast<bool>(RepositoryAdditionDispatch));
		const auto saveTip = !ref.has_value() ? QStringLiteral("Open a pull request signed in to save its repository.")
			: saved                           ? QStringLiteral("This pull request's repository is saved.")
											  : QStringLiteral("Keep this repository so my pull requests in it are found; nothing changes on GitHub.");
		SavePullRequestRepository->setToolTip(saveTip);
		SavePullRequestRepository->setStatusTip(saveTip);
	}

	auto workbench_window::show_review_threads_action() const noexcept -> QAction*
	{
		return nullptr;
	}

	auto workbench_window::conversation_panel() const noexcept
		-> github_conversation_panel*
	{
		return ConversationPanel;
	}

	auto workbench_window::agent_panel() const noexcept -> agent_launch_panel*
	{
		return AgentPanel;
	}

	auto workbench_window::private_notes() const noexcept -> private_notes_panel* { return PrivateNotes; }

	void workbench_window::withdraw_agent_surfaces()
	{
		for (auto* const widget : std::initializer_list<QWidget*>{ AgentPanel, SideChat, FindingQueue, ClaudeWrapper }) {
			if (widget != nullptr) {
				widget->hide();
			}
		}
		// A button whose default action is invisible hides itself, so the
		// drawer's chat and finding buttons and the workshop's controls follow
		// their actions wherever they are placed.
		auto actions = std::vector<QAction*>{ };
		if (SideChat != nullptr) {
			actions.insert(actions.end(), { SideChat->send_action(), SideChat->add_context_action(), SideChat->new_session_action(), SideChat->continue_old_revision_action() });
		}
		if (FindingQueue != nullptr) {
			for (auto index = std::size_t{ 0U }; index < finding_queue_action_count; ++index) {
				actions.push_back(FindingQueue->action(static_cast<finding_queue_action>(index)));
			}
		}
		if (ReviewEditor != nullptr) {
			actions.insert(actions.end(), { ReviewEditor->accept_revision_action(), ReviewEditor->reject_revision_action() });
			for (auto index = std::size_t{ 0U }; index < core::workshop_operation_count; ++index) {
				actions.push_back(ReviewEditor->workshop_action(static_cast<core::workshop_operation>(index)));
			}
		}
		for (auto* const action : actions) {
			if (action != nullptr) {
				action->setVisible(false);
			}
		}
		for (const auto* name : { "reviewWorkshopOriginal", "reviewWorkshopProposal", "reviewWorkshopOperation", "reviewWorkshopCandidate",
								  "tourProblemMagic", "tourSolutionMagic", "tourStopMagic", "tourSuggestOutline", "tourSuggestedOutline", "tourInsertOutline",
								  "privateAgentProfile", "privateRequestedModel", "privateRequestedEffort", "privateAskAgent", "privateCompositionAsk" }) {
			for (auto* const widget : findChildren<QWidget*>(QString::fromLatin1(name))) {
				widget->hide();
			}
		}
		// The composition editor's profile is a labelled form row; the row goes
		// with it, and the agent-assisted mode can no longer be chosen.
		for (auto* const profile : findChildren<QWidget*>(QStringLiteral("privateCompositionProfile"))) {
			for (auto* const form : profile->parentWidget()->findChildren<QFormLayout*>()) {
				if (form->indexOf(profile) >= 0) {
					form->setRowVisible(profile, false);
				}
			}
		}
		for (auto* const mode : findChildren<QComboBox*>(QStringLiteral("privateCompositionMode"))) {
			if (auto* const items = qobject_cast<QStandardItemModel*>(mode->model()); items != nullptr && items->rowCount() > 2) {
				items->item(2)->setEnabled(false);
			}
		}
		if (auto* const tabs = findChild<QTabWidget*>(QStringLiteral("privateConversationTabs")); tabs != nullptr && ConversationScroll != nullptr) {
			tabs->setTabText(tabs->indexOf(ConversationScroll), tr("Private notes"));
			tabs->setAccessibleName(tr("Private history and notes"));
		}
		if (auto* const purpose = findChild<QLabel*>(QStringLiteral("workbenchNotesPurpose")); purpose != nullptr) {
			purpose->setText(tr("Private to you and never published. Write a note about this pull request, then promote what you want to say to a draft in Comments."));
			purpose->setAccessibleDescription(purpose->text());
		}
		if (SettingsWindow != nullptr) {
			SettingsWindow->withdraw_page(settings_page::agents_and_context);
		}
	}

	auto workbench_window::side_chat() const noexcept -> side_chat_panel*
	{
		return SideChat;
	}

	auto workbench_window::finding_queue() const noexcept -> finding_queue_view*
	{
		return FindingQueue;
	}

	auto workbench_window::review_editor() const noexcept -> review_draft_editor*
	{
		return ReviewEditor;
	}

	auto workbench_window::defect_card() const noexcept -> defect_card_view*
	{
		return DefectCard;
	}

	auto workbench_window::classify_defect_action() const noexcept -> QAction*
	{
		return ClassifyDefect;
	}

	auto workbench_window::previous_defect_action() const noexcept -> QAction*
	{
		return PreviousDefect;
	}

	auto workbench_window::next_defect_action() const noexcept -> QAction*
	{
		return NextDefect;
	}

	auto workbench_window::defect_count_label() const noexcept -> QLabel*
	{
		return DefectCountLabel;
	}

	auto workbench_window::shown_defect_index() const noexcept
		-> std::optional<std::size_t>
	{
		return ShownDefectIndex;
	}

	auto workbench_window::defect_count() const noexcept -> std::size_t
	{
		return Defects.size();
	}

	auto workbench_window::queue_landing() const noexcept -> review_landing_model*
	{
		return QueueLanding;
	}

	auto workbench_window::queue_filter_model() const noexcept
		-> review_landing_filter_model*
	{
		return QueueFilterModel;
	}

	auto workbench_window::home_list() const noexcept -> qt::home_list*
	{
		return HomeList;
	}

	auto workbench_window::selected_queue_item() const -> std::optional<core::queue_item>
	{
		if (HomeList == nullptr) {
			return std::nullopt;
		}
		return HomeList->current_queue_item();
	}

	auto workbench_window::home_open_bar() const noexcept -> QLineEdit*
	{
		return HomeOpenBar;
	}

	void workbench_window::open_pull_request_item(const core::queue_item& aItem)
	{
		if (!GithubDispatch) {
			return;
		}
		const auto repository = QString::fromStdString(aItem.Ref.Repository);
		const auto slash      = repository.indexOf(QLatin1Char{ '/' });
		if (slash <= 0 || aItem.Ref.Number <= 0) {
			return;
		}
		auto intent = core::open_pull_request_intent{
			.Repository = core::repository_locator{
				.Owner = repository.left(slash).toStdString(),
				.Name  = repository.mid(slash + 1).toStdString(),
			},
			.Number = core::pull_request_number{ static_cast<std::uint32_t>(aItem.Ref.Number) },
		};
		// [[BPR-1357]]. The pull request already being opened asks for nothing
		// again, whichever row or key asked: the core says it is in flight.
		if (HomeOpen.has_value() && core::home_open_in_flight(HomeOpen->State, intent.Repository, intent.Number)) {
			return;
		}
		// [[BPR-0651]] [[BPR-0650]]. A pull request chosen from a list
		// arrives on Overview.
		OpenOnOverview = true;
		GithubDispatch(std::move(intent));
	}

	auto workbench_window::home_open_action() const noexcept -> QAction*
	{
		return HomeOpenAction;
	}

	auto workbench_window::home_open_notice() const noexcept -> QLabel*
	{
		return HomeOpenNotice;
	}

	auto workbench_window::home_open_details_button() const noexcept -> QPushButton*
	{
		return HomeOpenDetails;
	}

	auto workbench_window::home_refusal_card() const noexcept -> home_refusal*
	{
		return HomeRefusal;
	}

	void workbench_window::present_home_restoration_notice(const QString& aText)
	{
		HomeRestorationNotice = aText;
		update_home_open_notice();
	}

	auto workbench_window::home_recent_panel() const noexcept -> home_panel*
	{
		return HomeList != nullptr ? HomeList->recent_panel() : nullptr;
	}

	auto workbench_window::home_recent_row_action() const noexcept -> QToolButton*
	{
		return HomeList != nullptr ? HomeList->recent_action() : nullptr;
	}

	auto workbench_window::home_recent_list() const noexcept -> QListWidget*
	{
		return HomeRecentList;
	}

	auto workbench_window::home_recent_resume_action() const noexcept -> QAction*
	{
		return HomeRecentResumeAction;
	}

	auto workbench_window::home_recent_remove_action() const noexcept -> QAction*
	{
		return HomeRecentRemoveAction;
	}

	void workbench_window::set_recent_review_dispatch(
		std::function<void(application::recent_review_intent)> aDispatch)
	{
		RecentReviewDispatch = std::move(aDispatch);
	}

	void workbench_window::set_recent_removal_dispatch(std::function<void(application::recent_review_row)> aDispatch)
	{
		RecentRemovalDispatch = std::move(aDispatch);
		update_recent_resume_action();
	}

	void workbench_window::publish(const application::recent_reviews_projection& aProjection)
	{
		RecentReviews = aProjection;
		apply_recent_reviews();
		// The section appears and disappears with its entries, so the declared
		// chain is rebuilt around it. Focus itself is never moved: a
		// publication must not take the keyboard from wherever the reviewer
		// left it, which is the same rule [[BPR-0732]] holds Home's bar to.
		apply_focus_order();
	}

	void workbench_window::apply_recent_reviews()
	{
		if (HomeRecentList == nullptr) {
			return;
		}
		const auto previousRow = HomeRecentList->currentRow();
		{
			// The rows are replaced in one step, so nothing reads a half-filled
			// list; the row the reviewer had is kept where the new list still
			// has one. Making a row current is a selection, not a focus move.
			const QSignalBlocker blocked{ HomeRecentList };
			HomeRecentList->clear();
			for (const auto& entry : RecentReviews.Entries) {
				HomeRecentList->addItem(recent_review_item(entry));
			}
			if (!RecentReviews.Entries.empty()) {
				const auto rows = static_cast<int>(RecentReviews.Entries.size());
				// [[BPR-1365]]. Current, but selected only while Recently viewed is
				// the list with Home's keyboard: a publication never paints a
				// second selection beside the queue's.
				HomeRecentList->setCurrentRow(
					previousRow >= 0 && previousRow < rows ? previousRow : 0,
					HomeSelection == home_selection_list::recent ? QItemSelectionModel::ClearAndSelect : QItemSelectionModel::NoUpdate);
			}
		}
		// [[BPR-0776]]. The rows are the last panel of Home's list, which appears
		// with them. Home scrolls as one page, so the list is as tall as its
		// rows rather than five of them.
		if (HomeList != nullptr) {
			HomeList->set_recent(static_cast<int>(RecentReviews.Entries.size()),
								 home_recent_summary(RecentReviews.Entries));
		}
		update_home_surface();
		update_recent_resume_action();
		update_pending_panes();
		// [[BPR-1357]]. A row that appeared or went may carry the opening.
		if (HomeOpen.has_value()) {
			refresh_home_open();
		}
	}

	void workbench_window::update_home_surface()
	{
		// [[BPR-0776]]. The queue's surface holds its status, Home's list and
		// the queue's controls. An account shows all of them; without one only
		// Recently viewed can have anything to list.
		if (QueueSurface != nullptr) {
			QueueSurface->setVisible(HomeAccount || !RecentReviews.Entries.empty());
		}
		// Without an account the surface is shown only for Recently viewed, and
		// then the queue's status is hidden itself; otherwise it follows the
		// surface, as it always has. [[BPR-0783]]: the six typed controls are
		// Settings' now, so Home has none of its own to hide here.
		const auto controls = HomeAccount || RecentReviews.Entries.empty();
		if (QueueStatus != nullptr) {
			QueueStatus->setVisible(controls && QueueStatusNotable);
		}
		if (HomeList != nullptr) {
			HomeList->set_queue_status(QueueReady, HomeAccount);
		}
	}

	void workbench_window::update_recent_resume_action()
	{
		if (HomeRecentResumeAction == nullptr || HomeRecentList == nullptr) {
			return;
		}
		const auto row    = HomeRecentList->currentRow();
		const auto listed = row >= 0 && static_cast<std::size_t>(row) < RecentReviews.Entries.size();
		HomeRecentResumeAction->setEnabled(listed && RecentReviews.Entries[static_cast<std::size_t>(row)].Resumable);
		if (HomeRecentRemoveAction != nullptr) {
			HomeRecentRemoveAction->setEnabled(listed && static_cast<bool>(RecentRemovalDispatch));
		}
	}

	void workbench_window::remove_recent_review(const int aRow)
	{
		if (!RecentRemovalDispatch || aRow < 0 || static_cast<std::size_t>(aRow) >= RecentReviews.Entries.size()) {
			return;
		}
		RecentRemovalDispatch(application::recent_review_row{ static_cast<std::size_t>(aRow) });
	}

	void workbench_window::claim_home_selection(const home_selection_list aList)
	{
		HomeSelection = aList;
		if (HomeList != nullptr) {
			HomeList->set_selection_shown(aList == home_selection_list::queue);
		}
		for (const auto& [list, view] : { std::pair{ home_selection_list::recent, static_cast<QAbstractItemView*>(HomeRecentList) },
										  std::pair{ home_selection_list::cycles, static_cast<QAbstractItemView*>(HomeCyclesList) } }) {
			if (view == nullptr) {
				continue;
			}
			if (list == aList) {
				home_show_selection(*view);
			} else {
				home_hide_selection(*view);
			}
		}
		update_open_on_github_action();
		update_home_review_actions();
	}

	void workbench_window::resume_home_cycle(const int aRow)
	{
		if (aRow >= 0 && static_cast<std::size_t>(aRow) < CycleRecents.size() && ResumeCycleDispatch) {
			ResumeCycleDispatch(CycleRecents[static_cast<std::size_t>(aRow)].Cycle.Subject);
		}
	}

	void workbench_window::dispatch_recent_review(const int aRow, const bool aResume)
	{
		if (!RecentReviewDispatch || aRow < 0
			|| static_cast<std::size_t>(aRow) >= RecentReviews.Entries.size()) {
			return;
		}
		const auto index = static_cast<std::size_t>(aRow);
		if (aResume && !RecentReviews.Entries[index].Resumable) {
			return;
		}
		// [[BPR-1357]]. A row already being opened asks for nothing again.
		if (const auto* const item = HomeRecentList->item(aRow);
			HomeOpen.has_value() && item != nullptr && HomeList != nullptr && HomeList->row_delegate() != nullptr) {
			const auto& open = HomeList->row_delegate()->open();
			if ((open.Phase == home_row_open_phase::opening || open.Phase == home_row_open_phase::waiting)
				&& item->data(home_row_role::identity).toString() == open.Identity) {
				return;
			}
		}
		// [[BPR-0733]] [[BPR-0651]]. A pull request chosen from a list arrives
		// on Overview, exactly as a queue row's does.
		OpenOnOverview = true;
		RecentReviewDispatch(application::recent_review_intent{
			.Index  = index,
			.Resume = aResume,
		});
	}

	auto workbench_window::eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool
	{
#if defined(_WIN32)
		// [[BPR-1022]]. The window's own input, before any of its widgets.
		if (auto* const handle = windowHandle();
			aEvent != nullptr && handle != nullptr && aWatched == handle && read_strip_menu_key(*aEvent)) {
			return true;
		}
#endif
		if (aEvent != nullptr && aEvent->type() == QEvent::Paint && aWatched == statusBar() && home_status_echoes_bar()) {
			return true;
		}
		if (TrayHeader != nullptr && aWatched == TrayHeader && aEvent != nullptr && aEvent->type() == QEvent::Resize) {
			fit_tray_tabs();
		}
		// [[BPR-1590]] v002. The expanded review stays centred as the page resizes.
		if (ComposeReviewPage != nullptr && aWatched == ComposeReviewPage->viewport() && aEvent != nullptr && aEvent->type() == QEvent::Resize) {
			centre_review_page();
		}
		// [[BPR-1590]]. The sheet keeps to the review area's right edge.
		if (aWatched == CentralSplitter && aEvent != nullptr && (aEvent->type() == QEvent::Resize || aEvent->type() == QEvent::Move) && ReviewSheetOpen) {
			place_review_sheet();
		}
		if (aWatched == DifCornerSlot && aEvent != nullptr
			&& (aEvent->type() == QEvent::Show || aEvent->type() == QEvent::Hide
				|| aEvent->type() == QEvent::Move || aEvent->type() == QEvent::Resize)) {
			place_dif_corner();
		}
		// The new repository's field: Escape discards the draft, as Cancel does,
		// before Settings would take it to close the window.
		if (aWatched == SavedRepositoryName && aEvent != nullptr && aEvent->type() == QEvent::KeyPress
			&& static_cast<QKeyEvent*>(aEvent)->key() == Qt::Key_Escape) {
			close_repository_draft();
			return true;
		}
		// Settings › Repositories: Delete (or Backspace, as macOS lists take it)
		// removes the selected repository, exactly as Remove does.
		if (aWatched == SavedRepositoryList && aEvent != nullptr && aEvent->type() == QEvent::KeyPress) {
			const auto key = static_cast<QKeyEvent*>(aEvent)->key();
			if ((key == Qt::Key_Delete || key == Qt::Key_Backspace) && RemoveSavedRepository->isEnabled()) {
				RemoveSavedRepository->trigger();
				return true;
			}
		}
		if (aWatched == ReviewBar && aEvent != nullptr
			&& (aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::StyleChange)) {
			apply_strip_width();
		}
		if (aWatched == HomeCyclesList && aEvent != nullptr && aEvent->type() == QEvent::KeyPress) {
			const auto* const key = static_cast<QKeyEvent*>(aEvent);
			if ((key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter)
				&& (key->modifiers() == Qt::NoModifier || key->modifiers() == Qt::KeypadModifier)) {
				resume_home_cycle(HomeCyclesList->currentRow());
				return true;
			}
		}
		if (aWatched == HomeRecentList && aEvent != nullptr
			&& aEvent->type() == QEvent::KeyPress) {
			auto* const key = static_cast<QKeyEvent*>(aEvent);
			// Delete, or Backspace as macOS lists take it, removes the row, as
			// Settings › Repositories' list does.
			if ((key->key() == Qt::Key_Delete || key->key() == Qt::Key_Backspace)
				&& key->modifiers() == Qt::NoModifier) {
				remove_recent_review(HomeRecentList->currentRow());
				return true;
			}
			if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
				// Both Return gestures are read here rather than left to the
				// view. On macOS Return is an item view's editing key and
				// never activates a row, so a keyboard reviewer would have no
				// way into the list at all; elsewhere the view would activate
				// on the shifted key too and resume would also open. Taking
				// the key means one behaviour on every platform, and the
				// pointer's own activation still arrives through
				// `itemDoubleClicked` ([[BPR-1364]]).
				// [[BPR-1342]]. With ⌘ (Ctrl off macOS) the row opens in a
				// new window, on Overview.
				if (key->modifiers().testFlag(Qt::ControlModifier)) {
					const auto row = HomeRecentList->currentRow();
					if (row >= 0 && static_cast<std::size_t>(row) < RecentReviews.Entries.size()) {
						request_review_window(application::recent_review_row{ static_cast<std::size_t>(row) });
					}
					return true;
				}
				dispatch_recent_review(
					HomeRecentList->currentRow(),
					key->modifiers().testFlag(Qt::ShiftModifier));
				return true;
			}
		}
		return QMainWindow::eventFilter(aWatched, aEvent);
	}

	void workbench_window::add_setup_profile(application::github_profile_option aProfile)
	{
		if (std::ranges::any_of(GithubProfiles, [&](const auto& aExisting) { return aExisting.Profile == aProfile.Profile; })) {
			return;
		}
		GithubProfiles.push_back(aProfile);
		add_server_card(aProfile);
		SessionPanel->add_profile(std::move(aProfile));
		update_server_cards();
	}

	void workbench_window::set_setup_destination_dispatch(application::setup_destination_dispatch aDispatch)
	{
		SetupDestinationDispatch = std::move(aDispatch);
	}

	void workbench_window::present_setup_destination(const application::setup_destination_projection& aProjection)
	{
		if (RepositoryPreparing) {
			SetupDestinationRequest = aProjection.Pending ? std::optional{ aProjection.Pending->Request } : std::nullopt;
			if (aProjection.Pending && (aProjection.Pending->Stage == core::setup_destination_stage::add_server || aProjection.Pending->Stage == core::setup_destination_stage::sign_in)) {
				open_sign_in(aProjection.Pending->Destination.Hostname);
				if (aProjection.Pending->Stage == core::setup_destination_stage::add_server) {
					open_enterprise_server_draft();
					EnterpriseServerHostname->setText(QString::fromStdString(aProjection.Pending->Destination.Hostname));
					update_enterprise_server_actions();
				}
			} else if (!aProjection.Pending) {
				open_repository_addition();
				repository_addition_notice(aProjection.RepositoryPrepared
											   ? tr("Check the destination, then choose Add to save.")
											   : tr("Setup cancelled or unavailable. This draft has not been saved."));
			}
			return;
		}
		ApplyingSetupProjection   = true;
		SetupDestinationRequest   = aProjection.Pending ? std::optional{ aProjection.Pending->Request } : std::nullopt;
		SetupDestinationNeedsHome = aProjection.Pending
			&& aProjection.Pending->Stage != core::setup_destination_stage::opening
			&& aProjection.Pending->Stage != core::setup_destination_stage::public_preview;
		if (aProjection.Opened) {
			// [[BPR-0987]]. A link to the pull request already shown opens
			// nothing new, so the window keeps its view rather than returning
			// to Overview when that review next refreshes.
			OpenOnOverview = !aProjection.Navigated;
			HomeOpenBar->clear();
		}
		HomeSetupNotice = QString::fromStdString(aProjection.Notice);
		update_home_open_notice();
		if (aProjection.Opened && !aProjection.Notice.empty()) {
			statusBar()->showMessage(QString::fromStdString(aProjection.Notice));
		} else {
			statusBar()->clearMessage();
		}
		// [[BPR-0732]]. A public-preview stage needs no press: the host is
		// already opening it, so only Cancel is offered.
		const auto actionable = aProjection.Pending && (aProjection.Pending->Stage == core::setup_destination_stage::add_server || aProjection.Pending->Stage == core::setup_destination_stage::sign_in);
		SetupContinueButton->hide();
		if (actionable) {
			open_sign_in(aProjection.Pending->Destination.Hostname);
			if (aProjection.Pending->Stage == core::setup_destination_stage::add_server) {
				open_enterprise_server_draft();
				EnterpriseServerHostname->setText(QString::fromStdString(aProjection.Pending->Destination.Hostname));
				update_enterprise_server_actions();
			}
		}
		SetupContinueButton->setText(aProjection.Pending && aProjection.Pending->Stage == core::setup_destination_stage::add_server ? tr("Add server and continue") : tr("Sign in and continue"));
		SetupCancelButton->setVisible(aProjection.Pending && aProjection.Pending->Stage != core::setup_destination_stage::opening && (!SignInDialog || !SignInDialog->isVisible()));
		update_home_landing();
		ApplyingSetupProjection = false;
	}

	void workbench_window::present_link_location_notice(const QString& aText)
	{
		HomeSetupNotice = aText;
		update_home_open_notice();
		if (aText.isEmpty()) {
			statusBar()->clearMessage();
		} else {
			statusBar()->showMessage(aText);
		}
	}

	auto workbench_window::reveal_conversation_in_review(const std::string& aIdentity) -> bool
	{
		if (Finder == nullptr) {
			return false;
		}
		const auto location = Finder->locate(aIdentity);
		return location.has_value() && open_conversation_in_review(location->Thread, location->Post);
	}

	auto workbench_window::open_conversation_in_review(const std::string& aThread, const std::string& aPost, const bool aFocus) -> bool
	{
		if (Finder == nullptr || ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return false;
		}
		auto location = Finder->locate(aPost.empty() ? aThread : aPost);
		if (!location.has_value() || location->Thread != aThread) {
			location = Finder->locate(aThread);
		}
		if (!location.has_value()) {
			return false;
		}
		const auto route = core::route_workbench_thread(core::open_workbench_thread_intent{ .OnFile = !location->Path.isEmpty() });
		if (!route.Available) {
			return false;
		}
		// [[BPR-1293]]. The pick is Review's: the file through the files
		// drawer's intent, Review in the arrangement it last had, Comments
		// opened if it is closed (a manual open, as a gutter mark's press
		// is), and the thread in place once the file's comparison shows it.
		PendingPick = conversation_pick{
			.Thread = location->Thread,
			.Post   = location->Post,
			.File   = core::file_identity_of(location->Path.toStdString()),
			.Before = ComparisonSurface != nullptr && ComparisonSurface->current_result() != nullptr
				? std::optional{ ComparisonSurface->current_result()->Alignment.Digest }
				: std::nullopt,
			.Focus  = aFocus,
		};
		choose_home(false);
		// Review first, then the file: a file gesture that follows the
		// modality wins over the active tab's restoration.
		choose_modality(review_modality::review);
		if (const auto file = file_for_path(location->Path); file.has_value() && Dispatch) {
			Dispatch(core::select_workbench_file_intent{ .File = *file });
		}
		if (route.OpenComments && Dispatch && Drawers[1].Panel->state() != core::workbench_drawer_state::expanded) {
			Dispatch(core::set_workbench_drawer_intent{
				.Drawer = core::workbench_drawer::conversations,
				.State  = core::workbench_drawer_state::expanded,
			});
		}
		if (TrayTabs != nullptr && TrayTabs->currentIndex() != tray_comments_tab) {
			TrayTabs->setCurrentIndex(tray_comments_tab);
		}
		resume_conversation_pick();
		resume_pending_comment_pick();
		return true;
	}

	auto workbench_window::open_thread_from_threads(const std::string& aThread, const std::string& aPost, const core::workbench_thread_destination aDestination) -> bool
	{
		if (Finder == nullptr) {
			return false;
		}
		const auto& conversations = Finder->conversations();
		if (std::ranges::find(conversations, aThread, &finder_conversation::Id) == conversations.end()) {
			return false;
		}
		return move_thread(aPost.empty() ? aThread : aPost, core::workbench_thread_origin::threads, aDestination);
	}

	auto workbench_window::move_thread(const std::string& aIdentity, const core::workbench_thread_origin aOrigin, const core::workbench_thread_destination aDestination) -> bool
	{
		if (Finder == nullptr || ConversationPanel == nullptr || ConversationPanel->cards() == nullptr || aIdentity.empty()) {
			return false;
		}
		if (ThreadsRevealNotice) {
			ThreadsRevealNotice = false;
			ConversationPanel->present_notice(QString{ });
		}
		const auto say = [this](const QString& aText) {
			statusBar()->showMessage(aText);
			QAccessibleAnnouncementEvent event{ this, aText };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		};
		// [[BPR-1581]]. My draft, written or pending, by its note.
		const auto draft = aIdentity.starts_with("draft:unpublished");
		// The window observes; the core decides. A thread on a file is the
		// finder's; one that is not is the conversation model's alone.
		auto thread   = aIdentity;
		auto post     = std::string{ };
		auto onFile   = draft;
		auto resolved = false;
		auto unread   = false;
		auto path     = QString{ };
		if (!draft) {
			if (const auto location = Finder->locate(aIdentity); location.has_value()) {
				thread                    = location->Thread;
				post                      = location->Post;
				onFile                    = !location->Path.isEmpty();
				path                      = location->Path;
				const auto& conversations = Finder->conversations();
				if (const auto found = std::ranges::find(conversations, thread, &finder_conversation::Id); found != conversations.end()) {
					resolved = found->Resolved;
					unread   = found->Unread > 0U;
				}
			} else {
				auto* const model = ConversationPanel->model();
				auto        index = model->index_for_id(aIdentity);
				if (!index.isValid()) {
					return false;
				}
				while (index.parent().isValid()) {
					index = index.parent();
				}
				thread   = model->id_for_index(index);
				post     = aIdentity;
				resolved = index.data(github_conversation_model::resolved_role).toBool();
			}
		}
		auto placed = true;
		if (const auto root = ConversationPanel->model()->index_for_id(thread); root.isValid() && !draft) {
			using availability = application::github_conversation_anchor_availability;
			const auto anchor  = static_cast<availability>(root.data(github_conversation_model::anchor_availability_role).toInt());
			placed             = !root.data(github_conversation_model::outdated_role).toBool() && (anchor == availability::current || anchor == availability::file_level);
		}
		const auto view  = ConversationPanel->discuss_view();
		const auto route = core::route_workbench_thread(core::open_workbench_thread_intent{
			.Destination = aDestination,
			.Origin      = aOrigin,
			.Draft       = draft || thread == "draft:unpublished",
			.OnFile      = onFile,
			.Placed      = placed,
			.Hiding      = core::workbench_discuss_filters{
				.OtherFile    = view.Scope == cards_scope::this_file && (!onFile || (view.File.has_value() && *view.File != core::file_identity_of(path.toStdString()))),
				.OnlyNew      = view.OnlyNew && !unread,
				.HideResolved = view.HideResolved && resolved,
				.Listing      = view.Listing.has_value() && !view.Listing->contains(thread),
			},
		});
		if (!route.Available) {
			say(aDestination == core::workbench_thread_destination::threads ? tr("This comment is not on a file, so Threads has no row for it. Discuss shows it.")
																			: tr("This comment is not on a file, so Review has no place for it. Discuss shows it."));
			return false;
		}
		if (route.Modality == core::workbench_modality::threads) {
			return show_thread_in_threads(draft ? std::string{ "draft:unpublished" } : thread, draft ? std::string{ } : post);
		}
		if (route.Composer) {
			const auto shown = show_draft_in_discuss(aIdentity);
			if (shown) {
				say(tr("Discuss lists no drafts, so your draft opens in its composer."));
			}
			return shown;
		}
		if (route.Modality == core::workbench_modality::discuss) {
			return open_conversation_in_discuss(thread, post, route.Clear);
		}
		const auto opened = open_conversation_in_review(thread, post, aOrigin != core::workbench_thread_origin::threads);
		if (opened && route.Reason == core::workbench_thread_route_reason::draft_opens_in_review) {
			say(tr("Your draft is written beside its line, so it opens in Review."));
		} else if (opened && route.Reason == core::workbench_thread_route_reason::opens_without_a_line) {
			say(tr("This thread has no current line, so it opens among %1's threads without one.").arg(path.section(QLatin1Char{ '/' }, -1)));
		}
		return opened;
	}

	auto workbench_window::show_draft_in_discuss(const std::string& aCard) -> bool
	{
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr || ReviewEditor == nullptr) {
			return false;
		}
		auto* const cards = ConversationPanel->cards();
		if (cards->writing_draft()) {
			maximize_drawer_draft();
			return true;
		}
		// Another line's draft is brought to its line first, as opening its
		// note does, so the composer holds it.
		if (aCard != "draft:unpublished") {
			const auto& all  = cards->cards();
			const auto  card = std::ranges::find(all, aCard, &thread_card::Id);
			if (card == all.end() || !card->Side.has_value() || card->Line.value_or(0U) == 0U || ComparisonSurface == nullptr) {
				return false;
			}
			static_cast<void>(ComparisonSurface->select_conversation_line(*card->Side, *card->Line - 1U));
		}
		if (!ReviewEditor->shown_draft().has_value()) {
			return false;
		}
		if (begin_new_comment(false) && cards->writing_draft()) {
			maximize_drawer_draft();
		}
		return CompositionPage == ComposeInlinePage;
	}

	auto workbench_window::thread_card_on_line(const core::diff_side aSide, const std::uint32_t aLine) const -> std::optional<std::string>
	{
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return std::nullopt;
		}
		const auto& cards = ConversationPanel->cards()->cards();
		const auto  card  = std::ranges::find_if(cards, [aSide, aLine](const thread_card& aCard) {
			return aCard.Side == aSide && aCard.Line == aLine + 1U && !aCard.Outdated;
		});
		return card == cards.end() ? std::nullopt : std::optional{ card->Id };
	}

	auto workbench_window::show_selected_thread(const core::workbench_thread_destination aDestination) -> bool
	{
		if (ConversationPanel == nullptr || ConversationPanel->cards() == nullptr || Modality == nullptr) {
			return false;
		}
		// [[BPR-1586]]. Threads, like Discuss, is shown from Review.
		const auto toDiscuss = aDestination != core::workbench_thread_destination::review;
		const auto here      = Modality->current();
		if ((toDiscuss && here != review_modality::review) || (!toDiscuss && here != review_modality::discuss) || CompositionPage != nullptr) {
			return false;
		}
		auto* const cards     = ConversationPanel->cards();
		auto        selection = core::workbench_thread_selection{
			.OpenThread = cards->open_thread_id(),
			.PickedPost = cards->picked_post(),
		};
		if (const auto current = ConversationPanel->model()->id_for_index(ConversationPanel->view()->currentIndex()); !current.empty()) {
			auto index = ConversationPanel->model()->index_for_id(current);
			while (index.parent().isValid()) {
				index = index.parent();
			}
			selection.CurrentThread = ConversationPanel->model()->id_for_index(index);
			selection.CurrentPost   = current == selection.CurrentThread ? std::string{ } : current;
		}
		if (toDiscuss && ComparisonSurface != nullptr && ComparisonSurface->selection().has_value()) {
			const auto& line = ComparisonSurface->selection()->PreferredLine;
			if (const auto onLine = thread_card_on_line(line.Side, line.Line); onLine.has_value()) {
				selection.LineThread = *onLine;
			}
			const auto* const focus = QApplication::focusWidget();
			selection.CodeFocused   = focus != nullptr && (focus == ComparisonSurface->left_pane() || focus == ComparisonSurface->right_pane());
		}
		const auto target = core::choose_workbench_thread_target(selection);
		if (!target.has_value()) {
			statusBar()->showMessage(tr("Select a thread first."));
			return false;
		}
		const auto origin = toDiscuss ? core::workbench_thread_origin::review : core::workbench_thread_origin::discuss;
		return move_thread(target->Post.empty() ? target->Thread : target->Post, origin, aDestination);
	}

	auto workbench_window::show_thread_in_discuss_action() const noexcept -> QAction*
	{
		return ShowInDiscussAction;
	}

	auto workbench_window::show_thread_in_review_action() const noexcept -> QAction*
	{
		return ShowInReviewAction;
	}

	auto workbench_window::show_thread_in_threads_action() const noexcept -> QAction*
	{
		return ShowInThreadsAction;
	}

	auto workbench_window::show_thread_in_threads(const std::string& aThread, const std::string& aPost) -> bool
	{
		if (Finder == nullptr || Modality == nullptr) {
			return false;
		}
		choose_home(false);
		choose_modality(review_modality::threads);
		auto cleared = false;
		if (!Finder->reveal(aThread, aPost, &cleared)) {
			statusBar()->showMessage(tr("Threads has no row for this thread."));
			return false;
		}
		if (cleared) {
			statusBar()->showMessage(tr("To show this thread, Threads cleared its search and filters."));
		}
		return true;
	}

	void workbench_window::update_show_thread_actions()
	{
		const auto here = Modality != nullptr ? std::optional{ Modality->current() } : std::nullopt;
		// Ctrl+Alt is AltGr on Windows, where AltGr with 2 or 3 types a
		// character: the keys step aside while the keyboard is typing.
		const auto reading = HasGithubPullRequest && CompositionPage == nullptr && !showing_home() && !TypingFocus;
		if (ShowInDiscussAction != nullptr) {
			ShowInDiscussAction->setEnabled(reading && here == review_modality::review);
		}
		if (ShowInReviewAction != nullptr) {
			ShowInReviewAction->setEnabled(reading && here == review_modality::discuss);
		}
		if (ShowInThreadsAction != nullptr) {
			ShowInThreadsAction->setEnabled(reading && here == review_modality::review);
		}
	}

	auto workbench_window::open_conversation_in_discuss(const std::string& aThread, const std::string& aPost, const core::workbench_discuss_filters& aClear) -> bool
	{
		choose_home(false);
		choose_modality(review_modality::discuss);
		ConversationPanel->clear_discuss_filters(aClear.OtherFile, aClear.OnlyNew, aClear.HideResolved, aClear.Listing);
		auto* const cards = ConversationPanel->cards();
		if (std::ranges::none_of(cards->cards(), [&aThread](const thread_card& aCard) { return aCard.Id == aThread; })) {
			return false;
		}
		// As a Review pick: the thread opens in place with its post washed
		// amber and brought into view, and the selection names the post.
		// [[BPR-1581]]. A thread named by its root opens at its first post,
		// and the post's arrival is marked.
		auto post = aPost;
		if (post.empty()) {
			const auto& all  = cards->cards();
			const auto  card = std::ranges::find(all, aThread, &thread_card::Id);
			post             = card != all.end() && !card->Posts.empty() ? card->Posts.front().Id : aThread;
		}
		cards->open_thread_at(aThread, post);
		if (const auto index = ConversationPanel->model()->index_for_id(post); index.isValid()) {
			ConversationPanel->choose(index);
		}
		cards->mark_arrival();
		cards->setFocus(Qt::OtherFocusReason);
		if (aClear.any()) {
			auto cleared = QStringList{ };
			if (aClear.OtherFile) {
				cleared.push_back(tr("File scope"));
			}
			if (aClear.OnlyNew) {
				cleared.push_back(tr("Only new"));
			}
			if (aClear.HideResolved) {
				cleared.push_back(tr("Hide resolved"));
			}
			if (aClear.Listing) {
				cleared.push_back(tr("the Overview's list"));
			}
			const auto last  = cleared.takeLast();
			const auto names = cleared.isEmpty() ? last : tr("%1 and %2").arg(cleared.join(QStringLiteral(", ")), last);
			const auto text  = tr("To show this thread, Discuss turned off %1.").arg(names);
			ConversationPanel->present_notice(text);
			ThreadsRevealNotice = true;
			QAccessibleAnnouncementEvent event{ ConversationPanel, text };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		}
		return true;
	}

	void workbench_window::resume_conversation_pick()
	{
		if (!PendingPick.has_value() || ConversationPanel == nullptr || ConversationPanel->cards() == nullptr
			|| (Modality != nullptr && Modality->current() != review_modality::review)) {
			return;
		}
		auto* const cards   = ConversationPanel->cards();
		const auto  context = ComparisonSurface != nullptr ? ComparisonSurface->current_context() : std::nullopt;
		const auto  digest  = ComparisonSurface != nullptr && ComparisonSurface->current_result() != nullptr
			? std::optional{ ComparisonSurface->current_result()->Alignment.Digest }
			: std::nullopt;
		// The thread opens once its file's comparison is the one shown, so
		// the drawer's notes are that file's for good.
		const auto shown = context.has_value() && context->Key.File == PendingPick->File && digest.has_value();
		if (shown && std::ranges::any_of(cards->cards(), [this](const thread_card& aCard) { return aCard.Id == PendingPick->Thread; })) {
			const auto pick = *PendingPick;
			PendingPick.reset();
			cards->open_thread_at(pick.Thread, pick.Post);
			// The selection authority names the picked post itself, as a
			// revealed comment always has; its thread stays the one open.
			if (const auto post = ConversationPanel->model()->index_for_id(pick.Post); post.isValid()) {
				// [[BPR-1698]]. The pick reveals its line itself, once.
				// Choosing moves the code only through the view's current
				// change, and a long file's comparison is adopted turns after
				// it is published, from the surface's selection observer,
				// whose guard drops that change: the file opened where its
				// own selection put it, not at the thread's line. An
				// unchanged current row sends no change either.
				const auto applying = std::exchange(ApplyingConversationAnchor, true);
				ConversationPanel->choose(post);
				ApplyingConversationAnchor = applying;
				synchronize_selected_conversation(post);
			}
			// [[BPR-1581]]. The post's arrival is marked, as in Discuss, and a
			// move from Discuss leaves the keyboard on the notes.
			cards->mark_arrival();
			if (pick.Focus) {
				cards->setFocus(Qt::OtherFocusReason);
			}
			return;
		}
		// Another file's comparison arrived first: the reviewer went on.
		if (context.has_value() && context->Key.File != PendingPick->File && digest.has_value() && digest != PendingPick->Before) {
			PendingPick.reset();
		}
	}

	void workbench_window::toggle_threads()
	{
		if (Modality == nullptr || !HasGithubPullRequest) {
			return;
		}
		if (Modality->current() == review_modality::threads && CompositionPage == nullptr && !showing_home()) {
			leave_threads();
			return;
		}
		choose_home(false);
		choose_modality(review_modality::threads);
	}

	void workbench_window::leave_threads()
	{
		if (Modality == nullptr || Modality->current() != review_modality::threads) {
			return;
		}
		const auto back  = ThreadsReturn.value_or(review_modality::review);
		const auto focus = ThreadsReturnFocus;
		choose_modality(back == review_modality::threads ? review_modality::review : back);
		if (focus && focus->isVisible()) {
			focus->setFocus(Qt::OtherFocusReason);
		}
	}

	void workbench_window::present_finder_reading()
	{
		if (Finder == nullptr || ConversationPanel == nullptr || ConversationPanel->cards() == nullptr) {
			return;
		}
		auto* const cards = ConversationPanel->cards();
		Finder->set_colours(cards->accent(), cards->marker());
		const auto context = ComparisonSurface != nullptr ? ComparisonSurface->current_context() : std::nullopt;
		Finder->set_reading(finder_reading{
			.File   = context.has_value() ? std::optional{ context->Key.File } : std::nullopt,
			.Thread = cards->open_thread_id(),
			.Post   = cards->picked_post(),
		});
	}

	auto workbench_window::finder() const noexcept -> conversation_finder*
	{
		return Finder;
	}

	auto workbench_window::show_threads_action() const noexcept -> QAction*
	{
		return ShowThreadsAction;
	}

	void workbench_window::set_conversation_labels(std::optional<application::conversation_labels_projection> aLabels)
	{
		if (Finder != nullptr) {
			Finder->set_labels(std::move(aLabels));
		}
	}

	auto workbench_window::reveal_conversation(const std::string& aIdentity) -> bool
	{
		if (ConversationPanel == nullptr || ConversationPanel->model() == nullptr || Modality == nullptr) {
			return false;
		}
		const auto previous = Modality->current();
		choose_home(false);
		choose_modality(review_modality::discuss);
		auto index = ConversationPanel->model()->index_for_id(aIdentity);
		if (!index.isValid()) {
			// Discuss's file scope or filters may hide it; the link asked for
			// this conversation, so they widen as Clear filters would.
			ConversationPanel->scope_whole_pull_request_button()->setChecked(true);
			ConversationPanel->filter_unread_button()->setChecked(false);
			ConversationPanel->filter_unresolved_button()->setChecked(false);
			index = ConversationPanel->model()->index_for_id(aIdentity);
		}
		if (!index.isValid()) {
			choose_modality(previous);
			return false;
		}
		// The view's current row is what reveals a conversation, as a press on
		// it does; the cards follow it.
		ConversationPanel->choose(index);
		ConversationPanel->view()->scrollTo(index);
		return true;
	}

	void workbench_window::open_from_home_bar()
	{
		if (SessionPanel != nullptr && SessionPanel->status() == core::github_status::establishing_session) {
			return;
		}
		if (HomeOpenBar && SetupDestinationDispatch) {
			if (SignInDialog && SignInDialog->isVisible()) {
				SignInDialog->reject();
			}
			SetupDestinationDispatch(application::submit_setup_destination{ HomeOpenBar->text().toStdString() });
		}
	}

	auto workbench_window::home_refusal_wording() const -> std::optional<core::pull_request_refusal_wording>
	{
		if (SessionPanel == nullptr || HasGithubPullRequest || !PullRequestRefusal
			|| SessionPanel->status() != core::github_status::pull_request_failed) {
			return std::nullopt;
		}
		// The core words it from the typed refusal and this window's connection
		// facts; the window only gathers those facts.
		const auto  session = SessionPanel->session();
		const auto* option  = session ? SessionPanel->profile_option(session->Profile) : nullptr;
		return core::word_pull_request_refusal(
			*PullRequestRefusal,
			core::pull_request_refusal_context{
				.Login            = session && !PublicPreviewSignedOut ? session->Login : std::string{ },
				.GithubCliOffered = static_cast<bool>(GithubCliDiscoveryDispatch),
				.ThroughGithubCli = option != nullptr
					&& option->Transport == application::github_profile_transport::github_cli,
			});
	}

	auto workbench_window::home_refusal_text() const -> QString
	{
		if (SessionPanel == nullptr || HasGithubPullRequest) {
			return { };
		}
		if (SessionPanel->status() == core::github_status::pull_request_failed) {
			if (const auto wording = home_refusal_wording(); wording.has_value()) {
				return QString::fromStdString(wording->Heading) + QLatin1Char{ '\n' }
				+ QString::fromStdString(wording->Explanation);
			}
			return SessionPanel->showing_diagnostic()
				? QString::fromStdString(SessionPanel->diagnostic_text())
				: QStringLiteral("The pull request could not be loaded.");
		}
		// A signed-out mode refusal is not a pull request the bar named; it is
		// said on the reading host's card instead.
		return { };
	}

	void workbench_window::take_refusal_action(const core::pull_request_refusal_action aAction)
	{
		// [[BPR-1200]]. Each step is one Home already offers elsewhere; the
		// refusal leads to it rather than performing anything of its own.
		switch (aAction) {
		case core::pull_request_refusal_action::sign_in:
			begin_selected_sign_in();
			return;
		case core::pull_request_refusal_action::use_github_cli:
			begin_github_cli_from_home();
			return;
		case core::pull_request_refusal_action::use_another_account:
			open_sign_in(SessionPanel != nullptr ? SessionPanel->active_host_text() : std::string{ });
			return;
		}
	}

	void workbench_window::update_home_open_notice()
	{
		if (HomeOpenNotice == nullptr || HomeOpenDetails == nullptr || SessionPanel == nullptr) {
			return;
		}
		if (SessionPanel->status() == core::github_status::loading_pull_request) {
			DismissedHomeRefusal.clear();
		}
		// [[BPR-1357]]. The opening the core and the application decided, when
		// no shown row says it: what the bar asked for, or a row Home no longer
		// shows. A row that says it is where the reviewer is looking.
		const auto& state       = HomeOpen.has_value() ? HomeOpen->State : std::optional<core::home_open_state>{ };
		const auto  inFlight    = state.has_value() && state->Phase != core::home_open_phase::failed;
		const auto  failed      = state.has_value() && state->Phase == core::home_open_phase::failed;
		const auto  bandOpening = inFlight && !HomeOpenOnRow;
		// A setup stage's own notice speaks first, then a review restoration's,
		// then the refusal of the last load, unless the reviewer has already
		// begun rewriting what was said.
		const auto refusal = home_refusal_text();
		const auto passive = !HomeRestorationNotice.isEmpty() ? HomeRestorationNotice : refusal;
		// [[BPR-1357]]. A failure a row already says is said there, not twice.
		const auto rowSaysFailure = failed && HomeOpenOnRow && HomeRestorationNotice.isEmpty();
		const auto spoken         = HomeSetupNotice.isEmpty() && !passive.isEmpty()
			&& passive != DismissedHomeRefusal && !rowSaysFailure;
		const auto refused = spoken && HomeRestorationNotice.isEmpty();
		// [[BPR-0775]]. A first load in progress is said here as well, where the
		// pull request was named, since the stack's page that said it is not
		// Home's. It is not a refusal, so it has no Copy details. [[BPR-1357]]:
		// with the application's opening it names the pull request and the step;
		// a row that says it leaves this line to the rest of Home's status.
		const auto loading = SessionPanel->status() == core::github_status::loading_pull_request && !HasGithubPullRequest
			? (bandOpening ? home_open_text(false)
						   : (HomeOpen.has_value() ? QString{ } : QString::fromStdString(SessionPanel->status_text())))
			: QString{ };
		const auto notice  = !HomeSetupNotice.isEmpty() ? HomeSetupNotice
			: spoken                                    ? passive
														: loading;
		// [[BPR-1200]]. A refusal the core has its own words for is its card;
		// every other notice keeps the line.
		const auto wording = refused ? home_refusal_wording() : std::nullopt;
		const auto carded  = wording.has_value() && HomeRefusal != nullptr;
		HomeOpenNotice->setText(carded ? QString{ } : notice);
		HomeRefusalDetails.clear();
		if (refused) {
			HomeRefusalDetails = QString::fromStdString(SessionPanel->status_text());
			if (HomeRefusalDetails != refusal) {
				HomeRefusalDetails += QLatin1Char('\n') + refusal;
			}
		}
		const auto said = !carded && !notice.isEmpty();
		HomeOpenDetails->setVisible(refused && !carded);
		if (HomeOpenCancel != nullptr && HomeOpenRetry != nullptr) {
			// Offered only where the core offers them, and only beside the line
			// that says the opening.
			const auto sayingOpening = said && notice == loading && bandOpening;
			HomeOpenCancel->setVisible(sayingOpening && HomeOpen->Cancellable);
			if (HomeOpenActivity != nullptr && HomeOpenShortcut != nullptr) {
				HomeOpenActivity->setVisible(sayingOpening);
				HomeOpenShortcut->setVisible(!sayingOpening);
			}
			HomeOpenRetry->setVisible(refused && failed && !HomeOpenOnRow && HomeOpen->Retryable);
		}
		if (HomeStatus != nullptr && said) {
			HomeStatus->set_tone(refused                                         ? home_status_tone::problem
									 : (!loading.isEmpty() && notice == loading) ? home_status_tone::busy
																				 : home_status_tone::notice);
		}
		HomeOpenNotice->setVisible(said);
		if (HomeRefusal != nullptr) {
			if (carded) {
				HomeRefusal->present(*wording);
			}
			HomeRefusal->setVisible(carded);
		}
		update_home_status();
	}

	auto workbench_window::home_status_echoes_bar() const -> bool
	{
		const auto message = statusBar()->currentMessage();
		return showing_home() && !message.isEmpty() && HomeStatus != nullptr && HomeStatus->isVisible()
			&& HomeStatusText != nullptr && HomeStatusText->isVisible() && HomeStatusText->text() == message;
	}

	auto workbench_window::home_status_line() const noexcept -> qt::home_status_line*
	{
		return HomeStatus;
	}

	auto workbench_window::home_status_text() const noexcept -> QLabel*
	{
		return HomeStatusText;
	}

	auto workbench_window::home_open_cancel_button() const noexcept -> QPushButton*
	{
		return HomeOpenCancel;
	}

	auto workbench_window::home_open_retry_button() const noexcept -> QPushButton*
	{
		return HomeOpenRetry;
	}

	auto workbench_window::home_open() const noexcept -> const std::optional<application::home_open_projection>&
	{
		return HomeOpen;
	}

	void workbench_window::set_home_open_dispatch(application::home_open_dispatch aDispatch)
	{
		HomeOpenDispatch = std::move(aDispatch);
	}

	void workbench_window::set_close_review_dispatch(std::function<void()> aDispatch)
	{
		CloseReviewDispatch = std::move(aDispatch);
	}

	void workbench_window::close_review()
	{
		if (HomeLanding == nullptr || HomeLanding->state() != landing_state::back_to_review || !CloseReviewDispatch) {
			return;
		}
		// The pull request's short name, as a link names it, not the strip's
		// identity with its title.
		const auto closing = IdentityOwner.isEmpty() || IdentityRepository.isEmpty() || IdentityNumber.isEmpty()
			? QString{ }
			: QStringLiteral("%1/%2#%3").arg(IdentityOwner, IdentityRepository, IdentityNumber);
		CloseReviewDispatch();
		if (HomeLanding->state() == landing_state::back_to_review) {
			return;
		}
		statusBar()->showMessage(
			closing.isEmpty() ? QStringLiteral("Closed the review.") : QStringLiteral("Closed the review of %1.").arg(closing), 8000);
		if (showing_home() && HomeOpenBar != nullptr) {
			HomeOpenBar->setFocus(Qt::OtherFocusReason);
		}
	}

	void workbench_window::set_home_failure_ink(const QColor& aInk)
	{
		if (HomeStatus != nullptr) {
			HomeStatus->set_failure_ink(aInk);
		}
		PendingFailureInk = aInk;
		apply_pending_appearance();
	}

	void workbench_window::set_pending_high_contrast(const bool aHigh)
	{
		PendingHighContrast = aHigh;
		apply_pending_appearance();
	}

	auto workbench_window::pending_veils() const noexcept -> const pending_panes&
	{
		return PendingPanes;
	}

	auto workbench_window::pending_full_motion() const noexcept -> bool
	{
		// The system's Reduce Motion is honoured; the Dif motion choice is the
		// mascot's own and does not still the application's waits. A platform
		// with no such setting turns; one not yet observed, or whose
		// observation failed, stays still.
		return DifResolutionReady
			&& (DifResolution.Observation == core::motion_observation::full || DifResolution.Observation == core::motion_observation::unsupported);
	}

	void workbench_window::apply_pending_appearance()
	{
		// Every waiting pane in the window, its own and its surfaces', draws
		// alike: found rather than listed, so a surface added later follows.
		const auto full = pending_full_motion();
		for (auto* const widget : findChildren<QWidget*>()) {
			auto* const veil = dynamic_cast<pending_veil*>(widget);
			if (veil == nullptr) {
				continue;
			}
			veil->set_full_motion(full);
			veil->set_high_contrast(PendingHighContrast);
			if (PendingFailureInk.isValid()) {
				veil->set_failure_ink(PendingFailureInk);
			}
		}
		if (EpochSelectors != nullptr && EpochSelectors->readiness_mark() != nullptr) {
			EpochSelectors->readiness_mark()->set_full_motion(full);
		}
	}

	void workbench_window::build_pending_veils()
	{
		// [[BPR-1576]]. The review's content, while a refresh replaces it: one
		// veil and one rule across the drawers and the centre, under the bar.
		PendingPanes.Content = new pending_veil{ *ReviewSurface, pending_shape::block, QStringLiteral("Review"), CentralSplitter };
		PendingPanes.Content->setObjectName(QStringLiteral("reviewPendingVeil"));
		PendingPanes.Content->set_announces(false);
		const auto pane = [](QWidget* const aTarget, const QString& aName, const QString& aObject) -> pending_veil* {
			if (aTarget == nullptr) {
				return nullptr;
			}
			auto* const veil = new pending_veil{ *aTarget, pending_shape::rows, aName };
			veil->setObjectName(aObject);
			// How many rows are coming is not known yet: a short list says
			// "a list" without promising a long one.
			veil->set_placeholder_count(8);
			// The Overview says when the pull request is ready; the drawers
			// beside it do not say it again.
			veil->set_announces(false);
			return veil;
		};
		PendingPanes.Files         = pane(FilesTree, QStringLiteral("Files"), QStringLiteral("filesPendingVeil"));
		PendingPanes.Conversations = pane(ConversationPanel, QStringLiteral("Comments"), QStringLiteral("commentsPendingVeil"));
		PendingPanes.Threads       = pane(Finder, QStringLiteral("Threads"), QStringLiteral("threadsPendingVeil"));
		PendingPanes.Repositories  = pane(RepositoriesTree, QStringLiteral("Repositories"), QStringLiteral("repositoriesPendingVeil"));
		PendingPanes.HomeList      = pane(HomeList, QStringLiteral("My pull requests"), QStringLiteral("homeListPendingVeil"));
		if (PendingPanes.HomeList != nullptr) {
			// Home's list is the page: its own wait and failure are said.
			PendingPanes.HomeList->set_announces(true);
			PendingPanes.HomeList->set_retry([this] {
				if (QueueDiscoveryDispatch) {
					QueueDiscoveryDispatch(application::queue_discovery_intent::refresh);
				}
			});
		}
		apply_pending_appearance();
	}

	auto workbench_window::read_stage_words() const -> QString
	{
		if (SessionPanel == nullptr || SessionPanel->status() != core::github_status::loading_pull_request) {
			return { };
		}
		auto stage = QStringLiteral("the pull request's details");
		if (ReadProgress.has_value()) {
			switch (ReadProgress->Phase) {
			case application::github_read_phase::metadata:              break;
			case application::github_read_phase::files:                 stage = QStringLiteral("the changed files"); break;
			case application::github_read_phase::commits:               stage = QStringLiteral("the commits"); break;
			case application::github_read_phase::reviews:               stage = QStringLiteral("the reviews"); break;
			case application::github_read_phase::review_threads:        stage = QStringLiteral("the review threads"); break;
			case application::github_read_phase::comments:              stage = QStringLiteral("the comments"); break;
			case application::github_read_phase::checks:                stage = QStringLiteral("the checks"); break;
			case application::github_read_phase::related_pull_requests: stage = QStringLiteral("related pull requests"); break;
			case application::github_read_phase::update_history:        stage = QStringLiteral("the update history"); break;
			}
			if (ReadProgress->PagesReceived > 1U) {
				stage += QStringLiteral(", %1 pages so far").arg(ReadProgress->PagesReceived);
			}
		}
		return stage;
	}

	void workbench_window::update_pending_panes()
	{
		if (SessionPanel == nullptr) {
			return;
		}
		const auto reading    = SessionPanel->status() == core::github_status::loading_pull_request;
		const auto first      = reading && !HasGithubPullRequest && !HasCachedOverview;
		const auto refreshing = reading && HasGithubPullRequest;
		const auto stage      = read_stage_words();
		const auto waiting    = stage.isEmpty() ? QString{ } : QStringLiteral("Still waiting for GitHub: %1.").arg(stage);
		const auto apply      = [](pending_veil* const aVeil, const bool aPending, const bool aContent, const pending_words& aWords) {
			if (aVeil == nullptr) {
				return;
			}
			if (!aPending) {
				aVeil->present_ready();
				return;
			}
			aVeil->present(core::pending_activity::loading, aContent, false, aWords);
		};
		// The review's own words are its Refresh control's; the veil only
		// speaks once the read is slow.
		apply(PendingPanes.Content, refreshing, true, { .Waiting = waiting });
		apply(PendingPanes.Files, first, false, { .Loading = QStringLiteral("Loading the file list…"), .Waiting = waiting });
		apply(PendingPanes.Conversations, first, false, { .Loading = QStringLiteral("Loading comments…"), .Waiting = waiting });
		apply(PendingPanes.Threads, first, false, { .Loading = QStringLiteral("Loading threads…"), .Waiting = waiting });
		// Only a composed queue restores: without one nothing is on its way.
		const auto queued = static_cast<bool>(QueueDiscoveryDispatch);
		apply(PendingPanes.Repositories, queued && HomeAccount && QueueProjection.Status == application::queue_landing_status::restoring, false,
			  { .Loading = QStringLiteral("Loading saved repositories…") });
		if (PendingPanes.HomeList == nullptr) {
			return;
		}
		// Home's list: restoring, then personal discovery. Rows already listed
		// (the queue's or Recently viewed) stay readable while it runs.
		const auto& discovery    = QueueProjection.Discovery;
		const auto  rows         = (QueueLanding != nullptr && QueueLanding->rowCount() > 0) || !RecentReviews.Entries.empty();
		const auto  hasSaved     = std::ranges::any_of(QueueProjection.SavedRepositories, [this](const auto& aSaved) {
			return aSaved.Host == QueueProjection.Session.Host && aSaved.Account == QueueProjection.Session.Account;
		});
		const auto  restoring    = queued && HomeAccount && QueueProjection.Status == application::queue_landing_status::restoring;
		const auto  finding      = HomeAccount && hasSaved && discovery.State == application::queue_discovery_state::refreshing;
		const auto  repositories = discovery.Repositories == 1U ? QStringLiteral("1 saved repository") : QStringLiteral("%1 saved repositories").arg(discovery.Repositories);
		if (restoring) {
			PendingPanes.HomeList->present(core::pending_activity::loading, rows, false, { .Loading = QStringLiteral("Loading my pull requests…") });
		} else if (finding) {
			PendingPanes.HomeList->present(core::pending_activity::loading, rows, false,
										   { .Loading = QStringLiteral("Finding my pull requests in %1…").arg(repositories),
											 .Waiting = QStringLiteral("Still waiting for GitHub to search %1.").arg(repositories),
											 .Done    = QStringLiteral("My pull requests found.") });
		} else if (HomeAccount && hasSaved && discovery.State == application::queue_discovery_state::stale && !rows) {
			PendingPanes.HomeList->present(core::pending_activity::failed, false, false,
										   { .Failure = QStringLiteral("Couldn't find my pull requests. %1").arg(home_discovery_sentence(discovery)) });
		} else {
			PendingPanes.HomeList->present_ready();
		}
	}

	namespace {
		/// [[BPR-1575]]. What a refresh compares of one accepted read.
		[[nodiscard]] auto refresh_observation_of(const application::github_pull_request_projection& aPullRequest) -> core::pull_request_refresh_observation
		{
			auto observation = core::pull_request_refresh_observation{ .Head = aPullRequest.HeadOid, .Commits = aPullRequest.CommitCount };
			for (const auto& conversation : aPullRequest.Conversations) {
				if (conversation.Deleted) {
					continue;
				}
				switch (conversation.Kind) {
				case application::github_conversation_kind::review_comment:
				case application::github_conversation_kind::issue_comment:
					observation.Comments.push_back(conversation.Id);
					break;
				case application::github_conversation_kind::review:
					observation.Reviews.push_back(conversation.Id);
					break;
				case application::github_conversation_kind::review_thread:
					break;
				}
			}
			switch (aPullRequest.State) {
			case application::github_pull_request_state_projection::merged: observation.Status = core::refresh_status::merged; break;
			case application::github_pull_request_state_projection::closed: observation.Status = core::refresh_status::closed; break;
			case application::github_pull_request_state_projection::open:
				observation.Status = aPullRequest.Draft ? core::refresh_status::draft : core::refresh_status::open;
				break;
			}
			return observation;
		}

		[[nodiscard]] auto counted(const std::uint32_t aCount, const QString& aOne, const QString& aMany) -> QString
		{
			return aCount == 1U ? aOne : aMany.arg(aCount);
		}

		/// [[BPR-1575]]. "2 new comments and 1 new push", or empty for nothing.
		[[nodiscard]] auto refresh_change_words(const core::refresh_changes& aChanges) -> QString
		{
			auto parts = QStringList{ };
			if (aChanges.NewPullRequests != 0U) {
				parts.push_back(counted(aChanges.NewPullRequests, QStringLiteral("1 new pull request"), QStringLiteral("%1 new pull requests")));
			}
			if (aChanges.NewComments != 0U) {
				parts.push_back(counted(aChanges.NewComments, QStringLiteral("1 new comment"), QStringLiteral("%1 new comments")));
			}
			if (aChanges.NewReviews != 0U) {
				parts.push_back(counted(aChanges.NewReviews, QStringLiteral("1 new review"), QStringLiteral("%1 new reviews")));
			}
			if (aChanges.NewPush) {
				parts.push_back(aChanges.NewCommits == 0U ? QStringLiteral("1 new push")
														  : QStringLiteral("1 new push (%1)").arg(counted(aChanges.NewCommits, QStringLiteral("1 commit"), QStringLiteral("%1 commits"))));
			}
			if (aChanges.Status.has_value()) {
				switch (*aChanges.Status) {
				case core::refresh_status::merged: parts.push_back(QStringLiteral("now merged")); break;
				case core::refresh_status::closed: parts.push_back(QStringLiteral("now closed")); break;
				case core::refresh_status::draft:  parts.push_back(QStringLiteral("now a draft")); break;
				case core::refresh_status::open:   parts.push_back(QStringLiteral("now open for review")); break;
				}
			}
			if (parts.size() < 2) {
				return parts.join(QString{ });
			}
			const auto last = parts.takeLast();
			return parts.join(QStringLiteral(", ")) + QStringLiteral(" and ") + last;
		}

		/// [[BPR-1575]]. "just now", "2 min ago", "3 hr ago", "2 days ago".
		[[nodiscard]] auto refresh_age_words(const core::refresh_age aAge) -> QString
		{
			switch (aAge.Unit) {
			case core::refresh_age_unit::just_now: return QStringLiteral("just now");
			case core::refresh_age_unit::minutes:  return QStringLiteral("%1 min ago").arg(aAge.Count);
			case core::refresh_age_unit::hours:    return QStringLiteral("%1 hr ago").arg(aAge.Count);
			case core::refresh_age_unit::days:     return aAge.Count == 1U ? QStringLiteral("1 day ago") : QStringLiteral("%1 days ago").arg(aAge.Count);
			}
			return { };
		}

		[[nodiscard]] auto refresh_failure_words(const core::refresh_failure aFailure) -> QString
		{
			switch (aFailure) {
			case core::refresh_failure::offline:      return QStringLiteral("GitHub could not be reached");
			case core::refresh_failure::rate_limited: return QStringLiteral("GitHub's rate limit was reached");
			case core::refresh_failure::signed_out:   return QStringLiteral("the account needs to sign in again");
			case core::refresh_failure::other:        return QStringLiteral("GitHub's answer could not be used");
			}
			return { };
		}
	} // namespace

	void workbench_window::run_automatic_refresh(const std::function<void()>& aDispatch)
	{
		AutomaticRefreshNoted = true;
		if (aDispatch) {
			aDispatch();
		}
		AutomaticRefreshNoted = false;
	}

	auto workbench_window::pull_request_refresh() const noexcept -> const core::refresh_state&
	{
		return PullRefresh;
	}

	auto workbench_window::home_refresh() const noexcept -> const core::refresh_state&
	{
		return HomeRefresh;
	}

	auto workbench_window::refresh_note_widget() const noexcept -> refresh_note*
	{
		return RefreshNote;
	}

	auto workbench_window::refresh_freshness_label() const noexcept -> QLabel*
	{
		return RefreshFreshness;
	}

	auto workbench_window::refresh_clock() const -> std::int64_t
	{
		return HomeClock ? HomeClock().value_or(0) : 0;
	}

	auto workbench_window::refresh_freshness_text() const -> QString
	{
		const auto home = showing_home();
		if (!home && !HasGithubPullRequest) {
			return { };
		}
		if (home && !HomeAccount) {
			return { };
		}
		const auto& state = home ? HomeRefresh : PullRefresh;
		const auto  age   = state.UpdatedAt.has_value() ? refresh_age_words(core::refresh_age_at(*state.UpdatedAt, refresh_clock())) : QString{ };
		switch (state.Phase) {
		case core::refresh_phase::refreshing:
			return state.Trigger == core::refresh_trigger::reviewer ? QStringLiteral("Refreshing…") : QStringLiteral("Updating…");
		case core::refresh_phase::failed:
			return age.isEmpty() ? QStringLiteral("Not refreshed") : QStringLiteral("Not refreshed · updated %1").arg(age);
		case core::refresh_phase::updated:
		case core::refresh_phase::never:
			return age.isEmpty() ? QString{ } : QStringLiteral("Updated %1").arg(age);
		}
		return { };
	}

	auto workbench_window::refresh_result_text(const bool aHome, const core::refresh_state& aState) const -> QString
	{
		if (aState.Phase == core::refresh_phase::failed) {
			auto said = QStringLiteral("Couldn't refresh: %1.").arg(refresh_failure_words(aState.Failure));
			if (aState.Failure == core::refresh_failure::rate_limited && aState.RetryAfterSeconds.has_value()) {
				const auto minutes = (*aState.RetryAfterSeconds + 59U) / 60U;
				said += *aState.RetryAfterSeconds < 60U ? QStringLiteral(" Try again in %1 s.").arg(*aState.RetryAfterSeconds)
														: QStringLiteral(" Try again in %1 min.").arg(minutes);
			}
			if (aState.UpdatedAt.has_value()) {
				said += QStringLiteral(" Showing what was loaded %1.").arg(refresh_age_words(core::refresh_age_at(*aState.UpdatedAt, refresh_clock())));
			}
			return said;
		}
		const auto words = refresh_change_words(aState.Changes);
		const auto lead  = aState.Trigger == core::refresh_trigger::automatic ? QStringLiteral("Updated automatically")
			: aState.Trigger == core::refresh_trigger::elsewhere              ? QStringLiteral("Updated from another window")
																			  : QStringLiteral("Updated");
		if (words.isEmpty()) {
			return aHome ? QStringLiteral("Up to date: no new pull requests.") : QStringLiteral("Up to date: nothing new.");
		}
		return QStringLiteral("%1: %2.").arg(lead, words);
	}

	void workbench_window::observe_refresh_read(const core::pull_request_read_delta& aRead)
	{
		if (!aRead.Refresh) {
			return;
		}
		RefreshReadSeen = true;
		if (aRead.Stage == core::pull_request_read_stage::pending) {
			RefreshEnded.reset();
			RefreshBaseline = RefreshLatest;
			apply_refresh_event(false, core::refresh_started{ .Trigger = AutomaticRefreshNoted ? core::refresh_trigger::automatic : core::refresh_trigger::reviewer });
			return;
		}
		RefreshEnded = aRead;
	}

	void workbench_window::settle_pull_request_refresh(const application::github_presentation_update& aUpdate)
	{
		for (const auto& diagnostic : aUpdate.HostDiagnostics) {
			if (diagnostic.RetryAfterSeconds.has_value()) {
				RefreshRetryAfter = diagnostic.RetryAfterSeconds;
			}
		}
		if (RefreshEnded.has_value()) {
			const auto ended = *RefreshEnded;
			RefreshEnded.reset();
			switch (ended.Stage) {
			case core::pull_request_read_stage::loaded:
				apply_refresh_event(false, core::refresh_succeeded{ .Changes = core::compare_refresh(RefreshBaseline, RefreshLatest), .At = refresh_clock() });
				break;
			case core::pull_request_read_stage::failed:
				apply_refresh_event(false, core::refresh_failed{ .Failure = core::refresh_failure_of(ended.Reason), .RetryAfterSeconds = ended.Reason == core::github_reason_code::rate_limited ? RefreshRetryAfter : std::nullopt, .At = refresh_clock() });
				break;
			case core::pull_request_read_stage::cancelled:
			case core::pull_request_read_stage::pending:
				apply_refresh_event(false, core::refresh_cancelled{ });
				break;
			}
			RefreshRetryAfter.reset();
		} else if (ElsewhereChanges.has_value()) {
			// Another window of the profile read this pull request; its answer
			// was handed to this one.
			apply_refresh_event(false, core::refresh_started{ .Trigger = core::refresh_trigger::elsewhere });
			apply_refresh_event(false, core::refresh_succeeded{ .Changes = *ElsewhereChanges, .At = refresh_clock() });
		}
		ElsewhereChanges.reset();
		present_refresh();
	}

	void workbench_window::observe_home_refresh(const application::queue_discovery_status& aBefore, const application::queue_landing_projection& aAfter)
	{
		if (!aAfter.UpdatedItem) {
			HomeRefs.clear();
			HomeRefs.reserve(aAfter.Items.size());
			for (const auto& item : aAfter.Items) {
				HomeRefs.push_back(item.Ref);
			}
		}
		const auto& after = aAfter.Discovery;
		if (aBefore.Host != after.Host || aBefore.Account != after.Account) {
			HomeRefresh = core::refresh_state{ };
		}
		if (after.LastSuccess.has_value()) {
			HomeRefresh.UpdatedAt = after.LastSuccess;
		}
		using state           = application::queue_discovery_state;
		const auto refreshing = [](const state aState) { return aState == state::refreshing; };
		if (!refreshing(aBefore.State) && refreshing(after.State)) {
			HomeBaseline = HomeRefs;
			apply_refresh_event(true, core::refresh_started{ .Trigger = HomeRefreshAsked ? core::refresh_trigger::reviewer : core::refresh_trigger::automatic });
			HomeRefreshAsked = false;
		} else if (refreshing(aBefore.State) && !refreshing(after.State)) {
			switch (after.State) {
			case state::current:
			case state::partial:
				apply_refresh_event(true, core::refresh_succeeded{ .Changes = core::compare_refresh(HomeBaseline, HomeRefs), .At = after.LastSuccess.value_or(refresh_clock()) });
				break;
			case state::stale: {
				using problem = application::queue_discovery_problem;
				auto failure  = core::refresh_failure::other;
				switch (after.Problem) {
				case problem::rate_limited:
				case problem::secondary_rate_limited: failure = core::refresh_failure::rate_limited; break;
				case problem::offline:                failure = core::refresh_failure::offline; break;
				case problem::authentication:         failure = core::refresh_failure::signed_out; break;
				default:                              break;
				}
				apply_refresh_event(true, core::refresh_failed{ .Failure = failure, .RetryAfterSeconds = after.RetryAfterSeconds, .At = refresh_clock() });
				break;
			}
			case state::idle:
			case state::refreshing:
			case state::unavailable:
				apply_refresh_event(true, core::refresh_cancelled{ });
				break;
			}
		}
		present_refresh();
	}

	void workbench_window::apply_refresh_event(const bool aHome, const core::refresh_event& aEvent)
	{
		auto&      state   = aHome ? HomeRefresh : PullRefresh;
		const auto before  = state;
		state              = core::apply_refresh(before, aEvent);
		auto* const action = aHome ? QueueRefreshAction : GithubRefreshAction;
		auto* const button = PrimaryToolbar != nullptr && action != nullptr ? PrimaryToolbar->widgetForAction(action) : nullptr;
		auto*       timer  = aHome ? HomeSlowTimer : RefreshSlowTimer;
		if (state.Phase == core::refresh_phase::refreshing && before.Phase != core::refresh_phase::refreshing && timer != nullptr) {
			timer->start();
		}
		// The tick is the end of one read only: a new read or a failure takes it away.
		if (state.Phase != core::refresh_phase::updated) {
			(aHome ? HomeSettledCue : PullSettledCue) = false;
		}
		if (state.Phase == core::refresh_phase::updated && before.Phase == core::refresh_phase::refreshing) {
			(aHome ? HomeSettledCue : PullSettledCue) = true;
			if (auto* const cue = aHome ? HomeCueTimer : PullCueTimer; cue != nullptr) {
				cue->start();
			}
		}
		// The note speaks for the surface on screen only.
		const auto shown = aHome == showing_home();
		if (RefreshNote != nullptr && shown) {
			switch (state.Phase) {
			case core::refresh_phase::refreshing:
				if (state.Trigger == core::refresh_trigger::reviewer && (before.Phase != core::refresh_phase::refreshing || before.Trigger != core::refresh_trigger::reviewer)) {
					RefreshNote->present(refresh_note_tone::busy, aHome ? QStringLiteral("Finding my pull requests…") : QStringLiteral("Refreshing the pull request…"), button);
				}
				break;
			case core::refresh_phase::updated:
				if (before.Phase == core::refresh_phase::refreshing || state.SettledAt != before.SettledAt) {
					if (state.Trigger == core::refresh_trigger::reviewer || core::any(state.Changes)) {
						RefreshNote->present(refresh_note_tone::settled, refresh_result_text(aHome, state), button, 5'000);
					} else if (RefreshNote->tone() == refresh_note_tone::busy) {
						RefreshNote->dismiss();
					}
				}
				break;
			case core::refresh_phase::failed:
				if (state.SettledAt != before.SettledAt || before.Phase != core::refresh_phase::failed) {
					RefreshNote->present(refresh_note_tone::failed, refresh_result_text(aHome, state), button, 12'000);
				}
				break;
			case core::refresh_phase::never:
				if (RefreshNote->isVisible() && RefreshNote->tone() == refresh_note_tone::busy) {
					RefreshNote->dismiss();
				}
				break;
			}
		}
		auto said = QString{ };
		switch (core::refresh_announcement_between(before, state)) {
		case core::refresh_announcement::none:    break;
		case core::refresh_announcement::started: said = aHome ? QStringLiteral("Finding my pull requests.") : QStringLiteral("Refreshing the pull request."); break;
		case core::refresh_announcement::updated: said = refresh_result_text(aHome, state); break;
		case core::refresh_announcement::failed:  said = refresh_result_text(aHome, state) + QStringLiteral(" Retry is available."); break;
		}
		if (!said.isEmpty() && shown) {
			QAccessibleAnnouncementEvent event{ button != nullptr ? static_cast<QObject*>(button) : static_cast<QObject*>(this), said };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		}
		present_refresh();
	}

	void workbench_window::present_refresh()
	{
		if (PrimaryToolbar == nullptr || RefreshFrames == nullptr) {
			return;
		}
		const auto full    = pending_full_motion();
		const auto failure = PendingFailureInk.isValid() ? PendingFailureInk : palette().color(QPalette::WindowText);
		const auto present = [&](QAction* const aAction, const core::refresh_state& aState, const bool aCue, QTimer* const aSlow, const bool aHome) {
			if (aAction == nullptr) {
				return;
			}
			const auto busy = aState.Phase == core::refresh_phase::refreshing;
			const auto look = busy ? (full ? strip_refresh_look::turning : strip_refresh_look::waiting)
				: aCue             ? strip_refresh_look::settled
								   : strip_refresh_look::idle;
			aAction->setIcon(strip_refresh_icon(look, RefreshFrame, palette()));
			auto* const button = PrimaryToolbar->widgetForAction(aAction);
			if (button == nullptr) {
				return;
			}
			const auto alert = aState.Phase == core::refresh_phase::failed ? failure : QColor{ };
			if (button->property(strip_style::busy_property).toBool() != busy || button->property(strip_style::alert_property).value<QColor>() != alert) {
				button->setProperty(strip_style::busy_property, busy);
				button->setProperty(strip_style::alert_property, alert);
				button->update();
			}
			auto describe = QString{ };
			switch (aState.Phase) {
			case core::refresh_phase::refreshing:
				describe = aHome ? QStringLiteral("Finding my pull requests in saved repositories…") : QStringLiteral("Refreshing the pull request…");
				break;
			case core::refresh_phase::failed:
				describe = refresh_result_text(aHome, aState) + QStringLiteral(" Press to try again.");
				break;
			case core::refresh_phase::updated:
			case core::refresh_phase::never:   {
				const auto base = aHome ? QStringLiteral("Find my pull requests in saved repositories again; this never changes GitHub.")
										: QStringLiteral("Reload the open pull request from its host.");
				describe        = aState.UpdatedAt.has_value()
					? QStringLiteral("%1 Updated %2.").arg(base, refresh_age_words(core::refresh_age_at(*aState.UpdatedAt, refresh_clock())))
					: base;
				break;
			}
			}
			button->setAccessibleDescription(describe);
			// Past the slow mark the note says what the read still waits on.
			if (busy && aSlow != nullptr && !aSlow->isActive() && RefreshNote != nullptr && RefreshNote->isVisible()
				&& RefreshNote->tone() == refresh_note_tone::busy && RefreshNote->anchor() == button) {
				const auto stage = aHome ? QString{ } : read_stage_words();
				const auto said  = aHome ? QStringLiteral("Still waiting for GitHub to search %1.")
											   .arg(QueueProjection.Discovery.Repositories == 1U ? QStringLiteral("1 saved repository")
																								 : QStringLiteral("%1 saved repositories").arg(QueueProjection.Discovery.Repositories))
					: stage.isEmpty()    ? QStringLiteral("Still waiting for GitHub…")
										 : QStringLiteral("Still waiting for GitHub: %1…").arg(stage);
				if (RefreshNote->text() != said) {
					RefreshNote->present(refresh_note_tone::busy, said, button);
				}
			}
		};
		present(GithubRefreshAction, PullRefresh, PullSettledCue, RefreshSlowTimer, false);
		present(QueueRefreshAction, HomeRefresh, HomeSettledCue, HomeSlowTimer, true);
		const auto turning = full && (PullRefresh.Phase == core::refresh_phase::refreshing || HomeRefresh.Phase == core::refresh_phase::refreshing);
		if (turning && !RefreshFrames->isActive()) {
			RefreshFrames->start();
		} else if (!turning) {
			RefreshFrames->stop();
		}
		if (RefreshNote != nullptr) {
			RefreshNote->set_full_motion(full);
			RefreshNote->set_failure_ink(failure);
		}
		if (RefreshFreshness != nullptr) {
			const auto  words  = refresh_freshness_text();
			const auto& state  = showing_home() ? HomeRefresh : PullRefresh;
			const auto  failed = state.Phase == core::refresh_phase::failed && !words.isEmpty();
			const auto  text   = failed ? QStringLiteral("<span style=\"color:%1\">%2</span> · <a href=\"retry\">Retry</a>").arg(failure.name(), words.toHtmlEscaped())
										: words.toHtmlEscaped();
			if (RefreshFreshness->text() != text) {
				RefreshFreshness->setText(text);
			}
			// One width for every phrase it says, the widest, so neither it
			// nor the status line beside it moves as the words change.
			const auto widest = QFontMetrics{ RefreshFreshness->font() }.horizontalAdvance(QStringLiteral("Not refreshed · updated 59 min ago · Retry")) + 8;
			if (RefreshFreshness->width() != widest) {
				RefreshFreshness->setFixedWidth(widest);
			}
			RefreshFreshness->setToolTip(failed ? refresh_result_text(showing_home(), state) : words);
			RefreshFreshness->setAccessibleDescription(RefreshFreshness->toolTip());
			RefreshFreshness->setVisible(!words.isEmpty());
			// The words age on their own: woken exactly when they would change.
			if (state.UpdatedAt.has_value() && state.Phase != core::refresh_phase::refreshing && RefreshAgeTimer != nullptr) {
				const auto next = core::refresh_age_next_change_seconds(*state.UpdatedAt, refresh_clock());
				RefreshAgeTimer->start(static_cast<int>(std::clamp<std::int64_t>(next, 1, 86'400) * 1'000));
			}
		}
	}

	void workbench_window::present(const application::home_open_projection& aProjection)
	{
		HomeOpen = aProjection.State.has_value() ? std::optional{ aProjection } : std::nullopt;
		refresh_home_open();
	}

	void workbench_window::dispatch_home_open_action()
	{
		if (!HomeOpenDispatch || !HomeOpen.has_value() || !HomeOpen->State.has_value()) {
			return;
		}
		// The request the projection named; the application admits it against
		// the core's state, so a stale press asks for nothing.
		const auto request = HomeOpen->State->Request;
		if (HomeOpen->State->Phase == core::home_open_phase::failed) {
			if (HomeOpen->Retryable) {
				HomeOpenDispatch(application::retry_home_open{ request });
			}
			return;
		}
		if (HomeOpen->Cancellable) {
			HomeOpenDispatch(application::cancel_home_open{ request });
		}
	}

	auto workbench_window::home_open_text(const bool aRow) const -> QString
	{
		if (!HomeOpen.has_value() || !HomeOpen->State.has_value()) {
			return { };
		}
		const auto& state = *HomeOpen->State;
		const auto  name  = aRow ? QStringLiteral("#%1").arg(state.Number.value())
								 : QStringLiteral("%1/%2#%3")
									   .arg(QString::fromStdString(state.Repository.Owner), QString::fromStdString(state.Repository.Name))
									   .arg(state.Number.value());
		switch (state.Phase) {
		case core::home_open_phase::opening:
			return QStringLiteral("Opening %1…").arg(name);
		case core::home_open_phase::waiting: {
			// The step the read is on, from its own bounded progress.
			auto step = QStringLiteral("asking GitHub for the pull request");
			if (ReadProgress.has_value()) {
				switch (ReadProgress->Phase) {
				case application::github_read_phase::metadata:
					if (ReadProgress->PagesReceived != 0U) {
						step = QStringLiteral("reading the pull request");
					}
					break;
				case application::github_read_phase::files:                 step = QStringLiteral("reading changed files"); break;
				case application::github_read_phase::commits:               step = QStringLiteral("reading commits"); break;
				case application::github_read_phase::reviews:               step = QStringLiteral("reading reviews"); break;
				case application::github_read_phase::review_threads:        step = QStringLiteral("reading review threads"); break;
				case application::github_read_phase::comments:              step = QStringLiteral("reading comments"); break;
				case application::github_read_phase::checks:                step = QStringLiteral("reading checks"); break;
				case application::github_read_phase::related_pull_requests: step = QStringLiteral("reading related pull requests"); break;
				case application::github_read_phase::update_history:        step = QStringLiteral("reading update history"); break;
				}
			}
			return QStringLiteral("Opening %1: %2…").arg(name, step);
		}
		case core::home_open_phase::failed: {
			const auto reason = detail::reason_text(state.Reason, std::nullopt, std::nullopt);
			return reason.isEmpty() ? QStringLiteral("Couldn't open %1.").arg(name)
									: QStringLiteral("Couldn't open %1. %2").arg(name, reason);
		}
		}
		return { };
	}

	void workbench_window::refresh_home_open()
	{
		auto open = home_row_open{ };
		if (HomeOpen.has_value() && HomeOpen->State.has_value()) {
			const auto& state = *HomeOpen->State;
			open.Identity     = QStringLiteral("%1/%2#%3")
									.arg(QString::fromStdString(state.Repository.Owner), QString::fromStdString(state.Repository.Name))
									.arg(state.Number.value());
			open.Phase        = state.Phase == core::home_open_phase::opening ? home_row_open_phase::opening
				: state.Phase == core::home_open_phase::waiting               ? home_row_open_phase::waiting
																			  : home_row_open_phase::failed;
			open.Line         = home_open_text(true);
			open.Action       = HomeOpen->Cancellable ? tr("Cancel") : HomeOpen->Retryable ? tr("Retry")
																						   : QString{ };
		}
		if (HomeList != nullptr) {
			HomeList->set_open(open);
			HomeOpenOnRow = HomeList->open_view() != nullptr;
		} else {
			HomeOpenOnRow = false;
		}
		update_home_open_notice();
		// One polite announcement for each thing the opening says, wherever it
		// is shown; a reader hears the step and the failure as a sighted
		// reviewer reads them.
		const auto announcement = open.Phase == home_row_open_phase::none ? QString{ }
			: open.Phase == home_row_open_phase::waiting && HomeOpen->Cancellable
			? home_open_text(false) + QStringLiteral(" Cancel is available.")
			: home_open_text(false);
		if (!announcement.isEmpty() && announcement != LastHomeOpenAnnouncement && HomePage != nullptr) {
			LastHomeOpenAnnouncement = announcement;
			QAccessibleAnnouncementEvent event{ HomePage, announcement };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		}
		if (announcement.isEmpty()) {
			LastHomeOpenAnnouncement.clear();
		}
	}

	void workbench_window::update_home_status()
	{
		if (HomeStatusText == nullptr || HomeOpenNotice == nullptr || SessionPanel == nullptr) {
			return;
		}
		// [[BPR-1357]]. Home's general status, said in the band when the bar's
		// own line is not: a problem the window has, a connection under way, or
		// what was just done. The session line is idle and stays in the status
		// bar, so a quiet Home adds nothing.
		auto text = QString{ };
		auto tone = home_status_tone::notice;
		if (Failure) {
			text = failure_text(*Failure);
			tone = home_status_tone::problem;
		} else if (ProfileFailure) {
			text = profile_management_failure_text(*ProfileFailure);
			tone = home_status_tone::problem;
		} else if (AgentWorkspacesUnavailable) {
			text = QStringLiteral("Disposable agent workspaces are unavailable: their folder needs repair or has reached its limit.");
			tone = home_status_tone::problem;
		} else if (SavedLayoutUnavailable) {
			text = failure_text(application::workbench_host_failure::settings_unavailable);
			tone = home_status_tone::problem;
		} else if (SessionPanel->status() == core::github_status::establishing_session) {
			const auto host = QString::fromStdString(SessionPanel->active_host_text());
			text            = host.isEmpty() ? QStringLiteral("Connecting to GitHub…") : QStringLiteral("Connecting to %1…").arg(host);
			tone            = home_status_tone::busy;
		} else if (const auto message = statusBar() != nullptr ? statusBar()->currentMessage() : QString{ }; !message.isEmpty()) {
			text = message;
		}
		const auto shown = !text.isEmpty() && HomeOpenNotice->isHidden();
		HomeStatusText->setText(shown ? text : QString{ });
		if (shown && HomeStatus != nullptr) {
			HomeStatus->set_tone(tone);
		}
		HomeStatusText->setVisible(shown);
		statusBar()->update();
		const auto announcement = shown && showing_home() ? text : QString{ };
		if (!announcement.isEmpty() && announcement != LastHomeStatusAnnouncement && HomeStatus != nullptr) {
			LastHomeStatusAnnouncement = announcement;
			QAccessibleAnnouncementEvent event{ HomeStatus, announcement };
			event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
			QAccessible::updateAccessibility(&event);
		}
		if (announcement.isEmpty()) {
			LastHomeStatusAnnouncement.clear();
		}
	}

	auto workbench_window::select_queue_item(
		const core::pull_request_ref& aRef) -> bool
	{
		// [[BPR-0776]]. The row is found in whichever state's panel holds it,
		// which is expanded if the reviewer had collapsed it.
		if (HomeList != nullptr && HomeList->select(aRef, true)) {
			update_queue_status(QueueProjection);
			return true;
		}

		const auto present = [this, &aRef] {
			for (auto row = 0; row < QueueLanding->rowCount(); ++row) {
				const auto item = QueueLanding->item_at(QueueLanding->index(row, 0));
				if (item.has_value() && item->Ref == aRef) {
					return true;
				}
			}
			return false;
		}();
		QueueStatus->setText(
			present
				? tr("The notified review is hidden by the active queue filters.")
				: tr("The notified review is no longer in the local review queue."));
		QueueStatusNotable = true;
		update_home_surface();
		return false;
	}

	auto workbench_window::queue_role_filter() const noexcept -> QComboBox*
	{
		return QueueRoleFilter;
	}

	auto workbench_window::queue_observer_filter() const noexcept -> QComboBox*
	{
		return QueueObserverFilter;
	}

	auto workbench_window::queue_bucket_filter() const noexcept -> QComboBox*
	{
		return QueueBucketFilter;
	}

	auto workbench_window::queue_repository_filter() const noexcept -> QComboBox*
	{
		return QueueRepositoryFilter;
	}

	auto workbench_window::queue_quiet_filter() const noexcept -> QComboBox*
	{
		return QueueQuietFilter;
	}

	auto workbench_window::queue_sort_selector() const noexcept -> QComboBox*
	{
		return QueueSort;
	}

	auto workbench_window::queue_rule_kind_selector() const noexcept -> QComboBox*
	{
		return QueueRuleKind;
	}

	auto workbench_window::queue_rule_pattern_editor() const noexcept -> QLineEdit*
	{
		return QueueRulePattern;
	}

	auto workbench_window::queue_rule_exclusion() const noexcept -> QCheckBox*
	{
		return QueueRuleExcluding;
	}

	auto workbench_window::add_queue_rule_action() const noexcept -> QAction*
	{
		return AddQueueRule;
	}

	auto workbench_window::remove_queue_rule_action() const noexcept -> QAction*
	{
		return RemoveQueueRule;
	}

	auto workbench_window::queue_status_label() const noexcept -> QLabel*
	{
		return QueueStatus;
	}

	auto workbench_window::files_drawer_toggle_action() const noexcept -> QAction*
	{
		return Drawers[0].Toggle;
	}

	auto workbench_window::conversations_drawer_toggle_action() const noexcept
		-> QAction*
	{
		return Drawers[1].Toggle;
	}

	auto workbench_window::review_details_toggle_action() const noexcept -> QAction*
	{
		return ReviewDetailsAction;
	}

	auto workbench_window::epoch_selectors() const noexcept
		-> epoch_comparison_control*
	{
		return EpochSelectors;
	}

	auto workbench_window::focus_order() const -> QList<QWidget*>
	{
		auto       order  = QList<QWidget*>{ };
		const auto append = [&order](QWidget* const aWidget) {
			// A stop that cannot take focus is not an interactive stop. Skipping
			// it keeps the declared order equal to the order a keyboard user can
			// actually walk.
			if (aWidget != nullptr && aWidget->focusPolicy() != Qt::NoFocus) {
				order.push_back(aWidget);
			}
		};

		// The title row's stops in its reading order: Home, the drawer
		// toggles, the four modalities, Home's own controls, refresh, comments,
		// account, settings ([[BPR-1129]]).
		const auto presenter = [this](QAction* const aAction) {
			return qobject_cast<QToolButton*>(
				PrimaryToolbar->widgetForAction(aAction));
		};
#if defined(_WIN32)
		// [[BPR-0653]]. The compact menu control reads first, as it sits.
		append(StripMenuButton);
#endif
		append(presenter(HomeAction));
		append(presenter(Drawers[2].Toggle));
		append(presenter(Drawers[0].Toggle));
		if (Modality != nullptr) {
			for (std::size_t index = 0U; index < review_modality_count; ++index) {
				append(Modality->button(static_cast<review_modality>(index)));
			}
		}
		// [[BPR-0783]], [[BPR-1580]]. Home's View control, in the strip's
		// reading order, before the refresh glyph it stands with.
		append(HomeViewButton);
		append(presenter(GithubRefreshAction));
		append(presenter(QueueRefreshAction));
		append(presenter(Drawers[1].Toggle));
		append(presenter(AccountAction));
		append(presenter(SettingsAction));
#if defined(_WIN32)
		// [[BPR-0653]]. The caption buttons are the strip's last three stops.
		if (CaptionButtons != nullptr) {
			for (auto* const button : CaptionButtons->buttons()) {
				append(button);
			}
		}
#endif
		// [[BPR-1129]]. The review bar under the title row, in its reading
		// order: the comparison's menu, the change pair, the file pair, private
		// progress, Compose review and Comment.
		if (ReviewBar != nullptr && EpochSelectors != nullptr) {
			const auto barPresenter = [this](QAction* const aAction) {
				return qobject_cast<QToolButton*>(ReviewBar->widgetForAction(aAction));
			};
			append(UpdatesDisclosure);
			append(EpochSelectors->comparing_button());
			append(barPresenter(Navigation[0].Action));
			append(barPresenter(Navigation[1].Action));
			append(barPresenter(Navigation[2].Action));
			append(barPresenter(Navigation[3].Action));
			append(SeenStepButton);
			append(ReadStepButton);
			append(AcceptFileButton);
			append(ReviewControlsButton);
			append(barPresenter(ComposeReviewAction));
			append(barPresenter(CommentAction));
		}
		// The review header sits under the toolbars and above the workbench, so
		// its stops are declared there. Reading order and focus order agree.
		// The session chrome and the device sign-in are the Accounts and
		// hosts page of the Settings window, another top-level window with
		// its own tab chain; the workbench declares nothing across windows.
		// [[BPR-0733]] [[BPR-0776]]. Home's list: each shown panel's header, then
		// its rows or its sentence, in state order and Recently viewed last. A
		// panel that is not shown is no stop, and neither is a collapsed
		// panel's content.
		if (HomeList != nullptr) {
			for (auto* const stop : HomeList->focus_chain()) {
				append(stop);
			}
		}
		// [[BPR-0783]]. The six typed queue controls are Settings' Queue defaults
		// now, in another top-level window with its own tab chain, so the
		// workbench declares none of them — as it declares none of the rule
		// editor's ([[BPR-0652]]).
		// The files drawer's column, top to bottom: sign-in, the session
		// chrome, the queue, the pull request, then the files list.
		if (PullRequestPanel != nullptr) {
			append(PullRequestPanel->technical_details_button());
			append(PullRequestPanel->body_editor());
		}
		// A collapsed drawer's rail is its one stop; hidden while expanded.
		// [[BPR-0981]]. Each drawer's filter, its Clear filter (shown only in
		// the empty state) and, for files, the order selector precede its tree.
		append(Drawers[2].Panel->rail());
		append(RepositoriesFilter);
		if (RepositoriesFilterNotice != nullptr) {
			append(RepositoriesFilterNotice->clear_button());
		}
		append(Drawers[2].Panel->focus_stop());
		append(Drawers[0].Panel->rail());
		append(FilesOrderSelector);
		append(FilesFilter);
		if (FilesFilterNotice != nullptr) {
			append(FilesFilterNotice->clear_button());
		}
		append(Drawers[0].Panel->focus_stop());
		// The line first (its comparing menu is the review bar's now), then
		// the two selectors on one grid row above their two exact OIDs, the row
		// order the composed control already walks.
		if (HomeLanding != nullptr) {
			append(HomeOnboardingButton);
			append(HomeAddRepositoryButton);
			append(HomeLanding->primary_button());
			append(HomeLanding->close_button());
			append(HomeLanding->secondary_button());
			append(HomeLanding->tertiary_button());
		}
		append(EpochSelectors->line());
		append(EpochSelectors->left_selector());
		append(EpochSelectors->right_selector());
		append(EpochSelectors->left_oid_editor());
		append(EpochSelectors->right_oid_editor());
		// [[BPR-1132]]. The comparison's header: the tabs, the count of the
		// ones the width floor leaves out, and the new-tab button.
		if (DocumentTabs != nullptr) {
			append(DocumentTabs->tab_bar());
			append(DocumentTabs->new_tab_button());
		}
		// One stop stands for the whole composed comparison surface: its last.
		// Everything before it inside that surface is RFC-BPR-0004's declared
		// order, which the shell reports through that card rather than restating
		// here.
		append(ComparisonSurface->detail_pane());
		append(Drawers[1].Panel->rail());
		// [[BPR-1134]]. The tray's tabs, then the Comments tab's tool and its
		// conversation; the Notes tab's stops follow.
		append(TrayTabs);
		append(NextNewCommentButton);
		append(PreviousDefectButton);
		append(NextDefectButton);
		append(Drawers[1].Panel->focus_stop());
		if (PrivateNotes != nullptr) {
			// [[BPR-1592]]. The notes, then the open note's field.
			append(PrivateNotes);
			append(PrivateNotes->body_editor());
		}
		if (ReviewEditor != nullptr) {
			append(ReviewEditor->body_editor());
		}
		append(StatusLabel);
		return order;
	}

	auto workbench_window::comparison_surface() const noexcept -> diff_workbench*
	{
		return ComparisonSurface;
	}

	auto workbench_window::left_conversation_endpoint_current() const noexcept -> bool
	{
		return LeftConversationEndpointCurrent;
	}

	void workbench_window::set_conversation_endpoint_placement(
		const bool aLeftHasCurrentDiffRevision,
		const bool aRightHasCurrentDiffRevision)
	{
		LeftConversationEndpointCurrent  = aLeftHasCurrentDiffRevision;
		RightConversationEndpointCurrent = aRightHasCurrentDiffRevision;
		if (ConversationPanel == nullptr) {
			return;
		}
		ApplyingConversationAnchor = true;
		ConversationPanel->set_current_endpoint_placement(
			aLeftHasCurrentDiffRevision, aRightHasCurrentDiffRevision);
		ApplyingConversationAnchor = false;
		synchronize_selected_conversation(chosen_conversation());
		update_defect_surface();
		update_comment_target();
		update_older_pair_notice();
		// [[BPR-1046]]. The composition places the endpoints once a
		// comparison has arrived, which is when a waiting reply can select
		// its thread's line.
		resume_deferred_reply();
		resume_conversation_pick();
		resume_pending_comment_pick();
	}

	auto workbench_window::detail_pane() const noexcept -> QPlainTextEdit*
	{
		return ComparisonSurface->detail_pane();
	}

	auto workbench_window::status_text() const -> QString
	{
		return StatusLabel->text();
	}

	auto workbench_window::statistics() const noexcept
		-> workbench_window_statistics
	{
		return Statistics;
	}

	void workbench_window::resizeEvent(QResizeEvent* const aEvent)
	{
		QMainWindow::resizeEvent(aEvent);
		update_discovery_controls();
		apply_strip_width();
		update_dif_presentation();
		dispatch_geometry();
	}

	void workbench_window::moveEvent(QMoveEvent* const aEvent)
	{
		QMainWindow::moveEvent(aEvent);
		dispatch_geometry();
	}

	void workbench_window::set_geometry_dispatch(std::function<void(application::workbench_window_geometry)> aDispatch)
	{
		GeometryDispatch = std::move(aDispatch);
	}

	void workbench_window::restore_geometry(const application::workbench_window_geometry& aGeometry)
	{
		if (GeometryReady) {
			return;
		}
		ApplyingGeometry = true;
		if (application::valid(aGeometry) && aGeometry.Width != 0) {
			auto  rectangle = QRect{ aGeometry.X, aGeometry.Y, aGeometry.Width, aGeometry.Height };
			auto* target    = QGuiApplication::primaryScreen();
			int   overlap   = 0;
			for (auto* screen : QGuiApplication::screens()) {
				const auto visible = rectangle.intersected(screen->availableGeometry());
				const auto area    = visible.width() * visible.height();
				if (area > overlap) {
					target  = screen;
					overlap = area;
				}
			}
			if (target != nullptr) {
				const auto available = target->availableGeometry();
				// Keep the title/resize affordances reachable after a display is
				// removed. The shell's minimum size remains authoritative on tiny
				// displays; placement still keeps its upper edge visible.
				rectangle.setWidth(std::clamp(rectangle.width(), minimumWidth(), std::max(minimumWidth(), available.width())));
				rectangle.setHeight(std::clamp(rectangle.height(), minimumHeight(), std::max(minimumHeight(), available.height() - 32)));
				rectangle.moveLeft(std::clamp(rectangle.x(), available.left(), std::max(available.left(), available.right() - rectangle.width() + 1)));
				rectangle.moveTop(std::clamp(rectangle.y(), available.top() + 32, std::max(available.top() + 32, available.bottom() - rectangle.height() + 1)));
			}
			setGeometry(rectangle);
			if (aGeometry.Maximized) {
				setWindowState(windowState() | Qt::WindowMaximized);
			}
		}
		ApplyingGeometry = false;
		GeometryReady    = true;
		dispatch_geometry();
	}

	void workbench_window::dispatch_geometry()
	{
		if (!GeometryReady || ApplyingGeometry || !GeometryDispatch || !isVisible() || isMinimized() || isFullScreen()) {
			return;
		}
		const auto normal      = normalGeometry();
		const auto observation = application::workbench_window_geometry{ normal.x(), normal.y(), normal.width(), normal.height(), isMaximized() };
		if (application::valid(observation) && observation.Width != 0) {
			GeometryDispatch(observation);
		}
	}

	void workbench_window::showEvent(QShowEvent* const aEvent)
	{
		QMainWindow::showEvent(aEvent);
#if defined(_WIN32)
		// [[BPR-1022]]. The native window exists once shown, and a window that
		// is made again is shown again; installing twice installs once.
		if (auto* const handle = windowHandle(); handle != nullptr) {
			handle->installEventFilter(this);
		}
#endif
		// [[BPR-0732]]. The first showing decides where a signed-out Home
		// starts: at the bar.
		if (!HomeFocused) {
			update_home_landing();
		}
		// The native window exists once shown; the title text goes then.
		hide_native_title_text(*this);
		update_dif_presentation();
		dispatch_geometry();
	}

	void workbench_window::update_rail_summaries()
	{
		// The rails restate what the drawers hold: the selected file's place
		// among the actionable files, and how many review threads there are.
		// The unread count the design names waits for the projection that
		// carries it; nothing is invented meanwhile.
		auto files = QString{ };
		if (Cursor && Cursor->ActionableCount > 0U) {
			files = Cursor->Ordinal > 0U
				? QStringLiteral("%1 of %2")
					  .arg(Cursor->Ordinal)
					  .arg(Cursor->ActionableCount)
				: QString::number(Cursor->ActionableCount);
		}
		Drawers[0].Panel->rail()->set_summary(files);
		Drawers[1].Panel->rail()->set_summary(
			ReviewThreadTotal > 0U ? QString::number(ReviewThreadTotal)
								   : QString{ });
		// The repositories rail: how many pull requests need me.
		const auto needsMe = RepositoriesTree != nullptr
			? RepositoriesTree->tree_model()->needs_me_count()
			: std::size_t{ 0U };
		Drawers[2].Panel->rail()->set_summary(
			needsMe > 0U ? QString::number(needsMe) : QString{ });
	}

	void workbench_window::infer_modality_from_drawers()
	{
		// A restored layout arrives before any modality was chosen in this
		// window, so the segment shows the preset the drawers already are.
		// Once a modality has been chosen here, a manual toggle leaves the
		// segment alone, as the accepted design requires.
		if (Modality == nullptr || ModalityChosen) {
			return;
		}
		// [[BPR-0727]]. Overview is chosen, never inferred: its preset is every
		// drawer collapsed, which Review with its drawers hidden looks exactly
		// like, and inferring it would make the core refuse a toggle the strip
		// never promised to refuse. [[BPR-1295]]: with Read merged into Review,
		// every restored shape is Review's, including Read's old one (Files
		// and Repositories collapsed, Comments open), which keeps the room it
		// had. Discuss is never inferred either; a window already in it stays.
		// [[BPR-1293]]. Nor is Threads, which is never stored.
		if (Modality->current() != review_modality::discuss && Modality->current() != review_modality::threads) {
			Modality->adopt(review_modality::review);
		}
		apply_modality_surface();
	}

	void workbench_window::apply_review_details_visibility()
	{
		if (ReviewDetailsAction == nullptr) {
			return;
		}
		const auto visible = ReviewDetailsAction->isChecked();
		// The epoch line is chrome and stays; the exact anchors, OIDs and
		// history maintenance beneath it are the review details. [[BPR-1334]]:
		// the strip under the comparison is Line details now, with its own
		// View menu entry, so this no longer shows or hides it.
		if (EpochSelectors != nullptr) {
			EpochSelectors->set_details_visible(visible);
		}
		const auto description = visible
			? QStringLiteral(
				  "Hide the exact review anchors and give their height to the comparison.")
			: QStringLiteral(
				  "Show the exact review anchors.");
		ReviewDetailsAction->setToolTip(description);
		ReviewDetailsAction->setStatusTip(description);
		// [[BPR-1130]]. The details sit in the Updates line's control, which a
		// fold would otherwise hide with the timeline.
		apply_updates_line();
	}

	void workbench_window::apply_strip_width()
	{
		// The title row never folds a control into an overflow menu: a control
		// that disappears at one width and returns at another is exactly the
		// chrome the accepted design rejects. What yields is text: the name's
		// title and owner, then the modality's shortcuts, then the name.
		if (IdentityAction == nullptr) {
			return;
		}
		const auto available = width();
		// [[BPR-0722]], [[BPR-1129]]. With a pull request open the name stays
		// as far down as its number fits, taking whatever the title row's other
		// items leave (see update_strip_identity). With nothing open there is
		// nothing to identify, and the label takes no width.
		IdentityAction->setVisible((!StripHome && !IdentityText.isEmpty()) || (LocalCycleShown && !showing_home()));
		// [[BPR-1338]]. Open on GitHub goes with an accepted GitHub pull
		// request's name, never a local cycle's, and is measured with it below.
		// It is shown only where it acts: nothing in the title row reads as
		// disabled.
		const auto showPage = [this] {
			if (OpenOnGithubButtonAction != nullptr) {
				OpenOnGithubButtonAction->setVisible(IdentityAction->isVisible() && !(LocalCycleShown && !showing_home()) && PageContext.has_value());
			}
		};
		showPage();
		const auto identityShort = [this] {
			return IdentityAction->isVisible() && strip_identity_budget() < strip_identity_minimum();
		};
		const auto remeasure = [this] {
			if (auto* const layout = PrimaryToolbar->layout(); layout != nullptr) {
				layout->invalidate();
			}
		};
		if (Modality != nullptr) {
			// The segments keep their names and lose the shortcut beside them.
			// [[BPR-0824]]. Decided from the width first, then compact only if
			// the number is still short, with the toolbar remeasured after each,
			// so a resize never alternates between the two forms.
			// [[BPR-1129]]. The title row holds no navigation now, so the
			// shortcuts stay down to the width the old strip needed for them.
			Modality->set_abbreviated(false);
			// [[BPR-1251]], [[BPR-1324]]. The title and then the owner yield
			// before the shortcuts, so whether each shows is decided from the
			// room it has beside the whole modality. That room only shrinks as
			// the window narrows, so the room a compact modality frees can
			// never bring either back.
			Modality->set_compact(false);
			remeasure();
			IdentityTitleBudget = strip_identity_budget();
#if defined(_WIN32)
			// [[BPR-1324]]. Windows' title row also holds the menu control and
			// the caption buttons, so with four modalities the shortcuts go
			// below 1300 there, which keeps the whole name at 1280 as on every
			// platform. Each segment's tooltip names its key.
			constexpr auto compactBelow = 1'300;
#else
			constexpr auto compactBelow = 1'100;
#endif
			Modality->set_compact(available < compactBelow);
			remeasure();
			if (identityShort()) {
				Modality->set_compact(true);
				remeasure();
			}
		}
		if (identityShort()) {
			IdentityAction->setVisible(false);
			showPage();
		}
		update_strip_identity();
		// [[BPR-0653]]. The last yield, the owner's: when the row still asks
		// for more than the window after every step above, the modality
		// segments show three-character prefixes. It is taken only then, so a
		// row that already fits never shortens. Measured after the identity has
		// been elided to its budget, or the text it is about to give up would
		// be counted as overflow.
		if (Modality != nullptr && PrimaryToolbar != nullptr) {
			remeasure();
			if (PrimaryToolbar->sizeHint().width() > available) {
				Modality->set_abbreviated(true);
				remeasure();
				// The room the prefixes free is the identity's to use.
				update_strip_identity();
			}
		}
		apply_review_bar_width();
	}

	void workbench_window::apply_review_bar_width()
	{
		if (ReviewBar == nullptr || EpochSelectors == nullptr) {
			return;
		}
		// [[BPR-1129]]. Every control on the review bar stays top level. As the
		// window narrows the comparison's dates go first, then the positions'
		// connective words ("Changes 3/11" for "Changes 3 of 11"), then the
		// numbers. File and Changes always remain; full positions stay in their
		// tooltips. Each width starts whole and measures, so nothing returns at a narrower
		// width and a resize never alternates.
		const auto remeasure = [this] {
			if (auto* const layout = ReviewBar->layout(); layout != nullptr) {
				layout->invalidate();
			}
		};
		// [[BPR-1255]]. The floor follows the bar, the owner's choice of
		// 2026-10-06: the window is never narrower than the bar with everything
		// yielded, so nothing folds in a larger font either. At the default
		// font the yielded bar is narrower than the platform floor, which
		// stands. A window below a raised floor is widened to it.
		EpochSelectors->set_comparing_dates_shown(false);
		ReviewBarCompact        = true;
		ReviewBarPositionsShown = false;
		update_strip_positions();
		// [[BPR-1578]]. The progress track's own yield is the last: Seen and
		// Read keep their marks and lose their words.
		if (ProgressTrack != nullptr) {
			ProgressTrack->set_compact(true);
		}
		// [[BPR-1700]]. Without Compose review's words, so the floor is
		// the bar with its icon, as before they were added.
		ComposeReviewWords = compose_words::none;
		present_compose_entry();
		remeasure();
		const auto inset = ReviewBar->isVisible() ? std::max(0, width() - ReviewBar->width()) : 0;
		const auto floor = std::max(platform_floor_width, ReviewBar->sizeHint().width() + inset);
		if (minimumWidth() != floor) {
			setMinimumWidth(floor);
		}
		const auto available = width();
		EpochSelectors->set_comparing_dates_shown(true);
		ReviewBarCompact        = false;
		ReviewBarPositionsShown = true;
		update_strip_positions();
		if (ProgressTrack != nullptr) {
			ProgressTrack->set_compact(false);
		}
		// [[BPR-1700]]. Compose review's words shorten first, to
		// "Approve…", the verdict a newcomer looks for, and that goes last of
		// all, after the dates, the positions' words and the steps' names.
		ComposeReviewWords = compose_words::full;
		present_compose_entry();
		remeasure();
		if (ReviewBar->sizeHint().width() > available) {
			ComposeReviewWords = compose_words::brief;
			present_compose_entry();
			remeasure();
		}
		if (ReviewBar->sizeHint().width() > available) {
			EpochSelectors->set_comparing_dates_shown(false);
			remeasure();
		}
		if (ReviewBar->sizeHint().width() > available) {
			ReviewBarCompact = true;
			update_strip_positions();
			remeasure();
		}
		if (ReviewBar->sizeHint().width() > available) {
			ReviewBarPositionsShown = false;
			update_strip_positions();
			remeasure();
		}
		if (ReviewBar->sizeHint().width() > available && ProgressTrack != nullptr) {
			ProgressTrack->set_compact(true);
			remeasure();
		}
		if (ReviewBar->sizeHint().width() > available) {
			ComposeReviewWords = compose_words::none;
			present_compose_entry();
			remeasure();
		}
	}

	void workbench_window::changeEvent(QEvent* const aEvent)
	{
		QMainWindow::changeEvent(aEvent);
		switch (aEvent->type()) {
		case QEvent::WindowStateChange:
			dispatch_geometry();
			break;
		case QEvent::ActivationChange:
			update_dif_presentation();
			// [[BPR-0990]]. Coming back to a window that shows Home is the
			// other activation discovery hears.
			if (isActiveWindow() && showing_home() && HomeAccount && QueueDiscoveryDispatch) {
				QueueDiscoveryDispatch(application::queue_discovery_intent::activate);
			}
			break;
		case QEvent::ApplicationPaletteChange:
		case QEvent::PaletteChange:
			// Settings is a separate top-level window, so Qt does not inherit
			// the workbench's profile palette into either shell (BPR-1513).
			if (SettingsWindow != nullptr) {
				SettingsWindow->setPalette(palette());
			}
			if (MountedSettings) {
				MountedSettings->setPalette(palette());
			}
			// [[BPR-1566]]. A link's colour is set, not inherited: take the new
			// palette's, readable on its ground. Mark colours, beside it, too.
			for (auto* const link : { ConventionalCommentsLink, MarkColoursLink, WorkshopGuidanceLink }) {
				if (link != nullptr && link->parentWidget() != nullptr) {
					auto linkPalette = link->palette();
					linkPalette.setColor(QPalette::ButtonText, readable_link_colour(link->parentWidget()->palette()));
					link->setPalette(linkPalette);
				}
			}
			[[fallthrough]];
		case QEvent::ApplicationFontChange:
		case QEvent::FontChange:
		case QEvent::StyleChange:
		case QEvent::ThemeChange:
		case QEvent::DevicePixelRatioChange:
			refresh_toolbar_icons();
			// [[BPR-1592]]. The Agent tab's spark and the Notes tools follow
			// the theme.
			if (TrayTabs != nullptr && TrayTabs->count() > tray_findings_tab) {
				TrayTabs->setTabIcon(tray_findings_tab, drawer_icon(drawer_glyph::agent, palette()));
				fit_tray_tabs(true);
			}
			if (PrivateNotes != nullptr) {
				PrivateNotes->new_line_action()->setIcon(conversation_icon(conversation_glyph::comment, palette()));
				PrivateNotes->elsewhere_action()->setIcon(drawer_icon(drawer_glyph::pull_request, palette()));
			}
			// [[BPR-1255]]. A font, style or theme can move the review bar's
			// yielded width, and with it the window's floor.
			apply_strip_width();
			// The pull-request title and identity carry derived emphasis fonts,
			// which stop tracking the application font unless they are re-derived
			// here. `github_window` did this; the shell must too, or a reviewer
			// who enlarges the system font gets a title that does not grow.
			if (PullRequestPanel != nullptr) {
				PullRequestPanel->refresh_native_appearance();
			}
			if (ConversationPanel != nullptr) {
				ConversationPanel->relayout();
			}
			break;
		default:
			break;
		}
	}

	void workbench_window::set_review_navigation_dispatch(std::function<void(std::optional<core::workbench_command_id>)> aDispatch)
	{
		ReviewNavigationDispatch = std::move(aDispatch);
	}

	void workbench_window::refuse_review_navigation(const core::workbench_command_id aCommand)
	{
		if (showing_home() || showing_local_cycle() || current_modality() != review_modality::review) {
			return;
		}
		// A refused press says so for as long as a bound does: an arrow that
		// silently does nothing reads as broken.
		statusBar()->showMessage(QStringLiteral("This tab's file is unavailable. Choose another file or comparison."),
								 bounded_navigation_message_ms);
		focus_review_navigation(aCommand);
	}

	void workbench_window::dispatch_navigation(const std::size_t aIndex)
	{
		if (showing_home() || showing_local_cycle()) {
			return;
		}
		const auto id = static_cast<core::workbench_command_id>(aIndex);
		if (const auto offered = command(id); !offered || !offered->Enabled) {
			return;
		}
		// The core's normal entry restores Review's saved drawers. Bounds are
		// evaluated only after its active tab's exact source has returned.
		if (current_modality() != review_modality::review) {
			choose_modality(review_modality::review);
		}
		focus_review_navigation(id);
		if (ReviewNavigationDispatch) {
			ReviewNavigationDispatch(id);
		} else {
			perform_review_navigation(id);
		}
	}

	void workbench_window::focus_review_navigation(const core::workbench_command_id aCommand)
	{
		if (showing_home() || showing_local_cycle() || current_modality() != review_modality::review) {
			return;
		}
		if (ComparisonSurface->current_result() && ComparisonSurface->isVisible()) {
			ComparisonSurface->focus_source();
		} else if (const auto index = static_cast<std::size_t>(aCommand); index < Navigation.size()) {
			if (auto* const control = ReviewBar->widgetForAction(Navigation[index].Action); control != nullptr && control->isVisible()) {
				control->setFocus(Qt::ShortcutFocusReason);
			}
		}
	}

	void workbench_window::perform_review_navigation(const core::workbench_command_id aCommand)
	{
		const auto index = static_cast<std::size_t>(aCommand);
		const auto ends  = aCommand == core::workbench_command_id::first_change || aCommand == core::workbench_command_id::last_change;
		if ((index >= Navigation.size() && !ends) || showing_home() || showing_local_cycle() || current_modality() != review_modality::review) {
			return;
		}
		if (const auto offered = command(aCommand);
			!offered || !offered->Enabled || offered->Bound != core::workbench_reason_code::none) {
			// [[BPR-1532]]. First and Last change are disabled in Review where
			// nothing can be reached, so a press that entered Review from
			// elsewhere meets a disabled command and names its reason too. An
			// arrow is disabled only without a workbench, which names nothing.
			const auto reason = !offered ? core::workbench_reason_code::no_active_workbench
				: offered->Enabled       ? offered->Bound
										 : offered->DisabledReason;
			if (reason != core::workbench_reason_code::no_active_workbench) {
				statusBar()->showMessage(index < file_navigation_entries ? reason_text(reason) : change_reason_text(reason),
										 bounded_navigation_message_ms);
			}
			focus_review_navigation(aCommand);
			return;
		}
		if (index < file_navigation_entries) {
			if (Dispatch) {
				Dispatch(core::navigate_workbench_file_intent{
					.Direction = index == 0U
						? core::change_navigation_direction::previous
						: core::change_navigation_direction::next,
				});
			}
			focus_review_navigation(aCommand);
			return;
		}
		const auto direction = aCommand == core::workbench_command_id::previous_change || aCommand == core::workbench_command_id::first_change
			? core::change_navigation_direction::previous
			: core::change_navigation_direction::next;
		if (!ChangeDispatch) {
			return;
		}
		// The gesture repeats the policy the projection offered it, so the
		// emitted intent and the enablement that offered it cannot disagree about
		// stop versus wrap. First and Last change reach the end of the order.
		ChangeDispatch(core::navigate_diff_change_intent{
			.Direction = direction,
			.Policy    = Commands.Policy,
			.Reach     = ends ? core::change_navigation_reach::outermost : core::change_navigation_reach::adjacent,
		});
		focus_review_navigation(aCommand);
	}

	/// \brief Builds the window's menu bar and its View menu.
	///
	/// \details This window had no menu bar until BPR-0457. The owner asked
	/// for a "view menu option" and was given the choice between a real menu
	/// bar and a checkable entry in the existing toolbar popup; they chose the
	/// menu bar, which is the option that matches the words and, on macOS,
	/// merges into the system bar and costs the window no height. On Windows
	/// and Linux it takes a row, which is the price of the same answer being
	/// true everywhere.
	void workbench_window::build_menu_bar()
	{
		PrimaryMenuBar = new QMenuBar{ this };
		PrimaryMenuBar->setObjectName(QStringLiteral("workbenchMenuBar"));
		PrimaryMenuBar->setAccessibleName(QStringLiteral("Workbench menu bar"));
		PrimaryMenuBar->setAccessibleDescription(
			QStringLiteral("Presentation menus for the review workbench"));
		setMenuBar(PrimaryMenuBar);

		auto* fileMenu = PrimaryMenuBar->addMenu(QStringLiteral("&File"));
		fileMenu->setAccessibleName(QStringLiteral("File"));
		ProfilesMenu = PrimaryMenuBar->addMenu(QStringLiteral("&Profiles"));
		ProfilesMenu->setObjectName(QStringLiteral("workbenchProfilesMenu"));
		ProfilesMenu->setAccessibleName(QStringLiteral("Profiles"));
		NewBprWindowAction = ProfilesMenu->addAction(QStringLiteral("New Window"));
		NewBprWindowAction->setObjectName(QStringLiteral("newBprWindowAction"));
		NewBprWindowAction->setEnabled(false);
		connect(NewBprWindowAction, &QAction::triggered, this, [this] {
			if (ProfileManagementDispatch && CurrentBprProfile.value() != 0U) {
				ProfileManagementDispatch(application::new_bpr_window_intent{ CurrentBprProfile });
			}
		});
		ManageProfilesAction = ProfilesMenu->addAction(QStringLiteral("Manage Profiles…"));
		ManageProfilesAction->setObjectName(QStringLiteral("manageProfilesAction"));
		ManageProfilesAction->setEnabled(false);
		connect(ManageProfilesAction, &QAction::triggered, this, [this] {
			if (ManageProfiles) {
				ManageProfiles();
			}
		});
		// [[BPR-0777]]. RFC-BPR-0033 B3 reports a profile refusal in a window that
		// exists rather than presenting the chooser to carry it. Shown only after a
		// refusal, so at every other moment the menu is the profiles and their
		// two actions.
		ProfileStatusSeparator = ProfilesMenu->addSeparator();
		ProfileStatusSeparator->setVisible(false);
		ProfileStatusAction = ProfilesMenu->addAction(QString{ });
		ProfileStatusAction->setObjectName(QStringLiteral("profileManagementStatusAction"));
		ProfileStatusAction->setEnabled(false);
		ProfileStatusAction->setVisible(false);
		// [[BPR-1157]]. The local Review Cycles entry, before Quit.
		OpenProjectAction = fileMenu->addAction(QStringLiteral("Open Project…"));
		OpenProjectAction->setObjectName(QStringLiteral("openProjectAction"));
		OpenProjectAction->setToolTip(QStringLiteral("Review a local folder or file as a Review Cycle"));
		OpenProjectAction->setEnabled(false);
		connect(OpenProjectAction, &QAction::triggered, this, [this] {
			if (OpenProjectDispatch) {
				OpenProjectDispatch();
			}
		});
		fileMenu->addSeparator();
		QuitProfilesAction = fileMenu->addAction(QStringLiteral("Quit"));
		QuitProfilesAction->setObjectName(QStringLiteral("quitProfilesAction"));
		QuitProfilesAction->setMenuRole(QAction::QuitRole);
		QuitProfilesAction->setShortcuts(shortcut_keys(shortcut_command::quit));
		QuitProfilesAction->setEnabled(false);
		connect(QuitProfilesAction, &QAction::triggered, this, [this] {
			if (QuitProfiles) {
				QuitProfiles();
			}
		});

		// [[BPR-0990]]. The open pull request's two local gestures. Neither
		// reaches GitHub: Follow keeps it in my queue, Save Repository keeps
		// its repository for discovery.
		PullRequestMenu = PrimaryMenuBar->addMenu(QStringLiteral("P&ull Request"));
		PullRequestMenu->setObjectName(QStringLiteral("workbenchPullRequestMenu"));
		PullRequestMenu->setAccessibleName(QStringLiteral("Pull Request"));
		PullRequestMenu->setAccessibleDescription(
			QStringLiteral("Open the pull request on GitHub, keep it or its repository in my queue, or mark its comments read; nothing changes on GitHub"));
		FollowPullRequest = PullRequestMenu->addAction(QStringLiteral("&Follow"));
		FollowPullRequest->setObjectName(QStringLiteral("followPullRequestAction"));
		FollowPullRequest->setCheckable(true);
		FollowPullRequest->setEnabled(false);
		connect(FollowPullRequest, &QAction::triggered, this, [this] {
			const auto ref = open_queue_ref();
			if (!ref || !QueueFollowDispatch) {
				update_pull_request_queue_actions();
				return;
			}
			const auto item     = QueueLanding->item_for(*ref);
			const auto followed = item && item->Membership.Followed;
			QueueFollowDispatch(application::queue_follow_intent{ .Ref = *ref, .Title = OpenQueueTitle, .Follow = !followed });
			// The check mark is the queue's to decide.
			update_pull_request_queue_actions();
		});
		SavePullRequestRepository = PullRequestMenu->addAction(QStringLiteral("&Save Repository"));
		SavePullRequestRepository->setObjectName(QStringLiteral("savePullRequestRepositoryAction"));
		SavePullRequestRepository->setEnabled(false);
		connect(SavePullRequestRepository, &QAction::triggered, this, [this] {
			const auto ref = open_queue_ref();
			if (!ref || !RepositoryAdditionDispatch || RepositoryGesture == std::numeric_limits<std::uint64_t>::max()) {
				return;
			}
			RepositoryPendingGesture       = ++RepositoryGesture;
			RepositoryDisplayedDestination = QueueProjection.Session;
			RepositoryAdditionDispatch({ .Gesture = RepositoryPendingGesture, .Destination = RepositoryDisplayedDestination, .Text = ref->Repository });
		});
		// [[BPR-1699]]. Mark All Comments as Read: my read state for
		// every comment on the open pull request, in one durable change.
		// Like Follow it is local; GitHub is not told.
		PullRequestMenu->addSeparator();
		MarkAllCommentsRead = PullRequestMenu->addAction(QStringLiteral("&Mark All Comments as Read"));
		MarkAllCommentsRead->setObjectName(QStringLiteral("markAllCommentsReadAction"));
		MarkAllCommentsRead->setStatusTip(QStringLiteral("Mark every comment on this pull request as read. Only your own read state changes; nothing changes on GitHub."));
		MarkAllCommentsRead->setToolTip(MarkAllCommentsRead->statusTip());
		MarkAllCommentsRead->setEnabled(false);
		connect(MarkAllCommentsRead, &QAction::triggered, this, [this] {
			const auto dispatch = ConversationsReadDispatch;
			if (dispatch) {
				dispatch(std::nullopt);
			}
		});

		ViewMenu = PrimaryMenuBar->addMenu(QStringLiteral("&View"));
		ViewMenu->setObjectName(QStringLiteral("workbenchViewMenu"));
		ViewMenu->setAccessibleName(QStringLiteral("View"));
		auto* findMenu = ViewMenu->addMenu(tr("&Find"));
		findMenu->setObjectName(QStringLiteral("sourceFindMenu"));
		const auto findTitles = std::array{ tr("Find"), tr("Next pane match"), tr("Previous pane match") };
		const auto findNames  = std::array{ QStringLiteral("sourceFindAction"), QStringLiteral("sourceFindNextAction"), QStringLiteral("sourceFindPreviousAction") };
		for (std::size_t index = 0; index < SourceFindActions.size(); ++index) {
			auto* action = new QAction{ findTitles[index], this };
			action->setObjectName(findNames[index]);
			action->setShortcutContext(Qt::WindowShortcut);
			action->setShortcuts(shortcut_keys(std::array{ shortcut_command::find, shortcut_command::find_next, shortcut_command::find_previous }[index]));
			SourceFindActions[index] = action;
			action->setEnabled(false);
			addAction(action);
			findMenu->addAction(action);
			connect(action, &QAction::triggered, this, [this, index] {
				update_source_find_actions();
				if (!SourceFindActions[index]->isEnabled()) {
					return;
				}
				if (index == 0U) {
					ComparisonSurface->open_source_find();
				} else {
					ComparisonSurface->navigate_source_find(index == 1U ? core::change_navigation_direction::next : core::change_navigation_direction::previous);
				}
			});
		}
		const auto fileTitles = std::array{ tr("Find in Files"), tr("Next PR result"), tr("Previous PR result") };
		const auto fileNames  = std::array{ QStringLiteral("sourceFilesAction"), QStringLiteral("sourceFilesNextAction"), QStringLiteral("sourceFilesPreviousAction") };
		for (std::size_t index = 0; index < SourceFilesActions.size(); ++index) {
			auto* action = new QAction{ fileTitles[index], this };
			action->setObjectName(fileNames[index]);
			action->setShortcuts(shortcut_keys(std::array{ shortcut_command::find_in_files, shortcut_command::next_result, shortcut_command::previous_result }[index]));
			action->setShortcutContext(Qt::WindowShortcut);
			action->setEnabled(false);
			SourceFilesActions[index] = action;
			addAction(action);
			findMenu->addAction(action);
			connect(action, &QAction::triggered, this, [this, index] {update_source_find_actions();if(!SourceFilesActions[index]->isEnabled() || !PrSearchDispatch){return;}if(index==0){if(!SearchPanel->projection().Open){SearchWasCollapsed=DrawerStates.Files==core::workbench_drawer_state::collapsed;SearchPreviousTab=FileSearchTabs->currentIndex();}if(current_modality()!=review_modality::review || CompositionPage){choose_modality(review_modality::review);}if(Dispatch){Dispatch(core::set_workbench_drawer_intent{core::workbench_drawer::files,core::workbench_drawer_state::expanded});}FileSearchTabs->setCurrentWidget(SearchPanel);PrSearchDispatch(source_find_gesture::document_changed);}else{if(current_modality()!=review_modality::review || CompositionPage){choose_modality(review_modality::review);}PrSearchDispatch(index==1?source_find_gesture::next:source_find_gesture::previous);} });
		}
		connect(findMenu, &QMenu::aboutToShow, this, [this] { update_source_find_actions(); });
		connect(qApp, &QApplication::focusChanged, this, [this](QWidget*, QWidget*) { update_source_find_actions(); });

		ViewMenu->setAccessibleDescription(
			QStringLiteral("Presentation options for the comparison surface"));
		HeaderSourceAction = new QAction{ tr("Go to header/source"), this };
		HeaderSourceAction->setObjectName(QStringLiteral("goToHeaderSourceAction"));
		HeaderSourceAction->setShortcuts(shortcut_keys(shortcut_command::header_source));
		HeaderSourceAction->setShortcutContext(Qt::WindowShortcut);
		addAction(HeaderSourceAction);
		ViewMenu->addAction(HeaderSourceAction);
		ViewMenu->addSeparator();
		connect(HeaderSourceAction, &QAction::triggered, this, [this] {
			if (HeaderSourceAction->isEnabled() && HeaderSourceDispatch) {
				HeaderSourceDispatch();
			}
		});

		LineNumbersAction
			= new QAction{ QStringLiteral("&Line numbers"), this };
		LineNumbersAction->setObjectName(
			QStringLiteral("toggleWorkbenchLineNumbersAction"));
		LineNumbersAction->setCheckable(true);
		LineNumbersAction->setChecked(true);
		LineNumbersAction->setShortcuts(shortcut_keys(shortcut_command::line_numbers));
		LineNumbersAction->setShortcutContext(Qt::WindowShortcut);
		addAction(LineNumbersAction);
		ViewMenu->addAction(LineNumbersAction);
		// `triggered` rather than `toggled`: the checked state is the
		// projection's to decide, and a toggled handler would also fire when
		// the projection sets it, turning one gesture into two.
		connect(LineNumbersAction, &QAction::triggered, this, [this] {
			dispatch_line_numbers_toggle();
		});
		OnePaneAction = new QAction{ QStringLiteral("&One pane"), this };
		OnePaneAction->setObjectName(QStringLiteral("toggleWorkbenchOnePaneAction"));
		OnePaneAction->setCheckable(true);
		addAction(OnePaneAction);
		ViewMenu->addAction(OnePaneAction);
		connect(OnePaneAction, &QAction::triggered, this, [this] {
			dispatch_one_pane_toggle();
		});
		// [[BPR-1334]]. Line details under the comparison, shown by default.
		// The choice is the profile's review behaviour; the checked mark is
		// what the host published, so a refused save puts it back.
		LineDetailsAction = new QAction{ QStringLiteral("Line &Details"), this };
		LineDetailsAction->setObjectName(QStringLiteral("toggleWorkbenchLineDetailsAction"));
		LineDetailsAction->setCheckable(true);
		LineDetailsAction->setChecked(Behaviour.LineDetails == application::review_line_details::shown);
		LineDetailsAction->setToolTip(QStringLiteral("Show the current line's base and head versions in full under the comparison"));
		LineDetailsAction->setStatusTip(LineDetailsAction->toolTip());
		addAction(LineDetailsAction);
		ViewMenu->addAction(LineDetailsAction);
		connect(LineDetailsAction, &QAction::triggered, this, [this] {
			dispatch_line_details_toggle();
		});
		// [[BPR-1295]]. Hide Drawers. Command-B (Ctrl+B elsewhere), the
		// owner's left-hand key and the one Visual Studio Code uses for its
		// side bar. Checked while the drawers are hidden. Its presenter is the
		// title row's button right of the Comments toggle.
		ToggleDrawersAction = new QAction{ QStringLiteral("Hide &Drawers"), this };
		ToggleDrawersAction->setObjectName(QStringLiteral("toggleWorkbenchDrawersAction"));
		ToggleDrawersAction->setIconText(QStringLiteral("Hide drawers"));
		ToggleDrawersAction->setCheckable(true);
		ToggleDrawersAction->setShortcuts(shortcut_keys(shortcut_command::hide_drawers));
		ToggleDrawersAction->setShortcutContext(Qt::WindowShortcut);
		addAction(ToggleDrawersAction);
		ViewMenu->addAction(ToggleDrawersAction);
		connect(ToggleDrawersAction, &QAction::triggered, this, [this] {
			dispatch_drawers_toggle();
		});
		// [[BPR-1293]]. Command-J (Ctrl+J), unbound before: straight to
		// Threads from Overview, Review or Discuss, and back from it.
		ShowThreadsAction = new QAction{ QStringLiteral("&Threads"), this };
		ShowThreadsAction->setObjectName(QStringLiteral("showWorkbenchThreadsAction"));
		ShowThreadsAction->setShortcuts(shortcut_keys(shortcut_command::find_thread));
		ShowThreadsAction->setShortcutContext(Qt::WindowShortcut);
		ShowThreadsAction->setEnabled(false);
		{
			const auto tip = QStringLiteral("Find a thread in the pull request (%1)").arg(ShowThreadsAction->shortcut().toString(QKeySequence::NativeText));
			ShowThreadsAction->setToolTip(tip);
			ShowThreadsAction->setStatusTip(tip);
		}
		addAction(ShowThreadsAction);
		ViewMenu->addAction(ShowThreadsAction);
		connect(ShowThreadsAction, &QAction::triggered, this, [this] { toggle_threads(); });
		// [[BPR-1581]]. The selected thread in the other view: Option-Command-3
		// beside Command-3 for Discuss, Option-Command-2 beside Command-2 for
		// Review. Ctrl+Alt is AltGr on Windows, which types a character with
		// these digits, so both step aside while typing.
		ShowInDiscussAction = new QAction{ QStringLiteral("Show Thread in Discuss"), this };
		ShowInDiscussAction->setObjectName(QStringLiteral("showThreadInDiscussAction"));
		ShowInDiscussAction->setShortcuts(shortcut_keys(shortcut_command::show_in_discuss));
		ShowInDiscussAction->setShortcutContext(Qt::WindowShortcut);
		ShowInReviewAction = new QAction{ QStringLiteral("Show Thread in Review"), this };
		ShowInReviewAction->setObjectName(QStringLiteral("showThreadInReviewAction"));
		ShowInReviewAction->setShortcuts(shortcut_keys(shortcut_command::show_in_review));
		ShowInReviewAction->setShortcutContext(Qt::WindowShortcut);
		for (auto* const action : { ShowInDiscussAction, ShowInReviewAction }) {
			const auto tip = action == ShowInDiscussAction ? QStringLiteral("Show the selected thread in Discuss (%1)") : QStringLiteral("Show the selected thread in Review, beside its code (%1)");
			action->setToolTip(tip.arg(action->shortcut().toString(QKeySequence::NativeText)));
			action->setStatusTip(action->toolTip());
			action->setEnabled(false);
			addAction(action);
			ViewMenu->addAction(action);
		}
		connect(ShowInDiscussAction, &QAction::triggered, this, [this] { static_cast<void>(show_selected_thread(core::workbench_thread_destination::discuss)); });
		connect(ShowInReviewAction, &QAction::triggered, this, [this] { static_cast<void>(show_selected_thread(core::workbench_thread_destination::review)); });
		// [[BPR-1586]]. Option-Command-4 beside Command-4 for Threads.
		ShowInThreadsAction = new QAction{ QStringLiteral("Show Thread in Threads"), this };
		ShowInThreadsAction->setObjectName(QStringLiteral("showThreadInThreadsAction"));
		ShowInThreadsAction->setShortcuts(shortcut_keys(shortcut_command::show_in_threads));
		ShowInThreadsAction->setShortcutContext(Qt::WindowShortcut);
		ShowInThreadsAction->setToolTip(QStringLiteral("Show the selected thread's row in Threads (%1)").arg(ShowInThreadsAction->shortcut().toString(QKeySequence::NativeText)));
		ShowInThreadsAction->setStatusTip(ShowInThreadsAction->toolTip());
		ShowInThreadsAction->setEnabled(false);
		addAction(ShowInThreadsAction);
		ViewMenu->addAction(ShowInThreadsAction);
		connect(ShowInThreadsAction, &QAction::triggered, this, [this] { static_cast<void>(show_selected_thread(core::workbench_thread_destination::threads)); });
		EditorKeyYield = new editor_key_yield{ this, ToggleDrawersAction, editor_native_keys() };
		connect(qApp, &QApplication::focusChanged, this, [this](QWidget* const aOld, QWidget* const aNow) {
			update_header_source_action();
			update_source_find_actions();
			update_typing_keys();
			if (aNow != nullptr && editable_multi_line(aNow) && isAncestorOf(aNow)) {
				aNow->installEventFilter(EditorKeyYield);
			}
			// The command is disabled while an editor here has the keyboard,
			// which is what keeps the native macOS menu item from taking the
			// key: AppKit fires a menu key equivalent without asking Qt's
			// focus object (qt_hide_drawers_native_menu_key).
			if ((aOld != nullptr && isAncestorOf(aOld)) || (aNow != nullptr && isAncestorOf(aNow))) {
				apply_drawers_toggle();
			}
		});
		// [[BPR-1130]]. The fold's command, which the menu offers by keyboard as
		// the review bar's disclosure offers it by pointer. It takes no key of
		// its own: RFC-BPR-0052 decided the window's keys, and none is the fold.
		UpdatesLineAction = new QAction{ QStringLiteral("&Updates Line"), this };
		UpdatesLineAction->setObjectName(QStringLiteral("toggleUpdatesLineAction"));
		UpdatesLineAction->setCheckable(true);
		addAction(UpdatesLineAction);
		ViewMenu->addAction(UpdatesLineAction);
		connect(UpdatesLineAction, &QAction::triggered, this, [this] {
			dispatch_updates_line_toggle();
		});
		// [[BPR-1336]]. The comparison's zoom. Command-= and Command-+ (Ctrl+=
		// and Ctrl++) zoom in and Command-minus zooms out, as every browser and
		// editor does. [[BPR-1535]]: Actual Size is Command-0 (Ctrl+0), which
		// was Home's until the owner moved Home to Command-Shift-H; it replaces
		// Command-Option-0 rather than adding to it. Inside the open label menu
		// Command-0 is still Plain: the popup claims it before the window.
		ViewMenu->addSeparator();
		const auto zoomAction = [this](const QString& aText, const QString& aName, const shortcut_command aKeys, const core::code_zoom_step aStep) {
			auto* const action = new QAction{ aText, this };
			action->setObjectName(aName);
			action->setShortcuts(shortcut_keys(aKeys));
			action->setShortcutContext(Qt::WindowShortcut);
			action->setEnabled(false);
			addAction(action);
			ViewMenu->addAction(action);
			connect(action, &QAction::triggered, this, [this, aStep] { dispatch_code_zoom(aStep); });
			return action;
		};
		ZoomInAction     = zoomAction(tr("Zoom In"), QStringLiteral("zoomInCodeAction"), shortcut_command::zoom_in, core::code_zoom_step::in);
		ZoomOutAction    = zoomAction(tr("Zoom Out"), QStringLiteral("zoomOutCodeAction"), shortcut_command::zoom_out, core::code_zoom_step::out);
		ActualSizeAction = zoomAction(tr("Actual Size"), QStringLiteral("actualSizeCodeAction"), shortcut_command::actual_size, core::code_zoom_step::actual_size);
		apply_code_zoom();
#if defined(Q_OS_WIN)
		qApp->installEventFilter(new typed_text_yield{ this });
#endif
		qApp->installEventFilter(new typed_text_key_claim{ this, [this] { update_typing_keys(); } });
		ViewMenu->addSeparator();
		// [[BPR-1132]]. The tab keys, window-owned: Ctrl+Tab and Ctrl+Shift+Tab
		// move between the tabs (the Control key on macOS too, where Qt names
		// it Meta), and Cmd-W (Ctrl+W) closes the active tab. Nothing had
		// Cmd-W before, so no window command moves to Cmd-Shift-W.
		const auto tabAction = [this](const QString& aText, const QString& aName, const shortcut_command aKey) {
			auto* const action = new QAction{ aText, this };
			action->setObjectName(aName);
			action->setShortcuts(shortcut_keys(aKey));
			action->setShortcutContext(Qt::WindowShortcut);
			action->setEnabled(false);
			addAction(action);
			return action;
		};
		NextTabAction     = tabAction(tr("Next Tab"), QStringLiteral("nextDocumentTabAction"), shortcut_command::next_tab);
		PreviousTabAction = tabAction(tr("Previous Tab"), QStringLiteral("previousDocumentTabAction"), shortcut_command::previous_tab);
		CloseTabAction    = tabAction(tr("Close Tab"), QStringLiteral("closeDocumentTabAction"), shortcut_command::close_tab);
		ViewMenu->addAction(NextTabAction);
		ViewMenu->addAction(PreviousTabAction);
		fileMenu->addAction(CloseTabAction);
		connect(NextTabAction, &QAction::triggered, this, [this] {
			if (DocumentTabDispatch) {
				DocumentTabDispatch(core::activate_adjacent_tab_intent{ .Direction = core::change_navigation_direction::next });
			}
		});
		connect(PreviousTabAction, &QAction::triggered, this, [this] {
			if (DocumentTabDispatch) {
				DocumentTabDispatch(core::activate_adjacent_tab_intent{ .Direction = core::change_navigation_direction::previous });
			}
		});
		connect(CloseTabAction, &QAction::triggered, this, [this] {
			if (!DocumentTabDispatch || !TabsProjection) {
				return;
			}
			const auto active = std::ranges::find(TabsProjection->Tabs, true, &application::document_tab_entry::Active);
			if (active != TabsProjection->Tabs.end()) {
				DocumentTabDispatch(core::close_tab_intent{ .Tab = active->Id });
			}
		});
		// [[BPR-0646]]. Settings is a window, reached by the platform's
		// preferences shortcut; on macOS the role places it in the app menu.
		auto* diagnosticMenu = menuBar()->addMenu(QStringLiteral("&Help"));
		diagnosticMenu->setObjectName(QStringLiteral("releaseUpdateMenu"));
		// [[BPR-1567]]. Keyboard Shortcuts, first: Command-/ (Ctrl+/), the key
		// Slack and Google's editors list their shortcuts with. Command-? is
		// macOS's own Help-menu search, so it is not taken.
		KeyboardShortcutsAction = new QAction{ QStringLiteral("&Keyboard Shortcuts"), this };
		KeyboardShortcutsAction->setObjectName(QStringLiteral("keyboardShortcutsAction"));
		KeyboardShortcutsAction->setShortcuts(shortcut_keys(shortcut_command::keyboard_shortcuts));
		KeyboardShortcutsAction->setShortcutContext(Qt::WindowShortcut);
		KeyboardShortcutsAction->setStatusTip(QStringLiteral("List every keyboard shortcut, grouped by area and searchable"));
		addAction(KeyboardShortcutsAction);
		diagnosticMenu->addAction(KeyboardShortcutsAction);
		diagnosticMenu->addSeparator();
		connect(KeyboardShortcutsAction, &QAction::triggered, this, [this] { show_keyboard_shortcuts(); });
		auto* insights = diagnosticMenu->addAction(QStringLiteral("Insights…"));
		insights->setObjectName(QStringLiteral("openInsightsAction"));
#if defined(Q_OS_MACOS)
		static_cast<void>(use_native_insights_menu(*insights, *menuBar()));
#endif
		insights->setShortcuts(shortcut_keys(shortcut_command::insights));
		addAction(insights);
		connect(insights, &QAction::triggered, this, [this] { if (InsightsPresenter) { InsightsPresenter(); } });
		auto* diagnostics = diagnosticMenu->addAction(QStringLiteral("Diagnostics…"));
		diagnostics->setObjectName(QStringLiteral("openDiagnosticsAction"));
		connect(diagnostics, &QAction::triggered, this, [this] { open_settings(settings_page::storage_and_privacy); });
		diagnosticMenu->addSeparator();
		const auto addExport = [this, diagnosticMenu](const QString& aLabel, const QString& aName, core::diagnostic_export_policy aPolicy) {
			auto* action = diagnosticMenu->addAction(aLabel);
			action->setObjectName(aName);
			connect(action, &QAction::triggered, this, [this, aPolicy] { if (DiagnosticExporter) { DiagnosticExporter(aPolicy); } });
		};
		addExport(QStringLiteral("Export anonymized diagnostics…"), QStringLiteral("exportAnonymizedDiagnosticsAction"), core::diagnostic_export_policy::anonymized);
		addExport(QStringLiteral("Export with identifying details…"), QStringLiteral("exportIdentifyingDiagnosticsAction"), core::diagnostic_export_policy::identifying);
		SettingsAction = new QAction{ QStringLiteral("&Settings…"), this };
		SettingsAction->setObjectName(QStringLiteral("openSettingsAction"));
		SettingsAction->setMenuRole(QAction::PreferencesRole);
		// Preferences has no standard binding on Windows/GNOME. Qt maps CTRL
		// to Command on macOS; keep the explicit product shortcut on every host.
		SettingsAction->setShortcuts(shortcut_keys(shortcut_command::settings));
		// Each workbench owns this action. Application scope makes sibling
		// windows' identical shortcuts ambiguous instead of selecting the caller.
		SettingsAction->setShortcutContext(Qt::WindowShortcut);
		addAction(SettingsAction);
		ViewMenu->addSeparator();
		ViewMenu->addAction(SettingsAction);
		connect(SettingsAction, &QAction::triggered, this, [this] {
			open_settings(settings_page::appearance);
		});
	}

	void workbench_window::set_profile_management_dispatch(
		application::profile_management_dispatch aDispatch,
		std::function<void()> aManage, std::function<void()> aQuit)
	{
		ProfileManagementDispatch = std::move(aDispatch);
		ManageProfiles            = std::move(aManage);
		QuitProfiles              = std::move(aQuit);
		ManageProfilesAction->setEnabled(static_cast<bool>(ManageProfiles));
		QuitProfilesAction->setEnabled(static_cast<bool>(QuitProfiles));
	}

	void workbench_window::adopt_profile_management(
		const application::profile_management_projection& aProjection,
		const core::bpr_profile_id                        aCurrentProfile)
	{
		// [[BPR-0777]]. Adopted before the bounded-list guard: a refused projection is
		// exactly when this window has to say so.
		ProfileFailure = aProjection.Failure
			? std::optional{ aProjection.Failure->Reason }
			: std::nullopt;
		ProfileStatusAction->setText(ProfileFailure
										 ? profile_management_failure_text(*ProfileFailure)
										 : QString{ });
		ProfileStatusAction->setVisible(ProfileFailure.has_value());
		ProfileStatusSeparator->setVisible(ProfileFailure.has_value());
		update_status_text();
		if (aProjection.Profiles.size() > core::maximum_bpr_profiles) {
			return;
		}
		CurrentBprProfile = aCurrentProfile;
		NewBprWindowAction->setEnabled(aProjection.Ready && !aProjection.Closing
									   && std::ranges::any_of(aProjection.Profiles, [aCurrentProfile](const auto& aProfile) {
											  return aProfile.Profile == aCurrentProfile && aProfile.Runtime == application::profile_management_runtime::open;
										  }));
		update_home_review_actions();
		for (auto iterator = ProfileActions.begin(); iterator != ProfileActions.end();) {
			if (std::ranges::none_of(aProjection.Profiles, [iterator](const auto& aProfile) { return aProfile.Profile == iterator->first; })) {
				delete iterator->second;
				iterator = ProfileActions.erase(iterator);
			} else {
				++iterator;
			}
		}
		auto* before = NewBprWindowAction;
		for (auto iterator = aProjection.Profiles.rbegin(); iterator != aProjection.Profiles.rend(); ++iterator) {
			const auto& profile    = *iterator;
			auto [entry, inserted] = ProfileActions.try_emplace(profile.Profile, nullptr);
			if (inserted) {
				entry->second = new QAction{ ProfilesMenu };
				entry->second->setCheckable(true);
				entry->second->setData(QVariant::fromValue<qulonglong>(profile.Profile.value()));
				entry->second->setObjectName(QStringLiteral("openBprProfile_%1").arg(profile.Profile.value()));
				connect(entry->second, &QAction::triggered, this, [this, profileId = profile.Profile] {
					ProfileActions.at(profileId)->setChecked(profileId == CurrentBprProfile);
					if (ProfileManagementDispatch) {
						ProfileManagementDispatch(application::open_bpr_profile_intent{ profileId });
					}
				});
			}
			auto* action = entry->second;
			auto  label  = QString::fromStdString(profile.Label);
			label.replace(QStringLiteral("&"), QStringLiteral("&&"));
			action->setText(label);
			action->setChecked(profile.Profile == aCurrentProfile);
			action->setEnabled(aProjection.Ready && !aProjection.Closing
							   && profile.Runtime != application::profile_management_runtime::closing);
			const auto actions = ProfilesMenu->actions();
			if (actions.indexOf(action) + 1 != actions.indexOf(before) || !actions.contains(action)) {
				ProfilesMenu->insertAction(before, action);
			}
			before = action;
		}
	}

	auto workbench_window::profiles_menu() const noexcept -> QMenu*
	{
		return ProfilesMenu;
	}

	void workbench_window::set_diagnostic_exporter(std::function<void(core::diagnostic_export_policy)> aExporter)
	{
		DiagnosticExporter = std::move(aExporter);
	}

	void workbench_window::set_insights_presenter(std::function<void()> aPresenter) { InsightsPresenter = std::move(aPresenter); }

	void workbench_window::set_settings_presenter(std::function<void(settings_page)> aPresenter)
	{
		SettingsPresenter = std::move(aPresenter);
	}

	namespace {
		void swap_settings_pages(settings_window& aFirst, settings_window& aSecond)
		{
			auto*      first      = aFirst.pages();
			auto*      second     = aSecond.pages();
			const auto firstPage  = first->currentIndex();
			const auto secondPage = second->currentIndex();
			for (int index = 0; index < static_cast<int>(settings_page_count); ++index) {
				auto* firstPageWidget  = first->widget(index);
				auto* secondPageWidget = second->widget(index);
				first->removeWidget(firstPageWidget);
				second->removeWidget(secondPageWidget);
				first->insertWidget(index, secondPageWidget);
				second->insertWidget(index, firstPageWidget);
			}
			first->setCurrentIndex(firstPage);
			second->setCurrentIndex(secondPage);
		}
	}

	void workbench_window::mount_settings(settings_window& aSettings)
	{
		if (&aSettings == SettingsWindow || MountedSettings == &aSettings) {
			return;
		}
		unmount_settings();
		SettingsWindow->hide();
		swap_settings_pages(*SettingsWindow, aSettings);
		MountedSettings = &aSettings;
		aSettings.setPalette(palette());
		aSettings.set_close_handler([panel = QPointer<appearance_settings_panel>{ AppearanceSettings }] {
			if (panel) {
				panel->cancel_preview();
			}
		});
	}

	void workbench_window::unmount_settings()
	{
		if (!MountedSettings) {
			return;
		}
		auto* shell     = MountedSettings.data();
		MountedSettings = nullptr;
		shell->set_close_handler({ });
		swap_settings_pages(*SettingsWindow, *shell);
		AppearanceSettings->cancel_preview();
	}

	void workbench_window::set_sign_in_cancel_dispatch(std::function<void()> aDispatch)
	{
		SignInCancelDispatch = std::move(aDispatch);
	}

	auto workbench_window::sign_in_dialog() const noexcept -> QDialog* { return SignInDialog; }

	void workbench_window::connect_github_profile(const core::host_profile_id aProfile)
	{
		select_github_profile(aProfile);
		GithubConnectAction->trigger();
	}

	void workbench_window::present_cli_account_choices(std::vector<core::host_profile_id> aProfiles)
	{
		SignInCliProfiles = std::move(aProfiles);
		update_sign_in_surface();
	}

	void workbench_window::update_sign_in_surface()
	{
		if (!SignInDialog || !SignInDialog->isVisible()) {
			return;
		}
		for (auto* card : ServerCards) {
			const auto matches = SignInHostname.empty() || card->hostname() == SignInHostname;
			const auto cli     = !SignInCliOnly || std::ranges::any_of(SignInCliProfiles, [card](auto aProfile) { return card->holds(aProfile); });
			// [[BPR-0803]]. Choosing GitHub CLI while a device flow runs keeps
			// the card holding the flow in view: its code and address stay
			// beside the GitHub CLI choice until one of them ends. Without a
			// discovered account on this host the card shows only the flow.
			const auto flowHere = DevicePanel != nullptr && card->device_sign_in() == DevicePanel && DevicePanel->signing_in();
			card->setVisible(matches && (cli || flowHere));
			card->set_setup_mode(true, SignInCliOnly && cli);
		}
		if (auto* lede = AccountsPanel->findChild<QLabel*>(QStringLiteral("accountsLede"))) {
			lede->hide();
		}
		EnterpriseServerCancel->hide();
		AddEnterpriseServerButton->setVisible(QueueReady && static_cast<bool>(SavedServerDispatch) && SignInHostname.empty() && !SignInCliOnly && !EnterpriseDraftOpen);
		UseGithubCliButton->setVisible(static_cast<bool>(GithubCliDiscoveryDispatch)
									   && (!SignInCliOnly || GithubCliDiscoveryState == github_cli_discovery_state::empty
										   || GithubCliDiscoveryState == github_cli_discovery_state::process_failed
										   || GithubCliDiscoveryState == github_cli_discovery_state::malformed_response
										   || GithubCliDiscoveryState == github_cli_discovery_state::unsupported_version
										   || GithubCliDiscoveryState == github_cli_discovery_state::ineligible_accounts
										   || GithubCliDiscoveryState == github_cli_discovery_state::missing_executable
										   || GithubCliDiscoveryState == github_cli_discovery_state::launch_failed
										   || GithubCliDiscoveryState == github_cli_discovery_state::timed_out
										   || GithubCliDiscoveryState == github_cli_discovery_state::exited_unsuccessfully
										   || GithubCliDiscoveryState == github_cli_discovery_state::output_rejected));
		if (DevicePanel && DevicePanel->signing_in()) {
			DevicePanel->cancel_button()->hide();
		}
	}

	void workbench_window::open_sign_in(std::string aHostname, const bool aCliOnly)
	{
		if (!AccountsPanel || (SessionPanel != nullptr && !SessionPanel->connect_enabled())) {
			return;
		}
		if (!SignInDialog) {
			SignInDialog = new QDialog{ this };
			SignInDialog->setObjectName(QStringLiteral("sharedSignInDialog"));
			SignInDialog->setWindowModality(Qt::NonModal);
			auto* layout = new QVBoxLayout{ SignInDialog };
			SignInScroll = new QScrollArea{ SignInDialog };
			SignInScroll->setWidgetResizable(true);
			SignInScroll->setFrameShape(QFrame::NoFrame);
			layout->addWidget(SignInScroll);
			auto* buttons = new QDialogButtonBox{ QDialogButtonBox::Cancel, SignInDialog };
			layout->addWidget(buttons);
			connect(buttons, &QDialogButtonBox::rejected, SignInDialog, &QDialog::reject);
			connect(SignInDialog, &QDialog::finished, this, [this](int aResult) {
				SignInScroll->takeWidget();
				AccountsPanel->layout()->setAlignment(Qt::Alignment{ });
				if (AccountsReturnParent) {
					AccountsPanel->setParent(AccountsReturnParent);
					if (auto* returnLayout = qobject_cast<QVBoxLayout*>(AccountsReturnParent->layout())) {
						returnLayout->insertWidget(1, AccountsPanel);
					}
					AccountsPanel->show();
				}
				for (auto* card : ServerCards) {
					card->show();
					card->set_setup_mode(false);
				}
				if (auto* lede = AccountsPanel->findChild<QLabel*>(QStringLiteral("accountsLede"))) {
					lede->show();
				}
				EnterpriseServerCancel->show();
				SignInHostname.clear();
				SignInCliOnly = false;
				SignInCliProfiles.clear();
				UseGithubCliButton->setVisible(static_cast<bool>(GithubCliDiscoveryDispatch));
				close_enterprise_server_draft();
				update_server_cards();
				if (aResult == QDialog::Rejected && SignInCancelDispatch) {
					SignInCancelDispatch();
				}
			});
		}
		if (!SignInDialog->isVisible()) {
			AccountsReturnParent = AccountsPanel->parentWidget();
			AccountsPanel->layout()->setAlignment(Qt::AlignTop);
			SignInScroll->setWidget(AccountsPanel);
			SignInDialog->resize(620, 440);
		}
		SignInHostname = std::move(aHostname);
		SignInCliOnly  = aCliOnly;
		if (aCliOnly) {
			close_enterprise_server_draft();
		}
		SignInDialog->setWindowTitle(SignInHostname.empty() ? tr("Sign in") : tr("Sign in to %1").arg(QString::fromStdString(SignInHostname)));
		settings()->hide();
		SignInDialog->show();
		update_sign_in_surface();
		SignInDialog->raise();
		SignInDialog->activateWindow();
	}

	void workbench_window::open_settings(const settings_page aPage)
	{
		if (SignInDialog && SignInDialog->isVisible()) {
			SignInDialog->reject();
		}
		if (SettingsPresenter) {
			SettingsPresenter(aPage);
		} else if (SettingsWindow != nullptr) {
			SettingsWindow->present(aPage);
		}
	}

	auto workbench_window::settings() const noexcept -> settings_window*
	{
		return MountedSettings ? MountedSettings.data() : SettingsWindow;
	}

	auto workbench_window::keyboard_shortcuts_action() const noexcept -> QAction* { return KeyboardShortcutsAction; }

	void workbench_window::show_keyboard_shortcuts()
	{
		// Created on first use and kept: it reads a static table, and a
		// second press raises the same window rather than opening another.
		if (KeyboardShortcuts == nullptr) {
			KeyboardShortcuts = new keyboard_shortcuts_window{ this };
		}
		KeyboardShortcuts->show();
		KeyboardShortcuts->raise();
		KeyboardShortcuts->activateWindow();
	}

	auto workbench_window::keyboard_shortcuts() const noexcept -> keyboard_shortcuts_window* { return KeyboardShortcuts; }

	auto workbench_window::settings_action() const noexcept -> QAction*
	{
		return SettingsAction;
	}

	void workbench_window::dispatch_line_numbers_toggle()
	{
		// The widget never decides. Whatever Qt just did to the check mark is
		// undone by re-applying the projection, and the intent is what
		// eventually changes it.
		apply_line_numbers();
		if (!Dispatch) {
			return;
		}
		Dispatch(core::toggle_workbench_line_numbers_intent{ });
	}

	void workbench_window::apply_line_numbers()
	{
		const auto shown
			= LineNumberState.State == core::workbench_line_numbers::shown;
		LineNumbersAction->setChecked(shown);
		const auto projection
			= command(core::workbench_command_id::toggle_line_numbers);
		describe_action(
			LineNumbersAction,
			projection && projection->Enabled,
			shown
				? QStringLiteral(
					  "Hide the source line numbers and give the gutter's width "
					  "back to the code.")
				: QStringLiteral("Number each pane's own source lines."),
			drawer_reason_text(projection
								   ? projection->DisabledReason
								   : core::workbench_reason_code::no_active_workbench));
		if (ComparisonSurface != nullptr) {
			ComparisonSurface->set_line_numbers_visible(shown);
		}
	}

	void workbench_window::dispatch_one_pane_toggle()
	{
		apply_one_pane();
		if (Dispatch) {
			Dispatch(core::toggle_workbench_one_pane_intent{ });
		}
	}

	void workbench_window::apply_one_pane()
	{
		const auto enabled = OnePaneState.State == core::workbench_one_pane::enabled;
		OnePaneAction->setChecked(enabled);
		const auto projection = command(core::workbench_command_id::toggle_one_pane);
		describe_action(
			OnePaneAction,
			projection && projection->Enabled,
			enabled
				? QStringLiteral("Show the comparison in two source panes.")
				: QStringLiteral("Show both sides in one document."),
			drawer_reason_text(projection
								   ? projection->DisabledReason
								   : core::workbench_reason_code::no_active_workbench));
		if (ComparisonSurface != nullptr) {
			ComparisonSurface->set_one_pane_enabled(enabled);
		}
	}

	void workbench_window::dispatch_drawers_toggle()
	{
		// The action flips its own check when triggered; the core decides,
		// so what it last decided is shown again and only the gesture leaves.
		apply_drawers_toggle();
		if (Dispatch && ToggleDrawersAction->isEnabled()) {
			Dispatch(core::toggle_workbench_drawers_intent{ });
		}
	}

	void workbench_window::apply_drawers_toggle()
	{
		update_header_source_action();
		update_source_find_actions();
		if (ToggleDrawersAction == nullptr) {
			return;
		}
		const auto projection = command(core::workbench_command_id::toggle_drawers);
		// A new window's core is in Review before it has published a command,
		// and the drawers are the reviewer's then too ([[BPR-0675]]). Home has
		// no review, so nothing there to give the room to.
		const auto  offered   = !StripHome && (projection ? projection->Enabled : true);
		auto* const focused   = QApplication::focusWidget();
		const auto  composing = focused != nullptr && editable_multi_line(focused) && isAncestorOf(focused);
		const auto  enabled   = offered && !composing;
		const auto  open      = [](const core::workbench_drawer_state aState) {
			return aState == core::workbench_drawer_state::expanded;
		};
		const auto hidden = offered && !open(DrawerStates.Files) && !open(DrawerStates.Conversations)
			&& !open(DrawerStates.Repositories);
		ToggleDrawersAction->setChecked(hidden);
		// The tooltip names what Show brings back, left to right.
		auto names = QStringList{ };
		if (open(DrawerRestore.Drawers.Repositories)) {
			names.push_back(QStringLiteral("Repositories"));
		}
		if (open(DrawerRestore.Drawers.Files)) {
			names.push_back(QStringLiteral("Files"));
		}
		if (open(DrawerRestore.Drawers.Conversations)) {
			names.push_back(QStringLiteral("Comments"));
		}
		const auto last   = names.isEmpty() ? QStringLiteral("the drawers") : names.takeLast();
		const auto listed = names.isEmpty() ? last : QStringLiteral("%1 and %2").arg(names.join(QStringLiteral(", ")), last);
		const auto key    = ToggleDrawersAction->shortcut().toString(QKeySequence::NativeText);
		auto       name   = hidden ? QStringLiteral("Show %1").arg(listed) : QStringLiteral("Hide drawers");
		auto       reason = QStringLiteral("Open a pull request to hide and show its drawers.");
		if (offered && composing) {
			reason = QStringLiteral("The text you are writing keeps %1. Click the code first.").arg(key);
		} else if (projection && projection->DisabledReason == core::workbench_reason_code::overview_keeps_drawers_collapsed) {
			reason = QStringLiteral("Overview keeps every drawer closed");
		} else if (projection && projection->DisabledReason == core::workbench_reason_code::discuss_shows_the_conversation) {
			reason = QStringLiteral("In Discuss the thread is the page");
		} else if (projection && projection->DisabledReason == core::workbench_reason_code::threads_finds_a_thread) {
			reason = QStringLiteral("In Threads the finder is the page");
		}
		describe_action(ToggleDrawersAction, enabled, QStringLiteral("%1 (%2)").arg(name, key), reason);
		if (auto* const button = PrimaryToolbar != nullptr ? qobject_cast<QToolButton*>(PrimaryToolbar->widgetForAction(ToggleDrawersAction)) : nullptr;
			button != nullptr) {
			button->setAccessibleName(offered ? name : QStringLiteral("Hide drawers"));
			button->setAccessibleDescription(ToggleDrawersAction->toolTip());
		}
		// The glyph says the state: filled columns while the drawers show.
		if (ToggleDrawersAction->icon().isNull() || ToggleDrawersAction->property("bprDrawersHidden").toBool() != hidden) {
			ToggleDrawersAction->setProperty("bprDrawersHidden", hidden);
			ToggleDrawersAction->setIcon(strip_icon(hidden ? strip_glyph::drawers_hidden : strip_glyph::drawers_shown, palette()));
		}
	}

	auto workbench_window::drawer_holding_focus() const -> QWidget*
	{
		auto* const focused = QApplication::focusWidget();
		if (focused == nullptr) {
			return nullptr;
		}
		for (const auto& entry : Drawers) {
			if (entry.Panel != nullptr && entry.Panel->isAncestorOf(focused)) {
				return entry.Panel;
			}
		}
		return nullptr;
	}

	void workbench_window::keep_focus_out_of_hidden_drawers(QWidget* const aHeld)
	{
		// [[BPR-1295]]. Hiding the drawer that held focus would otherwise hand
		// it to whatever Qt finds next; the code is where the room went.
		if (aHeld == nullptr || aHeld->isVisible() || ComparisonSurface == nullptr) {
			return;
		}
		for (auto* const pane : { static_cast<QWidget*>(ComparisonSurface->right_pane()), static_cast<QWidget*>(ComparisonSurface->left_pane()) }) {
			if (pane != nullptr && pane->isVisible()) {
				pane->setFocus(Qt::OtherFocusReason);
				return;
			}
		}
		ComparisonSurface->setFocus(Qt::OtherFocusReason);
	}

	void workbench_window::dispatch_updates_line_toggle()
	{
		// Whatever Qt just did to the check mark or the arrow is undone by
		// showing the fold the core holds; the intent is what changes it.
		apply_updates_line();
		if (Dispatch) {
			Dispatch(core::toggle_workbench_updates_line_intent{ });
		}
	}

	void workbench_window::apply_updates_line()
	{
		const auto open    = updates_line_open();
		auto*      line    = EpochSelectors != nullptr ? EpochSelectors->line() : nullptr;
		const auto marked  = line != nullptr && line->history_marked();
		const auto history = line != nullptr ? line->statement() : QString{ };
		const auto purpose = open
			? QStringLiteral("Fold the Updates line; the review bar still names the comparison and its dates.")
			: QStringLiteral("Open the Updates line: the pull request's updates, with the compared two marked.");
		if (UpdatesLineAction != nullptr) {
			UpdatesLineAction->setChecked(open);
			UpdatesLineAction->setToolTip(purpose);
			UpdatesLineAction->setStatusTip(purpose);
		}
		if (UpdatesDisclosure != nullptr) {
			UpdatesDisclosure->set_open(open);
			UpdatesDisclosure->set_history_marked(marked);
			// The line's statement travels with the disclosure, so a folded
			// line still says whether its history is complete.
			const auto tip = history.isEmpty() ? purpose : QStringLiteral("%1\n%2").arg(purpose, history);
			if (UpdatesDisclosure->toolTip() != tip) {
				UpdatesDisclosure->setToolTip(tip);
				UpdatesDisclosure->setStatusTip(purpose);
			}
			UpdatesDisclosure->setAccessibleName(marked ? QStringLiteral("Updates, history incomplete") : QStringLiteral("Updates"));
		}
		if (EpochSelectors != nullptr && line != nullptr) {
			// Folded, the timeline leaves no row; the control stays only for
			// the review details it also holds.
			EpochSelectors->set_readiness(EpochSelectors->readiness_text(), open);
			const auto shown = open || EpochSelectors->details_visible();
			if (EpochSelectors->isHidden() == shown) {
				EpochSelectors->setVisible(shown);
			}
		}
	}

	void workbench_window::dispatch_drawer_toggle(const std::size_t aIndex)
	{
		// A checkable action flips itself the moment it is triggered. The core owns
		// drawer state, so the last decided state is re-asserted immediately and
		// only the gesture leaves; the window never shows a state it decided.
		update_action_state();
		if (!Dispatch) {
			return;
		}
		const auto drawer = [aIndex] {
			switch (aIndex) {
			case 0U:
				return core::workbench_drawer::files;
			case 1U:
				return core::workbench_drawer::conversations;
			default:
				return core::workbench_drawer::repositories;
			}
		}();
		Dispatch(core::toggle_workbench_drawer_intent{ .Drawer = drawer });
	}

	void workbench_window::dispatch_receipt(const std::size_t aIndex)
	{
		update_receipt_actions();
		if (!ReceiptDispatch || !Receipt.has_value()
			|| aIndex >= Receipt->Actions.size()
			|| !Receipt->Actions[aIndex].Enabled) {
			return;
		}
		++Statistics.ReceiptGestures;
		ReceiptDispatch(Receipt->Actions[aIndex].Command);
	}

	void workbench_window::observe_drawer_width(
		const core::workbench_drawer aDrawer,
		const std::uint32_t          aWidth)
	{
		// Geometry this window just applied is not a user resize. Reporting it
		// would let a width the window could merely grant overwrite the width the
		// host persisted.
		if (ApplyingLayout || !LayoutWidths) {
			return;
		}
		auto& retained = [this, aDrawer]() -> std::uint32_t& {
			switch (aDrawer) {
			case core::workbench_drawer::files:
				return LayoutWidths->FilesWidth;
			case core::workbench_drawer::conversations:
				return LayoutWidths->ConversationsWidth;
			case core::workbench_drawer::repositories:
				return LayoutWidths->RepositoriesWidth;
			}
			return LayoutWidths->FilesWidth;
		}();
		if (retained == aWidth) {
			return;
		}
		// The host owns and clamps the durable width. This local echo only keeps
		// one observed width from being reported twice before it is published back.
		retained = aWidth;
		if (WidthDispatch) {
			WidthDispatch(aDrawer, aWidth);
		}
	}

	void workbench_window::apply_drawer_layout()
	{
		ApplyingLayout = true;
		// [[BPR-0721]]. Whether a drawer shows its own controls is the
		// published preference's, applied before the state so a collapsed
		// drawer is presented the way the preference says.
		const auto controls = Behaviour.DrawerControls == application::review_drawer_controls::shown;
		auto       reshown  = false;
		for (auto& entry : Drawers) {
			reshown = reshown || (controls && !entry.Panel->controls_shown());
			entry.Panel->set_controls_shown(controls);
		}
		if (reshown) {
			// A panel that left the splitter comes back with the splitter's
			// record of the room it last had, which the rail cannot fill;
			// the sizes are set again so the centre takes the difference,
			// with the design's widths when the host has published none.
			CentralSplitter->refresh();
			if (!LayoutWidths) {
				CentralSplitter->setSizes(central_sizes());
			}
		}
		Drawers[0].Panel->adopt(presented_files_state());
		Drawers[1].Panel->adopt(DrawerStates.Conversations);
		Drawers[2].Panel->adopt(presented_repositories_state());
		// These full-content pages borrow the center width without changing the
		// retained drawer choices. Returning restores the same code arrangement.
		const auto fullPage = CompositionPage != nullptr || TourPageOpen
			|| (Modality != nullptr
				&& (Modality->current() == review_modality::discuss || Modality->current() == review_modality::threads));
		for (auto& entry : Drawers) {
			entry.Panel->setVisible(
				!fullPage
				&& (controls
					|| entry.Panel->state()
						== core::workbench_drawer_state::expanded));
		}
		ApplyingLayout = false;
		apply_drawer_widths();
		update_dif_presentation();
		// [[BPR-1212]]. A new comment that asked for the drawer opens in it.
		if (PendingDraft.has_value()) {
			QTimer::singleShot(0, this, [this] { resume_pending_draft(); });
		}
	}

	void workbench_window::apply_drawer_widths()
	{
		if (!LayoutWidths || ApplyingLayout) {
			return;
		}
		ApplyingLayout = true;
		// A layout from before the repositories drawer carries no width
		// for it. The design's 300 is what the window grants, echoed here
		// so the grant is not reported back as a resize.
		if (LayoutWidths->RepositoriesWidth == 0U) {
			LayoutWidths->RepositoriesWidth = 300U;
		}
		CentralSplitter->setSizes(central_sizes());
		ApplyingLayout = false;
	}

	void workbench_window::observe_handle_drag()
	{
		// [[BPR-0662]]. The handle moved under the reviewer's hold: each
		// expanded drawer's width is now theirs, and the host persists it.
		if (ApplyingLayout || !LayoutWidths) {
			return;
		}
		const auto sizes = CentralSplitter->sizes();
		if (sizes.size() != 4) {
			return;
		}
		const auto report = [this, &sizes](const int aIndex, const core::workbench_drawer aDrawer,
										   const core::workbench_drawer_state aState) {
			if (aState == core::workbench_drawer_state::expanded && sizes[aIndex] > 0) {
				observe_drawer_width(aDrawer, static_cast<std::uint32_t>(sizes[aIndex]));
			}
		};
		report(0, core::workbench_drawer::repositories, presented_repositories_state());
		report(1, core::workbench_drawer::files, presented_files_state());
		report(3, core::workbench_drawer::conversations, DrawerStates.Conversations);
		update_dif_presentation();
	}

	workbench_central_splitter::workbench_central_splitter(QWidget* const aParent)
		: QSplitter{ Qt::Horizontal, aParent }
	{
	}

	void workbench_central_splitter::move_handle(const int aIndex, const int aPosition)
	{
		moveSplitter(aPosition, aIndex);
	}

	auto workbench_central_splitter::createHandle() -> QSplitterHandle*
	{
		return new workbench_splitter_handle{ orientation(), this };
	}

	workbench_splitter_handle::workbench_splitter_handle(
		const Qt::Orientation aOrientation,
		QSplitter* const      aParent)
		: QSplitterHandle{ aOrientation, aParent }
	{
		// [[BPR-1330]]. The same cursor hold as the Overview summary seam.
		hold_resize_cursor(*this);
	}

	auto workbench_splitter_handle::rule_edge() const -> int
	{
		// Keep aimable space on both sides of the one-pixel rule. For an even
		// style width, choose the left middle column; odd widths are symmetric.
		return (width() - 1) / 2;
	}

	void workbench_splitter_handle::paintEvent(QPaintEvent* const aEvent)
	{
		QSplitterHandle::paintEvent(aEvent);
		auto painter = QPainter{ this };
		painter.fillRect(rect(), palette().color(QPalette::Active, QPalette::Window));
		// One constant weight, as the accepted asset draws it: the owner's
		// answer in the v009 attended session. See the class doc comment.
		painter.setPen(palette().color(QPalette::Active, QPalette::Mid));
		const auto edge = rule_edge();
		painter.drawLine(edge, 0, edge, height());
	}

	auto workbench_window::presented_files_state() const
		-> core::workbench_drawer_state
	{
		// The core decides the drawer while it offers the drawer command. When
		// it offers none -- no workbench is active -- the files drawer holds
		// the sign-in, the host chrome and the queue that would make one, so it
		// presents expanded: a rail the reviewer could not reopen would shut
		// the way in. The core's state is untouched and returns with the
		// command.
		const auto toggle = command(core::workbench_command_id::toggle_files_drawer);
		if (!toggle || !toggle->Enabled) {
			return core::workbench_drawer_state::expanded;
		}
		return DrawerStates.Files;
	}

	auto workbench_window::central_sizes() const -> QList<int>
	{
		if (CompositionPage != nullptr
			|| (Modality != nullptr
				&& (Modality->current() == review_modality::discuss || Modality->current() == review_modality::threads))) {
			return { 0, 0, CentralSplitter->width(), 0 };
		}
		const auto granted = [](const std::uint32_t aWidth, const std::uint32_t aDefault) {
			// An arithmetic guard on a host-owned value, not a second clamp: the
			// host already refuses to retain a width outside its own bound. A
			// width beneath the presented floor is a squeeze that was written
			// back, not a choice; the drawer opens at its design width.
			if (aWidth < minimum_presented_drawer_width) {
				return static_cast<int>(aDefault);
			}
			return static_cast<int>(
				std::min(aWidth, application::maximum_workbench_drawer_width));
		};
		const auto defaults = application::workbench_layout_settings{ };
		// Before the host publishes a layout the widths are the design's.
		const auto widths = LayoutWidths.value_or(application::workbench_layout_delta{ });
		// A collapsed drawer is its rail, so the rail's width is what it keeps;
		// [[BPR-0721]] with the drawer controls hidden it has left the
		// splitter, and neither it nor its handle takes any width.
		const auto collapsedWidth = Behaviour.DrawerControls == application::review_drawer_controls::shown
			? workbench_drawer_rail::width_pixels
			: 0;
		const auto files          = presented_files_state() == core::workbench_drawer_state::expanded
			? granted(widths.FilesWidth, defaults.FilesWidth)
			: collapsedWidth;
		const auto conversations
			= DrawerStates.Conversations == core::workbench_drawer_state::expanded
			? std::clamp(granted(widths.ConversationsWidth, conversation_drawer_default_width), conversation_drawer_minimum_width, conversation_drawer_maximum_width)
			: collapsedWidth;
		const auto repositories
			= presented_repositories_state() == core::workbench_drawer_state::expanded
			? granted(widths.RepositoriesWidth, defaults.RepositoriesWidth)
			: collapsedWidth;
		const auto handles = ((repositories > 0 ? 1 : 0) + (files > 0 ? 1 : 0) + (conversations > 0 ? 1 : 0))
			* CentralSplitter->handleWidth();
		const auto centre = std::max(
			1, CentralSplitter->width() - repositories - files - conversations - handles);
		return { repositories, files, centre, conversations };
	}

	void workbench_window::configure_tool_buttons()
	{
		for (const auto& entry : Navigation) {
			auto* const button = qobject_cast<QToolButton*>(
				ReviewBar->widgetForAction(entry.Action));
			if (button == nullptr) {
				continue;
			}
			button->setAccessibleName(entry.Accessible);
			button->setAccessibleDescription(entry.Purpose);
			button->setFocusPolicy(Qt::StrongFocus);
		}
		for (const auto& entry : Drawers) {
			auto* const button = qobject_cast<QToolButton*>(
				PrimaryToolbar->widgetForAction(entry.Toggle));
			if (button == nullptr) {
				continue;
			}
			button->setAccessibleName(entry.Accessible);
			button->setAccessibleDescription(entry.Collapse);
			button->setFocusPolicy(Qt::StrongFocus);
		}
		update_compose_mark();
		if (auto* const button = qobject_cast<QToolButton*>(
				PrimaryToolbar->widgetForAction(GithubRefreshAction));
			button != nullptr) {
			button->setAccessibleName(QStringLiteral("Refresh"));
			button->setAccessibleDescription(QStringLiteral(
				"Reload the open pull request from its host."));
			button->setFocusPolicy(Qt::StrongFocus);
		}
		if (auto* const button = qobject_cast<QToolButton*>(
				PrimaryToolbar->widgetForAction(HomeAction));
			button != nullptr) {
			button->setAccessibleName(QStringLiteral("Home"));
			button->setAccessibleDescription(HomeAction->toolTip());
			button->setFocusPolicy(Qt::StrongFocus);
		}
		if (auto* const button = qobject_cast<QToolButton*>(
				PrimaryToolbar->widgetForAction(SettingsAction));
			button != nullptr) {
			button->setAccessibleName(QStringLiteral("Settings"));
			button->setAccessibleDescription(QStringLiteral(
				"Open the Settings window."));
			button->setFocusPolicy(Qt::StrongFocus);
		}
		update_account_action();
		update_home_landing();
	}

	void workbench_window::refresh_toolbar_icons()
	{
		auto* const nativeStyle = style();
		if (nativeStyle == nullptr) {
			return;
		}
		// [[BPR-0646]]. The strip's glyphs are painted in the live palette's
		// ink, so a theme change repaints them here exactly as the arrows were.
		const auto& ink = palette();
		if (HomeAction != nullptr) {
			HomeAction->setIcon(strip_icon(strip_glyph::home, ink));
		}
#if defined(_WIN32)
		// [[BPR-1596]]. The menu control stands first in the Windows row; left
		// in the previous theme's ink it vanishes into the new ground and reads
		// as an empty space before Home.
		if (StripMenuButton != nullptr) {
			StripMenuButton->setIcon(windows_menu_icon(ink));
		}
#endif
		// [[BPR-1290]]. Next new reply's icon on the Comments tab row.
		if (NextNewCommentAction != nullptr) {
			NextNewCommentAction->setIcon(conversation_icon(conversation_glyph::next_new, ink));
		}
		Drawers[0].Toggle->setIcon(strip_icon(strip_glyph::files_drawer, ink));
		Drawers[2].Toggle->setIcon(strip_icon(strip_glyph::repositories_drawer, ink));
		Navigation[0].Action->setIcon(strip_icon(strip_glyph::previous_file, ink));
		Navigation[1].Action->setIcon(strip_icon(strip_glyph::next_file, ink));
		Navigation[2].Action->setIcon(strip_icon(strip_glyph::previous_change, ink));
		Navigation[3].Action->setIcon(strip_icon(strip_glyph::next_change, ink));
		ReviewDetailsAction->setIcon(strip_icon(strip_glyph::review_details, ink));
		// [[BPR-1331]]. The selected file's mark and the menu's marks, in the
		// new palette.
		refresh_progress_icons();
		if (ComposeReviewAction != nullptr && CommentAction != nullptr) {
			ComposeReviewAction->setIcon(strip_icon(strip_glyph::compose_review, ink));
			// The composition pages' Back and Restore follow the theme too.
			for (auto* const page : { ComposeReviewPage, ComposeInlinePage }) {
				if (page != nullptr) {
					for (auto* const back : page->findChildren<QPushButton*>(QString{ }, Qt::FindChildrenRecursively)) {
						if (back->objectName().endsWith(QStringLiteral("PageBack"))) {
							back->setIcon(conversation_icon(conversation_glyph::minimize, palette()));
						}
					}
				}
			}
			if (ReviewSheet != nullptr) {
				ReviewSheet->expand_button()->setIcon(conversation_icon(conversation_glyph::maximize, palette()));
				ReviewSheet->close_button()->setIcon(drawer_icon(drawer_glyph::close, palette()));
			}
			CommentAction->setIcon(strip_icon(strip_glyph::comment, ink));
		}
		// [[BPR-1338]]. One glyph for Open on GitHub wherever it is offered.
		if (OpenOnGithubAction != nullptr) {
			OpenOnGithubAction->setIcon(strip_icon(strip_glyph::open_on_github, ink));
			if (HomeList != nullptr) {
				HomeList->set_page_action(OpenOnGithubAction->icon(), OpenOnGithubAction->toolTip());
			}
		}
		Drawers[1].Toggle->setIcon(strip_icon(strip_glyph::comments_drawer, ink));
		if (ToggleDrawersAction != nullptr) {
			// Painted again in the new ink by the next projection's pass.
			ToggleDrawersAction->setIcon(QIcon{ });
			apply_drawers_toggle();
		}
		if (GithubRefreshAction != nullptr) {
			GithubRefreshAction->setIcon(strip_icon(strip_glyph::refresh, ink));
		}
		if (QueueRefreshAction != nullptr) {
			QueueRefreshAction->setIcon(strip_icon(strip_glyph::refresh, ink));
		}
		// [[BPR-1575]]. A refresh under way keeps its own look in the new ink.
		present_refresh();
		if (AccountAction != nullptr) {
			AccountAction->setIcon(strip_icon(strip_glyph::account, ink));
		}
		if (SettingsAction != nullptr) {
			SettingsAction->setIcon(strip_icon(strip_glyph::settings, ink));
		}
	}

	void workbench_window::set_progress_mark_colours(const progress_mark_colours& aColours)
	{
		ProgressMarkColours = aColours;
		refresh_progress_icons();
	}

	auto workbench_window::shown_progress_mark_colours() const -> progress_mark_colours
	{
		if (ProgressMarkColours.has_value()) {
			return *ProgressMarkColours;
		}
		const auto dark = palette().color(QPalette::Active, QPalette::Window).lightness() < 128;
		return builtin_progress_mark_colours(dark ? core::appearance_variant::dark : core::appearance_variant::light);
	}

	void workbench_window::refresh_progress_icons()
	{
		// [[BPR-1331]] v003. The owner rejected two pictures of one state
		// ("Pick one. I think the circles here is probably better overall but
		// it should get some color. All green aint the best either."), so the
		// button and its menu paint the same marks Overview and the Files
		// drawer do. The menu is the family's legend: each action shows the
		// mark it is about, Seen, Read, Accepted and Excluded, whatever its
		// label says now.
		const auto colours  = shown_progress_mark_colours();
		const auto disabled = palette().color(QPalette::Disabled, QPalette::ButtonText);
		if (ReviewControlsButton != nullptr) {
			ReviewControlsButton->setIcon(progress_mark_icon(shown_progress_mark(Receipt, LedgerUnavailable), colours, disabled));
		}
		// [[BPR-1578]]. The track's marks and fills follow the theme's colours.
		update_progress_track();
		if (ReviewControlsMenu == nullptr) {
			return;
		}
		constexpr auto marks = std::array{
			core::file_progress_mark::seen,
			core::file_progress_mark::read,
			core::file_progress_mark::accepted,
			core::file_progress_mark::excluded,
		};
		const auto& menu = ReviewControlsMenu->palette();
		for (std::size_t index = 0U; index < ReceiptActions.size() && index < marks.size(); ++index) {
			if (ReceiptActions[index] != nullptr) {
				ReceiptActions[index]->setIcon(progress_mark_icon(marks[index], colours, menu.color(QPalette::Disabled, QPalette::Text), menu.color(QPalette::Active, QPalette::Window)));
			}
		}
	}

	void workbench_window::apply_focus_order()
	{
		const auto order = focus_order();
		if (order.isEmpty()) {
			return;
		}
		for (auto index = qsizetype{ 1 }; index < order.size(); ++index) {
			auto* const previous = order[index - 1];
			auto* const next     = order[index];
			// setTabOrder moves its second argument. A stop inside a composed
			// region is left exactly where RFC-BPR-0003 and RFC-BPR-0004 put it,
			// so the shell orders its own chrome around those regions rather than
			// replacing their internal order with one it decided.
			if (EpochSelectors->isAncestorOf(next)
				|| ComparisonSurface->isAncestorOf(next)) {
				continue;
			}
			QWidget::setTabOrder(previous, next);
		}
		++Statistics.FocusOrderUpdates;
	}

	void workbench_window::update_action_state()
	{
		update_header_source_action();
		update_source_find_actions();
		// One projection for all four primary actions. Entry `i` of the core's
		// delta decides action `i`; the window adds no legality of its own and
		// keeps no second carrier for the change actions.
		for (std::size_t index = 0U; index < Navigation.size(); ++index) {
			const auto projection
				= command(static_cast<core::workbench_command_id>(index));
			const auto reason = projection
				? projection->DisabledReason
				: core::workbench_reason_code::no_active_workbench;
			// [[BPR-1328]]. An arrow at its bound stays enabled and names the
			// bound on the tooltip's second line.
			const auto enabled = projection && projection->Enabled;
			const auto shown   = enabled ? projection->Bound : reason;
			const auto detail  = index < file_navigation_entries ? reason_text(shown) : change_reason_text(shown);
			describe_navigation_action(Navigation[index].Action,
									   Navigation[index].Name,
									   enabled,
									   Navigation[index].Purpose,
									   detail);
		}
		describe_change_ends();

		update_drawer_action(0U,
							 core::workbench_command_id::toggle_files_drawer,
							 DrawerStates.Files);
		// The presented files state depends on whether the command is offered.
		if (Drawers[0].Panel->state() != presented_files_state()) {
			apply_drawer_layout();
		}
		update_drawer_action(
			1U,
			core::workbench_command_id::toggle_conversations_drawer,
			DrawerStates.Conversations);
		update_drawer_action(
			2U,
			core::workbench_command_id::toggle_repositories_drawer,
			DrawerStates.Repositories);
		if (Drawers[2].Panel->state() != presented_repositories_state()) {
			apply_drawer_layout();
		}
		apply_line_numbers();
		apply_one_pane();
		apply_drawers_toggle();
		apply_updates_line();
		update_receipt_actions();
		update_new_comment_group();
		// Last, and deliberately. The mode is not a second opinion about a
		// command's legality; it is the answer that stands once the core's
		// projections have had theirs.
		apply_public_preview_availability();
	}

	void workbench_window::update_accept_file_shortcut()
	{
		// [[BPR-1515]]. Accept's retained action becomes Revoke after
		// persistence, without this binding. Removing it while editing also
		// yields native menu equivalents without changing receipt eligibility.
		// [[BPR-1531]]. The key is Option-A (Alt+A), the owner's replacement
		// for Command-Shift-A. Option-A types "å", so the binding steps aside
		// for every widget that takes typed text, not only line and text
		// edits: the Cocoa plugin offers the key to the shortcut map before
		// the text input system sees it.
		auto* const       action  = ReceiptActions[2];
		const auto* const focused = QApplication::focusWidget();
		const auto        editing = focused != nullptr && isAncestorOf(focused) && takes_typed_text(focused);
		const auto        accepts = !Receipt || Receipt->Actions[2].Command.Kind == core::file_receipt_command_kind::accept;
		action->setShortcut(accepts && !editing ? accept_file_key() : QKeySequence{ });
		const auto key = action->shortcut().toString(QKeySequence::NativeText);
		action->setToolTip(key.isEmpty() ? action->statusTip()
										 : QStringLiteral("Accept file (%1)\n%2").arg(key, action->statusTip()));
		if (AcceptFileButton != nullptr) {
			AcceptFileButton->setAccessibleDescription(action->toolTip());
		}
	}

	void workbench_window::update_typing_keys()
	{
		const auto* const focused = QApplication::focusWidget();
		const auto        typing  = focused != nullptr && isAncestorOf(focused) && takes_typed_text(focused);
		if (typing == TypingFocus) {
			return;
		}
		TypingFocus = typing;
		update_accept_file_shortcut();
		update_show_thread_actions();
		if (!option_types_text || PrimaryMenuBar == nullptr) {
			return;
		}
		// [[BPR-1536]]. AppKit fires a native menu item's key equivalent
		// before Qt sees the key, with no ShortcutOverride ([[BPR-1295]]), so
		// the menu bar's Option+character keys are lifted while the keyboard
		// is in a widget that takes typed text, and restored after.
		if (!TypingFocus) {
			for (const auto& [action, keys] : LiftedOptionKeys) {
				if (action != nullptr) {
					action->setShortcuts(keys);
				}
			}
			LiftedOptionKeys.clear();
			return;
		}
		auto pending = QList<QAction*>{ PrimaryMenuBar->actions() };
		while (!pending.isEmpty()) {
			auto* const action = pending.takeLast();
			if (auto* const menu = action->menu(); menu != nullptr) {
				pending.append(menu->actions());
				continue;
			}
			const auto keys = action->shortcuts();
			auto       kept = QList<QKeySequence>{ };
			std::ranges::copy_if(keys, std::back_inserter(kept), [](const QKeySequence& aKey) {
				return aKey.count() != 1 || !option_character_key(aKey[0]);
			});
			if (kept.size() != keys.size()) {
				LiftedOptionKeys.emplace_back(action, keys);
				action->setShortcuts(kept);
			}
		}
	}

	auto workbench_window::presented_key(const QAction* const aAction) const -> QString
	{
		const auto lifted = std::ranges::find_if(LiftedOptionKeys, [aAction](const auto& aLifted) { return aLifted.first == aAction; });
		const auto key    = lifted != LiftedOptionKeys.end() && !lifted->second.isEmpty() ? lifted->second.front() : aAction->shortcut();
		return key.toString(QKeySequence::NativeText);
	}

	void workbench_window::update_receipt_actions()
	{
		const auto disabled = LedgerUnavailable
			? QStringLiteral("Private review progress is unavailable.")
			: QStringLiteral("Select an available text comparison first.");
		for (std::size_t index = 0U; index < ReceiptActions.size(); ++index) {
			auto* const action = ReceiptActions[index];
			if (action == nullptr) {
				continue;
			}
			const auto projected = Receipt.has_value()
					&& index < Receipt->Actions.size()
				? &Receipt->Actions[index]
				: nullptr;
			const auto kind      = projected != nullptr
				? projected->Command.Kind
				: index == 0U
				? core::file_receipt_command_kind::mark_seen
				: index == 1U
				? core::file_receipt_command_kind::mark_read
				: index == 2U
				? core::file_receipt_command_kind::accept
				: core::file_receipt_command_kind::exclude;
			const auto label     = [&] {
				switch (kind) {
				case core::file_receipt_command_kind::mark_seen:
					return QStringLiteral("&Seen");
				case core::file_receipt_command_kind::mark_read:
					return QStringLiteral("&Read");
				// [[BPR-1701]]. Seen's inverse, which takes Read off too,
				// and Read's, which leaves the file seen.
				case core::file_receipt_command_kind::mark_unread:
					return QStringLiteral("Mark u&nseen");
				case core::file_receipt_command_kind::unmark_read:
					return QStringLiteral("Mark &unread");
				case core::file_receipt_command_kind::accept:
					return QStringLiteral("&Accept");
				case core::file_receipt_command_kind::revoke_acceptance:
					return QStringLiteral("Revoke &acceptance");
				case core::file_receipt_command_kind::exclude:
					return QStringLiteral("E&xclude");
				case core::file_receipt_command_kind::include:
					return QStringLiteral("&Include");
				}
				return QStringLiteral("Private progress");
			}();
			action->setText(label);
			action->setIconText(QString{ label }.remove(QLatin1Char{ '&' }));
			const auto checked = Receipt.has_value()
				&& (index == 0U
						? Receipt->Receipt.Read != core::file_read_state::unread
						: index == 1U
						? Receipt->Receipt.Read == core::file_read_state::read
						: index == 2U ? Receipt->Receipt.Accepted
									  : Receipt->Receipt.Excluded);
			action->setChecked(checked);
			const auto purpose = [&] {
				switch (kind) {
				case core::file_receipt_command_kind::mark_seen:
					return QStringLiteral("Mark this file seen.");
				case core::file_receipt_command_kind::mark_unread:
					return QStringLiteral("Mark this file unseen. Read comes off with it.");
				case core::file_receipt_command_kind::mark_read:
					return QStringLiteral("Mark this file read.");
				case core::file_receipt_command_kind::unmark_read:
					return QStringLiteral("Mark this file unread. It stays seen.");
				case core::file_receipt_command_kind::accept:
					return QStringLiteral("Accept this file privately.");
				case core::file_receipt_command_kind::revoke_acceptance:
					return QStringLiteral("Revoke private acceptance for this file.");
				case core::file_receipt_command_kind::exclude:
				case core::file_receipt_command_kind::include:
					break;
				}
				return QStringLiteral("Update private progress for the selected exact file material.");
			}();
			describe_action(
				action,
				projected != nullptr && projected->Enabled && !LedgerUnavailable,
				purpose,
				index == 2U && Receipt.has_value() && Receipt->Receipt.Excluded && !LedgerUnavailable
					? QStringLiteral("Include this file from Private progress before accepting it.")
					: disabled);
		}
		update_accept_file_shortcut();
		if (AcceptFileButton != nullptr) {
			AcceptFileButton->setAccessibleName(ReceiptActions[2]->isChecked()
													? QStringLiteral("Revoke file acceptance")
													: QStringLiteral("Accept file"));
			AcceptFileButton->setAccessibleDescription(ReceiptActions[2]->toolTip());
		}
		// [[BPR-1129]] [[BPR-1331]]. The review bar's progress button shows the
		// selected file's mark, the one the core decided, as its icon, and says
		// it in its name and tooltip with the key and what a click offers. The
		// owner, 2026-10-08: "I think the toolbar for seen read accept needs to
		// update its icon to the current state".
		if (ReviewControlsButton != nullptr) {
			const auto mark  = shown_progress_mark(Receipt, LedgerUnavailable);
			auto       state = QString{ };
			auto       word  = QString{ };
			if (LedgerUnavailable) {
				state = QStringLiteral("Private review progress is unavailable.");
				word  = QStringLiteral("unavailable");
			} else if (!mark.has_value()) {
				state = QStringLiteral("No file comparison is selected.");
				word  = QStringLiteral("no file selected");
			} else {
				const auto& receipt = Receipt->Receipt;
				word                = QString::fromUtf8(core::name(*mark));
				auto facts          = QStringList{ QString::fromUtf8(core::name(receipt.Read)) };
				if (receipt.Accepted) {
					facts.push_back(QStringLiteral("accepted"));
				}
				if (receipt.Excluded) {
					facts.push_back(QStringLiteral("excluded"));
				}
				state = facts.size() == 1 ? QStringLiteral("This file is %1.").arg(facts.front())
										  : QStringLiteral("This file is %1 and %2.").arg(facts.mid(0, facts.size() - 1).join(QStringLiteral(", ")), facts.back());
			}
			// What a click offers: the actions the core enabled, by their current
			// names; with none enabled, the four the menu holds.
			auto offered = QStringList{ };
			auto every   = QStringList{ };
			for (auto* const action : ReceiptActions) {
				if (action == nullptr) {
					continue;
				}
				const auto label = QString{ action->text() }.remove(QLatin1Char{ '&' });
				every.push_back(label);
				if (action->isEnabled()) {
					offered.push_back(label);
				}
			}
			const auto listed = [](const QStringList& aLabels) {
				return aLabels.size() <= 1 ? aLabels.join(QString{ })
										   : QStringLiteral("%1 or %2").arg(aLabels.mid(0, aLabels.size() - 1).join(QStringLiteral(", ")), aLabels.back());
			};
			const auto click = offered.isEmpty() ? QStringLiteral("Click to open %1, none available now.").arg(listed(every))
												 : QStringLiteral("Click to choose %1.").arg(listed(offered));
			const auto key   = ReviewControlsButton->shortcut().toString(QKeySequence::NativeText);
			ReviewControlsButton->setIcon(progress_mark_icon(mark, shown_progress_mark_colours(), palette().color(QPalette::Disabled, QPalette::ButtonText)));
			ReviewControlsButton->setAccessibleName(QStringLiteral("Private progress: %1").arg(word));
			ReviewControlsButton->setToolTip(QStringLiteral("Private progress: %1 (%2)\n%3\n%4").arg(word, key, state, click));
			ReviewControlsButton->setStatusTip(state);
			ReviewControlsButton->setAccessibleDescription(QStringLiteral("%1 %2").arg(state, click));
		}
		update_progress_track();
	}

	auto workbench_window::progress_step_offered(const std::size_t aIndex) const -> bool
	{
		// [[BPR-1578]]. The command is the one the core projected for that
		// action, never one the track makes up. [[BPR-1701]]. The owner,
		// 2026-10-21: "In the toolbar I can uncheck accepted by clicking the
		// accepted button. I should be able to do the same for read and
		// seen." A step not reached checks it (Mark seen, Mark read); a step
		// reached unchecks it (Mark unseen, Mark unread), as Accept toggles to
		// Revoke. An excluded file still rests the track.
		if (LedgerUnavailable || !Receipt.has_value() || Receipt->Receipt.Excluded || aIndex >= Receipt->Actions.size()) {
			return false;
		}
		const auto& action  = Receipt->Actions[aIndex];
		const auto  kind    = action.Command.Kind;
		const auto  stepped = aIndex == progress_track::seen_step
			? kind == core::file_receipt_command_kind::mark_seen || kind == core::file_receipt_command_kind::mark_unread
			: kind == core::file_receipt_command_kind::mark_read || kind == core::file_receipt_command_kind::unmark_read;
		return action.Enabled && stepped;
	}

	void workbench_window::take_progress_step(const std::size_t aIndex)
	{
		if ((aIndex == progress_track::seen_step || aIndex == progress_track::read_step) && progress_step_offered(aIndex)) {
			dispatch_receipt(aIndex);
			return;
		}
		// A checkable step toggles itself on a click. The receipt decides its
		// checked state, so a click that dispatched nothing puts it back.
		update_progress_track();
	}

	void workbench_window::update_progress_track()
	{
		if (ProgressTrack == nullptr) {
			return;
		}
		auto shown = progress_track::state{ };
		if (Receipt.has_value() && !LedgerUnavailable) {
			const auto& receipt = Receipt->Receipt;
			shown.Excluded      = receipt.Excluded;
			shown.Live          = !receipt.Excluded;
			shown.Seen          = receipt.Read != core::file_read_state::unread;
			shown.Read          = receipt.Read == core::file_read_state::read;
			shown.Accepted      = receipt.Accepted;
		}
		ProgressTrack->set_state(shown, shown_progress_mark_colours());
		const auto unavailable = LedgerUnavailable ? QStringLiteral("Private review progress is unavailable.")
			: !Receipt.has_value()                 ? QStringLiteral("Select an available text comparison first.")
												   : QStringLiteral("Include this file from Private progress first.");
		const auto describe    = [&](QToolButton* const aStep, const std::size_t aIndex, const QString& aDone, const QString& aOffer) {
			if (aStep == nullptr) {
				return;
			}
			const auto offered = progress_step_offered(aIndex);
			const auto reached = aIndex == progress_track::seen_step ? shown.Seen : shown.Read;
			aStep->setEnabled(offered);
			// [[BPR-1701]]. Checked is the saved receipt's fact, so a
			// screen reader hears it too, and a click waits for persistence.
			aStep->setChecked(reached);
			const auto tip = !shown.Live ? unavailable : reached ? aDone
																 : aOffer;
			aStep->setToolTip(tip);
			aStep->setStatusTip(tip);
			aStep->setAccessibleDescription(tip);
		};
		describe(SeenStepButton,
				 progress_track::seen_step,
				 shown.Read ? QStringLiteral("This file is seen. Click to mark it unseen, which clears Read too.")
							: QStringLiteral("This file is seen. Click to mark it unseen."),
				 QStringLiteral("Mark this file seen."));
		describe(ReadStepButton, progress_track::read_step, QStringLiteral("This file is read. Click to mark it unread; it stays seen."), QStringLiteral("Mark this file read."));
	}

	void workbench_window::update_drawer_action(
		const std::size_t                  aIndex,
		const core::workbench_command_id   aId,
		const core::workbench_drawer_state aState)
	{
		const auto& entry      = Drawers[aIndex];
		const auto  projection = command(aId);
		const auto  expanded   = aState == core::workbench_drawer_state::expanded;
		entry.Toggle->setChecked(expanded);
		// [[BPR-0727]]. In Overview the toggle stays enabled, as the strip
		// requires, and says what the gesture will meet: the core keeps every
		// drawer collapsed until another modality is chosen.
		const auto enabledText = expanded ? entry.Collapse
			: current_modality() == review_modality::overview
			? reason_text(core::workbench_reason_code::overview_keeps_drawers_collapsed)
			: entry.Expand;
		// [[BPR-0675]]. A drawer's room is the reviewer's before the core has
		// published any command at all: a pull request whose comparison never
		// arrived publishes none, and the toggles were dead for exactly the
		// reviewer who most wanted the room back.
		describe_action(
			entry.Toggle,
			projection ? projection->Enabled : true,
			enabledText,
			drawer_reason_text(projection
								   ? projection->DisabledReason
								   : core::workbench_reason_code::no_active_workbench));
		if (auto* const button = qobject_cast<QToolButton*>(
				PrimaryToolbar->widgetForAction(entry.Toggle));
			button != nullptr) {
			button->setAccessibleDescription(entry.Toggle->toolTip());
		}
	}

	void workbench_window::update_status_text()
	{
		update_review_readiness();
		StatusLabel->setText(status_line());
		update_home_status();
		update_strip_positions();
		update_rail_summaries();
		update_home_landing();
	}

	void workbench_window::update_review_readiness()
	{
		if (EpochSelectors == nullptr || SessionPanel == nullptr) {
			return;
		}
		auto wording = QString{ };
		auto busy    = false;
		if (SessionPanel->status() == core::github_status::loading_pull_request) {
			busy       = true;
			auto stage = QStringLiteral("pull request details");
			if (ReadProgress) {
				switch (ReadProgress->Phase) {
				case application::github_read_phase::metadata:              break;
				case application::github_read_phase::files:                 stage = QStringLiteral("changed files"); break;
				case application::github_read_phase::commits:               stage = QStringLiteral("commits"); break;
				case application::github_read_phase::reviews:               stage = QStringLiteral("reviews"); break;
				case application::github_read_phase::review_threads:        stage = QStringLiteral("review threads"); break;
				case application::github_read_phase::comments:              stage = QStringLiteral("comments"); break;
				case application::github_read_phase::checks:                stage = QStringLiteral("checks"); break;
				case application::github_read_phase::related_pull_requests: stage = QStringLiteral("related pull requests"); break;
				case application::github_read_phase::update_history:        stage = QStringLiteral("update history"); break;
				}
			}
			wording = QStringLiteral("Loading %1…").arg(stage);
			if (ReadProgress && ReadProgress->PagesReceived != 0U) {
				wording += QStringLiteral(" %1 pages received.").arg(ReadProgress->PagesReceived);
			}
		} else if (HasGithubPullRequest) {
			const auto epochStatus = EpochSelectors->status();
			busy                   = HasEpochState && (epochStatus == core::epoch_status::loading_catalog || epochStatus == core::epoch_status::preparing || epochStatus == core::epoch_status::committing || epochStatus == core::epoch_status::resolving || epochStatus == core::epoch_status::maintaining);
			if (busy) {
				wording = QStringLiteral("Pull request loaded. Preparing file comparisons… %1").arg(EpochSelectors->status_text());
			} else if (EpochSelectors->comparison_available()) {
				wording = QStringLiteral("Ready to review. Select a file to load its comparison.");
			} else {
				wording = QStringLiteral("File comparisons unavailable. %1 Refresh to try again.").arg(EpochSelectors->status_text());
			}
		}
		EpochSelectors->set_readiness(busy || !EpochSelectors->comparison_available() ? wording : QString{ }, updates_line_open());
		// [[BPR-1576]]. A wait on the Updates line turns before its words.
		EpochSelectors->set_readiness_busy(busy, pending_full_motion());
		if (OverviewPage != nullptr) {
			OverviewPage->set_read_waiting(read_stage_words());
		}
	}

	auto workbench_window::status_line() const -> QString
	{
		// [[BPR-1157]]. A local cycle is not waiting for a pull request; the
		// cycle page says what it is doing itself.
		if (showing_local_cycle()) {
			return { };
		}
		auto status = QString{ };
		// [[BPR-0775]]. On Home the line is the session's, and never that no
		// pull request is open: Home is what the window shows when none is. A
		// host's own failure is still said after it.
		const auto home = showing_home();
		// [[BPR-0668]]. A cursor is a position in a comparison, and commands
		// are a workbench; neither is a pull request. Saying nothing was open
		// while the strip, the drawer and the conversations all named one told
		// the owner the product was broken and gave them nothing to do.
		if (!home && SessionPanel->status() == core::github_status::loading_pull_request) {
			status = EpochSelectors->readiness_text();
		} else if (!home && HasCachedOverview) {
			status = QStringLiteral("Cached review data; freshness unknown. Current review state is unavailable offline.");
		} else if (!home && HasGithubPullRequest && SessionPanel->status() == core::github_status::pull_request_failed) {
			// A failed refresh retains the accepted observation. State its
			// freshness beside the content, including when its diff is ready.
			status = QStringLiteral("Refresh failed. Showing previously loaded review data; freshness is unknown.");
		} else if (home) {
			status = home_session_line();
		} else if (HasGithubPullRequest && (!HasCommands || !Cursor)) {
			// [[BPR-0677]]. Work in progress is not failure. The owner'''s
			// comparison spent ninety-five seconds in revision_prepare while
			// the line told them to refresh, which would have thrown that
			// work away and started it again.
			const auto working = HasEpochState
				&& (EpochSelectors->status() == core::epoch_status::loading_catalog
					|| EpochSelectors->status() == core::epoch_status::preparing
					|| EpochSelectors->status() == core::epoch_status::committing
					|| EpochSelectors->status() == core::epoch_status::resolving
					|| EpochSelectors->status() == core::epoch_status::maintaining);
			status = working
				? QStringLiteral(
					  "This pull request is open. Its comparison is being prepared.")
				: QStringLiteral(
					  "This pull request is open. Its comparison is not ready; "
					  "Refresh to try again.");
		} else if (!HasCommands) {
			status = QStringLiteral("Open a pull request to begin reviewing.");
		} else if (!Cursor) {
			status = QStringLiteral("No pull request is open in the workbench.");
		} else if (Cursor->ActionableCount == 0U) {
			status = QStringLiteral("This pull request has no actionable files.");
		} else if (Cursor->Ordinal == 0U && !Cursor->File.has_value()) {
			// [[BPR-1287]]. Nothing is chosen, which is not a file outside the list.
			status = QStringLiteral("No file is open. Choose one of the %1 actionable file(s).")
						 .arg(Cursor->ActionableCount);
		} else if (Cursor->Ordinal == 0U) {
			status = QStringLiteral(
						 "The selected file is outside the %1 actionable file(s).")
						 .arg(Cursor->ActionableCount);
		} else {
			status = QStringLiteral("Actionable file %1 of %2.")
						 .arg(Cursor->Ordinal)
						 .arg(Cursor->ActionableCount);
		}
		if (Failure) {
			status.append(QStringLiteral(" %1").arg(failure_text(*Failure)));
		}
		// [[BPR-0742]]. Retained, so the projection that resets `Failure` does not
		// silence it; said once when a host failure already says the same.
		if (SavedLayoutUnavailable
			&& Failure != application::workbench_host_failure::settings_unavailable) {
			status.append(QStringLiteral(" %1").arg(failure_text(
				application::workbench_host_failure::settings_unavailable)));
		}
		// [[BPR-0777]]. A profile refusal is this window's to say on whatever it is
		// showing, because RFC-BPR-0033 routes it here instead of presenting the chooser.
		if (ProfileFailure) {
			status.append(QStringLiteral(" %1").arg(profile_management_failure_text(*ProfileFailure)));
		}
		// [[BPR-0817]]. Said on whatever the window shows, Home included, because
		// the agent panel, which also says it, is not showing at Home.
		if (AgentWorkspacesUnavailable) {
			status.append(QStringLiteral(" Disposable agent workspaces are unavailable: their folder needs repair or has reached its limit."));
		}
		if (home) {
			return status;
		}
		if (LedgerUnavailable) {
			status.append(QStringLiteral(" Private review progress is unavailable."));
		} else if (ReceiptFailure.has_value()) {
			status.append(QStringLiteral(" %1").arg(receipt_failure_text(*ReceiptFailure)));
		} else if (Receipt.has_value()) {
			status.append(QStringLiteral(" Private progress: %1, %2%3.")
							  .arg(Receipt->Receipt.Read == core::file_read_state::read
									   ? QStringLiteral("read")
									   : Receipt->Receipt.Read == core::file_read_state::seen
									   ? QStringLiteral("seen")
									   : QStringLiteral("unread"))
							  .arg(Receipt->Receipt.Accepted
									   ? QStringLiteral("accepted")
									   : QStringLiteral("not accepted"))
							  .arg(Receipt->Receipt.Excluded
									   ? QStringLiteral(", excluded")
									   : QString{ }));
		}
		if (HasEpochState) {
			// Quoted, never restated. Review-history loading, failure,
			// cancellation, and empty history are decided and worded by the
			// composed control; the shell repeats that exact sentence so its one
			// status line states the whole workbench state in text rather than in
			// enabled state or colour alone.
			const auto epochs = EpochSelectors->status_text();
			if (!epochs.isEmpty()) {
				status.append(QStringLiteral(" %1").arg(epochs));
			}
		}
		return status;
	}

	void workbench_window::announce_update()
	{
		const auto announcement = StatusLabel->text();
		if (announcement.isEmpty() || announcement == LastAnnouncement) {
			return;
		}
		LastAnnouncement = announcement;
		++Statistics.Announcements;
		QAccessibleAnnouncementEvent event{ this, announcement };
		event.setPoliteness(QAccessible::AnnouncementPoliteness::Polite);
		QAccessible::updateAccessibility(&event);
	}

	auto workbench_window::command(const core::workbench_command_id aId) const
		-> std::optional<core::workbench_command_projection>
	{
		if (!HasCommands) {
			return std::nullopt;
		}
		const auto index = static_cast<std::size_t>(aId);
		if (index >= Commands.Commands.size()
			|| Commands.Commands[index].Id != aId) {
			return std::nullopt;
		}
		return Commands.Commands[index];
	}
}
