#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_REVIEW_COMMENT_PERMALINK_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_REVIEW_COMMENT_PERMALINK_HPP

#include "bpr/application/review_comment_placement.hpp"
#include "bpr/application/review_mutation_port.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"

#include "bpr/core/review_tour.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace bpr::adapters::github::detail {
	/// \brief Exact retained source, supplied by the host after changed-file membership
	/// and text-material validation. The endpoint seals commit/path/blob authority;
	/// LineCount is observed from that blob, never inferred from the remote body.
	using review_permalink_source = application::review_permalink_source;
	using parsed_review_permalink = application::parsed_review_permalink;

	/// Bounded read-only address codec; this scope and these source facts grant no write authority.
	struct tour_permalink_scope
	{
		std::string              Hostname;
		core::repository_locator Repository;
	};
	[[nodiscard]] auto compile_tour_permalink(const tour_permalink_scope& aScope, const core::tour_code_destination& aSource) -> std::optional<std::string>;
	[[nodiscard]] auto parse_tour_permalink(const tour_permalink_scope& aScope, std::string_view aBody) -> std::optional<core::tour_code_destination>;

	/// \brief Compile only current-head right-side publication, including the 64 KiB
	/// wire-body bound. A second code permalink in the prose is ambiguous.
	/// \details RFC-BPR-0089, [[BPR-1706]]. On a commit-filtered diff the link
	/// names the line at the filter's commit on the endpoint's side, and a
	/// left endpoint is admitted; the anchor's side must be the endpoint's.
	[[nodiscard]] auto compile_review_permalink_body(
		const review_permalink_source& aSource,
		const core::source_line_ref&   aAnchor,
		std::string_view               aProse) -> std::optional<std::string>;

	/// \brief Rehydrate only against the supplied exact retained source (which may be
	/// historical). Caller must independently admit a file-subject root comment
	/// in the expected PR/changed file. A parsed anchor grants no write authority.
	[[nodiscard]] auto parse_review_permalink_body(
		const review_permalink_source& aSource,
		std::string_view               aBody) -> std::optional<parsed_review_permalink>;
} // namespace bpr::adapters::github::detail

#endif
