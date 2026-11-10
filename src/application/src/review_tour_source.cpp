#include "bpr/application/review_tour_source.hpp"
#include "bpr/application/epoch_application_host.hpp"
#include "bpr/core/review_preparation.hpp"
namespace bpr::application {
	auto materialize_tour_source_comparison(epoch_application_host& aEpoch, diff_line_matcher_port& aMatcher,
											const workbench_selected_diff_request& aRequest, std::stop_token aStop) -> std::optional<tour_source_comparison>
	{
		const auto resolved = aEpoch.resolve(aRequest, aStop);
		if (aStop.stop_requested() || resolved.Request != aRequest || resolved.Status != workbench_selected_diff_status::available
			|| !resolved.LeftBlob || !resolved.RightBlob || !resolved.LeftPath || !resolved.RightPath) {
			return std::nullopt;
		}
		const auto&                     diff = aRequest.Diff;
		const core::file_comparison_key key{ diff.Authority, diff.Authority.Repository, diff.Authority.PullRequest,
											 diff.Comparison.Left.Identity, diff.Comparison.Right.Identity, resolved.LeftBlob, resolved.RightBlob,
											 aRequest.File.File, diff.OptionsDigest, diff.Algorithm, std::nullopt };
		if (!core::valid(key)) {
			return std::nullopt;
		}
		const core::diff_request_context context{ aRequest.Context.Request, diff.ApplicationSession, key, aRequest.Context.Generation };
		auto                             read = [&](core::diff_side aSide, core::git_blob_oid aBlob) -> std::optional<std::vector<std::uint8_t>> {
			const diff_blob_request request{ .Context = context, .Side = aSide, .Blob = aBlob, .MaximumBytes = 256U * 1024U };
			auto                    material = aEpoch.materialize(request, aStop);
			if (material.Status != diff_blob_status::available || material.Context != context || material.Side != aSide
				|| material.Blob != aBlob || material.Bytes.size() > request.MaximumBytes || aStop.stop_requested()) {
				return std::nullopt;
			}
			return std::move(material.Bytes);
		};
		const auto left  = read(core::diff_side::left, *resolved.LeftBlob);
		const auto right = read(core::diff_side::right, *resolved.RightBlob);
		if (!left || !right) {
			return std::nullopt;
		}
		auto limits                = core::diff_compute_limits{ };
		limits.MaximumBytesPerSide = 256U * 1024U;
		const core::diff_compute_input input{ .Key = key, .Options = diff.Options, .LeftBytes = std::span<const std::uint8_t>{ *left }, .RightBytes = std::span<const std::uint8_t>{ *right }, .Limits = limits };
		auto                           stop     = stop_token_check{ aStop };
		auto                           matching = diff_line_matching{ aMatcher };
		auto                           computed = core::compute_file_diff(input, { .Stop = &stop }, matching);
		if (!computed.Outcome || !computed.Outcome->Result || aStop.stop_requested()) {
			return std::nullopt;
		}
		auto index = core::build_diff_semantic_index(std::make_shared<const core::computed_file_diff>(std::move(*computed.Outcome->Result)));
		if (!index || aStop.stop_requested()) {
			return std::nullopt;
		}
		const auto lpath = resolved.LeftPath->generic_u8string(), rpath = resolved.RightPath->generic_u8string();
		return tour_source_comparison{ std::move(*index),
									   { diff.Comparison.Left.Commit, resolved.LeftBlob, std::string{ lpath.begin(), lpath.end() }, core::diff_side::left, 0U, 0U },
									   { diff.Comparison.Right.Commit, resolved.RightBlob, std::string{ rpath.begin(), rpath.end() }, core::diff_side::right, 0U, 0U } };
	}
	auto review_tour_source_context(epoch_application_host& aEpoch, const workbench_pull_request_authority& aAuthority, const core::tour_code_destination& aSource) -> std::optional<workbench_selected_diff_context>
	{
		if (!core::valid(aSource)) {
			return std::nullopt;
		}
		const auto current = aEpoch.context(aAuthority);
		if (!current) {
			return std::nullopt;
		}
		// Both endpoints name the requested commit. This is bounded pinned code,
		// not a claim that the file exists on both sides of the PR comparison.
		return aEpoch.realignment_context(aAuthority, { aAuthority.Host, aAuthority.Repository, aAuthority.PullRequest, aSource.Commit, aSource.Commit, current->Comparison.Left.Identity.Epoch, current->Comparison.Right.Identity.Epoch });
	}

	auto materialize_review_tour_source(workbench_selected_diff_port& aEndpoints, epoch_application_host& aEpoch,
										const workbench_selected_diff_request& aRequest, const core::tour_code_destination& aSource, std::stop_token aStop)
		-> std::expected<review_tour_source_material, review_tour_source_failure>
	{
		if (aStop.stop_requested()) {
			return std::unexpected(review_tour_source_failure::cancelled);
		}
		const bool left = aSource.Side == core::diff_side::left;
		if (!core::valid(aSource) || aRequest.File.PresentationPath != aSource.Path || (left ? aRequest.Diff.Comparison.Left.Commit : aRequest.Diff.Comparison.Right.Commit) != aSource.Commit) {
			return std::unexpected(review_tour_source_failure::unavailable);
		}
		if (aSource.LastLine - aSource.FirstLine >= 200) {
			return std::unexpected(review_tour_source_failure::too_large);
		}
		const auto  resolved = aEndpoints.resolve(aRequest, aStop);
		const auto& blob     = left ? resolved.LeftBlob : resolved.RightBlob;
		const auto& path     = left ? resolved.LeftPath : resolved.RightPath;
		const auto& endpoint = left ? resolved.LeftEndpoint : resolved.RightEndpoint;
		if (resolved.Status != workbench_selected_diff_status::available || resolved.Request != aRequest || !blob || !path || !endpoint || !valid(*endpoint, aRequest, aSource.Side, *blob, *path) || (aSource.Blob && aSource.Blob != blob)) {
			return std::unexpected(review_tour_source_failure::unavailable);
		}
		const auto&                     diff = aRequest.Diff;
		const core::file_comparison_key key{ diff.Authority, diff.Authority.Repository, diff.Authority.PullRequest, diff.Comparison.Left.Identity, diff.Comparison.Right.Identity, resolved.LeftBlob, resolved.RightBlob, aRequest.File.File, diff.OptionsDigest, diff.Algorithm, std::nullopt };
		if (!core::valid(key)) {
			return std::unexpected(review_tour_source_failure::unavailable);
		}
		const diff_blob_request materialRequest{ .Context = { core::request_id{ 2 }, aRequest.Context.Session, key, aRequest.Context.Generation }, .Side = aSource.Side, .Blob = *blob, .MaximumBytes = 256 * 1024 };
		const auto              material = aEpoch.materialize_pinned_source_blob(resolved, materialRequest, aStop);
		if (aStop.stop_requested()) {
			return std::unexpected(review_tour_source_failure::cancelled);
		}
		if (material.Status != diff_blob_status::available || material.Context != materialRequest.Context || material.Side != materialRequest.Side || material.Blob != materialRequest.Blob || material.Bytes.size() > materialRequest.MaximumBytes) {
			return std::unexpected(review_tour_source_failure::unavailable);
		}
		const std::string bytes{ material.Bytes.begin(), material.Bytes.end() };
		if (!core::valid_writing_utf8(bytes) || bytes.find('\0') != std::string::npos) {
			return std::unexpected(review_tour_source_failure::unavailable);
		}
		std::size_t   first = 0, last = 0;
		std::uint32_t line = 0;
		while (line < aSource.FirstLine && first < bytes.size()) {
			auto next = bytes.find('\n', first);
			first     = next == std::string::npos ? bytes.size() : next + 1;
			++line;
		}
		if (line != aSource.FirstLine || first == bytes.size()) {
			return std::unexpected(review_tour_source_failure::range_unavailable);
		}
		last = first;
		for (; line <= aSource.LastLine; ++line) {
			if (last == bytes.size()) {
				return std::unexpected(review_tour_source_failure::range_unavailable);
			}
			auto next = bytes.find('\n', last);
			last      = next == std::string::npos ? bytes.size() : next + 1;
		}
		if (last - first > core::maximum_tour_field_bytes) {
			return std::unexpected(review_tour_source_failure::too_large);
		}
		const auto digest = core::comparison_digest(key);
		return review_tour_source_material{ { { digest, aSource.Side, aSource.FirstLine }, { digest, aSource.Side, aSource.LastLine }, aSource.Commit, blob, aSource.Path }, bytes.substr(first, last - first) };
	}
}
