#include "../support/resize_cursor_hold_check.hpp"
#include "diff_publication_fixture.hpp"
#include "product_diff.hpp"

#include "bpr/core/appearance_contrast.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/qt/anchored_tooltip.hpp"
#include "bpr/qt/diff_workbench.hpp"
#include "bpr/qt/line_details_pane.hpp"
#include "bpr/qt/type_preview.hpp"

#include <QAbstractButton>
#include <QAccessible>
#include <QApplication>
#include <QBackingStore>
#include <QElapsedTimer>
#include <QFont>
#include <QFontMetrics>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <QHelpEvent>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLocale>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPointingDevice>
#include <QProxyStyle>
#include <QScreen>
#include <QScrollBar>
#include <QSplitter>
#include <QSplitterHandle>
#include <QString>
#include <QStyle>
#include <QSysInfo>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextLayout>
#include <QThread>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
	using namespace bpr;

	auto expect(const bool aCondition, const std::string_view aMessage) -> void
	{
		if (!aCondition) {
			std::cerr << "FAILED: " << aMessage << '\n';
			std::exit(EXIT_FAILURE);
		}
	}

	[[nodiscard]] auto close(
		const qreal aLeft,
		const qreal aRight,
		const qreal aTolerance = 0.000'001) noexcept -> bool
	{
		return std::abs(aLeft - aRight) <= aTolerance;
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		auto bytes    = std::array<std::uint8_t, 32U>{ };
		bytes.front() = aSeed;
		bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	[[nodiscard]] constexpr auto blob(const std::uint8_t aSeed)
		-> core::git_blob_oid
	{
		auto bytes = std::array<std::uint8_t, 32U>{ };
		for (std::size_t index = 0U; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(aSeed + index);
		}
		return core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto key(
		const core::diff_options& aOptions,
		const std::uint8_t        aSeed) -> core::file_comparison_key
	{
		const auto digest = core::options_digest(aOptions);
		expect(digest.has_value(), "fixture options normalize");
		const auto scope     = opaque<core::pull_request_authority_scope_id>(11U);
		const auto authority = core::pull_request_authority{
			.Profile         = core::host_profile_id{ 10U },
			.ProfileRevision = core::github_profile_revision{ 20U },
			.Host            = core::git_host_id{ 30U },
			.HostAuthority   = opaque<core::host_authority_fingerprint>(13U),
			.Account         = core::github_account_id{ 40U },
			.Viewer          = core::github_viewer_actor_id{ 50U },
			.Repository      = core::repository_id{ 101U },
			.Cache           = opaque<core::repository_cache_scope_id>(14U),
			.PullRequest     = core::pull_request_id{ 202U },
			.Number          = core::pull_request_number{ 17U },
			.Scope           = scope,
		};
		return {
			.Authority   = authority,
			.Repository  = authority.Repository,
			.PullRequest = authority.PullRequest,
			.LeftAnchor  = {
				.Authority   = scope,
				.Incarnation = opaque<core::epoch_catalog_incarnation_id>(12U),
				.Kind        = core::comparison_anchor_kind::epoch_base,
				.Epoch       = core::epoch_ordinal{ static_cast<std::int64_t>(1U + aSeed) },
			},
			.RightAnchor = {
				.Authority   = scope,
				.Incarnation = opaque<core::epoch_catalog_incarnation_id>(12U),
				.Kind        = core::comparison_anchor_kind::epoch_head,
				.Epoch       = core::epoch_ordinal{ static_cast<std::int64_t>(2U + aSeed) },
			},
			.LeftBlob  = blob(static_cast<std::uint8_t>(21U + aSeed)),
			.RightBlob = blob(static_cast<std::uint8_t>(51U + aSeed)),
			.File      = opaque<core::file_identity>(static_cast<std::uint8_t>(31U + aSeed)),
			.Options   = *digest,
			.Algorithm = core::supported_diff_algorithm_revision,
		};
	}

	[[nodiscard]] auto bytes(const std::string_view aText)
		-> std::vector<std::uint8_t>
	{
		return { aText.begin(), aText.end() };
	}

	[[nodiscard]] auto computed(
		const std::string_view    aLeft,
		const std::string_view    aRight,
		const core::diff_options& aOptions,
		const std::uint8_t        aSeed) -> core::computed_file_diff
	{
		auto       left    = bytes(aLeft);
		auto       right   = bytes(aRight);
		const auto outcome = bpr::tests::product_file_diff({
			.Key        = key(aOptions, aSeed),
			.Options    = aOptions,
			.LeftBytes  = std::span<const std::uint8_t>{ left },
			.RightBytes = std::span<const std::uint8_t>{ right },
		});
		expect(outcome.Reason == core::diff_reason_code::none, "fixture diff computes");
		expect(outcome.Result.has_value(), "fixture diff publishes");
		return *outcome.Result;
	}

	[[nodiscard]] auto context(
		const core::computed_file_diff& aDiff,
		const std::uint64_t             aRequest,
		const std::uint64_t             aGeneration) -> core::diff_request_context
	{
		return {
			.Request            = core::request_id{ aRequest },
			.ApplicationSession = core::application_session_id{ 1U },
			.Key                = aDiff.Alignment.Key,
			.Generation         = core::request_generation{ aGeneration },
		};
	}

	[[nodiscard]] auto representative_text() -> std::pair<std::string, std::string>
	{
		auto left  = std::string(240U, 'L') + "-long-common-line\n";
		auto right = left;
		for (std::size_t line = 0U; line < 28U; ++line) {
			const auto common = "prefix-" + std::to_string(line) + "\n";
			left += common;
			right += common;
		}
		left += "old-a\nold-b\nold-c\nold-d\n";
		right += "new-a\nnew-b\nnew-c\nnew-d\nnew-e\nnew-f\nnew-g\n";
		for (std::size_t line = 0U; line < 12U; ++line) {
			const auto common = "middle-a-" + std::to_string(line) + "\n";
			left += common;
			right += common;
		}
		left += "delete-a\ndelete-b\ndelete-c\ndelete-d\n";
		for (std::size_t line = 0U; line < 12U; ++line) {
			const auto common = "middle-b-" + std::to_string(line) + "\n";
			left += common;
			right += common;
		}
		right += "insert-a\ninsert-b\ninsert-c\ninsert-d\ninsert-e\n";
		for (std::size_t line = 0U; line < 50U; ++line) {
			const auto common = "tail-" + std::to_string(line) + "\n";
			left += common;
			right += common;
		}
		return { std::move(left), std::move(right) };
	}

	[[nodiscard]] auto large_comparison_text(
		const std::size_t aLines,
		const std::size_t aChangeInterval)
		-> std::pair<std::string, std::string>
	{
		auto left  = std::string{ };
		auto right = std::string{ };
		left.reserve(aLines * 24U);
		right.reserve(aLines * 24U);
		for (std::size_t line = 0U; line < aLines; ++line) {
			const auto suffix = std::to_string(line) + '\n';
			if (line % aChangeInterval == 0U) {
				left += "earlier-change-" + suffix;
				right += "later-change-" + suffix;
			} else {
				left += "stable-context-" + suffix;
				right += "stable-context-" + suffix;
			}
		}
		return { std::move(left), std::move(right) };
	}

	[[nodiscard]] auto large_insertion_text()
		-> std::pair<std::string, std::string>
	{
		auto left  = std::string{ };
		auto right = std::string{ };
		for (std::size_t line = 0U; line < 32U; ++line) {
			const auto common = "leading-context-" + std::to_string(line) + '\n';
			left += common;
			right += common;
		}
		for (std::size_t line = 0U; line < 5'000U; ++line) {
			right += "large-insertion-" + std::to_string(line) + '\n';
		}
		for (std::size_t line = 0U; line < 32U; ++line) {
			const auto common = "trailing-context-" + std::to_string(line) + '\n';
			left += common;
			right += common;
		}
		return { std::move(left), std::move(right) };
	}

	[[nodiscard]] auto dense_move_text()
		-> std::pair<std::string, std::string>
	{
		auto           left  = std::string{ };
		auto           right = std::string{ };
		constexpr auto moves = 6U;
		for (auto line = 0U; line < moves; ++line) {
			left += "moved-scale-" + std::to_string(line) + '\n';
		}
		for (auto line = 0U; line < 4U; ++line) {
			const auto stable = "move-stable-" + std::to_string(line) + '\n';
			left += stable;
			right += stable;
		}
		for (auto line = moves; line > 0U; --line) {
			right += "moved-scale-" + std::to_string(line - 1U) + '\n';
		}
		return { std::move(left), std::move(right) };
	}

	[[nodiscard]] auto segment_of_kind(
		const core::computed_file_diff& aDiff,
		const core::alignment_kind      aKind,
		const std::size_t               aOccurrence = 0U)
		-> const core::alignment_segment&
	{
		auto occurrence = std::size_t{ 0U };
		for (const auto& segment : aDiff.Alignment.Segments) {
			if (segment.Kind == aKind && occurrence++ == aOccurrence) {
				return segment;
			}
		}
		throw std::runtime_error{ "fixture segment kind absent" };
	}

	void process_events()
	{
		for (auto count = 0; count < 4; ++count) {
			QApplication::processEvents();
		}
	}

	void settle_geometry(qt::diff_workbench& aWorkbench, const qint64 aDeadlineMs = 5'000)
	{
		auto timer = QElapsedTimer{ };
		timer.start();
		do {
			QApplication::processEvents();
		} while (aWorkbench.geometry_pending() && timer.elapsed() < aDeadlineMs);
		expect(!aWorkbench.geometry_pending(), "native measurement completes within the local test deadline");
		process_events();
	}

	struct accessible_announcement
	{
		QString                             Message;
		QObject*                            Target = nullptr;
		QAccessible::AnnouncementPoliteness Politeness
			= QAccessible::AnnouncementPoliteness::Polite;
	};

	std::vector<accessible_announcement>* AnnouncementSink = nullptr;

	void capture_accessibility_update(QAccessibleEvent* const aEvent)
	{
		if (AnnouncementSink == nullptr || aEvent == nullptr
			|| aEvent->type() != QAccessible::Announcement) {
			return;
		}
		const auto* const announcement
			= static_cast<const QAccessibleAnnouncementEvent*>(aEvent);
		AnnouncementSink->push_back({
			.Message    = announcement->message(),
			.Target     = aEvent->object(),
			.Politeness = announcement->politeness(),
		});
	}

	class scoped_accessibility_capture final
	{
	public:
		explicit scoped_accessibility_capture(
			std::vector<accessible_announcement>& aAnnouncements)
			: Previous{
				QAccessible::installUpdateHandler(capture_accessibility_update)
			}
		{
			AnnouncementSink = &aAnnouncements;
		}

		~scoped_accessibility_capture()
		{
			AnnouncementSink = nullptr;
			static_cast<void>(QAccessible::installUpdateHandler(Previous));
		}

		scoped_accessibility_capture(const scoped_accessibility_capture&) = delete;
		auto operator=(const scoped_accessibility_capture&)
			-> scoped_accessibility_capture&                         = delete;
		scoped_accessibility_capture(scoped_accessibility_capture&&) = delete;
		auto operator=(scoped_accessibility_capture&&)
			-> scoped_accessibility_capture& = delete;

	private:
		QAccessible::UpdateHandler Previous = nullptr;
	};

	[[nodiscard]] auto announcement_count(
		const std::vector<accessible_announcement>& aAnnouncements,
		const QObject*                              aTarget,
		const QString&                              aMessage) -> std::size_t
	{
		return static_cast<std::size_t>(std::ranges::count_if(
			aAnnouncements,
			[aTarget, &aMessage](const accessible_announcement& aAnnouncement) {
				return aAnnouncement.Target == aTarget
					&& aAnnouncement.Message == aMessage
					&& aAnnouncement.Politeness
					== QAccessible::AnnouncementPoliteness::Polite;
			}));
	}

	struct rendered_pane_state
	{
		int    ScrollValue = 0;
		QImage Pixels;
	};

	[[nodiscard]] auto rendered_state(QPlainTextEdit& aPane)
		-> rendered_pane_state
	{
		process_events();
		return {
			.ScrollValue = aPane.verticalScrollBar()->value(),
			.Pixels      = aPane.viewport()->grab().toImage(),
		};
	}

	[[nodiscard]] auto send_wheel(
		QWidget&      aSurface,
		const QPoint& aPixel,
		const QPoint& aAngle = { }) -> bool
	{
		auto event = QWheelEvent{
			QPointF{ 8.0, 8.0 },
			QPointF{ 8.0, 8.0 },
			aPixel,
			aAngle,
			Qt::NoButton,
			Qt::NoModifier,
			Qt::NoScrollPhase,
			false,
		};
		event.ignore();
		QCoreApplication::sendEvent(&aSurface, &event);
		return event.isAccepted();
	}

	void send_click(QWidget& aSurface, const QPointF aPosition)
	{
		auto press = QMouseEvent{
			QEvent::MouseButtonPress,
			aPosition,
			aPosition,
			aPosition,
			Qt::LeftButton,
			Qt::LeftButton,
			Qt::NoModifier,
		};
		auto release = QMouseEvent{
			QEvent::MouseButtonRelease,
			aPosition,
			aPosition,
			aPosition,
			Qt::LeftButton,
			Qt::NoButton,
			Qt::NoModifier,
		};
		QCoreApplication::sendEvent(&aSurface, &press);
		QCoreApplication::sendEvent(&aSurface, &release);
	}

	void send_key(QWidget& aSurface, const Qt::Key aKey)
	{
		auto press   = QKeyEvent{ QEvent::KeyPress, aKey, Qt::NoModifier };
		auto release = QKeyEvent{ QEvent::KeyRelease, aKey, Qt::NoModifier };
		QCoreApplication::sendEvent(&aSurface, &press);
		QCoreApplication::sendEvent(&aSurface, &release);
	}

	/// \brief Returns the gutter widget a pane numbers its own lines in.
	[[nodiscard]] auto gutter_of(QPlainTextEdit* const aPane) -> QWidget*
	{
		expect(aPane != nullptr, "the pane whose gutter is wanted exists");
		auto* const found = aPane->findChild<QWidget*>(
			QStringLiteral("diffSourceLineNumbers"));
		expect(found != nullptr, "the pane composes a line-number gutter");
		return found;
	}

	/// \brief Counts the distinct colours in one region of a raster.
	///
	/// \details A gutter that reserved its width and painted nothing is a
	/// single flat colour. Anything drawn in it is a second one.
	[[nodiscard]] auto distinct_colors(const QImage& aImage, const QRect& aRect)
		-> std::size_t
	{
		auto       seen    = std::set<QRgb>{ };
		const auto bounded = aRect.intersected(aImage.rect());
		for (auto y = bounded.top(); y <= bounded.bottom(); ++y) {
			for (auto x = bounded.left(); x <= bounded.right(); ++x) {
				seen.insert(aImage.pixel(x, y));
			}
		}
		return seen.size();
	}

	/// \brief Proves the panes number their own lines, visibly.
	///
	/// \details BPR-0414 was rejected because the owner could not tell which
	/// rendered row was line 42. Every automated check of that line passed at
	/// the time, because they all read the model and none asked whether a
	/// number had been painted. This asks.
	void test_the_panes_number_their_own_lines()
	{
		const auto [leftText, rightText] = representative_text();
		auto       options               = core::diff_options{ };
		const auto diff                  = computed(leftText, rightText, options, 0U);
		auto       workbench             = qt::diff_workbench{ };
		workbench.resize(1'260, 820);
		workbench.show();
		process_events();
		// The workbench admits a result only for its exact current loading
		// context, so a publish without one leaves the panes empty and every
		// check below would pass against a one-line document.
		const auto selected = context(diff, 1U, 1U);
		workbench.set_loading(selected);
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		// The shared bar moves the panes only once their measured axis exists,
		// and measuring is sliced across event turns ([[BPR-1123]]).
		settle_geometry(workbench);
		expect(workbench.left_pane()->blockCount() > 1
				   && workbench.right_pane()->blockCount() > 1,
			   "the panes really hold the fixture's source");

		for (auto* const source : { workbench.left_pane(), workbench.right_pane() }) {
			auto* const gutter = gutter_of(source);
			expect(gutter->isVisible() && gutter->width() > 0,
				   "each pane shows a gutter with real width");
			// The code has to actually move aside. A gutter painted over the
			// unshrunk viewport would sit on top of the source, which is the
			// failure this geometry check exists to refuse.
			expect(gutter->geometry().right() <= source->viewport()->x(),
				   "the gutter sits entirely left of the text viewport rather "
				   "than over it");
			expect(source->viewport()->x() >= gutter->width(),
				   "the pane reserved the gutter's width from its text");

			// The gutter's own raster, not the pane's. Sampling a strip of the
			// pane catches its frame border, and one border line is enough to
			// make an empty gutter look painted.
			const auto column = gutter->grab().toImage();
			expect(distinct_colors(column, column.rect()) > 1U,
				   "the gutter has numbers painted in it rather than a "
				   "reserved but empty strip");
		}

		// Both documents start at line one, so at the top of the comparison
		// the two gutters agree. That is not a renumbering; it is what
		// independent numbering looks like before anything one-sided has
		// gone past.
		workbench.scroll_controller()->home();
		process_events();
		const auto leftAtTop  = gutter_of(workbench.left_pane())->grab().toImage();
		const auto rightAtTop = gutter_of(workbench.right_pane())->grab().toImage();
		expect(leftAtTop.size() == rightAtTop.size(),
			   "both gutters are the same size for this comparison");
		expect(leftAtTop == rightAtTop,
			   "both panes begin at line one");

		// RFC-BPR-0004: each pane numbers its own document and is never
		// renumbered to match the other side. Scroll past the fixture's
		// one-sided blocks and the same screen row carries different numbers,
		// which is the whole reason a reviewer needs them.
		auto* const shared = workbench.shared_scrollbar();
		shared->setValue(shared->maximum());
		process_events();
		// [[BPR-1123]]. The shared bar's end is both panes' end. A jump made
		// before the measured axis existed was dropped, and the bar settled
		// wherever the selection was later revealed, a quarter of the way down.
		const auto atEnd = [](const QPlainTextEdit* aPane) {
			return aPane->verticalScrollBar()->value() == aPane->verticalScrollBar()->maximum();
		};
		expect(shared->value() == shared->maximum() && atEnd(workbench.left_pane()) && atEnd(workbench.right_pane()),
			   "the shared bar's end brings both panes to their own last page");
		const auto leftScrolled
			= gutter_of(workbench.left_pane())->grab().toImage();
		const auto rightScrolled
			= gutter_of(workbench.right_pane())->grab().toImage();
		expect(leftScrolled != leftAtTop,
			   "scrolling changes the numbers on screen, so they follow the "
			   "document rather than decorating the viewport");
		expect(leftScrolled != rightScrolled,
			   "past a one-sided block the two panes show different numbers on "
			   "the same row, because neither is renumbered to match the other");
	}

	/// \brief Proves the gutter is sized to the document it numbers.
	/// \brief Proves hiding the numbers gives their width back to the code.
	/// [[BPR-1432]]. Git and GitHub end a line only at LF, so a lone CR is
	/// content: it must not start a Qt block, or block numbers would stop being
	/// the decoded line numbers that comment anchors send to GitHub.
	void test_a_lone_cr_is_shown_inside_its_line()
	{
		const auto diff = computed("one\ntwo\rthree\nfour\n", "one\ntwo\rthree\nfour\r\n", core::diff_options{ }, 41U);
		expect(diff.LeftDocument && diff.LeftDocument->Lines.size() == 3U && diff.RightDocument && diff.RightDocument->Lines.size() == 3U,
			   "a lone CR does not end a decoded line");
		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'260, 820);
		workbench.show();
		process_events();
		const auto selected = context(diff, 41U, 1U);
		workbench.set_loading(selected);
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		settle_geometry(workbench);
		for (auto* const source : { workbench.left_pane(), workbench.right_pane() }) {
			// Three lines plus the empty block after the final newline.
			expect(source->blockCount() == 4, "each pane holds one block per decoded line");
			expect(source->document()->findBlockByNumber(1).text() == QStringLiteral("two\u240Dthree"),
				   "the lone CR shows as its symbol inside line two");
		}
	}

	void test_the_viewport_fill_survives_a_style_clearing_it()
	{
		auto workbench = qt::diff_workbench{ };
		// [[BPR-1287]]. The panes are shown once a comparison is chosen;
		// before that the workbench shows its idle direction instead.
		workbench.set_loading(context(computed("a\n", "b\n", core::diff_options{ }, 0U), 1U, 1U));
		workbench.resize(1'260, 820);
		workbench.show();
		process_events();
		for (auto* const source : { workbench.left_pane(), workbench.right_pane() }) {
			expect(source->viewport()->autoFillBackground(),
				   "a shown pane viewport fills with its palette base");
			// QWindows11Style clears this attribute during polish so the
			// style can paint its own panel fill, and QPalette::Base is then
			// never painted at all (BPR-0475). The restore is portable; the
			// clearing style is not, so the clear is simulated here.
			source->viewport()->setAutoFillBackground(false);
			auto styleChange = QEvent{ QEvent::StyleChange };
			QApplication::sendEvent(source, &styleChange);
			expect(source->viewport()->autoFillBackground(),
				   "a style re-polish cannot leave the viewport unfilled");
			source->viewport()->setAutoFillBackground(false);
			source->hide();
			source->show();
			process_events();
			expect(source->viewport()->autoFillBackground(),
				   "a fresh show restores the fill after any polish");
		}
	}

	void test_hiding_the_numbers_returns_the_gutter_width_to_the_code()
	{
		const auto [leftText, rightText] = representative_text();
		auto       options               = core::diff_options{ };
		const auto diff                  = computed(leftText, rightText, options, 0U);
		auto       workbench             = qt::diff_workbench{ };
		workbench.resize(1'260, 820);
		workbench.show();
		process_events();
		const auto selected = context(diff, 1U, 1U);
		workbench.set_loading(selected);
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		process_events();
		expect(workbench.left_pane()->blockCount() > 1
				   && workbench.right_pane()->blockCount() > 1,
			   "the panes really hold the fixture's source");

		// Measured with the numbers on, so the comparison below is against a
		// real reserved width rather than against zero twice.
		auto shownWidth  = std::vector<int>{ };
		auto shownOrigin = std::vector<int>{ };
		for (auto* const source : { workbench.left_pane(), workbench.right_pane() }) {
			shownWidth.push_back(gutter_of(source)->width());
			shownOrigin.push_back(source->viewport()->x());
			expect(shownWidth.back() > 0 && shownOrigin.back() > 0,
				   "the gutter starts with real width and pushes the text over");
		}

		workbench.set_line_numbers_visible(false);
		process_events();
		for (auto        index = std::size_t{ 0U };
			 auto* const source : { workbench.left_pane(), workbench.right_pane() }) {
			auto* const gutter = gutter_of(source);
			expect(!gutter->isVisible(),
				   "the gutter is hidden rather than merely blank");
			expect(gutter->width() == 0, "the gutter reserves nothing");
			// The assertion that makes this more than a visibility check. A
			// hidden widget that still reserved its width would leave an empty
			// margin and the code would have gained nothing, so the claim is
			// the exact distance the text moved. It is measured as a delta
			// because the pane's frame also contributes to the viewport's
			// origin, and the frame is not the gutter's to give back.
			expect(shownOrigin[index] - source->viewport()->x()
					   == shownWidth[index],
				   "the text moved left by exactly the gutter's width, so the "
				   "reserved space went back to the code");
			++index;
		}

		workbench.set_line_numbers_visible(true);
		process_events();
		for (auto        index = std::size_t{ 0U };
			 auto* const source : { workbench.left_pane(), workbench.right_pane() }) {
			auto* const gutter = gutter_of(source);
			expect(gutter->isVisible(), "the gutter comes back");
			expect(gutter->width() == shownWidth[index]
					   && source->viewport()->x() == shownOrigin[index],
				   "turning them back on restores the exact geometry they had, "
				   "so the toggle is reversible rather than approximately so");
			const auto column = gutter->grab().toImage();
			expect(distinct_colors(column, column.rect()) > 1U,
				   "the numbers are painted again, not just the space for them");
			++index;
		}
	}

	/// \brief [[BPR-1335]]. The pad either side of the widest number.
	constexpr auto ExpectedGutterPadding = 4;

	[[nodiscard]] auto decimal_digits(std::uint32_t aNumber) -> int
	{
		auto digits = 1;
		for (; aNumber >= 10U; aNumber /= 10U) {
			++digits;
		}
		return digits;
	}

	[[nodiscard]] auto widest_digit(const QFontMetrics& aMetrics) -> int
	{
		auto widest = 0;
		for (const auto digit : QStringLiteral("0123456789")) {
			widest = std::max(widest, aMetrics.horizontalAdvance(digit));
		}
		return widest;
	}

	/// \brief [[BPR-1335]]. The width a split pane's gutter should have.
	[[nodiscard]] auto expected_gutter(const QPlainTextEdit& aPane, const std::uint32_t aLongest) -> int
	{
		return decimal_digits(aLongest) * widest_digit(aPane.fontMetrics()) + 2 * ExpectedGutterPadding;
	}

	/// \brief [[BPR-1335]]. Where the first ink is in a row band of a grab,
	/// in logical pixels, or -1 when the band is only background.
	[[nodiscard]] auto first_ink_column(const QImage& aImage, const int aTop, const int aBottom) -> double
	{
		const auto ratio      = aImage.devicePixelRatio();
		const auto background = aImage.pixel(aImage.width() - 1, 0);
		const auto top        = std::max(0, static_cast<int>(std::floor(aTop * ratio)));
		const auto bottom     = std::min(aImage.height(), static_cast<int>(std::ceil(aBottom * ratio)));
		for (auto x = 0; x < aImage.width(); ++x) {
			for (auto y = top; y < bottom; ++y) {
				if (aImage.pixel(x, y) != background) {
					return x / ratio;
				}
			}
		}
		return -1.0;
	}

	/// \brief [[BPR-1335]]. The gutter is as wide as its longest number.
	///
	/// \details The owner, 2026-10-08: "Our line numbers pane looks to be
	/// winder then necessary. It should size to the longest line number".
	/// These files end without a newline, so their last line is the longest
	/// number. One ending in a newline also numbers the empty line after it,
	/// as VS Code does, and the last case checks the gutter makes room for it.
	void test_the_gutter_sizes_to_its_longest_number()
	{
		for (const auto lines : { 9U, 99U, 1'557U, 100'000U }) {
			auto left  = std::string{ };
			auto right = std::string{ };
			for (auto line = 1U; line <= lines; ++line) {
				const auto end = std::string{ line == lines ? "" : "\n" };
				left += "line " + std::to_string(line) + end;
				right += (line == (lines + 1U) / 2U ? "changed " : "line ") + std::to_string(line) + end;
			}
			const auto diff      = std::make_shared<const core::computed_file_diff>(computed(left, right, { }, 61U));
			auto       workbench = qt::diff_workbench{ };
			workbench.resize(1'260, 820);
			workbench.show();
			process_events();
			const auto selected = context(*diff, 1U, 1U);
			workbench.set_loading(selected);
			workbench.publish(bpr::qt::test::prepare_publication({ .Context = selected, .Result = diff }));
			// A 100000-line file is filled and measured over many turns, which
			// a loaded station can stretch past the shared five seconds.
			settle_geometry(workbench, 60'000);
			const auto label = std::to_string(lines) + "-line file";

			auto widths = std::vector<int>{ };
			for (auto* const source : { workbench.left_pane(), workbench.right_pane() }) {
				expect(source->blockCount() == static_cast<int>(lines), label + ": the pane holds every line of the file");
				const auto expected = expected_gutter(*source, lines);
				const auto actual   = gutter_of(source)->width();
				std::cout << "bpr-1335 " << label << " scale=" << workbench.devicePixelRatioF() << " digit=" << widest_digit(source->fontMetrics())
						  << " gutter=" << actual << " expected=" << expected << '\n';
				expect(actual == expected, label + ": the gutter is the longest number's digits times the widest digit, plus its padding");
				expect(source->viewport()->x() - source->contentsRect().left() == expected, label + ": the code starts exactly where the gutter ends");
				widths.push_back(actual);
			}

			// Showing the threads adds no column: a thread tints its number.
			workbench.set_thread_markers(true);
			process_events();
			for (auto index = std::size_t{ 0U }; auto* const source : { workbench.left_pane(), workbench.right_pane() }) {
				expect(gutter_of(source)->width() == widths[index], label + ": showing the threads keeps the gutter its numbers' width");
				++index;
			}
			workbench.set_thread_markers(false);
			// The narrower viewport is re-measured over several turns, and the
			// rebuild reveals the selected change again when it lands.
			settle_geometry(workbench, 60'000);

			// Scrolling to the end shows the longest number and changes nothing.
			auto* const pane   = workbench.right_pane();
			auto* const gutter = gutter_of(pane);
			const auto  origin = pane->viewport()->x();
			workbench.scroll_controller()->end();
			settle_geometry(workbench, 60'000);
			expect(gutter->width() == widths[1] && pane->viewport()->x() == origin, label + ": scrolling never moves the code sideways");
			const auto last = pane->document()->findBlockByNumber(static_cast<int>(lines) - 1);
			const auto row  = pane->cursorRect(QTextCursor{ last });
			expect(row.bottom() > 0 && row.top() < pane->viewport()->height(), label + ": the last line is on screen");
			const auto image = gutter->grab().toImage();
			const auto ink   = first_ink_column(image, row.top(), row.bottom() + 1);
			expect(ink >= ExpectedGutterPadding - 0.5 && ink < ExpectedGutterPadding + widest_digit(pane->fontMetrics()),
				   label + ": the longest number is painted whole, starting in its first digit's cell");

			// A different font is a different digit, and the gutter follows it.
			auto font = pane->font();
			font.setPointSizeF(font.pointSizeF() * 2.0);
			pane->setFont(font);
			process_events();
			expect(gutter->width() == expected_gutter(*pane, lines) && gutter->width() > widths[1],
				   label + ": a larger font widens the gutter to the new digit");
			expect(pane->viewport()->x() - pane->contentsRect().left() == gutter->width(), label + ": and the code moves with it");

			// One pane numbers both sides, each column sized to the larger
			// number and to its "base"/"head" heading.
			workbench.set_one_pane_enabled(true);
			process_events();
			auto* const unified = workbench.findChild<QPlainTextEdit*>(QStringLiteral("diffUnifiedSource"));
			const auto  metrics = unified->fontMetrics();
			const auto  column  = std::max({ decimal_digits(lines) * widest_digit(metrics),
											 metrics.horizontalAdvance(QStringLiteral("base")),
											 metrics.horizontalAdvance(QStringLiteral("head")) })
				+ 2 * ExpectedGutterPadding;
			std::cout << "bpr-1335 " << label << " one-pane gutter=" << gutter_of(unified)->width() << " expected=" << 2 * column << '\n';
			expect(gutter_of(unified)->width() == 2 * column, label + ": one pane's two columns are sized to its longest number");
			workbench.hide();
		}

		// A 99-line file ending in a newline numbers its empty last line 100.
		auto text = std::string{ };
		for (auto line = 1U; line <= 99U; ++line) {
			text += "line " + std::to_string(line) + '\n';
		}
		const auto diff      = std::make_shared<const core::computed_file_diff>(computed(text, text + "added\n", { }, 62U));
		auto       workbench = qt::diff_workbench{ };
		workbench.resize(1'260, 820);
		workbench.show();
		process_events();
		const auto selected = context(*diff, 1U, 1U);
		workbench.set_loading(selected);
		workbench.publish(bpr::qt::test::prepare_publication({ .Context = selected, .Result = diff }));
		settle_geometry(workbench, 60'000);
		expect(workbench.left_pane()->blockCount() == 100 && gutter_of(workbench.left_pane())->width() == expected_gutter(*workbench.left_pane(), 100U),
			   "the empty line after a final newline is numbered, and the gutter makes room for its number");
	}

	/// \brief [[BPR-1335]]. A thread's line number is tinted, and the head
	/// pane's "+" is GitHub's blue button at the start of the hovered line's
	/// code. Neither takes the number's place, and the gutter has no column.
	///
	/// \details The owner chose the tinted number and rejected a "+" that
	/// replaced the number (session 3c9017b5): "I like option 1 but I don't
	/// like the + replacing the line number, i think github actual does it
	/// better". With `BPR_1335_RENDER_DIR` set, each mode is rendered for the
	/// look: the "+" on line 2, then a drag on it from line 2 to line 6.
	void test_tinted_numbers_and_the_add_button()
	{
		auto left  = std::string{ };
		auto right = std::string{ };
		for (auto line = 1U; line <= 140U; ++line) {
			const auto text = "\tconst auto value_" + std::to_string(line) + " = compute(" + std::to_string(line) + ");\n";
			left += text;
			right += line == 12U ? "\tconst auto value_12 = compute_twice(12);\n" : text;
			if (line == 30U) {
				right += "\tlog(value_30);\n";
			}
		}
		const auto diff      = std::make_shared<const core::computed_file_diff>(computed(left, right, { }, 63U));
		const auto directory = qEnvironmentVariable("BPR_1335_RENDER_DIR");
		for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark }) {
			const auto name      = std::string{ mode == core::appearance_mode::dark ? "dark" : "light" };
			auto       workbench = qt::diff_workbench{ };
			auto       profile   = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			profile.apply_to(workbench);
			workbench.adopt_appearance(profile);
			profile.apply_code_font_to(workbench);
			workbench.resize(1'260, 520);
			workbench.show();
			process_events();
			const auto selected = context(*diff, 2U, 2U);
			workbench.set_loading(selected);
			workbench.publish(bpr::qt::test::prepare_publication({ .Context = selected, .Result = diff }));
			settle_geometry(workbench);
			workbench.scroll_controller()->home();
			process_events();

			const auto mark = [](const std::uint32_t aLine, const qt::diff_line_mark_kind aKind, const QString& aDescription) {
				auto made        = qt::diff_line_mark{ };
				made.Line        = aLine;
				made.Kind        = aKind;
				made.Description = aDescription;
				made.SourceLine  = aLine;
				return made;
			};
			auto rightMarks = std::vector<qt::diff_line_mark>{ };
			rightMarks.push_back(mark(3U, qt::diff_line_mark_kind::thread, QStringLiteral("Line 4: 2 posts")));
			rightMarks.push_back(mark(7U, qt::diff_line_mark_kind::new_thread, QStringLiteral("Line 8: 1 post, 1 new")));
			rightMarks.push_back(mark(11U, qt::diff_line_mark_kind::thread, QStringLiteral("Line 12: 3 posts, open")));
			rightMarks.back().Open = true;
			rightMarks.push_back(mark(16U, qt::diff_line_mark_kind::thread, QStringLiteral("Line 17: resolved")));
			rightMarks.back().Resolved = true;
			rightMarks.push_back(mark(20U, qt::diff_line_mark_kind::thread, QStringLiteral("Line 21: your draft")));
			rightMarks.back().Draft = true;
			auto leftMarks          = std::vector<qt::diff_line_mark>{ };
			leftMarks.push_back(mark(5U, qt::diff_line_mark_kind::thread, QStringLiteral("Line 6: 1 post")));
			leftMarks.back().Side = core::diff_side::left;
			workbench.set_line_marks(core::diff_side::right, rightMarks);
			workbench.set_line_marks(core::diff_side::left, leftMarks);
			auto pressed = std::vector<std::pair<core::diff_side, std::uint32_t>>{ };
			auto added   = std::vector<std::pair<std::uint32_t, std::uint32_t>>{ };
			workbench.set_line_mark_dispatch([&pressed](const core::diff_side aSide, const std::uint32_t aLine) { pressed.emplace_back(aSide, aLine); });
			// The dispatch is told the first and last lines.
			workbench.set_line_add_dispatch([&added](const std::uint32_t aFirst, const std::uint32_t aLast) { added.emplace_back(aFirst, aLast); });
			workbench.set_thread_markers(true);
			settle_geometry(workbench);
			workbench.scroll_controller()->home();
			process_events();

			auto* const code   = workbench.right_pane();
			auto* const base   = workbench.left_pane();
			auto* const gutter = gutter_of(code);
			expect(gutter->width() == expected_gutter(*code, static_cast<std::uint32_t>(code->blockCount()))
					   && gutter_of(base)->width() == expected_gutter(*base, static_cast<std::uint32_t>(base->blockCount())),
				   name + ": with threads shown, both gutters are their numbers only, with no column");
			const auto gutterY = [&](const core::diff_side aSide, const std::uint32_t aLine) {
				auto* const pane = aSide == core::diff_side::left ? base : code;
				return gutter_of(pane)->mapFrom(&workbench, workbench.line_rect(aSide, aLine).center()).y();
			};
			const auto codePoint = [&](const std::uint32_t aLine, const int aX) {
				return QPoint{ aX, code->viewport()->mapFrom(&workbench, workbench.line_rect(core::diff_side::right, aLine).center()).y() };
			};
			// [[BPR-1335]] v002. The "+" column: the strip the button is
			// drawn in, across the boundary of the number and the code. Four
			// pixels into the code is inside it; the gutter's last pixel is.
			const auto columnPoint = [&](const std::uint32_t aLine) { return codePoint(aLine, 4); };
			const auto pixel       = [](const QImage& aImage, const QPoint aLogical) {
				const auto ratio = aImage.devicePixelRatio();
				return QColor{ aImage.pixel(std::min(aImage.width() - 1, static_cast<int>(aLogical.x() * ratio)),
											std::min(aImage.height() - 1, static_cast<int>(aLogical.y() * ratio))) };
			};
			const auto band = [](const QImage& aImage, const QRect& aLogical) {
				const auto ratio = aImage.devicePixelRatio();
				return aImage.copy(QRect{ qRound(aLogical.left() * ratio), qRound(aLogical.top() * ratio), qRound(aLogical.width() * ratio), qRound(aLogical.height() * ratio) });
			};
			const auto inked = [](const QImage& aImage, const QColor& aGround) {
				auto count = 0;
				for (auto y = 0; y < aImage.height(); ++y) {
					for (auto x = 0; x < aImage.width(); ++x) {
						const auto colour = QColor{ aImage.pixel(x, y) };
						count += std::abs(colour.red() - aGround.red()) + std::abs(colour.green() - aGround.green()) + std::abs(colour.blue() - aGround.blue()) > 90 ? 1 : 0;
					}
				}
				return count;
			};
			const auto move = [](QWidget* const aTarget, const QPoint aPoint, const Qt::MouseButtons aButtons = Qt::NoButton) {
				QMouseEvent event{ QEvent::MouseMove, QPointF{ aPoint }, QPointF{ aTarget->mapToGlobal(aPoint) }, Qt::NoButton, aButtons, Qt::NoModifier };
				QApplication::sendEvent(aTarget, &event);
				process_events();
			};
			const auto mouse = [](QWidget* const aTarget, const QEvent::Type aType, const QPoint aPoint, const QPoint aGlobal) {
				QMouseEvent event{ aType, QPointF{ aPoint }, QPointF{ aGlobal }, Qt::LeftButton, aType == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton, Qt::NoModifier };
				QApplication::sendEvent(aTarget, &event);
				process_events();
			};

			// A thread's number keeps its place; its cell is tinted amber, a
			// new one's blue, and a line without one is not tinted.
			const auto still     = gutter->grab().toImage();
			const auto plainCell = pixel(still, QPoint{ 1, gutterY(core::diff_side::right, 1U) });
			const auto amberCell = pixel(still, QPoint{ 1, gutterY(core::diff_side::right, 3U) });
			const auto blueCell  = pixel(still, QPoint{ 1, gutterY(core::diff_side::right, 7U) });
			std::cout << "bpr-1335 tint " << name << " plain=" << plainCell.name().toStdString() << " thread=" << amberCell.name().toStdString()
					  << " new=" << blueCell.name().toStdString() << '\n';
			expect(amberCell != plainCell && amberCell.red() > amberCell.blue() + 20 && blueCell.blue() > blueCell.red() + 20,
				   name + ": a thread's number cell is tinted amber, a new one's blue, and a line without one is not");
			const auto rowHeight  = workbench.line_rect(core::diff_side::right, 3U).height();
			const auto threadBand = band(still, QRect{ 0, gutterY(core::diff_side::right, 3U) - rowHeight / 2, gutter->width(), rowHeight });
			expect(inked(threadBand, amberCell) > 0, name + ": the tinted cell still shows its line number");

			// The tinted cell opens its thread, anywhere on it, in either pane.
			const auto pressGutter = [&](QWidget* const aGutter, const QPoint aPoint) {
				pressed.clear();
				QMouseEvent press{ QEvent::MouseButtonPress, QPointF{ aPoint }, QPointF{ aGutter->mapToGlobal(aPoint) }, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
				QApplication::sendEvent(aGutter, &press);
			};
			pressGutter(gutter, QPoint{ 2, gutterY(core::diff_side::right, 3U) });
			expect(pressed.size() == 1U && pressed.front() == std::pair{ core::diff_side::right, 3U }, name + ": the left end of line 4's tinted number opens its thread");
			pressGutter(gutter, QPoint{ gutter->width() - 2, gutterY(core::diff_side::right, 7U) });
			expect(pressed.size() == 1U && pressed.front() == std::pair{ core::diff_side::right, 7U }, name + ": the right end of line 8's opens its thread");
			pressGutter(gutter_of(base), QPoint{ gutter_of(base)->width() / 2, gutterY(core::diff_side::left, 5U) });
			expect(pressed.size() == 1U && pressed.front() == std::pair{ core::diff_side::left, 5U }, name + ": the base side's tinted number opens its thread");
			pressGutter(gutter, QPoint{ 2, gutterY(core::diff_side::right, 1U) });
			expect(pressed.empty() && added.empty(), name + ": a number without a thread opens nothing and adds nothing");

			// The pointer in line 2's "+" column shows its "+": a blue button
			// at the start of the code, over the boundary with the number.
			// [[BPR-1335]] v002, the owner on v001: "i find it somewhat
			// distracting always being drawn. Maybe we only draw it when the
			// cursor is over the column where it would go?" Elsewhere on the
			// line's code or on its digits nothing is drawn.
			auto* const button = code->findChild<QAbstractButton*>(QStringLiteral("diffLineAddButton"));
			expect(button != nullptr && button->isHidden() && workbench.line_add_rect().isEmpty(), name + ": the \"+\" button waits, hidden");
			move(code->viewport(), codePoint(1U, 160));
			expect(button->isHidden() && workbench.line_add_rect().isEmpty(), name + ": the pointer on line 2's code, away from the \"+\" column, draws no \"+\"");
			move(gutter, QPoint{ 2, gutterY(core::diff_side::right, 1U) });
			expect(button->isHidden(), name + ": the pointer on line 2's digits draws no \"+\"");
			move(code->viewport(), columnPoint(1U));
			const auto boundary = code->viewport()->geometry().left();
			const auto place    = button->geometry();
			const auto row      = workbench.line_rect(core::diff_side::right, 1U);
			std::cout << "bpr-1335 add " << name << " boundary=" << boundary << " button=" << place.x() << ',' << place.y() << ' ' << place.width() << 'x' << place.height()
					  << " row=" << row.height() << '\n';
			expect(!button->isHidden() && workbench.line_add_rect() == place, name + ": the pointer in line 2's \"+\" column shows its \"+\"");
			expect(place.left() >= boundary - ExpectedGutterPadding && place.left() < boundary && place.right() > boundary + place.width() / 2
					   && place.width() == place.height() && place.width() <= 20 && place.width() >= std::min(12, row.height()),
				   name + ": the button straddles the start of the code, its left edge in the number's padding, so it covers no digit");
			const auto rowTop = code->mapFrom(&workbench, row.topLeft()).y();
			expect(std::abs((place.top() + place.bottom()) / 2 - (rowTop + rowTop + row.height() - 1) / 2) <= 1, name + ": the button is centred on its line");
			auto* const accessible = QAccessible::queryAccessibleInterface(button);
			expect(button->toolTip() == QStringLiteral("Add comment (↩)") && button->accessibleName() == QStringLiteral("Add comment")
					   && button->accessibleDescription().contains(QStringLiteral("line 2")) && button->focusPolicy() == Qt::NoFocus
					   && accessible != nullptr && accessible->role() == QAccessible::Button,
				   name + ": the button says Add comment (↩), names its line, and is an accessible button that leaves focus to the code");
			// [[BPR-1354]]. The owner on v002: "bug: Why is this damned
			// tooltip far away again. This is a consistent problem." Its
			// tooltip sits directly below the button, left edges aligned, not
			// at the pointer plus the cursor's height.
			{
				qt::hide_anchored_tooltip();
				QToolTip::hideText();
				const auto centre = button->rect().center();
				auto       help   = QHelpEvent{ QEvent::ToolTip, centre, button->mapToGlobal(centre) };
				QApplication::sendEvent(button, &help);
				const auto* const tip    = qobject_cast<const QLabel*>(qt::anchored_tooltip());
				const auto        global = QRect{ button->mapToGlobal(QPoint{ 0, 0 }), button->size() };
				const auto        area   = button->screen()->availableGeometry();
				const auto        box    = tip != nullptr ? tip->geometry() : QRect{ };
				std::cout << "bpr-1354 add-button tooltip " << name << " anchored=" << (tip != nullptr) << " qt=" << QToolTip::isVisible() << " button=" << global.x() << ','
						  << global.y() << ' ' << global.width() << 'x' << global.height() << " tip=" << box.x() << ',' << box.y() << '\n';
				expect(tip != nullptr && tip->text() == button->toolTip() && !QToolTip::isVisible(),
					   name + ": the \"+\"'s tooltip is the anchored one, not Qt's at the pointer");
				expect(box == qt::anchored_tooltip_geometry(global, box.size(), area)
						   && (box.top() == global.bottom() + 1 + qt::anchored_tooltip_gap || box.bottom() + 1 + qt::anchored_tooltip_gap == global.top())
						   && box.left() == std::max(area.left(), std::min(global.left(), area.left() + area.width() - box.width())),
					   name + ": the \"+\"'s tooltip sits directly below the button (above it at the screen's edge), left edges aligned");
				qt::hide_anchored_tooltip();
			}
			const auto hovered    = gutter->grab().toImage();
			const auto numberBand = QRect{ 0, gutterY(core::diff_side::right, 1U) - rowHeight / 2, gutter->width(), rowHeight };
			expect(band(hovered, numberBand) == band(still, numberBand), name + ": line 2's number is painted exactly as before; the \"+\" never replaces it");
			const auto drawn = code->grab().toImage();
			// The cross is sampled at the button's true centre, QRectF's, and
			// as the brightest device pixel within one of it: QRect::center()
			// of an even-sided button is half a pixel short, which at 1.25x
			// and 2x lands on the cross's anti-aliased edge (Linux station,
			// BPR-1335 v002).
			const auto brightest = [](const QImage& aImage, const QPointF aLogical) {
				const auto ratio  = aImage.devicePixelRatio();
				const auto centre = QPoint{ static_cast<int>(std::floor(aLogical.x() * ratio)), static_cast<int>(std::floor(aLogical.y() * ratio)) };
				auto       best   = QColor{ };
				for (auto dy = -1; dy <= 1; ++dy) {
					for (auto dx = -1; dx <= 1; ++dx) {
						const auto x = std::clamp(centre.x() + dx, 0, aImage.width() - 1);
						const auto y = std::clamp(centre.y() + dy, 0, aImage.height() - 1);
						const auto c = QColor{ aImage.pixel(x, y) };
						if (!best.isValid() || c.red() + c.green() + c.blue() > best.red() + best.green() + best.blue()) {
							best = c;
						}
					}
				}
				return best;
			};
			const auto face  = pixel(drawn, QPoint{ place.left() + place.width() / 6, place.center().y() });
			const auto cross = brightest(drawn, QRectF{ place }.center());
			std::cout << "bpr-1335 add " << name << " face=" << face.name().toStdString() << " cross=" << cross.name().toStdString() << '\n';
			expect(face.blue() > face.red() + 80 && cross.red() > 200 && cross.green() > 200 && cross.blue() > 200, name + ": the button is blue with a white cross");

			// A line with a thread offers no "+"; the column's part in the
			// number's padding offers it as its part over the code does;
			// moving on into the code, or leaving, hides it.
			move(code->viewport(), columnPoint(3U));
			expect(button->isHidden(), name + ": line 4, which has a thread, offers no \"+\"");
			move(gutter, QPoint{ gutter->width() - 1, gutterY(core::diff_side::right, 1U) });
			expect(!button->isHidden() && button->geometry() == place, name + ": the column's part in line 2's number padding shows the same \"+\"");
			move(code->viewport(), codePoint(1U, place.right() - boundary + 3));
			expect(button->isHidden(), name + ": just past the button, on the code, the \"+\" goes");
			move(code->viewport(), columnPoint(1U));
			expect(!button->isHidden(), name + ": back in the column it comes back");
			QEvent leave{ QEvent::Leave };
			QApplication::sendEvent(code->viewport(), &leave);
			expect(button->isHidden(), name + ": leaving the code hides it");

			if (!directory.isEmpty()) {
				const auto render = [&](const QString& aState) {
					const auto crop = [](QWidget& aPane, const int aWidth) {
						const auto image = aPane.grab().toImage();
						const auto ratio = image.devicePixelRatio();
						auto       part  = image.copy(0, 0, qRound(aWidth * ratio), qRound(std::min(aPane.height(), 400) * ratio));
						part.setDevicePixelRatio(ratio);
						return part;
					};
					const auto baseImage = crop(*base, gutter_of(base)->width() + 320);
					const auto headImage = crop(*code, gutter->width() + 320);
					const auto ratio     = headImage.devicePixelRatio();
					auto       sheet     = QImage{ baseImage.width() + qRound(16 * ratio) + headImage.width(), std::max(baseImage.height(), headImage.height()), QImage::Format_ARGB32 };
					sheet.setDevicePixelRatio(ratio);
					sheet.fill(base->palette().color(QPalette::Window));
					{
						auto painter = QPainter{ &sheet };
						painter.drawImage(QPointF{ 0.0, 0.0 }, baseImage);
						painter.drawImage(QPointF{ baseImage.width() / ratio + 16.0, 0.0 }, headImage);
					}
					expect(sheet.save(QStringLiteral("%1/%2-%3.png").arg(directory, QString::fromStdString(name), aState), "PNG"), "the render is written");
				};
				move(code->viewport(), codePoint(1U, 160));
				render(QStringLiteral("code"));
				move(code->viewport(), columnPoint(1U));
				render(QStringLiteral("hover"));
				mouse(button, QEvent::MouseButtonPress, button->rect().center(), button->mapToGlobal(button->rect().center()));
				move(button, button->mapFromGlobal(code->viewport()->mapToGlobal(codePoint(5U, 160))), Qt::LeftButton);
				render(QStringLiteral("drag"));
				mouse(button, QEvent::MouseButtonRelease, button->rect().center(), code->viewport()->mapToGlobal(codePoint(5U, 160)));
				added.clear();
			}

			// A click comments on its line, and the press never reaches the
			// code: no text selection, no change of the selected line.
			const auto before = workbench.selection();
			move(code->viewport(), columnPoint(1U));
			const auto centre = button->rect().center();
			mouse(button, QEvent::MouseButtonPress, centre, button->mapToGlobal(centre));
			mouse(button, QEvent::MouseButtonRelease, centre, button->mapToGlobal(centre));
			expect(added.size() == 1U && added.back() == std::pair{ 1U, 1U }, name + ": clicking the \"+\" adds a comment on line 2");
			expect(!code->textCursor().hasSelection() && workbench.selection() == before, name + ": the click selects no text and moves no selection");

			// A drag on the "+" covers the lines from where it started to the
			// one under the pointer, washed while it lasts, in either direction.
			const auto washAt   = codePoint(3U, code->viewport()->width() - 12);
			const auto unwashed = pixel(code->viewport()->grab().toImage(), washAt);
			move(code->viewport(), columnPoint(1U));
			mouse(button, QEvent::MouseButtonPress, centre, button->mapToGlobal(centre));
			move(button, button->mapFromGlobal(code->viewport()->mapToGlobal(codePoint(5U, 160))), Qt::LeftButton);
			const auto washed = pixel(code->viewport()->grab().toImage(), washAt);
			expect(!button->isHidden(), name + ": a drag keeps its \"+\" while the pointer is out on the code");
			mouse(button, QEvent::MouseButtonRelease, centre, code->viewport()->mapToGlobal(codePoint(5U, 160)));
			expect(washed != unwashed, name + ": the lines a drag covers are washed while it lasts");
			expect(added.size() == 2U && added.back() == std::pair{ 1U, 5U }, name + ": dragging the \"+\" from line 2 to line 6 adds a comment on lines 2 to 6");
			expect(pixel(code->viewport()->grab().toImage(), washAt) == unwashed, name + ": the wash goes when the drag ends");
			move(code->viewport(), columnPoint(5U));
			mouse(button, QEvent::MouseButtonPress, centre, button->mapToGlobal(centre));
			move(button, button->mapFromGlobal(code->viewport()->mapToGlobal(codePoint(2U, 160))), Qt::LeftButton);
			mouse(button, QEvent::MouseButtonRelease, centre, code->viewport()->mapToGlobal(codePoint(2U, 160)));
			expect(added.size() == 3U && added.back() == std::pair{ 2U, 5U }, name + ": dragging it upward from line 6 to line 3 covers lines 3 to 6, in order");

			// A press and a drag in the code are the text's: they select text,
			// and the "+" stays out of the way while they last.
			move(code->viewport(), codePoint(1U, 160));
			const auto from = codePoint(1U, 200);
			const auto to   = codePoint(2U, 260);
			mouse(code->viewport(), QEvent::MouseButtonPress, from, code->viewport()->mapToGlobal(from));
			move(code->viewport(), to, Qt::LeftButton);
			expect(button->isHidden(), name + ": the \"+\" hides while text is being selected");
			mouse(code->viewport(), QEvent::MouseButtonRelease, to, code->viewport()->mapToGlobal(to));
			expect(code->textCursor().hasSelection() && added.size() == 3U, name + ": a drag in the code selects text and adds no comment");

			// The selected line says it has a thread.
			workbench.set_line_details_visible(true);
			static_cast<void>(workbench.select_conversation_line(core::diff_side::right, 3U));
			process_events();
			auto* const detail = workbench.findChild<QPlainTextEdit*>(QStringLiteral("diffSelectedChangeDetail"));
			expect(detail != nullptr && detail->accessibleDescription().contains(QStringLiteral("Thread: Line 4: 2 posts")),
				   name + ": a selected line with a thread says so in its accessible description");
			static_cast<void>(workbench.select_conversation_line(core::diff_side::right, 1U));
			process_events();
			expect(!detail->accessibleDescription().contains(QStringLiteral("Thread:")), name + ": a line without one does not");
			workbench.hide();
		}
	}

	void test_selected_composition()
	{
		auto announcements               = std::vector<accessible_announcement>{ };
		auto capture                     = scoped_accessibility_capture{ announcements };
		const auto [leftText, rightText] = representative_text();
		auto       options               = core::diff_options{ };
		const auto diff                  = computed(leftText, rightText, options, 0U);
		expect(diff.Alignment.Groups.size() >= 3U, "fixture has several semantic changes");

		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'260, 820);
		workbench.show();
		process_events();
		expect(
			workbench.statistics().GeometryRebuilds == 0U
				&& workbench.statistics().GeometryRebuildHistorySize == 0U,
			"empty-workbench show and resize events do not create attributed geometry work");
		const auto expectedScale = qEnvironmentVariable("BPR_EXPECTED_QT_SCALE");
		if (!expectedScale.isEmpty()) {
			expect(
				close(
					workbench.devicePixelRatioF(),
					expectedScale.toDouble(),
					0.05),
				"selected workbench observes the requested Qt scale");
		}

		const auto firstContext = context(diff, 1U, 1U);
		workbench.set_loading(firstContext);
		workbench.set_loading(firstContext);
		expect(workbench.current_result() == nullptr, "loading clears old result and geometry");
		expect(
			announcement_count(
				announcements,
				&workbench,
				QStringLiteral("Loading selected file comparison…"))
				== 1U,
			"loading status is one polite deduplicated workbench announcement");
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = firstContext,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		// A rebuild is attributed when its measurement completes, and measuring
		// is sliced across event turns ([[BPR-1123]]).
		settle_geometry(workbench);
		const auto  statisticsAfterFirstPublish = workbench.statistics();
		const auto& firstGeometryCause
			= statisticsAfterFirstPublish.GeometryRebuildHistory[0U];
		expect(
			statisticsAfterFirstPublish.GeometryRebuilds > 0U
				&& statisticsAfterFirstPublish.GeometryRebuildHistorySize
					== statisticsAfterFirstPublish.GeometryRebuilds
				&& (firstGeometryCause.Changes & qt::measured_geometry_change::screen)
					== qt::measured_geometry_change::none
				&& (firstGeometryCause.Triggers
					& qt::diff_geometry_rebuild_trigger::show_event)
					== qt::diff_geometry_rebuild_trigger::none,
			"pre-result show and resize events cannot contaminate the first accepted diff attribution");
		expect(workbench.current_result() != nullptr, "accepted immutable result reaches workbench");
		expect(
			announcement_count(
				announcements, &workbench, workbench.status_text())
				== 1U,
			"ready status is one polite workbench announcement");
		expect(
			std::ranges::count_if(
				announcements,
				[&workbench](const accessible_announcement& aAnnouncement) {
					return aAnnouncement.Target == &workbench
						|| aAnnouncement.Target == workbench.link_map();
				})
				== 2,
			"published loading and ready states do not add a duplicate link-map announcement");
		expect(
			workbench.left_pane()->toPlainText() == QString::fromStdString(leftText)
				&& workbench.right_pane()->toPlainText() == QString::fromStdString(rightText),
			"natural panes contain exact source documents without alignment rows");
		expect(
			static_cast<std::uint32_t>(workbench.left_pane()->document()->blockCount())
					<= diff.Alignment.LeftLineCount + 1U
				&& static_cast<std::uint32_t>(workbench.right_pane()->document()->blockCount())
					<= diff.Alignment.RightLineCount + 1U,
			"Qt panes add at most their optional trailing visual block");
		expect(
			workbench.shared_scrollbar()->isEnabled()
				&& workbench.left_pane()->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff
				&& workbench.right_pane()->verticalScrollBarPolicy() == Qt::ScrollBarAlwaysOff,
			"one shared scrollbar is the visible vertical authority");
		expect(
			!workbench.accessibleName().isEmpty()
				&& !workbench.left_pane()->accessibleDescription().isEmpty()
				&& !workbench.right_pane()->accessibleDescription().isEmpty()
				&& !workbench.link_map()->accessibleName().isEmpty()
				&& !workbench.shared_scrollbar()->accessibleDescription().isEmpty()
				&& !workbench.detail_pane()->accessibleDescription().isEmpty(),
			"composed focus surfaces expose accessible identities and status");
		const auto firstGroup = workbench.selection()->Group;
		expect(workbench.navigate_change(core::change_navigation_direction::next,
										 core::change_navigation_policy::stop),
			   "the semantic navigation bridge selects the next canonical group");
		process_events();
		expect(
			workbench.selection()->Group != firstGroup,
			"semantic navigation changes the one retained selection");
		for (const auto* control : workbench.findChildren<QToolButton*>()) {
			expect(workbench.source_find()->isAncestorOf(control),
				   "native Find owns its buttons; comparison still has no duplicate change-navigation presenter");
		}
		workbench.left_pane()->setFocus(Qt::OtherFocusReason);
		process_events();
		expect(
			QApplication::focusWidget() == workbench.left_pane(),
			"focus enters the earlier natural source pane directly");
		send_key(*workbench.left_pane(), Qt::Key_Tab);
		process_events();
		expect(
			QApplication::focusWidget() == workbench.link_map(),
			"Tab follows the comparison surface's internal focus order");
		send_key(*workbench.link_map(), Qt::Key_Tab);
		process_events();
		expect(
			QApplication::focusWidget() == workbench.right_pane(),
			"Tab advances from the link map into the later natural source pane");
		workbench.left_pane()->setFocus(Qt::OtherFocusReason);
		process_events();

		auto* const controller = workbench.scroll_controller();
		controller->set_position(controller->maximum_position() * 0.45);
		const auto beforeLeftWheel = controller->position();
		process_events();
		const auto nativeWheelStartLeft  = rendered_state(*workbench.left_pane());
		const auto nativeWheelStartRight = rendered_state(*workbench.right_pane());
		expect(send_wheel(*workbench.left_pane()->viewport(), QPoint{ 0, -32 }),
			   "left pane wheel routes through shared controller");
		const auto leftWheel            = controller->position();
		const auto leftWheelNativeLeft  = rendered_state(*workbench.left_pane());
		const auto leftWheelNativeRight = rendered_state(*workbench.right_pane());
		controller->set_position(beforeLeftWheel);
		expect(send_wheel(*workbench.right_pane()->viewport(), QPoint{ 0, -32 }),
			   "right pane wheel routes through shared controller");
		const auto rightWheel            = controller->position();
		const auto rightWheelNativeLeft  = rendered_state(*workbench.left_pane());
		const auto rightWheelNativeRight = rendered_state(*workbench.right_pane());
		controller->set_position(beforeLeftWheel);
		expect(send_wheel(*workbench.link_map(), QPoint{ 0, -32 }),
			   "divider wheel routes through shared controller");
		const auto mapWheel            = controller->position();
		const auto mapWheelNativeLeft  = rendered_state(*workbench.left_pane());
		const auto mapWheelNativeRight = rendered_state(*workbench.right_pane());
		expect(
			close(leftWheel, rightWheel) && close(rightWheel, mapWheel)
				&& leftWheel > beforeLeftWheel,
			"all three wheel receivers produce the same signed comparison-space motion");
		expect(
			leftWheelNativeLeft.ScrollValue == rightWheelNativeLeft.ScrollValue
				&& rightWheelNativeLeft.ScrollValue == mapWheelNativeLeft.ScrollValue
				&& leftWheelNativeRight.ScrollValue == rightWheelNativeRight.ScrollValue
				&& rightWheelNativeRight.ScrollValue == mapWheelNativeRight.ScrollValue
				&& (leftWheelNativeLeft.ScrollValue != nativeWheelStartLeft.ScrollValue
					|| leftWheelNativeRight.ScrollValue
						!= nativeWheelStartRight.ScrollValue),
			"all three input receivers apply the same motion to the native panes");
		const auto qBeforeHorizontal     = controller->position();
		const auto rightBeforeHorizontal = workbench.right_pane()
											   ->horizontalScrollBar()
											   ->value();
		expect(
			workbench.left_pane()->horizontalScrollBar()->maximum() > 0
				&& send_wheel(
					*workbench.left_pane()->viewport(), QPoint{ -24, 0 }),
			"left pane admits pane-local horizontal wheel input");
		expect(
			close(controller->position(), qBeforeHorizontal)
				&& workbench.left_pane()->horizontalScrollBar()->value() > 0
				&& workbench.right_pane()->horizontalScrollBar()->value()
					== rightBeforeHorizontal,
			"horizontal input moves only its natural source pane and preserves q");

		const auto& insertion = segment_of_kind(diff, core::alignment_kind::right_only);
		expect(controller->reveal_segment(insertion.Id, 0.2), "insertion reveal succeeds");
		const auto insertionFirst      = *controller->current_frame();
		const auto insertionFirstLeft  = rendered_state(*workbench.left_pane());
		const auto insertionFirstRight = rendered_state(*workbench.right_pane());
		expect(controller->reveal_segment(insertion.Id, 0.8), "insertion late reveal succeeds");
		const auto insertionSecond      = *controller->current_frame();
		const auto insertionSecondLeft  = rendered_state(*workbench.left_pane());
		const auto insertionSecondRight = rendered_state(*workbench.right_pane());
		expect(
			close(
				insertionFirst.Left.ProjectedDocumentY,
				insertionSecond.Left.ProjectedDocumentY)
				&& insertionSecond.Right.ProjectedDocumentY
					> insertionFirst.Right.ProjectedDocumentY,
			"right insertion pins left while right advances");
		expect(
			insertionFirstLeft.ScrollValue == insertionSecondLeft.ScrollValue
				&& insertionSecondRight.ScrollValue
					> insertionFirstRight.ScrollValue
				&& insertionFirstLeft.Pixels == insertionSecondLeft.Pixels
				&& insertionFirstRight.Pixels != insertionSecondRight.Pixels,
			"right insertion visibly pins the native left pane while the right pane advances");
		const auto& deletion = segment_of_kind(diff, core::alignment_kind::left_only);
		expect(controller->reveal_segment(deletion.Id, 0.2), "deletion reveal succeeds");
		const auto deletionFirst      = *controller->current_frame();
		const auto deletionFirstLeft  = rendered_state(*workbench.left_pane());
		const auto deletionFirstRight = rendered_state(*workbench.right_pane());
		expect(controller->reveal_segment(deletion.Id, 0.8), "deletion late reveal succeeds");
		const auto deletionSecond      = *controller->current_frame();
		const auto deletionSecondLeft  = rendered_state(*workbench.left_pane());
		const auto deletionSecondRight = rendered_state(*workbench.right_pane());
		expect(
			deletionSecond.Left.ProjectedDocumentY > deletionFirst.Left.ProjectedDocumentY
				&& close(
					deletionFirst.Right.ProjectedDocumentY,
					deletionSecond.Right.ProjectedDocumentY),
			"left deletion pins right while left advances");
		expect(
			deletionSecondLeft.ScrollValue > deletionFirstLeft.ScrollValue
				&& deletionFirstRight.ScrollValue == deletionSecondRight.ScrollValue
				&& deletionFirstLeft.Pixels != deletionSecondLeft.Pixels
				&& deletionFirstRight.Pixels == deletionSecondRight.Pixels,
			"left deletion visibly advances the native left pane while pinning the right pane");
		const auto& replacement = segment_of_kind(diff, core::alignment_kind::replacement);
		expect(controller->reveal_segment(replacement.Id, 0.2), "replacement reveal succeeds");
		const auto replacementFirst      = *controller->current_frame();
		const auto replacementFirstLeft  = rendered_state(*workbench.left_pane());
		const auto replacementFirstRight = rendered_state(*workbench.right_pane());
		expect(controller->reveal_segment(replacement.Id, 0.8), "replacement late reveal succeeds");
		const auto replacementSecond      = *controller->current_frame();
		const auto renderedReplacement    = workbench.link_map()->current_frame();
		const auto replacementSecondLeft  = rendered_state(*workbench.left_pane());
		const auto replacementSecondRight = rendered_state(*workbench.right_pane());
		const auto leftAdvance            = replacementSecond.Left.ProjectedDocumentY
			- replacementFirst.Left.ProjectedDocumentY;
		const auto rightAdvance = replacementSecond.Right.ProjectedDocumentY
			- replacementFirst.Right.ProjectedDocumentY;
		expect(
			leftAdvance > 0.0 && rightAdvance > leftAdvance,
			"unequal replacement advances both panes proportionally");
		expect(
			renderedReplacement.has_value()
				&& renderedReplacement->Left.DocumentOriginY > 0.0
				&& renderedReplacement->Right.DocumentOriginY > 0.0,
			"rendered correspondence frame retains both applied pane origins");
		expect(
			replacementSecondLeft.ScrollValue > replacementFirstLeft.ScrollValue
				&& replacementSecondRight.ScrollValue
					> replacementFirstRight.ScrollValue
				&& replacementSecondRight.ScrollValue
						- replacementFirstRight.ScrollValue
					> replacementSecondLeft.ScrollValue
						- replacementFirstLeft.ScrollValue
				&& replacementFirstLeft.Pixels != replacementSecondLeft.Pixels
				&& replacementFirstRight.Pixels != replacementSecondRight.Pixels,
			"unequal replacement visibly advances both native panes proportionally");

		controller->set_position(controller->maximum_position() * 0.5);
		const auto reversible      = controller->position();
		const auto reversibleLeft  = rendered_state(*workbench.left_pane());
		const auto reversibleRight = rendered_state(*workbench.right_pane());
		for (std::size_t cycle = 0U; cycle < 1'000U; ++cycle) {
			controller->scroll_by(0.1);
			controller->scroll_by(-0.1);
		}
		expect(close(controller->position(), reversible, 0.000'000'001),
			   "forward and reverse motion does not accumulate drift");
		const auto reversedLeft  = rendered_state(*workbench.left_pane());
		const auto reversedRight = rendered_state(*workbench.right_pane());
		expect(
			reversedLeft.ScrollValue == reversibleLeft.ScrollValue
				&& reversedRight.ScrollValue == reversibleRight.ScrollValue
				&& reversedLeft.Pixels == reversibleLeft.Pixels
				&& reversedRight.Pixels == reversibleRight.Pixels,
			"forward and reverse traces restore both native pane origins and rendered frames");
		send_key(*workbench.left_pane(), Qt::Key_Home);
		expect(close(controller->position(), controller->minimum_position()),
			   "Home from the left source reaches comparison beginning");
		send_key(*workbench.link_map(), Qt::Key_PageDown);
		expect(controller->position() > controller->minimum_position(),
			   "Page from the divider advances comparison space");
		send_key(*workbench.right_pane(), Qt::Key_End);
		expect(close(controller->position(), controller->maximum_position()),
			   "End from the right source reaches the asymmetric tail without dead travel");
		workbench.shared_scrollbar()->setValue(
			(workbench.shared_scrollbar()->minimum()
			 + workbench.shared_scrollbar()->maximum())
			/ 2);
		process_events();
		expect(
			controller->position() > controller->minimum_position()
				&& controller->position() < controller->maximum_position(),
			"shared scrollbar drives the same retained q");

		expect(controller->reveal_segment(replacement.Id, 0.5),
			   "replacement is visible for ribbon interaction");
		process_events();
		const auto visible = workbench.link_map()->visible_links();
		const auto ribbon  = std::ranges::find(
			visible, replacement.Id,
			[](const qt::visible_diff_link& aLink) { return aLink.Identity.Segment; });
		expect(ribbon != visible.end(), "replacement ribbon is visible in selected composition");
		auto point = ribbon->Path.boundingRect().center();
		expect(ribbon->Path.contains(point), "ribbon midpoint is an interactive interior point");
		const auto announcementsBeforeRibbon = announcements.size();
		send_click(*workbench.link_map(), point);
		process_events();
		auto ribbonWorkbenchAnnouncements = std::size_t{ 0U };
		auto ribbonLinkMapAnnouncements   = std::size_t{ 0U };
		for (auto index = announcementsBeforeRibbon;
			 index < announcements.size();
			 ++index) {
			ribbonWorkbenchAnnouncements += announcements[index].Target == &workbench
				? 1U
				: 0U;
			ribbonLinkMapAnnouncements += announcements[index].Target
					== workbench.link_map()
				? 1U
				: 0U;
		}
		expect(
			workbench.selection().has_value()
				&& workbench.selection()->Group == ribbon->Identity.Group
				&& workbench.link_map()->selected().has_value()
				&& workbench.link_map()->selected()->Group == workbench.selection()->Group
				&& std::ranges::any_of(workbench.line_details()->projection().Rows,
									   [&workbench](const core::line_details_row& aRow) {
										   return aRow.Side == workbench.selection()->PreferredLine.Side
											   && aRow.Line == std::optional{ workbench.selection()->PreferredLine.Line };
									   }),
			"ribbon, semantic selection, top highlight, and Line details agree on stable group identity");
		expect(
			ribbonWorkbenchAnnouncements == 1U
				&& ribbonLinkMapAnnouncements == 0U,
			"ribbon selection emits one combined polite workbench announcement without a link-map duplicate");
		const auto selectedGroup = workbench.selection()->Group;
		expect(workbench.navigate_change(core::change_navigation_direction::next,
										 core::change_navigation_policy::stop),
			   "Next change navigates the canonical group vector");
		process_events();
		expect(workbench.selection()->Group != selectedGroup,
			   "Next change navigates the canonical group vector");
		expect(workbench.navigate_change(core::change_navigation_direction::previous,
										 core::change_navigation_policy::stop),
			   "Previous change returns through the same semantic bridge");
		process_events();
		expect(workbench.selection()->Group == selectedGroup,
			   "Previous change returns to the same stable group");

		const auto targetBlock = workbench.left_pane()->document()->findBlockByNumber(
			static_cast<int>(replacement.Left.Begin));
		QTextCursor cursor{ targetBlock };
		const auto  cursorPoint = workbench.left_pane()->cursorRect(cursor).center();
		send_click(*workbench.left_pane()->viewport(), cursorPoint);
		process_events();
		expect(
			workbench.selection().has_value()
				&& workbench.selection()->PreferredLine.Side == core::diff_side::left
				&& workbench.selection()->PreferredLine.Line == replacement.Left.Begin,
			"top-pane line click becomes a core semantic line intent");

		const auto selectionBeforeMetrics = workbench.selection();
		const auto rebuildsBeforeMetrics  = workbench.statistics().GeometryRebuilds;
		workbench.resize(1'080, 700);
		auto font = workbench.left_pane()->font();
		font.setPointSizeF(font.pointSizeF() + 2.0);
		workbench.setFont(font);
		// The panes' font changed, so every line is measured again: a full
		// pass, sliced across event turns ([[BPR-1123]]).
		settle_geometry(workbench);
		expect(
			workbench.statistics().GeometryRebuilds > rebuildsBeforeMetrics
				&& workbench.selection() == selectionBeforeMetrics
				&& workbench.link_map()->selected().has_value(),
			"resize/font metric change rebuilds Qt geometry and preserves semantic identity");
		auto* const vertical = workbench.findChild<QSplitter*>(
			QStringLiteral("diffWorkbenchVerticalSplitter"));
		expect(vertical != nullptr, "selected composition exposes its vertical splitter");
		const auto outerSizeBeforeDivider  = workbench.size();
		const auto viewportBeforeDivider   = workbench.left_pane()->viewport()->height();
		const auto statisticsBeforeDivider = workbench.statistics();
		const auto verticalSizes           = vertical->sizes();
		const auto verticalTotal           = std::accumulate(
			verticalSizes.begin(), verticalSizes.end(), 0);
		const auto largerTop = std::min(
			verticalTotal - 48, verticalSizes.front() + 32);
		expect(largerTop > verticalSizes.front(),
			   "the compact detail pane retains room for a divider gesture");
		vertical->setSizes({ largerTop, verticalTotal - largerTop });
		process_events();
		expect(
			workbench.size() == outerSizeBeforeDivider
				&& workbench.left_pane()->viewport()->height()
					> viewportBeforeDivider
				&& workbench.statistics().GeometryRebuilds
					== statisticsBeforeDivider.GeometryRebuilds + 1U
				&& workbench.statistics().GeometryRebuildHistorySize
					== statisticsBeforeDivider.GeometryRebuildHistorySize + 1U
				&& (workbench.statistics()
						.GeometryRebuildHistory[workbench.statistics().GeometryRebuildHistorySize - 1U]
						.Triggers
					& qt::diff_geometry_rebuild_trigger::pane_viewport_resize)
					!= qt::diff_geometry_rebuild_trigger::none
				&& workbench.selection() == selectionBeforeMetrics,
			"detail-divider resize coalesces one attributed viewport-axis rebuild without changing selection");
		// [[BPR-1123]]. A divider moves no line, so its rebuild takes every
		// line from the recorded heights rather than shaping each one again.
		expect(workbench.statistics().MeasurementLinesReused
				   == statisticsBeforeDivider.MeasurementLinesReused + diff.Alignment.LeftLineCount + diff.Alignment.RightLineCount,
			   "the divider's rebuild reuses every recorded line height");
		send_key(*workbench.right_pane(), Qt::Key_End);
		process_events();
		expect(
			workbench.left_pane()->verticalScrollBar()->value()
					== workbench.left_pane()->verticalScrollBar()->maximum()
				&& workbench.right_pane()->verticalScrollBar()->value()
					== workbench.right_pane()->verticalScrollBar()->maximum(),
			"rebuilt viewport extents let End reveal both native document tails");
		const auto rebuildsBeforeDpi = workbench.statistics().GeometryRebuilds;
		auto       dpiChange         = QEvent{ QEvent::DevicePixelRatioChange };
		QCoreApplication::sendEvent(&workbench, &dpiChange);
		process_events();
		expect(
			workbench.statistics().GeometryRebuilds > rebuildsBeforeDpi
				&& workbench.selection() == selectionBeforeMetrics,
			"DPI invalidation rebuilds measured geometry without replacing semantic selection");

		const auto initialRender           = workbench.grab().toImage();
		const auto originalPalette         = QApplication::palette();
		const auto originalFont            = workbench.font();
		const auto statisticsBeforePalette = workbench.statistics();
		auto       changedPalette          = originalPalette;
		changedPalette.setColor(QPalette::Window, QColor{ 31, 35, 41 });
		changedPalette.setColor(QPalette::Base, QColor{ 22, 25, 30 });
		changedPalette.setColor(QPalette::Text, QColor{ 236, 239, 244 });
		changedPalette.setColor(QPalette::WindowText, QColor{ 236, 239, 244 });
		QApplication::setPalette(changedPalette);
		process_events();
		const auto changedRender = workbench.grab().toImage();
		expect(
			workbench.selection() == selectionBeforeMetrics
				&& workbench.font() == originalFont
				&& workbench.statistics().GeometryRebuilds
					== statisticsBeforePalette.GeometryRebuilds
				&& workbench.statistics().GeometryRebuildHistorySize
					== statisticsBeforePalette.GeometryRebuildHistorySize
				&& initialRender != changedRender,
			"inherited palette transition changes pixels without rebuilding geometry "
			"or replacing font and selection");
		QApplication::setPalette(originalPalette);
		process_events();
		const auto statisticsBeforeStyle = workbench.statistics();
		auto       styleChange           = QEvent{ QEvent::StyleChange };
		QCoreApplication::sendEvent(&workbench, &styleChange);
		process_events();
		const auto  statisticsAfterStyle = workbench.statistics();
		const auto& styleCause           = statisticsAfterStyle.GeometryRebuildHistory[statisticsAfterStyle.GeometryRebuildHistorySize - 1U];
		expect(
			statisticsAfterStyle.GeometryRebuilds
					== statisticsBeforeStyle.GeometryRebuilds + 1U
				&& statisticsAfterStyle.GeometryRebuildHistorySize
					== statisticsBeforeStyle.GeometryRebuildHistorySize + 1U
				&& !statisticsAfterStyle.GeometryRebuildHistoryOverflow
				&& (styleCause.Changes
					& qt::measured_geometry_change::viewport_or_divider)
					!= qt::measured_geometry_change::none
				&& (styleCause.Triggers
					& qt::diff_geometry_rebuild_trigger::style_event)
					!= qt::diff_geometry_rebuild_trigger::none
				&& workbench.selection() == selectionBeforeMetrics,
			"style invalidation records its exact rebuild cause while preserving semantic selection");

		const auto second        = computed(leftText, rightText + "option-tail\n", options, 1U);
		const auto secondContext = context(second, 2U, 2U);
		workbench.set_loading(secondContext);
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = firstContext,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		expect(
			workbench.current_result() == nullptr
				&& workbench.statistics().RejectedStaleInput >= 1U,
			"file/epoch replacement rejects an old completion before geometry appears");
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = secondContext,
			.Result  = std::make_shared<const core::computed_file_diff>(second),
		}));
		process_events();
		expect(workbench.current_result() != nullptr, "replacement comparison publishes");

		auto optionVariant       = options;
		optionVariant.Whitespace = core::whitespace_policy::ignore_trailing;
		const auto third         = computed(leftText, rightText, optionVariant, 2U);
		const auto thirdContext  = context(third, 3U, 3U);
		workbench.set_loading(thirdContext);
		workbench.fail({
			.Context = secondContext,
			.Reason  = core::diff_reason_code::host_failure,
		});
		expect(workbench.current_result() == nullptr,
			   "stale failure cannot replace a newer options selection");
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = thirdContext,
			.Result  = std::make_shared<const core::computed_file_diff>(third),
		}));
		process_events();
		workbench.fail({
			.Context = thirdContext,
			.Reason  = core::diff_reason_code::host_failure,
		});
		expect(
			workbench.current_result() == nullptr
				&& !workbench.current_context().has_value()
				&& workbench.status_text() == QStringLiteral("Comparison unavailable"),
			"exact failure clears selected geometry and presents a typed status");
		expect(
			announcement_count(
				announcements,
				&workbench,
				QStringLiteral("Comparison unavailable"))
				== 1U,
			"exact failure is one polite workbench announcement");
		workbench.set_loading(thirdContext);
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = thirdContext,
			.Result  = std::make_shared<const core::computed_file_diff>(third),
		}));
		process_events();
		workbench.invalidate(third.Alignment.Key.Authority);
		expect(
			workbench.current_result() == nullptr
				&& !workbench.current_context().has_value()
				&& workbench.status_text() == QStringLiteral("Comparison invalidated"),
			"authority/cache invalidation synchronously clears result and geometry");
		expect(
			announcement_count(
				announcements,
				&workbench,
				QStringLiteral("Comparison invalidated"))
				== 1U,
			"invalidation is one polite workbench announcement");
		const auto rejectedBeforeLate = workbench.statistics().RejectedStaleInput;
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = thirdContext,
			.Result  = std::make_shared<const core::computed_file_diff>(third),
		}));
		expect(
			workbench.current_result() == nullptr
				&& workbench.statistics().RejectedStaleInput
					== rejectedBeforeLate + 1U,
			"late publication after invalidation cannot recreate cleared Qt state");

		const auto renderPath
			= qEnvironmentVariable("BPR_QT_DIFF_WORKBENCH_RENDER_PATH");
		if (!renderPath.isEmpty()) {
			workbench.set_loading(firstContext);
			workbench.publish(bpr::qt::test::prepare_publication({
				.Context = firstContext,
				.Result  = std::make_shared<const core::computed_file_diff>(diff),
			}));
			process_events();
			expect(workbench.grab().save(renderPath),
				   "selected workbench raster saves");
		}
	}

	void test_nontext_publication_status()
	{
		auto       workbench = qt::diff_workbench{ };
		const auto publish   = [&](const core::computed_file_diff& aDiff, const bool aCached) {
			const auto selected = context(aDiff, 1U, 1U);
			workbench.set_loading(selected);
			workbench.publish(bpr::qt::test::prepare_publication({ .Context  = selected,
																   .Result   = std::make_shared<const core::computed_file_diff>(aDiff),
																   .CacheHit = aCached }));
		};
		const auto changed = computed("old\n", "new\n", { }, 70U);
		publish(changed, false);
		expect(workbench.selection().has_value(), "changed text supplies a selection before refusal");
		// [[BPR-0719]]: an exhausted matrix budget degrades to an approximate
		// alignment, so only a segment overflow refuses the file now.
		auto limits            = core::diff_compute_limits{ };
		limits.MaximumSegments = 1U;
		const auto left = bytes("same\nold\n"), right = bytes("same\nnew\n");
		const auto limited = bpr::tests::product_file_diff({ .Key = key({ }, 71U), .Options = { }, .LeftBytes = left, .RightBytes = right, .Limits = limits });
		expect(limited.Result && limited.Reason == core::diff_reason_code::compute_limit, "real compute limit produces an unavailable result");
		for (const bool cached : { false, true }) {
			publish(*limited.Result, cached);
			expect(workbench.status_text() == QStringLiteral("Comparison unavailable — file exceeds the comparison work limit") + (cached ? QStringLiteral(" (cached)") : QString{ }), "published and cached limits preserve their diagnostic");
			expect(!workbench.selection() && workbench.left_pane()->toPlainText().isEmpty()
					   && workbench.right_pane()->toPlainText().isEmpty(),
				   "unavailable result clears prior source and selection");
		}
		const auto binary  = bytes(std::string_view{ "a\0b", 3U });
		const auto nontext = bpr::tests::product_file_diff({ .Key = key({ }, 72U), .Options = { }, .LeftBytes = binary, .RightBytes = right });
		expect(nontext.Result && nontext.Reason == core::diff_reason_code::binary_file, "real binary result available");
		publish(*nontext.Result, false);
		expect(workbench.status_text() == QStringLiteral("Binary file comparison"), "binary content is distinct from equal text and unavailable content");
		publish(computed("same\n", "same\n", { }, 73U), false);
		expect(workbench.status_text() == QStringLiteral("Diff ready — 0 semantic changes")
				   && workbench.left_pane()->toPlainText() == QStringLiteral("same\n"),
			   "equal text restores a genuine zero-change comparison");
		auto* label = workbench.findChild<QLabel*>(QStringLiteral("diffWorkbenchStatus"));
		expect(label && label->textInteractionFlags().testFlag(Qt::TextSelectableByKeyboard)
				   && label->textInteractionFlags().testFlag(Qt::TextSelectableByMouse)
				   && label->accessibleDescription() == label->text(),
			   "comparison messages remain copyable and accessible");
	}

	void test_semantic_selection_callback_tracks_both_panes_and_clear()
	{
		const auto [leftText, rightText] = representative_text();
		const auto diff                  = computed(leftText, rightText, { }, 53U);
		auto       workbench             = qt::diff_workbench{ };
		auto       selections
			= std::vector<std::optional<core::semantic_selection>>{ };
		workbench.set_selection_sink(
			[&selections](const std::optional<core::semantic_selection>& aSelection) {
				selections.push_back(aSelection);
			});
		auto readingGestures = 0;
		workbench.set_reading_gesture_sink([&readingGestures] { ++readingGestures; });
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();
		const auto selected = context(diff, 106U, 106U);
		workbench.set_loading(selected);
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		process_events();
		expect(!selections.empty() && selections.back() == workbench.selection(),
			   "accepted publication emits its reconciled semantic selection");
		expect(readingGestures == 0, "publication is not a user reading gesture");

		const auto leftBlock = workbench.left_pane()->document()->findBlockByNumber(0);
		send_click(
			*workbench.left_pane()->viewport(),
			workbench.left_pane()->cursorRect(QTextCursor{ leftBlock }).center());
		process_events();
		expect(selections.back().has_value()
				   && selections.back()->PreferredLine.Side == core::diff_side::left
				   && selections.back() == workbench.selection(),
			   "a left-pane click emits the exact left semantic source line");
		expect(readingGestures > 0, "line gesture reaches reading cancellation boundary");
		const auto beforeRestore = readingGestures;
		expect(workbench.restore_selection(*workbench.selection()) && readingGestures == beforeRestore, "exact restoration never emits a user gesture");

		const auto rightBlock = workbench.right_pane()->document()->findBlockByNumber(0);
		send_click(
			*workbench.right_pane()->viewport(),
			workbench.right_pane()->cursorRect(QTextCursor{ rightBlock }).center());
		process_events();
		expect(selections.back().has_value()
				   && selections.back()->PreferredLine.Side == core::diff_side::right
				   && selections.back() == workbench.selection(),
			   "a right-pane click emits the exact right semantic source line");

		workbench.clear();
		expect(!selections.back().has_value(),
			   "clearing the projection emits an absent semantic selection");
	}

	void test_conversation_anchor_reveals_and_paints_exact_line()
	{
		const auto [leftText, rightText] = representative_text();
		const auto diff                  = computed(leftText, rightText, { }, 54U);
		auto       workbench             = qt::diff_workbench{ };
		auto       externalCalls         = 0U;
		auto       observerCalls         = 0U;
		auto       readingGestures       = 0U;
		workbench.set_reading_gesture_sink([&readingGestures] { ++readingGestures; });
		workbench.set_selection_sink([&externalCalls](const auto&) {
			++externalCalls;
		});
		workbench.set_selection_observer([&observerCalls](const auto&, const bool) {
			++observerCalls;
		});
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();
		const auto selected = context(diff, 107U, 107U);
		workbench.set_loading(selected);
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		process_events();
		const auto externalBefore = externalCalls;
		const auto observerBefore = observerCalls;
		expect(workbench.reveal_conversation_line(core::diff_side::right, 2U),
			   "drawer anchor selects a valid exact right-side line");
		process_events();
		expect(workbench.conversation_anchor().has_value()
				   && workbench.conversation_anchor()->Side == core::diff_side::right
				   && workbench.conversation_anchor()->Line == 2U,
			   "conversation marker retains the exact zero-based side and line");
		expect(externalCalls == externalBefore + 1U
				   && observerCalls == observerBefore + 1U,
			   "drawer selection reaches both the retained application sink and Qt-local observer");

		const auto marker = workbench.conversation_anchor_marker_rect(
			core::diff_side::right);
		const auto block  = workbench.right_pane()->document()->findBlockByNumber(2);
		const auto target = workbench.right_pane()->cursorRect(QTextCursor{ block });
		expect(!marker.isEmpty() && marker.width() <= 4
				   && marker.top() == target.top()
				   && marker.bottom() >= target.bottom() - 1,
			   "marker geometry is a narrow margin shape on the requested target line");
		const auto pixels      = workbench.right_pane()->viewport()->grab().toImage();
		const auto ratio       = pixels.devicePixelRatio();
		const auto markerPixel = QPoint{
			qRound(marker.center().x() * ratio),
			qRound(marker.center().y() * ratio),
		};
		expect(pixels.pixelColor(markerPixel)
				   != workbench.right_pane()->palette().color(QPalette::Base),
			   "rendered target-line marker contributes a visible palette-derived pixel");

		const auto externalBeforeSameLine = externalCalls;
		const auto observerBeforeSameLine = observerCalls;
		const auto gesturesBeforeSameLine = readingGestures;
		send_click(*workbench.right_pane()->viewport(), target.center());
		process_events();
		expect(workbench.conversation_anchor().has_value()
				   && workbench.conversation_anchor()->Side == core::diff_side::right
				   && workbench.conversation_anchor()->Line == 2U
				   && !workbench.conversation_anchor_marker_rect(
									core::diff_side::right)
						   .isEmpty(),
			   "clicking the exact anchored line retains its conversation marker");
		expect(externalCalls == externalBeforeSameLine
				   && observerCalls == observerBeforeSameLine + 1U
				   && readingGestures == gesturesBeforeSameLine + 1U,
			   "an unchanged anchored-line click reports only the Qt-local gesture");

		workbench.scroll_controller()->end();
		const auto farPosition = workbench.scroll_controller()->position();
		expect(workbench.reveal_conversation_line(core::diff_side::right, 2U)
				   && workbench.scroll_controller()->position() < farPosition,
			   "reselecting the already-selected drawer line still performs the exact reveal");

		workbench.scroll_controller()->end();
		const auto beforeDeferredReveal = workbench.scroll_controller()->position();
		workbench.resize(1'190, 750);
		expect(workbench.reveal_conversation_line(core::diff_side::right, 0U),
			   "a drawer line remains a valid request while resized geometry is pending");
		process_events();
		const auto deferredMarker = workbench.conversation_anchor_marker_rect(
			core::diff_side::right);
		expect(workbench.scroll_controller()->position() < beforeDeferredReveal
				   && workbench.right_pane()->viewport()->rect().intersects(
					   deferredMarker),
			   "the queued geometry rebuild retries the exact conversation reveal into the viewport");
		send_click(*workbench.right_pane()->viewport(), target.center());
		process_events();
		expect(!workbench.conversation_anchor().has_value(),
			   "a pane gesture clears the conversation-specific marker");
	}

	void test_prepared_semantic_index_adoption()
	{
		auto       workbench = qt::diff_workbench{ };
		const auto diff      = std::make_shared<const core::computed_file_diff>(computed("old\n", "new\n", { }, 43U));
		const auto selected  = context(*diff, 92U, 92U);
		workbench.set_loading(selected);
		workbench.publish({ .Context = selected, .Result = diff });
		expect(!workbench.current_result(), "Qt refuses missing preparation instead of doing full validation on the UI thread");
		const auto prepared   = bpr::qt::test::prepare_publication({ .Context = selected, .Result = diff });
		auto       mismatched = prepared;
		mismatched.Result     = std::make_shared<const core::computed_file_diff>(*diff);
		workbench.publish(mismatched);
		expect(!workbench.current_result(), "equal content/digest cannot substitute the index's exact result object");
		workbench.publish(prepared);
		settle_geometry(workbench);
		expect(workbench.current_result() == diff && workbench.current_semantic_index() == prepared.Index
				   && workbench.statistics().SemanticIndexAdoptions == 1U,
			   "Qt shares the exact prepared index with no reconstruction or copy");
		workbench.clear();
		workbench.publish(prepared);
		expect(!workbench.current_result() && !workbench.current_semantic_index(),
			   "a valid prepared index does not authorize a stale request after clear");
	}

	/// \brief [[BPR-1264]]. A large publication fills its source documents over
	/// bounded GUI turns and adopts them together, with the panes' typography.
	///
	/// \details Replacing both panes' text in the publishing turn held the GUI
	/// thread for both files at once (84-164 ms on the Linux station for the
	/// 20,000-line fixture), over the 100 ms input target.
	void test_large_publication_fills_over_bounded_turns()
	{
		const auto [leftText, rightText] = large_comparison_text(6'000U, 8U);
		const auto diff                  = std::make_shared<const core::computed_file_diff>(computed(leftText, rightText, { }, 43U));
		auto       workbench             = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();
		auto font = workbench.left_pane()->font();
		font.setPointSizeF(font.pointSizeF() + 3.0);
		workbench.left_pane()->setFont(font);
		workbench.left_pane()->setTabStopDistance(37.0);
		const auto selected = context(*diff, 92U, 92U);
		workbench.set_loading(selected);
		workbench.publish(bpr::qt::test::prepare_publication({ .Context = selected, .Result = diff }));
		expect(!workbench.current_result() && workbench.geometry_pending() && workbench.statistics().StagedPublications == 1U
				   && workbench.statistics().Publications == 0U && workbench.left_pane()->document()->isEmpty(),
			   "a large publication adopts nothing in the publishing turn");
		settle_geometry(workbench);
		expect(workbench.current_result() == diff && workbench.statistics().Publications == 1U
				   && workbench.statistics().PublicationFillBatches > 1U,
			   "the documents fill over several bounded turns and are then adopted");
		expect(workbench.left_pane()->toPlainText() == QString::fromStdString(leftText)
				   && workbench.right_pane()->toPlainText() == QString::fromStdString(rightText),
			   "the adopted documents hold exactly the published text");
		expect(workbench.left_pane()->document()->defaultFont() == font && qFuzzyCompare(workbench.left_pane()->tabStopDistance(), 37.0)
				   && workbench.left_pane()->lineWrapMode() == QPlainTextEdit::NoWrap && workbench.left_pane()->isReadOnly(),
			   "the pane keeps its font, tab stop, wrapping and read-only state");
		expect(workbench.selection().has_value() && workbench.scroll_controller()->current_frame().has_value(),
			   "the adopted publication selects and measures as a direct one would");

		const auto adopted = workbench.statistics().Publications;
		workbench.set_loading(selected);
		workbench.publish(bpr::qt::test::prepare_publication({ .Context = selected, .Result = diff }));
		QApplication::processEvents();
		const auto small = std::make_shared<const core::computed_file_diff>(computed("old\n", "new\n", { }, 44U));
		const auto next  = context(*small, 93U, 93U);
		workbench.set_loading(next);
		workbench.publish(bpr::qt::test::prepare_publication({ .Context = next, .Result = small }));
		settle_geometry(workbench);
		expect(workbench.current_result() == small && workbench.statistics().SupersededStagedPublications == 1U
				   && workbench.statistics().Publications == adopted + 1U && workbench.left_pane()->toPlainText() == QStringLiteral("old\n"),
			   "a newer selection drops a staged publication before it is adopted");
	}

	void test_measurement_yields_and_cancels()
	{
		const auto [leftText, rightText] = large_comparison_text(2'000U, 8U);
		const auto diff                  = std::make_shared<const core::computed_file_diff>(computed(leftText, rightText, { }, 41U));
		auto       workbench             = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();
		const auto selectedContext = context(*diff, 90U, 90U);
		const auto publish         = [&] {
			workbench.set_loading(selectedContext);
			workbench.publish(bpr::qt::test::prepare_publication({ .Context = selectedContext, .Result = diff }));
		};
		publish();
		auto inputDelivered = false;
		auto input          = QTimer{ };
		input.setInterval(0);
		QObject::connect(&input, &QTimer::timeout, &workbench, [&] {
			if (workbench.statistics().MeasurementBatches == 0U) {
				return;
			}
			input.stop();
			expect(workbench.geometry_pending() && workbench.statistics().GeometryRebuilds == 0U,
				   "an input callback runs between batches before any partial axis is published");
			expect(send_wheel(*workbench.left_pane()->viewport(), QPoint{ 0, -20 }),
				   "pending measurement consumes the native wheel without one-pane scrolling");
			expect(workbench.statistics().FrameApplications == 0U, "input cannot apply an incomplete axis");
			inputDelivered = workbench.reveal_conversation_line(core::diff_side::right, 1'600U);
		});
		input.start();
		settle_geometry(workbench);
		expect(inputDelivered && workbench.statistics().MeasurementBatches > 1U
				   && workbench.statistics().MaximumMeasurementBatchLines <= 512U,
			   "measurement yields between bounded batches and accepts semantic input");
		expect(workbench.selection()->PreferredLine.Line == 1'600U
				   && !workbench.conversation_anchor_marker_rect(core::diff_side::right).isEmpty(),
			   "completed geometry reveals the exact line requested while pending");
		const auto rebuilds = workbench.statistics().GeometryRebuilds;
		const auto batches  = workbench.statistics().MeasurementBatches;
		workbench.resize(1'180, 740);
		auto changed = false;
		QObject::disconnect(&input, nullptr, &workbench, nullptr);
		QObject::connect(&input, &QTimer::timeout, &workbench, [&] {
			if (workbench.statistics().MeasurementBatches == batches) {
				return;
			}
			input.stop();
			expect(workbench.statistics().GeometryRebuilds == rebuilds, "resize has not published partial geometry");
			auto font = workbench.font();
			font.setPointSizeF(font.pointSizeF() + 2.0);
			workbench.setFont(font);
			changed = true;
		});
		input.start();
		settle_geometry(workbench);
		expect(changed && workbench.statistics().MeasurementRestarts > 0U
				   && workbench.statistics().GeometryRebuilds == rebuilds + 1U,
			   "font change during measurement discards old layout and publishes only its replacement");
		const auto currentRebuilds = workbench.statistics().GeometryRebuilds;
		publish();
		QApplication::processEvents();
		expect(workbench.geometry_pending(), "large replacement is pending after one event turn");
		workbench.clear();
		process_events();
		expect(!workbench.current_result() && !workbench.geometry_pending()
				   && workbench.statistics().GeometryRebuilds == currentRebuilds
				   && !workbench.scroll_controller()->current_frame(),
			   "clear cancels queued measurement and prevents stale geometry adoption");
		publish();
		QApplication::processEvents();
		const auto small = std::make_shared<const core::computed_file_diff>(computed("old\n", "new\n", { }, 42U));
		const auto next  = context(*small, 91U, 91U);
		workbench.set_loading(next);
		workbench.publish(bpr::qt::test::prepare_publication({ .Context = next, .Result = small }));
		settle_geometry(workbench);
		expect(workbench.current_result() == small
				   && workbench.scroll_controller()->current_frame()->Comparison == small->Alignment.Digest
				   && workbench.statistics().GeometryRebuilds == currentRebuilds + 1U,
			   "superseding comparison cannot adopt any old measurement callback");
		auto closing = std::make_unique<qt::diff_workbench>();
		closing->set_loading(selectedContext);
		closing->publish(bpr::qt::test::prepare_publication({ .Context = selectedContext, .Result = diff }));
		QApplication::processEvents();
		closing.reset();
		process_events();
	}

	/// \brief Reads the visible native row and its actual placement, including clipping.
	[[nodiscard]] auto rendered_origin(QPlainTextEdit& aPane) -> int
	{
		const auto block = aPane.cursorForPosition(aPane.viewport()->mask().boundingRect().topLeft()).block();
		const auto y     = aPane.viewport()->mapTo(&aPane, aPane.cursorRect(QTextCursor{ block }).topLeft()).y() - aPane.contentsRect().top();
		return block.blockNumber() * aPane.fontMetrics().lineSpacing() - y;
	}

	struct ribbon_check
	{
		std::size_t Checked    = 0U;
		std::size_t Mismatched = 0U;
	};

	[[nodiscard]] auto check_ribbons(qt::diff_workbench& aWorkbench, const core::computed_file_diff& aDiff) -> ribbon_check;

	void test_fractional_unified_and_keyboard_viewports()
	{
		auto text = std::string{ };
		for (auto line = 0; line < 200; ++line) {
			text += std::to_string(line) + std::string(240U, 'x') + "\n";
		}
		const auto diff = computed(text, text, { }, 74U);
		for (const auto unified : { false, true }) {
			auto workbench = qt::diff_workbench{ };
			workbench.resize(1'000, 520);
			workbench.show();
			const auto request = context(diff, 174U, 174U);
			workbench.set_loading(request);
			workbench.publish(qt::test::prepare_publication({ .Context = request, .Result = std::make_shared<const core::computed_file_diff>(diff) }));
			settle_geometry(workbench);
			workbench.set_one_pane_enabled(unified);
			settle_geometry(workbench);
			auto* const editor = unified ? workbench.findChild<QPlainTextEdit*>(QStringLiteral("diffUnifiedSource")) : workbench.right_pane();
			expect(workbench.reveal_conversation_line(core::diff_side::right, 60U), "fractional keyboard course reveals a known line");
			settle_geometry(workbench);
			expect(send_wheel(*editor->viewport(), QPoint{ 0, -5 }), "both presentations accept a within-line movement");
			process_events();
			class wheel_observer final : public QObject
			{
			public:
				const QPointingDevice* Device    = nullptr;
				Qt::ScrollPhase        Phase     = Qt::NoScrollPhase;
				quint64                Timestamp = 0U;
				bool                   eventFilter(QObject*, QEvent* aEvent) override
				{
					if (aEvent->type() == QEvent::Wheel) {
						const auto& wheel = *static_cast<QWheelEvent*>(aEvent);
						Device            = wheel.pointingDevice();
						Phase             = wheel.phase();
						Timestamp         = wheel.timestamp();
					}
					return false;
				}
			} observer;
			editor->viewport()->installEventFilter(&observer);
			auto device = QPointingDevice{ };
			for (const auto phase : { Qt::ScrollBegin, Qt::ScrollUpdate, Qt::ScrollMomentum, Qt::ScrollEnd }) {
				auto wheel = QWheelEvent{ { }, { }, QPoint{ -20, -1 }, { }, Qt::NoButton, Qt::NoModifier, phase, false, Qt::MouseEventSynthesizedBySystem, &device };
				wheel.setTimestamp(123U);
				const auto before = workbench.line_rect(core::diff_side::right, 60U);
				static_cast<void>(workbench.scroll_by_wheel(wheel));
				expect(observer.Device == &device && observer.Phase == phase && observer.Timestamp == 123U, "forwarded wheel input preserves device, phase and timestamp");
				expect(workbench.line_rect(core::diff_side::right, 60U).top() == before.top(), "forwarded horizontal input including momentum and ending never releases vertical wobble");
			}
			const auto rowBefore    = workbench.line_rect(core::diff_side::right, 60U);
			const auto pixels       = workbench.grab().toImage();
			const auto top          = editor->verticalScrollBar()->value();
			const auto rebuilds     = workbench.statistics().GeometryRebuilds;
			const auto anchorBefore = editor->viewport()->mapTo(&workbench, workbench.conversation_anchor_marker_rect(core::diff_side::right).topLeft());
			expect(send_wheel(*editor->viewport(), QPoint{ 0, -3 }), "both presentations accept successive small motion");
			process_events();
			expect(workbench.line_rect(core::diff_side::right, 60U).top() == rowBefore.top() - 3
					   && editor->verticalScrollBar()->value() == top && workbench.grab().toImage() != pixels,
				   "unified and split rendered source rows visibly move inside one native row");
			const auto anchorAfter = editor->viewport()->mapTo(&workbench, workbench.conversation_anchor_marker_rect(core::diff_side::right).topLeft());
			expect(anchorAfter.y() == anchorBefore.y() - 3, "conversation anchors share fractional source placement");
			expect(workbench.statistics().GeometryRebuilds == rebuilds, "both presentations retain cached geometry during fractional motion");
			expect(send_wheel(*editor->viewport(), QPoint{ 0, 3 }), "both presentations accept reversal");
			process_events();
			expect(workbench.line_rect(core::diff_side::right, 60U) == rowBefore, "both presentations retrace the source geometry without drift");
			const auto fractionalTop = editor->viewport()->mask().boundingRect().top();
			expect(fractionalTop > 0, "keyboard course begins fractionally positioned");
			send_key(*editor, Qt::Key_Down);
			process_events();
			expect(workbench.selection()->PreferredLine.Line == 61U && editor->viewport()->mask().boundingRect().top() == fractionalTop,
				   "in-view Down advances one source line without realigning the viewport");
			auto shift = QKeyEvent{ QEvent::KeyPress, Qt::Key_Down, Qt::ShiftModifier };
			QCoreApplication::sendEvent(editor, &shift);
			expect(workbench.selection()->PreferredLine.Line == 62U && editor->textCursor().hasSelection()
					   && editor->viewport()->mask().boundingRect().top() == fractionalTop,
				   "in-view Shift+Down extends the native selection without realignment");
			for (auto step = 0; step < 50; ++step) {
				send_key(*editor, Qt::Key_Down);
			}
			process_events();
			const auto visibleBottom = editor->viewport()->mapTo(&workbench, editor->viewport()->visibleRegion().intersected(editor->viewport()->mask()).boundingRect().bottomLeft()).y();
			expect(workbench.selection()->PreferredLine.Line == 112U && workbench.line_rect(core::diff_side::right, 112U).bottom() == visibleBottom,
				   "repeated arrows that scroll keep the selected row flush with the displayed bottom");
			const auto keyboardRow = workbench.line_rect(core::diff_side::right, 112U);
			expect(send_wheel(*editor->viewport(), QPoint{ 0, -1 }), "touchpad resumes after keyboard scrolling");
			process_events();
			expect(workbench.line_rect(core::diff_side::right, 112U).top() == keyboardRow.top() - 1, "touchpad resumes from the applied flush origin without restoring an old residual");
			send_key(*editor, Qt::Key_PageDown);
			process_events();
			expect(editor->viewport()->mask().boundingRect().top() == 0, "page scrolling ends on a source-line boundary");
			send_key(*editor, Qt::Key_End);
			process_events();
			const auto endRow = workbench.line_rect(core::diff_side::right, 199U).top();
			expect(send_wheel(*editor->viewport(), QPoint{ 0, 1 }), "reversal from End accepts a single pixel");
			process_events();
			expect(workbench.line_rect(core::diff_side::right, 199U).top() == endRow + 1, "the terminal document margin does not change the pixel stride when reversing from End");
			expect(send_wheel(*editor->viewport(), QPoint{ -20, 1 }), "presentation-reset course establishes horizontal intent");
			workbench.set_one_pane_enabled(!unified);
			settle_geometry(workbench);
			workbench.set_one_pane_enabled(unified);
			settle_geometry(workbench);
			expect(send_wheel(*editor->viewport(), QPoint{ 0, -1 }), "presentation replacement accepts small vertical input");
		}
	}

	/// [[BPR-1527]]. Read the completed raster, before a grab repaints it.
	/// Native scrolling must leave both source frames as a fresh native paint
	/// would draw them; a forced render alone hides backing-store corruption.
	void expect_current_source_frames(qt::diff_workbench& aWorkbench)
	{
		auto* const backing = aWorkbench.backingStore();
		expect(backing != nullptr, "the raster workbench has a backing store");
		auto actual = QImage{ };
		// paintDevice is valid only inside this boundary; an empty region
		// changes no pixels, and the copy keeps no pointer to the device.
		backing->beginPaint(QRegion{ });
		if (const auto* device = backing->paintDevice(); device && device->devType() == QInternal::Image) {
			actual = static_cast<const QImage*>(device)->copy();
		}
		backing->endPaint();
		expect(!actual.isNull(), "the completed raster backing image is available");
		const auto fresh = aWorkbench.grab().toImage();
		expect(actual.size() == fresh.size() && actual.devicePixelRatio() == fresh.devicePixelRatio(),
			   "completed and fresh workbench rasters share exact pixel geometry");
		for (auto* const source : { aWorkbench.left_pane(), aWorkbench.right_pane(), aWorkbench.findChild<QPlainTextEdit*>(QStringLiteral("diffUnifiedSource")) }) {
			if (!source || !source->isVisible()) {
				continue;
			}
			const auto frameHeight = source->frameWidth();
			if (frameHeight <= 0) {
				continue;
			}
			const auto logical = QRect{ source->mapTo(&aWorkbench, QPoint{ }), QSize{ source->width(), frameHeight } };
			const auto ratio   = actual.devicePixelRatio();
			const auto pixels  = QRectF{ QPointF{ logical.topLeft() } * ratio, QSizeF{ logical.size() } * ratio }.toAlignedRect();
			if (actual.copy(pixels).convertToFormat(QImage::Format_ARGB32) != fresh.copy(pixels).convertToFormat(QImage::Format_ARGB32)) {
				std::cerr << "BPR1527_SOURCE_FRAME_MISMATCH pane=" << source->objectName().toStdString()
						  << " pixels=" << pixels.x() << ',' << pixels.y() << ',' << pixels.width() << ',' << pixels.height() << '\n';
				expect(false, "completed native source frames match a fresh render after keyboard scrolling");
			}
		}
	}

	void test_keyboard_minimal_reveal()
	{
		auto text = std::string{ };
		for (auto line = 0; line < 200; ++line) {
			text += std::to_string(line) + std::string(240U, 'x') + "\n";
		}
		auto unequal = std::string{ };
		for (auto line = 0; line < 200; ++line) {
			if (line == 72) {
				for (auto replacement = 0; replacement < 7; ++replacement) {
					unequal += "replacement" + std::to_string(replacement) + std::string(240U, 'y') + "\n";
				}
			} else if (line < 73 || line > 75) {
				unequal += std::to_string(line) + std::string(240U, 'x') + "\n";
			}
		}
		const auto comparisons = std::array{ computed(text, text, { }, 75U), computed(text, unequal, { }, 76U) };
		for (std::size_t comparison = 0U; comparison < comparisons.size(); ++comparison) {
			const auto& diff = comparisons[comparison];
			for (const auto unified : { false, true }) {
				if (comparison != 0U && unified) {
					continue;
				}
				for (const auto side : { core::diff_side::left, core::diff_side::right }) {
					auto workbench = qt::diff_workbench{ };
					workbench.resize(1'000, 520);
					auto profile = qt::appearance_profile{ };
					workbench.adopt_appearance(profile);
					profile.apply_comparison_code_font_to(workbench);
					workbench.show();
					const auto request = context(diff, 175U, 175U);
					workbench.set_loading(request);
					workbench.publish(qt::test::prepare_publication({ .Context = request, .Result = std::make_shared<const core::computed_file_diff>(diff) }));
					settle_geometry(workbench);
					workbench.set_one_pane_enabled(unified);
					settle_geometry(workbench);
					auto* const editor = unified ? workbench.findChild<QPlainTextEdit*>(QStringLiteral("diffUnifiedSource")) : side == core::diff_side::left ? workbench.left_pane()
																																							 : workbench.right_pane();
					// Exercise a bottom edge one pixel beyond a whole number of rows.
					// Both a clipped target and a fully visible bottom row must be tested.
					const auto rowHeight      = workbench.line_rect(side, 60U).height();
					const auto viewportHeight = editor->viewport()->mask().boundingRect().height();
					workbench.resize(workbench.width(), workbench.height() + (rowHeight - viewportHeight % rowHeight) % rowHeight + 1);
					settle_geometry(workbench);
					expect(workbench.reveal_conversation_line(side, 60U), "minimal keyboard reveal starts at a known source line");
					settle_geometry(workbench);
					expect(send_wheel(*editor->viewport(), QPoint{ 0, -5 }), "minimal reveal starts from a fractional origin");
					process_events();
					const auto visible = [&] {
						const auto area      = editor->viewport()->visibleRegion().intersected(editor->viewport()->mask()).boundingRect();
						const auto displayed = QRect{ editor->viewport()->mapTo(&workbench, area.topLeft()), area.size() };
						expect(editor->horizontalScrollBar()->isVisible() && displayed.bottom() == editor->horizontalScrollBar()->mapTo(&workbench, QPoint{ }).y() - 1,
							   "actual source bottom meets the native horizontal scrollbar, excluding overscan");
						return displayed;
					};
					const auto tolerance      = 1.0 / editor->devicePixelRatioF();
					auto       reveals        = 0;
					auto       previousReveal = false;
					for (auto step = 0; step < 90; ++step) {
						const auto next    = workbench.selection()->PreferredLine.Line + 1U;
						const auto before  = workbench.line_rect(side, next);
						const auto clipped = before.bottom() > visible().bottom();
						auto       key     = QKeyEvent{ QEvent::KeyPress, Qt::Key_Down, step % 2 == 0 ? Qt::ShiftModifier : Qt::NoModifier, QString{ }, step > 0, 1 };
						QCoreApplication::sendEvent(editor, &key);
						process_events();
						const auto after = workbench.line_rect(side, next);
						expect(workbench.selection()->PreferredLine.Line == next, "each arrow navigates exactly one source line");
						expect(after.top() >= visible().top() && after.bottom() <= visible().bottom(), "Down fully reveals even a partially clipped source row");
						if (clipped) {
							++reveals;
							expect(std::abs(visible().bottom() - after.bottom()) <= tolerance,
								   "Down places the selected row bottom flush with the actual visible bottom within one device pixel");
							if (previousReveal) {
								expect(std::abs(before.top() - after.top() - after.height()) <= tolerance,
									   "consecutive Down reveals scroll exactly one source row");
							}
						} else {
							expect(after == before, "Down leaves a fully visible target at its existing position");
						}
						previousReveal = clipped;
					}
					expect(reveals > 10, "minimum-reveal course exercises the lower clipped-row branch");
					if (!unified) {
						expect(check_ribbons(workbench, diff).Mismatched == 0U, "edge-flush keyboard reveal preserves shared-axis ribbon geometry");
					}
					reveals = 0;
					for (auto step = 0; step < 90; ++step) {
						const auto next    = workbench.selection()->PreferredLine.Line - 1U;
						const auto before  = workbench.line_rect(side, next);
						const auto clipped = before.top() < visible().top();
						auto       key     = QKeyEvent{ QEvent::KeyPress, Qt::Key_Up, step % 2 == 0 ? Qt::ShiftModifier : Qt::NoModifier, QString{ }, step > 0, 1 };
						QCoreApplication::sendEvent(editor, &key);
						process_events();
						const auto after = workbench.line_rect(side, next);
						expect(workbench.selection()->PreferredLine.Line == next, "each arrow navigates exactly one source line");
						expect(after.top() >= visible().top() && after.bottom() <= visible().bottom(), "Up fully reveals the selected source row");
						if (clipped) {
							++reveals;
							expect(std::abs(after.top() - visible().top()) <= tolerance,
								   "Up places the selected row top flush with the actual visible top within one device pixel");
						} else {
							expect(after == before, "Up leaves a fully visible target at its existing position");
						}
					}
					expect(reveals > 10, "minimum-reveal course exercises the upper clipped-row branch");
					expect_current_source_frames(workbench);
				}
			}
		}
	}

	/// [[BPR-1476]]. Down from a selected line far below the view, and Up from
	/// one far above it, reveal the next line at the nearest edge. Qt's block
	/// geometry walks from the first visible block and stops about two
	/// viewport heights below it (one above), answering a nearer block's rect,
	/// so a reveal measured that way stops short and leaves the line hidden.
	void test_keyboard_reveal_from_far_out_of_view()
	{
		auto text     = std::string{ };
		auto inserted = std::string{ };
		for (auto line = 0; line < 3'000; ++line) {
			text += "line " + std::to_string(line) + "\n";
			inserted += (line == 40 ? std::string{ "inserted a\ninserted b\ninserted c\n" } : std::string{ }) + "line " + std::to_string(line) + "\n";
		}
		const auto comparisons = std::array{ computed(text, text, { }, 77U), computed(text, inserted, { }, 78U) };
		for (std::size_t comparison = 0U; comparison < comparisons.size(); ++comparison) {
			const auto& diff = comparisons[comparison];
			for (const auto unified : { false, true }) {
				if (comparison != 0U && unified) {
					continue;
				}
				for (const auto side : { core::diff_side::left, core::diff_side::right }) {
					auto workbench = qt::diff_workbench{ };
					workbench.resize(1'000, 520);
					workbench.show();
					const auto request = context(diff, 177U, 177U);
					workbench.set_loading(request);
					workbench.publish(qt::test::prepare_publication({ .Context = request, .Result = std::make_shared<const core::computed_file_diff>(diff) }));
					settle_geometry(workbench);
					workbench.set_one_pane_enabled(unified);
					settle_geometry(workbench);
					auto* const editor  = unified ? workbench.findChild<QPlainTextEdit*>(QStringLiteral("diffUnifiedSource")) : side == core::diff_side::left ? workbench.left_pane()
																																							  : workbench.right_pane();
					const auto  visible = [&] {
						const auto area = editor->viewport()->visibleRegion().intersected(editor->viewport()->mask()).boundingRect();
						return QRect{ editor->viewport()->mapTo(&workbench, area.topLeft()), area.size() };
					};
					const auto press = [editor](const Qt::Key aKey) {
						auto key = QKeyEvent{ QEvent::KeyPress, aKey, Qt::NoModifier };
						QCoreApplication::sendEvent(editor, &key);
						process_events();
					};
					const auto tolerance = 1.0 / editor->devicePixelRatioF();
					const auto count     = side == core::diff_side::left ? diff.Alignment.LeftLineCount : diff.Alignment.RightLineCount;
					const auto below     = count * 2U / 3U;
					expect(workbench.select_conversation_line(side, below) && !workbench.source_line_visible(side, below + 1U), "a line far below the view is selected");
					press(Qt::Key_Down);
					expect(workbench.selection() && workbench.selection()->PreferredLine.Line == below + 1U, "Down from a line far below the view selects the next");
					const auto next = workbench.line_rect(side, below + 1U);
					expect(workbench.source_line_visible(side, below + 1U) && next.top() >= visible().top() && std::abs(visible().bottom() - next.bottom()) <= tolerance,
						   "Down from a line far below the view reveals the next flush with the bottom");
					const auto above = count / 3U;
					expect(workbench.select_conversation_line(side, above) && !workbench.source_line_visible(side, above - 1U), "a line far above the view is selected");
					press(Qt::Key_Up);
					expect(workbench.selection() && workbench.selection()->PreferredLine.Line == above - 1U, "Up from a line far above the view selects the previous");
					const auto previous = workbench.line_rect(side, above - 1U);
					expect(workbench.source_line_visible(side, above - 1U) && previous.bottom() <= visible().bottom() && std::abs(previous.top() - visible().top()) <= tolerance,
						   "Up from a line far above the view reveals the previous flush with the top");
					if (!unified) {
						expect(check_ribbons(workbench, diff).Mismatched == 0U, "a far keyboard reveal keeps the shared-axis ribbon geometry");
					}
				}
			}
		}
	}

	void test_fractional_native_viewport()
	{
		const auto [leftText, rightText] = large_comparison_text(200U, 8U);
		const auto diff                  = computed(leftText, rightText, { }, 73U);
		auto       workbench             = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		const auto selectedContext = context(diff, 173U, 173U);
		workbench.set_loading(selectedContext);
		workbench.publish(bpr::qt::test::prepare_publication({ .Context = selectedContext, .Result = std::make_shared<const core::computed_file_diff>(diff) }));
		settle_geometry(workbench);
		auto* const controller   = workbench.scroll_controller();
		auto* const editor       = workbench.right_pane();
		const auto  measurements = qt::measure_plain_text_pane(*editor, diff.Alignment.RightLineCount);
		expect(measurements.has_value(), "native fractional course measures its source boundaries");
		expect(controller->reveal_pane_origin(core::diff_side::right, measurements->Boundaries[60U] + 5.0), "native fractional course starts inside a known row");
		settle_geometry(workbench);
		const auto row      = static_cast<std::uint32_t>(editor->cursorForPosition(editor->viewport()->visibleRegion().intersected(editor->viewport()->mask()).boundingRect().topLeft()).blockNumber());
		const auto before   = workbench.line_rect(core::diff_side::right, row);
		const auto bar      = editor->verticalScrollBar()->value();
		const auto rebuilds = workbench.statistics().GeometryRebuilds;
		const auto pixels   = workbench.grab().toImage();
		expect(send_wheel(*editor->viewport(), QPoint{ 0, -3 }), "fractional native motion accepts a sub-line pixel event");
		process_events();
		const auto after = workbench.line_rect(core::diff_side::right, row);
		expect(after.top() < before.top() && before.top() - after.top() <= 4, "sub-line input visibly moves the real source row before a line step");
		expect(workbench.grab().toImage() != pixels, "composed rendered pixels change within a row");
		const auto ribbons = check_ribbons(workbench, diff);
		expect(ribbons.Checked > 0U && ribbons.Mismatched == 0U, "fractional ribbons meet the actual native source boundaries");
		expect(editor->verticalScrollBar()->value() == bar, "sub-line placement retains the containing native row");
		expect(workbench.statistics().GeometryRebuilds == rebuilds, "fractional placement does not rebuild measured geometry");
		const auto point = editor->viewport()->visibleRegion().intersected(editor->viewport()->mask()).boundingRect().topLeft() + QPoint{ 40, 1 };
		const auto hit   = editor->cursorForPosition(point).blockNumber();
		send_click(*editor->viewport(), point);
		expect(workbench.selection() && workbench.selection()->PreferredLine.Line == static_cast<std::uint32_t>(hit), "native hit testing and composed source selection share the clipped viewport");
		expect(send_wheel(*editor->viewport(), QPoint{ 0, 3 }), "fractional reverse input accepted");
		process_events();
		expect(workbench.line_rect(core::diff_side::right, row).top() == before.top(), "fractional reversal retraces the rendered source origin");
		auto profile     = qt::appearance_profile{ };
		auto code        = profile.typography().Code;
		code.ZoomPercent = 150U;
		static_cast<void>(profile.select_code_font(code));
		profile.apply_comparison_code_font_to(workbench);
		settle_geometry(workbench);
		const auto zoomedMeasurements = qt::measure_plain_text_pane(*editor, diff.Alignment.RightLineCount);
		expect(zoomedMeasurements && controller->reveal_pane_origin(core::diff_side::right, zoomedMeasurements->Boundaries[60U] + 5.0), "zoomed fractional course starts inside a measured row");
		settle_geometry(workbench);
		const auto zoomedBefore = workbench.line_rect(core::diff_side::right, 60U).top();
		expect(send_wheel(*editor->viewport(), QPoint{ 0, -3 }), "zoomed reader accepts fractional movement");
		process_events();
		const auto zoomedRibbons = check_ribbons(workbench, diff);
		expect(workbench.line_rect(core::diff_side::right, 60U).top() == zoomedBefore - 3 && zoomedRibbons.Checked > 0U && zoomedRibbons.Mismatched == 0U,
			   "after code zoom, fractional text and ribbons retain their exact common origin");
	}

	/// \brief [[BPR-1516]]. Observe every painted row, including measurement turns.
	class divider_paint_observer final : public QObject
	{
	public:
		qt::diff_workbench&              Workbench;
		std::uint32_t                    LeftLine;
		std::uint32_t                    RightLine;
		std::vector<std::pair<int, int>> Rows;

		divider_paint_observer(qt::diff_workbench& aWorkbench, const std::uint32_t aLeft, const std::uint32_t aRight)
			: Workbench{ aWorkbench }
			, LeftLine{ aLeft }
			, RightLine{ aRight }
		{
			Workbench.left_pane()->viewport()->installEventFilter(this);
			Workbench.right_pane()->viewport()->installEventFilter(this);
		}

		void observe()
		{
			Rows.emplace_back(Workbench.line_rect(core::diff_side::left, LeftLine).top(),
							  Workbench.line_rect(core::diff_side::right, RightLine).top());
		}

	protected:
		auto eventFilter(QObject* aWatched, QEvent* aEvent) -> bool override
		{
			if (aEvent->type() == QEvent::Paint) {
				observe();
			}
			return QObject::eventFilter(aWatched, aEvent);
		}
	};

	void test_divider_drag_keeps_fractional_viewport()
	{
		for (const auto& [lines, longSource] : std::array{ std::pair{ 200U, false }, std::pair{ 200U, true }, std::pair{ 2'000U, true } }) {
			auto [leftText, rightText] = large_comparison_text(lines, 8U);
			// Unequal sides and long source lines; the shipped source readers
			// intentionally do not wrap, so divider width cannot change rows.
			leftText.insert(0, "earlier-only\n");
			rightText.insert(0, "later-only-a\nlater-only-b\n");
			const auto longLine = std::string(250U, 'x') + '\n';
			if (longSource) {
				leftText += longLine;
				rightText += longLine;
			}
			const auto diff      = computed(leftText, rightText, { }, 74U);
			auto       workbench = qt::diff_workbench{ };
			workbench.resize(1'200, 760);
			workbench.show();
			const auto selectedContext = context(diff, 174U, 174U);
			workbench.set_loading(selectedContext);
			workbench.publish(qt::test::prepare_publication({ .Context = selectedContext, .Result = std::make_shared<const core::computed_file_diff>(diff) }));
			settle_geometry(workbench);
			expect(workbench.select_conversation_line(core::diff_side::right, 62U), "divider retains an explicit source selection");
			auto textSelection = QTextCursor{ workbench.right_pane()->document()->findBlockByNumber(62) };
			textSelection.setPosition(workbench.right_pane()->document()->findBlockByNumber(65).position(), QTextCursor::KeepAnchor);
			workbench.right_pane()->setTextCursor(textSelection);
			settle_geometry(workbench);
			auto* const controller = workbench.scroll_controller();
			const auto  measured   = qt::measure_plain_text_pane(*workbench.right_pane(), diff.Alignment.RightLineCount);
			expect(measured && controller->reveal_pane_origin(core::diff_side::right, measured->Boundaries[60U] + 5.0), "divider starts five pixels inside a source row");
			settle_geometry(workbench);
			const auto topLine = [](QPlainTextEdit& aSource) {
				const auto visible = aSource.viewport()->visibleRegion().intersected(aSource.viewport()->mask()).boundingRect();
				return static_cast<std::uint32_t>(aSource.cursorForPosition(visible.topLeft()).blockNumber());
			};
			const auto leftLine  = topLine(*workbench.left_pane());
			const auto rightLine = topLine(*workbench.right_pane());
			const auto hitPoint  = workbench.right_pane()->viewport()->visibleRegion().intersected(workbench.right_pane()->viewport()->mask()).boundingRect().topLeft() + QPoint{ 40, 10 };
			const auto hitLine   = workbench.right_pane()->cursorForPosition(hitPoint).blockNumber();
			auto       observer  = divider_paint_observer{ workbench, leftLine, rightLine };
			observer.observe();
			const auto render = [&](const QString& aStep) {
				const auto destination = qEnvironmentVariable("BPR_QT_DIFF_WORKBENCH_RENDER_PATH");
				if (!destination.isEmpty()) {
					expect(workbench.grab().save(destination + QStringLiteral("-divider-%1-%2-%3.png").arg(lines).arg(longSource ? QStringLiteral("long") : QStringLiteral("short")).arg(aStep)), "the actual divider course renders its source panes");
				}
			};
			render(QStringLiteral("before"));
			const auto  initialRows = observer.Rows.front();
			const auto  selection   = workbench.selection();
			const auto  position    = controller->position();
			const auto  cursor      = workbench.right_pane()->textCursor();
			const auto  before      = workbench.statistics();
			auto* const handle      = workbench.link_map();
			auto* const splitter    = qobject_cast<QSplitter*>(handle->parentWidget());
			expect(splitter != nullptr, "the actual center handle belongs to the comparison splitter");
			const auto sizes = splitter->sizes();
			const auto start = handle->mapToGlobal(handle->rect().center());
			const auto send  = [handle](const QEvent::Type aType, const QPoint aGlobal, const Qt::MouseButton aButton, const Qt::MouseButtons aButtons) {
				auto event = QMouseEvent{ aType, QPointF{ handle->mapFromGlobal(aGlobal) }, QPointF{ aGlobal }, aButton, aButtons, Qt::NoModifier };
				QCoreApplication::sendEvent(handle, &event);
			};
			send(QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
			for (const auto direction : { 1, -1 }) {
				for (auto step = 1; step <= 20; ++step) {
					const auto offset = direction == 1 ? step * 6 : (20 - step) * 6;
					send(QEvent::MouseMove, start + QPoint{ offset, 0 }, Qt::NoButton, Qt::LeftButton);
					observer.observe();
					// Render before the queued rebuild and during its bounded turns:
					// a final settled screenshot alone misses the vertical shake.
					auto timer = QElapsedTimer{ };
					timer.start();
					do {
						static_cast<void>(workbench.grab());
						QApplication::processEvents();
						observer.observe();
					} while (workbench.geometry_pending() && timer.elapsed() < 5'000);
					expect(!workbench.geometry_pending(), "divider measurement settles within the local deadline");
				}
				render(direction == 1 ? QStringLiteral("wide") : QStringLiteral("reversed"));
			}
			send(QEvent::MouseButtonRelease, start, Qt::LeftButton, Qt::NoButton);
			settle_geometry(workbench);
			auto displacement = 0;
			auto rowChanges   = 0U;
			auto previousRows = initialRows;
			for (const auto& row : observer.Rows) {
				displacement = std::max({ displacement, std::abs(row.first - initialRows.first), std::abs(row.second - initialRows.second) });
				rowChanges += row != previousRows ? 1U : 0U;
				previousRows = row;
			}
			std::cout << "divider-drag platform=" << QGuiApplication::platformName().toStdString() << " arch=" << QSysInfo::currentCpuArchitecture().toStdString()
					  << " qt=" << qVersion() << " scale=" << workbench.devicePixelRatioF() << " left_lines=" << diff.Alignment.LeftLineCount << " right_lines=" << diff.Alignment.RightLineCount
					  << " samples=" << observer.Rows.size() << " displacement=" << displacement << " row_changes=" << rowChanges << " rebuilds=" << workbench.statistics().GeometryRebuilds - before.GeometryRebuilds << '\n';
			expect(splitter->sizes() == sizes && workbench.statistics().GeometryRebuilds > before.GeometryRebuilds, "continuous reversed pointer drags resize the panes and return to the initial widths");
			expect(displacement == 0, "horizontal divider dragging retains every painted vertical row position");
			expect(close(controller->position(), position) && workbench.selection() == selection
					   && workbench.right_pane()->textCursor().position() == cursor.position() && workbench.right_pane()->textCursor().anchor() == cursor.anchor(),
				   "divider dragging retains shared comparison position and source selection");
			expect(workbench.right_pane()->cursorForPosition(hitPoint).blockNumber() == hitLine, "native hit testing retains the same source line after reversed divider drags");
			expect((workbench.left_pane()->horizontalScrollBar()->maximum() > 0) == longSource
					   && (workbench.right_pane()->horizontalScrollBar()->maximum() > 0) == longSource,
				   "unwrapped width changes retain accurate native horizontal ranges for short and long files");
			expect(check_ribbons(workbench, diff).Mismatched == 0U, "resized unequal panes retain exact ribbon correspondence");
		}
	}

	void test_scale_and_high_frequency_evidence()
	{
		const auto [leftText, rightText] = large_comparison_text(2'000U, 8U);
		const auto diff                  = computed(leftText, rightText, { }, 40U);
		expect(
			diff.Alignment.Groups.size() >= 200U,
			"dense fixture retains many separated semantic changes");

		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		settle_geometry(workbench);
		const auto selectedContext = context(diff, 80U, 80U);
		workbench.set_loading(selectedContext);
		auto timer = QElapsedTimer{ };
		timer.start();
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = selectedContext,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		settle_geometry(workbench);
		const auto publicationMilliseconds = timer.elapsed();
		const auto publicationStats        = workbench.statistics();
		expect(
			publicationStats.SemanticIndexAdoptions == 1U
				&& publicationStats.SemanticIndexEntries
					>= diff.Alignment.Segments.size()
				&& publicationStats.GeometryRebuilds == 1U
				&& publicationStats.PaneMeasurements == 2U,
			"large publication adopts its prepared index once and measures both panes once");

		auto* const controller = workbench.scroll_controller();
		auto* const linkMap    = workbench.link_map();
		const auto  start      = controller->maximum_position() * 0.25;
		controller->set_position(start);
		settle_geometry(workbench);
		const auto unitStart = controller->position();
		expect(
			send_wheel(*workbench.left_pane()->viewport(), QPoint{ 0, -1 }),
			"scale probe establishes one accepted pixel-wheel unit");
		const auto unit = controller->position() - unitStart;
		expect(!close(unit, 0.0), "one pixel input retains nonzero subpixel motion");
		controller->set_position(start);
		settle_geometry(workbench);

		const auto measuredSource = qt::measure_plain_text_pane(*workbench.right_pane(), diff.Alignment.RightLineCount);
		expect(measuredSource.has_value(), "large-file course has native measured boundaries");
		const auto renderedOrigin = [&] {
			auto&      editor = *workbench.right_pane();
			const auto row    = editor.cursorForPosition(editor.viewport()->mask().boundingRect().topLeft()).block();
			const auto y      = editor.viewport()->mapTo(&editor, editor.cursorRect(QTextCursor{ row }).topLeft()).y() - editor.contentsRect().top();
			return measuredSource->Boundaries[static_cast<std::size_t>(row.blockNumber())] - y;
		};
		const auto renderedBefore   = renderedOrigin();
		const auto workbenchBefore  = workbench.statistics();
		const auto controllerBefore = controller->statistics();
		const auto linkBefore       = linkMap->statistics();
		timer.restart();
		constexpr auto inputCount = 1'000U;
		for (auto input = 0U; input < inputCount; ++input) {
			expect(
				send_wheel(*workbench.left_pane()->viewport(), QPoint{ 0, -1 }),
				"rapid pixel input remains routed through the shared controller");
		}
		const auto inputDispatchMilliseconds = timer.elapsed();
		const auto expectedPosition          = std::clamp(
			start + unit * static_cast<qreal>(inputCount),
			0.0,
			controller->maximum_position());
		expect(
			close(controller->position(), expectedPosition, 0.000'01),
			"rapid input retains the exact summed and clamped subpixel position");
		const auto beforeDrain = controller->statistics();
		timer.restart();
		settle_geometry(workbench);
		const auto eventDrainMilliseconds = timer.elapsed();
		expect(std::abs(renderedOrigin() - renderedBefore - 1'000.0) <= 1.0, "large-file course moves the real rendered source origin by its accepted pixel travel");
		const auto workbenchAfter    = workbench.statistics();
		const auto controllerAfter   = controller->statistics();
		const auto linkAfter         = linkMap->statistics();
		const auto visibilityQueries = linkAfter.VisibilityQueries
			- linkBefore.VisibilityQueries;
		const auto paintEvents = linkAfter.PaintEvents - linkBefore.PaintEvents;
		expect(
			workbenchAfter.SemanticIndexAdoptions
					== workbenchBefore.SemanticIndexAdoptions
				&& workbenchAfter.SemanticQueries
					== workbenchBefore.SemanticQueries
				&& workbenchAfter.GeometryRebuilds
					== workbenchBefore.GeometryRebuilds
				&& workbenchAfter.PaneMeasurements
					== workbenchBefore.PaneMeasurements,
			"ordinary rapid scroll performs no semantic query, index rebuild, axis rebuild, or pane scan");
		expect(
			controllerAfter.FramesApplied - controllerBefore.FramesApplied
					== inputCount
				&& beforeDrain.RepaintRequestsCoalesced
					> controllerBefore.RepaintRequestsCoalesced
				&& controllerAfter.RepaintDeliveries
					== controllerBefore.RepaintDeliveries + 1U,
			"rapid frames preserve motion while repaint delivery coalesces once");
		expect(
			linkAfter.RangeSearches - linkBefore.RangeSearches
					>= inputCount * 4U
				&& linkAfter.PeakCandidateEntries
					< linkAfter.RetainedChangedEntries
				&& linkAfter.PeakVisiblePaths
					<= linkAfter.PeakCandidateEntries
				&& linkAfter.CandidateEntriesVisited
						- linkBefore.CandidateEntriesVisited
					<= visibilityQueries * linkAfter.PeakCandidateEntries
				&& linkAfter.VisiblePathsBuilt - linkBefore.VisiblePathsBuilt
					<= visibilityQueries * linkAfter.PeakVisiblePaths
				&& linkAfter.PaintedPathsVisited
						- linkBefore.PaintedPathsVisited
					<= paintEvents * linkAfter.PeakVisiblePaths
				&& linkAfter.CapacityGrowths == linkBefore.CapacityGrowths,
			"every rapid frame and paint stays within peak near-visible work and grows no retained buffers");

		const auto resizeBefore    = workbench.statistics();
		const auto resizeSelection = workbench.selection();
		for (auto resize = 0U; resize < 100U; ++resize) {
			workbench.resize(
				1'180 + static_cast<int>(resize % 7U),
				740 + static_cast<int>(resize % 5U));
		}
		settle_geometry(workbench);
		const auto resizeAfter = workbench.statistics();
		expect(
			resizeAfter.GeometryRebuilds == resizeBefore.GeometryRebuilds + 1U
				&& resizeAfter.PaneMeasurements
					== resizeBefore.PaneMeasurements + 2U,
			"resize storm coalesces to one two-pane axis rebuild");
		expect(
			resizeAfter.SemanticIndexAdoptions
					== resizeBefore.SemanticIndexAdoptions
				&& resizeAfter.SemanticQueries == resizeBefore.SemanticQueries
				&& workbench.selection() == resizeSelection
				&& std::isfinite(controller->position()),
			"resize rebuild performs no semantic query and preserves selection and position");
		expect(
			workbench.size() == QSize{ 1'181, 744 },
			"resize storm applies its final requested dimensions");

		auto moveOptions                 = core::diff_options{ };
		moveOptions.Moves                = core::move_detection_policy::deterministic_exact;
		const auto [moveLeft, moveRight] = dense_move_text();
		const auto moved                 = computed(moveLeft, moveRight, moveOptions, 42U);
		expect(
			moved.Alignment.Moves.size() == 6U,
			"move-enabled scale fixture retains several selected move relations");
		const auto movedContext = context(moved, 82U, 82U);
		workbench.set_loading(movedContext);
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = movedContext,
			.Result  = std::make_shared<const core::computed_file_diff>(moved),
		}));
		settle_geometry(workbench);
		expect(
			workbench.link_map()->statistics().LastMoveOverlays
				== moved.Alignment.Moves.size(),
			"move-enabled selected group resolves every visible overlay without frame sorting");

		const auto [insertionLeft, insertionRight] = large_insertion_text();
		const auto insertion                       = computed(
			insertionLeft, insertionRight, { }, 41U);
		const auto insertionContext = context(insertion, 81U, 81U);
		workbench.set_loading(insertionContext);
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = insertionContext,
			.Result  = std::make_shared<const core::computed_file_diff>(insertion),
		}));
		settle_geometry(workbench);
		expect(
			insertion.Alignment.RightLineCount
					> insertion.Alignment.LeftLineCount + 4'000U
				&& workbench.statistics().SemanticIndexAdoptions == 3U
				&& workbench.link_map()->statistics().LastVisiblePaths
					<= workbench.link_map()->statistics().LastCandidateEntries,
			"very large one-sided insertion remains indexed and visible-path bounded");

		std::cout
			<< "bpr0080-scale-context platform="
			<< QSysInfo::prettyProductName().toStdString()
			<< " arch=" << QSysInfo::currentCpuArchitecture().toStdString()
			<< " qt=" << QT_VERSION_STR
			<< " threads=" << QThread::idealThreadCount()
			<< " dpr=" << workbench.devicePixelRatioF()
			<< " lines=" << diff.Alignment.LeftLineCount << '/'
			<< diff.Alignment.RightLineCount
			<< " segments=" << diff.Alignment.Segments.size()
			<< " groups=" << diff.Alignment.Groups.size()
			<< " publish_ms=" << publicationMilliseconds
			<< " input_dispatch_ms=" << inputDispatchMilliseconds
			<< " event_drain_ms=" << eventDrainMilliseconds << '\n';
	}

	/// \brief The painted diff must not grow with the file it is painting.
	auto test_line_painting_stays_bounded_by_the_viewport() -> void
	{
		// Every line differs, so an unbounded painter emits one selection per
		// line per pane. That is the shape of the defect: the widget count
		// stays at its bound and QPlainTextEdit still walks the whole
		// extra-selection list on every paint.
		// The same change interval the existing large-comparison test already
		// proves computable, so only the length varies - which is the thing
		// under test. An all-changed comparison would be a better stress but is
		// quadratic, and the core refuses it on its own matrix-cell budget.
		const auto small = large_comparison_text(200U, 8U);
		const auto large = large_comparison_text(2'000U, 8U);
		const auto lines = [](const auto& aPair) {
			return std::ranges::count(aPair.first, '\n');
		};
		expect(lines(large) >= 10 * lines(small),
			   "the large comparison really is an order of magnitude longer");

		const auto measure = [](const std::pair<std::string, std::string>& aText)
			-> std::size_t {
			const auto diff      = computed(aText.first, aText.second, { }, 40U);
			auto       workbench = qt::diff_workbench{ };
			workbench.resize(1'200, 760);
			workbench.show();
			process_events();
			// Painting only happens once the surface has been given the
			// semantic palette; a workbench without one paints nothing.
			workbench.adopt_appearance(qt::appearance_profile{ });
			const auto selected = context(diff, 80U, 80U);
			workbench.set_loading(selected);
			workbench.publish(bpr::qt::test::prepare_publication({
				.Context = selected,
				.Result  = std::make_shared<const core::computed_file_diff>(diff),
			}));
			process_events();
			const auto selections = static_cast<std::size_t>(workbench.left_pane()->extraSelections().size());
			settle_geometry(workbench);
			// [[BPR-1525]]. Glyph caching must follow the viewport rather than
			// accumulate every row the reviewer has passed. Check the actual
			// native layouts at both ends, including eviction of the old end.
			const auto checkCaches = [&] {
				for (auto* editor : { workbench.left_pane(), workbench.right_pane() }) {
					editor->viewport()->repaint();
					auto cached = 0;
					auto height = std::numeric_limits<qreal>::max();
					for (auto block = editor->document()->begin(); block.isValid(); block = block.next()) {
						if (block.layout() != nullptr && block.layout()->cacheEnabled()) {
							++cached;
							height = std::min(height, block.layout()->lineAt(0).height());
						}
					}
					expect(cached > 0 && height > 0 && cached <= qCeil(editor->viewport()->height() / height) + 2,
						   "glyph layout caches are nonempty and bounded by one viewport after scrolling");
				}
			};
			checkCaches();
			const auto initialTop = workbench.right_pane()->cursorForPosition(QPoint{ 0, 0 }).blockNumber();
			expect(workbench.shared_scrollbar()->maximum() > workbench.shared_scrollbar()->minimum(), "cache course has a scrollable source");
			workbench.shared_scrollbar()->setValue(workbench.shared_scrollbar()->maximum());
			process_events();
			expect(workbench.right_pane()->cursorForPosition(QPoint{ 0, 0 }).blockNumber() > initialTop + 100,
				   "cache course actually leaves the original native viewport");
			checkCaches();
			workbench.shared_scrollbar()->setValue(workbench.shared_scrollbar()->minimum());
			process_events();
			checkCaches();
			workbench.clear();
			process_events();
			return selections;
		};

		const auto smallSelections = measure(small);
		const auto largeSelections = measure(large);
		expect(smallSelections > 0U,
			   "an all-changed comparison paints something at all, so the "
			   "bound below is not satisfied by painting nothing");
		// The ratio is the claim, exactly as it is for the widget count. A
		// hundredfold longer file must not cost a hundredfold more selections.
		expect(largeSelections <= 4U * smallSelections,
			   "a comparison ten times longer paints no more than a constant "
			   "factor more line backgrounds");
	}
	/// The left side of the classified fixture. Line 1 is the load-bearing
	/// one: nothing about `that continues here` reads as a comment, and only
	/// the `/*` two lines above makes it one. A pane that reasoned about the
	/// line in front of it could not colour it, so painting it proves the
	/// pane took the answer from the complete-blob classification.
	///
	/// Line 4 carries a two-byte character ahead of a span, which is where a
	/// byte offset used as a character index goes wrong by exactly one.
	constexpr std::string_view classified_left
		= "/* a long banner comment\n"
		  "   that continues here\n"
		  "   and ends here */\n"
		  "int alpha = 1;\n"
		  "const char* na\xc3\xafve = \"text\";\n"
		  "int beta = 2;\n";
	constexpr std::string_view classified_right
		= "/* a long banner comment\n"
		  "   that continues here\n"
		  "   and ends here */\n"
		  "int alpha = 1;\n"
		  "const char* na\xc3\xafve = \"text\";\n"
		  "int beta = 3;\n";

	[[nodiscard]] auto classified_spans() -> application::token_span_list
	{
		using role = core::appearance_role;
		return {
			{ .Line = 0U, .Column = 0U, .Length = 24U, .Role = role::syntax_comment },
			{ .Line = 1U, .Column = 0U, .Length = 22U, .Role = role::syntax_comment },
			{ .Line = 2U, .Column = 0U, .Length = 19U, .Role = role::syntax_comment },
			{ .Line = 3U, .Column = 0U, .Length = 3U, .Role = role::syntax_type },
			{ .Line = 4U, .Column = 0U, .Length = 5U, .Role = role::syntax_keyword },
			// Byte 21 of the line, but character 20: the `ï` ahead of it is two
			// bytes and one UTF-16 unit.
			{ .Line = 4U, .Column = 21U, .Length = 6U, .Role = role::syntax_string },
			{ .Line = 5U, .Column = 0U, .Length = 3U, .Role = role::syntax_type },
		};
	}

	[[nodiscard]] auto segment_by_id(
		const core::computed_file_diff&   aDiff,
		const core::alignment_segment_id& aId) -> const core::alignment_segment&
	{
		const auto found = std::ranges::find_if(
			aDiff.Alignment.Segments,
			[&aId](const core::alignment_segment& aSegment) {
				return aSegment.Id == aId;
			});
		expect(
			found != aDiff.Alignment.Segments.end(),
			"a move names a segment the alignment contains");
		return *found;
	}

	/// \brief The diff line band painted on one line, if any.
	///
	/// \details The **first** full-width selection on the line, not the last.
	/// Three different things are full-width - the diff band, the change-group
	/// boundary, and the reviewer's selection - and they are emitted in that
	/// order because that is the order they must composite in. Reading the last
	/// one reports the selection colour on whichever line the reviewer is on,
	/// which is what the first version of this helper did: it reported the
	/// selection highlight and made a correctly painted moved band look wrong.
	///
	/// Anything narrower is intraline emphasis and is deliberately not reported
	/// here.
	[[nodiscard]] auto band_color(QPlainTextEdit& aPane, const int aLine)
		-> QColor
	{
		for (const auto& selection : aPane.extraSelections()) {
			if (
				selection.format.boolProperty(QTextFormat::FullWidthSelection)
				&& selection.cursor.blockNumber() == aLine) {
				return selection.format.background().color();
			}
		}
		return { };
	}

	/// \brief The character range of one line's intraline emphasis.
	[[nodiscard]] auto emphasis_range(
		QPlainTextEdit& aPane,
		const int       aLine,
		const QColor&   aColor) -> std::optional<std::pair<qsizetype, qsizetype>>
	{
		for (const auto& selection : aPane.extraSelections()) {
			if (
				selection.format.boolProperty(QTextFormat::FullWidthSelection)
				|| selection.cursor.blockNumber() != aLine
				|| selection.format.background().color() != aColor) {
				continue;
			}
			const auto block = selection.cursor.block();
			return std::pair{
				static_cast<qsizetype>(
					selection.cursor.selectionStart() - block.position()),
				static_cast<qsizetype>(
					selection.cursor.selectionEnd()
					- selection.cursor.selectionStart()),
			};
		}
		return std::nullopt;
	}

	[[nodiscard]] auto block_formats(QPlainTextEdit& aPane, const int aLine)
		-> QList<QTextLayout::FormatRange>
	{
		const auto block = aPane.document()->findBlockByNumber(aLine);
		if (!block.isValid() || block.layout() == nullptr) {
			return { };
		}
		return block.layout()->formats();
	}

	[[nodiscard]] auto tabbed_glyph_x(QPlainTextEdit& aPane, const int aLine, const int aDepth) -> int
	{
		const auto block = aPane.document()->findBlockByNumber(aLine);
		expect(block.isValid() && block.text().startsWith(QString(aDepth, QChar{ '\t' })),
			   "the original line retains its authored tab prefix");
		auto cursor = QTextCursor{ block };
		cursor.setPosition(block.position() + aDepth);
		return aPane.viewport()->mapTo(&aPane, aPane.cursorRect(cursor).topLeft()).x();
	}

	[[nodiscard]] auto tabbed_correspondence_geometry(qt::diff_workbench& aWorkbench, const int aTabWidth) -> std::array<int, 3U>
	{
		auto positions = std::array<int, 3U>{ };
		for (auto depth = 1; depth <= 3; ++depth) {
			const auto leftX  = tabbed_glyph_x(*aWorkbench.left_pane(), depth - 1, depth);
			const auto rightX = tabbed_glyph_x(*aWorkbench.right_pane(), depth, depth);
			expect(leftX == rightX, "corresponding tab depths have equal pane-relative first-glyph x");
			positions[static_cast<std::size_t>(depth - 1)] = leftX;
		}
		const auto advance = QFontMetricsF{ aWorkbench.left_pane()->font() }.horizontalAdvance(QLatin1Char{ ' ' }) * aTabWidth;
		expect(std::abs(positions[1] - positions[0] - advance) <= 1.0
				   && std::abs(positions[2] - positions[1] - advance) <= 1.0,
			   "painted tab depths advance by the selected number of code-font spaces");
		expect(positions[0] < positions[1] && positions[1] < positions[2],
			   "one, two and three authored tabs occupy distinct indentation depths");
		return positions;
	}

	std::vector<QString>* ZoomAnnouncements = nullptr;

	void capture_zoom_announcement(QAccessibleEvent* const aEvent)
	{
		if (ZoomAnnouncements != nullptr && aEvent != nullptr && aEvent->type() == QAccessible::Announcement) {
			ZoomAnnouncements->push_back(static_cast<const QAccessibleAnnouncementEvent*>(aEvent)->message());
		}
	}

	/// \brief [[BPR-1336]]. The comparison's zoom scales the panes, their gutters,
	/// their tab stops and the detail pane, and keeps the reader's line where it
	/// was; ⌘-scroll and pinch step it; a plain scroll still scrolls.
	auto test_comparison_zoom_scales_and_holds_the_reading_line() -> void
	{
		auto left = std::string{ };
		for (auto line = 0U; line < 2'000U; ++line) {
			left += "\tline " + std::to_string(line) + " of the zoom fixture\n";
		}
		auto right = left;
		right.replace(right.find("\tline 1200 of"), 13U, "\tLINE 1200 of");
		const auto diff      = computed(left, right, { }, 61U);
		auto       workbench = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		auto profile = qt::appearance_profile{ };
		auto code    = profile.typography().Code;
		workbench.adopt_appearance(profile);
		profile.apply_comparison_code_font_to(workbench);
		const auto selected = context(diff, 92U, 92U);
		workbench.set_loading(selected);
		workbench.publish(qt::test::prepare_publication({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		settle_geometry(workbench);
		expect(workbench.select_conversation_line(core::diff_side::right, 1'200U), "the reader is on line 1201");
		settle_geometry(workbench);
		// Put the line well above the rail, so keeping the rail's line would
		// move it, and keeping the top line would too.
		const auto lineTop = [&workbench] { return workbench.line_rect(core::diff_side::right, 1'200U).top(); };
		workbench.scroll_controller()->scroll_by(static_cast<qreal>(lineTop() - 120));
		process_events();
		const auto before = lineTop();
		expect(before > 60 && before < 200, "the reader's line sits high in the viewport: " + std::to_string(before));

		const auto gutter = [&workbench] {
			return workbench.right_pane()->findChild<QWidget*>(QStringLiteral("diffSourceLineNumbers"))->width();
		};
		const auto size        = workbench.right_pane()->font().pointSizeF();
		const auto gutterAt100 = gutter();
		const auto held        = workbench.statistics().ReadingPositionsHeld;
		auto       announced   = std::vector<QString>{ };
		ZoomAnnouncements      = &announced;
		const auto previous    = QAccessible::installUpdateHandler(capture_zoom_announcement);

		code.ZoomPercent = 150U;
		expect(profile.select_code_font(code).Changed, "a zoom changes presentation");
		profile.apply_comparison_code_font_to(workbench);
		const auto height = workbench.line_rect(core::diff_side::right, 1'200U).height();
		expect(std::abs(lineTop() - before) <= height, "in the same turn, the reader's line is still where it was");
		settle_geometry(workbench);
		expect(std::abs(lineTop() - before) <= height, "and stays there once the geometry is remeasured: " + std::to_string(before) + " to " + std::to_string(lineTop()));
		expect(workbench.statistics().ReadingPositionsHeld == held + 1U, "the remeasured geometry placed the held line once");
		for (const auto* const pane : { workbench.left_pane(), workbench.right_pane(), workbench.detail_pane() }) {
			expect(std::abs(pane->font().pointSizeF() - size * 1.5) < 0.01, "both panes and the detail pane show 150% of the code size");
		}
		expect(std::abs(workbench.right_pane()->tabStopDistance() - profile.comparison_tab_stop_distance()) < 0.01
				   && profile.comparison_tab_stop_distance() > profile.code_tab_stop_distance() * 1.2,
			   "the tab stop scales with the zoom");
		const auto digits = QFontMetricsF{ workbench.right_pane()->font() }.horizontalAdvance(QStringLiteral("2000"));
		expect(gutter() > gutterAt100 && gutter() >= digits,
			   "the gutter widens to hold its four digits at the new size: " + std::to_string(gutterAt100) + " to " + std::to_string(gutter()));
		expect(std::abs(profile.code_font().pointSizeF() - size) < 0.01, "the code font every other surface takes keeps the code size");

		workbench.show_code_zoom(150U, core::zoomed_point_size(code));
		auto* const pill = workbench.code_zoom_indicator();
		expect(pill->isVisible() && pill->accessibleName() == QStringLiteral("Code zoom 150%, %1 pt").arg(qt::point_size_text(core::zoomed_point_size(code))),
			   "the indicator shows the percentage and the size on screen");
		expect(!announced.empty() && announced.back() == QStringLiteral("Code zoom 150%, %1 points").arg(qt::point_size_text(core::zoomed_point_size(code))),
			   "and the same words are announced");
		ZoomAnnouncements = nullptr;
		static_cast<void>(QAccessible::installUpdateHandler(previous));
		auto timer = QElapsedTimer{ };
		timer.start();
		while (pill->isVisible() && timer.elapsed() < 3'000) {
			QApplication::processEvents();
			QThread::msleep(20);
		}
		expect(!pill->isVisible() && timer.elapsed() >= 1'000, "the indicator leaves by itself after a moment");

		code.ZoomPercent = 100U;
		expect(profile.select_code_font(code).Changed, "Actual Size changes presentation");
		profile.apply_comparison_code_font_to(workbench);
		settle_geometry(workbench);
		expect(std::abs(lineTop() - before) <= height && gutter() == gutterAt100, "Actual Size brings the line, the size and the gutter back");

		// ⌘-scroll and pinch.
		auto steps = std::vector<core::code_zoom_step>{ };
		workbench.set_code_zoom_dispatch([&steps](const core::code_zoom_step aStep) { steps.push_back(aStep); });
		const auto wheel = [&](QWidget& aSurface, const int aAngle, const Qt::KeyboardModifiers aModifiers) {
			auto event = QWheelEvent{ QPointF{ 8.0, 8.0 }, QPointF{ 8.0, 8.0 }, QPoint{ }, QPoint{ 0, aAngle }, Qt::NoButton, aModifiers, Qt::NoScrollPhase, false };
			event.ignore();
			QCoreApplication::sendEvent(&aSurface, &event);
			return event.isAccepted();
		};
		const auto position = workbench.scroll_controller()->position();
		expect(wheel(*workbench.right_pane()->viewport(), 120, Qt::ControlModifier) && steps == std::vector{ core::code_zoom_step::in },
			   "a ⌘-scroll notch away zooms in one step");
		for (auto part = 0; part < 3; ++part) {
			expect(wheel(*workbench.left_pane()->viewport(), -40, Qt::ControlModifier), "a trackpad's small deltas are taken");
		}
		expect(steps.size() == 2U && steps.back() == core::code_zoom_step::out, "three small deltas make one step out");
		expect(wheel(*workbench.link_map(), 120, Qt::ControlModifier) && steps.size() == 3U, "the connector zooms too");
		expect(std::abs(workbench.scroll_controller()->position() - position) < 0.001 && std::abs(workbench.right_pane()->font().pointSizeF() - size) < 0.01,
			   "⌘-scroll neither scrolls nor zooms a pane by itself");
		expect(wheel(*workbench.right_pane()->viewport(), -120, Qt::NoModifier) && steps.size() == 3U
				   && workbench.scroll_controller()->position() > position,
			   "a plain scroll still scrolls and does not zoom");
		const auto pinch = [&](const Qt::NativeGestureType aType, const qreal aValue) {
			auto event = QNativeGestureEvent{ aType, QPointingDevice::primaryPointingDevice(), 2, QPointF{ 8.0, 8.0 }, QPointF{ 8.0, 8.0 }, QPointF{ 8.0, 8.0 }, aValue, QPointF{ } };
			event.ignore();
			QCoreApplication::sendEvent(workbench.right_pane()->viewport(), &event);
			return event.isAccepted();
		};
		static_cast<void>(pinch(Qt::BeginNativeGesture, 0.0));
		expect(pinch(Qt::ZoomNativeGesture, 0.10) && steps.size() == 3U, "a small pinch is not yet a step");
		expect(pinch(Qt::ZoomNativeGesture, 0.10) && steps.size() == 4U && steps.back() == core::code_zoom_step::in, "a pinch out of 0.15 is one step in");
		expect(pinch(Qt::ZoomNativeGesture, -0.35) && steps.size() == 6U && steps.back() == core::code_zoom_step::out, "pinching in 0.35 is two steps out");
		static_cast<void>(pinch(Qt::EndNativeGesture, 0.0));
		expect(workbench.statistics().CodeZoomGestures == 6U, "every step was counted");
	}

	/// \brief [[BPR-1336]]. The comparison at 100% and 150% with the zoom pill,
	/// in the four themes, for the design page and the owner's look. Written
	/// only when a directory is named.
	auto test_code_zoom_design_captures() -> void
	{
		const auto directory = qEnvironmentVariable("BPR_CODE_ZOOM_CAPTURE_DIR");
		if (directory.isEmpty()) {
			return;
		}
		auto left = std::string{ };
		for (auto line = 0U; line < 400U; ++line) {
			left += "\tif (reader.line(" + std::to_string(line) + ") != expected) { return mismatch(" + std::to_string(line) + "); }\n";
		}
		auto right = left;
		right.replace(right.find("mismatch(212)"), 13U, "report(212)  ");
		const auto diff = computed(left, right, { }, 62U);
		for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark, core::appearance_mode::high_contrast_light, core::appearance_mode::high_contrast_dark }) {
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			auto workbench = qt::diff_workbench{ };
			workbench.resize(1'100, 560);
			profile.apply_to(workbench);
			workbench.adopt_appearance(profile);
			profile.apply_comparison_code_font_to(workbench);
			workbench.show();
			const auto selected = context(diff, 93U, 93U);
			workbench.set_loading(selected);
			workbench.publish(qt::test::prepare_publication({ .Context = selected, .Result = std::make_shared<const core::computed_file_diff>(diff) }));
			settle_geometry(workbench);
			static_cast<void>(workbench.select_conversation_line(core::diff_side::right, 212U));
			settle_geometry(workbench);
			const auto name = core::stable_name(mode);
			const auto save = [&](const QString& aStep) {
				process_events();
				expect(workbench.grab().save(QStringLiteral("%1/diff-zoom-%2-%3.png").arg(directory, QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size())), aStep), "PNG"),
					   "the zoom capture is written");
			};
			save(QStringLiteral("100"));
			for (const auto zoom : { 150U, 67U }) {
				auto code        = profile.typography().Code;
				code.ZoomPercent = zoom;
				static_cast<void>(profile.select_code_font(code));
				profile.apply_comparison_code_font_to(workbench);
				settle_geometry(workbench);
				workbench.show_code_zoom(zoom, core::zoomed_point_size(code));
				save(QString::number(zoom));
			}
		}
	}

	/// \brief Syntax and tab geometry preserve exact natural source correspondence.
	auto test_tabbed_source_correspondence_survives_syntax_and_tab_width() -> void
	{
		constexpr auto leftText  = "\tint one = 1;\n\t\tint two = 2;\n\t\t\tint three = 3;\n";
		const auto     rightText = std::string{ "// right-only introduction\n" } + leftText;
		const auto     diff      = computed(leftText, rightText, { }, 45U);
		auto           workbench = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		auto profile        = qt::appearance_profile{ };
		auto codeFont       = profile.typography().Code;
		codeFont.TabWidth   = 4U;
		codeFont.WhiteSpace = core::white_space_drawing::leading;
		expect(profile.select_code_font(codeFont).accepted(), "four-column tabs are accepted");
		workbench.adopt_appearance(profile);
		profile.apply_code_font_to(workbench);
		const auto selected = context(diff, 91U, 91U);
		workbench.set_loading(selected);
		workbench.publish(qt::test::prepare_publication({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		settle_geometry(workbench);
		const auto unstyledPositions = tabbed_correspondence_geometry(workbench, 4);
		// [[BPR-1107]]. Each pane draws its white space as the Appearance preview
		// does, through the same helper: with leading chosen, one line per
		// indenting tab, six a side here, on every paint.
		const auto drawnOnRepaint = [&workbench] {
			const auto before = workbench.statistics().WhiteSpacePainted;
			workbench.left_pane()->viewport()->repaint();
			workbench.right_pane()->viewport()->repaint();
			return workbench.statistics().WhiteSpacePainted - before;
		};
		expect(drawnOnRepaint() == 12U, "each pane draws a line for each of its six leading tabs once per paint");
		const auto drawnWith = [&](const core::white_space_drawing aDrawing) {
			codeFont.WhiteSpace = aDrawing;
			expect(profile.select_code_font(codeFont).accepted(), "every white space choice is accepted");
			workbench.adopt_appearance(profile);
			// The adoption's own update lands first, so the count below is
			// one paint of each pane.
			QApplication::processEvents();
			return drawnOnRepaint();
		};
		expect(drawnWith(core::white_space_drawing::all) == 32U,
			   "all: fifteen tabs and spaces on the left, seventeen on the right with its introduction's two spaces");
		expect(drawnWith(core::white_space_drawing::trailing) == 0U, "trailing: these lines have none");
		expect(drawnWith(core::white_space_drawing::selection) == 2U,
			   "selection: the selected change, the right-only introduction, shows its two spaces and nothing else does");
		expect(drawnWith(core::white_space_drawing::none) == 0U, "none: nothing");
		codeFont.WhiteSpace = core::white_space_drawing::leading;
		expect(profile.select_code_font(codeFont).accepted(), "leading is accepted again");
		workbench.adopt_appearance(profile);
		const auto spans = application::token_span_list{
			{ .Line = 0U, .Column = 1U, .Length = 3U, .Role = core::appearance_role::syntax_builtin },
			{ .Line = 1U, .Column = 2U, .Length = 3U, .Role = core::appearance_role::syntax_builtin },
			{ .Line = 2U, .Column = 3U, .Length = 3U, .Role = core::appearance_role::syntax_builtin },
		};
		auto rightSpans = spans;
		std::ranges::for_each(rightSpans, [](auto& aSpan) { ++aSpan.Line; });
		expect(workbench.adopt_token_spans(core::diff_side::left, std::make_shared<const application::token_span_list>(spans))
				   && workbench.adopt_token_spans(core::diff_side::right, std::make_shared<const application::token_span_list>(rightSpans)),
			   "both tabbed documents adopt their syntax spans");
		process_events();
		const auto verifySources = [&] {
			expect(workbench.left_pane()->toPlainText() == QString::fromUtf8(leftText)
					   && workbench.right_pane()->toPlainText() == QString::fromUtf8(rightText),
				   "styled panes retain exact decoded source text with no synthetic rows or whitespace rewrites");
			for (auto depth = 1; depth <= 3; ++depth) {
				const auto leftFormats  = block_formats(*workbench.left_pane(), depth - 1);
				const auto rightFormats = block_formats(*workbench.right_pane(), depth);
				expect(leftFormats.size() == 1 && rightFormats.size() == 1
						   && leftFormats.front().start == depth && rightFormats.front().start == depth
						   && leftFormats.front().length == 3 && rightFormats.front().length == 3
						   && leftFormats.front().format.foreground().color() == profile.color(core::appearance_role::syntax_builtin)
						   && rightFormats.front().format == leftFormats.front().format,
					   "syntax styles the exact first token after each tab prefix on both sides");
				const auto mapping = core::map_line(*workbench.current_semantic_index(),
													{ .Comparison = diff.Alignment.Digest, .Side = core::diff_side::left, .Line = static_cast<std::uint32_t>(depth - 1) });
				expect(mapping.Kind == core::line_mapping_kind::exact_line && mapping.Counterpart
						   && mapping.Counterpart->Side == core::diff_side::right && mapping.Counterpart->Line == static_cast<std::uint32_t>(depth),
					   "natural source line identities map across the right-only introduction");
			}
		};
		verifySources();
		expect(tabbed_correspondence_geometry(workbench, 4) == unstyledPositions,
			   "syntax formatting does not move authored indentation");
		const auto result = workbench.current_result();
		const auto index  = workbench.current_semantic_index();
		const auto font   = workbench.left_pane()->font();
		codeFont.TabWidth = 8U;
		expect(profile.select_code_font(codeFont).Changed, "eight-column tabs change presentation");
		workbench.adopt_appearance(profile);
		profile.apply_code_font_to(workbench);
		settle_geometry(workbench);
		verifySources();
		const auto widenedPositions = tabbed_correspondence_geometry(workbench, 8);
		expect(widenedPositions[0] > unstyledPositions[0] && widenedPositions[1] > unstyledPositions[1]
				   && widenedPositions[2] > unstyledPositions[2],
			   "a tab-width-only change widens every indentation depth");
		expect(workbench.current_result() == result && workbench.current_semantic_index() == index
				   && workbench.current_context() == selected && workbench.left_pane()->font() == font
				   && *workbench.current_result() == diff,
			   "tab geometry changes preserve the source, mapping index, comparison identity and font");
		codeFont.TabWidth = 2U;
		expect(profile.select_code_font(codeFont).Changed, "two-column tabs change presentation");
		profile.apply_code_font_to(workbench);
		settle_geometry(workbench);
		verifySources();
		const auto narrowedPositions = tabbed_correspondence_geometry(workbench, 2);
		expect(narrowedPositions[0] < unstyledPositions[0]
				   && narrowedPositions[1] < unstyledPositions[1]
				   && narrowedPositions[2] < unstyledPositions[2],
			   "two-column tab stops narrow all three authored indentation depths");
	}

	/// \brief [[BPR-1212]] v002, [[BPR-1533]]. A text selection keeps the
	/// syntax colours.
	///
	/// \details The owner, rejecting right-click-v001: "are we meant to lose
	/// all syntax highlighting when we select something?" Qt draws a text
	/// selection in `HighlightedText`, white over the accent. The pane draws
	/// it in the solid accent with each glyph in its own colour, moved only as
	/// far as it must to read on the accent ([[BPR-1533]]; v002's translucent
	/// wash could hardly be seen). Measured on the rendered pane: the darkest
	/// ink of a selected comment line, against the same line unselected.
	auto test_a_selection_keeps_the_syntax_colours() -> void
	{
		const auto diff      = computed(classified_left, classified_right, { }, 45U);
		auto       workbench = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();
		auto profile = qt::appearance_profile{ };
		static_cast<void>(profile.select_mode(core::appearance_mode::light));
		workbench.adopt_appearance(profile);
		const auto selected = context(diff, 91U, 91U);
		workbench.set_loading(selected);
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		process_events();
		expect(workbench.adopt_token_spans(core::diff_side::left, std::make_shared<const application::token_span_list>(classified_spans())),
			   "the left side adopts its classification");
		process_events();
		auto* const pane = workbench.left_pane();
		const auto  row  = [&workbench, pane](const std::uint32_t aLine) {
			const auto rect = workbench.line_rect(core::diff_side::left, aLine);
			return QRect{ pane->viewport()->mapFrom(&workbench, rect.topLeft()), rect.size() };
		};
		const auto distance = [](const QColor& aLeft, const QColor& aRight) {
			return std::abs(aLeft.red() - aRight.red()) + std::abs(aLeft.green() - aRight.green()) + std::abs(aLeft.blue() - aRight.blue());
		};
		// A row's ground (the commonest colour across its first characters,
		// inside the line's text, past the document margin) and its ink: the pixel there
		// farthest from the ground.
		const auto sample = [&](const QRect aRow) {
			pane->viewport()->repaint();
			const auto image  = pane->viewport()->grab().toImage();
			const auto scale  = image.devicePixelRatio();
			const auto at     = [&image, scale](const int aX, const int aY) { return image.pixelColor(static_cast<int>(aX * scale), static_cast<int>(aY * scale)); };
			auto       counts = std::map<QRgb, int>{ };
			for (auto y = aRow.top() + 1; y < aRow.bottom(); ++y) {
				for (auto x = aRow.left() + 6; x < aRow.left() + 110; ++x) {
					++counts[at(x, y).rgb()];
				}
			}
			const auto ground = QColor{ std::ranges::max_element(counts, { }, &std::map<QRgb, int>::value_type::second)->first };
			auto       ink    = ground;
			for (auto y = aRow.top() + 1; y < aRow.bottom(); ++y) {
				for (auto x = aRow.left() + 6; x < aRow.left() + 110; ++x) {
					const auto pixel = at(x, y);
					if (distance(pixel, ground) > distance(ink, ground)) {
						ink = pixel;
					}
				}
			}
			return std::pair{ ground, ink };
		};
		const auto comment                 = row(1U);
		const auto [plainGround, plainInk] = sample(comment);
		expect(distance(plainInk, profile.color(core::appearance_role::syntax_comment)) < 90, "unselected, the comment line is drawn in the comment colour");

		// Lines 1 to 3 selected, as a drag or Shift+Down selects them.
		auto cursor = QTextCursor{ pane->document() };
		cursor.setPosition(pane->document()->findBlockByNumber(0).position());
		cursor.setPosition(pane->document()->findBlockByNumber(2).position() + 10, QTextCursor::KeepAnchor);
		pane->setTextCursor(cursor);
		process_events();
		const auto [washGround, washInk] = sample(comment);
		const auto highlightedText       = pane->palette().color(QPalette::Active, QPalette::HighlightedText);
		const auto highlight             = qt::code_text_selection_fill(pane->palette().color(QPalette::Active, QPalette::Highlight),
																		pane->palette().color(QPalette::Base), profile.variant());
		expect(distance(washGround, highlight) <= 6 && distance(washGround, plainGround) > 60,
			   "the selection is the solid selection fill, not a wash");
		const auto readable = qt::to_presentation_color(core::readable_selection_ink(
			qt::to_appearance_color(profile.color(core::appearance_role::syntax_comment)), qt::to_appearance_color(highlight), profile.variant()));
		expect(distance(washInk, readable) < 90 && distance(washInk, readable) < distance(washInk, highlightedText),
			   "selected, the comment line keeps its comment colour, read on the selection, not the highlighted-text colour");
		expect(pane->textCursor().selectedText().size() > 0 && pane->textCursor() == cursor, "the selection itself is unchanged, for Copy");
		if (const auto render = qEnvironmentVariable("BPR_QT_DIFF_WORKBENCH_RENDER_PATH"); !render.isEmpty()) {
			expect(workbench.grab().save(render + QStringLiteral("-selection-keeps-syntax.png")), "capture the selection over coloured code");
		}
	}

	/// \brief [[BPR-1533]]. The fixture of the selection courses: an
	/// unchanged line, a removed line, a changed line whose change is one run
	/// of seven characters, and an added line, each in its own region.
	constexpr std::string_view selection_left
		= "int shared_value = 1;\n"
		  "keep_one();\n"
		  "int removed_value = 2;\n"
		  "keep_two();\n"
		  "fetch(alpha_value, beta_value, sixteen);\n"
		  "keep_three();\n"
		  "keep_four();\n";
	constexpr std::string_view selection_right
		= "int shared_value = 1;\n"
		  "keep_one();\n"
		  "keep_two();\n"
		  "fetch(alpha_value, beta_value, zzzzzzz);\n"
		  "keep_three();\n"
		  "int added_value = 3;\n"
		  "keep_four();\n";

	/// \brief [[BPR-1533]]. One rendered selection: the fill under the
	/// selected characters and the ground beside them on the same line.
	struct selection_sample
	{
		QColor Fill;
		QColor Ground;
	};

	/// \brief The commonest colour in a viewport rectangle, in device pixels.
	[[nodiscard]] auto commonest(const QImage& aImage, const QRect& aLogical) -> QColor
	{
		const auto scale  = aImage.devicePixelRatio();
		auto       counts = std::map<QRgb, int>{ };
		for (auto y = static_cast<int>(aLogical.top() * scale); y <= static_cast<int>(aLogical.bottom() * scale); ++y) {
			for (auto x = static_cast<int>(aLogical.left() * scale); x <= static_cast<int>(aLogical.right() * scale); ++x) {
				if (x >= 0 && y >= 0 && x < aImage.width() && y < aImage.height()) {
					++counts[aImage.pixel(x, y)];
				}
			}
		}
		return counts.empty() ? QColor{ } : QColor{ std::ranges::max_element(counts, { }, &std::map<QRgb, int>::value_type::second)->first };
	}

	/// \brief Selects characters `aFrom` to `aTo` of one block of `aPane`, as
	/// a double click selects a word.
	void select_characters(QPlainTextEdit& aPane, const int aBlock, const int aFrom, const int aTo)
	{
		const auto block  = aPane.document()->findBlockByNumber(aBlock);
		auto       cursor = QTextCursor{ block };
		cursor.setPosition(block.position() + aFrom);
		cursor.setPosition(block.position() + aTo, QTextCursor::KeepAnchor);
		aPane.setTextCursor(cursor);
		process_events();
	}

	/// \brief Samples, on the rendered pane, the fill under characters `aFrom`
	/// to `aTo` of one block and the ground beside them (characters
	/// `aGroundFrom` to `aGroundTo`), each the commonest colour there.
	[[nodiscard]] auto measure_selection(QPlainTextEdit& aPane, const int aBlock, const int aFrom, const int aTo, const int aGroundFrom,
										 const int aGroundTo) -> selection_sample
	{
		const auto block = aPane.document()->findBlockByNumber(aBlock);
		auto       at    = [&aPane, &block](const int aCharacter) {
			auto cursor = QTextCursor{ block };
			cursor.setPosition(block.position() + aCharacter);
			return aPane.cursorRect(cursor);
		};
		aPane.viewport()->repaint();
		const auto image = aPane.viewport()->grab().toImage();
		const auto line  = at(aFrom);
		const auto fill  = QRect{ QPoint{ at(aFrom).left() + 2, line.top() + 1 }, QPoint{ at(aTo).left() - 2, line.bottom() - 1 } };
		const auto beside
			= QRect{ QPoint{ at(aGroundFrom).left() + 1, line.top() + 1 }, QPoint{ at(aGroundTo).left() - 1, line.bottom() - 1 } };
		return { .Fill = commonest(image, fill), .Ground = commonest(image, beside) };
	}

	[[nodiscard]] auto luminance_ratio(const QColor& aFirst, const QColor& aSecond) -> double
	{
		return static_cast<double>(core::contrast_ratio_hundredths(qt::to_appearance_color(aFirst), qt::to_appearance_color(aSecond))) / 100.0;
	}

	[[nodiscard]] auto colour_distance(const QColor& aFirst, const QColor& aSecond) -> double
	{
		const auto red   = aFirst.red() - aSecond.red();
		const auto green = aFirst.green() - aSecond.green();
		const auto blue  = aFirst.blue() - aSecond.blue();
		return std::sqrt(static_cast<double>((red * red) + (green * green) + (blue * blue)));
	}

	/// \brief [[BPR-1534]]. Line details keeps the syntax colours under a
	/// text selection.
	///
	/// \details The owner, 2026-10-15: "the line details [loses] syntax
	/// highlights on selecting". Qt repainted every selected glyph in
	/// `HighlightedText`. Line details now draws the comparison panes'
	/// selection: the solid theme selection, each glyph in its own colour made
	/// readable on it. In each built-in variant a selected `int` is drawn in
	/// the type colour's selection ink and not in `HighlightedText`, the fill
	/// is the selection colour, and nothing outside the selection changes.
	auto test_line_details_keeps_the_syntax_colours_under_a_selection() -> void
	{
		const auto diff  = computed(selection_left, selection_right, { }, 49U);
		using role       = core::appearance_role;
		const auto spans = application::token_span_list{
			{ .Line = 0U, .Column = 0U, .Length = 3U, .Role = role::syntax_type },
			{ .Line = 5U, .Column = 0U, .Length = 3U, .Role = role::syntax_type },
			{ .Line = 5U, .Column = 18U, .Length = 1U, .Role = role::syntax_number },
		};
		const auto modes = std::array{
			std::pair{ "light", core::appearance_mode::light },
			std::pair{ "dark", core::appearance_mode::dark },
			std::pair{ "high-contrast-light", core::appearance_mode::high_contrast_light },
			std::pair{ "high-contrast-dark", core::appearance_mode::high_contrast_dark },
		};
		const auto render = qEnvironmentVariable("BPR_QT_DIFF_WORKBENCH_RENDER_PATH");
		for (const auto& [modeName, mode] : modes) {
			const auto name      = std::string{ modeName };
			auto       workbench = qt::diff_workbench{ };
			workbench.resize(1'100, 760);
			workbench.show();
			process_events();
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			profile.apply_to(workbench);
			workbench.adopt_appearance(profile);
			const auto selected = context(diff, 96U, 96U);
			workbench.set_loading(selected);
			workbench.publish(bpr::qt::test::prepare_publication({
				.Context = selected,
				.Result  = std::make_shared<const core::computed_file_diff>(diff),
			}));
			settle_geometry(workbench);
			expect(workbench.adopt_token_spans(core::diff_side::right, std::make_shared<const application::token_span_list>(spans)),
				   name + ": the head side adopts its classification");
			expect(workbench.select_conversation_line(core::diff_side::right, 5U), name + ": the added line selects");
			process_events();
			auto* const strip = workbench.line_details();
			auto        row   = -1;
			for (auto block = strip->document()->begin(); block.isValid(); block = block.next()) {
				if (block.text() == QStringLiteral("int added_value = 3;")) {
					row = block.blockNumber();
				}
			}
			expect(row >= 0, name + ": Line details shows the added line");
			// The caret where the selection will end, so the two paints differ
			// only by the selection.
			auto caretOnly = QTextCursor{ strip->document()->findBlockByNumber(row) };
			caretOnly.setPosition(caretOnly.block().position() + 18);
			strip->setTextCursor(caretOnly);
			process_events();
			strip->viewport()->repaint();
			const auto before = strip->viewport()->grab().toImage();
			// "int added_value = " selected; "3;" is not.
			select_characters(*strip, row, 0, 18);
			const auto sample = measure_selection(*strip, row, 0, 18, 18, 20);
			const auto after  = strip->viewport()->grab().toImage();
			if (!render.isEmpty()) {
				expect(workbench.grab().save(render + QStringLiteral("-bpr1534-%1.png").arg(QLatin1StringView{ modeName })), name + ": capture Line details");
			}
			const auto fill = qt::code_text_selection_fill(profile.color(role::code_selection_background), profile.color(role::code_background), profile.variant());
			expect(colour_distance(sample.Fill, fill) <= 4.0, name + ": the selection is the theme's selection fill");
			std::cout << "BPR-1534 " << name << ": fill " << sample.Fill.name().toStdString() << " ground " << sample.Ground.name().toStdString()
					  << " ratio " << luminance_ratio(sample.Fill, sample.Ground) << " distance " << colour_distance(sample.Fill, sample.Ground) << '\n';

			// The selected `int`: its ink is the pixel farthest from the fill.
			const auto block = strip->document()->findBlockByNumber(row);
			auto       caret = [strip, &block](const int aCharacter) {
				auto cursor = QTextCursor{ block };
				cursor.setPosition(block.position() + aCharacter);
				return strip->cursorRect(cursor);
			};
			const auto scale = after.devicePixelRatio();
			auto       ink   = sample.Fill;
			for (auto y = static_cast<int>((caret(0).top() + 1) * scale); y < static_cast<int>((caret(0).bottom() - 1) * scale); ++y) {
				for (auto x = static_cast<int>(caret(0).left() * scale); x < static_cast<int>(caret(3).left() * scale); ++x) {
					if (colour_distance(after.pixelColor(x, y), sample.Fill) > colour_distance(ink, sample.Fill)) {
						ink = after.pixelColor(x, y);
					}
				}
			}
			const auto type     = profile.color(role::syntax_type);
			const auto readable = qt::to_presentation_color(
				core::readable_selection_ink(qt::to_appearance_color(type), qt::to_appearance_color(fill), profile.variant()));
			const auto highlightedText = strip->palette().color(QPalette::Active, QPalette::HighlightedText);
			std::cout << "BPR-1534 " << name << ": type " << type.name().toStdString() << " ink " << ink.name().toStdString() << " expected "
					  << readable.name().toStdString() << " highlighted text " << highlightedText.name().toStdString() << " ink ratio "
					  << luminance_ratio(readable, fill) << '\n';
			expect(colour_distance(ink, readable) < 90.0 && colour_distance(ink, readable) < colour_distance(ink, highlightedText),
				   name + ": the selected type keeps its colour, read on the selection, not the highlighted-text colour");
			expect(core::contrast_ratio_hundredths(qt::to_appearance_color(readable), qt::to_appearance_color(fill))
					   >= core::minimum_contrast_hundredths(core::contrast_kind::text, profile.variant()),
				   name + ": the selected type's ink reaches the variant's text floor on the selection");

			// Outside the selected characters the strip is painted as Qt
			// painted it: the bands, the other row, the unselected "3;".
			const auto selectedRect = QRect{ QPoint{ caret(0).left() - 1, caret(0).top() }, QPoint{ caret(18).left() + 1, caret(0).bottom() } };
			auto       differing    = 0;
			for (auto y = 0; y < after.height(); ++y) {
				for (auto x = 0; x < after.width(); ++x) {
					if (after.pixel(x, y) != before.pixel(x, y)
						&& !selectedRect.contains(QPoint{ static_cast<int>(x / scale), static_cast<int>(y / scale) })) {
						++differing;
					}
				}
			}
			expect(differing == 0, name + ": nothing outside the selection changes");
			expect(strip->textCursor().selectedText() == QStringLiteral("int added_value = "), name + ": the selection itself is unchanged, for Copy");
		}
	}

	/// \brief [[BPR-1533]], [[BPR-1534]]. The selection fixture's
	/// classification, so the looks show syntax colours under a selection.
	[[nodiscard]] auto selection_spans(const core::diff_side aSide) -> application::token_span_list
	{
		using role = core::appearance_role;
		if (aSide == core::diff_side::left) {
			return {
				{ .Line = 0U, .Column = 0U, .Length = 3U, .Role = role::syntax_type },
				{ .Line = 0U, .Column = 19U, .Length = 1U, .Role = role::syntax_number },
				{ .Line = 1U, .Column = 0U, .Length = 8U, .Role = role::syntax_function },
				{ .Line = 2U, .Column = 0U, .Length = 3U, .Role = role::syntax_type },
				{ .Line = 2U, .Column = 20U, .Length = 1U, .Role = role::syntax_number },
				{ .Line = 3U, .Column = 0U, .Length = 8U, .Role = role::syntax_function },
				{ .Line = 4U, .Column = 0U, .Length = 5U, .Role = role::syntax_function },
				{ .Line = 5U, .Column = 0U, .Length = 10U, .Role = role::syntax_function },
				{ .Line = 6U, .Column = 0U, .Length = 9U, .Role = role::syntax_function },
			};
		}
		return {
			{ .Line = 0U, .Column = 0U, .Length = 3U, .Role = role::syntax_type },
			{ .Line = 0U, .Column = 19U, .Length = 1U, .Role = role::syntax_number },
			{ .Line = 1U, .Column = 0U, .Length = 8U, .Role = role::syntax_function },
			{ .Line = 2U, .Column = 0U, .Length = 8U, .Role = role::syntax_function },
			{ .Line = 3U, .Column = 0U, .Length = 5U, .Role = role::syntax_function },
			{ .Line = 4U, .Column = 0U, .Length = 10U, .Role = role::syntax_function },
			{ .Line = 5U, .Column = 0U, .Length = 3U, .Role = role::syntax_type },
			{ .Line = 5U, .Column = 18U, .Length = 1U, .Role = role::syntax_number },
			{ .Line = 6U, .Column = 0U, .Length = 9U, .Role = role::syntax_function },
		};
	}

	/// \brief [[BPR-1533]], [[BPR-1534]]. Renders the attended looks: a word
	/// double-clicked on an unchanged, a removed, an added and a changed line
	/// (each then the selected line), and one selected on an unchanged line
	/// that is not the selected line, with Line details showing the selected
	/// line under the comparison and the same word selected there; and an
	/// added line triple-clicked whole, so its coloured tokens are selected.
	/// No checks: the same harness renders the baseline for the before images.
	void render_selection_looks(const QString& aDirectory)
	{
		const auto diff = computed(selection_left, selection_right, { }, 50U);
		struct look
		{
			const char*     Name;
			core::diff_side Side;
			std::uint32_t   Line;
			int             Character;
			bool            DoubleClick;
		};
		const auto looks = std::array{
			look{ "unchanged-current", core::diff_side::right, 0U, 8, true },
			look{ "removed", core::diff_side::left, 2U, 8, true },
			look{ "added", core::diff_side::right, 5U, 8, true },
			look{ "changed", core::diff_side::right, 3U, 34, true },
			look{ "unchanged-not-current", core::diff_side::right, 0U, 8, false },
			look{ "added-whole-line", core::diff_side::right, 5U, 8, true },
		};
		const auto modes = std::array{
			std::pair{ "light", core::appearance_mode::light },
			std::pair{ "dark", core::appearance_mode::dark },
			std::pair{ "high-contrast-light", core::appearance_mode::high_contrast_light },
			std::pair{ "high-contrast-dark", core::appearance_mode::high_contrast_dark },
		};
		for (const auto& [modeName, mode] : modes) {
			for (const auto& each : looks) {
				auto workbench = qt::diff_workbench{ };
				workbench.resize(760, 420);
				workbench.show();
				process_events();
				auto profile = qt::appearance_profile{ };
				static_cast<void>(profile.select_mode(mode));
				profile.apply_to(workbench);
				workbench.adopt_appearance(profile);
				const auto selected = context(diff, 97U, 97U);
				workbench.set_loading(selected);
				workbench.publish(bpr::qt::test::prepare_publication({
					.Context = selected,
					.Result  = std::make_shared<const core::computed_file_diff>(diff),
				}));
				settle_geometry(workbench);
				for (const auto side : { core::diff_side::left, core::diff_side::right }) {
					static_cast<void>(workbench.adopt_token_spans(side, std::make_shared<const application::token_span_list>(selection_spans(side))));
				}
				process_events();
				auto* const pane  = each.Side == core::diff_side::left ? workbench.left_pane() : workbench.right_pane();
				const auto  block = pane->document()->findBlockByNumber(static_cast<int>(each.Line));
				auto        at    = QTextCursor{ block };
				at.setPosition(block.position() + each.Character);
				pane->setFocus();
				if (each.DoubleClick) {
					const auto point = QPointF{ pane->cursorRect(at).center() } + QPointF{ 3.0, 0.0 };
					send_click(*pane->viewport(), point);
					auto doubled = QMouseEvent{ QEvent::MouseButtonDblClick, point, point, point, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
					auto release = QMouseEvent{ QEvent::MouseButtonRelease, point, point, point, Qt::LeftButton, Qt::NoButton, Qt::NoModifier };
					QCoreApplication::sendEvent(pane->viewport(), &doubled);
					QCoreApplication::sendEvent(pane->viewport(), &release);
					// A third click selects the whole line, its coloured tokens
					// with it ([[BPR-1534]]'s look).
					if (std::string_view{ each.Name } == "added-whole-line") {
						send_click(*pane->viewport(), point);
					}
				} else {
					static_cast<void>(workbench.select_conversation_line(core::diff_side::right, 5U));
					process_events();
					at.movePosition(QTextCursor::StartOfWord);
					const auto start = at.position() - block.position();
					at.movePosition(QTextCursor::EndOfWord);
					select_characters(*pane, static_cast<int>(each.Line), start, at.position() - block.position());
				}
				process_events();
				// The same word selected in Line details, where its row shows.
				const auto word  = pane->textCursor().selectedText();
				auto*      strip = workbench.line_details();
				for (auto row = strip->document()->begin(); row.isValid() && !word.isEmpty(); row = row.next()) {
					if (row.text() == block.text()) {
						auto inStrip = QTextCursor{ row };
						inStrip.setPosition(row.position() + pane->textCursor().selectionStart() - block.position());
						inStrip.setPosition(row.position() + std::min(pane->textCursor().selectionEnd() - block.position(), row.length() - 1),
											QTextCursor::KeepAnchor);
						strip->setTextCursor(inStrip);
					}
				}
				process_events();
				pane->viewport()->repaint();
				strip->viewport()->repaint();
				const auto path = QStringLiteral("%1/%2-%3.png").arg(aDirectory, QLatin1StringView{ modeName }, QLatin1StringView{ each.Name });
				expect(workbench.grab().save(path), "save " + path.toStdString());
				// The fill under the word and the ground just after it, in the
				// pane and, where its row shows, in Line details; a selection
				// running to the line's end has no ground beside it.
				const auto from = pane->textCursor().selectionStart() - block.position();
				const auto to   = pane->textCursor().selectionEnd() - block.position();
				std::cout << "RENDERED " << path.toStdString() << " selected '" << word.toStdString() << "'";
				if (to + 2 <= block.length() - 1) {
					const auto measured = measure_selection(*pane, static_cast<int>(each.Line), from, to, to, to + 2);
					std::cout << " pane fill " << measured.Fill.name().toStdString() << " ground " << measured.Ground.name().toStdString() << " ratio "
							  << luminance_ratio(measured.Fill, measured.Ground) << " distance " << colour_distance(measured.Fill, measured.Ground);
					if (strip->textCursor().hasSelection()) {
						const auto row     = strip->textCursor().block();
						const auto details = measure_selection(*strip, row.blockNumber(), from, to, to, to + 2);
						std::cout << "; Line details fill " << details.Fill.name().toStdString() << " ground " << details.Ground.name().toStdString()
								  << " ratio " << luminance_ratio(details.Fill, details.Ground) << " distance " << colour_distance(details.Fill, details.Ground);
					}
				}
				std::cout << '\n';
			}
		}
	}

	/// \brief [[BPR-1533]]. A text selection reads on every line kind.
	///
	/// \details The owner, 2026-10-15: "quite hard to see a selected block (eg
	/// a word double clicked) in the middle of a change diff on the main pane
	/// [...] not so easy to see even on unchanged lines. The single line
	/// details pane shows it much more clearly". A word is selected on an
	/// unchanged, a removed, an added and a changed line, each the selected
	/// line (a double click selects its line first), in the four built-in
	/// variants, with the pane focused and not. The fill under the word must
	/// be the theme's selection fill, the one Line details paints (in Light
	/// a lighter mix of the selection colour, [[BPR-1533]] v002), and stand
	/// apart from the ground beside it: [[BPR-1212]] v002's wash measured 22
	/// to 37 sRGB units from it, and none at all inside a change, where the
	/// intraline emphasis covered it.
	auto test_a_text_selection_reads_on_every_line_kind() -> void
	{
		const auto diff  = computed(selection_left, selection_right, { }, 48U);
		auto       run   = std::optional<core::byte_span>{ };
		const auto pairs = std::span{ diff.Alignment.Pairs };
		for (const auto& edit : diff.Intraline) {
			const auto pair = std::ranges::find_if(pairs, [&edit](const core::line_pair& aPair) { return aPair.Id == edit.Pair; });
			if (pair != pairs.end() && pair->RightLine == 3U && edit.Right.End > edit.Right.Begin + 4U) {
				run = edit.Right;
			}
		}
		expect(run.has_value(), "the changed line carries an intraline run of at least five characters");
		const auto runBegin = static_cast<int>(run->Begin);
		const auto runEnd   = static_cast<int>(run->End);
		struct look
		{
			const char*     Name;
			core::diff_side Side;
			std::uint32_t   Line;
			int             From;
			int             To;
			int             GroundFrom;
			int             GroundTo;
		};
		const auto looks = std::array{
			look{ "unchanged", core::diff_side::right, 0U, 4, 16, 17, 21 },
			look{ "removed", core::diff_side::left, 2U, 4, 17, 18, 22 },
			look{ "added", core::diff_side::right, 5U, 4, 15, 16, 20 },
			look{ "changed", core::diff_side::right, 3U, runBegin + 2, runEnd - 2, runBegin, runBegin + 2 },
		};
		const auto modes = std::array{
			std::pair{ "light", core::appearance_mode::light },
			std::pair{ "dark", core::appearance_mode::dark },
			std::pair{ "high-contrast-light", core::appearance_mode::high_contrast_light },
			std::pair{ "high-contrast-dark", core::appearance_mode::high_contrast_dark },
		};
		const auto render = qEnvironmentVariable("BPR_QT_DIFF_WORKBENCH_RENDER_PATH");
		for (const auto& [modeName, mode] : modes) {
			auto workbench = qt::diff_workbench{ };
			workbench.resize(1'100, 760);
			workbench.show();
			process_events();
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			profile.apply_to(workbench);
			workbench.adopt_appearance(profile);
			const auto selected = context(diff, 95U, 95U);
			workbench.set_loading(selected);
			workbench.publish(bpr::qt::test::prepare_publication({
				.Context = selected,
				.Result  = std::make_shared<const core::computed_file_diff>(diff),
			}));
			settle_geometry(workbench);
			// [[BPR-1533]] v002: the theme's selection, mixed toward the canvas
			// in Light only ("slightly overdone on the light").
			const auto full      = profile.color(core::appearance_role::code_selection_background);
			const auto selection = qt::code_text_selection_fill(full, profile.color(core::appearance_role::code_background), profile.variant());
			if (mode == core::appearance_mode::light) {
				expect(luminance_ratio(selection, full) >= 1.2 && colour_distance(selection, full) >= 30.0,
					   std::string{ modeName } + ": the light selection is a lighter mix of the theme's selection colour");
			} else {
				expect(selection == full, std::string{ modeName } + ": the selection is the theme's selection colour at full strength");
			}
			for (const auto& each : looks) {
				const auto name = std::string{ modeName } + " " + each.Name;
				expect(workbench.select_conversation_line(each.Side, each.Line), name + ": the line selects");
				process_events();
				auto* const pane  = each.Side == core::diff_side::left ? workbench.left_pane() : workbench.right_pane();
				auto* const other = each.Side == core::diff_side::left ? workbench.right_pane() : workbench.left_pane();
				pane->setFocus();
				process_events();
				select_characters(*pane, static_cast<int>(each.Line), each.From, each.To);
				const auto focused = measure_selection(*pane, static_cast<int>(each.Line), each.From, each.To, each.GroundFrom, each.GroundTo);
				if (!render.isEmpty()) {
					expect(workbench.grab().save(render + QStringLiteral("-bpr1533-%1-%2.png").arg(QLatin1StringView{ modeName }, QLatin1StringView{ each.Name })),
						   name + ": capture the selection");
				}
				other->setFocus();
				process_events();
				const auto unfocused = measure_selection(*pane, static_cast<int>(each.Line), each.From, each.To, each.GroundFrom, each.GroundTo);
				std::cout << "BPR-1533 " << name << ": fill " << focused.Fill.name().toStdString() << " ground "
						  << focused.Ground.name().toStdString() << " ratio " << luminance_ratio(focused.Fill, focused.Ground)
						  << " distance " << colour_distance(focused.Fill, focused.Ground) << "; unfocused fill "
						  << unfocused.Fill.name().toStdString() << '\n';
				expect(colour_distance(focused.Fill, selection) <= 4.0, name + ": the selection is the theme's selection fill");
				expect(colour_distance(unfocused.Fill, selection) <= 4.0, name + ": unfocused, the selection keeps the same fill");
				expect(colour_distance(focused.Fill, focused.Ground) >= 40.0,
					   name + ": the selection stands clearly apart from the line it sits in");
			}
		}
	}

	/// \brief A classified blob must reach the pane's rendered formats.
	auto test_syntax_spans_colour_the_panes() -> void
	{
		const auto diff      = computed(classified_left, classified_right, { }, 44U);
		auto       workbench = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();

		auto profile = qt::appearance_profile{ };
		static_cast<void>(profile.select_mode(core::appearance_mode::light));
		workbench.adopt_appearance(profile);
		const auto selected = context(diff, 90U, 90U);
		workbench.set_loading(selected);
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		process_events();

		expect(
			block_formats(*workbench.left_pane(), 1).isEmpty(),
			"a pane that was handed no classification paints no token colour, "
			"rather than inventing one");

		const auto adopted = workbench.adopt_token_spans(
			core::diff_side::left,
			std::make_shared<const application::token_span_list>(
				classified_spans()));
		expect(adopted, "an ordered classification is adopted");
		process_events();

		const auto comment      = profile.color(core::appearance_role::syntax_comment);
		const auto continuation = block_formats(*workbench.left_pane(), 1);
		expect(
			continuation.size() == 1 && continuation.front().start == 0
				&& continuation.front().length == 22
				&& continuation.front().format.foreground().color() == comment,
			"a line the complete-blob pass classified as comment is painted as "
			"one, though nothing in the line itself says so");

		const auto stringColor = profile.color(core::appearance_role::syntax_string);
		const auto literalLine = block_formats(*workbench.left_pane(), 4);
		expect(
			literalLine.size() == 2,
			"both spans of the literal line are painted");
		expect(
			literalLine.at(1).start == 20 && literalLine.at(1).length == 6
				&& literalLine.at(1).format.foreground().color() == stringColor,
			"a span behind a two-byte character starts at its character index, "
			"not at its byte offset");

		expect(
			block_formats(*workbench.left_pane(), 3).size() == 1
				&& block_formats(*workbench.right_pane(), 3).isEmpty(),
			"a classification adopted for one side colours that side alone");

		const auto visibleLinks = workbench.link_map()->visible_links();
		expect(!visibleLinks.empty(),
			   "the selected comparison exposes a semantic group to retain");
		send_click(
			*workbench.link_map(), visibleLinks.front().Path.boundingRect().center());
		process_events();
		const auto resultBeforePreview    = workbench.current_result();
		const auto contextBeforePreview   = workbench.current_context();
		const auto selectionBeforePreview = workbench.selection();
		const auto positionBeforePreview  = workbench.scroll_controller()->position();
		const auto spansBeforePreview
			= workbench.token_spans(core::diff_side::left);
		const auto statisticsBeforePreview = workbench.statistics();
		const auto profileGeneration       = profile.generation();
		auto       palettePreview          = profile.palette();
		palettePreview[core::index_of(core::appearance_role::syntax_comment)]
			= core::appearance_color{ 1U, 2U, 3U, 255U };
		expect(profile.preview_palette(palettePreview),
			   "a colour-only preview advances presentation");
		workbench.adopt_appearance(profile);
		process_events();
		const auto statisticsAfterPreview = workbench.statistics();
		expect(
			block_formats(*workbench.left_pane(), 1).front().format.foreground().color()
				== QColor{ 1, 2, 3 },
			"the selected comparison repaints from the preview palette immediately");
		expect(profile.generation() == profileGeneration + 1U
				   && statisticsAfterPreview.SyntaxBlocksFormatted
					   > statisticsBeforePreview.SyntaxBlocksFormatted,
			   "only the presentation generation and paint work advance");
		expect(workbench.current_result() == resultBeforePreview
				   && workbench.current_context() == contextBeforePreview
				   && workbench.selection() == selectionBeforePreview
				   && workbench.scroll_controller()->position()
					   == positionBeforePreview,
			   "colour preview preserves selected diff identity, semantic selection, "
			   "and comparison position");
		expect(workbench.token_spans(core::diff_side::left) == spansBeforePreview
				   && statisticsAfterPreview.TokenSpanAdoptions
					   == statisticsBeforePreview.TokenSpanAdoptions
				   && statisticsAfterPreview.SemanticIndexAdoptions
					   == statisticsBeforePreview.SemanticIndexAdoptions
				   && statisticsAfterPreview.SelectionChanges
					   == statisticsBeforePreview.SelectionChanges
				   && statisticsAfterPreview.GeometryRebuilds
					   == statisticsBeforePreview.GeometryRebuilds,
			   "colour preview preserves span identity and rebuilds neither semantics "
			   "nor measured geometry");

		// The whole point of keying spans by blob and definition: appearance is
		// not part of the key, so a theme change repaints and never reparses.
		const auto beforeTheme = workbench.statistics();
		auto       dark        = qt::appearance_profile{ };
		static_cast<void>(dark.select_mode(core::appearance_mode::dark));
		workbench.adopt_appearance(dark);
		process_events();
		const auto darkComment = dark.color(core::appearance_role::syntax_comment);
		const auto repainted   = block_formats(*workbench.left_pane(), 1);
		expect(
			darkComment != comment && repainted.size() == 1
				&& repainted.front().format.foreground().color() == darkComment,
			"switching theme repaints the retained classification in the new "
			"palette");
		expect(
			workbench.statistics().TokenSpanAdoptions
				== beforeTheme.TokenSpanAdoptions,
			"switching theme adopts no new classification, so nothing "
			"downstream of the pane had to reparse");

		auto unordered = classified_spans();
		std::ranges::reverse(unordered);
		const auto refused = workbench.adopt_token_spans(
			core::diff_side::left,
			std::make_shared<const application::token_span_list>(unordered));
		expect(
			!refused
				&& workbench.statistics().RejectedTokenSpans
					== beforeTheme.RejectedTokenSpans + 1U,
			"a classification that is not ordered by line is refused whole");
		process_events();
		expect(
			block_formats(*workbench.left_pane(), 1).size() == 1,
			"a refused classification leaves the adopted one painting");

		// A classification belongs to one blob, so the next comparison must not
		// inherit it.
		workbench.clear();
		process_events();
		expect(
			!workbench.token_spans(core::diff_side::left),
			"clearing the projection drops the classification with it");
	}

	/// \brief Token colouring must not grow with the file it is colouring.
	auto test_syntax_formatting_stays_bounded_by_the_viewport() -> void
	{
		using role = core::appearance_role;
		struct measurement
		{
			std::uint64_t Blocks   = 0U;
			std::uint64_t Spans    = 0U;
			std::uint64_t Segments = 0U;
		};
		const auto measure
			= [](const std::size_t aLines) -> measurement {
			const auto text      = large_comparison_text(aLines, 8U);
			const auto diff      = computed(text.first, text.second, { }, 45U);
			auto       workbench = qt::diff_workbench{ };
			workbench.resize(1'200, 760);
			workbench.show();
			process_events();
			workbench.adopt_appearance(qt::appearance_profile{ });
			const auto selected = context(diff, 91U, 91U);
			workbench.set_loading(selected);
			workbench.publish(bpr::qt::test::prepare_publication({
				.Context = selected,
				.Result  = std::make_shared<const core::computed_file_diff>(diff),
			}));
			process_events();

			// One span on every line, which is the worst case a classifier can
			// hand over: if the pane walked the list rather than searching it,
			// this cost would scale with the file.
			auto spans = application::token_span_list{ };
			spans.reserve(aLines);
			for (auto line = std::size_t{ 0U }; line < aLines; ++line) {
				spans.push_back({
					.Line   = static_cast<std::uint32_t>(line),
					.Column = 0U,
					.Length = 7U,
					.Role   = role::syntax_keyword,
				});
			}
			expect(
				workbench.adopt_token_spans(
					core::diff_side::left,
					std::make_shared<const application::token_span_list>(
						std::move(spans))),
				"the per-line classification is adopted");
			process_events();

			// Measured at the far end of the file rather than at the top. At
			// line zero the visible window begins where the span list begins,
			// so a painter that scanned from the front and one that searched
			// would be indistinguishable — which is exactly how a scan-shaped
			// cost hides from a measurement taken on arrival.
			// [[BPR-1291]]: the bar's maximum shows only the last line, so step
			// back a page to have a full window of the file's end in view.
			settle_geometry(workbench);
			auto* const bar = workbench.shared_scrollbar();
			bar->setValue(bar->maximum());
			workbench.scroll_controller()->page_up();
			process_events();

			// One repaint, forced by a palette the panes do not already carry.
			// A second scroll would be a weaker instrument: the window barely
			// moves, and blocks whose formats are unchanged are deliberately
			// left alone.
			const auto before = workbench.statistics();
			auto       other  = qt::appearance_profile{ };
			static_cast<void>(other.select_mode(core::appearance_mode::dark));
			workbench.adopt_appearance(other);
			process_events();
			const auto after = workbench.statistics();
			return measurement{
				.Blocks = after.SyntaxBlocksFormatted - before.SyntaxBlocksFormatted,
				.Spans  = after.SyntaxSpansVisited - before.SyntaxSpansVisited,
				.Segments
				= after.AlignmentSegmentsVisited - before.AlignmentSegmentsVisited,
			};
		};

		const auto small = measure(200U);
		const auto large = measure(2'000U);
		expect(
			small.Blocks > 0U && small.Spans > 0U && small.Segments > 0U,
			"the bounds below are not satisfied by doing nothing");
		expect(
			large.Blocks <= 4U * small.Blocks,
			"a file ten times longer formats no more than a constant factor "
			"more blocks");
		// The block count alone would be satisfied by a painter that walked
		// every span in the file to find the visible ones. This is the half of
		// the bound that a block count cannot see.
		expect(
			large.Spans <= 4U * small.Spans,
			"a file ten times longer visits no more than a constant factor "
			"more classified spans");
		// And the same again for the alignment. BPR-0196 bounded the number of
		// bands it emitted and left the scan that chose them proportional to
		// the file; the selection count could not see the difference, and
		// neither could the two assertions above.
		expect(
			large.Segments <= 4U * small.Segments,
			"a file ten times longer visits no more than a constant factor "
			"more alignment segments");
	}
	/// \brief Moved code must not read as an unrelated delete plus add.
	auto test_moved_blocks_carry_the_moved_role() -> void
	{
		auto moveOptions         = core::diff_options{ };
		moveOptions.Moves        = core::move_detection_policy::deterministic_exact;
		const auto [left, right] = dense_move_text();
		const auto diff          = computed(left, right, moveOptions, 46U);
		expect(
			!diff.Alignment.Moves.empty(),
			"the fixture really does produce move relations, so the assertions "
			"below are about detected moves and not about their absence");

		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();
		auto profile = qt::appearance_profile{ };
		workbench.adopt_appearance(profile);
		const auto selected = context(diff, 92U, 92U);
		workbench.set_loading(selected);
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		process_events();

		expect(
			workbench.statistics().MovedBandsPainted > 0U,
			"a comparison the core reports moves for paints moved bands");

		// Read off the pane, and compared against the model's own answer: the
		// left side of a move is a deletion segment, so a surface that ignored
		// `Moves` would paint it with the removed colour and still look
		// plausible.
		const auto movedColor
			= profile.color(core::appearance_role::diff_moved_background);
		const auto removedColor
			= profile.color(core::appearance_role::diff_removed_background);
		expect(
			movedColor != removedColor,
			"the palette distinguishes the two, so the assertion below can");

		const auto& source = segment_by_id(diff, diff.Alignment.Moves.front().SourceDeletion);
		expect(
			source.Kind == core::alignment_kind::left_only,
			"the move's source really is a deletion, which is what would be "
			"painted as an ordinary removal by a surface that read no moves");
		expect(
			band_color(*workbench.left_pane(), static_cast<int>(source.Left.Begin))
				== movedColor,
			"the source of a move is painted as moved rather than as removed");

		const auto& destination
			= segment_by_id(diff, diff.Alignment.Moves.front().DestinationInsertion);
		expect(
			band_color(
				*workbench.right_pane(),
				static_cast<int>(destination.Right.Begin))
				== movedColor,
			"the destination of a move is painted as moved rather than as added");

		// An unrelated deletion must keep the removed colour, or "moved" would
		// mean nothing.
		const auto& plain = segment_of_kind(diff, core::alignment_kind::equal);
		expect(
			!band_color(*workbench.left_pane(), static_cast<int>(plain.Left.Begin))
				 .isValid(),
			"an unchanged line stays untinted, so the moved colour is a mark "
			"and not a wash");
	}

	/// \brief The fill a selected row's mark shows.
	///
	/// \details BPR-0323: on a selected row the mark is a deeper shade of the
	/// band the row is wearing — its own band with the wash over it — rather
	/// than any palette colour. Compositing the palette colour instead was
	/// BPR-0321, and the product owner rejected it: at 72/255 a pink stays
	/// pink on a blue band.
	[[nodiscard]] auto selected_row_fill(
		const QPlainTextEdit&         aPane,
		const qt::appearance_profile& aProfile,
		const QColor                  aBand) -> QColor
	{
		const auto highlight = aPane.palette().color(QPalette::Highlight);
		const auto washed    = core::composited(
			core::appearance_color{ static_cast<std::uint8_t>(highlight.red()),
									static_cast<std::uint8_t>(highlight.green()),
									static_cast<std::uint8_t>(highlight.blue()),
									255U },
			core::appearance_color{ static_cast<std::uint8_t>(aBand.red()),
									static_cast<std::uint8_t>(aBand.green()),
									static_cast<std::uint8_t>(aBand.blue()),
									255U },
			core::selection_wash_alpha);
		const auto mark = core::derived_emphasis_fill(washed, aProfile.variant());
		return QColor{ mark.Red, mark.Green, mark.Blue };
	}

	/// \brief Painted intraline ranges must be the ranges the core computed.
	auto test_intraline_ranges_match_the_model() -> void
	{
		// A two-byte character sits ahead of the edit on the changed line, so a
		// byte offset used as a character index is wrong by exactly one here
		// and right on every ASCII line around it.
		const auto left = std::string{
			"context-before\n"
			"const auto na\xc3\xafve = compute(alpha);\n"
			"context-after\n"
		};
		const auto right = std::string{
			"context-before\n"
			"const auto na\xc3\xafve = compute(omega);\n"
			"context-after\n"
		};
		const auto diff = computed(left, right, { }, 47U);
		expect(
			!diff.Intraline.empty(),
			"the fixture produces intraline edits, so the comparison below is "
			"against something");

		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();
		auto profile = qt::appearance_profile{ };
		workbench.adopt_appearance(profile);
		const auto selected = context(diff, 93U, 93U);
		workbench.set_loading(selected);
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		process_events();

		const auto emphasis = selected_row_fill(
			*workbench.right_pane(),
			profile,
			profile.color(core::appearance_role::diff_added_background));
		auto wash = workbench.right_pane()->palette().color(QPalette::Highlight);
		wash.setAlpha(core::selection_wash_alpha);
		auto compared = 0U;
		for (const auto& edit : diff.Intraline) {
			const auto pair = std::ranges::find_if(
				diff.Alignment.Pairs,
				[&edit](const core::line_pair& aPair) {
					return aPair.Id == edit.Pair;
				});
			expect(
				pair != diff.Alignment.Pairs.end(),
				"every intraline edit resolves through the model's own pairs");
			if (edit.Right.Begin >= edit.Right.End) {
				continue;
			}
			// The expectation is computed from the model and the document
			// bytes, not from the widget: this is the comparison the card asks
			// for, and reading the widget for both sides would compare the
			// painter with itself.
			const auto& document = diff.RightDocument;
			const auto& decoded  = document->Lines[pair->RightLine];
			const auto  prefix   = QString::fromUtf8(
				reinterpret_cast<const char*>(
					document->Bytes.data() + decoded.Content.Begin),
				static_cast<qsizetype>(edit.Right.Begin));
			const auto through = QString::fromUtf8(
				reinterpret_cast<const char*>(
					document->Bytes.data() + decoded.Content.Begin),
				static_cast<qsizetype>(edit.Right.End));
			const auto expectedStart  = prefix.size();
			const auto expectedLength = through.size() - prefix.size();
			expect(
				expectedStart
					!= static_cast<qsizetype>(edit.Right.Begin),
				"the multi-byte character really does put the character index "
				"and the byte offset apart, so this comparison can fail");

			const auto painted = emphasis_range(
				*workbench.right_pane(),
				static_cast<int>(pair->RightLine),
				emphasis);
			expect(
				painted.has_value(),
				"the changed line carries painted intraline emphasis");
			expect(
				painted->first == expectedStart
					&& painted->second == expectedLength,
				"the painted character range is the range the core computed, "
				"converted from bytes rather than assumed equal to them");
			const auto& selections    = workbench.right_pane()->extraSelections();
			const auto  washSelection = std::ranges::find_if(
				selections,
				[&wash, &pair](const QTextEdit::ExtraSelection& aSelection) {
					return aSelection.format.boolProperty(
							   QTextFormat::FullWidthSelection)
						&& aSelection.cursor.blockNumber()
						== static_cast<int>(pair->RightLine)
						&& aSelection.format.background().color() == wash;
				});
			const auto emphasisSelection = std::ranges::find_if(
				selections,
				[&emphasis, &pair](const QTextEdit::ExtraSelection& aSelection) {
					return !aSelection.format.boolProperty(
							   QTextFormat::FullWidthSelection)
						&& aSelection.cursor.blockNumber()
						== static_cast<int>(pair->RightLine)
						&& aSelection.format.background().color() == emphasis;
				});
			expect(
				washSelection != selections.end()
					&& emphasisSelection != selections.end()
					&& std::distance(selections.begin(), washSelection)
						< std::distance(selections.begin(), emphasisSelection),
				"the intraline role is painted after the selected-row wash, and\n"
				"carries that wash rather than covering it");
			++compared;
		}
		expect(compared > 0U, "at least one intraline edit was compared");
	}

	/// \brief The removed side must be emphasised exactly as the added side is.
	///
	/// \details The check above reads only `right_pane`, only
	/// `diff_intraline_added_background`, and only `edit.Right`. That is how a
	/// palette in which the removed emphasis was invisible survived to an
	/// attended review: nothing ever asked the left pane whether it had been
	/// marked. BPR-0320 fixed the palette; this keeps the coverage gap from
	/// reopening.
	auto test_intraline_ranges_are_painted_on_the_removed_side() -> void
	{
		const auto left = std::string{
			"context-before\n"
			"const auto na\xc3\xafve = compute(alpha);\n"
			"context-after\n"
		};
		const auto right = std::string{
			"context-before\n"
			"const auto na\xc3\xafve = compute(omega);\n"
			"context-after\n"
		};
		const auto diff = computed(left, right, { }, 47U);
		expect(
			!diff.Intraline.empty(),
			"the fixture produces intraline edits, so the comparison below is "
			"against something");

		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();
		auto profile = qt::appearance_profile{ };
		workbench.adopt_appearance(profile);
		const auto selected = context(diff, 93U, 93U);
		workbench.set_loading(selected);
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		process_events();

		const auto emphasis = selected_row_fill(
			*workbench.left_pane(),
			profile,
			profile.color(core::appearance_role::diff_removed_background));
		const QList<QTextEdit::ExtraSelection> selections
			= workbench.left_pane()->extraSelections();
		auto compared = 0U;
		for (const auto& edit : diff.Intraline) {
			const auto pair = std::ranges::find_if(
				diff.Alignment.Pairs,
				[&edit](const core::line_pair& aPair) {
					return aPair.Id == edit.Pair;
				});
			if (pair == diff.Alignment.Pairs.end()
				|| edit.Left.Begin >= edit.Left.End) {
				continue;
			}
			const auto painted = std::ranges::any_of(
				selections,
				[&emphasis, &pair](const QTextEdit::ExtraSelection& aSelection) {
					return !aSelection.format.boolProperty(
							   QTextFormat::FullWidthSelection)
						&& aSelection.cursor.blockNumber()
						== static_cast<int>(pair->LeftLine)
						&& aSelection.format.background().color() == emphasis;
				});
			expect(
				painted,
				"every removed-side intraline range carries the removed "
				"emphasis colour");
			++compared;
		}
		expect(compared > 0U, "at least one removed-side intraline edit exists");
	}

	/// \brief Selecting a change group must wash the whole group.
	/// \brief A selected row's emphasis must wear the row's own wash.
	///
	/// \details Reported by the product owner: "If the line fill is blue the
	/// intraline emphasis fill should also be a blue, currently it puts
	/// red/green there which makes it more difficult." The emphasis is painted
	/// opaque and last, so before the fix it was the one fill the selection
	/// wash never reached.
	auto test_selected_rows_derive_their_intraline_fill() -> void
	{
		const auto left = std::string{
			"context-before\n"
			"const auto value = compute(alpha);\n"
			"context-after\n"
		};
		const auto right = std::string{
			"context-before\n"
			"const auto value = compute(omega);\n"
			"context-after\n"
		};
		const auto diff = computed(left, right, { }, 51U);
		expect(!diff.Intraline.empty(), "the fixture produces intraline edits");

		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();
		auto profile = qt::appearance_profile{ };
		workbench.adopt_appearance(profile);
		const auto selected = context(diff, 95U, 95U);
		workbench.set_loading(selected);
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		settle_geometry(workbench);

		const auto bare
			= profile.color(core::appearance_role::diff_intraline_added_background);
		const auto washed = selected_row_fill(
			*workbench.right_pane(),
			profile,
			profile.color(core::appearance_role::diff_added_background));
		expect(washed != bare, "a selected row's fill is not the palette colour");

		// Select through the ribbon, the gesture a reviewer actually has.
		const auto visible = workbench.link_map()->visible_links();
		expect(!visible.empty(), "a ribbon is available to select");
		send_click(
			*workbench.link_map(), visible.front().Path.boundingRect().center());
		process_events();

		// Which rows are washed is read back from the painted bands rather
		// than from a member, so the test sees what a reviewer sees.
		const QList<QTextEdit::ExtraSelection> selections
			= workbench.right_pane()->extraSelections();
		auto washedRows = std::set<int>{ };
		for (const auto& selection : selections) {
			if (selection.format.boolProperty(QTextFormat::FullWidthSelection)
				&& selection.format.background().color().alpha()
					== core::selection_wash_alpha) {
				washedRows.insert(selection.cursor.blockNumber());
			}
		}
		expect(!washedRows.empty(), "the click washed at least one row");

		auto washedMarks = 0U;
		auto bareMarks   = 0U;
		for (const auto& selection : selections) {
			if (selection.format.boolProperty(QTextFormat::FullWidthSelection)) {
				continue;
			}
			if (!washedRows.contains(selection.cursor.blockNumber())) {
				continue;
			}
			const auto painted = selection.format.background().color();
			if (painted == washed) {
				++washedMarks;
			} else if (painted == bare) {
				++bareMarks;
			}
		}
		expect(
			washedMarks > 0U,
			"a selected row's mark is a deeper shade of the band it sits in");
		expect(
			bareMarks == 0U,
			"no selected row keeps the unselected added or removed hue");
	}

	auto test_dark_intraline_preserves_readable_syntax_hues() -> void
	{
		const auto left      = std::string{ "\treturn OLD_MACRO(value);\n\tconst auto label = \"draft\";\n" };
		const auto right     = std::string{ "\treturn NEW_MACRO(value);\n\tconst auto label = \"ready\";\n" };
		const auto diff      = computed(left, right, { }, 151U);
		auto       workbench = qt::diff_workbench{ };
		workbench.resize(1200, 600);
		auto profile = qt::appearance_profile{ };
		static_cast<void>(profile.select_mode(core::appearance_mode::dark));
		profile.apply_to(workbench);
		profile.apply_code_font_to(workbench);
		workbench.adopt_appearance(profile);
		workbench.show();
		const auto selected = context(diff, 195U, 195U);
		workbench.set_loading(selected);
		workbench.publish(qt::test::prepare_publication({ .Context = selected, .Result = std::make_shared<const core::computed_file_diff>(diff) }));
		const auto spans = std::make_shared<const application::token_span_list>(application::token_span_list{
			{ .Line = 0U, .Column = 8U, .Length = 9U, .Role = core::appearance_role::syntax_macro },
			{ .Line = 1U, .Column = 20U, .Length = 7U, .Role = core::appearance_role::syntax_string },
		});
		expect(workbench.adopt_token_spans(core::diff_side::left, spans) && workbench.adopt_token_spans(core::diff_side::right, spans), "classified changed macro and string reach both panes");
		settle_geometry(workbench);
		const auto verify = [&] {
			expect(workbench.left_pane()->accessibleName() == QStringLiteral("Earlier source")
					   && workbench.right_pane()->accessibleName() == QStringLiteral("Later source"),
				   "pane identities distinguish removal and addition without colour");
			auto checked = 0U;
			for (auto* const pane : { workbench.left_pane(), workbench.right_pane() }) {
				const auto selections = pane->extraSelections();
				for (const auto& mark : selections) {
					if (mark.format.boolProperty(QTextFormat::FullWidthSelection) || mark.format.background().style() == Qt::NoBrush) {
						continue;
					}
					const auto line       = mark.cursor.blockNumber();
					const auto column     = mark.cursor.selectionStart() - mark.cursor.block().position();
					const auto tokenBegin = line == 0 ? 8 : 20;
					if (column < tokenBegin) {
						continue;
					}
					auto ink = profile.color(line == 0 ? core::appearance_role::syntax_macro : core::appearance_role::syntax_string);
					for (const auto& layer : selections) {
						if (layer.cursor.selectionStart() <= mark.cursor.selectionStart() && layer.cursor.selectionEnd() > mark.cursor.selectionStart()
							&& layer.format.foreground().style() != Qt::NoBrush) {
							ink = layer.format.foreground().color();
						}
					}
					expect(ink.red() > ink.green() && (line != 0 || ink.blue() > ink.green()), "edited syntax retains its macro/string hue instead of turning black or white");
					expect(core::contrast_ratio_hundredths(qt::to_appearance_color(ink), qt::to_appearance_color(mark.format.background().color()))
							   >= core::minimum_contrast_hundredths(core::contrast_kind::emphasis_text, profile.variant()),
						   "every painted edited token clears the published contrast floor");
					++checked;
				}
			}
			expect(checked >= 4U, "both sides expose changed macro and string emphasis");
		};
		verify();
		const auto links = workbench.link_map()->visible_links();
		expect(!links.empty(), "the changed group is selectable");
		send_click(*workbench.link_map(), links.front().Path.boundingRect().center());
		settle_geometry(workbench);
		verify();
		const auto render = qEnvironmentVariable("BPR_QT_DIFF_WORKBENCH_RENDER_PATH");
		if (!render.isEmpty()) {
			expect(workbench.grab().save(render + QStringLiteral("-dark-intraline.png")), "capture layered dark syntax");
		}
	}

	auto test_group_selection_washes_every_line() -> void
	{
		const auto [left, right] = representative_text();
		const auto diff          = computed(left, right, { }, 48U);

		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();
		auto profile = qt::appearance_profile{ };
		workbench.adopt_appearance(profile);
		const auto selected = context(diff, 94U, 94U);
		workbench.set_loading(selected);
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		settle_geometry(workbench);

		// Selecting through the ribbon, because that is the gesture that
		// produces a group selection; setting one directly would prove the
		// painter reads a field rather than that a reviewer can reach it.
		const auto visible = workbench.link_map()->visible_links();
		expect(!visible.empty(), "a ribbon is available to select");
		const auto& ribbon = visible.front();
		send_click(
			*workbench.link_map(), ribbon.Path.boundingRect().center());
		process_events();

		const auto selection = workbench.selection();
		expect(
			selection.has_value()
				&& selection->Kind == core::semantic_selection_kind::group
				&& selection->Group.has_value(),
			"clicking a ribbon selects a change group, not a line");

		const auto group = std::ranges::find(
			diff.Alignment.Groups, *selection->Group, &core::change_group::Id);
		expect(group != diff.Alignment.Groups.end(), "the group is in the alignment");
		const auto span = group->Left.End - group->Left.Begin;
		expect(
			span > 1U,
			"the selected group covers more than one line on the left, so a "
			"single-line wash and a group wash can be told apart");

		// Counted from the pane's own selections rather than from the model:
		// the claim is that the wash reaches the lines, and a model that knew
		// the span while the pane painted one row is exactly the defect.
		//
		// Matched by colour, not merely by being full width. The added and
		// removed bands are full-width selections over these same lines, so a
		// count that ignored colour would tally them and pass with the wash
		// collapsed to a single row — which an earlier draft of this test did.
		auto wash = workbench.left_pane()->palette().color(QPalette::Highlight);
		wash.setAlpha(core::selection_wash_alpha);
		const auto washed = std::ranges::count_if(
			workbench.left_pane()->extraSelections(),
			[&](const QTextEdit::ExtraSelection& aSelection) {
				const auto line = aSelection.cursor.blockNumber();
				return aSelection.format.boolProperty(
						   QTextFormat::FullWidthSelection)
					&& aSelection.format.background().color() == wash
					&& line >= static_cast<int>(group->Left.Begin)
					&& line < static_cast<int>(group->Left.End);
			});
		expect(
			static_cast<std::uint64_t>(washed) >= span,
			"every line of the selected group carries a selection band");
	}

	/// \brief [[BPR-0986]]. The reading line wears the Appearance preview's current-line band.
	///
	/// \details Read off the panes' own selections, as the other bands are. An
	/// unchanged reading line wears `code_current_line_background` on its own
	/// side only; a changed one keeps the band that says it changed; and the one
	/// pane does the same for the reading row.
	auto test_the_reading_line_wears_the_current_line_band() -> void
	{
		auto workbench = qt::diff_workbench{ };
		workbench.resize(1000, 780);
		auto profile = qt::appearance_profile{ };
		static_cast<void>(profile.select_mode(core::appearance_mode::light));
		profile.apply_to(workbench);
		profile.apply_code_font_to(workbench);
		workbench.adopt_appearance(profile);
		workbench.show();
		const auto diff    = computed("start\nold line\nkeep-a\nkeep-b\nend\n", "start\nnew line\nkeep-a\nkeep-b\nend\n", { }, 81U);
		const auto request = context(diff, 151U, 151U);
		workbench.set_loading(request);
		workbench.publish(qt::test::prepare_publication({ .Context = request, .Result = std::make_shared<const core::computed_file_diff>(diff) }));
		settle_geometry(workbench);
		const auto current = profile.color(core::appearance_role::code_current_line_background);
		expect(current.isValid() && current != profile.color(core::appearance_role::diff_added_background)
				   && current != profile.color(core::appearance_role::diff_removed_background),
			   "the current-line role is its own colour, so the assertions below can tell it apart");
		const auto wears = [&](QPlainTextEdit& aPane, const int aLines) {
			auto lines = std::vector<int>{ };
			for (auto line = 0; line < aLines; ++line) {
				if (band_color(aPane, line) == current) {
					lines.push_back(line);
				}
			}
			return lines;
		};

		const auto before = workbench.statistics().CurrentLineBandsPainted;
		expect(workbench.select_conversation_line(core::diff_side::right, 2U), "select the unchanged line keep-a on the right");
		process_events();
		expect(wears(*workbench.right_pane(), 5) == std::vector<int>{ 2 } && wears(*workbench.left_pane(), 5).empty(),
			   "an unchanged reading line wears the current-line band, on its own side only");
		expect(workbench.statistics().CurrentLineBandsPainted > before, "the band is counted where it is composed");

		expect(workbench.select_conversation_line(core::diff_side::right, 1U), "select the replaced line on the right");
		process_events();
		expect(band_color(*workbench.right_pane(), 1) == profile.color(core::appearance_role::diff_added_background)
				   && wears(*workbench.right_pane(), 5).empty(),
			   "a changed reading line keeps the band that says it changed");

		workbench.set_one_pane_enabled(true);
		process_events();
		expect(workbench.select_conversation_line(core::diff_side::right, 2U), "select keep-a again in one pane");
		process_events();
		auto* const unified = workbench.findChild<QPlainTextEdit*>(QStringLiteral("diffUnifiedSource"));
		expect(unified != nullptr && unified->isVisible(), "one pane shows its single source");
		// start, old line, new line, keep-a: the reading line is the fourth row.
		expect(wears(*unified, 6) == std::vector<int>{ 3 }, "the one-pane reading row wears the current-line band");
	}

	/// \brief A ribbon must meet the pane at the block it names.
	///
	/// \details Checked scrolled, which is where BPR-0208 lived. Two separate
	/// errors put ribbons out of place: the map drew at its own y though it is
	/// a sibling of the panes and does not share their band, and the scroll
	/// origin was a fraction of the scrollbar's travel rather than a document
	/// measurement. Both are exercised here — the first by comparing in the
	/// parent's coordinates, the second by asserting after a reveal has
	/// scrolled the panes well down the file.
	auto test_link_map_endpoints_meet_their_blocks() -> void
	{
		const auto [left, right] = representative_text();
		const auto diff          = computed(left, right, { }, 48U);

		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();
		const auto selected = context(diff, 94U, 94U);
		workbench.set_loading(selected);
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		settle_geometry(workbench);

		auto* const map    = workbench.link_map();
		auto* const pane   = workbench.left_pane();
		auto* const parent = map->parentWidget();
		expect(
			map != nullptr && pane != nullptr && parent != nullptr,
			"the composed workbench has a link map, a pane and a parent");
		expect(
			pane->verticalScrollBar()->value()
				> pane->verticalScrollBar()->minimum(),
			"the pane really is scrolled, so a document-origin error can show");

		const auto links = map->visible_links();
		expect(!links.empty(), "the comparison shows at least one ribbon");

		const auto mapTop  = map->mapTo(parent, QPoint{ 0, 0 }).y();
		const auto paneTop = pane->viewport()->mapTo(parent, QPoint{ 0, 0 }).y();
		expect(
			mapTop != paneTop,
			"the map and the pane viewport really do start at different y, so "
			"the band correction can fail");

		auto checked = 0U;
		for (const auto& link : links) {
			const auto segment = std::ranges::find_if(
				diff.Alignment.Segments,
				[&link](const core::alignment_segment& aSegment) {
					return aSegment.Id == link.Identity.Segment;
				});
			if (segment == diff.Alignment.Segments.end()) {
				continue;
			}
			const auto block = pane->document()->findBlockByNumber(
				static_cast<int>(segment->Left.Begin));
			if (!block.isValid()) {
				continue;
			}
			const auto blockTop = pane->cursorRect(QTextCursor{ block }).top()
				+ paneTop;
			if (blockTop < paneTop
				|| blockTop > paneTop + pane->viewport()->height()) {
				continue;
			}
			const auto ribbonTop = qRound(link.LeftTop) + mapTop;
			// Exact, not approximate. Both quantities are integers derived
			// from the same per-line measurement, and measured at 100, 125 and
			// 200 per cent the difference is zero rather than small. A
			// tolerance would have to be two pixels wide to admit the band
			// error this test exists to catch, which would make it admit the
			// error rather than catch it.
			expect(
				ribbonTop == blockTop,
				"a ribbon meets the pane at the top of the block it names");
			++checked;
		}
		expect(checked > 0U, "at least one visible ribbon was checked");
	}

	/// \brief Change-group boundaries must be marked where the model puts them.
	auto test_group_boundaries_are_painted() -> void
	{
		const auto [left, right] = representative_text();
		const auto diff          = computed(left, right, { }, 48U);
		expect(
			!diff.Alignment.Groups.empty(),
			"the fixture produces change groups");

		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'200, 760);
		workbench.show();
		process_events();
		auto profile = qt::appearance_profile{ };
		workbench.adopt_appearance(profile);
		const auto selected = context(diff, 94U, 94U);
		workbench.set_loading(selected);
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		settle_geometry(workbench);

		auto& leftPane = *workbench.left_pane();

		// The boundary is a drawn rule, so the pane has to have painted for
		// there to be anything to assert. Grabbing the viewport is what makes
		// that happen, and it is also the only place the rule exists — it is
		// deliberately not an extra selection.
		const auto pixels = leftPane.viewport()->grab().toImage();
		expect(
			workbench.statistics().GroupBoundariesPainted > 0U,
			"a comparison with change groups draws at least one boundary rule");

		const auto rule
			= profile.color(core::appearance_role::diff_group_boundary_rule);
		expect(rule.isValid(), "the profile supplies a boundary rule colour");

		// A boundary belongs on the edge above the group's first line. Find a
		// group the viewport is actually showing and check the pixels there.
		auto checked = false;
		for (const auto& group : diff.Alignment.Groups) {
			if (group.First.Left == 0U) {
				continue;
			}
			const auto block = leftPane.document()->findBlockByNumber(
				static_cast<int>(group.First.Left));
			if (!block.isValid()) {
				continue;
			}
			// cursorRect is in logical pixels and the grab is in device ones,
			// so the two only agree at a device pixel ratio of one. The scaled
			// courses run at 1.25 and 2.0, where reading the image at a logical
			// y lands on a different line entirely.
			const auto ratio = pixels.devicePixelRatio();
			const auto top   = qRound(
				leftPane.cursorRect(QTextCursor{ block }).top() * ratio);
			if (top <= 0 || top >= pixels.height()) {
				continue;
			}
			// The pen is BoundaryRuleWidth logical pixels, so the run is that
			// many device rows thick and the search widens with the ratio.
			const auto reach = qRound(2.0 * ratio) + 1;
			auto       found = false;
			for (auto offset = -reach; offset <= reach && !found; ++offset) {
				const auto y = top + offset;
				if (y < 0 || y >= pixels.height()) {
					continue;
				}
				auto run = 0;
				for (auto x = 0; x < pixels.width(); ++x) {
					if (QColor{ pixels.pixelColor(x, y) } == rule) {
						++run;
					}
				}
				found = run > pixels.width() / 2;
			}
			expect(
				found,
				"the edge above a change group's first line carries a rule in "
				"the boundary colour, spanning the pane");

			// The defect BPR-0202 repaired: the unchanged line above the group
			// used to be filled edge to edge, in the same vocabulary the added
			// and removed bands use, to say something that was not true of it.
			expect(
				!band_color(leftPane, static_cast<int>(group.First.Left - 1U))
					 .isValid(),
				"the unchanged line above a change group carries no band of "
				"its own");
			checked = true;
			break;
		}
		expect(checked, "at least one change group boundary was visible to check");
	}
	void test_one_pane_round_trip_keeps_an_unselected_empty_side_anchor()
	{
		for (const auto deletion : { true, false }) {
			auto common = std::string{ };
			for (auto line = 0; line < 20; ++line) {
				common += "common " + std::to_string(line) + "\n";
			}
			auto longer = common;
			for (auto line = 0; line < 100; ++line) {
				longer += "one side " + std::to_string(line) + "\n";
			}
			for (auto line = 0; line < 80; ++line) {
				const auto tail = "tail " + std::to_string(line) + "\n";
				common += tail;
				longer += tail;
			}
			const auto diff = computed(deletion ? longer : common, deletion ? common : longer, { }, deletion ? 80U : 81U);
			expect(diff.Alignment.Groups.size() == 1U && (deletion ? diff.Alignment.Groups.front().Right : diff.Alignment.Groups.front().Left).empty(),
				   "round-trip fixture contains a large change with one empty side");
			auto workbench = qt::diff_workbench{ };
			workbench.resize(900, 540);
			workbench.show();
			const auto request = context(diff, 151U, 151U);
			workbench.set_loading(request);
			workbench.publish(qt::test::prepare_publication({ .Context = request, .Result = std::make_shared<const core::computed_file_diff>(diff) }));
			settle_geometry(workbench);
			const auto selectedSide = deletion ? core::diff_side::right : core::diff_side::left;
			expect(workbench.select_conversation_line(selectedSide, 5U), "select an equal line on the side absent from the large change");
			const auto selection = workbench.selection();
			workbench.set_one_pane_enabled(true);
			process_events();
			auto* const unified = workbench.findChild<QPlainTextEdit*>(QStringLiteral("diffUnifiedSource"));
			workbench.shared_scrollbar()->setValue(70);
			process_events();
			const auto before = unified->cursorForPosition(QPoint{ 0, 0 }).blockNumber();
			expect(before == 70 && unified->document()->findBlockByNumber(before).text() == QStringLiteral("one side 50"), "reading viewport is in the middle of the unselected one-sided block");
			expect(workbench.selection() == selection, "scrolling leaves the opposite-side semantic selection unchanged");
			workbench.set_one_pane_enabled(false);
			settle_geometry(workbench);
			workbench.set_one_pane_enabled(true);
			process_events();
			const auto after = unified->cursorForPosition(QPoint{ 0, 0 }).blockNumber();
			if (after != before) {
				std::cerr << "one-sided round trip deletion=" << deletion << " before=" << before << " after=" << after << '\n';
			}
			expect(after == before && workbench.selection() == selection,
				   "round trip preserves the top row's own-side identity independently of the semantic selection side");
			workbench.set_one_pane_enabled(false);
			settle_geometry(workbench);
			auto* const opposite    = selectedSide == core::diff_side::left ? workbench.left_pane() : workbench.right_pane();
			const auto  oppositeTop = opposite->cursorForPosition(QPoint{ 0, 0 }).blockNumber();
			expect(workbench.select_conversation_line(selectedSide, 5U) && workbench.selection() == selection,
				   "an explicit repeated line selection retakes its side without changing semantic identity");
			workbench.set_one_pane_enabled(true);
			process_events();
			const auto expectedTop = oppositeTop < 20 ? oppositeTop : oppositeTop + 100;
			expect(unified->cursorForPosition(QPoint{ 0, 0 }).blockNumber() == expectedTop,
				   "explicit line selection replaces the retained viewport side instead of pinning an old reading anchor");
		}
	}

	void test_one_pane_rows_anchors_and_link_map()
	{
		auto workbench = qt::diff_workbench{ };
		workbench.resize(1000, 780);
		auto profile = qt::appearance_profile{ };
		static_cast<void>(profile.select_mode(core::appearance_mode::light));
		profile.apply_to(workbench);
		profile.apply_code_font_to(workbench);
		workbench.adopt_appearance(profile);
		workbench.show();
		auto left  = std::string{ "start\nint count = 1;\nold second\nmiddle\nkeep-a\nkeep-b\nremoved only\nkeep-c\nkeep-d\nend\n" };
		auto right = std::string{ "start\nint count = 2;\nnew second\nmiddle\nkeep-a\nkeep-b\nkeep-c\nkeep-d\ninserted only\nend\n" };
		for (auto line = 0; line < 120; ++line) {
			const auto text = "tail " + std::to_string(line) + "\n";
			left += text;
			right += text;
		}
		const auto diff = computed(left, right, { }, 79U);
		expect(diff.Alignment.Groups.size() == 3U && !diff.Intraline.empty(), "unified fixture has replacement, deletion, insertion and word edits");
		const auto request = context(diff, 150U, 150U);
		workbench.set_loading(request);
		workbench.publish(qt::test::prepare_publication({ .Context = request, .Result = std::make_shared<const core::computed_file_diff>(diff) }));
		settle_geometry(workbench);
		const auto  selection     = workbench.selection();
		const auto  index         = workbench.current_semantic_index();
		auto* const hiddenUnified = workbench.findChild<QPlainTextEdit*>(QStringLiteral("diffUnifiedSource"));
		expect(hiddenUnified && hiddenUnified->document()->isEmpty(), "two-pane publication does not construct a hidden third source document");
		workbench.set_one_pane_enabled(true);
		process_events();
		auto* const unified = workbench.findChild<QPlainTextEdit*>(QStringLiteral("diffUnifiedSource"));
		auto* const map     = dynamic_cast<qt::one_pane_diff_link_map*>(workbench.findChild<QWidget*>(QStringLiteral("diffOnePaneLinkMap")));
		expect(unified != nullptr && unified->isVisible() && map != nullptr && map->isVisible(), "one pane displays one source document and its gutter link map");
		expect(!workbench.comparison_splitter()->isVisible() && workbench.selection() == selection && workbench.current_semantic_index() == index,
			   "presentation flip preserves selection and the one immutable semantic index");
		struct expected_row
		{
			const char*                  Text;
			std::optional<std::uint32_t> Base;
			std::optional<std::uint32_t> Head;
		};
		const auto rows = std::array{
			expected_row{ "start", 0U, 0U },
			expected_row{ "int count = 1;", 1U, std::nullopt },
			expected_row{ "old second", 2U, std::nullopt },
			expected_row{ "int count = 2;", std::nullopt, 1U },
			expected_row{ "new second", std::nullopt, 2U },
			expected_row{ "middle", 3U, 3U },
			expected_row{ "keep-a", 4U, 4U },
			expected_row{ "keep-b", 5U, 5U },
			expected_row{ "removed only", 6U, std::nullopt },
			expected_row{ "keep-c", 7U, 6U },
			expected_row{ "keep-d", 8U, 7U },
			expected_row{ "inserted only", std::nullopt, 8U },
			expected_row{ "end", 9U, 9U },
		};
		for (auto rowIndex = std::size_t{ 0U }; rowIndex < rows.size(); ++rowIndex) {
			const auto& row   = rows[rowIndex];
			const auto  block = unified->document()->findBlockByNumber(static_cast<int>(rowIndex));
			expect(block.text() == QString::fromUtf8(row.Text), "unified document follows exact old-then-new block order");
			const auto cursorRect = unified->cursorRect(QTextCursor{ block });
			const auto y          = unified->viewport()->mapTo(&workbench, cursorRect.center()).y();
			if (row.Base) {
				expect(workbench.line_rect(core::diff_side::left, *row.Base).contains(QPoint{ unified->viewport()->mapTo(&workbench, QPoint{ 10, 0 }).x(), y }), "base anchor resolves to its own unified row");
			}
			if (row.Head) {
				expect(workbench.line_rect(core::diff_side::right, *row.Head).contains(QPoint{ unified->viewport()->mapTo(&workbench, QPoint{ 10, 0 }).x(), y }), "head anchor resolves to its own unified row");
			}
			auto click = QMouseEvent{ QEvent::MouseButtonPress, QPointF{ cursorRect.center() }, QPointF{ unified->viewport()->mapToGlobal(cursorRect.center()) }, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
			QApplication::sendEvent(unified->viewport(), &click);
			expect(workbench.selection() && workbench.selection()->PreferredLine.Side == (row.Head ? core::diff_side::right : core::diff_side::left)
					   && workbench.selection()->PreferredLine.Line == row.Head.value_or(row.Base.value_or(0U)),
				   "click carries the displayed row's exact source side and number");
			const auto formats = block.layout()->formats();
			const auto struck  = std::ranges::any_of(formats, [](const QTextLayout::FormatRange& aFormat) { return aFormat.format.fontStrikeOut(); });
			expect(struck == !row.Head.has_value(), "only base-only rows carry a strike");
		}
		expect(workbench.line_rect(core::diff_side::left, 1U).top() < workbench.line_rect(core::diff_side::right, 1U).top(), "replacement sides have separate thread geometry");
		expect(workbench.select_conversation_line(core::diff_side::right, 0U), "select unchanged row for unwashed change evidence");
		process_events();
		const auto paintedSelections = unified->extraSelections();
		for (const auto& edit : diff.Intraline) {
			const auto pair = std::ranges::find(diff.Alignment.Pairs, edit.Pair, &core::line_pair::Id);
			expect(pair != diff.Alignment.Pairs.end(), "intraline identity resolves to the accepted pair");
			for (const auto side : { core::diff_side::left, core::diff_side::right }) {
				const auto span = side == core::diff_side::left ? edit.Left : edit.Right;
				if (span.empty()) {
					continue;
				}
				const auto line = side == core::diff_side::left ? pair->LeftLine : pair->RightLine;
				const auto row  = std::ranges::find_if(rows, [side, line](const expected_row& aRow) { return (side == core::diff_side::left ? aRow.Base : aRow.Head) == line; });
				expect(row != rows.end(), "intraline row belongs to the displayed fixture");
				const auto rowIndex = static_cast<int>(std::distance(rows.begin(), row));
				expect(std::ranges::any_of(paintedSelections, [&](const QTextEdit::ExtraSelection& aSelection) {
						   return !aSelection.format.property(QTextFormat::FullWidthSelection).toBool() && aSelection.cursor.blockNumber() == rowIndex
							   && aSelection.cursor.selectionStart() - aSelection.cursor.block().position() == static_cast<int>(span.Begin)
							   && aSelection.cursor.selectedText().size() == static_cast<qsizetype>(span.End - span.Begin);
					   }),
					   "word-level emphasis maps the exact source span to its unified row");
			}
		}
		const auto links = map->visible_links();
		expect(links.size() == 3U, "gutter projects the three visible blocks");
		expect(!links[0].RemovedRibbon.isEmpty() && !links[0].AddedRibbon.isEmpty(), "replacement draws both ribbons on one spine");
		expect(!links[1].RemovedRibbon.isEmpty() && links[1].AddedRibbon.isEmpty(), "pure deletion draws only its removed ribbon");
		expect(links[2].RemovedRibbon.isEmpty() && !links[2].AddedRibbon.isEmpty(), "pure insertion draws only its added ribbon");
		for (const auto& link : links) {
			expect(link.Spine.length() > 0.0 && link.Spine.length() <= 14.0, "each change has a short spine, not a whole-file overview");
			for (const auto* path : { &link.RemovedRibbon, &link.AddedRibbon }) {
				if (!path->isEmpty()) {
					expect(path->boundingRect().left() == link.Spine.x1(), "both ribbons converge on their own spine");
				}
			}
		}
		auto* const gutter      = unified->findChild<QWidget*>(QStringLiteral("diffSourceLineNumbers"));
		const auto  gutterImage = gutter->grab().toImage();
		const auto  ratio       = gutterImage.devicePixelRatio();
		const auto  removed     = profile.color(core::appearance_role::diff_removed_background);
		const auto  added       = profile.color(core::appearance_role::diff_added_background);
		const auto  number      = profile.color(core::appearance_role::code_line_number_text);
		const auto  renderPath  = qEnvironmentVariable("BPR_QT_DIFF_WORKBENCH_RENDER_PATH");
		if (!renderPath.isEmpty()) {
			auto gutterPath = renderPath;
			gutterPath.replace(QStringLiteral(".png"), QStringLiteral("-one-pane-gutter.png"));
			expect(gutterImage.save(gutterPath), "base/head gutter crop is retained for inspection");
		}
		for (auto rowIndex = std::size_t{ 0U }; rowIndex < rows.size(); ++rowIndex) {
			const auto& row     = rows[rowIndex];
			const auto  rowRect = unified->cursorRect(QTextCursor{ unified->document()->findBlockByNumber(static_cast<int>(rowIndex)) });
			const auto  gutterY = unified->viewport()->mapTo(&workbench, rowRect.topLeft()).y() - gutter->mapTo(&workbench, QPoint{ }).y();
			if (!row.Head || !row.Base) {
				expect(gutterImage.pixelColor(qRound(2 * ratio), qRound((gutterY + rowRect.height() / 2.0) * ratio)) == (!row.Head ? removed : added), "change field includes both number columns");
			}
			for (auto side = 0; side < 2; ++side) {
				auto       count  = 0;
				const auto column = gutterImage.width() / 2;
				for (auto y = qRound(gutterY * ratio); y < qRound((gutterY + rowRect.height()) * ratio); ++y) {
					for (auto x = side * column; x < (side + 1) * column; ++x) {
						const auto pixel = gutterImage.pixelColor(x, y);
						// A small glyph at 100% can contain only antialiased pixels.
						// Count substantial foreground coverage; a blank column's
						// background is far outside this per-channel tolerance.
						if (std::abs(pixel.red() - number.red()) <= 32
							&& std::abs(pixel.green() - number.green()) <= 32
							&& std::abs(pixel.blue() - number.blue()) <= 32) {
							++count;
						}
					}
				}
				if ((count > 0) != (side == 0 ? row.Base.has_value() : row.Head.has_value())) {
					std::cerr << "number column row=" << rowIndex << " side=" << side << " pixels=" << count << " y=" << gutterY << '\n';
				}
				expect((count > 0) == (side == 0 ? row.Base.has_value() : row.Head.has_value()), "displayed base/head number columns are present only on their own rows");
			}
		}
		expect(workbench.select_conversation_line(core::diff_side::left, 1U), "select a replacement for the current spine raster");
		process_events();
		const auto mapImage = map->grab().toImage();
		const auto mapRatio = mapImage.devicePixelRatio();
		const auto spine    = map->visible_links().front().Spine.center();
		expect(mapImage.pixelColor(qRound(spine.x() * mapRatio), qRound(spine.y() * mapRatio)) == map->palette().color(QPalette::Highlight), "current change spine paints in the source selection signal colour");
		if (!renderPath.isEmpty()) {
			auto onePanePath = renderPath;
			onePanePath.replace(QStringLiteral(".png"), QStringLiteral("-one-pane.png"));
			expect(workbench.grab().save(onePanePath), "one-pane rendered evidence is written");
		}
		expect(workbench.reveal_conversation_line(core::diff_side::left, 1U), "removed line reveals by exact base identity");
		expect(!workbench.conversation_anchor_marker_rect(core::diff_side::left).isEmpty() && workbench.conversation_anchor_marker_rect(core::diff_side::right).isEmpty(), "removed-line marker never aliases the head row");
		expect(workbench.reveal_conversation_line(core::diff_side::right, 1U), "added line reveals by exact head identity");
		expect(workbench.conversation_anchor_marker_rect(core::diff_side::left).isEmpty() && !workbench.conversation_anchor_marker_rect(core::diff_side::right).isEmpty(), "added-line marker belongs only to the head");
		expect(workbench.navigate_change(core::change_navigation_direction::next, core::change_navigation_policy::stop), "next change advances a whole block");
		expect(workbench.selection()->Group == diff.Alignment.Groups[1].Id, "one pane retains the core change ordinal");
		const auto selectedDeletion = workbench.selection();
		workbench.set_one_pane_enabled(false);
		settle_geometry(workbench);
		expect(workbench.selection() == selectedDeletion, "two-pane restoration keeps the same change ordinal");
		expect(unified->document()->isEmpty() && map->visible_links().empty(), "leaving one pane releases its source and line geometry");
		workbench.set_one_pane_enabled(true);
		process_events();
		workbench.shared_scrollbar()->setValue(50);
		process_events();
		const auto readingRow = unified->cursorForPosition(QPoint{ 0, 0 }).blockNumber();
		workbench.set_one_pane_enabled(false);
		settle_geometry(workbench);
		workbench.set_one_pane_enabled(true);
		process_events();
		expect(std::abs(unified->cursorForPosition(QPoint{ 0, 0 }).blockNumber() - readingRow) <= 1
				   && workbench.selection() == selectedDeletion,
			   "presentation round trip preserves the scrolled reading row and semantic selection");
		workbench.shared_scrollbar()->setValue(workbench.shared_scrollbar()->maximum());
		process_events();
		expect(map->visible_links().empty(), "scrolling beyond changes leaves an empty gutter rather than a file overview");
		workbench.clear();
		expect(unified->toPlainText().isEmpty() && map->visible_links().empty() && workbench.line_rect(core::diff_side::left, 1U).isEmpty(), "invalidation clears unified text, ribbons and old-side geometry");
	}
} // namespace

namespace {
	/// \brief [[BPR-1107]]. The design page's pane captures: the owner's own
	/// reference lines, side by side with one change, in each of the four themes
	/// and each white space choice. Written only when a directory is named, as
	/// the other capture courses do; the assertions run either way.
	/// \brief The block at a pane's top edge, read off the pane, not its bar.
	[[nodiscard]] auto top_line(QPlainTextEdit& aPane) -> int
	{
		return aPane.cursorForPosition(QPoint{ 0, 1 }).block().blockNumber();
	}

	/// \brief The viewport y of a pane's last line.
	[[nodiscard]] auto last_line_y(QPlainTextEdit& aPane) -> int
	{
		return aPane.cursorRect(QTextCursor{ aPane.document()->lastBlock() }).top()
			+ aPane.viewport()->y() - aPane.contentsRect().top();
	}

	/// \brief Whether each visible ribbon edge meets the pane at the top of the
	/// line it names, on both sides. Exact, as in
	/// `test_link_map_endpoints_meet_their_blocks`.
	[[nodiscard]] auto check_ribbons(qt::diff_workbench& aWorkbench, const core::computed_file_diff& aDiff) -> ribbon_check
	{
		auto* const map    = aWorkbench.link_map();
		auto* const parent = map->parentWidget();
		const auto  mapTop = map->mapTo(parent, QPoint{ 0, 0 }).y();
		auto        result = ribbon_check{ };
		const auto  edge   = [&](QPlainTextEdit& aPane, const std::uint32_t aLine, const qreal aRibbon) {
			const auto block = aPane.document()->findBlockByNumber(static_cast<int>(aLine));
			if (!block.isValid()) {
				return;
			}
			const auto blockTop = aPane.cursorRect(QTextCursor{ block }).top();
			if (blockTop < 0 || blockTop > aPane.viewport()->height()) {
				return;
			}
			++result.Checked;
			if (qRound(aRibbon) + mapTop != blockTop + aPane.viewport()->mapTo(parent, QPoint{ 0, 0 }).y()) {
				++result.Mismatched;
			}
		};
		for (const auto& link : map->visible_links()) {
			const auto segment = std::ranges::find_if(aDiff.Alignment.Segments, [&link](const core::alignment_segment& aSegment) {
				return aSegment.Id == link.Identity.Segment;
			});
			if (segment == aDiff.Alignment.Segments.end()) {
				continue;
			}
			edge(*aWorkbench.left_pane(), segment->Left.Begin, link.LeftTop);
			edge(*aWorkbench.left_pane(), segment->Left.End, link.LeftBottom);
			edge(*aWorkbench.right_pane(), segment->Right.Begin, link.RightTop);
			edge(*aWorkbench.right_pane(), segment->Right.End, link.RightBottom);
		}
		return result;
	}

	/// \brief [[BPR-1291]]. The owner's `renderer.cpp` (7 lines against 14,
	/// each pane showing an empty last line) in a workbench taller than
	/// either. Before, the shared bar travelled the whole comparison while
	/// both panes stayed at their top, and no pane could scroll past its end.
	void test_the_panes_scroll_past_their_end_and_the_bar_agrees()
	{
		constexpr auto base = "#include \"renderer.h\"\n\nvoid Renderer::draw()\n{\n\tclear();\n\tpresent();\n}\n";
		constexpr auto head = "#include \"renderer.h\"\n\nvoid Renderer::draw()\n{\n\tclear();\n\tdraw_scene();\n\tpresent();\n}\n\n"
							  "void Renderer::resize(const int aWidth, const int aHeight)\n{\n\tWidth  = aWidth;\n\tHeight = aHeight;\n}\n";
		const auto     diff = computed(base, head, { }, 53U);

		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'260, 820);
		workbench.show();
		process_events();
		const auto selected = context(diff, 95U, 95U);
		workbench.set_loading(selected);
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		settle_geometry(workbench);

		auto&       left       = *workbench.left_pane();
		auto&       right      = *workbench.right_pane();
		auto* const bar        = workbench.shared_scrollbar();
		auto* const controller = workbench.scroll_controller();
		const auto  lastLeft   = left.document()->blockCount() - 1;
		const auto  lastRight  = right.document()->blockCount() - 1;
		expect(lastLeft == 7 && lastRight == 14, "each pane ends in the empty line after its final line break");
		bar->triggerAction(QAbstractSlider::SliderToMinimum);
		process_events();
		expect(last_line_y(right) < right.viewport()->height() / 2,
			   "the file is shorter than half the viewport, so nothing is hidden at the top");
		expect(top_line(left) == 0 && top_line(right) == 0, "the bar's minimum shows both files from their first line");
		expect(bar->isEnabled() && bar->maximum() > bar->minimum(), "a file shorter than the viewport still has travel");

		bar->triggerAction(QAbstractSlider::SliderToMaximum);
		process_events();
		expect(top_line(left) == lastLeft && top_line(right) == lastRight && last_line_y(left) <= 0 && last_line_y(right) <= 0,
			   "the bar's maximum puts each side's last line at the top, as VS Code does");

		// Observe rendered travel: a native row number alone aliases distinct
		// fractional positions. The right side leaves its top first and moves one to one with the
		// position, so one line of travel is one line of the right pane.
		const auto lineHeight = static_cast<qreal>(right.cursorRect(QTextCursor{ right.document()->findBlockByNumber(2) }).top()
												   - right.cursorRect(QTextCursor{ right.document()->findBlockByNumber(1) }).top());
		const auto span       = controller->maximum_position() - controller->minimum_position();
		expect(lineHeight > 0.0 && span > lineHeight * (lastRight - 1), "the travel is the right file's height less its last line");
		auto previous = std::pair{ std::numeric_limits<int>::lowest(), std::numeric_limits<int>::lowest() };
		auto level    = 0;
		auto ribbons  = std::size_t{ 0U };
		for (auto step = 0; step <= lastRight + 1; ++step) {
			const auto value = static_cast<int>(std::llround(std::min(1.0, static_cast<qreal>(step) * lineHeight / span) * bar->maximum()));
			bar->setValue(value);
			process_events();
			const auto tops = std::pair{ rendered_origin(left), rendered_origin(right) };
			expect(tops.first >= previous.first && tops.second >= previous.second && tops != previous,
				   "each line of bar travel moves a pane, and neither pane moves back");
			const auto check = check_ribbons(workbench, diff);
			expect(check.Mismatched == 0U, "every visible ribbon meets both panes at the lines it names");
			ribbons += check.Checked;
			if (top_line(left) > 0) {
				expect(last_line_y(left) == last_line_y(right), "once both panes move, the two files' last lines are level");
				++level;
			}
			previous = tops;
		}
		expect(top_line(left) == lastLeft && top_line(right) == lastRight && level > 2 && ribbons > 10U,
			   "the sweep reached the end, past both files' ends, with ribbons in view");

		// Negative control: a pane moved behind the bar's back. The same checks
		// see it: the ribbons no longer meet the right pane, and the ends are
		// no longer level.
		bar->setValue(bar->maximum() / 2);
		process_events();
		expect(check_ribbons(workbench, diff).Mismatched == 0U, "the control starts aligned");
		right.verticalScrollBar()->setValue(right.verticalScrollBar()->value() + 1);
		process_events();
		const auto moved = check_ribbons(workbench, diff);
		expect(moved.Checked > 0U && moved.Mismatched > 0U, "a pane moved behind the bar's back is seen as out of line");
		expect(top_line(left) == 0 || last_line_y(left) != last_line_y(right), "and its end as out of level");
		bar->triggerAction(QAbstractSlider::SliderToMinimum);
		process_events();

		// The wheel and End reach the same end, and go no further.
		for (auto turn = 0; turn < 60; ++turn) {
			static_cast<void>(send_wheel(*right.viewport(), QPoint{ 0, -40 }));
		}
		process_events();
		expect(top_line(right) == lastRight && top_line(left) == lastLeft && close(controller->position(), controller->maximum_position()),
			   "the wheel scrolls the short file until its last line is at the top, and no further");
		send_key(right, Qt::Key_Home);
		process_events();
		expect(top_line(left) == 0 && top_line(right) == 0, "Home returns both files to their first line");
		send_key(right, Qt::Key_End);
		process_events();
		expect(top_line(left) == lastLeft && top_line(right) == lastRight, "End puts each last line at the top");

		// A file longer than the viewport scrolls past its end the same way.
		auto longBase = std::string{ };
		auto longHead = std::string{ };
		for (auto line = 0; line < 160; ++line) {
			longBase += "line-" + std::to_string(line) + '\n';
			longHead += (line == 80 ? std::string{ "changed-80" } : "line-" + std::to_string(line)) + '\n';
		}
		const auto longDiff    = computed(longBase, longHead, { }, 54U);
		const auto longContext = context(longDiff, 96U, 96U);
		workbench.set_loading(longContext);
		workbench.publish(bpr::qt::test::prepare_publication({
			.Context = longContext,
			.Result  = std::make_shared<const core::computed_file_diff>(longDiff),
		}));
		settle_geometry(workbench);
		send_key(right, Qt::Key_End);
		process_events();
		expect(top_line(left) == 160 && top_line(right) == 160 && last_line_y(left) <= 0 && last_line_y(right) <= 0,
			   "End puts a long file's last line at the top");
		send_key(right, Qt::Key_Home);
		process_events();
		expect(top_line(left) == 0 && top_line(right) == 0, "Home returns a long file to its first line");
		previous = { std::numeric_limits<int>::lowest(), std::numeric_limits<int>::lowest() };
		for (auto step = 0; step <= 50; ++step) {
			bar->setValue(bar->maximum() * step / 50);
			process_events();
			const auto tops = std::pair{ rendered_origin(left), rendered_origin(right) };
			expect(tops.first > previous.first && tops.second > previous.second,
				   "each fiftieth of the bar moves a long file's panes together");
			expect(check_ribbons(workbench, longDiff).Mismatched == 0U, "a long file's ribbons meet both panes");
			previous = tops;
		}

		// One pane scrolls past its end as well.
		workbench.set_one_pane_enabled(true);
		settle_geometry(workbench);
		auto* const unified = workbench.findChild<QPlainTextEdit*>(QStringLiteral("diffUnifiedSource"));
		expect(unified != nullptr && workbench.shared_scrollbar() == unified->verticalScrollBar()
				   && unified->verticalScrollBar()->maximum() == unified->document()->blockCount() - 1,
			   "the one-pane view's bar reaches its last line at the top");
		workbench.set_one_pane_enabled(false);
		settle_geometry(workbench);
	}

	void test_white_space_design_captures()
	{
		// The owner's example from his note of 2026-09-25, byte for byte, with
		// two trailing spaces added to the first line so Trailing has something
		// to draw. v1 copied the reference picture instead, and with it a stray
		// space after each run of tabs, so every tab run ended in a dot.
		constexpr auto leftText  = "     4 spaces.          8 more spaces  \n\t\t2 tabs.\n\t1 tab.\tAnother tab, smaller because of spacing\n";
		constexpr auto rightText = "     4 spaces.          8 more spaces  \n\t\t2 tabs.\n\t1 tab.\tAnother tab, smaller because of spacing!\n";
		const auto     diff      = computed(leftText, rightText, { }, 51U);
		const auto     directory = qEnvironmentVariable("BPR_WHITE_SPACE_CAPTURE_DIR");
		for (const auto mode : { core::appearance_mode::light, core::appearance_mode::dark, core::appearance_mode::high_contrast_light, core::appearance_mode::high_contrast_dark }) {
			auto workbench = qt::diff_workbench{ };
			workbench.resize(1'400, 320);
			workbench.show();
			auto profile = qt::appearance_profile{ };
			static_cast<void>(profile.select_mode(mode));
			const auto selected = context(diff, 92U, 92U);
			workbench.set_loading(selected);
			workbench.publish(qt::test::prepare_publication({
				.Context = selected,
				.Result  = std::make_shared<const core::computed_file_diff>(diff),
			}));
			settle_geometry(workbench);
			for (const auto drawing : { core::white_space_drawing::none, core::white_space_drawing::selection, core::white_space_drawing::leading,
										core::white_space_drawing::trailing, core::white_space_drawing::boundary, core::white_space_drawing::all }) {
				auto code       = profile.typography().Code;
				code.TabWidth   = 4U;
				code.WhiteSpace = drawing;
				// Large enough that a dot reads on the design page.
				code.Font.PointSizeCentipoints = 1600U;
				expect(profile.select_code_font(code).accepted(), "every white space choice is accepted for the capture");
				profile.apply_to(workbench);
				workbench.adopt_appearance(profile);
				profile.apply_code_font_to(workbench);
				// The reader on the first line, so Selection has a
				// selected line to draw on.
				static_cast<void>(workbench.select_conversation_line(core::diff_side::right, 0U));
				// The first change is revealed on the rail, and since
				// [[BPR-1291]] a file shorter than the viewport scrolls to put
				// it there too. Back to the top, where the reader is, once the
				// new font's geometry is in.
				settle_geometry(workbench);
				workbench.scroll_controller()->home();
				QApplication::processEvents();
				const auto before = workbench.statistics().WhiteSpacePainted;
				workbench.left_pane()->viewport()->repaint();
				workbench.right_pane()->viewport()->repaint();
				const auto drawn = workbench.statistics().WhiteSpacePainted - before;
				expect((drawing == core::white_space_drawing::none) == (drawn == 0U), "every choice but none draws something on these lines");
				if (!directory.isEmpty()) {
					// The two panes side by side, without the chrome around them.
					const auto left    = workbench.left_pane()->grab();
					const auto right   = workbench.right_pane()->grab();
					auto       picture = QPixmap{ QSize{ left.width() + right.width(), std::max(left.height(), right.height()) } };
					picture.setDevicePixelRatio(left.devicePixelRatio());
					picture.fill(Qt::transparent);
					{
						auto painter = QPainter{ &picture };
						painter.drawPixmap(0, 0, left);
						painter.drawPixmap(static_cast<int>(static_cast<qreal>(left.width()) / left.devicePixelRatio()), 0, right);
					}
					const auto path = QStringLiteral("%1/panes-%2-%3.png")
										  .arg(directory, QString::fromUtf8(core::stable_name(mode).data(), static_cast<qsizetype>(core::stable_name(mode).size())),
											   QString::fromUtf8(core::stable_name(drawing).data(), static_cast<qsizetype>(core::stable_name(drawing).size())));
					expect(picture.save(path, "PNG"), "the design capture is written");
				}
			}
			workbench.hide();
		}
	}

	/// \brief [[BPR-1334]]. Qt's transient scroll bars, which macOS draws by
	/// default: a bar that floats over the bottom of the viewport and takes no
	/// height of its own.
	///
	/// \details v002 placed the Line details gutter for a legacy bar only. The
	/// offscreen rows passed; the native macOS launch failed "the gutter sits
	/// beside the rows and above the bar", because there the bar overlays the
	/// viewport. This style gives offscreen runs that native geometry.
	class overlay_scroll_bar_style final : public QProxyStyle
	{
	public:
		[[nodiscard]] auto styleHint(const StyleHint aHint, const QStyleOption* const aOption, const QWidget* const aWidget,
									 QStyleHintReturn* const aReturn) const -> int override
		{
			return aHint == QStyle::SH_ScrollBar_Transient ? 1 : QProxyStyle::styleHint(aHint, aOption, aWidget, aReturn);
		}
		[[nodiscard]] auto pixelMetric(const PixelMetric aMetric, const QStyleOption* const aOption, const QWidget* const aWidget) const
			-> int override
		{
			return aMetric == QStyle::PM_ScrollView_ScrollBarOverlap ? QProxyStyle::pixelMetric(QStyle::PM_ScrollBarExtent, aOption, aWidget)
																	 : QProxyStyle::pixelMetric(aMetric, aOption, aWidget);
		}
	};

	/// \brief [[BPR-1666]]. A transient source scrollbar overlays the text.
	void test_source_overlay_scrollbar_keeps_viewport_extent()
	{
		auto overlay               = overlay_scroll_bar_style{ };
		auto [leftText, rightText] = large_comparison_text(200U, 8U);
		leftText += std::string(250U, 'x') + '\n';
		rightText += std::string(250U, 'x') + '\n';
		const auto diff      = computed(leftText, rightText, { }, 76U);
		auto       workbench = qt::diff_workbench{ };
		workbench.resize(1200, 760);
		for (auto* const source : { workbench.left_pane(), workbench.right_pane() }) {
			source->setStyle(&overlay);
			source->horizontalScrollBar()->setStyle(&overlay);
		}
		workbench.show();
		const auto selected = context(diff, 176U, 176U);
		workbench.set_loading(selected);
		workbench.publish(qt::test::prepare_publication({ .Context = selected, .Result = std::make_shared<const core::computed_file_diff>(diff) }));
		settle_geometry(workbench);
		expect(workbench.select_conversation_line(core::diff_side::right, 62U), "the overlay course opens a conversation line");
		expect(workbench.scroll_controller()->reveal_pane_origin(core::diff_side::right, 1205.0), "the overlay course starts at a fractional source origin");
		settle_geometry(workbench);
		for (auto* const source : { workbench.left_pane(), workbench.right_pane() }) {
			auto* const horizontal = source->horizontalScrollBar();
			expect(horizontal->isVisible() && horizontal->maximum() > 0 && horizontal->style()->styleHint(QStyle::SH_ScrollBar_Transient, nullptr, horizontal) != 0, "a wide source has a transient overlay bar");
			const auto visible   = source->viewport()->mask().boundingRect();
			const auto displayed = QRect{ source->viewport()->mapTo(source, visible.topLeft()), visible.size() };
			const auto bar       = QRect{ horizontal->mapTo(source, QPoint{ }), horizontal->size() };
			std::cout << "SOURCE_OVERLAY viewport=" << displayed.top() << ".." << displayed.bottom() << " bar=" << bar.top() << ".." << bar.bottom() << '\n';
			expect(displayed.bottom() >= bar.bottom(), "source text extends behind the native overlay scrollbar without reserving an opaque band");
		}
		const auto origin = workbench.scroll_controller()->position();
		const auto before = std::array{ workbench.line_rect(core::diff_side::left, 60U), workbench.line_rect(core::diff_side::right, 62U) };
		for (auto* const source : { workbench.left_pane(), workbench.right_pane() }) {
			source->horizontalScrollBar()->hide();
		}
		settle_geometry(workbench);
		expect(close(workbench.scroll_controller()->position(), origin) && before == std::array{ workbench.line_rect(core::diff_side::left, 60U), workbench.line_rect(core::diff_side::right, 62U) }, "hiding overlay bars retains the source origin and row geometry");
		for (auto* const source : { workbench.left_pane(), workbench.right_pane() }) {
			source->horizontalScrollBar()->show();
		}
		settle_geometry(workbench);
		expect(close(workbench.scroll_controller()->position(), origin) && before == std::array{ workbench.line_rect(core::diff_side::left, 60U), workbench.line_rect(core::diff_side::right, 62U) }, "showing overlay bars retains the source origin and row geometry");
		for (auto* const source : { workbench.left_pane(), workbench.right_pane() }) {
			source->horizontalScrollBar()->setStyle(nullptr);
			source->setStyle(nullptr);
		}
	}

	/// \brief [[BPR-1334]]. Line details' rows, gutter and horizontal bar, in
	/// whichever bar style the strip has: the gutter beside the rows only and
	/// not scrolling, and both rows above the bar rather than under it.
	void expect_line_details_bar_band(const qt::line_details_pane& aStrip, const std::string& aLabel)
	{
		const auto* const bar     = aStrip.horizontalScrollBar();
		const auto        barTop  = bar->mapTo(&aStrip, QPoint{ 0, 0 }).y();
		const auto        view    = aStrip.viewport()->geometry();
		const auto        gutter  = aStrip.gutter_widget()->geometry();
		const auto        rowsEnd = view.top() + 2 * aStrip.fontMetrics().lineSpacing();
		std::cout << "bpr-1334 " << aLabel << " bar-top=" << barTop << " viewport=" << view.top() << ".." << view.bottom()
				  << " gutter=" << gutter.top() << ".." << gutter.bottom() << '\n';
		expect(bar->isVisible() && bar->maximum() > 0, aLabel + ": a long row gives the strip its horizontal bar");
		expect(gutter.right() < view.left() && gutter.top() == view.top() && gutter.height() == view.height(),
			   aLabel + ": the gutter sits beside the rows, exactly as tall as they are, outside what scrolls");
		expect(view.bottom() < barTop && gutter.bottom() < barTop,
			   aLabel + ": the rows and the gutter end above the bar, so the bar covers neither");
		expect(rowsEnd <= barTop, aLabel + ": both rows are read above the bar");
	}

	/// \brief [[BPR-1334]]. Line details: the current line, base over head,
	/// full width and wrapped, with its numbers and changed characters.
	///
	/// \details The owner, 2026-10-08: "an optional on by default view page
	/// below the side by side which shows just the current cursor's line as a
	/// full width single pane diff".
	void test_line_details_shows_the_current_line_base_over_head()
	{
		auto words = std::string{ };
		for (auto index = 0; index < 48; ++index) {
			words += "argument_" + std::to_string(index) + ", ";
		}
		const auto longLeft  = "const auto value = compute(" + words + "alpha_tail);";
		const auto longRight = "const auto value = compute(" + words + "omega_tail);";
		const auto huge      = std::string(10'000U, 'x');
		const auto left      = "context-before\n" + longLeft + "\nshared middle line\nthis line is only in base\ncontext-after\n" + huge + "\n";
		const auto right     = "context-before\n" + longRight + "\nshared middle line\ncontext-after\n" + huge + "\nthis line is only in head\n";
		const auto diff      = computed(left, right, { }, 61U);

		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'100, 760);
		workbench.show();
		process_events();
		auto profile = qt::appearance_profile{ };
		workbench.adopt_appearance(profile);
		const auto selected = context(diff, 94U, 94U);
		workbench.set_loading(selected);
		workbench.publish(qt::test::prepare_publication({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		settle_geometry(workbench);

		auto* const strip = workbench.line_details();
		expect(strip != nullptr && strip == workbench.detail_pane() && strip->isVisible() && workbench.line_details_visible(),
			   "Line details is the one strip under the comparison, shown by default");
		// The owner rejected v001's wrapping: "Turn off word wrap, let it have
		// a horizontal scroll if the line is too long."
		expect(strip->lineWrapMode() == QPlainTextEdit::NoWrap
				   && strip->horizontalScrollBarPolicy() == Qt::ScrollBarAsNeeded,
			   "Line details does not wrap; a long row scrolls sideways");
		const auto block = [strip](const int aNumber) { return strip->document()->findBlockByNumber(aNumber); };

		// A changed line: base over head, numbered and marked, wrapped, with
		// the changed characters emphasised on each side.
		expect(workbench.select_conversation_line(core::diff_side::right, 1U), "the long changed line selects");
		process_events();
		expect(strip->document()->blockCount() == 2 && block(0).text() == QString::fromStdString(longLeft)
				   && block(1).text() == QString::fromStdString(longRight),
			   "a changed line shows its base version above its head version, whole");
		expect(strip->gutter_text(0) == QStringLiteral("2 −") && strip->gutter_text(1) == QStringLiteral("2 +"),
			   "each row carries its own line number and its side's mark");
		auto* const bar = strip->horizontalScrollBar();
		expect(block(0).layout()->lineCount() == 1 && block(1).layout()->lineCount() == 1 && bar->isVisible()
				   && bar->maximum() > 0,
			   "a line wider than the strip stays one line, and the strip gains a horizontal bar");
		// The first changed character is in view on open: the change is 'alpha'
		// against 'omega', near the end of a line several screens wide.
		const auto columnX = [strip](const int aBlock, const int aColumn) {
			auto at = QTextCursor{ strip->document()->findBlockByNumber(aBlock) };
			at.setPosition(at.block().position() + aColumn);
			return strip->cursorRect(at).left();
		};
		const auto changeColumn = static_cast<int>(longRight.find("omega"));
		expect(bar->value() > 0 && columnX(1, changeColumn) >= 0
				   && columnX(1, changeColumn) < strip->viewport()->width(),
			   "the strip opens scrolled so the first changed character is in view");
		// One bar for both rows: a column sits at one x on both, before and
		// after a scroll, and the scroll moves both by the same distance.
		auto* const gutter       = strip->gutter_widget();
		const auto  gutterPlace  = gutter->geometry();
		const auto  gutterBefore = gutter->grab().toImage();
		const auto  viewBefore   = strip->viewport()->grab().toImage();
		const auto  scrolledTo   = bar->value();
		const auto  aligned      = columnX(0, changeColumn);
		expect(aligned == columnX(1, changeColumn), "a column sits at the same x on the base row and the head row");
		bar->setValue(0);
		process_events();
		expect(columnX(0, changeColumn) == columnX(1, changeColumn)
				   && columnX(0, changeColumn) - aligned == scrolledTo && columnX(1, 0) == columnX(0, 0),
			   "the two rows scroll together under one bar, so their columns stay aligned");
		// The gutter stays put: its place and its painting are the same
		// scrolled and unscrolled, while the code under the bar moves.
		expect(strip->viewport()->grab().toImage() != viewBefore && gutter->geometry() == gutterPlace
				   && gutter->grab().toImage() == gutterBefore && gutter->geometry().right() < strip->viewport()->geometry().left(),
			   "the line-number and mark gutter does not scroll with the rows");
		expect(gutter->geometry().bottom() < bar->mapTo(strip, QPoint{ 0, 0 }).y(),
			   "the gutter stops above the horizontal bar rather than covering its start");
		const auto emphasised = [strip](const int aBlock, const QColor& aFill) {
			auto texts = QStringList{ };
			for (const auto& selection : strip->extraSelections()) {
				if (!selection.format.boolProperty(QTextFormat::FullWidthSelection) && selection.cursor.blockNumber() == aBlock
					&& selection.format.background().color() == aFill) {
					texts.push_back(selection.cursor.selectedText());
				}
			}
			return texts;
		};
		const auto removed = emphasised(0, profile.color(core::appearance_role::diff_intraline_removed_background));
		const auto added   = emphasised(1, profile.color(core::appearance_role::diff_intraline_added_background));
		expect(!removed.isEmpty() && !added.isEmpty()
				   && std::ranges::all_of(removed, [](const QString& aText) { return QStringLiteral("alpha").contains(aText); })
				   && std::ranges::all_of(added, [](const QString& aText) { return QStringLiteral("omega").contains(aText); }),
			   "only the changed characters are emphasised, in each side's intraline colour");

		// An unchanged line: both versions, no marks, no emphasis.
		expect(workbench.select_conversation_line(core::diff_side::left, 2U), "the unchanged line selects");
		process_events();
		expect(strip->document()->blockCount() == 2 && block(0).text() == QStringLiteral("shared middle line")
				   && block(1).text() == QStringLiteral("shared middle line") && strip->gutter_text(0) == QStringLiteral("3")
				   && strip->gutter_text(1) == QStringLiteral("3") && emphasised(0, profile.color(core::appearance_role::diff_intraline_removed_background)).isEmpty(),
			   "an unchanged line shows both versions without marks or emphasis");
		expect(!strip->first_change_x().has_value() && bar->value() == 0, "a line with no change opens at its start");

		// A removed line and an added line.
		expect(workbench.select_conversation_line(core::diff_side::left, 3U), "the removed line selects");
		process_events();
		expect(block(0).text() == QStringLiteral("this line is only in base") && strip->gutter_text(0) == QStringLiteral("4 −")
				   && block(1).text() == QStringLiteral("Not in head") && strip->gutter_text(1).isEmpty(),
			   "a removed line shows its base row and says it is not in head");
		expect(workbench.select_conversation_line(core::diff_side::right, 5U), "the added line selects");
		process_events();
		expect(block(0).text() == QStringLiteral("Not in base") && block(1).text() == QStringLiteral("this line is only in head")
				   && strip->gutter_text(1) == QStringLiteral("6 +"),
			   "an added line says it is not in base above its head row");

		// A very long line is cut by the core and says how much is left.
		expect(workbench.select_conversation_line(core::diff_side::left, 5U), "the very long line selects");
		process_events();
		const auto marker = QStringLiteral(" … %1 more bytes").arg(QLocale{ }.toString(static_cast<qulonglong>(10'000U - 4'096U)));
		expect(block(0).text() == QString::fromStdString(huge.substr(0U, 4'096U)) + marker
				   && block(1).text().endsWith(marker),
			   "a 10,000-byte line shows its first 4,096 bytes and how many more there are");

		// A mouse selection across lines shows each selected line.
		auto cursor = QTextCursor{ workbench.left_pane()->document()->findBlockByNumber(0) };
		cursor.setPosition(workbench.left_pane()->document()->findBlockByNumber(2).position() + 3, QTextCursor::KeepAnchor);
		workbench.left_pane()->setTextCursor(cursor);
		expect(workbench.select_conversation_line(core::diff_side::left, 0U), "the selection's first line is the current line");
		workbench.left_pane()->setTextCursor(cursor);
		process_events();
		expect(strip->projection().Rows.size() == 6U && strip->gutter_text(0) == QStringLiteral("1")
				   && strip->gutter_text(4) == QStringLiteral("3"),
			   "a selection across three lines shows three pairs");
		expect(std::ranges::all_of(std::views::iota(0, 6), [&block](const int aRow) { return block(aRow).layout()->lineCount() == 1; })
				   && block(2).text() == QString::fromStdString(longLeft),
			   "each row of a selection, the long one too, stays on one line");

		// [[BPR-1212]]'s keyboard line cursor: Down moves the current line and
		// the strip follows; Shift+Down extends it and the strip shows both.
		const auto stroke = [](QWidget& aSurface, const Qt::Key aKey, const Qt::KeyboardModifiers aModifiers) {
			auto shortcut = QKeyEvent{ QEvent::ShortcutOverride, aKey, aModifiers };
			shortcut.ignore();
			QCoreApplication::sendEvent(&aSurface, &shortcut);
			auto down = QKeyEvent{ QEvent::KeyPress, aKey, aModifiers };
			down.ignore();
			const auto taken = QCoreApplication::sendEvent(&aSurface, &down) && down.isAccepted();
			auto       up    = QKeyEvent{ QEvent::KeyRelease, aKey, aModifiers };
			QCoreApplication::sendEvent(&aSurface, &up);
			process_events();
			return taken;
		};
		workbench.left_pane()->setTextCursor(QTextCursor{ workbench.left_pane()->document() });
		expect(workbench.select_conversation_line(core::diff_side::left, 0U), "the cursor starts on the first line");
		workbench.left_pane()->setFocus();
		process_events();
		expect(stroke(*workbench.left_pane(), Qt::Key_Down, Qt::NoModifier), "Down is taken by the focused code pane");
		expect(strip->projection().Rows.size() == 2U && strip->gutter_text(0) == QStringLiteral("2 \u2212")
				   && block(0).text() == QString::fromStdString(longLeft),
			   "Down moves the line cursor and Line details follows it");
		expect(stroke(*workbench.left_pane(), Qt::Key_Down, Qt::ShiftModifier), "Shift+Down is taken");
		expect(strip->projection().Rows.size() == 4U && strip->gutter_text(0) == QStringLiteral("2 \u2212")
				   && strip->gutter_text(2) == QStringLiteral("3"),
			   "Shift+Down extends the selection and Line details shows both lines");

		// Hidden, it asks the core nothing; shown again, it catches up.
		workbench.set_line_details_visible(false);
		process_events();
		const auto hiddenProjections = workbench.statistics().LineDetailsProjections;
		// Line 4 is outside the text selection above, so the strip follows
		// the current line rather than the old selection.
		expect(workbench.select_conversation_line(core::diff_side::left, 3U), "a line selects with the strip hidden");
		process_events();
		expect(!strip->isVisible() && workbench.statistics().LineDetailsProjections == hiddenProjections,
			   "a hidden strip projects nothing");
		workbench.set_line_details_visible(true);
		process_events();
		expect(strip->isVisible() && workbench.statistics().LineDetailsProjections == hiddenProjections + 1U
				   && block(0).text() == QStringLiteral("this line is only in base"),
			   "showing the strip again projects the current line, not an old text selection");

		// The seam: the shared cursor hold, a remembered height, keys and Reset.
		auto* const handle = workbench.line_details_handle();
		expect(handle != nullptr && handle->objectName() == QStringLiteral("lineDetailsResize")
				   && tests::resize_cursor_holds(*handle, Qt::SplitVCursor, "line details seam"),
			   "the seam above Line details holds the vertical resize cursor");
		auto reported = std::vector<std::optional<std::uint32_t>>{ };
		workbench.set_line_details_resize_sink([&reported](const std::optional<std::uint32_t> aHeight) { reported.push_back(aHeight); });
		workbench.set_line_details_height(220U);
		process_events();
		expect(strip->height() == 220 && reported.empty(), "a restored height applies without being reported back");
		const auto line = strip->fontMetrics().lineSpacing();
		handle->setFocus();
		send_key(*handle, Qt::Key_Up);
		process_events();
		expect(reported.size() == 1U && reported.back() == std::optional{ static_cast<std::uint32_t>(220 + line) }
				   && strip->height() == 220 + line,
			   "Up on the seam makes the strip one line taller and reports it");
		workbench.resize(1'100, 900);
		process_events();
		expect(strip->height() == 220 + line && reported.size() == 1U,
			   "the window growing keeps the strip's height and reports nothing");
		auto press = QMouseEvent{ QEvent::MouseButtonDblClick, QPointF{ handle->rect().center() }, handle->mapToGlobal(QPointF{ handle->rect().center() }),
								  Qt::LeftButton, Qt::LeftButton, Qt::NoModifier };
		QApplication::sendEvent(handle, &press);
		process_events();
		expect(reported.size() == 2U && !reported.back().has_value() && !workbench.line_details_height().has_value(),
			   "a double-click resets the strip to its default height");
		// At its smallest the strip still shows a long line's two rows whole,
		// with the horizontal bar below them rather than over the head row.
		expect(workbench.select_conversation_line(core::diff_side::right, 1U), "the long changed line selects again");
		workbench.set_line_details_height(1U);
		process_events();
		expect(bar->isVisible() && strip->viewport()->height() >= 2 * strip->fontMetrics().lineSpacing(),
			   "the smallest strip keeps room for both rows above its horizontal bar");
		expect_line_details_bar_band(*strip, "legacy bar, smallest strip");
		workbench.set_line_details_height(std::nullopt);
		process_events();
		expect_line_details_bar_band(*strip, "legacy bar, default strip");

		// Overlay bars, macOS's default: the bar floats over the viewport's
		// bottom. The strip keeps that band free itself, so the gutter and
		// both rows still end above the bar, at the default and smallest
		// heights. The style changes under a shown strip, as it does when the
		// macOS setting changes.
		auto overlay = overlay_scroll_bar_style{ };
		bar->setStyle(&overlay);
		strip->verticalScrollBar()->setStyle(&overlay);
		process_events();
		expect(bar->style()->styleHint(QStyle::SH_ScrollBar_Transient, nullptr, bar) != 0
				   && bar->style()->pixelMetric(QStyle::PM_ScrollView_ScrollBarOverlap, nullptr, bar) > 0,
			   "the strip's bars take the transient, overlapping style");
		expect_line_details_bar_band(*strip, "overlay bar, default strip");
		workbench.set_line_details_height(1U);
		process_events();
		expect_line_details_bar_band(*strip, "overlay bar, smallest strip");
		workbench.set_line_details_height(std::nullopt);
		bar->setStyle(nullptr);
		strip->verticalScrollBar()->setStyle(nullptr);
		process_events();
		expect_line_details_bar_band(*strip, "legacy bar again");
		expect(strip->viewport()->geometry().bottom() + 1 == bar->mapTo(strip, QPoint{ 0, 0 }).y(),
			   "back on a legacy bar the strip keeps no extra band: the rows run down to the bar");

		if (const auto directory = qEnvironmentVariable("BPR_LINE_DETAILS_CAPTURE_DIR"); !directory.isEmpty()) {
			expect(workbench.select_conversation_line(core::diff_side::right, 1U), "the capture shows the changed line");
			process_events();
			expect(workbench.grab().save(directory + QStringLiteral("/line-details.png"), "PNG"), "the capture is written");
		}
		workbench.hide();
	}

	/// \brief [[BPR-1334]]. Line details colours its rows as the panes colour
	/// the same lines, in light and dark, under the change highlight.
	///
	/// \details The owner rejected v003: "there's no syntax highlighting." A
	/// row's token formats must equal its side's pane's for that line; a
	/// changed token keeps its own colour over the intraline fill; a cut line
	/// is coloured only as far as it is shown.
	void test_line_details_colours_tokens_as_the_panes_do()
	{
		const auto baseLine = std::string{ R"(int total = compute(alpha, "base text"); // sums the inputs)" };
		const auto headLine = std::string{ R"(int total = compute(omega, "base text"); // sums the inputs)" };
		// A long unchanged line of two-byte tokens three bytes apart, so one
		// token straddles the core's 4,096-byte cut (it starts at 4,095).
		auto longLine = std::string{ };
		for (auto index = 0; index < 1'500; ++index) {
			longLine += "kw ";
		}
		const auto left  = "context line\n" + baseLine + "\n" + longLine + "\n";
		const auto right = "context line\n" + headLine + "\n" + longLine + "\n";
		const auto diff  = computed(left, right, { }, 71U);

		using role          = core::appearance_role;
		const auto spansFor = [&longLine](const std::string& aChanged, const std::string_view aToken) {
			auto       spans = application::token_span_list{ };
			const auto at    = [&aChanged](const std::string_view aText) { return static_cast<std::uint32_t>(aChanged.find(aText)); };
			spans.push_back({ .Line = 1U, .Column = at("int"), .Length = 3U, .Role = role::syntax_type });
			spans.push_back({ .Line = 1U, .Column = at("compute"), .Length = 7U, .Role = role::syntax_function });
			spans.push_back({ .Line = 1U, .Column = at(aToken), .Length = 5U, .Role = role::syntax_constant });
			spans.push_back({ .Line = 1U, .Column = at("\"base text\""), .Length = 11U, .Role = role::syntax_string });
			spans.push_back({ .Line = 1U, .Column = at("// sums"), .Length = static_cast<std::uint32_t>(aChanged.size() - aChanged.find("// sums")), .Role = role::syntax_comment });
			for (std::uint32_t column = 0U; column + 1U < longLine.size(); column += 3U) {
				spans.push_back({ .Line = 2U, .Column = column, .Length = 2U, .Role = role::syntax_keyword });
			}
			return spans;
		};

		auto workbench = qt::diff_workbench{ };
		workbench.resize(1'100, 760);
		workbench.show();
		process_events();
		auto light = qt::appearance_profile{ };
		static_cast<void>(light.select_mode(core::appearance_mode::light));
		workbench.adopt_appearance(light);
		const auto selected = context(diff, 95U, 95U);
		workbench.set_loading(selected);
		workbench.publish(qt::test::prepare_publication({
			.Context = selected,
			.Result  = std::make_shared<const core::computed_file_diff>(diff),
		}));
		settle_geometry(workbench);
		auto* const strip = workbench.line_details();
		expect(workbench.select_conversation_line(core::diff_side::right, 1U), "the changed line selects");
		process_events();
		expect(strip->document()->blockCount() == 2 && block_formats(*strip, 0).isEmpty() && block_formats(*strip, 1).isEmpty(),
			   "before any classification the strip colours nothing, as the panes do not");

		// The classification arrives after the line is shown, as it does in
		// the application: the strip colours the rows it already shows.
		expect(workbench.adopt_token_spans(core::diff_side::left, std::make_shared<const application::token_span_list>(spansFor(baseLine, "alpha")))
				   && workbench.adopt_token_spans(core::diff_side::right, std::make_shared<const application::token_span_list>(spansFor(headLine, "omega"))),
			   "both sides adopt their classification");
		process_events();

		const auto expectAsThePanes = [&](const qt::appearance_profile& aProfile, const std::string& aMode) {
			const auto baseRow = block_formats(*strip, 0);
			const auto headRow = block_formats(*strip, 1);
			std::cout << "bpr-1334 syntax " << aMode << " base-ranges=" << baseRow.size() << " head-ranges=" << headRow.size() << '\n';
			expect(!baseRow.isEmpty() && baseRow == block_formats(*workbench.left_pane(), 1),
				   aMode + ": the base row's token colours are the base pane's for that line");
			expect(!headRow.isEmpty() && headRow == block_formats(*workbench.right_pane(), 1),
				   aMode + ": the head row's token colours are the head pane's for that line");
			const auto colourAt = [](const QList<QTextLayout::FormatRange>& aRanges, const int aCharacter) {
				for (const auto& range : aRanges) {
					if (range.start <= aCharacter && aCharacter < range.start + range.length) {
						return range.format.foreground().color();
					}
				}
				return QColor{ };
			};
			const auto compute = static_cast<int>(headLine.find("compute"));
			expect(colourAt(headRow, compute) == aProfile.color(role::syntax_function)
					   && colourAt(baseRow, static_cast<int>(baseLine.find("\"base"))) == aProfile.color(role::syntax_string),
				   aMode + ": a function and a string read in their palette colours");

			// The changed characters: the intraline fill as a background, and
			// the token's own colour kept over it (readable, as the panes keep it).
			const auto fill   = aProfile.color(role::diff_intraline_added_background);
			const auto normal = aProfile.color(role::syntax_normal);
			const auto token  = aProfile.color(role::syntax_constant);
			expect(token != normal, aMode + ": the changed token's colour is not the plain ink, so keeping it shows");
			const auto readable = [&](const QColor& aInk) {
				return qt::to_presentation_color(
					core::readable_emphasis_foreground(qt::to_appearance_color(aInk), qt::to_appearance_color(fill), aProfile.variant()));
			};
			auto marked = 0;
			for (const auto& emphasis : strip->extraSelections()) {
				if (emphasis.cursor.blockNumber() != 1 || emphasis.format.background().color() != fill) {
					continue;
				}
				const auto tokenBegin = static_cast<int>(headLine.find("omega"));
				for (auto at = emphasis.cursor.selectionStart() - emphasis.cursor.block().position();
					 at < emphasis.cursor.selectionEnd() - emphasis.cursor.block().position(); ++at) {
					if (at < tokenBegin || at >= tokenBegin + 5) {
						continue;
					}
					// What paints a character is the last selection over it that
					// sets a foreground.
					auto ink = QColor{ };
					for (const auto& over : strip->extraSelections()) {
						const auto position = emphasis.cursor.block().position() + at;
						if (over.cursor.selectionStart() <= position && position < over.cursor.selectionEnd()
							&& over.format.foreground().style() != Qt::NoBrush) {
							ink = over.format.foreground().color();
						}
					}
					expect(ink == readable(token) && ink != readable(normal),
						   aMode + ": a changed character of a token keeps the token's colour over the change highlight");
					++marked;
				}
			}
			expect(marked > 0, aMode + ": the changed token is highlighted on the head row");
		};
		expectAsThePanes(light, "light");

		// A theme change recolours the shown rows from the new palette.
		auto dark = qt::appearance_profile{ };
		static_cast<void>(dark.select_mode(core::appearance_mode::dark));
		workbench.adopt_appearance(dark);
		process_events();
		expect(dark.color(role::syntax_function) != light.color(role::syntax_function), "dark has its own function colour");
		expectAsThePanes(dark, "dark");

		// A cut line is coloured only as far as it is shown: the token across
		// the cut ends there, and none is past it.
		expect(workbench.select_conversation_line(core::diff_side::left, 2U), "the long line selects");
		process_events();
		const auto pane = block_formats(*workbench.left_pane(), 2);
		const auto cut  = block_formats(*strip, 0);
		const auto kept = std::ranges::count_if(pane, [](const QTextLayout::FormatRange& aRange) { return aRange.start < 4'096; });
		std::cout << "bpr-1334 syntax cut pane-ranges=" << pane.size() << " strip-ranges=" << cut.size() << '\n';
		expect(pane.size() == 1'500 && cut.size() == kept && kept == 1'366,
			   "the strip colours the tokens it shows, and none past the 4,096-byte cut");
		expect(std::ranges::equal(cut.mid(0, kept - 1), pane.mid(0, kept - 1)) && cut.back().start == 4'095 && cut.back().length == 1
				   && cut.back().format == pane.at(kept - 1).format && block_formats(*strip, 1) == cut,
			   "each shown token reads as in the pane, and the one across the cut ends at it");
		workbench.hide();
	}
} // namespace

namespace {
	/// [[BPR-1212]]. One key as a keyboard delivers it to the focused
	/// widget: the shortcut pass, then the press. True when the press was
	/// taken.
	auto press_key(QWidget& aSurface, const Qt::Key aKey, const Qt::KeyboardModifiers aModifiers = Qt::NoModifier) -> bool
	{
		auto shortcut = QKeyEvent{ QEvent::ShortcutOverride, aKey, aModifiers };
		shortcut.ignore();
		QCoreApplication::sendEvent(&aSurface, &shortcut);
		auto press = QKeyEvent{ QEvent::KeyPress, aKey, aModifiers };
		press.ignore();
		const auto taken   = QCoreApplication::sendEvent(&aSurface, &press) && press.isAccepted();
		auto       release = QKeyEvent{ QEvent::KeyRelease, aKey, aModifiers };
		QCoreApplication::sendEvent(&aSurface, &release);
		process_events();
		return taken;
	}

	[[nodiscard]] auto line_range_of(const qt::diff_workbench& aWorkbench) -> std::optional<std::pair<std::uint32_t, std::uint32_t>>
	{
		const auto range = aWorkbench.line_draft_range();
		if (!range) {
			return std::nullopt;
		}
		return std::pair{ range->first.Line, range->second.Line };
	}

	void test_the_keyboard_line_cursor_and_return()
	{
		// Two hundred lines with a change in the middle, taller than the view.
		auto left  = std::string{ };
		auto right = std::string{ };
		for (auto line = 0; line < 200; ++line) {
			left += "line " + std::to_string(line) + "\n";
			right += (line >= 100 && line < 104 ? "changed " : "line ") + std::to_string(line) + "\n";
		}
		const auto diff      = computed(left, right, { }, 90U);
		auto       workbench = qt::diff_workbench{ };
		auto       drafts    = std::vector<bool>{ };
		workbench.resize(1'000, 520);
		workbench.show();
		const auto request = context(diff, 190U, 190U);
		workbench.set_loading(request);
		workbench.publish(qt::test::prepare_publication({ .Context = request, .Result = std::make_shared<const core::computed_file_diff>(diff) }));
		settle_geometry(workbench);
		auto* const rightPane = workbench.right_pane();
		auto* const leftPane  = workbench.left_pane();

		for (auto* const pane : { leftPane, rightPane, static_cast<QPlainTextEdit*>(workbench.findChild<QPlainTextEdit*>(QStringLiteral("diffUnifiedSource"))) }) {
			const auto description = pane->accessibleDescription();
#if defined(Q_OS_MACOS)
			const auto enter = QStringLiteral("Return");
#else
			const auto enter = QStringLiteral("Enter");
#endif
			expect(description.contains(QStringLiteral("Up and Down move the selected line")) && description.contains(QStringLiteral("with Shift they extend"))
					   && description.contains(enter + QStringLiteral(" comments")) && description.contains(QStringLiteral("Shift+") + enter + QStringLiteral(" suggests")),
				   "each code pane's accessible description names Up, Down, Shift, Return and Shift+Return");
		}

		// Without a sink, Return is the pane's own and is not taken.
		rightPane->setFocus(Qt::OtherFocusReason);
		expect(!press_key(*rightPane, Qt::Key_Return), "with nothing to start a draft, Return stays the pane's");
		workbench.set_line_draft_dispatch([&drafts](const bool aSuggest) { drafts.push_back(aSuggest); });

		expect(workbench.reveal_conversation_line(core::diff_side::right, 20U), "a line is selected and in view");
		settle_geometry(workbench);
		const auto topBefore     = rightPane->verticalScrollBar()->value();
		auto       announcements = std::vector<accessible_announcement>{ };
		{
			const auto capture = scoped_accessibility_capture{ announcements };
			expect(press_key(*rightPane, Qt::Key_Down), "Down is taken by the focused code pane");
		}
		expect(workbench.selection() && workbench.selection()->PreferredLine.Side == core::diff_side::right && workbench.selection()->PreferredLine.Line == 21U,
			   "Down moves the selected line by one");
		expect(!rightPane->textCursor().hasSelection() && line_range_of(workbench) == std::pair{ 21U, 21U }, "a plain move selects one line");
		expect(rightPane->verticalScrollBar()->value() == topBefore, "a move inside the view does not scroll");
		expect(announcement_count(announcements, &workbench, QStringLiteral("Line 22 of the later version selected")) == 1U, "the selected line is announced once, politely");

		expect(press_key(*rightPane, Qt::Key_Down, Qt::ShiftModifier) && press_key(*rightPane, Qt::Key_Down, Qt::ShiftModifier), "Shift+Down is taken");
		expect(workbench.selection()->PreferredLine.Line == 23U && line_range_of(workbench) == std::pair{ 21U, 23U },
			   "Shift+Down extends the selection; the cursor is its moving end");
		expect(rightPane->textCursor().hasSelection() && rightPane->textCursor().selectedText().contains(QStringLiteral("line 21")) && rightPane->textCursor().selectedText().contains(QStringLiteral("line 23")),
			   "the extended lines wear the pane's own text selection");
		expect(workbench.last_announcement() == QStringLiteral("Lines 22 to 24 selected"), "the range is announced");
		for (auto step = 0; step < 3; ++step) {
			expect(press_key(*rightPane, Qt::Key_Up, Qt::ShiftModifier), "Shift+Up is taken");
		}
		expect(workbench.selection()->PreferredLine.Line == 20U && line_range_of(workbench) == std::pair{ 20U, 21U }, "Shift+Up past the anchor extends the other way");
		expect(press_key(*rightPane, Qt::Key_Down) && line_range_of(workbench) == std::pair{ 21U, 21U } && !rightPane->textCursor().hasSelection(),
			   "Down without Shift collapses to the next line");

		// Return and Shift+Return, keypad Enter, and no other modifier.
		drafts.clear();
		expect(press_key(*rightPane, Qt::Key_Return) && press_key(*rightPane, Qt::Key_Return, Qt::ShiftModifier)
				   && press_key(*rightPane, Qt::Key_Enter, Qt::KeypadModifier) && press_key(*rightPane, Qt::Key_Enter, Qt::KeypadModifier | Qt::ShiftModifier),
			   "Return, Shift+Return and keypad Enter are taken by the focused pane");
		expect(drafts == std::vector<bool>{ false, true, false, true }, "Return asks for a comment and Shift+Return for a suggestion");
		static_cast<void>(press_key(*rightPane, Qt::Key_Return, Qt::ControlModifier));
		static_cast<void>(press_key(*rightPane, Qt::Key_Return, Qt::AltModifier));
		static_cast<void>(press_key(*rightPane, Qt::Key_Down, Qt::AltModifier));
		expect(drafts.size() == 4U && workbench.selection()->PreferredLine.Line == 21U, "no other modifier is taken");
		auto* const detail = workbench.detail_pane();
		detail->setFocus(Qt::OtherFocusReason);
		static_cast<void>(press_key(*detail, Qt::Key_Return));
		static_cast<void>(press_key(*workbench.link_map(), Qt::Key_Return));
		expect(drafts.size() == 4U, "Return in the detail pane or the link map starts nothing");

		// The mouse and the keyboard share one selection.
		const auto rowCentre = [rightPane](const int aLine) {
			return rightPane->cursorRect(QTextCursor{ rightPane->document()->findBlockByNumber(aLine) }).center();
		};
		expect(workbench.reveal_conversation_line(core::diff_side::right, 24U), "the lines to click are in view");
		settle_geometry(workbench);
		send_click(*rightPane->viewport(), rowCentre(25));
		process_events();
		if (const auto range = line_range_of(workbench); workbench.selection()->PreferredLine.Line != 25U || range != std::pair{ 25U, 25U }) {
			std::cerr << "click: preferred=" << workbench.selection()->PreferredLine.Line << " range=" << (range ? range->first : 9999U) << "-" << (range ? range->second : 9999U)
					  << " point=" << rowCentre(25).x() << "," << rowCentre(25).y() << '\n';
		}
		expect(workbench.selection()->PreferredLine.Line == 25U && line_range_of(workbench) == std::pair{ 25U, 25U }, "a click selects its line");
		auto shiftClick = QMouseEvent{ QEvent::MouseButtonPress, rowCentre(28), rowCentre(28), rightPane->viewport()->mapToGlobal(rowCentre(28)), Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier };
		QCoreApplication::sendEvent(rightPane->viewport(), &shiftClick);
		auto shiftRelease = QMouseEvent{ QEvent::MouseButtonRelease, rowCentre(28), rowCentre(28), rightPane->viewport()->mapToGlobal(rowCentre(28)), Qt::LeftButton, Qt::NoButton, Qt::ShiftModifier };
		QCoreApplication::sendEvent(rightPane->viewport(), &shiftRelease);
		process_events();
		expect(line_range_of(workbench) == std::pair{ 25U, 28U }, "Shift-click extends the same selection");
		rightPane->setFocus(Qt::OtherFocusReason);
		expect(press_key(*rightPane, Qt::Key_Down, Qt::ShiftModifier) && line_range_of(workbench) == std::pair{ 25U, 29U },
			   "Shift+Down extends what the mouse selected");
		expect(workbench.select_conversation_line(core::diff_side::right, 60U) && line_range_of(workbench) == std::pair{ 60U, 60U },
			   "a selection left elsewhere on the pane is not the selected line's");

		// The cursor keeps itself in view through the linked scroll, both ways.
		expect(workbench.select_conversation_line(core::diff_side::right, 10U), "back near the top");
		for (auto step = 0; step < 120; ++step) {
			static_cast<void>(press_key(*rightPane, Qt::Key_Down));
			expect(workbench.source_line_visible(core::diff_side::right, workbench.selection()->PreferredLine.Line), "Down keeps the selected line in view");
		}
		expect(workbench.selection()->PreferredLine.Line == 130U && rightPane->verticalScrollBar()->value() > topBefore, "the view followed the cursor down");
		const auto leftTop  = leftPane->cursorForPosition(QPoint{ 0, 0 }).blockNumber();
		const auto rightTop = rightPane->cursorForPosition(QPoint{ 0, 0 }).blockNumber();
		expect(std::abs(leftTop - rightTop) <= 4, "the panes stay linked while the cursor scrolls them");
		for (auto step = 0; step < 140; ++step) {
			static_cast<void>(press_key(*rightPane, Qt::Key_Up));
		}
		expect(workbench.selection()->PreferredLine.Line == 0U && workbench.source_line_visible(core::diff_side::right, 0U), "Up stops at the first line, in view");
		expect(workbench.select_conversation_line(core::diff_side::right, 199U) && press_key(*rightPane, Qt::Key_Down)
				   && workbench.selection()->PreferredLine.Line == 199U,
			   "Down stops at the last line");

		// The left pane moves its own side.
		leftPane->setFocus(Qt::OtherFocusReason);
		expect(press_key(*leftPane, Qt::Key_Down), "the left pane takes Down");
		expect(workbench.selection()->PreferredLine.Side == core::diff_side::left, "with the left pane focused, Down selects a left line");
		expect(press_key(*leftPane, Qt::Key_Down, Qt::ShiftModifier) && line_range_of(workbench).has_value()
				   && workbench.line_draft_range()->first.Side == core::diff_side::left && workbench.line_draft_range()->second.Side == core::diff_side::left,
			   "a left range stays on the left, where the core refuses it");

		// One pane: rows, each end on its own side.
		workbench.set_one_pane_enabled(true);
		process_events();
		auto* const unified = workbench.findChild<QPlainTextEdit*>(QStringLiteral("diffUnifiedSource"));
		expect(workbench.select_conversation_line(core::diff_side::right, 90U), "a right line in one pane");
		unified->setFocus(Qt::OtherFocusReason);
		expect(press_key(*unified, Qt::Key_Down, Qt::ShiftModifier) && press_key(*unified, Qt::Key_Down, Qt::ShiftModifier),
			   "the one-pane view takes Shift+Down");
		const auto oneRange = workbench.line_draft_range();
		expect(oneRange && oneRange->first == core::source_line_ref{ diff.Alignment.Digest, core::diff_side::right, 90U } && oneRange->second.Line == 92U,
			   "one pane's rows extend the same way");
		drafts.clear();
		expect(press_key(*unified, Qt::Key_Return, Qt::ShiftModifier) && drafts == std::vector<bool>{ true }, "Shift+Return in one pane asks for a suggestion");
		workbench.set_one_pane_enabled(false);
		process_events();

		// With no line selected Return still asks; the window says how to select one.
		workbench.clear();
		process_events();
		drafts.clear();
		rightPane->setFocus(Qt::OtherFocusReason);
		static_cast<void>(press_key(*rightPane, Qt::Key_Return));
		expect(drafts == std::vector<bool>{ false } && !workbench.line_draft_range(), "with nothing selected the request comes with no range");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setApplicationName(QStringLiteral("BetterPullReviewDiffWorkbenchTests"));
	if (argc > 2 && std::string_view{ argv[1] } == "--render-selection-looks") {
		render_selection_looks(QString::fromLocal8Bit(argv[2]));
		return EXIT_SUCCESS;
	}
	if (argc > 1 && std::string_view{ argv[1] } == "--selection-only") {
		// One of them by name, or all three.
		const auto only = argc > 2 ? std::string_view{ argv[2] } : std::string_view{ };
		if (only.empty() || only == "line-details") {
			test_line_details_keeps_the_syntax_colours_under_a_selection();
		}
		if (only.empty() || only == "syntax") {
			test_a_selection_keeps_the_syntax_colours();
		}
		if (only.empty() || only == "line-kinds") {
			test_a_text_selection_reads_on_every_line_kind();
		}
		return EXIT_SUCCESS;
	}
	if (argc > 1 && std::string_view{ argv[1] } == "--source-overlay-only") {
		test_source_overlay_scrollbar_keeps_viewport_extent();
		return EXIT_SUCCESS;
	}
	test_source_overlay_scrollbar_keeps_viewport_extent();

	test_divider_drag_keeps_fractional_viewport();
	if (argc > 1 && std::string_view{ argv[1] } == "--divider-resize-only") {
		return EXIT_SUCCESS;
	}
	test_fractional_native_viewport();
	test_fractional_unified_and_keyboard_viewports();
	test_keyboard_minimal_reveal();
	test_keyboard_reveal_from_far_out_of_view();
	test_one_pane_round_trip_keeps_an_unselected_empty_side_anchor();
	test_one_pane_rows_anchors_and_link_map();
	{
		auto workbench = qt::diff_workbench{ };
		expect(workbench.findChild<QPlainTextEdit*>(QStringLiteral("diffUnifiedSource")) != nullptr,
			   "one-pane comparison has a dedicated unified source surface");
	}
	test_the_panes_number_their_own_lines();
	test_a_lone_cr_is_shown_inside_its_line();
	test_white_space_design_captures();
	test_the_viewport_fill_survives_a_style_clearing_it();
	test_the_gutter_sizes_to_its_longest_number();
	test_hiding_the_numbers_returns_the_gutter_width_to_the_code();
	test_tinted_numbers_and_the_add_button();
	test_selected_composition();
	test_nontext_publication_status();
	test_semantic_selection_callback_tracks_both_panes_and_clear();
	test_conversation_anchor_reveals_and_paints_exact_line();
	test_prepared_semantic_index_adoption();
	test_large_publication_fills_over_bounded_turns();
	test_measurement_yields_and_cancels();
	test_scale_and_high_frequency_evidence();
	test_line_painting_stays_bounded_by_the_viewport();
	test_syntax_spans_colour_the_panes();
	test_a_selection_keeps_the_syntax_colours();
	test_a_text_selection_reads_on_every_line_kind();
	test_tabbed_source_correspondence_survives_syntax_and_tab_width();
	test_comparison_zoom_scales_and_holds_the_reading_line();
	test_code_zoom_design_captures();
	test_syntax_formatting_stays_bounded_by_the_viewport();
	test_moved_blocks_carry_the_moved_role();
	test_intraline_ranges_match_the_model();
	test_intraline_ranges_are_painted_on_the_removed_side();
	test_group_boundaries_are_painted();
	test_link_map_endpoints_meet_their_blocks();
	test_the_panes_scroll_past_their_end_and_the_bar_agrees();
	test_selected_rows_derive_their_intraline_fill();
	test_dark_intraline_preserves_readable_syntax_hues();
	test_line_details_keeps_the_syntax_colours_under_a_selection();
	test_group_selection_washes_every_line();
	test_the_reading_line_wears_the_current_line_band();
	test_the_keyboard_line_cursor_and_return();
	test_line_details_shows_the_current_line_base_over_head();
	test_line_details_colours_tokens_as_the_panes_do();
	std::cout << "diff-workbench-tests:pass\n";
	return EXIT_SUCCESS;
}
