#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_STRIP_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_STRIP_HPP

#include "bpr/core/file_progress_mark.hpp"

#include <QFrame>
#include <QIcon>
#include <QKeySequence>
#include <QPalette>
#include <QPointer>
#include <QProxyStyle>
#include <QToolBar>
#include <QWidget>

#include <QPoint>
#include <QToolButton>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>

class QAction;
class QActionEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QToolButton;

namespace bpr::qt {
	/// \brief The line icons the strip draws, in RFC-BPR-0028's asset order.
	///
	/// \details Painted rather than loaded: every glyph is a stroke in the
	/// palette's button-text colour at the live device pixel ratio, so the
	/// strip reads in full ink on every theme and at every scale without a
	/// resource file per palette.
	enum class strip_glyph : std::uint8_t {
		files_drawer,
		comments_drawer,
		previous_change,
		next_change,
		previous_file,
		next_file,
		new_comment,
		refresh,
		account,
		settings,
		review_details,
		private_progress,
		home,
		/// [[BPR-0651]]. The repositories drawer toggle.
		repositories_drawer,
		/// [[BPR-1129]]. Compose review, a pencil on the review bar.
		compose_review,
		/// [[BPR-1129]]. Comment, a speech balloon on the review bar.
		comment,
		/// [[BPR-1295]]. Hide drawers: a window with a filled column at each
		/// side, the drawers that are showing.
		drawers_shown,
		/// [[BPR-1295]]. Show drawers: the same window with both columns
		/// outlined, the drawers that are hidden.
		drawers_hidden,
		/// [[BPR-1331]]. The private progress button's state marks, after the
		/// Overview's read column ([[BPR-0647]]) and [[BPR-1083]]'s weight scale:
		/// the emptier the mark, the less is claimed. Unread is a broken ring,
		/// nothing claimed yet.
		progress_unread,
		/// [[BPR-1331]]. Seen: a ring with its left half filled.
		progress_seen,
		/// [[BPR-1331]]. Read: a filled disc.
		progress_read,
		/// [[BPR-1331]]. Accepted: the filled disc with a tick cut out of it.
		progress_accepted,
		/// [[BPR-1331]]. Excluded: a ring struck through, left out of review.
		progress_excluded,
		/// [[BPR-1338]]. Open on GitHub: a page whose corner an arrow leaves,
		/// the common mark for "opens outside this application".
		open_on_github,
	};

	/// \brief [[BPR-1331]]. The glyph the private progress button shows for
	/// the mark the core decided; the plain tick while no file's progress is
	/// known.
	[[nodiscard]] auto progress_glyph(std::optional<core::file_progress_mark> aMark) noexcept -> strip_glyph;

	/// \brief Paints one strip glyph for a palette.
	[[nodiscard]] auto strip_icon(strip_glyph aGlyph, const QPalette& aPalette)
		-> QIcon;

	/// \brief Draws strip buttons as the accepted design draws them.
	///
	/// \details A bordered 22-pixel button in full ink, a soft accent fill
	/// when checked, and a solid accent fill for a button marked filled (the
	/// modality segments). Everything else falls through to the platform
	/// style, so menus, tooltips and focus behave as they do elsewhere. The
	/// adapter installs no application style; this one is set on the strip's
	/// own presenters only.
	class strip_style final : public QProxyStyle
	{
	public:
		/// \brief A style for a row of `aRowHeight` pixels, whose buttons it
		/// centres with an equal margin above and below.
		explicit strip_style(int aRowHeight = 30);

		/// \brief Height of every strip button.
		static constexpr int button_height = 22;
		/// \brief The dynamic property naming a solid-filled checked button.
		static constexpr const char* filled_property = "stripFilled";
		/// \brief [[BPR-1129]]. The dynamic property holding a count the button
		/// carries beside its icon, as a pill in the accent; empty for none.
		static constexpr const char* badge_property = "stripBadge";
		/// \brief [[BPR-1129]]. The dynamic property that marks a button's icon
		/// with the amber of my last review's tick.
		static constexpr const char* mark_property = "stripMark";

		void drawComplexControl(
			ComplexControl             aControl,
			const QStyleOptionComplex* aOption,
			QPainter*                  aPainter,
			const QWidget*             aWidget = nullptr) const override;
		[[nodiscard]] auto sizeFromContents(
			ContentsType        aType,
			const QStyleOption* aOption,
			const QSize&        aContentsSize,
			const QWidget*      aWidget = nullptr) const -> QSize override;
		[[nodiscard]] auto pixelMetric(
			PixelMetric         aMetric,
			const QStyleOption* aOption = nullptr,
			const QWidget*      aWidget = nullptr) const -> int override;

	private:
		int RowHeight = 30;
	};

	/// \brief [[BPR-1129]]. Gives a strip button a count to carry beside its
	/// icon, or none, and makes the button measure itself again.
	void set_strip_badge(QToolButton& aButton, const QString& aBadge);

	/// \brief [[BPR-1129]]. Which row a strip is.
	///
	/// \details The title row is the window's title bar: empty surface drags
	/// and zooms the window. The review bar under it is an ordinary row of
	/// controls whose empty surface does nothing.
	enum class strip_role : std::uint8_t {
		title_row,
		review_bar,
	};

	/// \brief Presents the 30-pixel review strip.
	///
	/// \details One row in full ink with a rule beneath it, as the accepted
	/// design draws it. It stays a `QToolBar` so the actions it carries keep
	/// their `widgetForAction` presenters, their window shortcuts and the
	/// focus order the shell declares over them. Every presenter the toolbar
	/// creates adopts the strip style as it is added.
	class workbench_strip final : public QToolBar
	{
	public:
		explicit workbench_strip(const QString& aTitle, QWidget* aParent = nullptr, strip_role aRole = strip_role::title_row);
		~workbench_strip() override;

		workbench_strip(const workbench_strip&)                    = delete;
		auto operator=(const workbench_strip&) -> workbench_strip& = delete;

		/// \brief The strip's height, from the accepted design.
		static constexpr int height_pixels = 30;
		/// \brief [[BPR-1129]]. The review bar's height, from RFC-BPR-0052's design.
		static constexpr int review_bar_height_pixels = 28;
		[[nodiscard]] auto   role() const noexcept -> strip_role;

		/// \brief Gives a button the strip's style and focus policy.
		void               adopt_button(QToolButton* aButton);
		[[nodiscard]] auto button_style() const noexcept -> strip_style*;
		/// \brief Joins each navigation label to its arrow pair with a palette-derived rail.
		void set_navigation_rails(QAction* aFileLabel, QAction* aFileLast, QAction* aChangeLabel, QAction* aChangeLast);
		/// \brief Whether a point in the strip is bare surface, no control.
		///
		/// \details The spacers, the inset, the dividers and the labels are the
		/// strip's title-bar surface; a button or any focusable control is not.
		/// The tracked drag uses it, and on Windows the host's hit test does
		/// ([[BPR-0653]]).
		[[nodiscard]] auto empty_surface_at(QPoint aPoint) const -> bool;

	protected:
		bool event(QEvent* aEvent) override;
		void paintEvent(QPaintEvent* aEvent) override;
		void actionEvent(QActionEvent* aEvent) override;
		/// \brief Empty strip space drags the window, as a title bar would.
		void mousePressEvent(QMouseEvent* aEvent) override;
		void mouseMoveEvent(QMouseEvent* aEvent) override;
		void mouseReleaseEvent(QMouseEvent* aEvent) override;
		/// \brief Double-clicking empty strip space zooms the window.
		void mouseDoubleClickEvent(QMouseEvent* aEvent) override;

	private:
		[[nodiscard]] auto over_empty_space(const QMouseEvent& aEvent) const -> bool;
		void               paint_navigation_rails(QPainter& aPainter) const;

		strip_style*                     Style = nullptr;
		strip_role                       Role  = strip_role::title_row;
		std::array<QPointer<QAction>, 4> NavigationRails;
		/// \brief Where inside the window frame the drag took hold, while held.
		std::optional<QPoint> DragHold;
	};

	/// \brief The 1 by 18 pixel rule between strip regions.
	class strip_divider final : public QFrame
	{
	public:
		explicit strip_divider(QWidget* aParent = nullptr);

	protected:
		void paintEvent(QPaintEvent* aEvent) override;
	};

	/// \brief [[BPR-1130]]. The review bar's Updates disclosure.
	///
	/// \details RFC-BPR-0052: "Updates" leads the review bar, and the
	/// disclosure beside it folds the Updates line's timeline away and opens
	/// it again, so the bar itself never moves. The window gives it the
	/// fold command and the state the core decided; it decides nothing. While
	/// history is incomplete or ambiguous it carries the line's mark after its
	/// name, so a folded line still says so.
	class updates_disclosure final : public QToolButton
	{
	public:
		explicit updates_disclosure(QWidget* aParent = nullptr);

		/// \brief Shows the fold the core decided: a right arrow folded, a
		/// down arrow open.
		void               set_open(bool aOpen);
		[[nodiscard]] auto open() const noexcept -> bool;
		void               set_history_marked(bool aMarked);
		[[nodiscard]] auto history_marked() const noexcept -> bool;

		[[nodiscard]] auto sizeHint() const -> QSize override;
		[[nodiscard]] auto minimumSizeHint() const -> QSize override;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		bool Open   = false;
		bool Marked = false;
	};

	/// \brief The four review modalities, in strip order, which is their
	/// keys' order.
	///
	/// \details [[BPR-1295]]. Read merged into Review on 2026-10-06.
	/// [[BPR-1293]]. Conversations, the pull request's finder, is fourth on
	/// Ctrl+4, after Discuss on Ctrl+3 (the owner, 2026-10-06: "can we swap
	/// discuss to 3 and conversations to 4").
	enum class review_modality : std::uint8_t {
		overview,
		review,
		discuss,
		conversations,
	};

	inline constexpr std::size_t review_modality_count = 4U;

	[[nodiscard]] auto review_modality_title(review_modality aModality) -> QString;

	/// \brief [[BPR-1295]]. The key that chooses a modality.
	///
	/// \details Set per modality rather than by strip position: Overview
	/// Ctrl+1, Review Ctrl+2, Discuss Ctrl+3 and Conversations Ctrl+4
	/// (Command on macOS); the owner, 2026-10-06: "can we swap discuss to 3
	/// and conversations to 4". Ctrl+J also goes to Conversations, from the
	/// window's own action ([[BPR-1293]]).
	[[nodiscard]] auto review_modality_shortcut(review_modality aModality) -> QKeySequence;

	using review_modality_dispatch = std::function<void(review_modality)>;

	/// \brief The filled segmented modality control at the strip's centre.
	///
	/// \details Presentation only. Choosing a segment reports the modality;
	/// what a modality means for the drawers is decided where the drawers are
	/// decided. The control shows the modality it was last told, so a manual
	/// drawer toggle leaves the segment where it is until the next modality.
	class modality_control final : public QWidget
	{
	public:
		explicit modality_control(workbench_strip& aStrip, QWidget* aParent = nullptr);

		void set_dispatch(review_modality_dispatch aDispatch);
		/// \brief Shows a modality as current without reporting it.
		void adopt(review_modality aModality);
		/// \brief Drops the shortcut beside each name when width is short.
		void               set_compact(bool aCompact);
		[[nodiscard]] auto compact() const noexcept -> bool;
		/// \brief Shows each name as its three-character prefix, the strip's last yield.
		///
		/// \details [[BPR-0653]]. Only the visible text shortens: Ove, Rev,
		/// Dis and Con. The accessible names and tooltips keep the full words.
		void               set_abbreviated(bool aAbbreviated);
		[[nodiscard]] auto abbreviated() const noexcept -> bool;
		[[nodiscard]] auto current() const noexcept -> review_modality;
		[[nodiscard]] auto action(review_modality aModality) const noexcept
			-> QAction*;
		[[nodiscard]] auto button(review_modality aModality) const noexcept
			-> QToolButton*;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		void choose(review_modality aModality);
		void relabel();

		review_modality_dispatch                        Dispatch;
		review_modality                                 Current     = review_modality::review;
		bool                                            Compact     = false;
		bool                                            Abbreviated = false;
		std::array<QAction*, review_modality_count>     Actions{ };
		std::array<QToolButton*, review_modality_count> Buttons{ };
	};
} // namespace bpr::qt

#endif
