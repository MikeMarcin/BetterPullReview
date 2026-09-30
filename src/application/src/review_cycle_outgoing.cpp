#include "bpr/application/review_cycle_outgoing.hpp"

#include "bpr/application/review_cycle_comparison.hpp"
#include "bpr/core/review_cycle_git.hpp"

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <span>
#include <string_view>
#include <utility>
#include <variant>

namespace bpr::application {
	namespace {
		constexpr std::uint64_t maximum_reference_file_bytes = 64U * 1024U;

		[[nodiscard]] auto text(const std::vector<std::uint8_t>& aBytes) -> std::string_view
		{
			return { reinterpret_cast<const char*>(aBytes.data()), aBytes.size() };
		}

		/// A reference name this reads: under `refs/`, with no empty, `.`-led
		/// or `..` segment, no control character, space or `~^:?*[\` and no
		/// `.lock` ending.
		[[nodiscard]] auto admissible_reference(const std::string_view aName) -> bool
		{
			if (!aName.starts_with("refs/") || aName.size() > core::maximum_outgoing_reference_bytes || aName.ends_with('/') || aName.ends_with(".lock")
				|| aName.find("..") != std::string_view::npos || aName.find("@{") != std::string_view::npos) {
				return false;
			}
			for (const auto character : aName) {
				const auto value = static_cast<unsigned char>(character);
				if (value < 0x21U || value == 0x7FU || std::string_view{ "~^:?*[\\" }.find(character) != std::string_view::npos) {
					return false;
				}
			}
			auto start = std::size_t{ 0 };
			while (start < aName.size()) {
				const auto end     = aName.find('/', start);
				const auto segment = aName.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);
				if (segment.empty() || segment.front() == '.') {
					return false;
				}
				if (end == std::string_view::npos) {
					break;
				}
				start = end + 1U;
			}
			return true;
		}

		/// One reference: its loose file, else `packed-refs`. A loose symbolic
		/// reference is followed once.
		auto resolve_named(workspace_root_session& aSession, const std::string_view aName, const core::git_hash_algorithm aAlgorithm, const bool aFollow, const std::stop_token& aStop)
			-> std::expected<core::git_commit_oid, std::optional<workspace_failure>>
		{
			if (!admissible_reference(aName)) {
				return std::unexpected(std::nullopt);
			}
			const auto loose = aSession.read_git(workspace_git_area::common, aName, maximum_reference_file_bytes, aStop);
			if (loose) {
				const auto contents = text(*loose);
				if (contents.starts_with("ref: ")) {
					auto target = contents.substr(5U);
					while (!target.empty() && (target.back() == '\n' || target.back() == '\r')) {
						target.remove_suffix(1U);
					}
					if (!aFollow) {
						return std::unexpected(std::nullopt);
					}
					return resolve_named(aSession, target, aAlgorithm, false, aStop);
				}
				if (const auto commit = core::parse_git_reference(contents, aAlgorithm)) {
					return *commit;
				}
				return std::unexpected(std::nullopt);
			}
			if (loose.error() != workspace_failure::missing) {
				return std::unexpected(loose.error());
			}
			const auto packed = aSession.read_git(workspace_git_area::common, "packed-refs", maximum_workspace_metadata_bytes, aStop);
			if (!packed) {
				return std::unexpected(packed.error() == workspace_failure::missing ? std::nullopt : std::optional{ packed.error() });
			}
			if (const auto commit = core::find_packed_reference(text(*packed), aName, aAlgorithm)) {
				return *commit;
			}
			return std::unexpected(std::nullopt);
		}

		void target_paths(const core::cycle_feedback_target& aTarget, std::vector<std::string>& aPaths)
		{
			std::visit(
				[&aPaths](const auto& aValue) {
					if constexpr (!std::is_same_v<std::decay_t<decltype(aValue)>, core::cycle_whole_target>) {
						aPaths.push_back(aValue.Path);
					}
				},
				aTarget);
		}

		auto failed(const cycle_outgoing_status aStatus) -> cycle_outgoing_outcome
		{
			return { .Status = aStatus };
		}

		auto material_failure(const review_material_status aStatus) -> cycle_outgoing_outcome
		{
			if (aStatus == review_material_status::cancelled) {
				return failed(cycle_outgoing_status::cancelled);
			}
			return { .Status = cycle_outgoing_status::material, .Material = aStatus };
		}

		auto git_failure(publication_git_error aError) -> cycle_outgoing_outcome
		{
			if (aError.Failure == publication_git_failure::cancelled) {
				return failed(cycle_outgoing_status::cancelled);
			}
			return { .Status = cycle_outgoing_status::repository, .Git = std::move(aError) };
		}
	}

	auto resolve_reference(workspace_root_session& aSession, const std::string_view aName, const core::git_hash_algorithm aAlgorithm, std::stop_token aStop)
		-> std::expected<core::git_commit_oid, std::optional<workspace_failure>>
	{
		if (const auto exact = core::parse_git_commit_oid(aName)) {
			if (exact->algorithm() != aAlgorithm) {
				return std::unexpected(std::nullopt);
			}
			return *exact;
		}
		if (aName == "HEAD") {
			const auto head = aSession.read_git(workspace_git_area::worktree, "HEAD", maximum_reference_file_bytes, aStop);
			if (!head) {
				return std::unexpected(head.error() == workspace_failure::missing ? std::nullopt : std::optional{ head.error() });
			}
			const auto parsed = core::parse_git_head(text(*head), aAlgorithm);
			if (parsed.Commit) {
				return *parsed.Commit;
			}
			if (parsed.Reference) {
				return resolve_named(aSession, *parsed.Reference, aAlgorithm, false, aStop);
			}
			return std::unexpected(std::nullopt);
		}
		return resolve_named(aSession, aName, aAlgorithm, true, aStop);
	}

	review_cycle_outgoing_host::review_cycle_outgoing_host(workspace_source_port& aSource, review_cycle_store& aStore, publication_candidate_port& aGit) noexcept
		: Source{ aSource }
		, Store{ aStore }
		, Git{ aGit }
	{
	}

	auto review_cycle_outgoing_host::plan(const core::review_cycle_state& aCycle, const cycle_outgoing_request& aRequest, std::stop_token aStop) -> cycle_outgoing_outcome
	{
		if (!aCycle.Workspace) {
			return failed(cycle_outgoing_status::not_bound);
		}
		const auto kind = aCycle.Workspace->Root.Kind;
		if (kind != core::workspace_root_kind::git_checkout && kind != core::workspace_root_kind::git_linked_worktree) {
			return { .Status = cycle_outgoing_status::refused, .Refusal = core::outgoing_refusal::repository_required };
		}
		if (!aCycle.Initial || !aCycle.Latest) {
			return failed(cycle_outgoing_status::not_captured);
		}
		const auto startId = aRequest.Start.value_or(aCycle.Initial->Id.Id);
		const auto finalId = aRequest.Final.value_or(aCycle.Latest->Id.Id);
		auto       views   = review_cycle_comparison_host{ Store, aCycle.Subject.Authority };
		const auto start   = views.manifest(aCycle.Subject, startId, aStop);
		if (!start) {
			return start.error() == review_material_status::missing ? failed(cycle_outgoing_status::not_captured) : material_failure(start.error());
		}
		const auto final = views.manifest(aCycle.Subject, finalId, aStop);
		if (!final) {
			return final.error() == review_material_status::missing ? failed(cycle_outgoing_status::not_captured) : material_failure(final.error());
		}
		const auto algorithm = (*final)->ObjectFormat;

		// P and T are read from the bound checkout, which must still be the
		// root the cycle was bound to.
		auto session = Source.open(aCycle.Workspace->Root.Location, aStop);
		if (!session) {
			return session.error() == workspace_failure::cancelled ? failed(cycle_outgoing_status::cancelled)
																   : cycle_outgoing_outcome{ .Status = cycle_outgoing_status::workspace, .Workspace = session.error() };
		}
		if (core::verify_workspace(*aCycle.Workspace, (*session)->observation()) != core::workspace_verification::matches) {
			return failed(cycle_outgoing_status::root_changed);
		}
		const auto resolve = [&](const std::string& aName) -> std::expected<core::git_commit_oid, cycle_outgoing_outcome> {
			auto resolved = resolve_reference(**session, aName, algorithm, aStop);
			if (resolved) {
				return *resolved;
			}
			if (resolved.error()) {
				return std::unexpected(*resolved.error() == workspace_failure::cancelled ? failed(cycle_outgoing_status::cancelled)
																						 : cycle_outgoing_outcome{ .Status = cycle_outgoing_status::workspace, .Workspace = resolved.error() });
			}
			return std::unexpected(failed(cycle_outgoing_status::reference_unavailable));
		};
		const auto repository = publication_repository{ .Checkout = aCycle.Workspace->Root.Location, .Algorithm = algorithm };
		const auto list       = [&](const core::git_commit_oid& aCommit) -> std::expected<publication_commit_listing, cycle_outgoing_outcome> {
			auto listing = Git.list_commit(repository, aCommit, aStop);
			if (!listing) {
				return std::unexpected(git_failure(std::move(listing.error())));
			}
			// The listing is admitted only for the tree the commit's own bytes name.
			if (core::verified_commit_tree(listing->CommitObject, aCommit) != listing->Tree) {
				return std::unexpected(git_failure({ .Failure = publication_git_failure::malformed }));
			}
			return std::move(*listing);
		};

		const auto baseCommit = resolve(aRequest.Base);
		if (!baseCommit) {
			return baseCommit.error();
		}
		auto baseListing = list(*baseCommit);
		if (!baseListing) {
			return baseListing.error();
		}
		auto request = core::outgoing_request{
			.Mode          = aRequest.Mode,
			.StartRevision = startId,
			.FinalRevision = finalId,
			.Start         = **start,
			.Final         = **final,
			.Base          = { .Commit    = *baseCommit,
							   .Tree      = baseListing->Tree,
							   .Entries   = std::move(baseListing->Entries),
							   .Reference = core::parse_git_commit_oid(aRequest.Base) ? std::string{ } : aRequest.Base },
			.Overrides     = aRequest.Overrides,
		};
		if (aRequest.Target) {
			const auto head = resolve(*aRequest.Target);
			if (!head) {
				return head.error();
			}
			request.Target = core::publication_target{ .Reference = *aRequest.Target, .Source = core::publication_target_source::local_reference, .Head = *head };
			if (*head != *baseCommit) {
				auto targetListing = list(*head);
				if (!targetListing) {
					return targetListing.error();
				}
				request.Target->Entries = std::move(targetListing->Entries);
			}
		}

		// Every captured object's Git identity, from retained bytes only.
		auto bridge = review_material_bridge{ nullptr, &Store, aCycle.Subject.Authority };
		auto seen   = std::map<core::cycle_material_digest, bool>{ };
		for (const auto& [revision, manifest] : { std::pair{ startId, *start }, std::pair{ finalId, *final } }) {
			for (const auto& entry : manifest->Entries) {
				if (!entry.Content || entry.Availability != core::manifest_availability::captured || !seen.emplace(*entry.Content, true).second) {
					continue;
				}
				const auto bytes = bridge.materialize(cycle_material_request{ .Subject = aCycle.Subject, .Revision = revision, .Object = { .Digest = *entry.Content, .Size = entry.Size }, .MaximumBytes = maximum_cycle_object_bytes },
													  aStop);
				if (bytes.Status != review_material_status::available) {
					return material_failure(bytes.Status);
				}
				auto identity = core::outgoing_identity{ .Content = *entry.Content, .Raw = core::retag<core::git_entry_oid_kind>(core::git_blob_identity(bytes.Bytes, algorithm)) };
				if (entry.Profile.text() && (entry.Profile.LineEndings == core::material_line_endings::crlf || entry.Profile.LineEndings == core::material_line_endings::mixed)) {
					if (const auto normalized = core::line_feed_form(bytes.Bytes)) {
						identity.LineFeed = core::retag<core::git_entry_oid_kind>(core::git_blob_identity(*normalized, algorithm));
					}
				}
				request.Identities.push_back(identity);
			}
		}

		// Paths and folders the notes name make starting work there relevant.
		auto after = std::optional<core::cycle_feedback_item_id>{ };
		do {
			auto page = Store.feedback(aCycle.Subject, after, aStop);
			if (!page) {
				return page.error().Failure == cycle_store_failure::cancelled ? failed(cycle_outgoing_status::cancelled) : material_failure(review_material_status::failure);
			}
			for (const auto& thread : page->Threads) {
				target_paths(thread.Primary, request.ReviewedPaths);
				for (const auto& related : thread.Related) {
					target_paths(related.Target, request.ReviewedPaths);
				}
			}
			after = page->Next;
		} while (after);
		std::ranges::sort(request.ReviewedPaths);
		request.ReviewedPaths.erase(std::ranges::unique(request.ReviewedPaths).begin(), request.ReviewedPaths.end());
		if (aStop.stop_requested()) {
			return failed(cycle_outgoing_status::cancelled);
		}

		auto planned = core::plan_outgoing(request);
		if (!planned) {
			return { .Status = cycle_outgoing_status::refused, .Refusal = planned.error() };
		}
		auto preview = core::render_outgoing_preview(*planned);
		return { .Status = cycle_outgoing_status::planned, .Plan = std::move(*planned), .Preview = std::move(preview) };
	}
}
