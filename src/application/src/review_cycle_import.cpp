#include "bpr/application/review_cycle_import.hpp"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::application {
	namespace {
		auto bytes_of(const std::string_view aText) -> std::span<const std::uint8_t>
		{
			return { reinterpret_cast<const std::uint8_t*>(aText.data()), aText.size() };
		}

		auto failed(const cycle_store_failure aFailure) -> std::unexpected<cycle_store_error>
		{
			return std::unexpected(cycle_store_error{ .Failure = aFailure });
		}
	}

	auto import_round_bundle(review_cycle_store& aStore, const cycle_operation_id& aOperation, const std::span<const std::uint8_t> aBundle, std::stop_token aStop)
		-> cycle_store_result<cycle_import_result>
	{
		auto opened = core::open_round_bundle(aBundle);
		if (!opened) {
			return cycle_import_result{ .Refusal = std::move(opened.error()) };
		}
		// Only a completely validated bundle reaches the store.
		auto pieces = std::vector<std::span<const std::uint8_t>>{ bytes_of(opened->Round.Canonical) };
		for (const auto& [digest, content] : opened->Objects) {
			pieces.push_back(bytes_of(content));
		}
		auto installed = aStore.install_all(pieces, aStop);
		if (!installed) {
			return std::unexpected(installed.error());
		}
		auto commit = cycle_import_commit{ .Summary = core::summarize_import(opened->Round), .Payload = installed->front() };
		commit.Objects.assign(installed->begin() + 1, installed->end());
		const auto receipt = aStore.admit_import(aOperation, commit, aStop);
		if (!receipt) {
			return std::unexpected(receipt.error());
		}
		auto record = aStore.load_import(core::cycle_import_id{ receipt->Allocated }, aStop);
		if (!record) {
			return std::unexpected(record.error());
		}
		if (!*record) {
			return failed(cycle_store_failure::corrupt);
		}
		return cycle_import_result{ .Import = std::move(*record), .Existing = !receipt->Changed, .Replayed = receipt->Replayed };
	}

	namespace {
		auto full_source(const core::local_cycle_subject& aSubject, const core::cycle_source_revision_id aRevision) -> core::cycle_comparison_pair
		{
			return { .Left = std::nullopt, .Right = { .Cycle = aSubject.Cycle, .Id = aRevision } };
		}

		auto local_of(const std::vector<core::cycle_revision_mapping>& aRevisions, const std::string& aForeign) -> std::optional<core::cycle_source_revision_id>
		{
			const auto found = std::ranges::find(aRevisions, aForeign, &core::cycle_revision_mapping::Foreign);
			return found == aRevisions.end() ? std::nullopt : std::optional{ found->Local };
		}

		auto load_cycle(review_cycle_store& aStore, const core::local_cycle_subject& aSubject, std::stop_token aStop) -> cycle_store_result<core::review_cycle_state>
		{
			auto cycle = aStore.load(aSubject, std::move(aStop));
			if (!cycle) {
				return std::unexpected(cycle.error());
			}
			if (!*cycle) {
				return failed(cycle_store_failure::not_found);
			}
			return std::move(**cycle);
		}
	}

	auto read_imported_round(review_cycle_store& aStore, const core::cycle_import_id aImport, std::stop_token aStop) -> cycle_store_result<cycle_imported_round>
	{
		auto record = aStore.load_import(aImport, aStop);
		if (!record) {
			return std::unexpected(record.error());
		}
		if (!*record) {
			return failed(cycle_store_failure::not_found);
		}
		const auto bytes = aStore.read_object((*record)->Payload, std::move(aStop));
		if (!bytes) {
			return std::unexpected(bytes.error());
		}
		auto content = core::decode_round_payload({ reinterpret_cast<const char*>(bytes->data()), bytes->size() });
		if (!content || content->Digest != (*record)->Summary.Digest) {
			return failed(cycle_store_failure::corrupt);
		}
		return cycle_imported_round{ .Record = std::move(**record), .Content = std::move(*content) };
	}

	auto attach_imported_round(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const cycle_import_attach_request& aRequest,
							   std::stop_token aStop) -> cycle_store_result<cycle_import_attach_result>
	{
		auto imported = read_imported_round(aStore, aRequest.Import, aStop);
		if (!imported) {
			return std::unexpected(imported.error());
		}
		auto cycle = load_cycle(aStore, aSubject, aStop);
		if (!cycle) {
			return std::unexpected(cycle.error());
		}
		// What each mapped local revision retains at each path the round names by bytes.
		auto observed = std::vector<core::cycle_mapped_file>{ };
		for (const auto& file : core::foreign_files(imported->Content.Payload)) {
			const auto local = local_of(aRequest.Revisions, file.Revision);
			if (!local || !core::valid_cycle_id(*local)) {
				continue;
			}
			const auto held = aHost.file(aSubject, full_source(aSubject, *local), file.Path, { }, aStop);
			if (held.Status == review_material_status::cancelled) {
				return failed(cycle_store_failure::cancelled);
			}
			const auto present = held.Path && held.Path->Right.State == core::cycle_path_state::present;
			observed.push_back({ .Local = *local, .Path = file.Path, .Blob = present ? held.Path->Right.Content : std::nullopt });
		}
		auto association = core::associate_import(imported->Record.Summary, imported->Content.Payload, *cycle, aRequest.Projects, aRequest.Revisions, observed, imported->Record.Association);
		if (!association) {
			return cycle_import_attach_result{ .Refusal = association.error() };
		}
		const auto receipt = aStore.associate_import(aRequest.Operation, aRequest.Import, *association, aStop);
		if (!receipt) {
			return std::unexpected(receipt.error());
		}
		return cycle_import_attach_result{ .Association = std::move(*association), .Changed = receipt->Changed };
	}

	auto fork_imported_round(review_cycle_store& aStore, review_cycle_comparison_host& aHost, const core::local_cycle_subject& aSubject, const cycle_operation_id& aOperation,
							 const core::cycle_import_id aImport, std::stop_token aStop) -> cycle_store_result<cycle_import_fork_result>
	{
		auto imported = read_imported_round(aStore, aImport, aStop);
		if (!imported) {
			return std::unexpected(imported.error());
		}
		const auto& association = imported->Record.Association;
		if (!association || association->Cycle != aSubject) {
			return cycle_import_fork_result{ .Refusal = core::cycle_import_refusal{ .Problem = core::cycle_import_problem::not_associated } };
		}
		auto cycle = load_cycle(aStore, aSubject, aStop);
		if (!cycle) {
			return std::unexpected(cycle.error());
		}
		// The retained bytes behind every range location, to anchor locally.
		auto files = std::vector<core::cycle_round_file>{ };
		for (const auto& item : imported->Content.Payload.Items) {
			for (const auto& anchor : item.Anchors) {
				const auto local = local_of(association->Revisions, anchor.Revision);
				if (anchor.Kind != core::cycle_anchor_kind::range || !local) {
					continue;
				}
				const auto known = std::ranges::any_of(files, [&](const core::cycle_round_file& aFile) { return aFile.Revision == *local && aFile.Path == *anchor.Path; });
				if (known) {
					continue;
				}
				const auto held = aHost.file(aSubject, full_source(aSubject, *local), *anchor.Path, { }, aStop);
				if (held.Status == review_material_status::cancelled) {
					return failed(cycle_store_failure::cancelled);
				}
				if (held.Status == review_material_status::available && held.Path && held.Path->Right.Content) {
					files.push_back({ .Revision = *local, .Path = *anchor.Path, .Blob = *held.Path->Right.Content, .Bytes = { held.Source.begin(), held.Source.end() } });
				}
			}
		}
		auto plan = core::plan_import_fork(imported->Content.Payload, *association, files);
		if (!plan) {
			return cycle_import_fork_result{ .Refusal = plan.error() };
		}
		const auto receipt = aStore.fork_import(aOperation, aImport, aSubject, cycle->Revision, *plan, aStop);
		if (!receipt) {
			return std::unexpected(receipt.error());
		}
		auto notes = aStore.import_forks(aImport, aStop);
		if (!notes) {
			return std::unexpected(notes.error());
		}
		return cycle_import_fork_result{ .Notes = std::move(*notes), .Existing = !receipt->Changed };
	}
}
