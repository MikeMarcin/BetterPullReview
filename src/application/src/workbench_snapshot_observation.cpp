#include "bpr/application/workbench_snapshot_observation.hpp"

#include "bpr/core/diff_model.hpp"

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <span>
#include <utility>
#include <vector>

namespace bpr::application {
	namespace {
		/// \brief Builds one answer that repeats the request exactly.
		///
		/// \details Every return path goes through this. `Context` and
		/// `Authority` must repeat the request or the host refuses the terminal
		/// as foreign, and a failure path that forgot to copy them would be a
		/// misrouted answer rather than a reported failure.
		[[nodiscard]] auto answer(
			const workbench_observation_request& aRequest,
			const workbench_observation_status   aStatus)
			-> workbench_file_observation
		{
			return workbench_file_observation{
				.Context    = aRequest.Context,
				.Authority  = aRequest.Authority,
				.Status     = aStatus,
				.Actionable = { },
			};
		}

		[[nodiscard]] auto repository_path(const std::string& aPath)
			-> std::filesystem::path
		{
			auto utf8 = std::u8string{ };
			utf8.reserve(aPath.size());
			std::ranges::transform(
				aPath, std::back_inserter(utf8),
				[](const char aByte) { return static_cast<char8_t>(aByte); });
			return std::filesystem::path{ utf8 };
		}

		[[nodiscard]] auto observed_file(
			const github_file_record&     aRecord,
			const core::workbench_file_id aIdentity)
			-> std::optional<workbench_file_observation_entry>
		{
			const auto change = [&aRecord] {
				switch (aRecord.Change) {
				case github_file_change::added:
					return workbench_file_change_kind::added;
				case github_file_change::modified:
					return workbench_file_change_kind::modified;
				case github_file_change::removed:
					return workbench_file_change_kind::removed;
				case github_file_change::renamed:
					return workbench_file_change_kind::renamed;
				case github_file_change::copied:
					return workbench_file_change_kind::copied;
				case github_file_change::type_changed:
					return workbench_file_change_kind::type_changed;
				case github_file_change::unchanged:
					return workbench_file_change_kind::unchanged;
				case github_file_change::uninitialized:
					return workbench_file_change_kind::uninitialized;
				}
				return workbench_file_change_kind::uninitialized;
			}();
			if (change == workbench_file_change_kind::uninitialized) {
				return std::nullopt;
			}
			auto entry = workbench_file_observation_entry{
				.File             = aIdentity,
				.PresentationPath = aRecord.Path,
				.CurrentPath      = repository_path(aRecord.Path),
				.PreviousPath     = aRecord.PreviousPath.has_value()
					? std::optional{ repository_path(*aRecord.PreviousPath) }
					: std::nullopt,
				.Change           = change,
				.ContentKind      = aRecord.ContentKind == github_file_content_kind::binary
					? workbench_file_content_kind::binary
					: aRecord.ContentKind == github_file_content_kind::text
					? workbench_file_content_kind::text
					: workbench_file_content_kind::unknown,
			};
			return valid(entry) ? std::optional{ std::move(entry) } : std::nullopt;
		}

		/// \brief Marks every repeat of an identity after its first position.
		///
		/// \details [[BPR-0972]]. One sort of (identity, position) pairs, so a
		/// catalogue of N files costs O(N log N) rather than searching the
		/// accepted prefix for each file.
		[[nodiscard]] auto later_repeats(const std::span<const core::file_identity> aIdentities)
			-> std::vector<bool>
		{
			auto ordered = std::vector<std::pair<core::file_identity, std::size_t>>{ };
			ordered.reserve(aIdentities.size());
			for (std::size_t position = 0U; position < aIdentities.size(); ++position) {
				ordered.emplace_back(aIdentities[position], position);
			}
			std::ranges::sort(ordered);
			auto repeated = std::vector<bool>(aIdentities.size(), false);
			for (std::size_t index = 1U; index < ordered.size(); ++index) {
				if (ordered[index].first == ordered[index - 1U].first) {
					repeated[ordered[index].second] = true;
				}
			}
			return repeated;
		}
	} // namespace

	auto workbench_file_entry(const github_file_record& aRecord)
		-> std::optional<workbench_file_observation_entry>
	{
		const auto identity = core::file_identity_of(aRecord.Path);
		return identity.valid() ? observed_file(aRecord, identity) : std::nullopt;
	}

	snapshot_workbench_observation::snapshot_workbench_observation(
		file_supplier          aFiles,
		actionability_supplier aActionability,
		filter_supplier        aFilter)
		: Files{ std::move(aFiles) }
		, Actionability{ std::move(aActionability) }
		, Filter{ std::move(aFilter) }
	{
	}

	auto snapshot_workbench_observation::assemble(
		const workbench_observation_request& aRequest,
		std::stop_token                      aStop) -> workbench_file_observation
	{
		if (aStop.stop_requested()) {
			return answer(aRequest, workbench_observation_status::cancelled);
		}
		if (!Files) {
			return answer(aRequest, workbench_observation_status::unavailable);
		}

		const auto files = Files(aRequest.Authority);
		if (!files) {
			return answer(aRequest, workbench_observation_status::unavailable);
		}

		// Reported rather than truncated, and with no partial order attached.
		// A truncated order would hand the core false neighbour facts: the last
		// file it could see would look like the last file there is, and
		// "next change" would stop early without saying so.
		if (files->size() > aRequest.MaximumFiles) {
			return answer(aRequest, workbench_observation_status::limit_exceeded);
		}

		auto identities = std::vector<core::file_identity>{ };
		identities.reserve(files->size());
		for (const auto& record : *files) {
			if (aStop.stop_requested()) {
				return answer(aRequest, workbench_observation_status::cancelled);
			}

			// Derived from the head path. A rename therefore renames the file's
			// identity too, which is the decision recorded on
			// `core::file_identity_of` and not an oversight here.
			const auto identity = core::file_identity_of(record.Path);
			if (!identity.valid()) {
				// A path this build cannot name makes the whole answer
				// unavailable rather than a shorter list. Silently omitting the
				// record would hide a file from the reviewer entirely, and the
				// read model already bounds path length, so reaching this means
				// the snapshot is malformed rather than merely large.
				return answer(
					aRequest, workbench_observation_status::unavailable);
			}
			identities.push_back(identity);
		}

		// The port owns uniqueness within the order; the host resolves the
		// first matching entry. A snapshot repeating a path would otherwise
		// put one file in the order twice and make navigation revisit it.
		const auto repeated = later_repeats(identities);
		auto       result   = answer(aRequest, workbench_observation_status::available);
		result.Actionable.reserve(files->size());
		for (std::size_t position = 0U; position < files->size(); ++position) {
			if (aStop.stop_requested()) {
				return answer(aRequest, workbench_observation_status::cancelled);
			}
			if (repeated[position]) {
				continue;
			}
			const auto& record   = (*files)[position];
			const auto  identity = identities[position];
			auto        file     = observed_file(record, identity);
			if (!file.has_value()) {
				return answer(aRequest, workbench_observation_status::unavailable);
			}
			// [[BPR-0972]]. Numbered by catalogue position, and withheld files
			// named, so the host can restore one in place later.
			file->Ordinal = position;
			// RFC-BPR-0089. A commit filter's exact endpoints, or its split.
			if (Filter) {
				if (const auto filtered = Filter(aRequest.Authority, identity)) {
					file->Filter        = filtered->Endpoints;
					file->SplitByFilter = filtered->Split;
					if (!valid(*file)) {
						return answer(aRequest, workbench_observation_status::unavailable);
					}
				}
			}
			if (Actionability) {
				const auto facts = Actionability(aRequest.Authority, identity);
				if (facts.has_value() && facts->Receipt.File == identity
					&& facts->Receipt.Material.valid()
					&& !core::actionable(core::reason_for(*facts))) {
					result.Withheld.push_back(std::move(*file));
					continue;
				}
			}
			result.Actionable.push_back(std::move(*file));
		}

		// Presentation order remains the snapshot's own order. Only a supplied
		// exact-material receipt may remove a file; unknown material stays visible
		// rather than inheriting a receipt from another comparison.
		return result;
	}
} // namespace bpr::application
