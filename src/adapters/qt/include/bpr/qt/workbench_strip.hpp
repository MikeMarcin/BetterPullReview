#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_STRIP_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_WORKBENCH_STRIP_HPP

#include <QFrame>
#include <QIcon>
#include <QPalette>
#include <QProxyStyle>
#include <QToolBar>
#include <QWidget>

#include <QPoint>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>

class QAction;
class QActionEvent;
class QMouseEvent;
class QPaintEvent;
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
	};

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
		strip_style();

		/// \brief Height of every strip button.
		static constexpr int button_height = 22;
		/// \brief The dynamic property naming a solid-filled checked button.
		static constexpr const char* filled_property = "stripFilled";

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
		explicit workbench_strip(const QString& aTitle, QWidget* aParent = nullptr);
		~workbench_strip() override;

		workbench_strip(const workbench_strip&)                    = delete;
		auto operator=(const workbench_strip&) -> workbench_strip& = delete;

		/// \brief The strip's height, from the accepted design.
		static constexpr int height_pixels = 30;

		/// \brief Gives a button the strip's style and focus policy.
		void               adopt_button(QToolButton* aButton);
		[[nodiscard]] auto button_style() const noexcept -> strip_style*;

	protected:
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

		strip_style* Style = nullptr;
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

	/// \brief The four review modalities, in strip order.
	enum class review_modality : std::uint8_t {
		overview,
		read,
		review,
		discuss,
	};

	inline constexpr std::size_t review_modality_count = 4U;

	[[nodiscard]] auto review_modality_title(review_modality aModality) -> QString;

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
		[[nodiscard]] auto current() const noexcept -> review_modality;
		[[nodiscard]] auto action(review_modality aModality) const noexcept
			-> QAction*;
		[[nodiscard]] auto button(review_modality aModality) const noexcept
			-> QToolButton*;

	protected:
		void paintEvent(QPaintEvent* aEvent) override;

	private:
		void choose(review_modality aModality);

		review_modality_dispatch                        Dispatch;
		review_modality                                 Current = review_modality::review;
		bool                                            Compact = false;
		std::array<QAction*, review_modality_count>     Actions{ };
		std::array<QToolButton*, review_modality_count> Buttons{ };
	};
} // namespace bpr::qt

#endif
