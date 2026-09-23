#include "bpr/application/review_synchronization.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace bpr::application {
	auto name(const reconciliation_kind aKind) noexcept -> std::string_view
	{
		switch (aKind) {
		case reconciliation_kind::unchanged:
			return "unchanged";
		case reconciliation_kind::added:
			return "added";
		case reconciliation_kind::updated:
			return "updated";
		case reconciliation_kind::removed:
			return "removed";
		case reconciliation_kind::conflicted:
			return "conflicted";
		case reconciliation_kind::removed_conflicted:
			return "removed_conflicted";
		}
		return "unchanged";
	}

	auto reconciliation_result::conflicts() const -> std::vector<std::string>
	{
		auto identifiers = std::vector<std::string>{ };
		for (const auto& entry : Entries) {
			if (entry.Kind == reconciliation_kind::conflicted
				|| entry.Kind == reconciliation_kind::removed_conflicted) {
				identifiers.push_back(entry.ServerId);
			}
		}
		return identifiers;
	}

	auto reconcile(
		const std::span<const review_comment_projection> aLocal,
		const std::span<const review_comment_projection> aObserved,
		const std::span<const std::string>               aLocallyEdited)
		-> reconciliation_result
	{
		auto known = std::unordered_map<std::string, const review_comment_projection*>{ };
		for (const auto& comment : aLocal) {
			known.emplace(comment.ServerId, &comment);
		}

		auto edited = std::unordered_set<std::string>{ };
		for (const auto& identifier : aLocallyEdited) {
			edited.insert(identifier);
		}

		auto result = reconciliation_result{ };
		result.Projection.reserve(aObserved.size());

		for (const auto& observed : aObserved) {
			const auto existing = known.find(observed.ServerId);
			if (existing == known.end()) {
				// A comment nobody here knew about. Someone else wrote it, and
				// it belongs in the projection exactly as the server reports.
				result.Projection.push_back(observed);
				result.Entries.push_back(reconciliation_entry{
					.ServerId = observed.ServerId,
					.Kind     = reconciliation_kind::added,
				});
				continue;
			}

			const auto& local   = *existing->second;
			const auto  changed = local != observed;

			// The server's text becomes the projection either way. A conflict
			// is reported, not resolved: the unpublished edit is still the
			// reviewer's to keep or abandon, and this code may not choose.
			result.Projection.push_back(observed);
			if (!changed) {
				result.Entries.push_back(reconciliation_entry{
					.ServerId = observed.ServerId,
					.Kind     = reconciliation_kind::unchanged,
				});
				continue;
			}
			result.Entries.push_back(reconciliation_entry{
				.ServerId = observed.ServerId,
				.Kind     = edited.contains(observed.ServerId)
					? reconciliation_kind::conflicted
					: reconciliation_kind::updated,
			});
		}

		// Anything the server no longer reports is gone. Retaining it because
		// the projection remembers it would show a reader a comment that is not
		// there, which is the projection asserting itself over the authority.
		// A deletion under an unpublished edit is still a conflict: the edit
		// has nowhere to go, and a person must be told ([[BPR-0888]]).
		auto present = std::unordered_set<std::string>{ };
		for (const auto& observed : aObserved) {
			present.insert(observed.ServerId);
		}
		for (const auto& comment : aLocal) {
			if (!present.contains(comment.ServerId)) {
				result.Entries.push_back(reconciliation_entry{
					.ServerId = comment.ServerId,
					.Kind     = edited.contains(comment.ServerId)
						? reconciliation_kind::removed_conflicted
						: reconciliation_kind::removed,
				});
			}
		}

		// Ordered by identity so two refreshes of the same state are identical
		// rather than merely equivalent.
		std::ranges::sort(
			result.Projection,
			{ },
			&review_comment_projection::ServerId);
		std::ranges::sort(result.Entries, { }, &reconciliation_entry::ServerId);
		return result;
	}
} // namespace bpr::application
