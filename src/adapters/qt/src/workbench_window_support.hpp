#ifndef BPR_SRC_ADAPTERS_QT_SRC_WORKBENCH_WINDOW_SUPPORT_HPP
#define BPR_SRC_ADAPTERS_QT_SRC_WORKBENCH_WINDOW_SUPPORT_HPP
#include "bpr/qt/epoch_comparison_control.hpp"
#include "bpr/qt/overview_status_drawer.hpp"
#include "bpr/qt/workbench_window.hpp"
#include "drawer_tab_bar.hpp"

#include <QAction>
#include <QFlags>
#include <QFont>
#include <QKeyCombination>
#include <QKeySequence>
#include <QObject>
#include <QPaintEvent>
#include <QSize>
#include <QString>
#include <QWidget>

#include <array>
#include <cstddef>
#include <optional>
#include <vector>

// Helpers the workbench window's responsibility sources share.
namespace bpr::qt {
	using tray_tab_bar = detail::drawer_tab_bar;

	/// \brief [[BPR-1212]]. The code a maximized draft is on, above
	/// Discuss's composer: the draft's lines under a blue bracket and three
	/// lines either side, exactly as the head holds them.
	class draft_excerpt_view final : public QWidget
	{
	public:
		explicit draft_excerpt_view(QWidget* aParent);

		void present(const QString& aPath, const std::uint32_t aFirstNumber, std::vector<QString> aLines,
					 const std::uint32_t aBracketFirst, const std::uint32_t aBracketLast, const QFont& aCode);

		void clear();

		[[nodiscard]] auto words() const -> QString;

		[[nodiscard]] auto sizeHint() const -> QSize override;

		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* /*aEvent*/) override;

	private:
		[[nodiscard]] auto header() const -> int;

		[[nodiscard]] auto row() const -> int;

		QString              Path;
		std::uint32_t        FirstNumber = 1U;
		std::vector<QString> Lines;
		std::uint32_t        BracketFirst = 1U;
		std::uint32_t        BracketLast  = 1U;
		QFont                Code;
	};
#if !defined(_WIN32)
	inline constexpr int platform_floor_width = 900;
#endif
	/// \brief [[BPR-1134]]. The right tray's tabs, in order.
	constexpr int tray_comments_tab = 0;

	/// [[BPR-1592]]. Notes holds only private notes; the agent's material
	/// is the provisional Agent tab's, after Tour, until [[BPR-1594]].
	constexpr int tray_notes_tab = 1;

	constexpr int tray_tour_tab = 2;

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

	/// \brief [[BPR-1290]]. The narrowest and widest the conversations
	/// drawer drags to: the inline drawer prototype's range, which the
	/// owner kept ("I like it", 2026-10-06). This supersedes [[BPR-0728]]'s
	/// 160 pixel floor for this drawer.
	constexpr int conversation_drawer_minimum_width = 280;

	constexpr int conversation_drawer_maximum_width = 760;

	/// \brief [[BPR-1925]]. The drawer's range while the Tour tab shows the
	/// tour and the chat about its marked part side by side: the owner's
	/// chosen mockup widened it from about 420 to about 740.
	constexpr int tour_chat_drawer_minimum_width = 600;

	constexpr int tour_chat_drawer_maximum_width = 960;

	/// \brief [[BPR-1328]]. How long a press at a navigation bound says why
	/// it did nothing.
	constexpr int bounded_navigation_message_ms = 3000;

	[[nodiscard]] auto reason_text(const core::workbench_reason_code aReason)
		-> QString;

	// The drawer commands share the workbench reason vocabulary, so only the
	// reason a drawer can actually be refused for needs drawer wording.
	[[nodiscard]] auto drawer_reason_text(
		const core::workbench_reason_code aReason) -> QString;

	// The change commands share the workbench reason vocabulary, so only the
	// reason whose shared wording is file-flavoured needs a change sentence.
	[[nodiscard]] auto change_reason_text(
		const core::workbench_reason_code aReason) -> QString;

	[[nodiscard]] auto failure_text(
		const application::workbench_host_failure aFailure) -> QString;

	/// [[BPR-1857]]. A file that cannot open for want of a comparison says
	/// why, in Review history's own words, instead of only pointing there.
	[[nodiscard]] auto failure_text(
		const application::workbench_host_failure aFailure,
		const epoch_comparison_control*           aEpoch) -> QString;

	/// \brief [[BPR-1532]]. The owner, 2026-10-15: Option-Shift-Up and
	/// Option-Shift-Down (Alt+Shift+Up and Alt+Shift+Down elsewhere) go to
	/// the first and the last change in the current file.
	[[nodiscard]] auto change_end_keys() -> std::array<QKeySequence, 2>;

	/// \brief [[BPR-1295]]. Whether a widget is an editable multi-line
	/// editor: a composer, a reply, a note or a suggestion.
	[[nodiscard]] auto editable_multi_line(const QObject* const aObject) -> bool;

	/// \brief [[BPR-1531]]. Accept file's key: Option-A on macOS, Alt+A
	/// elsewhere (owner, 2026-10-15, replacing Command-Shift-A).
	[[nodiscard]] auto accept_file_key() -> QKeySequence;

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
	[[nodiscard]] auto takes_typed_text(const QWidget* const aWidget) -> bool;

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
	[[nodiscard]] auto option_character_key(const QKeyCombination aKey) -> bool;

	/// \brief [[BPR-1328]]. Describes one file or change arrow.
	///
	/// \details The owner kept the keys and asked that they be
	/// discoverable, so the tooltip's first line is the command and its
	/// key in the platform's own notation ("Next change (⌥↓)"). A second
	/// line names the bound a press would meet ("Last change in this
	/// file.") or why the arrow is disabled. The status tip carries the
	/// sentence alone.
	void describe_navigation_action(
		QAction* const                aAction,
		const QString&                aName,
		const bool                    aEnabled,
		const QString&                aEnabledText,
		const QString&                aDetail,
		const std::optional<QString>& aKey = std::nullopt);

	void describe_action(
		QAction* const aAction,
		const bool     aEnabled,
		const QString& aEnabledText,
		const QString& aDisabledText);

	[[nodiscard]] auto counted(const std::uint32_t aCount, const QString& aOne, const QString& aMany) -> QString;

	/// [[BPR-1575]]. "just now", "2 min ago", "3 hr ago", "2 days ago".
	[[nodiscard]] auto refresh_age_words(const core::refresh_age aAge) -> QString;
}
#endif
