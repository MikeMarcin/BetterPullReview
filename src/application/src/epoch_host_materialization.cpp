#include "epoch_host_implementation.hpp"

#include "bpr/application/git_process_port.hpp"
#include "bpr/application/github_host_authority.hpp"
#include "bpr/core/anonymous_read_policy.hpp"
#include "bpr/core/diff_transition.hpp"

#include <chrono>
#include <tuple>

namespace bpr::application {
	namespace {
		[[nodiscard]] auto catalog_key(
			const accepted_pull_request_handoff& aHandoff)
			-> epoch_catalog_authority_key
		{
			return {
				.Profile         = aHandoff.Profile,
				.ProfileRevision = aHandoff.ProfileRevision,
				.Host            = aHandoff.Host,
				.HostAuthority   = aHandoff.HostAuthority,
				.Account         = aHandoff.Account,
				.Viewer          = aHandoff.Viewer,
				.Repository      = aHandoff.Repository,
				.PullRequest     = aHandoff.PullRequest,
				.Number          = aHandoff.Number,
				.ObjectFormat    = aHandoff.ObjectFormat,
				// The mode is part of the catalog key, so a signed-out read
				// opens its own scope rather than reusing an account's.
				.Mode = aHandoff.Mode,
			};
		}

		[[nodiscard]] auto agrees(
			const core::pull_request_authority&  aAuthority,
			const accepted_pull_request_handoff& aHandoff) noexcept -> bool
		{
			return aAuthority.Profile == aHandoff.Profile
				&& aAuthority.ProfileRevision == aHandoff.ProfileRevision
				&& aAuthority.Host == aHandoff.Host
				&& aAuthority.HostAuthority == aHandoff.HostAuthority
				&& aAuthority.Account == aHandoff.Account
				&& aAuthority.Viewer == aHandoff.Viewer
				&& aAuthority.Repository == aHandoff.Repository
				&& aAuthority.PullRequest == aHandoff.PullRequest
				&& aAuthority.Number == aHandoff.Number
				&& aAuthority.Mode == aHandoff.Mode;
		}

		[[nodiscard]] auto accepted_revision(
			const accepted_pull_request_handoff&    aHandoff,
			const core::epoch_source_observation_id aObservation)
			-> core::accepted_pull_request_revision
		{
			return {
				.Source = {
					.Observation           = aObservation,
					.Snapshot              = aHandoff.Snapshot,
					.IdentityGeneration    = aHandoff.IdentityGeneration,
					.PullRequestGeneration = aHandoff.PullRequestGeneration,
				},
				.Origin              = aHandoff.Origin,
				.BaseBranch          = aHandoff.BaseBranch,
				.Base                = aHandoff.Base,
				.Head                = aHandoff.Head,
				.CurrentViewerReview = aHandoff.CurrentViewerReview,
			};
		}

		[[nodiscard]] static auto reconcile_timeline(
			const accepted_pull_request_handoff&  aHandoff,
			core::epoch_occurrence_history        aHistory,
			const core::observed_utc_milliseconds aFirstSeen)
			-> core::epoch_occurrence_history
		{
			// A REST timeline does not contain ordinary push boundaries. Retain
			// locally observed occurrences and report the missing boundary even
			// when this bounded read exhausted all of its pages.
			aHistory.Incomplete = true;
			if (aHandoff.TimelineAvailability != epoch_timeline_availability::available) {
				return aHistory;
			}
			for (const auto& event : aHandoff.ForcePushes) {
				const auto evidence = core::epoch_occurrence_evidence{
					.Kind    = core::epoch_occurrence_kind::force_push_event,
					.EventId = event.EventId,
					.Head    = event.Head,
				};
				const auto id = core::make_epoch_occurrence_id(
					aHandoff.Hostname.value(), aHandoff.PullRequest, evidence);
				if (!id) {
					aHistory.Ambiguous = true;
					continue;
				}
				const auto found = std::ranges::find_if(
					aHistory.Occurrences, [id](const auto& aOccurrence) {
						return aOccurrence.Id == *id;
					});
				if (found != aHistory.Occurrences.end()) {
					if (found->Evidence != evidence
						|| found->DateSource != core::epoch_occurrence_date_source::github_event
						|| found->DateUtcMilliseconds != event.EventUtcMilliseconds) {
						aHistory.Ambiguous = true;
					}
					continue;
				}
				if (aHistory.Occurrences.size() >= core::maximum_epoch_occurrence_count) {
					aHistory.Ambiguous = true;
					break;
				}
				aHistory.Occurrences.push_back(core::epoch_occurrence{
					.Id                  = *id,
					.Evidence            = evidence,
					.DateSource          = core::epoch_occurrence_date_source::github_event,
					.DateUtcMilliseconds = event.EventUtcMilliseconds,
					.FirstSeen           = aFirstSeen,
				});
			}
			std::ranges::sort(aHistory.Occurrences, core::epoch_occurrence_less);
			return aHistory;
		}
	}

	/// \brief The one exact network read a blob read still owes, if any.
	/// [[BPR-1360]]
	///
	/// \details Touches no cache session and no actor state, so it may run
	/// off the actor while the actor waits for it.
	void epoch_application_host::implementation::read_remote_blob(actor_blob_read& aRead, const std::stop_token aStop) noexcept
	{
		if (!aRead.Remote) {
			return;
		}
		try {
			const auto fetched = aRead.Transport->read_exact_blob(*aRead.Remote, aStop);
			if (fetched) {
				aRead.Bytes.emplace();
				aRead.Bytes->reserve(fetched->size());
				for (const auto byte : *fetched) {
					aRead.Bytes->push_back(std::to_integer<std::uint8_t>(byte));
				}
				aRead.FetchedRemotely = true;
			} else if (fetched.error().Failure == git_smart_transport_failure::cancelled) {
				aRead.Observation.Status = diff_blob_status::cancelled;
			} else if (fetched.error().Failure == git_smart_transport_failure::response_limit) {
				aRead.Observation.Status = diff_blob_status::materialization_limit;
			}
		} catch (...) {
			aRead.Bytes.reset();
			aRead.FetchedRemotely    = false;
			aRead.Observation.Status = diff_blob_status::failure;
		}
		aRead.Remote.reset();
		aRead.PreparedRead.reset();
	}

	/// \brief Publishes a blob read under the actor's authority lock.
	/// [[BPR-1360]]
	///
	/// \details Takes the lock if the caller does not already hold it and
	/// leaves it held, so the caller hands the result over before an
	/// invalidation can intervene.
	void epoch_application_host::implementation::finish_actor_blob_read(actor_blob_read&                               aRead,
																		const std::shared_ptr<authority_effect_actor>& aActor,
																		const std::shared_ptr<authority_context>&      aSelected,
																		const blob_authority&                          aAuthorization,
																		const std::uint64_t                            aAdmittedGeneration,
																		const std::stop_token                          aStop,
																		std::unique_lock<std::mutex>&                  aPublicationLock)
	{
		if (!aRead.Opened) {
			return;
		}
		const auto& request = aRead.Request;
		const auto& cached  = *aRead.Cached;
		if (aRead.Bytes.has_value()) {
			if (!aPublicationLock.owns_lock()) {
				aPublicationLock = std::unique_lock{ aActor->BlobAuthorityMutex };
			}
			if (aStop.stop_requested() || !Consumer->active() || BlobIngressClosing.load(std::memory_order_acquire)) {
				aRead.Observation.Status = diff_blob_status::cancelled;
			} else if (aActor->BlobAuthority != std::optional{ aAuthorization }
					   || aActor->BlobAuthorityGeneration != aAdmittedGeneration
					   || (aRead.PinnedSource && aActor->RealignmentAuthority != std::optional{ selected_diff_authority{ .Workbench = aRead.PinnedSource->Request.WorkbenchAuthority, .Diff = aRead.PinnedSource->Request.Diff } })
					   || (aRead.SelectedGrantSequence
						   && (aActor->SelectedBlobGrantSequence != *aRead.SelectedGrantSequence
							   || !aActor->SelectedBlobGrant
							   || !exact_selected_blob_request(*aActor->SelectedBlobGrant, request)))
					   || (aRead.PreparedGrantSequence
						   && (aActor->PreparedGrantSequence != *aRead.PreparedGrantSequence
							   || std::ranges::none_of(aActor->PreparedGrants,
													   [&request](const workbench_selected_diff_observation& aGrant) {
														   return exact_selected_blob_request(aGrant, request);
													   })))) {
				aRead.Observation.Status = diff_blob_status::failure;
			} else {
				if (aRead.FetchedRemotely) {
					try {
						retain_blob(*aActor, request.Blob, *aRead.Bytes);
					} catch (...) {
						// Retention is an optimization; the read stands.
					}
				}
				if (aRead.SelectedGrantSequence) {
					retain_blob(*aActor, request.Blob, *aRead.Bytes);
				}
				aRead.Observation.Status = diff_blob_status::available;
				aRead.Observation.Bytes  = std::move(*aRead.Bytes);
			}
		} else if (!aRead.RetainedOverLimit && cached.error() == git_cache_failure::cancelled) {
			aRead.Observation.Status = diff_blob_status::cancelled;
		} else if (aRead.RetainedOverLimit || cached.error() == git_cache_failure::quota_exceeded) {
			aRead.Observation.Status = diff_blob_status::materialization_limit;
		} else if (cached.error() == git_cache_failure::object_mismatch
				   && (object_loading_for(aSelected->Handoff.Shape) != pull_request_object_loading::selected_objects
					   || !aSelected->Handoff.SmartGit || !aSelected->Handoff.SmartGit->supports_sparse_commit_trees())) {
			aRead.Observation.Status = diff_blob_status::missing;
		}
	}

	auto epoch_application_host::implementation::materialize(
		const diff_blob_request&                           aRequest,
		const std::stop_token                              aStop,
		std::optional<workbench_selected_diff_observation> aPinnedSource) -> diff_blob_observation
	{
		auto result = diff_blob_observation{
			.Context = aRequest.Context,
			.Side    = aRequest.Side,
			.Blob    = aRequest.Blob,
			.Status  = diff_blob_status::failure,
		};
		if (aPinnedSource) {
			const auto& source   = *aPinnedSource;
			const auto& endpoint = aRequest.Side == core::diff_side::left ? source.LeftEndpoint : source.RightEndpoint;
			const auto& path     = aRequest.Side == core::diff_side::left ? source.LeftPath : source.RightPath;
			if (aRequest.MaximumBytes > 256U * 1024U || !exact_selected_blob_request(source, aRequest)
				|| source.Request.Diff.Comparison.Left.Commit != source.Request.Diff.Comparison.Right.Commit
				|| !path || !endpoint || !valid(*endpoint, source.Request, aRequest.Side, aRequest.Blob, *path)) {
				return result;
			}
		}
		if (!begin_blob_call()) {
			return result;
		}
		struct blob_call_guard final
		{
			implementation& Host;

			~blob_call_guard()
			{
				Host.finish_blob_call();
			}
		} call{ *this };
		const auto key = actor_key(aRequest);
		if (
			!key.has_value()
			|| aRequest.MaximumBytes
				> maximum_git_cache_blob_materialization_bytes) {
			return result;
		}

		auto actor = std::shared_ptr<authority_effect_actor>{ };
		{
			std::scoped_lock lock{ CacheActorsMutex };
			const auto       found = std::ranges::find(
				CacheActors, *key, &decltype(CacheActors)::value_type::first);
			if (found != CacheActors.end()) {
				actor = found->second;
			}
		}
		if (!actor) {
			return result;
		}
		const auto authorization           = blob_authority_for(aRequest);
		const auto authorizationGeneration = blob_authorization_generation(
			actor, authorization);
		if (!authorizationGeneration) {
			return result;
		}
		auto selectedContext = std::shared_ptr<authority_context>{ };
		auto reviewerBlob    = false;
		{
			std::scoped_lock lock{ actor->BlobAuthorityMutex };
			if (actor->BlobAuthority == std::optional{ authorization }
				&& actor->BlobAuthorityGeneration == *authorizationGeneration) {
				selectedContext = actor->BlobContext.lock();
				reviewerBlob    = actor->SelectedBlobGrant
					&& exact_selected_blob_request(*actor->SelectedBlobGrant, aRequest);
			}
		}
		if (!selectedContext) {
			return result;
		}
		// [[BPR-1344]]. A prepared comparison's read is preparation's job;
		// the reviewer's own selected file never is.
		const auto preparation = aRequest.Prepared && !reviewerBlob;
		auto       waiter      = std::shared_ptr<blob_waiter>{ };
		try {
			waiter = std::make_shared<blob_waiter>();
		} catch (...) {
			return result;
		}
		auto admitted = admit_job(
			std::nullopt,
			std::nullopt,
			std::nullopt,
			std::nullopt,
			false,
			false,
			preparation);
		if (!admitted) {
			return result;
		}
		auto control = std::move(*admitted);

		const auto expected = git_cache_open_request{
			.ApplicationSession = aRequest.Context.ApplicationSession,
			.Authority          = aRequest.Context.Key.Authority,
			.ObjectFormat       = aRequest.Blob.algorithm(),
		};
		try {
			// [[BPR-1359]]. A blob of the reviewer's selected file is the
			// reviewer's read, and does not wait out a prepared one.
			auto turn = reviewerBlob ? std::make_shared<reviewer_work>(actor) : std::shared_ptr<reviewer_work>{ };
			enqueue_actor_work(
				actor,
				[this,
				 actor,
				 selectedContext,
				 authorization,
				 admittedGeneration = *authorizationGeneration,
				 expected,
				 aRequest,
				 aStop,
				 waiter,
				 control,
				 pinnedSource = std::move(aPinnedSource),
				 reviewer     = std::move(turn)] {
					auto read                     = actor_blob_read{ aRequest };
					read.PinnedSource             = pinnedSource;
					auto publicationAuthorityLock = std::unique_lock<std::mutex>{ };
					try {
						auto forwardCallerStop = std::stop_callback{
							aStop,
							[control] {
								static_cast<void>(control->Stop.request_stop());
							}
						};
						const auto workStop = control->Stop.get_token();
						begin_actor_blob_read(read, actor, selectedContext, authorization, admittedGeneration, expected,
											  control->Stop);
						read_remote_blob(read, workStop);
						finish_actor_blob_read(read, actor, selectedContext, authorization, admittedGeneration, workStop,
											   publicationAuthorityLock);
					} catch (...) {
						read.Observation.Status = diff_blob_status::failure;
					}
					auto observation    = std::move(read.Observation);
					auto waiterLock     = std::unique_lock{ waiter->Mutex };
					waiter->Result      = std::move(observation);
					const auto reserved = retire_actor_job(control);
					struct actor_exit_reservation final
					{
						implementation& Host;
						bool            Reserved;
						bool            Preparation;

						~actor_exit_reservation()
						{
							Host.release_actor_job_reservation(Reserved, Preparation);
						}
					} actorExit{ *this, reserved, control->Preparation };
					waiterLock.unlock();
					waiter->Completed.notify_all();
				},
				preparation ? profile_epoch_actor_priority::preparation : profile_epoch_actor_priority::reviewer);
		} catch (...) {
			finish_job(control);
			return result;
		}

		// Registered before the lock is taken: a token that has already been
		// stopped invokes the callback during construction, and doing that
		// while holding the mutex would deadlock. Setting `Abandoned` under
		// the mutex before notifying means no wakeup can be lost, because the
		// predicate is evaluated on entry.
		auto releaseOnStop = std::stop_callback{ aStop, [waiter] {
													{
														const auto guard  = std::lock_guard{ waiter->Mutex };
														waiter->Abandoned = true;
													}
													waiter->Completed.notify_all();
												} };
		std::unique_lock lock{ waiter->Mutex };
		// Bounded, where it used to wait forever. `waiter` is a shared owner,
		// so abandoning the wait is safe: the closure still completes and
		// writes its result into a record nothing reads.
		static_cast<void>(waiter->Completed.wait_for(
			lock, Limits.BlobWait, [&waiter] {
				return waiter->Result.has_value() || waiter->Abandoned;
			}));
		if (waiter->Result.has_value()) {
			return std::move(*waiter->Result);
		}
		// `result` already carries the request's identity. Say which of the
		// two releases happened rather than collapsing both to `failure`: a
		// cancelled read is the reviewer moving on, and an elapsed bound is an
		// actor that never got to it.
		result.Status = waiter->Abandoned
			? diff_blob_status::cancelled
			: diff_blob_status::failure;
		return result;
	}

	/// \brief Both sides of the reviewer's cold open, read at once.
	/// [[BPR-1360]]
	///
	/// \details One job on the actor does both sides' cache and grant
	/// work, as two `materialize` calls would. When both sides must be
	/// read from the network, the actor lends the left side's read to this
	/// caller, which is only waiting, and reads the right side itself, so
	/// the open waits about one round trip rather than two. Both are the
	/// reviewer's reads ([[BPR-1359]]): the job stops a prepared read in
	/// flight and none is admitted while it is queued or running, and
	/// only this caller's stop or the host's closing stops either read.
	/// Absent unless both blobs belong to the reviewer's selected file:
	/// the diff host then reads the sides one at a time, as before.
	auto epoch_application_host::implementation::materialize_both(const diff_blob_request& aLeft,
																  const diff_blob_request& aRight,
																  const std::stop_token    aStop)
		-> std::optional<std::pair<diff_blob_observation, diff_blob_observation>>
	{
		if (aLeft.Side != core::diff_side::left || aRight.Side != core::diff_side::right
			|| aLeft.Context != aRight.Context) {
			return std::nullopt;
		}
		if (!begin_blob_call()) {
			return std::nullopt;
		}
		struct blob_call_guard final
		{
			implementation& Host;

			~blob_call_guard()
			{
				Host.finish_blob_call();
			}
		} call{ *this };
		const auto key = actor_key(aLeft);
		if (!key.has_value() || actor_key(aRight) != key
			|| std::max(aLeft.MaximumBytes, aRight.MaximumBytes) > maximum_git_cache_blob_materialization_bytes) {
			return std::nullopt;
		}
		auto actor = std::shared_ptr<authority_effect_actor>{ };
		{
			std::scoped_lock lock{ CacheActorsMutex };
			const auto       found = std::ranges::find(CacheActors, *key, &decltype(CacheActors)::value_type::first);
			if (found != CacheActors.end()) {
				actor = found->second;
			}
		}
		if (!actor) {
			return std::nullopt;
		}
		const auto authorization           = blob_authority_for(aLeft);
		const auto authorizationGeneration = blob_authorization_generation(actor, authorization);
		if (!authorizationGeneration) {
			return std::nullopt;
		}
		auto selectedContext = std::shared_ptr<authority_context>{ };
		{
			std::scoped_lock lock{ actor->BlobAuthorityMutex };
			if (actor->BlobAuthority == std::optional{ authorization }
				&& actor->BlobAuthorityGeneration == *authorizationGeneration && actor->SelectedBlobGrant
				&& exact_selected_blob_request(*actor->SelectedBlobGrant, aLeft)
				&& exact_selected_blob_request(*actor->SelectedBlobGrant, aRight)) {
				selectedContext = actor->BlobContext.lock();
			}
		}
		if (!selectedContext) {
			return std::nullopt;
		}

		const auto unread = [](const diff_blob_request& aRequest) {
			auto observation    = diff_blob_observation{ };
			observation.Context = aRequest.Context;
			observation.Side    = aRequest.Side;
			observation.Blob    = aRequest.Blob;
			observation.Status  = diff_blob_status::failure;
			return observation;
		};
		auto result = std::pair{ unread(aLeft), unread(aRight) };
		auto waiter = std::shared_ptr<blob_pair_waiter>{ };
		try {
			waiter = std::make_shared<blob_pair_waiter>();
		} catch (...) {
			return result;
		}
		auto admitted = admit_job(std::nullopt, std::nullopt, std::nullopt, std::nullopt, false);
		if (!admitted) {
			return result;
		}
		auto       control  = std::move(*admitted);
		const auto expected = git_cache_open_request{
			.ApplicationSession = aLeft.Context.ApplicationSession,
			.Authority          = aLeft.Context.Key.Authority,
			.ObjectFormat       = aLeft.Blob.algorithm(),
		};
		try {
			auto turn = std::make_shared<reviewer_work>(actor);
			enqueue_actor_work(
				actor,
				[this,
				 actor,
				 selectedContext,
				 authorization,
				 admittedGeneration = *authorizationGeneration,
				 expected,
				 aLeft,
				 aRight,
				 aStop,
				 waiter,
				 control,
				 reviewer = std::move(turn)] {
					auto left                     = actor_blob_read{ aLeft };
					auto right                    = actor_blob_read{ aRight };
					auto publicationAuthorityLock = std::unique_lock<std::mutex>{ };
					try {
						auto forwardCallerStop = std::stop_callback{
							aStop, [control] { static_cast<void>(control->Stop.request_stop()); }
						};
						const auto workStop = control->Stop.get_token();
						begin_actor_blob_read(left, actor, selectedContext, authorization, admittedGeneration, expected,
											  control->Stop);
						if (left.Bytes || left.Remote) {
							begin_actor_blob_read(right, actor, selectedContext, authorization, admittedGeneration,
												  expected, control->Stop);
						} else {
							// The left side has already failed, and the
							// diff host reports the left side first: the
							// right side's read would be wasted.
							right.Observation.Status = diff_blob_status::cancelled;
						}
						read_both_remote_blobs(*waiter, left, right, workStop);
						finish_actor_blob_read(left, actor, selectedContext, authorization, admittedGeneration, workStop,
											   publicationAuthorityLock);
						finish_actor_blob_read(right, actor, selectedContext, authorization, admittedGeneration,
											   workStop, publicationAuthorityLock);
					} catch (...) {
						left.Observation.Status  = diff_blob_status::failure;
						right.Observation.Status = diff_blob_status::failure;
						left.Observation.Bytes.clear();
						right.Observation.Bytes.clear();
					}
					auto waiterLock     = std::unique_lock{ waiter->Mutex };
					waiter->Result      = std::pair{ std::move(left.Observation), std::move(right.Observation) };
					const auto reserved = retire_actor_job(control);
					struct actor_exit_reservation final
					{
						implementation& Host;
						bool            Reserved;

						~actor_exit_reservation()
						{
							Host.release_actor_job_reservation(Reserved);
						}
					} actorExit{ *this, reserved };
					waiterLock.unlock();
					waiter->Changed.notify_all();
				});
		} catch (...) {
			finish_job(control);
			return result;
		}

		// Registered before the lock is taken, per `materialize`.
		auto releaseOnStop = std::stop_callback{ aStop, [waiter] {
													{
														const auto guard  = std::lock_guard{ waiter->Mutex };
														waiter->Abandoned = true;
													}
													waiter->Changed.notify_all();
												} };
		std::unique_lock lock{ waiter->Mutex };
		while (true) {
			const auto woken = waiter->Changed.wait_for(lock, Limits.BlobWait, [&waiter] {
				return waiter->Result.has_value() || waiter->Abandoned
					|| waiter->Lend == blob_pair_waiter::lent_read::offered;
			});
			if (waiter->Result.has_value()) {
				return std::move(waiter->Result);
			}
			if (!woken || waiter->Abandoned) {
				break;
			}
			// The actor lent this caller one side's network read.
			waiter->Lend  = blob_pair_waiter::lent_read::taken;
			auto lentRead = std::move(waiter->Lent);
			waiter->Lent  = { };
			lock.unlock();
			lentRead();
			lock.lock();
			waiter->Lend = blob_pair_waiter::lent_read::returned;
			waiter->Changed.notify_all();
		}
		waiter->Gone = true;
		waiter->Changed.notify_all();
		const auto status    = waiter->Abandoned ? diff_blob_status::cancelled : diff_blob_status::failure;
		result.first.Status  = status;
		result.second.Status = status;
		return result;
	}

	/// \brief Makes both sides' owed network reads, at once when both owe
	/// one. [[BPR-1360]]
	///
	/// \details Runs on the actor. The left side's read is lent to the
	/// caller waiting in `materialize_both`, and the right side's is made
	/// here meanwhile; the actor then waits for the lent read to come
	/// back. A lent read the caller never took, because it stopped
	/// waiting, is made here instead (a stopped job's read ends at once).
	void epoch_application_host::implementation::read_both_remote_blobs(blob_pair_waiter&     aWaiter,
																		actor_blob_read&      aLeft,
																		actor_blob_read&      aRight,
																		const std::stop_token aStop) noexcept
	{
		auto lent = false;
		if (aLeft.Remote && aRight.Remote) {
			try {
				std::scoped_lock lock{ aWaiter.Mutex };
				if (!aWaiter.Gone && !aWaiter.Abandoned) {
					aWaiter.Lent = [&aLeft, aStop] { read_remote_blob(aLeft, aStop); };
					aWaiter.Lend = blob_pair_waiter::lent_read::offered;
					lent         = true;
				}
			} catch (...) {
				lent = false;
			}
		}
		if (lent) {
			aWaiter.Changed.notify_all();
		}
		read_remote_blob(aRight, aStop);
		if (lent) {
			std::unique_lock lock{ aWaiter.Mutex };
			aWaiter.Changed.wait(lock, [&aWaiter] {
				return aWaiter.Lend == blob_pair_waiter::lent_read::returned
					|| (aWaiter.Lend == blob_pair_waiter::lent_read::offered && (aWaiter.Gone || aWaiter.Abandoned));
			});
			lent         = aWaiter.Lend == blob_pair_waiter::lent_read::returned;
			aWaiter.Lend = blob_pair_waiter::lent_read::none;
			aWaiter.Lent = { };
		}
		if (!lent) {
			read_remote_blob(aLeft, aStop);
		}
	}

	auto epoch_application_host::implementation::materialize_agent_workspace(
		const agent_workspace_materialization_request& aRequest,
		const std::stop_token                          aStop)
		-> std::expected<owned_agent_workspace, agent_workspace_error>
	{
		if (!begin_blob_call()) {
			return std::unexpected{ agent_workspace_failure::cancelled };
		}
		struct workspace_call_guard final
		{
			implementation& Host;

			~workspace_call_guard()
			{
				Host.finish_blob_call();
			}
		} call{ *this };
		const auto key = actor_key(aRequest);
		if (!key.has_value()) {
			return std::unexpected{ agent_workspace_failure::invalid_request };
		}
		auto actor = std::shared_ptr<authority_effect_actor>{ };
		{
			std::scoped_lock lock{ CacheActorsMutex };
			const auto       found = std::ranges::find(
				CacheActors,
				*key,
				&decltype(CacheActors)::value_type::first);
			if (found != CacheActors.end()) {
				actor = found->second;
			}
		}
		if (!actor) {
			return std::unexpected{ agent_workspace_failure::unavailable };
		}
		const auto authorization           = blob_authority_for(aRequest);
		const auto authorizationGeneration = blob_authorization_generation(
			actor,
			authorization);
		if (!authorizationGeneration.has_value()) {
			return std::unexpected{ agent_workspace_failure::stale };
		}
		const auto expected = git_cache_open_request{
			.ApplicationSession = aRequest.ApplicationSession,
			.Authority          = aRequest.Authority,
			.ObjectFormat       = aRequest.Session.Binding.HeadOid.algorithm(),
		};
		// [[BPR-1873]]. A sparse review's cache holds the head's commit
		// and trees and none of its blobs or parents, so the workspace cannot
		// be copied out of it: it borrows the linked clone instead, and the
		// head is never downloaded whole. Without a clone there is nothing to
		// borrow.
		auto context = std::shared_ptr<authority_context>{ };
		{
			std::scoped_lock lock{ actor->BlobAuthorityMutex };
			context = actor->BlobContext.lock();
		}
		const auto sparse = context && blob_authority_for(*context) == authorization && sparse_review(*context);
		if (sparse && !aRequest.HistoryClone) {
			// [[BPR-1873]]. No clone: the agent sees only the pull
			// request's files, and the brief and the turn say so.
			auto files = read_pull_request_files(actor, *context, expected, aRequest.Session.Binding, aStop);
			if (!files) {
				return std::unexpected{ std::move(files.error()) };
			}
			auto cloneLacksHead = false;
			return materialize_on_actor(actor, authorization, *authorizationGeneration, expected, aRequest, nullptr, cloneLacksHead, aStop, std::move(*files));
		}
		// [[BPR-1772]]. A linked clone that already holds the head lends
		// every file of it, so a sparse review asks the clone first. A
		// large head the clone holds is never refused for its size.
		auto delta = std::shared_ptr<git_workspace_delta>{ };
		if (sparse) {
			auto fromClone          = aRequest;
			fromClone.HeadFromClone = true;
			auto cloneLacksHead     = false;
			auto lent               = materialize_on_actor(actor, authorization, *authorizationGeneration, expected, fromClone, nullptr, cloneLacksHead, aStop);
			if (lent.has_value() || !cloneLacksHead) {
				return lent;
			}
			// [[BPR-1884]], RFC-BPR-0092. The clone BetterPullReview keeps
			// is brought up to the pull request instead, with no per-file
			// bound, and lends the head; the next question's fetch is only
			// what is newer. A fetch that fails leaves the clone as it was
			// and the question goes on through the delta below.
			if (aRequest.HistoryCloneManaged) {
				if (refresh_managed_clone(*context, *aRequest.HistoryClone, aStop)) {
					auto again = materialize_on_actor(actor, authorization, *authorizationGeneration, expected, fromClone, nullptr, cloneLacksHead, aStop);
					if (again.has_value() || !cloneLacksHead) {
						return again;
					}
				}
				if (aStop.stop_requested()) {
					return std::unexpected{ agent_workspace_failure::cancelled };
				}
			}
			// [[BPR-1873]]. The clone lacks the head: only what it lacks is
			// fetched, off the actor, under the review's own lease, into a
			// private store; it lives until the workspace made from it is
			// done. Nothing is written to the clone.
			auto acquired = acquire_workspace_delta(actor, *context, expected, aRequest.Session.Binding.HeadOid, *aRequest.HistoryClone, aStop);
			if (!acquired) {
				return std::unexpected{ std::move(acquired.error()) };
			}
			delta = std::move(*acquired);
		}
		auto cloneLacksHead = false;
		return materialize_on_actor(actor, authorization, *authorizationGeneration, expected, aRequest, std::move(delta), cloneLacksHead, aStop);
	}

	/// \brief Makes one workspace on the review's actor, from its cache,
	/// the linked clone, the clone and `aDelta`, or the cache's trees and
	/// `aPullRequestFiles`, and waits for it.
	///
	/// \details `aCloneLacksHead` is set when the request asked for the
	/// clone's head only and the clone does not hold it.
	auto epoch_application_host::implementation::materialize_on_actor(
		const std::shared_ptr<authority_effect_actor>& aActor,
		const blob_authority&                          aAuthorization,
		const std::uint64_t                            aGeneration,
		const git_cache_open_request&                  aExpected,
		const agent_workspace_materialization_request& aRequest,
		std::shared_ptr<git_workspace_delta>           aDelta,
		bool&                                          aCloneLacksHead,
		const std::stop_token                          aStop,
		std::optional<std::vector<git_workspace_file>> aPullRequestFiles)
		-> std::expected<owned_agent_workspace, agent_workspace_error>
	{
		auto source = std::make_shared<git_workspace_source>();
		if (aDelta) {
			source->Delta = git_workspace_delta_pack{
				.Pack  = aDelta->pack(),
				.Bytes = aDelta->pack_bytes(),
				.Head  = aDelta->head(),
			};
		}
		source->PullRequestFiles = std::move(aPullRequestFiles);
		auto waiter              = std::shared_ptr<workspace_waiter>{ };
		try {
			waiter = std::make_shared<workspace_waiter>();
		} catch (...) {
			return std::unexpected{ agent_workspace_failure::unavailable };
		}
		auto admitted = admit_job(
			std::nullopt,
			std::nullopt,
			std::nullopt,
			std::nullopt,
			false);
		if (!admitted.has_value()) {
			return std::unexpected{ agent_workspace_failure::unavailable };
		}
		auto control = std::move(*admitted);
		try {
			enqueue_actor_work(
				aActor,
				[this,
				 aActor,
				 aAuthorization,
				 admittedGeneration = aGeneration,
				 aExpected,
				 aRequest,
				 aStop,
				 waiter,
				 control,
				 aDelta,
				 source] {
					auto result = std::expected<
						owned_agent_workspace,
						agent_workspace_error>{
						std::unexpected{ agent_workspace_failure::unavailable }
					};
					try {
						auto forwardCallerStop = std::stop_callback{
							aStop,
							[control] {
								static_cast<void>(control->Stop.request_stop());
							}
						};
						const auto workStop = control->Stop.get_token();
						// Held, not read: the delta's pack outlives the copy.
						static_cast<void>(aDelta);
						if (
							workStop.stop_requested()
							|| !Consumer->active() || BlobIngressClosing.load(std::memory_order_acquire)) {
							result = std::unexpected{
								agent_workspace_failure::cancelled
							};
						} else if (
							blob_authorization_generation(aActor, aAuthorization)
							!= admittedGeneration) {
							result = std::unexpected{ agent_workspace_failure::stale };
						} else if (
							open_actor_cache(*aActor, Caches, aExpected, workStop).has_value()) {
							auto materialized = aActor->Cache->materialize_agent_workspace(
								aRequest,
								*source,
								workStop);
							if (materialized.has_value()) {
								std::scoped_lock authorityLock{
									aActor->BlobAuthorityMutex
								};
								if (
									workStop.stop_requested()
									|| !Consumer->active()
									|| aActor->BlobAuthority
										!= std::optional{ aAuthorization }
									|| aActor->BlobAuthorityGeneration
										!= admittedGeneration) {
									static_cast<void>(
										aActor->Cache->cleanup_agent_workspace(
											*materialized,
											{ }));
									result = std::unexpected{
										workStop.stop_requested()
											? agent_workspace_failure::cancelled
											: agent_workspace_failure::stale
									};
								} else {
									result = std::move(*materialized);
								}
							} else {
								auto failure = agent_workspace_failure::unavailable;
								if (aRequest.HeadFromClone && materialized.error() == git_cache_failure::object_mismatch) {
									const auto guard       = std::lock_guard{ waiter->Mutex };
									waiter->CloneLacksHead = true;
								}
								if (
									materialized.error()
									== git_cache_failure::cancelled) {
									failure = agent_workspace_failure::cancelled;
								} else if (
									materialized.error()
									== git_cache_failure::invalid_authority) {
									failure = agent_workspace_failure::stale;
								}
								result = std::unexpected{ agent_workspace_error{
									failure,
									std::move(materialized.error().Detail) } };
							}
						}
					} catch (...) {
						result = std::unexpected{ agent_workspace_failure::unavailable };
					}
					auto waiterLock = std::unique_lock{ waiter->Mutex };
					// The caller may have given up while this was queued or
					// running. A materialized workspace it will never read is
					// a directory nobody owns, so remove it here rather than
					// leak it. Decided under the waiter's mutex so the caller
					// cannot abandon between the check and the store; the
					// removal path does not take this mutex, and nothing
					// contends for it once the caller is gone. BPR-0397.
					if (
						waiter->Abandoned && result.has_value()
						&& aActor->Cache != nullptr) {
						static_cast<void>(aActor->Cache->cleanup_agent_workspace(
							*result,
							{ }));
						result = std::unexpected{
							agent_workspace_failure::cancelled
						};
					}
					waiter->Result      = std::move(result);
					const auto reserved = retire_actor_job(control);
					struct actor_exit_reservation final
					{
						implementation& Host;
						bool            Reserved;

						~actor_exit_reservation()
						{
							Host.release_actor_job_reservation(Reserved);
						}
					} actorExit{ *this, reserved };
					waiterLock.unlock();
					waiter->Completed.notify_all();
				});
		} catch (...) {
			finish_job(control);
			return std::unexpected{ agent_workspace_failure::unavailable };
		}

		// Registered before the lock, per `materialize`'s deadlock note.
		auto releaseOnStop = std::stop_callback{ aStop, [waiter] {
													{
														const auto guard  = std::lock_guard{ waiter->Mutex };
														waiter->Abandoned = true;
													}
													waiter->Completed.notify_all();
												} };
		std::unique_lock lock{ waiter->Mutex };
		static_cast<void>(waiter->Completed.wait_for(
			lock, Limits.WorkspaceWait, [&waiter] {
				return waiter->Result.has_value() || waiter->Abandoned;
			}));
		if (waiter->Result.has_value()) {
			aCloneLacksHead = waiter->CloneLacksHead;
			return std::move(*waiter->Result);
		}
		// Bounded by `WorkspaceWait`, not `BlobWait`: this writes a whole
		// tree to disk behind the actor's FIFO. Say which release happened —
		// a caller that moved on is `cancelled`, an actor that never got to
		// the work is `unavailable`.
		return std::unexpected{ waiter->Abandoned
									? agent_workspace_failure::cancelled
									: agent_workspace_failure::unavailable };
	}

	auto epoch_application_host::implementation::cleanup_agent_workspace(
		const owned_agent_workspace& aWorkspace,
		const std::stop_token aStop, const bool aInspect)
		-> std::expected<void, agent_workspace_error>
	{
		if (!Consumer->active() || aWorkspace.ApplicationSession != WindowApplicationSession) {
			return std::unexpected{ agent_workspace_failure::cleanup_failed };
		}
		static_cast<void>(begin_cleanup_call());
		struct cleanup_call_guard final
		{
			implementation& Host;

			~cleanup_call_guard()
			{
				Host.finish_blob_call();
			}
		} call{ *this };
		const auto key = actor_key(aWorkspace);
		if (!key.has_value()) {
			return std::unexpected{ agent_workspace_failure::invalid_request };
		}
		auto actor = std::shared_ptr<authority_effect_actor>{ };
		{
			std::scoped_lock lock{ CacheActorsMutex };
			const auto       found = std::ranges::find(
				CacheActors,
				*key,
				&decltype(CacheActors)::value_type::first);
			if (found != CacheActors.end()) {
				actor = found->second;
			}
		}
		if (!actor) {
			return std::unexpected{ agent_workspace_failure::cleanup_failed };
		}
		auto waiter = std::shared_ptr<workspace_cleanup_waiter>{ };
		try {
			waiter = std::make_shared<workspace_cleanup_waiter>();
		} catch (...) {
			return std::unexpected{ agent_workspace_failure::cleanup_failed };
		}
		auto admitted = admit_job(
			std::nullopt,
			std::nullopt,
			std::nullopt,
			std::nullopt,
			false,
			true);
		if (!admitted.has_value()) {
			return std::unexpected{ agent_workspace_failure::cleanup_failed };
		}
		auto       control  = std::move(*admitted);
		const auto expected = git_cache_open_request{
			.ApplicationSession = aWorkspace.ApplicationSession,
			.Authority          = aWorkspace.Authority,
			.ObjectFormat       = aWorkspace.Workspace.PinnedHead->algorithm(),
		};
		try {
			enqueue_actor_work(
				actor,
				[this, actor, expected, aWorkspace, aStop, waiter, control, aInspect] {
					auto result = std::expected<void, agent_workspace_error>{ };
					try {
						auto forwardCallerStop = std::stop_callback{
							aStop,
							[control] {
								static_cast<void>(control->Stop.request_stop());
							}
						};
						if (
							!open_actor_cache(*actor, Caches, expected, control->Stop.get_token())) {
							result = std::unexpected{
								agent_workspace_failure::cleanup_failed
							};
						} else {
							const auto cleaned = aInspect ? actor->Cache->inspect_agent_workspace(aWorkspace, control->Stop.get_token())
														  : actor->Cache->cleanup_agent_workspace(aWorkspace, control->Stop.get_token());
							if (!cleaned.has_value()) {
								result = std::unexpected{ agent_workspace_error{
									cleaned.error() == git_cache_failure::cancelled
										? agent_workspace_failure::cancelled
										: agent_workspace_failure::cleanup_failed,
									std::move(cleaned.error().Detail) } };
							}
						}
					} catch (...) {
						result = std::unexpected{
							agent_workspace_failure::cleanup_failed
						};
					}
					auto waiterLock     = std::unique_lock{ waiter->Mutex };
					waiter->Result      = std::move(result);
					const auto reserved = retire_actor_job(control);
					struct actor_exit_reservation final
					{
						implementation& Host;
						bool            Reserved;

						~actor_exit_reservation()
						{
							Host.release_actor_job_reservation(Reserved);
						}
					} actorExit{ *this, reserved };
					waiterLock.unlock();
					waiter->Completed.notify_all();
				});
		} catch (...) {
			finish_job(control);
			return std::unexpected{ agent_workspace_failure::cleanup_failed };
		}

		// Registered before the lock, per `materialize`'s deadlock note.
		auto releaseOnStop = std::stop_callback{ aStop, [waiter] {
													{
														const auto guard  = std::lock_guard{ waiter->Mutex };
														waiter->Abandoned = true;
													}
													waiter->Completed.notify_all();
												} };
		std::unique_lock lock{ waiter->Mutex };
		static_cast<void>(waiter->Completed.wait_for(
			lock, Limits.WorkspaceWait, [&waiter] {
				return waiter->Result.has_value() || waiter->Abandoned;
			}));
		if (waiter->Result.has_value()) {
			return std::move(*waiter->Result);
		}
		// Abandoning this one leaves nothing behind: the removal still runs
		// on the actor, and only its outcome goes unread. So the caller is
		// told what released it and nothing else needs undoing. BPR-0397.
		return std::unexpected{ waiter->Abandoned
									? agent_workspace_failure::cancelled
									: agent_workspace_failure::cleanup_failed };
	}

	void epoch_application_host::implementation::accept(accepted_pull_request_handoff aHandoff)
	{
		if (!Consumer->current() || !application::valid(aHandoff)
			|| aHandoff.ApplicationSession != State.ApplicationSession) {
			if (aHandoff.SmartGit) {
				aHandoff.SmartGit->revoke();
			}
			return;
		}
		if (Closing || DeferredShutdown) {
			if (aHandoff.SmartGit) {
				aHandoff.SmartGit->revoke();
			}
			return;
		}
		auto accepted = std::shared_ptr<accepted_pull_request_handoff>{ };
		try {
			accepted = std::make_shared<accepted_pull_request_handoff>(
				std::move(aHandoff));
		} catch (...) {
			if (aHandoff.SmartGit) {
				aHandoff.SmartGit->revoke();
			}
			if (TurnActive) {
				defer_diagnostic(core::epoch_reason_code::unavailable);
			} else {
				publish_diagnostic(core::epoch_reason_code::unavailable);
			}
			return;
		}
		if (TurnActive || deferred_work_pending()) {
			static_cast<void>(defer_ordinary(
				deferred_accept{ std::move(accepted) }));
			return;
		}
		run_turn([this, &accepted] {
			accept_now(std::move(*accepted));
		});
	}

	void epoch_application_host::implementation::accept_now(accepted_pull_request_handoff aHandoff)
	{
		if (!Consumer->current() || Closing) {
			if (aHandoff.SmartGit) {
				aHandoff.SmartGit->revoke();
			}
			return;
		}
		auto actor = std::shared_ptr<authority_effect_actor>{ };
		try {
			actor = actor_for(actor_key(aHandoff));
		} catch (...) {
			if (aHandoff.SmartGit) {
				aHandoff.SmartGit->revoke();
			}
			publish_diagnostic(core::epoch_reason_code::unavailable);
			return;
		}
		if (!actor) {
			if (aHandoff.SmartGit) {
				aHandoff.SmartGit->revoke();
			}
			publish_diagnostic(core::epoch_reason_code::quota_exceeded);
			return;
		}
		const auto token   = OfferToken + 1U;
		auto       offered = std::shared_ptr<accepted_pull_request_handoff>{ };
		try {
			offered = std::make_shared<accepted_pull_request_handoff>(
				std::move(aHandoff));
		} catch (...) {
			if (aHandoff.SmartGit) {
				aHandoff.SmartGit->revoke();
			}
			publish_diagnostic(core::epoch_reason_code::unavailable);
			return;
		}
		auto admitted = admit_job(
			std::nullopt, std::nullopt, std::nullopt, token);
		if (!admitted) {
			if (offered->SmartGit) {
				offered->SmartGit->revoke();
			}
			publish_diagnostic(core::reason_for(admitted.error()));
			return;
		}
		auto control = std::move(*admitted);
		auto cleanup = std::shared_ptr<pending_context_cleanup>{ };
		if (Active && Active->requires_cache_cleanup()) {
			try {
				cleanup = std::make_shared<pending_context_cleanup>(
					pending_context_cleanup{ .Context = Active });
				auto cleanupAdmission = admit_job(
					std::nullopt,
					std::nullopt,
					std::nullopt,
					std::nullopt,
					true,
					true);
				if (!cleanupAdmission) {
					finish_job(control);
					if (offered->SmartGit) {
						offered->SmartGit->revoke();
					}
					publish_diagnostic(core::reason_for(cleanupAdmission.error()));
					return;
				}
				cleanup->Control = std::move(*cleanupAdmission);
			} catch (...) {
				finish_job(control);
				release_pending_cleanup(cleanup);
				if (offered->SmartGit) {
					offered->SmartGit->revoke();
				}
				publish_diagnostic(core::epoch_reason_code::unavailable);
				return;
			}
		}
		struct offer_admission_rollback final
		{
			implementation&                                Host;
			std::shared_ptr<job_control>                   Control;
			std::shared_ptr<accepted_pull_request_handoff> Offered;
			std::shared_ptr<pending_context_cleanup>       Cleanup;
			std::uint64_t                                  Token;
			bool                                           Armed    = true;
			bool                                           Diagnose = true;

			~offer_admission_rollback()
			{
				if (!Armed) {
					return;
				}
				Host.finish_job(Control);
				Host.release_pending_cleanup(Cleanup);
				if (Token == Host.OfferToken) {
					Host.PendingAuthority.reset();
					Host.PendingOfferToken.reset();
				}
				if (Offered->SmartGit) {
					Offered->SmartGit->revoke();
				}
				if (Diagnose) {
					Host.publish_diagnostic(core::epoch_reason_code::unavailable);
				}
			}

			void dismiss() noexcept
			{
				Armed = false;
			}
		} rollback{ *this, control, offered, cleanup, token };
		++OfferToken;
		if (PendingOfferToken) {
			cancel_offer_job(*PendingOfferToken);
		}
		const bool preservesActiveAuthority = Active && agrees(Active->Authority, *offered);
		if (Active && !preservesActiveAuthority) {
			if (cleanup
				&& !discard_context_preparation(
					cleanup->Context,
					std::nullopt,
					cleanup->Control)) {
				rollback.Diagnose = false;
				return;
			}
			if (cleanup) {
				cleanup->Context.reset();
				cleanup->Control.reset();
			}
		}

		PendingAuthority     = invalidation_identity(*offered);
		PendingOfferToken    = token;
		auto* const catalogs = &Catalogs;
		auto* const caches   = &Caches;
		auto* const entropy  = &Entropy;
		auto* const clock    = &Clock;
		// Keep worker and scheduling ownership distinct before entering the
		// call. Capturing and moving one local in separate arguments would
		// make the worker's actor depend on argument evaluation order.
		auto       workerActor = actor;
		const auto submitted   = submit_effect(
			[offered,
			 effectActor = std::move(workerActor),
			 catalogs,
			 caches,
			 entropy,
			 clock,
			 token,
			 cleanup, diagnostics = epoch_diagnostics{ DiagnosticLog, 0U, offered->ApplicationSession.value() }](
				const std::stop_token aStop) mutable -> worker_completion {
				struct lease_guard final
				{
					git_smart_authority Authority;

					~lease_guard()
					{
						if (Authority && !Authority->revoked()) {
							Authority->revoke();
						}
					}

					[[nodiscard]] auto release() noexcept -> git_smart_authority
					{
						return std::move(Authority);
					}
				} lease{ std::move(offered->SmartGit) };
				const auto diagnostic = [token, cleanup](
											const core::epoch_reason_code aReason)
					-> worker_completion {
					return [token, aReason, cleanup](implementation& aHost) {
						aHost.release_pending_cleanup(cleanup);
						if (token != aHost.OfferToken) {
							return;
						}
						aHost.PendingAuthority.reset();
						aHost.PendingOfferToken.reset();
						aHost.publish_diagnostic(aReason);
					};
				};
				if (aStop.stop_requested()) {
					return [cleanup](implementation& aHost) {
						aHost.release_pending_cleanup(cleanup);
					};
				}
				auto opened = diagnostics.perform(
					diagnostic_epoch_stage::catalog_open,
					[&] { return catalogs->open(
							  epoch_catalog_open_request{
								  .ApplicationSession = offered->ApplicationSession,
								  .Authority          = catalog_key(*offered),
							  },
							  aStop); });
				if (!opened) {
					const auto reason = core::reason_for(
						catalog_failure(opened.error()));
					return diagnostic(reason);
				}
				if (!*opened) {
					return diagnostic(
						core::epoch_reason_code::repair_required);
				}

				const auto initialBinding = (*opened)->binding();
				if (!application::valid(initialBinding)
					|| initialBinding.ObjectFormat != offered->ObjectFormat
					|| !agrees(initialBinding.Authority, *offered)) {
					return diagnostic(core::epoch_reason_code::invalid_identity);
				}

				const auto expectedCache = git_cache_open_request{
					.ApplicationSession = offered->ApplicationSession,
					.Authority          = initialBinding.Authority,
					.ObjectFormat       = offered->ObjectFormat,
				};
				if (effectActor->Cache && effectActor->Cache->request() != expectedCache) {
					effectActor->Cache.reset();
				}
				const auto recovered = diagnostics.perform(
					diagnostic_epoch_stage::catalog_recovery,
					[&] { return recover_catalog(
							  **opened,
							  *caches,
							  *entropy,
							  *offered,
							  effectActor->Cache,
							  aStop); });
				if (!recovered) {
					return diagnostic(core::reason_for(recovered.error()));
				}

				// Occurrence evidence is supplementary to the live revision.
				// A failed timeline or sidecar read never refuses the accepted PR.
				auto occurrenceHistory = std::optional<core::epoch_occurrence_history>{ };
				auto currentEpoch      = std::optional<core::review_epoch_summary>{ };
				try {
					if (auto stored = (*opened)->occurrences(aStop)) {
						// Publish exact stored evidence even when optional reconciliation refuses.
						occurrenceHistory = *stored;
						if (const auto snapshot = (*opened)->snapshot(aStop);
							snapshot && snapshot->Binding == (*opened)->binding()) {
							currentEpoch = snapshot->LatestLive;
						}
						if (const auto observed = clock->observe_utc(); observed && core::valid(*observed)) {
							auto reconciled = reconcile_timeline(*offered, std::move(*stored), *observed);
							reconciled      = reconcile_local_occurrences(*offered, std::move(reconciled), **opened, currentEpoch, aStop);
							// [[BPR-1340]]. A new process has no cache open yet, and only a
							// new head's commit opened one, so an update recorded undated
							// stayed "Date unavailable" while the head did not move. Open the
							// actor's cache here when there is a date to read; a refusal
							// leaves the date unavailable and the offer unchanged.
							if (std::ranges::any_of(reconciled.Occurrences, [](const auto& aOccurrence) {
									return aOccurrence.DateSource == core::epoch_occurrence_date_source::unavailable;
								})) {
								static_cast<void>(open_actor_cache(*effectActor, *caches, expectedCache, aStop));
							}
							enrich_commit_dates(reconciled, effectActor->Cache.get(), aStop);
							std::ranges::sort(reconciled.Occurrences, core::epoch_occurrence_less);
							if ((*opened)->record_occurrences(reconciled, aStop)) {
								occurrenceHistory = std::move(reconciled);
							}
						}
					}
				} catch (...) {
					// Keep stored evidence, or the explicit unavailable fallback.
					// Publication must not depend on supplementary persistence.
				}

				std::optional<core::epoch_source_observation_id> source;
				for (std::size_t attempt = 0U;
					 attempt < maximum_identity_attempts;
					 ++attempt) {
					source = mint_identity<core::epoch_source_observation_id>(
						*entropy);
					if (!source) {
						break;
					}
					const auto exists = diagnostics.perform(
						diagnostic_epoch_stage::catalog_prepare,
						[&] { return (*opened)->source_observation_exists(*source, aStop); });
					if (!exists) {
						const auto reason = core::reason_for(
							catalog_failure(exists.error()));
						return diagnostic(reason);
					}
					if (!*exists) {
						break;
					}
					source.reset();
				}
				if (!source || aStop.stop_requested()) {
					return diagnostic(core::epoch_reason_code::unavailable);
				}

				const auto binding = (*opened)->binding();
				if (!application::valid(binding)
					|| binding.ObjectFormat != offered->ObjectFormat
					|| !agrees(binding.Authority, *offered)) {
					return diagnostic(core::epoch_reason_code::invalid_identity);
				}
				auto revision = accepted_revision(*offered, *source);
				auto context  = std::make_shared<authority_context>(
					std::move(*offered),
					binding.Authority,
					revision,
					std::move(*opened),
					std::move(effectActor),
					std::move(occurrenceHistory),
					std::move(currentEpoch));
				context->Handoff.SmartGit = lease.release();
				return [authorityContext = std::move(context), token, cleanup](
						   implementation& aHost) {
					if (aHost.Closing || token != aHost.OfferToken) {
						aHost.release_pending_cleanup(cleanup);
						if (authorityContext->Handoff.SmartGit) {
							authorityContext->Handoff.SmartGit->revoke();
						}
						return;
					}
					aHost.PendingAuthority.reset();
					aHost.PendingOfferToken.reset();
					auto previous = aHost.Active;
					if (cleanup && cleanup->Control) {
						if (!aHost.discard_context_preparation(
								cleanup->Context,
								std::nullopt,
								cleanup->Control)) {
							cleanup->Control.reset();
							cleanup->Context.reset();
							if (authorityContext->Handoff.SmartGit) {
								authorityContext->Handoff.SmartGit->revoke();
							}
							return;
						}
						cleanup->Control.reset();
						cleanup->Context.reset();
					}
					// RFC-BPR-0089 M4. A refresh that keeps the review and its
					// head keeps the chain its touches job diffed: the same
					// commits may still be read, under the new snapshot. Without
					// it every filtered diff stopped reading after a refresh, as
					// the reread after each publication is.
					auto keptFilter = std::optional<std::tuple<workbench_pull_request_authority, std::vector<core::git_commit_oid>, std::shared_ptr<git_chain_tree_store>>>{ };
					if (previous && previous->Actor == authorityContext->Actor && previous->Authority == authorityContext->Authority
						&& previous->Revision.Head == authorityContext->Revision.Head) {
						std::scoped_lock filterLock{ previous->Actor->BlobAuthorityMutex };
						if (previous->Actor->FilterAuthority && !previous->Actor->FilterCommits.empty()) {
							auto rebased                  = *previous->Actor->FilterAuthority;
							rebased.Snapshot              = authorityContext->Revision.Source.Snapshot;
							rebased.IdentityGeneration    = authorityContext->Revision.Source.IdentityGeneration;
							rebased.PullRequestGeneration = authorityContext->Revision.Source.PullRequestGeneration;
							keptFilter.emplace(std::move(rebased), previous->Actor->FilterCommits, previous->Actor->ChainTrees);
						}
					}
					aHost.Active  = authorityContext;
					bool accepted = false;
					static_cast<void>(aHost.dispatch_core(
						core::pull_request_revision_accepted_event{
							.ApplicationSession = authorityContext->Handoff.ApplicationSession,
							.Authority          = authorityContext->Authority,
							.Revision           = authorityContext->Revision,
							.Basis              = authorityContext->Handoff.BaseBasis,
						},
						[&](const core::epoch_transition_output&) {
							accepted = aHost.State.Authority
								&& aHost.State.AcceptedRevision
								&& *aHost.State.Authority
									== authorityContext->Authority
								&& *aHost.State.AcceptedRevision
									== authorityContext->Revision;
							if (accepted && previous && previous->Authority == authorityContext->Authority && previous->Revision.Head != authorityContext->Revision.Head) {
								aHost.remember_earlier_head(previous->Authority, previous->Revision.Head);
							}
							if (accepted && previous) {
								implementation::revoke_blob_authority(
									previous->Actor,
									implementation::blob_authority_for(*previous));
								if (previous->Handoff.SmartGit) {
									previous->Handoff.SmartGit->revoke();
								}
							}
						}));
					if (accepted) {
						if (!authorityContext->OccurrenceHistory && previous
							&& previous->Authority == authorityContext->Authority && previous->OccurrenceHistory) {
							// A failed read must clear previously projected evidence for this authority.
							authorityContext->OccurrenceHistory = core::epoch_occurrence_history{ };
						}
						if (authorityContext->OccurrenceHistory) {
							aHost.publish_occurrences(
								authorityContext->Catalog->binding().Catalog,
								*authorityContext->OccurrenceHistory,
								authorityContext->CurrentEpoch);
						}
						const auto invalidation = invalidation_identity(
							authorityContext->Handoff);
						const auto invalidationPending = std::ranges::any_of(
							aHost.DeferredInvalidations,
							[&](const auto& aPending) {
								return aPending == std::optional{ invalidation };
							});
						if (
							!aHost.Closing && !aHost.DeferredShutdown
							&& !invalidationPending) {
							implementation::authorize_blobs(
								authorityContext->Actor,
								implementation::blob_authority_for(*authorityContext),
								authorityContext);
							if (keptFilter) {
								std::scoped_lock filterLock{ authorityContext->Actor->BlobAuthorityMutex };
								auto& [filterAuthority, filterCommits, chainTrees] = *keptFilter;
								authorityContext->Actor->FilterAuthority           = std::move(filterAuthority);
								authorityContext->Actor->FilterCommits             = std::move(filterCommits);
								authorityContext->Actor->ChainTrees                = std::move(chainTrees);
							}
						}
						return;
					}
					const auto previousStillOwned = previous
						&& aHost.State.Authority
						&& aHost.State.AcceptedRevision
						&& *aHost.State.Authority == previous->Authority
						&& *aHost.State.AcceptedRevision
							== previous->Revision;
					if (previousStillOwned) {
						aHost.Active = std::move(previous);
						if (authorityContext->Handoff.SmartGit) {
							authorityContext->Handoff.SmartGit->revoke();
						}
						return;
					}
					aHost.Active.reset();
					if (authorityContext->Handoff.SmartGit) {
						authorityContext->Handoff.SmartGit->revoke();
					}
					if (previous) {
						implementation::revoke_blob_authority(
							previous->Actor,
							implementation::blob_authority_for(*previous));
						if (previous->Handoff.SmartGit) {
							previous->Handoff.SmartGit->revoke();
						}
					}
				};
			},
			[token, cleanup](implementation& aHost) {
				aHost.release_pending_cleanup(cleanup);
				if (token == aHost.OfferToken) {
					aHost.PendingAuthority.reset();
					aHost.PendingOfferToken.reset();
					aHost.publish_diagnostic(
						core::epoch_reason_code::unavailable);
				}
			},
			[token, cleanup](
				implementation& aHost, const core::epoch_failure aFailure) {
				aHost.release_pending_cleanup(cleanup);
				if (token == aHost.OfferToken) {
					aHost.PendingAuthority.reset();
					aHost.PendingOfferToken.reset();
					aHost.publish_diagnostic(core::reason_for(aFailure));
				}
			},
			std::nullopt,
			std::nullopt,
			std::nullopt,
			token,
			std::move(actor),
			std::move(control));
		rollback.dismiss();
		if (!submitted) {
			if (token == OfferToken) {
				PendingAuthority.reset();
				PendingOfferToken.reset();
				if (offered->SmartGit) {
					offered->SmartGit->revoke();
				}
			}
			return;
		}
		if (Active) {
			cancel_authority_jobs();
		}
		if (Active && !preservesActiveAuthority) {
			auto retired = std::move(Active);
			revoke_blob_authority(
				retired->Actor, blob_authority_for(*retired));
			if (retired->Handoff.SmartGit) {
				retired->Handoff.SmartGit->revoke();
			}
			dispatch_core(core::pull_request_revision_invalidated_event{
				.ApplicationSession = State.ApplicationSession,
				.Authority          = retired->Authority,
				.Revision           = retired->Revision,
				.Generation         = State.EpochGeneration,
			});
		}
	}
}
