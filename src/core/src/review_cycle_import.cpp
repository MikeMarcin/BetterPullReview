#include "bpr/core/review_cycle_import.hpp"

#include "bpr/core/review_cycle_json.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <tuple>

namespace bpr::core {
	namespace {
		using json = cycle_json_value;

		auto refused(const cycle_import_problem aProblem, std::string aDetail = { }) -> std::unexpected<cycle_import_refusal>
		{
			return std::unexpected(cycle_import_refusal{ .Problem = aProblem, .Detail = std::move(aDetail) });
		}

		auto bytes_of(const std::string_view aText) -> std::span<const std::uint8_t>
		{
			return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
		}

		auto project_of(const review_cycle_state& aCycle) -> workspace_project_id
		{
			return aCycle.Workspace ? aCycle.Workspace->Project : derive_workspace_project(aCycle.Subject);
		}

		auto captured(const review_cycle_state& aCycle, const cycle_source_revision_id aRevision) -> bool
		{
			return aCycle.Latest && valid_cycle_id(aRevision) && aRevision.value() <= aCycle.Latest->Id.Id.value();
		}

		/// A location of the sender's round as a target in the local cycle.
		auto local_target(const cycle_wire_anchor& aAnchor, const std::map<std::string, cycle_source_revision_id>& aRevisions, const std::span<const cycle_round_file> aFiles)
			-> std::expected<cycle_feedback_target, cycle_import_refusal>
		{
			const auto mapped = aRevisions.find(aAnchor.Revision);
			if (aAnchor.Kind == cycle_anchor_kind::cycle) {
				return cycle_feedback_target{ cycle_whole_target{ } };
			}
			if (mapped == aRevisions.end()) {
				return refused(cycle_import_problem::unmapped_revision, aAnchor.Revision);
			}
			const auto local = mapped->second;
			if (aAnchor.Kind == cycle_anchor_kind::module) {
				return cycle_feedback_target{ cycle_module_target{ .Path = *aAnchor.Path, .Revision = local } };
			}
			if (aAnchor.Kind == cycle_anchor_kind::file) {
				return cycle_feedback_target{ cycle_file_target{ .Revision = local, .Path = *aAnchor.Path, .Blob = *aAnchor.Blob } };
			}
			const auto file = std::ranges::find_if(aFiles, [&](const cycle_round_file& aFile) { return aFile.Revision == local && aFile.Path == *aAnchor.Path && aFile.Blob == *aAnchor.Blob; });
			if (file == aFiles.end()) {
				return refused(cycle_import_problem::mismatched_source, *aAnchor.Path);
			}
			auto anchor = make_anchor(local, *aAnchor.Path, bytes_of(file->Bytes), *aAnchor.Start, *aAnchor.End);
			if (!anchor) {
				return refused(cycle_import_problem::mismatched_source, *aAnchor.Path);
			}
			return cycle_feedback_target{ std::move(*anchor) };
		}
	}

	auto summarize_import(const cycle_decoded_round& aRound) -> cycle_import_summary
	{
		return {
			.Digest         = aRound.Digest,
			.ForeignCycle   = aRound.Payload.CycleId,
			.ForeignRound   = aRound.Payload.RoundId,
			.ForeignProject = aRound.Payload.Project.Id,
		};
	}

	auto imported_project_key(const std::string_view aForeignProject) -> workspace_project_id
	{
		constexpr auto domain = std::string_view{ "BPR-imported-project-v1" };
		auto           bytes  = std::string{ domain };
		bytes.push_back('\0');
		bytes.append(aForeignProject);
		return workspace_project_id{ sha256(bytes_of(bytes)) };
	}

	auto encode_import_association(const cycle_import_association& aAssociation) -> std::string
	{
		auto revisions = json::array{ };
		for (const auto& mapping : aAssociation.Revisions) {
			revisions.push_back(json{ json::object{ { .Key = "foreign", .Value = json{ mapping.Foreign } }, { .Key = "local", .Value = json{ static_cast<std::int64_t>(mapping.Local.value()) } } } });
		}
		return canonical_cycle_json(json{ json::object{
										{ .Key = "cycle", .Value = json{ static_cast<std::int64_t>(aAssociation.Cycle.Cycle.value()) } },
										{ .Key = "generation", .Value = json{ static_cast<std::int64_t>(aAssociation.Cycle.Generation) } },
										{ .Key = "revisions", .Value = json{ std::move(revisions) } },
									} })
			.value_or(std::string{ });
	}

	auto decode_import_association(const std::string_view aBytes, const local_cycle_authority& aAuthority) -> std::optional<cycle_import_association>
	{
		const auto parsed = parse_cycle_json(aBytes, 1U << 20U);
		if (!parsed) {
			return std::nullopt;
		}
		static constexpr auto keys    = std::array<std::string_view, 3>{ "cycle", "generation", "revisions" };
		const auto*           members = std::get_if<json::object>(&parsed->Value);
		if (!members || !has_exactly(*members, keys)) {
			return std::nullopt;
		}
		const auto* cycle      = std::get_if<std::int64_t>(&find_member(*members, "cycle")->Value);
		const auto* generation = std::get_if<std::int64_t>(&find_member(*members, "generation")->Value);
		const auto* revisions  = std::get_if<json::array>(&find_member(*members, "revisions")->Value);
		if (!cycle || !generation || !revisions || *cycle <= 0 || *generation < 0) {
			return std::nullopt;
		}
		auto                  result      = cycle_import_association{ .Cycle = { .Authority = aAuthority, .Cycle = review_cycle_id{ static_cast<std::uint64_t>(*cycle) }, .Generation = static_cast<std::uint64_t>(*generation) } };
		static constexpr auto mappingKeys = std::array<std::string_view, 2>{ "foreign", "local" };
		for (const auto& value : *revisions) {
			const auto* fields = std::get_if<json::object>(&value.Value);
			if (!fields || !has_exactly(*fields, mappingKeys)) {
				return std::nullopt;
			}
			const auto* foreign = std::get_if<std::string>(&find_member(*fields, "foreign")->Value);
			const auto* local   = std::get_if<std::int64_t>(&find_member(*fields, "local")->Value);
			if (!foreign || !local || *local <= 0) {
				return std::nullopt;
			}
			result.Revisions.push_back({ .Foreign = *foreign, .Local = cycle_source_revision_id{ static_cast<std::uint64_t>(*local) } });
		}
		return result;
	}

	auto stable_name(const cycle_import_problem aValue) noexcept -> std::string_view
	{
		switch (aValue) {
		case cycle_import_problem::unmapped_project:   return "unmapped_project";
		case cycle_import_problem::invalid_cycle:      return "invalid_cycle";
		case cycle_import_problem::unmapped_revision:  return "unmapped_revision";
		case cycle_import_problem::invalid_mapping:    return "invalid_mapping";
		case cycle_import_problem::mismatched_source:  return "mismatched_source";
		case cycle_import_problem::already_associated: return "already_associated";
		case cycle_import_problem::not_associated:     return "not_associated";
		}
		return "invalid_mapping";
	}

	auto foreign_revisions(const cycle_round_payload& aPayload) -> std::vector<std::string>
	{
		auto names = std::set<std::string>{ aPayload.SourceRevision };
		for (const auto& item : aPayload.Items) {
			for (const auto& anchor : item.Anchors) {
				names.insert(anchor.Revision);
			}
			for (const auto& suggestion : item.Suggestions) {
				names.insert(suggestion.Anchor.Revision);
			}
		}
		for (const auto& source : aPayload.Sources) {
			names.insert(source.Revision);
		}
		return { names.begin(), names.end() };
	}

	auto foreign_files(const cycle_round_payload& aPayload) -> std::vector<cycle_foreign_file>
	{
		auto       files = std::set<cycle_foreign_file>{ };
		const auto named = [&](const cycle_wire_anchor& aAnchor) {
			if (aAnchor.Path && aAnchor.Blob) {
				files.insert({ .Revision = aAnchor.Revision, .Path = *aAnchor.Path, .Blob = *aAnchor.Blob });
			}
		};
		for (const auto& item : aPayload.Items) {
			std::ranges::for_each(item.Anchors, named);
			for (const auto& suggestion : item.Suggestions) {
				named(suggestion.Anchor);
			}
		}
		for (const auto& source : aPayload.Sources) {
			files.insert({ .Revision = source.Revision, .Path = source.Path, .Blob = source.Blob });
		}
		return { files.begin(), files.end() };
	}

	auto associate_import(const cycle_import_summary& aImport, const cycle_round_payload& aPayload, const review_cycle_state& aCycle, const std::span<const workspace_import_mapping> aProjects,
						  std::vector<cycle_revision_mapping> aRevisions, const std::span<const cycle_mapped_file> aObserved, const std::optional<cycle_import_association>& aExisting)
		-> std::expected<cycle_import_association, cycle_import_refusal>
	{
		if (!valid(aCycle.Subject) || aCycle.Lifecycle == cycle_lifecycle::archived) {
			return refused(cycle_import_problem::invalid_cycle);
		}
		const auto resolved = resolve_imported_project(aProjects, imported_project_key(aImport.ForeignProject));
		if (!resolved || *resolved != project_of(aCycle)) {
			return refused(cycle_import_problem::unmapped_project, aImport.ForeignProject);
		}
		std::ranges::sort(aRevisions);
		const auto named = foreign_revisions(aPayload);
		auto       map   = std::map<std::string, cycle_source_revision_id>{ };
		for (const auto& mapping : aRevisions) {
			if (!map.emplace(mapping.Foreign, mapping.Local).second || !std::ranges::binary_search(named, mapping.Foreign) || !captured(aCycle, mapping.Local)) {
				return refused(cycle_import_problem::invalid_mapping, mapping.Foreign);
			}
		}
		for (const auto& foreign : named) {
			if (!map.contains(foreign)) {
				return refused(cycle_import_problem::unmapped_revision, foreign);
			}
		}
		for (const auto& file : foreign_files(aPayload)) {
			const auto local = map.at(file.Revision);
			const auto seen  = std::ranges::find_if(aObserved, [&](const cycle_mapped_file& aObservedFile) { return aObservedFile.Local == local && aObservedFile.Path == file.Path; });
			if (seen == aObserved.end() || seen->Blob != file.Blob) {
				return refused(cycle_import_problem::mismatched_source, file.Path);
			}
		}
		auto candidate = cycle_import_association{ .Cycle = aCycle.Subject, .Revisions = std::move(aRevisions) };
		if (aExisting && *aExisting != candidate) {
			return refused(cycle_import_problem::already_associated);
		}
		return candidate;
	}

	auto plan_import_fork(const cycle_round_payload& aPayload, const cycle_import_association& aAssociation, const std::span<const cycle_round_file> aFiles)
		-> std::expected<std::vector<std::pair<std::string, begin_cycle_feedback>>, cycle_import_refusal>
	{
		auto map = std::map<std::string, cycle_source_revision_id>{ };
		for (const auto& mapping : aAssociation.Revisions) {
			map.emplace(mapping.Foreign, mapping.Local);
		}
		auto plan = std::vector<std::pair<std::string, begin_cycle_feedback>>{ };
		for (const auto& item : aPayload.Items) {
			if (item.Anchors.size() > maximum_cycle_related_references + 1U) {
				return refused(cycle_import_problem::invalid_mapping, item.Id);
			}
			auto command = begin_cycle_feedback{ .Primary = cycle_whole_target{ }, .Body = item.Body, .Provenance = authored_provenance::unknown };
			for (std::size_t index = 0; index < item.Anchors.size(); ++index) {
				auto target = local_target(item.Anchors[index], map, aFiles);
				if (!target) {
					return std::unexpected(target.error());
				}
				if (index == 0U) {
					command.Primary = std::move(*target);
				} else {
					command.Related.push_back({ .Target = std::move(*target) });
				}
			}
			plan.emplace_back(item.Id, std::move(command));
		}
		return plan;
	}
}
