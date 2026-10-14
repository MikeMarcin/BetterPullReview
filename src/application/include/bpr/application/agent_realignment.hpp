#ifndef BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_REALIGNMENT_HPP
#define BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_REALIGNMENT_HPP

#include "bpr/application/diff_line_matcher.hpp"
#include "bpr/application/diff_runtime_ports.hpp"
#include "bpr/application/workbench_runtime_ports.hpp"
#include "bpr/core/agent_finding.hpp"
#include "bpr/core/agent_session.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/diff_model.hpp"

#include <chrono>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <stop_token>
#include <string>
#include <string_view>
#include <vector>

namespace bpr::application {
	/// \brief The bound on one continuation re-alignment, stated once.
	///
	/// \details A continued turn recomputes the alignment of the revision its
	/// session is bound to, so the work it may do is the work one fresh
	/// alignment may do and no more: this many files, each under the diff
	/// host's own `core::diff_compute_limits`. The reviewer asks about one
	/// selected file, so one file is the whole of it; a request naming more
	/// is refused rather than served, and nothing here retains anything past
	/// the call.
	inline constexpr std::size_t maximum_agent_realignment_files = 1U;

	/// \brief How long one continuation re-alignment may take, stated once.
	///
	/// \details The other half of the same bound. A recompute resolves and
	/// materializes through the ports the review's own epoch work is already
	/// using, so it can queue behind that work for seconds; a reviewer waiting
	/// on an answer cannot be made to wait behind it indefinitely. Past this
	/// the request is stopped and the turn is sent with nothing frozen, which
	/// is the documented answer when the old revision cannot be aligned again.
	/// Refused past the bound rather than held: a gesture that blocked would
	/// be the failure this bound exists to prevent.
	inline constexpr std::chrono::milliseconds maximum_agent_realignment_wait{
		30'000
	};

	/// \brief Why a re-alignment produced no identities.
	///
	/// \details Every value here is a refusal the caller may state. The
	/// continued turn falls back to freezing nothing, which is the documented
	/// answer when the old revision cannot be aligned again.
	enum class agent_realignment_refusal : std::uint8_t {
		none,
		/// The request does not name a complete binding, file, or options.
		incomplete_request,
		/// The request names more files than the bound admits.
		file_limit_exceeded,
		/// One or both of the bound revision's objects are gone.
		objects_unavailable,
		/// The alignment's own size limits refused the recompute.
		compute_limit,
		/// The caller asked to stop before the recompute finished.
		cancelled,
		/// The ports the recompute runs on were unavailable.
		host_failure,
	};

	[[nodiscard]] auto stable_name(agent_realignment_refusal aRefusal) noexcept
		-> std::string_view;

	/// \brief One request for the alignment of an agent session's own revision.
	///
	/// \details `Binding` is the session's kept binding — the epoch pair and
	/// the base/head commits it was started against. The remaining members are
	/// the resolution context the reviewer's current selection produced; the
	/// comparison `Diff` names is retargeted onto `Binding`'s pair before
	/// anything is resolved, so the current revision contributes the authority
	/// and the diff options and nothing else.
	struct agent_realignment_request
	{
		core::agent_revision_binding                  Binding;
		workbench_request_context                     Context;
		workbench_pull_request_authority              WorkbenchAuthority;
		workbench_selected_diff_context               Diff;
		std::vector<workbench_file_observation_entry> Files;
		core::diff_options                            Options;
		core::diff_compute_limits                     Limits;

		auto operator<=>(const agent_realignment_request&) const = default;
	};

	/// \brief Checks whether the supplied value satisfies its contract.
	[[nodiscard]] auto valid(const agent_realignment_request& aRequest) noexcept
		-> bool;

	/// \brief Retargets one file lookup onto the kept binding's epoch pair.
	///
	/// \details Stated as its own function because two callers already needed
	/// it: the recompute below, and every test that has to name the exact
	/// request the epoch host is expected to be asked.
	[[nodiscard]] auto realignment_lookup(
		const agent_realignment_request&        aRequest,
		const workbench_file_observation_entry& aFile)
		-> workbench_selected_diff_request;

	/// \brief What one re-alignment produced.
	///
	/// \details Two views of one answer. `Files` is what the turn is frozen
	/// and validated against — the per-path identities, in the shape the
	/// snapshot on disk carries them. `Alignments` is the alignment those
	/// identities were read off, one entry per entry in `Files` and in the
	/// same order, kept because a continued turn has to say *where* in the
	/// old revision the reviewer is pointing and the digest, segments, and
	/// groups it says that with are this revision's and not the one on
	/// screen. Both are empty on every refusal: a re-alignment that did not
	/// answer produced no identities to state.
	///
	/// Bounded by `maximum_agent_realignment_files` like everything else
	/// here, so this is one alignment of one file and not a cache of a
	/// revision.
	struct agent_realignment_outcome
	{
		agent_realignment_refusal                      Refusal = agent_realignment_refusal::none;
		std::vector<core::agent_finding_snapshot_file> Files;
		std::vector<core::file_alignment>              Alignments;

		auto operator<=>(const agent_realignment_outcome&) const = default;
	};

	/// \brief Recomputes the alignment of a revision the review moved past.
	///
	/// \details A capability, not a store. The answer is computed for the one
	/// turn that asked and is not retained, so nothing here has a retention
	/// bound or an eviction lifetime to get wrong.
	class agent_realignment_port
	{
	public:
		virtual ~agent_realignment_port() = default;

		[[nodiscard]] virtual auto realign(
			const agent_realignment_request& aRequest,
			std::stop_token                  aStop)
			-> agent_realignment_outcome = 0;
	};

	/// \brief Serves re-alignment from the machinery a fresh alignment uses.
	///
	/// \details Resolves the bound revision's blobs through the same
	/// `workbench_selected_diff_port` the workbench resolves through,
	/// materializes them through the same `diff_blob_port` the diff worker
	/// materializes through, and computes with `core::compute_file_diff` and
	/// the same line matcher ([[BPR-1485]]). No
	/// second copy of the algorithm exists here, so a continued turn and a
	/// fresh one cannot align the same revision differently.
	class composed_agent_realignment final : public agent_realignment_port
	{
	public:
		composed_agent_realignment(
			workbench_selected_diff_port& aFiles, diff_blob_port& aBlobs, diff_line_matcher_port& aMatcher) noexcept;

		[[nodiscard]] auto realign(
			const agent_realignment_request& aRequest,
			std::stop_token                  aStop)
			-> agent_realignment_outcome override;

	private:
		workbench_selected_diff_port* Files = nullptr;
		diff_blob_port*               Blobs   = nullptr;
		diff_line_matcher_port*       Matcher = nullptr;
	};
} // namespace bpr::application

#endif // BPR_SRC_APPLICATION_INCLUDE_BPR_APPLICATION_AGENT_REALIGNMENT_HPP
