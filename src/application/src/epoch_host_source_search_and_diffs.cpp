#include "epoch_host_implementation.hpp"

#include "bpr/application/git_process_port.hpp"
#include "bpr/core/comparison_preparation.hpp"
#include "bpr/core/epoch_transition.hpp"
#include "bpr/core/window_review_restoration.hpp"

#include <chrono>
#include <initializer_list>
#include <limits>
#include <string_view>
#include <thread>

namespace bpr::application {
	/// \brief Proves a comparison endpoint is durably sparse: the only
	/// state in which a missing blob may be read from the network.
	///
	/// \details Runs on the actor, which owns the cache session. Factored
	/// out of `materialize` for [[BPR-1327]], whose batched read for
	/// prepared files requires exactly the same proof.
	auto epoch_application_host::implementation::durably_sparse_anchor(
		authority_effect_actor&        aActor,
		authority_context&             aContext,
		const core::comparison_anchor& aAnchorToProve,
		const std::stop_token          aStop) -> bool
	{
		const auto snapshot = aContext.Catalog->snapshot(aStop);
		if (!snapshot || !snapshot->LatestLive
			|| snapshot->LatestLive->Base != aContext.Revision.Base
			|| snapshot->LatestLive->Head != aContext.Revision.Head) {
			return false;
		}
		const auto exactSparseAnchor = [&](const core::comparison_anchor& aAnchor) {
			if (!core::valid(aAnchor)
				|| aAnchor.Availability != core::anchor_availability::available
				|| (aAnchor.Identity.Kind != core::comparison_anchor_kind::epoch_base
					&& aAnchor.Identity.Kind != core::comparison_anchor_kind::epoch_head)
				|| aAnchor.Identity.Authority != aContext.Authority.Scope
				|| aAnchor.Identity.Incarnation != snapshot->Binding.Catalog.Incarnation) {
				return false;
			}
			const auto exact = aContext.Catalog->exact_anchors(
				exact_anchor_lookup_request{
					.Catalog = snapshot->Binding.Catalog,
					.Anchors = { aAnchor.Identity },
				},
				aStop);
			if (!exact || exact->Catalog != snapshot->Binding.Catalog
				|| exact->Anchors.size() != 1U || exact->Anchors.front() != aAnchor) {
				return false;
			}
			const auto recorded = aContext.Catalog->retained_residency(
				aAnchor.Identity.Epoch, aStop);
			if (!recorded || *recorded != git_revision_residency::sparse_commit_trees) {
				return false;
			}
			const auto retained = aActor.Cache->inspect_sparse_retained(
				aAnchor.Identity.Epoch, aStop);
			if (!retained || !application::valid(*retained)
				|| retained->Residency != git_revision_residency::sparse_commit_trees
				|| retained->Scope != aContext.Authority.Scope
				|| retained->Ordinal != aAnchor.Identity.Epoch
				|| (aAnchor.Identity.Kind == core::comparison_anchor_kind::epoch_base
						? retained->Base
						: retained->Head)
					!= aAnchor.Commit) {
				return false;
			}
			const auto inspected = aActor.Cache->inspect_sparse_commit_object(
				aAnchor.Commit, aStop);
			return inspected && application::valid(*inspected)
				&& inspected->Commit == aAnchor.Commit
				&& inspected->Residency == git_revision_residency::sparse_commit_trees;
		};
		return exactSparseAnchor(aAnchorToProve);
	}

	/// \details [[BPR-1327]]. `aPrepared` resolves a file the reviewer has
	/// not opened yet: the same lookups under the same held authority, kept
	/// as a prepared grant instead of replacing the selected one. A later
	/// selection of that exact file is answered from its prepared grant
	/// without running Git again.
	auto epoch_application_host::implementation::resolve_selected_diff(
		const workbench_selected_diff_request& aRequest,
		const std::stop_token                  aStop,
		const bool                             aPrepared)
		-> workbench_selected_diff_observation
	{
		auto result = workbench_selected_diff_observation{
			.Request = aRequest,
			.Status  = workbench_selected_diff_status::failure,
		};
		if (aRequest.File.ContentKind == workbench_file_content_kind::binary) {
			result.Status = workbench_selected_diff_status::binary;
			return result;
		}
		// RFC-BPR-0089. A split file has no one exact diff; nothing is read
		// unless the request names one of its per-commit diffs (M3).
		if (aRequest.File.SplitByFilter && !aRequest.Filter) {
			result.Status = workbench_selected_diff_status::split_by_commit_filter;
			return result;
		}
		if (!begin_blob_call()) {
			return result;
		}
		struct selected_call_guard final
		{
			implementation& Host;

			~selected_call_guard()
			{
				Host.finish_blob_call();
			}
		} call{ *this };
		const auto key = actor_key(aRequest);
		if (!key.has_value()) {
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
		const auto authorizationGeneration
			= selected_diff_authorization_generation(actor, aRequest);
		if (!authorizationGeneration.has_value()) {
			result.Status = workbench_selected_diff_status::stale;
			return result;
		}
		std::uint64_t grantSequence    = 0U;
		std::uint64_t preparedSequence = 0U;
		bool          selectedReview   = false;
		{
			std::scoped_lock lock{ actor->BlobAuthorityMutex };
			selectedReview   = actor->SelectedDiffAuthority == std::optional{ selected_diff_authority{
								   .Workbench = aRequest.WorkbenchAuthority,
								   .Diff      = aRequest.Diff,
							   } };
			preparedSequence = actor->PreparedGrantSequence;
			if (aPrepared) {
				if (!selectedReview || actor->BlobAuthorityGeneration != *authorizationGeneration) {
					result.Status = workbench_selected_diff_status::stale;
					return result;
				}
			} else if (selectedReview) {
				const auto sameSelection = actor->SelectedBlobGrant
					&& actor->SelectedBlobGrant->Request.WorkbenchAuthority == aRequest.WorkbenchAuthority
					&& actor->SelectedBlobGrant->Request.Diff == aRequest.Diff
					&& actor->SelectedBlobGrant->Request.File == aRequest.File
					&& actor->SelectedBlobGrant->Request.Filter == aRequest.Filter;
				if (!sameSelection) {
					actor->SelectedBlobGrant.reset();
					++actor->SelectedBlobGrantSequence;
				}
				grantSequence = actor->SelectedBlobGrantSequence;
				if (actor->BlobAuthorityGeneration == *authorizationGeneration) {
					const auto prepared = std::ranges::find_if(
						actor->PreparedGrants, [&aRequest](const workbench_selected_diff_observation& aGrant) {
							return aGrant.Request.WorkbenchAuthority == aRequest.WorkbenchAuthority
								&& aGrant.Request.Diff == aRequest.Diff && aGrant.Request.File == aRequest.File
								&& aGrant.Request.Filter == aRequest.Filter;
						});
					if (prepared != actor->PreparedGrants.end()) {
						// [[BPR-1327]]. Prepared under this exact authority: the
						// same blobs and paths, bound to this request.
						auto reused = workbench_selected_diff_observation{
							.Request   = aRequest,
							.Status    = workbench_selected_diff_status::available,
							.LeftBlob  = prepared->LeftBlob,
							.RightBlob = prepared->RightBlob,
							.LeftPath  = prepared->LeftPath,
							.RightPath = prepared->RightPath,
						};
						if (reused.LeftBlob && reused.LeftPath) {
							reused.LeftEndpoint = epoch_application_host::mint_selected_diff_endpoint(
								aRequest, core::diff_side::left, *reused.LeftBlob, *reused.LeftPath);
						}
						if (reused.RightBlob && reused.RightPath) {
							reused.RightEndpoint = epoch_application_host::mint_selected_diff_endpoint(
								aRequest, core::diff_side::right, *reused.RightBlob, *reused.RightPath);
						}
						actor->SelectedBlobGrant = reused;
						return reused;
					}
				}
			}
		}
		auto waiter = std::shared_ptr<selected_diff_waiter>{ };
		try {
			waiter = std::make_shared<selected_diff_waiter>();
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
			aPrepared);
		if (!admitted.has_value()) {
			return result;
		}
		auto       control  = std::move(*admitted);
		const auto expected = git_cache_open_request{
			.ApplicationSession = aRequest.Diff.ApplicationSession,
			.Authority          = aRequest.Diff.Authority,
			.ObjectFormat       = aRequest.Diff.Comparison.Left.Commit.algorithm(),
		};
		try {
			// [[BPR-1359]]. The reviewer's own resolve does not wait out a
			// prepared exact read; a prepared resolve is preparation.
			auto turn = aPrepared ? std::shared_ptr<reviewer_work>{ } : std::make_shared<reviewer_work>(actor);
			enqueue_actor_work(
				actor,
				[this,
				 actor,
				 admittedGeneration = *authorizationGeneration,
				 grantSequence,
				 preparedSequence,
				 selectedReview,
				 aPrepared,
				 expected,
				 aRequest,
				 aStop,
				 waiter,
				 control,
				 reviewer = std::move(turn)] {
					auto observation = workbench_selected_diff_observation{
						.Request = aRequest,
						.Status  = workbench_selected_diff_status::failure,
					};
					auto publicationAuthorityLock = std::unique_lock<std::mutex>{ };
					try {
						auto forwardCallerStop = std::stop_callback{
							aStop,
							[control] {
								static_cast<void>(control->Stop.request_stop());
							}
						};
						const auto workStop = control->Stop.get_token();
						if (workStop.stop_requested()
							|| !Consumer->active() || BlobIngressClosing.load(std::memory_order_acquire)) {
							observation.Status = workbench_selected_diff_status::cancelled;
						} else if (selected_diff_authorization_generation(actor, aRequest)
								   != admittedGeneration) {
							observation.Status = workbench_selected_diff_status::stale;
						} else if (open_actor_cache(*actor, Caches, expected, workStop).has_value()) {
							// RFC-BPR-0089. A sparse review's chain commits are in its
							// transient chain store, not its cache.
							const auto chainTrees = [&] {
								std::scoped_lock chainLock{ actor->BlobAuthorityMutex };
								return aRequest.Filter ? actor->ChainTrees : std::shared_ptr<git_chain_tree_store>{ };
							}();
							const auto lookup = [&](const std::filesystem::path& aPath,
													const core::git_commit_oid   aCommit)
								-> git_cache_result<std::optional<core::git_blob_oid>> {
								if (chainTrees && chainTrees->holds(aCommit)) {
									const auto text  = aPath.generic_u8string();
									const auto found = chainTrees->find_blob_at_path(aCommit, std::string{ reinterpret_cast<const char*>(text.data()), text.size() }, workStop);
									if (!found) {
										return std::unexpected{ found.error().Failure == git_smart_transport_failure::cancelled ? git_cache_failure::cancelled
																																: git_cache_failure::object_mismatch };
									}
									return *found;
								}
								return actor->Cache->find_blob_at_path(
									git_cache_tree_entry_request{
										.Commit = aCommit,
										.Path   = aPath,
									},
									workStop);
							};
							struct endpoint_resolution final
							{
								std::optional<core::git_blob_oid>    Blob;
								std::optional<std::filesystem::path> Path;
								bool                                 Ambiguous = false;
							};
							const auto resolveEndpoint = [&](const core::git_commit_oid aCommit)
								-> git_cache_result<endpoint_resolution> {
								const auto current = lookup(aRequest.File.CurrentPath, aCommit);
								if (!current.has_value()) {
									return std::unexpected{ current.error() };
								}
								if (aRequest.File.Change
									!= workbench_file_change_kind::renamed) {
									return endpoint_resolution{
										.Blob = *current,
										.Path = current->has_value()
											? std::optional{ aRequest.File.CurrentPath }
											: std::nullopt,
									};
								}
								const auto previous = lookup(*aRequest.File.PreviousPath, aCommit);
								if (!previous.has_value()) {
									return std::unexpected{ previous.error() };
								}
								if (current->has_value() && previous->has_value()) {
									return endpoint_resolution{ .Ambiguous = true };
								}
								return endpoint_resolution{
									.Blob = current->has_value() ? *current : *previous,
									.Path = current->has_value()
										? std::optional{ aRequest.File.CurrentPath }
										: previous->has_value()
										? aRequest.File.PreviousPath
										: std::nullopt,
								};
							};
							// RFC-BPR-0089. A filtered request reads its file at the two
							// chain commits it names, authorized above.
							const auto leftCommit     = aRequest.Filter ? aRequest.Filter->Left : aRequest.Diff.Comparison.Left.Commit;
							const auto rightCommit    = aRequest.Filter ? aRequest.Filter->Right : aRequest.Diff.Comparison.Right.Commit;
							auto       left           = resolveEndpoint(leftCommit);
							auto       right          = left.has_value()
								? resolveEndpoint(rightCommit)
								: git_cache_result<endpoint_resolution>{
									  std::unexpected{ left.error() }
								  };
							auto       endpointsReady = left.has_value() && right.has_value();
							if (!endpointsReady) {
								const auto failure = !left.has_value()
									? left.error()
									: right.error();
								observation.Status = failure == git_cache_failure::cancelled
									? workbench_selected_diff_status::cancelled
									: workbench_selected_diff_status::failure;
							}
							if (endpointsReady
								&& aRequest.File.Change
									== workbench_file_change_kind::renamed) {
								const auto leftNeedsPair
									= left->Ambiguous || !left->Blob.has_value();
								const auto rightNeedsPair
									= right->Ambiguous || !right->Blob.has_value();
								if (leftNeedsPair || rightNeedsPair) {
									const auto pairs = actor->Cache->find_rename_pairs(
										git_cache_rename_pair_request{
											.Left         = leftCommit,
											.Right        = rightCommit,
											.CurrentPath  = aRequest.File.CurrentPath,
											.PreviousPath = aRequest.File.PreviousPath,
										},
										workStop);
									if (!pairs.has_value()) {
										observation.Status
											= pairs.error() == git_cache_failure::cancelled
											? workbench_selected_diff_status::cancelled
											: workbench_selected_diff_status::failure;
										endpointsReady = false;
									} else {
										auto boundPair = std::optional<git_rename_pair>{ };
										for (auto index = std::size_t{ 0U };
											 index < pairs->Count;
											 ++index) {
											const auto& pair     = pairs->Records[index];
											const auto  accepted = [&](const std::filesystem::path& aPath) {
												return aPath == aRequest.File.CurrentPath
													|| (aRequest.File.PreviousPath.has_value()
														&& aPath == *aRequest.File.PreviousPath);
											};
											if ((!left->Path.has_value()
												 || pair.LeftPath == *left->Path)
												&& (!right->Path.has_value()
													|| pair.RightPath == *right->Path)
												&& (accepted(pair.LeftPath)
													|| accepted(pair.RightPath))) {
												if (boundPair.has_value()) {
													boundPair.reset();
													break;
												}
												boundPair = pair;
											}
										}
										if (!boundPair.has_value()) {
											observation.Status = leftNeedsPair && rightNeedsPair
												? workbench_selected_diff_status::ambiguous_both
												: leftNeedsPair
												? workbench_selected_diff_status::ambiguous_left
												: workbench_selected_diff_status::ambiguous_right;
											endpointsReady     = false;
										} else {
											const auto exactLeft = lookup(
												boundPair->LeftPath,
												leftCommit);
											const auto exactRight = exactLeft.has_value()
												? lookup(
													  boundPair->RightPath,
													  rightCommit)
												: git_cache_result<std::optional<core::git_blob_oid>>{
													  std::unexpected{ exactLeft.error() }
												  };
											if (!exactLeft.has_value() || !exactRight.has_value()) {
												const auto failure = !exactLeft.has_value()
													? exactLeft.error()
													: exactRight.error();
												observation.Status
													= failure == git_cache_failure::cancelled
													? workbench_selected_diff_status::cancelled
													: workbench_selected_diff_status::failure;
												endpointsReady = false;
											} else if (!exactLeft->has_value()
													   || !exactRight->has_value()
													   || (left->Blob.has_value()
														   && left->Blob != *exactLeft)
													   || (right->Blob.has_value()
														   && right->Blob != *exactRight)) {
												observation.Status
													= workbench_selected_diff_status::failure;
												endpointsReady = false;
											} else {
												left = endpoint_resolution{
													.Blob = *exactLeft,
													.Path = boundPair->LeftPath,
												};
												right = endpoint_resolution{
													.Blob = *exactRight,
													.Path = boundPair->RightPath,
												};
											}
										}
									}
								}
							}
							if (endpointsReady && !left->Blob.has_value()
								&& !right->Blob.has_value()) {
								observation.Status = workbench_selected_diff_status::both_missing;
								endpointsReady     = false;
							}
							// Provider M/type/unchanged facts require material on both
							// selected sides. A different comparison basis may genuinely
							// lack the path; report that instead of publishing a normal M
							// with invented empty source or substituting another commit.
							const auto requiresBoth = aRequest.File.Change == workbench_file_change_kind::modified
								|| aRequest.File.Change == workbench_file_change_kind::type_changed
								|| aRequest.File.Change == workbench_file_change_kind::unchanged;
							if (endpointsReady && requiresBoth && (!left->Blob || !right->Blob)) {
								observation.Status = !left->Blob ? workbench_selected_diff_status::missing_left
																 : workbench_selected_diff_status::missing_right;
								endpointsReady     = false;
							}
							if (endpointsReady) {
								publicationAuthorityLock = std::unique_lock{
									actor->BlobAuthorityMutex
								};
								if (workStop.stop_requested()
									|| !Consumer->active() || BlobIngressClosing.load(std::memory_order_acquire)) {
									observation.Status = workbench_selected_diff_status::cancelled;
								} else if (const auto held = std::optional{ selected_diff_authority{
											   .Workbench = aRequest.WorkbenchAuthority,
											   .Diff      = aRequest.Diff,
										   } };
										   (actor->SelectedDiffAuthority != held
											&& actor->RealignmentAuthority != held)
										   || actor->BlobAuthorityGeneration
											   != admittedGeneration
										   || (aPrepared
											   && (actor->SelectedDiffAuthority != held
												   || actor->PreparedGrantSequence != preparedSequence))
										   || (!aPrepared && selectedReview && actor->SelectedBlobGrantSequence != grantSequence)) {
									observation.Status = workbench_selected_diff_status::stale;
								} else {
									observation.Status = workbench_selected_diff_status::available;
									if (left->Blob.has_value() && left->Path.has_value()) {
										observation.LeftEndpoint
											= epoch_application_host::mint_selected_diff_endpoint(
												aRequest,
												core::diff_side::left,
												*left->Blob,
												*left->Path);
									}
									if (right->Blob.has_value() && right->Path.has_value()) {
										observation.RightEndpoint
											= epoch_application_host::mint_selected_diff_endpoint(
												aRequest,
												core::diff_side::right,
												*right->Blob,
												*right->Path);
									}
									observation.LeftBlob  = std::move(left->Blob);
									observation.RightBlob = std::move(right->Blob);
									observation.LeftPath  = std::move(left->Path);
									observation.RightPath = std::move(right->Path);
									if (aPrepared) {
										std::erase_if(actor->PreparedGrants, [&aRequest](const workbench_selected_diff_observation& aGrant) {
											return aGrant.Request.File == aRequest.File && aGrant.Request.Filter == aRequest.Filter;
										});
										if (actor->PreparedGrants.size() >= core::maximum_prepared_comparison_window) {
											actor->PreparedGrants.erase(actor->PreparedGrants.begin());
										}
										actor->PreparedGrants.push_back(observation);
									} else if (selectedReview) {
										actor->SelectedBlobGrant = observation;
									}
								}
							}
						}
					} catch (...) {
						observation.Status = workbench_selected_diff_status::failure;
					}
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
				// [[BPR-1344]]. A file prepared for the reviewer waits behind their own work.
				aPrepared ? profile_epoch_actor_priority::preparation : profile_epoch_actor_priority::reviewer);
		} catch (...) {
			finish_job(control);
			return result;
		}

		// Registered before the lock is taken, for the reason `materialize`
		// records: an already-stopped token invokes its callback during
		// construction, and doing that under the waiter's mutex would
		// deadlock. BPR-0397.
		auto releaseOnStop = std::stop_callback{ aStop, [waiter] {
													{
														const auto guard  = std::lock_guard{ waiter->Mutex };
														waiter->Abandoned = true;
													}
													waiter->Completed.notify_all();
												} };
		std::unique_lock lock{ waiter->Mutex };
		static_cast<void>(waiter->Completed.wait_for(
			lock, Limits.BlobWait, [&waiter] {
				return waiter->Result.has_value() || waiter->Abandoned;
			}));
		if (waiter->Result.has_value()) {
			return std::move(*waiter->Result);
		}
		// `result` already carries the request. Bounded by `BlobWait` rather
		// than `WorkspaceWait` because this is the same class of work as
		// `materialize`: a read of objects the cache already holds, queued
		// behind whatever the repository actor is running.
		result.Status = waiter->Abandoned
			? workbench_selected_diff_status::cancelled
			: workbench_selected_diff_status::failure;
		return result;
	}

	auto epoch_application_host::implementation::search_actor(const core::source_search_scope& aScope) const -> std::shared_ptr<authority_effect_actor>
	{
		std::scoped_lock actorsLock{ CacheActorsMutex };
		for (const auto& [key, actor] : CacheActors) {
			static_cast<void>(key);
			std::scoped_lock lock{ actor->BlobAuthorityMutex };
			if (actor->SearchScope == aScope && actor->BlobAuthority == blob_authority{ aScope.Session, aScope.Authority }) {
				return actor;
			}
		}
		return { };
	}

	auto epoch_application_host::implementation::bind_source_search(const workbench_pull_request_authority& aAuthority, const core::comparison_pair& aComparison, std::optional<core::document_tab_id> aTab,
																	std::optional<std::vector<core::source_search_filtered_file>> aFiltered) -> std::optional<core::source_search_scope>
	{
		if (!authorized_selected_diff_context(aAuthority, { }, false) || !core::valid_exact_pair(aComparison) || !Active
			|| aComparison.Left.Identity.Authority != Active->Authority.Scope || aComparison.Right.Identity.Authority != Active->Authority.Scope
			|| aComparison.Left.Identity.Incarnation != State.AcceptedComparison->Left.Identity.Incarnation
			|| aComparison.Right.Identity.Incarnation != State.AcceptedComparison->Right.Identity.Incarnation) {
			return { };
		}
		auto scope       = core::source_search_scope{ };
		scope.Session    = State.ApplicationSession;
		scope.Authority  = Active->Authority;
		scope.Comparison = aComparison;
		scope.Tab        = aTab;
		scope.Target     = core::source_search_target::files;
		if (aFiltered) {
			if (aFiltered->size() > core::maximum_source_search_filtered_files) {
				return { };
			}
			scope.FilteredFiles = static_cast<std::uint32_t>(aFiltered->size());
		}
		auto             files = aFiltered ? std::move(*aFiltered) : std::vector<core::source_search_filtered_file>{ };
		auto             actor = Active->Actor;
		std::scoped_lock lock{ actor->BlobAuthorityMutex };
		if (actor->SearchScope && actor->SearchScope->Comparison == aComparison && actor->SearchScope->Tab == aTab
			&& actor->SearchScope->FilteredFiles.has_value() == scope.FilteredFiles.has_value() && actor->SearchFiltered == files) {
			return actor->SearchScope;
		}
		if (actor->SearchGeneration == std::numeric_limits<std::uint64_t>::max()) {
			return { };
		}
		scope.CatalogueGeneration = ++actor->SearchGeneration;
		if (!core::valid(scope) || (scope.FilteredFiles && !core::valid(scope, files))) {
			return { };
		}
		actor->SearchScope    = scope;
		actor->SearchFiltered = std::move(files);
		actor->SearchCorpus.reset();
		if (actor->SearchRead) {
			actor->SearchRead->Stop.request_stop();
		}
		return scope;
	}

	auto epoch_application_host::implementation::exact_search_anchors(authority_context& aContext, const core::comparison_pair& aPair, std::stop_token aStop) -> bool
	{
		const auto snapshot = aContext.Catalog->snapshot(aStop);
		if (!snapshot) {
			return false;
		}
		auto identities = std::vector{ aPair.Left.Identity };
		if (aPair.Right.Identity != aPair.Left.Identity) {
			identities.push_back(aPair.Right.Identity);
		}
		const auto anchors = aContext.Catalog->exact_anchors({ snapshot->Binding.Catalog, identities }, aStop);
		return anchors && anchors->Catalog == snapshot->Binding.Catalog && anchors->Anchors.size() == identities.size()
			&& anchors->Anchors.front() == aPair.Left && anchors->Anchors.back() == aPair.Right;
	}

	auto epoch_application_host::implementation::build_source_corpus(const std::shared_ptr<authority_effect_actor>& aActor, const std::shared_ptr<authority_context>& aContext, const core::source_search_scope& aScope, std::stop_token aWorkStop) -> source_corpus_observation
	{
		if (!exact_search_anchors(*aContext, aScope.Comparison, aWorkStop)) {
			return { };
		}
		auto changes = aActor->Cache->changed_entries({ aScope.Comparison.Left.Commit, aScope.Comparison.Right.Commit }, aWorkStop);
		if (!changes) {
			return { changes.error() == git_cache_failure::quota_exceeded ? core::source_search_status::resource_limit : aWorkStop.stop_requested() ? core::source_search_status::cancelled
																																					: core::source_search_status::unavailable,
					 { } };
		}
		if (changes->size() > maximum_git_changed_entries) {
			return { core::source_search_status::resource_limit, { } };
		}
		auto corpus   = std::make_shared<source_corpus>();
		corpus->Scope = aScope;
		corpus->Entries.reserve(changes->size());
		corpus->OwnedBytes = sizeof(source_corpus) + changes->size() * sizeof(source_corpus_entry);
		for (auto& change : *changes) {
			if (aWorkStop.stop_requested()) {
				return { core::source_search_status::cancelled, { } };
			}
			if (!change.Left && !change.Right) {
				return { };
			}
			for (const auto& side : { change.Left, change.Right }) {
				if (!side) {
					continue;
				}
				if (side->Path.empty() || side->Path.size() > maximum_git_tree_entry_path_bytes || !side->Object.valid()
					|| side->Object.algorithm() != aScope.Comparison.Left.Commit.algorithm()
					|| core::inspect_source_search_query(side->Path) == core::source_search_input::invalid_utf8) {
					return { };
				}
				corpus->OwnedBytes += side->Path.capacity() + 1U;
			}
			if (corpus->OwnedBytes > maximum_git_changed_catalogue_bytes) {
				return { core::source_search_status::resource_limit, { } };
			}
			auto entry   = source_corpus_entry{ };
			entry.Left   = std::move(change.Left);
			entry.Right  = std::move(change.Right);
			entry.Change = change.Change;
			entry.File   = core::file_identity_of(entry.path());
			corpus->Entries.push_back(std::move(entry));
		}
		std::ranges::sort(corpus->Entries, { }, &source_corpus_entry::path);
		for (auto i = std::size_t{ 1U }; i < corpus->Entries.size(); ++i) {
			if (corpus->Entries[i - 1U].path() == corpus->Entries[i].path()) {
				return { };
			}
		}
		if (aScope.FilteredFiles) {
			auto files = std::vector<core::source_search_filtered_file>{ };
			{
				std::scoped_lock lock{ aActor->BlobAuthorityMutex };
				if (aActor->SearchScope != aScope) {
					return { };
				}
				files = aActor->SearchFiltered;
			}
			if (!filter_source_corpus(*aActor, aScope, files, aWorkStop, *corpus)) {
				return { aWorkStop.stop_requested() ? core::source_search_status::cancelled : core::source_search_status::unavailable, { } };
			}
		}
		std::scoped_lock lock{ aActor->BlobAuthorityMutex };
		if (aActor->SearchScope != aScope || !aActor->BlobAuthority || aWorkStop.stop_requested()) {
			return { };
		}
		aActor->SearchCorpus = corpus;
		return { core::source_search_status::complete, std::move(corpus) };
	}

	/// \brief RFC-BPR-0089 ([[BPR-1688]]). Narrows a catalogue to a
	/// filtered review's files, each side read at its filtered diff's
	/// commits: in the retained cache or, for a sparse review, the chain
	/// store. Every commit named is one the touches job authorized. A file
	/// the comparison does not list (a shown commit added it, a hidden one
	/// removed it) is searched at those commits too.
	auto epoch_application_host::implementation::filter_source_corpus(authority_effect_actor& aActor, const core::source_search_scope& aScope,
																	  const std::vector<core::source_search_filtered_file>& aFiles, const std::stop_token aStop, source_corpus& aCorpus) -> bool
	{
		auto chainTrees = std::shared_ptr<git_chain_tree_store>{ };
		{
			std::scoped_lock lock{ aActor.BlobAuthorityMutex };
			for (const auto& file : aFiles) {
				if (file.Endpoints
					&& (!std::ranges::binary_search(aActor.FilterCommits, file.Endpoints->Left) || !std::ranges::binary_search(aActor.FilterCommits, file.Endpoints->Right))) {
					return false;
				}
			}
			chainTrees = aActor.ChainTrees;
		}
		const auto lookup = [&](const std::string& aPath, const core::git_commit_oid& aCommit) -> std::optional<std::optional<core::git_blob_oid>> {
			if (chainTrees && chainTrees->holds(aCommit)) {
				auto held = chainTrees->find_blob_at_path(aCommit, aPath, aStop);
				return held ? std::optional{ *held } : std::nullopt;
			}
			auto found = aActor.Cache->find_blob_at_path(
				git_cache_tree_entry_request{
					.Commit = aCommit,
					.Path   = std::filesystem::path{ std::u8string{ reinterpret_cast<const char8_t*>(aPath.data()), aPath.size() } },
				},
				aStop);
			return found ? std::optional{ *found } : std::nullopt;
		};
		auto entries = std::vector<source_corpus_entry>{ };
		entries.reserve(aFiles.size());
		for (const auto& file : aFiles) {
			if (aStop.stop_requested()) {
				return false;
			}
			const auto listed = std::ranges::find(aCorpus.Entries, std::string_view{ file.Path }, &source_corpus_entry::path);
			if (!file.Endpoints) {
				if (listed != aCorpus.Entries.end()) {
					entries.push_back(*listed);
				}
				continue;
			}
			const auto& found  = listed != aCorpus.Entries.end() ? std::optional{ *listed } : std::nullopt;
			const auto  sideAt = [&](const std::optional<git_cache_changed_side>& aListed, const core::git_commit_oid& aCommit) -> std::optional<std::optional<git_cache_changed_side>> {
				const auto path = aListed ? aListed->Path : file.Path;
				const auto blob = lookup(path, aCommit);
				if (!blob) {
					return std::nullopt;
				}
				if (!*blob) {
					return std::optional<git_cache_changed_side>{ };
				}
				return std::optional{ git_cache_changed_side{ .Path = path, .Object = **blob, .Mode = aListed ? aListed->Mode : 0100644U } };
			};
			auto left  = sideAt(found ? found->Left : std::nullopt, file.Endpoints->Left);
			auto right = sideAt(found ? found->Right : std::nullopt, file.Endpoints->Right);
			if (!left || !right) {
				return false;
			}
			if (!*left && !*right) {
				continue;
			}
			auto entry   = source_corpus_entry{ };
			entry.Left   = std::move(*left);
			entry.Right  = std::move(*right);
			entry.Change = entry.Left && entry.Right ? 'M' : entry.Right ? 'A'
																		 : 'D';
			entry.File   = core::file_identity_of(entry.path());
			entries.push_back(std::move(entry));
		}
		std::ranges::sort(entries, { }, &source_corpus_entry::path);
		if (std::ranges::adjacent_find(entries, { }, &source_corpus_entry::path) != entries.end() || aScope.Comparison.Left.Commit.algorithm() != aScope.Comparison.Right.Commit.algorithm()) {
			return false;
		}
		aCorpus.Entries = std::move(entries);
		return true;
	}

	auto epoch_application_host::implementation::search_catalogue_turn(const std::shared_ptr<authority_effect_actor>& aActor, const std::shared_ptr<authority_context>& aContext, const core::source_search_scope& aScope, std::stop_token aStop) -> source_corpus_observation
	{
		auto stop     = std::stop_source{ };
		auto forward  = std::stop_callback{ aStop, [&] { stop.request_stop(); } };
		auto priority = prepared_read_registration{ *aActor, stop, false };
		if (!priority.admitted()) {
			return { core::source_search_status::searching, { } };
		}
		auto result = build_source_corpus(aActor, aContext, aScope, stop.get_token());
		if (stop.stop_requested() && !aStop.stop_requested()) {
			return { core::source_search_status::searching, { } };
		}
		return result;
	}

	auto epoch_application_host::implementation::catalogue(const core::source_search_scope& aScope, std::stop_token aStop) -> source_corpus_observation
	{
		if (aStop.stop_requested()) {
			return { core::source_search_status::cancelled, { } };
		}
		if (!begin_blob_call()) {
			return { };
		}
		struct call_guard
		{
			implementation& Host;
			~call_guard() { Host.finish_blob_call(); }
		} call{ *this };
		const auto actor = search_actor(aScope);
		if (!actor) {
			return { };
		}
		auto context = std::shared_ptr<authority_context>{ };
		{
			std::scoped_lock lock{ actor->BlobAuthorityMutex };
			if (actor->SearchCorpus) {
				return { core::source_search_status::complete, actor->SearchCorpus };
			}
			context = actor->BlobContext.lock();
		}
		if (!context) {
			return { };
		}
		while (!aStop.stop_requested()) {
			if (search_actor(aScope) != actor) {
				return { };
			}
			const auto result = run_on_actor<source_corpus_observation>(actor, { .ApplicationSession = aScope.Session, .Authority = aScope.Authority, .ObjectFormat = aScope.Comparison.Left.Commit.algorithm() }, [actor, context, aScope](std::stop_token aWorkStop) { return search_catalogue_turn(actor, context, aScope, aWorkStop); }, aStop);
			if (!result) {
				return { aStop.stop_requested() ? core::source_search_status::cancelled : core::source_search_status::unavailable, { } };
			}
			if (result->Status != core::source_search_status::searching) {
				return *result;
			}
			std::this_thread::yield();
		}
		return { core::source_search_status::cancelled, { } };
	}

	auto epoch_application_host::implementation::acquire_source_corpus_material(const std::shared_ptr<authority_effect_actor>& aActor, const std::shared_ptr<authority_context>& aContext, const std::shared_ptr<const source_corpus>& aCorpus, core::diff_side aSide, const std::shared_ptr<corpus_read_waiter>& aWaiter, const std::shared_ptr<job_control>& aControl) -> source_corpus_material
	{
		auto material = source_corpus_material{ };
		try {
			auto forwardJobStop = std::stop_callback{ aControl->Stop.get_token(), [aWaiter] { aWaiter->Stop.request_stop(); } };
			auto priority       = prepared_read_registration{ *aActor, aWaiter->Stop, false };
			if (!priority.admitted()) {
				return { diff_blob_status::cancelled, true, { } };
			}
			const auto workStop   = aWaiter->Stop.get_token();
			const auto scope      = aCorpus->Scope;
			auto       validGrant = false;
			{
				std::scoped_lock lock{ aActor->BlobAuthorityMutex };
				validGrant = aActor->SearchCorpus == aCorpus && aActor->SearchScope == scope;
			}
			if (validGrant && !workStop.stop_requested() && Consumer->active() && !BlobIngressClosing.load(std::memory_order_acquire)
				&& open_actor_cache(*aActor, Caches, { .ApplicationSession = scope.Session, .Authority = scope.Authority, .ObjectFormat = aWaiter->Blob.algorithm() }, workStop)) {
				auto bytes = std::optional<std::vector<std::uint8_t>>{ };
				{
					std::scoped_lock lock{ aActor->BlobAuthorityMutex };
					bytes = retained_blob(*aActor, aWaiter->Blob);
				}
				material.Cached = bytes.has_value();
				auto cached     = bytes ? git_cache_result<std::vector<std::uint8_t>>{ std::move(*bytes) } : aActor->Cache->materialize_blob({ aWaiter->Blob, maximum_git_cache_blob_materialization_bytes }, workStop);
				if (cached) {
					bytes = std::move(*cached);
				} else {
					bytes.reset();
					material.Status    = cached.error() == git_cache_failure::quota_exceeded ? diff_blob_status::materialization_limit : diff_blob_status::missing;
					const auto& anchor = aSide == core::diff_side::left ? scope.Comparison.Left : scope.Comparison.Right;
					if (cached.error() == git_cache_failure::object_mismatch && aContext->Handoff.SmartGit && aContext->Handoff.SmartGit->supports_sparse_commit_trees()
						&& durably_sparse_anchor(*aActor, *aContext, anchor, workStop)) {
						if (priority.admitted()) {
							auto smart               = git_smart_request_context{ };
							smart.ApplicationSession = aContext->Handoff.ApplicationSession;
							smart.Authority          = aContext->Authority;
							smart.Revision           = aContext->Revision;
							smart.Repository         = aContext->Handoff.Locator;
							smart.BaseRef            = aContext->Handoff.BaseRef;
							const auto transport     = aContext->Handoff.SmartGit->acquire(smart);
							if (transport && *transport) {
								const auto fetched = (*transport)->read_exact_blob({ smart, aWaiter->Blob, maximum_git_cache_blob_materialization_bytes }, workStop);
								if (fetched) {
									bytes.emplace(fetched->size());
									std::ranges::transform(*fetched, bytes->begin(), [](std::byte aByte) { return std::to_integer<std::uint8_t>(aByte); });
									// A mismatch or a collision detection both end here, at failure.
									if (blob_matches(aWaiter->Blob, *bytes) != blob_check::matches) {
										bytes.reset();
										material.Status = diff_blob_status::failure;
									}
								} else {
									material.Status = fetched.error().Failure == git_smart_transport_failure::response_limit ? diff_blob_status::materialization_limit : diff_blob_status::failure;
								}
							}
						}
					}
				}
				std::scoped_lock lock{ aActor->BlobAuthorityMutex };
				if (bytes && aActor->SearchCorpus == aCorpus && aActor->SearchScope == scope && !workStop.stop_requested()) {
					if (bytes->size() > maximum_git_cache_blob_materialization_bytes) {
						material.Status = diff_blob_status::materialization_limit;
					} else {
						retain_blob(*aActor, aWaiter->Blob, *bytes, true);
						material.Bytes  = std::make_shared<const std::vector<std::uint8_t>>(std::move(*bytes));
						material.Status = diff_blob_status::available;
					}
				}
			}
			if (workStop.stop_requested()) {
				material.Status = diff_blob_status::cancelled;
				std::scoped_lock lock{ aActor->BlobAuthorityMutex };
				material.Yielded = !aControl->Stop.stop_requested() && aActor->SearchCorpus == aCorpus && still_needed(aWaiter->Needed);
			}
		} catch (...) {
			material = { };
		}
		return material;
	}

	auto epoch_application_host::implementation::read_source_corpus(const std::shared_ptr<const source_corpus>& aCorpus, std::size_t aEntry, core::diff_side aSide, std::stop_token aStop, source_corpus_read_needed aNeeded) -> source_corpus_material
	{
		if (aStop.stop_requested()) {
			return { diff_blob_status::cancelled, false, { } };
		}
		if (aSide != core::diff_side::left && aSide != core::diff_side::right) {
			return { };
		}
		if (!aCorpus || aEntry >= aCorpus->Entries.size() || !begin_blob_call()) {
			return { };
		}
		struct call_guard
		{
			implementation& Host;
			~call_guard() { Host.finish_blob_call(); }
		} call{ *this };
		const auto actor = search_actor(aCorpus->Scope);
		if (!actor) {
			return { };
		}
		const auto& side = aSide == core::diff_side::left ? aCorpus->Entries[aEntry].Left : aCorpus->Entries[aEntry].Right;
		if (!side) {
			return { diff_blob_status::missing, false, { } };
		}
		if (side->Mode != 0100644U && side->Mode != 0100755U) {
			return { diff_blob_status::generated_file_unsupported, false, { } };
		}
		auto context   = std::shared_ptr<authority_context>{ };
		auto waiter    = std::shared_ptr<corpus_read_waiter>{ };
		auto coalesced = false;
		{
			std::scoped_lock lock{ actor->BlobAuthorityMutex };
			if (actor->SearchCorpus != aCorpus) {
				return { };
			}
			context = actor->BlobContext.lock();
			if (actor->SearchRead) {
				if (actor->SearchRead->Blob != side->Object || actor->SearchRead->Corpus.lock() != aCorpus) {
					return { diff_blob_status::cancelled, true, { } };
				}
				waiter         = actor->SearchRead;
				waiter->Needed = std::move(aNeeded);
				coalesced      = true;
			} else {
				waiter            = std::make_shared<corpus_read_waiter>();
				waiter->Blob      = side->Object;
				waiter->Corpus    = aCorpus;
				waiter->Needed    = std::move(aNeeded);
				actor->SearchRead = waiter;
			}
		}
		if (!context) {
			std::scoped_lock lock{ actor->BlobAuthorityMutex };
			if (actor->SearchRead == waiter && !coalesced) {
				actor->SearchRead.reset();
			}
			return { };
		}
		if (!coalesced) {
			auto admitted = admit_job({ }, { }, { }, { }, false, false, true);
			if (!admitted) {
				std::scoped_lock lock{ actor->BlobAuthorityMutex };
				if (actor->SearchRead == waiter) {
					actor->SearchRead.reset();
				}
				return { };
			}
			auto control = std::move(*admitted);
			try {
				enqueue_actor_work(actor, [this, actor, context, corpus = aCorpus, aSide, waiter, control] {
                        auto material = acquire_source_corpus_material(actor,context,corpus,aSide,waiter,control);

                        {
                            std::scoped_lock lock{actor->BlobAuthorityMutex};
                            if (actor->SearchRead == waiter) { actor->SearchRead.reset(); }
                        }
                        {
                            std::scoped_lock lock{waiter->Mutex};
                            waiter->Material = std::move(material);
                            waiter->Done = true;
                        }
                        const auto reserved = retire_actor_job(control);
                        waiter->Changed.notify_all();
                        release_actor_job_reservation(reserved,control->Preparation); }, profile_epoch_actor_priority::preparation);
			} catch (...) {
				finish_job(control);
				std::scoped_lock lock{ actor->BlobAuthorityMutex };
				if (actor->SearchRead == waiter) {
					actor->SearchRead.reset();
				}
				return { };
			}
		}
		auto wake = std::stop_callback{ aStop, [waiter, actor] {
										   {
											   std::scoped_lock lock{ actor->BlobAuthorityMutex };
											   if (!still_needed(waiter->Needed)) {
												   waiter->Stop.request_stop();
											   }
										   }
										   waiter->Changed.notify_all();
									   } };
		std::unique_lock lock{ waiter->Mutex };
		const auto       done = waiter->Changed.wait_for(lock, Limits.BlobWait, [&] { return waiter->Done || aStop.stop_requested(); });
		if (aStop.stop_requested()) {
			return { diff_blob_status::cancelled, false, { } };
		}
		if (!done) {
			waiter->Stop.request_stop();
			return { };
		}
		return waiter->Material;
	}

	/// \brief Prepares the files the reviewer is about to open. [[BPR-1327]]
	auto epoch_application_host::implementation::prepare_selected_diffs(
		const std::vector<workbench_selected_diff_request>& aRequests,
		const std::stop_token                               aStop) -> epoch_prepared_comparisons
	{
		auto result = epoch_prepared_comparisons{ };
		if (aRequests.empty() || aRequests.size() > core::maximum_prepared_comparison_window) {
			return result;
		}
		const auto& first = aRequests.front();
		if (!std::ranges::all_of(aRequests, [&first](const workbench_selected_diff_request& aRequest) {
				return aRequest.WorkbenchAuthority == first.WorkbenchAuthority && aRequest.Diff == first.Diff;
			})) {
			return result;
		}
		for (const auto& request : aRequests) {
			if (aStop.stop_requested()) {
				return result;
			}
			auto observation = resolve_selected_diff(request, aStop, true);
			if (observation.Status == workbench_selected_diff_status::available) {
				result.Observations.push_back(std::move(observation));
			} else if (observation.Status == workbench_selected_diff_status::stale
					   || observation.Status == workbench_selected_diff_status::cancelled) {
				return result;
			}
		}
		fetch_prepared_blobs(first, result, aStop);
		return result;
	}

	/// \brief Reads the prepared files' missing sparse blobs in batches.
	///
	/// \details Only blobs a prepared grant names, only for an endpoint the
	/// actor proves durably sparse (the proof a single exact read needs),
	/// and only through a transport that reads batches. The network round
	/// trip runs on the caller's thread, never on the actor, so the
	/// reviewer's own work on the repository is not queued behind it.
	/// Verified bytes are retained for the authority that read them. A
	/// batch that fails, other than by cancellation, suspends preparation's
	/// single reads under the authority until one succeeds. [[BPR-1359]]
	void epoch_application_host::implementation::fetch_prepared_blobs(const workbench_selected_diff_request& aTemplate,
																	  epoch_prepared_comparisons&            aResult,
																	  const std::stop_token                  aStop)
	{
		if (aResult.Observations.empty() || aStop.stop_requested() || !begin_blob_call()) {
			return;
		}
		struct blob_call_guard final
		{
			implementation& Host;

			~blob_call_guard()
			{
				Host.finish_blob_call();
			}
		} call{ *this };
		const auto key = actor_key(aTemplate);
		if (!key.has_value()) {
			return;
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
			return;
		}
		const auto authorization = blob_authority{
			.ApplicationSession = aTemplate.Diff.ApplicationSession,
			.Authority          = aTemplate.Diff.Authority,
		};
		auto context    = std::shared_ptr<authority_context>{ };
		auto generation = std::uint64_t{ 0U };
		auto left       = std::vector<core::git_blob_oid>{ };
		auto right      = std::vector<core::git_blob_oid>{ };
		{
			std::scoped_lock lock{ actor->BlobAuthorityMutex };
			if (actor->BlobAuthority != std::optional{ authorization }) {
				return;
			}
			generation      = actor->BlobAuthorityGeneration;
			context         = actor->BlobContext.lock();
			const auto want = [&actor, &left, &right](const std::optional<core::git_blob_oid>& aBlob,
													  std::vector<core::git_blob_oid>&         aSide) {
				if (aBlob && std::ranges::find(left, *aBlob) == left.end()
					&& std::ranges::find(right, *aBlob) == right.end()
					&& !retained_blob(*actor, *aBlob)) {
					aSide.push_back(*aBlob);
				}
			};
			for (const auto& observation : aResult.Observations) {
				want(observation.LeftBlob, left);
				want(observation.RightBlob, right);
			}
		}
		if (!context || (left.empty() && right.empty())
			|| object_loading_for(context->Handoff.Shape) != pull_request_object_loading::selected_objects
			|| !context->Handoff.SmartGit || !context->Handoff.SmartGit->supports_sparse_commit_trees()) {
			return;
		}
		const auto comparison = aTemplate.Diff.Comparison;
		const auto durable    = run_on_actor<std::pair<bool, bool>>(
			actor,
			git_cache_open_request{
				.ApplicationSession = aTemplate.Diff.ApplicationSession,
				.Authority          = aTemplate.Diff.Authority,
				.ObjectFormat       = comparison.Left.Commit.algorithm(),
			},
			[actor, context, comparison](const std::stop_token aWorkStop) {
				return std::pair{
					durably_sparse_anchor(*actor, *context, comparison.Left, aWorkStop),
					durably_sparse_anchor(*actor, *context, comparison.Right, aWorkStop),
				};
			},
			aStop);
		if (!durable) {
			return;
		}
		auto wanted = durable->first ? std::move(left) : std::vector<core::git_blob_oid>{ };
		if (durable->second) {
			wanted.insert(wanted.end(), right.begin(), right.end());
		}
		if (wanted.empty()) {
			return;
		}
		const auto smartContext = git_smart_request_context{
			.ApplicationSession = context->Handoff.ApplicationSession,
			.Authority          = context->Authority,
			.Revision           = context->Revision,
			.Repository         = context->Handoff.Locator,
			.BaseRef            = context->Handoff.BaseRef,
		};
		const auto transport = context->Handoff.SmartGit->acquire(smartContext);
		if (!transport || !*transport) {
			return;
		}
		for (std::size_t offset = 0U; offset < wanted.size() && !aStop.stop_requested();
			 offset += maximum_git_exact_blob_batch) {
			const auto count   = std::min(maximum_git_exact_blob_batch, wanted.size() - offset);
			auto       request = git_smart_exact_blobs_request{
				.Context       = smartContext,
				.ExpectedBlobs = { wanted.begin() + static_cast<std::ptrdiff_t>(offset),
								   wanted.begin() + static_cast<std::ptrdiff_t>(offset + count) },
				.MaximumBytes  = maximum_git_exact_blob_batch_bytes,
			};
			const auto read = (*transport)->read_exact_blobs(request, aStop);
			++aResult.RemoteBatches;
			const auto cancelled = aStop.stop_requested()
				|| (!read && read.error().Failure == git_smart_transport_failure::cancelled);
			std::scoped_lock lock{ actor->BlobAuthorityMutex };
			if (actor->BlobAuthority != std::optional{ authorization }
				|| actor->BlobAuthorityGeneration != generation) {
				return;
			}
			if (!read || read->size() != count) {
				// A transport that cannot batch reads nothing ahead; the
				// reviewer's open still reads its own file exactly.
				// [[BPR-1359]]. Nor are the prepared files' blobs then read
				// one at a time: until a batch succeeds, preparation backs
				// off rather than spend a process per blob.
				actor->PreparedReadsSuspended = actor->PreparedReadsSuspended || !cancelled;
				return;
			}
			actor->PreparedReadsSuspended = false;
			for (std::size_t index = 0U; index < count; ++index) {
				const auto& answer = (*read)[index];
				if (!answer || answer->size() > maximum_git_exact_blob_batch_bytes) {
					continue;
				}
				auto bytes = std::vector<std::uint8_t>{ };
				bytes.reserve(answer->size());
				for (const auto byte : *answer) {
					bytes.push_back(std::to_integer<std::uint8_t>(byte));
				}
				if (blob_matches(request.ExpectedBlobs[index], bytes) != blob_check::matches) {
					continue;
				}
				retain_blob(*actor, request.ExpectedBlobs[index], bytes);
				++aResult.RemoteBlobs;
			}
		}
	}

	/// The host's own check that bytes are the blob they claim to be,
	/// beside the transport's, through the core's collision-detecting
	/// SHA-1. SHA-1 object IDs only; a SHA-256 blob is not retained from a
	/// batch. Every caller ends anything but a match at a failure.
	auto epoch_application_host::implementation::blob_matches(const core::git_blob_oid&        aBlob,
															  const std::vector<std::uint8_t>& aBytes) noexcept -> blob_check
	{
		if (aBlob.algorithm() != core::git_hash_algorithm::sha1) {
			return blob_check::mismatch;
		}
		auto hash = core::git_object_hasher{ core::git_hash_algorithm::sha1, core::git_object_type::blob, aBytes.size() };
		hash.update(aBytes);
		const auto digest = hash.finish();
		if (!digest) {
			return digest.error() == core::git_object_refusal::collision_detected ? blob_check::collision_detected : blob_check::mismatch;
		}
		return std::ranges::equal(std::span{ *digest }.first(20U), std::span{ aBlob.bytes() }.first(20U)) ? blob_check::matches : blob_check::mismatch;
	}

	/// \brief The actor's part of a blob read, up to the network.
	/// [[BPR-1360]]
	///
	/// \details Runs on the actor. Leaves `Remote` set when the blob must
	/// be read exactly from the network.
	void epoch_application_host::implementation::begin_actor_blob_read(actor_blob_read&                               aRead,
																	   const std::shared_ptr<authority_effect_actor>& aActor,
																	   const std::shared_ptr<authority_context>&      aSelected,
																	   const blob_authority&                          aAuthorization,
																	   const std::uint64_t                            aAdmittedGeneration,
																	   const git_cache_open_request&                  aExpected,
																	   const std::stop_source&                        aJobStop)
	{
		const auto& request  = aRead.Request;
		const auto  workStop = aJobStop.get_token();
		if (workStop.stop_requested() || !Consumer->active() || BlobIngressClosing.load(std::memory_order_acquire)) {
			aRead.Observation.Status = diff_blob_status::cancelled;
			return;
		}
		if (blob_authorization_generation(aActor, aAuthorization) != aAdmittedGeneration) {
			aRead.Observation.Status = diff_blob_status::failure;
			return;
		}
		if (!open_actor_cache(*aActor, Caches, aExpected, workStop).has_value()) {
			return;
		}
		aRead.Opened = true;
		aRead.Cached = aActor->Cache->materialize_blob(
			git_cache_blob_request{
				.Blob         = request.Blob,
				.MaximumBytes = request.MaximumBytes,
			},
			workStop);
		if (*aRead.Cached) {
			aRead.Bytes = std::move(**aRead.Cached);
			return;
		}
		if (aRead.Cached->error() != git_cache_failure::object_mismatch
			|| object_loading_for(aSelected->Handoff.Shape) != pull_request_object_loading::selected_objects
			|| blob_authority_for(*aSelected) != aAuthorization || !aSelected->Handoff.SmartGit
			|| !aSelected->Handoff.SmartGit->supports_sparse_commit_trees()
			|| blob_authorization_generation(aActor, aAuthorization) != aAdmittedGeneration) {
			return;
		}
		auto selectedPair = std::optional<core::comparison_pair>{ };
		{
			std::scoped_lock lock{ aActor->BlobAuthorityMutex };
			if (aRead.PinnedSource) {
				if (aActor->RealignmentAuthority != std::optional{ selected_diff_authority{ .Workbench = aRead.PinnedSource->Request.WorkbenchAuthority, .Diff = aRead.PinnedSource->Request.Diff } }) {
					return;
				}
				selectedPair = aRead.PinnedSource->Request.Diff.Comparison;
			} else if (aActor->SelectedBlobGrant && exact_selected_blob_request(*aActor->SelectedBlobGrant, request)) {
				aRead.SelectedGrantSequence = aActor->SelectedBlobGrantSequence;
				selectedPair                = aActor->SelectedBlobGrant->Request.Diff.Comparison;
			} else if (const auto prepared = std::ranges::find_if(
						   aActor->PreparedGrants,
						   [&request](const workbench_selected_diff_observation& aGrant) {
							   return exact_selected_blob_request(aGrant, request);
						   });
					   prepared != aActor->PreparedGrants.end()) {
				// [[BPR-1327]]. A prepared file's own blob.
				aRead.PreparedGrantSequence = aActor->PreparedGrantSequence;
				selectedPair                = prepared->Request.Diff.Comparison;
			}
			if (selectedPair) {
				// Bytes already read and verified for this authority need
				// no second network read.
				aRead.Bytes = retained_blob(*aActor, request.Blob);
				if (aRead.Bytes && aRead.Bytes->size() > request.MaximumBytes) {
					aRead.Bytes.reset();
					aRead.RetainedOverLimit = true;
				}
			}
		}
		if (aRead.Bytes || aRead.RetainedOverLimit || !selectedPair
			|| (!aRead.PinnedSource && !aRead.SelectedGrantSequence && !aRead.PreparedGrantSequence)) {
			return;
		}
		if (aRead.PinnedSource) {
			// [[BPR-1377]]. Pinned source may name either side or an older
			// retained commit, independently of the displayed epoch pair.
			// Its sealed endpoint proves exact path membership; the retained
			// cache must still physically hold that commit's sparse tree.
			const auto commit = aActor->Cache->inspect_sparse_commit_object(selectedPair->Left.Commit, workStop);
			if (!commit || !application::valid(*commit) || commit->Commit != selectedPair->Left.Commit
				|| commit->Residency != git_revision_residency::sparse_commit_trees) {
				return;
			}
		} else if (!durably_sparse_anchor(*aActor, *aSelected,
										  request.Side == core::diff_side::left ? selectedPair->Left : selectedPair->Right, workStop)) {
			return;
		}
		// [[BPR-1359]]. A prepared file's blob read for preparation alone:
		// it yields to reviewer work on this actor, and is not spent at all
		// while this authority's batched read is failing.
		if (aRead.PreparedGrantSequence && !aRead.SelectedGrantSequence) {
			aRead.PreparedRead.emplace(*aActor, aJobStop);
			if (!aRead.PreparedRead->admitted()) {
				aRead.Observation.Status = aRead.PreparedRead->suspended() ? diff_blob_status::failure
																		   : diff_blob_status::cancelled;
				aRead.PreparedRead.reset();
				return;
			}
		}
		const auto smartContext = git_smart_request_context{
			.ApplicationSession = aSelected->Handoff.ApplicationSession,
			.Authority          = aSelected->Authority,
			.Revision           = aSelected->Revision,
			.Repository         = aSelected->Handoff.Locator,
			.BaseRef            = aSelected->Handoff.BaseRef,
		};
		auto transport = aSelected->Handoff.SmartGit->acquire(smartContext);
		if (transport && *transport) {
			// Assigned member by member: GCC 15 at -O3 misreads a moved
			// designated-initializer temporary's strings (tests/AGENTS.md).
			aRead.Transport = std::move(*transport);
			aRead.Remote.emplace();
			aRead.Remote->Context      = smartContext;
			aRead.Remote->ExpectedBlob = request.Blob;
			aRead.Remote->MaximumBytes = static_cast<std::size_t>(request.MaximumBytes);
			return;
		}
		if (!transport && transport.error().Failure == git_smart_transport_failure::cancelled) {
			aRead.Observation.Status = diff_blob_status::cancelled;
		}
		aRead.PreparedRead.reset();
	}
}
