#include "bpr/application/review_cycle_publisher.hpp"

#include "bpr/application/review_material_bridge.hpp"
#include "bpr/core/sha256.hpp"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace bpr::application {
	namespace {
		/// One journal change's operation identity: the same publication,
		/// revision and change always get the same one, so a retry after a
		/// crash replays instead of recording twice.
		auto journal_operation(const core::local_cycle_subject& aSubject, const core::cycle_publication_id aPublication, const std::uint64_t aRevision, const std::string_view aChange)
			-> cycle_operation_id
		{
			auto bytes = std::vector<std::uint8_t>{ };
			for (const auto character : std::string_view{ "bpr.publication.journal.v1:" }) {
				bytes.push_back(static_cast<std::uint8_t>(character));
			}
			for (const auto character : aChange) {
				bytes.push_back(static_cast<std::uint8_t>(character));
			}
			const auto append = [&bytes](const std::uint64_t aValue) {
				for (auto shift = 56; shift >= 0; shift -= 8) {
					bytes.push_back(static_cast<std::uint8_t>(aValue >> static_cast<unsigned>(shift)));
				}
			};
			append(aSubject.Authority.Profile.value());
			bytes.insert(bytes.end(), aSubject.Authority.Incarnation.bytes().begin(), aSubject.Authority.Incarnation.bytes().end());
			append(aSubject.Cycle.value());
			append(aSubject.Generation);
			append(aPublication.value());
			append(aRevision);
			return cycle_operation_id{ core::sha256(bytes) };
		}

		auto target_of(const core::publication_plan& aPlan, const github_server_contract aServer) -> github_publication_target
		{
			const auto& destination = aPlan.Destination;
			return { .Host = destination.Host, .Server = aServer, .Account = destination.Account, .Login = destination.Login, .Owner = destination.Owner, .Repository = destination.Repository };
		}

		auto limited(const cycle_publish_limit aLimit, stored_cycle_publication aPublication) -> cycle_publish_outcome
		{
			return { .Status = cycle_publish_status::limited, .Limit = aLimit, .Publication = std::move(aPublication) };
		}

		auto provider(github_publication_error aError, stored_cycle_publication aPublication) -> cycle_publish_outcome
		{
			return { .Status      = aError.Failure == github_publication_failure::cancelled ? cycle_publish_status::cancelled : cycle_publish_status::provider,
					 .Provider    = aError,
					 .Publication = std::move(aPublication) };
		}

		/// What an attempt's failure means for the journal.
		auto outcome_of(const github_publication_error& aError) -> core::publication_effect_state
		{
			switch (aError.Failure) {
			case github_publication_failure::refused:
			case github_publication_failure::mismatch:
			case github_publication_failure::too_large:
				return aError.Transmission == github_publication_transmission::definitely_not_sent && aError.HttpStatus == 0U ? core::publication_effect_state::not_sent
																															  : core::publication_effect_state::refused;
			case github_publication_failure::unsupported_host:
				return core::publication_effect_state::not_sent;
			case github_publication_failure::unavailable:
			case github_publication_failure::cancelled:
			case github_publication_failure::rate_limited:
				break;
			}
			// Anything that may have reached the server is uncertain until read.
			return aError.Transmission == github_publication_transmission::definitely_not_sent ? core::publication_effect_state::not_sent : core::publication_effect_state::uncertain;
		}

		[[nodiscard]] auto workflow_path(const std::string_view aPath) -> bool
		{
			return aPath.starts_with(".github/workflows/");
		}

		/// The link a confirmed pull request records, drift marked when the
		/// server's head, base branch or base head is not what the plan named.
		auto link_of(const core::publication_plan& aPlan, const github_pull_request_observation& aPull) -> core::publication_link
		{
			return {
				.Number     = aPull.Number,
				.NodeId     = aPull.NodeId,
				.Url        = aPull.Url,
				.Head       = aPull.HeadCommit,
				.BaseBranch = aPull.BaseRef,
				.Drifted    = aPull.HeadCommit != aPlan.Commit || aPull.BaseRef != aPlan.Destination.BaseBranch || (aPlan.TargetHead && aPull.BaseCommit != *aPlan.TargetHead),
			};
		}

		/// Every pull request whose head is the plan's branch in the plan's
		/// repository and whose base is the plan's base.
		auto candidates(const core::publication_plan& aPlan, const std::vector<github_pull_request_observation>& aPulls) -> std::vector<github_pull_request_observation>
		{
			auto found = std::vector<github_pull_request_observation>{ };
			std::ranges::copy_if(aPulls, std::back_inserter(found), [&aPlan](const github_pull_request_observation& aPull) { return match_pull_request(aPlan, aPull).Candidate; });
			return found;
		}
	}

	auto match_pull_request(const core::publication_plan& aPlan, const github_pull_request_observation& aPull) -> cycle_pull_request_match
	{
		const auto& destination = aPlan.Destination;
		const auto  fold        = [](const std::string_view aLeft, const std::string_view aRight) {
			return aLeft.size() == aRight.size() && std::ranges::equal(aLeft, aRight, [](const char aA, const char aB) {
					   const auto lower = [](const char aValue) { return aValue >= 'A' && aValue <= 'Z' ? static_cast<char>(aValue + ('a' - 'A')) : aValue; };
					   return lower(aA) == lower(aB);
				   });
		};
		const auto candidate = aPull.HeadRef == destination.NewBranch && aPull.BaseRef == destination.BaseBranch && fold(aPull.HeadOwner, destination.Owner)
			&& fold(aPull.HeadRepository, destination.Repository);
		return { .Candidate = candidate, .Ours = candidate && aPull.HeadCommit == aPlan.Commit && fold(aPull.Author, destination.Login) };
	}

	auto name(const cycle_publish_limit aLimit) noexcept -> std::string_view
	{
		switch (aLimit) {
		case cycle_publish_limit::unsupported_host:
			return "unsupported_host";
		case cycle_publish_limit::object_format:
			return "object_format";
		case cycle_publish_limit::no_push_permission:
			return "no_push_permission";
		case cycle_publish_limit::archived:
			return "archived";
		case cycle_publish_limit::protected_path:
			return "protected_path";
		case cycle_publish_limit::parent_not_on_remote:
			return "parent_not_on_remote";
		case cycle_publish_limit::target_moved:
			return "target_moved";
		case cycle_publish_limit::branch_exists:
			return "branch_exists";
		case cycle_publish_limit::blob_too_large:
			return "blob_too_large";
		case cycle_publish_limit::material:
			return "material";
		case cycle_publish_limit::pull_request_exists:
			return "pull_request_exists";
		}
		return "unknown";
	}

	review_cycle_publisher::review_cycle_publisher(review_cycle_publication_host& aJournal, review_cycle_store& aStore, github_publication_port& aProvider, const github_server_contract aServer,
												   const core::github_account_id aAccount) noexcept
		: Journal{ aJournal }
		, Store{ aStore }
		, Provider{ aProvider }
		, Server{ aServer }
		, Account{ aAccount }
	{
	}

	auto review_cycle_publisher::publish(const core::local_cycle_subject& aSubject, const core::cycle_publication_id aPublication, std::stop_token aStop) -> cycle_publish_outcome
	{
		auto current = Journal.load(aSubject, aPublication, aStop);
		if (!current) {
			return { .Status = cycle_publish_status::store, .Store = current.error() };
		}
		auto        publication = std::move(*current);
		const auto& plan        = publication.Plan;
		if (publication.Journal.Generation == 0U || publication.Journal.Phase == core::publication_phase::cancelled || publication.Journal.Phase == core::publication_phase::linked
			|| publication.Journal.Phase == core::publication_phase::failed || publication.Journal.Phase == core::publication_phase::target_drift
			|| publication.Journal.Phase == core::publication_phase::superseded || publication.Journal.Phase == core::publication_phase::uncertain) {
			return { .Status = cycle_publish_status::unauthorized, .Publication = std::move(publication) };
		}
		const auto target = target_of(plan, Server);

		// Capability first: nothing is written until every check passes.
		if (Account != plan.Destination.Account) {
			return limited(cycle_publish_limit::unsupported_host, std::move(publication));
		}
		if (plan.Format != core::git_hash_algorithm::sha1) {
			return limited(cycle_publish_limit::object_format, std::move(publication));
		}
		if (std::ranges::any_of(plan.Blobs, [](const core::candidate_blob& aBlob) { return workflow_path(aBlob.Path); }) || std::ranges::any_of(plan.Deleted, workflow_path)) {
			return limited(cycle_publish_limit::protected_path, std::move(publication));
		}
		if (std::ranges::any_of(plan.Blobs, [](const core::candidate_blob& aBlob) { return aBlob.Size > maximum_github_publication_blob_bytes; })) {
			return limited(cycle_publish_limit::blob_too_large, std::move(publication));
		}
		const auto repository = Provider.observe_repository(target, aStop);
		if (!repository) {
			return repository.error().Failure == github_publication_failure::unsupported_host ? limited(cycle_publish_limit::unsupported_host, std::move(publication))
																							  : provider(repository.error(), std::move(publication));
		}
		if (repository->Archived) {
			return limited(cycle_publish_limit::archived, std::move(publication));
		}
		if (!repository->CanPush) {
			return limited(cycle_publish_limit::no_push_permission, std::move(publication));
		}
		const auto parent = Provider.observe_commit(target, plan.Parent, aStop);
		if (!parent) {
			return provider(parent.error(), std::move(publication));
		}
		if (!*parent) {
			return limited(cycle_publish_limit::parent_not_on_remote, std::move(publication));
		}
		const auto base = Provider.observe_branch(target, plan.Destination.BaseBranch, aStop);
		if (!base) {
			return provider(base.error(), std::move(publication));
		}
		if (!*base || (plan.TargetHead && **base != *plan.TargetHead)) {
			return limited(cycle_publish_limit::target_moved, std::move(publication));
		}
		const auto branchEffect = std::ranges::any_of(publication.Journal.Effects, [](const core::publication_effect& aEffect) { return aEffect.Kind == core::publication_effect_kind::branch; });
		const auto existing     = Provider.observe_branch(target, plan.Destination.NewBranch, aStop);
		if (!existing) {
			return provider(existing.error(), std::move(publication));
		}
		if (*existing && !branchEffect) {
			return limited(cycle_publish_limit::branch_exists, std::move(publication));
		}

		auto bridge = review_material_bridge{ nullptr, &Store, aSubject.Authority };
		while (const auto step = core::next_publication_step(plan, publication.Journal)) {
			if (step->Kind == core::publication_effect_kind::pull_request) {
				return pull_request(aSubject, aPublication, std::move(publication), target, aStop);
			}
			// Bytes are read and checked before the attempt is recorded.
			auto bytes = std::vector<std::uint8_t>{ };
			if (step->Kind == core::publication_effect_kind::blob) {
				const auto blob = std::ranges::find_if(plan.Blobs, [&](const core::candidate_blob& aBlob) { return core::canonical_hex(aBlob.Object).view() == step->Subject; });
				if (blob == plan.Blobs.end()) {
					return limited(cycle_publish_limit::material, std::move(publication));
				}
				auto read = bridge.materialize(
					cycle_material_request{ .Subject = aSubject, .Revision = plan.Final, .Object = { .Digest = blob->Content, .Size = blob->Size }, .MaximumBytes = maximum_github_publication_blob_bytes }, aStop);
				if (read.Status != review_material_status::available) {
					return limited(cycle_publish_limit::material, std::move(publication));
				}
				bytes = std::move(read.Bytes);
				if (blob->LineFeed) {
					auto normalized = core::line_feed_form(bytes);
					if (!normalized) {
						return limited(cycle_publish_limit::material, std::move(publication));
					}
					bytes = std::move(*normalized);
				}
				if (core::retag<core::git_entry_oid_kind>(core::git_blob_identity(bytes, plan.Format)) != blob->Object) {
					return limited(cycle_publish_limit::material, std::move(publication));
				}
			}
			const auto begun = Journal.change(aSubject, journal_operation(aSubject, aPublication, publication.Revision, "begin"), aPublication, publication.Revision,
											  core::begin_publication_effect{ .Kind = step->Kind, .Subject = step->Subject, .Generation = publication.Journal.Generation, .Account = Account },
											  aStop);
			if (!begun) {
				return { .Status = cycle_publish_status::store, .Store = begun.error(), .Publication = std::move(publication) };
			}
			publication = begun->Publication;

			auto attempt = github_publication_result<void>{ };
			auto remote  = std::string{ step->Subject };
			auto taken   = false;
			switch (step->Kind) {
			case core::publication_effect_kind::blob:
				attempt = Provider.create_blob(target, bytes, *core::parse_git_entry_oid(step->Subject), aStop);
				break;
			case core::publication_effect_kind::tree: {
				auto changes = std::vector<core::git_tree_entry>{ };
				for (const auto& blob : plan.Blobs) {
					changes.push_back({ .Path = blob.Path, .Mode = blob.Mode, .Object = blob.Object });
				}
				attempt = Provider.create_tree(target, (*parent)->Tree, changes, plan.Deleted, plan.Tree, aStop);
				break;
			}
			case core::publication_effect_kind::commit:
				attempt = Provider.create_commit(target, plan.Content, plan.Commit, aStop);
				break;
			case core::publication_effect_kind::branch: {
				const auto created = Provider.create_branch(target, plan.Destination.NewBranch, plan.Commit, aStop);
				if (!created) {
					attempt = std::unexpected{ created.error() };
				} else if (*created == github_branch_creation::exists) {
					// Never overwritten, never adopted: another branch holds the name.
					taken  = true;
					remote = "refs/heads/" + plan.Destination.NewBranch;
				} else {
					remote = "refs/heads/" + plan.Destination.NewBranch;
				}
				break;
			}
			case core::publication_effect_kind::pull_request:
				break;
			}
			const auto outcome   = taken ? core::publication_effect_state::foreign : attempt ? core::publication_effect_state::confirmed
																							 : outcome_of(attempt.error());
			const auto completed = Journal.change(aSubject, journal_operation(aSubject, aPublication, publication.Revision, "complete"), aPublication, publication.Revision,
												  core::complete_publication_effect{ .Sequence = begun->Sequence, .Outcome = outcome, .Remote = remote }, aStop);
			if (!completed) {
				return { .Status = cycle_publish_status::store, .Store = completed.error(), .Publication = std::move(publication) };
			}
			publication = completed->Publication;
			if (outcome != core::publication_effect_state::confirmed) {
				return { .Status = cycle_publish_status::stopped, .Provider = attempt ? std::nullopt : std::optional{ attempt.error() }, .Publication = std::move(publication) };
			}
		}
		return { .Status      = publication.Journal.Phase == core::publication_phase::linked ? cycle_publish_status::linked : cycle_publish_status::branch_created,
				 .Publication = std::move(publication) };
	}

	auto review_cycle_publisher::recover(const core::local_cycle_subject& aSubject, const core::cycle_publication_id aPublication, std::stop_token aStop) -> cycle_publish_outcome
	{
		auto current = Journal.load(aSubject, aPublication, aStop);
		if (!current) {
			return { .Status = cycle_publish_status::store, .Store = current.error() };
		}
		auto       publication = std::move(*current);
		const auto plan        = publication.Plan;
		const auto target      = target_of(plan, Server);
		for (const auto& effect : std::vector<core::publication_effect>{ publication.Journal.Effects }) {
			if (effect.State != core::publication_effect_state::uncertain) {
				continue;
			}
			auto observed = core::publication_effect_state::uncertain;
			auto remote   = effect.Remote;
			auto link     = std::optional<core::publication_link>{ };
			switch (effect.Kind) {
			case core::publication_effect_kind::blob: {
				const auto present = Provider.observe_blob(target, *core::parse_git_entry_oid(effect.Subject), aStop);
				if (!present) {
					return provider(present.error(), std::move(publication));
				}
				observed = *present ? core::publication_effect_state::confirmed : core::publication_effect_state::absent;
				break;
			}
			case core::publication_effect_kind::tree: {
				const auto present = Provider.observe_tree(target, plan.Tree, aStop);
				if (!present) {
					return provider(present.error(), std::move(publication));
				}
				observed = *present ? core::publication_effect_state::confirmed : core::publication_effect_state::absent;
				break;
			}
			case core::publication_effect_kind::commit: {
				const auto present = Provider.observe_commit(target, plan.Commit, aStop);
				if (!present) {
					return provider(present.error(), std::move(publication));
				}
				observed = !*present                                                                 ? core::publication_effect_state::absent
					: ((*present)->Tree == plan.Tree && (*present)->Parents == plan.Content.Parents) ? core::publication_effect_state::confirmed
																									 : core::publication_effect_state::foreign;
				break;
			}
			case core::publication_effect_kind::branch: {
				// This plan's journal intended the branch; only a branch at
				// exactly the planned commit is taken as its effect.
				const auto head = Provider.observe_branch(target, plan.Destination.NewBranch, aStop);
				if (!head) {
					return provider(head.error(), std::move(publication));
				}
				observed = !*head ? core::publication_effect_state::absent : **head == plan.Commit ? core::publication_effect_state::confirmed
																								   : core::publication_effect_state::foreign;
				remote   = "refs/heads/" + plan.Destination.NewBranch;
				break;
			}
			case core::publication_effect_kind::pull_request: {
				// A lost answer is reconciled by reading every pull request
				// for the exact head and base: exactly one of ours is this
				// plan's; more than one needs the reviewer; only others' is
				// foreign; none means it was not created.
				const auto pulls = Provider.find_pull_requests(target, plan.Destination.NewBranch, plan.Destination.BaseBranch, aStop);
				if (!pulls) {
					return provider(pulls.error(), std::move(publication));
				}
				const auto found = candidates(plan, *pulls);
				auto       ours  = std::vector<github_pull_request_observation>{ };
				std::ranges::copy_if(found, std::back_inserter(ours), [&plan](const github_pull_request_observation& aPull) { return match_pull_request(plan, aPull).Ours; });
				if (ours.size() == 1U && found.size() == 1U) {
					observed = core::publication_effect_state::confirmed;
					link     = link_of(plan, ours.front());
					remote   = ours.front().Url;
				} else if (found.empty()) {
					observed = core::publication_effect_state::absent;
				} else if (ours.empty()) {
					observed = core::publication_effect_state::foreign;
				}
				break;
			}
			}
			const auto changed = Journal.change(aSubject, journal_operation(aSubject, aPublication, publication.Revision, "observe"), aPublication, publication.Revision,
												core::observe_publication_effect{ .Sequence = effect.Sequence, .Observed = observed, .Remote = remote, .Link = link }, aStop);
			if (!changed) {
				return { .Status = cycle_publish_status::store, .Store = changed.error(), .Publication = std::move(publication) };
			}
			publication = changed->Publication;
		}
		return { .Status = cycle_publish_status::recovered, .Publication = std::move(publication) };
	}
	auto review_cycle_publisher::pull_request(const core::local_cycle_subject& aSubject, const core::cycle_publication_id aPublication, stored_cycle_publication aCurrent,
											  const github_publication_target& aTarget, std::stop_token aStop) -> cycle_publish_outcome
	{
		auto        publication = std::move(aCurrent);
		const auto& plan        = publication.Plan;
		// Read before creating: a pull request this plan never asked for is
		// never duplicated or silently adopted.
		const auto existing = Provider.find_pull_requests(aTarget, plan.Destination.NewBranch, plan.Destination.BaseBranch, aStop);
		if (!existing) {
			return provider(existing.error(), std::move(publication));
		}
		if (!candidates(plan, *existing).empty()) {
			return limited(cycle_publish_limit::pull_request_exists, std::move(publication));
		}
		const auto begun = Journal.change(aSubject, journal_operation(aSubject, aPublication, publication.Revision, "begin"), aPublication, publication.Revision,
										  core::begin_publication_effect{ .Kind = core::publication_effect_kind::pull_request, .Subject = "pull", .Generation = publication.Journal.Generation, .Account = Account },
										  aStop);
		if (!begun) {
			return { .Status = cycle_publish_status::store, .Store = begun.error(), .Publication = std::move(publication) };
		}
		publication        = begun->Publication;
		const auto created = Provider.create_pull_request(
			aTarget, { .Title = plan.PullTitle, .Body = plan.PullBody, .Head = plan.Destination.NewBranch, .Base = plan.Destination.BaseBranch, .Draft = plan.Draft }, aStop);
		auto outcome = core::complete_publication_effect{ .Sequence = begun->Sequence };
		if (created) {
			outcome.Outcome = core::publication_effect_state::confirmed;
			outcome.Remote  = created->Url;
			outcome.Link    = link_of(plan, *created);
		} else if (created.error().Failure == github_publication_failure::refused && created.error().HttpStatus == 422U) {
			// Refused, perhaps because another pull request for this head
			// appeared since the read: never create a second one.
			const auto after = Provider.find_pull_requests(aTarget, plan.Destination.NewBranch, plan.Destination.BaseBranch, aStop);
			outcome.Outcome  = after && !candidates(plan, *after).empty() ? core::publication_effect_state::foreign : !after ? core::publication_effect_state::uncertain
																															 : core::publication_effect_state::refused;
		} else {
			outcome.Outcome = outcome_of(created.error());
		}
		const auto completed = Journal.change(aSubject, journal_operation(aSubject, aPublication, publication.Revision, "complete"), aPublication, publication.Revision, outcome, aStop);
		if (!completed) {
			return { .Status = cycle_publish_status::store, .Store = completed.error(), .Publication = std::move(publication) };
		}
		publication = completed->Publication;
		if (outcome.Outcome != core::publication_effect_state::confirmed) {
			return { .Status = cycle_publish_status::stopped, .Provider = created ? std::nullopt : std::optional{ created.error() }, .Publication = std::move(publication) };
		}
		// The created pull request's actual head and base, read again: a
		// concurrent push or retarget is recorded, not hidden.
		const auto reread = Provider.observe_pull_request(aTarget, created->Number, aStop);
		if (reread && *reread) {
			const auto refreshed = Journal.change(aSubject, journal_operation(aSubject, aPublication, publication.Revision, "refresh"), aPublication, publication.Revision,
												  core::refresh_publication_link{ .Link = link_of(plan, **reread) }, aStop);
			if (refreshed) {
				publication = refreshed->Publication;
			}
		}
		return { .Status = cycle_publish_status::linked, .Publication = std::move(publication) };
	}
}
