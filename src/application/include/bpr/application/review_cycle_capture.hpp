#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_CAPTURE_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_REVIEW_CYCLE_CAPTURE_HPP

#include "bpr/application/review_cycle_store.hpp"
#include "bpr/application/workspace_source_port.hpp"
#include "bpr/core/review_cycle_git.hpp"
#include "bpr/core/review_cycle_manifest.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

/// \file
/// \brief Capture of a bound workspace into an immutable revision
/// (RFC-BPR-0054, BPR-1149).
/// \details A capture reads the bound root through the read-only workspace
/// port, installs each file's exact bytes into the cycle store, and then
/// observes the whole scope again. Only when the second observation agrees
/// with the first (same paths, kinds, identities, sizes and change stamps,
/// the same index bytes and the same root) is a manifest installed and the
/// revision recorded, in one store transaction that also pins every content
/// object. A disagreement discards the attempt and retries, at most
/// `core::maximum_capture_retries` times, then reports `unstable`; a partial
/// or unstable attempt never becomes a revision. The result is labelled
/// `observed_stable`: a quiescent handoff observation, not an atomic
/// snapshot.
namespace bpr::application {
	/// \brief How far a capture has got, for a progress line ([[BPR-1184]]).
	struct cycle_capture_progress
	{
		enum class phase : std::uint8_t {
			/// The first observation: each file's bytes read and kept.
			reading,
			/// The second observation: every path looked at again.
			checking,
			/// The manifest and the revision, in one store transaction.
			recording,
		};
		phase         Phase                                            = phase::reading;
		std::uint64_t Files                                            = 0;
		std::uint32_t Attempt                                          = 1;
		auto          operator<=>(const cycle_capture_progress&) const = default;
	};
	/// Called on the capture's own thread, at most once per
	/// `cycle_capture_progress_files` files and at each phase.
	using cycle_capture_progress_sink                           = std::function<void(const cycle_capture_progress&)>;
	inline constexpr std::uint64_t cycle_capture_progress_files = 1024U;

	struct cycle_capture_request
	{
		cycle_operation_id Operation;
		std::uint64_t      ExpectedRevision = 0;
		/// Root-relative paths to leave out, in addition to the checkout's
		/// ignore rules and the disclosed default set. Recorded in the manifest.
		std::vector<std::string> Exclusions;
		/// Record another observation even when the bytes are unchanged.
		bool          Deliberate       = false;
		std::uint64_t MaximumFileBytes = core::maximum_captured_file_bytes;
		/// Optional; receives progress while the capture runs.
		cycle_capture_progress_sink Progress;
	};

	enum class cycle_capture_status : std::uint8_t {
		/// Recorded, or an identical recapture that changed nothing.
		captured,
		/// The cycle has no workspace binding.
		not_bound,
		/// The location answers with another root; an explicit rebind is needed.
		root_changed,
		/// The workspace port refused; see `Workspace`.
		workspace,
		/// The source changed during every attempt.
		unstable,
		/// More admitted entries than a capture allows; narrow the scope.
		too_many_entries,
		/// The store refused; see `Store`. Quota refusals carry the inventory.
		store,
		cancelled,
	};

	/// \brief What the capture observed, for a quality indicator that needs
	/// no warning dialog. Content-free except for local paths.
	struct cycle_capture_diagnostics
	{
		std::uint32_t Attempts  = 0;
		std::uint64_t FilesRead = 0;
		std::uint64_t BytesRead = 0;
		/// Paths that differed between the passes of the last unstable attempt.
		std::uint64_t Changed = 0;
		/// Display labels of the first of them.
		std::vector<std::string>    ChangedExamples;
		core::cycle_capture_quality Quality = core::cycle_capture_quality::observed_stable;
		/// Present when a revision was recorded.
		std::optional<core::cycle_capture_explanation> Explanation;
	};

	struct cycle_capture_outcome
	{
		cycle_capture_status             Status = cycle_capture_status::cancelled;
		std::optional<workspace_failure> Workspace;
		/// Why the checkout's Git metadata was refused, with `workspace`.
		std::optional<core::git_repository_problem>    Repository;
		std::optional<core::workspace_verification>    Verification;
		std::optional<cycle_store_error>               Store;
		std::optional<cycle_write_receipt>             Receipt;
		std::optional<core::cycle_capture_observation> Capture;
		std::optional<cycle_object_ref>                Manifest;
		cycle_capture_diagnostics                      Diagnostics;
	};

	/// \brief Result of comparing a candidate root with the latest revision.
	struct cycle_material_check_outcome
	{
		cycle_capture_status             Status = cycle_capture_status::cancelled;
		std::optional<workspace_failure> Workspace;
		/// Why the checkout's Git metadata was refused, with `workspace`.
		std::optional<core::git_repository_problem>   Repository;
		std::optional<cycle_store_error>              Store;
		std::optional<core::workspace_material_check> Check;
		/// The candidate root as the check opened it: what a rebind confirmed
		/// against this check binds ([[BPR-1279]]).
		std::optional<core::workspace_root_observation> Root;
	};

	/// \brief Captures bound workspaces into a cycle store. Every call runs on
	/// the effect lane; the source and store must outlive the host.
	class review_cycle_capture_host
	{
	public:
		review_cycle_capture_host(workspace_source_port& aSource, review_cycle_store& aStore) noexcept;

		/// \brief Opens a location for binding. The observation is what a
		/// `bind_cycle_workspace` command records.
		auto observe(std::string_view aLocation, std::stop_token aStop) -> std::expected<core::workspace_root_observation, workspace_failure>;
		/// \brief Captures the cycle's bound root under its current scope.
		auto capture(const core::review_cycle_state& aCycle, const cycle_capture_request& aRequest, std::stop_token aStop) -> cycle_capture_outcome;
		/// \brief Reads a candidate root's bytes under the cycle's scope and
		/// compares them with its latest revision, installing nothing: the
		/// material check an explicit rebind records.
		auto check_material(const core::review_cycle_state& aCycle, std::string_view aLocation, std::stop_token aStop) -> cycle_material_check_outcome;

	private:
		workspace_source_port& Source;
		review_cycle_store&    Store;
	};
}

#endif
