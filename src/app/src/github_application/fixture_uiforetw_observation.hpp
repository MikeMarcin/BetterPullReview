#ifndef BPR_APP_GITHUB_APPLICATION_FIXTURE_UIFORETW_OBSERVATION_HPP
#define BPR_APP_GITHUB_APPLICATION_FIXTURE_UIFORETW_OBSERVATION_HPP
#if defined(BPR_BUILD_TESTING)
	#include "bpr/core/appearance_model.hpp"
	#include "bpr/core/diff_compute.hpp"
	#include "bpr/core/diff_model.hpp"
	#include "bpr/core/epoch_model.hpp"
	#include "bpr/qt/appearance_profile.hpp"
	#include "bpr/qt/diff_workbench.hpp"
	#include <QColor>
	#include <QPlainTextEdit>
	#include <QRect>
	#include <QRectF>
	#include <array>
	#include <cstddef>
	#include <filesystem>
	#include <glaze/json.hpp>
	#include <optional>
	#include <span>
	#include <string_view>
#endif

// The UIforETW fixture review's blob identities and the intraline, role and
// navigation observations its course and its capture both make.
namespace bpr::app {
#if defined(BPR_BUILD_TESTING)
	inline constexpr std::string_view uiforetw_base_oid
		= "8b70ee179fe55b705ee4e613ecbc89a0acebc1bc";

	inline constexpr std::string_view uiforetw_head_oid
		= "e40058be7d5fe319342dffb567ce30e42bc6f4de";

	inline constexpr std::string_view uiforetw_rc_path
		= "UIforETW/UIforETW.rc";

	inline constexpr std::string_view uiforetw_rc_base_blob
		= "b995e189fb58da146945e1fb22380cdf4a7f4e47";

	inline constexpr std::string_view uiforetw_rc_head_blob
		= "6335c1b2e14cd5e63142ce1dfc283fcf538fb134";

	[[nodiscard]] auto decoded_line(
		const core::decoded_text_document& aDocument,
		const std::uint32_t                aLine) -> std::string_view;

	struct intraline_pair_location
	{
		std::uint32_t                        Left  = 0U;
		std::uint32_t                        Right = 0U;
		core::byte_span                      LeftSpan;
		core::byte_span                      RightSpan;
		core::alignment_segment_id           Segment;
		std::optional<core::change_group_id> Group;
	};

	[[nodiscard]] auto find_intraline_pair(
		const core::computed_file_diff& aResult,
		const std::string_view          aLeftFragment,
		const std::string_view          aRightFragment)
		-> std::optional<intraline_pair_location>;

	[[nodiscard]] auto character_offset(
		const std::span<const std::uint8_t> aBytes,
		const std::uint64_t                 aOffset) -> int;

	[[nodiscard]] auto intraline_selection_rect(
		QPlainTextEdit&                    aPane,
		const core::decoded_text_document& aDocument,
		const std::uint32_t                aLine,
		const core::byte_span              aSpan) -> std::optional<QRect>;

	/// \brief Scrolls both panes until two intraline pairs are on screen.
	///
	/// \details [[BPR-0859]]. `aReport` names what stayed off screen. A
	/// caller that waits for the reveal reports only on the tick it gives
	/// up, so a run that settles on its third tick stays quiet.
	[[nodiscard]] auto reveal_intraline_pairs(
		qt::diff_workbench&             aWorkbench,
		const core::computed_file_diff& aResult,
		const intraline_pair_location&  aFirst,
		const intraline_pair_location&  aSecond,
		bool                            aReport = true) -> bool;

	[[nodiscard]] auto navigate_to_group(
		qt::diff_workbench&             aWorkbench,
		const core::computed_file_diff& aResult,
		const core::change_group_id&    aTarget) -> bool;

	[[nodiscard]] auto reveal_intraline_pairs(
		qt::diff_workbench&             aWorkbench,
		const core::computed_file_diff& aResult,
		const intraline_pair_location&  aFirst,
		const intraline_pair_location&  aSecond,
		const bool                      aReport) -> bool;

	/// \brief The colours one intraline role can legitimately paint as.
	///
	/// \details BPR-0321 composites the row's selection wash into the
	/// emphasis, so one semantic role renders as its palette colour on an
	/// unselected row and as that colour under the wash on a selected one.
	/// Both are the role. A check that accepted only the bare colour would
	/// read a correctly washed mark as a missing one, which is exactly how
	/// this course failed when the wash first landed.
	[[nodiscard]] auto role_renderings(
		const QPlainTextEdit&          aPane,
		const QColor&                  aRole,
		const QColor&                  aRowBand,
		const core::appearance_variant aVariant) -> std::array<QColor, 3U>;

	[[nodiscard]] auto role_paint_count(
		const QPlainTextEdit&          aPane,
		const QColor&                  aExpected,
		const QColor&                  aRowBand,
		const core::appearance_variant aVariant) -> std::size_t;

	[[nodiscard]] auto intraline_roles_visible(
		const qt::diff_workbench&     aWorkbench,
		const qt::appearance_profile& aProfile,
		const std::size_t             aMinimum) -> bool;

	template <typename... Values>
	[[nodiscard]] auto capture_object(Values... aValues)
	{
		return glz::obj_copy<Values...>{ .value = glz::make_tuple(std::move(aValues)...) };
	}

	[[nodiscard]] inline auto rectangle_json(const QRect& aRect)
	{
		return capture_object(
			"x",
			aRect.x(),
			"y",
			aRect.y(),
			"width",
			aRect.width(),
			"height",
			aRect.height());
	}

	[[nodiscard]] inline auto rectangle_json(const QRectF& aRect)
	{
		return capture_object(
			"x",
			aRect.x(),
			"y",
			aRect.y(),
			"width",
			aRect.width(),
			"height",
			aRect.height());
	}

	[[nodiscard]] auto selected_diff_publication_matches(
		const auto&            aImplementation,
		const std::string_view aPath,
		const std::string_view aBaseBlob,
		const std::string_view aHeadBlob) -> bool
	{
		if (!aImplementation.LastSelectedDiff.has_value()) {
			return false;
		}
		const auto  left        = core::parse_git_blob_oid(aBaseBlob);
		const auto  right       = core::parse_git_blob_oid(aHeadBlob);
		const auto& publication = *aImplementation.LastSelectedDiff;
		return left.has_value() && right.has_value()
			&& publication.Request.File.PresentationPath == aPath
			&& publication.Request.File.CurrentPath
			== std::filesystem::path{ aPath }
		&& publication.Request.File.File == core::file_identity_of(aPath)
			&& publication.Key.LeftBlob == left
			&& publication.Key.RightBlob == right
			&& publication.Key.File == core::file_identity_of(aPath);
	}

	[[nodiscard]] auto selected_diff_matches(
		const auto&            aImplementation,
		const std::string_view aPath,
		const std::string_view aBaseBlob,
		const std::string_view aHeadBlob) -> bool
	{
		return selected_diff_publication_matches(
				   aImplementation, aPath, aBaseBlob, aHeadBlob)
			&& aImplementation.Window != nullptr
			&& aImplementation.Window->comparison_surface() != nullptr
			&& aImplementation.Window->comparison_surface()->current_result()
			!= nullptr
			&& aImplementation.Window->comparison_surface()
				   ->current_result()
				   ->Alignment.Key
			== aImplementation.LastSelectedDiff->Key;
	}
#endif
}
#endif
