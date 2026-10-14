#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DIFF_LINE_MATCHER_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DIFF_LINE_MATCHER_HPP

#include "bpr/core/diff_compute_components.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <stop_token>
#include <utility>
#include <vector>

/// \file
/// \brief [[BPR-1485]]. The primary line matcher every diff the host computes
/// uses (RFC-BPR-0082): linked xdiff, Myers with the indent heuristic, behind
/// this port. The core supplies the decoded sides; the matcher returns the
/// matched lines; the core checks them against its own line classes and
/// assembles everything else.
namespace bpr::application {
	/// \brief One comparison's sides, as the core decoded them.
	struct diff_line_match_input
	{
		/// A side the comparison does not have is null and compares as empty.
		const core::decoded_text_document* Left  = nullptr;
		const core::decoded_text_document* Right = nullptr;
		core::diff_options                 Options;
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

		[[nodiscard]] virtual auto match(const diff_line_match_input& aInput) -> std::optional<std::vector<core::diff_match>> = 0;
	};

	/// \brief The engine's stop predicate over a stop token. [[BPR-1366]]
	class stop_token_check final : public core::diff_compute_stop
	{
	public:
		explicit stop_token_check(std::stop_token aToken) noexcept
			: Token{ std::move(aToken) }
		{
		}

		[[nodiscard]] auto stop_requested(std::uint64_t /*aSteps*/) -> bool override
		{
			return Token.stop_requested();
		}

	private:
		std::stop_token Token;
	};

	/// \brief The port's matches as the core's primary component, called on
	/// the computing thread. For computations nothing stops.
	class diff_line_matching final : public core::diff_components
	{
	public:
		explicit diff_line_matching(diff_line_matcher_port& aMatcher) noexcept;

		[[nodiscard]] auto parts() const noexcept -> core::diff_component_parts override;
		[[nodiscard]] auto primary_matches(const core::diff_primary_request& aRequest, core::diff_component_meter& aMeter)
			-> std::optional<std::vector<core::diff_match>> override;

	private:
		diff_line_matcher_port* Matcher = nullptr;
	};

	/// \brief The port's matches for a computation that may be stopped.
	///
	/// \details [[BPR-1485]] The matcher has no stop hook, so it runs on a
	/// thread of its own with its own copy of both sides, while the computing
	/// thread waits and polls the computation's stop. A stop returns at once
	/// and abandons that thread: it runs on, detached, and its result is
	/// discarded. The comparison is computed again from the start the next
	/// time it is wanted.
	class abandonable_diff_line_matching final : public core::diff_components
	{
	public:
		explicit abandonable_diff_line_matching(std::shared_ptr<diff_line_matcher_port> aMatcher) noexcept;

		[[nodiscard]] auto parts() const noexcept -> core::diff_component_parts override;
		[[nodiscard]] auto primary_matches(const core::diff_primary_request& aRequest, core::diff_component_meter& aMeter)
			-> std::optional<std::vector<core::diff_match>> override;

	private:
		std::shared_ptr<diff_line_matcher_port> Matcher;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_DIFF_LINE_MATCHER_HPP
