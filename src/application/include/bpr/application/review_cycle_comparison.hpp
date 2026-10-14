#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_COMPARISON_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_COMPARISON_HPP

#include "bpr/application/diff_line_matcher.hpp"
#include "bpr/application/review_cycle_store.hpp"
#include "bpr/application/review_material_bridge.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/review_cycle_comparison.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Retained cycle material fed to comparison views (RFC-BPR-0054,
/// BPR-1151).
/// \details Every byte comes from the cycle store through the material
/// bridge, only for an object the named revision pinned. A missing or
/// corrupt object is reported as such; nothing is read from the workspace, so
/// later edits, deletions or a restart never change what a view shows.
/// Pages are bounded; file comparisons use the shared diff engine.
namespace bpr::application {
	struct cycle_comparison_page_observation
	{
		review_material_status                     Status = review_material_status::failure;
		std::optional<core::cycle_comparison_page> Page;
	};

	/// \brief One file of a view.
	struct cycle_file_observation
	{
		review_material_status                     Status = review_material_status::failure;
		std::optional<core::cycle_path_comparison> Path;
		/// Full source: the exact retained bytes of the one side.
		std::vector<std::uint8_t> Source;
		/// A pair: the shared engine's comparison of the two retained sides.
		std::optional<core::computed_material_diff> Diff;
		core::diff_reason_code                      Reason = core::diff_reason_code::none;
	};

	/// \brief Serves comparison pages and files from one catalog. Runs on the
	/// effect lane; the store must outlive it.
	class review_cycle_comparison_host
	{
	public:
		/// [[BPR-1485]] `aMatcher` computes the line matches of a pair's diff;
		/// without one, a pair's file is refused (`host_failure`).
		review_cycle_comparison_host(review_cycle_store& aStore, core::local_cycle_authority aAuthority,
									 std::shared_ptr<diff_line_matcher_port> aMatcher = nullptr) noexcept;

		/// \brief The decoded manifest a revision recorded, verified against
		/// the revision's recorded summary.
		auto manifest(const core::local_cycle_subject& aSubject, core::cycle_source_revision_id aRevision, std::stop_token aStop)
			-> std::expected<std::shared_ptr<const core::cycle_manifest>, review_material_status>;
		/// \brief One bounded page of a resolved pair.
		auto page(const core::local_cycle_subject& aSubject, const core::cycle_comparison_pair& aPair, const std::optional<std::string>& aAfter, std::size_t aLimit,
				  bool aChangedOnly, std::stop_token aStop) -> cycle_comparison_page_observation;
		/// \brief One file: full source for a single revision, or the shared
		/// engine's diff for a pair.
		auto file(const core::local_cycle_subject& aSubject, const core::cycle_comparison_pair& aPair, std::string_view aPath, const core::diff_options& aOptions,
				  std::stop_token aStop) -> cycle_file_observation;

	private:
		auto content(const core::local_cycle_subject& aSubject, const core::cycle_source_revision_ref& aRevision, const core::cycle_path_side& aSide,
					 std::uint64_t aMaximumBytes, std::stop_token aStop) -> review_material_observation;

		struct cached_manifest
		{
			core::review_cycle_id                       Cycle;
			core::cycle_source_revision_id              Revision;
			std::shared_ptr<const core::cycle_manifest> Manifest;
		};
		/// Decoded manifests are immutable per revision; the two most recent
		/// stay decoded so paging a pair does not reread them.
		static constexpr std::size_t cached_manifests = 2U;

		review_cycle_store&          Store;
		core::local_cycle_authority  Authority;
		review_material_bridge                  Bridge;
		std::shared_ptr<diff_line_matcher_port> Matcher;
		std::vector<cached_manifest>            Cache;
	};
}

#endif
