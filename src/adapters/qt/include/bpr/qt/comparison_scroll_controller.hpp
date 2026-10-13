#ifndef BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_COMPARISON_SCROLL_CONTROLLER_HPP
#define BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_COMPARISON_SCROLL_CONTROLLER_HPP

#include "bpr/qt/measured_alignment_axis.hpp"
#include "bpr/qt/pixel_scroll_gesture.hpp"

#include <QObject>
#include <QPointer>
#include <QScrollBar>
#include <QtTypes>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>

class QEvent;
class QKeyEvent;
class QWheelEvent;
class QWidget;

namespace bpr::qt {
	/// \brief Identifies which comparison surface received an input event.
	enum class comparison_scroll_receiver : std::uint8_t {
		left_pane,
		right_pane,
		link_map,
	};

	/// \brief [[BPR-1328]]. An end of the comparison a key scrolls to.
	enum class comparison_scroll_edge : std::uint8_t {
		top,
		bottom,
	};

	/// \brief [[BPR-1328]]. The end of the comparison a code-pane key press
	/// scrolls to, if it is one.
	///
	/// \details The owner, 2026-10-08: "let cmd+down scroll to the bottom and
	/// cmd+up to the top". The platform's document start and end:
	/// `QKeySequence::MoveToStartOfDocument` and `MoveToEndOfDocument`
	/// (Ctrl+Home and Ctrl+End on Windows and Linux), and on macOS Command-Up
	/// and Command-Down under every platform plugin, the offscreen one too,
	/// whose key scheme is not macOS's.
	[[nodiscard]] auto comparison_scroll_edge_for(const QKeyEvent& aEvent) -> std::optional<comparison_scroll_edge>;

	/// \brief Configures Qt-local shared comparison scrolling.
	struct comparison_scroll_configuration
	{
		qreal SynchronizationRailFraction = 0.43;
		qreal AngleStepExtent             = 16.0;
		qreal PageFraction                = 0.9;
		int   ScrollbarPrecision          = 100'000;
	};

	/// \brief Describes one pane's projection in the current shared frame.
	struct comparison_pane_scroll_frame
	{
		qreal ProjectedDocumentY = 0.0;
		qreal DocumentOriginY    = 0.0;
		qreal ViewportY          = 0.0;
		bool  LeadingClamped     = false;
		bool  TrailingClamped    = false;

		auto operator<=>(const comparison_pane_scroll_frame&) const = default;
	};

	/// \brief Describes one applied two-pane comparison scroll frame.
	struct comparison_scroll_frame
	{
		core::file_comparison_digest              Comparison;
		qreal                                     Position = 0.0;
		std::optional<core::alignment_segment_id> Segment;
		comparison_pane_scroll_frame              Left;
		comparison_pane_scroll_frame              Right;

		auto operator<=>(const comparison_scroll_frame&) const = default;
	};

	/// \brief Records bounded controller work for direct interaction evidence.
	struct comparison_scroll_statistics
	{
		std::uint64_t WheelEvents               = 0U;
		std::uint64_t PixelWheelEvents          = 0U;
		std::uint64_t AngleWheelEvents          = 0U;
		std::uint64_t HorizontalEvents          = 0U;
		std::uint64_t ScrollbarInputs           = 0U;
		std::uint64_t FramesApplied             = 0U;
		std::uint64_t ReentrantInputsSuppressed = 0U;
		std::uint64_t FrameSinkFailures         = 0U;
		std::uint64_t RepaintSinkFailures       = 0U;
		std::uint64_t RepaintDeliveries         = 0U;
		std::uint64_t RepaintRequestsCoalesced  = 0U;
	};

	using comparison_scroll_frame_sink
		= std::function<void(const comparison_scroll_frame&)>;
	using comparison_scroll_repaint_sink = std::function<void()>;

	/// \brief Owns the sole retained vertical position for a linked comparison.
	///
	/// \details The position is where the synchronization rail crosses the
	/// comparison. [[BPR-1291]]: its range is only the positions at which some
	/// pane moves. It starts where the first pane would leave its top and ends
	/// where the last pane has its last line at the top, so the panes scroll
	/// past their end as VS Code's do and no step of the shared bar leaves
	/// both still. Past the comparison's end both sides advance together, so
	/// their ends stay joined through the connector.
	class comparison_scroll_controller final : public QObject
	{
	public:
		explicit comparison_scroll_controller(
			QScrollBar&                     aSharedScrollBar,
			comparison_scroll_configuration aConfiguration = { },
			QObject*                        aParent        = nullptr);
		~comparison_scroll_controller() override;

		/// \brief Binds one live Qt input surface and optional pane-local horizontal bar.
		void bind_input_surface(
			comparison_scroll_receiver aReceiver,
			QWidget&                   aSurface,
			QScrollBar*                aHorizontalScrollBar = nullptr);
		/// \brief Binds the focused widget that receives navigation key events.
		void bind_key_surface(
			comparison_scroll_receiver aReceiver,
			QWidget&                   aSurface);
		/// \brief Supplies the guarded, exception-contained frame callback.
		void set_frame_sink(comparison_scroll_frame_sink aSink);
		/// \brief Supplies a coalesced, exception-contained repaint callback.
		void set_repaint_sink(comparison_scroll_repaint_sink aSink);

		/// \brief Replaces geometry, retaining a same-comparison segment fraction.
		void set_axis(measured_alignment_axis aAxis);
		/// \brief Clears measured geometry and disables shared scrolling.
		void clear_axis();
		/// \brief Disables geometry-dependent input while retaining the remeasurement anchor.
		void suspend_axis();
		/// \brief Applies an exact comparison-space target after clamping.
		void set_position(qreal aPosition);
		/// \brief Applies a signed comparison-space delta without integer round-trip.
		void scroll_by(qreal aDelta);
		/// \brief Clears transient touchpad discrimination after a presentation change.
		void reset_pixel_gesture() noexcept;
		/// \brief Aligns the receiving pane through the shared measured authority.
		void align_pane_origin(core::diff_side aSide);
		void page_up();
		void page_down();
		void home();
		void end();
		/// \brief Places a stable segment position on the synchronization rail.
		[[nodiscard]] auto reveal_segment(
			core::alignment_segment_id aSegment,
			qreal                      aFraction = 0.0) -> bool;
		/// \brief Scrolls so one side's viewport top sits at a document origin.
		[[nodiscard]] auto reveal_pane_origin(
			core::diff_side aSide,
			qreal           aOrigin) -> bool;

		/// \brief [[BPR-1290]]. Where the bar's thumb stands, from 0 to 1, when
		/// a side's zero-based line is revealed at the rail.
		[[nodiscard]] auto scrollbar_fraction(core::diff_side aSide, std::uint32_t aLine) const noexcept
			-> std::optional<qreal>;
		[[nodiscard]] auto position() const noexcept -> qreal;
		[[nodiscard]] auto minimum_position() const noexcept -> qreal;
		[[nodiscard]] auto maximum_position() const noexcept -> qreal;
		[[nodiscard]] auto current_frame() const noexcept
			-> const std::optional<comparison_scroll_frame>&;
		[[nodiscard]] auto statistics() const noexcept
			-> comparison_scroll_statistics;

	protected:
		[[nodiscard]] auto eventFilter(QObject* aWatched, QEvent* aEvent)
			-> bool override;

	private:
		struct input_binding
		{
			QPointer<QObject>    Surface;
			QPointer<QObject>    KeySurface;
			QPointer<QScrollBar> HorizontalScrollBar;
			qreal                HorizontalResidual = 0.0;
		};
		struct retained_axis_anchor
		{
			core::alignment_segment_id Segment;
			qreal                      Fraction = 0.0;
			/// How far past the comparison's end the position was.
			qreal PastEnd = 0.0;
		};
		struct pending_scrollbar_target
		{
			int   Value    = 0;
			qreal Position = 0.0;
		};

		[[nodiscard]] static auto receiver_index(
			comparison_scroll_receiver aReceiver) noexcept -> std::size_t;
		[[nodiscard]] auto wheel_receiver_for(QObject* aSurface) const noexcept
			-> std::optional<comparison_scroll_receiver>;
		[[nodiscard]] auto key_receiver_for(QObject* aSurface) const noexcept
			-> std::optional<comparison_scroll_receiver>;
		[[nodiscard]] auto retained_anchor() const noexcept
			-> std::optional<retained_axis_anchor>;
		[[nodiscard]] auto position_for_anchor(
			const retained_axis_anchor& aAnchor) const noexcept
			-> std::optional<qreal>;
		[[nodiscard]] auto make_frame(qreal aPosition) const noexcept
			-> std::optional<comparison_scroll_frame>;
		[[nodiscard]] auto make_pane_frame(
			qreal aProjectedY,
			qreal aMaximumOrigin,
			qreal aViewportExtent) const noexcept
			-> comparison_pane_scroll_frame;
		[[nodiscard]] auto rail_extent(core::diff_side aSide) const noexcept
			-> qreal;
		[[nodiscard]] auto position_reaching(
			core::diff_side aSide,
			qreal           aDocumentY) const noexcept -> qreal;
		[[nodiscard]] auto clamp_position(qreal aPosition) const noexcept
			-> qreal;
		void               update_live_range() noexcept;
		void               apply_position(qreal aPosition, bool aForce);
		void               update_scrollbar_projection();
		void               schedule_repaint();
		void               deliver_repaint();
		void               drain_pending_axis_change();
		void               schedule_pending_axis_change();
		[[nodiscard]] auto handle_wheel_event(
			comparison_scroll_receiver aReceiver,
			QWheelEvent&               aEvent) -> bool;
		[[nodiscard]] auto handle_key_event(QKeyEvent& aEvent) -> bool;
		void               handle_scrollbar_action(int aAction);
		[[nodiscard]] auto apply_horizontal_delta(
			input_binding& aBinding,
			qreal          aDelta) -> bool;
		[[nodiscard]] auto angle_step_extent() const noexcept -> qreal;
		[[nodiscard]] auto page_extent() const noexcept -> qreal;
		[[nodiscard]] auto single_step_extent() const noexcept -> qreal;
		[[nodiscard]] auto scrollbar_value_for(qreal aPosition) const noexcept
			-> int;
		[[nodiscard]] auto position_for_scrollbar_value(int aValue) const noexcept
			-> qreal;

		QPointer<QScrollBar>                              SharedScrollBar;
		comparison_scroll_configuration                   Configuration;
		std::array<input_binding, 3>                      InputBindings;
		comparison_scroll_frame_sink                      FrameSink;
		comparison_scroll_repaint_sink                    RepaintSink;
		std::optional<measured_alignment_axis>            Axis;
		std::map<core::alignment_segment_id, std::size_t> SegmentIndexes;
		std::optional<comparison_scroll_frame>            CurrentFrame;
		std::optional<measured_alignment_axis>            PendingAxis;
		std::optional<pending_scrollbar_target>           PendingScrollbarTarget;
		qreal                                             Q                   = 0.0;
		qreal                                             MinimumQ            = 0.0;
		qreal                                             MaximumQ            = 0.0;
		bool                                              AxisSuspended       = false;
		bool                                              ApplyingFrame       = false;
		bool                                              RepaintPending      = false;
		bool                                              ClearAxisPending    = false;
		bool                                              DrainingAxisChange  = false;
		bool                                              AxisChangeScheduled = false;
		pixel_scroll_gesture                              PixelGesture;
		comparison_scroll_statistics                      Statistics;
	};
} // namespace bpr::qt

#endif // BPR_SRC_ADAPTERS_QT_INCLUDE_BPR_QT_COMPARISON_SCROLL_CONTROLLER_HPP
