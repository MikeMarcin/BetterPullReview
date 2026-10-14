#include "bpr/application/agent_realignment.hpp"

#include "bpr/core/agent_finding.hpp"
#include "bpr/core/agent_session.hpp"
#include "bpr/core/diff_compute.hpp"
#include "bpr/core/diff_model.hpp"
#include "bpr/core/diff_transition.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <stop_token>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto refusal_for_blob_status(
			const diff_blob_status aStatus) noexcept -> agent_realignment_refusal
		{
			switch (aStatus) {
			case diff_blob_status::available:
				return agent_realignment_refusal::none;
			case diff_blob_status::missing:
				return agent_realignment_refusal::objects_unavailable;
			case diff_blob_status::materialization_limit:
				return agent_realignment_refusal::compute_limit;
			case diff_blob_status::cancelled:
				return agent_realignment_refusal::cancelled;
			case diff_blob_status::binary:
			case diff_blob_status::decode_failure:
			case diff_blob_status::generated_file_unsupported:
			case diff_blob_status::failure:
				break;
			}
			return agent_realignment_refusal::host_failure;
		}

		[[nodiscard]] auto refusal_for_lookup_status(
			const workbench_selected_diff_status aStatus) noexcept
			-> agent_realignment_refusal
		{
			switch (aStatus) {
			case workbench_selected_diff_status::available:
				return agent_realignment_refusal::none;
			// The bound revision's tree no longer answers for this path,
			// so its objects are gone as far as this turn is concerned.
			case workbench_selected_diff_status::missing_left:
			case workbench_selected_diff_status::missing_right:
			case workbench_selected_diff_status::both_missing:
			case workbench_selected_diff_status::stale:
				return agent_realignment_refusal::objects_unavailable;
			case workbench_selected_diff_status::cancelled:
				return agent_realignment_refusal::cancelled;
			case workbench_selected_diff_status::binary:
			case workbench_selected_diff_status::ambiguous_left:
			case workbench_selected_diff_status::ambiguous_right:
			case workbench_selected_diff_status::ambiguous_both:
			case workbench_selected_diff_status::failure:
				break;
			}
			return agent_realignment_refusal::host_failure;
		}

		[[nodiscard]] auto refusal_for_compute_reason(
			const core::diff_reason_code aReason) noexcept
			-> agent_realignment_refusal
		{
			switch (aReason) {
			case core::diff_reason_code::missing_left_blob:
			case core::diff_reason_code::missing_right_blob:
			case core::diff_reason_code::stale:
				return agent_realignment_refusal::objects_unavailable;
			case core::diff_reason_code::materialization_limit:
			case core::diff_reason_code::compute_limit:
				return agent_realignment_refusal::compute_limit;
			case core::diff_reason_code::cancelled:
				return agent_realignment_refusal::cancelled;
			case core::diff_reason_code::none:
			case core::diff_reason_code::binary_file:
			case core::diff_reason_code::decode_failure:
			case core::diff_reason_code::generated_file_unsupported:
			case core::diff_reason_code::invalid_alignment:
			case core::diff_reason_code::host_failure:
				break;
			}
			return agent_realignment_refusal::host_failure;
		}

		[[nodiscard]] auto realignment_key(
			const workbench_selected_diff_request&     aLookup,
			const workbench_selected_diff_observation& aObservation)
			-> core::file_comparison_key
		{
			const auto& diff = aLookup.Diff;
			return core::file_comparison_key{
				.Authority   = diff.Authority,
				.Repository  = diff.Authority.Repository,
				.PullRequest = diff.Authority.PullRequest,
				.LeftAnchor  = diff.Comparison.Left.Identity,
				.RightAnchor = diff.Comparison.Right.Identity,
				.LeftBlob    = aObservation.LeftBlob,
				.RightBlob   = aObservation.RightBlob,
				.File        = aLookup.File.File,
				.Options     = diff.OptionsDigest,
				.Algorithm   = diff.Algorithm,
			};
		}

		[[nodiscard]] auto snapshot_for(
			const std::string&          aPath,
			const core::file_alignment& aAlignment)
			-> core::agent_finding_snapshot_file
		{
			auto file = core::agent_finding_snapshot_file{
				.Path           = aPath,
				.Comparison     = aAlignment.Digest,
				.LeftLineCount  = aAlignment.LeftLineCount,
				.RightLineCount = aAlignment.RightLineCount,
			};
			file.Segments.reserve(aAlignment.Segments.size());
			for (const auto& segment : aAlignment.Segments) {
				file.Segments.push_back(segment.Id);
			}
			file.Groups.reserve(aAlignment.Groups.size());
			for (const auto& group : aAlignment.Groups) {
				file.Groups.push_back(group.Id);
			}
			file.Moves.reserve(aAlignment.Moves.size());
			for (const auto& move : aAlignment.Moves) {
				file.Moves.push_back(move.Id);
			}
			return file;
		}
	} // namespace

	auto stable_name(const agent_realignment_refusal aRefusal) noexcept
		-> std::string_view
	{
		switch (aRefusal) {
		case agent_realignment_refusal::none:
			return "none";
		case agent_realignment_refusal::incomplete_request:
			return "incomplete_request";
		case agent_realignment_refusal::file_limit_exceeded:
			return "file_limit_exceeded";
		case agent_realignment_refusal::objects_unavailable:
			return "objects_unavailable";
		case agent_realignment_refusal::compute_limit:
			return "compute_limit";
		case agent_realignment_refusal::cancelled:
			return "cancelled";
		case agent_realignment_refusal::host_failure:
			break;
		}
		return "host_failure";
	}

	auto valid(const agent_realignment_request& aRequest) noexcept -> bool
	{
		if (aRequest.Files.empty() || !core::valid(aRequest.Options)) {
			return false;
		}
		for (const auto& file : aRequest.Files) {
			if (!valid(file) || file.PresentationPath.empty()) {
				return false;
			}
		}
		const auto digest = core::options_digest(aRequest.Options);
		// The epochs are deliberately not required to differ: a base and a head
		// anchor of one epoch carry the same ordinal and are told apart by
		// their kind, so an inequality here would refuse the commonest pair.
		return digest.has_value() && *digest == aRequest.Diff.OptionsDigest
			&& aRequest.Diff.Authority.Repository == aRequest.Binding.Repository
			&& aRequest.Diff.Authority.PullRequest
			== aRequest.Binding.PullRequest;
	}

	auto realignment_lookup(
		const agent_realignment_request&        aRequest,
		const workbench_file_observation_entry& aFile)
		-> workbench_selected_diff_request
	{
		auto lookup = workbench_selected_diff_request{
			.Context            = aRequest.Context,
			.WorkbenchAuthority = aRequest.WorkbenchAuthority,
			.Diff               = aRequest.Diff,
			.File               = aFile,
		};
		// The whole retargeting, and deliberately nothing else: the pair the
		// session was started against replaces the pair under review, and the
		// authority, options, and algorithm stay what they are so the key this
		// produces is the key that revision's alignment already had.
		lookup.Diff.Comparison.Left.Identity.Epoch  = aRequest.Binding.LeftEpoch;
		lookup.Diff.Comparison.Left.Commit          = aRequest.Binding.BaseOid;
		lookup.Diff.Comparison.Right.Identity.Epoch = aRequest.Binding.RightEpoch;
		lookup.Diff.Comparison.Right.Commit         = aRequest.Binding.HeadOid;
		return lookup;
	}

	composed_agent_realignment::composed_agent_realignment(
		workbench_selected_diff_port& aFiles, diff_blob_port& aBlobs, diff_line_matcher_port& aMatcher) noexcept
		: Files{ &aFiles }
		, Blobs{ &aBlobs }
		, Matcher{ &aMatcher }
	{
	}

	auto composed_agent_realignment::realign(
		const agent_realignment_request& aRequest, std::stop_token aStop)
		-> agent_realignment_outcome
	{
		auto outcome = agent_realignment_outcome{ };
		if (Files == nullptr || Blobs == nullptr || Matcher == nullptr) {
			outcome.Refusal = agent_realignment_refusal::host_failure;
			return outcome;
		}
		// The bound is checked before any effect runs, so a request over it
		// costs one comparison rather than one tree walk per file.
		if (aRequest.Files.size() > maximum_agent_realignment_files) {
			outcome.Refusal = agent_realignment_refusal::file_limit_exceeded;
			return outcome;
		}
		if (!valid(aRequest)) {
			outcome.Refusal = agent_realignment_refusal::incomplete_request;
			return outcome;
		}
		auto frozen     = std::vector<core::agent_finding_snapshot_file>{ };
		auto alignments = std::vector<core::file_alignment>{ };
		for (const auto& entry : aRequest.Files) {
			if (aStop.stop_requested()) {
				outcome.Refusal = agent_realignment_refusal::cancelled;
				return outcome;
			}
			const auto lookup   = realignment_lookup(aRequest, entry);
			const auto resolved = Files->resolve(lookup, aStop);
			if (resolved.Request != lookup) {
				outcome.Refusal = agent_realignment_refusal::host_failure;
				return outcome;
			}
			outcome.Refusal = refusal_for_lookup_status(resolved.Status);
			if (outcome.Refusal != agent_realignment_refusal::none) {
				return outcome;
			}
			const auto key = realignment_key(lookup, resolved);
			if (!core::valid(key)) {
				outcome.Refusal = agent_realignment_refusal::incomplete_request;
				return outcome;
			}
			const auto context = core::diff_request_context{
				.Request            = aRequest.Context.Request,
				.ApplicationSession = aRequest.Diff.ApplicationSession,
				.Key                = key,
				.Generation         = aRequest.Context.Generation,
			};
			auto       left  = std::optional<std::vector<std::uint8_t>>{ };
			auto       right = std::optional<std::vector<std::uint8_t>>{ };
			const auto read  = [&](const core::diff_side                     aSide,
								   const std::optional<core::git_blob_oid>&  aBlob,
								   std::optional<std::vector<std::uint8_t>>& aBytes)
				-> agent_realignment_refusal {
				if (!aBlob.has_value()) {
					return agent_realignment_refusal::none;
				}
				const auto request = diff_blob_request{
					.Context      = context,
					.Side         = aSide,
					.Blob         = *aBlob,
					.MaximumBytes = aRequest.Limits.MaximumBytesPerSide,
				};
				auto observed = Blobs->materialize(request, aStop);
				if (observed.Context != request.Context
					|| observed.Side != request.Side
					|| observed.Blob != request.Blob) {
					return agent_realignment_refusal::host_failure;
				}
				const auto refusal = refusal_for_blob_status(observed.Status);
				if (refusal != agent_realignment_refusal::none) {
					return refusal;
				}
				if (observed.Bytes.size() > request.MaximumBytes) {
					return agent_realignment_refusal::compute_limit;
				}
				aBytes = std::move(observed.Bytes);
				return agent_realignment_refusal::none;
			};
			outcome.Refusal = read(core::diff_side::left, resolved.LeftBlob, left);
			if (outcome.Refusal != agent_realignment_refusal::none) {
				return outcome;
			}
			outcome.Refusal
				= read(core::diff_side::right, resolved.RightBlob, right);
			if (outcome.Refusal != agent_realignment_refusal::none) {
				return outcome;
			}
			// The one alignment computation this program has, asked about an
			// older pair. Nothing about the recompute differs from a fresh
			// one but the two commits it names, so the identities it produces
			// are the identities that revision's alignment had.
			const auto input = core::diff_compute_input{
				.Key        = key,
				.Options    = aRequest.Options,
				.LeftBytes  = left.has_value()
					? std::optional{ std::span<const std::uint8_t>{ *left } }
					: std::nullopt,
				.RightBytes = right.has_value()
					? std::optional{ std::span<const std::uint8_t>{ *right } }
					: std::nullopt,
				.Limits     = aRequest.Limits,
			};
			auto matching = diff_line_matching{ *Matcher };
			auto computed = core::compute_file_diff(input, matching);
			if (!computed.Result.has_value()) {
				outcome.Refusal = refusal_for_compute_reason(computed.Reason);
				return outcome;
			}
			try {
				frozen.push_back(snapshot_for(
					entry.PresentationPath, computed.Result->Alignment));
				// Kept beside the snapshot rather than derived from it: the
				// snapshot states which identities exist, and a turn that has
				// to point at one of them needs the line counts and spans they
				// were computed over as well.
				alignments.push_back(std::move(computed.Result->Alignment));
			} catch (...) {
				outcome.Refusal = agent_realignment_refusal::host_failure;
				return outcome;
			}
		}
		outcome.Files      = std::move(frozen);
		outcome.Alignments = std::move(alignments);
		return outcome;
	}
} // namespace bpr::application
