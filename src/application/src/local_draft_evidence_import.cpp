#include "bpr/application/local_draft_evidence_import.hpp"

#include <filesystem>
#include <utility>
#include <vector>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto failed(const evidence_import_failure aFailure) -> std::unexpected<evidence_import_error>
		{
			return std::unexpected{ evidence_import_error{ .Failure = aFailure } };
		}

		[[nodiscard]] auto unreadable(const workspace_failure aFailure) -> std::unexpected<evidence_import_error>
		{
			if (aFailure == workspace_failure::cancelled) {
				return failed(evidence_import_failure::cancelled);
			}
			return std::unexpected{ evidence_import_error{ .Failure = evidence_import_failure::unreadable, .Workspace = aFailure } };
		}

		[[nodiscard]] auto refused(core::evidence_import_refused aRefusal) -> std::unexpected<evidence_import_error>
		{
			return std::unexpected{ evidence_import_error{ .Failure = evidence_import_failure::refused, .Refusal = std::move(aRefusal) } };
		}

		/// Reads one regular file beneath the root, whole, within a bound.
		[[nodiscard]] auto read_whole(workspace_root_session& aRoot, const std::string_view aPath, const std::uint64_t aLimit, const std::stop_token aStop)
			-> std::expected<std::vector<std::uint8_t>, workspace_failure>
		{
			auto bytes = std::vector<std::uint8_t>{ };
			auto read  = aRoot.read(aPath, aLimit, [&bytes](const std::span<const std::uint8_t> aChunk) {
                bytes.insert(bytes.end(), aChunk.begin(), aChunk.end());
                return true; }, aStop);
			if (!read) {
				return std::unexpected{ read.error() };
			}
			return bytes;
		}

		/// One artifact as the import's folder holds it. Nothing outside the
		/// folder, nothing through a link and nothing over its bound is copied.
		[[nodiscard]] auto resolve(workspace_root_session& aRoot, const core::evidence_artifact_request& aRequest, std::uint64_t& aBudget, std::vector<local_draft_object>& aObjects,
								   const std::stop_token aStop) -> std::expected<core::evidence_artifact, workspace_failure>
		{
			if (!aRequest.Url.empty()) {
				return core::evidence_artifact{ .Name = aRequest.Name, .State = core::artifact_state::link, .Reference = aRequest.Url };
			}
			auto artifact = core::evidence_artifact{ .Name = aRequest.Name, .State = core::artifact_state::missing, .Reference = aRequest.Path };
			auto bytes    = read_whole(aRoot, aRequest.Path, std::min(core::maximum_evidence_artifact_bytes, aBudget), aStop);
			if (!bytes) {
				switch (bytes.error()) {
				case workspace_failure::cancelled:         return std::unexpected{ workspace_failure::cancelled };
				case workspace_failure::missing:           artifact.State = core::artifact_state::missing; break;
				case workspace_failure::too_large:         artifact.State = core::artifact_state::too_large; break;
				case workspace_failure::unsupported:
				case workspace_failure::permission_denied:
				case workspace_failure::escape:
				case workspace_failure::replaced:
				case workspace_failure::unsafe_path:
				case workspace_failure::changed:
				case workspace_failure::io:                artifact.State = core::artifact_state::refused; break;
				}
				return artifact;
			}
			// A content identity of its own kind: never confused with a
			// repository's SHA-1 objects in the same store.
			const auto identity = core::git_blob_identity(*bytes, core::git_hash_algorithm::sha256);
			if (!identity) {
				artifact.State = core::artifact_state::refused;
				return artifact;
			}
			artifact.State  = core::artifact_state::stored;
			artifact.Object = core::retag<core::git_entry_oid_kind>(*identity);
			artifact.Size   = bytes->size();
			aBudget -= std::min<std::uint64_t>(aBudget, bytes->size());
			aObjects.push_back({ .Object = *artifact.Object, .Bytes = std::move(*bytes) });
			return artifact;
		}
	}

	auto import_local_draft_evidence(local_draft_store& aStore, workspace_source_port& aSource, const core::local_draft_id& aDraft, const std::string_view aDocument,
									 const std::int64_t aNow, const std::stop_token aStop) -> std::expected<evidence_import_summary, evidence_import_error>
	{
		const auto path = std::filesystem::path{ std::u8string{ aDocument.begin(), aDocument.end() } };
		if (!path.is_absolute() || !path.has_filename() || !path.has_parent_path()) {
			return unreadable(workspace_failure::unsafe_path);
		}
		const auto folder = path.parent_path().u8string();
		const auto file   = path.filename().u8string();
		auto       root   = aSource.open(std::string_view{ reinterpret_cast<const char*>(folder.data()), folder.size() }, aStop);
		if (!root) {
			return unreadable(root.error());
		}
		auto text = read_whole(**root, std::string_view{ reinterpret_cast<const char*>(file.data()), file.size() }, core::maximum_evidence_import_bytes, aStop);
		if (!text) {
			if (text.error() == workspace_failure::too_large) {
				return refused({ .Reason = core::evidence_import_refusal::too_large });
			}
			return unreadable(text.error());
		}
		auto document = core::parse_evidence_import(std::string_view{ reinterpret_cast<const char*>(text->data()), text->size() });
		if (!document) {
			return refused(std::move(document.error()));
		}
		auto artifacts = std::vector<core::evidence_artifact>{ };
		auto objects   = std::vector<local_draft_object>{ };
		auto budget    = maximum_evidence_import_artifact_bytes;
		for (const auto& request : core::evidence_artifact_requests(*document)) {
			auto artifact = resolve(**root, request, budget, objects, aStop);
			if (!artifact) {
				return failed(evidence_import_failure::cancelled);
			}
			artifacts.push_back(std::move(*artifact));
		}
		if (aStop.stop_requested()) {
			return failed(evidence_import_failure::cancelled);
		}
		for (auto attempt = 0; attempt < 2; ++attempt) {
			auto current = aStore.find(aDraft);
			if (!current) {
				return std::unexpected{ evidence_import_error{ .Failure = evidence_import_failure::store, .Store = current.error() } };
			}
			auto next = core::import_evidence(*current, current->Version, *document, artifacts, aNow);
			if (!next) {
				return refused(std::move(next.error()));
			}
			auto written = aStore.put(*next, current->Version, objects);
			if (written) {
				auto summary = evidence_import_summary{ .Claims    = document->Claims.size(),
														.Checks    = document->Checks.size(),
														.Results   = document->Results.size(),
														.Questions = document->Questions.size() };
				for (const auto& result : std::span{ next->Evidence.Results }.last(document->Results.size())) {
					summary.Bound += result.Candidate != 0U ? 1U : 0U;
				}
				for (const auto& artifact : artifacts) {
					if (artifact.State == core::artifact_state::stored) {
						++summary.Stored;
					} else if (artifact.State != core::artifact_state::link) {
						++summary.Unstored;
					}
				}
				summary.Draft = std::move(*next);
				return summary;
			}
			if (written.error().Failure != local_draft_store_failure::stale) {
				return std::unexpected{ evidence_import_error{ .Failure = evidence_import_failure::store, .Store = written.error() } };
			}
		}
		return refused({ .Reason = core::evidence_import_refusal::stale });
	}
}
