#include "github_application/fixture_uiforetw_observation.hpp"
#include "review_submission_workflow.hpp"

#if defined(BPR_BUILD_TESTING)
	#include "bpr/app/github_application.hpp"
	#include "bpr/application/appearance_application_host.hpp"
	#include "bpr/application/appearance_runtime_ports.hpp"
	#include "bpr/application/workbench_runtime_ports.hpp"
	#include "bpr/core/appearance_model.hpp"
	#include "bpr/core/diff_compute.hpp"
	#include "bpr/core/diff_model.hpp"
	#include "bpr/core/diff_semantics.hpp"
	#include "bpr/core/epoch_model.hpp"
	#include "bpr/qt/appearance_profile.hpp"
	#include "bpr/qt/diff_link_map.hpp"
	#include "bpr/qt/diff_workbench.hpp"
	#include "bpr/qt/document_tab_strip.hpp"
	#include "bpr/qt/epoch_comparison_control.hpp"
	#include "bpr/qt/workbench_drawer.hpp"
	#include "bpr/qt/workbench_window.hpp"
	#include "github_application/window_composition.hpp"
	#include "github_application/window_probe_states.hpp"
	#include "github_application_fixture_access.hpp"
	#include "terminal_diagnostics.hpp"
	#include <QAbstractButton>
	#include <QAbstractScrollArea>
	#include <QAbstractSlider>
	#include <QAction>
	#include <QAnyStringView>
	#include <QApplication>
	#include <QBrush>
	#include <QByteArray>
	#include <QByteArrayView>
	#include <QChar>
	#include <QColor>
	#include <QCoreApplication>
	#include <QDesktopServices>
	#include <QDir>
	#include <QEventLoop>
	#include <QFile>
	#include <QFileDevice>
	#include <QFlags>
	#include <QGuiApplication>
	#include <QIODevice>
	#include <QIODeviceBase>
	#include <QIcon>
	#include <QImage>
	#include <QKeySequence>
	#include <QLatin1Char>
	#include <QLatin1String>
	#include <QList>
	#include <QListSpecialMethods>
	#include <QListSpecialMethodsBase>
	#include <QMainWindow>
	#include <QMenu>
	#include <QMenuBar>
	#include <QMetaObject>
	#include <QObject>
	#include <QPaintDevice>
	#include <QPainterPath>
	#include <QPainterPathStroker>
	#include <QPalette>
	#include <QPixmap>
	#include <QPlainTextEdit>
	#include <QPoint>
	#include <QPointF>
	#include <QRect>
	#include <QRectF>
	#include <QRegion>
	#include <QRgb>
	#include <QScrollBar>
	#include <QSize>
	#include <QStatusBar>
	#include <QString>
	#include <QStyle>
	#include <QTextBlock>
	#include <QTextCharFormat>
	#include <QTextCursor>
	#include <QTextDocument>
	#include <QTextEdit>
	#include <QTextFormat>
	#include <QTextStream>
	#include <QToolBar>
	#include <QToolButton>
	#include <QWidget>
	#include <algorithm>
	#include <array>
	#include <chrono>
	#include <cstddef>
	#include <glaze/json.hpp>
	#include <iterator>
	#include <limits>
	#include <memory>
	#include <optional>
	#include <span>
	#include <string>
	#include <string_view>
	#include <tuple>
	#include <utility>
	#include <vector>
#endif

namespace bpr::app {
	namespace {
#if defined(BPR_BUILD_TESTING)
		/// \brief [[BPR-1036]]. The capture's layout floors, in thousandths of
		/// the window the comparison must take.
		///
		/// \details The height floors are the measured stacks'. [[BPR-1130]]:
		/// RFC-BPR-0052's Updates line folds, a new window starts folded, and
		/// open its timeline is one 34-pixel row with no caption, so the stack
		/// is taken both ways. [[BPR-1132]]: the comparison's header is the
		/// 28-pixel document tab strip, in place of the diff status row and its
		/// spacing. Measured on macOS, folded, the stack is the menu bar 20, the
		/// title row 30, the review bar 28, the tab strip 28 and the status bar
		/// 22: the comparison is 872. Open, the timeline's 34 sit under the bar:
		/// 838. On Linux [[BPR-1036]]'s stack (a 14-pixel status and 6 of
		/// spacing) gives 876 folded and 842 open, derived until the Linux
		/// station measures them. Windows has no menu bar row and keeps the
		/// owner's 850 budget ([[BPR-0246]]) for the open line, the tighter
		/// state, and so 884 folded, unmeasured until the Windows station runs
		/// this course. The folded stack is the open one less the timeline's
		/// row exactly, on every platform. Any further row that takes height
		/// fails and is named. The course script asserts the same floors.
		inline constexpr int uiforetw_capture_width_floor = 750;
	#if defined(__APPLE__)
		inline constexpr int uiforetw_capture_open_height_floor = 838;

		inline constexpr int uiforetw_capture_folded_height_floor = 872;
	#endif
		/// \brief [[BPR-1036]]. Names what shares the capture window with the
		/// comparison when the comparison is under a layout floor.
		///
		/// \details The failure line used to give two ratios and no widgets, so
		/// what took the room was a guess. This prints the window and the rows
		/// the specification stacks around the comparison -- the menu bar, the
		/// strip, the epoch line and the status bar -- as `y+height`, or why
		/// one takes no room. Then, at every level from the comparison up to
		/// the window, each shown widget that sits above or below the
		/// comparison's branch (`stack=`) or beside it (`beside=`), so a row
		/// the specification does not name is named too. `unstacked` is the
		/// height neither the comparison nor a stacked row accounts for:
		/// layout margins and spacing.
		void report_capture_layout(
			QTextStream&                aStream,
			const qt::workbench_window& aWindow,
			const QWidget&              aComparison)
		{
			const auto inWindow = [&aWindow](const QWidget& aWidget) {
				return QRect{ aWidget.mapTo(&aWindow, QPoint{ 0, 0 }), aWidget.size() };
			};
			const auto row = [&inWindow](const QWidget* const aWidget) {
				if (aWidget == nullptr) {
					return QStringLiteral("absent");
				}
				if (!aWidget->isVisible()) {
					return QStringLiteral("hidden");
				}
				const auto rect = inWindow(*aWidget);
				return QStringLiteral("%1+%2").arg(rect.top()).arg(rect.height());
			};
			const auto name = [](const QWidget& aWidget) {
				return aWidget.objectName().isEmpty()
					? QString::fromLatin1(aWidget.metaObject()->className())
					: aWidget.objectName();
			};
			auto stacked = std::vector<std::pair<QRect, QString>>{ };
			auto beside  = std::vector<std::pair<QRect, QString>>{ };
			for (const auto* branch = &aComparison;
				 branch != &aWindow && branch->parentWidget() != nullptr;
				 branch = branch->parentWidget()) {
				const auto along = inWindow(*branch);
				for (const auto* const sibling :
					 branch->parentWidget()->findChildren<QWidget*>(Qt::FindDirectChildrenOnly)) {
					if (sibling == branch || sibling->isWindow() || !sibling->isVisible()) {
						continue;
					}
					const auto rect = inWindow(*sibling);
					const auto rows = rect.top() <= along.bottom() && along.top() <= rect.bottom();
					const auto columns
						= rect.left() <= along.right() && along.left() <= rect.right();
					if (columns && !rows) {
						stacked.emplace_back(rect, name(*sibling));
					} else if (rows && !columns) {
						beside.emplace_back(rect, name(*sibling));
					}
				}
			}
			std::ranges::sort(stacked, { }, [](const auto& aEntry) { return aEntry.first.top(); });
			std::ranges::sort(beside, { }, [](const auto& aEntry) { return aEntry.first.left(); });
			// Widgets that share a row, such as two buttons side by side, take
			// its height once.
			const auto comparison = inWindow(aComparison);
			auto       unstacked  = aWindow.height() - comparison.height();
			auto       covered    = std::numeric_limits<int>::min();
			auto       stackText  = QStringList{ };
			for (const auto& [rect, widget] : stacked) {
				unstacked -= std::max(0, rect.bottom() - std::max(rect.top() - 1, covered));
				covered = std::max(covered, rect.bottom());
				stackText.append(QStringLiteral("%1@%2+%3").arg(widget).arg(rect.top()).arg(rect.height()));
			}
			auto besideText = QStringList{ };
			for (const auto& [rect, widget] : beside) {
				besideText.append(QStringLiteral("%1@%2+%3").arg(widget).arg(rect.left()).arg(rect.width()));
			}
			aStream << " width_floor=" << uiforetw_capture_width_floor
					<< " folded_height_floor=" << uiforetw_capture_folded_height_floor
					<< " open_height_floor=" << uiforetw_capture_open_height_floor
					<< " window=" << aWindow.width() << 'x' << aWindow.height()
					<< " menu_bar=" << row(aWindow.menu_bar())
					<< " strip=" << row(aWindow.primary_toolbar())
					<< " review_bar=" << row(aWindow.review_bar())
					<< " epoch_line=" << row(aWindow.epoch_selectors())
					<< " tabs=" << row(aWindow.document_tabs())
					<< " comparison=" << comparison.left() << ',' << comparison.top() << ' '
					<< comparison.width() << 'x' << comparison.height()
					<< " status_bar=" << row(aWindow.statusBar())
					<< " stack=" << (stackText.isEmpty() ? QStringLiteral("none") : stackText.join(QLatin1Char{ ',' }))
					<< " unstacked=" << unstacked
					<< " beside=" << (besideText.isEmpty() ? QStringLiteral("none") : besideText.join(QLatin1Char{ ',' }));
		}

		struct intraline_render_observation
		{
			intraline_pair_location Pair;
			QRect                   LeftRect;
			QRect                   RightRect;
			std::size_t             LeftRolePixels  = 0U;
			std::size_t             RightRolePixels = 0U;
		};

		[[nodiscard]] auto role_pixel_count(
			QPlainTextEdit&                aPane,
			const QRect&                   aRect,
			const QColor&                  aExpected,
			const QColor&                  aRowBand,
			const core::appearance_variant aVariant) -> std::size_t
		{
			const auto image = aPane.viewport()->grab(aRect).toImage().convertToFormat(
				QImage::Format_ARGB32);
			if (image.isNull() || !aExpected.isValid()) {
				return 0U;
			}
			const auto accepted = role_renderings(aPane, aExpected, aRowBand, aVariant);
			auto       count    = std::size_t{ 0U };
			for (auto y = 0; y < image.height(); ++y) {
				for (auto x = 0; x < image.width(); ++x) {
					const auto pixel = image.pixel(x, y);
					if (std::ranges::any_of(
							accepted,
							[pixel](const QColor& aCandidate) {
								return aCandidate.isValid()
									&& aCandidate.rgba() == pixel;
							})) {
						++count;
					}
				}
			}
			return count;
		}

		/// \brief Records the actual normal and disabled pixels of one toolbar icon.
		struct navigation_icon_contrast_observation
		{
			QColor       Background;
			QColor       NormalForeground;
			QColor       DisabledForeground;
			std::size_t  NormalForegroundPixels     = 0U;
			std::size_t  DisabledForegroundPixels   = 0U;
			std::int64_t NormalContrastHundredths   = 0;
			std::int64_t DisabledContrastHundredths = 0;
			bool         StatesDistinct             = false;
		};

		[[nodiscard]] auto exact_icon_foreground_pixels(
			const QIcon&      aIcon,
			const QSize&      aSize,
			const qreal       aScale,
			const QIcon::Mode aMode,
			const QColor&     aExpected) -> std::size_t
		{
			const auto image = aIcon.pixmap(aSize, aScale, aMode, QIcon::Off)
								   .toImage()
								   .convertToFormat(QImage::Format_ARGB32);
			if (image.isNull() || !aExpected.isValid()) {
				return 0U;
			}
			constexpr auto rgbComponentMask = QRgb{ 0x00FF'FFFFU };
			const auto     expected         = aExpected.rgb() & rgbComponentMask;
			auto           count            = std::size_t{ 0U };
			for (auto y = 0; y < image.height(); ++y) {
				for (auto x = 0; x < image.width(); ++x) {
					const auto pixel = image.pixel(x, y);
					if (qAlpha(pixel) >= 224
						&& (pixel & rgbComponentMask) == expected) {
						++count;
					}
				}
			}
			return count;
		}

		[[nodiscard]] auto observe_navigation_icon_contrast(
			const QToolButton& aButton)
			-> navigation_icon_contrast_observation
		{
			const auto background = aButton.palette().color(
				QPalette::Active, QPalette::Button);
			const auto normal = aButton.palette().color(
				QPalette::Active, QPalette::ButtonText);
			const auto disabled = aButton.palette().color(
				QPalette::Disabled, QPalette::ButtonText);
			const auto size          = aButton.iconSize().expandedTo(QSize{ 16, 16 });
			const auto scale         = aButton.devicePixelRatioF();
			const auto normalImage   = aButton.icon()
										   .pixmap(size, scale, QIcon::Normal, QIcon::Off)
										   .toImage();
			const auto disabledImage = aButton.icon()
										   .pixmap(size, scale, QIcon::Disabled, QIcon::Off)
										   .toImage();
			return {
				.Background             = background,
				.NormalForeground       = normal,
				.DisabledForeground     = disabled,
				.NormalForegroundPixels = exact_icon_foreground_pixels(
					aButton.icon(), size, scale, QIcon::Normal, normal),
				.DisabledForegroundPixels = exact_icon_foreground_pixels(
					aButton.icon(), size, scale, QIcon::Disabled, disabled),
				.NormalContrastHundredths   = static_cast<std::int64_t>(std::lround(
					100.0 * qt::contrast_ratio(qt::to_appearance_color(background), qt::to_appearance_color(normal)))),
				.DisabledContrastHundredths = static_cast<std::int64_t>(std::lround(
					100.0 * qt::contrast_ratio(qt::to_appearance_color(background), qt::to_appearance_color(disabled)))),
				.StatesDistinct             = !normalImage.isNull()
					&& !disabledImage.isNull() && normalImage != disabledImage,
			};
		}

		[[nodiscard]] auto exact_role_selection_present(
			const QPlainTextEdit&              aPane,
			const core::decoded_text_document& aDocument,
			const std::uint32_t                aLine,
			const core::byte_span              aSpan,
			const QColor&                      aExpected,
			const QColor&                      aRowBand,
			const core::appearance_variant     aVariant) -> bool
		{
			if (aLine >= aDocument.Lines.size()) {
				return false;
			}
			const auto& decoded = aDocument.Lines[aLine];
			if (decoded.Content.End < decoded.Content.Begin
				|| decoded.Content.End > aDocument.Bytes.size()) {
				return false;
			}
			const auto bytes = std::span<const std::uint8_t>{
				aDocument.Bytes.data() + decoded.Content.Begin,
				static_cast<std::size_t>(decoded.Content.End - decoded.Content.Begin)
			};
			const auto block = aPane.document()->findBlockByNumber(
				static_cast<int>(aLine));
			if (!block.isValid()) {
				return false;
			}
			const auto from     = block.position() + character_offset(bytes, aSpan.Begin);
			const auto to       = block.position() + character_offset(bytes, aSpan.End);
			const auto accepted = role_renderings(aPane, aExpected, aRowBand, aVariant);
			return std::ranges::any_of(
				aPane.extraSelections(),
				[&accepted, from, to](const QTextEdit::ExtraSelection& aSelection) {
					return !aSelection.format.boolProperty(
							   QTextFormat::FullWidthSelection)
						&& (std::ranges::contains(
								accepted,
								aSelection.format.background().color())
							|| aSelection.format.fontUnderline())
						&& aSelection.cursor.selectionStart() == from
						&& aSelection.cursor.selectionEnd() == to;
				});
		}

		[[nodiscard]] auto observe_intraline_render(
			qt::diff_workbench&             aWorkbench,
			const core::computed_file_diff& aResult,
			const qt::appearance_profile&   aProfile,
			const std::string_view          aLeftFragment,
			const std::string_view          aRightFragment)
			-> std::optional<intraline_render_observation>
		{
			const auto pair = find_intraline_pair(
				aResult, aLeftFragment, aRightFragment);
			if (!pair.has_value()) {
				return std::nullopt;
			}
			auto* const left     = aWorkbench.left_pane();
			auto* const right    = aWorkbench.right_pane();
			const auto  leftRect = intraline_selection_rect(
				*left, *aResult.LeftDocument, pair->Left, pair->LeftSpan);
			const auto rightRect = intraline_selection_rect(
				*right, *aResult.RightDocument, pair->Right, pair->RightSpan);
			if (!leftRect.has_value() || !rightRect.has_value()
				|| leftRect->isEmpty() || rightRect->isEmpty()
				|| !QRect{ QPoint{ 0, 0 }, left->viewport()->size() }.contains(
					*leftRect, true)
				|| !QRect{ QPoint{ 0, 0 }, right->viewport()->size() }.contains(
					*rightRect, true)) {
				return std::nullopt;
			}
			const auto removed = aProfile.color(
				core::appearance_role::diff_intraline_removed_background);
			const auto added = aProfile.color(
				core::appearance_role::diff_intraline_added_background);
			if (!exact_role_selection_present(
					*left,
					*aResult.LeftDocument,
					pair->Left,
					pair->LeftSpan,
					removed,
					aProfile.color(core::appearance_role::diff_removed_background),
					aProfile.variant())
				|| !exact_role_selection_present(
					*right,
					*aResult.RightDocument,
					pair->Right,
					pair->RightSpan,
					added,
					aProfile.color(core::appearance_role::diff_added_background),
					aProfile.variant())) {
				return std::nullopt;
			}
			return intraline_render_observation{
				.Pair           = *pair,
				.LeftRect       = *leftRect,
				.RightRect      = *rightRect,
				.LeftRolePixels = role_pixel_count(
					*left,
					*leftRect,
					removed,
					aProfile.color(core::appearance_role::diff_removed_background),
					aProfile.variant()),
				.RightRolePixels = role_pixel_count(
					*right,
					*rightRect,
					added,
					aProfile.color(core::appearance_role::diff_added_background),
					aProfile.variant()),
			};
		}

		struct connector_pixel_observation
		{
			QColor       Background;
			QColor       Dominant;
			std::size_t  SampledPixels            = 0U;
			std::size_t  ContrastPixels           = 0U;
			std::size_t  DominantPixels           = 0U;
			std::int64_t MedianContrastHundredths = 0;
		};

		struct connector_render_observation
		{
			QRectF                      Bounds;
			QPointF                     Start;
			QPointF                     End;
			connector_pixel_observation Pixels;
			bool                        Selected      = false;
			bool                        LeftEndpoint  = false;
			bool                        RightEndpoint = false;
			bool                        LeftOverlap   = false;
			bool                        RightOverlap  = false;
		};

		[[nodiscard]] auto dominant_connector_pixel(std::vector<QRgb> aPixels)
			-> std::pair<QColor, std::size_t>
		{
			if (aPixels.empty()) {
				return { QColor{ }, 0U };
			}
			std::ranges::sort(aPixels);
			auto bestPixel = aPixels.front();
			auto bestCount = std::size_t{ 0U };
			for (auto first = aPixels.begin(); first != aPixels.end();) {
				const auto last = std::ranges::find_if(
					first, aPixels.end(), [first](const QRgb aPixel) {
						return aPixel != *first;
					});
				const auto count = static_cast<std::size_t>(
					std::distance(first, last));
				if (count > bestCount) {
					bestPixel = *first;
					bestCount = count;
				}
				first = last;
			}
			return { QColor::fromRgba(bestPixel), bestCount };
		}

		[[nodiscard]] auto observe_connector_pixels(
			qt::diff_link_map&           aMap,
			const qt::visible_diff_link& aTarget)
			-> connector_pixel_observation
		{
			const auto image = aMap.grab().toImage().convertToFormat(
				QImage::Format_ARGB32);
			const auto scale = image.devicePixelRatio();
			if (image.isNull() || scale <= 0.0) {
				return { };
			}

			const auto links   = aMap.visible_links();
			auto       stroker = QPainterPathStroker{ };
			stroker.setWidth(3.0);
			const auto targetMask
				= aTarget.Path.united(stroker.createStroke(aTarget.Path));
			auto backgroundPixels = std::vector<QRgb>{ };
			auto targetPixels     = std::vector<QRgb>{ };
			backgroundPixels.reserve(
				static_cast<std::size_t>(image.sizeInBytes() / 4));
			targetPixels.reserve(backgroundPixels.capacity() / 4U);
			for (auto y = 0; y < image.height(); ++y) {
				for (auto x = 0; x < image.width(); ++x) {
					const auto point = QPointF{
						(static_cast<qreal>(x) + 0.5) / scale,
						(static_cast<qreal>(y) + 0.5) / scale,
					};
					const auto inAny = std::ranges::any_of(
						links,
						[point](const qt::visible_diff_link& aLink) {
							return aLink.Path.contains(point);
						});
					if (targetMask.contains(point)) {
						const auto overlapsAnother = std::ranges::any_of(
							links,
							[&aTarget, point](const qt::visible_diff_link& aLink) {
								return &aLink != &aTarget
									&& aLink.Path.contains(point);
							});
						if (!overlapsAnother) {
							targetPixels.push_back(image.pixel(x, y));
						}
					} else if (!inAny) {
						backgroundPixels.push_back(image.pixel(x, y));
					}
				}
			}

			const auto [background, backgroundCount]
				= dominant_connector_pixel(std::move(backgroundPixels));
			const auto [dominant, dominantCount]
				= dominant_connector_pixel(targetPixels);
			if (!background.isValid() || backgroundCount == 0U
				|| !dominant.isValid() || targetPixels.empty()) {
				return { };
			}

			auto contrasts = std::vector<std::int64_t>{ };
			contrasts.reserve(targetPixels.size());
			for (const auto pixel : targetPixels) {
				contrasts.push_back(static_cast<std::int64_t>(std::lround(
					100.0 * qt::contrast_ratio(qt::to_appearance_color(background), qt::to_appearance_color(QColor::fromRgba(pixel))))));
			}
			std::ranges::sort(contrasts);
			return {
				.Background     = background,
				.Dominant       = dominant,
				.SampledPixels  = targetPixels.size(),
				.ContrastPixels = static_cast<std::size_t>(
					std::ranges::count_if(
						contrasts,
						[](const auto aContrast) { return aContrast >= 300; })),
				.DominantPixels           = dominantCount,
				.MedianContrastHundredths = contrasts[contrasts.size() / 2U],
			};
		}

		[[nodiscard]] auto observe_connector_render(
			qt::diff_workbench&                 aWorkbench,
			const intraline_render_observation& aTarget)
			-> std::optional<connector_render_observation>
		{
			auto* const map   = aWorkbench.link_map();
			const auto  found = std::ranges::find_if(
				map->visible_links(),
				[&aTarget](const qt::visible_diff_link& aLink) {
					return aLink.Identity.Segment == aTarget.Pair.Segment;
				});
			if (found == map->visible_links().end()) {
				return std::nullopt;
			}
			const auto mapTop   = map->mapToGlobal(QPoint{ 0, 0 }).y();
			const auto relative = [mapTop](
									  QPlainTextEdit& aPane,
									  const QRect&    aRect) {
				const auto top = aPane.viewport()->mapToGlobal(aRect.topLeft()).y()
					- mapTop;
				return std::pair{ static_cast<qreal>(top),
								  static_cast<qreal>(top + aRect.height()) };
			};
			const auto [leftTop, leftBottom] = relative(
				*aWorkbench.left_pane(), aTarget.LeftRect);
			const auto [rightTop, rightBottom] = relative(
				*aWorkbench.right_pane(), aTarget.RightRect);
			const auto overlaps = [](const qreal aFirstTop,
									 const qreal aFirstBottom,
									 const qreal aSecondTop,
									 const qreal aSecondBottom) {
				return std::max(aFirstTop, aSecondTop)
					<= std::min(aFirstBottom, aSecondBottom);
			};
			const auto startElement = found->Path.elementAt(0);
			const auto endElement   = found->Path.elementAt(3);
			const auto start        = QPointF{ startElement.x, startElement.y };
			const auto end          = QPointF{ endElement.x, endElement.y };
			const auto selection    = map->selected();
			const auto isSelected   = selection.has_value()
				&& (selection->Group.has_value()
							&& found->Identity.Group.has_value()
						? selection->Group == found->Identity.Group
						: selection->Segment == found->Identity.Segment);
			return connector_render_observation{
				.Bounds        = found->Path.boundingRect(),
				.Start         = start,
				.End           = end,
				.Pixels        = observe_connector_pixels(*map, *found),
				.Selected      = isSelected,
				.LeftEndpoint  = std::abs(start.x()) <= 0.5,
				.RightEndpoint = std::abs(end.x() - map->width()) <= 0.5,
				.LeftOverlap   = overlaps(
					found->LeftTop, found->LeftBottom, leftTop, leftBottom),
				.RightOverlap = overlaps(
					found->RightTop, found->RightBottom, rightTop, rightBottom),
			};
		}

		// Capture facts own every value, including strings returned by Qt.
		struct capture_json_options : glz::opts
		{
			// Glaze reads its options by these exact member names.
			// NOLINTNEXTLINE(readability-identifier-naming)
			std::uint8_t indentation_width = 4U;
			// NOLINTNEXTLINE(readability-identifier-naming)
			bool escape_control_characters = true;
			constexpr capture_json_options() { prettify = true; }
		};

		[[nodiscard]] auto point_json(const QPointF& aPoint)
		{
			return capture_object(
				"x",
				aPoint.x(),
				"y",
				aPoint.y());
		}

		[[nodiscard]] auto appearance_mode_name(
			const core::appearance_mode aMode) -> QString
		{
			switch (aMode) {
			case core::appearance_mode::system:
				return QStringLiteral("system");
			case core::appearance_mode::light:
				return QStringLiteral("light");
			case core::appearance_mode::dark:
				return QStringLiteral("dark");
			case core::appearance_mode::high_contrast_system:
				return QStringLiteral("high-contrast-system");
			case core::appearance_mode::high_contrast_light:
				return QStringLiteral("high-contrast-light");
			case core::appearance_mode::high_contrast_dark:
				return QStringLiteral("high-contrast-dark");
			}
			return QStringLiteral("unknown");
		}
#endif
	}

#if defined(BPR_BUILD_TESTING)
	auto detail::github_application_fixture_access::
		capture_uiforetw_selected_diff_evidence(
			github_application&         aApplication,
			const std::string_view      aDirectory,
			const core::appearance_mode aMode) -> bool
	{
		auto* const implementation = aApplication.Implementation.get();
		if (implementation == nullptr || implementation->Window == nullptr
			|| implementation->AppearanceHost == nullptr
			|| implementation->AppearanceProfile == nullptr
			|| !implementation->UIforEtwSelectedDiffProbe.has_value()
			|| implementation->UIforEtwSelectedDiffProbe->Phase
				!= uiforetw_selected_diff_probe_state::phase::passed
			|| implementation->AppearanceHost->settings().Mode != aMode
			|| aDirectory.empty()) {
			return false;
		}
		auto* const       window     = implementation->Window.get();
		auto* const       workbench  = window->comparison_surface();
		const auto* const result     = workbench != nullptr
			? workbench->current_result().get()
			: nullptr;
		const auto        firstPair  = result != nullptr
			? find_intraline_pair(
				  *result,
				  "WS_TABSTOP,464,28,67,10",
				  "WS_TABSTOP,464,24,67,10")
			: std::optional<intraline_pair_location>{ };
		const auto        secondPair = result != nullptr
			? find_intraline_pair(
				  *result,
				  "WS_TABSTOP,464,42,98,10",
				  "WS_TABSTOP,464,36,98,10")
			: std::optional<intraline_pair_location>{ };
		if (workbench == nullptr || workbench->current_result() == nullptr
			|| !firstPair.has_value() || !secondPair.has_value()
			|| !firstPair->Group.has_value() || !secondPair->Group.has_value()
			|| !selected_diff_matches(
				*implementation,
				uiforetw_rc_path,
				uiforetw_rc_base_blob,
				uiforetw_rc_head_blob)) {
			return false;
		}
		// [[BPR-1033]]. The panes measure their lines in batches of about 4 ms,
		// and the link map shows no link until that measurement completes, so
		// four passes alone depend on the machine. Wait for the measurement
		// itself, bounded, then drain as before.
		const auto drain = [workbench] {
			const auto started = std::chrono::steady_clock::now();
			while (workbench->geometry_pending()
				   && std::chrono::steady_clock::now() - started < std::chrono::seconds{ 5 }) {
				QApplication::processEvents();
			}
			for (auto tick = 0; tick < 4; ++tick) {
				QApplication::processEvents();
			}
		};
		const auto selected = [workbench](const intraline_pair_location& aPair) {
			return aPair.Group.has_value()
				&& workbench->selection().has_value()
				&& workbench->selection()->Segment == aPair.Segment
				&& workbench->selection()->Group == aPair.Group
				&& workbench->link_map()->selected()
				== qt::diff_link_map_identity{
					   .Segment = aPair.Segment,
					   .Group   = aPair.Group,
				   };
		};
		const auto observeTarget = [workbench, result, implementation](
									   const std::string_view aLeft,
									   const std::string_view aRight) {
			return observe_intraline_render(
				*workbench,
				*result,
				*implementation->AppearanceProfile,
				aLeft,
				aRight);
		};
		const auto connectorValid = [](const connector_render_observation& aValue) {
			return aValue.LeftEndpoint && aValue.RightEndpoint
				&& aValue.LeftOverlap && aValue.RightOverlap
				&& aValue.Pixels.Background.isValid()
				&& aValue.Pixels.Dominant.isValid()
				&& aValue.Pixels.SampledPixels >= 24U
				&& aValue.Pixels.ContrastPixels >= 24U
				&& aValue.Pixels.DominantPixels >= 24U;
		};
		const auto selectedReadable = [&connectorValid](
										  const connector_render_observation& aValue) {
			return aValue.Selected && connectorValid(aValue);
		};
		const auto unselectedReadable = [&connectorValid](
											const connector_render_observation& aValue) {
			return !aValue.Selected && connectorValid(aValue)
				&& aValue.Pixels.ContrastPixels * 2U
				>= aValue.Pixels.SampledPixels
				&& aValue.Pixels.MedianContrastHundredths >= 300;
		};
		const auto statesDistinct = [](const connector_render_observation& aSelected,
									   const connector_render_observation& aUnselected) {
			return aSelected.Pixels.Background == aUnselected.Pixels.Background
				&& aSelected.Pixels.Dominant != aUnselected.Pixels.Dominant;
		};

		if (!navigate_to_group(*workbench, *result, *firstPair->Group)
			|| !reveal_intraline_pairs(
				*workbench, *result, *firstPair, *secondPair)) {
			return false;
		}
		drain();
		const auto firstSelected = observeTarget(
			"WS_TABSTOP,464,28,67,10", "WS_TABSTOP,464,24,67,10");
		const auto secondUnselected = observeTarget(
			"WS_TABSTOP,464,42,98,10", "WS_TABSTOP,464,36,98,10");
		const auto firstSelectedConnector    = firstSelected.has_value()
			? observe_connector_render(*workbench, *firstSelected)
			: std::optional<connector_render_observation>{ };
		const auto secondUnselectedConnector = secondUnselected.has_value()
			? observe_connector_render(*workbench, *secondUnselected)
			: std::optional<connector_render_observation>{ };
		if (!firstSelected.has_value() || !selected(*firstPair)
			|| firstSelected->LeftRolePixels == 0U
			|| firstSelected->RightRolePixels == 0U
			|| !secondUnselected.has_value()
			|| !firstSelectedConnector.has_value()
			|| !secondUnselectedConnector.has_value()
			|| !selectedReadable(*firstSelectedConnector)
			|| !unselectedReadable(*secondUnselectedConnector)) {
			bpr::app::detail::terminal_text_stream{ } << "UIFORETW_CAPTURE_FIRST_TARGET_FAILED\n";
			// [[BPR-1033]]. One term per clause of the condition above, in its
			// order, with the values each clause reads, so a failure names the
			// clause instead of the capture. After the marker, so every
			// earlier line keeps its place.
			auto       facts     = bpr::app::detail::terminal_text_stream{ };
			const auto rectangle = [&facts](const QRect& aRect) {
				facts << aRect.x() << ',' << aRect.y() << ',' << aRect.width() << ',' << aRect.height();
			};
			const auto target = [&facts, &rectangle](const char* const aName, const std::optional<intraline_render_observation>& aValue) {
				facts << ' ' << aName << "_observed=" << aValue.has_value();
				if (aValue.has_value()) {
					facts << ' ' << aName << "_left_rect=";
					rectangle(aValue->LeftRect);
					facts << ' ' << aName << "_right_rect=";
					rectangle(aValue->RightRect);
					facts << ' ' << aName << "_left_role_pixels=" << aValue->LeftRolePixels << ' ' << aName
						  << "_right_role_pixels=" << aValue->RightRolePixels;
				}
			};
			const auto color = [](const QColor& aColor) {
				return aColor.isValid() ? aColor.name(QColor::HexArgb) : QStringLiteral("invalid");
			};
			const auto connector = [&facts, &color](const char* const aName, const std::optional<connector_render_observation>& aValue) {
				facts << ' ' << aName << "_connector_observed=" << aValue.has_value();
				if (aValue.has_value()) {
					facts << ' ' << aName << "_connector=selected:" << aValue->Selected << ",endpoints:" << aValue->LeftEndpoint
						  << aValue->RightEndpoint << ",overlaps:" << aValue->LeftOverlap << aValue->RightOverlap
						  << ",background:" << color(aValue->Pixels.Background) << ",dominant:" << color(aValue->Pixels.Dominant)
						  << ",sampled:" << aValue->Pixels.SampledPixels << ",contrast:" << aValue->Pixels.ContrastPixels
						  << ",dominant_pixels:" << aValue->Pixels.DominantPixels
						  << ",median_contrast_hundredths:" << aValue->Pixels.MedianContrastHundredths;
				}
			};
			const auto workbenchSelection = workbench->selection().has_value()
				&& workbench->selection()->Segment == firstPair->Segment
				&& workbench->selection()->Group == firstPair->Group;
			facts << "UIFORETW_CAPTURE_FIRST_TARGET_FACTS:";
			target("first", firstSelected);
			facts << " first_selected=" << selected(*firstPair) << " workbench_selection=" << workbenchSelection
				  << " link_map_selection="
				  << (workbench->link_map()->selected()
					  == qt::diff_link_map_identity{
						  .Segment = firstPair->Segment,
						  .Group   = firstPair->Group,
					  });
			target("second", secondUnselected);
			// The connector clauses read the link map's visible links; a target
			// with no connector means the map shows no link for its segment.
			const auto* const map      = workbench->link_map();
			const auto        segments = [map](const intraline_pair_location& aPair) {
				return std::ranges::count_if(map->visible_links(), [&aPair](const qt::visible_diff_link& aLink) {
					return aLink.Identity.Segment == aPair.Segment;
				});
			};
			facts << " geometry_pending=" << workbench->geometry_pending() << " link_map=visible:" << map->isVisible()
				  << ",size:" << map->width() << 'x' << map->height()
				  << ",links:" << map->visible_links().size() << ",first_segment_links:" << segments(*firstPair)
				  << ",second_segment_links:" << segments(*secondPair) << ",selected:" << map->selected().has_value();
			connector("first", firstSelectedConnector);
			connector("second", secondUnselectedConnector);
			facts << " selected_readable="
				  << (firstSelectedConnector.has_value() && selectedReadable(*firstSelectedConnector))
				  << " unselected_readable="
				  << (secondUnselectedConnector.has_value() && unselectedReadable(*secondUnselectedConnector)) << '\n';
			return false;
		}

		if (!navigate_to_group(*workbench, *result, *secondPair->Group)
			|| !reveal_intraline_pairs(
				*workbench, *result, *firstPair, *secondPair)) {
			return false;
		}
		drain();
		const auto secondSelected = observeTarget(
			"WS_TABSTOP,464,42,98,10", "WS_TABSTOP,464,36,98,10");
		const auto firstUnselected = observeTarget(
			"WS_TABSTOP,464,28,67,10", "WS_TABSTOP,464,24,67,10");
		const auto secondSelectedConnector  = secondSelected.has_value()
			? observe_connector_render(*workbench, *secondSelected)
			: std::optional<connector_render_observation>{ };
		const auto firstUnselectedConnector = firstUnselected.has_value()
			? observe_connector_render(*workbench, *firstUnselected)
			: std::optional<connector_render_observation>{ };
		if (!secondSelected.has_value() || !selected(*secondPair)
			|| secondSelected->LeftRolePixels == 0U
			|| secondSelected->RightRolePixels == 0U
			|| !firstUnselected.has_value()
			|| !secondSelectedConnector.has_value()
			|| !firstUnselectedConnector.has_value()
			|| !selectedReadable(*secondSelectedConnector)
			|| !unselectedReadable(*firstUnselectedConnector)
			|| !statesDistinct(
				*firstSelectedConnector, *firstUnselectedConnector)
			|| !statesDistinct(
				*secondSelectedConnector, *secondUnselectedConnector)) {
			bpr::app::detail::terminal_text_stream{ } << "UIFORETW_CAPTURE_SECOND_TARGET_FAILED\n";
			return false;
		}

		if (!navigate_to_group(*workbench, *result, *firstPair->Group)
			|| !reveal_intraline_pairs(
				*workbench, *result, *firstPair, *secondPair)) {
			return false;
		}
		drain();
		const auto first = observeTarget(
			"WS_TABSTOP,464,28,67,10", "WS_TABSTOP,464,24,67,10");
		const auto second = observeTarget(
			"WS_TABSTOP,464,42,98,10", "WS_TABSTOP,464,36,98,10");
		const auto firstConnector  = first.has_value()
			? observe_connector_render(*workbench, *first)
			: std::optional<connector_render_observation>{ };
		const auto secondConnector = second.has_value()
			? observe_connector_render(*workbench, *second)
			: std::optional<connector_render_observation>{ };
		if (!intraline_roles_visible(
				*workbench, *implementation->AppearanceProfile, 2U)
			|| !first.has_value() || !second.has_value()
			|| first->LeftRolePixels == 0U || first->RightRolePixels == 0U
			|| second->LeftRolePixels == 0U || second->RightRolePixels == 0U
			|| !firstConnector.has_value() || !secondConnector.has_value()
			|| !selectedReadable(*firstConnector)
			|| !unselectedReadable(*secondConnector) || !selected(*firstPair)) {
			bpr::app::detail::terminal_text_stream{ } << "UIFORETW_CAPTURE_FINAL_TARGET_FAILED\n";
			return false;
		}

		auto* const detail             = workbench->detail_pane();
		auto* const comparisonSurface  = workbench->comparison_splitter();
		auto* const linkMap            = workbench->link_map();
		auto* const toolbar            = window->primary_toolbar();
		auto* const reviewBar          = window->review_bar();
		auto* const filesAction        = window->files_drawer_toggle_action();
		auto* const repositoriesAction = window->repositories_drawer_toggle_action();
		auto* const previousFile       = window->previous_file_action();
		auto* const nextFile           = window->next_file_action();
		auto* const previousChange     = window->previous_change_action();
		auto* const nextChange         = window->next_change_action();
		auto* const conversationsAction
			= window->conversations_drawer_toggle_action();
		auto* const reviewDetails  = window->review_details_toggle_action();
		auto* const reviewControls = window->review_controls_button();
		if (window->epoch_selectors()->details_visible()
			|| detail == nullptr || detail->isVisible()
			|| window->files_drawer()->width()
				> qt::workbench_drawer_rail::width_pixels
			|| window->conversations_drawer()->width()
				> qt::workbench_drawer_rail::width_pixels
			|| comparisonSurface == nullptr || linkMap == nullptr
			|| toolbar == nullptr || reviewBar == nullptr || filesAction == nullptr || repositoriesAction == nullptr
			|| previousFile == nullptr || nextFile == nullptr
			|| previousChange == nullptr || nextChange == nullptr
			|| conversationsAction == nullptr || reviewDetails == nullptr
			|| reviewControls == nullptr) {
			return false;
		}
		const auto requiredActions = std::array{
			filesAction,
			previousFile,
			nextFile,
			previousChange,
			nextChange,
			conversationsAction,
		};
		// [[BPR-1129]]. The drawer toggles are the title row's; the navigation
		// is the review bar's, under it.
		auto requiredPresenters = std::array<QToolButton*, 6U>{ };
		for (std::size_t index = 0U; index < requiredActions.size(); ++index) {
			const auto* const row     = index == 0U || index + 1U == requiredActions.size()
				? static_cast<const QToolBar*>(toolbar)
				: reviewBar;
			requiredPresenters[index] = qobject_cast<QToolButton*>(
				row->widgetForAction(requiredActions[index]));
			if (requiredPresenters[index] == nullptr) {
				return false;
			}
		}
		const auto navigationIconObservations = std::array{
			observe_navigation_icon_contrast(*requiredPresenters[1]),
			observe_navigation_icon_contrast(*requiredPresenters[2]),
			observe_navigation_icon_contrast(*requiredPresenters[3]),
			observe_navigation_icon_contrast(*requiredPresenters[4]),
		};
		const auto normalIconContrastFloor
			= aMode == core::appearance_mode::high_contrast_light
				|| aMode == core::appearance_mode::high_contrast_dark
			? std::int64_t{ 700 }
			: std::int64_t{ 450 };
		const auto navigationIconsPaletteAware = std::ranges::all_of(
			navigationIconObservations,
			[normalIconContrastFloor](
				const navigation_icon_contrast_observation& aObservation) {
				return aObservation.NormalForegroundPixels > 0U
					&& aObservation.DisabledForegroundPixels > 0U
					&& aObservation.NormalContrastHundredths
					>= normalIconContrastFloor
					&& aObservation.DisabledContrastHundredths >= 300
					&& aObservation.StatesDistinct;
			});
		const auto toolbarActions   = toolbar->actions();
		const auto reviewBarActions = reviewBar->actions();
		const auto widgetActionNamed
			= [](const QList<QAction*>& aActions, const qsizetype aIndex, const char* const aName) {
				  return aIndex >= 0 && aIndex < aActions.size()
					  && aActions[aIndex]->objectName() == QLatin1String{ aName };
			  };
		// [[BPR-1129]]. RFC-BPR-0052's title row, left to right: the platform
		// inset, Home, repositories, files, the name, Open on GitHub right
		// after it ([[BPR-1338]]), a spacer, the modality
		// control, Home's View ([[BPR-1580]], hidden outside Home), refresh,
		// Home's discovery refresh (hidden outside Home),
		// comments, Hide Drawers ([[BPR-1295]]), a rule, account, settings.
		// A hidden action is still one of the row's actions: the row pins order,
		// not what is shown.
		//
		// [[BPR-0653]]. On Windows the title row carries two more actions: a
		// compact menu control takes index 1, right after the inset, which
		// shifts every later action along by one, and the caption buttons end
		// the row after settings. Hence the lead and trailing offsets rather
		// than a second copy of the row: the row stays asserted whole, in order
		// and exactly, on every platform.
	#if defined(_WIN32)
		constexpr auto lead     = qsizetype{ 1 };
		constexpr auto trailing = qsizetype{ 1 };
	#else
		constexpr auto lead     = qsizetype{ 0 };
		constexpr auto trailing = qsizetype{ 0 };
	#endif
		const auto at
			= [](const qsizetype aIndex) { return aIndex + lead; };
		const auto named = [&widgetActionNamed, &toolbarActions](
							   const qsizetype aIndex, const char* const aName) {
			return widgetActionNamed(toolbarActions, aIndex + lead, aName);
		};
		const auto exactRowOrder = toolbarActions.size() == 17 + lead + trailing
			&& widgetActionNamed(toolbarActions, 0, "workbenchStripInsetAction")
	#if defined(_WIN32)
			&& widgetActionNamed(toolbarActions, 1, "workbenchStripMenuAction")
	#endif
			&& toolbarActions[at(1)] == window->home_action()
			&& toolbarActions[at(2)] == repositoriesAction
			&& toolbarActions[at(3)] == filesAction
			&& named(4, "workbenchStripIdentityAction")
			// [[BPR-1852]]. A draft's chip, hidden unless the pull request is one.
			&& named(5, "workbenchStripDraftChipAction")
			&& named(6, "workbenchOpenOnGithubAction")
			&& named(7, "workbenchNavigationSpacerAction")
			&& named(8, "workbenchModalityAction")
			&& named(9, "homeViewAction")
			&& toolbarActions[at(10)] == window->github_refresh_action()
			&& toolbarActions[at(11)] == window->queue_refresh_action()
			&& toolbarActions[at(12)] == conversationsAction
			&& toolbarActions[at(13)] == window->toggle_drawers_action()
			&& named(14, "workbenchStripRightDividerAction")
			&& toolbarActions[at(15)] == window->account_action()
			&& toolbarActions[at(16)] == window->settings_action()
	#if defined(_WIN32)
			&& named(17, "workbenchCaptionButtonsAction")
	#endif
			;
		// [[BPR-1129]]. The review bar, left to right: an inset, Updates, the
		// comparison's menu and a rule, the file pair with its position, the
		// change pair with its position, a gap, the progress track holding
		// Seen, Read, Accept file ([[BPR-1507]]) and private progress on one
		// rail ([[BPR-1578]]), a spacer, the
		// review chip ([[BPR-1345]], hidden with no review in progress),
		// Compose review and Comment. Review details is the comparison's
		// menu's.
		const auto* const acceptFile          = window->findChild<QToolButton*>(QStringLiteral("workbenchAcceptFileButton"));
		const auto        exactReviewBarOrder = reviewBarActions.size() == 19
			&& widgetActionNamed(reviewBarActions, 0, "reviewBarInsetAction")
			&& widgetActionNamed(reviewBarActions, 1, "reviewBarUpdatesAction")
			&& widgetActionNamed(reviewBarActions, 2, "reviewBarComparingAction")
			&& widgetActionNamed(reviewBarActions, 3, "reviewBarComparisonDividerAction")
			&& widgetActionNamed(reviewBarActions, 4, "workbenchStripFilePositionAction")
			&& reviewBarActions[5] == previousFile
			&& reviewBarActions[6] == nextFile
			&& widgetActionNamed(reviewBarActions, 7, "workbenchStripChangePositionAction")
			&& reviewBarActions[8] == previousChange
			&& reviewBarActions[9] == nextChange
			&& widgetActionNamed(reviewBarActions, 10, "reviewBarProgressGapAction")
			&& widgetActionNamed(reviewBarActions, 11, "workbenchProgressTrackAction")
			&& acceptFile != nullptr && acceptFile->parentWidget() == reviewBar->widgetForAction(reviewBarActions[11])
			&& acceptFile->defaultAction() == window->accept_action()
			&& reviewControls->parentWidget() == reviewBar->widgetForAction(reviewBarActions[11])
			// [[BPR-1815]]. A rule, the layout toggle and Go to header/source.
			&& widgetActionNamed(reviewBarActions, 12, "reviewBarViewDividerAction")
			&& reviewBarActions[13] == window->one_pane_action()
			&& reviewBarActions[14] == window->header_source_action()
			&& widgetActionNamed(reviewBarActions, 15, "reviewBarSpacerAction")
			&& reviewBarActions[16] == window->review_chip_action()
			&& reviewBarActions[17] == window->compose_review_action()
			&& reviewBarActions[18] == window->comment_action()
			&& window->epoch_selectors()->comparing_button()->menu()->actions().contains(reviewDetails);
		if (!exactRowOrder || !exactReviewBarOrder) {
			// [[BPR-0859]]. An exact row order that no longer holds names the
			// rows it did find, so the difference is read from the run rather
			// than from some thirty conjuncts in a debugger.
			auto stream = bpr::app::detail::terminal_text_stream{ };
			stream << "UIFORETW_NAVIGATION_ROW_UNEXPECTED:actions="
				   << toolbarActions.size() << ",review_bar=" << reviewBarActions.size() << '\n';
			for (auto index = qsizetype{ 0 }; index < toolbarActions.size();
				 ++index) {
				stream << "UIFORETW_NAVIGATION_ROW_ACTION:" << index << '='
					   << (toolbarActions[index]->objectName().isEmpty()
							   ? QStringLiteral("<unnamed>")
							   : toolbarActions[index]->objectName())
					   << '\n';
			}
			for (auto index = qsizetype{ 0 }; index < reviewBarActions.size(); ++index) {
				stream << "UIFORETW_REVIEW_BAR_ACTION:" << index << '='
					   << (reviewBarActions[index]->objectName().isEmpty()
							   ? QStringLiteral("<unnamed>")
							   : reviewBarActions[index]->objectName())
					   << '\n';
			}
		}
		// [[BPR-1033]]. On Windows the caption buttons end the row, after
		// settings, so the row's right edge holds them too. The contract takes
		// their width off its right-edge gap, which keeps one bound for what
		// every platform puts there. Empty where the row has no window controls
		// at its right.
	#if defined(_WIN32)
		const auto windowControlsInRow = exactRowOrder
			? toolbar->actionGeometry(toolbarActions.constLast())
			: QRect{ };
	#else
		const auto windowControlsInRow = QRect{ };
	#endif
		auto iconOnly    = toolbar->toolButtonStyle() == Qt::ToolButtonIconOnly && reviewBar->toolButtonStyle() == Qt::ToolButtonIconOnly;
		auto nativeIcons = true;
		auto accessible  = true;
		auto uniqueNames = true;
		for (std::size_t index = 0U; index < requiredActions.size(); ++index) {
			const auto* const action = requiredActions[index];
			const auto* const button = requiredPresenters[index];
			iconOnly                 = iconOnly
				&& button->toolButtonStyle() == Qt::ToolButtonIconOnly;
			nativeIcons = nativeIcons && !action->icon().isNull()
				&& !button->icon().isNull();
			accessible = accessible && !button->accessibleName().isEmpty()
				&& !button->accessibleDescription().isEmpty()
				&& !action->toolTip().isEmpty() && !action->shortcut().isEmpty()
				&& button->focusPolicy() == Qt::StrongFocus;
			for (std::size_t other = index + 1U;
				 other < requiredActions.size(); ++other) {
				uniqueNames = uniqueNames
					&& requiredPresenters[index]->accessibleName()
						!= requiredPresenters[other]->accessibleName()
					&& requiredActions[index]->objectName()
						!= requiredActions[other]->objectName();
			}
		}
		const auto inWindow = [window](const QWidget& aWidget) {
			return QRect{
				aWidget.mapTo(window, QPoint{ 0, 0 }),
				aWidget.size(),
			};
		};
		// [[BPR-1130]]. A new window starts with the Updates line folded, and
		// RFC-BPR-0052 AC-11 takes the stack both ways. The line opens through
		// the window's own command, so the core decides it and the host
		// publishes it; it is measured open and folded again before anything
		// below is captured.
		const auto initiallyFolded = !window->updates_line_open() && !window->epoch_selectors()->isVisible();
		const auto settleFold      = [window, &drain](const bool aOpen) {
			const auto started = std::chrono::steady_clock::now();
			while (window->updates_line_open() != aOpen
				   && std::chrono::steady_clock::now() - started < std::chrono::seconds{ 5 }) {
				QApplication::processEvents();
			}
			drain();
			return window->updates_line_open() == aOpen && window->epoch_selectors()->isVisible() == aOpen;
		};
		window->updates_line_action()->trigger();
		const auto opened         = settleFold(true);
		const auto openLineRect   = inWindow(*window->epoch_selectors());
		const auto openComparison = inWindow(*comparisonSurface);
		auto       openStack      = QString{ };
		{
			auto stream = QTextStream{ &openStack };
			report_capture_layout(stream, *window, *comparisonSurface);
		}
		window->updates_line_action()->trigger();
		const auto refolded = settleFold(false);
		if (!initiallyFolded || !opened || !refolded) {
			bpr::app::detail::terminal_text_stream{ } << "UIFORETW_UPDATES_LINE_FOLD_FAILED:initially_folded=" << initiallyFolded
													  << " opened=" << opened << " refolded=" << refolded << '\n';
			return false;
		}
		// [[BPR-1413]]. A pane moves its viewport up by the within-line
		// offset and makes it a line taller, masked to the pane's content
		// area. The pane the reviewer sees is that mask, as BPR-1413's own
		// tests read it; the widget itself reaches past the comparison.
		const auto visiblePane = [&inWindow, window](const QWidget& aViewport) {
			const auto mask = aViewport.mask();
			if (mask.isEmpty()) {
				return inWindow(aViewport);
			}
			const auto area = mask.boundingRect();
			return QRect{ aViewport.mapTo(window, area.topLeft()), area.size() };
		};
		const auto comparisonRect = inWindow(*comparisonSurface);
		const auto leftPaneRect   = visiblePane(*workbench->left_pane()->viewport());
		const auto rightPaneRect  = visiblePane(*workbench->right_pane()->viewport());
		const auto gutterRect     = inWindow(*linkMap);
		const auto widthRatio     = (1'000 * comparisonRect.width())
			/ std::max(1, window->width());
		const auto heightRatio = (1'000 * comparisonRect.height())
			/ std::max(1, window->height());
		const auto openHeightRatio = (1'000 * openComparison.height())
			/ std::max(1, window->height());
		if (widthRatio < uiforetw_capture_width_floor
			|| heightRatio < uiforetw_capture_folded_height_floor
			|| openHeightRatio < uiforetw_capture_open_height_floor
			|| comparisonRect.height() - openComparison.height() != openLineRect.height()) {
			// [[BPR-1036]]. Name what took the room, not only how much is left:
			// the folded stack, then the open one.
			auto stream = bpr::app::detail::terminal_text_stream{ };
			stream << "UIFORETW_CAPTURE_LAYOUT_FAILED:" << widthRatio << ':'
				   << heightRatio << ":open=" << openHeightRatio;
			report_capture_layout(stream, *window, *comparisonSurface);
			stream << " open_line=" << openLineRect.top() << '+' << openLineRect.height()
				   << " open:" << openStack << ' ' << course_modality_facts(aApplication) << '\n';
			return false;
		}

		const auto path = QString::fromUtf8(
			aDirectory.data(), static_cast<qsizetype>(aDirectory.size()));
		if (!QDir{ }.mkpath(path)) {
			return false;
		}
		const auto directory  = QDir{ path };
		const auto full       = window->grab();
		const auto comparison = comparisonSurface->grab();
		if (full.isNull() || comparison.isNull()
			|| !full.save(directory.filePath(QStringLiteral("full-window.png")), "PNG")
			|| !comparison.save(
				directory.filePath(QStringLiteral("intraline-comparison.png")),
				"PNG")) {
			return false;
		}

		const auto removed = implementation->AppearanceProfile->color(
			core::appearance_role::diff_intraline_removed_background);
		const auto added = implementation->AppearanceProfile->color(
			core::appearance_role::diff_intraline_added_background);
		const auto& key      = implementation->LastSelectedDiff->Key;
		const auto  blobText = [](const core::git_blob_oid& aOid) {
			const auto text = core::canonical_hex(aOid);
			return std::string{ text.view() };
		};
		const auto identityText = []<typename Identity>(const Identity& aIdentity) {
			const auto& bytes = aIdentity.bytes();
			return QString::fromLatin1(
					   QByteArray{
						   reinterpret_cast<const char*>(bytes.data()),
						   static_cast<qsizetype>(bytes.size()) }
						   .toHex())
				.toStdString();
		};
		const auto connectorJson = [](const connector_render_observation& aValue) {
			return capture_object(
				"state",
				aValue.Selected ? "selected"
								: "unselected",
				"gutter_background",
				aValue.Pixels.Background.name(QColor::HexArgb).toStdString(),
				"dominant_path_pixel",
				aValue.Pixels.Dominant.name(QColor::HexArgb).toStdString(),
				"sampled_path_pixels",
				static_cast<qint64>(aValue.Pixels.SampledPixels),
				"contrast_pixels",
				static_cast<qint64>(aValue.Pixels.ContrastPixels),
				"dominant_path_pixels",
				static_cast<qint64>(aValue.Pixels.DominantPixels),
				"contrast_floor_hundredths",
				300,
				"median_contrast_hundredths",
				static_cast<qint64>(aValue.Pixels.MedianContrastHundredths),
				"bounds",
				rectangle_json(aValue.Bounds),
				"start",
				point_json(aValue.Start),
				"end",
				point_json(aValue.End),
				"left_endpoint_at_pane_edge",
				aValue.LeftEndpoint,
				"right_endpoint_at_pane_edge",
				aValue.RightEndpoint,
				"left_target_row_overlap",
				aValue.LeftOverlap,
				"right_target_row_overlap",
				aValue.RightOverlap);
		};
		const auto firstGroup = std::ranges::find(
			result->Alignment.Groups, *firstPair->Group, &core::change_group::Id);
		const auto secondGroup = std::ranges::find(
			result->Alignment.Groups, *secondPair->Group, &core::change_group::Id);
		if (firstGroup == result->Alignment.Groups.end()
			|| secondGroup == result->Alignment.Groups.end()) {
			return false;
		}
		const auto targetJson = [&identityText, &connectorJson](
									const intraline_render_observation& aTarget,
									const intraline_render_observation& aSelectedTarget,
									const connector_render_observation& aConnector,
									const connector_render_observation& aSelectedConnector,
									const connector_render_observation& aUnselectedConnector,
									const qint64                        aChangeIndex) {
			return capture_object(
				"segment",
				identityText(aTarget.Pair.Segment),
				"group",
				aTarget.Pair.Group.has_value()
					? identityText(*aTarget.Pair.Group)
					: std::string{ },
				"left_line",
				static_cast<qint64>(aTarget.Pair.Left),
				"right_line",
				static_cast<qint64>(aTarget.Pair.Right),
				"left_rect_in_pane",
				rectangle_json(aTarget.LeftRect),
				"right_rect_in_pane",
				rectangle_json(aTarget.RightRect),
				"left_role_pixels",
				static_cast<qint64>(aTarget.LeftRolePixels),
				"right_role_pixels",
				static_cast<qint64>(aTarget.RightRolePixels),
				"selected_observed",
				true,
				"selected_left_role_pixels",
				static_cast<qint64>(aSelectedTarget.LeftRolePixels),
				"selected_right_role_pixels",
				static_cast<qint64>(aSelectedTarget.RightRolePixels),
				"change_index",
				aChangeIndex,
				"connector",
				connectorJson(aConnector),
				"selected_connector",
				connectorJson(aSelectedConnector),
				"unselected_connector",
				connectorJson(aUnselectedConnector));
		};
		const auto firstChangeIndex = static_cast<qint64>(
			std::distance(result->Alignment.Groups.begin(), firstGroup));
		const auto secondChangeIndex = static_cast<qint64>(
			std::distance(result->Alignment.Groups.begin(), secondGroup));
		const auto iconContrastJson = [](
										  const navigation_icon_contrast_observation& aValue) {
			return capture_object(
				"background",
				aValue.Background.name(QColor::HexArgb).toStdString(),
				"normal_foreground",
				aValue.NormalForeground.name(QColor::HexArgb).toStdString(),
				"disabled_foreground",
				aValue.DisabledForeground.name(QColor::HexArgb).toStdString(),
				"normal_foreground_pixels",
				static_cast<qint64>(aValue.NormalForegroundPixels),
				"disabled_foreground_pixels",
				static_cast<qint64>(aValue.DisabledForegroundPixels),
				"normal_contrast_hundredths",
				static_cast<qint64>(aValue.NormalContrastHundredths),
				"disabled_contrast_hundredths",
				static_cast<qint64>(aValue.DisabledContrastHundredths),
				"states_distinct",
				aValue.StatesDistinct);
		};
		auto facts = capture_object(
			"schema",
			"bpr.uiforetw_intraline_capture.v7",
			"appearance",
			appearance_mode_name(aMode).toStdString(),
			"qt_platform",
			QGuiApplication::platformName().toStdString(),
			"qt_version",
			std::string{ qVersion() },
			"style",
			std::string{ window->style()->metaObject()->className() },
			"device_pixel_ratio",
			full.devicePixelRatio(),
			"logical_width",
			window->width(),
			"logical_height",
			window->height(),
			"pixel_width",
			full.width(),
			"pixel_height",
			full.height(),
			"comparison_pixel_width",
			comparison.width(),
			"comparison_pixel_height",
			comparison.height(),
			"selected_path",
			std::string{ uiforetw_rc_path },
			"base_commit",
			std::string{ uiforetw_base_oid },
			"head_commit",
			std::string{ uiforetw_head_oid },
			"base_blob",
			blobText(*key.LeftBlob),
			"head_blob",
			blobText(*key.RightBlob),
			"removed_role",
			removed.name(QColor::HexArgb).toStdString(),
			"added_role",
			added.name(QColor::HexArgb).toStdString(),
			"intraline_edit_count",
			static_cast<qint64>(workbench->current_result()->Intraline.size()),
			"fixture_diff_helper_invocations",
			static_cast<qint64>(implementation->FixtureDiffHelperInvocations),
			"previous_file_navigation_exercised",
			implementation->UIforEtwSelectedDiffProbe->Evidence.PreviousFileNavigation,
			"next_file_navigation_exercised",
			implementation->UIforEtwSelectedDiffProbe->Evidence.NextFileNavigation,
			"previous_change_navigation_exercised",
			implementation->UIforEtwSelectedDiffProbe->Evidence.PreviousChangeNavigation,
			"next_change_navigation_exercised",
			implementation->UIforEtwSelectedDiffProbe->Evidence.NextChangeNavigation,
			"files_drawer_toggle_exercised",
			implementation->UIforEtwSelectedDiffProbe->Evidence.FilesDrawerToggle,
			"conversations_drawer_toggle_exercised",
			implementation->UIforEtwSelectedDiffProbe->Evidence.ConversationsDrawerToggle,
			"repositories_drawer_toggle_exercised",
			implementation->UIforEtwSelectedDiffProbe->Evidence.RepositoriesDrawerToggle,
			"title_row_exact_order",
			exactRowOrder,
			"review_bar_exact_order",
			exactReviewBarOrder,
			"navigation_actions_icon_only",
			iconOnly,
			"navigation_actions_have_native_icons",
			nativeIcons,
			"navigation_icons_palette_aware",
			navigationIconsPaletteAware,
			"navigation_icon_contrast",
			capture_object(
				"previous_file",
				iconContrastJson(navigationIconObservations[0]),
				"next_file",
				iconContrastJson(navigationIconObservations[1]),
				"previous_change",
				iconContrastJson(navigationIconObservations[2]),
				"next_change",
				iconContrastJson(navigationIconObservations[3])),
			"navigation_actions_accessible",
			accessible,
			"navigation_accessible_names_unique",
			uniqueNames,
			"files_drawer_visible",
			false,
			"conversations_drawer_visible",
			false,
			"review_details_visible",
			false,
			"epoch_controls_visible",
			false,
			"selected_change_detail_visible",
			false,
			"full_window_rect",
			rectangle_json(window->rect()),
			"title_row_rect_in_window",
			rectangle_json(inWindow(*toolbar)),
			"review_bar_rect_in_window",
			rectangle_json(inWindow(*reviewBar)),
			"files_toggle_rect_in_title_row",
			rectangle_json(requiredPresenters[0]->geometry()),
			"conversations_toggle_rect_in_title_row",
			rectangle_json(requiredPresenters[5]->geometry()),
			"hide_drawers_rect_in_title_row",
			rectangle_json(toolbar->actionGeometry(window->toggle_drawers_action())),
			"window_controls_rect_in_title_row",
			rectangle_json(windowControlsInRow),
			"comparing_rect_in_bar",
			rectangle_json(window->epoch_selectors()->comparing_button()->geometry()),
			"previous_change_rect_in_bar",
			rectangle_json(requiredPresenters[3]->geometry()),
			"next_change_rect_in_bar",
			rectangle_json(requiredPresenters[4]->geometry()),
			"previous_file_rect_in_bar",
			rectangle_json(requiredPresenters[1]->geometry()),
			"next_file_rect_in_bar",
			rectangle_json(requiredPresenters[2]->geometry()),
			"private_progress_rect_in_bar",
			// [[BPR-1578]]. Private progress is the progress track's last cell,
			// so its rectangle is mapped from the track into the bar.
			rectangle_json(QRect{ reviewControls->mapTo(reviewBar, QPoint{ }), reviewControls->size() }),
			"compose_review_rect_in_bar",
			rectangle_json(reviewBar->actionGeometry(window->compose_review_action())),
			"comment_rect_in_bar",
			rectangle_json(reviewBar->actionGeometry(window->comment_action())),
			"review_details_in_comparison_menu",
			window->epoch_selectors()->comparing_button()->menu()->actions().contains(reviewDetails),
			"comparison_rect_in_window",
			rectangle_json(comparisonRect),
			"left_pane_rect_in_window",
			rectangle_json(leftPaneRect),
			"right_pane_rect_in_window",
			rectangle_json(rightPaneRect),
			"gutter_rect_in_window",
			rectangle_json(gutterRect),
			"files_drawer_rect_in_splitter",
			rectangle_json(window->files_drawer()->geometry()),
			"conversations_drawer_rect_in_splitter",
			rectangle_json(window->conversations_drawer()->geometry()),
			"repositories_drawer_rect_in_splitter",
			rectangle_json(window->repositories_drawer()->geometry()),
			"review_details_rect_in_window",
			rectangle_json(inWindow(*window->epoch_selectors())),
			"comparison_width_ratio_permille",
			widthRatio,
			"comparison_height_ratio_permille",
			heightRatio,
			"updates_line_initially_folded",
			initiallyFolded,
			"updates_line_open_rect_in_window",
			rectangle_json(openLineRect),
			"updates_line_open_comparison_height",
			openComparison.height(),
			"updates_line_open_height_ratio_permille",
			openHeightRatio,
			"document_tabs_rect_in_window",
			rectangle_json(inWindow(*window->document_tabs())),
			"left_horizontal_offset",
			workbench->left_pane()->horizontalScrollBar()->value(),
			"right_horizontal_offset",
			workbench->right_pane()->horizontalScrollBar()->value(),
			"selected_change_index",
			firstChangeIndex,
			"selected_segment",
			identityText(first->Pair.Segment),
			"selected_group",
			identityText(*first->Pair.Group),
			"replacement_28_to_24",
			targetJson(*first, *firstSelected, *firstConnector, *firstSelectedConnector, *firstUnselectedConnector, firstChangeIndex),
			"replacement_42_to_36",
			targetJson(*second, *secondSelected, *secondConnector, *secondSelectedConnector, *secondUnselectedConnector, secondChangeIndex));
		auto file = QFile{ directory.filePath("capture-facts.json") };
		if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			return false;
		}
		std::string bytes;
		if (glz::write<capture_json_options{ }>(facts, bytes)) {
			return false;
		}
		bytes.push_back('\n');
		return file.write(bytes.data(), static_cast<qint64>(bytes.size())) == static_cast<qint64>(bytes.size()) && file.flush();
	}
#endif
}
