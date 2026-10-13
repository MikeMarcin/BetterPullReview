#include "bpr/application/review_cycle_outgoing.hpp"

#include "bpr/application/review_cycle_comparison.hpp"
#include "bpr/application/review_cycle_git_metadata.hpp"

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

		/// [[BPR-1450]]. SHA-1 collision detection refused an object the plan
		/// needs an identity for; nothing is substituted.
		auto collision_refusal() -> cycle_outgoing_outcome
		{
			return { .Status = cycle_outgoing_status::refused, .Refusal = core::outgoing_refusal::collision_detected };
		}

		auto git_failure(publication_git_error aError) -> cycle_outgoing_outcome
		{
			if (aError.Failure == publication_git_failure::cancelled) {
				return failed(cycle_outgoing_status::cancelled);
			}
			return { .Status = cycle_outgoing_status::repository, .Git = std::move(aError) };
		}
	}

	review_cycle_outgoing_host::review_cycle_outgoing_host(workspace_source_port& aSource, review_cycle_store& aStore, publication_candidate_port& aGit) noexcept
		: Source{ aSource }
		, Store{ aStore }
		, Git{ aGit }
	{
	}

	auto review_cycle_outgoing_host::assemble(const core::review_cycle_state& aCycle, const cycle_outgoing_request& aRequest, const std::stop_token& aStop)
		-> std::expected<assembled_plan, cycle_outgoing_outcome>
	{
		const auto stopWith     = [](const cycle_outgoing_status aStatus) { return std::unexpected(cycle_outgoing_outcome{ .Status = aStatus }); };
		const auto materialStop = [](const review_material_status aStatus) {
			return std::unexpected(aStatus == review_material_status::cancelled ? cycle_outgoing_outcome{ .Status = cycle_outgoing_status::cancelled }
																				: cycle_outgoing_outcome{ .Status = cycle_outgoing_status::material, .Material = aStatus });
		};
		if (!aCycle.Workspace) {
			return stopWith(cycle_outgoing_status::not_bound);
		}
		const auto kind = aCycle.Workspace->Root.Kind;
		if (kind != core::workspace_root_kind::git_checkout && kind != core::workspace_root_kind::git_linked_worktree) {
			return std::unexpected(cycle_outgoing_outcome{ .Status = cycle_outgoing_status::refused, .Refusal = core::outgoing_refusal::repository_required });
		}
		if (!aCycle.Initial || !aCycle.Latest) {
			return stopWith(cycle_outgoing_status::not_captured);
		}
		const auto startId = aRequest.Start.value_or(aCycle.Initial->Id.Id);
		const auto finalId = aRequest.Final.value_or(aCycle.Latest->Id.Id);
		auto       views   = review_cycle_comparison_host{ Store, aCycle.Subject.Authority };
		const auto start   = views.manifest(aCycle.Subject, startId, aStop);
		if (!start) {
			return start.error() == review_material_status::missing ? stopWith(cycle_outgoing_status::not_captured) : materialStop(start.error());
		}
		const auto final = views.manifest(aCycle.Subject, finalId, aStop);
		if (!final) {
			return final.error() == review_material_status::missing ? stopWith(cycle_outgoing_status::not_captured) : materialStop(final.error());
		}
		const auto algorithm = (*final)->ObjectFormat;

		// P and T are read from the bound checkout, which must still be the
		// root the cycle was bound to.
		auto session = Source.open(aCycle.Workspace->Root.Location, aStop);
		if (!session) {
			return std::unexpected(session.error() == workspace_failure::cancelled ? cycle_outgoing_outcome{ .Status = cycle_outgoing_status::cancelled }
																				   : cycle_outgoing_outcome{ .Status = cycle_outgoing_status::workspace, .Workspace = session.error() });
		}
		if (core::verify_workspace(*aCycle.Workspace, (*session)->observation()) != core::workspace_verification::matches) {
			return stopWith(cycle_outgoing_status::root_changed);
		}
		// A repository the readers refuse is refused here too, with its reason.
		if (const auto format = read_git_repository_format(**session, aStop); !format) {
			if (format.error().Workspace == workspace_failure::cancelled) {
				return stopWith(cycle_outgoing_status::cancelled);
			}
			return std::unexpected(cycle_outgoing_outcome{ .Status = cycle_outgoing_status::workspace, .Workspace = format.error().Workspace, .Repository = format.error().Repository });
		}
		const auto resolve = [&](const std::string& aName) -> std::expected<core::git_commit_oid, cycle_outgoing_outcome> {
			auto resolved = resolve_reference(**session, aName, algorithm, aStop);
			if (resolved) {
				return *resolved;
			}
			if (resolved.error()) {
				return std::unexpected(*resolved.error() == workspace_failure::cancelled ? cycle_outgoing_outcome{ .Status = cycle_outgoing_status::cancelled }
																						 : cycle_outgoing_outcome{ .Status = cycle_outgoing_status::workspace, .Workspace = resolved.error() });
			}
			return std::unexpected(cycle_outgoing_outcome{ .Status = cycle_outgoing_status::reference_unavailable });
		};
		const auto repository = publication_repository{ .Checkout = aCycle.Workspace->Root.Location, .Algorithm = algorithm };
		const auto list       = [&](const core::git_commit_oid& aCommit) -> std::expected<publication_commit_listing, cycle_outgoing_outcome> {
			auto listing = Git.list_commit(repository, aCommit, aStop);
			if (!listing) {
				return std::unexpected(git_failure(std::move(listing.error())));
			}
			// The listing is admitted only for the tree the commit's own bytes name.
			const auto tree = core::verified_commit_tree(listing->CommitObject, aCommit);
			if (!tree && tree.error() == core::verified_commit_error::collision_detected) {
				return std::unexpected(collision_refusal());
			}
			if (tree != listing->Tree) {
				return std::unexpected(git_failure({ .Failure = publication_git_failure::malformed }));
			}
			return std::move(*listing);
		};

		const auto baseCommit = resolve(aRequest.Base);
		if (!baseCommit) {
			return std::unexpected(baseCommit.error());
		}
		auto baseListing = list(*baseCommit);
		if (!baseListing) {
			return std::unexpected(baseListing.error());
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
				return std::unexpected(head.error());
			}
			request.Target = core::publication_target{ .Reference = *aRequest.Target, .Source = core::publication_target_source::local_reference, .Head = *head };
			if (*head != *baseCommit) {
				auto targetListing = list(*head);
				if (!targetListing) {
					return std::unexpected(targetListing.error());
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
					return materialStop(bytes.Status);
				}
				const auto raw = core::git_blob_identity(bytes.Bytes, algorithm);
				if (!raw && raw.error() == core::git_object_refusal::collision_detected) {
					return std::unexpected(collision_refusal());
				}
				auto identity = core::outgoing_identity{ .Content = *entry.Content, .Raw = core::retag<core::git_entry_oid_kind>(raw.value_or(core::git_blob_oid{ })) };
				if (entry.Profile.text() && (entry.Profile.LineEndings == core::material_line_endings::crlf || entry.Profile.LineEndings == core::material_line_endings::mixed)) {
					if (const auto normalized = core::line_feed_form(bytes.Bytes)) {
						const auto lineFeed = core::git_blob_identity(*normalized, algorithm);
						if (!lineFeed && lineFeed.error() == core::git_object_refusal::collision_detected) {
							return std::unexpected(collision_refusal());
						}
						identity.LineFeed = core::retag<core::git_entry_oid_kind>(lineFeed.value_or(core::git_blob_oid{ }));
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
				return page.error().Failure == cycle_store_failure::cancelled ? stopWith(cycle_outgoing_status::cancelled) : materialStop(review_material_status::failure);
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
			return stopWith(cycle_outgoing_status::cancelled);
		}

		auto planned = core::plan_outgoing(request);
		if (!planned) {
			return std::unexpected(cycle_outgoing_outcome{ .Status = cycle_outgoing_status::refused, .Refusal = planned.error() });
		}
		return assembled_plan{ .Request = std::move(request), .Plan = std::move(*planned), .Repository = repository };
	}

	auto review_cycle_outgoing_host::plan(const core::review_cycle_state& aCycle, const cycle_outgoing_request& aRequest, std::stop_token aStop) -> cycle_outgoing_outcome
	{
		auto assembled = assemble(aCycle, aRequest, aStop);
		if (!assembled) {
			return std::move(assembled.error());
		}
		auto preview = core::render_outgoing_preview(assembled->Plan);
		return { .Status = cycle_outgoing_status::planned, .Plan = std::move(assembled->Plan), .Preview = std::move(preview) };
	}

	auto review_cycle_outgoing_host::write_blobs(const core::review_cycle_state& aCycle, const assembled_plan& aAssembled, const std::vector<core::candidate_blob>& aBlobs,
												 const std::stop_token& aStop) -> std::optional<cycle_outgoing_outcome>
	{
		auto bridge = review_material_bridge{ nullptr, &Store, aCycle.Subject.Authority };
		for (const auto& blob : aBlobs) {
			auto bytes = bridge.materialize(
				cycle_material_request{ .Subject = aCycle.Subject, .Revision = aAssembled.Plan.FinalRevision, .Object = { .Digest = blob.Content, .Size = blob.Size }, .MaximumBytes = maximum_cycle_object_bytes },
				aStop);
			if (bytes.Status != review_material_status::available) {
				return material_failure(bytes.Status);
			}
			if (blob.LineFeed) {
				auto normalized = core::line_feed_form(bytes.Bytes);
				if (!normalized) {
					return material_failure(review_material_status::corrupt);
				}
				bytes.Bytes = std::move(*normalized);
			}
			// The bytes must be the object the plan named before Git sees them.
			const auto identity = core::git_blob_identity(bytes.Bytes, aAssembled.Repository.Algorithm);
			if (!identity && identity.error() == core::git_object_refusal::collision_detected) {
				return collision_refusal();
			}
			if (!identity || core::retag<core::git_entry_oid_kind>(*identity) != blob.Object) {
				return material_failure(review_material_status::corrupt);
			}
			if (auto written = Git.write_blob(aAssembled.Repository, blob.Object, bytes.Bytes, aStop); !written) {
				return git_failure(std::move(written.error()));
			}
		}
		return std::nullopt;
	}

	auto review_cycle_outgoing_host::build(const core::review_cycle_state& aCycle, const cycle_candidate_request& aRequest, std::stop_token aStop) -> cycle_candidate_outcome
	{
		auto assembled = assemble(aCycle, aRequest.Outgoing, aStop);
		if (!assembled) {
			return { .Outcome = std::move(assembled.error()) };
		}
		auto result            = cycle_candidate_outcome{ .Outcome = { .Status = cycle_outgoing_status::planned, .Plan = assembled->Plan } };
		auto candidate         = core::prepare_candidate(assembled->Plan, assembled->Request.Identities, aRequest.Candidate);
		result.Outcome.Preview = core::render_outgoing_preview(assembled->Plan);
		if (!candidate) {
			result.Outcome.Status = cycle_outgoing_status::limited;
			result.Limit          = std::move(candidate.error());
			return result;
		}
		if (auto failure = write_blobs(aCycle, *assembled, candidate->Blobs, aStop)) {
			return { .Outcome = std::move(*failure) };
		}
		auto changes = std::vector<core::git_tree_entry>{ };
		for (const auto& blob : candidate->Blobs) {
			changes.push_back({ .Path = blob.Path, .Mode = blob.Mode, .Object = blob.Object });
		}
		if (auto tree = Git.write_tree(assembled->Repository, { .Base = assembled->Plan.BaseTree, .Changes = std::move(changes), .Deletions = candidate->Deleted, .Expected = candidate->Tree }, aStop); !tree) {
			return { .Outcome = git_failure(std::move(tree.error())) };
		}
		if (auto commit = Git.write_commit(assembled->Repository, candidate->Content, candidate->Commit, aStop); !commit) {
			return { .Outcome = git_failure(std::move(commit.error())) };
		}
		result.Outcome.Preview += core::render_candidate_preview(*candidate);
		result.Candidate = std::move(*candidate);
		return result;
	}

	auto review_cycle_outgoing_host::prepare_external(const core::review_cycle_state& aCycle, const cycle_outgoing_request& aRequest, const core::candidate_requirements& aRequirements,
													  std::stop_token aStop) -> cycle_external_outcome
	{
		auto assembled = assemble(aCycle, aRequest, aStop);
		if (!assembled) {
			return { .Outcome = std::move(assembled.error()) };
		}
		auto result      = cycle_external_outcome{ .Outcome = { .Status = cycle_outgoing_status::planned, .Plan = assembled->Plan, .Preview = core::render_outgoing_preview(assembled->Plan) } };
		auto preparation = core::prepare_external(assembled->Plan, aRequirements);
		if (!preparation) {
			result.Outcome.Status = cycle_outgoing_status::limited;
			result.Limit          = std::move(preparation.error());
			return result;
		}
		if (auto failure = write_blobs(aCycle, *assembled, preparation->Blobs, aStop)) {
			return { .Outcome = std::move(*failure) };
		}
		auto changes = std::vector<core::git_tree_entry>{ };
		for (const auto& blob : preparation->Blobs) {
			changes.push_back({ .Path = blob.Path, .Mode = blob.Mode, .Object = blob.Object });
		}
		if (auto tree = Git.write_tree(assembled->Repository, { .Base = assembled->Plan.BaseTree, .Changes = std::move(changes), .Deletions = preparation->Deleted, .Expected = preparation->Tree }, aStop);
			!tree) {
			return { .Outcome = git_failure(std::move(tree.error())) };
		}
		auto patch = Git.diff_trees(assembled->Repository, assembled->Plan.BaseTree, preparation->Tree, aStop);
		if (!patch) {
			return { .Outcome = git_failure(std::move(patch.error())) };
		}
		result.Patch       = std::move(*patch);
		result.Preparation = std::move(*preparation);
		return result;
	}
}
