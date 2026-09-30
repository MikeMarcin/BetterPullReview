#include "bpr/core/review_cycle_candidate.hpp"

#include "bpr/core/review_cycle_workspace.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <utility>

namespace bpr::core {
	namespace {
		[[nodiscard]] auto refuse(const candidate_limit aLimit, std::vector<std::string> aPaths = { }) -> std::unexpected<candidate_refusal>
		{
			return std::unexpected{ candidate_refusal{ .Limit = aLimit, .Paths = std::move(aPaths) } };
		}

		/// Rows whose gap blocks the native build or needs a decision, by limit.
		[[nodiscard]] auto blocking(const outgoing_plan& aPlan, const bool aExternal) -> std::optional<candidate_refusal>
		{
			const auto collect = [&aPlan](auto aSelect) {
				auto paths = std::vector<std::string>{ };
				for (const auto& row : aPlan.Rows) {
					if (aSelect(row)) {
						paths.push_back(row.Path);
					}
				}
				return paths;
			};
			const auto selected = [](const outgoing_gap aGap) { return [aGap](const outgoing_row& aRow) { return aRow.Selected && aRow.Gap == aGap; }; };
			if (auto paths = collect(selected(outgoing_gap::path_conflict)); !paths.empty()) {
				return candidate_refusal{ .Limit = candidate_limit::path_conflict, .Paths = std::move(paths) };
			}
			if (auto paths = collect(selected(outgoing_gap::gitlink)); !paths.empty()) {
				return candidate_refusal{ .Limit = candidate_limit::gitlink, .Paths = std::move(paths) };
			}
			if (auto paths = collect([](const outgoing_row& aRow) { return aRow.Gap == outgoing_gap::depends_on_starting_work && !aRow.Choice; }); !paths.empty()) {
				return candidate_refusal{ .Limit = candidate_limit::undecided, .Paths = std::move(paths) };
			}
			if (!aExternal) {
				if (auto paths = collect(selected(outgoing_gap::filter_managed)); !paths.empty()) {
					return candidate_refusal{ .Limit = candidate_limit::filter_managed, .Paths = std::move(paths) };
				}
			}
			if (aPlan.NoChange) {
				return candidate_refusal{ .Limit = candidate_limit::no_change };
			}
			return std::nullopt;
		}

		[[nodiscard]] auto changes(const outgoing_plan& aPlan, std::vector<candidate_blob>& aBlobs, std::vector<std::string>& aDeleted) -> bool
		{
			for (const auto& row : aPlan.Rows) {
				if (!row.Selected) {
					continue;
				}
				if (row.Final.State == outgoing_state::present) {
					if (!row.Final.Object || !row.Final.Content) {
						return false;
					}
					aBlobs.push_back({ .Path = row.Path, .Mode = row.Final.Mode, .Object = *row.Final.Object, .Content = *row.Final.Content, .Size = row.Final.Size });
				} else if (row.Base.State == outgoing_state::present) {
					aDeleted.push_back(row.Path);
				}
			}
			return true;
		}
	}

	auto name(const candidate_limit aLimit) noexcept -> std::string_view
	{
		switch (aLimit) {
		case candidate_limit::no_change:
			return "no_change";
		case candidate_limit::undecided:
			return "undecided";
		case candidate_limit::filter_managed:
			return "filter_managed";
		case candidate_limit::gitlink:
			return "gitlink";
		case candidate_limit::path_conflict:
			return "path_conflict";
		case candidate_limit::signing_required:
			return "signing_required";
		case candidate_limit::normalization:
			return "normalization";
		case candidate_limit::metadata:
			return "metadata";
		}
		return "unknown";
	}

	auto candidate_message(const std::string_view aTitle, const std::string_view aBody) -> std::optional<std::string>
	{
		if (aTitle.empty() || aTitle.size() > maximum_candidate_title_bytes || aTitle.find_first_of("\r\n") != std::string_view::npos || aTitle.front() == ' '
			|| aTitle.back() == ' ') {
			return std::nullopt;
		}
		auto message = std::string{ aTitle };
		message += '\n';
		auto body = aBody;
		while (!body.empty() && body.back() == '\n') {
			body.remove_suffix(1U);
		}
		if (!body.empty()) {
			message += '\n';
			message += body;
			message += '\n';
		}
		return message;
	}

	auto prepare_candidate(const outgoing_plan& aPlan, const std::span<const outgoing_identity> aIdentities, const candidate_request& aRequest)
		-> std::expected<publication_candidate, candidate_refusal>
	{
		if (const auto blocked = blocking(aPlan, false)) {
			return std::unexpected{ *blocked };
		}
		if (aRequest.Requirements.SigningRequired) {
			return refuse(candidate_limit::signing_required);
		}
		auto candidate = publication_candidate{ .Format = aPlan.BaseCommit.algorithm(), .Parent = aPlan.BaseCommit, .Entries = aPlan.Candidate };
		if (!changes(aPlan, candidate.Blobs, candidate.Deleted)) {
			return refuse(candidate_limit::no_change);
		}

		// Normalization: named, selected text with CRLF endings, once each.
		auto named = std::set<std::string>{ };
		for (const auto& path : aRequest.NormalizeLineEndings) {
			const auto blob = std::ranges::find(candidate.Blobs, path, &candidate_blob::Path);
			if (!named.insert(path).second || blob == candidate.Blobs.end()) {
				return refuse(candidate_limit::normalization, { path });
			}
			const auto identity = std::ranges::find(aIdentities, blob->Content, &outgoing_identity::Content);
			if (identity == aIdentities.end() || !identity->LineFeed || identity->Raw != blob->Object) {
				return refuse(candidate_limit::normalization, { path });
			}
			candidate.Transformations.push_back({ .Path = path, .Before = blob->Content, .BeforeObject = blob->Object, .After = *identity->LineFeed });
			blob->Object     = *identity->LineFeed;
			blob->LineFeed   = true;
			const auto entry = std::ranges::lower_bound(candidate.Entries, path, { }, &git_tree_entry::Path);
			if (entry == candidate.Entries.end() || entry->Path != path) {
				return refuse(candidate_limit::normalization, { path });
			}
			entry->Object = *identity->LineFeed;
		}
		std::ranges::sort(candidate.Transformations, { }, &candidate_transformation::Path);

		const auto tree = build_git_trees(candidate.Entries, candidate.Format);
		if (!tree) {
			return refuse(candidate_limit::path_conflict);
		}
		candidate.Tree = tree->Root;
		if (candidate.Tree == aPlan.BaseTree) {
			return refuse(candidate_limit::no_change);
		}
		const auto message = candidate_message(aRequest.Metadata.Title, aRequest.Metadata.Body);
		if (!message) {
			return refuse(candidate_limit::metadata);
		}
		candidate.Content = {
			.Tree      = candidate.Tree,
			.Parents   = { aPlan.BaseCommit },
			.Author    = aRequest.Metadata.Author,
			.Committer = aRequest.Metadata.Committer,
			.Message   = *message,
		};
		const auto commit = git_commit_identity(candidate.Content);
		if (!commit) {
			return refuse(candidate_limit::metadata);
		}
		candidate.Commit = *commit;
		return candidate;
	}

	auto render_candidate_preview(const publication_candidate& aCandidate) -> std::string
	{
		const auto signature = [](const git_signature& aSignature) {
			return aSignature.Name + " <" + aSignature.Email + "> at " + std::to_string(aSignature.Seconds) + (aSignature.OffsetMinutes < 0 ? " UTC-" : " UTC+")
				+ std::to_string((aSignature.OffsetMinutes < 0 ? -aSignature.OffsetMinutes : aSignature.OffsetMinutes) / 60) + ":"
				+ std::to_string((aSignature.OffsetMinutes < 0 ? -aSignature.OffsetMinutes : aSignature.OffsetMinutes) % 60 / 10)
				+ std::to_string((aSignature.OffsetMinutes < 0 ? -aSignature.OffsetMinutes : aSignature.OffsetMinutes) % 10);
		};
		auto out = std::string{ "\nCandidate commit " };
		out += canonical_hex(aCandidate.Commit).view();
		out += ": one parent (P ";
		out += canonical_hex(aCandidate.Parent).view();
		out += "), tree ";
		out += canonical_hex(aCandidate.Tree).view();
		out += ", unsigned.\n";
		out += "Author: " + signature(aCandidate.Content.Author) + "\n";
		out += "Committer: " + signature(aCandidate.Content.Committer) + "\n";
		out += "Message:\n";
		auto message = std::string_view{ aCandidate.Content.Message };
		while (!message.empty()) {
			const auto end = message.find('\n');
			out += "  ";
			out += message.substr(0U, end);
			out += '\n';
			message.remove_prefix(end == std::string_view::npos ? message.size() : end + 1U);
		}
		if (aCandidate.Transformations.empty()) {
			out += "Transformations: none; every byte is as captured.\n";
		} else {
			out += "Transformations:\n";
			for (const auto& transformation : aCandidate.Transformations) {
				out += "  CRLF -> LF  " + workspace_path_label(transformation.Path) + ": blob " + std::string{ canonical_hex(transformation.BeforeObject).view() } + " -> "
					+ std::string{ canonical_hex(transformation.After).view() } + "\n";
			}
		}
		return out;
	}

	auto prepare_external(const outgoing_plan& aPlan, const candidate_requirements& aRequirements) -> std::expected<external_preparation, candidate_refusal>
	{
		if (const auto blocked = blocking(aPlan, true)) {
			return std::unexpected{ *blocked };
		}
		auto preparation = external_preparation{ .Parent = aPlan.BaseCommit, .Entries = aPlan.Candidate, .SigningRequired = aRequirements.SigningRequired };
		if (!changes(aPlan, preparation.Blobs, preparation.Deleted)) {
			return refuse(candidate_limit::no_change);
		}
		for (const auto& row : aPlan.Rows) {
			if (row.Selected && row.Gap == outgoing_gap::filter_managed) {
				preparation.ToolingPaths.push_back(row.Path);
			}
		}
		const auto tree = build_git_trees(preparation.Entries, aPlan.BaseCommit.algorithm());
		if (!tree) {
			return refuse(candidate_limit::path_conflict);
		}
		preparation.Tree = tree->Root;
		auto text        = std::string{ "External preparation: this is not native publication, and nothing has been published.\n" };
		text += "The saved patch holds the selected captured bytes against P " + std::string{ canonical_hex(aPlan.BaseCommit).view() } + ".\n";
		text += "Check out P in a workspace of your choosing, apply the patch with `git apply --binary`, then stage, review, commit and push with your own tools.\n";
		text += "Your filters, hooks, signing and repository policy run there under your control; BPR ran none of them.\n";
		if (!preparation.ToolingPaths.empty()) {
			text += "These paths hold the file bytes, not the object a Git filter such as LFS makes; stage them through your filter:\n";
			for (const auto& path : preparation.ToolingPaths) {
				text += "  " + workspace_path_label(path) + '\n';
			}
		}
		if (preparation.SigningRequired) {
			text += "The destination requires signed commits: sign the commit with your own signing configuration.\n";
		}
		text += "When a pull request exists, link it back to this cycle; it is recorded as externally published.\n";
		preparation.Instructions = std::move(text);
		return preparation;
	}
}
