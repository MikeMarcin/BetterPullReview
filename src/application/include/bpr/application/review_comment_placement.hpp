#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_COMMENT_PLACEMENT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_COMMENT_PLACEMENT_HPP

#include "bpr/application/github_projection.hpp"
#include "bpr/application/github_read_model.hpp"
#include "bpr/application/review_mutation_port.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"

#include <map>

namespace bpr::application {
	/// \brief Exact retained text material; the sealed endpoint supplies commit/path/blob authority.
	struct review_permalink_source
	{
		review_mutation_target_binding                  Target;
		std::optional<workbench_selected_diff_endpoint> Endpoint;
		core::file_comparison_key                       Comparison;
		std::uint32_t                                   LineCount                                         = 0U;
		auto                                            operator<=>(const review_permalink_source&) const = default;
	};

	/// \brief A visible permalink's presentation anchor and byte-preserved reviewer prose.
	struct parsed_review_permalink
	{
		core::source_line_ref Anchor;
		std::string           Prose;
	};

	/// \brief Bounded, effect-free provider grammar decoding; grants no source or mutation access.
	using review_permalink_decoder = std::optional<parsed_review_permalink> (*)(const review_permalink_source&, std::string_view);

	/// \brief Reprojects accepted server truth against one selected retained source.
	/// \details The cache retains only current root bodies and is discarded on source replacement.
	/// Replies inherit their root; unavailable or edited links never recover an old local anchor.
	class review_comment_placement final
	{
	public:
		explicit review_comment_placement(review_permalink_decoder aDecode)
			: Decode{ aDecode }
		{ }
		[[nodiscard]] auto project(const github_pull_request_snapshot&    aSnapshot,
								   std::optional<review_permalink_source> aSource) -> github_pull_request_projection;
		void               clear();

	private:
		struct cached_root
		{
			std::string                            Identity;
			std::string                            Body;
			std::optional<parsed_review_permalink> Parsed;
		};
		review_permalink_decoder               Decode;
		std::optional<review_permalink_source> Source;
		std::map<std::string, cached_root>     Roots;
	};
}
#endif
