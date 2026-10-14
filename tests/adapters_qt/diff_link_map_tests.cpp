#include "bpr/core/diff_compute.hpp"
#include "bpr/qt/comparison_scroll_controller.hpp"
#include "bpr/qt/diff_link_map.hpp"
#include "bpr/qt/measured_alignment_axis.hpp"
#include "product_diff.hpp"

#include <QAccessible>
#include <QApplication>
#include <QHBoxLayout>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPointF>
#include <QScrollBar>
#include <QSplitter>
#include <QString>
#include <QTextDocument>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
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
		const qreal aActual,
		const qreal aExpected,
		const qreal aTolerance = 0.000'001) noexcept -> bool
	{
		return std::abs(aActual - aExpected) <= aTolerance;
	}

	template <typename Identity>
	[[nodiscard]] constexpr auto opaque(const std::uint8_t aSeed) -> Identity
	{
		std::array<std::uint8_t, 32> bytes{ };
		bytes.front() = aSeed;
		bytes.back()  = static_cast<std::uint8_t>(aSeed ^ 0xA5U);
		return Identity{ bytes };
	}

	[[nodiscard]] constexpr auto blob(const std::uint8_t aSeed)
		-> core::git_blob_oid
	{
		std::array<std::uint8_t, 32> bytes{ };
		for (std::size_t index = 0U; index < 20U; ++index) {
			bytes[index] = static_cast<std::uint8_t>(
				aSeed + static_cast<std::uint8_t>(index));
		}
		return core::git_blob_oid{ core::git_hash_algorithm::sha1, bytes };
	}

	[[nodiscard]] auto comparison_key(
		const core::diff_options& aOptions,
		const std::uint8_t        aIdentitySeed = 0U)
		-> core::file_comparison_key
	{
		const auto digest = core::options_digest(aOptions);
		expect(digest.has_value(), "fixture options are normalized");
		const auto scope
			= opaque<core::pull_request_authority_scope_id>(11U);
		const auto incarnation
			= opaque<core::epoch_catalog_incarnation_id>(12U);
		const auto authority = core::pull_request_authority{
			.Profile         = core::host_profile_id{ 10U },
			.ProfileRevision = core::github_profile_revision{ 20U },
			.Host            = core::git_host_id{ 30U },
			.HostAuthority
			= opaque<core::host_authority_fingerprint>(13U),
			.Account     = core::github_account_id{ 40U },
			.Viewer      = core::github_viewer_actor_id{ 50U },
			.Repository  = core::repository_id{ 101U },
			.Cache       = opaque<core::repository_cache_scope_id>(14U),
			.PullRequest = core::pull_request_id{ 202U },
			.Number      = core::pull_request_number{ 17U },
			.Scope       = scope,
		};
		return {
			.Authority   = authority,
			.Repository  = authority.Repository,
			.PullRequest = authority.PullRequest,
			.LeftAnchor  = {
				.Authority   = scope,
				.Incarnation = incarnation,
				.Kind        = core::comparison_anchor_kind::epoch_base,
				.Epoch       = core::epoch_ordinal{ 1 },
			},
			.RightAnchor = {
				.Authority   = scope,
				.Incarnation = incarnation,
				.Kind        = core::comparison_anchor_kind::epoch_head,
				.Epoch       = core::epoch_ordinal{ 2 },
			},
			.LeftBlob  = blob(static_cast<std::uint8_t>(21U + aIdentitySeed)),
			.RightBlob = blob(static_cast<std::uint8_t>(51U + aIdentitySeed)),
			.File      = opaque<core::file_identity>(static_cast<std::uint8_t>(31U + aIdentitySeed)),
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
		const std::string_view aLeft,
		const std::string_view aRight,
		core::diff_options     aOptions      = { },
		const std::uint8_t     aIdentitySeed = 0U) -> core::computed_file_diff
	{
		auto       left    = bytes(aLeft);
		auto       right   = bytes(aRight);
		const auto outcome = bpr::tests::product_file_diff({
			.Key        = comparison_key(aOptions, aIdentitySeed),
			.Options    = aOptions,
			.LeftBytes  = std::span<const std::uint8_t>{ left },
			.RightBytes = std::span<const std::uint8_t>{ right },
		});
		expect(outcome.Reason == core::diff_reason_code::none,
			   "fixture diff computes");
		expect(outcome.Result.has_value(), "fixture diff publishes");
		expect(core::validate_computed_file_diff(*outcome.Result).valid(),
			   "fixture diff validates");
		return *outcome.Result;
	}

	[[nodiscard]] auto uniform_measurements(
		const std::uint32_t aLines,
		const qreal         aLineExtent,
		const qreal         aViewport) -> qt::pane_line_measurements
	{
		auto boundaries = std::vector<qreal>{ };
		boundaries.reserve(static_cast<std::size_t>(aLines) + 1U);
		for (std::uint32_t line = 0U; line <= aLines; ++line) {
			boundaries.push_back(static_cast<qreal>(line) * aLineExtent);
		}
		return {
			.Boundaries       = std::move(boundaries),
			.DocumentExtent   = static_cast<qreal>(aLines) * aLineExtent,
			.ViewportExtent   = aViewport,
			.DevicePixelRatio = 1.0,
		};
	}

	[[nodiscard]] auto frame_for(
		const qt::measured_alignment_axis& aAxis,
		const qreal                        aLeftOrigin,
		const qreal                        aRightOrigin)
		-> qt::comparison_scroll_frame
	{
		return {
			.Comparison = aAxis.comparison(),
			.Left       = {
				.DocumentOriginY = aLeftOrigin,
			},
			.Right = {
				.DocumentOriginY = aRightOrigin,
			},
		};
	}

	[[nodiscard]] auto point_inside(const qt::visible_diff_link& aLink)
		-> QPointF
	{
		return {
			aLink.Path.boundingRect().center().x(),
			(aLink.LeftTop + aLink.LeftBottom + aLink.RightTop
			 + aLink.RightBottom)
				/ 4.0,
		};
	}

	[[nodiscard]] auto link_of_kind(
		const qt::diff_link_map&   aMap,
		const core::alignment_kind aKind) -> const qt::visible_diff_link&
	{
		const auto links = aMap.visible_links();
		const auto found = std::ranges::find_if(
			links,
			[aKind](const auto& aLink) { return aLink.Kind == aKind; });
		expect(found != links.end(), "expected visible link kind exists");
		return *found;
	}

	void send_mouse(
		QWidget&               aTarget,
		const QEvent::Type     aType,
		const QPointF&         aLocal,
		const Qt::MouseButton  aButton,
		const Qt::MouseButtons aButtons)
	{
		const auto global = QPointF{ aTarget.mapToGlobal(aLocal.toPoint()) };
		auto       event  = QMouseEvent{
			aType,
			aLocal,
			aLocal,
			global,
			aButton,
			aButtons,
			Qt::NoModifier,
		};
		QApplication::sendEvent(&aTarget, &event);
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

	struct live_harness
	{
		QWidget                          Surface;
		QHBoxLayout                      Layout{ &Surface };
		qt::diff_comparison_splitter     Splitter;
		QPlainTextEdit*                  Left  = new QPlainTextEdit{ };
		QPlainTextEdit*                  Right = new QPlainTextEdit{ };
		QScrollBar                       Shared{ Qt::Vertical };
		qt::comparison_scroll_controller Controller{ Shared };
		core::computed_file_diff         Diff;
		qt::diff_link_map*               Map = nullptr;

		live_harness(const std::string_view aLeft, const std::string_view aRight)
			: Diff{ computed(aLeft, aRight) }
		{
			Left->setLineWrapMode(QPlainTextEdit::NoWrap);
			Right->setLineWrapMode(QPlainTextEdit::NoWrap);
			Left->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			Right->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
			Left->setPlainText(QString::fromUtf8(aLeft.data(),
												 static_cast<qsizetype>(aLeft.size())));
			Right->setPlainText(QString::fromUtf8(aRight.data(),
												  static_cast<qsizetype>(aRight.size())));
			Left->setAccessibleName(QStringLiteral("Earlier source"));
			Right->setAccessibleName(QStringLiteral("Later source"));
			Splitter.addWidget(Left);
			Splitter.addWidget(Right);
			Splitter.setSizes({ 470, 470 });
			Layout.setContentsMargins(12, 12, 12, 12);
			Layout.setSpacing(0);
			Layout.addWidget(&Splitter, 1);
			Layout.addWidget(&Shared);
			Surface.resize(1'100, 480);
			Surface.show();
			QApplication::processEvents();
			Map = Splitter.link_map();
			expect(Map != nullptr, "splitter creates one DiffLinkMap handle");

			const auto leftGeometry = qt::measure_plain_text_pane(
				*Left, Diff.Alignment.LeftLineCount);
			const auto rightGeometry = qt::measure_plain_text_pane(
				*Right, Diff.Alignment.RightLineCount);
			expect(leftGeometry.has_value() && rightGeometry.has_value(),
				   "live natural-layout panes measure");
			auto built = qt::build_measured_alignment_axis(
				Diff.Alignment, *leftGeometry, *rightGeometry);
			expect(built.valid(), "live link-map axis builds");

			Controller.set_frame_sink([this](const auto& aFrame) {
				static_cast<void>(Map->set_frame(aFrame));
			});
			Controller.set_repaint_sink([this]() { Map->update(); });
			Controller.bind_input_surface(
				qt::comparison_scroll_receiver::left_pane,
				*Left->viewport(), Left->horizontalScrollBar());
			Controller.bind_input_surface(
				qt::comparison_scroll_receiver::right_pane,
				*Right->viewport(), Right->horizontalScrollBar());
			Controller.bind_input_surface(
				qt::comparison_scroll_receiver::link_map, *Map);
			Controller.bind_key_surface(
				qt::comparison_scroll_receiver::link_map, *Map);
			Map->set_geometry(*built.Axis, Diff.Alignment.Moves);
			Controller.set_axis(std::move(*built.Axis));
			QApplication::processEvents();
		}
	};

	auto test_rendered_geometry_selection_and_native_drag() -> void
	{
		auto leftText = std::string{
			"header\nold-a\nold-b\nkeep-one\ndelete-a\ndelete-b\nkeep-two\n"
		};
		auto rightText = std::string{
			"header\nnew-a\nnew-b\nnew-c\nkeep-one\nkeep-two\ninsert-a\ninsert-b\n"
		};
		for (std::size_t line = 0U; line < 40U; ++line) {
			const auto common = "common-tail-" + std::to_string(line) + "\n";
			leftText += common;
			rightText += common;
		}
		leftText += "late-old-a\nlate-old-b\nend\n";
		rightText += "late-new-a\nlate-new-b\nlate-new-c\nend\n";
		auto  harness = live_harness{ leftText, rightText };
		auto& map     = *harness.Map;

		const auto expectedScale = qEnvironmentVariable("BPR_EXPECTED_QT_SCALE");
		if (!expectedScale.isEmpty()) {
			const auto scale = expectedScale.toDouble();
			expect(close(map.devicePixelRatioF(), scale, 0.05),
				   "rendered link map observes the selected Qt scale");
		}
		const auto links = map.visible_links();
		expect(!links.empty(), "live selected composition retains visible links");
		expect(std::ranges::none_of(
				   links,
				   [](const auto& aLink) {
					   return aLink.Kind == core::alignment_kind::equal;
				   }),
			   "equal segments do not create painted ribbons");

		const auto& replacement = link_of_kind(
			map, core::alignment_kind::replacement);
		const auto& insertion = link_of_kind(
			map, core::alignment_kind::right_only);
		const auto& deletion = link_of_kind(
			map, core::alignment_kind::left_only);
		expect(replacement.LeftBottom > replacement.LeftTop
				   && replacement.RightBottom > replacement.RightTop,
			   "replacement opens on both pane edges");
		expect(close(insertion.LeftTop, insertion.LeftBottom)
				   && insertion.RightBottom > insertion.RightTop,
			   "insertion collapses only the left edge into a wedge");
		expect(close(deletion.RightTop, deletion.RightBottom)
				   && deletion.LeftBottom > deletion.LeftTop,
			   "deletion collapses only the right edge into a wedge");
		for (const auto& link : links) {
			expect(close(link.Path.elementAt(0).x, 0.0),
				   "every ribbon starts at the left pane edge");
			expect(close(link.Path.elementAt(3).x, map.width()),
				   "every ribbon upper boundary meets the right pane edge");
			const auto point = point_inside(link);
			expect(link.Path.contains(point),
				   "representative interior point is inside the filled interval");
			expect(map.hit_test(point) == link.Identity,
				   "path-derived hit test returns exact stable identity");
		}

		auto selected = std::vector<qt::diff_link_map_identity>{ };
		auto revealed = std::vector<qt::diff_link_map_identity>{ };
		auto hovered  = std::vector<std::optional<qt::diff_link_map_identity>>{ };
		map.set_selection_sink(
			[&selected](const auto& aIdentity) { selected.push_back(aIdentity); });
		map.set_reveal_sink(
			[&revealed](const auto& aIdentity) { revealed.push_back(aIdentity); });
		map.set_hover_sink(
			[&hovered](const auto& aIdentity) { hovered.push_back(aIdentity); });

		const auto clickIdentity    = replacement.Identity;
		const auto clickPoint       = point_inside(replacement);
		const auto sizesBeforeClick = harness.Splitter.sizes();
		send_mouse(
			map, QEvent::MouseButtonPress, clickPoint,
			Qt::LeftButton, Qt::LeftButton);
		const auto belowThreshold = clickPoint + QPointF{
			static_cast<qreal>(std::max(0, QApplication::startDragDistance() - 1)),
			0.0,
		};
		send_mouse(
			map, QEvent::MouseMove, belowThreshold,
			Qt::NoButton, Qt::LeftButton);
		send_mouse(
			map, QEvent::MouseButtonRelease, belowThreshold,
			Qt::LeftButton, Qt::NoButton);
		expect(selected.size() == 1U && selected.back() == clickIdentity
				   && map.selected() == clickIdentity,
			   "sub-threshold click selects the exact semantic link");
		expect(harness.Splitter.sizes() == sizesBeforeClick,
			   "sub-threshold pointer motion does not resize the splitter");

		const auto selectionCount  = selected.size();
		const auto sizesBeforeDrag = harness.Splitter.sizes();
		send_mouse(
			map, QEvent::MouseButtonPress, clickPoint,
			Qt::LeftButton, Qt::LeftButton);
		const auto beyondThreshold = clickPoint + QPointF{ static_cast<qreal>(QApplication::startDragDistance() + 28), 0.0 };
		send_mouse(
			map, QEvent::MouseMove, beyondThreshold,
			Qt::NoButton, Qt::LeftButton);
		send_mouse(
			map, QEvent::MouseButtonRelease, beyondThreshold,
			Qt::LeftButton, Qt::NoButton);
		QApplication::processEvents();
		expect(harness.Splitter.sizes() != sizesBeforeDrag,
			   "movement beyond the platform drag threshold resizes panes");
		expect(selected.size() == selectionCount,
			   "splitter drag does not accidentally select a ribbon");

		const auto currentReplacement = link_of_kind(
			map, core::alignment_kind::replacement);
		const auto currentPoint = point_inside(currentReplacement);
		send_mouse(
			map, QEvent::MouseMove, currentPoint,
			Qt::NoButton, Qt::NoButton);
		expect(map.hovered() == currentReplacement.Identity
				   && !hovered.empty() && hovered.back() == currentReplacement.Identity,
			   "hover publishes the stable visible-link identity");
		send_mouse(
			map, QEvent::MouseButtonDblClick, currentPoint,
			Qt::LeftButton, Qt::LeftButton);
		expect(!revealed.empty()
				   && revealed.back() == currentReplacement.Identity,
			   "double-click publishes the exact reveal identity");

		const auto selectedBeforeResize = map.selected();
		harness.Surface.resize(1'240, 520);
		QApplication::processEvents();
		expect(map.selected() == selectedBeforeResize,
			   "resize preserves stable semantic selection");
		const auto resizedLink = std::ranges::find_if(
			map.visible_links(),
			[&selectedBeforeResize](const auto& aLink) {
				return aLink.Identity == selectedBeforeResize;
			});
		expect(resizedLink != map.visible_links().end()
				   && map.hit_test(point_inside(*resizedLink)) == selectedBeforeResize,
			   "hit testing remains exact after divider and viewport resize");

		const auto leftRemeasured = qt::measure_plain_text_pane(
			*harness.Left, harness.Diff.Alignment.LeftLineCount);
		const auto rightRemeasured = qt::measure_plain_text_pane(
			*harness.Right, harness.Diff.Alignment.RightLineCount);
		expect(leftRemeasured.has_value() && rightRemeasured.has_value(),
			   "resized live panes remeasure");
		auto remeasured = qt::build_measured_alignment_axis(
			harness.Diff.Alignment, *leftRemeasured, *rightRemeasured);
		expect(remeasured.valid(), "resized measured axis rebuilds");
		map.set_geometry(*remeasured.Axis, harness.Diff.Alignment.Moves);
		harness.Controller.set_axis(std::move(*remeasured.Axis));
		QApplication::processEvents();
		expect(map.selected() == selectedBeforeResize,
			   "same-comparison geometry rebuild preserves stable selection");

		const auto geometryRebuilds    = map.statistics().GeometryRebuilds;
		const auto hoveredBeforeEnd    = map.hovered();
		const auto hoverCountBeforeEnd = hovered.size();
		harness.Controller.end();
		QApplication::processEvents();
		expect(!map.visible_links().empty(),
			   "trailing changed interval is visible after shared scroll to end");
		const auto scrolledLink = map.visible_links().back();
		expect(map.hit_test(point_inside(scrolledLink)) == scrolledLink.Identity,
			   "path-derived hit testing remains exact after shared scrolling");
		expect(map.statistics().GeometryRebuilds == geometryRebuilds,
			   "shared scrolling does not rebuild retained semantic geometry");
		expect(map.hovered() != hoveredBeforeEnd
				   && hovered.size() == hoverCountBeforeEnd + 1U
				   && hovered.back() == map.hovered(),
			   "shared scrolling reconciles stationary-pointer hover and publishes the change");
		harness.Controller.home();
		QApplication::processEvents();

		map.setFocus(Qt::OtherFocusReason);
		const auto selectionBeforeKey = map.selected();
		auto       announcements      = std::vector<accessible_announcement>{ };
		const auto accessibilityCapture
			= scoped_accessibility_capture{ announcements };
		auto key = QKeyEvent{
			QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier
		};
		QApplication::sendEvent(&map, &key);
		expect(selected.size() == selectionCount + 2U,
			   "keyboard next-change invokes the same semantic selection sink");
		expect(map.selected().has_value() && selectionBeforeKey.has_value()
				   && map.selected() != selectionBeforeKey
				   && (!map.selected()->Group.has_value()
					   || map.selected()->Group != selectionBeforeKey->Group),
			   "keyboard next-change advances to a distinct semantic group");
		expect(announcements.size() == 1U,
			   "keyboard selection publishes one accessible status announcement");
		expect(announcements.front().Message == map.accessibleDescription(),
			   "accessible announcement carries the rendered selection status");
		expect(announcements.front().Target == &map,
			   "accessible announcement identifies the correspondence map");
		expect(announcements.front().Politeness
				   == QAccessible::AnnouncementPoliteness::Polite,
			   "accessible selection status uses polite announcement priority");
		auto revealKey = QKeyEvent{
			QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier
		};
		const auto revealsBeforeKey = revealed.size();
		QApplication::sendEvent(&map, &revealKey);
		expect(revealed.size() == revealsBeforeKey + 1U,
			   "keyboard reveal invokes the nonvisual reveal action");

		auto* const accessible = QAccessible::queryAccessibleInterface(&map);
		expect(accessible != nullptr
				   && !accessible->text(QAccessible::Name).isEmpty()
				   && accessible->text(QAccessible::Description).contains(QStringLiteral("selected"))
				   && accessible->state().focusable,
			   "link map exposes focusable accessible name and selected status");

		const auto leftBlocks = static_cast<std::uint32_t>(
			harness.Left->document()->blockCount());
		const auto rightBlocks = static_cast<std::uint32_t>(
			harness.Right->document()->blockCount());
		expect(leftBlocks >= harness.Diff.Alignment.LeftLineCount
				   && leftBlocks <= harness.Diff.Alignment.LeftLineCount + 1U
				   && rightBlocks >= harness.Diff.Alignment.RightLineCount
				   && rightBlocks <= harness.Diff.Alignment.RightLineCount + 1U,
			   "natural top panes retain only source lines and Qt trailing blocks");

		const auto beforePalette   = map.grab().toImage();
		const auto beforeIdentity  = map.selected();
		const auto beforeGeometry  = map.visible_links().front().Path;
		const auto originalPalette = QApplication::palette();
		auto       darkPalette     = originalPalette;
		darkPalette.setColor(QPalette::Window, QColor{ 35, 38, 42 });
		darkPalette.setColor(QPalette::Base, QColor{ 27, 30, 34 });
		darkPalette.setColor(QPalette::Text, QColor{ 232, 234, 236 });
		darkPalette.setColor(QPalette::Mid, QColor{ 126, 132, 140 });
		darkPalette.setColor(QPalette::Highlight, QColor{ 73, 148, 236 });
		QApplication::setPalette(darkPalette);
		QApplication::processEvents();
		const auto afterPalette = map.grab().toImage();
		expect(beforePalette != afterPalette
				   && map.statistics().PaletteOrStyleChanges > 0U,
			   "live application palette change repaints inherited colors");
		expect(map.selected() == beforeIdentity
				   && map.visible_links().front().Path == beforeGeometry,
			   "palette response preserves semantic identity and geometry");
		const auto appearanceChanges
			= map.statistics().PaletteOrStyleChanges;
		auto themeChange = QEvent{ QEvent::ThemeChange };
		QApplication::sendEvent(&map, &themeChange);
		expect(map.statistics().PaletteOrStyleChanges > appearanceChanges,
			   "theme change reaches the correspondence-map appearance handler");
		expect(map.selected() == beforeIdentity
				   && map.visible_links().front().Path == beforeGeometry,
			   "theme change schedules inherited repaint without replacing identity or geometry");
		QApplication::setPalette(originalPalette);
		QApplication::processEvents();

		const auto renderPath
			= qEnvironmentVariable("BPR_QT_DIFF_LINK_MAP_RENDER_PATH");
		if (!renderPath.isEmpty()) {
			expect(harness.Surface.grab().save(renderPath),
				   "selected live link-map composition raster saves");
		}
	}

	auto test_move_marker_topology_and_temporary_overlay() -> void
	{
		auto options    = core::diff_options{ };
		options.Moves   = core::move_detection_policy::deterministic_exact;
		const auto diff = computed(
			"head\npivot\nmove-one\nmove-two\nanchor-0\nanchor-1\nanchor-2\n"
			"anchor-3\nanchor-4\ntail\n",
			"head\n00000000000000000000\n11111111111111111111\n"
			"22222222222222222222\n33333333333333333333\n"
			"44444444444444444444\n55555555555555555555\n"
			"66666666666666666666\n77777777777777777777\n"
			"88888888888888888888\n99999999999999999999\npivot\nanchor-0\n"
			"anchor-1\nanchor-2\nanchor-3\nanchor-4\nmove-one\nmove-two\ntail\n",
			options,
			7U);
		expect(diff.Alignment.Moves.size() == 1U,
			   "exact moved block produces one core move relation");
		auto built = qt::build_measured_alignment_axis(
			diff.Alignment,
			uniform_measurements(diff.Alignment.LeftLineCount, 24.0, 700.0),
			uniform_measurements(diff.Alignment.RightLineCount, 24.0, 700.0));
		expect(built.valid(), "move fixture measured axis builds");

		auto splitter = qt::diff_comparison_splitter{ };
		splitter.addWidget(new QWidget{ });
		splitter.addWidget(new QWidget{ });
		splitter.resize(760, 700);
		splitter.show();
		QApplication::processEvents();
		auto* const map = splitter.link_map();
		expect(map != nullptr, "move fixture owns link map");
		map->set_geometry(*built.Axis, diff.Alignment.Moves);
		expect(map->set_frame(frame_for(*built.Axis, 0.0, 0.0)),
			   "same-comparison move frame applies");
		static_cast<void>(map->grab());
		const auto links  = map->visible_links();
		const auto source = std::ranges::find_if(
			links, [](const auto& aLink) { return aLink.SourceMove.has_value(); });
		const auto destination = std::ranges::find_if(
			links,
			[](const auto& aLink) { return aLink.DestinationMove.has_value(); });
		expect(source != links.end() && destination != links.end()
				   && source->SourceMove == destination->DestinationMove
				   && source->SourceMarkerY.has_value()
				   && destination->DestinationMarkerY.has_value()
				   && close(
					   *source->SourceMarkerY,
					   (source->LeftTop + source->LeftBottom) / 2.0)
				   && close(
					   *destination->DestinationMarkerY,
					   (destination->RightTop + destination->RightBottom) / 2.0)
				   && std::abs(
						  *source->SourceMarkerY
						  - source->Path.boundingRect().center().y())
					   > 40.0
				   && std::abs(
						  *destination->DestinationMarkerY
						  - destination->Path.boundingRect().center().y())
					   > 40.0
				   && map->statistics().LastMoveMarkers == 2U
				   && map->statistics().LastMoveOverlays == 0U,
			   "displaced move markers attach to their exact pane-edge intervals");
		const auto sourceIdentity    = source->Identity;
		const auto relation          = *source->SourceMove;
		const auto sourceMarker      = *source->SourceMarkerY;
		const auto destinationMarker = *destination->DestinationMarkerY;
		map->set_selected(sourceIdentity);
		static_cast<void>(map->grab());
		const auto overlays = map->visible_move_overlays();
		expect(overlays.size() == 1U
				   && overlays.front().Relation == relation
				   && close(overlays.front().SourceY, sourceMarker)
				   && close(
					   overlays.front().DestinationY, destinationMarker)
				   && close(overlays.front().Path.elementAt(0).x, 0.0)
				   && close(
					   overlays.front().Path.elementAt(0).y, sourceMarker)
				   && close(overlays.front().Path.elementAt(3).x, map->width())
				   && close(
					   overlays.front().Path.elementAt(3).y, destinationMarker)
				   && map->statistics().LastMoveMarkers == 2U
				   && map->statistics().LastMoveOverlays == 1U,
			   "selected move caches one pane-edge-to-pane-edge correspondence overlay");
	}

	auto test_callback_containment_and_lifetime() -> void
	{
		auto throwing = std::make_unique<live_harness>(
			"head\nold\ntail\n", "head\nnew\ntail\n");
		auto* const throwingMap   = throwing->Map;
		const auto  throwingPoint = point_inside(
			link_of_kind(*throwingMap, core::alignment_kind::replacement));
		throwingMap->set_selection_sink(
			[](const auto&) { throw std::runtime_error{ "selection sink" }; });
		send_mouse(
			*throwingMap, QEvent::MouseButtonPress, throwingPoint,
			Qt::LeftButton, Qt::LeftButton);
		send_mouse(
			*throwingMap, QEvent::MouseButtonRelease, throwingPoint,
			Qt::LeftButton, Qt::NoButton);
		expect(throwingMap->statistics().CallbackFailures == 1U,
			   "throwing selection callback is contained at the Qt boundary");

		auto clearing = std::make_unique<live_harness>(
			"head\nold\ntail\n", "head\nnew\ntail\n");
		auto* const clearingMap = clearing->Map;
		auto        clearedHover
			= std::vector<std::optional<qt::diff_link_map_identity>>{ };
		clearingMap->set_hover_sink(
			[&clearedHover](const auto& aIdentity) {
				clearedHover.push_back(aIdentity);
			});
		send_mouse(
			*clearingMap,
			QEvent::MouseMove,
			point_inside(link_of_kind(
				*clearingMap, core::alignment_kind::replacement)),
			Qt::NoButton,
			Qt::NoButton);
		expect(clearingMap->hovered().has_value()
				   && clearedHover.size() == 1U
				   && clearedHover.back().has_value(),
			   "pointer hover reaches downstream state before geometry clears");
		clearingMap->clear_geometry();
		expect(!clearingMap->hovered().has_value()
				   && clearedHover.size() == 2U
				   && !clearedHover.back().has_value(),
			   "geometry clear publishes the required downstream hover reset");

		auto deleting = std::make_unique<live_harness>(
			"head\nold\ntail\n", "head\nnew\ntail\n");
		auto* const deletingMap   = deleting->Map;
		const auto  deletingPoint = point_inside(
			link_of_kind(*deletingMap, core::alignment_kind::replacement));
		deletingMap->set_selection_sink(
			[&deleting](const auto&) { deleting.reset(); });
		send_mouse(
			*deletingMap, QEvent::MouseButtonPress, deletingPoint,
			Qt::LeftButton, Qt::LeftButton);
		send_mouse(
			*deletingMap, QEvent::MouseButtonRelease, deletingPoint,
			Qt::LeftButton, Qt::NoButton);
		expect(deleting == nullptr,
			   "synchronous parent destruction from selection callback is safe");
	}

	auto test_indexed_visibility_and_stale_frame_rejection() -> void
	{
		auto                  left        = std::string{ };
		auto                  right       = std::string{ };
		constexpr std::size_t changeCount = 320U;
		for (std::size_t index = 0U; index < changeCount; ++index) {
			left += "anchor-" + std::to_string(index) + "\nleft-"
				+ std::to_string(index) + "\n";
			right += "anchor-" + std::to_string(index) + "\nright-"
				+ std::to_string(index) + "\n";
		}
		const auto diff  = computed(left, right, { }, 19U);
		auto       built = qt::build_measured_alignment_axis(
			diff.Alignment,
			uniform_measurements(diff.Alignment.LeftLineCount, 10.0, 240.0),
			uniform_measurements(diff.Alignment.RightLineCount, 10.0, 240.0));
		expect(built.valid(), "large changed-axis fixture builds");

		auto splitter = qt::diff_comparison_splitter{ };
		splitter.addWidget(new QWidget{ });
		splitter.addWidget(new QWidget{ });
		splitter.resize(760, 240);
		splitter.show();
		QApplication::processEvents();
		auto* const map = splitter.link_map();
		expect(map != nullptr, "large fixture owns link map");
		map->set_geometry(*built.Axis);
		const auto rebuilds = map->statistics().GeometryRebuilds;
		expect(map->set_frame(frame_for(*built.Axis, 3'000.0, 3'000.0)),
			   "indexed visibility frame applies");
		const auto middle = map->statistics();
		expect(middle.RetainedChangedEntries >= changeCount - 1U
				   && middle.LastCandidateEntries < middle.RetainedChangedEntries / 4U
				   && middle.LastVisiblePaths <= middle.LastCandidateEntries,
			   "ordered lookup bounds work to a near-visible subset");
		expect(map->set_frame(frame_for(*built.Axis, 4'000.0, 4'000.0)),
			   "ordinary scroll applies another visible frame");
		expect(map->statistics().GeometryRebuilds == rebuilds,
			   "ordinary scroll does not rebuild retained semantic geometry");

		const auto other     = computed("a\n", "b\n", { }, 27U);
		auto       otherAxis = qt::build_measured_alignment_axis(
			other.Alignment,
			uniform_measurements(other.Alignment.LeftLineCount, 10.0, 240.0),
			uniform_measurements(other.Alignment.RightLineCount, 10.0, 240.0));
		expect(otherAxis.valid(), "stale-frame comparison fixture builds");
		const auto visibleBefore = map->visible_links().size();
		expect(!map->set_frame(frame_for(*otherAxis.Axis, 0.0, 0.0))
				   && map->visible_links().size() == visibleBefore
				   && map->statistics().RejectedStaleFrames == 1U,
			   "different-comparison frame is rejected without replacing geometry");
		auto invalidOrigin                 = frame_for(*built.Axis, 0.0, 0.0);
		invalidOrigin.Left.DocumentOriginY = -1.0;
		expect(!map->set_frame(invalidOrigin)
				   && map->visible_links().size() == visibleBefore
				   && map->statistics().RejectedStaleFrames == 2U,
			   "invalid same-comparison origin is rejected without replacing geometry");
	}
} // namespace

auto main(int argc, char** argv) -> int
{
	QApplication application{ argc, argv };
	QApplication::setApplicationName(
		QStringLiteral("BetterPullReviewDiffLinkMapTests"));

	test_rendered_geometry_selection_and_native_drag();
	test_move_marker_topology_and_temporary_overlay();
	test_callback_containment_and_lifetime();
	test_indexed_visibility_and_stale_frame_rejection();

	std::cout << "diff-link-map-tests:pass\n";
	return EXIT_SUCCESS;
}
