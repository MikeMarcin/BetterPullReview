#include "bpr/application/review_cycle_publishing.hpp"

#include <algorithm>
#include <iterator>
#include <utility>

namespace bpr::application {
	namespace {
		/// Ends a step by reading what the cycle holds now.
		auto finish(review_cycle_store& aStore, const core::review_cycle_state& aCycle, review_cycle_publication_step aStep, const std::stop_token& aStop)
			-> cycle_store_result<review_cycle_publication_step>
		{
			auto read = read_publications(aStore, aCycle, false, aStop);
			if (!read) {
				return std::unexpected(read.error());
			}
			aStep.Publications = std::move(read->Publications);
			aStep.Links        = std::move(read->Links);
			return aStep;
		}

		/// The hosts one step composes over the store, the workspace and the catalog.
		struct hosts
		{
			review_cycle_outgoing_host    Outgoing;
			review_cycle_publication_host Journal;

			hosts(workspace_source_port& aSource, review_cycle_store& aStore, publication_candidate_port& aGit)
				: Outgoing{ aSource, aStore, aGit }
				, Journal{ Outgoing, aStore }
			{
			}
		};

		auto catalog_failure(review_cycle_store& aStore, const core::review_cycle_state& aCycle, publication_git_error aError, const std::stop_token& aStop)
			-> cycle_store_result<review_cycle_publication_step>
		{
			auto step    = review_cycle_publication_step{ };
			step.Catalog = std::move(aError);
			return finish(aStore, aCycle, std::move(step), aStop);
		}

		/// Every association the cycle holds now, to decide a link against.
		auto known_links(review_cycle_store& aStore, const core::review_cycle_state& aCycle, const std::stop_token& aStop) -> cycle_store_result<std::vector<core::cycle_pull_association>>
		{
			auto read = read_publications(aStore, aCycle, false, aStop);
			if (!read) {
				return std::unexpected(read.error());
			}
			return std::move(read->Links);
		}
	}

	review_cycle_candidate_catalog::review_cycle_candidate_catalog(std::function<publication_git_result<std::unique_ptr<publication_candidate_port>>(std::stop_token)> aMake)
		: Make{ std::move(aMake) }
	{
	}

	auto review_cycle_candidate_catalog::get(std::stop_token aStop) -> publication_git_result<publication_candidate_port*>
	{
		if (!Catalog) {
			if (!Make) {
				return std::unexpected(publication_git_error{ .Failure = publication_git_failure::unavailable, .Detail = "no local Git catalog is configured" });
			}
			auto made = Make(std::move(aStop));
			if (!made) {
				return std::unexpected(std::move(made.error()));
			}
			if (!*made) {
				return std::unexpected(publication_git_error{ .Failure = publication_git_failure::unavailable });
			}
			Catalog = std::move(*made);
		}
		return Catalog.get();
	}

	auto read_publications(review_cycle_store& aStore, const core::review_cycle_state& aCycle, const bool aRecover, std::stop_token aStop) -> cycle_store_result<review_cycle_publication_step>
	{
		if (aRecover) {
			if (auto recovered = recover_publications_after_restart(aStore, aCycle.Subject, aStop); !recovered) {
				return std::unexpected(recovered.error());
			}
		}
		auto step  = review_cycle_publication_step{ };
		auto after = std::optional<core::cycle_publication_id>{ };
		do {
			auto page = aStore.publications(aCycle.Subject, after, aStop);
			if (!page) {
				return std::unexpected(page.error());
			}
			for (auto& publication : page->Publications) {
				if (auto linked = core::published_association(publication.Id.Id, publication.Plan, publication.Journal)) {
					step.Links.push_back(std::move(*linked));
				}
				step.Publications.push_back(std::move(publication));
			}
			after = page->Next;
		} while (after);
		auto external = aStore.associations(aCycle.Subject, aStop);
		if (!external) {
			return std::unexpected(external.error());
		}
		std::ranges::move(*external, std::back_inserter(step.Links));
		return step;
	}

	auto plan_publication(review_cycle_store& aStore, workspace_source_port& aSource, review_cycle_candidate_catalog& aCatalog, const core::review_cycle_state& aCycle,
						  const cycle_outgoing_request& aRequest, std::stop_token aStop) -> cycle_store_result<review_cycle_publication_step>
	{
		auto git = aCatalog.get(aStop);
		if (!git) {
			return catalog_failure(aStore, aCycle, std::move(git.error()), aStop);
		}
		auto host = review_cycle_outgoing_host{ aSource, aStore, **git };
		auto step = review_cycle_publication_step{ .Planned = host.plan(aCycle, aRequest, aStop) };
		return finish(aStore, aCycle, std::move(step), aStop);
	}

	auto prepare_publication(review_cycle_store& aStore, workspace_source_port& aSource, review_cycle_candidate_catalog& aCatalog, const core::review_cycle_state& aCycle,
							 const cycle_operation_id& aOperation, const cycle_publication_request& aRequest, std::stop_token aStop) -> cycle_store_result<review_cycle_publication_step>
	{
		auto git = aCatalog.get(aStop);
		if (!git) {
			return catalog_failure(aStore, aCycle, std::move(git.error()), aStop);
		}
		auto composed = hosts{ aSource, aStore, **git };
		auto step     = review_cycle_publication_step{ .Prepared = composed.Journal.prepare(aCycle, aOperation, aRequest, aStop) };
		return finish(aStore, aCycle, std::move(step), aStop);
	}

	auto publish_publication(review_cycle_store& aStore, workspace_source_port& aSource, review_cycle_candidate_catalog& aCatalog, const core::review_cycle_state& aCycle,
							 const review_cycle_publication_account& aAccount, const review_cycle_publish_request& aRequest, std::stop_token aStop)
		-> cycle_store_result<review_cycle_publication_step>
	{
		if (!aAccount.Provider) {
			return std::unexpected(cycle_store_error{ .Failure = cycle_store_failure::refused, .Publication = core::publication_refusal::unauthorized });
		}
		auto git = aCatalog.get(aStop);
		if (!git) {
			return catalog_failure(aStore, aCycle, std::move(git.error()), aStop);
		}
		auto composed = hosts{ aSource, aStore, **git };
		// Only the reviewer's confirmation of the digest they saw, under the
		// account authenticated now, authorizes; the core refuses anything else.
		const auto authorized = composed.Journal.authorize(aCycle.Subject, aRequest.Operation, aRequest.Publication, aRequest.ExpectedRevision,
														   { .Actor = core::cycle_actor::reviewer, .PlanDigest = aRequest.Digest, .Account = aAccount.Account }, aStop);
		if (!authorized) {
			return std::unexpected(authorized.error());
		}
		auto publisher = review_cycle_publisher{ composed.Journal, aStore, *aAccount.Provider, aAccount.Server, aAccount.Account };
		auto step      = review_cycle_publication_step{ .Published = publisher.publish(aCycle.Subject, aRequest.Publication, aStop) };
		return finish(aStore, aCycle, std::move(step), aStop);
	}

	auto recover_publication(review_cycle_store& aStore, workspace_source_port& aSource, review_cycle_candidate_catalog& aCatalog, const core::review_cycle_state& aCycle,
							 const review_cycle_publication_account& aAccount, const core::cycle_publication_id aPublication, std::stop_token aStop)
		-> cycle_store_result<review_cycle_publication_step>
	{
		if (!aAccount.Provider) {
			return std::unexpected(cycle_store_error{ .Failure = cycle_store_failure::refused, .Publication = core::publication_refusal::unauthorized });
		}
		auto git = aCatalog.get(aStop);
		if (!git) {
			return catalog_failure(aStore, aCycle, std::move(git.error()), aStop);
		}
		auto composed  = hosts{ aSource, aStore, **git };
		auto publisher = review_cycle_publisher{ composed.Journal, aStore, *aAccount.Provider, aAccount.Server, aAccount.Account };
		auto step      = review_cycle_publication_step{ .Published = publisher.recover(aCycle.Subject, aPublication, aStop) };
		return finish(aStore, aCycle, std::move(step), aStop);
	}

	auto cancel_publication(review_cycle_store& aStore, const core::review_cycle_state& aCycle, const cycle_operation_id& aOperation, const core::cycle_publication_id aPublication,
							const std::uint64_t aExpectedRevision, std::stop_token aStop) -> cycle_store_result<review_cycle_publication_step>
	{
		const auto receipt = aStore.change_publication(aOperation, aCycle.Subject, aPublication, aExpectedRevision, core::cancel_publication{ }, aStop);
		if (!receipt) {
			return std::unexpected(receipt.error());
		}
		return finish(aStore, aCycle, { }, aStop);
	}

	auto save_publication_patch(review_cycle_store& aStore, workspace_source_port& aSource, review_cycle_candidate_catalog& aCatalog, const core::review_cycle_state& aCycle,
								const cycle_outgoing_request& aRequest, cycle_export_destination& aDestination, const std::string& /*aName*/, std::stop_token aStop)
		-> cycle_store_result<review_cycle_publication_step>
	{
		auto git = aCatalog.get(aStop);
		if (!git) {
			return catalog_failure(aStore, aCycle, std::move(git.error()), aStop);
		}
		auto host = review_cycle_outgoing_host{ aSource, aStore, **git };
		auto step = review_cycle_publication_step{ .External = host.prepare_external(aCycle, aRequest, { }, aStop) };
		if (step.External->Preparation && !step.External->Patch.empty()) {
			// A file already there is never replaced: the reviewer chooses another name.
			auto saved = save_round_document(aDestination, step.External->Patch, std::nullopt, aStop);
			if (saved) {
				step.PatchSave = std::move(*saved);
			} else {
				step.PatchFailure = saved.error();
			}
		}
		return finish(aStore, aCycle, std::move(step), aStop);
	}

	auto link_pull_request(review_cycle_store& aStore, const core::review_cycle_state& aCycle, const review_cycle_publication_account& aAccount, const cycle_operation_id& aOperation,
						   const core::cycle_pull_address& aAddress, std::stop_token aStop) -> cycle_store_result<review_cycle_publication_step>
	{
		auto step  = review_cycle_publication_step{ };
		auto known = known_links(aStore, aCycle, aStop);
		if (!known) {
			return std::unexpected(known.error());
		}
		if (const auto found = std::ranges::find_if(*known, [&](const core::cycle_pull_association& aKnown) { return core::same_pull_request(aKnown.Address, aAddress); }); found != known->end()) {
			step.Linked      = *found;
			step.LinkRefusal = core::cycle_link_refusal::already_linked;
			return finish(aStore, aCycle, std::move(step), aStop);
		}
		if (!aAccount.Provider) {
			return std::unexpected(cycle_store_error{ .Failure = cycle_store_failure::refused, .Publication = core::publication_refusal::unauthorized });
		}
		if (!core::same_pull_request({ .Host = aAccount.Host, .Owner = aAddress.Owner, .Repository = aAddress.Repository, .Number = aAddress.Number }, aAddress)) {
			step.LinkRefusal = core::cycle_link_refusal::host;
			return finish(aStore, aCycle, std::move(step), aStop);
		}
		const auto target = github_publication_target{
			.Host = aAccount.Host, .Server = aAccount.Server, .Account = aAccount.Account, .Login = aAccount.Login, .Owner = aAddress.Owner, .Repository = aAddress.Repository
		};
		// One read of exactly that pull request; nothing is written remotely.
		const auto read = aAccount.Provider->observe_pull_request(target, aAddress.Number, aStop);
		if (!read) {
			step.Provider = read.error();
			return finish(aStore, aCycle, std::move(step), aStop);
		}
		if (!*read) {
			step.LinkRefusal = core::cycle_link_refusal::not_found;
			return finish(aStore, aCycle, std::move(step), aStop);
		}
		const auto& pull     = **read;
		const auto  observed = core::cycle_pull_observation{
			.Address  = { .Host = aAccount.Host, .Owner = aAddress.Owner, .Repository = aAddress.Repository, .Number = pull.Number },
			.NodeId   = pull.NodeId,
			.Url      = pull.Url,
			.State    = pull.State,
			.Draft    = pull.Draft,
			.HeadRef  = pull.HeadRef,
			.Head     = pull.HeadCommit,
			.BaseRef  = pull.BaseRef,
			.BaseHead = pull.BaseCommit.valid() ? std::optional{ pull.BaseCommit } : std::nullopt,
			.Account  = aAccount.Account,
			.Login    = aAccount.Login,
		};
		auto decided = core::decide_external_link(aAddress, observed, *known, aCycle.Latest ? std::optional{ aCycle.Latest->Id.Id } : std::nullopt);
		if (!decided) {
			step.LinkRefusal = decided.error();
			return finish(aStore, aCycle, std::move(step), aStop);
		}
		const auto recorded = aStore.record_association(aOperation, aCycle.Subject, *decided, aStop);
		if (!recorded) {
			return std::unexpected(recorded.error());
		}
		step.Linked = std::move(*decided);
		return finish(aStore, aCycle, std::move(step), aStop);
	}
}
