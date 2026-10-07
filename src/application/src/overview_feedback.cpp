#include "bpr/application/overview_feedback.hpp"

#include "bpr/application/feedback_counters.hpp"

#include <algorithm>
#include <map>
#include <numeric>
#include <set>
#include <string_view>
#include <utility>

namespace bpr::application {
	namespace {
		using core::feedback_counter_scope;
		using core::feedback_counter_scope_kind;

		[[nodiscard]] auto folder_of(std::string_view aPath) -> std::string
		{
			const auto slash = aPath.rfind('/');
			return slash == std::string_view::npos ? std::string{ } : std::string{ aPath.substr(0U, slash) };
		}

		[[nodiscard]] auto scope(feedback_counter_scope_kind aKind, std::string aPath = { }) -> feedback_counter_scope
		{
			auto result = feedback_counter_scope{ };
			result.Kind = aKind;
			result.Path = std::move(aPath);
			return result;
		}

		/// The disjoint core scopes one row counts, and the scope the merged
		/// tally is named for.
		struct row_scopes
		{
			std::vector<feedback_counter_scope> Parts;
			feedback_counter_scope              Where;
		};

		/// Files with a conversation that the pull request does not list:
		/// they belong to the remainder, so no conversation goes unshown.
		[[nodiscard]] auto unlisted_files(const feedback_counter_model& aModel, const std::set<std::string_view>& aListed) -> std::vector<feedback_counter_scope>
		{
			auto result = std::vector<feedback_counter_scope>{ };
			for (const auto& kept : aModel.scopes()) {
				if (kept.Kind == feedback_counter_scope_kind::file && !aListed.contains(kept.Path)) {
					result.push_back(kept);
				}
			}
			return result;
		}

		[[nodiscard]] auto scopes_of(const feedback_counter_model& aModel, std::span<const std::string> aFiles, overview_feedback_row_kind aRow, const std::string& aPath)
			-> std::optional<row_scopes>
		{
			auto result = row_scopes{ };
			switch (aRow) {
			case overview_feedback_row_kind::file:
				if (std::ranges::find(aFiles, aPath) == aFiles.end()) {
					return std::nullopt;
				}
				result.Where = scope(feedback_counter_scope_kind::file, aPath);
				result.Parts.push_back(result.Where);
				return result;
			case overview_feedback_row_kind::folder:
				for (const auto& file : aFiles) {
					if (folder_of(file) == aPath) {
						result.Parts.push_back(scope(feedback_counter_scope_kind::file, file));
					}
				}
				if (result.Parts.empty()) {
					return std::nullopt;
				}
				result.Where = scope(feedback_counter_scope_kind::directory, aPath);
				return result;
			case overview_feedback_row_kind::remainder: {
				const auto listed = std::set<std::string_view>{ aFiles.begin(), aFiles.end() };
				result.Where      = scope(feedback_counter_scope_kind::unattributed);
				result.Parts.push_back(result.Where);
				auto unlisted = unlisted_files(aModel, listed);
				result.Parts.insert(result.Parts.end(), unlisted.begin(), unlisted.end());
				return result;
			}
			case overview_feedback_row_kind::pull_request:
				result.Where = scope(feedback_counter_scope_kind::pull_request);
				result.Parts.push_back(result.Where);
				return result;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto merged(const feedback_counter_model& aModel, const row_scopes& aScopes) -> core::feedback_counter_tally
		{
			if (aScopes.Parts.size() == 1U && aScopes.Parts.front() == aScopes.Where) {
				return aModel.tally_of(aScopes.Where);
			}
			auto parts = std::vector<core::feedback_counter_tally>{ };
			parts.reserve(aScopes.Parts.size());
			for (const auto& part : aScopes.Parts) {
				parts.push_back(aModel.tally_of(part));
			}
			return core::merge_feedback_counter_tallies(parts, aScopes.Where);
		}
	} // namespace

	auto project_overview_feedback(const feedback_counter_model& aModel, const std::span<const std::string> aFiles, const overview_feedback_request& aRequest)
		-> std::optional<overview_feedback_projection>
	{
		if (!aModel.observed() || !core::check_feedback_label_columns(aRequest.Columns)) {
			return std::nullopt;
		}
		const auto columns = core::labels_of(aRequest.Columns);
		const auto project = [&](const core::feedback_counter_tally& aTally) { return core::project_feedback_counters(aTally, columns, aRequest.Mode); };
		auto       result  = overview_feedback_projection{ };
		result.Request     = aRequest;
		result.Generation  = aModel.generation();

		// Each listed file once, then each folder from its files' tallies.
		auto tallies = std::vector<core::feedback_counter_tally>{ };
		tallies.reserve(aFiles.size());
		auto byFolder = std::map<std::string, std::vector<std::size_t>>{ };
		result.Files.reserve(aFiles.size());
		for (std::size_t index = 0U; index < aFiles.size(); ++index) {
			const auto& path = aFiles[index];
			tallies.push_back(aModel.tally_of(scope(feedback_counter_scope_kind::file, path)));
			auto row   = overview_feedback_row{ };
			row.Kind   = overview_feedback_row_kind::file;
			row.Path   = path;
			row.Counts = project(tallies.back());
			result.Files.push_back(std::move(row));
			byFolder[folder_of(path)].push_back(index);
		}
		auto folderRows = std::map<std::string, std::size_t>{ };
		result.Folders.reserve(byFolder.size());
		for (const auto& [folder, members] : byFolder) {
			auto parts = std::vector<core::feedback_counter_tally>{ };
			parts.reserve(members.size());
			for (const auto member : members) {
				parts.push_back(tallies[member]);
			}
			auto row   = overview_feedback_row{ };
			row.Kind   = overview_feedback_row_kind::folder;
			row.Path   = folder;
			row.Counts = project(core::merge_feedback_counter_tallies(parts, scope(feedback_counter_scope_kind::directory, folder)));
			folderRows.emplace(folder, result.Folders.size());
			result.Folders.push_back(std::move(row));
		}
		if (const auto remainder = scopes_of(aModel, aFiles, overview_feedback_row_kind::remainder, { })) {
			result.Remainder.Counts = project(merged(aModel, *remainder));
		}
		result.Remainder.Kind     = overview_feedback_row_kind::remainder;
		result.PullRequest.Kind   = overview_feedback_row_kind::pull_request;
		result.PullRequest.Counts = project(aModel.tally_of(scope(feedback_counter_scope_kind::pull_request)));

		if (aRequest.Sort) {
			// Folders by their own number, then files within each by theirs;
			// a tie keeps the order the files were given.
			const auto& target   = *aRequest.Sort;
			auto        firstAt  = std::map<std::string, std::size_t>{ };
			auto        folderOf = std::vector<std::string>{ };
			folderOf.reserve(aFiles.size());
			for (std::size_t index = 0U; index < aFiles.size(); ++index) {
				folderOf.push_back(folder_of(aFiles[index]));
				firstAt.try_emplace(folderOf.back(), index);
			}
			auto order = std::vector<std::size_t>(aFiles.size());
			std::iota(order.begin(), order.end(), std::size_t{ 0U });
			std::ranges::stable_sort(order, [&](const std::size_t aLeft, const std::size_t aRight) {
				const auto& leftFolder  = folderOf[aLeft];
				const auto& rightFolder = folderOf[aRight];
				if (leftFolder != rightFolder) {
					const auto left  = core::feedback_sort_key_of(result.Folders[folderRows.at(leftFolder)].Counts, target);
					const auto right = core::feedback_sort_key_of(result.Folders[folderRows.at(rightFolder)].Counts, target);
					if (core::feedback_sorts_before(left, right)) {
						return true;
					}
					if (core::feedback_sorts_before(right, left)) {
						return false;
					}
					return firstAt.at(leftFolder) < firstAt.at(rightFolder);
				}
				return core::feedback_sorts_before(core::feedback_sort_key_of(result.Files[aLeft].Counts, target), core::feedback_sort_key_of(result.Files[aRight].Counts, target));
			});
			result.Order.reserve(order.size());
			for (const auto index : order) {
				result.Order.push_back(aFiles[index]);
			}
		}
		return result;
	}

	auto list_overview_drill_down(const feedback_counter_model& aModel, const std::span<const std::string> aFiles, const overview_feedback_request& aRequest,
								  const overview_drill_down& aDrillDown) -> std::optional<overview_drill_down_listing>
	{
		if (!aModel.observed() || !core::check_feedback_label_columns(aRequest.Columns)) {
			return std::nullopt;
		}
		const auto scopes = scopes_of(aModel, aFiles, aDrillDown.Row, aDrillDown.Path);
		if (!scopes) {
			return std::nullopt;
		}
		const auto columns = core::labels_of(aRequest.Columns);
		auto       listing = overview_drill_down_listing{ };
		listing.DrillDown  = aDrillDown;
		listing.Mode       = aRequest.Mode;
		listing.Epoch      = aModel.population().Epoch;
		// The same population, columns and mode the number was counted from:
		// each part's list is exactly its number, and the parts are disjoint.
		for (const auto& part : scopes->Parts) {
			auto selection = core::select_feedback_units(aModel.population(), part, columns, aRequest.Mode, aDrillDown.Target);
			if (selection.Refusal != core::feedback_selection_refusal::none) {
				return std::nullopt;
			}
			listing.Conversations.insert(listing.Conversations.end(), std::make_move_iterator(selection.Ids.begin()), std::make_move_iterator(selection.Ids.end()));
		}
		std::ranges::sort(listing.Conversations);
		const auto counts = core::project_feedback_counters(merged(aModel, *scopes), columns, aRequest.Mode);
		listing.Count     = core::feedback_count_of(counts, aDrillDown.Target);
		listing.Limits    = counts.Limits;
		return listing;
	}
}
