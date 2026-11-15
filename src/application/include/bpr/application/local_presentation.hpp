#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_LOCAL_PRESENTATION_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_LOCAL_PRESENTATION_HPP

#include "bpr/application/diff_line_matcher.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/diff_semantics.hpp"
#include "bpr/core/diff_transition.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief One local file comparison, computed for the pull request window's
/// own diff viewer (RFC-BPR-0108, owner 2026-11-13). The same engine, the same
/// result and index types and the same request context a pull request's file
/// reaches the viewer with, so the draft is shown by the viewer itself and
/// keeps its syntax colours, navigation and selection.
namespace bpr::application {
	struct local_presentation
	{
		core::diff_request_context Context;
		std::string                Path;
		/// Absent when the comparison was refused; `Reason` says why.
		std::shared_ptr<const core::computed_file_diff>  Result;
		std::shared_ptr<const core::diff_semantic_index> Index;
		core::diff_reason_code                           Reason = core::diff_reason_code::none;
		/// The sides' exact texts, for the classifier: absent for a missing side.
		std::shared_ptr<const std::string> Left;
		std::shared_ptr<const std::string> Right;

		auto operator==(const local_presentation&) const -> bool = default;
	};

	/// \brief Computes one local comparison the window presents. Absent only
	/// when stopped or when neither side is present.
	/// \param aSequence A nonzero number that orders this presentation after
	/// the ones before it in one window; it is the request and generation.
	[[nodiscard]] auto present_local_comparison(std::string_view aSubject, std::string_view aPath, const std::optional<std::span<const std::uint8_t>>& aLeft,
												const std::optional<std::span<const std::uint8_t>>& aRight, std::shared_ptr<diff_line_matcher_port> aMatcher, std::uint64_t aSequence,
												std::stop_token aStop) -> std::optional<local_presentation>;
}

#endif
