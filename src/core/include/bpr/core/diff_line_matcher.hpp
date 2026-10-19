#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_LINE_MATCHER_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_LINE_MATCHER_HPP

#include "bpr/core/diff_compute_components.hpp"

#include <memory>
#include <optional>
#include <vector>

namespace bpr::core {
	/// \brief One comparison's sides, as the core decoded them.
	struct diff_line_match_input
	{
		/// A side the comparison does not have is null and compares as empty.
		const decoded_text_document* Left  = nullptr;
		const decoded_text_document* Right = nullptr;
		diff_options                 Options;
	};

	/// \brief Matches the lines of two decoded sides.
	///
	/// \details Two lines may match only when their normalized forms under
	/// `Options` are equal, and both indices strictly increase. The call cannot
	/// be interrupted and keeps no state between calls: it may run on a thread
	/// whose caller has been stopped and has gone, so it touches nothing but
	/// its input, and it is safe to call from several threads at once. Returns
	/// nothing when it cannot match, which refuses the comparison.
	class diff_line_matcher_port
	{
	public:
		diff_line_matcher_port()                                                 = default;
		diff_line_matcher_port(const diff_line_matcher_port&)                    = delete;
		diff_line_matcher_port(diff_line_matcher_port&&)                         = delete;
		auto operator=(const diff_line_matcher_port&) -> diff_line_matcher_port& = delete;
		auto operator=(diff_line_matcher_port&&) -> diff_line_matcher_port&      = delete;
		virtual ~diff_line_matcher_port()                                        = default;

		[[nodiscard]] virtual auto match(const diff_line_match_input& aInput) -> std::optional<std::vector<diff_match>> = 0;
	};

	/// \brief Creates the stateless primary matcher backed by the separately linked
	/// xdiff shared library. No provider types escape this private implementation.
	[[nodiscard]] auto make_xdiff_line_matcher() -> std::shared_ptr<diff_line_matcher_port>;
} // namespace bpr::core

#endif // BPR_SRC_CORE_INCLUDE_BPR_CORE_DIFF_LINE_MATCHER_HPP
