#ifndef BPR_SRC_ADAPTERS_GITHUB_SRC_REVIEW_COMMENT_PERMALINK_HPP
#define BPR_SRC_ADAPTERS_GITHUB_SRC_REVIEW_COMMENT_PERMALINK_HPP

#include "bpr/application/review_mutation_port.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace bpr::adapters::github::detail {
	/// \brief Exact retained source, supplied by the host after changed-file membership
	/// and text-material validation. The endpoint seals commit/path/blob authority;
	/// LineCount is observed from that blob, never inferred from the remote body.
	struct review_permalink_source
	{
		application::review_mutation_target_binding                  Target;
		std::optional<application::workbench_selected_diff_endpoint> Endpoint;
		core::file_comparison_key                                    Comparison;
		std::uint32_t                                                LineCount = 0U;
	};

	struct parsed_review_permalink
	{
		core::source_line_ref Anchor;
		std::string           Prose;
	};

	/// \brief Compile only current-head right-side publication, including the 64 KiB
	/// wire-body bound. A second code permalink in the prose is ambiguous.
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
