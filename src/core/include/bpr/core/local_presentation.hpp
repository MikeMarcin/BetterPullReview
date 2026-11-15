#ifndef BPR_SRC_CORE_INCLUDE_BPR_CORE_LOCAL_PRESENTATION_HPP
#define BPR_SRC_CORE_INCLUDE_BPR_CORE_LOCAL_PRESENTATION_HPP

#include "bpr/core/diff_compute.hpp"
#include "bpr/core/diff_model.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

/// \file
/// \brief A local comparison shown by the pull request window's own diff
/// viewer (RFC-BPR-0108 as amended by the owner, 2026-11-13: "augment the real
/// PR window, do not rebuild it"). A local draft's candidate file and a Local
/// Changes file have no pull request, epoch or GitHub identity; this gives each
/// one a comparison identity of the shape the viewer takes, derived only from
/// the subject, the path and the exact bytes, and domain-separated from every
/// pull request's. Nothing here reads Git or GitHub.
namespace bpr::core {
	/// \brief The authority and the two anchors every comparison of one local
	/// subject shares, so Find in files over the subject's files reaches the
	/// comparisons the viewer shows.
	struct local_presentation_identity
	{
		pull_request_authority     Authority;
		comparison_anchor_identity Left;
		comparison_anchor_identity Right;
	};
	[[nodiscard]] auto local_presentation_identity_of(std::string_view aSubject) -> local_presentation_identity;

	/// \brief The file identity a local subject's path has in the Files drawer
	/// and in its comparisons. Equal subjects and paths give equal identities.
	[[nodiscard]] auto local_presentation_file(std::string_view aSubject, std::string_view aPath) -> file_identity;

	/// \brief The comparison identity of one local file. Absent when neither
	/// side is present or `aOptions` does not normalise.
	/// \param aSubject What the comparison belongs to: a draft and candidate,
	/// or a worktree and its Local Changes comparison.
	/// \details The blobs are the sides' Git blob identities (SHA-1), so the
	/// classifier's cache and the window's syntax adoption key them as they
	/// key a pull request's.
	[[nodiscard]] auto local_presentation_key(std::string_view aSubject, std::string_view aPath, std::optional<std::span<const std::uint8_t>> aLeft,
											  std::optional<std::span<const std::uint8_t>> aRight, const diff_options& aOptions) -> std::optional<file_comparison_key>;
}

#endif
