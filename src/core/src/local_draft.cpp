#include "bpr/core/local_draft.hpp"

#include "bpr/core/diff_compute.hpp"
#include "bpr/core/review_cycle_publication.hpp"
#include "bpr/core/sha256.hpp"
#include "bpr/core/utf8.hpp"

#include <glaze/glaze.hpp>

#include <algorithm>
#include <map>
#include <ranges>
#include <set>
#include <tuple>
#include <utility>

namespace bpr::core::detail {
	/// The draft as glaze reads and writes it. Identities are canonical hex.
	struct wire_identity
	{
		std::uint64_t Volume     = 0;
		std::uint64_t ObjectHigh = 0;
		std::uint64_t ObjectLow  = 0;
	};
	struct wire_side
	{
		std::uint32_t Mode = 0;
		std::string   Object;
		std::uint64_t Size    = 0;
		bool          Working = false;
	};
	struct wire_change
	{
		std::string              Path;
		std::string              Change;
		std::optional<wire_side> Left;
		std::optional<wire_side> Right;
	};
	struct wire_candidate
	{
		std::uint64_t            Ordinal = 0;
		std::string              Head;
		std::string              TargetTip;
		std::string              Base;
		std::vector<std::string> Included;
		std::vector<wire_change> Changes;
		std::vector<std::string> Qualifications;
		std::int64_t             CapturedAt = 0;
	};
	struct wire_note
	{
		std::uint64_t Id = 0;
		std::string   Text;
		std::string   Path;
		std::uint64_t Candidate = 0;
		std::string   Origin;
	};
	struct wire_hop
	{
		std::string Remote;
		std::string Location;
	};
	struct wire_association
	{
		std::string   Host;
		std::string   Owner;
		std::string   Name;
		std::uint64_t Number = 0;
		std::string   HeadRef;
	};
	struct wire_draft
	{
		std::string                     Format;
		std::uint32_t                   Version = 0;
		std::string                     Id;
		std::uint64_t                   Revision = 0;
		std::string                     Title;
		std::string                     Location;
		wire_identity                   Repository;
		wire_identity                   Worktree;
		std::string                     Branch;
		std::string                     TargetRemote;
		std::string                     TargetHost;
		std::string                     TargetOwner;
		std::string                     TargetName;
		std::string                     TargetBranch;
		std::string                     TargetOrigin;
		std::vector<wire_hop>           Chain;
		std::vector<wire_candidate>     Candidates;
		std::vector<wire_note>          Notes;
		std::uint64_t                   NextNote = 1;
		std::optional<wire_association> Association;
		/// Written before RFC-BPR-0109 was deferred (2026-11-12): read and
		/// dropped, never written. A store migration rewrites such drafts.
		std::optional<glz::raw_json> Evidence;
		std::optional<glz::raw_json> Preparation;
		std::optional<glz::raw_json> Publication;
		std::optional<glz::raw_json> Workflow;
		std::int64_t                 Created = 0;
		std::int64_t                 Updated = 0;
	};
} // namespace bpr::core::detail

template <>
struct glz::meta<bpr::core::detail::wire_identity>
{
	using value_type            = bpr::core::detail::wire_identity;
	static constexpr auto value = object("volume", &value_type::Volume, "high", &value_type::ObjectHigh, "low", &value_type::ObjectLow);
};
template <>
struct glz::meta<bpr::core::detail::wire_side>
{
	using value_type            = bpr::core::detail::wire_side;
	static constexpr auto value = object("mode", &value_type::Mode, "object", &value_type::Object, "size", &value_type::Size, "working", &value_type::Working);
};
template <>
struct glz::meta<bpr::core::detail::wire_change>
{
	using value_type            = bpr::core::detail::wire_change;
	static constexpr auto value = object("path", &value_type::Path, "change", &value_type::Change, "left", &value_type::Left, "right", &value_type::Right);
};
template <>
struct glz::meta<bpr::core::detail::wire_candidate>
{
	using value_type            = bpr::core::detail::wire_candidate;
	static constexpr auto value = object("ordinal", &value_type::Ordinal, "head", &value_type::Head, "target_tip", &value_type::TargetTip, "base", &value_type::Base, "included", &value_type::Included, "changes", &value_type::Changes,
										 "qualifications", &value_type::Qualifications, "captured_at", &value_type::CapturedAt);
};
template <>
struct glz::meta<bpr::core::detail::wire_note>
{
	using value_type            = bpr::core::detail::wire_note;
	static constexpr auto value = object("id", &value_type::Id, "text", &value_type::Text, "path", &value_type::Path, "candidate", &value_type::Candidate, "origin", &value_type::Origin);
};
template <>
struct glz::meta<bpr::core::detail::wire_hop>
{
	using value_type            = bpr::core::detail::wire_hop;
	static constexpr auto value = object("remote", &value_type::Remote, "location", &value_type::Location);
};
template <>
struct glz::meta<bpr::core::detail::wire_association>
{
	using value_type            = bpr::core::detail::wire_association;
	static constexpr auto value = object("host", &value_type::Host, "owner", &value_type::Owner, "name", &value_type::Name, "number", &value_type::Number, "head_ref", &value_type::HeadRef);
};
template <>
struct glz::meta<bpr::core::detail::wire_draft>
{
	using value_type            = bpr::core::detail::wire_draft;
	static constexpr auto value = object("format", &value_type::Format, "version", &value_type::Version, "id", &value_type::Id, "revision", &value_type::Revision, "title", &value_type::Title, "location", &value_type::Location,
										 "repository", &value_type::Repository, "worktree", &value_type::Worktree, "branch", &value_type::Branch, "target_remote", &value_type::TargetRemote, "target_host",
										 &value_type::TargetHost, "target_owner", &value_type::TargetOwner, "target_name", &value_type::TargetName, "target_branch", &value_type::TargetBranch, "target_origin",
										 &value_type::TargetOrigin, "chain", &value_type::Chain, "candidates", &value_type::Candidates, "notes", &value_type::Notes, "next_note", &value_type::NextNote, "association",
										 &value_type::Association, "evidence", &value_type::Evidence, "preparation", &value_type::Preparation, "publication", &value_type::Publication, "workflow", &value_type::Workflow, "created", &value_type::Created, "updated", &value_type::Updated);
};

namespace bpr::core {
	namespace {
		constexpr auto draft_format         = std::string_view{ "bpr.local-draft" };
		constexpr auto draft_format_version = std::uint32_t{ 1U };
		constexpr auto read_options         = glz::opts{ .error_on_unknown_keys = true };

		[[nodiscard]] auto lower_hex(const std::string_view aText) noexcept -> bool
		{
			return std::ranges::all_of(aText, [](const char aByte) { return (aByte >= '0' && aByte <= '9') || (aByte >= 'a' && aByte <= 'f'); });
		}

		[[nodiscard]] auto plain_text(const std::string_view aText, const std::size_t aLimit, const bool aLines) -> bool
		{
			return aText.size() <= aLimit && utf8::valid(aText) && std::ranges::none_of(aText, [aLines](const char aByte) {
					   const auto byte = static_cast<unsigned char>(aByte);
					   return (byte < 0x20U && !(aLines && (aByte == '\n' || aByte == '\t'))) || byte == 0x7FU;
				   });
		}

		[[nodiscard]] auto valid_path(const std::string_view aPath) -> bool
		{
			if (aPath.empty() || aPath.size() > 4096U || aPath.front() == '/' || aPath.back() == '/' || !utf8::valid(aPath)) {
				return false;
			}
			for (const auto part : std::views::split(aPath, '/')) {
				const auto segment = std::string_view{ part.begin(), part.end() };
				if (segment.empty() || segment == "." || segment == ".." || segment == ".git") {
					return false;
				}
			}
			return std::ranges::none_of(aPath, [](const char aByte) { return aByte == '\0'; });
		}

		[[nodiscard]] auto same_repository(const remote_location& aLeft, const remote_location& aRight) noexcept -> bool
		{
			const auto folded = [](std::string_view aText) {
				auto text = std::string{ aText };
				std::ranges::transform(text, text.begin(), [](const char aByte) { return aByte >= 'A' && aByte <= 'Z' ? static_cast<char>(aByte - 'A' + 'a') : aByte; });
				return text;
			};
			return folded(aLeft.Host) == folded(aRight.Host) && folded(aLeft.Owner) == folded(aRight.Owner) && folded(aLeft.Name) == folded(aRight.Name);
		}

		[[nodiscard]] auto stale(const local_draft& aDraft, const std::uint64_t aExpected) noexcept -> bool
		{
			return aDraft.Version != aExpected;
		}

		[[nodiscard]] auto advanced(local_draft aDraft, const std::int64_t aNow) -> local_draft
		{
			++aDraft.Version;
			aDraft.Updated = std::max(aDraft.Updated, aNow);
			return aDraft;
		}

		[[nodiscard]] auto side_of(const git_tree_entry& aEntry) -> draft_side
		{
			return draft_side{ .Mode = aEntry.Mode, .Object = aEntry.Object, .Size = 0U, .Origin = draft_material_origin::committed };
		}

		[[nodiscard]] auto change_between(const std::optional<draft_side>& aLeft, const std::optional<draft_side>& aRight) -> std::optional<draft_change>
		{
			if (!aLeft && !aRight) {
				return std::nullopt;
			}
			if (!aLeft) {
				return draft_change::added;
			}
			if (!aRight) {
				return draft_change::deleted;
			}
			if (aLeft->Object != aRight->Object) {
				return draft_change::modified;
			}
			if (aLeft->Mode != aRight->Mode) {
				return draft_change::mode;
			}
			return std::nullopt;
		}

		[[nodiscard]] auto valid_candidate(const local_draft_candidate& aCandidate) -> bool
		{
			const auto algorithm = aCandidate.Head.algorithm();
			if (aCandidate.Ordinal == 0U || !aCandidate.Head.valid() || !aCandidate.TargetTip.valid() || !aCandidate.Base.valid() || aCandidate.TargetTip.algorithm() != algorithm
				|| aCandidate.Base.algorithm() != algorithm || aCandidate.Changes.size() > maximum_local_draft_changes
				|| aCandidate.Included.size() > maximum_local_draft_working_files) {
				return false;
			}
			const auto sideValid = [algorithm](const std::optional<draft_side>& aSide) {
				return !aSide || (aSide->Object.valid() && aSide->Object.algorithm() == algorithm && aSide->Size <= maximum_local_draft_file_bytes);
			};
			const auto* previous = static_cast<const std::string*>(nullptr);
			for (const auto& change : aCandidate.Changes) {
				if (!valid_path(change.Path) || (previous != nullptr && !(*previous < change.Path)) || !sideValid(change.Left) || !sideValid(change.Right)
					|| change_between(change.Left, change.Right) != change.Change) {
					return false;
				}
				previous = &change.Path;
			}
			return std::ranges::all_of(aCandidate.Included, [](const std::string& aPath) { return valid_path(aPath); });
		}

		template <typename TEnum, std::size_t TCount>
		[[nodiscard]] auto enum_from(const std::string_view aName, const std::array<TEnum, TCount>& aValues) -> std::optional<TEnum>
		{
			for (const auto value : aValues) {
				if (name(value) == aName) {
					return value;
				}
			}
			return std::nullopt;
		}

		constexpr auto all_changes        = std::array{ draft_change::added, draft_change::modified, draft_change::deleted, draft_change::mode };
		constexpr auto all_qualifications = std::array{ candidate_qualification::includes_working_files, candidate_qualification::sparse_checkout, candidate_qualification::working_scan_incomplete,
														candidate_qualification::line_ending_conversion };
		constexpr auto all_origins        = std::array{ target_origin::chosen, target_origin::remote_default };

		[[nodiscard]] auto note_origin_name(const note_origin aOrigin) noexcept -> std::string_view
		{
			return aOrigin == note_origin::harness ? "harness" : "author";
		}
	}

	auto valid(const local_draft_id& aId) noexcept -> bool
	{
		return aId.Value.size() == 32U && lower_hex(aId.Value);
	}

	auto local_remote_path(const std::string_view aUrl) -> std::optional<std::string>
	{
		if (aUrl.empty() || aUrl.size() > maximum_remote_text_bytes || normalize_remote_url(aUrl)) {
			return std::nullopt;
		}
		if (aUrl.starts_with("file://")) {
			const auto path = aUrl.substr(7U);
			return path.starts_with('/') ? std::optional{ std::string{ path } } : std::nullopt;
		}
		const auto windowsDrive = aUrl.size() >= 3U && ((aUrl[0] >= 'A' && aUrl[0] <= 'Z') || (aUrl[0] >= 'a' && aUrl[0] <= 'z')) && aUrl[1] == ':'
			&& (aUrl[2] == '\\' || aUrl[2] == '/');
		if (aUrl.starts_with('/') || windowsDrive || aUrl.starts_with("./") || aUrl.starts_with("../") || aUrl == "." || aUrl == "..") {
			return std::string{ aUrl };
		}
		// An scp-like `host:path` or any other scheme is not a local clone.
		return std::nullopt;
	}

	auto name(const remote_chain_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case remote_chain_refusal::cycle:                 return "cycle";
		case remote_chain_refusal::unreadable:            return "unreadable";
		case remote_chain_refusal::too_many_hops:         return "too_many_hops";
		case remote_chain_refusal::no_hosted_remote:      return "no_hosted_remote";
		case remote_chain_refusal::several_local_remotes: return "several_local_remotes";
		}
		return "unknown";
	}

	auto step_remote_chain(remote_chain_walk& aWalk, const std::string_view aLocation, const std::optional<std::vector<git_remote>>& aRemotes) -> remote_chain_step
	{
		const auto location = std::string{ aLocation };
		if (std::ranges::find(aWalk.Visited, location) != aWalk.Visited.end()) {
			return remote_chain_refused{ .Reason = remote_chain_refusal::cycle, .Location = location };
		}
		aWalk.Visited.push_back(location);
		if (!aRemotes) {
			return remote_chain_refused{ .Reason = remote_chain_refusal::unreadable, .Location = location };
		}
		auto hosted = std::vector<hosted_destination>{ };
		auto local  = std::vector<std::pair<std::string, std::string>>{ };
		for (const auto& remote : *aRemotes) {
			if (auto repository = normalize_remote_url(remote.Url)) {
				hosted.push_back({ .Remote = remote.Name, .Repository = std::move(*repository) });
			} else if (auto path = local_remote_path(remote.Url)) {
				local.emplace_back(remote.Name, std::move(*path));
			}
		}
		if (!hosted.empty()) {
			return remote_chain_resolved{ .Hops = aWalk.Hops, .Destinations = std::move(hosted) };
		}
		if (local.empty()) {
			return remote_chain_refused{ .Reason = remote_chain_refusal::no_hosted_remote, .Location = location };
		}
		if (local.size() > 1U) {
			return remote_chain_refused{ .Reason = remote_chain_refusal::several_local_remotes, .Location = location };
		}
		if (aWalk.Hops.size() >= maximum_local_remote_hops) {
			return remote_chain_refused{ .Reason = remote_chain_refusal::too_many_hops, .Location = location };
		}
		return remote_chain_follow{ .From = location, .Remote = local.front().first, .Path = local.front().second };
	}

	auto name(const target_origin aOrigin) noexcept -> std::string_view
	{
		return aOrigin == target_origin::chosen ? "chosen" : "remote_default";
	}

	auto name(const target_question aQuestion) noexcept -> std::string_view
	{
		switch (aQuestion) {
		case target_question::several_repositories:      return "several_repositories";
		case target_question::no_default_branch:         return "no_default_branch";
		case target_question::chosen_repository_missing: return "chosen_repository_missing";
		}
		return "unknown";
	}

	auto choose_target(const std::span<const destination_facts> aFacts, const std::optional<chosen_target>& aPrior) -> std::expected<chosen_target, target_selection>
	{
		auto choices = std::vector<destination_facts>(aFacts.begin(), aFacts.end());
		if (aPrior) {
			const auto kept = std::ranges::find_if(aFacts, [&aPrior](const destination_facts& aFact) {
				return aFact.Destination.Remote == aPrior->Target.Remote && same_repository(aFact.Destination.Repository, aPrior->Target.Repository);
			});
			if (kept != aFacts.end()) {
				return *aPrior;
			}
			return std::unexpected{ target_selection{ .Question = target_question::chosen_repository_missing, .Choices = std::move(choices) } };
		}
		auto repositories = std::vector<remote_location>{ };
		for (const auto& fact : aFacts) {
			if (std::ranges::none_of(repositories, [&fact](const remote_location& aKnown) { return same_repository(aKnown, fact.Destination.Repository); })) {
				repositories.push_back(fact.Destination.Repository);
			}
		}
		if (repositories.size() != 1U) {
			return std::unexpected{ target_selection{ .Question = target_question::several_repositories, .Choices = std::move(choices) } };
		}
		// One repository, perhaps under several remote names: the first that
		// knows its default branch decides.
		for (const auto& fact : aFacts) {
			if (fact.DefaultBranch && valid_publication_branch(*fact.DefaultBranch)) {
				return chosen_target{
					.Target = { .Remote = fact.Destination.Remote, .Repository = fact.Destination.Repository, .Branch = *fact.DefaultBranch },
					.Origin = target_origin::remote_default,
				};
			}
		}
		return std::unexpected{ target_selection{ .Question = target_question::no_default_branch, .Choices = std::move(choices) } };
	}

	auto choose_target_explicitly(const std::span<const destination_facts> aFacts, const local_draft_target& aChoice) -> std::optional<chosen_target>
	{
		if (!valid_publication_branch(aChoice.Branch)) {
			return std::nullopt;
		}
		const auto known = std::ranges::any_of(aFacts, [&aChoice](const destination_facts& aFact) {
			return aFact.Destination.Remote == aChoice.Remote && same_repository(aFact.Destination.Repository, aChoice.Repository);
		});
		return known ? std::optional{ chosen_target{ .Target = aChoice, .Origin = target_origin::chosen } } : std::nullopt;
	}

	auto name(const source_state aState) noexcept -> std::string_view
	{
		switch (aState) {
		case source_state::branch:     return "branch";
		case source_state::detached:   return "detached";
		case source_state::unborn:     return "unborn";
		case source_state::unreadable: return "unreadable";
		}
		return "unknown";
	}

	auto name(const comparison_state aState) noexcept -> std::string_view
	{
		switch (aState) {
		case comparison_state::ready:              return "ready";
		case comparison_state::target_unavailable: return "target_unavailable";
		case comparison_state::no_common_base:     return "no_common_base";
		case comparison_state::several_bases:      return "several_bases";
		}
		return "unknown";
	}

	auto decide_comparison(const std::span<const git_commit_oid> aMergeBases) -> local_draft_comparison
	{
		auto bases = std::vector<git_commit_oid>(aMergeBases.begin(), aMergeBases.end());
		std::ranges::sort(bases);
		const auto [first, last] = std::ranges::unique(bases);
		bases.erase(first, last);
		if (bases.empty()) {
			return { .State = comparison_state::no_common_base };
		}
		if (bases.size() > 1U) {
			return { .State = comparison_state::several_bases, .Bases = std::move(bases) };
		}
		return { .State = comparison_state::ready, .Base = bases.front() };
	}

	auto name(const draft_change aChange) noexcept -> std::string_view
	{
		switch (aChange) {
		case draft_change::added:    return "added";
		case draft_change::modified: return "modified";
		case draft_change::deleted:  return "deleted";
		case draft_change::mode:     return "mode";
		}
		return "unknown";
	}

	auto compare_trees(const std::span<const git_tree_entry> aBase, const std::span<const git_tree_entry> aHead) -> std::vector<draft_path_change>
	{
		auto sides = std::map<std::string, std::pair<std::optional<draft_side>, std::optional<draft_side>>>{ };
		for (const auto& entry : aBase) {
			sides[entry.Path].first = side_of(entry);
		}
		for (const auto& entry : aHead) {
			sides[entry.Path].second = side_of(entry);
		}
		auto changes = std::vector<draft_path_change>{ };
		for (auto& [path, pair] : sides) {
			if (const auto change = change_between(pair.first, pair.second)) {
				changes.push_back({ .Path = path, .Change = *change, .Left = std::move(pair.first), .Right = std::move(pair.second) });
			}
		}
		return changes;
	}

	auto name(const working_edit aEdit) noexcept -> std::string_view
	{
		switch (aEdit) {
		case working_edit::modified:  return "modified";
		case working_edit::deleted:   return "deleted";
		case working_edit::untracked: return "untracked";
		}
		return "unknown";
	}

	auto name(const working_qualification aQualification) noexcept -> std::string_view
	{
		switch (aQualification) {
		case working_qualification::none:        return "none";
		case working_qualification::filtered:    return "filtered";
		case working_qualification::too_large:   return "too_large";
		case working_qualification::unsupported: return "unsupported";
		}
		return "unknown";
	}

	auto name(const candidate_qualification aQualification) noexcept -> std::string_view
	{
		switch (aQualification) {
		case candidate_qualification::includes_working_files:  return "includes_working_files";
		case candidate_qualification::sparse_checkout:         return "sparse_checkout";
		case candidate_qualification::working_scan_incomplete: return "working_scan_incomplete";
		case candidate_qualification::line_ending_conversion:  return "line_ending_conversion";
		}
		return "unknown";
	}

	auto name(const draft_candidate_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case draft_candidate_refusal::unknown_working_file:   return "unknown_working_file";
		case draft_candidate_refusal::qualified_working_file: return "qualified_working_file";
		case draft_candidate_refusal::bounds:                 return "bounds";
		}
		return "unknown";
	}

	auto compose_candidate_changes(const std::span<const draft_path_change> aCommitted, const std::span<const git_tree_entry> aBase, const std::span<const working_file> aWorking,
								   const std::span<const std::string> aIncluded) -> std::expected<std::vector<draft_path_change>, draft_candidate_refusal>
	{
		if (aIncluded.size() > maximum_local_draft_working_files) {
			return std::unexpected{ draft_candidate_refusal::bounds };
		}
		auto byPath = std::map<std::string, draft_path_change>{ };
		for (const auto& change : aCommitted) {
			byPath.emplace(change.Path, change);
		}
		for (const auto& path : aIncluded) {
			const auto working = std::ranges::find(aWorking, path, &working_file::Path);
			if (working == aWorking.end()) {
				return std::unexpected{ draft_candidate_refusal::unknown_working_file };
			}
			if (working->Qualification != working_qualification::none || (working->Edit != working_edit::deleted && !working->Object)) {
				return std::unexpected{ draft_candidate_refusal::qualified_working_file };
			}
			// The left side is always the base: a working edit changes only
			// what the candidate proposes.
			const auto base  = std::ranges::find(aBase, path, &git_tree_entry::Path);
			auto       left  = base == aBase.end() ? std::optional<draft_side>{ } : std::optional{ side_of(*base) };
			auto       right = working->Edit == working_edit::deleted ? std::optional<draft_side>{ }
																	  : std::optional{ draft_side{ .Mode = working->Mode, .Object = *working->Object, .Size = working->Size, .Origin = draft_material_origin::working } };
			byPath.erase(path);
			if (const auto change = change_between(left, right)) {
				byPath.emplace(path, draft_path_change{ .Path = path, .Change = *change, .Left = std::move(left), .Right = std::move(right) });
			}
		}
		if (byPath.size() > maximum_local_draft_changes) {
			return std::unexpected{ draft_candidate_refusal::bounds };
		}
		auto changes = std::vector<draft_path_change>{ };
		changes.reserve(byPath.size());
		for (auto& [path, change] : byPath) {
			changes.push_back(std::move(change));
		}
		return changes;
	}

	auto valid(const local_draft& aDraft) -> bool
	{
		const auto validTarget = valid_publication_branch(aDraft.Target.Target.Branch) && !aDraft.Target.Target.Repository.Host.empty()
			&& !aDraft.Target.Target.Repository.Owner.empty() && !aDraft.Target.Target.Repository.Name.empty();
		auto ordinals = std::set<std::uint64_t>{ };
		for (const auto& candidate : aDraft.Candidates) {
			if (!valid_candidate(candidate) || !ordinals.insert(candidate.Ordinal).second) {
				return false;
			}
		}
		auto notes = std::set<std::uint64_t>{ };
		for (const auto& note : aDraft.Notes) {
			if (note.Id == 0U || note.Id >= aDraft.NextNote || !notes.insert(note.Id).second || note.Text.empty() || !plain_text(note.Text, maximum_local_draft_note_bytes, true)
				|| (!note.Path.empty() && !valid_path(note.Path))) {
				return false;
			}
		}
		const auto boundStops = std::ranges::all_of(aDraft.Preparation.Tour.Stops, [&ordinals](const local_tour_stop& aStop) {
			return ordinals.contains(aStop.Source.Candidate)
				&& std::ranges::all_of(aStop.Earlier, [&ordinals](const local_source_binding& aBinding) { return ordinals.contains(aBinding.Candidate); });
		});
		return boundStops && valid(aDraft.Preparation) && (!aDraft.Workflow || valid(*aDraft.Workflow)) && valid(aDraft.Id) && aDraft.Version > 0U && !aDraft.Title.empty() && plain_text(aDraft.Title, maximum_local_draft_title_bytes, false)
			&& !aDraft.Worktree.Location.empty() && valid_publication_branch(aDraft.Branch) && validTarget && aDraft.Chain.size() <= maximum_local_remote_hops
			&& !aDraft.Candidates.empty() && aDraft.Candidates.size() <= maximum_local_draft_candidates && aDraft.Notes.size() <= maximum_local_draft_notes
			&& std::ranges::is_sorted(aDraft.Candidates, { }, &local_draft_candidate::Ordinal);
	}

	auto latest_candidate(const local_draft& aDraft) noexcept -> const local_draft_candidate*
	{
		return aDraft.Candidates.empty() ? nullptr : &aDraft.Candidates.back();
	}

	auto name(const worktree_match aMatch) noexcept -> std::string_view
	{
		switch (aMatch) {
		case worktree_match::same:            return "same";
		case worktree_match::branch_switched: return "branch_switched";
		case worktree_match::moved:           return "moved";
		case worktree_match::other:           return "other";
		}
		return "unknown";
	}

	auto match_worktree(const local_draft& aDraft, const workspace_root_observation& aObserved, const std::string_view aBranch) -> worktree_match
	{
		if (!aObserved.Repository || !aObserved.Worktree || *aObserved.Repository != aDraft.Worktree.Repository || *aObserved.Worktree != aDraft.Worktree.Worktree) {
			return worktree_match::other;
		}
		if (aObserved.Location != aDraft.Worktree.Location) {
			return aBranch == aDraft.Branch ? worktree_match::moved : worktree_match::other;
		}
		return aBranch == aDraft.Branch ? worktree_match::same : worktree_match::branch_switched;
	}

	auto resolve_worktree(const std::span<const local_draft> aDrafts, const workspace_root_observation& aObserved, const std::string_view aBranch) -> worktree_resolution
	{
		auto resolution = worktree_resolution{ };
		for (std::size_t index = 0U; index < aDrafts.size(); ++index) {
			switch (match_worktree(aDrafts[index], aObserved, aBranch)) {
			case worktree_match::same:
				if (!resolution.Resume || aDrafts[*resolution.Resume].Updated < aDrafts[index].Updated) {
					resolution.Resume = index;
				}
				break;
			case worktree_match::moved:
				if (!resolution.Moved) {
					resolution.Moved = index;
				}
				break;
			case worktree_match::branch_switched: resolution.OtherBranches.push_back(index); break;
			case worktree_match::other:           break;
			}
		}
		if (resolution.Resume) {
			resolution.Moved.reset();
		}
		return resolution;
	}

	auto name(const draft_refusal aRefusal) noexcept -> std::string_view
	{
		switch (aRefusal) {
		case draft_refusal::stale:           return "stale";
		case draft_refusal::drafts_full:     return "drafts_full";
		case draft_refusal::candidates_full: return "candidates_full";
		case draft_refusal::notes_full:      return "notes_full";
		case draft_refusal::note_too_long:   return "note_too_long";
		case draft_refusal::title_invalid:   return "title_invalid";
		case draft_refusal::unknown_note:    return "unknown_note";
		case draft_refusal::unchanged:       return "unchanged";
		case draft_refusal::invalid:         return "invalid";
		}
		return "unknown";
	}

	auto default_draft_title(const std::string_view aBranch) -> std::string
	{
		auto       name  = aBranch;
		const auto slash = name.find_last_of('/');
		if (slash != std::string_view::npos && slash + 1U < name.size()) {
			name = name.substr(slash + 1U);
		}
		auto title = std::string{ };
		for (const auto byte : name) {
			title.push_back(byte == '-' || byte == '_' ? ' ' : byte);
		}
		if (!title.empty() && title.front() >= 'a' && title.front() <= 'z') {
			title.front() = static_cast<char>(title.front() - 'a' + 'A');
		}
		if (title.size() > maximum_local_draft_title_bytes || !plain_text(title, maximum_local_draft_title_bytes, false) || title.empty()) {
			return "Local draft";
		}
		return title;
	}

	auto create_local_draft(const std::span<const local_draft> aExisting, local_draft_id aId, local_draft_worktree aWorktree, std::string aBranch, chosen_target aTarget,
							std::vector<local_remote_hop> aChain, local_draft_candidate aFirst, const std::int64_t aNow) -> std::expected<local_draft, draft_refusal>
	{
		if (aExisting.size() >= maximum_local_drafts) {
			return std::unexpected{ draft_refusal::drafts_full };
		}
		if (std::ranges::any_of(aExisting, [&aId](const local_draft& aDraft) { return aDraft.Id == aId; })) {
			return std::unexpected{ draft_refusal::invalid };
		}
		aFirst.Ordinal = 1U;
		auto draft     = local_draft{
			.Id         = std::move(aId),
			.Version    = 1U,
			.Title      = default_draft_title(aBranch),
			.Worktree   = std::move(aWorktree),
			.Branch     = std::move(aBranch),
			.Target     = std::move(aTarget),
			.Chain      = std::move(aChain),
			.Candidates = { std::move(aFirst) },
			.Created    = aNow,
			.Updated    = aNow,
		};
		if (!valid(draft)) {
			return std::unexpected{ draft_refusal::invalid };
		}
		return draft;
	}

	auto add_candidate(const local_draft& aDraft, const std::uint64_t aExpectedVersion, local_draft_candidate aCandidate, const std::int64_t aNow)
		-> std::expected<local_draft, draft_refusal>
	{
		if (stale(aDraft, aExpectedVersion)) {
			return std::unexpected{ draft_refusal::stale };
		}
		if (aDraft.Candidates.size() >= maximum_local_draft_candidates) {
			return std::unexpected{ draft_refusal::candidates_full };
		}
		const auto* latest = latest_candidate(aDraft);
		aCandidate.Ordinal = latest == nullptr ? 1U : latest->Ordinal + 1U;
		if (latest != nullptr && latest->Head == aCandidate.Head && latest->TargetTip == aCandidate.TargetTip && latest->Base == aCandidate.Base
			&& latest->Changes == aCandidate.Changes && latest->Included == aCandidate.Included) {
			return std::unexpected{ draft_refusal::unchanged };
		}
		auto draft = advanced(aDraft, aNow);
		draft.Candidates.push_back(std::move(aCandidate));
		if (!valid(draft)) {
			return std::unexpected{ draft_refusal::invalid };
		}
		return draft;
	}

	auto add_note(const local_draft& aDraft, const std::uint64_t aExpectedVersion, std::string aText, std::string aPath, const note_origin aOrigin, const std::int64_t aNow)
		-> std::expected<local_draft, draft_refusal>
	{
		if (stale(aDraft, aExpectedVersion)) {
			return std::unexpected{ draft_refusal::stale };
		}
		if (aDraft.Notes.size() >= maximum_local_draft_notes) {
			return std::unexpected{ draft_refusal::notes_full };
		}
		if (aText.empty() || !plain_text(aText, maximum_local_draft_note_bytes, true)) {
			return std::unexpected{ draft_refusal::note_too_long };
		}
		if (!aPath.empty() && !valid_path(aPath)) {
			return std::unexpected{ draft_refusal::invalid };
		}
		auto draft = advanced(aDraft, aNow);
		draft.Notes.push_back({ .Id = draft.NextNote, .Text = std::move(aText), .Path = std::move(aPath), .Candidate = latest_candidate(aDraft)->Ordinal, .Origin = aOrigin });
		++draft.NextNote;
		return draft;
	}

	auto remove_note(const local_draft& aDraft, const std::uint64_t aExpectedVersion, const std::uint64_t aNote, const std::int64_t aNow) -> std::expected<local_draft, draft_refusal>
	{
		if (stale(aDraft, aExpectedVersion)) {
			return std::unexpected{ draft_refusal::stale };
		}
		if (std::ranges::find(aDraft.Notes, aNote, &local_draft_note::Id) == aDraft.Notes.end()) {
			return std::unexpected{ draft_refusal::unknown_note };
		}
		auto draft = advanced(aDraft, aNow);
		std::erase_if(draft.Notes, [aNote](const local_draft_note& aEntry) { return aEntry.Id == aNote; });
		return draft;
	}

	auto rename_draft(const local_draft& aDraft, const std::uint64_t aExpectedVersion, std::string aTitle, const std::int64_t aNow) -> std::expected<local_draft, draft_refusal>
	{
		if (stale(aDraft, aExpectedVersion)) {
			return std::unexpected{ draft_refusal::stale };
		}
		if (aTitle.empty() || !plain_text(aTitle, maximum_local_draft_title_bytes, false)) {
			return std::unexpected{ draft_refusal::title_invalid };
		}
		auto draft  = advanced(aDraft, aNow);
		draft.Title = std::move(aTitle);
		return draft;
	}

	auto retarget_draft(const local_draft& aDraft, const std::uint64_t aExpectedVersion, chosen_target aTarget, std::vector<local_remote_hop> aChain, const std::int64_t aNow)
		-> std::expected<local_draft, draft_refusal>
	{
		if (stale(aDraft, aExpectedVersion)) {
			return std::unexpected{ draft_refusal::stale };
		}
		auto draft   = advanced(aDraft, aNow);
		draft.Target = std::move(aTarget);
		draft.Chain  = std::move(aChain);
		if (!valid(draft)) {
			return std::unexpected{ draft_refusal::invalid };
		}
		return draft;
	}

	auto rebind_draft(const local_draft& aDraft, const std::uint64_t aExpectedVersion, std::string aLocation, const std::int64_t aNow) -> std::expected<local_draft, draft_refusal>
	{
		if (stale(aDraft, aExpectedVersion)) {
			return std::unexpected{ draft_refusal::stale };
		}
		if (aLocation.empty()) {
			return std::unexpected{ draft_refusal::invalid };
		}
		auto draft              = advanced(aDraft, aNow);
		draft.Worktree.Location = std::move(aLocation);
		return draft;
	}

	auto associate_draft(const local_draft& aDraft, const std::uint64_t aExpectedVersion, local_draft_association aAssociation, const std::string_view aBaseRef,
						 const std::int64_t aNow) -> std::expected<local_draft, draft_refusal>
	{
		if (stale(aDraft, aExpectedVersion)) {
			return std::unexpected{ draft_refusal::stale };
		}
		const auto target = remote_location{ .Host = aAssociation.Host, .Owner = aAssociation.Owner, .Name = aAssociation.Name };
		if (aAssociation.Number == 0U || aAssociation.HeadRef != aDraft.Branch || aBaseRef != aDraft.Target.Target.Branch
			|| !same_repository(target, aDraft.Target.Target.Repository)) {
			return std::unexpected{ draft_refusal::invalid };
		}
		auto draft        = advanced(aDraft, aNow);
		draft.Association = std::move(aAssociation);
		return draft;
	}

	auto drafts_in_preparation(const std::span<const local_draft> aDrafts) -> std::vector<std::size_t>
	{
		auto indexes = std::vector<std::size_t>{ };
		for (std::size_t index = 0U; index < aDrafts.size(); ++index) {
			if (!aDrafts[index].Association) {
				indexes.push_back(index);
			}
		}
		std::ranges::stable_sort(indexes, [&aDrafts](const std::size_t aLeft, const std::size_t aRight) {
			return std::tie(aDrafts[aRight].Updated, aDrafts[aLeft].Id) < std::tie(aDrafts[aLeft].Updated, aDrafts[aRight].Id);
		});
		return indexes;
	}

	auto draft_for_pull_request(const std::span<const local_draft> aDrafts, const std::string_view aHost, const std::string_view aOwner, const std::string_view aName,
								const std::uint64_t aNumber) -> std::optional<std::size_t>
	{
		const auto wanted = remote_location{ .Host = std::string{ aHost }, .Owner = std::string{ aOwner }, .Name = std::string{ aName } };
		for (std::size_t index = 0U; index < aDrafts.size(); ++index) {
			const auto& association = aDrafts[index].Association;
			if (association && association->Number == aNumber
				&& same_repository(remote_location{ .Host = association->Host, .Owner = association->Owner, .Name = association->Name }, wanted)) {
				return index;
			}
		}
		return std::nullopt;
	}

	auto local_draft_file_comparison(const local_draft_id& aDraft, const std::uint64_t aCandidate, const std::string_view aPath, const std::optional<git_entry_oid>& aLeft,
									 const std::optional<git_entry_oid>& aRight, const diff_options& aOptions) -> std::optional<material_comparison_key>
	{
		const auto options = options_digest(aOptions);
		if ((!aLeft && !aRight) || !options || !valid(aDraft)) {
			return std::nullopt;
		}
		constexpr auto domain = std::string_view{ "BPR-local-draft-file-comparison-v1" };
		auto           bytes  = std::vector<std::uint8_t>(domain.begin(), domain.end());
		const auto     append = [&bytes](const std::string_view aText) {
			for (std::size_t shift = 0U; shift < 64U; shift += 8U) {
				bytes.push_back(static_cast<std::uint8_t>((static_cast<std::uint64_t>(aText.size()) >> shift) & 0xFFU));
			}
			bytes.insert(bytes.end(), aText.begin(), aText.end());
		};
		bytes.push_back(0U);
		append(aDraft.Value);
		append(std::to_string(aCandidate));
		append(aPath);
		append(aLeft ? canonical_hex(*aLeft).view() : std::string_view{ });
		append(aRight ? canonical_hex(*aRight).view() : std::string_view{ });
		bytes.insert(bytes.end(), options->bytes().begin(), options->bytes().end());
		append(std::to_string(supported_diff_algorithm_revision.value()));
		return material_comparison_key{
			.Comparison   = file_comparison_digest{ sha256(bytes) },
			.LeftPresent  = aLeft.has_value(),
			.RightPresent = aRight.has_value(),
			.Options      = *options,
			.Algorithm    = supported_diff_algorithm_revision,
		};
	}

	auto encode_local_draft(const local_draft& aDraft) -> std::string
	{
		const auto identity = [](const workspace_object_identity& aIdentity) {
			return detail::wire_identity{ .Volume = aIdentity.Volume, .ObjectHigh = aIdentity.ObjectHigh, .ObjectLow = aIdentity.ObjectLow };
		};
		const auto side = [](const std::optional<draft_side>& aSide) -> std::optional<detail::wire_side> {
			if (!aSide) {
				return std::nullopt;
			}
			return detail::wire_side{
				.Mode = aSide->Mode, .Object = std::string{ canonical_hex(aSide->Object).view() }, .Size = aSide->Size, .Working = aSide->Origin == draft_material_origin::working
			};
		};
		auto wire = detail::wire_draft{
			.Format       = std::string{ draft_format },
			.Version      = draft_format_version,
			.Id           = aDraft.Id.Value,
			.Revision     = aDraft.Version,
			.Title        = aDraft.Title,
			.Location     = aDraft.Worktree.Location,
			.Repository   = identity(aDraft.Worktree.Repository),
			.Worktree     = identity(aDraft.Worktree.Worktree),
			.Branch       = aDraft.Branch,
			.TargetRemote = aDraft.Target.Target.Remote,
			.TargetHost   = aDraft.Target.Target.Repository.Host,
			.TargetOwner  = aDraft.Target.Target.Repository.Owner,
			.TargetName   = aDraft.Target.Target.Repository.Name,
			.TargetBranch = aDraft.Target.Target.Branch,
			.TargetOrigin = std::string{ name(aDraft.Target.Origin) },
			.NextNote     = aDraft.NextNote,
			.Created      = aDraft.Created,
			.Updated      = aDraft.Updated,
		};
		for (const auto& hop : aDraft.Chain) {
			wire.Chain.push_back({ .Remote = hop.Remote, .Location = hop.Location });
		}
		for (const auto& candidate : aDraft.Candidates) {
			auto entry = detail::wire_candidate{
				.Ordinal    = candidate.Ordinal,
				.Head       = std::string{ canonical_hex(candidate.Head).view() },
				.TargetTip  = std::string{ canonical_hex(candidate.TargetTip).view() },
				.Base       = std::string{ canonical_hex(candidate.Base).view() },
				.Included   = candidate.Included,
				.CapturedAt = candidate.CapturedAt,
			};
			for (const auto& change : candidate.Changes) {
				entry.Changes.push_back({ .Path = change.Path, .Change = std::string{ name(change.Change) }, .Left = side(change.Left), .Right = side(change.Right) });
			}
			for (const auto qualification : candidate.Qualifications) {
				entry.Qualifications.emplace_back(name(qualification));
			}
			wire.Candidates.push_back(std::move(entry));
		}
		for (const auto& note : aDraft.Notes) {
			wire.Notes.push_back({ .Id = note.Id, .Text = note.Text, .Path = note.Path, .Candidate = note.Candidate, .Origin = std::string{ note_origin_name(note.Origin) } });
		}
		if (aDraft.Association) {
			const auto& association = *aDraft.Association;
			wire.Association        = detail::wire_association{
				.Host = association.Host, .Owner = association.Owner, .Name = association.Name, .Number = association.Number, .HeadRef = association.HeadRef
			};
		}
		if (aDraft.Preparation != local_draft_preparation{ }) {
			auto preparation = encode_draft_preparation(aDraft.Preparation);
			if (preparation.empty()) {
				return { };
			}
			wire.Preparation = glz::raw_json{ std::move(preparation) };
		}
		if (aDraft.Publication) {
			auto publication = encode_local_draft_publication(*aDraft.Publication);
			if (publication.empty()) {
				return { };
			}
			wire.Publication = glz::raw_json{ std::move(publication) };
		}
		if (aDraft.Workflow) {
			wire.Workflow = glz::raw_json{ encode_draft_workflow(*aDraft.Workflow) };
		}
		auto text = std::string{ };
		if (glz::write_json(wire, text)) {
			return { };
		}
		return text;
	}

	auto decode_local_draft(const std::string_view aText) -> std::optional<local_draft>
	{
		auto wire = detail::wire_draft{ };
		if (glz::read<read_options>(wire, aText) || wire.Format != draft_format || wire.Version != draft_format_version) {
			return std::nullopt;
		}
		const auto identity = [](const detail::wire_identity& aIdentity) {
			return workspace_object_identity{ .Volume = aIdentity.Volume, .ObjectHigh = aIdentity.ObjectHigh, .ObjectLow = aIdentity.ObjectLow };
		};
		auto       failed = false;
		const auto side   = [&failed](const std::optional<detail::wire_side>& aSide) -> std::optional<draft_side> {
			if (!aSide) {
				return std::nullopt;
			}
			const auto object = parse_git_entry_oid(aSide->Object);
			if (!object) {
				failed = true;
				return std::nullopt;
			}
			return draft_side{ .Mode = aSide->Mode, .Object = *object, .Size = aSide->Size, .Origin = aSide->Working ? draft_material_origin::working : draft_material_origin::committed };
		};
		const auto origin = enum_from(wire.TargetOrigin, all_origins);
		if (!origin) {
			return std::nullopt;
		}
		auto draft = local_draft{
			.Id       = { wire.Id },
			.Version  = wire.Revision,
			.Title    = wire.Title,
			.Worktree = { .Location = wire.Location, .Repository = identity(wire.Repository), .Worktree = identity(wire.Worktree) },
			.Branch   = wire.Branch,
			.Target   = { .Target = { .Remote = wire.TargetRemote, .Repository = { .Host = wire.TargetHost, .Owner = wire.TargetOwner, .Name = wire.TargetName }, .Branch = wire.TargetBranch },
						  .Origin = *origin },
			.NextNote = wire.NextNote,
			.Created  = wire.Created,
			.Updated  = wire.Updated,
		};
		for (const auto& hop : wire.Chain) {
			draft.Chain.push_back({ .Remote = hop.Remote, .Location = hop.Location });
		}
		for (const auto& entry : wire.Candidates) {
			const auto head = parse_git_commit_oid(entry.Head);
			const auto tip  = parse_git_commit_oid(entry.TargetTip);
			const auto base = parse_git_commit_oid(entry.Base);
			if (!head || !tip || !base) {
				return std::nullopt;
			}
			auto candidate = local_draft_candidate{ .Ordinal = entry.Ordinal, .Head = *head, .TargetTip = *tip, .Base = *base, .Included = entry.Included, .CapturedAt = entry.CapturedAt };
			for (const auto& change : entry.Changes) {
				const auto kind = enum_from(change.Change, all_changes);
				if (!kind) {
					return std::nullopt;
				}
				candidate.Changes.push_back({ .Path = change.Path, .Change = *kind, .Left = side(change.Left), .Right = side(change.Right) });
			}
			for (const auto& qualification : entry.Qualifications) {
				const auto value = enum_from(qualification, all_qualifications);
				if (!value) {
					return std::nullopt;
				}
				candidate.Qualifications.push_back(*value);
			}
			draft.Candidates.push_back(std::move(candidate));
		}
		for (const auto& note : wire.Notes) {
			if (note.Origin != "author" && note.Origin != "harness") {
				return std::nullopt;
			}
			draft.Notes.push_back(
				{ .Id = note.Id, .Text = note.Text, .Path = note.Path, .Candidate = note.Candidate, .Origin = note.Origin == "harness" ? note_origin::harness : note_origin::author });
		}
		if (wire.Association) {
			const auto& association = *wire.Association;
			draft.Association       = local_draft_association{
				.Host = association.Host, .Owner = association.Owner, .Name = association.Name, .Number = association.Number, .HeadRef = association.HeadRef
			};
		}
		if (wire.Preparation) {
			auto preparation = decode_draft_preparation(wire.Preparation->str);
			if (!preparation) {
				return std::nullopt;
			}
			draft.Preparation = std::move(*preparation);
		}
		if (wire.Publication) {
			auto publication = decode_local_draft_publication(wire.Publication->str);
			if (!publication || publication->Plan.DraftId != draft.Id.Value) {
				return std::nullopt;
			}
			draft.Publication = std::move(*publication);
		}
		if (wire.Workflow) {
			auto workflow = decode_draft_workflow(wire.Workflow->str);
			if (!workflow) {
				return std::nullopt;
			}
			draft.Workflow = std::move(*workflow);
		}
		if (failed || !valid(draft)) {
			return std::nullopt;
		}
		return draft;
	}
}
