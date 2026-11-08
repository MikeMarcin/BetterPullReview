#include "epoch_host_implementation.hpp"

#include "bpr/application/git_process_port.hpp"
#include "bpr/application/operation_diagnostics.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/generated_files.hpp"
#include "bpr/core/operation_diagnosis.hpp"

#include <initializer_list>
#include <string_view>

namespace bpr::application {
	// [[BPR-0654]]. See `epoch_application_host::resolve_changed_paths`.
	auto epoch_application_host::implementation::resolve_changed_paths(
		changed_paths_request    aRequest,
		changed_paths_completion aCompletion) -> bool
	{
		if (!aCompletion || !aRequest.Left.valid() || aRequest.Left == aRequest.Right
			|| aRequest.Left.algorithm() != aRequest.Right.algorithm()) {
			return false;
		}
		auto reviewActor = review_cache_actor(aRequest.WorkbenchAuthority, aRequest.Right);
		if (!reviewActor) {
			return false;
		}
		auto       actor    = std::move(reviewActor->first);
		const auto expected = std::move(reviewActor->second);
		auto       admitted = admit_job(
			std::nullopt, std::nullopt, std::nullopt, std::nullopt, false);
		if (!admitted.has_value()) {
			return false;
		}
		auto        control      = std::move(*admitted);
		const auto  gate         = std::weak_ptr<completion_gate>{ CompletionGate };
		auto* const decisionLane = &DecisionLane;
		try {
			enqueue_actor_work(
				actor,
				[this,
				 actor,
				 expected,
				 gate,
				 decisionLane,
				 control,
				 request    = std::move(aRequest),
				 completion = std::move(aCompletion)]() mutable {
					auto observation = changed_paths_observation{
						.Request = request,
						.Status  = changed_paths_status::failure,
					};
					const auto workStop = control->Stop.get_token();
					const auto statusOf = [](const git_cache_error& aError) {
						return aError.Code == git_cache_failure::cancelled
							? changed_paths_status::cancelled
							: changed_paths_status::failure;
					};
					if (workStop.stop_requested()
						|| !Consumer->active() || BlobIngressClosing.load(std::memory_order_acquire)) {
						observation.Status = changed_paths_status::cancelled;
					} else if (!open_actor_cache(*actor, Caches, expected, workStop)) {
						observation.Status = changed_paths_status::stale;
					} else if (const auto inspected
							   = actor->Cache->inspect_commit_object(request.Left, workStop);
							   !inspected.has_value()) {
						observation.Status = statusOf(inspected.error());
					} else if (!application::valid(*inspected)
							   || inspected->Commit != request.Left) {
						observation.Status = changed_paths_status::failure;
					} else {
						observation.Status = changed_paths_status::resolved;
						for (const auto& path : request.Paths) {
							const auto left = actor->Cache->find_blob_at_path(
								git_cache_tree_entry_request{ .Commit = request.Left, .Path = path },
								workStop);
							if (!left.has_value()) {
								observation.Status = statusOf(left.error());
								observation.Changed.clear();
								break;
							}
							const auto right = actor->Cache->find_blob_at_path(
								git_cache_tree_entry_request{ .Commit = request.Right, .Path = path },
								workStop);
							if (!right.has_value()) {
								observation.Status = statusOf(right.error());
								observation.Changed.clear();
								break;
							}
							if (*left != *right) {
								observation.Changed.push_back(path);
							}
						}
					}
					enqueue_completion(
						gate,
						decisionLane,
						std::move(control),
						[answer  = std::move(observation),
						 deliver = std::move(completion)](implementation&) {
							deliver(answer);
						});
				});
		} catch (...) {
			finish_job(control);
			return false;
		}
		return true;
	}

	/// \brief RFC-BPR-0089. See `epoch_application_host::resolve_commit_filter_touches`.
	auto epoch_application_host::implementation::resolve_commit_filter_touches(
		commit_filter_touches_request    aRequest,
		commit_filter_touches_completion aCompletion) -> bool
	{
		if (!aCompletion || aRequest.Steps.empty() || aRequest.Steps.size() > maximum_commit_filter_touch_steps
			|| !std::ranges::all_of(aRequest.Steps, [&aRequest](const commit_filter_touch_step& aStep) {
				   return aStep.Parent.valid() && aStep.Commit.valid() && aStep.Parent != aStep.Commit
					   && aStep.Parent.algorithm() == aRequest.Head.algorithm() && aStep.Commit.algorithm() == aRequest.Head.algorithm();
			   })) {
			return false;
		}
		auto reviewActor = review_cache_actor(aRequest.WorkbenchAuthority, aRequest.Head);
		if (!reviewActor) {
			return false;
		}
		auto       actor    = std::move(reviewActor->first);
		const auto expected = std::move(reviewActor->second);
		// The review's own lease, for a sparse review's chain acquisition.
		auto reviewContext = Active && Active->Actor == actor && Active->Revision.Head == aRequest.Head ? Active : std::shared_ptr<authority_context>{ };
		auto admitted      = admit_job(
			std::nullopt, std::nullopt, std::nullopt, std::nullopt, false);
		if (!admitted.has_value()) {
			return false;
		}
		auto        control      = std::move(*admitted);
		const auto  gate         = std::weak_ptr<completion_gate>{ CompletionGate };
		auto* const decisionLane = &DecisionLane;
		try {
			enqueue_actor_work(
				actor,
				[this,
				 actor,
				 expected,
				 context = std::move(reviewContext),
				 gate,
				 decisionLane,
				 control,
				 request    = std::move(aRequest),
				 completion = std::move(aCompletion)]() mutable {
					auto observation = commit_filter_touches_observation{
						.Request = request,
						.Status  = commit_filter_touches_status::failure,
					};
					observation.Status = commit_filter_touches(*actor, expected, context, request, control->Stop.get_token(), observation.Touches);
					if (observation.Status != commit_filter_touches_status::resolved) {
						observation.Touches.clear();
					} else {
						// The commits a filtered diff of this review may now be
						// read at: each diffed commit and its parent.
						auto commits = std::vector<core::git_commit_oid>{ };
						commits.reserve(2U * request.Steps.size());
						for (const auto& step : request.Steps) {
							commits.push_back(step.Parent);
							commits.push_back(step.Commit);
						}
						std::ranges::sort(commits);
						commits.erase(std::unique(commits.begin(), commits.end()), commits.end());
						std::scoped_lock lock{ actor->BlobAuthorityMutex };
						actor->FilterAuthority = request.WorkbenchAuthority;
						actor->FilterCommits   = std::move(commits);
					}
					enqueue_completion(
						gate,
						decisionLane,
						std::move(control),
						[answer  = std::move(observation),
						 deliver = std::move(completion)](implementation&) {
							deliver(answer);
						});
				});
		} catch (...) {
			finish_job(control);
			return false;
		}
		return true;
	}

	/// \brief RFC-BPR-0089. Each step's first-parent tree diff, on the actor.
	///
	/// \details Every commit and parent must be resident with its root tree
	/// (a sparse review holds only its epochs' commits until the chain is
	/// acquired); otherwise the answer is `objects_unavailable` and nothing
	/// is diffed. The tree diff is the cache's own bounded one, with exact
	/// rename and copy detection, so it needs no blob.
	auto epoch_application_host::implementation::commit_filter_touches(authority_effect_actor& aActor, const git_cache_open_request& aExpected,
																	   const std::shared_ptr<authority_context>& aContext, const commit_filter_touches_request& aRequest,
																	   const std::stop_token aStop, std::vector<commit_filter_commit_touch>& aTouches) -> commit_filter_touches_status
	{
		const auto statusOf = [](const git_cache_error& aError) {
			return aError.Code == git_cache_failure::cancelled     ? commit_filter_touches_status::cancelled
				: aError.Code == git_cache_failure::quota_exceeded ? commit_filter_touches_status::limit_exceeded
																   : commit_filter_touches_status::failure;
		};
		if (aStop.stop_requested() || !Consumer->active() || BlobIngressClosing.load(std::memory_order_acquire)) {
			return commit_filter_touches_status::cancelled;
		}
		if (!open_actor_cache(aActor, Caches, aExpected, aStop)) {
			return commit_filter_touches_status::stale;
		}
		const auto resident = [&](const core::git_commit_oid& aCommit) -> std::optional<commit_filter_touches_status> {
			if (aStop.stop_requested()) {
				return commit_filter_touches_status::cancelled;
			}
			// The commit and its root tree are what a tree diff reads; a
			// SHA-1 cache answers that without walking the closure, which
			// across a long chain would repeat the whole history per commit.
			// Only a refusal there pays for the closure check.
			auto inspected = aExpected.ObjectFormat == core::git_hash_algorithm::sha1
				? aActor.Cache->inspect_sparse_commit_object(aCommit, aStop)
				: git_cache_result<git_cache_commit_inspection>{ std::unexpected{ git_cache_failure::invalid_request } };
			if (!inspected.has_value() && inspected.error() != git_cache_failure::cancelled) {
				inspected = aActor.Cache->inspect_commit_object(aCommit, aStop);
			}
			if (inspected.has_value()) {
				return application::valid(*inspected) && inspected->Commit == aCommit ? std::nullopt
																					  : std::optional{ commit_filter_touches_status::failure };
			}
			return inspected.error() == git_cache_failure::cancelled || aStop.stop_requested() ? commit_filter_touches_status::cancelled
				: inspected.error() == git_cache_failure::object_mismatch                      ? commit_filter_touches_status::objects_unavailable
																							   : commit_filter_touches_status::failure;
		};
		auto missing = false;
		for (const auto& step : aRequest.Steps) {
			for (const auto& commit : { step.Parent, step.Commit }) {
				if (const auto refused = resident(commit)) {
					if (*refused != commit_filter_touches_status::objects_unavailable) {
						return *refused;
					}
					missing = true;
				}
			}
		}
		// A sparse review holds only its epochs' commits. The chain is
		// acquired once into a transient quarantined store, under the
		// review's own lease and the sparse acquisition's bounds.
		auto store = std::shared_ptr<git_chain_tree_store>{ };
		if (missing) {
			auto acquired = acquire_chain_trees(aContext, aRequest, aStop);
			if (!acquired) {
				return acquired.error();
			}
			store = std::move(*acquired);
		}
		struct touch_side final
		{
			std::string        Path;
			core::git_blob_oid Blob;
		};
		struct touch_change final
		{
			std::optional<touch_side> Left;
			std::optional<touch_side> Right;
			char                      Change = 'M';
		};
		const auto diff = [&](const commit_filter_touch_step& aStep) -> std::expected<std::vector<touch_change>, commit_filter_touches_status> {
			auto changes = std::vector<touch_change>{ };
			if (store) {
				auto read = store->changed_entries(aStep.Parent, aStep.Commit, aStop);
				if (!read) {
					return std::unexpected{ read.error().Failure == git_smart_transport_failure::cancelled ? commit_filter_touches_status::cancelled
																										   : commit_filter_touches_status::failure };
				}
				for (auto& entry : *read) {
					changes.push_back({ .Left   = entry.Left ? std::optional{ touch_side{ .Path = std::move(entry.Left->Path), .Blob = entry.Left->Object } } : std::nullopt,
										.Right  = entry.Right ? std::optional{ touch_side{ .Path = std::move(entry.Right->Path), .Blob = entry.Right->Object } } : std::nullopt,
										.Change = entry.Change });
				}
				return changes;
			}
			auto read = aActor.Cache->changed_entries({ .Left = aStep.Parent, .Right = aStep.Commit }, aStop);
			if (!read.has_value()) {
				return std::unexpected{ statusOf(read.error()) };
			}
			for (auto& entry : *read) {
				changes.push_back({ .Left   = entry.Left ? std::optional{ touch_side{ .Path = std::move(entry.Left->Path), .Blob = entry.Left->Object } } : std::nullopt,
									.Right  = entry.Right ? std::optional{ touch_side{ .Path = std::move(entry.Right->Path), .Blob = entry.Right->Object } } : std::nullopt,
									.Change = entry.Change });
			}
			return changes;
		};
		aTouches.reserve(aRequest.Steps.size());
		for (const auto& step : aRequest.Steps) {
			const auto changes = diff(step);
			if (!changes.has_value()) {
				return changes.error();
			}
			if (changes->size() >= maximum_git_changed_entries) {
				return commit_filter_touches_status::limit_exceeded;
			}
			auto touch   = commit_filter_commit_touch{ };
			touch.Commit = step.Commit;
			touch.Paths.reserve(changes->size());
			for (const auto& change : *changes) {
				const auto& side = change.Right ? change.Right : change.Left;
				if (!side || side->Path.empty() || side->Path.size() > maximum_git_tree_entry_path_bytes) {
					return commit_filter_touches_status::failure;
				}
				auto path   = commit_filter_touched_path{ };
				path.Path   = side->Path;
				path.Change = change.Change;
				// RFC-BPR-0089 M3. Each side's blob, for the same-change rematch.
				if (change.Left) {
					path.LeftBlob = change.Left->Blob;
				}
				if (change.Right) {
					path.RightBlob = change.Right->Blob;
				}
				if ((change.Change == 'R' || change.Change == 'C') && change.Left && change.Right) {
					path.PreviousPath = change.Left->Path;
				}
				touch.Paths.push_back(std::move(path));
			}
			std::ranges::sort(touch.Paths, { }, &commit_filter_touched_path::Path);
			if (std::ranges::adjacent_find(touch.Paths, { }, &commit_filter_touched_path::Path) != touch.Paths.end()) {
				return commit_filter_touches_status::failure;
			}
			aTouches.push_back(std::move(touch));
		}
		if (aStop.stop_requested()) {
			return commit_filter_touches_status::cancelled;
		}
		if (store) {
			std::scoped_lock lock{ aActor.BlobAuthorityMutex };
			aActor.ChainTrees = std::move(store);
		}
		return commit_filter_touches_status::resolved;
	}

	/// \brief RFC-BPR-0089. Acquires the chain's trees for a sparse review
	/// through its own lease, or says why it cannot.
	auto epoch_application_host::implementation::acquire_chain_trees(const std::shared_ptr<authority_context>& aContext, const commit_filter_touches_request& aRequest,
																	 const std::stop_token aStop)
		-> std::expected<std::shared_ptr<git_chain_tree_store>, commit_filter_touches_status>
	{
		const auto sparse = aContext && object_loading_for(aContext->Handoff.Shape) == pull_request_object_loading::selected_objects
			&& aContext->Handoff.SmartGit && !aContext->Handoff.SmartGit->revoked() && aContext->Handoff.SmartGit->supports_sparse_commit_trees();
		if (!sparse || aRequest.Head != aContext->Revision.Head) {
			return std::unexpected{ commit_filter_touches_status::objects_unavailable };
		}
		const auto context = git_smart_request_context{
			.ApplicationSession = aContext->Handoff.ApplicationSession,
			.Authority          = aContext->Authority,
			.Revision           = aContext->Revision,
			.Repository         = aContext->Handoff.Locator,
			.BaseRef            = aContext->Handoff.BaseRef,
			.AcquisitionMode    = git_smart_acquisition_mode::sparse_commit_trees,
		};
		auto chain  = git_smart_chain_request{ .Context = context, .Head = aRequest.Head, .Depth = 0U, .Commits = { } };
		chain.Depth = static_cast<std::uint32_t>(aRequest.Steps.size() + 1U);
		for (const auto& step : aRequest.Steps) {
			chain.Commits.push_back(step.Parent);
			chain.Commits.push_back(step.Commit);
		}
		std::ranges::sort(chain.Commits);
		chain.Commits.erase(std::unique(chain.Commits.begin(), chain.Commits.end()), chain.Commits.end());
		if (!application::valid(chain)) {
			return std::unexpected{ commit_filter_touches_status::objects_unavailable };
		}
		auto transport = aContext->Handoff.SmartGit->acquire(context);
		if (!transport || !*transport) {
			return std::unexpected{ commit_filter_touches_status::objects_unavailable };
		}
		auto store = (*transport)->acquire_chain_trees(chain, aStop);
		if (!store || !*store) {
			const auto failure = store ? git_smart_transport_failure::unavailable : store.error().Failure;
			return std::unexpected{ failure == git_smart_transport_failure::cancelled            ? commit_filter_touches_status::cancelled
										: failure == git_smart_transport_failure::response_limit ? commit_filter_touches_status::limit_exceeded
																								 : commit_filter_touches_status::objects_unavailable };
		}
		for (const auto& commit : chain.Commits) {
			if (!(*store)->holds(commit)) {
				return std::unexpected{ commit_filter_touches_status::objects_unavailable };
			}
		}
		return std::move(*store);
	}

	/// \brief Whether the review loads only selected objects through a lease
	/// that can acquire them: its cache holds commits and trees, no blobs.
	auto epoch_application_host::implementation::sparse_review(const authority_context& aContext) -> bool
	{
		return object_loading_for(aContext.Handoff.Shape) == pull_request_object_loading::selected_objects && aContext.Handoff.SmartGit
			&& !aContext.Handoff.SmartGit->revoked() && aContext.Handoff.SmartGit->supports_sparse_commit_trees();
	}

	/// \brief [[BPR-1873]]. Says, in a reviewer's terms, why what the
	/// linked clone lacks of the head could not be fetched, and keeps the
	/// transport's own words for the log. A size wall is always `too_large`,
	/// naming the pull request's new objects, never "could not be read".
	auto epoch_application_host::implementation::delta_failure(const git_smart_transport_error& aError) -> agent_workspace_error
	{
		using failure = git_smart_transport_failure;
		auto reason   = agent_workspace_unavailable_reason::download_failed;
		auto cause    = std::string_view{ "they could not be read" };
		switch (aError.Failure) {
		case failure::cancelled:
			return agent_workspace_error{ agent_workspace_failure::cancelled };
		case failure::authentication:
		case failure::permission:
			reason = agent_workspace_unavailable_reason::sign_in_refused;
			cause  = "GitHub refused the signed-in account";
			break;
		case failure::network:
		case failure::timeout:
		case failure::certificate:
		case failure::proxy:
		case failure::redirect:
			reason = agent_workspace_unavailable_reason::connection_failed;
			cause  = "the connection to GitHub failed";
			break;
		case failure::request_limit:
		case failure::response_limit:
			reason = agent_workspace_unavailable_reason::too_large;
			cause  = "the pull request's new objects are larger than the fetch allows";
			break;
		case failure::invalid_request:
		case failure::unavailable:
		case failure::malformed_response:
		case failure::consumer_rejected:
		case failure::collision_detected:
			break;
		}
		auto detail = std::string{ "What the linked clone lacks of the head was not fetched for the agent: " };
		detail.append(cause).append(" (transport failure ").append(std::to_string(static_cast<unsigned>(aError.Failure)));
		detail.append(", step ").append(std::to_string(static_cast<unsigned>(aError.Step))).append(").\n");
		return agent_workspace_error{ agent_workspace_failure::unavailable, std::move(detail), reason };
	}

	/// \brief [[BPR-1873]]. Fetches what the linked clone lacks of a sparse
	/// review's head for an agent workspace, through the review's own
	/// lease, or says why not.
	///
	/// \details Only a commit this review's cache already holds is fetched,
	/// so the workspace can name no commit the review did not accept. The
	/// clone is read and never written.
	auto epoch_application_host::implementation::acquire_workspace_delta(const std::shared_ptr<authority_effect_actor>& aActor, const authority_context& aContext,
																		 const git_cache_open_request& aExpected, const core::git_commit_oid& aHead,
																		 const std::filesystem::path& aClone, const std::stop_token aStop)
		-> std::expected<std::shared_ptr<git_workspace_delta>, agent_workspace_error>
	{
		const auto held = run_on_actor<bool>(
			aActor, aExpected,
			[aActor, aHead](const std::stop_token aWorkStop) {
				const auto inspected = aActor->Cache->inspect_sparse_commit_object(aHead, aWorkStop);
				return inspected.has_value() && application::valid(*inspected) && inspected->Commit == aHead;
			},
			aStop);
		if (aStop.stop_requested()) {
			return std::unexpected{ agent_workspace_error{ agent_workspace_failure::cancelled } };
		}
		if (!held || !*held) {
			return std::unexpected{ agent_workspace_error{ agent_workspace_failure::unavailable, "The review's copy no longer holds the head the agent asked about.\n",
														   agent_workspace_unavailable_reason::copy_failed } };
		}
		const auto context = git_smart_request_context{
			.ApplicationSession = aContext.Handoff.ApplicationSession,
			.Authority          = aContext.Authority,
			.Revision           = aContext.Revision,
			.Repository         = aContext.Handoff.Locator,
			.BaseRef            = aContext.Handoff.BaseRef,
			.AcquisitionMode    = git_smart_acquisition_mode::sparse_commit_trees,
		};
		const auto request = git_smart_workspace_delta_request{ .Context = context, .Head = aHead, .Clone = aClone };
		if (!application::valid(request)) {
			return std::unexpected{ delta_failure(git_smart_transport_error{ git_smart_transport_failure::invalid_request }) };
		}
		auto transport = aContext.Handoff.SmartGit->acquire(context);
		if (!transport || !*transport) {
			return std::unexpected{ delta_failure(transport ? git_smart_transport_error{ } : transport.error()) };
		}
		auto delta = (*transport)->acquire_workspace_delta(request, aStop);
		if (!delta) {
			return std::unexpected{ delta_failure(delta.error()) };
		}
		if (!*delta || (*delta)->head() != aHead) {
			return std::unexpected{ delta_failure(git_smart_transport_error{ git_smart_transport_failure::malformed_response }) };
		}
		return std::move(*delta);
	}

	/// \brief [[BPR-1884]]. Fetches the pull request's head and base
	/// into the clone BetterPullReview keeps, when it lacks them, through
	/// the review's own lease, off the actor; records the outcome.
	///
	/// \details Never inside the agent's sandbox or with its environment:
	/// it runs here, before the workspace exists, on the transport's own
	/// pinned configuration. The transport refuses any folder outside the
	/// profile's folder of kept clones.
	auto epoch_application_host::implementation::refresh_managed_clone(const authority_context& aContext, const std::filesystem::path& aClone, const std::stop_token aStop) -> bool
	{
		const auto watch = operation_stopwatch{ };
		record_operation(DiagnosticLog.get(), { .Action = core::operation_action::clone_refresh }, true);
		const auto context = git_smart_request_context{
			.ApplicationSession = aContext.Handoff.ApplicationSession,
			.Authority          = aContext.Authority,
			.Revision           = aContext.Revision,
			.Repository         = aContext.Handoff.Locator,
			.BaseRef            = aContext.Handoff.BaseRef,
			.AcquisitionMode    = git_smart_acquisition_mode::sparse_commit_trees,
		};
		const auto request = git_smart_managed_clone_refresh_request{ .Context = context, .Clone = aClone };
		auto       outcome = git_smart_transport_result<void>{ std::unexpected{ git_smart_transport_error{ git_smart_transport_failure::invalid_request } } };
		if (application::valid(request) && aContext.Handoff.SmartGit) {
			auto transport = aContext.Handoff.SmartGit->acquire(context);
			outcome        = !transport ? git_smart_transport_result<void>{ std::unexpected{ transport.error() } }
				: !*transport           ? git_smart_transport_result<void>{ std::unexpected{ git_smart_transport_error{ } } }
										: (*transport)->refresh_managed_clone(request, aStop);
		}
		const auto failure = outcome                                                                      ? core::operation_failure::none
			: outcome.error().Failure == git_smart_transport_failure::cancelled || aStop.stop_requested() ? core::operation_failure::cancelled
																										  : core::operation_failure::refresh_failed;
		record_operation(DiagnosticLog.get(), { .Action = core::operation_action::clone_refresh, .Failure = failure, .DurationMilliseconds = watch.milliseconds() });
		return outcome.has_value();
	}

	/// \brief [[BPR-1879]]. Makes the clone BetterPullReview keeps for a
	/// sparse review's repository, through the review's own lease.
	///
	/// \details Only for the review whose authority and session the request
	/// names, and only when it was read sparsely through a `gh` lease that
	/// can: that is the lease whose credential and pinned private Git
	/// configuration every sparse acquisition already uses. Runs off the
	/// repository's actor; the cache is never touched.
	auto epoch_application_host::implementation::keep_managed_clone(const managed_clone_request& aRequest, const git_smart_clone_progress& aProgress, const std::stop_token aStop)
		-> std::expected<void, managed_clone_error>
	{
		if (!application::valid(aRequest)) {
			return std::unexpected{ managed_clone_error{ managed_clone_failure::unavailable, "The request does not name a review.\n" } };
		}
		if (!begin_blob_call()) {
			return std::unexpected{ managed_clone_error{ managed_clone_failure::cancelled, { } } };
		}
		struct clone_call_guard final
		{
			implementation& Host;

			~clone_call_guard()
			{
				Host.finish_blob_call();
			}
		} call{ *this };
		const auto key = cache_actor_key{
			.Profile         = aRequest.Authority.Profile,
			.ProfileRevision = aRequest.Authority.ProfileRevision,
			.Host            = aRequest.Authority.Host,
			.HostAuthority   = aRequest.Authority.HostAuthority,
			.Account         = aRequest.Authority.Account,
			.Repository      = aRequest.Authority.Repository,
			.ObjectFormat    = aRequest.Head.algorithm(),
			.Mode            = aRequest.Authority.Mode,
		};
		auto actor = std::shared_ptr<authority_effect_actor>{ };
		{
			std::scoped_lock lock{ CacheActorsMutex };
			const auto       found = std::ranges::find(CacheActors, key, &decltype(CacheActors)::value_type::first);
			if (found != CacheActors.end()) {
				actor = found->second;
			}
		}
		auto context = std::shared_ptr<authority_context>{ };
		if (actor) {
			std::scoped_lock lock{ actor->BlobAuthorityMutex };
			context = actor->BlobContext.lock();
		}
		const auto authorization = blob_authority{ .ApplicationSession = aRequest.ApplicationSession, .Authority = aRequest.Authority };
		if (!context || blob_authority_for(*context) != authorization || !sparse_review(*context)) {
			return std::unexpected{ managed_clone_error{ managed_clone_failure::unavailable, "The review on screen cannot keep a clone: it is not read through GitHub CLI's sparse reads.\n" } };
		}
		const auto transportContext = git_smart_request_context{
			.ApplicationSession = context->Handoff.ApplicationSession,
			.Authority          = context->Authority,
			.Revision           = context->Revision,
			.Repository         = context->Handoff.Locator,
			.BaseRef            = context->Handoff.BaseRef,
			.AcquisitionMode    = git_smart_acquisition_mode::sparse_commit_trees,
		};
		const auto request   = git_smart_managed_clone_request{ .Context = transportContext, .Destination = aRequest.Destination };
		auto       transport = context->Handoff.SmartGit->acquire(transportContext);
		if (!transport || !*transport) {
			return std::unexpected{ managed_clone_error{ managed_clone_failure::unavailable, "GitHub CLI's Git transport could not be acquired for this review.\n" } };
		}
		auto made = (*transport)->acquire_managed_clone(request, aProgress, aStop);
		if (made) {
			return { };
		}
		using failure = git_smart_transport_failure;
		auto error    = managed_clone_error{ };
		switch (made.error().Failure) {
		case failure::cancelled:
			error.Failure = managed_clone_failure::cancelled;
			break;
		case failure::authentication:
		case failure::permission:
			error.Failure = managed_clone_failure::sign_in_refused;
			break;
		case failure::network:
		case failure::timeout:
		case failure::certificate:
		case failure::proxy:
		case failure::redirect:
			error.Failure = managed_clone_failure::connection_failed;
			break;
		case failure::request_limit:
		case failure::response_limit:
			error.Failure = managed_clone_failure::too_large;
			break;
		default:
			error.Failure = aStop.stop_requested() ? managed_clone_failure::cancelled : managed_clone_failure::failed;
			break;
		}
		error.Detail = "The clone BetterPullReview keeps was not made (transport failure " + std::to_string(static_cast<unsigned>(made.error().Failure)) + ", step "
			+ std::to_string(static_cast<unsigned>(made.error().Step)) + ").\n";
		return std::unexpected{ std::move(error) };
	}

	/// \brief [[BPR-1873]]. Reads the pull request's files at the head for a
	/// sparse review with no clone, so its agent sees only those (owner,
	/// 2026-11-05: "the agent runs on the PR only, and says so").
	///
	/// \details The files are the head's side of the base...head change, as
	/// the review lists them; deletions and submodules have none. Bytes the
	/// review already holds are used; the rest are read through the
	/// review's own lease, in batches and then one at a time, and every one
	/// is checked against its blob before it is used.
	auto epoch_application_host::implementation::read_pull_request_files(const std::shared_ptr<authority_effect_actor>& aActor, const authority_context& aContext,
																		 const git_cache_open_request& aExpected, const core::agent_revision_binding& aBinding,
																		 const std::stop_token aStop) -> std::expected<std::vector<git_workspace_file>, agent_workspace_error>
	{
		const auto unavailable = [](std::string aDetail) {
			return std::unexpected{ agent_workspace_error{ agent_workspace_failure::unavailable, std::move(aDetail), agent_workspace_unavailable_reason::copy_failed } };
		};
		using listed = std::expected<std::vector<git_workspace_file>, git_cache_failure>;
		auto files   = run_on_actor<listed>(
			aActor, aExpected,
			[aActor, aBinding](const std::stop_token aWorkStop) -> listed {
				auto changes = aActor->Cache->changed_entries({ .Left = aBinding.BaseOid, .Right = aBinding.HeadOid }, aWorkStop);
				if (!changes) {
					return std::unexpected{ changes.error().Code };
				}
				auto result = std::vector<git_workspace_file>{ };
				for (const auto& change : *changes) {
					if (change.Right && (change.Right->Mode == 0100644U || change.Right->Mode == 0100755U || change.Right->Mode == 0120000U)) {
						result.push_back(git_workspace_file{ .Path = change.Right->Path, .Mode = change.Right->Mode, .Blob = change.Right->Object, .Bytes = { } });
					}
				}
				std::scoped_lock lock{ aActor->BlobAuthorityMutex };
				for (auto& file : result) {
					const auto retained = std::ranges::find(aActor->RetainedBlobs, file.Blob, &retained_sparse_blob::Blob);
					if (retained != aActor->RetainedBlobs.end()) {
						file.Bytes = retained->Bytes;
					}
				}
				return result;
			},
			aStop);
		if (aStop.stop_requested()) {
			return std::unexpected{ agent_workspace_error{ agent_workspace_failure::cancelled } };
		}
		if (!files || !*files) {
			return unavailable("The pull request's files could not be listed from the review's copy.\n");
		}
		auto result = std::move(**files);
		if (result.size() > maximum_pull_request_workspace_files) {
			return std::unexpected{ agent_workspace_error{ agent_workspace_failure::unavailable,
														   "The pull request changes more files than the agent's private copy holds.\n",
														   agent_workspace_unavailable_reason::copy_failed } };
		}
		const auto smartContext = git_smart_request_context{
			.ApplicationSession = aContext.Handoff.ApplicationSession,
			.Authority          = aContext.Authority,
			.Revision           = aContext.Revision,
			.Repository         = aContext.Handoff.Locator,
			.BaseRef            = aContext.Handoff.BaseRef,
		};
		const auto missing = [&result] {
			auto indices = std::vector<std::size_t>{ };
			for (std::size_t index = 0U; index < result.size(); ++index) {
				if (result[index].Bytes.empty() && blob_matches(result[index].Blob, { }) != blob_check::matches) {
					indices.push_back(index);
				}
			}
			return indices;
		};
		auto wanted = missing();
		if (!wanted.empty()) {
			auto transport = aContext.Handoff.SmartGit->acquire(smartContext);
			if (!transport || !*transport) {
				return std::unexpected{ pull_request_files_failure(transport ? git_smart_transport_error{ } : transport.error()) };
			}
			// Batches first: a transport that cannot batch answers none, and
			// every file is then read on its own.
			for (std::size_t offset = 0U; offset < wanted.size() && !aStop.stop_requested(); offset += maximum_git_exact_blob_batch) {
				const auto count   = std::min(maximum_git_exact_blob_batch, wanted.size() - offset);
				auto       request = git_smart_exact_blobs_request{ .Context = smartContext, .ExpectedBlobs = { }, .MaximumBytes = maximum_git_exact_blob_batch_bytes };
				for (std::size_t index = 0U; index < count; ++index) {
					request.ExpectedBlobs.push_back(result[wanted[offset + index]].Blob);
				}
				const auto read = (*transport)->read_exact_blobs(request, aStop);
				if (!read || read->size() != count) {
					break;
				}
				for (std::size_t index = 0U; index < count; ++index) {
					if (const auto& answer = (*read)[index]; answer) {
						auto bytes = std::vector<std::uint8_t>{ };
						bytes.reserve(answer->size());
						for (const auto byte : *answer) {
							bytes.push_back(std::to_integer<std::uint8_t>(byte));
						}
						if (blob_matches(request.ExpectedBlobs[index], bytes) == blob_check::matches) {
							result[wanted[offset + index]].Bytes = std::move(bytes);
						}
					}
				}
			}
			auto total = std::size_t{ 0U };
			for (const auto index : missing()) {
				if (aStop.stop_requested()) {
					return std::unexpected{ agent_workspace_error{ agent_workspace_failure::cancelled } };
				}
				auto read = (*transport)->read_exact_blob(git_smart_exact_blob_request{ .Context = smartContext, .ExpectedBlob = result[index].Blob, .MaximumBytes = maximum_git_exact_blob_bytes }, aStop);
				if (!read) {
					return std::unexpected{ pull_request_files_failure(read.error()) };
				}
				auto bytes = std::vector<std::uint8_t>{ };
				bytes.reserve(read->size());
				for (const auto byte : *read) {
					bytes.push_back(std::to_integer<std::uint8_t>(byte));
				}
				if (blob_matches(result[index].Blob, bytes) != blob_check::matches) {
					return std::unexpected{ pull_request_files_failure(git_smart_transport_error{ git_smart_transport_failure::malformed_response }) };
				}
				result[index].Bytes = std::move(bytes);
			}
			for (const auto& file : result) {
				total += file.Bytes.size();
			}
			if (total > maximum_pull_request_workspace_bytes) {
				return std::unexpected{ agent_workspace_error{ agent_workspace_failure::unavailable,
															   "The pull request's files are larger than the agent's private copy holds.\n",
															   agent_workspace_unavailable_reason::copy_failed } };
			}
		}
		if (aStop.stop_requested()) {
			return std::unexpected{ agent_workspace_error{ agent_workspace_failure::cancelled } };
		}
		return result;
	}

	/// \brief [[BPR-1873]]. Why the pull request's files could not be read:
	/// the agent's copy could not be made, with the transport's words for
	/// the log. The delta's reasons speak of a clone, which there is none of.
	auto epoch_application_host::implementation::pull_request_files_failure(const git_smart_transport_error& aError) -> agent_workspace_error
	{
		if (aError.Failure == git_smart_transport_failure::cancelled) {
			return agent_workspace_error{ agent_workspace_failure::cancelled };
		}
		return agent_workspace_error{ agent_workspace_failure::unavailable,
									  "The pull request's files were not read for the agent (transport failure " + std::to_string(static_cast<unsigned>(aError.Failure))
										  + ", step " + std::to_string(static_cast<unsigned>(aError.Step)) + ").\n",
									  agent_workspace_unavailable_reason::copy_failed };
	}

	/// \brief RFC-BPR-0089 M2. See `epoch_application_host::resolve_generated_attributes`.
	auto epoch_application_host::implementation::resolve_generated_attributes(
		generated_attributes_request    aRequest,
		generated_attributes_completion aCompletion) -> bool
	{
		if (!aCompletion || aRequest.Directories.empty() || aRequest.Directories.size() > maximum_generated_attribute_directories
			|| !aRequest.Directories.front().empty() || !std::ranges::is_sorted(aRequest.Directories)
			|| std::ranges::adjacent_find(aRequest.Directories) != aRequest.Directories.end()
			|| std::ranges::any_of(aRequest.Directories, [](const std::string& aDirectory) {
				   return aDirectory.size() + 16U > maximum_git_tree_entry_path_bytes || aDirectory.starts_with('/') || aDirectory.ends_with('/');
			   })) {
			return false;
		}
		auto reviewActor = review_cache_actor(aRequest.WorkbenchAuthority, aRequest.Head);
		if (!reviewActor) {
			return false;
		}
		auto       actor    = std::move(reviewActor->first);
		const auto expected = std::move(reviewActor->second);
		// The review's own lease, for a sparse review's exact blob reads.
		auto reviewContext = Active && Active->Actor == actor && Active->Revision.Head == aRequest.Head ? Active : std::shared_ptr<authority_context>{ };
		auto admitted      = admit_job(
			std::nullopt, std::nullopt, std::nullopt, std::nullopt, false);
		if (!admitted.has_value()) {
			return false;
		}
		auto        control      = std::move(*admitted);
		const auto  gate         = std::weak_ptr<completion_gate>{ CompletionGate };
		auto* const decisionLane = &DecisionLane;
		try {
			enqueue_actor_work(
				actor,
				[this,
				 actor,
				 expected,
				 context = std::move(reviewContext),
				 gate,
				 decisionLane,
				 control,
				 request    = std::move(aRequest),
				 completion = std::move(aCompletion)]() mutable {
					auto observation = generated_attributes_observation{
						.Request  = request,
						.Status   = generated_attributes_status::failure,
						.Files    = { },
						.Exceeded = std::nullopt,
					};
					observation.Status = generated_attributes(*actor, expected, context, request, control->Stop.get_token(), observation);
					if (observation.Status != generated_attributes_status::resolved) {
						observation.Files.clear();
					}
					enqueue_completion(
						gate,
						decisionLane,
						std::move(control),
						[answer  = std::move(observation),
						 deliver = std::move(completion)](implementation&) {
							deliver(answer);
						});
				});
		} catch (...) {
			finish_job(control);
			return false;
		}
		return true;
	}

	/// \brief RFC-BPR-0089 M2. The head's `.gitattributes` blobs, on the actor.
	///
	/// \details Each directory's file is looked up at head by literal
	/// path, in the retained cache or, for a sparse review whose head
	/// trees only its chain store holds, there. A blob the cache does not
	/// hold is read exactly through the review's own lease when the review
	/// is sparse, and checked against its OID. More files than the core
	/// reads, or a larger one, stop the read and say so.
	auto epoch_application_host::implementation::generated_attributes(authority_effect_actor& aActor, const git_cache_open_request& aExpected,
																	  const std::shared_ptr<authority_context>& aContext, const generated_attributes_request& aRequest,
																	  const std::stop_token aStop, generated_attributes_observation& aObservation) -> generated_attributes_status
	{
		const auto statusOf = [](const git_cache_failure aFailure) {
			return aFailure == git_cache_failure::cancelled      ? generated_attributes_status::cancelled
				: aFailure == git_cache_failure::object_mismatch ? generated_attributes_status::objects_unavailable
																 : generated_attributes_status::failure;
		};
		if (aStop.stop_requested() || !Consumer->active() || BlobIngressClosing.load(std::memory_order_acquire)) {
			return generated_attributes_status::cancelled;
		}
		if (!open_actor_cache(aActor, Caches, aExpected, aStop)) {
			return generated_attributes_status::stale;
		}
		const auto chainTrees = [&] {
			std::scoped_lock lock{ aActor.BlobAuthorityMutex };
			return aActor.ChainTrees;
		}();
		const auto sparse = aContext && object_loading_for(aContext->Handoff.Shape) == pull_request_object_loading::selected_objects
			&& aContext->Handoff.SmartGit && !aContext->Handoff.SmartGit->revoked() && aContext->Handoff.SmartGit->supports_sparse_commit_trees()
			&& aRequest.Head == aContext->Revision.Head;
		const auto lookup = [&](const std::string& aPath) -> std::expected<std::optional<core::git_blob_oid>, generated_attributes_status> {
			auto found = aActor.Cache->find_blob_at_path(
				git_cache_tree_entry_request{
					.Commit = aRequest.Head,
					.Path   = std::filesystem::path{ std::u8string{ reinterpret_cast<const char8_t*>(aPath.data()), aPath.size() } },
				},
				aStop);
			if (found.has_value()) {
				return *found;
			}
			if (found.error().Code != git_cache_failure::object_mismatch || !chainTrees || !chainTrees->holds(aRequest.Head)) {
				return std::unexpected{ statusOf(found.error().Code) };
			}
			auto held = chainTrees->find_blob_at_path(aRequest.Head, aPath, aStop);
			if (!held) {
				return std::unexpected{ held.error().Failure == git_smart_transport_failure::cancelled ? generated_attributes_status::cancelled
																									   : generated_attributes_status::objects_unavailable };
			}
			return *held;
		};
		const auto read = [&](const core::git_blob_oid& aBlob) -> std::expected<std::string, generated_attributes_status> {
			auto bytes = aActor.Cache->materialize_blob({ .Blob = aBlob, .MaximumBytes = core::maximum_generated_attribute_file_bytes }, aStop);
			if (!bytes.has_value()) {
				if (bytes.error().Code == git_cache_failure::quota_exceeded) {
					return std::unexpected{ generated_attributes_status::file_too_large };
				}
				if (bytes.error().Code != git_cache_failure::object_mismatch || !sparse) {
					return std::unexpected{ statusOf(bytes.error().Code) };
				}
				auto smart               = git_smart_request_context{ };
				smart.ApplicationSession = aContext->Handoff.ApplicationSession;
				smart.Authority          = aContext->Authority;
				smart.Revision           = aContext->Revision;
				smart.Repository         = aContext->Handoff.Locator;
				smart.BaseRef            = aContext->Handoff.BaseRef;
				const auto transport     = aContext->Handoff.SmartGit->acquire(smart);
				if (!transport || !*transport) {
					return std::unexpected{ generated_attributes_status::objects_unavailable };
				}
				const auto fetched = (*transport)->read_exact_blob({ smart, aBlob, core::maximum_generated_attribute_file_bytes }, aStop);
				if (!fetched) {
					const auto failure = fetched.error().Failure;
					return std::unexpected{ failure == git_smart_transport_failure::response_limit  ? generated_attributes_status::file_too_large
												: failure == git_smart_transport_failure::cancelled ? generated_attributes_status::cancelled
																									: generated_attributes_status::objects_unavailable };
				}
				auto exact = std::vector<std::uint8_t>(fetched->size());
				std::ranges::transform(*fetched, exact.begin(), [](const std::byte aByte) { return std::to_integer<std::uint8_t>(aByte); });
				// A mismatch or a collision detection both end here, at failure.
				if (blob_matches(aBlob, exact) != blob_check::matches) {
					return std::unexpected{ generated_attributes_status::failure };
				}
				bytes = std::move(exact);
			}
			if (bytes->size() > core::maximum_generated_attribute_file_bytes) {
				return std::unexpected{ generated_attributes_status::file_too_large };
			}
			return std::string{ bytes->begin(), bytes->end() };
		};
		for (const auto& directory : aRequest.Directories) {
			if (aStop.stop_requested()) {
				return generated_attributes_status::cancelled;
			}
			const auto path  = directory.empty() ? std::string{ ".gitattributes" } : directory + "/.gitattributes";
			const auto found = lookup(path);
			if (!found) {
				return found.error();
			}
			if (!*found) {
				continue;
			}
			if (aObservation.Files.size() == core::maximum_generated_attribute_files) {
				return generated_attributes_status::too_many_files;
			}
			auto contents = read(**found);
			if (!contents) {
				if (contents.error() == generated_attributes_status::file_too_large) {
					aObservation.Exceeded = directory;
				}
				return contents.error();
			}
			aObservation.Files.push_back({ .Directory = directory, .Contents = std::move(*contents) });
		}
		return aStop.stop_requested() ? generated_attributes_status::cancelled : generated_attributes_status::resolved;
	}

	/// \brief RFC-BPR-0089 M4. See `epoch_application_host::resolve_filtered_comment_lines`.
	auto epoch_application_host::implementation::resolve_filtered_comment_lines(
		filtered_comment_lines_request    aRequest,
		filtered_comment_lines_completion aCompletion) -> bool
	{
		const auto pathValid = [](const std::string& aPath) {
			return !aPath.empty() && aPath.size() <= maximum_git_tree_entry_path_bytes && !aPath.starts_with('/');
		};
		const auto& filter = aRequest.Key.Filter;
		if (!aCompletion || !filter || !core::valid(*filter) || !aRequest.MergeBase.valid() || !pathValid(aRequest.LeftPath)
			|| !pathValid(aRequest.RightPath) || !pathValid(aRequest.HeadPath) || !pathValid(aRequest.BasePath)) {
			return false;
		}
		auto reviewActor = review_cache_actor(aRequest.WorkbenchAuthority, aRequest.Head);
		if (!reviewActor) {
			return false;
		}
		auto       actor    = std::move(reviewActor->first);
		const auto expected = std::move(reviewActor->second);
		{
			// The diff's commits are ones this review's touches job diffed;
			// the merge base is one of them or the accepted comparison's left.
			std::scoped_lock lock{ actor->BlobAuthorityMutex };
			const auto       authorized = [&actor](const core::git_commit_oid& aCommit) {
				return std::ranges::binary_search(actor->FilterCommits, aCommit);
			};
			const auto comparisonLeft = State.AcceptedComparison.has_value() && State.AcceptedComparison->Left.Commit == aRequest.MergeBase;
			if (actor->FilterAuthority != std::optional{ aRequest.WorkbenchAuthority } || !authorized(filter->Left) || !authorized(filter->Right)
				|| !authorized(aRequest.Head) || !(comparisonLeft || authorized(aRequest.MergeBase))) {
				return false;
			}
		}
		auto reviewContext = Active && Active->Actor == actor && Active->Revision.Head == aRequest.Head ? Active : std::shared_ptr<authority_context>{ };
		auto admitted      = admit_job(
			std::nullopt, std::nullopt, std::nullopt, std::nullopt, false);
		if (!admitted.has_value()) {
			return false;
		}
		auto        control      = std::move(*admitted);
		const auto  gate         = std::weak_ptr<completion_gate>{ CompletionGate };
		auto* const decisionLane = &DecisionLane;
		try {
			enqueue_actor_work(
				actor,
				[this,
				 actor,
				 expected,
				 context = std::move(reviewContext),
				 gate,
				 decisionLane,
				 control,
				 request    = std::move(aRequest),
				 completion = std::move(aCompletion)]() mutable {
					auto observation = filtered_comment_lines_observation{
						.Request = request,
						.Status  = filtered_comment_lines_status::failure,
						.Lines   = { },
					};
					observation.Status = filtered_comment_lines(*actor, expected, context, request, control->Stop.get_token(), observation.Lines);
					if (observation.Status != filtered_comment_lines_status::resolved) {
						observation.Lines = { };
					}
					enqueue_completion(
						gate,
						decisionLane,
						std::move(control),
						[answer  = std::move(observation),
						 deliver = std::move(completion)](implementation&) {
							deliver(answer);
						});
				});
		} catch (...) {
			finish_job(control);
			return false;
		}
		return true;
	}

	/// \brief [[BPR-1733]]. See `epoch_application_host::resolve_head_move_lines`.
	auto epoch_application_host::implementation::resolve_head_move_lines(
		head_move_lines_request    aRequest,
		head_move_lines_completion aCompletion) -> bool
	{
		const auto pathValid = [](const std::string& aPath) {
			return !aPath.empty() && aPath.size() <= maximum_git_tree_entry_path_bytes && !aPath.starts_with('/');
		};
		if (!aCompletion || !aRequest.EarlierHead.valid() || aRequest.EarlierHead == aRequest.Head || aRequest.EarlierHead.algorithm() != aRequest.Head.algorithm()
			|| !pathValid(aRequest.EarlierPath) || !pathValid(aRequest.HeadPath) || !State.Authority
			|| std::ranges::find(EarlierHeads, std::pair{ *State.Authority, aRequest.EarlierHead }) == EarlierHeads.end()) {
			return false;
		}
		auto reviewActor = review_cache_actor(aRequest.WorkbenchAuthority, aRequest.Head);
		if (!reviewActor) {
			return false;
		}
		auto       actor         = std::move(reviewActor->first);
		const auto expected      = std::move(reviewActor->second);
		auto       reviewContext = Active && Active->Actor == actor && Active->Revision.Head == aRequest.Head ? Active : std::shared_ptr<authority_context>{ };
		auto       admitted      = admit_job(std::nullopt, std::nullopt, std::nullopt, std::nullopt, false);
		if (!admitted.has_value()) {
			return false;
		}
		auto        control      = std::move(*admitted);
		const auto  gate         = std::weak_ptr<completion_gate>{ CompletionGate };
		auto* const decisionLane = &DecisionLane;
		try {
			enqueue_actor_work(
				actor,
				[this,
				 actor,
				 expected,
				 context = std::move(reviewContext),
				 gate,
				 decisionLane,
				 control,
				 request    = std::move(aRequest),
				 completion = std::move(aCompletion)]() mutable {
					auto observation   = head_move_lines_observation{ .Request = request, .Status = head_move_lines_status::failure, .Lines = std::nullopt };
					observation.Status = head_move_lines(*actor, expected, context, request, control->Stop.get_token(), observation.Lines);
					if (observation.Status != head_move_lines_status::resolved) {
						observation.Lines.reset();
					}
					enqueue_completion(gate, decisionLane, std::move(control), [answer = std::move(observation), deliver = std::move(completion)](implementation&) { deliver(answer); });
				});
		} catch (...) {
			finish_job(control);
			return false;
		}
		return true;
	}

	/// \brief [[BPR-1802]]. See `epoch_application_host::resolve_permalink_source`.
	auto epoch_application_host::implementation::resolve_permalink_source(
		permalink_source_request    aRequest,
		permalink_source_completion aCompletion) -> bool
	{
		if (!aCompletion || !aRequest.Commit.valid() || !aRequest.Head.valid() || aRequest.Commit.algorithm() != aRequest.Head.algorithm()
			|| aRequest.Path.empty() || aRequest.Path.size() > maximum_git_tree_entry_path_bytes || aRequest.Path.starts_with('/')) {
			return false;
		}
		auto reviewActor = review_cache_actor(aRequest.WorkbenchAuthority, aRequest.Head);
		if (!reviewActor) {
			return false;
		}
		auto       actor         = std::move(reviewActor->first);
		const auto expected      = std::move(reviewActor->second);
		auto       reviewContext = Active && Active->Actor == actor && Active->Revision.Head == aRequest.Head ? Active : std::shared_ptr<authority_context>{ };
		auto       admitted      = admit_job(std::nullopt, std::nullopt, std::nullopt, std::nullopt, false);
		if (!admitted.has_value()) {
			return false;
		}
		auto        control      = std::move(*admitted);
		const auto  gate         = std::weak_ptr<completion_gate>{ CompletionGate };
		auto* const decisionLane = &DecisionLane;
		try {
			enqueue_actor_work(
				actor,
				[this,
				 actor,
				 expected,
				 context = std::move(reviewContext),
				 gate,
				 decisionLane,
				 control,
				 request    = std::move(aRequest),
				 completion = std::move(aCompletion)]() mutable {
					auto observation   = permalink_source_observation{ .Request = request, .Status = permalink_source_status::failure, .Bytes = std::nullopt };
					observation.Status = read_exact_file_sides(*actor, expected, context, request.Head, core::file_comparison_key{ }, control->Stop.get_token(),
															   [&](const auto& aLoad, const auto& /*aCompare*/) -> permalink_source_status {
																   auto side = aLoad(request.Path, request.Commit);
																   if (!side) {
																	   return side.error();
																   }
																   if (side->Bytes) {
																	   if (side->Bytes->size() > maximum_permalink_source_bytes) {
																		   return permalink_source_status::too_large;
																	   }
																	   observation.Bytes.emplace(side->Bytes->begin(), side->Bytes->end());
																   }
																   return permalink_source_status::resolved;
															   });
					if (observation.Status != permalink_source_status::resolved) {
						observation.Bytes.reset();
					}
					enqueue_completion(gate, decisionLane, std::move(control), [answer = std::move(observation), deliver = std::move(completion)](implementation&) { deliver(answer); });
				});
		} catch (...) {
			finish_job(control);
			return false;
		}
		return true;
	}

	/// \brief [[BPR-1735]]. See `epoch_application_host::resolve_github_diff_lines`.
	auto epoch_application_host::implementation::resolve_github_diff_lines(
		github_diff_lines_request    aRequest,
		github_diff_lines_completion aCompletion) -> bool
	{
		const auto pathValid = [](const std::string& aPath) {
			return !aPath.empty() && aPath.size() <= maximum_git_tree_entry_path_bytes && !aPath.starts_with('/');
		};
		if (!aCompletion || !aRequest.MergeBase.valid() || !aRequest.Head.valid() || aRequest.MergeBase.algorithm() != aRequest.Head.algorithm()
			|| !pathValid(aRequest.HeadPath) || !pathValid(aRequest.BasePath) || !State.AcceptedComparison.has_value()
			|| State.AcceptedComparison->Left.Commit != aRequest.MergeBase || State.AcceptedComparison->Right.Commit != aRequest.Head) {
			return false;
		}
		auto reviewActor = review_cache_actor(aRequest.WorkbenchAuthority, aRequest.Head);
		if (!reviewActor) {
			return false;
		}
		auto       actor         = std::move(reviewActor->first);
		const auto expected      = std::move(reviewActor->second);
		auto       reviewContext = Active && Active->Actor == actor && Active->Revision.Head == aRequest.Head ? Active : std::shared_ptr<authority_context>{ };
		auto       admitted      = admit_job(std::nullopt, std::nullopt, std::nullopt, std::nullopt, false);
		if (!admitted.has_value()) {
			return false;
		}
		auto        control      = std::move(*admitted);
		const auto  gate         = std::weak_ptr<completion_gate>{ CompletionGate };
		auto* const decisionLane = &DecisionLane;
		try {
			enqueue_actor_work(
				actor,
				[this,
				 actor,
				 expected,
				 context = std::move(reviewContext),
				 gate,
				 decisionLane,
				 control,
				 request    = std::move(aRequest),
				 completion = std::move(aCompletion)]() mutable {
					auto observation   = github_diff_lines_observation{ .Request = request, .Status = github_diff_lines_status::failure, .BaseToHead = std::nullopt };
					observation.Status = github_diff_lines(*actor, expected, context, request, control->Stop.get_token(), observation.BaseToHead);
					if (observation.Status != github_diff_lines_status::resolved) {
						observation.BaseToHead.reset();
					}
					enqueue_completion(gate, decisionLane, std::move(control), [answer = std::move(observation), deliver = std::move(completion)](implementation&) { deliver(answer); });
				});
		} catch (...) {
			finish_job(control);
			return false;
		}
		return true;
	}

	/// \brief [[BPR-1735]]. The file at the merge base and at the head,
	/// compared, on the actor, read as a filtered diff's sides are.
	auto epoch_application_host::implementation::github_diff_lines(authority_effect_actor& aActor, const git_cache_open_request& aExpected,
																   const std::shared_ptr<authority_context>& aContext, const github_diff_lines_request& aRequest,
																   const std::stop_token aStop, std::optional<core::line_correspondence>& aLines) -> github_diff_lines_status
	{
		using status = github_diff_lines_status;
		return read_exact_file_sides(aActor, aExpected, aContext, aRequest.Head, aRequest.Key, aStop, [&](const auto& aLoad, const auto& aCompare) -> status {
			auto base = aLoad(aRequest.BasePath, aRequest.MergeBase);
			if (!base) {
				return base.error();
			}
			auto head = aLoad(aRequest.HeadPath, aRequest.Head);
			if (!head) {
				return head.error();
			}
			aLines = aCompare(*base, *head);
			return aStop.stop_requested() ? status::cancelled : aLines ? status::resolved
																	   : status::failure;
		});
	}

	/// \brief RFC-BPR-0089 M4. The file at the filtered diff's commits, the
	/// head and the merge base, compared, on the actor.
	///
	/// \details The merge base against the head is compared too: GitHub
	/// resolves a line moved to the head only inside its hunks. Each blob
	/// is looked up by literal path in the retained
	/// cache or, for a sparse review whose chain commits only its chain
	/// store holds, there, and read exactly as `generated_attributes` reads
	/// one. Each pair is compared by the review's own engine with exact
	/// whitespace, so an unchanged line is byte for byte the same.
	auto epoch_application_host::implementation::filtered_comment_lines(authority_effect_actor& aActor, const git_cache_open_request& aExpected,
																		const std::shared_ptr<authority_context>& aContext, const filtered_comment_lines_request& aRequest,
																		const std::stop_token aStop, core::filtered_comment_lines& aLines) -> filtered_comment_lines_status
	{
		using status = filtered_comment_lines_status;
		return read_exact_file_sides(aActor, aExpected, aContext, aRequest.Head, aRequest.Key, aStop, [&](const auto& aLoad, const auto& aCompare) -> status {
			const auto& diff  = *aRequest.Key.Filter;
			auto        right = aLoad(aRequest.RightPath, diff.Right);
			if (!right) {
				return right.error();
			}
			auto base = aLoad(aRequest.BasePath, aRequest.MergeBase);
			if (!base) {
				return base.error();
			}
			auto head = diff.Right == aRequest.Head ? right : aLoad(aRequest.HeadPath, aRequest.Head);
			if (!head) {
				return head.error();
			}
			// A line moved to the head is resolved there only inside a hunk of
			// the merge base to the head.
			aLines.BaseToHead = aCompare(*base, *head);
			if (diff.Right != aRequest.Head) {
				aLines.RightToHead = aCompare(*right, *head);
				aLines.BaseToRight = aCompare(*base, *right);
			}
			if (aStop.stop_requested()) {
				return status::cancelled;
			}
			if (diff.Left != aRequest.MergeBase) {
				auto left = aLoad(aRequest.LeftPath, diff.Left);
				if (!left) {
					return left.error();
				}
				aLines.LeftToBase = aCompare(*left, *base);
			}
			return aStop.stop_requested() ? status::cancelled : status::resolved;
		});
	}

	/// \brief [[BPR-1733]]. The file at an earlier head and at the head,
	/// compared, on the actor, read as a filtered diff's sides are.
	auto epoch_application_host::implementation::head_move_lines(authority_effect_actor& aActor, const git_cache_open_request& aExpected,
																 const std::shared_ptr<authority_context>& aContext, const head_move_lines_request& aRequest,
																 const std::stop_token aStop, std::optional<core::line_correspondence>& aLines) -> head_move_lines_status
	{
		using status = head_move_lines_status;
		return read_exact_file_sides(aActor, aExpected, aContext, aRequest.Head, aRequest.Key, aStop, [&](const auto& aLoad, const auto& aCompare) -> status {
			auto earlier = aLoad(aRequest.EarlierPath, aRequest.EarlierHead);
			if (!earlier) {
				return earlier.error();
			}
			auto head = aLoad(aRequest.HeadPath, aRequest.Head);
			if (!head) {
				return head.error();
			}
			aLines = aCompare(*earlier, *head);
			return aStop.stop_requested() ? status::cancelled : aLines ? status::resolved
																	   : status::failure;
		});
	}

	/// \brief [[BPR-1212]]. The review's cache actor and the session it
	/// expects, for a job asked about the accepted head; none when the
	/// request would be refused before admission.
	auto epoch_application_host::implementation::review_cache_actor(
		const workbench_pull_request_authority& aAuthority,
		const core::git_commit_oid&             aHead)
		-> std::optional<std::pair<std::shared_ptr<authority_effect_actor>, git_cache_open_request>>
	{
		if (!aHead.valid() || !authorized_selected_diff_context(aAuthority, std::nullopt).has_value()
			|| State.AcceptedRevision->Head != aHead || State.PendingIngestion.has_value()) {
			return std::nullopt;
		}
		const auto& authority = *State.Authority;
		const auto  key       = cache_actor_key{
			.Profile         = authority.Profile,
			.ProfileRevision = authority.ProfileRevision,
			.Host            = authority.Host,
			.HostAuthority   = authority.HostAuthority,
			.Account         = authority.Account,
			.Repository      = authority.Repository,
			.ObjectFormat    = aHead.algorithm(),
			.Mode            = authority.Mode,
		};
		auto actor = std::shared_ptr<authority_effect_actor>{ };
		{
			std::scoped_lock lock{ CacheActorsMutex };
			const auto       found = std::ranges::find(
				CacheActors, key, &decltype(CacheActors)::value_type::first);
			if (found != CacheActors.end()) {
				actor = found->second;
			}
		}
		if (!actor) {
			return std::nullopt;
		}
		return std::pair{ std::move(actor), git_cache_open_request{
												.ApplicationSession = State.ApplicationSession,
												.Authority          = authority,
												.ObjectFormat       = aHead.algorithm(),
											} };
	}

	auto epoch_application_host::implementation::resolve_github_base_numbering(
		github_base_numbering_request    aRequest,
		github_base_numbering_completion aCompletion) -> bool
	{
		if (!aCompletion || !aRequest.BaseTip.valid() || !aRequest.Left.valid() || aRequest.LeftPath.empty()
			|| aRequest.BaseTip.algorithm() != aRequest.Head.algorithm() || aRequest.Left.algorithm() != aRequest.Head.algorithm()) {
			return false;
		}
		auto reviewActor = review_cache_actor(aRequest.WorkbenchAuthority, aRequest.Head);
		if (!reviewActor) {
			return false;
		}
		auto admitted = admit_job(
			std::nullopt, std::nullopt, std::nullopt, std::nullopt, false);
		if (!admitted.has_value()) {
			return false;
		}
		auto        control      = std::move(*admitted);
		const auto  gate         = std::weak_ptr<completion_gate>{ CompletionGate };
		auto* const decisionLane = &DecisionLane;
		auto        actor        = std::move(reviewActor->first);
		try {
			enqueue_actor_work(
				actor,
				[this,
				 actor,
				 expected = std::move(reviewActor->second),
				 gate,
				 decisionLane,
				 control,
				 request    = std::move(aRequest),
				 completion = std::move(aCompletion)]() mutable {
					auto observation = github_base_numbering_observation{
						.Request = request,
						.Status  = github_base_numbering_status::failure,
					};
					const auto workStop = control->Stop.get_token();
					const auto statusOf = [](const git_cache_error& aError) {
						return aError.Code == git_cache_failure::cancelled
							? github_base_numbering_status::cancelled
							: github_base_numbering_status::failure;
					};
					if (workStop.stop_requested()
						|| !Consumer->active() || BlobIngressClosing.load(std::memory_order_acquire)) {
						observation.Status = github_base_numbering_status::cancelled;
					} else if (!open_actor_cache(*actor, Caches, expected, workStop)) {
						observation.Status = github_base_numbering_status::stale;
					} else if (const auto derived = actor->Cache->derive_comparison_base(
								   git_cache_comparison_base_request{ .Head = request.Head, .SelectedBase = request.BaseTip }, workStop);
							   !derived.has_value()) {
						observation.Status = statusOf(derived.error());
					} else if (derived->Status != git_comparison_base_status::unique || !derived->Base || !derived->Base->valid()) {
						// No common ancestor, several, or objects the cache
						// does not hold (a sparse review): not knowable here.
						observation.Status = github_base_numbering_status::unavailable;
					} else {
						observation.MergeBase = derived->Base;
						if (*derived->Base == request.Left) {
							observation.Status = github_base_numbering_status::same;
						} else if (const auto atBase = actor->Cache->find_blob_at_path(
									   git_cache_tree_entry_request{ .Commit = *derived->Base, .Path = request.LeftPath }, workStop);
								   !atBase.has_value()) {
							observation.Status = statusOf(atBase.error());
						} else if (const auto atLeft = actor->Cache->find_blob_at_path(
									   git_cache_tree_entry_request{ .Commit = request.Left, .Path = request.LeftPath }, workStop);
								   !atLeft.has_value()) {
							observation.Status = statusOf(atLeft.error());
						} else {
							observation.Status = atBase->has_value() && *atBase == *atLeft
								? github_base_numbering_status::same
								: github_base_numbering_status::differs;
						}
					}
					enqueue_completion(
						gate,
						decisionLane,
						std::move(control),
						[answer  = std::move(observation),
						 deliver = std::move(completion)](implementation&) {
							deliver(answer);
						});
				});
		} catch (...) {
			finish_job(control);
			return false;
		}
		return true;
	}
}
