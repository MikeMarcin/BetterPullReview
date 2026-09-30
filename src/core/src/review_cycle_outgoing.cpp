#include "bpr/core/review_cycle_outgoing.hpp"

#include "bpr/core/review_cycle_material.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bpr::core {
	namespace {
		/// One capture, indexed for path lookups.
		class capture_view
		{
		public:
			capture_view(const cycle_manifest& aManifest, const std::span<const outgoing_identity> aIdentities)
				: Manifest{ aManifest }
				, Identities{ aIdentities }
				, Scope{ .Version = aManifest.ScopeVersion, .Paths = aManifest.Scope }
				, Complete{ aManifest.Coverage == cycle_capture_coverage::complete && aManifest.RefusedPaths == 0U }
			{
				for (const auto& exclusion : aManifest.Exclusions) {
					(exclusion.Directory ? ExcludedDirectories : ExcludedPaths).insert(exclusion.Path);
				}
				for (const auto& entry : aManifest.Entries) {
					if (entry.Kind == manifest_entry_kind::directory || entry.Kind == manifest_entry_kind::gitlink) {
						Boundaries.insert(entry.Path);
					}
				}
			}

			/// The side a capture shows for a path; `aBase` supplies a mode the
			/// capture could not observe.
			[[nodiscard]] auto side(const std::string& aPath, const std::optional<git_tree_entry>& aBase) const -> std::expected<outgoing_side, outgoing_refusal>
			{
				if (const auto* entry = find_entry(Manifest, aPath); entry != nullptr) {
					if (entry->Availability != manifest_availability::captured || !entry->Content
						|| (entry->Kind != manifest_entry_kind::file && entry->Kind != manifest_entry_kind::symlink)) {
						return outgoing_side{ .State = outgoing_state::unavailable, .Mode = entry->Mode, .Unread = entry->Availability };
					}
					const auto identity = std::ranges::lower_bound(Identities, *entry->Content, { }, &outgoing_identity::Content);
					if (identity == Identities.end() || identity->Content != *entry->Content || !identity->Raw.valid()) {
						return std::unexpected{ outgoing_refusal::missing_identity };
					}
					return outgoing_side{
						.State         = outgoing_state::present,
						.Mode          = mode_of(*entry, aBase),
						.Object        = identity->Raw,
						.Content       = entry->Content,
						.Size          = entry->Size,
						.Tracked       = entry->Origin == manifest_origin::tracked,
						.FilterManaged = entry->FilterManaged,
					};
				}
				if (std::ranges::binary_search(Manifest.Deletions, aPath)) {
					return outgoing_side{ .State = outgoing_state::deleted, .Tracked = true };
				}
				if (!scope_contains(Scope, aPath)) {
					return outgoing_side{ .State = outgoing_state::outside_scope };
				}
				if (hidden(aPath)) {
					return outgoing_side{ .State = outgoing_state::unavailable };
				}
				return outgoing_side{ .State = Complete ? outgoing_state::absent : outgoing_state::unknown };
			}

			[[nodiscard]] auto line_feed(const outgoing_side& aSide) const -> std::optional<git_entry_oid>
			{
				if (!aSide.Content) {
					return std::nullopt;
				}
				const auto identity = std::ranges::lower_bound(Identities, *aSide.Content, { }, &outgoing_identity::Content);
				return identity != Identities.end() && identity->Content == *aSide.Content ? identity->LineFeed : std::nullopt;
			}

			[[nodiscard]] auto complete() const noexcept -> bool
			{
				return Complete;
			}

		private:
			[[nodiscard]] static auto mode_of(const manifest_entry& aEntry, const std::optional<git_tree_entry>& aBase) -> std::uint32_t
			{
				if (aEntry.Kind == manifest_entry_kind::symlink) {
					return git_mode_symlink;
				}
				if (aEntry.Mode == git_mode_regular || aEntry.Mode == git_mode_executable) {
					return aEntry.Mode;
				}
				// No execute bit observed: keep the base's file mode, never invent one.
				if (aBase && (aBase->Mode == git_mode_regular || aBase->Mode == git_mode_executable)) {
					return aBase->Mode;
				}
				return git_mode_regular;
			}

			/// Excluded by a rule, or beneath a boundary the capture did not enter.
			[[nodiscard]] auto hidden(const std::string_view aPath) const -> bool
			{
				if (ExcludedPaths.contains(std::string{ aPath })) {
					return true;
				}
				for (auto slash = aPath.rfind('/'); slash != std::string_view::npos; slash = aPath.rfind('/', slash - 1U)) {
					const auto parent = std::string{ aPath.substr(0U, slash) };
					if (ExcludedDirectories.contains(parent) || ExcludedPaths.contains(parent) || Boundaries.contains(parent)) {
						return true;
					}
					if (slash == 0U) {
						break;
					}
				}
				return false;
			}

			const cycle_manifest&              Manifest;
			std::span<const outgoing_identity> Identities;
			cycle_scope_specification          Scope;
			bool                               Complete = false;
			std::set<std::string>              ExcludedPaths;
			std::set<std::string>              ExcludedDirectories;
			std::set<std::string>              Boundaries;
		};

		[[nodiscard]] auto known(const outgoing_state aState) noexcept -> bool
		{
			return aState == outgoing_state::present || aState == outgoing_state::absent || aState == outgoing_state::deleted;
		}

		[[nodiscard]] auto changed(const outgoing_change aChange) noexcept -> bool
		{
			return aChange != outgoing_change::unchanged && aChange != outgoing_change::unknown;
		}

		/// Compares two sides by object and mode. `aLineFeed`, when given,
		/// is the right side's LF form: matching it is not a content change.
		[[nodiscard]] auto compare(const outgoing_side& aLeft, const outgoing_side& aRight, const std::optional<git_entry_oid>& aLineFeed, bool& aLineEndingsOnly)
			-> outgoing_change
		{
			if (!known(aLeft.State) || !known(aRight.State)) {
				return outgoing_change::unknown;
			}
			const auto left  = aLeft.State == outgoing_state::present;
			const auto right = aRight.State == outgoing_state::present;
			if (!left && !right) {
				return outgoing_change::unchanged;
			}
			if (left != right) {
				return left ? outgoing_change::deleted : outgoing_change::added;
			}
			if (aLeft.Object == aRight.Object) {
				return aLeft.Mode == aRight.Mode ? outgoing_change::unchanged : outgoing_change::mode_changed;
			}
			if (aLineFeed && aLeft.Object == *aLineFeed && aLeft.Mode == aRight.Mode) {
				aLineEndingsOnly = true;
				return outgoing_change::unchanged;
			}
			return outgoing_change::modified;
		}

		/// A capture side against P: bytes a filter would transform cannot be
		/// compared with P's object.
		[[nodiscard]] auto against_base(const outgoing_side& aBase, const outgoing_side& aCapture, const std::optional<git_entry_oid>& aLineFeed, bool& aLineEndingsOnly)
			-> outgoing_change
		{
			if (aCapture.State == outgoing_state::present && aCapture.FilterManaged && aBase.State == outgoing_state::present) {
				return outgoing_change::unknown;
			}
			return compare(aBase, aCapture, aLineFeed, aLineEndingsOnly);
		}

		[[nodiscard]] auto base_side(const std::optional<git_tree_entry>& aEntry) -> outgoing_side
		{
			if (!aEntry) {
				return { .State = outgoing_state::absent };
			}
			return { .State = outgoing_state::present, .Mode = aEntry->Mode, .Object = aEntry->Object };
		}

		[[nodiscard]] auto find(const std::vector<git_tree_entry>& aEntries, const std::string& aPath) -> std::optional<git_tree_entry>
		{
			const auto found = std::ranges::lower_bound(aEntries, aPath, { }, &git_tree_entry::Path);
			if (found == aEntries.end() || found->Path != aPath) {
				return std::nullopt;
			}
			return *found;
		}

		[[nodiscard]] auto valid_reference(const std::string_view aReference) noexcept -> bool
		{
			return aReference.size() <= maximum_outgoing_reference_bytes && aReference.find_first_of(std::string_view{ "\n\r\0", 3 }) == std::string_view::npos;
		}

		/// Marks selected rows that would put a file and a folder at one path.
		void mark_conflicts(const std::vector<git_tree_entry>& aCandidate, std::vector<outgoing_row>& aRows)
		{
			auto conflicted = std::set<std::string>{ };
			for (std::size_t index = 0; index < aCandidate.size(); ++index) {
				const auto prefix = aCandidate[index].Path + "/";
				auto       next   = std::ranges::lower_bound(aCandidate, prefix, { }, &git_tree_entry::Path);
				for (; next != aCandidate.end() && next->Path.starts_with(prefix); ++next) {
					conflicted.insert(aCandidate[index].Path);
					conflicted.insert(next->Path);
				}
			}
			for (auto& row : aRows) {
				if (row.Selected && conflicted.contains(row.Path)) {
					row.Gap = outgoing_gap::path_conflict;
				}
			}
		}

		[[nodiscard]] auto octal(const std::uint32_t aMode) -> std::string
		{
			auto text  = std::string{ };
			auto value = aMode;
			do {
				text.insert(text.begin(), static_cast<char>('0' + (value & 7U)));
				value >>= 3U;
			} while (value != 0U);
			return text;
		}

		[[nodiscard]] auto letter(const outgoing_change aChange) noexcept -> char
		{
			switch (aChange) {
			case outgoing_change::added:
				return 'A';
			case outgoing_change::modified:
				return 'M';
			case outgoing_change::mode_changed:
				return 'T';
			case outgoing_change::deleted:
				return 'D';
			case outgoing_change::unchanged:
			case outgoing_change::unknown:
				break;
			}
			return '?';
		}

		[[nodiscard]] auto origin(const outgoing_row& aRow) -> std::string
		{
			const auto start       = changed(aRow.StartingWork);
			const auto improvement = changed(aRow.Improvement);
			auto       text        = std::string{ start && improvement ? "starting work and cycle improvement" : start ? "starting work"
													  : improvement                                                    ? "cycle improvement"
																													   : "origin unknown" };
			if (aRow.LineEndingsOnly) {
				text += "; differs from P only by CRLF line endings";
			}
			if (aRow.Outgoing == outgoing_change::mode_changed) {
				text += "; mode " + octal(aRow.Base.Mode) + " -> " + octal(aRow.Final.Mode);
			}
			if (aRow.RenamedTo) {
				text += "; renamed to " + workspace_path_label(*aRow.RenamedTo);
			}
			if (aRow.Drifted) {
				text += "; the target changed this path since P";
			}
			return text;
		}

		/// Why a capture saw a path and did not read it, in the preview's words.
		[[nodiscard]] auto unread_reason(const std::optional<manifest_availability> aReason) -> std::string_view
		{
			if (!aReason) {
				return "not read";
			}
			switch (*aReason) {
			case manifest_availability::too_large:
				return "too large to read";
			case manifest_availability::sparse:
				return "left out by sparse checkout";
			case manifest_availability::unreadable:
				return "the read was refused";
			case manifest_availability::unsupported_kind:
				return "not a file or link";
			case manifest_availability::outside_root:
				return "behind a link or on another volume";
			case manifest_availability::link_target_unsupported:
				return "this platform cannot read its link target";
			case manifest_availability::missing_scope:
				return "it names nothing";
			case manifest_availability::not_materialized:
				return "a submodule's content";
			case manifest_availability::captured:
				break;
			}
			return "not read";
		}
	}

	auto name(const outgoing_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case outgoing_refusal::repository_required:
			return "repository_required";
		case outgoing_refusal::object_format:
			return "object_format";
		case outgoing_refusal::invalid_base:
			return "invalid_base";
		case outgoing_refusal::invalid_capture:
			return "invalid_capture";
		case outgoing_refusal::missing_identity:
			return "missing_identity";
		case outgoing_refusal::unknown_override:
			return "unknown_override";
		case outgoing_refusal::bounds:
			return "bounds";
		}
		return "unknown";
	}

	auto name(const outgoing_gap aGap) noexcept -> std::string_view
	{
		switch (aGap) {
		case outgoing_gap::none:
			return "none";
		case outgoing_gap::depends_on_starting_work:
			return "depends_on_starting_work";
		case outgoing_gap::unknown_start:
			return "unknown_start";
		case outgoing_gap::final_unknown:
			return "final_unknown";
		case outgoing_gap::final_unavailable:
			return "final_unavailable";
		case outgoing_gap::filter_managed:
			return "filter_managed";
		case outgoing_gap::gitlink:
			return "gitlink";
		case outgoing_gap::path_conflict:
			return "path_conflict";
		}
		return "unknown";
	}

	auto name(const outgoing_change aChange) noexcept -> std::string_view
	{
		switch (aChange) {
		case outgoing_change::unchanged:
			return "unchanged";
		case outgoing_change::added:
			return "added";
		case outgoing_change::modified:
			return "modified";
		case outgoing_change::mode_changed:
			return "mode_changed";
		case outgoing_change::deleted:
			return "deleted";
		case outgoing_change::unknown:
			return "unknown";
		}
		return "unknown";
	}

	auto plan_outgoing(const outgoing_request& aRequest) -> std::expected<outgoing_plan, outgoing_refusal>
	{
		const auto gitRoot = [](const workspace_root_kind aKind) { return aKind == workspace_root_kind::git_checkout || aKind == workspace_root_kind::git_linked_worktree; };
		if (!gitRoot(aRequest.Start.RootKind) || !gitRoot(aRequest.Final.RootKind)) {
			return std::unexpected{ outgoing_refusal::repository_required };
		}
		const auto algorithm = aRequest.Base.Commit.algorithm();
		if (!aRequest.Base.Commit.valid() || !aRequest.Base.Tree.valid() || aRequest.Base.Tree.algorithm() != algorithm || aRequest.Start.ObjectFormat != algorithm
			|| aRequest.Final.ObjectFormat != algorithm || (aRequest.Target && (!aRequest.Target->Head.valid() || aRequest.Target->Head.algorithm() != algorithm))) {
			return std::unexpected{ outgoing_refusal::object_format };
		}
		if (aRequest.Base.Entries.size() > maximum_git_tree_entries || aRequest.Overrides.size() > maximum_outgoing_overrides
			|| aRequest.ReviewedPaths.size() > maximum_git_tree_entries
			|| (aRequest.Target && aRequest.Target->Entries && aRequest.Target->Entries->size() > maximum_git_tree_entries)) {
			return std::unexpected{ outgoing_refusal::bounds };
		}
		if (!valid_reference(aRequest.Base.Reference) || (aRequest.Target && !valid_reference(aRequest.Target->Reference))) {
			return std::unexpected{ outgoing_refusal::invalid_base };
		}
		if (validate_manifest(aRequest.Start) || validate_manifest(aRequest.Final) || !valid_cycle_id(aRequest.StartRevision) || !valid_cycle_id(aRequest.FinalRevision)
			|| aRequest.FinalRevision < aRequest.StartRevision) {
			return std::unexpected{ outgoing_refusal::invalid_capture };
		}
		// P's listing must be exactly P's tree, or untouched entries could not
		// be preserved exactly.
		const auto rebuilt = build_git_trees(aRequest.Base.Entries, algorithm);
		if (!rebuilt || rebuilt->Root != aRequest.Base.Tree) {
			return std::unexpected{ outgoing_refusal::invalid_base };
		}

		auto identities = aRequest.Identities;
		std::ranges::sort(identities);
		for (const auto& identity : identities) {
			if (identity.Raw.algorithm() != algorithm || (identity.LineFeed && identity.LineFeed->algorithm() != algorithm)) {
				return std::unexpected{ outgoing_refusal::object_format };
			}
		}
		const auto start    = capture_view{ aRequest.Start, identities };
		const auto final    = capture_view{ aRequest.Final, identities };
		const auto reviewed = cycle_scope_specification{ .Version = 1, .Paths = aRequest.ReviewedPaths };

		auto overrides = std::map<std::string, outgoing_choice>{ };
		for (const auto& choice : aRequest.Overrides) {
			if (!overrides.emplace(choice.Path, choice.Choice).second) {
				return std::unexpected{ outgoing_refusal::unknown_override };
			}
		}

		auto paths = std::set<std::string>{ };
		for (const auto& entry : aRequest.Base.Entries) {
			paths.insert(entry.Path);
		}
		for (const auto* manifest : { &aRequest.Start, &aRequest.Final }) {
			for (const auto& entry : manifest->Entries) {
				paths.insert(entry.Path);
			}
			paths.insert(manifest->Deletions.begin(), manifest->Deletions.end());
		}

		auto plan = outgoing_plan{
			.Mode          = aRequest.Mode,
			.StartRevision = aRequest.StartRevision,
			.FinalRevision = aRequest.FinalRevision,
			.BaseCommit    = aRequest.Base.Commit,
			.BaseTree      = aRequest.Base.Tree,
			.BaseReference = aRequest.Base.Reference,
			.StartComplete = start.complete(),
			.FinalComplete = final.complete(),
		};
		if (aRequest.Target) {
			plan.TargetSource    = aRequest.Target->Source;
			plan.TargetReference = aRequest.Target->Reference;
			plan.TargetHead      = aRequest.Target->Head;
			plan.TargetMoved     = aRequest.Target->Head != aRequest.Base.Commit;
		}

		for (const auto& path : paths) {
			const auto baseEntry = find(aRequest.Base.Entries, path);
			auto       startSide = start.side(path, baseEntry);
			auto       finalSide = final.side(path, baseEntry);
			if (!startSide || !finalSide) {
				return std::unexpected{ outgoing_refusal::missing_identity };
			}
			auto row = outgoing_row{ .Path = path, .Base = base_side(baseEntry), .Start = *startSide, .Final = *finalSide };

			auto startEndings    = false;
			auto finalEndings    = false;
			auto ignored         = false;
			row.StartingWork     = against_base(row.Base, row.Start, start.line_feed(row.Start), startEndings);
			row.Improvement      = compare(row.Start, row.Final, std::nullopt, ignored);
			const auto finalBase = against_base(row.Base, row.Final, final.line_feed(row.Final), finalEndings);
			row.LineEndingsOnly  = startEndings || finalEndings;

			const auto improved     = changed(row.Improvement);
			const auto startChanged = changed(row.StartingWork);
			const auto finalKnown   = known(row.Final.State);
			const auto differs      = finalKnown && (changed(finalBase) || (finalBase == outgoing_change::unknown && improved));
			const auto filtered     = (row.Final.State == outgoing_state::present && row.Final.FilterManaged) || (row.Start.State == outgoing_state::present && row.Start.FilterManaged);
			const auto gitlink      = row.Base.Mode == git_mode_gitlink;

			if (!finalKnown) {
				// [[BPR-1244]]. Seen and not read: listed unless neither capture read
				// a path P has, where nothing is known to have changed.
				if (row.Final.Unread && (row.Base.State != outgoing_state::present || known(row.Start.State))) {
					row.Gap = outgoing_gap::final_unavailable;
				} else if (startChanged) {
					row.Gap = outgoing_gap::final_unknown;
				}
			} else if (differs) {
				if (gitlink) {
					row.Gap      = outgoing_gap::gitlink;
					row.Proposed = improved;
				} else if (filtered) {
					row.Gap      = outgoing_gap::filter_managed;
					row.Proposed = improved;
				} else if (!known(row.Start.State)) {
					row.Gap = outgoing_gap::unknown_start;
				} else if (improved && startChanged) {
					if (aRequest.Mode == outgoing_selection_mode::starting_work_and_improvements) {
						row.Proposed = true;
					} else {
						row.Gap = outgoing_gap::depends_on_starting_work;
					}
				} else if (improved) {
					row.Proposed = true;
				} else if (startChanged) {
					const auto relevant = row.Start.State != outgoing_state::present || row.Start.Tracked || (!reviewed.Paths.empty() && scope_contains(reviewed, path));
					row.Unrelated       = !relevant;
					row.Proposed        = relevant && aRequest.Mode == outgoing_selection_mode::starting_work_and_improvements;
				}
			}

			const auto listed = startChanged || improved || differs || row.Gap != outgoing_gap::none;
			if (const auto choice = overrides.find(path); choice != overrides.end()) {
				// Only a path that can go out takes a choice.
				if (!listed || !differs) {
					return std::unexpected{ outgoing_refusal::unknown_override };
				}
				row.Choice = choice->second;
				overrides.erase(choice);
			}
			if (!listed) {
				continue;
			}
			row.Selected = differs && (row.Choice ? *row.Choice == outgoing_choice::include : row.Proposed);
			row.Outgoing = row.Selected ? (finalBase == outgoing_change::unknown ? outgoing_change::modified : finalBase) : outgoing_change::unchanged;
			plan.Rows.push_back(std::move(row));
		}
		if (!overrides.empty()) {
			return std::unexpected{ outgoing_refusal::unknown_override };
		}

		// P's entries, with only the selected rows changed.
		auto candidate = std::map<std::string, git_tree_entry>{ };
		for (const auto& entry : aRequest.Base.Entries) {
			candidate.emplace(entry.Path, entry);
		}
		for (const auto& row : plan.Rows) {
			if (!row.Selected) {
				continue;
			}
			if (row.Final.State == outgoing_state::present) {
				candidate.insert_or_assign(row.Path, git_tree_entry{ .Path = row.Path, .Mode = row.Final.Mode, .Object = *row.Final.Object });
			} else {
				candidate.erase(row.Path);
			}
		}
		plan.Candidate.reserve(candidate.size());
		for (auto& [path, entry] : candidate) {
			plan.Candidate.push_back(std::move(entry));
		}
		mark_conflicts(plan.Candidate, plan.Rows);

		auto added = std::map<git_entry_oid, std::string>{ };
		for (const auto& row : plan.Rows) {
			if (row.Outgoing == outgoing_change::added && row.Final.Object) {
				added.try_emplace(*row.Final.Object, row.Path);
			}
		}
		for (auto& row : plan.Rows) {
			if (row.Outgoing == outgoing_change::deleted && row.Base.Object) {
				if (const auto moved = added.find(*row.Base.Object); moved != added.end()) {
					row.RenamedTo = moved->second;
				}
			}
		}
		if (aRequest.Target && aRequest.Target->Entries && plan.TargetMoved) {
			for (auto& row : plan.Rows) {
				row.Drifted = row.Selected && find(*aRequest.Target->Entries, row.Path) != find(aRequest.Base.Entries, row.Path);
			}
		}
		for (const auto& row : plan.Rows) {
			plan.Undecided += row.Gap == outgoing_gap::depends_on_starting_work && !row.Choice ? 1U : 0U;
			plan.Blocked += row.Selected && (row.Gap == outgoing_gap::filter_managed || row.Gap == outgoing_gap::gitlink || row.Gap == outgoing_gap::path_conflict) ? 1U : 0U;
			plan.Unread += row.Gap == outgoing_gap::final_unavailable ? 1U : 0U;
		}
		if (const auto built = build_git_trees(plan.Candidate, algorithm); built) {
			plan.CandidateTree = built->Root;
		}
		plan.NoChange = plan.Candidate == aRequest.Base.Entries;
		return plan;
	}

	auto render_outgoing_preview(const outgoing_plan& aPlan) -> std::string
	{
		auto out  = std::string{ };
		auto line = [&out](const std::string_view aText) {
			out += aText;
			out += '\n';
		};
		line("Publication preview: nothing has been published.");
		line("Inspected R" + std::to_string(aPlan.StartRevision.value()) + (aPlan.StartComplete ? " (complete)" : " (partial: paths it did not see keep P's entries)") + "; final R"
			 + std::to_string(aPlan.FinalRevision.value()) + (aPlan.FinalComplete ? " (complete)." : " (partial: paths it did not see keep P's entries)."));
		line("Parent P: " + std::string{ canonical_hex(aPlan.BaseCommit).view() } + (aPlan.BaseReference.empty() ? std::string{ } : " (" + aPlan.BaseReference + ")") + ".");
		if (aPlan.TargetHead) {
			line("Target T: " + aPlan.TargetReference + " at " + std::string{ canonical_hex(*aPlan.TargetHead).view() }
				 + (aPlan.TargetSource == publication_target_source::local_reference ? " (this checkout's local reference, not the remote now)" : " (observed on the provider)")
				 + (aPlan.TargetMoved ? "; it has moved since P: the candidate's parent is stale against it." : "; it is P."));
		} else {
			line("Target T: not observed.");
		}
		line(aPlan.Mode == outgoing_selection_mode::starting_work_and_improvements ? "Starting choice: the relevant starting work plus the cycle's improvements."
																				   : "Starting choice: only the cycle's improvements.");
		if (aPlan.NoChange) {
			line("No change: the candidate's tree is P's tree. There is nothing to publish.");
		} else if (aPlan.CandidateTree.valid()) {
			line("Candidate tree: " + std::string{ canonical_hex(aPlan.CandidateTree).view() } + ".");
		}
		if (aPlan.Unread != 0U) {
			line(std::to_string(aPlan.Unread) + (aPlan.Unread == 1U ? " path the final capture could not read is" : " paths the final capture could not read are")
				 + " not in the candidate as captured. See Not included.");
		}
		auto section = [&](const std::string_view aTitle, auto aSelect, auto aDescribe) {
			auto first = true;
			for (const auto& row : aPlan.Rows) {
				if (!aSelect(row)) {
					continue;
				}
				if (first) {
					line("");
					line(aTitle);
					first = false;
				}
				line(std::string{ "  " } + aDescribe(row) + "  " + workspace_path_label(row.Path));
			}
		};
		section("Outgoing (P -> candidate):", [](const outgoing_row& aRow) { return aRow.Selected && aRow.Gap == outgoing_gap::none; }, [](const outgoing_row& aRow) { return std::string{ letter(aRow.Outgoing) } + "  " + origin(aRow) + ":"; });
		section("Needs a decision:", [](const outgoing_row& aRow) { return aRow.Gap == outgoing_gap::depends_on_starting_work && !aRow.Choice; }, [](const outgoing_row&) { return std::string{ "the improvement depends on starting work that is not included:" }; });
		section("Cannot be built here (prepare externally or leave out):", [](const outgoing_row& aRow) { return aRow.Selected && (aRow.Gap == outgoing_gap::filter_managed || aRow.Gap == outgoing_gap::gitlink || aRow.Gap == outgoing_gap::path_conflict); }, [](const outgoing_row& aRow) { return std::string{ aRow.Gap == outgoing_gap::filter_managed ? "a Git filter such as LFS makes its committed object:"
																																																																														: aRow.Gap == outgoing_gap::gitlink  ? "a submodule boundary changed:"
																																																																																							 : "a file and a folder would share this path:" }; });
		section("Not included:", [](const outgoing_row& aRow) { return !aRow.Selected && !(aRow.Gap == outgoing_gap::depends_on_starting_work && !aRow.Choice); }, [](const outgoing_row& aRow) {
					if (aRow.Choice == outgoing_choice::exclude) {
						return std::string{ "left out by choice:" };
					}
					switch (aRow.Gap) {
					case outgoing_gap::unknown_start:
						return std::string{ "changed where the starting capture did not look; origin unknown:" };
					case outgoing_gap::final_unknown:
						return std::string{ "the final capture did not read it; P's entry stays:" };
					case outgoing_gap::final_unavailable:
						return "the final capture saw it but could not read it (" + std::string{ unread_reason(aRow.Final.Unread) } + ")"
							 + (aRow.Base.State == outgoing_state::present ? "; P's entry stays:" : "; it is not in the candidate:");
					case outgoing_gap::filter_managed:
						return std::string{ "managed by a Git filter:" };
					case outgoing_gap::gitlink:
						return std::string{ "a submodule boundary:" };
					case outgoing_gap::none:
					case outgoing_gap::depends_on_starting_work:
					case outgoing_gap::path_conflict:
						break;
					}
					if (aRow.Unrelated) {
						return std::string{ "starting work nothing in the cycle touched:" };
					}
					if (changed(aRow.StartingWork) && !changed(aRow.Improvement)) {
						return std::string{ "starting work, not in this choice:" };
					}
					return std::string{ "no outgoing change:" }; });
		return out;
	}
}
