#include "bpr/qt/review_tour_editor.hpp"

#include "bpr/qt/markdown_body_view.hpp"
#include "bpr/qt/review_cycle_view.hpp"
#include "bpr/qt/workbench_window.hpp"

#include "conversation_glyphs.hpp"
#include "github_presentation_text.hpp"

#include "bpr/qt/github_conversation_model.hpp"

#include "bpr/qt/menu_tool_button.hpp"

#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/device_authorization_panel.hpp"
#include "bpr/qt/dif_runtime.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/feedback_label_controls.hpp"
#include "bpr/qt/feedback_label_settings.hpp"
#include "bpr/qt/finding_queue_view.hpp"
#include "bpr/qt/home_list.hpp"
#include "bpr/qt/info_glyph.hpp"
#include "bpr/qt/native_window_chrome.hpp"
#include "bpr/qt/private_notes_panel.hpp"
#include "bpr/qt/private_review_panel.hpp"
#include "bpr/qt/profile_management_window.hpp"
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
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
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
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStatusBar>
#include <QStringList>
#include <QStyle>
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
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

namespace bpr::qt {
	namespace {
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

		/// \brief [[BPR-1200]]. A read's first two answered pages are its
		/// repository and its pull request: GitHub has shown both to this
		/// connection, so the read may leave Home for Overview.
		constexpr std::uint32_t minimum_answered_pages = 2U;

		/// \brief [[BPR-1134]]. The right tray's tabs, in order.
		constexpr int tray_comments_tab = 0;
		constexpr int tray_findings_tab = 1;

		/// \brief [[BPR-1134]]. The Comments tab's count of threads with new
		/// comments: a pill in the accent, painted from the palette.
		class tray_count_pill final : public QWidget
		{
		public:
			explicit tray_count_pill(QWidget* const aParent)
				: QWidget{ aParent }
			{
				setFocusPolicy(Qt::NoFocus);
				setAttribute(Qt::WA_TransparentForMouseEvents, true);
			}

			void set_count(const std::size_t aCount)
			{
				Text = aCount == 0U ? QString{ } : QString::number(aCount);
				setVisible(aCount != 0U);
				updateGeometry();
				update();
			}

			[[nodiscard]] auto text() const -> const QString&
			{
				return Text;
			}

			[[nodiscard]] auto sizeHint() const -> QSize override
			{
				const auto height = fontMetrics().height();
				return QSize{ std::max(height, fontMetrics().horizontalAdvance(Text) + height / 2 + 4), height };
			}

		protected:
			void paintEvent(QPaintEvent* const aEvent) override
			{
				static_cast<void>(aEvent);
				auto painter = QPainter{ this };
				painter.setRenderHint(QPainter::Antialiasing, true);
				painter.setPen(Qt::NoPen);
				painter.setBrush(palette().color(QPalette::Active, QPalette::Highlight));
				const auto box = QRectF{ rect() };
				painter.drawRoundedRect(box, box.height() / 2.0, box.height() / 2.0);
				painter.setPen(palette().color(QPalette::Active, QPalette::HighlightedText));
				painter.drawText(rect(), Qt::AlignCenter, Text);
			}

		private:
			QString Text;
		};

		/// \brief [[BPR-0775]]. Dif's resting size in the bottom-left corner, in logical pixels.
		///
		/// \details [[BPR-1209]]. Hand-back 49 asks for a smaller corner presence.
		/// One status-row height leaves the first-launch Home and workshop prime
		/// slots unchanged while reducing the corner illustration by thirty percent.
		constexpr auto dif_corner_size = 28;

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
		/// \brief [[BPR-1290]]. The tray's tab row: the tabs at the left and the
		/// Comments tab's tools at the right, or on a second row when the drawer
		/// is too narrow for both, so no tab scrolls out of reach.
		class tray_header final : public QWidget
		{
		public:
			explicit tray_header(QWidget* aParent)
				: QWidget{ aParent }
			{
				Grid = new QGridLayout{ this };
				Grid->setContentsMargins(0, 0, 6, 0);
				Grid->setHorizontalSpacing(1);
				Grid->setVerticalSpacing(0);
				Grid->setColumnStretch(1, 1);
			}

			void set_parts(QWidget* aTabs, QWidget* aTools)
			{
				Tabs  = aTabs;
				Tools = aTools;
				Grid->addWidget(Tabs, 0, 0, Qt::AlignBottom | Qt::AlignLeft);
				arrange();
			}

		protected:
			void resizeEvent(QResizeEvent* aEvent) override
			{
				QWidget::resizeEvent(aEvent);
				arrange();
			}

			auto event(QEvent* aEvent) -> bool override
			{
				if (aEvent->type() == QEvent::LayoutRequest || aEvent->type() == QEvent::FontChange) {
					arrange();
				}
				return QWidget::event(aEvent);
			}

		private:
			void arrange()
			{
				if (Tabs == nullptr || Tools == nullptr) {
					return;
				}
				const auto wrap = width() > 0 && Tabs->sizeHint().width() + Tools->sizeHint().width() + 8 > width();
				if (wrap == Wrapped && Grid->indexOf(Tools) >= 0) {
					return;
				}
				Wrapped = wrap;
				// Wrapped, the tabs take the whole row.
				Grid->setContentsMargins(0, 0, wrap ? 0 : 6, 0);
				Grid->removeWidget(Tools);
				if (wrap) {
					Grid->addWidget(Tools, 1, 0, 1, 3, Qt::AlignRight | Qt::AlignVCenter);
				} else {
					Grid->addWidget(Tools, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
				}
			}

			QGridLayout* Grid    = nullptr;
			QWidget*     Tabs    = nullptr;
			QWidget*     Tools   = nullptr;
			bool         Wrapped = false;
		};

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
#if defined(__APPLE__)
		/// \brief Width the macOS window buttons occupy at the strip's left.
		constexpr int traffic_light_inset = 78;
#endif

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
			case core::workbench_reason_code::at_first_file:
				return QStringLiteral("This is the first actionable file.");
			case core::workbench_reason_code::at_last_file:
				return QStringLiteral("This is the last actionable file.");
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
				return QStringLiteral(
					"This is the first change in this comparison.");
			case core::workbench_reason_code::at_last_change:
				return QStringLiteral(
					"This is the last change in this comparison.");
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
					"In Discuss the conversation is the page. Choose Review to "
					"hide or show the drawers.");
			case core::workbench_reason_code::conversations_finds_a_conversation:
				return QStringLiteral(
					"In Conversations the finder is the page. Choose Review to "
					"hide or show the drawers.");
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
			item->setToolTip(detail);
			item->setData(Qt::AccessibleDescriptionRole, detail);
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
		class editor_key_yield final : public QObject
		{
		public:
			editor_key_yield(QObject* const aParent, const QAction* const aAction)
				: QObject{ aParent }
				, Action{ aAction }
			{
			}

		protected:
			[[nodiscard]] auto eventFilter(QObject* const aWatched, QEvent* const aEvent) -> bool override
			{
				if ((aEvent->type() == QEvent::ShortcutOverride || aEvent->type() == QEvent::KeyPress)
					&& Action != nullptr && editable_multi_line(aWatched)) {
					const auto* const key = static_cast<QKeyEvent*>(aEvent);
					if (QKeySequence{ key->keyCombination() } == Action->shortcut()) {
						aEvent->accept();
						return true;
					}
				}
				return QObject::eventFilter(aWatched, aEvent);
			}

		private:
			QPointer<const QAction> Action;
		};

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

		const auto makeDrawer = [this](const QString&      aText,
									   const QString&      aName,
									   const QString&      aAccessible,
									   const QString&      aObjectName,
									   const QString&      aCollapse,
									   const QString&      aExpand,
									   const QKeySequence& aShortcut) -> drawer_entry {
			auto* const action = new QAction{ aText, this };
			action->setObjectName(aObjectName);
			action->setCheckable(true);
			action->setShortcut(aShortcut);
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
			QKeySequence{ QStringLiteral("Alt+Shift+1") });
		Drawers[1] = makeDrawer(
			// [[BPR-1293]]. The drawer is Comments, as its tab and title-row
			// toggle say; only the modality is called Conversations.
			QStringLiteral("&Comments drawer"),
			QStringLiteral("Comments drawer"),
			QStringLiteral("Comments drawer toggle"),
			QStringLiteral("toggleWorkbenchConversationsDrawerAction"),
			QStringLiteral("Hide the comments drawer and give its width to the comparison."),
			QStringLiteral("Show the comments drawer at its saved width."),
			QKeySequence{ QStringLiteral("Alt+Shift+2") });
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
			QKeySequence{ QStringLiteral("Alt+Shift+R") });

		const auto make = [this](const QString&      aText,
								 const QString&      aName,
								 const QString&      aAccessible,
								 const QString&      aObjectName,
								 const QString&      aPurpose,
								 const QKeySequence& aShortcut) -> navigation_entry {
			auto* const action = new QAction{ aText, this };
			action->setObjectName(aObjectName);
			action->setShortcut(aShortcut);
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
							 QKeySequence{ QStringLiteral("Alt+Up") });
		Navigation[1] = make(QStringLiteral("&Next file"),
							 QStringLiteral("Next file"),
							 QStringLiteral("Next file"),
							 QStringLiteral("nextWorkbenchFileAction"),
							 QStringLiteral("Select the next actionable file."),
							 QKeySequence{ QStringLiteral("Alt+Down") });
		Navigation[2] = make(QStringLiteral("Pre&vious change"),
							 QStringLiteral("Previous change"),
							 QStringLiteral("Previous change"),
							 QStringLiteral("previousWorkbenchChangeAction"),
							 QStringLiteral("Select the previous semantic change."),
							 QKeySequence{ QStringLiteral("Alt+PgUp") });
		Navigation[3] = make(QStringLiteral("Ne&xt change"),
							 QStringLiteral("Next change"),
							 QStringLiteral("Next change"),
							 QStringLiteral("nextWorkbenchChangeAction"),
							 QStringLiteral("Select the next semantic change."),
							 QKeySequence{ QStringLiteral("Alt+PgDown") });

		for (std::size_t index = 0U; index < Navigation.size(); ++index) {
			connect(Navigation[index].Action,
					&QAction::triggered,
					this,
					[this, index] { dispatch_navigation(index); });
		}
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
		ReviewDetailsAction->setShortcut(
			QKeySequence{ QStringLiteral("Alt+Shift+3") });
		ReviewDetailsAction->setShortcutContext(Qt::WindowShortcut);
		ReviewDetailsAction->setToolTip(QStringLiteral(
			"Show the exact review anchors and the selected-change details."));
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
		receiptAction(3U,
					  QStringLiteral("E&xclude"),
					  QStringLiteral("toggleSelectedFileExclusionAction"),
					  QStringLiteral("Change private exclusion for the selected file."));

		ReviewControlsButton = new menu_tool_button{ this };
		ReviewControlsButton->setObjectName(QStringLiteral("workbenchReviewControlsButton"));
		ReviewControlsButton->setText(QStringLiteral("Private &progress"));
		ReviewControlsButton->setShortcut(QKeySequence{ QStringLiteral("Alt+Shift+4") });
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
		// their first control; nothing elsewhere. A widget item rather than a
		// margin, because the toolbar layout lays its items from its own edge.
		StripInset = new QWidget{ PrimaryToolbar };
		StripInset->setObjectName(QStringLiteral("workbenchStripInset"));
		StripInset->setFocusPolicy(Qt::NoFocus);
#if defined(__APPLE__)
		StripInset->setFixedWidth(traffic_light_inset);
#else
		StripInset->setFixedWidth(0);
#endif
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
		// from any review, checked while Home shows.
		HomeAction = new QAction{ QStringLiteral("&Home"), this };
		HomeAction->setObjectName(QStringLiteral("workbenchHomeAction"));
		HomeAction->setCheckable(true);
		HomeAction->setShortcut(QKeySequence{ QStringLiteral("Ctrl+0") });
		HomeAction->setShortcutContext(Qt::WindowShortcut);
		HomeAction->setIconText(QStringLiteral("Home"));
		HomeAction->setToolTip(QStringLiteral(
			"Show Home: sign-in or the review queue, and the way back to the review."));
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
		// [[BPR-0656]]. Next new comment keeps its key and its window
		// ownership; [[BPR-1129]] its count rides on the comments toggle and its
		// presenter leaves the title row.
		NextNewCommentAction = new QAction{ QStringLiteral("Next new &comment"), this };
		NextNewCommentAction->setObjectName(QStringLiteral("nextNewCommentAction"));
		NextNewCommentAction->setIconText(QStringLiteral("Next new comment"));
		NextNewCommentAction->setToolTip(QStringLiteral("Next new comment"));
		NextNewCommentAction->setStatusTip(QStringLiteral(
			"Reveal the next thread with a post newer than what you have read."));
		NextNewCommentAction->setShortcut(QKeySequence{ QStringLiteral("Alt+N") });
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
		FilePositionAction = rowWidget(
			ReviewBar, FilePositionLabel, QStringLiteral("workbenchStripFilePositionAction"));
		ReviewBar->addAction(Navigation[0].Action);
		ReviewBar->addAction(Navigation[1].Action);
		ChangePositionLabel = rowLabel(
			ReviewBar,
			QStringLiteral("workbenchStripChangePosition"),
			QStringLiteral("Change position"),
			QStringLiteral("The selected change's position among the comparison's changes"));
		ChangePositionAction = rowWidget(
			ReviewBar, ChangePositionLabel, QStringLiteral("workbenchStripChangePositionAction"));
		ReviewBar->addAction(Navigation[2].Action);
		ReviewBar->addAction(Navigation[3].Action);
		ReviewBar->set_navigation_rails(FilePositionAction, Navigation[1].Action, ChangePositionAction, Navigation[3].Action);
		rowWidget(ReviewBar, ReviewControlsButton, QStringLiteral("workbenchReviewControlsAction"));
		rowSpacer(ReviewBar, QStringLiteral("reviewBarSpacer"), QStringLiteral("reviewBarSpacerAction"));
		// [[BPR-1129]]. The two composition commands are the window's, so their
		// keys work from anywhere in it without a visible presenter; the bar and
		// the Pull Request menu present them. They keep the object names the
		// composition pages and their tests already know.
		const auto makeComposition = [this](const QString& aText, const QString& aName, const QString& aKey, const bool aInline) {
			auto* const action = new QAction{ aText, this };
			action->setObjectName(aName);
			action->setIconText(action->text().remove(QLatin1Char{ '&' }));
			action->setShortcut(QKeySequence{ aKey });
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
		ComposeReviewAction = makeComposition(tr("Compose &review"), QStringLiteral("composeReviewAction"), QStringLiteral("Alt+R"), false);
		CommentAction       = makeComposition(tr("Co&mment"), QStringLiteral("composeInlineDraftAction"), QStringLiteral("Alt+C"), true);
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
		PrepareTourAction->setShortcut(QKeySequence{ QStringLiteral("Alt+T") });
		PrepareTourAction->setEnabled(false);
		addAction(PrepareTourAction);
		PullRequestMenu->addAction(PrepareTourAction);
		connect(PrepareTourAction, &QAction::triggered, this, [this] { open_review_tour(); });
		AddTourStopAction = new QAction{ tr("Add tour stop"), this };
		AddTourStopAction->setObjectName(QStringLiteral("addTourStopAction"));
		AddTourStopAction->setShortcut(QKeySequence{ QStringLiteral("Alt+Shift+T") });
		AddTourStopAction->setEnabled(false);
		addAction(AddTourStopAction);
		PullRequestMenu->addAction(AddTourStopAction);
		connect(PrepareTourAction, &QAction::changed, this, [this] { AddTourStopAction->setEnabled(PrepareTourAction->isEnabled()); });
		connect(AddTourStopAction, &QAction::triggered, this, [this] { TourEditor->trigger(tour_editor_action::add_stop); });
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
			"Resizable file drawer, comparison centre, and conversation drawer"));
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
		for (auto* pane : { ComparisonSurface->left_pane(), ComparisonSurface->right_pane() }) {
			pane->setContextMenuPolicy(Qt::CustomContextMenu);
			connect(pane, &QWidget::customContextMenuRequested, this, [this, pane](const QPoint& aPosition) {
				auto* menu = pane->createStandardContextMenu();
				menu->setAttribute(Qt::WA_DeleteOnClose);
				menu->addSeparator();
				menu->addAction(AddTourStopAction);
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
		OverviewPage->set_entry_dispatch(
			[this](const application::overview_entry_kind aEntry, const QString& aPath) {
				enter_overview_at(aEntry, aPath);
			});
		OverviewPage->set_file_dispatch([this](const QString& aPath) { enter_review_at(aPath); });
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
		ComparisonSurface->set_thread_tick_dispatch([this](const core::diff_side aSide, const std::uint32_t aLine) { go_to_thread_at(aSide, aLine); });
		ConversationPanel->cards()->set_marks_observer([this] {
			apply_thread_marks();
			present_reply_placement();
		});
		ConversationPanel->cards()->set_scroll_forward([this](QWheelEvent& aEvent) {
			return ComparisonSurface != nullptr && ComparisonSurface->scroll_by_wheel(aEvent);
		});
		ConversationPanel->cards()->set_reply_text_dispatch([this](const std::string& aThread, const QString& aText) {
			write_inline_reply(aThread, aText);
		});
		// [[BPR-0661]]. A file marks itself read once fully seen only when the
		// preference says so; the shared scrollbar is what says "fully".
		if (auto* const scrollbar = ComparisonSurface->shared_scrollbar(); scrollbar != nullptr) {
			connect(scrollbar, &QScrollBar::valueChanged, this, [this](int) { consider_mark_read_when_fully_seen(); });
			connect(scrollbar, &QScrollBar::rangeChanged, this, [this](int, int) { consider_mark_read_when_fully_seen(); });
		}
		ConversationPanel->cards()->set_action_dispatch(
			[this](const card_action aAction, const std::string& aIdentity) {
				// [[BPR-1046]]. A reply still waiting for its comparison is
				// given up by any later card gesture.
				DeferredReply.reset();
				switch (aAction) {
				case card_action::defect:
					ConversationPanel->cards()->show_thread_menu(aIdentity);
					break;
				case card_action::publish_draft:
					if (ReviewEditor != nullptr) {
						ReviewEditor->publish_action()->trigger();
					}
					break;
				case card_action::edit_draft:
					if (ReviewEditor != nullptr) {
						open_review_composition(true);
					}
					break;
				case card_action::delete_draft:
					if (ReviewEditor != nullptr) {
						ReviewEditor->discard_action()->trigger();
					}
					break;
				case card_action::mark_read:
					// [[BPR-0656]]. The ledger host advances the cursor and
					// the read state comes back through present_read_state.
					if (ConversationReadDispatch) {
						ConversationReadDispatch(aIdentity);
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
					// [[BPR-1133]]. Beside the code as in Discuss: every card
					// answers its own thread, through the one Reply path;
					// [[BPR-1290]] written in place, in the open thread.
					reply_to_thread(aIdentity, true);
					break;
				case card_action::show_line:
					reveal_thread_line(aIdentity);
					break;
				case card_action::publish_reply:
					// [[BPR-1290]]. The reply in place is the draft editor's
					// draft; nothing publishes but its own Publish.
					if (ReviewEditor != nullptr && ReviewEditor->publish_action()->isEnabled()) {
						ReviewEditor->publish_action()->trigger();
					}
					break;
				case card_action::cancel_reply:
					// Cancel gives up what was typed, through the editor's own Discard.
					if (ReviewEditor != nullptr && ReviewEditor->shown_draft().has_value()
						&& ReviewEditor->shown_draft()->ReplyToThread.has_value()
						&& !ReviewEditor->body_editor()->toPlainText().isEmpty()
						&& ReviewEditor->discard_action()->isEnabled()) {
						ReviewEditor->discard_action()->trigger();
					}
					break;
				case card_action::open_in_code: {
					// [[BPR-0649]]. Back to the code at the thread's line: the
					// file the thread names is selected through the same
					// intent the files drawer sends, Review is entered in the
					// arrangement it last had ([[BPR-1295]]), and the thread
					// made current so the line is revealed.
					const auto index = ConversationPanel->model()->index_for_id(aIdentity);
					if (index.isValid()) {
						ConversationPanel->choose(index);
						enter_review_at(index.data(github_conversation_model::path_role).toString());
						synchronize_selected_conversation(index);
					}
					break;
				}
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
				// [[BPR-1293]]. So is Conversations: the arrows move the
				// comparison underneath, not the finder's cursor.
				if (Modality != nullptr
					&& (Modality->current() == review_modality::discuss || Modality->current() == review_modality::conversations)) {
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
		});
		connect(ReviewEditor->body_editor(), &QPlainTextEdit::textChanged, this, [this] { present_inline_reply(); });
		connect(ReviewEditor->publish_action(), &QAction::changed, this, [this] { present_inline_reply(); });
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
			tr("Classify selected conversation as defect"), this
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
		auto* discussLayout = new QVBoxLayout{ DiscussPage };
		discussLayout->setContentsMargins(0, 0, 0, 0);
		CentreStack->addWidget(DiscussPage);
		// [[BPR-1293]]. Conversations: the finder fills the centre, as
		// Discuss's page does, and the drawers are hidden while it shows.
		ConversationsPage = new QWidget{ CentreStack };
		ConversationsPage->setObjectName(QStringLiteral("workbenchConversationsPage"));
		auto* const conversationsLayout = new QVBoxLayout{ ConversationsPage };
		conversationsLayout->setContentsMargins(0, 0, 0, 0);
		Finder = new conversation_finder{ ConversationsPage };
		conversationsLayout->addWidget(Finder);
		// The drawers are hidden while it shows, so the page always has the
		// window's whole width; as a page of the stack it must not widen the
		// centre's floor while Review shows its drawers.
		ConversationsPage->setMinimumSize(1, 1);
		CentreStack->addWidget(ConversationsPage);
		Finder->set_pick_dispatch([this](const finder_location& aLocation) {
			static_cast<void>(open_conversation_in_review(aLocation.Thread, aLocation.Post));
		});
		Finder->set_leave_dispatch([this] { leave_conversations(); });
		const auto makeCompositionPage = [this](QWidget* aEditor, const QString& aName) {
			auto* scroll = new QScrollArea{ CentreStack };
			scroll->setObjectName(aName);
			scroll->setWidgetResizable(true);
			scroll->setFrameShape(QFrame::NoFrame);
			scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			auto* page   = new QWidget{ scroll };
			auto* column = new QVBoxLayout{ page };
			auto* back   = new QPushButton{ tr("Back"), page };
			back->setObjectName(aName + QStringLiteral("Back"));
			back->setAccessibleDescription(tr("Return to the previous surface without submitting"));
			connect(back, &QPushButton::clicked, this, [this] { close_review_composition(); });
			// [[BPR-1129]]. Esc is Back, from anywhere on the page.
			auto* const escape = new QShortcut{ QKeySequence{ Qt::Key_Escape }, scroll };
			escape->setObjectName(aName + QStringLiteral("Escape"));
			escape->setContext(Qt::WidgetWithChildrenShortcut);
			connect(escape, &QShortcut::activated, this, [this] { close_review_composition(); });
			column->addWidget(back, 0, Qt::AlignLeft);
			column->addWidget(aEditor, 1);
			scroll->setWidget(page);
			CentreStack->addWidget(scroll);
			return scroll;
		};
		ComposeReviewPage = makeCompositionPage(ReviewSubmissionEditor, QStringLiteral("composeReviewPage"));
		ComposeInlinePage = makeCompositionPage(ReviewEditor, QStringLiteral("composeInlineDraftPage"));
		// Keep the decision and its illustration visible while the before/after
		// scrolls. Mascot preferences must never hide these semantic controls.
		WorkshopHeader = new QWidget{ CentreSurface };
		WorkshopHeader->setObjectName(QStringLiteral("workbenchWorkshopHeader"));
		auto* workshopEscape = new QShortcut{ QKeySequence{ Qt::Key_Escape }, WorkshopHeader };
		workshopEscape->setContext(Qt::WidgetWithChildrenShortcut);
		connect(workshopEscape, &QShortcut::activated, this, [this] { close_review_composition(); });
		auto* workshopHeader = new QHBoxLayout{ WorkshopHeader };
		workshopHeader->addWidget(new QLabel{ tr("Comment workshop"), WorkshopHeader });
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
		DifWorkshopSlot = new QWidget{ WorkshopHeader };
		DifWorkshopSlot->setObjectName(QStringLiteral("workbenchDifWorkshopSlot"));
		DifWorkshopSlot->setAccessibleName(tr("Decorative Dif workshop illustration"));
		DifWorkshopSlot->setFixedSize(64, 64);
		auto* workshopIllustration = new QHBoxLayout{ DifWorkshopSlot };
		workshopIllustration->setContentsMargins(0, 0, 0, 0);
		workshopHeader->addWidget(DifWorkshopSlot);
		WorkshopHeader->hide();
		centre->insertWidget(centre->indexOf(CentreStack), WorkshopHeader);
		// [[BPR-1129]]. The kept-draft mark follows what the workflow presents.
		ReviewSubmissionEditor->set_presentation_observer([this] { update_compose_mark(); });
		// [[BPR-1085]]. A preflight row links back to its exact item: my unsent
		// draft opens its page; a conversation is revealed in Discuss.
		ReviewSubmissionEditor->set_item_reveal([this](const std::string& aIdentity) {
			if (aIdentity.starts_with("draft:")) {
				open_review_composition(true);
				return;
			}
			close_review_composition();
			static_cast<void>(reveal_conversation(aIdentity));
		});
		auto* const notesPage       = new QWidget{ Drawers[1].Panel };
		auto* const notesPageLayout = new QVBoxLayout{ notesPage };
		notesPageLayout->setContentsMargins(0, 0, 0, 0);
		auto* const notesPurpose = new QLabel{ tr("Private to you and never published. Write a note or ask an agent about this pull request, then promote what you want to say to a draft in Comments."), notesPage };
		notesPurpose->setObjectName(QStringLiteral("workbenchNotesPurpose"));
		notesPurpose->setTextFormat(Qt::PlainText);
		notesPurpose->setWordWrap(true);
		notesPurpose->setAccessibleName(tr("About Notes"));
		notesPurpose->setAccessibleDescription(notesPurpose->text());
		notesPurpose->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
		notesPageLayout->addWidget(notesPurpose);
		PrivateNotes = new private_notes_panel{ conversationContents };
		findingsLayout->addWidget(PrivateNotes);
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
		auto* privateTabs = new QTabWidget{ notesPage };
		privateTabs->setObjectName(QStringLiteral("privateConversationTabs"));
		privateTabs->setAccessibleName(tr("Private history and agent tools"));
		PrivateReview = new private_review_panel{ notesPage };
		privateTabs->addTab(makeTabColumn(PrivateReview, QStringLiteral("privateHistoryScroll")), tr("Private history"));
		privateTabs->addTab(ConversationScroll, tr("Agent tools"));
		notesPageLayout->addWidget(privateTabs, 1);

		// [[BPR-1134]]. The tray's header is its tabs: Comments, then Notes
		// and Tour. The drawer still
		// opens on Comments. Choosing Notes is RFC-BPR-0031's explicit move
		// into the AI tools, the action that cues compare_glance in the corner; it
		// dispatches no agent or review action and never opens the drawer.
		auto* const conversationShell       = new QWidget{ Drawers[1].Panel };
		auto* const conversationShellLayout = new QVBoxLayout{ conversationShell };
		conversationShellLayout->setContentsMargins(0, 0, 0, 0);
		conversationShellLayout->setSpacing(0);
		TrayTabs = new QTabBar{ conversationShell };
		TrayTabs->setObjectName(QStringLiteral("workbenchTrayTabs"));
		TrayTabs->setAccessibleName(QStringLiteral("Right tray"));
		TrayTabs->setAccessibleDescription(QStringLiteral(
			"Comments, Notes and Tour. Notes and tour preparation are private until explicitly published."));
		TrayTabs->setDocumentMode(true);
		TrayTabs->setExpanding(false);
		TrayTabs->setDrawBase(true);
		TrayTabs->setFocusPolicy(Qt::StrongFocus);
		TrayTabs->addTab(tr("Comments"));
		TrayTabs->addTab(tr("Notes"));
		TrayTabs->addTab(tr("Tour"));
		TrayTabs->setAccessibleTabName(2, tr("Tour"));
		TrayTabs->setAccessibleTabName(1, tr("Notes"));
		TrayTabs->setTabToolTip(0, tr("The pull request's conversations, with their scope and filters"));
		TrayTabs->setTabToolTip(1, tr("Your private notes, AI conversations and review feedback. Nothing here is published; promote an item to a draft in Comments to publish it."));
		TrayCommentsCount = new tray_count_pill{ TrayTabs };
		TrayCommentsCount->setObjectName(QStringLiteral("workbenchTrayCommentsCount"));
		TrayCommentsCount->hide();
		TrayTabs->setTabButton(0, QTabBar::RightSide, TrayCommentsCount);
		TrayPages = new QStackedWidget{ conversationShell };
		TrayPages->setObjectName(QStringLiteral("workbenchTrayPages"));
		TrayPages->addWidget(CommentsScroll);
		TrayPages->addWidget(notesPage);
		TourEditor = new review_tour_editor{ conversationShell };
		TrayPages->addWidget(makeTabColumn(TourEditor, QStringLiteral("workbenchTourScroll")));
		// [[BPR-1290]]. The tab row carries the Comments tab's tools at its
		// right: Only new, Hide resolved and Next new reply.
		auto* const trayHeader = new tray_header{ conversationShell };
		trayHeader->setObjectName(QStringLiteral("workbenchTrayHeader"));
		CommentsTools = new QWidget{ trayHeader };
		CommentsTools->setObjectName(QStringLiteral("workbenchCommentsTools"));
		auto* const commentsToolsRow = new QHBoxLayout{ CommentsTools };
		commentsToolsRow->setContentsMargins(0, 0, 0, 0);
		commentsToolsRow->setSpacing(1);
		// [[BPR-1293]]. The finder button, as the prototype draws it at the
		// top of the drawer: it goes to Conversations.
		ConversationsFinderButton = new QToolButton{ CommentsTools };
		ConversationsFinderButton->setObjectName(QStringLiteral("workbenchConversationsFinderButton"));
		ConversationsFinderButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
		ConversationsFinderButton->setAutoRaise(true);
		ConversationsFinderButton->setIconSize(QSize{ 16, 16 });
		ConversationsFinderButton->setFocusPolicy(Qt::StrongFocus);
		ConversationsFinderButton->setIcon(conversation_icon(conversation_glyph::finder, palette()));
		ConversationsFinderButton->setAccessibleName(QStringLiteral("Find a conversation in the pull request"));
		ConversationsFinderButton->setToolTip(QStringLiteral("Find a conversation in the pull request (%1)").arg(QKeySequence{ QStringLiteral("Ctrl+J") }.toString(QKeySequence::NativeText)));
		ConversationsFinderButton->setAccessibleDescription(ConversationsFinderButton->toolTip());
		connect(ConversationsFinderButton, &QToolButton::clicked, this, [this] { toggle_conversations(); });
		commentsToolsRow->addWidget(ConversationsFinderButton);
		commentsToolsRow->addWidget(ConversationPanel->read_tools());
		NextNewCommentButton->setParent(CommentsTools);
		commentsToolsRow->addWidget(NextNewCommentButton);
		trayHeader->set_parts(TrayTabs, CommentsTools);
		conversationShellLayout->addWidget(trayHeader);
		conversationShellLayout->addWidget(TrayPages, 1);
		AiToolsAction = new QAction{ tr("Notes"), this };
		AiToolsAction->setObjectName(QStringLiteral("showWorkbenchAiToolsAction"));
		AiToolsAction->setCheckable(true);
		AiToolsAction->setToolTip(
			tr("Your private notes, AI conversations and review feedback. Nothing here is published; promote an item to a draft in Comments to publish it."));
		connect(TrayTabs, &QTabBar::currentChanged, this, [this](const int aIndex) {
			TrayPages->setCurrentIndex(aIndex);
			CommentsTools->setVisible(aIndex == tray_comments_tab);
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
			HomeFocusBarAction->setShortcut(QKeySequence{ Qt::CTRL | Qt::Key_L });
			HomeFocusBarAction->setShortcutContext(Qt::WindowShortcut);
			addAction(HomeFocusBarAction);
			connect(HomeFocusBarAction, &QAction::triggered, this, [this] { focus_home_bar(); });
			auto* const openShortcut = new QLabel{
				HomeFocusBarAction->shortcut().toString(QKeySequence::NativeText), openFrame
			};
			openShortcut->setObjectName(QStringLiteral("homeOpenShortcut"));
			openShortcut->setForegroundRole(QPalette::PlaceholderText);
			openShortcut->setAccessibleName(QStringLiteral("Shortcut to the Open bar"));
			auto* const frameRow = new QHBoxLayout{ openFrame };
			frameRow->setContentsMargins(12, 8, 12, 8);
			frameRow->setSpacing(8);
			frameRow->addWidget(HomeOpenLabel);
			frameRow->addWidget(HomeOpenBar, 1);
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
			auto* const openRow = new QHBoxLayout{ };
			openRow->setContentsMargins(24, 16, 24, 0);
			openRow->addWidget(openFrame, 1);
			homeColumn->addLayout(openRow);
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
			auto* const noticeRow = new QHBoxLayout{ };
			noticeRow->setContentsMargins(24, 2, 24, 0);
			noticeRow->addWidget(HomeOpenNotice, 1);
			noticeRow->addWidget(HomeOpenDetails, 0, Qt::AlignTop);
			homeColumn->addLayout(noticeRow);
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
						!HomeOpenBar->text().trimmed().isEmpty());
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
			auto* const recentList = new home_recent_view{ HomePage };
			recentList->hide();
			HomeRecentList = recentList;
			HomeRecentList->setObjectName(QStringLiteral("homeRecentList"));
			HomeRecentList->setAccessibleName(QStringLiteral("Recently viewed pull requests"));
			HomeRecentList->setAccessibleDescription(QStringLiteral(
				"Return opens a pull request on its overview; Shift and Return resume its saved reading"));
			// The action is the row's menu as well as its hover and focus
			// button, so the gesture is where the row is.
			HomeRecentList->setContextMenuPolicy(Qt::ActionsContextMenu);
			HomeRecentList->addAction(HomeRecentResumeAction);
			HomeRecentList->installEventFilter(this);
			connect(HomeRecentList, &QListWidget::itemActivated, this,
					[this](QListWidgetItem* const aItem) {
						dispatch_recent_review(
							aItem == nullptr ? -1 : HomeRecentList->row(aItem), false);
					});
			connect(HomeRecentList, &QListWidget::currentRowChanged, this,
					[this](int) { update_recent_resume_action(); });
			connect(HomeRecentResumeAction, &QAction::triggered, this, [this] {
				dispatch_recent_review(HomeRecentList->currentRow(), true);
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
			HomeCyclesList->setMaximumHeight(HomeCyclesList->fontMetrics().lineSpacing() * 5 + 8);
			HomeCyclesList->hide();
			cyclesColumn->addWidget(HomeCyclesList);
			homeColumn->addWidget(HomeCycles);
			connect(openProject, &QToolButton::clicked, this, [this] {
				if (OpenProjectAction != nullptr) {
					OpenProjectAction->trigger();
				}
			});
			connect(HomeCyclesList, &QListWidget::itemActivated, this, [this](QListWidgetItem* const aItem) {
				const auto row = aItem == nullptr ? -1 : HomeCyclesList->row(aItem);
				if (row >= 0 && static_cast<std::size_t>(row) < CycleRecents.size() && ResumeCycleDispatch) {
					ResumeCycleDispatch(CycleRecents[static_cast<std::size_t>(row)].Cycle.Subject);
				}
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
			if (HomeList != nullptr) {
				HomeList->adopt_recent(*recentList);
				HomeList->set_recent_action_dispatch([this](const int aRow) { dispatch_recent_review(aRow, true); });
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
		GithubConnectAction->setShortcut(
			QKeySequence{ QStringLiteral("Ctrl+Shift+C") });
		GithubConnectAction->setShortcutContext(Qt::ApplicationShortcut);
		addAction(GithubConnectAction);
		GithubOpenAction = new QAction{
			QStringLiteral("&Open pull request"), this
		};
		GithubOpenAction->setObjectName(QStringLiteral("openPullRequestAction"));
		GithubOpenAction->setShortcut(QKeySequence::Open);
		GithubOpenAction->setShortcutContext(Qt::ApplicationShortcut);
		addAction(GithubOpenAction);
		GithubRefreshAction = new QAction{ QStringLiteral("&Refresh"), this };
		GithubRefreshAction->setObjectName(
			QStringLiteral("refreshPullRequestAction"));
		GithubRefreshAction->setShortcut(QKeySequence::Refresh);
		GithubRefreshAction->setShortcutContext(Qt::ApplicationShortcut);
		addAction(GithubRefreshAction);
		// [[BPR-0990]]. Home's refresh glyph is the profile's discovery, not
		// the pull request's refresh: the same place in the strip, its own
		// action, shown only on Home with an account. It has no shortcut of
		// its own; the window's Refresh shortcut stays the pull request's.
		QueueRefreshAction = new QAction{ QStringLiteral("Refresh my pull requests"), this };
		QueueRefreshAction->setObjectName(QStringLiteral("refreshQueueAction"));
		QueueRefreshAction->setIconText(QStringLiteral("Refresh"));
		QueueRefreshAction->setVisible(false);
		QueueRefreshAction->setEnabled(false);
		addAction(QueueRefreshAction);
		connect(QueueRefreshAction, &QAction::triggered, this, [this] {
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
		// [[BPR-0652]]. Reviews & queue: the observer rules, and a plain
		// statement of what the page will hold once its authorities exist.
		auto* const reviews = new QWidget;
		reviews->setObjectName(QStringLiteral("workbenchReviewsPage"));
		reviews->setAccessibleName(QStringLiteral("Reviews and queue"));
		auto* const reviewsColumn = new QVBoxLayout{ reviews };
		reviewsColumn->setContentsMargins(0, 0, 0, 0);
		auto* const reviewsSubtitle = new QLabel{
			QStringLiteral("Local behaviour only. Nothing here changes assignments or subscriptions on GitHub."),
			reviews
		};
		reviewsSubtitle->setObjectName(QStringLiteral("reviewsSubtitle"));
		reviewsSubtitle->setWordWrap(true);
		reviewsColumn->addWidget(reviewsSubtitle);
		build_behaviour_editor(reviews, reviewsColumn);
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
		// [[BPR-0659]]. Saved repositories: the ones the drawer keeps, per
		// the account the queue session names.
		auto* const savedHeading = new QLabel{ QStringLiteral("Saved repositories"), reviews };
		savedHeading->setObjectName(QStringLiteral("reviewsSavedHeading"));
		savedHeading->setFont(headingFont);
		reviewsColumn->addWidget(savedHeading);
		build_saved_repositories_editor(reviews, *reviewsColumn);
		SettingsWindow->adopt_panel(settings_page::reviews_and_queue, reviews);
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
			FilesFilterNotice = new filter_empty_notice{
				QStringLiteral("files"), QStringLiteral("files"), filesSurface
			};
			FilesFilterNotice->hide();
			surfaceColumn->addWidget(FilesFilterNotice);
			FilesTree = new files_tree_view{ filesSurface };
			surfaceColumn->addWidget(FilesTree, 1);
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
			Drawers[0].Panel->host_beside_list(filesSurface, FilesTree);
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

		// [[BPR-0646]]. Refresh sits in the strip's right region, before the
		// comments toggle, as the design places it. Same action as the
		// session chrome's button, so there is one refresh with one state.
		PrimaryToolbar->insertAction(Drawers[1].Toggle, GithubRefreshAction);
		PrimaryToolbar->insertAction(Drawers[1].Toggle, QueueRefreshAction);
		if (auto* const glyph = qobject_cast<QToolButton*>(PrimaryToolbar->widgetForAction(QueueRefreshAction)); glyph != nullptr) {
			glyph->setFocusPolicy(Qt::StrongFocus);
			glyph->setAccessibleName(QStringLiteral("Refresh my pull requests"));
		}
		// [[BPR-0783]]. Home's own controls sit before the refresh glyph, so the
		// strip reads Group, State headers, Filter, refresh, the divider, account
		// and settings.
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
		HomeDiscoveryStatus = new QLabel{ surface };
		HomeDiscoveryStatus->setObjectName(QStringLiteral("homeDiscoveryStatus"));
		HomeDiscoveryStatus->setAccessibleName(QStringLiteral("My pull requests status"));
		HomeDiscoveryStatus->setWordWrap(true);
		HomeDiscoveryStatus->setVisible(false);
		layout->addWidget(HomeDiscoveryStatus);

		QueueLanding     = new review_landing_model{ surface };
		QueueFilterModel = new review_landing_filter_model{ *QueueLanding, surface };
		// [[BPR-0650]] [[BPR-0776]]. Home's list: a panel per state, in state
		// order, over the same decided queue, and Recently viewed last. A
		// click or Return on a row opens its pull request on Overview, as a
		// row of the repositories tree does; an empty queue's sentence links
		// to Reviews & queue.
		HomeList = new qt::home_list{ *QueueFilterModel, surface };
		// The built-in state colours until the composition hands over the
		// profile it resolved.
		HomeList->set_state_colours(home_state_colours_for(appearance_profile{ }));
		HomeList->set_open_dispatch([this](const core::queue_item& aItem) { open_pull_request_item(aItem); });
		HomeList->set_settings_dispatch([this] { open_settings(settings_page::reviews_and_queue); });
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
		// Reviews & queue, adopted there once the Settings window exists.
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
		SettingsWindow->adopt_panel(settings_page::storage_and_privacy, storage);
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
		// Home is shown and an account can fill its list, Group, State headers,
		// Filter and the refresh glyph stand before the right divider, the account
		// and settings. Signed out they act on nothing, so they are absent rather
		// than disabled, and the review strip is unchanged.
		HomeGroupMenu = new QMenu{ this };
		HomeGroupMenu->setObjectName(QStringLiteral("homeGroupMenu"));
		HomeGroupMenu->setAccessibleName(QStringLiteral("Group Home's pull requests"));
		HomeGroupMenu->setAccessibleDescription(QStringLiteral(
			"None, Organization, Repository or Account; the state order stays beneath every grouping"));
		const auto groupings = std::array{
			std::pair{ application::home_grouping::none, QStringLiteral("None") },
			std::pair{ application::home_grouping::organization, QStringLiteral("Organization") },
			std::pair{ application::home_grouping::repository, QStringLiteral("Repository") },
			std::pair{ application::home_grouping::account, QStringLiteral("Account") },
		};
		for (const auto& [value, title] : groupings) {
			auto* const action = HomeGroupMenu->addAction(title);
			action->setObjectName(QStringLiteral("homeGroupBy%1Action").arg(title));
			action->setCheckable(true);
			action->setData(static_cast<int>(value));
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
		HomeGroupButton = new menu_tool_button{ this };
		HomeGroupButton->setObjectName(QStringLiteral("homeGroupButton"));
		HomeGroupButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
		HomeGroupButton->setPopupMode(QToolButton::InstantPopup);
		HomeGroupButton->setMenu(HomeGroupMenu);
		HomeGroupButton->setAccessibleName(QStringLiteral("Group"));
		Strip->adopt_button(HomeGroupButton);
		HomeGroupItem = PrimaryToolbar->insertWidget(GithubRefreshAction, HomeGroupButton);
		HomeGroupItem->setObjectName(QStringLiteral("homeGroupAction"));

		HomeStateHeaders = new QAction{ QStringLiteral("State headers"), this };
		HomeStateHeaders->setObjectName(QStringLiteral("homeStateHeadersAction"));
		HomeStateHeaders->setCheckable(true);
		HomeStateHeaders->setChecked(true);
		HomeStateHeaders->setIconText(QStringLiteral("State headers"));
		HomeStateHeaders->setToolTip(QStringLiteral(
			"Show each state as its own panel. With it off the rows sit in state order with a state mark."));
		HomeStateHeaders->setStatusTip(HomeStateHeaders->toolTip());
		addAction(HomeStateHeaders);
		PrimaryToolbar->insertAction(GithubRefreshAction, HomeStateHeaders);
		if (auto* const button = qobject_cast<QToolButton*>(PrimaryToolbar->widgetForAction(HomeStateHeaders));
			button != nullptr) {
			button->setToolButtonStyle(Qt::ToolButtonTextOnly);
			button->setAccessibleName(QStringLiteral("State headers"));
			button->setAccessibleDescription(HomeStateHeaders->toolTip());
			button->setFocusPolicy(Qt::StrongFocus);
		}
		connect(HomeStateHeaders, &QAction::toggled, this, [this](const bool aChecked) {
			if (ApplyingHomeView) {
				return;
			}
			HomeView.StateHeaders = aChecked;
			apply_home_view();
			dispatch_home_view();
		});

		build_home_filter_menu();
		HomeFilterButton = new menu_tool_button{ this };
		HomeFilterButton->setObjectName(QStringLiteral("homeFilterButton"));
		HomeFilterButton->setToolButtonStyle(Qt::ToolButtonTextOnly);
		HomeFilterButton->setPopupMode(QToolButton::InstantPopup);
		HomeFilterButton->setMenu(HomeFilterMenu);
		HomeFilterButton->setAccessibleName(QStringLiteral("Filter"));
		Strip->adopt_button(HomeFilterButton);
		HomeFilterItem = PrimaryToolbar->insertWidget(GithubRefreshAction, HomeFilterButton);
		HomeFilterItem->setObjectName(QStringLiteral("homeFilterAction"));
		apply_home_view();
	}

	void workbench_window::build_home_filter_menu()
	{
		// [[BPR-0783]] Decisions (c). Exactly the four typed preferences that
		// survive as Home filters, each section listing every value of its
		// vocabulary in the board's word for it. Repository and Bucket are not
		// here: Group and the state order do their job.
		HomeFilterMenu = new QMenu{ this };
		HomeFilterMenu->setObjectName(QStringLiteral("homeFilterMenu"));
		HomeFilterMenu->setAccessibleName(QStringLiteral("Filter Home's pull requests"));
		HomeFilterMenu->setAccessibleDescription(QStringLiteral(
			"My role, Added by, Quiet items and Sort within a group; nothing here changes GitHub"));
		// [[BPR-0783]] review finding. Each section is the one exclusive choice the
		// board draws, not four independent check boxes: an item cannot be
		// unticked into nothing, and choosing one unticks its siblings.
		auto sections = std::array<QActionGroup*, 4U>{ };
		for (auto& section : sections) {
			section = new QActionGroup{ this };
			section->setExclusive(true);
		}
		const auto item = [this, &sections](const std::size_t aSection,
											const QString&    aTitle,
											const int         aValue,
											auto              aGesture) {
			auto* const action = HomeFilterMenu->addAction(aTitle);
			action->setObjectName(QStringLiteral("homeFilter%1Action").arg(QString{ aTitle }.remove(QLatin1Char{ ' ' })));
			action->setCheckable(true);
			action->setData(aValue);
			sections[aSection]->addAction(action);
			HomeFilterSections[aSection].push_back(action);
			connect(action, &QAction::triggered, this, [this, aGesture] {
				if (ApplyingHomeView) {
					return;
				}
				aGesture();
			});
		};

		HomeFilterMenu->addSection(QStringLiteral("My role"));
		for (const auto& [value, title] : std::array{
				 std::pair{ std::optional<core::github_role>{ }, QStringLiteral("Any") },
				 std::pair{ std::optional{ core::github_role::author }, QStringLiteral("Author") },
				 std::pair{ std::optional{ core::github_role::reviewer }, QStringLiteral("Reviewer") },
				 std::pair{ std::optional{ core::github_role::requested_reviewer }, QStringLiteral("Requested") },
				 std::pair{ std::optional{ core::github_role::participant }, QStringLiteral("Participant") },
				 std::pair{ std::optional{ core::github_role::none }, QStringLiteral("No role") } }) {
			item(0U, title, value.has_value() ? static_cast<int>(*value) : -1, [this, value] {
				auto filter = home_filter();
				filter.Role = value;
				choose_home_filter(application::queue_preference_field::role, filter);
			});
		}

		HomeFilterMenu->addSection(QStringLiteral("Added by"));
		for (const auto& [value, title] : std::array{
				 std::pair{ std::optional<core::observer_rule_kind>{ }, QStringLiteral("Any") },
				 std::pair{ std::optional{ core::observer_rule_kind::repository }, QStringLiteral("Repository rule") },
				 std::pair{ std::optional{ core::observer_rule_kind::author }, QStringLiteral("Author rule") },
				 std::pair{ std::optional{ core::observer_rule_kind::path }, QStringLiteral("Path rule") } }) {
			item(1U, title, value.has_value() ? static_cast<int>(*value) : -1, [this, value] {
				auto filter    = home_filter();
				filter.AddedBy = value;
				choose_home_filter(application::queue_preference_field::observer, filter);
			});
		}

		HomeFilterMenu->addSection(QStringLiteral("Quiet items"));
		for (const auto& [value, title] : std::array{
				 std::pair{ application::queue_quiet_filter::attention, QStringLiteral("Hide") },
				 std::pair{ application::queue_quiet_filter::all, QStringLiteral("Show") },
				 std::pair{ application::queue_quiet_filter::quiet, QStringLiteral("Only quiet") } }) {
			item(2U, title, static_cast<int>(value), [this, value] {
				auto filter  = home_filter();
				filter.Quiet = value;
				choose_home_filter(application::queue_preference_field::quiet, filter);
			});
		}

		HomeFilterMenu->addSection(QStringLiteral("Sort within a group"));
		for (const auto& [value, title] : std::array{
				 std::pair{ application::queue_sort_order::core_priority, QStringLiteral("Priority") },
				 std::pair{ application::queue_sort_order::repository, QStringLiteral("Repository") },
				 std::pair{ application::queue_sort_order::pull_request_number, QStringLiteral("Pull request number") } }) {
			item(3U, title, static_cast<int>(value), [this, value] {
				auto filter = home_filter();
				filter.Sort = value;
				choose_home_filter(application::queue_preference_field::sort, filter);
			});
		}

		HomeFilterMenu->addSeparator();
		// The sentence carries an ampersand, which a menu would otherwise eat as
		// a mnemonic (`menu_literal`); the wording here is the one the reviewer
		// reads.
		auto* const footer = HomeFilterMenu->addAction(menu_literal(QStringLiteral(
			"Nothing here changes GitHub. Saved repositories and observer rules are in Settings › Reviews & queue.")));
		footer->setObjectName(QStringLiteral("homeFilterFooter"));
		footer->setEnabled(false);
		// The footer says where they are; this is the way there, by keyboard as
		// well as by pointer.
		HomeFilterSettings = HomeFilterMenu->addAction(menu_literal(QStringLiteral("Open Reviews & queue")));
		HomeFilterSettings->setObjectName(QStringLiteral("homeFilterSettingsAction"));
		HomeFilterSettings->setToolTip(QStringLiteral(
			"Opens Settings, Reviews & queue, where saved repositories and observer rules are kept."));
		HomeFilterSettings->setStatusTip(HomeFilterSettings->toolTip());
		connect(HomeFilterSettings, &QAction::triggered, this, [this] {
			open_settings(settings_page::reviews_and_queue);
		});
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

	void workbench_window::apply_home_view()
	{
		ApplyingHomeView = true;
		if (HomeGroupButton != nullptr) {
			auto* const chosen = HomeGroupActions[static_cast<std::size_t>(HomeView.Grouping)];
			const auto  name   = chosen != nullptr ? chosen->text() : QStringLiteral("None");
			// The board's own words, so the strip says what it is doing.
			HomeGroupButton->setText(QStringLiteral("Group: %1").arg(name));
			const auto purpose = QStringLiteral("Grouping Home by %1. None, Organization, Repository or Account.")
									 .arg(name.toLower());
			HomeGroupButton->setAccessibleDescription(purpose);
			HomeGroupButton->setToolTip(purpose);
			for (auto index = std::size_t{ 0U }; index < HomeGroupActions.size(); ++index) {
				if (HomeGroupActions[index] != nullptr) {
					HomeGroupActions[index]->setChecked(index == static_cast<std::size_t>(HomeView.Grouping));
				}
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
					chosen = action->text();
				}
			}
			return chosen;
		};
		const auto role    = shown(0U, filter.Role.has_value() ? static_cast<int>(*filter.Role) : -1);
		const auto addedBy = shown(1U, filter.AddedBy.has_value() ? static_cast<int>(*filter.AddedBy) : -1);
		const auto quiet   = shown(2U, static_cast<int>(filter.Quiet));
		const auto sort    = shown(3U, static_cast<int>(filter.Sort));
		if (HomeFilterButton != nullptr) {
			// A filter that is not the default is said without colour, and the
			// four effective values are what the control is described as.
			const auto filtered = filter != application::home_filter{ };
			HomeFilterButton->setText(filtered ? QStringLiteral("Filter •") : QStringLiteral("Filter"));
			const auto purpose = QStringLiteral("%1My role %2. Added by %3. Quiet items %4. Sort %5.")
									 .arg(filtered ? QStringLiteral("Filtered. ") : QString{ }, role, addedBy, quiet, sort);
			HomeFilterButton->setAccessibleDescription(purpose);
			HomeFilterButton->setToolTip(purpose);
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

	auto workbench_window::home_group_button() const noexcept -> QToolButton*
	{
		return HomeGroupButton;
	}

	auto workbench_window::home_group_menu() const noexcept -> QMenu*
	{
		return HomeGroupMenu;
	}

	auto workbench_window::home_group_action(const application::home_grouping aGrouping) const noexcept -> QAction*
	{
		return HomeGroupActions[static_cast<std::size_t>(aGrouping)];
	}

	auto workbench_window::home_state_headers_action() const noexcept -> QAction*
	{
		return HomeStateHeaders;
	}

	auto workbench_window::home_filter_button() const noexcept -> QToolButton*
	{
		return HomeFilterButton;
	}

	auto workbench_window::home_filter_menu() const noexcept -> QMenu*
	{
		return HomeFilterMenu;
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
		// something the panels do not: restoring, unavailable, a filter that
		// hides every row, a limit. The count is the panels' headers and an
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
		const auto rulesChanged        = QueueRules != aProjection.Rules;
		const auto serversChanged      = QueueProjection.SavedServers != aProjection.SavedServers;
		const auto repositoriesChanged = QueueProjection.SavedRepositories != aProjection.SavedRepositories;

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
			.Status = aProjection.Status, .Rules = aProjection.Rules, .Preferences = aProjection.Preferences, .Session = aProjection.Session, .Online = aProjection.Online, .BackoffSeconds = aProjection.BackoffSeconds, .SavedRepositories = aProjection.SavedRepositories, .SavedServers = aProjection.SavedServers, .Failure = aProjection.Failure, .Discovery = aProjection.Discovery
		};
		QueueRules = aProjection.Rules;
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
		if (SavedRepositorySelector != nullptr && repositoriesChanged) {
			const auto selected = SavedRepositorySelector->currentText();
			SavedRepositorySelector->clear();
			for (const auto& saved : aProjection.SavedRepositories) {
				SavedRepositorySelector->addItem(QString::fromStdString(saved.Repository));
			}
			if (const auto restored = SavedRepositorySelector->findText(selected); restored >= 0) {
				SavedRepositorySelector->setCurrentIndex(restored);
			}
		}
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
		update_enterprise_server_actions();
		update_discovery_controls();
		update_pull_request_queue_actions();
		apply_focus_order();
	}

	void workbench_window::build_saved_repositories_editor(QWidget* const aParent, QVBoxLayout& aLayout)
	{
		SavedRepositoriesEditor = new QWidget{ aParent };
		SavedRepositoriesEditor->setObjectName(QStringLiteral("savedRepositoriesEditor"));
		SavedRepositoriesEditor->setAccessibleName(QStringLiteral("Saved repositories"));
		SavedRepositoriesEditor->setAccessibleDescription(QStringLiteral(
			"The repositories the drawer keeps for this account; add one as owner/name or remove the selected one"));
		auto* const controls = new QHBoxLayout{ SavedRepositoriesEditor };
		controls->setContentsMargins(0, 0, 0, 0);
		SavedRepositorySelector = new QComboBox{ SavedRepositoriesEditor };
		SavedRepositorySelector->setObjectName(QStringLiteral("savedRepositorySelector"));
		SavedRepositorySelector->setAccessibleName(QStringLiteral("Saved repositories list"));
		SavedRepositorySelector->setAccessibleDescription(QStringLiteral("Select a saved repository to remove"));
		controls->addWidget(SavedRepositorySelector, 1);
		SavedRepositoryName = new QLineEdit{ SavedRepositoriesEditor };
		SavedRepositoryName->setObjectName(QStringLiteral("savedRepositoryName"));
		SavedRepositoryName->setAccessibleName(QStringLiteral("Repository to save"));
		SavedRepositoryName->setAccessibleDescription(QStringLiteral("owner/name of a repository to keep in the drawer"));
		SavedRepositoryName->setPlaceholderText(QStringLiteral("owner/name"));
		SavedRepositoryName->setMaxLength(static_cast<int>(core::maximum_observer_rule_pattern_bytes));
		controls->addWidget(SavedRepositoryName, 1);
		AddSavedRepository = new QAction{ tr("Save repository"), this };
		AddSavedRepository->setObjectName(QStringLiteral("addSavedRepository"));
		RemoveSavedRepository = new QAction{ tr("Remove saved repository"), this };
		RemoveSavedRepository->setObjectName(QStringLiteral("removeSavedRepository"));
		AddSavedRepositoryButton = new QToolButton{ SavedRepositoriesEditor };
		AddSavedRepositoryButton->setDefaultAction(AddSavedRepository);
		AddSavedRepositoryButton->setObjectName(QStringLiteral("addSavedRepositoryButton"));
		AddSavedRepositoryButton->setAccessibleName(QStringLiteral("Save repository"));
		AddSavedRepositoryButton->setAccessibleDescription(QStringLiteral(
			"Keep the named repository in the drawer for this account; nothing changes on GitHub"));
		controls->addWidget(AddSavedRepositoryButton);
		RemoveSavedRepositoryButton = new QToolButton{ SavedRepositoriesEditor };
		RemoveSavedRepositoryButton->setDefaultAction(RemoveSavedRepository);
		RemoveSavedRepositoryButton->setObjectName(QStringLiteral("removeSavedRepositoryButton"));
		RemoveSavedRepositoryButton->setAccessibleName(QStringLiteral("Remove saved repository"));
		RemoveSavedRepositoryButton->setAccessibleDescription(QStringLiteral(
			"Stop keeping the selected repository in the drawer"));
		controls->addWidget(RemoveSavedRepositoryButton);
		connect(AddSavedRepository, &QAction::triggered, this, [this] {
			dispatch_saved_repository(application::saved_repository_action::add);
		});
		connect(RemoveSavedRepository, &QAction::triggered, this, [this] {
			dispatch_saved_repository(application::saved_repository_action::remove);
		});
		connect(SavedRepositoryName, &QLineEdit::textChanged, this, [this] { update_saved_repository_actions(); });
		connect(SavedRepositorySelector, qOverload<int>(&QComboBox::currentIndexChanged), this,
				[this] { update_saved_repository_actions(); });
		aLayout.addWidget(SavedRepositoriesEditor);
		update_saved_repository_actions();
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
			GithubCliDiscoveryStatus->setText(QStringLiteral("Checking active GitHub CLI accounts…"));
			GithubCliDiscoveryStatus->show();
			break;
		case github_cli_discovery_state::ready:
			GithubCliDiscoveryStatus->setText(
				QStringLiteral("Found %1 active GitHub CLI account%2.")
					.arg(aAccounts)
					.arg(aAccounts == 1U ? QString{ } : QStringLiteral("s")));
			GithubCliDiscoveryStatus->show();
			break;
		case github_cli_discovery_state::empty:
			GithubCliDiscoveryStatus->setText(QStringLiteral(
				"GitHub CLI reported no active authenticated accounts. Run gh auth login, then try again."));
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
		case github_cli_discovery_state::malformed_response:
			GithubCliDiscoveryStatus->setText(QStringLiteral(
				"GitHub CLI returned an account status this version of BetterPullReview could not safely read."));
			GithubCliDiscoveryStatus->show();
			break;
		}
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

	auto workbench_window::saved_repositories_editor() const noexcept -> QWidget*
	{
		return SavedRepositoriesEditor;
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
			const auto index = SavedRepositorySelector->currentIndex();
			if (index < 0
				|| static_cast<std::size_t>(index) >= QueueProjection.SavedRepositories.size()) {
				return;
			}
			repository = QueueProjection.SavedRepositories[static_cast<std::size_t>(index)];
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
		// An account is needed to save for: the queue session names it once
		// a pull request has been opened signed in.
		const auto account = QueueReady && static_cast<bool>(SavedRepositoryDispatch)
			&& !QueueProjection.Session.Host.empty() && !QueueProjection.Session.Account.empty();
		AddSavedRepository->setEnabled(
			account
			&& application::valid(application::saved_repository{
				.Host       = QueueProjection.Session.Host,
				.Account    = QueueProjection.Session.Account,
				.Repository = SavedRepositoryName != nullptr ? SavedRepositoryName->text().trimmed().toStdString() : std::string{ },
			}));
		RemoveSavedRepository->setEnabled(
			account && SavedRepositorySelector != nullptr && SavedRepositorySelector->currentIndex() >= 0);
		AddSavedRepository->setToolTip(account
										   ? QStringLiteral("Keep this repository in the drawer; this never changes GitHub.")
										   : QStringLiteral("Open a pull request signed in first, so there is an account to save for."));
		RemoveSavedRepository->setToolTip(QStringLiteral("Stop keeping the selected repository in the drawer."));
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
			apply_label_helper();
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
			|| aFailure.Failure == application::workbench_host_failure::selected_file_missing_both) {
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
			|| (Modality != nullptr && (Modality->current() == review_modality::discuss || Modality->current() == review_modality::conversations))) {
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

	void workbench_window::choose_modality(const review_modality aModality, const core::workbench_modality_entry aEntry)
	{
		CompositionPage = nullptr;
		// [[BPR-1293]]. Conversations goes back where it was entered from.
		if (Modality != nullptr && aModality == review_modality::conversations && Modality->current() != review_modality::conversations) {
			ConversationsReturn      = Modality->current();
			ConversationsReturnFocus = Finder != nullptr && Finder->isAncestorOf(QApplication::focusWidget()) ? nullptr : QApplication::focusWidget();
		}
		if (Modality != nullptr) {
			Modality->adopt(aModality);
		}
		// [[BPR-0661]]. "Last used" is the modality chosen while a pull
		// request was open.
		if (HasGithubPullRequest) {
			LastUsedModality = aModality;
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
				case review_modality::conversations:
					return core::workbench_modality::conversations;
				}
				return core::workbench_modality::review;
			}(),
			.Entry = aEntry,
		});
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
		const auto caption     = QStringLiteral("%1 — %2 %3").arg(parts.join(QStringLiteral(" · ")), QString::number(count), count == 1U ? QStringLiteral("conversation") : QStringLiteral("conversations"));
		const auto description = QStringLiteral("Listed from the Overview: exactly the conversations this count counted. Show every conversation clears it.");
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
		// [[BPR-1293]]. My draft is a row of Conversations, with its lock.
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
	}

	void workbench_window::apply_thread_marks()
	{
		if (ComparisonSurface == nullptr || ConversationPanel == nullptr) {
			return;
		}
		// Amber for a thread on the line; blue for a thread with a post newer
		// than what the reader has read ([[BPR-0656]]). [[BPR-1290]]: each
		// marker shows its first post's label mark, and the open thread's line
		// is washed and its marker ringed.
		auto* const cards = ConversationPanel->cards();
		ComparisonSurface->set_mark_colours(cards->marker(), cards->accent());
		for (const auto side : { core::diff_side::left, core::diff_side::right }) {
			auto marks = std::vector<diff_line_mark>{ };
			for (const auto& mark : cards->line_marks(side)) {
				marks.push_back(diff_line_mark{
					.Line        = mark.Line,
					.Kind        = mark.Unread ? diff_line_mark_kind::new_thread : diff_line_mark_kind::thread,
					.Label       = mark.Label,
					.Emphasis    = mark.Emphasis,
					.Resolved    = mark.Resolved,
					.Draft       = mark.Draft,
					.Open        = mark.Open,
					.Hovered     = mark.Hovered,
					.Description = mark.Description,
				});
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
		if (const auto identity = thread_on_line(aSide, aLine); identity.has_value()) {
			ConversationPanel->cards()->toggle_thread(*identity);
		}
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
			.LabelHelper = ReviewEditor->label_controls()->helper(),
		});
	}

	void workbench_window::set_conversation_read_dispatch(conversation_read_dispatch aDispatch)
	{
		ConversationReadDispatch = std::move(aDispatch);
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
		return TrayCommentsCount == nullptr ? QString{ } : static_cast<const tray_count_pill*>(TrayCommentsCount)->text();
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
		close_review_composition();
		choose_home(false);
		choose_modality(review_modality::review);
		if (Drawers[1].Panel->state() != core::workbench_drawer_state::expanded) {
			conversations_drawer_toggle_action()->trigger();
		}
		TrayTabs->setCurrentIndex(2);
		TourEditor->show_problem();
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
		const auto& unread = ConversationPanel->cards()->unread_threads_in_file_order();
		NextNewCommentAction->setEnabled(!unread.empty());
		// [[BPR-1134]]. The Comments tab carries the same count.
		if (TrayTabs != nullptr && TrayCommentsCount != nullptr) {
			static_cast<tray_count_pill*>(TrayCommentsCount)->set_count(unread.size());
			TrayTabs->setAccessibleTabName(
				tray_comments_tab,
				unread.empty() ? tr("Comments")
							   : tr("Comments, %1 with new comments").arg(unread.size() == 1U ? tr("one thread") : tr("%1 threads").arg(unread.size())));
		}
		NextNewCommentAction->setToolTip(
			unread.empty() ? QStringLiteral("Next new comment: nothing unread")
						   : QStringLiteral("Next new comment (%1 threads)").arg(unread.size()));
		// [[BPR-1129]]. The comments toggle carries the count of threads with a
		// post newer than what I have read, and says it to accessibility.
		if (auto* const toggle = qobject_cast<QToolButton*>(PrimaryToolbar->widgetForAction(Drawers[1].Toggle));
			toggle != nullptr) {
			set_strip_badge(*toggle, unread.empty() ? QString{ } : QString::number(unread.size()));
			const auto counted = unread.empty()
				? Drawers[1].Toggle->toolTip()
				: QStringLiteral("%1 %2 with new comments.").arg(Drawers[1].Toggle->toolTip()).arg(unread.size() == 1U ? QStringLiteral("One thread") : QStringLiteral("%1 threads").arg(unread.size()));
			toggle->setAccessibleDescription(counted);
		}
	}

	void workbench_window::update_compose_mark()
	{
		if (ComposeReviewAction == nullptr || CommentAction == nullptr) {
			return;
		}
		// [[BPR-1129]]. The mark is the review workflow's draft state, as its
		// editor was last given it; the rule for "kept" is the core's.
		const auto& shown     = ReviewSubmissionEditor != nullptr ? ReviewSubmissionEditor->shown_draft() : std::optional<core::review_submission_draft>{ };
		const auto  keptDraft = shown.has_value() && core::kept(*shown);
		const auto  key       = [](const QAction* const aAction) {
			return aAction->shortcut().toString(QKeySequence::NativeText);
		};
		const auto compose = keptDraft
			? QStringLiteral("Compose review (%1)\nA review draft is kept for this pull request.").arg(key(ComposeReviewAction))
			: QStringLiteral("Compose review (%1)").arg(key(ComposeReviewAction));
		ComposeReviewAction->setToolTip(compose);
		ComposeReviewAction->setStatusTip(compose);
		update_comment_target();
		if (ReviewBar == nullptr) {
			return;
		}
		if (auto* const button = qobject_cast<QToolButton*>(ReviewBar->widgetForAction(ComposeReviewAction)); button != nullptr) {
			button->setAccessibleName(QStringLiteral("Compose review"));
			button->setAccessibleDescription(keptDraft
												 ? QStringLiteral("Open the review page; a review draft is kept for this pull request")
												 : QStringLiteral("Open the review page without submitting"));
			if (button->property(strip_style::mark_property).toBool() != keptDraft) {
				button->setProperty(strip_style::mark_property, keptDraft);
				button->update();
			}
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
		// [[BPR-1293]]. In Conversations, as in Discuss, Comment answers the
		// thread the reviewer picked and never a selected line.
		const auto discuss = Modality != nullptr && (Modality->current() == review_modality::discuss || Modality->current() == review_modality::conversations);
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
		const auto discuss = Modality != nullptr && (Modality->current() == review_modality::discuss || Modality->current() == review_modality::conversations);
		if (!discuss && ComparisonSurface != nullptr) {
			if (const auto selection = ComparisonSurface->selection(); selection.has_value()) {
				return tr("Co&mment on line %1…").arg(static_cast<qulonglong>(selection->PreferredLine.Line) + 1U);
			}
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
		const auto key        = CommentAction->shortcut().toString(QKeySequence::NativeText);
		const auto selection  = ComparisonSurface != nullptr ? ComparisonSurface->selection() : std::nullopt;
		const auto draftReady = ReviewEditor != nullptr
			&& (!ReviewEditor->body_editor()->isReadOnly() || ReviewEditor->reload_draft_action()->isEnabled() || ReviewEditor->reapply_draft_action()->isEnabled())
			&& ReviewEditor->shown_draft() && selection
			&& ReviewEditor->shown_draft()->Anchor == selection->PreferredLine;
		auto reason = QString{ };
		if (comparing_off_current_head()) {
			reason = tr("Select the pull request's current head; this tab compares an earlier update.");
		} else if (target.isEmpty()) {
			reason = tr("Select a current-head diff line or thread to comment.");
		} else if (selection && !core::supported_comment_target(selection->PreferredLine.Side, comment_reply_thread().has_value())) {
			reason = tr("New comments need a line on the right, current-head side. Select a right-side line, or reply to an existing thread.");
		} else if (!draftReady) {
			reason = tr("No editable draft is available. Wait for the review to load, or open an active pull request with an account that can comment.");
		}
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
			button->setAccessibleDescription(reason.isEmpty() ? QStringLiteral("%1, on the comment page, without submitting").arg(words) : reason);
		}
	}

	void workbench_window::comment_or_reply()
	{
		update_comment_target();
		if (!CommentAction->isEnabled()) {
			return;
		}
		if (const auto thread = comment_reply_thread(); thread.has_value()) {
			reply_to_thread(*thread);
			return;
		}
		open_review_composition(true);
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
		const auto& unread = ConversationPanel->cards()->unread_threads_in_file_order();
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
		// The conversation view's current index is what reveals the line, as a
		// press on a card does; the cards follow it.
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
		const auto discuss         = Modality != nullptr && Modality->current() == review_modality::discuss;
		const auto conversations   = Modality != nullptr && Modality->current() == review_modality::conversations;
		const auto enteringDiscuss = discuss && ConversationPanel != nullptr && ConversationPanel->parentWidget() != DiscussPage;
		if (ConversationPanel != nullptr) {
			if (discuss && ConversationPanel->parentWidget() != DiscussPage) {
				DiscussPage->layout()->addWidget(ConversationPanel);
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
		if (conversations && ConversationsPage != nullptr) {
			page = ConversationsPage;
		}
		if (CompositionPage != nullptr) {
			page = CompositionPage;
		}
		const auto enteringOverview      = page == OverviewPage && CentreStack->currentWidget() != OverviewPage;
		const auto enteringConversations = page == ConversationsPage && CentreStack->currentWidget() != ConversationsPage;
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
		// [[BPR-1293]]. Entering Conversations: the search has focus and the
		// cursor starts on the conversation open in Review.
		if (enteringConversations && Finder != nullptr) {
			present_finder_reading();
			Finder->enter();
		}
		if (ShowConversationsAction != nullptr) {
			ShowConversationsAction->setEnabled(HasGithubPullRequest);
		}
	}

	void workbench_window::open_review_composition(const bool aInline)
	{
		if (CompositionPage == nullptr) {
			CompositionReturnFocus = QApplication::focusWidget();
		}
		CompositionPage = aInline ? ComposeInlinePage : ComposeReviewPage;
		apply_modality_surface();
		auto* body = aInline ? ReviewEditor->body_editor() : ReviewSubmissionEditor->body_editor();
		if (body->isVisible() && body->isEnabled()) {
			body->setFocus(Qt::OtherFocusReason);
		} else if (auto* back = CompositionPage->findChild<QPushButton*>(aInline ? QStringLiteral("composeInlineDraftPageBack") : QStringLiteral("composeReviewPageBack"))) {
			back->setFocus(Qt::OtherFocusReason);
		}
	}

	void workbench_window::close_review_composition()
	{
		CompositionPage = nullptr;
		apply_modality_surface();
		if (CompositionReturnFocus && CompositionReturnFocus->isVisible()) {
			CompositionReturnFocus->setFocus(Qt::OtherFocusReason);
		}
	}

	void workbench_window::enter_overview_at(
		const application::overview_entry_kind aEntry,
		const QString&                         aPath)
	{
		// The overview names a path; the core knows the file by identity.
		// Selecting it is the same intent the files drawer sends. [[BPR-1295]]:
		// Start review and Continue review both enter Review, in the
		// arrangement it last had in this window.
		static_cast<void>(aEntry);
		enter_review_at(aPath);
	}

	void workbench_window::enter_review_at(const QString& aPath)
	{
		if (const auto file = file_for_path(aPath); file.has_value() && Dispatch) {
			Dispatch(core::select_workbench_file_intent{ .File = *file });
		}
		choose_modality(review_modality::review);
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
			const auto discuss = Modality != nullptr && (Modality->current() == review_modality::discuss || Modality->current() == review_modality::conversations);
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

	// [[BPR-0661]]. Two groups of segmented rows: the modality rules (which
	// modality a pull request opens in; which one Continue review enters) and
	// the reading behaviour (when a file is marked read; the comparison on
	// return; the comments drawer's first filter). Each group resets on its
	// own. A row dispatches the whole record; the host validates, persists and
	// publishes it, and the page follows what is published, so a refused
	// record reverts.
	void workbench_window::build_behaviour_editor(QWidget* const aPage, QVBoxLayout* const aColumn)
	{
		BehaviourEditor = new QWidget{ aPage };
		BehaviourEditor->setObjectName(QStringLiteral("reviewBehaviourEditor"));
		BehaviourEditor->setAccessibleName(QStringLiteral("Review behaviour"));
		auto* const column = new QVBoxLayout{ BehaviourEditor };
		column->setContentsMargins(0, 0, 0, 0);
		column->setSpacing(6);

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
		};
		const auto addGroup = [this, column](const QString&          aHeading,
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
			column->addLayout(headingRow);
			for (const auto& one : aRows) {
				auto* const line  = new QHBoxLayout{ };
				auto* const label = new QLabel{ one.Label, BehaviourEditor };
				label->setObjectName(one.Name + QStringLiteral("Label"));
				label->setMinimumWidth(180);
				line->addWidget(label, 0, one.Top ? Qt::AlignTop : Qt::Alignment{ });
				if (one.Control != nullptr) {
					label->setBuddy(one.Control);
					line->addWidget(one.Control, 0, one.Top ? Qt::AlignTop : Qt::Alignment{ });
					if (one.Trailing != nullptr) {
						line->addSpacing(one.TrailingSpacing);
						line->addWidget(one.Trailing, 0, Qt::AlignVCenter);
					}
					line->addStretch(1);
					column->addLayout(line);
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
				column->addLayout(line);
			}
			return reset;
		};
		addGroup(
			QStringLiteral("Modality rules"),
			QStringLiteral("resetModalityRules"),
			{
				row{ 0U,
					 QStringLiteral("A pull request opens in"),
					 QStringLiteral("openModality"),
					 { { QStringLiteral("Overview"), QStringLiteral("openModalityOverview") },
					   { QStringLiteral("Review"), QStringLiteral("openModalityReview") },
					   { QStringLiteral("Last used"), QStringLiteral("openModalityLastUsed") } } },
				// [[BPR-1295]]. Row 1, "Continue review enters", went with
				// Read: Start and Continue review both enter Review.
			},
			[](application::review_behaviour_settings& aNext) {
				aNext.OpenModality = application::review_behaviour_settings{ }.OpenModality;
			});
		addGroup(
			QStringLiteral("Reading"),
			QStringLiteral("resetReadingBehaviour"),
			{
				row{ 2U,
					 QStringLiteral("Mark a file read"),
					 QStringLiteral("markRead"),
					 { { QStringLiteral("By hand"), QStringLiteral("markReadByHand") },
					   { QStringLiteral("When fully seen"), QStringLiteral("markReadWhenFullySeen") } } },
				row{ 3U,
					 QStringLiteral("On return, compare"),
					 QStringLiteral("returnComparison"),
					 { { QStringLiteral("Whole PR"), QStringLiteral("returnComparisonWholePr") },
					   { QStringLiteral("Since my last review"), QStringLiteral("returnComparisonSinceLastReview") },
					   { QStringLiteral("Since last upload"), QStringLiteral("returnComparisonSinceLastUpload") } } },
				row{ 4U,
					 QStringLiteral("Comments drawer opens on"),
					 QStringLiteral("commentsFilter"),
					 { { QStringLiteral("All"), QStringLiteral("commentsFilterAll") },
					   { QStringLiteral("Unread"), QStringLiteral("commentsFilterUnread") },
					   { QStringLiteral("Unresolved"), QStringLiteral("commentsFilterUnresolved") } } },
			},
			[](application::review_behaviour_settings& aNext) {
				aNext.MarkRead         = application::review_behaviour_settings{ }.MarkRead;
				aNext.ReturnComparison = application::review_behaviour_settings{ }.ReturnComparison;
				aNext.CommentsFilter   = application::review_behaviour_settings{ }.CommentsFilter;
			});
		// [[BPR-0721]]. The owner found the collapsed rails and the Hide
		// buttons took room the strip's toggles already cover, and asked for
		// a setting rather than their removal. Off is the default.
		addGroup(
			QStringLiteral("Drawers"),
			QStringLiteral("resetDrawers"),
			{
				row{ 5U,
					 QStringLiteral("Drawer controls"),
					 QStringLiteral("drawerControls"),
					 { { QStringLiteral("Hidden"), QStringLiteral("drawerControlsHidden") },
					   { QStringLiteral("Shown"), QStringLiteral("drawerControlsShown") } } },
			},
			[](application::review_behaviour_settings& aNext) {
				aNext.DrawerControls = application::review_behaviour_settings{ }.DrawerControls;
			});
		// [[BPR-1083]]. The owner, 2026-10-05: "i think settings should also
		// have a toggle to switch between the icons and the shortcode icons,
		// because i liked both of them." Icons are the default. The Comment
		// labels section's other rows are BPR-1088's. The owner, accepting it
		// (mark-style-v001): "it'd be nice to see a preview", so a few marks
		// in the published style sit beside the choices.
		FeedbackMarksPreview = new feedback_mark_preview{ BehaviourEditor };
		// [[BPR-1088]]. The rest of the section the owner accepted (BPR-1079
		// Settings v1, adjusted to Overview chips in v2): the label helper,
		// off by default; where the Overview's counts start, Open by default;
		// a link to the label tints; and the Overview chips, ticked and put in
		// order. Each row dispatches the whole record and shows what the host
		// publishes, so a refused or failed save reverts here.
		LabelHelperSwitch = new QCheckBox{ BehaviourEditor };
		LabelHelperSwitch->setObjectName(QStringLiteral("labelHelperSwitch"));
		LabelHelperSwitch->setAccessibleName(QStringLiteral("Label helper"));
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
		auto* const helperInfo = new info_glyph{ QStringLiteral("The label chip and :shortcodes: while writing. Off: write labels yourself; they still count."), helperRow };
		helperInfo->setObjectName(QStringLiteral("labelHelperInfo"));
		helperLayout->addWidget(helperInfo);
		auto* const countsInfo = new info_glyph{ QStringLiteral("Where the Overview's comment counts start. Open counts unresolved conversations; All counts every one. Switching on an Overview changes only that view."), BehaviourEditor };
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
			linkPalette.setColor(QPalette::ButtonText, linkPalette.color(QPalette::Link));
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
		auto* const commentLabelsReset = addGroup(
			QStringLiteral("Comment labels"),
			QStringLiteral("resetCommentLabels"),
			{
				row{ .Index = 0U, .Label = QStringLiteral("Label helper"), .Name = QStringLiteral("labelHelper"), .Options = { }, .Control = helperRow },
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
				aNext.LabelHelper    = defaults.LabelHelper;
				aNext.OverviewCounts = defaults.OverviewCounts;
				aNext.LabelColumns   = defaults.LabelColumns;
			});
		commentLabelsReset->setToolTip(QStringLiteral("Reset comment labels"));
		commentLabelsReset->setAccessibleDescription(QStringLiteral("Returns the label helper, label marks, Overview counts and Overview chips to their defaults; nothing else changes"));
		aColumn->addWidget(BehaviourEditor);
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
		if (ComparisonSurface != nullptr) {
			ComparisonSurface->set_mark_theme(theme);
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

	void workbench_window::apply_label_helper()
	{
		// [[BPR-1088]]. Only a published change reaches the editor, so the
		// helper follows the profile's setting in every window of it.
		if (ReviewEditor == nullptr || AppliedLabelHelper == std::optional{ Behaviour.LabelHelper }) {
			return;
		}
		AppliedLabelHelper = Behaviour.LabelHelper;
		ReviewEditor->set_label_helper(Behaviour.LabelHelper == application::review_label_helper::on);
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
		if (LabelHelperSwitch != nullptr) {
			LabelHelperSwitch->setChecked(Behaviour.LabelHelper == application::review_label_helper::on);
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
			.OpenModality     = static_cast<application::review_open_modality>(chosen(0U)),
			.MarkRead         = static_cast<application::review_mark_read_behaviour>(chosen(2U)),
			.ReturnComparison = static_cast<application::review_return_comparison>(chosen(3U)),
			.CommentsFilter   = static_cast<application::review_comments_filter>(chosen(4U)),
			.DrawerControls   = static_cast<application::review_drawer_controls>(chosen(5U)),
			.FeedbackMarks    = static_cast<application::review_feedback_marks>(chosen(6U)),
			.LabelHelper      = LabelHelperSwitch != nullptr && LabelHelperSwitch->isChecked() ? application::review_label_helper::on : application::review_label_helper::off,
			.OverviewCounts   = static_cast<application::review_overview_counts>(chosen(7U)),
			.LabelColumns     = LabelColumnsEditor != nullptr ? LabelColumnsEditor->columns() : Behaviour.LabelColumns,
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
			(HasGithubPullRequest || HasCachedOverview) ? landing_state::back_to_review
				: account                               ? landing_state::choose_pull_request
														: landing_state::sign_in);
		place_home_landing();
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
			// [[BPR-0783]]. Group, State headers and Filter are Home's own, and
			// only while an account can fill the list: absent from the review
			// strip, and absent rather than disabled while signed out, which is
			// how RFC-BPR-0028's rule still holds there. [[BPR-0990]]: on Home
			// the refresh glyph standing with them is the profile's discovery;
			// the pull request's refresh is the review strip's.
			for (auto* const action : { HomeGroupItem, HomeStateHeaders, HomeFilterItem }) {
				if (action != nullptr) {
					action->setVisible(home && HomeAccount);
				}
			}
			if (GithubRefreshAction != nullptr) {
				GithubRefreshAction->setVisible(!stripHome);
			}
			if (QueueRefreshAction != nullptr) {
				QueueRefreshAction->setVisible(home && HomeAccount);
			}
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
			Dif->setFixedSize(64, 64);
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
		// Over a review the row comes first, above the bar; signed out the
		// card follows the bar and what is said under it.
		const auto wanted = [this, state] {
			return state == landing_state::back_to_review ? 0 : HomeColumn->indexOf(HomeSetupRow) + 1;
		};
		if (HomeColumn->indexOf(HomeLanding) == wanted()) {
			return;
		}
		HomeColumn->removeWidget(HomeLanding);
		HomeColumn->insertWidget(wanted(), HomeLanding);
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
		FilePositionLabel->setText(ReviewBarPositionsShown ? fileText : QStringLiteral("File"));
		FilePositionLabel->setToolTip(fileWords);

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
		ChangePositionLabel->setText(ReviewBarPositionsShown ? changeText : QStringLiteral("Changes"));
		ChangePositionLabel->setToolTip(changeWords);
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
			// then return when the shortcuts go.
			const auto titleRoom = IdentityTitleBudget - margins.left() - margins.right();
			if (room >= titled && titleRoom >= titled) {
				text = metrics.elidedText(IdentityText, Qt::ElideRight, room);
			} else if (room >= metrics.horizontalAdvance(owned)) {
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

	auto workbench_window::previous_change_action() const noexcept -> QAction*
	{
		return Navigation[2].Action;
	}

	auto workbench_window::next_change_action() const noexcept -> QAction*
	{
		return Navigation[3].Action;
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

	auto workbench_window::view_menu() const noexcept -> QMenu*
	{
		return ViewMenu;
	}

	auto workbench_window::line_numbers_action() const noexcept -> QAction*
	{
		return LineNumbersAction;
	}

	auto workbench_window::one_pane_action() const noexcept -> QAction*
	{
		return OnePaneAction;
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
			choose_modality(review_modality::overview);
			apply_modality_surface();
		}
		if (aUpdate.CachedSource && HasCachedOverview && !HasGithubPullRequest) {
			OverviewPage->present_cached_source(*aUpdate.CachedSource);
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
				OverviewPage->present_loading();
				choose_modality(review_modality::overview);
			}
			if (aTransition.Status->Status != core::github_status::loading_pull_request && !HasGithubPullRequest) {
				OverviewPage->clear();
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
		update_strip_identity();
		// The gap BPR-0191 recorded closes here. In the two-window arrangement
		// the anchor could only follow a conversation projection, because nothing
		// observed a comparison context change. The shell composes both surfaces,
		// so it sets the anchor from the file the reviewer is actually reading.
		ConversationPanel->set_anchor_file(conversation_anchor_file());
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
			choose_modality(opening_modality());
		}
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

	void workbench_window::clear_github_pull_request()
	{
		MarkdownAuthority.clear();
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
		update_strip_identity();
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
			shown.SignInAdmitted = card->offers_device_sign_in()
				? DevicePanel != nullptr && DevicePanel->begin_button()->isEnabled()
				: admitted;
			const auto signedIn  = shown.State == server_card_state::connected && shown.Session.has_value();
			if (holdsSignIn && deviceSigning && !signedIn) {
				shown.State = server_card_state::signing_in;
			}
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
		if (HomeDiscoveryStatus != nullptr) {
			const auto sentence = HomeAccount && session ? home_discovery_sentence(discovery) : QString{ };
			if (HomeDiscoveryStatus->text() != sentence) {
				HomeDiscoveryStatus->setText(sentence);
			}
			HomeDiscoveryStatus->setVisible(!sentence.isEmpty());
		}
	}

	void workbench_window::update_pull_request_queue_actions()
	{
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
		SavePullRequestRepository->setEnabled(ref.has_value() && !saved && static_cast<bool>(SavedRepositoryDispatch));
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
		for (const auto* name : { "reviewWorkshopOriginal", "reviewWorkshopProposal", "reviewWorkshopOperation",
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
		// [[BPR-0651]] [[BPR-0650]]. A pull request chosen from a list
		// arrives on Overview.
		OpenOnOverview = true;
		GithubDispatch(core::open_pull_request_intent{
			.Repository = core::repository_locator{
				.Owner = repository.left(slash).toStdString(),
				.Name  = repository.mid(slash + 1).toStdString(),
			},
			.Number = core::pull_request_number{ static_cast<std::uint32_t>(aItem.Ref.Number) },
		});
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

	void workbench_window::set_recent_review_dispatch(
		std::function<void(application::recent_review_intent)> aDispatch)
	{
		RecentReviewDispatch = std::move(aDispatch);
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
				HomeRecentList->setCurrentRow(
					previousRow >= 0 && previousRow < rows ? previousRow : 0);
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
		const auto row = HomeRecentList->currentRow();
		HomeRecentResumeAction->setEnabled(
			row >= 0 && static_cast<std::size_t>(row) < RecentReviews.Entries.size()
			&& RecentReviews.Entries[static_cast<std::size_t>(row)].Resumable);
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
		if (aWatched == DifCornerSlot && aEvent != nullptr
			&& (aEvent->type() == QEvent::Show || aEvent->type() == QEvent::Hide
				|| aEvent->type() == QEvent::Move || aEvent->type() == QEvent::Resize)) {
			place_dif_corner();
		}
		if (aWatched == ReviewBar && aEvent != nullptr
			&& (aEvent->type() == QEvent::FontChange || aEvent->type() == QEvent::StyleChange)) {
			apply_strip_width();
		}
		if (aWatched == HomeRecentList && aEvent != nullptr
			&& aEvent->type() == QEvent::KeyPress) {
			auto* const key = static_cast<QKeyEvent*>(aEvent);
			if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
				// Both Return gestures are read here rather than left to the
				// view. On macOS Return is an item view's editing key and
				// never activates a row, so a keyboard reviewer would have no
				// way into the list at all; elsewhere the view would activate
				// on the shifted key too and resume would also open. Taking
				// the key means one behaviour on every platform, and the
				// pointer's own activation still arrives through
				// `itemActivated`.
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

	auto workbench_window::open_conversation_in_review(const std::string& aThread, const std::string& aPost) -> bool
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
		};
		choose_home(false);
		// Review first, then the file: a file gesture that follows the
		// modality wins over the active tab's restoration.
		choose_modality(review_modality::review);
		if (const auto file = file_for_path(location->Path); file.has_value() && Dispatch) {
			Dispatch(core::select_workbench_file_intent{ .File = *file });
		}
		if (Dispatch && Drawers[1].Panel->state() != core::workbench_drawer_state::expanded) {
			Dispatch(core::set_workbench_drawer_intent{
				.Drawer = core::workbench_drawer::conversations,
				.State  = core::workbench_drawer_state::expanded,
			});
		}
		if (TrayTabs != nullptr && TrayTabs->currentIndex() != tray_comments_tab) {
			TrayTabs->setCurrentIndex(tray_comments_tab);
		}
		resume_conversation_pick();
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
				ConversationPanel->choose(post);
			}
			return;
		}
		// Another file's comparison arrived first: the reviewer went on.
		if (context.has_value() && context->Key.File != PendingPick->File && digest.has_value() && digest != PendingPick->Before) {
			PendingPick.reset();
		}
	}

	void workbench_window::toggle_conversations()
	{
		if (Modality == nullptr || !HasGithubPullRequest) {
			return;
		}
		if (Modality->current() == review_modality::conversations && CompositionPage == nullptr && !showing_home()) {
			leave_conversations();
			return;
		}
		choose_home(false);
		choose_modality(review_modality::conversations);
	}

	void workbench_window::leave_conversations()
	{
		if (Modality == nullptr || Modality->current() != review_modality::conversations) {
			return;
		}
		const auto back  = ConversationsReturn.value_or(review_modality::review);
		const auto focus = ConversationsReturnFocus;
		choose_modality(back == review_modality::conversations ? review_modality::review : back);
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

	auto workbench_window::show_conversations_action() const noexcept -> QAction*
	{
		return ShowConversationsAction;
	}

	auto workbench_window::conversations_finder_button() const noexcept -> QToolButton*
	{
		return ConversationsFinderButton;
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
		// A setup stage's own notice speaks first, then a review restoration's,
		// then the refusal of the last load, unless the reviewer has already
		// begun rewriting what was said.
		const auto refusal = home_refusal_text();
		const auto passive = !HomeRestorationNotice.isEmpty() ? HomeRestorationNotice : refusal;
		const auto spoken  = HomeSetupNotice.isEmpty() && !passive.isEmpty()
			&& passive != DismissedHomeRefusal;
		const auto refused = spoken && HomeRestorationNotice.isEmpty();
		// [[BPR-0775]]. A first load in progress is said here as well, where the
		// pull request was named, since the stack's page that said it is not
		// Home's. It is not a refusal, so it has no Copy details.
		const auto loading = SessionPanel->status() == core::github_status::loading_pull_request && !HasGithubPullRequest
			? QString::fromStdString(SessionPanel->status_text())
			: QString{ };
		const auto notice  = !HomeSetupNotice.isEmpty() ? HomeSetupNotice
			: spoken                                    ? passive
														: loading;
		// [[BPR-1200]]. A refusal the core has its own words for is its card;
		// every other notice keeps the line.
		const auto wording = refused ? home_refusal_wording() : std::nullopt;
		const auto carded  = wording.has_value() && HomeRefusal != nullptr;
		HomeOpenNotice->setText(carded ? QString{ } : notice);
		HomeOpenNotice->setVisible(!carded && !notice.isEmpty());
		HomeRefusalDetails.clear();
		if (refused) {
			HomeRefusalDetails = QString::fromStdString(SessionPanel->status_text());
			if (HomeRefusalDetails != refusal) {
				HomeRefusalDetails += QLatin1Char('\n') + refusal;
			}
		}
		HomeOpenDetails->setVisible(refused && !carded);
		if (HomeRefusal != nullptr) {
			if (carded) {
				HomeRefusal->present(*wording);
			}
			HomeRefusal->setVisible(carded);
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
		// [[BPR-0783]]. Home's three controls, in the strip's reading order,
		// before the refresh glyph they stand with.
		append(HomeGroupButton);
		append(presenter(HomeStateHeaders));
		append(HomeFilterButton);
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
			append(HomeLanding->primary_button());
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
			append(PrivateNotes->notes_list());
			const auto noteButton = [this](QAction* aAction) -> QToolButton* {
				for (auto* button : PrivateNotes->findChildren<QToolButton*>()) {
					if (button->defaultAction() == aAction) {
						return button;
					}
				}
				return nullptr;
			};
			append(noteButton(PrivateNotes->new_review_action()));
			append(noteButton(PrivateNotes->new_file_action()));
			append(PrivateNotes->body_editor());
			append(noteButton(PrivateNotes->save_action()));
			append(noteButton(PrivateNotes->promote_action()));
			append(noteButton(PrivateNotes->erase_action()));
			append(noteButton(PrivateNotes->discard_action()));
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
		// [[BPR-1293]]. Nor is Conversations, which is never stored.
		if (Modality->current() != review_modality::discuss && Modality->current() != review_modality::conversations) {
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
		// history maintenance beneath it and the selected-change detail pane
		// are the review details.
		if (EpochSelectors != nullptr) {
			EpochSelectors->set_details_visible(visible);
		}
		if (ComparisonSurface != nullptr
			&& ComparisonSurface->detail_pane() != nullptr) {
			ComparisonSurface->detail_pane()->setVisible(visible);
		}
		const auto description = visible
			? QStringLiteral(
				  "Hide the exact review anchors and the selected-change details and give their height to the comparison.")
			: QStringLiteral(
				  "Show the exact review anchors and the selected-change details.");
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
			// [[BPR-1251]]. The title yields before the shortcuts, so whether it
			// shows is decided from the room it has beside the whole modality.
			// That room only shrinks as the window narrows, so the room a
			// compact modality frees can never bring the title back.
			Modality->set_compact(false);
			remeasure();
			IdentityTitleBudget = strip_identity_budget();
			Modality->set_compact(available < 1'100);
			remeasure();
			if (identityShort()) {
				Modality->set_compact(true);
				remeasure();
			}
		}
		if (identityShort()) {
			IdentityAction->setVisible(false);
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
		remeasure();
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
		case QEvent::ApplicationFontChange:
		case QEvent::FontChange:
		case QEvent::StyleChange:
		case QEvent::ThemeChange:
		case QEvent::DevicePixelRatioChange:
			refresh_toolbar_icons();
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

	void workbench_window::dispatch_navigation(const std::size_t aIndex)
	{
		if (aIndex < file_navigation_entries) {
			if (Dispatch) {
				Dispatch(core::navigate_workbench_file_intent{
					.Direction = aIndex == 0U
						? core::change_navigation_direction::previous
						: core::change_navigation_direction::next,
				});
			}
			return;
		}
		if (!ChangeDispatch) {
			return;
		}
		// The gesture repeats the policy the projection offered it, so the
		// emitted intent and the enablement that offered it cannot disagree about
		// stop versus wrap.
		ChangeDispatch(core::navigate_diff_change_intent{
			.Direction = aIndex == file_navigation_entries
				? core::change_navigation_direction::previous
				: core::change_navigation_direction::next,
			.Policy    = Commands.Policy,
		});
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
		QuitProfilesAction->setShortcut(QKeySequence::Quit);
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
			QStringLiteral("Keep the open pull request or its repository in my queue; nothing changes on GitHub"));
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
			if (!ref || !SavedRepositoryDispatch) {
				return;
			}
			SavedRepositoryDispatch(application::saved_repository_intent{
				.Action     = application::saved_repository_action::add,
				.Repository = { .Host = ref->Host, .Account = ref->Account, .Repository = ref->Repository },
			});
		});

		ViewMenu = PrimaryMenuBar->addMenu(QStringLiteral("&View"));
		ViewMenu->setObjectName(QStringLiteral("workbenchViewMenu"));
		ViewMenu->setAccessibleName(QStringLiteral("View"));
		ViewMenu->setAccessibleDescription(
			QStringLiteral("Presentation options for the comparison surface"));

		LineNumbersAction
			= new QAction{ QStringLiteral("&Line numbers"), this };
		LineNumbersAction->setObjectName(
			QStringLiteral("toggleWorkbenchLineNumbersAction"));
		LineNumbersAction->setCheckable(true);
		LineNumbersAction->setChecked(true);
		LineNumbersAction->setShortcut(
			QKeySequence{ QStringLiteral("Alt+Shift+5") });
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
		// [[BPR-1295]]. Hide Drawers. Command-B (Ctrl+B elsewhere), the
		// owner's left-hand key and the one Visual Studio Code uses for its
		// side bar. Checked while the drawers are hidden. Its presenter is the
		// title row's button right of the Comments toggle.
		ToggleDrawersAction = new QAction{ QStringLiteral("Hide &Drawers"), this };
		ToggleDrawersAction->setObjectName(QStringLiteral("toggleWorkbenchDrawersAction"));
		ToggleDrawersAction->setIconText(QStringLiteral("Hide drawers"));
		ToggleDrawersAction->setCheckable(true);
		ToggleDrawersAction->setShortcut(QKeySequence{ QStringLiteral("Ctrl+B") });
		ToggleDrawersAction->setShortcutContext(Qt::WindowShortcut);
		addAction(ToggleDrawersAction);
		ViewMenu->addAction(ToggleDrawersAction);
		connect(ToggleDrawersAction, &QAction::triggered, this, [this] {
			dispatch_drawers_toggle();
		});
		// [[BPR-1293]]. Command-J (Ctrl+J), unbound before: straight to
		// Conversations from Overview, Review or Discuss, and back from it.
		ShowConversationsAction = new QAction{ QStringLiteral("&Conversations"), this };
		ShowConversationsAction->setObjectName(QStringLiteral("showWorkbenchConversationsAction"));
		ShowConversationsAction->setShortcut(QKeySequence{ QStringLiteral("Ctrl+J") });
		ShowConversationsAction->setShortcutContext(Qt::WindowShortcut);
		ShowConversationsAction->setEnabled(false);
		{
			const auto tip = QStringLiteral("Find a conversation in the pull request (%1)").arg(ShowConversationsAction->shortcut().toString(QKeySequence::NativeText));
			ShowConversationsAction->setToolTip(tip);
			ShowConversationsAction->setStatusTip(tip);
			if (ConversationsFinderButton != nullptr) {
				ConversationsFinderButton->setToolTip(tip);
				ConversationsFinderButton->setAccessibleDescription(tip);
			}
		}
		addAction(ShowConversationsAction);
		ViewMenu->addAction(ShowConversationsAction);
		connect(ShowConversationsAction, &QAction::triggered, this, [this] { toggle_conversations(); });
		EditorKeyYield = new editor_key_yield{ this, ToggleDrawersAction };
		connect(qApp, &QApplication::focusChanged, this, [this](QWidget* const aOld, QWidget* const aNow) {
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
		// [[BPR-1132]]. The tab keys, window-owned: Ctrl+Tab and Ctrl+Shift+Tab
		// move between the tabs (the Control key on macOS too, where Qt names
		// it Meta), and Cmd-W (Ctrl+W) closes the active tab. Nothing had
		// Cmd-W before, so no window command moves to Cmd-Shift-W.
		const auto tabAction = [this](const QString& aText, const QString& aName, const QKeySequence& aKey) {
			auto* const action = new QAction{ aText, this };
			action->setObjectName(aName);
			action->setShortcut(aKey);
			action->setShortcutContext(Qt::WindowShortcut);
			action->setEnabled(false);
			addAction(action);
			return action;
		};
#if defined(Q_OS_MACOS)
		NextTabAction     = tabAction(tr("Next Tab"), QStringLiteral("nextDocumentTabAction"), QKeySequence{ QStringLiteral("Meta+Tab") });
		PreviousTabAction = tabAction(tr("Previous Tab"), QStringLiteral("previousDocumentTabAction"), QKeySequence{ QStringLiteral("Meta+Shift+Tab") });
#else
		NextTabAction     = tabAction(tr("Next Tab"), QStringLiteral("nextDocumentTabAction"), QKeySequence{ QStringLiteral("Ctrl+Tab") });
		PreviousTabAction = tabAction(tr("Previous Tab"), QStringLiteral("previousDocumentTabAction"), QKeySequence{ QStringLiteral("Ctrl+Shift+Tab") });
#endif
		CloseTabAction = tabAction(tr("Close Tab"), QStringLiteral("closeDocumentTabAction"), QKeySequence{ QStringLiteral("Ctrl+W") });
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
		SettingsAction->setShortcut(QKeySequence{ Qt::CTRL | Qt::Key_Comma });
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
										   || GithubCliDiscoveryState == github_cli_discovery_state::unsupported_version));
		if (DevicePanel && DevicePanel->signing_in()) {
			DevicePanel->cancel_button()->hide();
		}
	}

	void workbench_window::open_sign_in(std::string aHostname, const bool aCliOnly)
	{
		if (!AccountsPanel) {
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
			reason = QStringLiteral("In Discuss the conversation is the page");
		} else if (projection && projection->DisabledReason == core::workbench_reason_code::conversations_finds_a_conversation) {
			reason = QStringLiteral("In Conversations the finder is the page");
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
		const auto fullPage = CompositionPage != nullptr
			|| (Modality != nullptr
				&& (Modality->current() == review_modality::discuss || Modality->current() == review_modality::conversations));
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
				&& (Modality->current() == review_modality::discuss || Modality->current() == review_modality::conversations))) {
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
		ReviewBar->adopt_button(ReviewControlsButton);
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
		ReviewControlsButton->setIcon(strip_icon(strip_glyph::private_progress, ink));
		if (ComposeReviewAction != nullptr && CommentAction != nullptr) {
			ComposeReviewAction->setIcon(strip_icon(strip_glyph::compose_review, ink));
			CommentAction->setIcon(strip_icon(strip_glyph::comment, ink));
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
		if (AccountAction != nullptr) {
			AccountAction->setIcon(strip_icon(strip_glyph::account, ink));
		}
		if (SettingsAction != nullptr) {
			SettingsAction->setIcon(strip_icon(strip_glyph::settings, ink));
		}

		constexpr auto receiptIcons = std::array{
			QStyle::SP_DialogYesButton,
			QStyle::SP_DialogApplyButton,
			QStyle::SP_DialogOkButton,
			QStyle::SP_DialogCancelButton,
		};
		for (std::size_t index = 0U; index < ReceiptActions.size(); ++index) {
			ReceiptActions[index]->setIcon(nativeStyle->standardIcon(
				receiptIcons[index], nullptr, this));
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
		// One projection for all four primary actions. Entry `i` of the core's
		// delta decides action `i`; the window adds no legality of its own and
		// keeps no second carrier for the change actions.
		for (std::size_t index = 0U; index < Navigation.size(); ++index) {
			const auto projection
				= command(static_cast<core::workbench_command_id>(index));
			const auto reason = projection
				? projection->DisabledReason
				: core::workbench_reason_code::no_active_workbench;
			describe_action(Navigation[index].Action,
							projection && projection->Enabled,
							Navigation[index].Purpose,
							index < file_navigation_entries
								? reason_text(reason)
								: change_reason_text(reason));
		}

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
		// Last, and deliberately. The mode is not a second opinion about a
		// command's legality; it is the answer that stands once the core's
		// projections have had theirs.
		apply_public_preview_availability();
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
				case core::file_receipt_command_kind::mark_unread:
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
			action->setIconText(label);
			const auto checked = Receipt.has_value()
				&& (index == 0U
						? Receipt->Receipt.Read != core::file_read_state::unread
						: index == 1U
						? Receipt->Receipt.Read == core::file_read_state::read
						: index == 2U ? Receipt->Receipt.Accepted
									  : Receipt->Receipt.Excluded);
			action->setChecked(checked);
			describe_action(
				action,
				projected != nullptr && projected->Enabled && !LedgerUnavailable,
				QStringLiteral("Update private progress for the selected exact file material."),
				disabled);
		}
		// [[BPR-1129]]. The review bar's progress icon says this file's state in
		// its tooltip, with the key and the four actions it offers.
		if (ReviewControlsButton != nullptr) {
			auto state = QString{ };
			if (LedgerUnavailable) {
				state = QStringLiteral("Private review progress is unavailable.");
			} else if (!Receipt.has_value()) {
				state = QStringLiteral("No file comparison is selected.");
			} else {
				const auto& receipt = Receipt->Receipt;
				auto        words   = QStringList{ receipt.Read == core::file_read_state::read         ? QStringLiteral("Read")
													   : receipt.Read == core::file_read_state::unread ? QStringLiteral("unread")
																									   : QStringLiteral("Seen") };
				if (receipt.Accepted) {
					words.push_back(QStringLiteral("accepted"));
				}
				if (receipt.Excluded) {
					words.push_back(QStringLiteral("excluded"));
				}
				state = QStringLiteral("This file is %1.").arg(words.join(QStringLiteral(", ")));
			}
			const auto key = ReviewControlsButton->shortcut().toString(QKeySequence::NativeText);
			ReviewControlsButton->setToolTip(QStringLiteral("Private progress (%1)\n%2 Seen, Read, Accept or Exclude").arg(key, state));
			ReviewControlsButton->setStatusTip(state);
			ReviewControlsButton->setAccessibleDescription(QStringLiteral("%1 Opens Seen, Read, Accept and Exclude.").arg(state));
		}
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
